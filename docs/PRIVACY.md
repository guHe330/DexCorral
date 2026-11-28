# DexCorral Privacy

No accounts, no telemetry, no analytics, no ads. DexCorral has exactly one feature that touches the network: an update check, off by default, described in full below.

## What is stored, and where

Everything stays on your PC.

| What | Where |
|---|---|
| Your configuration | `%AppData%\DexCorral\config.json` |
| Install location, language, autostart, shell-extension registration | Registry; full table under [What DexCorral writes outside its own folder](TECHNICAL_NOTES.md#what-dexcorral-writes-outside-its-own-folder) in the Technical Notes |
| Debug logs, written only while **Write debug logs** is on in the settings window | `%AppData%\DexCorral\*.log` |

Alongside your corral names, positions and tab settings, `config.json` **holds the full path of every file you have organized into a corral**. That is how corrals survive a restart. It never leaves your PC on its own, but it is also the file [CONTRIBUTING.md](CONTRIBUTING.md) asks for in bug reports, so read it over and redact before attaching it to a public issue.

## The update check

**Off by default.** Enable it with **Check for Updates Automatically** in the settings window (tray menu, **Settings...**); 
**Check for Updates Now** runs a single check without turning anything on.

While enabled, DexCorral makes one HTTPS request when it starts, if the last check that
got an answer (automatic or **Check for Updates Now**) is 24 hours old or older, and
again each time it becomes 24 hours old while DexCorral runs; DexCorral looks once an
hour whether that time has come. A last check dated in the future, which a wrong clock
can leave behind, does not hold the next one back. Switching the setting on runs one
check right away. If an automatic request fails, for example because the network is not
up yet, the first failure since DexCorral started is retried five minutes later; after
that DexCorral tries once an hour until a request succeeds:

```
GET https://api.github.com/repos/guHe330/DexCorral/releases/latest
```

- It **sends nothing about you or your machine**: no version, no identifier, no
  configuration, no cookies. Besides what Windows puts into every HTTPS request, such
  as the host name `api.github.com`, it carries three fixed headers, the same on every
  PC. The last two ask GitHub for its documented API format:

  ```
  User-Agent: DexCorral-UpdateCheck
  Accept: application/vnd.github+json
  X-GitHub-Api-Version: 2022-11-28
  ```
- GitHub necessarily sees your IP address and the time, exactly as if you had opened the
  releases page in a browser. That is GitHub's to handle, under [their privacy
  statement](https://docs.github.com/site-policy/privacy-policies/github-general-privacy-statement).
- It **downloads and installs nothing.** A newer release gets you a tray balloon.
  Opening the page is your click; the download is yours to make.

With the setting off, DexCorral makes no network connections at all.

## What CI enforces

Two of the claims above are checked by
[security-guards.yml](../.github/workflows/security-guards.yml) on every push and pull
request, so undoing them breaks the build:

- The update check must use WinHTTP's default certificate validation, with no TLS
  validation overrides.
- The release URL arrives over the network and is handed to `ShellExecuteW`, so it must
  stay behind the `IsTrustedReleaseUrl` allowlist.

## Checking for yourself

DexCorral is GPLv3 with the source on GitHub, so none of this has to be taken on trust.
Every release also ships a build provenance attestation, confirming the binary you
downloaded was built by this repository's release workflow from the tagged source:

```powershell
gh attestation verify DexCorral_<version>_Setup.exe --repo guHe330/DexCorral
```
