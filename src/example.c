#include <windows.h>
#include <wchar.h>

#include "wsvc.h"

static DWORD clientMain(HANDLE stop)
{
    while (WaitForSingleObject(stop, 1000) == WAIT_TIMEOUT) {
        /*
         * clientPoll();
         */
    }

    return 0;
}

static const wsvcCfg service = {
    .name    = L"MuratSupport",
    .display = L"Murat Support",
    .desc    = L"Remote support service",
    .run     = clientMain
};

int wmain(int argc, wchar_t **argv)
{
    if (argc != 2)
        return 1;

    if (!wcscmp(argv[1], L"--service"))
        return wsvcRun(&service);

    if (!wcscmp(argv[1], L"--install"))
        return wsvcInstall(&service);

    if (!wcscmp(argv[1], L"--remove"))
        return wsvcRemove(&service);

    if (!wcscmp(argv[1], L"--start"))
        return wsvcStart(&service);

    if (!wcscmp(argv[1], L"--stop"))
        return wsvcStop(&service);

    if (!wcscmp(argv[1], L"--status"))
        return wsvcStatus(&service);

    return 1;
}
