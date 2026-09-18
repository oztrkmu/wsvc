#include "wsvc.h"
#include <stdio.h>

#define CHECK_INVALID(call) do { \
    SetLastError(NO_ERROR); \
    if ((call) != -1 || GetLastError() != ERROR_INVALID_PARAMETER) { \
        fprintf(stderr, "validation failed at line %d\n", __LINE__); return 1; \
    } \
} while (0)

int main(void)
{
    SERVICE_STATUS_PROCESS status;
    wsvcCfg cfg = {0};
    CHECK_INVALID(wsvcRun(NULL));
    CHECK_INVALID(wsvcInstall(NULL));
    CHECK_INVALID(wsvcStart(NULL));
    CHECK_INVALID(wsvcStop(NULL));
    CHECK_INVALID(wsvcRemove(NULL));
    CHECK_INVALID(wsvcQuery(NULL, &status));
    CHECK_INVALID(wsvcStatus(NULL));
    CHECK_INVALID(wsvcInstall(&cfg));
    cfg.name = L"invalid/name";
    CHECK_INVALID(wsvcStart(&cfg));
    cfg.name = L"wsvc-validation";
    CHECK_INVALID(wsvcRun(&cfg));
    CHECK_INVALID(wsvcQuery(&cfg, NULL));
    cfg.startup = (wsvcStartup)99;
    CHECK_INVALID(wsvcInstall(&cfg));
    return 0;
}
