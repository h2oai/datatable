
Contributing
============

``datatable`` is an open-source project released under the Mozilla Public
License v2. Open source projects live by their user and developer communities.
We welcome and encourage your contributions of any kind!

No matter what your skill set or level of engagement is with ``datatable``,
you can help others by improving the ecosystem of documentation, bug report
and feature request tickets, and code.

We invite anyone who is interested to contribute, whether through pull requests,
tests, GitHub issues, feature suggestions, or even generic discussion.

If you have questions about using ``datatable``, post them on `Stack Overflow`_
using the ``[py-datatable]`` tag.


.. _`Stack Overflow`: https://stackoverflow.com/questions/tagged/py-datatable


.. _`local-setup`:

Preparing local copy of datatable repository
--------------------------------------------

If this is the first time you're contributing to ``datatable``, then follow
these steps in order to set up your local development environment:

1. Make sure you have command-line tools ``git`` and ``make`` installed.
   You should also have a text editor or an IDE of your choice.

2. Go to https://github.com/h2oai/datatable and click the "fork" button
   in the top right corner. You may need to create a GitHub account if
   you don't have one already.

3. Clone the repository on your local computer:

   .. code-block:: console

      $ git clone https://github.com/your_user_name/datatable

4. Lastly, add the original ``datatable`` repository as the upstream:

   .. code-block:: console

      $ cd datatable
      $ git remote add upstream https://github.com/h2oai/datatable
      $ git fetch upstream
      $ git config branch.main.remote upstream
      $ git config branch.main.merge refs/heads/main

This completes the setup of your local datatable fork. Make sure to note
the location of the ``datatable/`` directory that you created in step 3.
You will need to return there when issuing any subsequent ``git`` commands
detailed futher.


Creating a Pull Request
-----------------------

Start by fetching any changes that might have occurred since the last time
you were working with the repository:

.. code-block:: console

   $ git checkout main
   $ git fetch upstream
   $ git merge upstream/main


Then create a new local branch where you will be working on your changes.
The name of the branch should be a short identifier that will help you
recognize what this branch is about. It's a good idea to prefix the branch
name with your initials so that it doesn't conflict with branches from other
developers:

.. code-block:: console

   $ git checkout -b your_branch_name


After this it is time to make the desired changes to the project. There are
separate guides on how to work with documentation and how to work with core
code changes. It is also a good idea to commit the code frequently, using
``git add`` and ``git commit`` changes.

Note: While many projects ask for detailed and informative commit messages,
we don't. Our policy is to squash all commits when merging a pull request, and
therefore the only detailed message that is needed is the PR description.

When you think your proposed change is ready, verify that everything is in
order by running ``git status`` -- it should say "nothing to commit, working
tree clean". At this point the changes need to be pushed into the "origin",
which is your repository fork:

.. code-block:: console

   $ git push origin your_branch_name

Then go back to the GitHub website to your fork of the datatable repository
https://github.com/your\_user\_name/datatable. There you should see a pop-up
that notifies about the changes pushed to ``your_branch_name``. There will also
be a green button "Compare & pull request". Pressing that button you will see
an "Open a pull request" form.

When opening a pull request, make sure to provide an informative title and a
detailed description of the proposed changes. If the pull request directly
addresses one of the issues, make sure to note that in the text of the PR
description.

Make sure the checkbox "Allow edits by maintainers" is turned on, and then
press "Create pull request".

At this point your Pull Request will be scheduled for review at the main
datatable repository. Once reviewed, you may be asked to change something, in
which case you can make the necessary modifications locally, then commit and
push them.
