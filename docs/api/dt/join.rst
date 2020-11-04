
.. xfunction:: datatable.join
    :src: src/core/expr/py_join.cc ojoin::pyobj::m__init__
    :doc: src/core/expr/py_join.cc doc_join
    :tests: tests/test-join.py

Examples
--------

.. code-block:: python

    df1 = dt.Frame("""date	X1	X2
            	01-01-2020	H	10
            	01-02-2020	H	30
            	01-03-2020	Y	15
            	01-04-2020	Y	20""")

    df2 = dt.Frame("""X1	X3
            	       H	5
                       Y	10""")


First, create a key on the right frame (``df2``). Note that the join key(``X1``) has unique values and has the same name in the left frame(``df1``)::

    df2.key = "X1"

Join is now possible::

    df1[:, :, join(df2)]

        date	        X1	X2	X3
    0	01-01-2020	H	10	5
    1	01-02-2020	H	30	5
    2	01-03-2020	Y	15	10
    3	01-04-2020	Y	20	10

You can access the values in the right frame in an expression, using the secondary namespace object `g  <https://datatable.readthedocs.io/en/latest/api/dt/g.html#>`__::

    df1[:, update(X2=f.X2 * g.X3), join(df2)]
    df1

            date	X1	X2
    0	01-01-2020	H	50
    1	01-02-2020	H	150
    2	01-03-2020	Y	150
    3	01-04-2020	Y	200



