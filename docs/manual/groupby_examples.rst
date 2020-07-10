.. py:currentmodule:: datatable

Grouping with ``by``
====================

The ``by`` modifier splits a dataframe into groups, either via the provided column(s) or an ``f-expression``, and then applies ``i`` and ``j`` within each group.  This `split-apply-combine <https://www.jstatsoft.org/article/view/v040i01#:~:text=Abstract%3A,all%20the%20pieces%20back%20together.>`_ strategy allows for a number of operations:

- Aggregations per group
- Transformation of a column or columns, where the shape of the dataframe is maintained
- Filtration, where some data are kept and the others discarded, based on a condition or conditions

See :func:`by()` for more details.

Aggregation
-----------

The aggregate function is applied in the ``j`` section.

- Group by one column::

    from datatable import dt, f, by, ifelse, update, sort, count, min, max, mean, sum, rowsum, count

    #https://stackoverflow.com/questions/39922986/pandas-group-by-and-sum/51133583#51133583

    data =  """Fruit   Date       Name  Number
               Apples  10/6/2016  Bob     7
               Apples  10/6/2016  Bob     8
               Apples  10/6/2016  Mike    9
               Apples  10/7/2016  Steve  10
               Apples  10/7/2016  Bob     1
               Oranges 10/7/2016  Bob     2
               Oranges 10/6/2016  Tom    15
               Oranges 10/6/2016  Mike   57
               Oranges 10/6/2016  Bob    65
               Oranges 10/7/2016  Tony    1
               Grapes  10/7/2016  Bob     1
               Grapes  10/7/2016  Tom    87
               Grapes  10/7/2016  Bob    22
               Grapes  10/7/2016  Bob    12
               Grapes  10/7/2016  Tony   15"""

    df = dt.Frame(data)

    df[:, sum(f.Number), by('Fruit')]

        Fruit	Number
    0	Apples	35
    1	Grapes	137
    2	Oranges	140

- Group by multiple columns::

    df[:, sum(f.Number), by('Fruit', 'Name')]

        Fruit	Name	Number
    0	Apples	Bob	16
    1	Apples	Mike	9
    2	Apples	Steve	10
    3	Grapes	Bob	35
    4	Grapes	Tom	87
    5	Grapes	Tony	15
    6	Oranges	Bob	67
    7	Oranges	Mike	57
    8	Oranges	Tom	15
    9	Oranges	Tony	1

- By column position::

    df[:, sum(f.Number), by(f[0])]

        Fruit	Number
    0	Apples	35
    1	Grapes	137
    2	Oranges	140

- By boolean expression::

    df[:, sum(f.Number), by(f.Fruit == "Apples")]

        C0	Number
    0	0	277
    	1	35

- Combination of column and boolean expression::

    df[:, sum(f.Number), by(f.Name, f.Fruit == "Apples")]

       Name	C0	Number
    0	Bob	0	102
    1	Bob	1	16
    2	Mike	0	57
    3	Mike	1	9
    4	Steve	1	10
    5	Tom	0	102
    6	Tony	0	16


**Note:**
    - The resulting dataframe has the grouping column(s) as the first column(s).
    - The grouping columns are excluded from ``j``, unless explicitly included.

- Apply multiple aggregate functions to a column in the ``j`` section::

    aggs = {func.__name__ : func(f.Number) for func in (dt.min, dt.max)}

    df[:, aggs, by('Fruit','Date')]

        Fruit	Date	       min	max
    0	Apples	10/6/2016	7	9
    1	Apples	10/7/2016	1	10
    2	Grapes	10/7/2016	1	87
    3	Oranges	10/6/2016	15	65
    4	Oranges	10/7/2016	1	2

- `Apply aggregate functions to multiple columns <https://stackoverflow.com/questions/46431243/pandas-dataframe-groupby-how-to-get-sum-of-multiple-columns>`_::

    # Task: Get sum of col3 and col4, grouped by col1 and col2
   
    data = """ col1   col2   col3   col4   col5
                a      c      1      2      f
                a      c      1      2      f
                a      d      1      2      f
                b      d      1      2      g
                b      e      1      2      g
                b      e      1      2      g"""

    df = dt.Frame(data)

    df[:,[sum(f.col3), sum(f.col4)], by('col1', 'col2')]


        col1	col2	col3	col4
    0	a	c	2	4
    1	a	d	1	2
    2	b	d	1	2
    3	b	e	2	4

