
Transforming Data
=================

This section focuses on column operation - creating or modifying columns within a Frame. Most of the transformation operations occur via the `j` section in the ``DT[i, j, ...]`` syntax.

Creating Columns
----------------

    >>> from datatable import dt, f, by, update, ifelse, Type
    >>> from datetime import date
    >>>
    >>> source = {"dates" : [date(2000, 1, 5), date(2010, 11, 23), date(2020, 2, 29), None],
    ...           "integers" : range(1, 5),
    ...           "floats" : [10.0, 11.5, 12.3, -13],
    ...           "strings" : ['A', 'B', None, 'D']
    ...           }
    >>> DT = dt.Frame(source)
    >>> DT
       | dates       integers   floats  strings
       | date32         int32  float64  str32
    -- + ----------  --------  -------  -------
     0 | 2000-01-05         1     10    A
     1 | 2010-11-23         2     11.5  B
     2 | 2020-02-29         3     12.3  NA
     3 | NA                 4    -13    D
    [4 rows x 4 columns]

Columns can be created via a number of options:


By assignment
^^^^^^^^^^^^^

    >>> DT['double'] = DT[:, f.integers * 2]
    >>> DT
       | dates       integers   floats  strings  double
       | date32         int32  float64  str32     int32
    -- + ----------  --------  -------  -------  ------
     0 | 2000-01-05         1     10    A             2
     1 | 2010-11-23         2     11.5  B             4
     2 | 2020-02-29         3     12.3  NA            6
     3 | NA                 4    -13    D             8
    [4 rows x 5 columns]



Via the `extend` method
^^^^^^^^^^^^^^^^^^^^^^^

    >>> DT = DT[:, f[:].extend({"concat":f.strings + '_tail'})]
    >>> DT
       | dates       integers   floats  strings  double  concat
       | date32         int32  float64  str32     int32  str32 
    -- + ----------  --------  -------  -------  ------  ------
     0 | 2000-01-05         1     10    A             2  A_tail
     1 | 2010-11-23         2     11.5  B             4  B_tail
     2 | 2020-02-29         3     12.3  NA            6  NA    
     3 | NA                 4    -13    D             8  D_tail
    [4 rows x 6 columns]


Via :func:`update`
^^^^^^^^^^^^^^^^^^
This is an inplace operation; unlike the previous two, an assignment is not required::

    >>> DT[:, update(month = dt.time.month(f.dates))]
    >>> DT
       | dates       integers   floats  strings  double  concat  month
       | date32         int32  float64  str32     int32  str32   int32
    -- + ----------  --------  -------  -------  ------  ------  -----
     0 | 2000-01-05         1     10    A             2  A_tail      1
     1 | 2010-11-23         2     11.5  B             4  B_tail     11
     2 | 2020-02-29         3     12.3  NA            6  NA          2
     3 | NA                 4    -13    D             8  D_tail     NA
    [4 rows x 7 columns]

Multiple columns can be added in one go:: 

    >>> data = {"A": [1, 2, 3, 4, 5],
    ...         "B": [4, 5, 6, 7, 8],
    ...         "C": [7, 8, 9, 10, 11],
    ...         "D": [5, 7, 2, 9, -1]}
    >>>
    >>> DF = dt.Frame(data)
    >>> DF
       |     A      B      C      D
       | int32  int32  int32  int32
    -- + -----  -----  -----  -----
     0 |     1      4      7      5
     1 |     2      5      8      7
     2 |     3      6      9      2
     3 |     4      7     10      9
     4 |     5      8     11     -1
    [5 rows x 4 columns]

    >>> # via assignment
    >>> DF[:, ['E', 'F']] = DF[:, [f.A*2, f.B-4]]
    >>> DF
       |     A      B      C      D      E      F
       | int32  int32  int32  int32  int32  int32
    -- + -----  -----  -----  -----  -----  -----
     0 |     1      4      7      5      2      0
     1 |     2      5      8      7      4      1
     2 |     3      6      9      2      6      2
     3 |     4      7     10      9      8      3
     4 |     5      8     11     -1     10      4
    [5 rows x 6 columns]


    >>> # via extend
    >>> DF = DF[:, f[:].extend({"string"  : dt.as_type(f.A, str), 
    ...                         "integers": range(5)})]
    >>> DF
       |     A      B      C      D      E      F  string  integers
       | int32  int32  int32  int32  int32  int32  str32      int32
    -- + -----  -----  -----  -----  -----  -----  ------  --------
     0 |     1      4      7      5      2      0  1              0
     1 |     2      5      8      7      4      1  2              1
     2 |     3      6      9      2      6      2  3              2
     3 |     4      7     10      9      8      3  4              3
     4 |     5      8     11     -1     10      4  5              4
    [5 rows x 8 columns]


    >>> # via update
    >>> DF[:, update(G = f.D, letters = "letters")]
    >>> DF
       |     A      B      C      D      E      F  string  integers      G  letters
       | int32  int32  int32  int32  int32  int32  str32      int32  int32  str32  
    -- + -----  -----  -----  -----  -----  -----  ------  --------  -----  -------
     0 |     1      4      7      5      2      0  1              0      5  letters
     1 |     2      5      8      7      4      1  2              1      7  letters
     2 |     3      6      9      2      6      2  3              2      2  letters
     3 |     4      7     10      9      8      3  4              3      9  letters
     4 |     5      8     11     -1     10      4  5              4     -1  letters
    [5 rows x 10 columns]

