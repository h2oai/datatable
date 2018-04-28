#!/usr/bin/env python3
# © H2O.ai 2018; -*- encoding: utf-8 -*-
#   This Source Code Form is subject to the terms of the Mozilla Public
#   License, v. 2.0. If a copy of the MPL was not distributed with this
#   file, You can obtain one at http://mozilla.org/MPL/2.0/.
#-------------------------------------------------------------------------------
import datatable as dt
from datatable.utils.misc import plural_form as plural
from datatable.utils.typechecks import typed, Frame_t, TValueError



@typed(frames=Frame_t, force=bool, bynames=bool)
def rbind(self, *frames, force=False, bynames=True):
    """
    Append rows of `frames` to the current Frame.

    This is equivalent to `list.extend()` in Python: the Frames are combined
    by rows, i.e. rbinding a Frame of shape [n x k] to a Frame of shape
    [m x k] produces a Frame of shape [(m + n) x k].

    This method modifies the current Frame in-place. If you do not want
    the current Frame modified, then append all Frames to an empty Frame:
    `dt.Frame().rbind(frame1, frame2)`.

    If Frame(s) being appended have columns of types different from the
    current Frame, then these columns will be promoted to the largest of two
    types: bool -> int -> float -> string.

    If you need to append multiple Frames, then it is more efficient to
    collect them into an array first and then do a single `rbind()`, than it is
    to append them one-by-one.

    Appending data to a Frame opened from disk will force loading the
    current Frame into memory, which may fail with an OutOfMemory exception.

    Parameters
    ----------
    frames: sequence or list of Frames
        One or more Frame to append. These Frames should have the same
        columnar structure as the current Frame (unless option `force` is
        used).

    force: boolean, default False
        If True, then the Frames are allowed to have mismatching set of
        columns. Any gaps in the data will be filled with NAs.

    bynames: boolean, default True
        If True, the columns in Frames are matched by their names. For
        example, if one Frame has columns ["colA", "colB", "colC"] and the
        other ["colB", "colA", "colC"] then we will swap the order of the first
        two columns of the appended Frame before performing the append.
        However if `bynames` is False, then the column names will be ignored,
        and the columns will be matched according to their order, i.e. i-th
        column in the current Frame to the i-th column in each appended
        Frame.
    """
    n = self.ncols

    # `spec` will be the description of how the DataTables are to be merged:
    # it is a list of tuples (core.DataTable, Optional[List[int]]), where the
    # first item in the tuple is a Frame being appended, and the second item
    # is the array of column indices within that Frame. For example, if the
    # array is [1, 0, None, 2, None] then it means that we need to take the
    # Frame being appended, reorder its columns as (2nd column, 1st column,
    # column of NAs, 3rd column, column of NAs) and only then "stitch" to the
    # resulting Frame of 5 columns.
    spec = []
    final_names = list(self.names)

    # Append by column names, filling with NAs as necessary
    if bynames:
        # `inames` is a mapping of column_name => column_index.
        inames = self._inames
        for df in frames:
            _dt = df.internal
            if df.nrows == 0: continue
            if n == 0:
                n = df.ncols
                final_names = list(df.names)
                inames = df._inames.copy()
            elif not (df.ncols == n or force):
                raise TValueError(
                    "Cannot rbind frame with %s to a frame with %s. If"
                    " you wish to rbind the frames anyways, filling missing "
                    "values with NAs, then use `force=True`"
                    % (plural(df.ncols, "column"), plural(n, "column")))
            if final_names == list(df.names):
                spec.append((_dt, None))
                continue
            # Column mapping that specifies which column of `df` should be
            # appended where in the result.
            res = [None] * len(final_names)
            for i, col in enumerate(df.names):
                icol = inames.get(col)
                if icol is not None:
                    res[icol] = i
                elif force:
                    final_names.append(col)
                    inames[col] = len(final_names) - 1
                    res.append(i)
                    n += 1
                else:
                    raise TValueError(
                        "Column `%s` is not found in the source frame. "
                        "If you want to rbind the frames anyways filling "
                        "missing values with NAs, then use `force=True`"
                        % col)
            spec.append((_dt, res))

    # Append by column numbers
    else:
        for df in frames:
            _dt = df.internal
            if df.nrows == 0: continue
            if n == 0:
                n = df.ncols
                final_names = list(df.names)
            if df.ncols != n:
                if not force:
                    raise TValueError(
                        "Cannot rbind frame with %s to a frame with %s. If you "
                        "wish to rbind the Frames anyways filling missing "
                        "values with NAs, then use option `force=True`"
                        % (plural(df.ncols, "column"), plural(n, "column")))
                elif df.ncols > n:
                    final_names += list(df.names[n:])
                    n = df.ncols
            spec.append((_dt, None))

    # Perform the append operation on C level
    _dt = self.internal
    _dt.rbind(len(final_names), spec)
    self._fill_from_dt(_dt, names=final_names)
    return self