- If the columns are contiguous (next to each other), the aggregation can be applied once and replicated through the columns::

    df[:, sum(f["col3":"col4"]), by('col1', 'col2')]

            col1	col2	col3	col4
    0	a	c	2	4
    1	a	d	1	2
    2	b	d	1	2
    3	b	e	2	4

- Apply different aggregate functions to different columns::

    df[:,[max(f.col3), min(f.col4)], by('col1', 'col2')]

        col1	col2	col3	col4
    0	a	c	1	2
    1	a	d	1	2
    2	b	d	1	2
    3	b	e	1	2

- `Apply aggregate functions to separate groups of columns <https://stackoverflow.com/questions/46891001/pandas-sum-multiple-columns-and-get-results-in-multiple-columns>`_ in ``j``::

    # Task: Group by column idx and get the row sum of A and B, C and D
  
    data = """ idx  A   B   C   D   cat
                J   1   2   3   1   x
                K   4   5   6   2   x
                L   7   8   9   3   y
                M   1   2   3   4   y
                N   4   5   6   5   z
                O   7   8   9   6   z"""

    df = dt.Frame(data)

    df[:,
        {"AB" : rowsum(f['A':'B']),
         "CD" : rowsum(f['C':'D'])},
      by('idx')
      ]

        idx	AB	CD
    0	J	3	4
    1	K	9	8
    2	L	15	12
    3	M	3	7
    4	N	9	11
    5	O	15	15

- Nested aggregations::

    df[:,
        {"AB" : sum(rowsum(f['A':'B'])),
         "CD" : sum(rowsum(f['C':'D']))},
       by('cat')
       ]

        cat	AB	CD
    0	x	12	12
    1	y	18	19
    2	z	24	26

- `Computation between aggregated columns <https://stackoverflow.com/questions/40183800/pandas-difference-between-largest-and-smallest-value-within-group/40183864#40183864>`_::

    # Task : Get the difference between the largest and smallest value within each group
   
    data = """GROUP VALUE
                1     5
                2     2
                1     10
                2     20
                1     7"""

    df = dt.Frame(data)

    df[:,max(f.VALUE) - min(f.VALUE), by('GROUP')]

        GROUP	C0
    0	 1	 5
    1	 2	 18

- `Null values are not excluded from the grouping column <https://stackoverflow.com/questions/18429491/pandas-groupby-columns-with-nan-missing-values>`_::

    data = """  a    b    c
                1    2.0  3
                1    NaN  4
                2    1.0  3
                1    2.0  2"""

    df = dt.Frame(data)

    df[:, sum(f[:]), by('b')]

        	b	a	c
        0	NA	1	4
        1	1	2	3
        2	2	2	5

If you wish to ignore null values, you first filter them out::

    df[f.b != None, :][:, sum(f[:]), by('b')]

        b	a	c
    0	1	2	3
    1	2	2	5

Filtration
-----------

This occurs in the ``i`` section of the groupby, where only a subset of the data per group is needed; selection is limited to integers or slicing. 

**Note:**
    - ``i`` is applied after the grouping, not before.
    - ``f-expressions`` in the ``i`` section is not yet implemented for groupby.

- Select the first row per group::

    data =   """A B
                1 10
                1 20
                2 30
                2 40
                3 10"""

    df = dt.Frame(data)

    # passing 0 as index gets the first row after the grouping
    # note that python's index starts from 0, not 1
   
    df[0, :, by('A')]


        A	B
    0	1	10
    1	2	30
    2	3	10

- Select the last row per group::

    df[-1, :, by('A')]

        A	B
    0	1	20
    1	2	40
    2	3	10

- Select the nth row per group::

    # Task: select the second row per group
   
    df[1, :, by('A')]

    	A	B
    0	1	20
    1	2	40

**Note:**
    - Filtering this way can be used to drop duplicates; you can decide to keep the first or last non-duplicate.

- `Select the latest entry per group <https://stackoverflow.com/questions/41525911/group-by-pandas-dataframe-and-select-latest-in-each-group>`_::

    data = """   id    product   date
                220    6647     2014-09-01
                220    6647     2014-09-03
                220    6647     2014-10-16
                826    3380     2014-11-11
                826    3380     2014-12-09
                826    3380     2015-05-19
                901    4555     2014-09-01
                901    4555     2014-10-05
                901    4555     2014-11-01"""

    df = dt.Frame(data)

    df[-1, :, by('id'), sort('date')]

    	id	product	date
    0	220	6647	2014-10-16
    1	826	3380	2015-05-19
    2	901	4555	2014-11-01