Mutate Existing Columns
-----------------------
The assignment and :func:`update()` options can be used to mutate existing columns::

    >>> # via assignment
    >>> DT['double'] = DT[:, f.double ** 2]
    >>> DT
       | dates       integers   floats  strings   double  concat  month
       | date32         int32  float64  str32    float64  str32   int32
    -- + ----------  --------  -------  -------  -------  ------  -----
     0 | 2000-01-05         1     10    A              4  A_tail      1
     1 | 2010-11-23         2     11.5  B             16  B_tail     11
     2 | 2020-02-29         3     12.3  NA            36  NA          2
     3 | NA                 4    -13    D             64  D_tail     NA
    [4 rows x 7 columns]

    >>> # via update
    >>> DT[:, update(concat = f.concat[:1])]
    >>> DT
       | dates       integers   floats  strings   double  concat  month
       | date32         int32  float64  str32    float64  str32   int32
    -- + ----------  --------  -------  -------  -------  ------  -----
     0 | 2000-01-05         1     10    A              4  A           1
     1 | 2010-11-23         2     11.5  B             16  B          11
     2 | 2020-02-29         3     12.3  NA            36  NA          2
     3 | NA                 4    -13    D             64  D          NA
    [4 rows x 7 columns]

The extend method adds new columns to the Frame; it does not change existing columns.

Operations between Columns
--------------------------



































































    >>> DT[:, dt.Type.float64]
       |  floats
       | float64
    -- + -------
     0 |    10
     1 |    11.5
     2 |    12.3
     3 |   -13
    [4 rows x 1 column]

    >>> DT[:, dt.Type.date32]
       | dates
       | date32
    -- + ----------
     0 | 2000-01-05
     1 | 2010-11-23
     2 | 2020-02-29
     3 | NA
    [4 rows x 1 column]

A list of types can be selected as well::

    >>> DT[:, [date, str]]
       | dates       strings
       | date32      str32
    -- + ----------  -------
     0 | 2000-01-05  A
     1 | 2010-11-23  B
     2 | 2020-02-29  NA
     3 | NA          D
    [4 rows x 2 columns]


By list
^^^^^^^

Using a list allows for selection of multiple columns::

    >>> DT[:, ['integers', 'strings']]
       | integers  strings
       |    int32  str32
    -- + --------  -------
     0 |        1  A
     1 |        2  B
     2 |        3  NA
     3 |        4  D
    [4 rows x 2 columns]

A tuple of selectors is also allowed, although not recommended from stylistic
perspective::

    >>> DT[:, (-3, 2, 3)]
       | integers   floats  strings
       |    int32  float64  str32
    -- + --------  -------  -------
     0 |        1     10    A
     1 |        2     11.5  B
     2 |        3     12.3  NA
     3 |        4    -13    D
    [4 rows x 3 columns]

Selection via `list comprehension`_/`generator expression`_ is possible::

    >>> DT[:, [num for num in range(DT.ncols) if num % 2 == 0]]
       | dates        floats
       | date32      float64
    -- + ----------  -------
     0 | 2000-01-05     10
     1 | 2010-11-23     11.5
     2 | 2020-02-29     12.3
     3 | NA            -13
    [4 rows x 2 columns]

Selecting columns via a mix of column names and positions (integers) is not
allowed::

    >>> DT[:, ['dates', 2]]
    TypeError: Mixed selector types are not allowed. Element 1 is of type integer, whereas the previous element(s) were of type string


Via slicing
^^^^^^^^^^^
When slicing with strings, both the ``start`` and ``end`` column names are
included in the returned frame::

    >>> DT[:, 'dates':'strings']
       | dates       integers   floats  strings
       | date32         int32  float64  str32
    -- + ----------  --------  -------  -------
     0 | 2000-01-05         1     10    A
     1 | 2010-11-23         2     11.5  B
     2 | 2020-02-29         3     12.3  NA
     3 | NA                 4    -13    D
    [4 rows x 4 columns]

