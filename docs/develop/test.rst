
Test page
=========

This is a test page, it has no useful content. It's sole purpose is to collect
various elements of markup to ensure that they render properly in the current
theme. For developers we recommend to visually check this page after any tweak
to the accompanying CSS files.

If you notice any visual irregularities somewhere else within the documentation,
please add those examples to this file as a kind of "manual test".


Inline markup
-------------

- **Bold text** is not actually corageous, it merely looks thicker.
- **Part**ially bold text;
- *Italic text* is still English, except the latters are slanted.
- ``Literal text`` is no more literal than any other text, but uses the
  monospace font.
- ``ABC``s, or ``ABC``'s?
- :kbd:`Ctrl+Alt+Del` is a keyboard shortcut (``:kbd:`` role)
- :sub:`subscript text` can be used if you need to go low (``:sub:``)
- :sup:`superscript text` but if they go low, we go high! (``:sup:``)
- :guilabel:`label` may come in handy too (``:guilabel:``)

The ``smartquotes`` Sphinx plugin is responsible for converting "straight"
quotes (``""``) into "typographic" quotes (``“”``). Similarly for the 'single'
quotes (``''`` into ``‘’``). Don't forget about single quotes in the middle of
a word, or at the end' of a word. Lastly, double-dash (``--``) should be
rendered as an n-dash -- like this, and triple-dash (``---``) as an m-dash
--- like this.

Hyperlinks may come in a variety of different flavors. In particular, links
leading outside of this website must have clear indicator that they are
external. The internal links should not have such an indicator:

- A plain hyperlink: https://www.example.com/
- A labeled hyperlink: `example website`_
- Link to a PEP document: :PEP:`574` (use ``:PEP:`` role)
- Link to a target within this page: `Bill of Rights`_
- Link to a target on another page: :ref:`join tutorial`
- Links to API objects: :class:`Frame`, :func:`fread`, :data:`dt.math.tau`


Headers
-------
This section is dedicated to headers of various levels. Note that there can be
only one level-1 header (at the top of the page). All other headers are
therefore level-2 or smaller. At the same time, headers below level 4 are not
supported.

Sub-header A
~~~~~~~~~~~~
Paragraph within a subheader. Please check that the spacing between the headers
and the text looks reasonable.

Sub-sub header A.1
^^^^^^^^^^^^^^^^^^
Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor
incididunt ut labore et dolore magna aliqua.

Sub-sub header A.2
^^^^^^^^^^^^^^^^^^
Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut
aliquip ex ea commodo consequat. Duis aute irure dolor in reprehenderit in
voluptate velit esse cillum dolore eu fugiat nulla pariatur.

Sub-header B
~~~~~~~~~~~~
Nothing to see here, move along citizen.


Lists
-----

Embedding lists into text may be somewhat tricky, for example here's the list
that contains a short enumeration of items. It is supposed to be rendered in
a "compact" style (``.simple`` in CSS):

- one
- two
- three

Same list, but ordered:

1. one
2. two
3. three

Finally, a more complicated list that is still considered "simple" by docutils
(see ``SimpleListChecker``: a list is simple if every list item contains either
a single paragraph, or a paragraph followed by a simple list). Here we exhibit
four variants of the same list, altering ordered/unordered property:

.. list-table::

  * -
      - Lorem ipsum dolor sit

        - amet
        - consectetur
        - adipiscing
        - elit,

      - sed do eiusmod

      - tempor

      - incididnut ut labore

        - et dolore
        - magna aliqua.

      - Ut enim ad minim
    -
      1. Lorem ipsum dolor sit

         - amet
         - consectetur
         - adipiscing
         - elit,

      2. sed do eiusmod

      3. tempor

      4. incididnut ut labore

         - et dolore
         - magna aliqua.

      5. Ut enim ad minim
    -
      - Lorem ipsum dolor sit

        1. amet
        2. consectetur
        3. adipiscing
        4. elit,

      - sed do eiusmod

      - tempor

      - incididnut ut labore

        1. et dolore
        2. magna aliqua.

      - Ut enim ad minim
    -
      1. Lorem ipsum dolor sit

         1. amet
         2. consectetur
         3. adipiscing
         4. elit,

      2. sed do eiusmod

      3. tempor

      4. incididnut ut labore

         1. et dolore
         2. magna aliqua.

      5. Ut enim ad minim


