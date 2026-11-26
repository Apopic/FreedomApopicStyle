@echo off
setlocal
for /f "usebackq tokens=*" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "FAS_VS=%%i"
if not defined FAS_VS exit /b 1
call "%FAS_VS%\Common7\Tools\VsDevCmd.bat" -no_logo -arch=x64 -host_arch=x64
if errorlevel 1 exit /b 1
cd /d "%~dp0"
if not exist build mkdir build
cl /nologo /std:c++20 /EHsc /utf-8 /O2 /W3 CoreTests.cpp /Febuild\CoreTests.exe /Fobuild\CoreTests.obj
if errorlevel 1 exit /b 1
build\CoreTests.exe
exit /b %errorlevel%