**Note:**
    -If ``sort`` and ``by`` modifiers are present, the sorting occurs after the grouping, and occurs within each group.

- Replicate ``SQL``'s ``HAVING`` clause::

    #Task: Filter for groups where the length/count is greater than 1

    df = dt.Frame([[1, 1, 5], [2, 3, 6]], names=['A', 'B'])

    df
        A	B
    0	1	2
    1	1	3
    2	5	6

    # Get the count of each group,
    # and assign to a new column, using the update method
    # note that the update operation is in-place;
    # there is no need to assign back to the dataframe
    df[:, update(filter_col = count()), by('A')]

    # The new column will be added to the end
    # We use an f-expression to return rows
    # in each group where the count is greater than 1

    df[f.filter_col > 1, f[:-1]]

        A	B
    0	1	2
    1	1	3

- `Keep only rows per group where diff is the minimum <https://stackoverflow.com/questions/23394476/keep-other-columns-when-doing-groupby>`_::

    data = """ item    diff   otherstuff
                1       2            1
                1       1            2
                1       3            7
                2      -1            0
                2       1            3
                2       4            9
                2      -6            2
                3       0            0
                3       2            9"""

    df = dt.Frame(data)

    df[:,
       #get boolean for rows where diff column is minimum for each group
       update(filter_col = f.diff == min(f.diff)),
       by('item')]

    df[f.filter_col == 1, :-1]

        item	diff	otherstuff
    0	 1	 1	    2
    1	 2	−6	    2
    2	 3	 0	    0


- `Keep only entries where make has both 0 and 1 in sales <https://stackoverflow.com/questions/51109245/groupby-and-filter-pandas>`_::

    data = """   make    country other_columns   sale
                honda    tokyo       data          1
                honda    hirosima    data          0
                toyota   tokyo       data          1
                toyota   hirosima    data          0
                suzuki   tokyo       data          0
                suzuki   hirosima    data          0
                ferrari  tokyo       data          1
                ferrari  hirosima    data          0
                nissan   tokyo       data          1
                nissan   hirosima    data          0"""

    df = dt.Frame(data)

    df[:,
       update(filter_col = sum(f.sale)),
       by('make')]

    df[f.filter_col == 1, :-1]

        make	 country  other_columns	  sale
    0	honda	 tokyo	        data	    1
    1	honda	 hirosima	data	    0
    2	toyota	 tokyo	        data	    1
    3	toyota	 hirosima	data	    0
    4	ferrari	 tokyo	        data	    1
    5	ferrari	 hirosima	data	    0
    6	nissan	 tokyo	        data	    1
    7	nissan	 hirosima	data	    0

Transformation
--------------

This is when a function is applied to a column after a groupby and the resulting column is appended back to the dataframe.  The number of rows of the dataframe is unchanged. The ``update`` method makes this possible and easy. Let's look at a couple of examples:

- `Get the minimum and maximum of column <https://stackoverflow.com/questions/51074911/pandas-get-minimum-of-one-column-in-group-when-groupby-another>`_ per group, and append to dataframe::

    data = """  c     y
                9     0
                8     0
                3     1
                6     2
                1     3
                2     3
                5     3
                4     4
                0     4
                7     4"""

    df = dt.Frame(data)

    # Assign the new columns via the update method

    df[:,
       update(min_col = min(f.c),
              max_col = max(f.c)),
      by('y')]

    df
                c	y   min_col  max_col
        0	9	0	8	9
        1	8	0	8	9
        2	3	1	3	3
        3	6	2	6	6
        4	1	3	1	5
        5	2	3	1	5
        6	5	3	1	5
        7	4	4	0	7
        8	0	4	0	7
        9	7	4	0	7

