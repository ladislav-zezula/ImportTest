#ifndef UNICODE
#define UNICODE
#define _UNICODE
#endif
#define _CRT_SECURE_NO_WARNINGS
#include <tchar.h>
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include <strsafe.h>
#include <objbase.h>

static HINSTANCE g_hInst = NULL;

// Retrieves the pointer to plain name and extension
template <typename XCHAR>
XCHAR * WINAPI GetPlainName(const XCHAR * szFileName)
{
    const XCHAR * szPlainName = szFileName;

    while(szFileName[0] != 0)
    {
        if(szFileName[0] == '\\' || szFileName[0] == '/')
            szPlainName = szFileName + 1;
        szFileName++;
    }

    return (XCHAR *)szPlainName;
}

HRESULT WINAPI TestExport()
{
    HANDLE hEvent;
    FILE * fp;
    WCHAR szDllName[MAX_PATH];

    // Open the log file and append the log message to the end
    if((fp = _tfopen(_T("test_file.log"), _T("at"))) != NULL)
    {
        GetModuleFileName(g_hInst, szDllName, _countof(szDllName));
        _ftprintf(fp, _T("Success: %s\n"), GetPlainName(szDllName));
        fclose(fp);
    }

    // Also open event and signal it
    hEvent = OpenEvent(EVENT_MODIFY_STATE, FALSE, _T("ImportTest_Succeeded"));
    if(hEvent != NULL)
    {
        SetEvent(hEvent);
        CloseHandle(hEvent);
    }

    return S_OK;
}

int WINAPI DllMain(HINSTANCE hDll, DWORD dwReason, LPVOID)
{
    if(dwReason == DLL_PROCESS_ATTACH)
        g_hInst = hDll;
    return TRUE;
}
