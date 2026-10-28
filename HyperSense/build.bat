@echo off
rem NOTE: delayed expansion must stay off - the repo path contains "!!",
rem which cmd would otherwise strip from every expanded path.
rem
rem Usage:  build.bat            GUI build   -> HyperSense64.exe
rem         build.bat classic    CLASSIC build -> HyperSense64Classic.exe
rem The two binaries share every line of code; the classic one only starts in the
rem moneta64.exe-style one-row layout (the toggle still switches it back).
setlocal
cd /d "%~dp0"

set "GXX="
where g++ >nul 2>&1 && set "GXX=g++"
if not defined GXX if exist "C:\msys64\ucrt64\bin\g++.exe" set "GXX=C:\msys64\ucrt64\bin\g++.exe"
if not defined GXX if exist "C:\msys64\mingw64\bin\g++.exe" set "GXX=C:\msys64\mingw64\bin\g++.exe"
if not defined GXX (
    echo [x] g++ not found. Install MSYS2 UCRT64 or add MinGW-w64 to PATH.
    exit /b 1
)

for %%I in ("%GXX%") do set "BIN=%%~dpI"
set "PATH=%BIN%;%PATH%"
set "WINDRES=%BIN%windres.exe"
if not exist "%WINDRES%" set "WINDRES=windres"

set "VARIANT=%~1"
if "%VARIANT%"=="" goto :gui
if /i "%VARIANT%"=="gui" goto :gui
if /i "%VARIANT%"=="classic" goto :classic
echo [x] unknown variant "%VARIANT%" - use: build.bat  or  build.bat classic
exit /b 1

:gui
set "DEFS="
set "RESDEFS="
set "RESOBJ=res\resource.o"
set "OUTNAME=HyperSense64.exe"
set "FLAVOUR=GUI"
goto :build

:classic
set "DEFS=-DHS_CLASSIC_DEFAULT"
set "RESDEFS=-DHS_CLASSIC_BUILD"
set "RESOBJ=res\resource_classic.o"
set "OUTNAME=HyperSense64Classic.exe"
set "FLAVOUR=CLASSIC UI"
goto :build

:build
if not exist "res\icon.ico" (
    echo [*] Generating icon...
    python tools\make_icon.py || goto :err
)

echo [*] Compiling resources (%FLAVOUR%)...
"%WINDRES%" -I res %RESDEFS% res\resource.rc -O coff -o "%RESOBJ%" || goto :err

echo [*] Building %OUTNAME% (%FLAVOUR% + built-in Moneta engine)...
"%GXX%" -std=c++17 -O2 -DUNICODE -D_UNICODE -D_WIN32_WINNT=0x0A00 %DEFS% ^
    -I..\Headers -I.. ^
    src\main.cpp ..\Source\*.cpp "%RESOBJ%" ^
    -o "%OUTNAME%" ^
    -mwindows -static -static-libgcc -static-libstdc++ -s ^
    -lcomctl32 -lcomdlg32 -lshell32 -lshlwapi -ladvapi32 -lole32 -luuid -ldwmapi ^
    -lgdi32 -luser32 -lpsapi -limagehlp -lwintrust -lcrypt32 || goto :err

echo [*] Copying standalone build to the repository root...
copy /y "%OUTNAME%" "..\%OUTNAME%" >nul 2>&1

echo.
echo [ok] Build complete: %~dp0%OUTNAME%
exit /b 0

:err
echo.
echo [x] Build failed.
exit /b 1
