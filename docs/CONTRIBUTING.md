# Contributing to DexCorral

Thanks for your interest in DexCorral. Bug reports, feature suggestions, and pull requests
are all welcome.

## Reporting bugs and requesting features

Open an [issue](https://github.com/guHe330/DexCorral/issues). For bugs, please include:

- Your Windows version (e.g. Windows 11 Pro 24H2)
- The DexCorral version (visible in the tray menu)
- What you did, what you expected, and what happened instead
- A screenshot, if the problem is visual

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
