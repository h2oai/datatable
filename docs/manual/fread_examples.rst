.. _`Fread Examples`:


Fread Examples
=================

This function is capable of reading data from a variety of input formats (text
files, plain text, files embedded in archives, excel files, ...), producing
a Frame as the result. You can even read in data from the command line.

See :func:`fread()` for all the available parameters.

**Note:** If you wish to read in multiple files, use :func:`iread()`; it
returns an iterator of Frames.


Read data
---------

Read from a text file::

    >>> from datatable import dt, fread
    >>> fread('iris.csv')
        | sepal_length  sepal_width  petal_length  petal_width  species
        |      float64      float64       float64      float64  str32
    --- + ------------  -----------  ------------  -----------  ---------
      0 |          5.1          3.5           1.4          0.2  setosa
      1 |          4.9          3             1.4          0.2  setosa
      2 |          4.7          3.2           1.3          0.2  setosa
      3 |          4.6          3.1           1.5          0.2  setosa
      4 |          5            3.6           1.4          0.2  setosa
      5 |          5.4          3.9           1.7          0.4  setosa
      6 |          4.6          3.4           1.4          0.3  setosa
      7 |          5            3.4           1.5          0.2  setosa
      8 |          4.4          2.9           1.4          0.2  setosa
      9 |          4.9          3.1           1.5          0.1  setosa
     10 |          5.4          3.7           1.5          0.2  setosa
     11 |          4.8          3.4           1.6          0.2  setosa
     12 |          4.8          3             1.4          0.1  setosa
     13 |          4.3          3             1.1          0.1  setosa
     14 |          5.8          4             1.2          0.2  setosa
      … |            …            …             …            …  …
    145 |          6.7          3             5.2          2.3  virginica
    146 |          6.3          2.5           5            1.9  virginica
    147 |          6.5          3             5.2          2    virginica
    148 |          6.2          3.4           5.4          2.3  virginica
    149 |          5.9          3             5.1          1.8  virginica
    [150 rows x 5 columns]

Read text data directly::

    >>> data = ('col1,col2,col3\n'
    ...         'a,b,1\n'
    ...         'a,b,2\n'
    ...         'c,d,3')
    >>> fread(data)
       | col1   col2    col3
       | str32  str32  int32
    -- + -----  -----  -----
     0 | a      b          1
     1 | a      b          2
     2 | c      d          3
    [3 rows x 3 columns]

Read from a url::

    >>> url = "https://raw.githubusercontent.com/Rdatatable/data.table/master/vignettes/flights14.csv"
    >>> fread(url)
           |  year  month    day  dep_delay  arr_delay  carrier  origin  dest   air_time  distance   hour
           | int32  int32  int32      int32      int32  str32    str32   str32     int32     int32  int32
    ------ + -----  -----  -----  ---------  ---------  -------  ------  -----  --------  --------  -----
         0 |  2014      1      1         14         13  AA       JFK     LAX         359      2475      9
         1 |  2014      1      1         -3         13  AA       JFK     LAX         363      2475     11
         2 |  2014      1      1          2          9  AA       JFK     LAX         351      2475     19
         3 |  2014      1      1         -8        -26  AA       LGA     PBI         157      1035      7
         4 |  2014      1      1          2          1  AA       JFK     LAX         350      2475     13
         5 |  2014      1      1          4          0  AA       EWR     LAX         339      2454     18
         6 |  2014      1      1         -2        -18  AA       JFK     LAX         338      2475     21
         7 |  2014      1      1         -3        -14  AA       JFK     LAX         356      2475     15
         8 |  2014      1      1         -1        -17  AA       JFK     MIA         161      1089     15
         9 |  2014      1      1         -2        -14  AA       JFK     SEA         349      2422     18
        10 |  2014      1      1         -5        -17  AA       EWR     MIA         161      1085     16
        11 |  2014      1      1          7         -5  AA       JFK     SFO         365      2586     17
        12 |  2014      1      1          3          1  AA       JFK     BOS          39       187     12
        13 |  2014      1      1        142        133  AA       JFK     LAX         345      2475     19
        14 |  2014      1      1         -5        -26  AA       JFK     BOS          35       187     17
         … |     …      …      …          …          …  …        …       …             …         …      …
    253311 |  2014     10     31          1        -30  UA       LGA     IAH         201      1416     14
    253312 |  2014     10     31         -5        -14  UA       EWR     IAH         189      1400      8
    253313 |  2014     10     31         -8         16  MQ       LGA     RDU          83       431     11
    253314 |  2014     10     31         -4         15  MQ       LGA     DTW          75       502     11
    253315 |  2014     10     31         -5          1  MQ       LGA     SDF         110       659      8
    [253316 rows x 11 columns]

