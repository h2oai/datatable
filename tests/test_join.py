#!/usr/bin/env python
# © H2O.ai 2018; -*- encoding: utf-8 -*-
#   This Source Code Form is subject to the terms of the Mozilla Public
#   License, v. 2.0. If a copy of the MPL was not distributed with this
#   file, You can obtain one at http://mozilla.org/MPL/2.0/.
#-------------------------------------------------------------------------------
import datatable as dt
import pytest
import random
from tests import random_string
from datatable import join, ltype, stype, f, g, mean



def test_join_simple():
    d0 = dt.Frame([[1, 3, 2, 1, 1, 2, 0], list("abcdefg")], names=("A", "B"))
    d1 = dt.Frame([range(4), ["zero", "one", "two", "three"]], names=("A", "V"),
                  stypes=d0.stypes)
    d1.key = "A"
    res = d0[:, :, join(d1)]
    res.internal.check()
    assert res.shape == (7, 3)
    assert res.names == ("A", "B", "V")
    assert res.topython() == [
        [1, 3, 2, 1, 1, 2, 0],
        ["a", "b", "c", "d", "e", "f", "g"],
        ["one", "three", "two", "one", "one", "two", "zero"]]


def test_join_strings():
    d0 = dt.Frame([[1, 3, 2, 1, 1, 2, 0], list("cabdabb")], names=("A", "B"))
    d1 = dt.Frame([list("abcd"), range(0, 20, 5)], names=("B", "V"))
    d1.key = "B"
    res = d0[:, :, join(d1)]
    res.internal.check()
    assert res.shape == (7, 3)
    assert res.names == ("A", "B", "V")
    assert res.topython() == [
        [1, 3, 2, 1, 1, 2, 0],
        ["c", "a", "b", "d", "a", "b", "b"],
        [10, 0, 5, 15, 0, 5, 5]]


def test_join_missing_levels():
    d0 = dt.Frame(A=[1, 2, 3])
    d1 = dt.Frame(A=[1, 2], K=[True, False])
    d1.key = "A"
    res = d0[:, :, join(d1)]
    res.internal.check()
    assert res.topython() == [[1, 2, 3], [True, False, None]]


def test_join_errors():
    d0 = dt.Frame(A=[1, 2, 3])
    d1 = dt.Frame(B=range(10), stype=dt.float64)
    with pytest.raises(ValueError) as e:
        d0[:, :, join(d1)]
    assert "The join frame is not keyed" in str(e.value)
    d1.key = "B"
    with pytest.raises(ValueError) as e:
        d0[:, :, join(d1)]
    assert "Key column `B` does not exist in the left Frame" in str(e.value)
    d1.names = ("A",)
    with pytest.raises(TypeError) as e:
        d0[:, :, join(d1)]
    assert ("Join column `A` has type int in the left Frame, and type real "
            "in the right Frame" in str(e.value))


@pytest.mark.parametrize("seed,lt", [(random.getrandbits(32), ltype.bool),
                                     (random.getrandbits(32), ltype.int),
                                     (random.getrandbits(32), ltype.real),
                                     (random.getrandbits(32), ltype.str)])
def test_join_random(seed, lt):
    random.seed(seed)
    ndata = int(random.expovariate(0.0005))
    nkeys = int(random.expovariate(0.01)) + 1
    st = random.choice(lt.stypes)
    if lt == ltype.bool:
        keys = [True, False]
    elif lt == ltype.int:
        nbits = (6 if st == stype.int8 else
                 12 if st == stype.int16 else
                 24)
        keys = list(set(random.getrandbits(nbits) for _ in range(nkeys)))
    elif lt == ltype.real:
        keys = [random.random() for _ in range(nkeys)]
        if st == stype.float32:
            keys = list(set(dt.Frame(keys, stype=st).topython()[0]))
        else:
            keys = list(set(keys))
    else:
        l = int(random.expovariate(0.05)) + 1
        keys = list(set(random_string(l) for _ in range(nkeys)))
    nkeys = len(keys)

    dkey = dt.Frame(KEY=keys, VAL=range(nkeys), stypes={"KEY": st})
    dkey.key = "KEY"
    keys, vals = dkey.topython()
    main = [random.choice(keys) for i in range(ndata)]
    dmain = dt.Frame(KEY=main, stype=st)
    res = [vals[keys.index(main[i])] for i in range(ndata)]

    djoined = dmain[:, :, join(dkey)]
    djoined.internal.check()
    assert djoined.shape == (ndata, 2)
    assert djoined.names == ("KEY", "VAL")
    assert djoined.topython() == [main, res]



