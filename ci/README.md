<!---
  Copyright 2018 H2O.ai

  Permission is hereby granted, free of charge, to any person obtaining a
  copy of this software and associated documentation files (the "Software"),
  to deal in the Software without restriction, including without limitation
  the rights to use, copy, modify, merge, publish, distribute, sublicense,
  and/or sell copies of the Software, and to permit persons to whom the
  Software is furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in
  all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
  IN THE SOFTWARE.
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
    ```md
    ### [Unreleased](https://github.com/h2oai/datatable/compare/HEAD...v{MMM})
    ```
  line at the top of the file should become
    ```md
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

- Wait for the PR to finish all (but one) tests / checks successfully. If not,
  push additional commits into this branch to fix any emergent problems. One
  check will remain in the "pending" state until you manually complete it -
  see the next step.

- Click on the pending task to go to Jenkins. On the very bottom, there should
  be a prompt whether this build should be promoted (if the prompt is not there,
  the build might be still in progress or has failed). Click proceed — this
  will upload the artifacts to
  `s3://h2o-release/datatable/stable/datatable-{MN0}/` and draft a GitHub
  release.

- Merge the PR into master, but **DO NOT delete the branch**. This branch
  should be kept forever.

- Go to [Releases](https://github.com/h2oai/datatable/releases) on GitHub —
  there should be a new latest release. Review the changelog and artifacts, and
  check that everything is in order.

- From the Release page download 3 artifacts: one `.tar.gz` source distribution,
  and two `_macosx_*.whl` distributions (for python 3.5 and 3.6). Then upload
  them onto PyPI:
  ```bash
  ls ~/Downloads/datatable-{MN0}*
  twine upload ~/Downloads/datatable-{MN0}*
  ```

- go to <https://pypi.org/manage/project/datatable/releases/> and check that
  the release was uploaded successfully.

- ??? documentation update



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
    ```md
    ### [v{MMN}](https://github.com/h2oai/datatable/compare/v{MMN}...v{MMM} — {DATE})
    ```
  where `DATE` is the current date. This line should be at the top of the file
  immediately preceding the `[v{MMM}] ...` line. Add a description of the
  changes being introduced in this patch.

- Commit the changes, and push the commit to GitHub.

- Wait for tests to finish (fix problems if some checks fail), then go to
  Jenkins and do the same procedure as in case of new minor version (approve
  the prompt in console, review and publish the GitHub release draft).

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
