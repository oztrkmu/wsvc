/* wsvc — Windows service helpers by Murat / oztrkmu. */
#include "wsvc.h"
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>

static SERVICE_STATUS_HANDLE serviceHandle;
static SERVICE_STATUS serviceState;
static SRWLOCK stateLock = SRWLOCK_INIT;
static HANDLE stopEvent;
static wsvcCfg serviceConfig;

static int fail(DWORD code)
{
    SetLastError(code);
    return -1;
}

static int valid(const wsvcCfg *cfg)
{
    if (!cfg || !cfg->name || !*cfg->name || wcslen(cfg->name) > 256 ||
        wcspbrk(cfg->name, L"/\\"))
        return fail(ERROR_INVALID_PARAMETER);
    return 0;
}

/* Caller holds stateLock. */
static void report(DWORD value, DWORD code, DWORD hint)
{
    serviceState.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    serviceState.dwCurrentState = value;
    serviceState.dwWin32ExitCode = code;
    serviceState.dwWaitHint = hint;
    serviceState.dwControlsAccepted = value == SERVICE_RUNNING
        ? SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN : 0;
    serviceState.dwCheckPoint = (value == SERVICE_START_PENDING ||
        value == SERVICE_STOP_PENDING) ? serviceState.dwCheckPoint + 1 : 0;
    SetServiceStatus(serviceHandle, &serviceState);
}

static DWORD WINAPI control(DWORD code, DWORD type, LPVOID data, LPVOID ctx)
{
    DWORD result = NO_ERROR;
    (void)type; (void)data; (void)ctx;
    AcquireSRWLockExclusive(&stateLock);
    switch (code) {
    case SERVICE_CONTROL_STOP:
    case SERVICE_CONTROL_SHUTDOWN:
        if (serviceState.dwCurrentState == SERVICE_RUNNING) {
            report(SERVICE_STOP_PENDING, NO_ERROR, 30000);
            SetEvent(stopEvent);
        }
        break;
    case SERVICE_CONTROL_INTERROGATE:
        SetServiceStatus(serviceHandle, &serviceState);
        break;
    default:
        result = ERROR_CALL_NOT_IMPLEMENTED;
    }
    ReleaseSRWLockExclusive(&stateLock);
    return result;
}

static void WINAPI serviceMain(DWORD argc, LPWSTR *argv)
{
    DWORD code;
    (void)argc; (void)argv;
    serviceHandle = RegisterServiceCtrlHandlerExW(serviceConfig.name, control, NULL);
    if (!serviceHandle)
        return;
    AcquireSRWLockExclusive(&stateLock);
    ZeroMemory(&serviceState, sizeof(serviceState));
    report(SERVICE_START_PENDING, NO_ERROR, 3000);
    stopEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
    if (!stopEvent) {
        report(SERVICE_STOPPED, GetLastError(), 0);
        ReleaseSRWLockExclusive(&stateLock);
        return;
    }
    report(SERVICE_RUNNING, NO_ERROR, 0);
    ReleaseSRWLockExclusive(&stateLock);
    code = serviceConfig.run(stopEvent);
    AcquireSRWLockExclusive(&stateLock);
    report(SERVICE_STOPPED, code, 0);
    CloseHandle(stopEvent);
    stopEvent = NULL;
    ReleaseSRWLockExclusive(&stateLock);
}

int wsvcRun(const wsvcCfg *cfg)
{
    SERVICE_TABLE_ENTRYW table[2] = {{0}};
    if (valid(cfg) != 0)
        return -1;
    if (!cfg->run)
        return fail(ERROR_INVALID_PARAMETER);
    serviceConfig = *cfg;
    table[0].lpServiceName = (LPWSTR)cfg->name;
    table[0].lpServiceProc = serviceMain;
    return StartServiceCtrlDispatcherW(table) ? 0 : -1;
}

