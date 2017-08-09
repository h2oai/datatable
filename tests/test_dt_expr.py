import pytest
import datatable as dt
from tests import assert_equals


# Param 0 contains list of indicated type
# Param 1 contains list of indicated type w/ NAs

@pytest.fixture(params=[[False, True, False, False, True],
                        [True, None, None, True, False]])
def dt_bool(request):
    return request.param, dt.DataTable(request.param)


@pytest.fixture(params=[[5, -3, 6, 3, 0],
                        [None, -1, 0, 26, -3]])
def dt_int(request):
    return request.param, dt.DataTable(request.param)


# ---------------------------------------------------------------------------
# UnaryOpExpr (__neg__, __pos__, __invert__)
# ---------------------------------------------------------------------------

# __invert__
def test_dt_unary_invert_bool(dt_bool):
    source, dt0 = dt_bool
    res = dt0(rows=lambda f: ~f[0])
    assert res.internal.check()
    res_source = [x for x in source if x is not None and not x]
    assert_equals(res, dt.DataTable(res_source))

def test_dt_unary_invert_int(dt_int):
    source, dt0 = dt_int
    res = dt0(rows=lambda f: ~f[0] > 0)
    assert res.internal.check()
    res_source = [x for x in source if x is not None and ~x > 0]
    assert_equals(res, dt.DataTable(res_source))


# __pos__ (equivalent to noop)
def test_dt_unary_pos_bool(dt_bool):
    source, dt0 = dt_bool
    res = dt0[lambda f: +f[0], :]
    assert res.internal.check()
    res_source = [x for x in source if x is not None and x]
    assert_equals(res, dt.DataTable(res_source))

def test_dt_unary_pos_int(dt_int):
    source, dt0 = dt_int
    res = dt0[lambda f: +f[0] > 0, :]
    assert res.internal.check()
    source_res = [x for x in source if x is not None and x > 0]
    assert_equals(res, dt.DataTable(source_res))


# __neg__
def test_dt_unary_neg_bool(dt_bool):
    source, dt0 = dt_bool
    with pytest.raises(TypeError):
        res = dt0[lambda f:-f[0], :]

def test_dt_unary_neg_int(dt_int):
    source, dt0 = dt_int
    res = dt0[lambda f: -f[0] > 0, :]
    assert res.internal.check()
    source_res = [x for x in source if x is not None and -x > 0]
    assert_equals(res, dt.DataTable(source_res))


# ---------------------------------------------------------------------------
# Isna
# ---------------------------------------------------------------------------
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