Read from an archive (if there are multiple files, only the first will be read;
you can specify the path to the specific file you are interested in)::

    >>> fread("data.zip/mtcars.csv")

**Note:** Use :func:`iread()` if you wish to read in multiple files in an
archive; an iterator of Frames is returned.

Read from ``.xls`` or ``.xlsx`` files ::

    >>> fread("excel.xlsx")

For excel files, you can specify the sheet to be read::

    >>> fread("excel.xlsx/Sheet1")

**Note:**
        - `xlrd <https://pypi.org/project/xlrd/>`_ must be installed to read in excel files.

        -  Use :func:`iread()` if you wish to read in multiple sheets; an iterator of Frames is returned.

Read in data from the command line. Simply pass the command line statement to
the ``cmd`` parameter::

    >>> # https://blog.jpalardy.com/posts/awk-tutorial-part-2/
    >>> # You specify the `cmd` parameter
    >>> # Here we filter data for the year 2015
    >>> fread(cmd = """cat netflix.tsv | awk 'NR==1; /^2015-/'""")

The command line can be very handy with large data; you can do some of the
preprocessing before reading in the data to ``datatable``.


Detect Thousand Separator
-------------------------

``Fread`` handles thousand separator, with the assumption that the separator
is a ``,``::

    >>> fread("""Name|Salary|Position
    ...          James|256,000|evangelist
    ...         Ragnar|1,000,000|conqueror
    ...           Loki|250360|trickster""")
       | Name     Salary  Position
       | str32     int32  str32
    -- + ------  -------  ----------
     0 | James    256000  evangelist
     1 | Ragnar  1000000  conqueror
     2 | Loki     250360  trickster
    [3 rows x 3 columns]


Specify the Delimiter
---------------------

