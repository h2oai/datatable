

Fread Examples
=================

Read Data
----------

Read text data directly::

    from datatable import dt, fread

    data = ('col1,col2,col3\n'
            'a,b,1\n'
            'a,b,2\n'
            'c,d,3')

    fread(data)

    col1  col2  col3
     a     b     1
     a     b     2
     a     b     3

Read from text file (any file that has the `.read()` method)::

    result = fread('iris.csv')
    result.head(5)

        sepal_length	sepal_width	petal_length	petal_width	species
    0	5.1	            3.5	            1.4	            0.2	        setosa
    1	4.9	            3	            1.4	            0.2	        setosa
    2	4.7	            3.2	            1.3	            0.2	        setosa
    3	4.6	            3.1	            1.5	            0.2	        setosa
    4	5	            3.6	            1.4	            0.2	        setosa


Read from url::

    url = "https://raw.githubusercontent.com/Rdatatable/data.table/master/vignettes/flights14.csv"
    fread(url)

Read from an archive. If there are multiple files, only the first will be read; you can specify the path to the specific file you are interested in::


    fread("data.zip/mtcars.csv")

Read from `.xlsx` files - `.xls` files are not supported::

    fread("excel.xlsx")

For excel files, you can specify the sheet to be read::

    fread("excel.xlsx/Sheet1")

Note: `xlrd <https://pypi.org/project/xlrd/>`_ must be installed to read in excel files.

You can also read in data from the command line. Simply pass the command line statement to the `cmd` parameter::

     #https://blog.jpalardy.com/posts/awk-tutorial-part-2/
     #You specify the `cmd` parameter
     #Here we filter data for the year 2015
     fread(cmd = """cat netflix.tsv | awk 'NR==1; /^2015-/'""")

The command line can be very handy with large data; you can do some of the preprocessing before reading in the data to `datatable`.

Dealing with Unicode Data
-------------------------

`Fread` is pretty powerful and can automatically detect the encoding in a number of cases.::

    data = ("num,name\n"
            "1,alïce\n"
            "2,bøb\n"
            "3,cárol\n"
            )

    fread(data)

              num       name
        0	1	alïce
        1	2	bøb
        2	3	cárol

You can also specify the encoding; `fread` will recode into `UTF-8` before reading.
Check `here <https://docs.python.org/3/library/codecs.html#standard-encodings>`_ for Python's list of standard encodings.

Detect Thousand Separator
-------------------------

Thousand separators are automatically detected::

    data = """Name|Salary|Position
             James|256,000|evangelist
            Ragnar|1,000,000|conqueror
              Loki|250360|trickster"""

    fread(data)

        Name	Salary	Position
    0	James	256000	evangelist
    1	Ragnar	1000000	conqueror
    2	Loki	250360	trickster

Specify the Delimiter
---------------------

