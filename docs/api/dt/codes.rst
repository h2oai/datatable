
.. xfunction:: datatable.codes
    :src: src/core/expr/fexpr_codes.cc pyfn_codes
    :tests: tests/types/test-categorical.py
    :cvar: doc_dt_codes
    :signature: codes(cols)

    .. x-version-added:: 1.1.0

    Get integer codes for categorical data.

    Parameters
    ----------
    cols: FExpr
        Input categorical data.

    return: FExpr
        f-expression that returns integer codes for each column
        from `cols`.

    except: TypeError
        The exception is raised when one of the columns from `cols`
        has a non-categorical type.