static SC_HANDLE openService(const wsvcCfg *cfg, DWORD access)
{
    SC_HANDLE manager, service;
    DWORD code;
    if (valid(cfg) != 0)
        return NULL;
    manager = OpenSCManagerW(NULL, NULL, SC_MANAGER_CONNECT);
    if (!manager)
        return NULL;
    service = OpenServiceW(manager, cfg->name, access);
    code = service ? NO_ERROR : GetLastError();
    CloseServiceHandle(manager);
    SetLastError(code);
    return service;
}

static int finish(SC_HANDLE service, DWORD code)
{
    CloseServiceHandle(service);
    return code == NO_ERROR ? 0 : fail(code);
}

int wsvcInstall(const wsvcCfg *cfg)
{
    const DWORD capacity = 32768;
    wchar_t *path, *command;
    DWORD length, code = NO_ERROR;
    SC_HANDLE manager, service;
    SERVICE_DESCRIPTIONW description;
    SERVICE_DELAYED_AUTO_START_INFO delayed;
    SERVICE_FAILURE_ACTIONSW recovery = {0};
    SC_ACTION actions[] = {{SC_ACTION_RESTART, 5000},
                           {SC_ACTION_RESTART, 15000}, {SC_ACTION_NONE, 0}};
    if (valid(cfg) != 0)
        return -1;
    if (cfg->startup < WSVC_START_DELAYED || cfg->startup > WSVC_START_MANUAL)
        return fail(ERROR_INVALID_PARAMETER);
    path = calloc(capacity, sizeof(*path));
    command = calloc(capacity + 16, sizeof(*command));
    if (!path || !command) {
        free(path); free(command);
        return fail(ERROR_NOT_ENOUGH_MEMORY);
    }
    length = GetModuleFileNameW(NULL, path, capacity);
    if (!length || length >= capacity) {
        code = length ? ERROR_INSUFFICIENT_BUFFER : GetLastError();
        free(path); free(command);
        return fail(code);
    }
    if (swprintf(command, capacity + 16, L"\"%ls\" --service", path) < 0) {
        free(path); free(command);
        return fail(ERROR_INSUFFICIENT_BUFFER);
    }
    free(path);
    manager = OpenSCManagerW(NULL, NULL, SC_MANAGER_CREATE_SERVICE);
    if (!manager) {
        code = GetLastError(); free(command);
        return fail(code);
    }
    service = CreateServiceW(manager, cfg->name,
        cfg->display ? cfg->display : cfg->name,
        SERVICE_CHANGE_CONFIG | SERVICE_START | DELETE,
        SERVICE_WIN32_OWN_PROCESS,
        cfg->startup == WSVC_START_MANUAL ? SERVICE_DEMAND_START : SERVICE_AUTO_START,
        SERVICE_ERROR_NORMAL, command, NULL, NULL, NULL,
        L"NT AUTHORITY\\LocalService", NULL);
    code = service ? NO_ERROR : GetLastError();
    free(command);
    CloseServiceHandle(manager);
    if (!service)
        return fail(code);
    description.lpDescription = (LPWSTR)cfg->desc;
    delayed.fDelayedAutostart = cfg->startup == WSVC_START_DELAYED;
    recovery.dwResetPeriod = 86400;
    recovery.cActions = sizeof(actions) / sizeof(actions[0]);
    recovery.lpsaActions = actions;
    if (cfg->desc && !ChangeServiceConfig2W(service, SERVICE_CONFIG_DESCRIPTION, &description))
        code = GetLastError();
    if (!code && cfg->startup != WSVC_START_MANUAL &&
        !ChangeServiceConfig2W(service, SERVICE_CONFIG_DELAYED_AUTO_START_INFO, &delayed))
        code = GetLastError();
    if (!code && !ChangeServiceConfig2W(service, SERVICE_CONFIG_FAILURE_ACTIONS, &recovery))
        code = GetLastError();
    if (code) {
        /* Roll back a newly created service; never alter an existing service. */
        if (!DeleteService(service))
            fwprintf(stderr, L"wsvc: install rollback failed: %lu\n", GetLastError());
    }
    return finish(service, code);
}

static DWORD query(SC_HANDLE service, SERVICE_STATUS_PROCESS *status)
{
    DWORD needed;
    return QueryServiceStatusEx(service, SC_STATUS_PROCESS_INFO,
        (LPBYTE)status, sizeof(*status), &needed) ? NO_ERROR : GetLastError();
}

