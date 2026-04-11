@echo off
:: OyNIx Browser v2.3 - Windows Launcher
:: Finds Python and launches the browser without showing a console window
setlocal enabledelayedexpansion

title OyNIx Browser

:: Check for bundled Python first (PyInstaller/embedded)
if exist "%~dp0python\python.exe" (
    set PYTHON="%~dp0python\python.exe"
    goto :launch
)

:: Check for system Python
where python >nul 2>&1
if %ERRORLEVEL% EQU 0 (
    set PYTHON=python
    goto :launch
)

where python3 >nul 2>&1
if %ERRORLEVEL% EQU 0 (
    set PYTHON=python3
    goto :launch
)

:: Check common install locations
for %%P in (
    "%LOCALAPPDATA%\Programs\Python\Python312\python.exe"
    "%LOCALAPPDATA%\Programs\Python\Python311\python.exe"
    "%LOCALAPPDATA%\Programs\Python\Python310\python.exe"
    "C:\Python312\python.exe"
    "C:\Python311\python.exe"
    "C:\Python310\python.exe"
) do (
    if exist %%P (
        set PYTHON=%%P
        goto :launch
    )
)

echo ERROR: Python 3.10+ not found.
echo Install Python from https://python.org
pause
exit /b 1

:launch
:: Set PYTHONPATH to find oynix package
set PYTHONPATH=%~dp0;%PYTHONPATH%

:: Launch without console window using pythonw if available
set PYTHONW=%PYTHON:python.exe=pythonw.exe%
if exist "%PYTHONW%" (
    start "" %PYTHONW% -m oynix %*
) else (
    start "" %PYTHON% -m oynix %*
)