However, when slicing via position, the columns are returned up to, but not
including the final position; this is similar to the slicing pattern for
Python's sequences::

    >>> DT[:, 1:3]
       | integers   floats
       |    int32  float64
    -- + --------  -------
     0 |        1     10
     1 |        2     11.5
     2 |        3     12.3
     3 |        4    -13
    [4 rows x 2 columns]

    >>> DT[:, ::-1]
       | strings   floats  integers  dates
       | str32    float64     int32  date32
    -- + -------  -------  --------  ----------
     0 | A           10           1  2000-01-05
     1 | B           11.5         2  2010-11-23
     2 | NA          12.3         3  2020-02-29
     3 | D          -13           4  NA
    [4 rows x 4 columns]

It is possible to select columns via slicing, even if the indices are not in
the Frame::

    >>> DT[:, 3:10]  # there are only four columns in the Frame
       | strings
       | str32
    -- + -------
     0 | A
     1 | B
     2 | NA
     3 | D
    [4 rows x 1 column]

Unlike with integer slicing, providing a name of the column that is not in the Frame
will result in an error::

    >>> DT[:, "integers" : "categoricals"]
    KeyError: Column categoricals does not exist in the Frame

Slicing is also possible with the standard ``slice`` function::

    >>> DT[:, slice('integers', 'strings')]
       | integers   floats  strings
       |    int32  float64  str32
    -- + --------  -------  -------
     0 |        1     10    A
     1 |        2     11.5  B
     2 |        3     12.3  NA
     3 |        4    -13    D
    [4 rows x 3 columns]

With the ``slice`` function, multiple slicing on the columns is possible::

    >>> DT[:, [slice("dates", "integers"), slice("floats", "strings")]]
       | dates       integers   floats  strings
       | date32         int32  float64  str32
    -- + ----------  --------  -------  -------
     0 | 2000-01-05         1     10    A
     1 | 2010-11-23         2     11.5  B
     2 | 2020-02-29         3     12.3  NA
     3 | NA                 4    -13    D
    [4 rows x 4 columns]

    >>> DT[:, [slice("integers", "dates"), slice("strings", "floats")]]
       | integers  dates       strings   floats
       |    int32  date32      str32    float64
    -- + --------  ----------  -------  -------
     0 |        1  2000-01-05  A           10
     1 |        2  2010-11-23  B           11.5
     2 |        3  2020-02-29  NA          12.3
     3 |        4  NA          D          -13
    [4 rows x 4 columns]

Slicing on strings can be combined with column names during selection::

    >>> DT[:, [slice("integers", "dates"), "strings"]]
       | integers  dates       strings
       |    int32  date32      str32
    -- + --------  ----------  -------
     0 |        1  2000-01-05  A
     1 |        2  2010-11-23  B
     2 |        3  2020-02-29  NA
     3 |        4  NA          D
    [4 rows x 3 columns]

But not with integers::

    >>> DT[:, [slice("integers", "dates"), 1]]
    TypeError: Mixed selector types are not allowed. Element 1 is of type integer, whereas the previous element(s) were of type string

Slicing on position can be combined with column position::

    >>> DT[:, [slice(1, 3), 0]]
       | integers   floats  dates
       |    int32  float64  date32
    -- + --------  -------  ----------
     0 |        1     10    2000-01-05
     1 |        2     11.5  2010-11-23
     2 |        3     12.3  2020-02-29
     3 |        4    -13    NA
    [4 rows x 3 columns]

But not with strings::

    >>> DT[:, [slice(1, 3), "dates"]]
    TypeError: Mixed selector types are not allowed. Element 1 is of type string, whereas the previous element(s) were of type integer


Via booleans
^^^^^^^^^^^^

When selecting via booleans, the sequence length must be equal to the number
of columns in the frame::

    >>> DT[:, [True, True, False, False]]
       | dates       integers
       | date32         int32
    -- + ----------  --------
     0 | 2000-01-05         1
     1 | 2010-11-23         2
     2 | 2020-02-29         3
     3 | NA                 4
    [4 rows x 2 columns]

Booleans generated from a `list comprehension`_/`generator expression`_ allow
for nifty selections::

    >>> DT[:, ["i" in name for name in DT.names]]
       | integers  strings
       |    int32  str32
    -- + --------  -------
     0 |        1  A
     1 |        2  B
     2 |        3  NA
     3 |        4  D
    [4 rows x 2 columns]

In this example we want to select columns that are numeric (integers or floats)
and whose average is greater than 3::

    >>> DT[:, [column.type.is_numeric 
    ...        and column.mean1() > 3 
    ...        for column in DT]]
       |  floats
       | float64
    -- + -------
     0 |    10
     1 |    11.5
     2 |    12.3
     3 |   -13
    [4 rows x 1 column]


