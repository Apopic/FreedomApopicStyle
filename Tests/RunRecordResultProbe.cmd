@echo off
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0RunSplitLifecycleProbe.ps1" -Probe RecordResultProbe
exit /b %errorlevel%
