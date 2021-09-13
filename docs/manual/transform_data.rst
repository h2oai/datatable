
Transforming Data
=================

This section looks at the different options for transforming data. We'll also see how to create new columns and update existing ones.

Example Data
------------

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


Column Transformation
---------------------
Operations on columns occur in the  `j` section of ``DT[i, j, by]``, and involve the use of :ref:`f-expressions`::

   >>> DT[:, f.integers * 2]    
      |    C0
      | int32
   -- + -----
    0 |     2
    1 |     4
    2 |     6
    3 |     8
   [4 rows x 1 column]

   >>> DT[:, f.integers + f.floats]
      |      C0
      | float64
   -- + -------
    0 |    11  
    1 |    13.5
    2 |    15.3
    3 |    -9  
   [4 rows x 1 column]

   >>> DT[:, 'pre_' + f.strings]
      | C0   
      | str32
   -- + -----
    0 | pre_A
    1 | pre_B
    2 | NA   
    3 | pre_D
   [4 rows x 1 column]

Operation between columns is possible::

   >>> DT[:, f.integers + f.floats]
      |      C0
      | float64
   -- + -------
    0 |    11  
    1 |    13.5
    2 |    15.3
    3 |    -9  
   [4 rows x 1 column]

   >>> DT[:, f.integers +'_' + f.strings]
      | C0   
      | str32
   -- + -----
    0 | 1_A  
    1 | 2_B  
    2 | NA   
    3 | 4_D  
   [4 rows x 1 column]

.. note::

    Operations between columns creates a new column, and a generic name is ascribed to the column, usually `C0`.

Datatable Functions can be applied to the columns::

   >>> DT[:, dt.math.pow(f.floats, 3)]
      |       C0
      |  float64
   -- + --------
    0 |  1000   
    1 |  1520.88
    2 |  1860.87
    3 | -2197   
   [4 rows x 1 column]

   >>> DT[:, dt.time.year(f.dates)]
      | dates
      | int32
   -- + -----
    0 |  2000
    1 |  2010
    2 |  2020
    3 |    NA
   [4 rows x 1 column]

Functions can be applied across columns, row-wise and column-wise::

   >>> DT[:, f['integers':'floats'].sum()]
      | integers   floats
      |    int64  float64
   -- + --------  -------
    0 |       10     20.8
   [1 row x 2 columns]

   >>> DT[:, f['integers':'floats'].rowsum()]
      |      C0
      | float64
   -- + -------
    0 |    11  
    1 |    13.5
    2 |    15.3
    3 |    -9  
   [4 rows x 1 column]

Transformation of a column based on a condition is possible, via :func:`ifelse()`, which operates similarly to Python's `if-else` idiom::

   >>> DT[:, ifelse(f.integers % 2 == 0, 'even', 'odd')]
      | C0   
      | str32
   -- + -----
    0 | odd  
    1 | even 
    2 | odd  
    3 | even 
   [4 rows x 1 column]

Iteration on a Frame
--------------------
Iterating through a :class:`Frame` allows access to the individual columns; each column is treated as a :class:`Frame`::

   >>> [frame for frame in DT]

      | dates     
      | date32    
   -- + ----------
    0 | 2000-01-05
    1 | 2010-11-23
    2 | 2020-02-29
    3 | NA        
   [4 rows x 1 column]

      | integers
      |    int32
   -- + --------
    0 |        1
    1 |        2
    2 |        3
    3 |        4
   [4 rows x 1 column]

      |  floats
      | float64
   -- + -------
    0 |    10  
    1 |    11.5
    2 |    12.3
    3 |   -13  
   [4 rows x 1 column]

      | strings
      | str32  
   -- + -------
    0 | A      
    1 | B      
    2 | NA     
    3 | D      
   [4 rows x 1 column]
   
With iteration, different operations can be applied to different columns::

   >>> outcome = [frame.mean() if frame.type.is_numeric else frame[0, :] for frame in DT]
   >>> outcome
      | dates     
      | date32    
   -- + ----------
    0 | 2000-01-05
   [1 row x 1 column]

      | integers
      |  float64
   -- + --------
    0 |      2.5
   [1 row x 1 column]

      |  floats
      | float64
   -- + -------
    0 |     5.2
   [1 row x 1 column]

      | strings
      | str32  
   -- + -------
    0 | A      
   [1 row x 1 column]


   >>> DT[:, outcome] # or dt.cbind(outcome)
      | dates       integers   floats  strings
      | date32       float64  float64  str32  
   -- + ----------  --------  -------  -------
    0 | 2000-01-05       2.5      5.2  A      
   [1 row x 4 columns]


Sorting a Frame
---------------
A :class:`Frame` can be sorted via the :func:`sort()` function, or the :meth:`datatable.Frame.sort` method::

   >>> DT[:, :, dt.sort('dates')]
      | dates       integers   floats  strings
      | date32         int32  float64  str32  
   -- + ----------  --------  -------  -------
    0 | NA                 4    -13    D      
    1 | 2000-01-05         1     10    A      
    2 | 2010-11-23         2     11.5  B      
    3 | 2020-02-29         3     12.3  NA     
   [4 rows x 4 columns]

   >>> DT.sort('dates')
      | dates       integers   floats  strings
      | date32         int32  float64  str32  
   -- + ----------  --------  -------  -------
    0 | NA                 4    -13    D      
    1 | 2000-01-05         1     10    A      
    2 | 2010-11-23         2     11.5  B      
    3 | 2020-02-29         3     12.3  NA     
   [4 rows x 4 columns]