Via :ref:`f-expressions`
^^^^^^^^^^^^^^^^^^^^^^^^
All the selection options above (except boolean) are also possible via :ref:`f-expressions`::

    >>> DT[:, f.dates]
       | dates
       | date32
    -- + ----------
     0 | 2000-01-05
     1 | 2010-11-23
     2 | 2020-02-29
     3 | NA
    [4 rows x 1 column]

    >>> DT[:, f[-1]]
       | strings
       | str32
    -- + -------
     0 | A
     1 | B
     2 | NA
     3 | D
    [4 rows x 1 column]

    >>> DT[:, f['integers':'strings']]
       | integers   floats  strings
       |    int32  float64  str32
    -- + --------  -------  -------
     0 |        1     10    A
     1 |        2     11.5  B
     2 |        3     12.3  NA
     3 |        4    -13    D
    [4 rows x 3 columns]

    >>> DT[:, f['integers':]]
       | integers   floats  strings
       |    int32  float64  str32
    -- + --------  -------  -------
     0 |        1     10    A
     1 |        2     11.5  B
     2 |        3     12.3  NA
     3 |        4    -13    D
    [4 rows x 3 columns]

    >>> DT[:, f[1::-1]]
       | integers  dates
       |    int32  date32
    -- + --------  ----------
     0 |        1  2000-01-05
     1 |        2  2010-11-23
     2 |        3  2020-02-29
     3 |        4  NA
    [4 rows x 2 columns]

    >>> DT[:, f[date, int, float]]
       | dates       integers   floats
       | date32         int32  float64
    -- + ----------  --------  -------
     0 | 2000-01-05         1     10
     1 | 2010-11-23         2     11.5
     2 | 2020-02-29         3     12.3
     3 | NA                 4    -13
    [4 rows x 3 columns]

    >>> DT[:, f["dates":"integers", "floats":"strings"]]
       | dates       integers   floats  strings
       | date32         int32  float64  str32
    -- + ----------  --------  -------  -------
     0 | 2000-01-05         1     10    A
     1 | 2010-11-23         2     11.5  B
     2 | 2020-02-29         3     12.3  NA
     3 | NA                 4    -13    D
    [4 rows x 4 columns]


.. note::

    If the columns names are python keywords (``def``, ``del``, ...), the dot
    notation is not possible with :ref:`f-expressions`; you have to use
    the brackets notation to access these columns.

.. note::

    Selecting columns with ``DT[:, f[None]]`` returns an empty Frame. This is
    different from ``DT[:, None]``, which currently returns all the columns.
    The behavior of ``DT[:, None]`` may change in the future::

        >>> DT[:, None]
           | dates       integers   floats  strings
           | date32         int32  float64  str32
        -- + ----------  --------  -------  -------
         0 | 2000-01-05         1     10    A
         1 | 2010-11-23         2     11.5  B
         2 | 2020-02-29         3     12.3  NA
         3 | NA                 4    -13    D
        [4 rows x 4 columns]

        >>> DT[:, f[None]]
           |
           |
        -- +
         0 |
         1 |
         2 |
         3 |
        [4 rows x 0 columns]



Selecting Data -- Rows
----------------------
There are a number of ways to select rows of data via the ``i`` section.

.. note:: The index labels in a :class:`Frame` are just for aesthetics; they
  serve no actual purpose during selection.


By Position
^^^^^^^^^^^
Only integer values are acceptable::

    >>> DT[0, :]
       | dates       integers   floats  strings
       | date32         int32  float64  str32
    -- + ----------  --------  -------  -------
     0 | 2000-01-05         1       10  A
    [1 row x 4 columns]

    >>> DT[-1, :]  # last row
       | dates   integers   floats  strings
       | date32     int32  float64  str32
    -- + ------  --------  -------  -------
     0 | NA             4      -13  D
    [1 row x 4 columns]


Via Sequence of Positions
^^^^^^^^^^^^^^^^^^^^^^^^^

Any acceptable sequence of positions is applicable here. Listed below are some
of these sequences.

- List (tuple)::

    >>> DT[[1, 2, 3], :]
       | dates       integers   floats  strings
       | date32         int32  float64  str32
    -- + ----------  --------  -------  -------
     0 | 2010-11-23         2     11.5  B
     1 | 2020-02-29         3     12.3  NA
     2 | NA                 4    -13    D
    [3 rows x 4 columns]

- An integer `numpy`_ 1-D Array::

    >>> DT[np.arange(3), :]
       | dates       integers   floats  strings
       | date32         int32  float64  str32
    -- + ----------  --------  -------  -------
     0 | 2000-01-05         1     10    A
     1 | 2010-11-23         2     11.5  B
     2 | 2020-02-29         3     12.3  NA
    [3 rows x 4 columns]

- A one column integer Frame::

    >>> DT[dt.Frame([1, 2, 3]), :]
       | dates       integers   floats  strings
       | date32         int32  float64  str32
    -- + ----------  --------  -------  -------
     0 | 2010-11-23         2     11.5  B
     1 | 2020-02-29         3     12.3  NA
     2 | NA                 4    -13    D
    [3 rows x 4 columns]

