
.. _name-mangling:

=============
Name mangling
=============

Column names in a :class:`Frame` satisfy several invariants:

- they are all non-empty strings;

- within a single Frame column names must be unique;

- no column name may contain characters from the ASCII `C0 control block`_.
  This set of forbidden characters includes: the NULL character ``\0``,
  TAB character ``\t``, newline ``\n``, and similar.


If the user makes an attempt to create a Frame that would violate some of
these assumptions, then instead of failing we will attempt to **mangle** the
provided names, forcing them to satisfy the above requirements.

Given a list of column names requested by the user, the following algorithm
is used:

1. First, we check all the non-empty names in the list, from left to right.
   If a name contains characters in the range ``\x00-\x1F``, then every run
   of 1 or more such characters is replaced with a single dot.

2. Once the special characters are removed from the name, we check it against
   the set of names that were already encountered. If the current name hasn't
   been seen before, then we add it to the final list of names and proceed to
   consider the next name in the list. However, if the name was seen before,
   then it goes into the *deduplication* stage.

3. When a name needs to be deduplicated, we do the following:

   - If the name ends with a number, then split it into two parts: the ``stem``
     and the numeric suffix. Let ``count`` be the value of the numeric suffix
     plus 1;

   - If the name does not end with a number, then append a dot (``.``) to the
     name and consider this the ``stem``. For the ``count`` variable, take the
     value of option ``dt.options.frame.name_auto_index``.

   - Concatenate ``stem`` and ``count``, and check whether this name has been
     seen before. If it was, then increment ``count`` by 1, and repeat this
     step.

   - Use ``stem + count`` as this column's final name. Continue processing
     other columns.

4. Finally, re-scan the list of column names once again, this time replacing
   all the empty names. For each empty name we proceed exactly as in (3),
   using ``dt.options.frame.name_auto_prefix`` as the ``stem``, and
   ``dt.options.frame.name_auto_index`` as the initial ``count``.


Examples
--------

The default value of ``dt.options.frame.name_auto_prefix`` is ``"C"``, and the
default value of ``dt.options.frame.name_auto_index`` is ``0``. This means that
if no column names are given, they will be named as ``C0, C1, C2, ...``::

    >>> dt.Frame([[]] * 5).names
    ('C0', 'C1', 'C2', 'C3', 'C4')

If the column names contain duplicates, then they will gain a numeric suffix
(or reuse the existing suffix, if any)::

    >>> dt.Frame(names=["A", "A", "A"]).names
    ('A', 'A.0', 'A.1')
    >>> dt.Frame(names=["R3"] * 4).names
    ('R3', 'R4', 'R5', 'R6')

If some of the column names are given, while others are missing, then the
missing names will be filled as ``C0, C1, ...``::

    >>> dt.Frame(names=["A", None, "B", None]).names
    ('A', 'C0', 'B', 'C1')

When replacing the missing names, explicitly given names will have a higher
precedence and tend to retain their names::

    >>> dt.Frame(names=["A", None, "C0", "C1"]).names
    ('A', 'C2', 'C0', 'C1')

However, deduplication of the existing names happen from left to right, which
may affect the subsequent columns::

    >>> dt.Frame(names=["A1", "A1", "A2", "A3"]).names
    ('A1', 'A2', 'A3', 'A4')



.. _`C0 control block`: https://en.wikipedia.org/wiki/C0_and_C1_control_codes
