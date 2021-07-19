
.. xfunction:: datatable.join
    :src: src/core/expr/py_join.cc ojoin::pyobj::m__init__
    :cvar: doc_dt_join
    :tests: tests/test-join.py
    :signature: join(frame)

    Join clause for use in Frame’s square-bracket selector.

    This clause is equivalent to the SQL `JOIN`, though for the moment
    datatable only supports left outer joins. In order to join,
    the `frame` must be :attr:`keyed <dt.Frame.key>` first, and then joined
    to another frame `DT` as::

        >>> DT[:, :, join(X)]

    provided that `DT` has the column(s) with the same name(s) as
    the key in `frame`.

    Parameters
    ----------
    frame: Frame
        An input keyed frame to be joined to the current one.

    return: Join Object
        In most of the cases the returned object is directly used in the
        Frame’s square-bracket selector.

    except: ValueError
        The exception is raised if `frame` is not keyed.

    See Also
    --------
    - :ref:`Tutorial on joins <join tutorial>`

    Examples
    --------
    .. code-block:: python

        >>> df1 = dt.Frame("""    date    X1  X2
        ...                   01-01-2020  H   10
        ...                   01-02-2020  H   30
        ...                   01-03-2020  Y   15
        ...                   01-04-2020  Y   20""")
        >>>
        >>> df2 = dt.Frame("""X1  X3
        ...                   H   5
        ...                   Y   10""")


    First, create a key on the right frame (``df2``). Note that the join key
    (``X1``) has unique values and has the same name in the left frame (``df1``)::

        >>> df2.key = "X1"

    Join is now possible::

        >>> df1[:, :, join(df2)]
           | date        X1        X2     X3
           | str32       str32  int32  int32
        -- + ----------  -----  -----  -----
         0 | 01-01-2020  H         10      5
         1 | 01-02-2020  H         30      5
         2 | 01-03-2020  Y         15     10
         3 | 01-04-2020  Y         20     10
        [4 rows x 4 columns]

    You can refer to columns of the joined frame using prefix :data:`g. <dt.g>`, similar to how columns of the left frame can be accessed using prefix :data:`f. <dt.f>`::

        >>> df1[:, update(X2=f.X2 * g.X3), join(df2)]
        >>> df1
           | date        X1        X2
           | str32       str32  int32
        -- + ----------  -----  -----
         0 | 01-01-2020  H         50
         1 | 01-02-2020  H        150
         2 | 01-03-2020  Y        150
         3 | 01-04-2020  Y        200
        [4 rows x 3 columns]