- An integer `pandas Series`_::

    >>> DT[pd.Series([1, 2, 3]), :]
       | dates       integers   floats  strings
       | date32         int32  float64  str32
    -- + ----------  --------  -------  -------
     0 | 2010-11-23         2     11.5  B
     1 | 2020-02-29         3     12.3  NA
     2 | NA                 4    -13    D
    [3 rows x 4 columns]

- A python `range`_::

    >>> DT[range(1, 3), :]
       | dates       integers   floats  strings
       | date32         int32  float64  str32
    -- + ----------  --------  -------  -------
     0 | 2010-11-23         2     11.5  B
     1 | 2020-02-29         3     12.3  NA
    [2 rows x 4 columns]

- A `generator expression`_::

    >>> DT[(num for num in range(4)), :]
       | dates       integers   floats  strings
       | date32         int32  float64  str32
    -- + ----------  --------  -------  -------
     0 | 2000-01-05         1     10    A
     1 | 2010-11-23         2     11.5  B
     2 | 2020-02-29         3     12.3  NA
     3 | NA                 4    -13    D
    [4 rows x 4 columns]

If the position passed to ``i`` does not exist, an error is raised

    >>> DT[(num for num in range(7)), :]
    ValueError: Index 4 is invalid for a Frame with 4 rows


The `set`_ sequence is not acceptable in the ``i`` or ``j`` sections.

Except for ``lists``/``tuples``, all the other sequence types passed into
the ``i`` section can only contain positive integers.


Via booleans
^^^^^^^^^^^^

When selecting rows via boolean sequence, the length of the sequence must be
the same as the number of rows::

    >>> DT[[True, True, False, False], :]
       | dates       integers   floats  strings
       | date32         int32  float64  str32
    -- + ----------  --------  -------  -------
     0 | 2000-01-05         1     10    A
     1 | 2010-11-23         2     11.5  B
    [2 rows x 4 columns]

    >>> DT[(n%2 == 0 for n in range(DT.nrows)), :]
       | dates       integers   floats  strings
       | date32         int32  float64  str32
    -- + ----------  --------  -------  -------
     0 | 2000-01-05         1     10    A
     1 | 2020-02-29         3     12.3  NA
    [2 rows x 4 columns]


Via slicing
^^^^^^^^^^^

Slicing works similarly to slicing a python ``list``::

    >>> DT[1:3, :]
       | dates       integers   floats  strings
       | date32         int32  float64  str32
    -- + ----------  --------  -------  -------
     0 | 2010-11-23         2     11.5  B
     1 | 2020-02-29         3     12.3  NA
    [2 rows x 4 columns]

    >>> DT[::-1, :]
       | dates       integers   floats  strings
       | date32         int32  float64  str32
    -- + ----------  --------  -------  -------
     0 | NA                 4    -13    D
     1 | 2020-02-29         3     12.3  NA
     2 | 2010-11-23         2     11.5  B
     3 | 2000-01-05         1     10    A
    [4 rows x 4 columns]

    >>> DT[-1:-3:-1, :]
       | dates       integers   floats  strings
       | date32         int32  float64  str32
    -- + ----------  --------  -------  -------
     0 | NA                 4    -13    D
     1 | 2020-02-29         3     12.3  NA
    [2 rows x 4 columns]

Slicing is also possible with the ``slice`` function::

    >>> DT[slice(1, 3), :]
       | dates       integers   floats  strings
       | date32         int32  float64  str32
    -- + ----------  --------  -------  -------
     0 | 2010-11-23         2     11.5  B
     1 | 2020-02-29         3     12.3  NA
    [2 rows x 4 columns]

It is possible to select rows with multiple slices. Let's increase the number
of rows in the Frame::

    >>> DT = dt.repeat(DT, 3)
    >>> DT
       | dates       integers   floats  strings
       | date32         int32  float64  str32
    -- + ----------  --------  -------  -------
     0 | 2000-01-05         1     10    A
     1 | 2010-11-23         2     11.5  B
     2 | 2020-02-29         3     12.3  NA
     3 | NA                 4    -13    D
     4 | 2000-01-05         1     10    A
     5 | 2010-11-23         2     11.5  B
     6 | 2020-02-29         3     12.3  NA
     7 | NA                 4    -13    D
     8 | 2000-01-05         1     10    A
     9 | 2010-11-23         2     11.5  B
    10 | 2020-02-29         3     12.3  NA
    11 | NA                 4    -13    D
    [12 rows x 4 columns]

    >>> DT[[slice(1, 3), slice(5, 8)], :]
       | dates       integers   floats  strings
       | date32         int32  float64  str32
    -- + ----------  --------  -------  -------
     0 | 2010-11-23         2     11.5  B
     1 | 2020-02-29         3     12.3  NA
     2 | 2010-11-23         2     11.5  B
     3 | 2020-02-29         3     12.3  NA
     4 | NA                 4    -13    D
    [5 rows x 4 columns]

    >>> DT[[slice(5, 8), 1, 3, slice(10, 12)], :]
       | dates       integers   floats  strings
       | date32         int32  float64  str32
    -- + ----------  --------  -------  -------
     0 | 2010-11-23         2     11.5  B
     1 | 2020-02-29         3     12.3  NA
     2 | NA                 4    -13    D
     3 | 2010-11-23         2     11.5  B
     4 | NA                 4    -13    D
     5 | 2020-02-29         3     12.3  NA
     6 | NA                 4    -13    D
    [7 rows x 4 columns]


