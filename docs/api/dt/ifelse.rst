
.. xfunction:: datatable.ifelse
    :src: src/core/expr/fexpr_ifelse.cc FExpr_IfElse::evaluate_n
    :doc: src/core/expr/fexpr_ifelse.cc doc_ifelse
    :tests: tests/expr/test-ifelse.py

Examples
--------

- Single condition
    - Task: Assign `Green` if `Set` is `Z`, else `Red`

.. code:: python

    from datatable import dt, f, by, ifelse

    df = dt.Frame("""Type       Set
                      A          Z
                      B          Z           
                      B          X
                      C          Y""")

    df[:, update(colour =  ifelse(f.Set=="Z",  # condition
                                  "Green",     # if condition is True
                                  "Red"))      # if condition is False
                                      ]

    df

        Type	Set	colour
    0	A	Z	Green
    1	B	Z	Green
    2	B	X	Red
    3	C	Y	Red

- Multiple conditions
    - Task: Create new column ``value`` where
         | if ``a`` > 0 then ``a``,
         | else if ``b`` > 0 then ``b``,
         | else ``c``

.. code:: python

    df = dt.Frame({"a": [0,0,1,2],
                   "b": [0,3,4,5],
                   "c": [6,7,8,9]}
    df

        a	b	c
    0	0	0	6
    1	0	3	7
    2	1	4	8
    3	2	5	9


    df[:, update(value=ifelse(f.a>0, f.a,  # first condition and result
                              f.b>0, f.b,  # second condtion and result
                              f.c))        # default if no codition is True
                              ]

    df

        a	b	c     value
    0	0	0	6	6
    1	0	3	7	3
    2	1	4	8	1
    3	2	5	9	2

