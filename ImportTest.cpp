#include <tchar.h>
#include <conio.h>
#include <windows.h>
#include <strsafe.h>

typedef struct _IMAGE_MAP
{
    PIMAGE_DOS_HEADER pDosHdr;
    PIMAGE_FILE_HEADER pFileHdr;
    PIMAGE_SECTION_HEADER pSections;
    LPBYTE pbFilePtr;
    LPBYTE pbFileEnd;
} IMAGE_MAP, *PIMAGE_MAP;

static LPCTSTR szDllName = _T("test_file.dll");
static LPCSTR szDllNameA = "test_file.dll";
static LPCTSTR szExeName = _T("test_file.exe");
static LPCTSTR szExeName2 = _T("test_file_temp.exe");

int WINAPI ForcePathExist(LPCTSTR szFileName, BOOL bIsDirectory);

static CHAR GetPrintable(CHAR ch)
{
    return (ch >= 0x20) ? ch : ' ';
}

static LPBYTE GetFileOffset(IMAGE_MAP & ImageMap, ULONG Rva, ULONG Size)
{
    PIMAGE_SECTION_HEADER pSection = ImageMap.pSections;
    LPBYTE pbFileOffset;
    ULONG SectionDistance;
    WORD NumberOfSections = ImageMap.pFileHdr->NumberOfSections;

    // Find the raw offset of the RVA
    for(WORD i = 0; i < NumberOfSections; i++, pSection++)
    {
        if(pSection->VirtualAddress <= Rva && Rva < pSection->VirtualAddress + pSection->Misc.VirtualSize)
        {
            // Get the pointer in the file
            SectionDistance = (Rva - pSection->VirtualAddress);
            pbFileOffset = ImageMap.pbFilePtr + pSection->PointerToRawData + SectionDistance;

            // Check ranges
            if((pSection->PointerToRawData + SectionDistance + Size) <= (pSection->PointerToRawData + pSection->SizeOfRawData))
            {
                if((pbFileOffset + Size) <= ImageMap.pbFileEnd)
                {
                    return pbFileOffset;
                }
            }
        }
    }

    return NULL;
}