static DWORD waitFor(SC_HANDLE service, DWORD desired)
{
    ULONGLONG started = GetTickCount64();
    SERVICE_STATUS_PROCESS status;
    for (;;) {
        DWORD code = query(service, &status);
        if (code)
            return code;
        if (status.dwCurrentState == desired)
            return NO_ERROR;
        if (desired == SERVICE_RUNNING && status.dwCurrentState == SERVICE_STOPPED)
            return status.dwWin32ExitCode ? status.dwWin32ExitCode : ERROR_SERVICE_NOT_ACTIVE;
        if (GetTickCount64() - started >= 30000)
            return ERROR_TIMEOUT;
        Sleep(100);
    }
}

int wsvcStart(const wsvcCfg *cfg)
{
    SC_HANDLE service = openService(cfg, SERVICE_START | SERVICE_QUERY_STATUS);
    DWORD code = NO_ERROR;
    if (!service)
        return -1;
    if (!StartServiceW(service, 0, NULL)) {
        code = GetLastError();
        if (code == ERROR_SERVICE_ALREADY_RUNNING)
            code = NO_ERROR;
    }
    if (!code)
        code = waitFor(service, SERVICE_RUNNING);
    return finish(service, code);
}

int wsvcStop(const wsvcCfg *cfg)
{
    SC_HANDLE service = openService(cfg, SERVICE_STOP | SERVICE_QUERY_STATUS);
    SERVICE_STATUS_PROCESS status;
    SERVICE_STATUS response;
    DWORD code;
    if (!service)
        return -1;
    code = query(service, &status);
    if (!code && status.dwCurrentState != SERVICE_STOPPED &&
        status.dwCurrentState != SERVICE_STOP_PENDING &&
        !ControlService(service, SERVICE_CONTROL_STOP, &response)) {
        code = GetLastError();
        if (code == ERROR_SERVICE_NOT_ACTIVE)
            code = NO_ERROR;
    }
    if (!code)
        code = waitFor(service, SERVICE_STOPPED);
    return finish(service, code);
}

int wsvcRemove(const wsvcCfg *cfg)
{
    SC_HANDLE service = openService(cfg, DELETE | SERVICE_QUERY_STATUS);
    SERVICE_STATUS_PROCESS status;
    DWORD code;
    if (!service)
        return -1;
    code = query(service, &status);
    if (!code && status.dwCurrentState != SERVICE_STOPPED)
        code = ERROR_SERVICE_CANNOT_ACCEPT_CTRL;
    if (!code && !DeleteService(service))
        code = GetLastError();
    return finish(service, code);
}

int wsvcQuery(const wsvcCfg *cfg, SERVICE_STATUS_PROCESS *status)
{
    SC_HANDLE service;
    DWORD code;
    if (!status)
        return fail(ERROR_INVALID_PARAMETER);
    service = openService(cfg, SERVICE_QUERY_STATUS);
    if (!service)
        return -1;
    code = query(service, status);
    return finish(service, code);
}

int wsvcStatus(const wsvcCfg *cfg)
{
    SERVICE_STATUS_PROCESS status;
    const wchar_t *name;
    if (wsvcQuery(cfg, &status) != 0)
        return -1;
    switch (status.dwCurrentState) {
    case SERVICE_STOPPED: name = L"stopped"; break;
    case SERVICE_START_PENDING: name = L"starting"; break;
    case SERVICE_STOP_PENDING: name = L"stopping"; break;
    case SERVICE_RUNNING: name = L"running"; break;
    case SERVICE_CONTINUE_PENDING: name = L"continuing"; break;
    case SERVICE_PAUSE_PENDING: name = L"pausing"; break;
    case SERVICE_PAUSED: name = L"paused"; break;
    default: name = L"unknown";
    }
    wprintf(L"%ls: %ls (PID %lu, exit %lu)\n", cfg->name, name,
        status.dwProcessId, status.dwWin32ExitCode);
    return 0;
}
