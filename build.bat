@echo off
rem ============================================================
rem  DaBa build script
rem   - MSVC : run from "x64 Native Tools Command Prompt for VS"
rem   - MinGW: gcc / windres (MSYS2 mingw64 or w64devkit) in PATH
rem ============================================================
setlocal
cd /d "%~dp0"
if not exist build mkdir build
if not exist bin mkdir bin

where cl >nul 2>nul
if %errorlevel%==0 goto msvc
where gcc >nul 2>nul
if %errorlevel%==0 goto mingw
echo [ERROR] No C compiler found.
echo   Install Visual Studio Build Tools (C++), then run this from
echo   "x64 Native Tools Command Prompt", or put MinGW-w64 gcc in PATH.
exit /b 1

:msvc
echo [MSVC] building...
rc /nologo /fo build\daba.res src\daba.rc || exit /b 1
cl /nologo /O2 /MT /W3 /utf-8 /D_CRT_SECURE_NO_WARNINGS /Isrc src\daba.c build\daba.res /Fo:build\ /Fe:bin\DaBa.exe ^
   /link /SUBSYSTEM:WINDOWS user32.lib gdi32.lib shell32.lib ole32.lib uuid.lib dwmapi.lib uxtheme.lib shlwapi.lib gdiplus.lib advapi32.lib || exit /b 1
goto done

:mingw
echo [MinGW] building...
windres src/daba.rc -O coff -o build/daba.res.o || exit /b 1
gcc -O2 -std=c11 -municode -mwindows -Isrc src/daba.c build/daba.res.o -o bin/DaBa.exe ^
    -lgdi32 -luser32 -lshell32 -lole32 -luuid -ldwmapi -luxtheme -lshlwapi -lgdiplus -s || exit /b 1

:done
if not exist bin\libmpv-2.dll echo [NOTE] Put libmpv-2.dll next to bin\DaBa.exe before running.
echo OK: bin\DaBa.exe
