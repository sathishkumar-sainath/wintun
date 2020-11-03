/* SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (C) 2018-2020 WireGuard LLC. All Rights Reserved.
 */

#include "entry.h"

#if ACCEPT_WOW64 == 1

#include "adapter.h"
#include "logger.h"
#include "wintun.h"

#include <Windows.h>
#include <cfgmgr32.h>
#include <objbase.h>
#include <assert.h>

#define EXPORT comment(linker, "/EXPORT:" __FUNCTION__ "=" __FUNCDNAME__)

static DWORD
WriteFormatted(_In_ DWORD StdHandle, _In_z_ const WCHAR *Template, ...)
{
    WCHAR *FormattedMessage = NULL;
    DWORD SizeWritten;
    va_list Arguments;
    va_start(Arguments, Template);
    DWORD Len = FormatMessageW(
        FORMAT_MESSAGE_FROM_STRING | FORMAT_MESSAGE_ALLOCATE_BUFFER,
        Template,
        0,
        0,
        (void *)&FormattedMessage,
        0,
        &Arguments);
    WriteFile(GetStdHandle(StdHandle), FormattedMessage, Len * sizeof(WCHAR), &SizeWritten, NULL);
    LocalFree(FormattedMessage);
    va_end(Arguments);
    return SizeWritten / sizeof(WCHAR);
}

static BOOL CALLBACK
ConsoleLogger(_In_ WINTUN_LOGGER_LEVEL Level, _In_z_ const WCHAR *LogLine)
{
    const WCHAR *Template;
    switch (Level)
    {
    case WINTUN_LOG_INFO:
        Template = L"[+] %1\n";
        break;
    case WINTUN_LOG_WARN:
        Template = L"[-] %1\n";
        break;
    case WINTUN_LOG_ERR:
        Template = L"[!] %1\n";
        break;
    default:
        return FALSE;
    }
    WriteFormatted(STD_ERROR_HANDLE, Template, LogLine);
    return TRUE;
}

static int Argc;
static WCHAR **Argv;

static void
Init(void)
{
    WintunSetLogger(ConsoleLogger);
    Argv = CommandLineToArgvW(GetCommandLineW(), &Argc);
}

static void
Done(void)
{
    LocalFree(Argv);
}

VOID __stdcall CreateAdapter(HWND hwnd, HINSTANCE hinst, LPSTR lpszCmdLine, int nCmdShow)
{
#    pragma EXPORT
    UNREFERENCED_PARAMETER(hwnd);
    UNREFERENCED_PARAMETER(hinst);
    UNREFERENCED_PARAMETER(lpszCmdLine);
    UNREFERENCED_PARAMETER(nCmdShow);

    Init();
    if (Argc < 4)
        goto cleanup;
    if (wcslen(Argv[2]) >= WINTUN_MAX_POOL)
        goto cleanup;
    if (wcslen(Argv[3]) >= MAX_ADAPTER_NAME)
        goto cleanup;
    GUID RequestedGUID;
    if (Argc > 4 && FAILED(CLSIDFromString(Argv[4], &RequestedGUID)))
        goto cleanup;

    WINTUN_ADAPTER *Adapter;
    BOOL RebootRequired;
    DWORD Result = WintunCreateAdapter(Argv[2], Argv[3], Argc > 4 ? &RequestedGUID : NULL, &Adapter, &RebootRequired);
    WCHAR GuidStr[MAX_GUID_STRING_LEN];
    WriteFormatted(
        STD_OUTPUT_HANDLE,
        L"%1!X! %2!.*s! %3!X!",
        Result,
        StringFromGUID2(Result == ERROR_SUCCESS ? &Adapter->CfgInstanceID : &GUID_NULL, GuidStr, _countof(GuidStr)),
        GuidStr,
        RebootRequired);
    WintunFreeAdapter(Adapter);

cleanup:
    Done();
}

VOID __stdcall DeleteAdapter(HWND hwnd, HINSTANCE hinst, LPSTR lpszCmdLine, int nCmdShow)
{
#    pragma EXPORT
    UNREFERENCED_PARAMETER(hwnd);
    UNREFERENCED_PARAMETER(hinst);
    UNREFERENCED_PARAMETER(lpszCmdLine);
    UNREFERENCED_PARAMETER(nCmdShow);

    Init();
    if (Argc < 3)
        goto cleanup;

    WINTUN_ADAPTER Adapter = { 0 };
    BOOL ForceCloseSessions = wcstoul(Argv[2], NULL, 10);
    if (FAILED(CLSIDFromString(Argv[3], &Adapter.CfgInstanceID)))
        goto cleanup;
    BOOL RebootRequired;
    WINTUN_STATUS Ret = WintunDeleteAdapter(&Adapter, ForceCloseSessions, &RebootRequired);
    WriteFormatted(STD_OUTPUT_HANDLE, L"%1!X! %2!X!", Ret, RebootRequired);

cleanup:
    Done();
}

VOID __stdcall DeleteDriver(HWND hwnd, HINSTANCE hinst, LPSTR lpszCmdLine, int nCmdShow)
{
#    pragma EXPORT
    UNREFERENCED_PARAMETER(hwnd);
    UNREFERENCED_PARAMETER(hinst);
    UNREFERENCED_PARAMETER(lpszCmdLine);
    UNREFERENCED_PARAMETER(nCmdShow);

    Init();
    WriteFormatted(STD_OUTPUT_HANDLE, L"%1!X!", WintunDeleteDriver());
    Done();
}
#endif
