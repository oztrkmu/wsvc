#include "wautorun.h"
#include <stdio.h>
#include <wchar.h>

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "autorun check failed at line %d (%lu)\n", \
            __LINE__, GetLastError()); \
        failed = 1; goto cleanup; \
    } \
} while (0)

int main(void)
{
    wchar_t name[80], command[WAUTORUN_COMMAND_CAPACITY];
    wchar_t tooLong[WAUTORUN_COMMAND_CAPACITY + 1];
    const wchar_t *first = L"\"C:\\Program Files\\wsvc test\\app.exe\" --background";
    const wchar_t *second = L"\"C:\\Program Files\\wsvc test\\app.exe\" --updated";
    int failed = 0;
    int created = 0;
    size_t i;
    swprintf(name, 80, L"wsvc-autorun-test-%lu-%llu", GetCurrentProcessId(),
        (unsigned long long)GetTickCount64());

    /* Never replace an existing entry, even in case of a test name collision. */
    if (wautorunQuery(WAUTORUN_USER, name, command) != -1 ||
        GetLastError() != ERROR_FILE_NOT_FOUND)
        return 1;

    CHECK(wautorunSet((wautorunScope)99, name, first) == -1 &&
        GetLastError() == ERROR_INVALID_PARAMETER);
    CHECK(wautorunSet(WAUTORUN_USER, L"", first) == -1 &&
        GetLastError() == ERROR_INVALID_PARAMETER);
    CHECK(wautorunSet(WAUTORUN_USER, name, NULL) == -1 &&
        GetLastError() == ERROR_INVALID_PARAMETER);
    CHECK(wautorunQuery(WAUTORUN_USER, name, NULL) == -1 &&
        GetLastError() == ERROR_INVALID_PARAMETER);
    CHECK(wautorunRemove((wautorunScope)99, name) == -1 &&
        GetLastError() == ERROR_INVALID_PARAMETER);
    for (i = 0; i < WAUTORUN_COMMAND_CAPACITY; ++i)
        tooLong[i] = L'x';
    tooLong[i] = L'\0';
    CHECK(wautorunSet(WAUTORUN_USER, name, tooLong) == -1 &&
        GetLastError() == ERROR_INVALID_PARAMETER);

    CHECK(wautorunSet(WAUTORUN_USER, name, first) == 0);
    created = 1;
    CHECK(wautorunQuery(WAUTORUN_USER, name, command) == 0);
    CHECK(wcscmp(command, first) == 0);
    CHECK(wautorunSet(WAUTORUN_USER, name, second) == 0);
    CHECK(wautorunQuery(WAUTORUN_USER, name, command) == 0);
    CHECK(wcscmp(command, second) == 0);
    CHECK(wautorunRemove(WAUTORUN_USER, name) == 0);
    CHECK(wautorunRemove(WAUTORUN_USER, name) == 0);
    CHECK(wautorunQuery(WAUTORUN_USER, name, command) == -1 &&
        GetLastError() == ERROR_FILE_NOT_FOUND);
cleanup:
    if (created && wautorunRemove(WAUTORUN_USER, name) != 0)
        failed = 1;
    return failed;
}
