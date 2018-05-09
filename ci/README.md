<!---
  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
-->

# Outline of the release process

Current version of `datatable` can be found in the `datatable/__version__.py`
file. In the description below, this version will be denoted as
`{MAJOR}.{MINOR}.{MICRO}` (or `MMM` for short).


## Releasing a new minor version

This is the most common type of release. Use it when neither major nor micro
update applies.

- Create a new local branch called `rel-{MAJOR}.{MINOR+1}` on top of `master`.
- In the `__version__.py` modify the version to `{MAJOR}.{MINOR+1}.0` (call
  this `MN0` for short).
- Update the `CHANGELOG.md`: the
    ```
    ### [Unreleased](https://github.com/h2oai/datatable/compare/HEAD...v{MMM})
    ```
  line at the top of the file should become
    ```
    ### [Unreleased](https://github.com/h2oai/datatable/compare/HEAD...v{MN0})
    ### [v{MN0}](https://github.com/h2oai/datatable/compare/v{MN0}...v{MMM} — {DATE})
    ```
  where `DATE` is the today's date. Note that the date is separated from the
  version by an em-dash (—), not the simple dash (-).
- Look through the changelog entries being released, and fix any grammar,
  style or spelling mistakes, also check for omissions.
- Update the "Installation" section in `README.md` to point to the new
  latest version of the package. (This step can be removed after #952 is
  implemented).
- Commit the changes with commit message `Release v{MN0}` and push the branch.
  On GitHub, create a new pull request from this branch.
- Wait for the PR to finish all tests / checks successfully. If not, push
  additional commits into this branch to fix any emergent problems.
- Merge the PR into master, but **DO NOT delete the branch**. This branch
  should be kept forever.
- ??? go to Jenkins and promote the builds to S3
- ??? use `twine upload` to make the new release available on PyPi
- ??? make a release on GitHub



## Releasing a new micro version

Micro releases are uncommon. Use them only when you need to patch an existing
version with some bug fixes. Do not add new functionality into a patch release
(unless in unusual circumstances).

- If you need to patch version `{MAJOR}.{MINOR}.{MICRO}`, start by checking out
  the branch `rel-{MAJOR}.{MINOR}`.
- Cherry-pick any commits that you need into this branch from master (or from
  any other branch).
- In the `__version__.py` file change the version to `{MAJOR}.{MINOR}.{MICRO+1}`
  (call it `MMN`).
- Update the `CHANGELOG.md` by adding a new line with
    ```
    ### [v{MMN}](https://github.com/h2oai/datatable/compare/v{MMN}...v{MMM} — {DATE})
    ```
  where `DATE` is the current date. This line should be at the top of the file
  immediately preceding the `[v{MMM}] ...` line. Add a description of the
  changes being introduced in this patch.
- Commit the changes, and push the commit to GitHub.
- ???



## Releasing a new major version

This is the rarest type of release. We should probably aim to release a new
major version every half-a-year or every year.

Otherwise, major version does not differ much from the minor version update:
change the version number to `{MAJOR+1}.0.0`. In addition to steps outlined
in the minor version release section, the following might be needed:

- Look through the code for places that say
  - "will be removed in version {MAJOR+1}"
  - "will be changed in version {MAJOR+1}"
  - "will become deprecated in version {MAJOR+1}"
  and make those changes accordingly.
- Update `CHANGELOG.md` noting all functionality that was removed/ changed/
  deprecated.
- Publish a blog post outlining all main improvements since the previous major
  release.
- Make a twitter announcement on @pydatatable
