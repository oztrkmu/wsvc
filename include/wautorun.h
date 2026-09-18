/* Registry Run entries, independent of the service API. */
#ifndef WAUTORUN_H
#define WAUTORUN_H

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif
#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum wautorunScope {
    WAUTORUN_USER = 0, /* Current user; no administrator rights needed. */
    WAUTORUN_MACHINE   /* All users; requires an elevated process. */
} wautorunScope;

#define WAUTORUN_COMMAND_CAPACITY 261

/* Return 0 on success, -1 on failure; use GetLastError() immediately.
 * Set adds or replaces the named entry. Supply an absolute executable path,
 * quoted if needed, followed by arguments (at most 260 characters total).
 * Query needs a WAUTORUN_COMMAND_CAPACITY wchar_t buffer; a missing entry
 * returns ERROR_FILE_NOT_FOUND. Remove succeeds if the entry is absent.
 * Both scopes use the 64-bit registry view on 64-bit Windows.
 */
int wautorunSet(wautorunScope scope, const wchar_t *name, const wchar_t *command);
int wautorunQuery(wautorunScope scope, const wchar_t *name,
    wchar_t command[WAUTORUN_COMMAND_CAPACITY]);
int wautorunRemove(wautorunScope scope, const wchar_t *name);

#ifdef __cplusplus
}
#endif
#endif
