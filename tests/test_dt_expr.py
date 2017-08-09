import pytest
import datatable as dt
from tests import assert_equals


def test_dt_isna_single_col():
    dt0 = dt.DataTable([None, 4, None, 39, 2])
    res = dt.isna(dt0)
    assert res.internal.check()
    assert res.types == ("bool", )
    assert_equals(res, dt.DataTable({"V0": [True, False, True, False, False]}))

    

def test_dt_isna_multi_col():
    dt0 = dt.DataTable([[3, None, 2],
                        [None, 3, 9]])
    with pytest.raises(TypeError) as e:
        dt.isna(dt0)



def test_dt_isna_expr():
    dt0 = dt.DataTable([[6, 2, None, None, 6],
                        [2, None, 5, 2, None],
                        [None, 2, 7, None, 3.5]],
                       colnames=list("ABC"))
    res = dt0[lambda f: dt.isna(f.C), :]
    assert res.internal.check()
    assert_equals(res[:-1], dt.DataTable([[6, None],
                                          [2, 2]],
                                          colnames=list("AB")))



@pytest.mark.skip(reason="str handling not fully implemented")
def test_dt_isna_expr_str():
    dt0 = dt.DataTable([[6, 2, None, None, 6],
                        [2, None, 5, 2, None],
                        [None, 2, 7, 6, "hello"]],
                       colnames=list("ABC"))
    res = dt0[lambda f: dt.isna(f.C), :]
    assert res.internal.check()
    assert_equals(res, dt.DataTable([[6],
                                     [2],
                                     [None]],
                                    colnames=list("ABC")))