Compare this to the following list, which is supposed to be rendered with more
spacing between the elements:

- Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor
  incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis
  nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat.
  Duis aute irure dolor in reprehenderit in voluptate velit esse cillum dolore
  eu fugiat nulla pariatur. Excepteur sint occaecat cupidatat non proident, sunt
  in culpa qui officia deserunt mollit anim id est laborum.

- Vitae purus faucibus ornare suspendisse sed. Sit amet mauris commodo quis
  imperdiet. Id velit ut tortor pretium viverra suspendisse potenti nullam ac.
  Enim eu turpis egestas pretium aenean.

  Neque laoreet suspendisse interdum consectetur libero. Tellus elementum
  sagittis vitae et leo duis ut. Vel pretium lectus quam id leo. Eget nunc
  scelerisque viverra mauris in. Integer enim neque volutpat ac tincidunt
  vitae semper quis lectus. Urna molestie at elementum eu facilisis sed.

- Molestie at elementum eu facilisis sed. Nisi vitae suscipit tellus mauris a
  diam maecenas sed enim. Morbi tincidunt ornare massa eget egestas. Condimentum
  lacinia quis vel eros. Viverra accumsan in nisl nisi scelerisque. Lorem sed
  risus ultricies tristique. Phasellus egestas tellus rutrum tellus pellentesque
  eu tincidunt tortor aliquam. Semper feugiat nibh sed pulvinar. Quis hendrerit
  dolor magna eget est lorem ipsum dolor.

  Amet commodo nulla facilisi nullam vehicula ipsum a arcu cursus. Pellentesque
  elit eget gravida cum sociis natoque. Sit amet risus nullam eget felis eget
  nunc lobortis mattis.

  Tellus rutrum tellus pellentesque eu tincidunt tortor. Eget arcu dictum
  varius duis. Eleifend mi in nulla posuere sollicitudin aliquam ultrices
  sagittis orci.

- Ut ornare lectus sit amet est placerat in. Leo urna molestie at elementum. At
  auctor urna nunc id. Risus at ultrices mi tempus imperdiet nulla malesuada.

The next section demonstrates how different kinds of lists nest within each
other.


.. _`Bill of Rights`:

Bill of Rights
~~~~~~~~~~~~~~
The Conventions of a number of the States having at the time of their adopting
the Constitution, expressed a desire, in order to prevent misconstruction or
abuse of its powers, that further declaratory and restrictive clauses should
be added: And as extending the ground of public confidence in the Government,
will best insure the beneficent ends of its institution

Resolved by the Senate and House of Representatives of the United States of
America, in Congress assembled, two thirds of both Houses concurring, that the
following Articles be proposed to the Legislatures of the several States, as
Amendments to the Constitution of the United States, all or any of which
Articles, when ratified by three fourths of the said Legislatures, to be valid
to all intents and purposes, as part of the said Constitution; viz.:

Articles in addition to, and Amendment of the Constitution of the United States
of America, proposed by Congress, and ratified by the Legislatures of the
several States, pursuant to the fifth Article of the original Constitution.

1. Congress shall make no law respecting

   - an establishment of religion, or prohibiting the free exercise thereof; or
   - abridging the freedom of speech, or of the press; or
   - the right of the people peaceably to assemble, and to petition the
     Government for a redress of grievances.

2. A well regulated Militia, being necessary to the security of a free State,
   the right of the people to keep and bear Arms, shall not be infringed.

3. No Soldier shall, in time of peace be quartered in any house, without the
   consent of the Owner, nor in time of war, but in a manner to be prescribed
   by law.

4. The right of the people to be secure in their persons, houses, papers, and
   effects, against unreasonable searches and seizures, shall not be violated,
   and no Warrants shall issue, but upon probable cause, supported by Oath or
   affirmation, and particularly describing the place to be searched, and the
   persons or things to be seized.

5. No person shall be

   - held to answer for a capital, or otherwise infamous crime, unless on a
     presentment or indictment of a Grand Jury, except in cases arising in
     the land or naval forces, or in the Militia, when in actual service in
     time of War or public danger; nor shall any person be

   - subject for the same offence to be twice put in jeopardy of life or
     limb; nor shall be

   - compelled in any criminal case to be a witness against himself, nor be

   - deprived of

     - life,
     - liberty, or
     - property,

     without due process of law;

   - nor shall private property be taken for public use, without just
     compensation.

