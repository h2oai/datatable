
.. _f-expressions:

``f``-expressions
=================

The ``datatable`` module exports a special symbol ``f``, which can be used
to refer to the columns of a frame currently being operated on. If this sounds
cryptic, consider that the most common way to operate on a frame is via the
square-bracket call ``DT[i, j, by, ...]``. It is often the case that within
this expression you would want to refer to individual columns of the frame:
either to create a filter, a transform, or specify a grouping variable, etc.
In all such cases the ``f`` symbol is used, and it is considered to be
evaluated within the context of the frame ``DT``.

For example, consider the expression::

    >>> f.price

By itself, it just means *a column named "price", in an unspecified frame*.
This expression becomes concrete, however, when used with a particular frame.
For example::

    >>> train_dt[f.price > 0, :]

selects all rows in ``train_dt`` where the price is positive. Thus, within the
call to ``train_dt[...]``, the symbol ``f`` refers to the frame ``train_dt``.

The standalone f-expression may occasionally be useful too: it can be saved in
a variable and then re-applied to several different frames. Each time ``f``
will refer to the frame to which it is being applied::

    >>> price_filter = (f.price > 0)
    >>> train_filtered = train_dt[price_filter, :]
    >>> test_filtered = test_dt[price_filter, :]

The simple expression ``f.price`` can be saved in a variable too. In fact,
there is a Frame helper method ``.export_names()`` which does exactly that:
returns a tuple of variables for each column name in the frame, allowing you to
omit the ``f.`` prefix::

    >>> Id, Price, Quantity = DT.export_names()
    >>> DT[:, [Id, Price, Quantity, Price * Quantity]]



Single-column selector
----------------------

As you have seen, the expression ``f.NAME`` refers to a column called "NAME".
This notation is handy, but not universal. What do you do if the column's name
contains spaces or unicode characters? Or if a column's name is not known, only
its index? Or if the name is in a variable? For these purposes ``f`` supports
the square-bracket selectors::

    >>> f[-1]           # select the last column
    >>> f["Price ($)"]  # select column names "Price ($)"

Generally, ``f[i]`` means either the column at index ``i`` if ``i`` is an
integer, or the column with name ``i`` if ``i`` is a string.

Using an integer index follows the standard Python rule for list subscripts:
negative indices are interpreted as counting from the end of the frame, and
requesting a column with an index outside of ``[-ncols; ncols)`` will raise
an error.

This square-bracket form is also useful when you want to access a column
dynamically, i.e. if its name is not known in advance. For example, suppose
there is a frame with columns ``"2017_01"``, ``"2017_02"``, ..., ``"2019_12"``.
Then all these columns can be addressed as::

    >>> [f["%d_%02d" % (year, month)]
    ...  for month in range(1, 13)
    ...  for year in [2017, 2018, 2019]]


.. _`columnsets`:

Multi-column selector
---------------------

In the previous section you have seen that ``f[i]`` refers to a single column
when ``i`` is either an integer or a string. However we alo support the case
when ``i`` is a slice or a type::

    >>> f[:]          # select all columns
    >>> f[::-1]       # select all columns in reverse order
    >>> f[:5]         # select the first 5 columns
    >>> f[3:4]        # select the fourth column
    >>> f["B":"H"]    # select columns from B to H, inclusive
    >>> f[int]        # select all integer columns
    >>> f[float]      # select all floating-point columns
    >>> f[dt.str32]   # select all columns with stype `str32`
    >>> f[None]       # select no columns (empty columnset)

In all these cases a *columnset* is returned. This columnset may contain a
variable number of columns or even no columns at all, depending on the frame
to which this f-expression is applied.

Applying a slice to symbol ``f`` follows the same semantics as if ``f`` was a
list of columns. Thus ``f[:10]`` means the first 10 columns of a frame, or all
columns if the frame has less than 10. Similarly, ``f[9:10]`` selects the 10th
column of a frame if it exists, or nothing if the frame has less than 10
columns. Compare this to selector ``f[9]``, which also selects the 10th column
of a frame if it exists, but throws an exception if it doesn't.

