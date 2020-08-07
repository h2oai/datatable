.. py:currentmodule:: datatable

Row Functions
==============

| Functions ``rowall``, ``rowany``, ``rowcount``, ``rowfirst``, ``rowlast``, ``rowmax``, ``rowmean``, ``rowmin``, ``rowsd``, ``rowsum`` are functions that aggregate across rows instead of columns and return a single column. These functions are equivalent to `Pandas <https://pandas.pydata.org/pandas-docs/stable/index.html>`_ aggregation functions with parameter ``(axis=1)``. 
| These functions make it easy to compute rowwise aggregations - for instance, you may want the sum of columns `A`, `B`, `C` and `D`. You could say : ``f.A + f.B + f.C + f.D``. Rowsum makes it easier - ``dt.rowsum(f['A':'D'])``.

Rowall, Rowany
-----------------

These work only on Boolean expressions - ``rowall`` checks if all the values in the row are ``True``, while ``rowany`` checks if any value in the row is True. It is similar to Pandas' `all <https://pandas.pydata.org/pandas-docs/stable/reference/api/pandas.DataFrame.all.html>`_ or `any <https://pandas.pydata.org/pandas-docs/stable/reference/api/pandas.DataFrame.any.html#pandas.DataFrame.any>`_ with a parameter of ``(axis=1)``. A single Boolean column is returned. ::

    from datatable import dt, f, by

    df = dt.Frame({'A': [True, True], 'B': [True, False]})
    df

    	A	B
    0	1	1
    1	1	0

    # rowall :
    df[:, dt.rowall(f[:])]

        C0
    0	1
    1	0

    # rowany :
    df[:, dt.rowany(f[:])]

    	C0
    0	1
    1	1

The single boolean column that is returned can be very handy when filtering in the ``i`` section.

