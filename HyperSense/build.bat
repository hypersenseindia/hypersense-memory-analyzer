@echo off
rem NOTE: delayed expansion must stay off - the repo path contains "!!",
rem which cmd would otherwise strip from every expanded path.
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

if not exist "res\icon.ico" (
    echo [*] Generating icon...
    python tools\make_icon.py || goto :err
)

echo [*] Compiling resources...
"%WINDRES%" -I res res\resource.rc -O coff -o res\resource.o || goto :err

echo [*] Building HyperSense64.exe (GUI + built-in Moneta engine)...
"%GXX%" -std=c++17 -O2 -DUNICODE -D_UNICODE -D_WIN32_WINNT=0x0A00 ^
    -I..\Headers -I.. ^
    src\main.cpp ..\Source\*.cpp res\resource.o ^
    -o HyperSense64.exe ^
    -mwindows -static -static-libgcc -static-libstdc++ -s ^
    -lcomctl32 -lcomdlg32 -lshell32 -lshlwapi -ladvapi32 -lole32 -luuid -ldwmapi ^
    -lgdi32 -luser32 -lpsapi -limagehlp -lwintrust -lcrypt32 || goto :err

echo [*] Copying standalone build to the repository root...
copy /y HyperSense64.exe "..\HyperSense64.exe" >nul 2>&1

echo.
echo [ok] Build complete: %~dp0HyperSense64.exe
exit /b 0

:err
echo.
echo [x] Build failed.
exit /b 1
