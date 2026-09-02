# Security Policy

## Reporting a vulnerability

Please do not open a public issue for security problems.

Use **GitHub private vulnerability reporting**: the "Report a vulnerability" button under the [Security tab](https://github.com/guHe330/DexCorral/security). It keeps the report, the discussion and the advisory in one place, and stays private until an advisory is published.

Useful to include: affected version, your Windows version, steps to reproduce, and what an attacker gains. Report in English or German.

DexCorral is a one-person project, so please allow some time for a reply. You will be credited in the advisory unless you prefer otherwise. There is no bug bounty.

## Supported versions

Only the latest release gets security fixes.

## Before you report

DexCorral injects `DexCorralHook.dll` into `explorer.exe` and manipulates the desktop ListView across processes. That is how the product works, not a vulnerability in itself, though antivirus heuristics sometimes read it that way. Findings about how that injection could be abused by a third party are very much in scope.

Out of scope: anything requiring an attacker who already runs code as the same user, SmartScreen warnings on unsigned builds (known, code signing is planned), and self-built or modified binaries.
