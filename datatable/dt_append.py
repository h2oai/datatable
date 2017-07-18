#!/usr/bin/env python3
# Copyright 2017 H2O.ai; Apache License Version 2.0;  -*- encoding: utf-8 -*-
import datatable
from datatable.utils.misc import plural_form as plural
from datatable.utils.typechecks import typed, DataTable_t



@typed(dts=DataTable_t, force=bool, bynames=bool, inplace=bool)
def rbind(self, *dts, force=False, bynames=True, inplace=True):
    """
    Append rows of datatables `dts` to the current datatable.

    This is equivalent to `list.extend()` in Python: the datatables are combined
    by rows, i.e. rbinding a datatable of shape [n x k] to a datatable of shape
    [m x k] produces a datatable of shape [(m + n) x k].

    By default, the source datatable is modified in-place. If you do not want
    the original datatable modified, use option `inplace=False`.

    If datatable(s) being appended have columns of different types than the
    source datatable, then such column will be promoted to the "largest" of two
    types: bool -> int -> decimal -> float -> string -> categorical.

    If you need to append multiple datatables, then it is more efficient to
    collect them into an array first and then do a single `rbind()`, than it is
    to append them one-by-one.

    Appending data to a datatable opened from disk will force loading the
    source datatable into memory, which may fail with an OutOfMemory exception.

    Parameters
    ----------
    dts: sequence or list of DataTables
        One or more datatable to append. These datatables should have the same
        columnar structure as the current datatable (unless option `force` is
        used).

    force: boolean, default False
        If True, then the datatables are allowed to have mismatching set of
        columns. Any gaps in the data will be filled with NAs.

    bynames: boolean, default True
        If True, the columns in datatables are matched by their names. For
        example, if one datatable has columns ["colA", "colB", "colC"] and the
        other ["colB", "colA", "colC"] then we will swap the order of the first
        two columns of the appended datatable before performing the append.
        However if `bynames` is False, then the column names will be ignored,
        and the columns will be matched according to their order, i.e. i-th
        column in the current datatable to the i-th column in each appended
        datatable.

    inplace: boolean, default True
        If True, then the data is appended to the current datatable modifying
        it in-place. If False, then the current datatable remains unchanged
        and a copy datatable is created and returned instead.
    """
    n = self.ncols

    # `spec` will be the description of how the DataTables are to be merged:
    # it is a list of tuples (_datatable.DataTable, [int]), where the first
    # element of the tuple is a datatable being appended, and the second element
    # is the array of column indices within that datatable. For example, if the
    # array is [1, 0, None, 2, None] then it means that we need to take the
    # datatable being appended, reorder its columns as (2nd column, 1st column,
    # column of NAs, 3rd column, column of NAs) and only then "stitch" to the
    # resulting datatable of 5 columns.
    spec = []
    final_names = list(self.names)

    # Which DataTable to operate upon. If not `inplace` then we will create
    # a blank DataTable and append everything to it.
    src = self
    if not inplace:
        src = datatable.DataTable()
        spec.append((self.internal, list(range(n))))

    # Append by column names, filling with NAs as necessary
    if bynames:
        # Create mapping of column_name => column_index (or list of indices if
        # there are multiple columns with the same name). Do not reuse
        # `self._inames` here since we will be modifying this dictionary.
        inames = dict()
        for i, col in enumerate(self.names):
            if col in inames:
                if isinstance(inames[col], list):
                    inames[col].append(i)
                else:
                    inames[col] = [inames[col], i]
            else:
                inames[col] = i
        for dt in dts:
            if not(dt.ncols == n or force):
                raise ValueError(
                    "Cannot rbind datatable with %s to a datatable with %s. If"
                    " you wish rbind the datatables anyways filling missing "
                    "values with NAs, then use option `force=True`"
                    % (plural(dt.ncols, "column"), plural(n, "column")))
            # Column mapping that specifies which column of `dt` should be
            # appended where in the result.
            res = [None] * len(final_names)
            # For each column name, how many columns with this name already
            # matched for the datatable `dt`.
            used_inames = dict()
            for i, col in enumerate(dt.names):
                icol = inames.get(col)
                if icol is None:
                    # Column with this name not present in the spec yet
                    if not force:
                        raise ValueError(
                            "Column '%s' is not found in the source datatable. "
                            "If you want to rbind the datatables anyways "
                            "filling missing values with NAs, then specify "
                            "option `force=True`" % col)
                    final_names.append(col)
                    inames[col] = len(final_names) - 1
                    used_inames[col] = 1
                    res.append(i)
                elif isinstance(icol, int):
                    # Only 1 column with such name in the spec
                    if col in used_inames:
                        final_names.append(col)
                        used_inames[col] += 1
                        inames[col] = [icol, len(final_names) - 1]
                        res.append(i)
                    else:
                        used_inames[col] = 1
                        res[icol] = i
                else:
                    # Multiple columns with such name in the spec
                    u = used_inames.get(col, 0)
                    if u >= len(icol):
                        final_names.append(col)
                        used_inames[col] += 1
                        inames[col].append(len(final_names) - 1)
                        res.append(i)
                    else:
                        used_inames[col] = u + 1
                        res[icol[u]] = i
            spec.append((dt.internal, res))

    # Append by column numbers
    else:
        for dt in dts:
            nn = dt.ncols
            if nn != n and not force:
                raise ValueError(
                    "Cannot rbind datatable with %s to a datatable with %s. If"
                    " you wish rbind the datatables anyways filling missing "
                    "values with NAs, then use option `force=True`"
                    % (plural(nn, "column"), plural(n, "column")))
            if nn > len(final_names):
                final_names += list(dt.names[len(final_names):])
            spec.append((dt.internal, list(range(nn))))

    # Perform the append operation on C level
    src.internal.rbind(len(final_names), spec)
    src._fill_from_dt(src.internal, names=final_names)
    return src