static DWORD PatchImportDirectory(LPBYTE pbFile, DWORD cbFile, LPCSTR szOldImportName, LPCSTR szNewImportName)
{
    PIMAGE_DATA_DIRECTORY pDataDirs;
    PIMAGE_DOS_HEADER pDosHdr;
    IMAGE_MAP ImageMap = {0};
    DWORD NumberOfRvaAndSizes = 0;
    DWORD dwErrCode = ERROR_SUCCESS;

    // Get headers
    pDosHdr = (PIMAGE_DOS_HEADER)pbFile;
    if(pDosHdr->e_magic != IMAGE_DOS_SIGNATURE || pDosHdr->e_lfanew == 0)
        return ERROR_BAD_FORMAT;

    ImageMap.pDosHdr = pDosHdr;
    ImageMap.pFileHdr = (PIMAGE_FILE_HEADER)(pbFile + pDosHdr->e_lfanew + sizeof(DWORD));
    ImageMap.pSections = (PIMAGE_SECTION_HEADER)(pbFile + pDosHdr->e_lfanew + sizeof(DWORD) + sizeof(IMAGE_FILE_HEADER) + ImageMap.pFileHdr->SizeOfOptionalHeader);
    ImageMap.pbFilePtr = pbFile;
    ImageMap.pbFileEnd = pbFile + cbFile;

    // Get data directory array
    if(ImageMap.pFileHdr->Machine == IMAGE_FILE_MACHINE_I386)
    {
        PIMAGE_NT_HEADERS32 pNtHdrs = (PIMAGE_NT_HEADERS32)(pbFile + pDosHdr->e_lfanew);
        
        NumberOfRvaAndSizes = pNtHdrs->OptionalHeader.NumberOfRvaAndSizes;
        pDataDirs = pNtHdrs->OptionalHeader.DataDirectory;
    }
    else
    {
        PIMAGE_NT_HEADERS64 pNtHdrs = (PIMAGE_NT_HEADERS64)(pbFile + pDosHdr->e_lfanew);

        NumberOfRvaAndSizes = pNtHdrs->OptionalHeader.NumberOfRvaAndSizes;
        pDataDirs = pNtHdrs->OptionalHeader.DataDirectory;
    }

    // Find the import and patch it
    if(NumberOfRvaAndSizes >= IMAGE_DIRECTORY_ENTRY_IMPORT)
    {
        PIMAGE_IMPORT_DESCRIPTOR pImport;
        LPSTR szImportName;
        ULONG ImportRva = pDataDirs[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress;

        for(;;)
        {
            // Get the pointer to the import directory
            pImport = (PIMAGE_IMPORT_DESCRIPTOR)GetFileOffset(ImageMap, ImportRva, sizeof(IMAGE_IMPORT_DESCRIPTOR));
            if(pImport == NULL || pImport->Name == 0)
                break;

            // Get the pointer to the name
            szImportName = (LPSTR)GetFileOffset(ImageMap, pImport->Name, 16);
            if(szImportName == NULL)
                break;

            // Check the import and patch it, if it matches
            if(!_stricmp(szImportName, szOldImportName))
            {
                StringCchCopyA(szImportName, 16, szNewImportName);
                return ERROR_SUCCESS;
            }

            // Move to tne next import
            ImportRva += sizeof(IMAGE_IMPORT_DESCRIPTOR);
        }

        dwErrCode = ERROR_BAD_FORMAT;
    }
    else
        dwErrCode = ERROR_FILE_NOT_FOUND;

    return dwErrCode;
}

static DWORD PrepareExe(LPCTSTR szSourceName, LPCTSTR szTargetName, LPCSTR szOldImportName, LPCSTR szNewImportName)
{
    ULARGE_INTEGER FileSize = {0};
    LPBYTE pbFile;
    HANDLE hFile;
    HANDLE hMap;
    DWORD dwErrCode = ERROR_SUCCESS;

    // Make a copy of the EXE
    if(CopyFile(szSourceName, szTargetName, FALSE))
    {
        // Open the EXE
        hFile = CreateFile(szTargetName, GENERIC_ALL, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
        if(hFile != INVALID_HANDLE_VALUE)
        {
            // We need the file size
            FileSize.LowPart = GetFileSize(hFile, &FileSize.HighPart);

            // Map the file tp memory
            hMap = CreateFileMapping(hFile, NULL, PAGE_READWRITE, 0, 0, NULL);
            if(hMap != NULL)
            {
                pbFile = (LPBYTE)MapViewOfFile(hMap, FILE_MAP_READ | FILE_MAP_WRITE, 0, 0, 0);
                if(pbFile != NULL)
                {
                    // Patch the import
                    dwErrCode = PatchImportDirectory(pbFile, FileSize.LowPart, szOldImportName, szNewImportName);
                    UnmapViewOfFile(pbFile);
                }
                else
                    dwErrCode = GetLastError();

                CloseHandle(hMap);
            }
            else
                dwErrCode = GetLastError();

            CloseHandle(hFile);
        }
        else
            dwErrCode = GetLastError();
    }
    else
        dwErrCode = GetLastError();

    return dwErrCode;
}

int __cdecl _tmain()
{
    TCHAR szFullPathT[MAX_PATH];
    CHAR szFullPathA[MAX_PATH];
    DWORD dwErrCode;

    // Verify if "test_file.dll" exists
    if(GetFileAttributes(szDllName) & FILE_ATTRIBUTE_DIRECTORY)
    {
        _tprintf(_T("The DLL file (%s) not found.\n"), szDllName);
        return 3;
    }

    // Verify if "test_file.exe" exists
    if(GetFileAttributes(szExeName) & FILE_ATTRIBUTE_DIRECTORY)
    {
        _tprintf(_T("The EXE file (%s) not found.\n"), szExeName);
        return 3;
    }

    // Perform the synthetic test
    for(unsigned char ch = 1; ch != 0; ch++)
    {
        PROCESS_INFORMATION pi = {0};
        STARTUPINFO si = {sizeof(STARTUPINFO)};
        HANDLE hEvent;

        // Notify the user about testing
        printf(" * 0x%02x (%c) ... ", (ch & 0xFF), GetPrintable(ch));

        // Prepare the name of the DLL. We also need to prepare the Unicode version of the name,
        // because that's what the LoadLibraryA will do
        StringCchCopyA(szFullPathA, _countof(szFullPathA), szDllNameA);
        szFullPathA[4] = ch;
        MultiByteToWideChar(CP_ACP, 0, szFullPathA, -1, szFullPathT, _countof(szFullPathT));

        // Make sure that the DLL is in that path
        ForcePathExist(szFullPathT, FALSE);

        // Only copy the DLL if the name is actually different
        if(_tcscmp(szDllName, szFullPathT))
        {
            if(!CopyFile(szDllName, szFullPathT, FALSE))
            {
                _tprintf(_T("[Failed to create DLL]\n"));
                continue;
            }
        }

        // Prepare the EXE file so that it has that import
        dwErrCode = PrepareExe(szExeName, szExeName2, szDllNameA, szFullPathA);
        if(dwErrCode != ERROR_SUCCESS)
        {
            _tprintf(_T("[Failed to prepare EXE]\n"));
            continue;
        }

        // Create the event. If the process runs successfully, it will set it
        hEvent = CreateEvent(NULL, TRUE, FALSE, _T("ImportTest_Succeeded"));
        if(hEvent == NULL)
        {
            _tprintf(_T("[Failed to create event]\n"));
            continue;
        }

        // Try to run the process
        StringCchCopy(szFullPathT, _countof(szFullPathT), szExeName2);
        if(!CreateProcess(NULL, szFullPathT, NULL, NULL, FALSE, NULL, NULL, NULL, &si, &pi))
        {
            _tprintf(_T("[Failed to run EXE]\n"));
            continue;
        }

        // Check whether the event is set
        if(WaitForSingleObject(hEvent, 10000) == WAIT_TIMEOUT)
            _tprintf(_T("Timeout\n"));
        else
            _tprintf(_T("OK\n"));

        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        CloseHandle(hEvent);
    }

    printf("Test complete. Press any key to exit ...\n");
    _getch();
}


//-----------------------------------------------------------------------------
// ForcePathExist

// Skips "\\Server\Share"
static LPTSTR SkipServerAndShare(LPTSTR szPath)
{
    // Skip the one between server and share
    szPath = _tcschr(szPath + 2, _T('\\'));
    if(szPath == NULL)
        return NULL;

    // Skip the one after share name
    szPath = _tcschr(szPath + 1, _T('\\'));
    if(szPath == NULL)
        return NULL;

    return szPath + 1;
}

// Skips ".\DirName" and "..\DirName"
static LPTSTR SkipDotPart(LPTSTR szPath)
{
    // Skip dots
    while(szPath[0] == _T('.'))
        szPath++;

    // Skip the slash or backslash
    if(szPath[0] == _T('/') || szPath[0] == _T('\\'))
        szPath++;

    return szPath;
}

// Ensures that the path for the extracted file already exists
// or creates it.
int WINAPI ForcePathExist(LPCTSTR szFileName, BOOL bIsDirectory)
{
    LPTSTR szNextPart;
    LPTSTR szWorkPath;
    size_t cchLength;
    int  nError = ERROR_SUCCESS;

    // Sanity checks
    if(szFileName == NULL || szFileName[0] == 0)
        return ERROR_SUCCESS;

    // Create copy of the path. Reserve space for "\\.\"
    cchLength = _tcslen(szFileName) + 4 + 1;
    szWorkPath = new TCHAR[cchLength];
    if(szWorkPath == NULL)
        return ERROR_NOT_ENOUGH_MEMORY;

    // Copy the path to the work buffer
    StringCchCopy(szWorkPath, cchLength, szFileName);

    // Check for full paths like "\\\\?\\"
    if(!_tcsncmp(szWorkPath, _T("\\\\?\\"), 4))
    {
        szNextPart = szWorkPath + 6;
    }

    // Check for full paths like "\\\\.\\"
    else if(!_tcsncmp(szWorkPath, _T("\\\\.\\"), 4))
    {
        szNextPart = szWorkPath + 6;
    }

    // Check for network paths "\\Server\Share\Dir\File.ext:
    else if(szWorkPath[0] == _T('\\') && szWorkPath[1] == _T('\\'))
    {
        szNextPart = SkipServerAndShare(szWorkPath);
        if(szNextPart == NULL)
            return ERROR_INVALID_NETNAME;
    }

    // Check for the DOS-style full paths
    else if(szWorkPath[1] == _T(':') && szWorkPath[2] == _T('\\'))
    {
#ifdef _UNICODE
        StringCchCopy(szWorkPath, cchLength, _T("\\\\?\\"));
        StringCchCopy(szWorkPath + 4, cchLength - 4, szFileName);
        szNextPart = szWorkPath + 6;
#else
        szNextPart = szWorkPath + 2;
#endif
    }

    // Check for relative paths like "..\Dir" or ".\Dir"
    else if(szWorkPath[0] == _T('.'))
    {
        szNextPart = SkipDotPart(szWorkPath + 1);
    }

    // Default - use as-is
    else
    {
        szNextPart = szWorkPath;
    }

    // If there is no next part, fail it
    while((szNextPart = (LPTSTR)_tcspbrk(szNextPart + 1, _T("\\/"))) != NULL)
    {
        // Cut that part
        szNextPart[0] = 0;

        // Create the directory
        if(!CreateDirectory(szWorkPath, NULL))
        {
            if((nError = GetLastError()) != ERROR_ALREADY_EXISTS)
                break;
            nError = ERROR_SUCCESS;
        }

        // Restore the path
        szNextPart[0] = _T('\\');
    }

    // If it is a directory, force create the last part
    if(nError == ERROR_SUCCESS && bIsDirectory && *szFileName != 0)
    {
        if(!CreateDirectory(szWorkPath, NULL))
        {
            if((nError = GetLastError()) == ERROR_ALREADY_EXISTS)
                nError = ERROR_SUCCESS;
        }
    }

    delete[] szWorkPath;
    return nError;
}
