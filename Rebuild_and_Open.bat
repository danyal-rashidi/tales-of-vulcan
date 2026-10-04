@echo off
setlocal EnableDelayedExpansion
title Tales of Vulcan - Rebuild and Open
rem Put this file in the tales-of-vulcan folder (next to TalesofVulcan.uproject) and double-click it.
rem It deletes the old Binaries/Intermediate build folders, rebuilds the C++ code, then opens Unreal.

set "PROJDIR=%~dp0"
set "UPROJECT=%PROJDIR%TalesofVulcan.uproject"

if not exist "%UPROJECT%" (
    echo [!] TalesofVulcan.uproject was not found next to this file.
    echo     Move Rebuild_and_Open.bat into the tales-of-vulcan folder and run it again.
    pause
    exit /b 1
)

echo === Step 1/4: Finding Unreal Engine ===
set "ENGINE="
rem The project is linked to an engine registered under this ID
for /f "tokens=2,*" %%a in ('reg query "HKCU\Software\Epic Games\Unreal Engine\Builds" /v "{AFE3B72E-43AD-9C16-C112-F1BF2B0B9160}" 2^>nul ^| findstr /i "REG_SZ"') do set "ENGINE=%%b"
rem Otherwise use the newest Epic Games Launcher install
if not defined ENGINE (
    for /d %%d in ("C:\Program Files\Epic Games\UE_5*") do set "ENGINE=%%d"
)
if not defined ENGINE (
    echo [!] Could not find Unreal Engine. Is it installed in C:\Program Files\Epic Games ?
    pause
    exit /b 1
)
if not exist "%ENGINE%\Engine\Build\BatchFiles\Build.bat" (
    echo [!] Found "%ENGINE%" but it does not look like an Unreal Engine folder.
    pause
    exit /b 1
)
echo     Using: %ENGINE%

echo.
echo === Step 2/4: Closing Unreal and deleting the old build ===
tasklist /fi "imagename eq UnrealEditor.exe" | find /i "UnrealEditor.exe" >nul && (
    echo     Unreal is open - please save your work and close it, then press a key.
    pause
)
if exist "%PROJDIR%Binaries" rmdir /s /q "%PROJDIR%Binaries"
if exist "%PROJDIR%Intermediate" rmdir /s /q "%PROJDIR%Intermediate"
echo     Done.

echo.
echo === Step 3/4: Rebuilding the game code (this takes a few minutes) ===
call "%ENGINE%\Engine\Build\BatchFiles\Build.bat" TalesofVulcanEditor Win64 Development -Project="%UPROJECT%" -WaitMutex -FromMsBuild > "%PROJDIR%build_log.txt" 2>&1
if errorlevel 1 (
    echo.
    echo [!] The build FAILED. The errors are below and saved in build_log.txt.
    echo     Send a screenshot of this window, or the build_log.txt file, to Claude.
    echo.
    findstr /i /c:"error" "%PROJDIR%build_log.txt"
    pause
    exit /b 1
)
echo     Build succeeded.

echo.
echo === Step 4/4: Opening Tales of Vulcan in Unreal ===
start "" "%ENGINE%\Engine\Binaries\Win64\UnrealEditor.exe" "%UPROJECT%"
echo     Unreal is starting. You can close this window.
timeout /t 5 >nul
exit /b 0
