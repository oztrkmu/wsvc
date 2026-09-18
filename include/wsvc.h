/* wsvc — Windows service helpers by Murat / oztrkmu. */
#ifndef WSVC_H
#define WSVC_H

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif
#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Zero initialization selects delayed automatic startup. */
typedef enum wsvcStartup {
    WSVC_START_DELAYED = 0,
    WSVC_START_AUTO,
    WSVC_START_MANUAL
} wsvcStartup;

typedef struct wsvcCfg {
    const wchar_t *name;
    const wchar_t *display;
    const wchar_t *desc;
    /* Must return promptly when stop is signaled. Return a Win32 error code. */
    DWORD (*run)(HANDLE stop);
    wsvcStartup startup;
} wsvcCfg;

/* Return 0 on success, -1 on failure (GetLastError gives the Win32 error).
 * Start/stop wait up to 30 seconds. Remove requires a stopped service.
 * Installation uses this executable with --service, under LocalService.
 * Run supports one service per process; do not call it concurrently.
 */
int wsvcRun(const wsvcCfg *cfg);
int wsvcInstall(const wsvcCfg *cfg);
int wsvcStart(const wsvcCfg *cfg);
int wsvcStop(const wsvcCfg *cfg);
int wsvcRemove(const wsvcCfg *cfg);
int wsvcQuery(const wsvcCfg *cfg, SERVICE_STATUS_PROCESS *status);
int wsvcStatus(const wsvcCfg *cfg);

#ifdef __cplusplus
}
#endif
#endif
