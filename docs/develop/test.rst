
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



.. _`example website`: https://www.example.com/
