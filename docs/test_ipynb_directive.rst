test_ipynb_directive
====================

This section describes how to install H2O's ``datatable``.


.. ipython:: python

    x = 2
    x**3



.. ipython:: python

    import datatable as dt
    import math
    DT = dt.Frame(A=range(5), B=[1.7, 3.4, 0, None, -math.inf],
                     stypes={"A": dt.int64})
    DT


.. ipython:: python

    import pandas as pd
    pd.DataFrame({"A": range(3)})