6. In all criminal prosecutions, the accused shall enjoy the right to a speedy
   and public trial, by an impartial jury of the State and district wherein the
   crime shall have been committed, which district shall have been previously
   ascertained by law, and to be informed of the nature and cause of the
   accusation; to be confronted with the witnesses against him; to have
   compulsory process for obtaining witnesses in his favor, and to have the
   Assistance of Counsel for his defence.

7. In Suits at common law, where the value in controversy shall exceed twenty
   dollars, the right of trial by jury shall be preserved, and no fact tried by
   a jury, shall be otherwise re-examined in any Court of the United States,
   than according to the rules of the common law.

8. Excessive bail shall not be required, nor excessive fines imposed, nor
   cruel and unusual punishments inflicted.

9. The enumeration in the Constitution, of certain rights, shall not be
   construed to deny or disparage others retained by the people.

10. The powers not delegated to the United States by the Constitution, nor
    prohibited by it to the States, are reserved to the States respectively,
    or to the people.


Code samples
------------

Literal block after a paragraph. The spacing between this text and the code
block below should be small, similar to regular spacing between lines::

    >>> import datatable as dt
    >>>
    >>> DT = dt.Frame(A = [3, 1, 4, 1, 5])
    >>> DT.shape
    (5, 1)
    >>> repr(DT)
    '<Frame#7fe06e063ca8 5x1>'
    >>> # This is how a simple frame would be rendered:
    >>> DT
       |  A
    -- + --
     0 |  3
     1 |  1
     2 |  4
     3 |  1
     4 |  5

    [5 rows x 1 column]
    >>> DT + DT
    TypeError: unsupported operand type(s) for +: 'datatable.Frame' and 'datatable.Frame'

This is a paragraph after the code block. The spacing should be roughly the
same as between regular paragraphs.

And here's an example with a keyed frame::

    >>> DT = dt.Frame({"A": [1, 2, 3, 4, 5],
    ...                "B": [4, 5, 6, 7, 8],
    ...                "C": [7, 8, 9, 10, 11],
    ...                "D": [5, 7, 2, 9, -1],
    ...                "E": ['a','b','c','d','e']})
    >>> DT.key = ['E', 'D']
    >>> DT
    E          D |     A      B      C
    str32  int32 | int32  int32  int32
    -----  ----- + -----  -----  -----
    a          5 |     1      4      7
    b          7 |     2      5      8
    c          2 |     3      6      9
    d          9 |     4      7     10
    e         -1 |     5      8     11
    [5 rows x 5 columns]


The following is a test for multi-line output from code samples::

    >>> for i in range(5):
    ...     print(1/(4 - i))
    0.25
    0.3333333333333333
    0.5
    1.0
    ZeroDivisionError: division by zero

The following is a plain piece of python code (i.e. without input/output
sections):

.. code-block:: python

    #!/usr/bin/python
    import everything as nothing

    class MyClass(object):
        r"""
        Just some sample code
        """

        def __init__(self, param1, param2):
            assert isinstance(param1, str)
            self.x = param1.lower() + "!!!"
            self.y = param2

        @classmethod
        def enjoy(cls, item):
            print(str(cls) + " likes " + item)

    if __name__ == "__main__":
        data = [MyClass('abc', 2)]
        data += [1, 123, -14, +297, 2_300_000]
        data += [True, False, None]
        data += [0x123, 0o123, 0b111]
        data += [2.71, 1.23e+45, -1.0001e-11, -math.inf]
        data += ['abc', "def", """ghijk""", b"lmnop"]
        data += [f"This is an f-string {len(data)}.\n"]
        data += [r"\w+\n?\x0280\\ [abc123]+$",
                 "\w+\n?\x0280\\ [abc123]+$",]
        data += [..., Ellipsis]

        # This cannot happen:
        if data and not data:
            assert AssertionError

Languages other than python are also supported. For example, the following
is a shell code sample (``console`` "language"):