Besides the usual numeric ranges, you can also use name ranges. These ranges
include the first named column, the last named column, and all columns in
between. It is not possible to mix positional and named columns in a range,
and it is not possible to specify a step. If the range is ``x:y``, yet column
``x`` comes after ``y`` in the frame, then the columns will be selected in the
reverse order: first ``x``, then the column preceding ``x``, and so on, until
column ``y`` is selected last::

    >>> f["C1":"C9"]   # Select columns from C1 up to C9
    >>> f["C9":"C1"]   # Select columns C9, C8, C7, ..., C2, C1
    >>> f[:"C3"]       # Select all columns up to C3
    >>> f["C5":]       # Select all columns after C5

Finally, you can select all columns of a particular type by using that type
as an f-selector. You can pass either common python types ``bool``, ``int``,
``float``, ``str``; or you can pass an stype such as ``dt.int32``, or an ltype such as
``dt.ltype.obj``. You can also pass `None` to not select any columns. By itself
this may not be very useful, but occasionally you may need this as a fallback
in conditional expressions::

    >>> f[int if select_types == "integer" else
    ...   float if select_types == "floating" else
    ...   None]  # otherwise don't select any columns

A columnset can be used in situations where a sequence of columns is expected,
such as:

- the ``j`` node of ``DT[i,j,...]``;
- within ``by()`` and ``sort()`` functions;
- with certain functions that operate on sequences of columns: ``rowsum()``,
  ``rowmean``, ``rowmin``, etc;
- many other functions that normally operate on a single column will
  automatically map over all columns in columnset::

    >>> sum(f[:])       # equivalent to [sum(f[i]) for i in range(DT.ncols)]
    >>> f[:3] + f[-3:]  # same as [f[0]+f[-3], f[1]+f[-2], f[2]+f[-1]]

.. x-version-added:: 0.10.0


Modifying a columnset
---------------------

Columnsets support operations that either add or remove elements from the set.
This is done using methods ``.extend()`` and ``.remove()``.

The ``.extend()`` method takes a columnset as an argument (also a list, or dict,
or sequence of columns) and produces a new columnset containing both the
original and the new columns. The columns need not be unique: the same column
may appear multiple times in a columnset. This method allows to add transformed
columns into the columnset as well::

    >>> f[int].extend(f[float])          # integer and floating-point columns
    >>> f[:3].extend(f[-3:])             # the first and the last 3 columns
    >>> f.A.extend(f.B)                  # columns "A" and "B"
    >>> f[str].extend(dt.str32(f[int]))  # string columns, and also all integer
    >>>                                  # columns converted to strings
    >>> # All columns, and then one additional column named 'cost', which contains
    >>> # column `price` multiplied by `quantity`:
    >>> f[:].extend({"cost": f.price * f.quantity})

When a columnset is extended, the order of the elements is preserved. Thus, a
columnset is closer in functionality to a python list than to a set. In
addition, some of the elements in a columnset can have names if the columnset
is created from a dictionary. The names may be non-unique too.

The ``.remove()`` method is the opposite of ``.extend()``: it takes an existing
columnset and then removes all columns that are passed as the argument::

    >>> f[:].remove(f[str])    # all columns except columns of type string
    >>> f[:10].remove(f.A)     # the first 10 columns without column "A"
    >>> f[:].remove(f[3:-3])   # same as `f[:3].extend(f[-3:])`, at least in the
    >>>                        # context of a frame with 6+ columns

Removing a column that is not in the columnset is not considered an error,
similar to how set-difference operates. Thus, ``f[:].remove(f.A)`` may be
safely applied to a frame that doesn't have column "A": the columns that cannot
be removed are simply ignored.

If a columnset includes some column several times, and then you request to
remove that column, then only the first occurrence in the sequence will be
removed. Generally, the multiplicity of some column "A" in columnset
``cs1.remove(cs2)`` will be equal to the multiplicity of "A" in ``cs1`` minus the
multiplicity of "A" in ``cs2``, or 0 if such difference would be negative.
Thus,::

    >>> f[:].extend(f[int]).remove(f[int])

will have the effect of moving all integer columns to the end of the columnset
(since ``.remove()`` removes the first occurrence of a column it finds).

It is not possible to remove a transformed column from a columnset. An error
will be thrown if the argument of ``.remove()`` contains any transformed
columns.


.. x-version-added:: 0.10.0

