@echo off
if not "x%WDKDIR%" == "x" goto SELECT_DLL
echo The WDKDIR environment variable is not set
echo Set this variable to your WDK directory (without ending backslash)
echo Example: set WDKDIR C:\WinDDK\6001
pause
goto:eof

:SELECT_DLL
set PROJECT_DIR=%~dp0
set Dll_NAME=test_file

:BUILD_DLL_64
echo Building %DLL_NAME%.dll (64-bit) ...
set DDKBUILDENV=
call %WDKDIR%\bin\setenv.bat %WDKDIR%\ fre x64 WLH
cd %PROJECT_DIR%
build.exe -czgw
del buildfre_wlh_amd64.log
echo.

:BUILD_DLL_32
echo Building %DLL_NAME%.dll (32-bit) ...
set DDKBUILDENV=
call %WDKDIR%\bin\setenv.bat %WDKDIR%\ fre w2k
cd %PROJECT_DIR%
build.exe -czgw
del buildfre_w2k_x86.log
echo.

:CLEANUP
cd %PROJECT_DIR%
if exist build.bat del build.bat
