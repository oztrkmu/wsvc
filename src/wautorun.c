#include "wautorun.h"
#include <wchar.h>

static const wchar_t runKey[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";

static int result(LSTATUS code)
{
    if (code == ERROR_SUCCESS)
        return 0;
    SetLastError((DWORD)code);
    return -1;
}

static LSTATUS openRun(wautorunScope scope, const wchar_t *name,
    REGSAM access, int create, HKEY *key)
{
    HKEY root;
    if ((scope != WAUTORUN_USER && scope != WAUTORUN_MACHINE) || !name || !*name)
        return ERROR_INVALID_PARAMETER;
    root = scope == WAUTORUN_USER ? HKEY_CURRENT_USER : HKEY_LOCAL_MACHINE;
    access |= KEY_WOW64_64KEY;
    if (create)
        return RegCreateKeyExW(root, runKey, 0, NULL, 0, access, NULL, key, NULL);
    return RegOpenKeyExW(root, runKey, 0, access, key);
}

int wautorunSet(wautorunScope scope, const wchar_t *name, const wchar_t *command)
{
    HKEY key;
    LSTATUS code;
    size_t length;
    if (!command || !*command)
        return result(ERROR_INVALID_PARAMETER);
    length = wcslen(command);
    if (length >= WAUTORUN_COMMAND_CAPACITY)
        return result(ERROR_INVALID_PARAMETER);
    code = openRun(scope, name, KEY_SET_VALUE, 1, &key);
    if (code != ERROR_SUCCESS)
        return result(code);
    code = RegSetValueExW(key, name, 0, REG_SZ, (const BYTE *)command,
        (DWORD)((length + 1) * sizeof(*command)));
    RegCloseKey(key);
    return result(code);
}

int wautorunQuery(wautorunScope scope, const wchar_t *name,
    wchar_t command[WAUTORUN_COMMAND_CAPACITY])
{
    HKEY key;
    LSTATUS code;
    DWORD bytes = WAUTORUN_COMMAND_CAPACITY * sizeof(*command);
    if (!command)
        return result(ERROR_INVALID_PARAMETER);
    command[0] = L'\0';
    code = openRun(scope, name, KEY_QUERY_VALUE, 0, &key);
    if (code != ERROR_SUCCESS)
        return result(code);
    code = RegGetValueW(key, NULL, name, RRF_RT_REG_SZ | RRF_ZEROONFAILURE,
        NULL, command, &bytes);
    RegCloseKey(key);
    return result(code);
}

int wautorunRemove(wautorunScope scope, const wchar_t *name)
{
    HKEY key;
    LSTATUS code = openRun(scope, name, KEY_SET_VALUE, 0, &key);
    if (code == ERROR_FILE_NOT_FOUND)
        return 0;
    if (code != ERROR_SUCCESS)
        return result(code);
    code = RegDeleteValueW(key, name);
    RegCloseKey(key);
    return result(code == ERROR_FILE_NOT_FOUND ? ERROR_SUCCESS : code);
}
