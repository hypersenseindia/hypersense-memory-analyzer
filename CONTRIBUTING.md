# Contributing

Thanks for taking the time to improve HYPERSENSEGLOBAL.

## Before you start

* This project is a GPLv3 derivative of [Moneta](https://github.com/forrest-orr/moneta).
  Contributions are accepted under the same licence.
* Use it only on systems you own or are authorised to test. Pull requests that add
  evasion, anti-analysis, persistence or injection features will be rejected.

## Building and testing

See [docs/BUILDING.md](docs/BUILDING.md). In short:

```bat
cd HyperSense
build.bat
```

Then verify a real scan — the engine exits `1` on success:

```bat
HyperSense64.exe --engine -m ioc -v detail -p <pid>
```

## Pull requests

* One logical change per PR, with a short description of the *observable* difference.
* Match the existing style: 4-space indent, `g_` for globals, `HS_` for control IDs,
  `Hs*` for helpers, no new third-party dependencies.
* If you touch engine code, say which upstream file and line you changed and why — the diff
  against upstream Moneta must stay reviewable.
* If you fix a crash, include the exit code and, where possible, the `cdb` backtrace.
* Keep `README.md`, `CHANGELOG.md` and `NOTICE.md` in sync with behaviour changes.

## Reporting bugs

Open an issue with:

1. version (`Properties → Details → File version`),
2. Windows build,
3. target process and the exact UI settings or command line,
4. the report file (`.txt` or `.html`) — redact host names and SIDs if needed,
5. what you expected instead.

## Security issues

Do **not** open a public issue — follow [SECURITY.md](SECURITY.md).