- Filter for rows where at least one cell is greater than 0 ::

    df = dt.Frame({'a': [0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 0],
                   'b': [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
                   'c': [0, 0, 0, 0, 0, 5, 0, 0, 0, 0, 0],
                   'd': [0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0],
                   'e': [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
                   'f': [0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0]})

    df

	a	b	c	d	e	f
    0	0	0	0	0	0	0
    1	0	0	0	0	0	1
    2	0	0	0	0	0	0
    3	0	0	0	0	0	0
    4	0	0	0	0	0	0
    5	0	0	5	0	0	0
    6	1	0	0	0	0	0
    7	0	0	0	0	0	0
    8	0	0	0	1	0	0
    9	1	0	0	0	0	0
    10	0	0	0	0	0	0

    df[dt.rowany(f[:] > 0), :]


        a	b	c	d	e	f
    0	0	0	0	0	0	1
    1	0	0	5	0	0	0
    2	1	0	0	0	0	0
    3	0	0	0	1	0	0
    4	1	0	0	0	0	0

- Filter for rows where all the cells are 0 ::

    df[dt.rowall(f[:] == 0), :]


        a	b	c	d	e	f
    0	0	0	0	0	0	0
    1	0	0	0	0	0	0
    2	0	0	0	0	0	0
    3	0	0	0	0	0	0
    4	0	0	0	0	0	0
    5	0	0	0	0	0	0

- Filter for rows where all the columns' values are the same ::

    df = dt.Frame("""Name  A1   A2  A3  A4
                     deff  0    0   0   0
                     def1  0    1   0   0
                     def2  0    0   0   0
                     def3  1    0   0   0
                     def4  0    0   0   0""")

    # compare the first integer column with the rest,
    # use rowall to find rows where all is True
    # and filter with the resulting boolean
    df[dt.rowall(f[1]==f[1:]), :]  
    
        Name	A1	A2	A3	A4
    0	deff	0	0	0	0
    1	def2	0	0	0	0
    2	def4	0	0	0	0
               
Rowfirst, Rowlast
------------------
These look for the first and last non-missing value in a row respectively. ::

    df = dt.Frame({'A':[1, None, None, None], 
                   'B':[None, 3, 4, None], 
                   'C':[2, None, 5, None]})
    df

        A	B	C
    0	1	NA	2
    1	NA	3	NA
    2	NA	4	5
    3	NA	NA	NA

    # rowfirst :
    df[:, dt.rowfirst(f[:])]

        C0
    0	1
    1	3
    2	4
    3	NA

    # rowlast :
    df[:, dt.rowlast(f[:])]

        C0
    0	2
    1	3
    2	5
    3	NA

- Get rows where the last value in the row is greater than the first value in the row ::

    df = dt.Frame({'a': [50, 40, 30, 20, 10],
                   'b': [60, 10, 40, 0, 5],
                   'c': [40, 30, 20, 30, 40]})

    df

    	a	b	c
    0	50	60	40
    1	40	10	30
    2	30	40	20
    3	20	0	30
    4	10	5	40

    df[dt.rowlast(f[:]) > dt.rowfirst(f[:]), :]

        a	b	c
    0	20	0	30
    1	10	5	40



Rowmax, Rowmin
---------------
These get the maximum and values per row, respectively. ::

    df = dt.Frame({"C": [2, 5, 30, 20, 10],
                   "D": [10, 8, 20, 20, 1]})

    df

    	C	D
    0	2	10
    1	5	8
    2	30	20
    3	20	20
    4	10	1

    # rowmax
    df[:, dt.rowmax(f[:])]

        C0
    0	10
    1	8
    2	30
    3	20
    4	10

    # rowmin
    df[:, dt.rowmin(f[:])]

        C0
    0	2
    1	5
    2	20
    3	20
    4	1

- Find the difference between the maximum and minimum of each row ::

    df = dt.Frame("""Value1  Value2  Value3  Value4  
                        5       4      3        2       
                        4       3      2        1
                        3       3      5        1""")

    df[:, dt.update(max_min = dt.rowmax(f[:]) - dt.rowmin(f[:]))]
    df

	Value1	Value2	Value3	Value4	max_min
	    5	    4	    3	    2	    3
	    4	    3	    2	    1	    3
	    3	    3	    5	    1	    4

Rowsum, Rowmean, Rowcount, Rowsd
--------------------------------
``Rowsum`` and ``Rowmean`` get the sum and mean of rows respectively; ``Rowcount`` counts the number of non-missing values in a row, while ``Rowsd`` aggregates a row to get the standard deviation

- Get the count, sum, mean and standard deviation for each row ::

    df = dt.Frame("""ORD  A   B   C    D
                    198  23  45  NaN  12
                    138  25  NaN NaN  62
                    625  52  36  49   35
                    457  NaN NaN NaN  82
                    626  52  32  39   45""")

    df[:, dt.update(rowcount = dt.rowcount(f[:]),
                    rowsum = dt.rowsum(f[:]),
                    rowmean = dt.rowmean(f[:]),
                    rowsd = dt.rowsd(f[:])
                    )]

    df

        ORD	A	B	C	D	rowcount  rowsum  rowmean  rowsd
    0	198	23	45	NA	12	    4	    278	    69.5   86.7583
    1	138	25	NA	NA	62	    3	    225	    75	   57.6108
    2	625	52	36	49	35	    5	    797	    159.4  260.389
    3	457	NA	NA	NA	82	    2	    539	    269.5  265.165
    4	626	52	32	39	45	    5	    794	    158.8  261.277

- Find rows where the number of nulls is greater than 3 ::

    df = dt.Frame({'city': ["city1", "city2", "city3", "city4"],
                   'state': ["state1", "state2", "state3", "state4"],
                   '2005': [144, 205, 123, None],
                   '2006': [173, 211, 123, 124],
                   '2007': [None, None, None, None],
                   '2008': [None, 206, None, None],
                   '2009': [None, None, 124, 123],
                   '2010': [128, 273, None, None]})

    df

    	city	state	2005	2006	2007	2008	2009	2010
    0	city1	state1	144	173	NA	NA	NA	128
    1	city2	state2	205	211	NA	206	NA	273
    2	city3	state3	123	123	NA	NA	124	NA
    3	city4	state4	NA	124	NA	NA	123	NA

    # get columns that are null, then sum on the rows
    # and finally filter where the sum is greater than 3
    df[dt.rowsum(dt.isna(f[:])) > 3, :]

        city	state	2005	2006	2007	2008	2009	2010
    0	city4	state4	NA	124	NA	NA	123	NA

- Rowwise sum of the float columns ::

    df = dt.Frame("""ID   W_1       W_2     W_3 
                     1    0.1       0.2     0.3
                     1    0.2       0.4     0.5
                     2    0.3       0.3     0.2
                     2    0.1       0.3     0.4
                     2    0.2       0.0     0.5
                     1    0.5       0.3     0.2
                     1    0.4       0.2     0.1""")

    df[:, dt.update(sum_floats = dt.rowsum(f[float]))]

        ID	W_1	W_2	W_3	sum_floats
    0	1	0.1	0.2	0.3	0.6
    1	1	0.2	0.4	0.5	1.1
    2	2	0.3	0.3	0.2	0.8
    3	2	0.1	0.3	0.4	0.8
    4	2	0.2	0	0.5	0.7
    5	1	0.5	0.3	0.2	1
    6	1	0.4	0.2	0.1	0.7
