# wsvc

A small C library for Windows services. Install, start, stop, query and remove
a service, or run your own worker with a stop event.

## Build

On Windows, with CMake and the Visual Studio C tools:

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

On Linux, with the MinGW-w64 cross-compiler:

```sh
make
```

Both builds produce a static library and `wsvc-example.exe` under `build/`.
The Visual Studio build puts them in `build/Release/`. Running them requires Windows.

## Try the example

Place the executable in a permanent folder that LocalService can read and execute.
Run these commands from that folder in an administrator PowerShell:

```powershell
.\wsvc-example.exe --install manual
.\wsvc-example.exe --start
.\wsvc-example.exe --status
.\wsvc-example.exe --stop
.\wsvc-example.exe --remove
```

The example registers as `WsvcExample` and waits for a stop request.
`--install` also accepts `auto` or `delayed` (the default).
Use `--help` for all commands.

## Use in your application

Add `src/wsvc.c` and `include/wsvc.h` to your project and link `advapi32`,
or link the CMake `wsvc` target. Adapt [example/main.c](example/main.c):

- Set the service name, display name and description in `wsvcCfg`.
- Put your work in the callback. Return promptly when its stop event is signaled.
- Handle `--service` by calling `wsvcRun`; Windows uses this argument to launch it.

The API returns `0` on success and `-1` on failure; read `GetLastError()` immediately
after a failure. `wsvcQuery` returns service details; `wsvcStatus` prints them.
Start and stop wait up to 30 seconds. Stop the service before removing it.

Services run as LocalService. Use absolute file paths and grant that account any
required access. Only one service can run per process.

## Start an application at logon

Use [wautorun.h](include/wautorun.h) and [wautorun.c](src/wautorun.c) independently
with `advapi32`, or link the existing `wsvc` library.

```c
#include "wautorun.h"

/* Check each return value; GetLastError() describes failures. */
wautorunSet(WAUTORUN_USER, L"MyApp",
    L"\"C:\\Program Files\\MyApp\\app.exe\" --background");

wchar_t command[WAUTORUN_COMMAND_CAPACITY];
wautorunQuery(WAUTORUN_USER, L"MyApp", command);
wautorunRemove(WAUTORUN_USER, L"MyApp");
```

- `WAUTORUN_USER`: current user's `HKCU` Run entry; no elevation needed.
- `WAUTORUN_MACHINE`: all users' `HKLM` Run entry; run the installer elevated.

Choose the scope explicitly and use the same scope to query or remove it.
Set replaces an entry with the same name. Remove also succeeds if it is absent.
Commands are limited to 260 characters; quote executable paths containing spaces.
On 64-bit Windows, both builds use the 64-bit registry view.

Run entries start applications at **logon**, with the logged-in user's rights.
They do not grant administrator rights. For startup before logon, use the service
API. Windows startup settings or policy may disable or delay Run entries.
See Microsoft's [Run key documentation](https://learn.microsoft.com/en-us/windows/win32/setupapi/run-and-runonce-registry-keys).

## Tests

GitHub Actions builds with MSVC and MinGW and runs API, CLI and service lifecycle
tests on Windows. To run the lifecycle test locally, use an administrator
PowerShell in a disposable Windows environment:

```powershell
.\tests\service.ps1 -Executable .\build\Release\wsvc-example.exe
```

Author: Murat / oztrkmu. No license has been selected yet.
