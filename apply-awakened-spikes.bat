@echo off
rem Puts the awakened-spikes branch onto main and removes the temporary build copy.
rem Old local main stays saved as backup/local-main-before-v6.
cd /d "%~dp0"

tasklist /FI "IMAGENAME eq UnrealEditor.exe" | find /I "UnrealEditor.exe" >nul
if not errorlevel 1 (
    echo Unreal Editor is still open. Close it and run this again.
    pause
    exit /b 1
)

echo This resets main to the awakened-spikes branch. Uncommitted changes in this folder will be lost.
pause

git cherry-pick --quit 2>nul
git checkout main || goto :fail
git reset --hard awakened-spikes || goto :fail

if exist "..\tov-awakened" git worktree remove --force ..\tov-awakened

echo.
git log --oneline -1
echo Done.
pause
exit /b 0

:fail
echo Something went wrong. Nothing was pushed; your old main is on backup/local-main-before-v6.
pause
exit /b 1
