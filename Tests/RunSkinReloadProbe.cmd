@echo off
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0RunSplitLifecycleProbe.ps1" -Probe SkinReloadProbe
exit /b %errorlevel%
