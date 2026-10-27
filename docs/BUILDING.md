# Building from source

## Requirements

| | |
| --- | --- |
| OS | Windows 10 or 11, x64 |
| Compiler | MinGW-w64 via [MSYS2](https://www.msys2.org/), **UCRT64** environment |
| Resources | `windres` (ships with the toolchain) |
| Optional | Python 3 + Pillow — only to regenerate the icon if `res/icon.ico` is missing |

Install the toolchain once:

```bat
winget install MSYS2.MSYS2
```

then in an MSYS2 UCRT64 shell:

```bash
pacman -S --needed mingw-w64-ucrt-x86_64-gcc
```

## Build

```bat
cd HyperSense
build.bat
```

The script:

1. locates `g++` on `PATH`, or falls back to `C:\msys64\ucrt64\bin\g++.exe` /
   `C:\msys64\mingw64\bin\g++.exe`, and puts that directory on `PATH` (the compiler needs
   its own DLLs);
2. regenerates `res\icon.ico` with `tools\make_icon.py` **only** when the file is missing;
3. compiles `res\resource.rc` (icon + manifest + version info) to COFF with `windres`;
4. compiles `src\main.cpp` plus every `..\Source\*.cpp` engine translation unit as one
   static, stripped executable;
5. copies the result to the repository root.

Outputs:

```
HyperSense\HyperSense64.exe    (build product)
HyperSense64.exe               (copy, this is the file you ship)
```

## Verify the build

The engine is not a separate program, but it is reachable for testing:

```bat
:: run a scan of an existing process by PID
.\HyperSense64.exe --engine -m ioc -v detail --option statistics -p 1234

:: scan everything (long)
.\HyperSense64.exe --engine -m ioc -v surface -p 1234
```

Exit codes: **1 = scan completed**, `0 = usage error`, anything else = crash. A report of
`exit 1 (scan complete)` in the UI is the expected result.

A quick smoke test that needs no target:

```bat
.\HyperSense64.exe --help
```

## Continuous integration

`.github/workflows/build.yml` builds the same executable on `windows-latest` with MSYS2 and
uploads it as a workflow artifact. Tags matching `v*` attach the binary to a draft release.

## Troubleshooting

**`g++ not found`** — install the UCRT64 compiler (above) or add
`C:\msys64\ucrt64\bin` to `PATH`.

**`cannot execute binary file` / missing `libstdc++-6.dll`** — a non-static build. The
`-static -static-libgcc -static-libstdc++` flags in `build.bat` are what make the executable
standalone; keep them.

**Build fails on a path containing `!`** — the script keeps cmd's *delayed expansion*
disabled on purpose. Do not add `setlocal EnableDelayedExpansion`.

**`0xC0000374` (heap corruption) at exit** — a `new[]` buffer is being freed with scalar
`delete`. See `NOTICE.md` §3 for the ones already fixed.

**Debugging** — MinGW has no usable `gdb` on Windows here. Use WinDbg/CDB:

```
"C:\Program Files (x86)\Windows Kits\10\Debuggers\x64\cdb.exe" ^
  -c "sxe c0000374; g; kb 60" HyperSense64.exe --engine -m ioc -v detail -p <pid>
```

Addresses are image-relative + `0x140000000`; resolve them with
`addr2line -e HyperSense64.exe -f -C <va>`.
