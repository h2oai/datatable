
.. xfunction:: datatable.update
    :src: src/core/expr/py_update.cc oupdate::oupdate_pyobject::m__init__
    :doc: src/core/expr/py_update.cc doc_update


Examples
--------

.. code-block:: python

    from datatable import update

    DT = dt.Frame([range(5), [4, 3, 9, 11, -1]], names=("A", "B"))

        A	B
    0	0	4
    1	1	3
    2	2	9
    3	3	11
    4	4      −1

- Create new columns and update existing columns::

    DT[:, update(C = f.A * 2,
                 D = f.B // 3,
                 A = f.A * 4,
                 B = f.B + 1)]

        A	B	C	D
    0	0	5	0	1
    1	4	4	2	1
    2	8	10	4	3
    3	12	12	6	3
    4	16	0	8      −1

- Add new column with `unpacking <https://docs.python.org/3/tutorial/controlflow.html#unpacking-argument-lists>`__; this can be handy for dynamicallly adding columns with dictionary comprehensions, or if the names are not valid python keywords ::

    DT[:, update(**{"extra column": f.A + f.B + f.C + f.D})]
    DT

        A	B	C	D  extra column
    0	0	5	0	1	6
    1	4	4	2	1	11
    2	8	10	4	3	25
    3	12	12	6	3	33
    4	16	0	8      −1	23

- You can also update a subset of data ::

    DT[f.A > 10, update(A=f.A * 5)]
    DT

        A	B	C	D  extra column
    0	0	5	0	1	6
    1	4	4	2	1	11
    2	8	10	4	3	25
    3	60	12	6	3	33
    4	80	0	8      −1	23



