
Contributing documentation
==========================

The documentation for ``datatable`` project is written entirely in the
ReStructured Text (RST) format and rendered using the Sphinx engine. These
technologies are standard for Python.

The source RST files for the documentation live in the ``/docs`` directory
of the project. With every pull request they are sent to ``readthedocs.io``
host, where they are automatically compiled into HTML and made available
to the public. You can find the link to this new build in the list of "checks"
at the bottom of every PR.

The basic workflow for developing documentation is following:

    1. :ref:`Set up <local-setup>` a local datatable repository;

    2. Go into the ``docs/`` directory and create new or change existing
       documentation files;

    3. Run in a terminal

        .. code-block:: console

            $ make html

    4. If there were no errors, the documentation can be viewed locally
       by opening the file ``docs/_build/html/index.html`` in a browser.

The ``make html`` command needs to be re-run after every change you make.
Occasionally you may also need to ``make clean`` if something doesn't seem to
work properly.


Basic formatting
----------------

At the most basic level, RST document is a plain text, where paragraphs are
separated with empty lines:

.. code-block:: rst

    Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod
    tempor incididunt ut labore et dolore magna aliqua.

    Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut
    aliquip ex ea commodo consequat. Duis aute irure dolor in reprehenderit in
    voluptate velit esse cillum dolore eu fugiat nulla pariatur.

The line breaks within each paragraph are ignored; on the rendered page the
lines will be as wide as is necessary to fill the page. With that in mind, we
ask to avoid lines that are longer than 80 characters in width, if possible.
This makes it much easier to work with code on small screen devices.


Page headings are a simple line of underlined text:

.. code-block:: rst

    Heading Level 1
    ===============

    Heading Level 2
    ---------------

    Heading Level 3
    ~~~~~~~~~~~~~~~

Each document must have exactly one level-1 heading; otherwise the page would
not render properly.


Basic **bold text**, *italic text* and ``literal text`` is written as follows.
(Note that literals use double backticks, which is a frequent cause of
formatting errors.):

.. code-block:: rst

    **bold text**
    *italic text*
    ``literal text``


Bulleted and ordered lists are done similarly to Markdown:

.. code-block:: rst

    - list item 1;
    - list item 2;
    - a longer list item, that might need to be
      be carried over to the next line.

    1. ordered list item 1

    2. ordered list item 2

       This is the next paragraph of list item 2.

The content of each list item can be arbitrarily complex, as long as it is
properly indented.


.. _`code blocks`:

Code blocks
-----------

There are two main ways to format a block of code. The simplest way is to finish
a paragraph with a double-colon ``::`` and then start the next paragraph (code)
indented with 4 spaces:

.. code-block:: rst

    Here is a code example::

        >>> print("Hello, world!", flush=True)

In this case the code will be highlighted assuming it is a python sample. If the
code corresponds to some other language, you'll need to use an explicit
``code-block`` directive:

.. code-block:: rst

    .. code-block:: shell

        $ pip install datatable

This directive allows you to explicitly select the language of your code snippet,
which will affect how it is highlighted. The code inside ``code-block`` must be
indented, and there has to be an empty line between the ``.. code-block::``
declaration and the actual code.

When writing python code examples, the best practice is to use python console
format, i.e. prepend all input lines with ``>>>`` (or ``...`` for continuation
lines), and keep all output lines without a prefix. When documenting an error,
remove all traceback information and leave only the error message:

.. code-block:: rst

    >>> import datatable as dt
    >>> DT = dt.Frame(A=[5], B=[17], D=['zelo'])
    >>> DT
       |     A      B  D
       | int32  int32  str32
    -- + -----  -----  -----
     0 |     5     17  zelo
    [1 row x 3 columns]

    >>> DT.hello()
    AttributeError: 'datatable.Frame' object has no attribute 'hello'

This code snippet will be rendered as follows:

.. code-block:: python

    >>> import datatable as dt
    >>> DT = dt.Frame(A=[5], B=[17], D=['zelo'])
    >>> DT
       |     A      B  D
       | int32  int32  str32
    -- + -----  -----  -----
     0 |     5     17  zelo
    [1 row x 3 columns]

    >>> DT.hello()
    AttributeError: 'datatable.Frame' object has no attribute 'hello'


Hyperlinks
----------

