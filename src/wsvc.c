/*
 * wsvc - minimal windows service helper
 * author: Murat / oztrkmu
 */

#include "wsvc.h"

#include <stdio.h>
#include <wchar.h>

static SERVICE_STATUS_HANDLE svcHandle;
static SERVICE_STATUS svcState;

static HANDLE stopEvent;
static wsvcCfg svcCfg;

static void err(const wchar_t *msg)
{
    fwprintf(stderr, L"wsvc: %ls: %lu\n", msg, GetLastError());
}

static void state(DWORD value, DWORD code, DWORD wait)
{
    svcState.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    svcState.dwCurrentState = value;
    svcState.dwWin32ExitCode = code;
    svcState.dwWaitHint = wait;

    svcState.dwControlsAccepted =
        value == SERVICE_RUNNING
        ? SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN
        : 0;

    SetServiceStatus(svcHandle, &svcState);
}

static DWORD WINAPI control(
    DWORD code,
    DWORD type,
    LPVOID data,
    LPVOID ctx)
{
    (void)type;
    (void)data;
    (void)ctx;

    switch (code) {
    case SERVICE_CONTROL_STOP:
    case SERVICE_CONTROL_SHUTDOWN:

        if (svcState.dwCurrentState != SERVICE_RUNNING)
            return NO_ERROR;

        state(SERVICE_STOP_PENDING, 0, 3000);

        if (stopEvent)
            SetEvent(stopEvent);

        break;
    }

    return NO_ERROR;
}

static void WINAPI serviceMain(DWORD argc, LPWSTR *argv)
{
    DWORD rc;

    (void)argc;
    (void)argv;

    svcHandle = RegisterServiceCtrlHandlerExW(
        svcCfg.name,
        control,
        NULL
    );

    if (!svcHandle)
        return;

    ZeroMemory(&svcState, sizeof(svcState));

    state(SERVICE_START_PENDING, 0, 3000);

    stopEvent = CreateEventW(
        NULL,
        TRUE,
        FALSE,
        NULL
    );

    if (!stopEvent) {
        rc = GetLastError();
        state(SERVICE_STOPPED, rc, 0);
        return;
    }

    state(SERVICE_RUNNING, 0, 0);

    rc = svcCfg.run(stopEvent);

    CloseHandle(stopEvent);
    stopEvent = NULL;

    state(SERVICE_STOPPED, rc, 0);
}

int wsvcRun(const wsvcCfg *cfg)
{
    SERVICE_TABLE_ENTRYW table[2];

    if (!cfg || !cfg->name || !cfg->run)
        return -1;

    svcCfg = *cfg;

    table[0].lpServiceName = (LPWSTR)cfg->name;
    table[0].lpServiceProc = serviceMain;

    table[1].lpServiceName = NULL;
    table[1].lpServiceProc = NULL;

    if (!StartServiceCtrlDispatcherW(table)) {
        err(L"dispatcher");
        return -1;
    }

    return 0;
}

int wsvcInstall(const wsvcCfg *cfg)
{
    WCHAR exe[MAX_PATH];
    WCHAR cmd[MAX_PATH + 32];

    SC_HANDLE scm;
    SC_HANDLE svc;

    SERVICE_DESCRIPTIONW desc;
    SERVICE_DELAYED_AUTO_START_INFO delay;

    SC_ACTION action[3];
    SERVICE_FAILURE_ACTIONS failure;

    DWORD n;

    if (!cfg || !cfg->name)
        return -1;

    n = GetModuleFileNameW(NULL, exe, MAX_PATH);

    if (!n || n >= MAX_PATH) {
        err(L"path");
        return -1;
    }

    if (swprintf(
            cmd,
            MAX_PATH + 32,
            L"\"%ls\" --service",
            exe) < 0)
        return -1;

    scm = OpenSCManagerW(
        NULL,
        NULL,
        SC_MANAGER_CREATE_SERVICE
    );

    if (!scm) {
        err(L"scm");
        return -1;
    }

    svc = CreateServiceW(
        scm,
        cfg->name,
        cfg->display ? cfg->display : cfg->name,

        SERVICE_CHANGE_CONFIG |
        SERVICE_QUERY_STATUS |
        SERVICE_START |
        SERVICE_STOP |
        DELETE,

        SERVICE_WIN32_OWN_PROCESS,
        SERVICE_AUTO_START,
        SERVICE_ERROR_NORMAL,

        cmd,

        NULL,
        NULL,
        NULL,

        L"NT AUTHORITY\\LocalService",
        NULL
    );

    if (!svc) {
        err(L"create");
        CloseServiceHandle(scm);
        return -1;
    }

    if (cfg->desc) {
        desc.lpDescription = (LPWSTR)cfg->desc;

        if (!ChangeServiceConfig2W(
                svc,
                SERVICE_CONFIG_DESCRIPTION,
                &desc))
            err(L"desc");
    }

    delay.fDelayedAutostart = TRUE;

    if (!ChangeServiceConfig2W(
            svc,
            SERVICE_CONFIG_DELAYED_AUTO_START_INFO,
            &delay))
        err(L"delay");

    action[0].Type = SC_ACTION_RESTART;
    action[0].Delay = 5000;

    action[1].Type = SC_ACTION_RESTART;
    action[1].Delay = 15000;

    action[2].Type = SC_ACTION_RESTART;
    action[2].Delay = 60000;

    ZeroMemory(&failure, sizeof(failure));

    failure.dwResetPeriod = 86400;
    failure.cActions = 3;
    failure.lpsaActions = action;

    if (!ChangeServiceConfig2W(
            svc,
            SERVICE_CONFIG_FAILURE_ACTIONS,
            &failure))
        err(L"recovery");

    wprintf(L"wsvc: installed\n");

    CloseServiceHandle(svc);
    CloseServiceHandle(scm);

    return 0;
}