@typed(dts=DataTable_t, force=bool, inplace=bool)
def cbind(self, *dts, force=False, inplace=True):
    """
    Append columns of datatables `dts` to the current datatable.

    This is equivalent to `pandas.concat(axis=1)`: the datatables are combined
    by columns, i.e. cbinding a datatable of shape [n x m] to a datatable of
    shape [n x k] produces a datatable of shape [n x (m + k)].

    As a special case, if you cbind a single-row datatable, then that row will
    be replicated as many times as there are rows in the current datatable. This
    makes it easy to create constant columns, or to append reduction results
    (such as min/max/mean/etc) to the current datatable.

    If datatable(s) being appended have different number of rows (with the
    exception of datatables having 1 row), then the operation will fail by
    default. You can force cbinding these datatables anyways by providing option
    `force=True`: this will fill all "short" datatables with NAs. Thus there is
    a difference in how datatables with 1 row are treated compared to datatables
    with any other number of rows.

    When appending several datatables with the similar column names, the
    resulting datatable will have those names unmodified. Thus, it is entirely
    possible to create a datatable which has more than one column with the same
    name.

    Parameters
    ----------
    dts: sequence or list of DataTables
        One or more datatable to append. They should have the same number of
        rows (unless option `force` is also provided).

    force: boolean, default False
        If True, allows datatables to be appended even if they have unequal
        number of rows. The resulting datatable will have number of rows equal
        to the largest among all datatables. Those datatables which have less
        than the largest number of rows, will be padded with NAs (with the
        exception of datatables having just 1 row, which will be replicated
        instead of filling with NAs).

    inplace: boolean, default True
        If True, then the data is appended to the current datatable in-place,
        causing it to be modified. If False, then a new datatable will be
        constructed and returned instead (and no existing datatables will be
        modified).

    Returns
    -------
    The current datatable, modified, if `inplace` is True; or a new datatable
    containing all datatables concatenated, if `inplace` is False.
    """
    datatables = []
    column_names = list(self.names)

    # Which DataTable to operate upon. If not `inplace` then we will create
    # a blank DataTable and merge everything to it.
    src = self
    if not inplace:
        src = datatable.DataTable()
        datatables.append(self.internal)

    # Check that all datatables have compatible number of rows, and compose the
    # list of _DataTables to be passed down into the C level.
    nrows = -1
    for dt in dts:
        nn = dt.nrows
        if nrows == -1:
            nrows = nn
        if not(nn == nrows or nn == 1 or force):
            if nrows <= 1:
                nrows = nn
            else:
                raise ValueError(
                    "Cannot merge datatable with %s to a datatable with %s. If "
                    "you want to disregard this warning and merge datatables "
                    "anyways, then please provide option `inplace=True`"
                    % (plural(nn, "row"), plural(nrows, "row")))
        datatables.append(dt.internal)
        column_names.extend(list(dt.names))

    src.internal.cbind(datatables)
    src._fill_from_dt(src.internal, names=column_names)
    return src
