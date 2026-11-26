@echo off
chcp 65001 > nul
setlocal
for /f "usebackq tokens=*" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "FAS_VS=%%i"
if not defined FAS_VS exit /b 1
call "%FAS_VS%\Common7\Tools\VsDevCmd.bat" -no_logo -arch=x64 -host_arch=x64
if errorlevel 1 exit /b 1
cd /d "%~dp0"
if not exist build mkdir build
cl /nologo /std:c++20 /EHsc /utf-8 /MT /O1 /Gy /bigobj /w /I"C:\DxLib_VC\プロジェクトに追加すべきファイル_VC用" /I"C:\Socket_h-main\include" /I"C:\magic_enum-v0.9.8\include" /I"C:\vcpkg\packages\zlib_x64-windows\include" /I"C:\libarrier-main\include" /I"C:\cppunzip-main" /I"C:\json\include" /I"C:\fast_float" SkinSwitchProbe.cpp /Febuild\SkinSwitchProbe.exe /Fobuild\SkinSwitchProbe.obj /link /SUBSYSTEM:CONSOLE /OPT:REF /LIBPATH:"C:\DxLib_VC\プロジェクトに追加すべきファイル_VC用" "C:\ICU\lib64\icuuc.lib" "C:\ICU\lib64\icudt.lib" "C:\ICU\lib64\icuin.lib"
if errorlevel 1 exit /b 1
build\SkinSwitchProbe.exe %*
exit /b %errorlevel%
