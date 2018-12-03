# Contributing guidelines

This document is a set of guidelines for contributors. Nothing is set in stone,
so please just use common sense.

## Table of contents

[How to contribute](#how-to-contribute)

* [Reporting bugs](#reporting-bugs)
* [Enhancements and Feature requests](#enhancements-and-feature-requests)
* [Committing code](#committing-code)

[Guidelines and standards](#guidelines-and-standards)

* [General](#general)
* [Git commit messages](#git-commit-messages)
* [Python style guidelines](#python-style-guidelines)
* [Documentation style guidelines](#documentation-style-guidelines)


## How to contribute

There are plenty of ways to contribute, not only by submitting new code!
Reporting bugs, adding/improving documentation and tests are just as important
to us.


### Reporting bugs

* First, please make sure the bug was not already reported by searching on
  GitHub under [issues](https://github.com/h2oai/datatable/issues).

* Only when you are sure the bug has not yet been reported,
  [open a new issue](https://github.com/h2oai/datatable/issues/new).

* Please follow the [issue template](ISSUE_TEMPLATE.md) and provide as many
  details as possible. A clear but concise, title, your environment, and an
  [MCVE](https://stackoverflow.com/help/mcve) will make everyone's life easier.


### Enhancements and Feature requests

We are open to new enhancements/feature requests!

* Just like with bugs, please check whether there is already an existing issue
  in the tracker.

* If not, then you can create a new issue with an appropriate label
  (`feature request` or `enhancement`).

* Use a descriptive but concise title.

* Start off by stating what would this improvement fix/enhance and why would it
  be useful.

* Provide as detailed a description as possible (but do not write a novel).

* Preferably add a specific example.

* If necessary (or you just think it would be beneficial), add drawings,
  diagrams, etc.


### Committing code

Should you want to improve the codebase, please submit a pull request. If you
are new to GitHub, check their
[how-to](https://help.github.com/articles/using-pull-requests/) first.

Please be sure to comment on an issue should you decide to give it a go so
other developers know about it. All issue-related discussions should take place
in the issue's comment section.

Before submitting your PR for a
[review](https://github.com/h2oai/datatable/pulls), please make sure your
changes follow the standards described below, are well tested, and that all the
previous tests are passing.


### Guidelines and standards

#### General

* Test your code. Be it a bug fix, enhancement, or a new feature, please provide
  a set of tests showing 1) that it actually runs 2) does what it is supposed
  to be doing and 3) does not break existing codebase

* Keep in mind performance - runtime speed, resource use, and accuracy are
  important to us.

* API breaking changes/fixes will need an extended discussion.

* If you add new features or add a non trivial piece of code, please do document
  it if needed.


#### Git commit messages

Clean and descriptive commit messages keep the maintainers happy, so please
take a minute to polish yours.

Preferable message structure (from: http://git-scm.com/book/ch5-2.html):

> ```text
> Short (50 chars or less) summary of changes
>
> More detailed explanatory text, if necessary.  Wrap it to about 72
> characters or so.  In some contexts, the first line is treated as the
> subject of an email and the rest of the text as the body.  The blank
> line separating the summary from the body is critical (unless you omit
> the body entirely); tools like rebase can get confused if you run the
> two together.
>
> Further paragraphs come after blank lines.
>
>   - Bullet points are okay, too
>
>   - Typically a hyphen or asterisk is used for the bullet, preceded by a
>     single space, with blank lines in between, but conventions vary here
> ```

* Keep the initial line short (50 characters or less).

* Use imperative mood in the first line i.e. "Fix", "Add", "Change" instead of
  "Fixed", "Added", "Changed".

* Second line blank.

* Add issue/pull request number in the message.

* Don't end the summary line with a period.

* If you are having trouble describing what a commit does, make sure it does
  not include several logical changes or bug fixes. Should that be the case,
  please split it up into several commits using `git add -p`.


#### Python style guidelines

For Python code please follow the
[Google Python Style Guide](https://google.github.io/styleguide/pyguide.html).
