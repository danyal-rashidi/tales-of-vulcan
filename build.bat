@echo off
rem Compiles the game's C++ code. Run after pulling changes to Source\, with Unreal Editor closed.
cd /d "%~dp0"

tasklist /FI "IMAGENAME eq UnrealEditor.exe" | find /I "UnrealEditor.exe" >nul
if not errorlevel 1 (
    echo Unreal Editor is still open. Close it and run this again.
    pause
    exit /b 1
)

call "C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat" TalesofVulcanEditor Win64 Development -Project="%~dp0TalesofVulcan.uproject" -WaitMutex
if errorlevel 1 (
    echo.
    echo Build FAILED. Scroll up for the error.
) else (
    echo.
    echo Build succeeded. You can open Unreal now.
)
pause