int wsvcStart(const wsvcCfg *cfg)
{
    SC_HANDLE scm;
    SC_HANDLE svc;

    scm = OpenSCManagerW(
        NULL,
        NULL,
        SC_MANAGER_CONNECT
    );

    if (!scm) {
        err(L"scm");
        return -1;
    }

    svc = OpenServiceW(
        scm,
        cfg->name,
        SERVICE_START
    );

    if (!svc) {
        err(L"open");
        CloseServiceHandle(scm);
        return -1;
    }

    if (!StartServiceW(svc, 0, NULL)) {
        DWORD e = GetLastError();

        if (e != ERROR_SERVICE_ALREADY_RUNNING) {
            SetLastError(e);
            err(L"start");

            CloseServiceHandle(svc);
            CloseServiceHandle(scm);

            return -1;
        }
    }

    wprintf(L"wsvc: started\n");

    CloseServiceHandle(svc);
    CloseServiceHandle(scm);

    return 0;
}

int wsvcStop(const wsvcCfg *cfg)
{
    SC_HANDLE scm;
    SC_HANDLE svc;

    SERVICE_STATUS state;

    scm = OpenSCManagerW(
        NULL,
        NULL,
        SC_MANAGER_CONNECT
    );

    if (!scm) {
        err(L"scm");
        return -1;
    }

    svc = OpenServiceW(
        scm,
        cfg->name,
        SERVICE_STOP
    );

    if (!svc) {
        err(L"open");
        CloseServiceHandle(scm);
        return -1;
    }

    if (!ControlService(
            svc,
            SERVICE_CONTROL_STOP,
            &state)) {

        DWORD e = GetLastError();

        if (e != ERROR_SERVICE_NOT_ACTIVE) {
            SetLastError(e);
            err(L"stop");

            CloseServiceHandle(svc);
            CloseServiceHandle(scm);

            return -1;
        }
    }

    wprintf(L"wsvc: stopped\n");

    CloseServiceHandle(svc);
    CloseServiceHandle(scm);

    return 0;
}

int wsvcRemove(const wsvcCfg *cfg)
{
    SC_HANDLE scm;
    SC_HANDLE svc;

    scm = OpenSCManagerW(
        NULL,
        NULL,
        SC_MANAGER_CONNECT
    );

    if (!scm) {
        err(L"scm");
        return -1;
    }

    svc = OpenServiceW(
        scm,
        cfg->name,
        DELETE
    );

    if (!svc) {
        err(L"open");
        CloseServiceHandle(scm);
        return -1;
    }

    if (!DeleteService(svc)) {
        err(L"remove");

        CloseServiceHandle(svc);
        CloseServiceHandle(scm);

        return -1;
    }

    wprintf(L"wsvc: removed\n");

    CloseServiceHandle(svc);
    CloseServiceHandle(scm);

    return 0;
}

int wsvcStatus(const wsvcCfg *cfg)
{
    SC_HANDLE scm;
    SC_HANDLE svc;

    SERVICE_STATUS_PROCESS st;
    DWORD need;

    scm = OpenSCManagerW(
        NULL,
        NULL,
        SC_MANAGER_CONNECT
    );

    if (!scm) {
        err(L"scm");
        return -1;
    }

    svc = OpenServiceW(
        scm,
        cfg->name,
        SERVICE_QUERY_STATUS
    );

    if (!svc) {
        err(L"open");
        CloseServiceHandle(scm);
        return -1;
    }

    if (!QueryServiceStatusEx(
            svc,
            SC_STATUS_PROCESS_INFO,
            (LPBYTE)&st,
            sizeof(st),
            &need)) {

        err(L"status");

        CloseServiceHandle(svc);
        CloseServiceHandle(scm);

        return -1;
    }

    switch (st.dwCurrentState) {
    case SERVICE_RUNNING:
        wprintf(L"wsvc: running\n");
        break;

    case SERVICE_STOPPED:
        wprintf(L"wsvc: stopped\n");
        break;

    case SERVICE_START_PENDING:
        wprintf(L"wsvc: starting\n");
        break;

    case SERVICE_STOP_PENDING:
        wprintf(L"wsvc: stopping\n");
        break;

    default:
        wprintf(
            L"wsvc: state: %lu\n",
            st.dwCurrentState
        );
    }

    CloseServiceHandle(svc);
    CloseServiceHandle(scm);

    return 0;
}