- `Fill missing values by group mean <https://stackoverflow.com/questions/19966018/pandas-filling-missing-values-by-mean-in-each-group>`_::

    data = {'value' : [1, np.nan, np.nan, 2, 3, 1, 3, np.nan, 3],
            'name' : ['A','A', 'B','B','B','B', 'C','C','C']}

    df = dt.Frame(data)

    df
        value	name
    0	1	A
    1	NA	A
    2	NA	B
    3	2	B
    4	3	B
    5	1	B
    6	3	C
    7	NA	C
    8	3	C

    # This uses a combination of update and ifelse methods:

    df[:,
       update(value = ifelse(f.value == None,
                             mean(f.value),
                             f.value)),
       by('name')]

    df
        value	name
    0	1	A
    1	1	A
    2	2	B
    3	2	B
    4	3	B
    5	1	B
    6	3	C
    7	3	C
    8	3	C

- `Transform and Aggregate on Multiple Columns <https://stackoverflow.com/questions/53212490/pandas-groupby-transform-and-multiple-columns>`_::

    df = dt.Frame({'a' : [1,2,3,4,5,6],
                   'b' : [1,2,3,4,5,6],
                   'c' : ['q', 'q', 'q', 'q', 'w', 'w'],
                   'd' : ['z','z','z','o','o','o']})
    df

        a	b	c	d
    0	1	1	q	z
    1	2	2	q	z
    2	3	3	q	z
    3	4	4	q	o
    4	5	5	w	o
    5	6	6	w	o

    # Task: Get the sum of the aggregate of column a and b,
    # grouped by c and d,
    # and append to dataframe

    df[:,
       update(e = sum(f.a) + sum(f.b)),
       by('c', 'd')
       ]

    df

        a	b	c	d	e
    0	1	1	q	z	12
    1	2	2	q	z	12
    2	3	3	q	z	12
    3	4	4	q	o	8
    4	5	5	w	o	22
    5	6	6	w	o	22

- `Replicate R's groupby mutate <https://stackoverflow.com/questions/40923165/python-pandas-equivalent-to-r-groupby-mutate>`_::

    df = dt.Frame(dict(a = (1,1,0,1,0),
                       b = (1,0,0,1,0),
                       c = (10,5,1,5,10),
                       d = (3,1,2,1,2))
                  )

    df

        a	b	c	d
    0	1	1	10	3
    1	1	0	5	1
    2	0	0	1	2
    3	1	1	5	1
    4	0	0	10	2

    # Task: Get ratio by dividing column c by the product of column c and d,
    # grouped by a and b

    df[:,
       update(ratio = f.c / sum(f.c * f.d)),
       by('a', 'b')
       ]

    df

        a	b	c	d	ratio
    0	1	1	10	3	0.285714
    1	1	0	5	1	1
    2	0	0	1	2	0.0454545
    3	1	1	5	1	0.142857
    4	0	0	10	2	0.454545



Groupby on Boolean Expressions
-------------------------------

- `Conditional Sum with groupby <https://stackoverflow.com/questions/17266129/python-pandas-conditional-sum-with-groupby>`_::

    data = """    data1        data2     key1  key2
                 0.361601    0.375297     a    one
                 0.069889    0.809772     a    two
                 1.468194    0.272929     b    one
                -1.138458    0.865060     b    two
                -0.268210    1.250340     a    one"""

    df = dt.Frame(data)

    # Task: Sum data1 column, grouped by key1 and rows where key2== "one"

    df[:,
       sum(f.data1),
       by(f.key2 == "one", f.key1)][f.C0 == 1, 1:]

        key1	data1
    0	a	0.093391
    1	b	1.46819

- `Conditional Sums based on various Criteria <https://stackoverflow.com/questions/15259547/conditional-sums-for-pandas-aggregate>`_::

    data = """ A_id       B       C
                a1      "up"     100
                a2     "down"    102
                a3      "up"     100
                a3      "up"     250
                a4     "left"    100
                a5     "right"   102"""

    df[:,
       {"sum_up": sum(f.B == "up"),
        "sum_down" : sum(f.B == "down"),
        "over_200_up" : sum((f.B == "up") & (f.C > 200))
        },
       by('A_id')]

       A_id	sum_up	sum_down  over_200_up
    0	a1	  1	     0	        0
    1	a2	  0	     1	        0
    2	a3	  2	     0	        1
    3	a4	  0	     0	        0
    4	a5	  0	     0	        0


More Examples
-------------

