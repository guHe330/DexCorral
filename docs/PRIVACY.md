# DexCorral Privacy

No accounts, no telemetry, no analytics, no ads. DexCorral has exactly one feature that touches the network: an update check, off by default, described in full below.

## What is stored, and where

Everything stays on your PC.

| What | Where |
|---|---|
| Your configuration | `%AppData%\DexCorral\config.json` |
| Install location, language, autostart, shell-extension registration | Registry — full table under [Configuration](USER_MANUAL.md#configuration) in the User Manual |
| Debug logs, written only while **Debug Logging** is on | `%AppData%\DexCorral\*.log` |

One thing about `config.json` is worth knowing: alongside your corral names, positions and tab settings, **it holds the full path of every file you have organized into a corral**. That is how corrals survive a restart. It never leaves your PC on its own, but it is also the file [CONTRIBUTING.md](CONTRIBUTING.md) asks for in bug reports, so read it over and redact before attaching it to a public issue.

## The update check

**Off by default.** Enable it with **Check for Updates Automatically** in the tray menu; 
**Check for Updates Now** runs a single check without turning anything on.

While enabled, at startup and at most once every 24 hours, DexCorral makes one HTTPS
request:

```
GET https://api.github.com/repos/guHe330/DexCorral/releases/latest
```

- It **sends nothing about you or your machine** — no version, no identifier, no
  configuration. The request carries `DexCorral-UpdateCheck` as its user agent and
  nothing else.
- GitHub necessarily sees your IP address and the time, exactly as if you had opened the
  releases page in a browser. That is GitHub's to handle, under [their privacy
  statement](https://docs.github.com/site-policy/privacy-policies/github-general-privacy-statement).
- It **downloads and installs nothing.** A newer release gets you a tray balloon.
  Opening the page is your click; the download is yours to make.

With the setting off, DexCorral makes no network connections at all.

## Enforced, not just promised

Two of the claims above are checked by
[security-guards.yml](../.github/workflows/security-guards.yml) on every push and pull
request, so undoing them breaks the build:

- The update check must use WinHTTP's default certificate validation — no TLS
  validation overrides.
- The release URL arrives over the network and is handed to `ShellExecuteW`, so it must
  stay behind the `IsTrustedReleaseUrl` allowlist.

## Checking for yourself

DexCorral is GPLv3 with the source on GitHub, so none of this has to be taken on trust.
Every release also ships a build provenance attestation, confirming the binary you
downloaded was built by this repository's release workflow from the tagged source:

```powershell
gh attestation verify DexCorral_1.0.28_Setup.exe --repo guHe330/DexCorral
```
