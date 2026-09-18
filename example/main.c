/* Personalize this configuration and worker for your application. */
#include "wsvc.h"
#include <stdio.h>
#include <wchar.h>

static DWORD worker(HANDLE stop)
{
    /* Put periodic work here; use absolute paths for all service files. */
    DWORD result;
    while ((result = WaitForSingleObject(stop, 1000)) == WAIT_TIMEOUT) {
        /* Application work. Keep each iteration short and interruptible. */
    }
    return result == WAIT_OBJECT_0 ? NO_ERROR : GetLastError();
}

static void usage(void)
{
    wprintf(L"wsvc example\n\n"
        L"Usage: wsvc-example.exe COMMAND\n"
        L"  --install [delayed|auto|manual]  Register this executable\n"
        L"  --start                         Start and wait (30s)\n"
        L"  --stop                          Stop and wait (30s)\n"
        L"  --status                        Show state, PID and exit code\n"
        L"  --remove                        Delete a stopped service\n"
        L"  --service                       SCM entry point\n"
        L"  --help                          Show help\n\n"
        L"Service: WsvcExample | Account: LocalService\n"
        L"Installation defaults to delayed automatic startup.\n");
}

int wmain(int argc, wchar_t **argv)
{
    wsvcCfg config = {
        L"WsvcExample", L"wsvc Example",
        L"Example Windows service using wsvc", worker,
        WSVC_START_DELAYED
    };
    int result;
    if (argc == 1 || (argc == 2 && !wcscmp(argv[1], L"--help"))) {
        usage();
        return 0;
    }
    if (!wcscmp(argv[1], L"--install") && (argc == 2 || argc == 3)) {
        if (argc == 3) {
            if (!wcscmp(argv[2], L"auto"))
                config.startup = WSVC_START_AUTO;
            else if (!wcscmp(argv[2], L"manual"))
                config.startup = WSVC_START_MANUAL;
            else if (wcscmp(argv[2], L"delayed"))
                goto invalid;
        }
        result = wsvcInstall(&config);
    } else if (argc != 2) {
        goto invalid;
    } else if (!wcscmp(argv[1], L"--service")) {
        result = wsvcRun(&config);
    } else if (!wcscmp(argv[1], L"--start")) {
        result = wsvcStart(&config);
    } else if (!wcscmp(argv[1], L"--stop")) {
        result = wsvcStop(&config);
    } else if (!wcscmp(argv[1], L"--remove")) {
        result = wsvcRemove(&config);
    } else if (!wcscmp(argv[1], L"--status")) {
        result = wsvcStatus(&config);
    } else {
        goto invalid;
    }
    if (result != 0) {
        DWORD code = GetLastError();
        wchar_t message[1024] = {0};
        FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            NULL, code, 0, message, 1024, NULL);
        fwprintf(stderr, L"wsvc: %ls failed (%lu): %ls\n", argv[1], code, message);
        return 1;
    }
    if (wcscmp(argv[1], L"--status") && wcscmp(argv[1], L"--service"))
        wprintf(L"wsvc: %ls completed\n", argv[1]);
    return 0;
invalid:
    fwprintf(stderr, L"wsvc: invalid arguments; use --help\n");
    return 2;
}