- `Aggregation on Values in a Column <https://stackoverflow.com/questions/46501703/groupby-column-and-find-min-and-max-of-each-group>`_::

    data = """  Day    Element  Data_Value
                01-01   TMAX    112
                01-01   TMAX    101
                01-01   TMIN    60
                01-01   TMIN    0
                01-01   TMIN    25
                01-01   TMAX    113
                01-01   TMAX    115
                01-01   TMAX    105
                01-01   TMAX    111
                01-01   TMIN    44
                01-01   TMIN    83
                01-02   TMAX    70
                01-02   TMAX    79
                01-02   TMIN    0
                01-02   TMIN    60
                01-02   TMAX    73
                01-02   TMIN    31
                01-02   TMIN    26
                01-02   TMAX    71
                01-02   TMIN    26"""

    # Task : group by Day and find minimum Data_Value for TMIN 
    # and maximum Data_Value for TMAX

    df[:,
       f.Day.extend({"TMAX" : ifelse(f.Element=="TMAX", f.Data_Value, None),
                     "TMIN" : ifelse(f.Element=="TMIN", f.Data_Value, None)})
      ][:, [max(f.TMAX), min(f.TMIN)], by('Day')]


        Day	TMAX	TMIN
    0	01-01	115	0
    1	01-02	79	0

- `Filter row based on aggregate value <https://stackoverflow.com/questions/15322632/python-pandas-df-groupby-agg-column-reference-in-agg>`_::

    data = """  word  tag count
                a     S    30
                the   S    20
                a     T    60
                an    T    5
                the   T    10"""

    df = dt.Frame(data)

    # Task: find, for every "word", the "tag" that has the most "count"

    df[:,
       update(filter_col = f.count == max(f.count)),
       by('word')
       ]

    df[f.filter_col == 1, f[:-1]]

        word	tag	count
    0	the	S	20
    1	a	T	60
    2	an	T	5


- `Group By and Conditional Sum and Add Back to Data Frame <https://stackoverflow.com/questions/62774794/pandas-group-by-and-conditional-sum-and-add-back-to-data-frame#62774845>`_::

    data =  """ ID  Num  Letter  Count
                1   17   D       1
                1   12   D       2
                1   13   D       3
                2   17   D       4
                2   12   A       5
                2   16   D       1
                3   16   D       1"""

    df = dt.Frame(data)

    # Task: Sum the 'Count' value for each 'ID',
    # when 'Num' is (17 or 12) and 'Letter' is 'D',
    # and also add the calculation back to the original data frame in 'Total'.

    expression = ((f.Num==17) | (f.Num==12)) & (f.Letter == "D")

    df[:,
       update(Total = sum(ifelse(expression, f.Count, 0))),
       by('ID')]

    df

        ID	Num	Letter	Count	Total
    0	1	17	D	1	3
    1	1	12	D	2	3
    2	1	13	D	3	3
    3	2	17	D	4	4
    4	2	12	A	5	4
    5	2	16	D	1	4
    6	3	16	D	1	0


- `Multiple indexing with multiple min and max in one aggregate <https://stackoverflow.com/questions/62295617/multiple-indexing-with-multiple-idxmin-and-idmax-in-one-aggregate-in-pandas/62295885#62295885>`_::

    data = {
            "id" : [1, 1, 1, 2, 2, 2, 2, 3, 3, 3],
            "col1" : [1, 3, 5, 2, 5, 3, 6, 3, 67, 7],
            "col2" : [4, 6, 8, 3, 65, 3, 5, 4, 4, 7],
            "col3" : [34, 64, 53, 5, 6, 2, 4, 6, 4, 67],
            }

    df = dt.Frame(data)

    df

        id	col1	col2	col3
    0	1	1	4	34
    1	1	3	6	64
    2	1	5	8	53
    3	2	2	3	5
    4	2	5	65	6
    5	2	3	3	2
    6	2	6	5	4
    7	3	3	4	6
    8	3	67	4	4
    9	3	7	7	67

    # Task : find col1 where col2 is max,
    # col2 where col3 is min,
    # and col1 where col3 is max

    df[:,
       {'col1' : sum(ifelse(f.col2 == max(f.col2),
                            f.col1, None)),

        'col2' : sum(ifelse(f.col3 == min(f.col3),
                            f.col2, None)),

        'col3' : sum(ifelse(f.col3 == max(f.col3),
                            f.col1, None))
          },
       by('id')]

        id	col1	col2	col3
    0	1	5	4	3
    1	2	5	3	5
    2	3	7	4	7

