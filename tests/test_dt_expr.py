import pytest
import datatable as dt


# Sets of tuples containing test columns of each type
dt_bool = {(False, True, False, False, True),
           (True, None, None, True, False)}

dt_int = {(5, -3, 6, 3, 0),
          (None, -1, 0, 26, -3)}

dt_all = dt_bool | dt_int

# Designate which types are compatible with each operator
dt_invert_pass = dt_bool | dt_int
dt_neg_pass = dt_int
dt_pos_pass = dt_all
dt_isna_pass = dt_all

# ---------------------------------------------------------------------------
# Unary Operator Tests
# ---------------------------------------------------------------------------

# __invert__
@pytest.mark.parametrize("source,valid", [(x, True) for x in dt_invert_pass] +
                                         [(x, False) for x in dt_all - dt_invert_pass])
def test_dt_invert(source, valid):
    dt0 = dt.DataTable(source)
    if valid:
        dt_res = dt0(select=lambda f: ~f[0])
        assert dt_res.internal.check()
        assert dt_res.stypes == dt0.stypes
        assert dt_res.topython()[0] == \
            [None if x is None else not x if isinstance(x, bool) else ~x for x in source]
    else:
        with pytest.raises(TypeError):
            dt0(select=lambda f: ~f[0])

# __neg__
@pytest.mark.parametrize("source,valid", [(x, True) for x in dt_neg_pass] +
                                         [(x, False) for x in dt_all - dt_neg_pass])
def test_dt_neg(source, valid):
    dt0 = dt.DataTable(source)
    if valid:
        dt_res = dt0(select=lambda f: -f[0])
        assert dt_res.internal.check()
        assert dt_res.stypes == dt0.stypes
        assert dt_res.topython()[0] == \
            [None if x is None else -x for x in source]
    else:
        with pytest.raises(TypeError):
            dt0(select=lambda f: -f[0])

# __neg__
@pytest.mark.parametrize("source,valid", [(x, True) for x in dt_pos_pass] +
                                         [(x, False) for x in dt_all - dt_pos_pass])
def test_dt_pos(source, valid):
    dt0 = dt.DataTable(source)
    if valid:
        dt_res = dt0(select=lambda f: +f[0])
        assert dt_res.internal.check()
        assert dt_res.stypes == dt0.stypes
        assert dt_res.topython()[0] == list(source)
    else:
        with pytest.raises(TypeError):
            dt0(select=lambda f: +f[0])

# isna()
@pytest.mark.parametrize("source,valid", [(x, True) for x in dt_isna_pass] +
                                         [(x, False) for x in dt_all - dt_isna_pass])
def test_dt_isna(source, valid):
    dt0 = dt.DataTable(source)
    if valid:
        dt_res = dt0(select=lambda f: dt.isna(f[0]))
        assert dt_res.internal.check()
        assert dt_res.stypes == ("i1b",)
        assert dt_res.topython()[0] == \
            [x is None for x in source]
    else:
        with pytest.raises(TypeError):
            dt0(select=lambda f: dt.isna(f[0]))


