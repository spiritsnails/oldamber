# Contributing to OldAmber

Thanks for your interest in improving OldAmber. Bug fixes, documentation,
platform support, testing, and focused improvements are welcome.

## Before starting

For a small, self-contained fix, feel free to open a pull request directly.
For a large feature, architectural change, major refactor, or change to game
behavior, please open an issue first. This helps confirm that the work fits the
project before significant time is spent on it.

Bug reports and feature requests should use the repository's existing GitHub
issue forms. Questions, general discussion, and early ideas are usually better
suited to the project Discord.

## Project scope

OldAmber aims to preserve the behavior and feel of the original Red and Blue
releases while extending the engine carefully where appropriate.

Maintainers have final say over project scope, implementation, release timing,
and whether a contribution is accepted. An accepted idea may require revisions
before it is included. Contributor credit will be preserved through the pull
request history or release notes as appropriate.

## Copyrighted material

Do not submit:

- ROM files or ROM fragments
- Save files
- Extracted graphics, music, sound, maps, text, or other cartridge data
- `assets.pak` or anything generated under `packages/`
- Material copied from another project without a compatible license and proper
  attribution

Screenshots used to demonstrate a bug are welcome in issues and pull request
descriptions, but they should not be committed as project assets.

OldAmber expects users to provide their own legally acquired ROM. Contributions
must preserve that separation between the engine and cartridge-derived data.

AI-generated artwork, music, sound effects, and other creative assets are not
accepted. Contributors are responsible for understanding, testing, and having
the right to submit everything included in their contribution.

## Pull requests

Keep each pull request focused on one problem or closely related set of
changes. Avoid unrelated cleanup, broad formatting changes, or additional
features in the same pull request.

A pull request should include:

- A clear explanation of the problem and the solution
- A linked issue when one exists
- The platforms tested
- Whether Red, Blue, or both were tested
- Exact testing steps and results
- Screenshots for visible changes when useful
- Any known limitations or follow-up work

Do not include generated build output, local logs, imported game data, personal
configuration, or save data.

## Building and testing

Follow the platform-specific instructions in the repository README. At minimum,
confirm that the affected target builds and that the changed behavior works in
game.

Test both Red and Blue when the change touches shared engine behavior, asset
extraction, version-specific data, or packaging. If you cannot test a relevant
platform or version, state that clearly in the pull request instead of guessing.

Regression fixes should include concise reproduction steps. Automated tests are
welcome where the affected system supports them, but they do not replace an
in-game check for gameplay or presentation changes.

## Code and content guidelines

- Match the surrounding code and naming style.
- Prefer small, direct changes over speculative rewrites.
- Preserve existing behavior outside the scope of the contribution.
- Treat warnings, crashes, out-of-bounds access, and save corruption as defects.
- Do not silently change save compatibility or user-data locations.
- Keep authored dialogue and other original text out of C when the existing
  runtime data format can represent it.
- Do not add dependencies without discussing them first.

## Licensing

By submitting a contribution, you confirm that you have the right to submit it
and agree that it may be distributed under this repository's MIT License.
