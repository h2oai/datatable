
`f`-expressions
===============

The ``datatable`` module exports a special symbol ``f``, which is a shortcut
for "the frame currently being operated on". This symbol can be used to
refer to individual columns in a frame. For example, the expression::

    f.price

refers to a column named "price". Note that, devoid of context, this expression
is abstract. It can only gain a concrete meaning when applied within the context
of a particular frame. For example,::

    train_df[f.price > 0, :]

selects all rows in ``train_df`` where the price is positive. Thus, within the
call to ``train_df[...]``, the symbol ``f`` refers to the frame ``train_df``.

If the same expression is re-applied to several different frames, then ``f``
will refer to each of those frames in turn::

    filter = (f.price > 0)
    train_filtered = train[filter, :]
    test_filtered = test[filter, :]


Single-column selector
----------------------

As you have seen, the expression ``f.NAME`` can be used to refer to a column
called "NAME". This notation doesn't work, however, if the column's name is
not a valid python identifier; or if a column must be selected by its index.
For these purposes the symbol ``f`` supports the square-bracket selectors::

    f[-1]
    f["Price ($)"]

Generally, ``f[i]`` means either the column at index ``i`` (if ``i`` is an
integer), or the column with name ``i`` (if ``i`` is a string).

This square-bracket form is also useful when you want to access a column
dynamically, i.e. if its name is not known in advance. For example, suppose
there is a frame with columns "2017_01", "2017_02", ..., "2019_12". Then
such columns can be addressed as::

    [f["%d_%02d" % (year, month)]
     for month in range(1, 13)
     for year in [2017, 2018, 2019]]


.. _`columnsets`:

Multi-column selector
---------------------

In the previous section you have seen that ``f[i]`` resolves to a single
column when ``i`` is either an integer or a string. However we alo support
the case when ``i`` is a slice, or a type::

    f[:]          # select all columns
    f[::-1]       # select all columns in reverse order
    f[:5]         # select the first 5 columns
    f[3:4]        # select the fourth column
    f["B":"H"]    # select columns from B to H, inclusive
    f[int]        # select all integer columns
    f[float]      # select all floating-point columns
    f[dt.str32]   # select all columns with stype `str32`
    f[None]       # select no columns (empty columnset)

In all these cases a *columnset* is returned. Such columnset may contain 0 or
more columns, depending upon the frame to which it is applied.

A columnset can be used in situations where a sequence of columns is expected,
such as:

- the ``i`` node of ``DT[i,j,...]```;
- within ``by()`` and ``sort()`` functions;
- with certain functions that operate on a sequence of columns: ``rowsum()``,
  ``rowmean``, ``rowmin``, etc;
- many other functions that normally operate on a single column will
  automatically map over all columns in columnset::

    sum(f[:])       # equivalent to [sum(f[0]), sum(f[1]), ...]
    f[:3] + f[-3:]  # same as [f[0]+f[-3], f[1]+f[-2], f[2]+f[-1]]

.. versionadded:: 0.10.0


Modifying a columnset
---------------------

Columnsets support operations that either add or remove elements from the set.
This is done using methods ``.extend()`` and ``.remove()``.

The ``.extend()`` method takes a columnset as an argument (also a list, or dict,
or sequence of columns) and produces a new columnset containing both the
original and the new columns. The columns need not be unique: the same column
may appear multiple times in a columnset. This method allows to add transformed
columns into the columnset as well::

    f[int].extend(f[float])          # integer and floating-point columns
    f[:3].extend(f[-3:])             # the first and the last 3 columns
    f.A.extend(f.B)                  # columns "A" and "B"
    f[str].extend(dt.str32(f[int]))  # string columns, and also all integer
                                     # columns converted to strings
    # All columns, and then one additional column named 'cost', which contains
    # column `price` multiplied by `quantity`:
    f[:].extend({"cost": f.price * f.quantity})

When a columnset is extended, the order of the elements is preserved. Thus, a
columnset is closer in functionality to a python list than to a set. In
addition, some of the elements in a columnset can have names, if the columnset
is created from a dictionary. The names may be non-unique too.

The ``.remove()`` method is the opposite of ``.extend()``: it takes an existing
columnset and then removes all columns that are passed as the argument::

    f[:].remove(f[str])    # all columns except columns of type string
    f[:10].remove(f.A)     # the first 10 columns without column "A"
    f[:].remove(f[3:-3])   # same as `f[:3].extend(f[-3:])`, at least in the
                           # context of a frame with 6+ columns

Removing a column that is not in the columnset is not considered an error,
similar to how set-difference operates. Thus, ``f[:].remove(f.A)`` may be
safely applied to a frame that doesn't have column "A": the columns that cannot
be removed are simply ignored.

If a columnset includes some column several times, and then you request to
remove that column, then only the first occurrence in the sequence will be
removed. Generally, the multiplicity of some column "A" in columnset
``cs1.remove(cs2)`` will be equal the multiplicity of "A" in ``cs1`` minus the
multiplicity of "A" in ``cs2``, or 0 if such difference would be negative.
Thus,::

    f[:].extend(f[int]).remove(f[int])

will have the effect of moving all integer columns to the end of the columnset
(since ``.remove()`` removes the first occurrence of a column it finds).

It is not possible to remove a transformed column from a columnset. An error
will be thrown if the argument of ``.remove()`` contains any transformed
columns.


.. versionadded:: 0.10.0