def test_join_update():
    d0 = dt.Frame([[1, 2, 3, 2, 3, 1, 3, 2, 2, 1], range(10)], names=("A", "B"))
    d1 = d0[:, mean(f.B), f.A]
    d1.key = "A"
    d0[:, "AA", join(d1)] = g.V0
    assert d0.names == ("A", "B", "AA")
    a = 4.75
    b = 14.0 / 3
    assert d0.topython() == [[1, 2, 3, 2, 3, 1, 3, 2, 2, 1],
                             [0, 1, 2, 3, 4, 5, 6, 7, 8, 9],
                             [b, a, 4, a, 4, b, 4, a, a, b]]


def test_join_and_select_g_col():
    # Check that selecting a g-column does not confuse it with an f-column.
    # See issue #1352
    F = dt.Frame(a=[0, 2, 3], b=[3, 4, 2])
    G = dt.Frame(b=[2, 4], c=["foo", "bar"])
    G.key = "b"
    R = F[:, g.c, join(G)]
    R.internal.check()
    assert R.shape == (3, 1)
    assert R.stypes == (stype.str32,)
    # assert R.names == ("c",)   # not working yet
    assert R.topython() == [[None, "bar", "foo"]]


def test_join_multi():
    fr1 = dt.Frame(A=[1, 2, 1, 2],
                   B=[3, 3, 4, 4],
                   C=["goo", "blah", "zoe", "rij"])
    fr1.key = ("A", "B")
    fr2 = dt.Frame([[1, 2, 3, 2, 3, 1, 2, 1, 1],
                    [3, 4, 5, 4, 3, 3, 3, 4, 3]], names=("A", "B"))
    res = fr2[:, :, join(fr1)]
    assert res.names == ("A", "B", "C")
    assert res.to_list() == [[1, 2, 3, 2, 3, 1, 2, 1, 1],
                             [3, 4, 5, 4, 3, 3, 3, 4, 3],
                             ["goo", "rij", None, "rij", None,
                              "goo", "blah", "zoe", "goo"]]


@pytest.mark.parametrize("seed", [random.getrandbits(32) for _ in range(10)])
def test_join_multi_random(seed):
    seed = 1806372749
    num_stypes = [dt.int8, dt.int16, dt.int32, dt.int64, dt.float32, dt.float64]
    str_stypes = [dt.str32, dt.str64]
    src0 = [False, True, None]
    src1 = list(range(10)) + [None]
    src2 = list(range(-50, 50)) + [None]
    src3 = list("abcdefg?") + [None]
    all_srcs = [src0, src1, src2, src3]
    all_stypes = [num_stypes, num_stypes, num_stypes, str_stypes]
    random.seed(seed)
    nkeys = random.randint(2, 4)
    isrcs = [random.randint(0, 3) for _ in range(nkeys)]
    jcolsources = [all_srcs[t] for t in isrcs]
    jstypes = [random.choice(all_stypes[t]) for t in isrcs]
    jnrows = int(random.expovariate(0.001) + 10)
    jsrcrows = [tuple(random.choice(s) for s in jcolsources)
                for i in range(jnrows)]
    jsrcrows = list(set(jsrcrows))
    jnrows = len(jsrcrows)
    jvalcol = [random.randint(0, 1000001) for i in range(jnrows)]
    jframe = dt.Frame(jsrcrows, names=list("ABCD")[:nkeys], stypes=jstypes)
    jframe.cbind(dt.Frame(V=jvalcol, stype=dt.int32))
    jframe.key = jframe.names[:nkeys]

    xnrows = int(random.expovariate(0.001) + 5)
    xnrows = 10
    xstypes = [random.choice(all_stypes[t]) for t in isrcs]
    xsrcrows = [tuple(random.choice(s) for s in jcolsources)
                for i in range(xnrows)]
    xframe = dt.Frame(xsrcrows, stypes=xstypes, names=list("ABCD")[:nkeys])

    jdict = {jsrcrows[i]: jvalcol[i] for i in range(jnrows)}
    rescol = [jdict.get(xsrcrows[i], None) for i in range(xnrows)]

    print("\n  Joining xframe %r" % (xframe.stypes,))
    print("  to jframe %r" % (jframe.stypes,))
    print("    X = %r" % list(zip(*xframe.to_list())))
    print("    J = %r" % list(zip(*jframe.to_list())))
    joinframe = xframe[:, :, join(jframe)]
    assert joinframe[:, "V"].to_list()[0] == rescol
