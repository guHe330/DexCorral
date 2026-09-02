# Contributing to DexCorral

Thanks for your interest in DexCorral. Bug reports, feature suggestions, and pull requests are all welcome.

## Reporting bugs and requesting features

Open an [issue](https://github.com/guHe330/DexCorral/issues/new/choose) and pick a template. The forms ask for what is actually needed to reproduce a problem:

- Your Windows version (e.g. Windows 11 Pro 24H2)
- The DexCorral version (visible in the tray menu)
- What you did, what you expected, and what happened instead
- A screenshot, if the problem is visual
- Your display setup, if more than one monitor is involved

A partial report beats no report. If a field does not apply, leave it empty and post anyway.

**Windows 11 only.** Bugs reported from Windows 10 are closed as out of scope, including ones hit after bypassing the version check with `--force`. The override exists so you can experiment, not so you can file reports from there.

For layout, tab and persistence bugs, attaching `%AppData%\DexCorral\config.json` is the fastest route to a fix. It lists your corral names, positions and the paths of the files you have organized, so look it over and redact anything you would rather not publish.

## Testing is a contribution

You do not need to write code to help, and right now testing is worth more than code.

DexCorral is alpha software, and development covers only a handful of hardware and display configurations. Anything beyond those is untested territory. The gaps where reports from other setups help most:

- **Multiple monitors.** Per-monitor positions, per-resolution layouts, and monitor unplug/replug handling need far wider coverage than they get here, especially across mixed refresh rates, arrangements and hot-plug order. The [Multi-monitor test report](https://github.com/guHe330/DexCorral/issues/new?template=multi_monitor_report.yml) form has a checklist; one completed section is a useful report.
- **Mixed DPI**, such as a 150% laptop panel next to a 100% external screen.
- **Unusual display hardware**: ultrawides, portrait orientation, TVs, wireless displays, docks.
- **Large or unusual desktops**: hundreds of icons, OneDrive-synced folders, network paths.

Reports that everything worked are worth posting too. Knowing that restart persistence survives on a three-monitor setup is as useful as knowing that it does not.

## Scope, before you write code

DexCorral is a personal tool. It is developed to fit the maintainer's own workflow, and changes that do not serve that purpose are declined regardless of how well they are implemented. See [Project Scope](../README.md#project-scope) in the README.

So: **open an issue before writing code.** A short "would you take a PR that does X?" costs you five minutes and can save you an evening. Bug fixes are almost always welcome without asking.

Reviews happen roughly once a week.

## Contributor License Agreement

Before a pull request can be merged, you need to sign the [Contributor License Agreement](CLA.md): three lines, and you keep your copyright.

This is a one-time, automated step. When you open your first pull request, a bot comments with a link. To sign, reply on the pull request with exactly:

```
I have read the CLA Document and I hereby sign the CLA
```

Your signature is recorded in `signatures/version1/cla.json` and covers all your future contributions, so you will not be asked again.

If you contribute as part of your job, make sure your employer permits it.

## Building

See the [Build Guide](BUILD_GUIDE.md) for prerequisites and what the build script does.

```powershell
cd DexCorral
./build.ps1
```

## Pull requests

- Branch from `main` and keep each pull request focused on one change.
- Match the surrounding code style: 4-space indent, Allman braces, `PascalCase` for types and methods, `camelCase` for locals, doc comments with `///`.
- Every source file carries the GPL header from `COPYRIGHT_HEADER.txt` in the repository root. New files need it too.
- Run `./build.ps1` before submitting; the unit tests must pass.
- Describe what the change does and how you tested it. If it changes something visible, a before/after screenshot helps.
- User-visible changes should get a line in the changelog (see below).

## Changelog and release notes

**Your part:** if the change is visible to a user, add one line under the `## [Unreleased]` heading at the top of [docs/CHANGELOG.md](./CHANGELOG.md), in the matching `### Added` / `### Changed` / `### Fixed` group (create the group if it is missing).

Keep it to one or two sentences: what changed, and why if that is not obvious. No symbol names, no file paths, no implementation detail. That belongs in the commit message. Do not add a version heading, do not edit already-released sections, and do not touch anything under `Distribute/`.

**Maintainer's part:** versioning, the dated release section, the *What's New* notes, tagging, and publishing the GitHub Release.

If you are not sure a change is user-visible, add the line anyway; it is easier to drop one than to reconstruct it at release time.