.. code-block:: console

    $ # list all files in a directory
    $ ls -l
    total 48
    -rw-r--r--   1 pasha  staff   804B Dec 10 09:14 Makefile
    drwxr-xr-x  24 pasha  staff   768B Dec 10 09:14 api/
    -rw-r--r--   1 pasha  staff   7.1K Dec 11 14:10 conf.py
    drwxr-xr-x   7 pasha  staff   224B Dec 10 09:14 develop/
    -rw-r--r--   1 pasha  staff    62B Jul 29 14:02 docutils.conf
    -rw-r--r--   1 pasha  staff   4.2K Dec 10 09:14 index.rst
    drwxr-xr-x  12 pasha  staff   384B Dec 10 09:14 manual/
    drwxr-xr-x  19 pasha  staff   608B Dec 10 13:19 releases/
    drwxr-xr-x   6 pasha  staff   192B Dec 10 13:19 start/

    $ # Here are some more advanced syntax elements:
    $ echo "PYTHONHOME = $PYTHONHOME"
    $ export TMP=/tmp
    $ docker run -it --init -v `pwd`:/cwd ${DOCKER_CONTAINER}

RST code sample:

.. code-block:: rst

    Heading
    +++++++

    Da-da-da-daaaaa, **BOOM**.
    Da-da-da-da-da-dA-da-da-da-da-da-da-da-da,
    DA-DA-DA-DA, DA-DA DA Da, BOOM,
    DAA-DA-DA-*DAAAAAAAAAAAA*, ty-dum

    - item 1 (:func:`foo() <dt.foo>`);
    - item 2;
    - ``item 3`` is a :ref:`reference`.

    .. custom-directive-here:: not ok
        :option1: value1
        :option2: value2

        Here be dragons

    .. _`plain target`:

    There could be other |markup| elements too, such as `links`_, and
    various inline roles, eg :sup:`what's up!`. Plain URLs probably
    won't be highlighted: http://datatable.com/.

    ..
        just a comment |here|
        .. yep:: still a comment

    .. _`example website`: https://example.com/

C++ code sample:

.. code-block:: C++

    #include <cstring>

    int main() {
        return 0;
    }

SQL code sample:

.. code-block:: SQL

    SELECT * FROM students WHERE name='Robert'; DROP TABLE students; --'


Special care must be taken in case the code in the samples has very long line
lengths. Generally, the code should not overflow its container block, or
make the page wider than normal. Instead, the code block should get a
horizontal scroll bar::

    >>> days = ["Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday", "Sunday", "Extra Sunday", "Fourth Weekend"]
    >>> print(days * 2)
    ['Monday', 'Tuesday', 'Wednesday', 'Thursday', 'Friday', 'Saturday', 'Sunday', 'Extra Sunday', 'Fourth Weekend', 'Monday', 'Tuesday', 'Wednesday', 'Thursday', 'Friday', 'Saturday', 'Sunday', 'Extra Sunday', 'Fourth Weekend']

Same, but for a non-python code:

.. code-block:: rst

    Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis
    nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat.
    Duis aute irure dolor in reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla pariatur. Excepteur sint occaecat cupidatat non proident, sunt in culpa qui officia deserunt mollit anim id est laborum.



Admonitions
-----------

First, the ``.. note::`` block, which should display in a prominent box:

.. note::

    Please pay attention!

There's usually some other content coming after the note. It should be properly
spaced from the admonition.

.. note::

    Here's a note with more content. Why does it have so much content? Nobody
    knows. In theory admonitions should be short and to the point, but this one
    is not playing by the rules. It just goes on and on and on and on,
    and it seems like it would never end. Even as you think that maybe at last
    the end is near, a new paragraph suddenly appears:

    And that new paragraph just repeats the same nonsense all over again.
    Really, there is no any good reason for it to keep going, but it does
    nevertheless, as if trying to stretch the limits of how many words can be
    spilled without any of them making any sense.

.. note::

    - First, this is a note with a list
    - Second, it may contain several list items
    - Third is here just to make things look more impressive
    - Fourth is the last one (or is it?)



X-versions
----------

.. x-version-added:: 0.8.0

The ``..x-version-added`` directive usually comes as a first item after a
header, so it has reduced margins from the top, and has to have adequate
margins on the bottom to compensate.

.. x-version-deprecated:: 0.10.0

The ``..x-version-changed`` directive is a paragraph-level, and it usually has
some additional content describing what exactly has changed.

.. x-version-changed:: 0.9.0

    Nobody knows what exactly changed, but most observers agree that
    *something* did.

    While we're trying to figure out what it was, please visit the release
    notes (linked above) and see if it makes sense to you.




.. _`example website`: https://www.example.com/