@typed(frames=Frame_t, force=bool, inplace=bool)
def cbind(self, *frames, force=False, inplace=True):
    """
    Append columns of Frames `frames` to the current Frame.

    This is equivalent to `pandas.concat(axis=1)`: the Frames are combined
    by columns, i.e. cbinding a Frame of shape [n x m] to a Frame of
    shape [n x k] produces a Frame of shape [n x (m + k)].

    As a special case, if you cbind a single-row Frame, then that row will
    be replicated as many times as there are rows in the current Frame. This
    makes it easy to create constant columns, or to append reduction results
    (such as min/max/mean/etc) to the current Frame.

    If Frame(s) being appended have different number of rows (with the
    exception of Frames having 1 row), then the operation will fail by
    default. You can force cbinding these Frames anyways by providing option
    `force=True`: this will fill all "short" Frames with NAs. Thus there is
    a difference in how Frames with 1 row are treated compared to Frames
    with any other number of rows.

    Parameters
    ----------
    frames: sequence or list of Frames
        One or more Frame to append. They should have the same number of
        rows (unless option `force` is also used).

    force: boolean, default False
        If True, allows Frames to be appended even if they have unequal
        number of rows. The resulting Frame will have number of rows equal
        to the largest among all Frames. Those Frames which have less
        than the largest number of rows, will be padded with NAs (with the
        exception of Frames having just 1 row, which will be replicated
        instead of filling with NAs).

    inplace: boolean, default True [DEPRECATED]
        If True, then the data is appended to the current Frame in-place,
        causing it to be modified. If False, then a new Frame will be
        constructed and returned instead (and no existing Frames will be
        modified).

    Returns
    -------
    The current Frame, modified, if `inplace` is True; or a new Frame
    containing all Frames concatenated, if `inplace` is False.
    """
    datatables = []
    column_names = list(self.names)

    # Which Frame to operate upon. If not `inplace` then we will create
    # a blank Frame and merge everything to it.
    src = self
    if not inplace:
        src = dt.Frame()
        datatables.append(self.internal)

    # Check that all Frames have compatible number of rows, and compose the
    # list of _DataTables to be passed down into the C level.
    nrows = src.nrows or -1
    for df in frames:
        if df.ncols == 0: continue
        nn = df.nrows
        if nrows == -1:
            nrows = nn
        if not(nn == nrows or nn == 1 or force):
            if nrows <= 1:
                nrows = nn
            else:
                raise TValueError(
                    "Cannot merge Frame with %s to a Frame with %s. If "
                    "you want to disregard this warning and merge Frames "
                    "anyways, then use option `force=True`"
                    % (plural(nn, "row"), plural(nrows, "row")))
        datatables.append(df.internal)
        column_names.extend(list(df.names))

    src.internal.cbind(datatables)
    src._fill_from_dt(src.internal, names=column_names)
    return src
