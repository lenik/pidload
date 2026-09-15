@echo off
REM SPDX-License-Identifier: AGPL-3.0-or-later
REM Windows entry for WiX: runs build.ps1 under PowerShell.
setlocal
cd /d "%~dp0"
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0build.ps1" %*
exit /b %ERRORLEVEL%