You can specify the delimiter via the `sep` parameter.
Note that the  separator must be a single character string; non-ASCII characters are not allowed as the separator, as well as any characters in ``["'`0-9a-zA-Z]``::

    data = """
           1:2:3:4
           5:6:7:8
           9:10:11:12
           """

    fread(data, sep=":")

    	C0	C1	C2	C3
    0	1	2	3	4
    1	5	6	7	8
    2	9	10	11	12

Dealing with Null Values and Blank Rows
---------------------------------------

You can pass a list of values to be treated as null, via the `na_strings` parameter::

    data = """
           ID|Charges|Payment_Method
           634-VHG|28|Cheque
           365-DQC|33.5|Credit card
           264-PPR|631|--
           845-AJO|42.3|
           789-KPO|56.9|Bank Transfer
           """

    fread(data, na_strings=['--', ''])

        ID	    Charges  Payment_Method
    0	634-VHG	    28	     Cheque
    1	365-DQC	    33.5     Credit card
    2	264-PPR	    631	     NA
    3	845-AJO	    42.3     NA
    4	789-KPO	    56.9     Bank Transfer


For rows with null values, set `fill=True`::

    data = ('a,b,c,d\n'
            '1,2,3,4\n'
            '5,6,7,8\n'
            '9,10,11')

    fread(data, fill=True)

    	a	b	c	d
    0	1	2	3	4
    1	5	6	7	8
    2	9	10	11	NA

You can skip empty lines::

    data = ('a,b,c,d\n'
            '\n'
            '1,2,3,4\n'
            '5,6,7,8\n'
            '\n'
            '9,10,11,12')

    fread(data, skip_blank_lines=True)

        a	b	c	d
    0	1	2	3	4
    1	5	6	7	8
    2	9	10	11	12

Dealing with Column Names
-------------------------

If the data has no headers, `fread` will assign default column names::

    data = ('1,2\n'
            '3,4\n')

    fread(data)

        C0	C1
    0	1	2
    1	3	4

You can pass in column names via the `columns` parameter::

    fread(data, columns=['A','B'])

        A	B
    0	1	2
    1	3	4

You can change column names with the `columns` parameter::

    data = ('a,b,c,d\n'
            '1,2,3,4\n'
            '5,6,7,8\n'
            '9,10,11,12')

    fread(data, columns=["A","B","C","D"])

        A	B	C	D
    0	1	2	3	4
    1	5	6	7	8
    2	9	10	11	12

You can change `some` of the column names via a `dictionary`::

    fread(data, columns={"a":"A", "b":"B"})

        A	B	c	d
    0	1	2	3	4
    1	5	6	7	8
    2	9	10	11	12

By deafult, `fread` takes the first line in the data as the header. If, however, you do not want the first line as the header, set the `header` parameter to False::

    fread(data,  header=False)


        C0	C1	C2	C3
    0	a	b	c	d
    1	1	2	3	4
    2	5	6	7	8
    3	9	10	11	12

You can pass a new list of column names as well::

    fread(data,  header=False, columns=["A","B","C","D"])

    	A	B	C	D
    0	a	b	c	d
    1	1	2	3	4
    2	5	6	7	8
    3	9	10	11	12

Row Selection
-------------

`Fread` has a `skip_to_line` parameter, where you can specify what line to read data from::

    data = ('skip this line\n'
            'a,b,c,d\n'
            '1,2,3,4\n'
            '5,6,7,8\n'
            '9,10,11,12')

    fread(data, skip_to_line=2)

        a	b	c	d
    0	1	2	3	4
    1	5	6	7	8
    2	9	10	11	12

You can also skip to a line containing a particular string, and start reading data from that line. Note that `skip_to_string` and `skip_to_line` cannot be combined; you can only use one::

    data = ('skip this line\n'
            'a,b,c,d\n'
            'first, second, third, last\n'
            '1,2,3,4\n'
            '5,6,7,8\n'
            '9,10,11,12')

    fread(data, skip_to_string='first')


        first	second	third	last
    0	1	2	3	4
    1	5	6	7	8
    2	9	10	11	12


You can set the maximum number of rows to read::

    data = ('a,b,c,d\n'
            '1,2,3,4\n'
            '5,6,7,8\n'
            '9,10,11,12')

    fread(data, max_nrows=2)


        a	b	c	d
    0	1	2	3	4
    1	5	6	7	8

    data = ('skip this line\n'
            'a,b,c,d\n'
            '1,2,3,4\n'
            '5,6,7,8\n'
            '9,10,11,12')

    fread(data, skip_to_line=2, max_nrows=2)

        a	b	c	d
    0	1	2	3	4
    1	5	6	7	8

Setting Column Type
--------------------

You can determine the data types via the `columns` parameter::

    data = ('a,b,c,d\n'
            '1,2,3,4\n'
            '5,6,7,8\n'
            '9,10,11,12')

    #this is useful when you are interested in only a subset of the columns
    fread(data, columns={"a":dt.float32, "b":dt.str32})

You can also pass in the data types by position::

    fread(data, columns = (stype.int32, stype.str32, stype.float32))

You can also change `all` the column data types with a single assignment::

    fread(data, columns = dt.float32)

You can change the data type for a `slice` of the columns::

    #this changes the data type to float for the first three columns
    fread(data, columns={float:slice(3)})

Note that there are a small number of stypes within `datatable` (`int8`, `int16`, `int32`, `int64`, `float32`, `float64`, `str32`, `str64`, `obj64`, `bool8`)

Selecting Columns
-----------------

There are various ways to select columns in `fread` :

- Select with a `dictionary`::

        data = ('a,b,c,d\n'
                '1,2,3,4\n'
                '5,6,7,8\n'
                '9,10,11,12')

        #pass Ellipsis:None to discard any columns that are not needed
        fread(data, columns={"a":"a", Ellipsis:None})

        a
    0	1
    1	5
    2	9

Selecting via a dictionary makes more sense when selecting and renaming columns at the same time.


- Select columns with a `set`::

    fread(data, columns={"a","b"})

        a	b
    0	1	2
    1	5	6
    2	9	10

- Select range of columns with `slice`::

    fread(data, columns=slice(1,3))

        b	c
    0	2	3
    1	6	7
    2	10	11

    fread(data, columns = slice(None,3,2))

        a	c
    0	1	3
    1	5	7
    2	9	11


- Select range of columns with `range`::

    fread(data, columns = range(1,3))

        b	c
    0	2	3
    1	6	7
    2	10	11

- Boolean Selection::

    fread(data, columns=[False, False, True, True])

        c	d
    0	3	4
    1	7	8
    2	11	12

- Select with a list comprehension::

    fread(data, columns=lambda cols:[col.name in ("a","c") for col in cols])

        a	c
    0	1	3
    1	5	7
    2	9	11

- Exclude columns with `None`::

    fread(data, columns = ['a',None,None,'d'])

    	a	d
    0	1	4
    1	5	8
    2	9	12

- Drop columns by assigning `None` to the columns via a `dictionary`::

    data = ("A,B,C,D\n"
            "1,3,5,7\n"
            "2,4,6,8\n")

    fread(data, columns={"B":None,"D":None})

        A	C
    0	1	5
    1	2	6

- Exclude columns with list comprehension::

    fread(data, columns=lambda cols:[col.name not in ("a","c") for col in cols])


        b	d
    0	2	4
    1	6	8
    2	10	12

- Drop a column and change data type::

    fread(data, columns={"B":None, "C":str})

    	A	C	D
    0	1	5	7
    1	2	6	8

- Change column name and type, and drop a column::

     #pass a tuple, where the first item in the tuple is the new column name,
     #and the other item is the new data type.
    fread(data, columns={"A":("first", float), "B":None,"D":None})

        first	C
    0	1	5
    1	2	6

With list comprehensions, you can dynamically select columns::

    #select columns that have length, and species column
    fread('iris.csv',
      #use a boolean list comprehension to get the required columns
      columns = lambda cols : [(col.name=='species')
                               or ("length" in col.name)
                               for col in cols],
      max_nrows=5)

      sepal_length	petal_length	species
    0	5.1	            1.4	        setosa
    1	4.9	            1.4	        setosa
    2	4.7	            1.3	        setosa
    3	4.6	            1.5	        setosa
    4	5	            1.4	        setosa


    #select columns by position
    fread('iris.csv',
           columns = lambda cols : [ind in (1,4) for ind, col in enumerate(cols)],
           max_nrows=5)

        sepal_length	petal_length	petal_width
    0	5.1	                1.4	    0.2
    1	4.9	                1.4	    0.2
    2	4.7	                1.3	    0.2
    3	4.6	                1.5	    0.2
    4	5	                1.4         0.2



