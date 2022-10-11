
.. xfunction:: datatable.categories
    :src: src/core/expr/fexpr_categories.cc pyfn_categories
    :tests: tests/types/test-categorical.py
    :cvar: doc_dt_categories
    :signature: categories(cols)

    .. x-version-added:: 1.1.0

    For each column from `cols` get the underlying categories.

    Parameters
    ----------
    cols: FExpr
        Input categorical data.

    return: FExpr
        f-expression that returns categories for each column
        from `cols`.

    except: TypeError
        The exception is raised when one of the columns from `cols`
        has a non-categorical type.
