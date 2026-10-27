# Security policy

## Supported versions

| Version | Supported |
| --- | --- |
| 1.0.x | Yes |
| < 1.0 | No |

## Reporting a vulnerability

Open a **private** report through GitHub:
**Security → Report a vulnerability**
(<https://github.com/hypersenseindia/hypersense-memory-analyzer/security/advisories/new>)

Please include the version, the target process, the command line or UI settings used, and
the report file if you can share it. Do not attach full memory dumps of a machine that
holds personal data.

Expect an initial reply within 7 days. Fixes ship in the next tagged release with credit
unless you prefer to stay anonymous.

## Scope

In scope:

* memory-safety bugs in the GUI or in the embedded engine (use-after-free, overflows,
  uninitialised reads that produce wrong results);
* crashes or hangs caused by malformed *external* input — a hostile target process, a
  crafted PE module inside a target, or an oversized report path;
* privilege issues: anything that lets the GUI or the scan child do more than the user
  already can.

Out of scope:

* antivirus or SmartScreen verdicts — see [FALSE_POSITIVES.md](FALSE_POSITIVES.md);
* the fact that the tool *can* read process memory, which is its function;
* bugs reproducible only against a target the user has already compromised;
* missing hardening on a machine that is already administrator-controlled.

## Design notes relevant to security

* The GUI runs with `asInvoker`. Elevation is an explicit user action.
* The scan child is created with an explicit command line, no shell, inherited stdout pipe
  and `CREATE_NO_WINDOW`; it receives no network access and writes only the report the user
  selects.
* `SeDebugPrivilege` is enabled inside the scan child immediately before a scan, never at
  GUI start-up.
* Nothing is persisted between runs and no telemetry is collected.