If you want to create a link to an external website, then you would do it in
two parts. First, the text of the link should be surrounded by backticks and
followed by an underscore. Second, the URL of the link is declared at
the bottom of the page (or section) via a special directive:

.. code-block:: rst

    Say you want to create a link to an `example website`_. The text "example
    website" in the previous sentence will be turned into a link, whose URL is
    declared somewhere later on the page.

    And here we declare the target (the backticks are actually optional):

    .. _`example website`: https://example.com/


If you want to create a link to another page or section of this documentation,
then it is done similarly: first you create the target, then refer to that
target within the document.

Creating the target is done similarly to how we declared an external URL, only
this time you simply omit the URL. The RST engine will then assume that the
target points to the following element on the page (which should usually be a
section heading, an image, a table, etc):

.. code-block:: rst

    .. _`hello world`:

    Hello world example
    ~~~~~~~~~~~~~~~~~~~

Then you can refer to this target the same way that you referred to an external
URL in the previous example. However, this would only work if you refer to this
anchor within the same page. If you want to refer to this anchor within another
rst document, then you would need to use the ``:ref:`` role:

.. code-block:: rst

    We can refer to "hello world example" even from a different document
    like this: :ref:`hello world`. Also, you can use the following syntax to
    refer to the same anchor but change its description text:
    :ref:`the simplest program <hello world>`.


Lastly, there are also special auto-generated targets in the API Reference
part of the documentation. These targets describe each class, function, method,
and other exported symbols of the ``datatable`` module. In order to refer to
these targets, special syntax is used:

.. code-block:: rst

    :mod:`datatable`
    :class:`datatable.Frame`
    :meth:`datatable.Frame.to_csv`
    :func:`datatable.fread`

which will be rendered as :mod:`datatable`, :class:`datatable.Frame`,
:meth:`datatable.Frame.to_csv`, :func:`datatable.fread`.

The "renamed link" syntax can also be used:

.. code-block:: rst

    :func:`fread(input) <datatable.fread>`

If repeating the ``datatable.`` part is tedious, then you can add the following
declaration at the top of the page:

.. code-block:: rst

    .. py:currentmodule:: datatable


Note that some of these links may render in red. It means the documentation for
the referenced function/class/object is missing and still needs to be added:
:py:func:`datatable.missing_function()`.


Advanced directives
-------------------

All rst documents are arranged into a tree. All non-leaf nodes of this tree
must include a ``.. toctree::`` directive, which may also be declared hidden:

.. code-block:: rst

    .. toctree::
        :hidden:

        child_doc_1
        Explicit name <child_doc_2>


The ``.. image::`` directive can be used to insert an image, which may also be
a link:

.. code-block:: rst

    .. image:: <image URL>
        :target: <target URL if the image is a link>


In order to note that some functionality was added or changed in a specific
version, use:

.. code-block:: rst

    .. x-version-added:: 0.10.0

    .. x-version-deprecated:: 1.0.0

    .. x-version-changed:: 0.11.0

        Here's what changed: blah-blah-blah

The ``.. seealso::`` directive adds a Wikipedia-style "see also:" entry at the
beginning of a section. The argument of this directive should contain a link
to the content that you want the user to see. This directive is best to include
immeditately after a heading:

.. code-block:: rst

    .. seealso:: :ref:`columnsets`

Directive ``.. x-comparison-table::`` allows to create a two-column table
specifically designed for comparing two entities across multiple comparison
points. It is primarily used to create the "compare datatable with another
library" manual pages. The content of this directive is comprised of multiple
"sections" separated with ``====``, and each section has 2 or 3 parts
(separated with ``----``): an optional common header, then the content of the
first column, and then the second:

.. code-block:: rst

    .. x-comparison-table::
        :header1: datatable
        :header2: another-library

        Section 1 header
        ----
        Column 1
        ----
        Column 2

        ====
        Section 2 header
        ----
        Column 1
        ----
        Column 2


Changelog support
-----------------

RST is language that supports extensions. One of the custom extensions that we
use supports maintaining a changelog. First, the ``.. changelog::`` directive
which is used in ``releases/vN.N.N.rst`` files declares that each of those
files describes a particular release of datatable. The format is as follows:

.. code-block:: rst

    .. changelog::
        :version: <version number>
        :released: <release date>
        :wheels: URL1
                 URL2
                 etc.

        changelog content...

        .. contributors::

            N @username <full name>
            --
            N @username <full name>


The effect of this declaration is the following:

- The title of the page is automatically inserted, together with an anchor
  that can be used to refer to this page;

- A Wikipedia-style infobox is added on the right side of the page. This
  infobox contains the release date, links to the previous/next release,
  and the links to all wheels that where released at that version. The wheels
  are grouped by the python version / operating system. An sdist link may also
  be included as one of the "wheels".

- Within the ``.. changelog::`` directive, a special form of list items is
  supported:

  .. code-block:: rst

      -[new] New feature that was added

      -[enh] Improvement of an existing feature or function

      -[fix] Bug fix

      -[api] API change

  In addition, if any such item ends with the text of the form ``[#333]``,
  then this will be automatically converted into a link to a github issue/PR
  with that number.

- The ``.. contributors::`` directive can only be used inside a changelog,
  and it should list the contributors who participated in creation of this
  particular release. The list of contributors is prepared using the script
  ``ci/gh.py``



Documenting API
---------------

When it comes to documenting specific functions/classes/methods of the
``datatable`` module, we use another extension: ``.. xfunction::`` (or
``.. xclass::``, ``.. xmethod::``, etc). This is because this part of the
documentation is declared within the C++ code, so that it can be available
from within a regular python session.

Inside the documentation tree, each function/method/etc that has to be
documented is declared as follows:

.. code-block:: rst

    .. xfunction:: datatable.rbind
        :src: src/core/frame/rbind.cc py_rbind
        :doc: src/core/frame/rbind.cc doc_py_rbind
        :tests: tests/munging/test-rbind.py

Here we declare the function :func:`datatable.rbind`, whose source code is
located in file ``src/core/frame/rbind.cc`` in function ``py_rbind()``. The
docstring of this function is located in the same file in a variable
``static const char* doc_py_rbind``. The content of the latter variable will
be pre-processed and then rendered as RST. The ``:doc:`` parameter is optional,
if omitted the directive will attempt to find the docstring automatically.

The optional ``:tests:`` parameter should point to a file where the tests for
this function are located. This will be included as a link in the rendered
output.


In order to document a getter/setter property of a class, use the following:

.. code-block:: rst

    .. xdata:: datatable.Frame.key
        :src: src/core/frame/key.cc Frame::get_key Frame::set_key
        :doc: src/core/frame/key.cc doc_key
        :tests: tests/test-keys.py
        :settable: new_key
        :deletable:

The ``:src:`` parameter can now accept two function names: the getter and the
setter. In addition, the ``:settable:`` parameter will have the name of the setter
value as it will be displayed in the docs. Lastly, ``:deletable:`` marks this
class property as deletable.


The docstring of the function/method/etc is preprocessed before it is rendered
into the RST document. This processing includes the following steps:

- The "Parameters" section is parsed and the definitions of all function
  parameters are extracted.

- The contents of the "Examples" section are parsed as if it was a literal
  block, converting from python-console format into the format jupyter-style
  code blocks. In addition, if the output of any command contains a datatable
  Frame, it will also be converted into a Jupyter-style table.

- All other sections are displayed as-is.


Here's an example of a docstring:

.. code-block:: C++

    static const char* doc_rbind =
    R"(rbind(self, *frames, force=False, bynames=True)
    --

    Append rows of `frames` to the current frame.

    This method modifies the current frame in-place. If you do not want
    the current frame modified, then use the :func:`dt.rbind()` function.

    Parameters
    ----------
    frames: Frame | List[Frame]
        One or more frames to append.

    force: bool
        If True, then the frames are allowed to have mismatching set of
        columns. Any gaps in the data will be filled with NAs.

    bynames: bool
        If True (default), the columns in frames are matched by their
        names, otherwise by their order.

    Examples
    --------
    >>> DT = dt.Frame(A=[1, 2, 3], B=[4, 7, 0])
    >>> frame1 = dt.Frame(A=[-1], B=[None])
    >>> DT.rbind(frame1)
    >>> DT
       |  A   B
    -- + --  --
     0 |  1   4
     1 |  2   7
     2 |  3   0
     3 | -1  NA
    --
    [4 rows x 2 columns]
    )";
