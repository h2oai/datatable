
Transforming Data
=================

This section looks at the different options within the `j` section of ``DT[i, j, by]`` for transforming data. We'll also see how to create new columns and update existing ones.

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

Functions can be applied across columns::

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

Iteration on a Frame
--------------------
Iterating through a :class:`Frame` allows access to the columns; each column is treated as a :class:`Frame`::

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

   >>> outcome = [frame.mean() if frame.type.is_numeric else frame[0, :]  for frame in DT]
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