Sorting is possible via :ref:`f-expressions`::

   >>>  DT[:, :, dt.sort(f.floats)]
      | dates       integers   floats  strings
      | date32         int32  float64  str32  
   -- + ----------  --------  -------  -------
    0 | NA                 4    -13    D      
    1 | 2000-01-05         1     10    A      
    2 | 2010-11-23         2     11.5  B      
    3 | 2020-02-29         3     12.3  NA     
   [4 rows x 4 columns]

   >>> DT.sort(f.strings)
      | dates       integers   floats  strings
      | date32         int32  float64  str32  
   -- + ----------  --------  -------  -------
    0 | 2020-02-29         3     12.3  NA     
    1 | 2000-01-05         1     10    A      
    2 | 2010-11-23         2     11.5  B      
    3 | NA                 4    -13    D      
   [4 rows x 4 columns]

The default sorting order is ascending; and if there are any nulls in the sorting columns, they go to the top. 

The sorting order and the position of nulls can be changed in a number of ways:

-  Sorting can be in descending order via the `reverse` parameter::

      >>> DT[:, :, dt.sort('integers', reverse = True)]
         | dates       integers   floats  strings
         | date32         int32  float64  str32  
      -- + ----------  --------  -------  -------
       0 | NA                 4    -13    D      
       1 | 2020-02-29         3     12.3  NA     
       2 | 2010-11-23         2     11.5  B      
       3 | 2000-01-05         1     10    A      
      [4 rows x 4 columns]

.. note::

   The ``reverse`` parameter is available only in the :func:`sort()` function

- Sorting in descending order is possible by negating the :ref:`f-expressions` within the :func:`sort()` function, or the :meth:`datatable.Frame.sort` method::

      >>> DT[:, :, dt.sort(-f.integers)]
         | dates       integers   floats  strings
         | date32         int32  float64  str32  
      -- + ----------  --------  -------  -------
       0 | NA                 4    -13    D      
       1 | 2020-02-29         3     12.3  NA     
       2 | 2010-11-23         2     11.5  B      
       3 | 2000-01-05         1     10    A      
      [4 rows x 4 columns]


      >>> DT.sort(-f.integers)
         | dates       integers   floats  strings
         | date32         int32  float64  str32  
      -- + ----------  --------  -------  -------
       0 | NA                 4    -13    D      
       1 | 2020-02-29         3     12.3  NA     
       2 | 2010-11-23         2     11.5  B      
       3 | 2000-01-05         1     10    A      
      [4 rows x 4 columns]

- The position of null values within the sorting column can be controlled with the ``na_position`` parameter::

      >>> DT[:, :, dt.sort('dates', na_position = 'last')]
         | dates       integers   floats  strings
         | date32         int32  float64  str32  
      -- + ----------  --------  -------  -------
       0 | 2000-01-05         1     10    A      
       1 | 2010-11-23         2     11.5  B      
       2 | 2020-02-29         3     12.3  NA     
       3 | NA                 4    -13    D      
      [4 rows x 4 columns]

.. note::

   The `na_position` parameter is available only in the :func:`sort()` function

.. note::

   The default value for ``na_position`` is `first`

Sorting is possible on multiple columns::

   >>> multiples = dt.Frame({'OrderID': ['o1','o2','o3','o4','o5'],
   ...                       'CustomerID': ['c1','c1','c2','c2','c3'],
   ...                       'CustomerRating': [5,1,3, np.NaN,np.NaN]
   ...                     })
   >>>
   >>> multiples
      | OrderID  CustomerID  CustomerRating
      | str32    str32              float64
   -- + -------  ----------  --------------
    0 | o1       c1                       5
    1 | o2       c1                       1
    2 | o3       c2                       3
    3 | o4       c2                      NA
    4 | o5       c3                      NA
   [5 rows x 3 columns]


   >>> multiples[:, :, dt.sort(f.CustomerID, -f.OrderID)]
      | OrderID  CustomerID  CustomerRating
      | str32    str32              float64
   -- + -------  ----------  --------------
    0 | o2       c1                       1
    1 | o1       c1                       5
    2 | o4       c2                      NA
    3 | o3       c2                       3
    4 | o5       c3                      NA
   [5 rows x 3 columns]

   >>> multiples.sort(f.CustomerID, -f.OrderID)
      | OrderID  CustomerID  CustomerRating
      | str32    str32              float64
   -- + -------  ----------  --------------
    0 | o2       c1                       1
    1 | o1       c1                       5
    2 | o4       c2                      NA
    3 | o3       c2                       3
    4 | o5       c3                      NA
   [5 rows x 3 columns]


Column Assignment
-----------------
