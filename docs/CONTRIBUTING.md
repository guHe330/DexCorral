# Contributing to DexCorral

Thanks for your interest in DexCorral. Bug reports, feature suggestions, and pull requests
are all welcome.

## Reporting bugs and requesting features

Open an [issue](https://github.com/guHe330/DexCorral/issues/new/choose) and pick a template. The
forms ask for what is actually needed to reproduce a problem:

- Your Windows version (e.g. Windows 11 Pro 24H2)
- The DexCorral version (visible in the tray menu)
- What you did, what you expected, and what happened instead
- A screenshot, if the problem is visual
- Your display setup, if more than one monitor is involved

A partial report beats no report. If a field does not apply, leave it empty and post anyway.

For layout, tab and persistence bugs, attaching `%AppData%\DexCorral\config.json` is the fastest
route to a fix. It lists your corral names, positions and the paths of the files you have organized,
so look it over and redact anything you would rather not publish.

## Testing is a contribution

You do not need to write code to help, and right now testing is worth more than code.

DexCorral is alpha software developed on **one machine, with one monitor**, against the maintainer's
own way of using a desktop. Every setup that differs from that is untested territory. The gaps that
most need someone else's hardware:

- **Multiple monitors.** Corral positions are stored per monitor and per resolution, corrals move to
  the primary display when their monitor is disconnected and return when it comes back — none of it
  has ever run on a real multi-display setup. There is a
  [Multi-monitor test report](https://github.com/guHe330/DexCorral/issues/new?template=multi_monitor_report.yml)
  template with a checklist; one completed section is a useful report.
- **Mixed DPI**, such as a 150% laptop panel next to a 100% external screen.
- **Unusual display hardware** — ultrawides, portrait orientation, TVs, wireless displays, docks.
- **Large or unusual desktops** — hundreds of icons, OneDrive-synced folders, network paths.

Reports that everything worked are worth posting too. Knowing that restart persistence survives on a
three-monitor setup is as useful as knowing that it does not.

## Scope, before you write code

DexCorral is a personal tool. It is developed to fit the maintainer's own workflow, and changes
that do not serve that purpose are declined regardless of how well they are implemented. See
[Project Scope](../README.md#project-scope) in the README.

So: **open an issue before writing code.** A short "would you take a PR that does X?" costs you
five minutes and can save you an evening. Bug fixes are almost always welcome without asking.

Reviews happen roughly once a week.

## Contributor License Agreement

Before a pull request can be merged, you need to sign the
[Contributor License Agreement](CLA.md). It is three lines long: your contribution is licensed
under the GPLv3, you have the right to submit it, and you keep your copyright.

This is a one-time step and it is automated. When you open your first pull request, a bot comments
with a link. To sign, reply on the pull request with exactly:

```
I have read the CLA Document and I hereby sign the CLA
```

Your signature is recorded in `signatures/version1/cla.json` and covers all your future
contributions, so you will not be asked again.

If you contribute as part of your job, make sure your employer permits it.

## Building

See the [Build Guide](BUILD_GUIDE.md). In short, you need Visual Studio 2022 with the
C++ desktop workload and CMake, then:

```powershell
cd DexCorral
./build.ps1
```

The build script kills running DexCorral instances, compiles, runs the unit tests, and
produces the installer.

## Pull requests

- Branch from `main` and keep each pull request focused on one change.
- Match the surrounding code style: 4-space indent, Allman braces, `PascalCase` for types and
  methods, `camelCase` for locals, doc comments with `///`.
- Every source file carries the GPL header from `COPYRIGHT_HEADER.txt` in the repository root. New files need it too.
- Run `./build.ps1` before submitting — the unit tests must pass.
- Describe what the change does and how you tested it. If it changes something visible,
  a before/after screenshot helps.
- User-visible changes should get a line in the changelog (see below).

## Changelog and release notes

**Your part:** if the change is visible to a user, add one line under the `## [Unreleased]`
heading at the top of [docs/CHANGELOG.md](./CHANGELOG.md), in the matching `### Added` /
`### Changed` / `### Fixed` group (create the group if it is missing). Write it the way the rest
of the file is written: what changed and why, naming the symbols or files involved. Do not add a
version heading, do not edit already-released sections, and do not touch anything under
`Distribute/`.

**Maintainer's part:** versioning and everything downstream of it — bumping
`DexCorral/include/Version.h`, moving `[Unreleased]` into a dated `## [X.Y.Z]` section, writing
the plain-language *What's New* release notes in `Distribute/RELEASE_TEMPLATE.md`, tagging, and
publishing the GitHub Release.

If you are not sure a change is user-visible, add the line anyway — it is easier to drop one than
to reconstruct it at release time.

## Questions

If you are unsure whether a change would be welcome, open an issue first and ask. That is
usually faster than building something that turns out not to fit.
