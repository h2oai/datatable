
.. xmethod:: datatable.FExpr.__init__
    :src: src/core/expr/fexpr.cc PyFExpr::m__init__

    __init__(self, e)
    --

    Create a new :class:`dt.FExpr` object out of ``e``.

    The ``FExpr`` serves as a simple wrapper of the underlying object,
    allowing it to be combined with othef ``FExpr``s.

    This constructor almost never needs to be run manually by the user.


    Parameters
    ----------
    e: None | bool | int | str | float | slice | list |  tuple | dict | \
       type | stype | ltype | Generator | FExpr | Frame | range | \
       pd.DataFrame | pd.Series | np.array | np.ma.masked_array
        The argument that will be converted into an ``FExpr``.