Via :ref:`f-expressions`
^^^^^^^^^^^^^^^^^^^^^^^^
:ref:`f-expressions` return booleans that can be used to filter/select the
appropriate rows::

    >>> DT[f.dates < dt.Frame([date(2020,1,1)]), :]
       | dates       integers   floats  strings
       | date32         int32  float64  str32
    -- + ----------  --------  -------  -------
     0 | 2000-01-05         1     10    A
     1 | 2010-11-23         2     11.5  B
    [2 rows x 4 columns]


    >>> DT[f.integers % 2 != 0, :]
       | dates       integers   floats  strings
       | date32         int32  float64  str32
    -- + ----------  --------  -------  -------
     0 | 2000-01-05         1     10    A
     1 | 2020-02-29         3     12.3  NA
    [2 rows x 4 columns]

    >>> DT[(f.integers == 3) & (f.strings == None), ...]
       | dates       integers   floats  strings
       | date32         int32  float64  str32
    -- + ----------  --------  -------  -------
     0 | 2020-02-29         3     12.3  NA
     1 | 2020-02-29         3     12.3  NA
     2 | 2020-02-29         3     12.3  NA
    [3 rows x 4 columns]

Selection is possible via the data types::

    >>> DT[f[float] < 1, :]
       | dates   integers   floats  strings
       | date32     int32  float64  str32
    -- + ------  --------  -------  -------
     0 | NA             4      -13  D
     1 | NA             4      -13  D
     2 | NA             4      -13  D
    [3 rows x 4 columns]

    >>> DT[dt.rowsum(f[int, float]) > 12, :]
       | dates       integers   floats  strings
       | date32         int32  float64  str32
    -- + ----------  --------  -------  -------
     0 | 2010-11-23         2     11.5  B
     1 | 2020-02-29         3     12.3  NA
     2 | 2010-11-23         2     11.5  B
     3 | 2020-02-29         3     12.3  NA
     4 | 2010-11-23         2     11.5  B
     5 | 2020-02-29         3     12.3  NA
    [6 rows x 4 columns]



Select rows and columns
-----------------------
Specific selections can occur in rows and columns simultaneously::

    >>> DT[0, slice(1, 3)]
       | integers   floats
       |    int32  float64
    -- + --------  -------
     0 |        1       10
    [1 row x 2 columns]

    >>> DT[2 : 6, ["i" in name for name in DT.names]]
       | integers  strings
       |    int32  str32
    -- + --------  -------
     0 |        3  NA
     1 |        4  D
     2 |        1  A
     3 |        2  B
    [4 rows x 2 columns]

    >>> DT[f.integers > dt.mean(f.floats) - 3, f['strings' : 'integers']]
       | strings   floats  integers
       | str32    float64     int32
    -- + -------  -------  --------
     0 | NA          12.3         3
     1 | D          -13           4
     2 | NA          12.3         3
     3 | D          -13           4
     4 | NA          12.3         3
     5 | D          -13           4
    [6 rows x 3 columns]


Single value access
-------------------

Passing single integers into the ``i`` and ``j`` sections returns a scalar value::

    >>> DT[0, 0]
    datetime.date(2000, 1, 5)

    >>> DT[0, 2]
    10.0

    >>> DT[-3, 'strings']
    'B'


Deselect rows/columns
---------------------

Deselection of rows/columns is possible via `list comprehension`_/`generator expression`_

- Deselect a single column/row::

    >>> # The list comprehension returns the specific column names
    >>> DT[:, [name for name in DT.names if name != "integers"]]
       | dates        floats  strings
       | date32      float64  str32
    -- + ----------  -------  -------
     0 | 2000-01-05     10    A
     1 | 2010-11-23     11.5  B
     2 | 2020-02-29     12.3  NA
     3 | NA            -13    D
     4 | 2000-01-05     10    A
     5 | 2010-11-23     11.5  B
     6 | 2020-02-29     12.3  NA
     7 | NA            -13    D
     8 | 2000-01-05     10    A
     9 | 2010-11-23     11.5  B
    10 | 2020-02-29     12.3  NA
    11 | NA            -13    D
    [12 rows x 3 columns]

    >>> # A boolean sequence is returned in the list comprehension
    >>> DT[[num != 5 for num in range(DT.nrows)], 'dates']
       | dates
       | date32
    -- + ----------
     0 | 2000-01-05
     1 | 2010-11-23
     2 | 2020-02-29
     3 | NA
     4 | 2000-01-05
     5 | 2020-02-29
     6 | NA
     7 | 2000-01-05
     8 | 2010-11-23
     9 | 2020-02-29
    10 | NA
    [11 rows x 1 column]