You can specify the delimiter via the ``sep`` parameter.
Note that the  separator must be a single character string; non-ASCII characters are not allowed as the separator, as well as any characters in ``["'`0-9a-zA-Z]``::

    >>> data = """
    ...        1:2:3:4
    ...        5:6:7:8
    ...        9:10:11:12
    ...        """
    >>>
        >>> fread(data, sep=":")
       |    C0     C1     C2     C3
       | int32  int32  int32  int32
    -- + -----  -----  -----  -----
     0 |     1      2      3      4
     1 |     5      6      7      8
     2 |     9     10     11     12
    [3 rows x 4 columns]


Dealing with Null Values and Blank Rows
---------------------------------------

You can pass a list of values to be treated as null, via the ``na_strings`` parameter::

    >>> data = """
    ...        ID|Charges|Payment_Method
    ...        634-VHG|28|Cheque
    ...        365-DQC|33.5|Credit card
    ...        264-PPR|631|--
    ...        845-AJO|42.3|
    ...        789-KPO|56.9|Bank Transfer
    ...        """
    >>>
    >>> fread(data, na_strings=['--', ''])
       | ID       Charges  Payment_Method
       | str32    float64  str32
    -- + -------  -------  --------------
     0 | 634-VHG     28    Cheque
     1 | 365-DQC     33.5  Credit card
     2 | 264-PPR    631    NA
     3 | 845-AJO     42.3  NA
     4 | 789-KPO     56.9  Bank Transfer
    [5 rows x 3 columns]

For rows with less values than in other rows,  you can set ``fill=True``; ``fread`` will fill with ``NA``::

    >>> data = ('a,b,c,d\n'
    ...         '1,2,3,4\n'
    ...         '5,6,7,8\n'
    ...         '9,10,11')
    >>>
    >>> fread(data, fill=True)
       |     a      b      c      d
       | int32  int32  int32  int32
    -- + -----  -----  -----  -----
     0 |     1      2      3      4
     1 |     5      6      7      8
     2 |     9     10     11     NA
    [3 rows x 4 columns]

You can skip empty lines::

    >>> data = ('a,b,c,d\n'
    ...         '\n'
    ...         '1,2,3,4\n'
    ...         '5,6,7,8\n'
    ...         '\n'
    ...         '9,10,11,12')
    >>>
    >>> fread(data, skip_blank_lines=True)
       |     a      b      c      d
       | int32  int32  int32  int32
    -- + -----  -----  -----  -----
     0 |     1      2      3      4
     1 |     5      6      7      8
     2 |     9     10     11     12
    [3 rows x 4 columns]


Dealing with Column Names
-------------------------

If the data has no headers, ``fread`` will assign default column names::

    >>> data = ('1,2\n'
    ...         '3,4\n')
    >>> fread(data)
       |    C0     C1
       | int32  int32
    -- + -----  -----
     0 |     1      2
     1 |     3      4
    [2 rows x 2 columns]

You can pass in column names via the ``columns`` parameter::

    >>> fread(data, columns=['A','B'])
       |     A      B
       | int32  int32
    -- + -----  -----
     0 |     1      2
     1 |     3      4
    [2 rows x 2 columns]

You can change column names::

    >>> data = ('a,b,c,d\n'
    ...         '1,2,3,4\n'
    ...         '5,6,7,8\n'
    ...         '9,10,11,12')
    >>>
    >>> fread(data, columns=["A","B","C","D"])
       |     A      B      C      D
       | int32  int32  int32  int32
    -- + -----  -----  -----  -----
     0 |     1      2      3      4
     1 |     5      6      7      8
     2 |     9     10     11     12
    [3 rows x 4 columns]

You can change *some* of the column names via a dictionary::

    >>> fread(data, columns={"a":"A", "b":"B"})
       |     A      B      c      d
       | int32  int32  int32  int32
    -- + -----  -----  -----  -----
     0 |     1      2      3      4
     1 |     5      6      7      8
     2 |     9     10     11     12
    [3 rows x 4 columns]

``Fread`` uses heuristics to determine whether the first row is data or not;
occasionally it may guess incorrectly, in which case, you can set the
``header`` parameter to *False*::

    >>> fread(data,  header=False)
       | C0     C1     C2     C3
       | str32  str32  str32  str32
    -- + -----  -----  -----  -----
     0 | a      b      c      d
     1 | 1      2      3      4
     2 | 5      6      7      8
     3 | 9      10     11     12
    [4 rows x 4 columns]

You can pass a new list of column names as well::

    >>> fread(data,  header=False, columns=["A","B","C","D"])
       | A      B      C      D
       | str32  str32  str32  str32
    -- + -----  -----  -----  -----
     0 | a      b      c      d
     1 | 1      2      3      4
     2 | 5      6      7      8
     3 | 9      10     11     12
    [4 rows x 4 columns]


Row Selection
-------------

``Fread`` has a ``skip_to_line`` parameter, where you can specify what line to
read the data from::

    >>> data = ('skip this line\n'
    ...         'a,b,c,d\n'
    ...         '1,2,3,4\n'
    ...         '5,6,7,8\n'
    ...         '9,10,11,12')
    >>>
    >>> fread(data, skip_to_line=2)
       |     a      b      c      d
       | int32  int32  int32  int32
    -- + -----  -----  -----  -----
     0 |     1      2      3      4
     1 |     5      6      7      8
     2 |     9     10     11     12
    [3 rows x 4 columns]

You can also skip to a line containing a particular string with the
``skip_to_string`` parameter, and start reading data from that line. Note that
``skip_to_string`` and ``skip_to_line`` cannot be combined; you can only use
one::

    >>> data = ('skip this line\n'
    ...         'a,b,c,d\n'
    ...         'first, second, third, last\n'
    ...         '1,2,3,4\n'
    ...         '5,6,7,8\n'
    ...         '9,10,11,12')
    >>>
    >>> fread(data, skip_to_string='first')
       | first  second  third   last
       | int32   int32  int32  int32
    -- + -----  ------  -----  -----
     0 |     1       2      3      4
     1 |     5       6      7      8
     2 |     9      10     11     12
    [3 rows x 4 columns]

You can set the maximum number of rows to read with the ``max_nrows`` parameter::

    >>> data = ('a,b,c,d\n'
    ...         '1,2,3,4\n'
    ...         '5,6,7,8\n'
    ...         '9,10,11,12')
    >>>
    >>> fread(data, max_nrows=2)
       |     a      b      c      d
       | int32  int32  int32  int32
    -- + -----  -----  -----  -----
     0 |     1      2      3      4
     1 |     5      6      7      8
    [2 rows x 4 columns]

    >>> data = ('skip this line\n'
    ...         'a,b,c,d\n'
    ...         '1,2,3,4\n'
    ...         '5,6,7,8\n'
    ...         '9,10,11,12')
    >>>
    >>> fread(data, skip_to_line=2, max_nrows=2)
       |     a      b      c      d
       | int32  int32  int32  int32
    -- + -----  -----  -----  -----
     0 |     1      2      3      4
     1 |     5      6      7      8
    [2 rows x 4 columns]


Setting Column Type
--------------------

You can determine the data types via the ``columns`` parameter::

    >>> data = ('a,b,c,d\n'
    ...         '1,2,3,4\n'
    ...         '5,6,7,8\n'
    ...         '9,10,11,12')
    >>>
    >>> # this is useful when you are interested in only a subset of the columns
    ... fread(data, columns={"a":dt.float32, "b":dt.str32})
       |       a  b          c      d
       | float64  str32  int32  int32
    -- + -------  -----  -----  -----
     0 |       1  2          3      4
     1 |       5  6          7      8
     2 |       9  10        11     12
    [3 rows x 4 columns]

You can also pass in the data types by *position*::

    >>> fread(data, columns = [dt.int32, dt.str32, None, dt.float32])
       |     a  b            d
       | int32  str32  float64
    -- + -----  -----  -------
     0 |     1  2            4
     1 |     5  6            8
     2 |     9  10          12
    [3 rows x 3 columns]

You can also change *all* the column data types with a single assignment::

    >>> fread(data, columns = dt.float32)
       |       a        b        c        d
       | float64  float64  float64  float64
    -- + -------  -------  -------  -------
     0 |       1        2        3        4
     1 |       5        6        7        8
     2 |       9       10       11       12
    [3 rows x 4 columns]

You can change the data type for a *slice* of the columns (here ``slice(3)``
is equivalent to ``[:3]``)::

    >>> # this changes the data type to float for the first three columns
    ... fread(data, columns={float:slice(3)})
       |       a        b        c      d
       | float64  float64  float64  int32
    -- + -------  -------  -------  -----
     0 |       1        2        3      4
     1 |       5        6        7      8
     2 |       9       10       11     12
    [3 rows x 4 columns]



Selecting Columns
-----------------

There are various ways to select columns in ``fread`` :

- Select with a *dictionary*::

    >>> data = ('a,b,c,d\n'
    ...         '1,2,3,4\n'
    ...         '5,6,7,8\n'
    ...         '9,10,11,12')
    >>>
    >>> # pass ``Ellipsis : None`` or ``... : None``,
    >>> # to discard any columns that are not needed
    >>> fread(data, columns={"a":"a", ... : None})
       |     a
       | int32
    -- + -----
     0 |     1
     1 |     5
     2 |     9
    [3 rows x 1 column]

Selecting via a dictionary makes more sense when selecting and renaming columns at the same time.


- Select columns with a *set*::

    >>> fread(data, columns={"a","b"})
       |     a      b
       | int32  int32
    -- + -----  -----
     0 |     1      2
     1 |     5      6
     2 |     9     10
    [3 rows x 2 columns]

- Select range of columns with *slice*::

    >>> # select the second and third column
    >>> fread(data, columns=slice(1,3))
       |     b      c
       | int32  int32
    -- + -----  -----
     0 |     2      3
     1 |     6      7
     2 |    10     11
    [3 rows x 2 columns]

    >>> # select the first column
    >>> # jump two hoops and
    >>> # select the third column
    >>> fread(data, columns = slice(None,3,2))
       |     a      c
       | int32  int32
    -- + -----  -----
     0 |     1      3
     1 |     5      7
     2 |     9     11
    [3 rows x 2 columns]

- Select range of columns with *range*::

    >>> fread(data, columns = range(1,3))
       |     b      c
       | int32  int32
    -- + -----  -----
     0 |     2      3
     1 |     6      7
     2 |    10     11
    [3 rows x 2 columns]

- Boolean Selection::

    >>> fread(data, columns=[False, False, True, True])
       |     c      d
       | int32  int32
    -- + -----  -----
     0 |     3      4
     1 |     7      8
     2 |    11     12
    [3 rows x 2 columns]

- Select with a list comprehension::

    >>> fread(data, columns=lambda cols:[col.name in ("a","c") for col in cols])
       |     a      c
       | int32  int32
    -- + -----  -----
     0 |     1      3
     1 |     5      7
     2 |     9     11
    [3 rows x 2 columns]

- Exclude columns with *None*::

    >>> fread(data, columns = ['a',None,None,'d'])
       |     a      d
       | int32  int32
    -- + -----  -----
     0 |     1      4
     1 |     5      8
     2 |     9     12
    [3 rows x 2 columns]

- Exclude columns with list comprehension::

    >>> fread(data, columns=lambda cols:[col.name not in ("a","c") for col in cols])
       |     b      d
       | int32  int32
    -- + -----  -----
     0 |     2      4
     1 |     6      8
     2 |    10     12
    [3 rows x 2 columns]

- Drop columns by assigning *None* to the columns via a dictionary::

    >>> data = ("A,B,C,D\n"
    ...         "1,3,5,7\n"
    ...         "2,4,6,8\n")
    >>>
    >>> fread(data, columns={"B":None,"D":None})
       |     A      C
       | int32  int32
    -- + -----  -----
     0 |     1      5
     1 |     2      6
    [2 rows x 2 columns]

- Drop a column and change data type::

    >>> fread(data, columns={"B":None, "C":str})
       |     A  C          D
       | int32  str32  int32
    -- + -----  -----  -----
     0 |     1  5          7
     1 |     2  6          8
    [2 rows x 3 columns]

- Change column name and type, and drop a column::

    >>> # pass a tuple, where the first item in the tuple is the new column name,
    >>> # and the other item is the new data type.
    >>> fread(data, columns={"A":("first", float), "B":None,"D":None})
       |   first      C
       | float64  int32
    -- + -------  -----
     0 |       1      5
     1 |       2      6
    [2 rows x 2 columns]

You can also select which columns to read dynamically, based on the names/types
of the columns in the file::

    >>> def colfilter(columns):
    ...     return [col.name=='species' or "length" in col.name
    ...             for col in columns]
    ...
    >>> fread('iris.csv', columns=colfilter, max_nrows=5)
       | sepal_length  petal_length  species
       |      float64       float64  str32
    -- + ------------  ------------  -------
     0 |          5.1           1.4  setosa
     1 |          4.9           1.4  setosa
     2 |          4.7           1.3  setosa
     3 |          4.6           1.5  setosa
     4 |          5             1.4  setosa
    [5 rows x 3 columns]

The same approach can be used to auto-rename columns as they are read from
the file::

    >>> def rename(columns):
    ...     return [col.name.upper() for col in columns]
    ...
    >>> fread('iris.csv', columns=rename, max_nrows=5)
       | SEPAL_LENGTH  SEPAL_WIDTH  PETAL_LENGTH  PETAL_WIDTH  SPECIES
       |      float64      float64       float64      float64  str32
    -- + ------------  -----------  ------------  -----------  -------
     0 |          5.1          3.5           1.4          0.2  setosa
     1 |          4.9          3             1.4          0.2  setosa
     2 |          4.7          3.2           1.3          0.2  setosa
     3 |          4.6          3.1           1.5          0.2  setosa
     4 |          5            3.6           1.4          0.2  setosa
    [5 rows x 5 columns]
