# Contributing to DexCorral

> **A personal project, shared openly.** DexCorral is built and maintained by a single developer as a passion project. It's not backed by a company or a team, just one person who wanted a better way to organize a cluttered desktop. I'm sharing it because I think others might find it useful too. Development follows my own priorities and pace, but feedback and bug reports are always welcome.

Thanks for taking the time to read this. Because DexCorral is a one-person project, the way contributions work here is a little different from a company-backed repo. Please read this page before opening an issue or a pull request — it sets honest expectations and explains the licensing terms that apply to any code you submit.

## What you can expect from me

I work on DexCorral in my spare time, around a full-time life. That shapes a few ground rules:

- **Pull requests are not reviewed on any timeline.** I may get to a PR in days, in months, or not at all. Please don't expect timely consideration — open one only if you're comfortable with that.
- **I may decline any pull request without giving a reason.** A change might not fit the direction, the scope, the code style, or simply my plans for the project. A decline isn't a judgment of your work — it's just me keeping a personal project the way I want it.
- **I may not respond to issues.** Bug reports and feature requests are genuinely appreciated and I read what I can, but time constraints mean many will go unanswered or unactioned. That's not rudeness — it's capacity.
- **DexCorral is GPLv3 — so fork freely.** If you need a change I haven't picked up, the [GPLv3 license](LICENSE) gives you the right to fork the project and take it in your own direction. That's the whole point of free software, and I encourage it. Please keep your fork GPLv3-compliant and clearly distinct from the official DexCorral builds.

None of this is meant to discourage you. Small, well-scoped fixes and clear bug reports are the easiest things for me to act on, and they're always welcome.

## How to report a bug

Before opening an issue, please:

1. Check the [open bug reports](https://github.com/guHe330/DexCorralCpp/issues?q=is%3Aissue+label%3Abug) to avoid duplicates.
2. Include your Windows version, your DexCorral version (see the tray menu / `About`), and clear steps to reproduce.
3. Describe what you expected to happen and what actually happened. Screenshots help, especially for rendering or layout issues.

## How to suggest a feature

Check the [planned features](https://github.com/guHe330/DexCorralCpp/issues?q=is%3Aissue+label%3Aenhancement) first, then open an issue describing the problem you're trying to solve — not just the solution you have in mind. I can't promise it will be built, but well-reasoned suggestions are useful input.

## Submitting code

> **Note:** During the alpha the repository may be read-only, and pull requests may be closed unread regardless of their quality. Check the repo's current status before investing time in a change.

If and when the repo is open for pull requests:

- Keep it focused — one logical change per PR is much easier to consider than a large mixed one.
- Match the existing C++17 / Win32 style and the conventions described in the project docs.
- Build cleanly and make sure the unit tests pass (`build.ps1` runs them; see the [Build Guide](docs/BUILD_GUIDE.md)).
- Explain *why* the change is worth making, not just what it does.

## Contributor License Agreement (CLA)

To keep DexCorral's licensing flexible and under single ownership, **every contributor must agree to a Contributor License Agreement before any pull request can be merged.** This is enforced automatically: when you open a pull request, the CLA Assistant bot will comment with a link, and your PR cannot be merged until you have signed.

The CLA does **not** take your copyright away — you keep ownership of the code you write. What it does is grant me a broad license to use that code, including the right to relicense it. In plain terms, by submitting a contribution you agree to the following:

> You retain copyright in your contribution. By submitting it, you grant Gunter Heiss (the DexCorral project owner) a perpetual, worldwide, non-exclusive, royalty-free, irrevocable license to use, reproduce, modify, adapt, publish, distribute, sublicense, **and relicense** your contribution — in whole or in part, under any license terms, including the GPLv3 and including future proprietary or commercial terms — as part of DexCorral or any derivative work, without further notice to or permission from you.
>
> You confirm that the contribution is your own original work (or that you have the right to submit it), that you have the authority to grant this license, and that to your knowledge it does not infringe anyone else's rights. Contributions are provided "as is," without warranty of any kind.

Why this matters: as a solo maintainer I want to be able to relicense or commercially license DexCorral in the future without having to track down every past contributor for permission. Granting this license up front keeps that option open. If you are not comfortable granting it, please don't submit code — but you are of course still free to fork under the GPLv3.

> **Not legal advice.** This summary describes the intent of the CLA in plain language. The full text presented by the CLA Assistant bot is the binding version. Have a professional review the wording before you rely on it.

## License

DexCorral is released under the [GNU General Public License v3.0](LICENSE). Anything you contribute is, at minimum, made available under the GPLv3 (and, per the CLA above, may also be relicensed by the project owner).
