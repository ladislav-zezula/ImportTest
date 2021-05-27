#include <tchar.h>
#include <conio.h>
#include <windows.h>
#include <strsafe.h>

#include "resource.h"

__declspec(dllimport) HRESULT WINAPI TestExport();
#pragma comment(lib, "test_file.lib")

int __cdecl _tmain()
{
    TestExport();
}
