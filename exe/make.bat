@echo off
if not "x%WDKDIR%" == "x" goto SELECT_EXE
echo The WDKDIR environment variable is not set
echo Set this variable to your WDK directory (without ending backslash)
echo Example: set WDKDIR C:\WinDDK\6001
pause
goto:eof

:SELECT_EXE
set PROJECT_DIR=%~dp0
set EXE_NAME=test_file

:BUILD_EXE_64
echo Building %EXE_NAME%.exe 64-bit (free) ...
set DDKBUILDENV=
echo on
copy ..\dll\objfre_wlh_amd64\amd64\test_file.lib .
call %WDKDIR%\bin\setenv.bat %WDKDIR%\ fre x64 WLH
cd %PROJECT_DIR%
build.exe -czgw
del buildfre_wlh_amd64.log
del test_file.lib
echo.

:BUILD_EXE_32
echo Building %EXE_NAME%.exe (32-bit) ...
set DDKBUILDENV=
copy ..\dll\objfre_w2k_x86\i386\test_file.lib .
call %WDKDIR%\bin\setenv.bat %WDKDIR%\ fre w2k
cd %PROJECT_DIR%
build.exe -czgw
del buildfre_w2k_x86.log
del test_file.lib
echo.

:CLEANUP
cd %PROJECT_DIR%
if exist build.bat del build.bat