- Deselect multiple columns/rows::

    >>> DT[:, [name not in ("integers", "dates") for name in DT.names]]
       |  floats  strings
       | float64  str32
    -- + -------  -------
     0 |    10    A
     1 |    11.5  B
     2 |    12.3  NA
     3 |   -13    D
     4 |    10    A
     5 |    11.5  B
     6 |    12.3  NA
     7 |   -13    D
     8 |    10    A
     9 |    11.5  B
    10 |    12.3  NA
    11 |   -13    D
    [12 rows x 2 columns]

    >>> DT[(num not in range(3, 8) for num in range(DT.nrows)), ['integers', 'floats']]
       | integers   floats
       |    int32  float64
    -- + --------  -------
     0 |        1     10
     1 |        2     11.5
     2 |        3     12.3
     3 |        1     10
     4 |        2     11.5
     5 |        3     12.3
     6 |        4    -13
    [7 rows x 2 columns]

    >>> DT[:, [num not in (2, 3) for num in range(DT.ncols)]]
       | dates       integers
       | date32         int32
    -- + ----------  --------
     0 | 2000-01-05         1
     1 | 2010-11-23         2
     2 | 2020-02-29         3
     3 | NA                 4
     4 | 2000-01-05         1
     5 | 2010-11-23         2
     6 | 2020-02-29         3
     7 | NA                 4
     8 | 2000-01-05         1
     9 | 2010-11-23         2
    10 | 2020-02-29         3
    11 | NA                 4
    [12 rows x 2 columns]

    >>> # an alternative to the previous example
    >>> DT[:, [num not in (2, 3) for num, _ in enumerate(DT.names)]]
       | dates       integers
       | date32         int32
    -- + ----------  --------
     0 | 2000-01-05         1
     1 | 2010-11-23         2
     2 | 2020-02-29         3
     3 | NA                 4
     4 | 2000-01-05         1
     5 | 2010-11-23         2
     6 | 2020-02-29         3
     7 | NA                 4
     8 | 2000-01-05         1
     9 | 2010-11-23         2
    10 | 2020-02-29         3
    11 | NA                 4
    [12 rows x 2 columns]

- Deselect by data type::

    >>> # This selects columns that are not numeric
    >>> DT[2:7, [not coltype.is_numeric for coltype in DT.types]]
       | dates       strings
       | date32      str32
    -- + ----------  -------
     0 | 2020-02-29  NA
     1 | NA          D
     2 | 2000-01-05  A
     3 | 2010-11-23  B
     4 | 2020-02-29  NA
    [5 rows x 2 columns]

Slicing could be used to exclude rows/columns. The code below excludes rows from position 3 to 6::

    >>> DT[[slice(None, 3), slice(7, None)], :]
       | dates       integers   floats  strings
       | date32         int32  float64  str32
    -- + ----------  --------  -------  -------
     0 | 2000-01-05         1     10    A
     1 | 2010-11-23         2     11.5  B
     2 | 2020-02-29         3     12.3  NA
     3 | NA                 4    -13    D
     4 | 2000-01-05         1     10    A
     5 | 2010-11-23         2     11.5  B
     6 | 2020-02-29         3     12.3  NA
     7 | NA                 4    -13    D
    [8 rows x 4 columns]


Columns can also be deselected via the :meth:`remove() <dt.FExpr.remove>`
method, where the column name, column position, or data type is passed to the
:data:`f` symbol::

    >>> DT[:, f[:].remove(f.dates)]
       | integers   floats  strings
       |    int32  float64  str32
    -- + --------  -------  -------
     0 |        1     10    A
     1 |        2     11.5  B
     2 |        3     12.3  NA
     3 |        4    -13    D
     4 |        1     10    A
     5 |        2     11.5  B
     6 |        3     12.3  NA
     7 |        4    -13    D
     8 |        1     10    A
     9 |        2     11.5  B
    10 |        3     12.3  NA
    11 |        4    -13    D
    [12 rows x 3 columns]

    >>> DT[:, f[:].remove(f[0])]
       | integers   floats  strings
       |    int32  float64  str32
    -- + --------  -------  -------
     0 |        1     10    A
     1 |        2     11.5  B
     2 |        3     12.3  NA
     3 |        4    -13    D
     4 |        1     10    A
     5 |        2     11.5  B
     6 |        3     12.3  NA
     7 |        4    -13    D
     8 |        1     10    A
     9 |        2     11.5  B
    10 |        3     12.3  NA
    11 |        4    -13    D
    [12 rows x 3 columns]

    >>> DT[:, f[:].remove(f[1:3])]
       | dates       strings
       | date32      str32
    -- + ----------  -------
     0 | 2000-01-05  A
     1 | 2010-11-23  B
     2 | 2020-02-29  NA
     3 | NA          D
     4 | 2000-01-05  A
     5 | 2010-11-23  B
     6 | 2020-02-29  NA
     7 | NA          D
     8 | 2000-01-05  A
     9 | 2010-11-23  B
    10 | 2020-02-29  NA
    11 | NA          D
    [12 rows x 2 columns]

    >>> DT[:, f[:].remove(f['strings':'integers'])]
       | dates
       | date32
    -- + ----------
     0 | 2000-01-05
     1 | 2010-11-23
     2 | 2020-02-29
     3 | NA
     4 | 2000-01-05
     5 | 2010-11-23
     6 | 2020-02-29
     7 | NA
     8 | 2000-01-05
     9 | 2010-11-23
    10 | 2020-02-29
    11 | NA
    [12 rows x 1 column]


    >>> DT[:, f[:].remove(f[int, float])]
       | dates       strings
       | date32      str32
    -- + ----------  -------
     0 | 2000-01-05  A
     1 | 2010-11-23  B
     2 | 2020-02-29  NA
     3 | NA          D
     4 | 2000-01-05  A
     5 | 2010-11-23  B
     6 | 2020-02-29  NA
     7 | NA          D
     8 | 2000-01-05  A
     9 | 2010-11-23  B
    10 | 2020-02-29  NA
    11 | NA          D
    [12 rows x 2 columns]

    >>> DT[:, f[:].remove(f[:])]
       |
       |
    -- +
     0 |
     1 |
     2 |
     3 |
     4 |
     5 |
     6 |
     7 |
     8 |
     9 |
    10 |
    11 |
    [12 rows x 0 columns]


Delete rows/columns
-------------------

To actually delete a row (or a column), use the `del`_ statement; this is an
in-place operation, and as such no reassignment is needed

- Delete multiple rows::

    >>> del DT[3:7, :]
    >>> DT
       | dates       integers   floats  strings
       | date32         int32  float64  str32
    -- + ----------  --------  -------  -------
     0 | 2000-01-05         1     10    A
     1 | 2010-11-23         2     11.5  B
     2 | 2020-02-29         3     12.3  NA
     3 | NA                 4    -13    D
     4 | 2000-01-05         1     10    A
     5 | 2010-11-23         2     11.5  B
     6 | 2020-02-29         3     12.3  NA
     7 | NA                 4    -13    D
    [8 rows x 4 columns]

- Delete a single row::

    >>> del DT[3, :]
    >>>
    >>> DT
       | dates       integers   floats
       | date32         int32  float64
    -- + ----------  --------  -------
     0 | 2000-01-05         1     10
     1 | 2010-11-23         2     11.5
     2 | 2020-02-29        NA     NA
     3 | 2000-01-05        NA     NA
     4 | 2010-11-23         2     11.5
     5 | 2020-02-29         3     12.3
     6 | NA                 4    -13
    [7 rows x 3 columns]

- Delete a column::

    >>> del DT['strings']
    >>>
    >>> DT
       | dates       integers   floats
       | date32         int32  float64
    -- + ----------  --------  -------
     0 | 2000-01-05         1     10
     1 | 2010-11-23         2     11.5
     2 | 2020-02-29         3     12.3
     3 | NA                 4    -13
     4 | 2000-01-05         1     10
     5 | 2010-11-23         2     11.5
     6 | 2020-02-29         3     12.3
     7 | NA                 4    -13
    [8 rows x 3 columns]


- Delete multiple columns::

    >>> del DT[:, ['dates', 'floats']]
    >>>
    >>> DT
       | integers
       |    int32
    -- + --------
     0 |        1
     1 |        2
     2 |       NA
     3 |       NA
     4 |        2
     5 |        3
     6 |        4
    [7 rows x 1 column]





.. _`pandas Series`: https://pandas.pydata.org/pandas-docs/stable/reference/api/pandas.Series.html
.. _`numpy`: https://numpy.org/doc/stable/reference/generated/numpy.array.html#:~:text=array,-numpy.&text=An%20array%2C%20any%20object%20exposing,data%2Dtype%20for%20the%20array.
.. _`range`: https://docs.python.org/3/library/functions.html#func-range
.. _`generator expression`: https://docs.python.org/3/reference/expressions.html?highlight=generator#generator-expressions
.. _`set`: https://docs.python.org/3/tutorial/datastructures.html#sets
.. _`built-in types`: https://docs.python.org/3/library/stdtypes.html#built-in-types
.. _`del`: https://docs.python.org/3/reference/simple_stmts.html#the-del-statement
.. _`list comprehension`: https://docs.python.org/3/tutorial/datastructures.html#list-comprehensions
