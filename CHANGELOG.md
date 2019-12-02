<!---
  Copyright 2018 H2O.ai

  Permission is hereby granted, free of charge, to any person obtaining a
  copy of this software and associated documentation files (the "Software"),
  to deal in the Software without restriction, including without limitation
  the rights to use, copy, modify, merge, publish, distribute, sublicense,
  and/or sell copies of the Software, and to permit persons to whom the
  Software is furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in
  all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
  IN THE SOFTWARE.
-->

# Changelog
The format is based on [Keep a Changelog](http://keepachangelog.com/)
and this project adheres to [Semantic Versioning](http://semver.org/).


This is an archive of Changelog up to and including version 0.9.0. For
more recent changes, visit the
[ReadTheDocs](https://datatable.readthedocs.io/en/latest/changelog.html)
website.


## [0.9.0][] — 2019-06-15

### Added

- Added function `dt.models.kfold(nrows, nsplits)` to prepare indices for
  k-fold splitting. This function will return `nsplits` pairs of row selectors
  such that when these selectors are applied to an `nrows`-rows frame, that
  frame will be split into train and test part according to the K-fold
  splitting scheme.

- Added function `dt.models.kfold_random(nrows, nsplits, seed)`, which is
  similar to `kfold(nrows, nsplits)`, except that the assignment of rows into
  folds is randomized, not deterministic.

- `Frame.rbind()` can now also accept a list or tuple of frames (previously
  only a vararg sequence was allowed).

- Method `.len()` can be applied to a string column to obtain the lengths
  of strings in each row.

- Method `.re_match(re)` applies to a string column, and produces boolean
  indicator whether each value matches the regular expression `re` or not.
  The method matches the entire string, not just the beginning. Thus, it
  most closely resembles Python function `re.fullmatch()`.

- Added early stopping support to FTRL algo, that can now do binomial and
  multinomial classification for categorical targets, as well as regression
  for continuous targets.

- New function `dt.median()` can be used to compute median of a certain
  column or expression, either per group or for the entire Frame (#1530).

- `Frame.__str__()` now returns a string containing the preview of the
  frame's data. This allows datatable frames to be used with `print()`.

- Added method `dt.options.describe()`, which will print the available
  options together with their values and descriptions.

- Added `dt.options.context(option=value)`, which can be used in a with-
  statement to temporarily change the value of one or more options, and
  then go back to their original values at the end of the with-block.

- Added options `fread.log.escape_unicode` (controls treatment of unicode
  characters in fread's verbose log); and `display.use_colors` (allows
  to turn on/off colored output in the console).

- `dt.options` now helps the user when they make a typo: if an option
  with a certain name does not exist, the error message will suggest the
  correct spelling.

- most long-running operations in `datatable` will now show a progress bar.
  Its behavior can be controlled via `dt.options.progress` set of options.

- internal function `dt.internal.compiler_version()`.

- New `datatable.math` module is a library of various mathematical functions
  that can be applied to datatable Frames. The set of functions is close to
  what is available in the standard python `math` module. See documentation
  for more details.

- New module `datatable.sphinxext.dtframe_directive`, which can be used as
  a plugin for Sphinx. This module adds directive `.. dtframe` that allows
  to easily include a Frame display in an .rst document.

- Frame can now be treated as an iterable over the columns. Thus, a Frame
  object can now be used in a for-loop, producing its individual columns.

- A Frame can now be treated as a mapping; in particular both `dict(frame)`
  and `**frame` are now valid.

- Single-column frames can be be used as sources for Frame construction.

- CSV writer now quotes fields containing single-quote mark (`'`).

- Added parameter `quoting=` to method `Frame.to_csv()`. The accepted values
  are 4 constants from the standard `csv` module: `csv.QUOTE_MINIMAL`
  (default), `csv.QUOTE_ALL`, `csv.QUOTE_NONNUMERIC` and `csv.QUOTE_NONE`.

- Added parameter `compression=` to method `Frame.to_csv()`, with possibility
  to request gzip compression for the output file. By default the compression
  method will be inferred from the file name.


### Fixed

- Fixed crash in certain circumstances when a key was applied after a
  groupby (#1639).

- `Frame.to_numpy()` now returns a numpy `masked_array` if the frame has
  any NA values (#1619).

- A keyed frame will now be rendered correctly when viewing it in python
  console via `Frame.view()` (#1672).

- Str32 column can no longer overflow during the `.replace()` operation,
  or when converting from python, numpy or pandas, etc. In all these cases
  we will now transparently create a Str64 column instead (#1694).

- The reported frame size (`sys.getsizeof(DT)`) is now more accurate; in
  particular the content of string columns is no longer ignored (#1697).

- Type casting into str32 no longer produces an error if the resulting column
  is larger than 2GB. Now a str64 column will be returned instead (#1695).

- Fixed memory leak during computation of a generic `DT[i, j]` expression.
  Another memory leak was during generation of string columns, now also fixed
  (#1705).

- Fixed crash upon exiting from a python terminal, if the user ever called
  function `frame_column_rowindex().type` (#1703).

- Pandas "boolean column with NAs" (of dtype `object`) now converts into
  datatable `bool8` column when pandas DataFrame is converted into a datatable
  Frame (#1730).

- Fixed conversion to numpy of a view Frame which contains NAs (#1738).

- `datatable` can now be safely used with `multiprocessing`, or other modules
  that perform fork-without-exec (#1758). The child process will spawn its
  own thread pool that will have the same number of threads as the parent.
  Adjust `dt.options.nthreads` in the child process(es) if different number
  of threads is required.

- The interactive mode is no longer improperly turned on in IPython (#1789).

- Fixed issue with mis-aligned frame headers in IPython, caused by IPython
  inserting `Out[X]:` in front of the rendered Frame display (#1793).

- Improved rendering of Frames in terminals with white background: we no longer
  use 'bright_white' color for emphasis, only 'bold' (#1793).

- Fixed crash when a new column was created via partial assignment, i.e.
  `DT[i, "new_col"] = expr` (#1800).

- Fixed memory leaks/crashes when materializing an object column (#1805).

- Fixed creating a Frame from a pandas DataFrame that has duplicate column
  names (#1816).

- Fixed a UnicodeDecodeError that could be thrown when viewing a Frame with
  unicode characters in Jupyter notebook. The error only manifested for
  strings that were longer than 50 bytes in length (#1825).

- Fixed crash when `Frame.colindex()` was used without any arguments, now this
  raises an exception instead (#1834).

- Fixed possible crash when writing to disk that doesn't have enough free space
  on it (#1837).

- Fixed invalid Frame being created when reading a large string column (str64)
  with fread, and the column contains NA values.

- Fixed FTRL model not resuming properly after unpickling (#1846).

- Fixed crash that occurred when sorting by multiple columns, and the first
  column is of low cardinality (#1857).

- Fixed display of NA values produced during a join, when a Frame was displayed
  in Jupyter Lab (#1872).

- Fixed a crash when replacing values in a str64 column (#1890).

- `cbind()` no longer throws an error when passed a generator producing
  temporary frames (#1905).

- Fixed comparison of string columns vs. value `None` (#1912).

- Fixed a crash when trying to select individual cells from a joined Frame,
  for the cells that were un-matched during the join (#1917).

- Fixed a crash when writing a joined frame into CSV (#1919).

- Fixed a crash when writing into CSV string view columns, especially of
  str64 type (#1921).


### Changed

- A Frame will no longer be shown in "interactive" mode in console by default.
  The previous behavior can be restored with
  `dt.options.display.interactive = True`. Alternatively, you can explore a
  Frame interactively using `frame.view(True)`.

- Improved performance of type-casting a view column: now the code avoids
  materializing the column before performing the cast.

- `Frame` class is now defined fully in C++, improving code robustness and
  performance. The property `Frame.internal` was removed, as it no longer
  represents anything. Certain internal properties of `Frame` can be accessed
  via functions declared in the `dt.internal.` module.

- `datatable` no longer uses OpenMP for parallelism. Instead, we use our own
  thread pool to perform multi-threaded computations (#1736).

- Parameter `progress_fn` in function `dt.models.aggregate()` is removed.
  In its place you can set the global option `dt.options.progress.callback`.

- Removed deprecated Frame methods `.topython()`, `.topandas()`, `.tonumpy()`,
  and `Frame.__call__()`.

- Syntax `DT[col]` has been restored (was previously deprecated in 0.7.0),
  however it works only for `col` an integer or a string. Support for slices
  may be added in the future, or not: there is a potential to confuse
  `DT[a:b]` for a row selection. A column slice may still be selected via
  the i-j selector `DT[:, a:b]`.

- The `nthreads=` parameter in `Frame.to_csv()` was removed. If needed, please
  set the global option `dt.options.nthreads`.


### Deprecated

- Frame method `.scalar()` is now deprecated and will be removed in release
  0.10.0. Please use `frame[0, 0]` instead.

- Frame method `.append()` is now deprecated and will be removed in release
  0.10.0. Please use `.rbind()` instead.

- Frame method `.save()` was renamed into `.to_jay()` (for consistency with
  other `.to_*()` methods). The old name is still usable, but marked as
  deprecated and will be removed in 0.10.0.


### Notes

- Thanks to everyone who helped make `datatable` more stable by discovering
  and reporting bugs that were fixed in this release:

  - [Arno Candel][] (#1619, #1730, #1738, #1800, #1803, #1846, #1857, #1890,
    #1891, #1919, #1921),

  - [Antorsae][] (#1639),

  - [Olivier][] (#1872),

  - [Hawk Berry][] (#1834),

  - [Jonathan McKinney][] (#1816, #1837),

  - [Mateusz Dymczyk][] (#1912),

  - [NachiGithub][] (#1789, #1793),

  - [Pasha Stetsenko][] (#1672, #1694, #1695, #1697, #1703, #1705, #1905,
    #1917),

  - [Tom Kraljevic][] (#1805),

  - [XiaomoWu][] (#1825)



## [0.8.0][] — 2019-02-04

### Added

- Method `frame.to_tuples()` converts a Frame into a list of tuples, each
  tuple representing a single row (#1439).

- Method `frame.to_dict()` converts the Frame into a dictionary where the
  keys are column names and values are lists of elements in each column
  (#1439).

- Methods `frame.head(n)` and `frame.tail(n)` added, returning the first/last
  `n` rows of the Frame respectively (#1307).

- Frame objects can now be pickled using the standard Python `pickle`
  interface (#1444). This also has an added benefit of reducing the potential
  for a deadlock when using the `multiprocessing` module.

- Added function `repeat(frame, n)` that creates a new Frame by row-binding
  `n` copies of the `frame` (#1459).

- Module `datatable` now exposes C API, to allow other C/C++ libraries interact
  with datatable Frames natively (#1469). See "datatable/include/datatable.h"
  for the description of the API functions. Thanks [Qiang Kou][] for testing
  this functionality.

- The column selector `j` in `DT[i, j]` can now be a list/iterator of booleans.
  This list should have length `DT.ncols`, and the entries in this list will
  indicate whether to select the corresponding column of the Frame or not
  (#1503). This can be used to implement a simple column filter, for example:
    ```python
    del DT[:, (name.endswith("_tmp") for name in DT.names)]
    ```

- Added ability to train and fit an FTRL-Proximal (Follow The Regularized
  Leader) online learning algorithm on a data frame (#1389). The implementation
  is multi-threaded and has high performance.

- Added functions `log` and `log10` for computing the natural and base-10
  logarithms of a column (#1558).

- Sorting functionality is now integrated into the `DT[i, j, ...]` call via
  the function `sort()`. If sorting is specified alongside a groupby, the
  values will be sorted within each group (#1531).

- A slice-valued `i` expression can now be combined with a `by()` operator
  in `DT[i, j, by()]`. The result is that the slice `i` is applied to each
  group produced by `by()`, before the `j` is evaluated (#1585).

- Implemented sorting in reverse direction, via `sort(-col)`, where `col` is
  any regular column selector such as `f.A` or `f[column]`. The `-` sign is
  symbolic, no actual negation occurs. As such, this works even for string
  columns (#792).


### Fixed

- Fixed rendering of "view" Frames in a Jupyter notebook (#1448). This bug
  caused the frame to display wrong data when viewed in a notebook.

- Fixed crash when an int-column `i` selector is applied to a Frame which
  already had another row filter applied (#1437).

- `Frame.copy()` now retains the frame's key, if any (#1443).

- Installation from source distribution now works as expected (#1451).

- When a `g.`-column is used but there is no join frame, an appropriate
  error message is now emitted (#1481).

- The equality operators `==` / `!=` can now be applied to string columns too
  (#1491).

- Function `dt.split_into_nhot()` now works correctly with view Frames (#1507).

- `DT.replace()` now works correctly when the replacement list is `[+inf]` or
  `[1.7976931348623157e+308]` (#1510).

- FTRL algorithm now works correctly with view frames (#1502).

- Partial column update (i.e. expression of the form `DT[i, j] = R`) now works
  for string columns as well (#1523).

- `DT.replace()` now throws an error if called with 0 or 1 argument (#1525).

- Fixed crash when viewing a frame obtained by resizing a 0-row frame (#1527).

- Function `count()` now returns correct result within the `DT[i, j]` expression
  with non-trivial `i` (#1316).

- Fixed groupby when it is applied to a Frame with view columns (#1542).

- When replacing an empty set of columns, the replacement frame can now be
  also empty (i.e. have shape `[0 x 0]`) (#1544).

- Fixed join results when join is applied to a view frame (#1540).

- Fixed `Frame.replace()` in view string columns (#1549).

- A 0-row integer column can now be used as `i` in `DT[i, j]` (#1551).

- A string column produced from a partial join now materializes correctly
  (#1556).

- Fixed incorrect result during "true division" of integer columns, when one
  of the values was negative and the other positive (#1562).

- `Frame.to_csv()` no longer crashes on Unix when writing an empty frame
  (#1565).

- The build process on MacOS now ensures that the `libomp.dylib` is properly
  referenced via `@rpath`. This prevents installation problems caused by the
  dynamic dependencies referenced by their absolute paths which are not valid
  outside of the build machine (#1559).

- Fixed crash when the RHS of assignment `DT[i, j] = ...` was a list of
  expressions (#1539).

- Fixed crash when an empty `by()` condition was used in `DT[i, j, by]` (#1572).

- Expression `DT[:, :, by(...)]` no longer produces duplicates of columns used
  in the by-clause (#1576).

- In certain circumstances mixing computed and plain columns under groupby
  caused incorrect result (#1578).

- Fixed an internal error which was occurring when multiple row filters were
  applied to a Frame in sequence (#1592).

- Fixed rbinding of frames if one of them is a negative step slice (#1594).

- Fixed a crash that occurred with the latest `pandas` 0.24.0 (#1600).

- Fixed invalid result when cbinding several 0-row frames (#1604).


### Changed

- The primary datatable expression `DT[i, j, ...]` is now evaluated entirely
  in C++, improving performance and reliability.

- Setting `frame.nrows` now always pads the Frame with NAs, even if the Frame
  has only 1 row. Previously changing `.nrows` on a 1-row Frame caused its
  value to be repeated. Use `frame.repeat()` in order to expand the Frame
  by copying its values.

- Improved the performance of setting `frame.nrows`. Now if the frame has
  multiple columns, a view will be created.

- When no columns are selected in `DT[i, j]`, the returned frame will now
  have the same number of rows as if at least 1 column was selected. Previously
  an empty `[0 x 0]` frame was returned.

- Assigning a value to a column `DT[:, 'A'] = x` will attempt to preserve the
  column's stype; or if not possible, the column will be upcasted within its
  logical type.

- It is no longer possible to assign a value of an incompatible logical type to
  an existing column. For example, an assignment `DT[:, 'A'] = 3` is now legal
  only if column A is of integer or real type, but will raise an exception if A
  is a boolean or string.

- `Frame.rbind()` method no longer has a return value. The method always updated
  the frame in-place, so it was confusing to both update in-place and return the
  original frame (#1610).

- `min()` / `max()` over an empty or all-NA column now returns `None` instead of
  +Inf / -Inf respectively (#1624).


### Deprecated

- Frame methods `.topython()`, `.topandas()` and `.tonumpy()` are now
  deprecated, they will be removed in 0.9.0. Please use `.to_list()`,
  `.to_pandas()` and `.to_numpy()` instead.

- Calling a frame object `DT(rows=i, select=j, groupby=g, join=z, sort=s)` is
  now deprecated. Use the expression `DT[i, j, by(g), join(z), sort(s)]`
  instead, where symbols `by()`, `join()` and `sort()` can all be imported
  from the `datatable` namespace (#1579).


### Removed

- Single-item Frame selectors are now prohibited: `DT[col]` is an error. In
  the future this expression will be interpreted as a row selector instead.


### Notes

- `datatable` now uses integration with
  [Codacy](https://app.codacy.com/project/st-pasha/datatable/dashboard)
  to keep track of code quality and potential errors.

- Internally, we now allow each Column in a Frame to have its own separate
  RowIndex. This will improve the performance, especially in join/cbind
  operations. Applications that use the `datatable`'s C API may need to be
  updated to account for this (#1188).

- This release was prepared by:

  - [Pasha Stetsenko][] - core functionality improvements, bug fixes,
    refactoring;

  - [Oleksiy Kononenko][] - FTRL algo implementation, fixes in the Aggregator;

  - [Michael Frasco][] - documentation fixes;

  - [Michal Raška][] - build system maintenance.

- Additional thanks to people who helped make `datatable` more stable by
  discovering and reporting bugs that were fixed in this release:

  [Pasha Stetsenko][] (#1316, #1443, #1481, #1539, #1542, #1551, #1572, #1576,
  #1578, #1592, #1594, #1602, #1604),
  [Arno Candel][] (#1437, #1491, #1510, #1525, #1549, #1556, #1562),
  [Michael Frasco][] (#1448),
  [Jonathan McKinney][] (#1451, #1565),
  [CarlosThinkBig][] (#1475),
  [Olivier][] (#1502),
  [Oleksiy Kononenko][] (#1507, #1600),
  [Nishant Kalonia][] (#1527, #1540),
  [Megan Kurka][] (#1544),
  [Joseph Granados][] (#1559).



## [0.7.0][] — 2018-11-16

### Added

- Frame can now be created from a list/dict of numpy arrays.

- Filters can now be used together with groupby expressions.

- fread's verbose output now includes time spent opening the input file.

- Added ability to read/write Jay files.

- Frames can now be constructed via the keyword-args list of columns
  (i.e. `Frame(A=..., B=...)`).

- Implemented logical operators "and" `&` and "or" `|` for eager evaluator.

- Implemented integer division `//` and modulo `%` operators.

- A Frame can now have a key column (or columns).

- Key column(s) are saved when the frame is saved into a Jay file.

- A Frame can now be naturally-joined with a keyed Frame.

- Columns can now be updated within join expressions.

- The error message when selecting a column that does not exist in the Frame
  now refers to similarly-named columns in that Frame, if there are any. At
  most 3 possible columns are reported, and they are ordered from most likely
  to least likely (#1253).

- Frame() constructor now accepts a list of tuples, which it treats as rows
  when creating the frame.

- Frame() can now be constructed from a list of named tuples, which will
  be treated as rows and field names will be used as column names.

- frame.copy() can now be used to create a copy of the Frame.

- Frame() can now be constructed from a list of dictionaries, where each
  item in the list represents a single row.

- Frame() can now be created from a datetime64 numpy array (#1274).

- Groupby calculations are now parallel.

- `Frame.cbind()` now accepts a list of frames as the argument.

- Frame can now be sorted by multiple columns.

- new function `split_into_nhot()` to split a string column into fragments
  and then convert them into a set of indicator variables ("n-hot encode").

- ability to convert object columns into strings.

- implemented `Frame.replace()` function.

- function `abs()` to find the absolute value of elements in the frame.

- improved handling of Excel files by fread:
  - sheet name can now be used as a path component in the file name,
    causing only that particular sheet to be parsed;

  - further, a cell range can be specified as a path component after the
    sheet name, forcing fread to consider only the provided cell range;

  - fread can now handle the situation when a spreadsheet has multiple
    separate tables in the same sheet. They will now be detected automatically
    and returned to the user as separate Frame objects (the name of each
    frame will contain the sheet name and cell range from where the data was
    extracted).

- HTML rendering of Frames inside a Jupyter notebook.

- set-theoretic functions: `union`, `intersect`, `setdiff` and `symdiff`.

- support for multi-column keys.

- ability to join Frames on multiple columns.

- In Jupyter notebook columns now have visual indicators of their types.
  The logical types are color-coded, and the size of each element is
  given by the number of dots (#1428).


### Changed

- `names` argument in `Frame()` constructor can no longer be a string --
  use a list or tuple of strings instead.

- `Frame.resize()` removed -- same functionality is available via
  assigning to `Frame.nrows`.

- `Frame.rename()` removed -- .name setter can be used instead.

- `Frame([])` now creates a 0x0 Frame instead of 0x1.

- Parameter `inplace` in `Frame.cbind()` removed (was deprecated).
  Instead of `inplace=False` use `dt.cbind(...)`.

- `Frame.cbind()` no longer returns anything (previously it returned self,
  but this was confusing w.r.t whether it modifies the target, or returns
  a modified copy).

- `DT[i, j]` now returns a python scalar value if `i` is integer, and `j`
  is integer/string. This is referred to as "explicit element selection".
  In the unlikely scenario when a single element needs to be returned as
  a frame, one can always write `DT[i:i+1, j]` or `DT[[i], j]`.

- The performance of explicit element selection improved by a factor of 200x.

- Building no longer requires an LLVM distribution.

- `DT[col]` syntax has been deprecated and now emits a warning. This
  will be converted to an error in version 0.8.0, and will be interpreted
  as row selector in 0.9.0.

- default format for `Frame.save()` is now "jay".


### Fixed

- bug in dt.cbind() where the first Frame in the list was ignored.

- bug with applying a cast expression to a view column.

- occasional memory errors caused by a lack of available mmap handles.

- memory leak in groupby operations.

- `names` parameter in Frame constructor is now checked for correctness.

- bug in fread with QR bump occurring out-of-sample.

- `import datatable` now takes only 0.13s, down from 0.6s.

- fread no longer wastes time reading the full input, if max_nrows option
  is used.

- bug where max_nrows parameter was sometimes causing a seg.fault

- fread performance bug caused by memory-mapped file being accidentally
  copied into RAM.

- rare crash in fread when resizing the number of rows.

- saving view frames to csv.

- crash when sorting string columns containins NA strings.

- crash when applying a filter to a 0-rows frame.

- if `x` is a Frame, then `y = dt.Frame(x)` now creates a shallow copy
  instead of a copy-by-reference.

- upgraded dependency version for typesentry, the previous version was not
  compatible with Python 3.7.

- rare crash when converting a string column from pandas DataFrame, when
  that column contains many non-ASCII characters.

- f-column-selectors should no longer throw errors and produce only unique
  ids when stringified (#1241).

- crash when saving a frame with many boolean columns into CSV (#1278).

- incorrect .stypes/.ltypes property after calling cbind().

- calculation of min/max values in internal rowindex upon row resizing.

- frame.sort() with no arguments no longer produces an error.

- f-expressions now do not crash when reused with a different Frame.

- g-columns can be properly selected in a join (#1352).

- writing to disk of columns > 2GB in size (#1387).

- crash when sorting by multiple columns and the first column was
  of string type (#1401).



## [0.6.0][] — 2018-06-05

### Added

- fread will detect feather file and issue an appropriate error message.

- when fread extracts data from archives into memory, it will now display
  the size of the extracted data in verbose mode.

- syntax `DT[i, j, by]` is now supported.

- multiple reduction operators can now be performed at once.

- in groupby, reduction columns can now be combined with regular or computed
  columns.

- during grouping, group keys are now added automatically to the select list.

- implement `sum()` reducer.

- `==` operator now works for string columns too.

- Improved performance of groupby operations.


### Fixed

- fread will no longer emit an error if there is an NA string in the header.

- if the input contains excessively long lines, fread will no longer waste time
  printing a sample of first 5 lines in verbose mode.

- fixed wrong calculation of mean / standard deviation of line length in fread
  if the sample contained broken lines.

- frame view will no longer get stuck in a Jupyter notebook.


## [0.5.0][] — 2018-05-25

### Added

- rbind()-ing now works on columns of all types (including between any types).

- `dt.rbind()` function to perform out-of-place row binding.

- ability to change the number of rows in a Frame.

- ability to modify a Frame in-place by assigning new values to particular
  cells.

- `dt.__git_version__` variable containing the commit hash from which the
  package was built.

- ability to read .bz2 compressed files with fread.


### Fixed

- Ensure that fread only emits messages to Python from the master thread.
- Fread can now properly recognize quoted NA strings.
- Fixed error when unbounded f-expressions were printed to console.
- Fixed problems when operating with too many memory-mapped Frames at once.
- Fixed incorrect groupby calculation in some rare cases.



## [0.4.0][] — 2018-05-07

### Added

- Fread now parses integers with thousands separator (e.g. "1,000").

- Added option `fread.anonymize` which forces fread to anonymize all user input
  in the verbose logs / error messages.

- Allow type-casts from booleans / integers / floats into strings.



## [0.3.2][] — 2018-04-25

### Added

- Implemented sorting for `str64` columns.

- write_csv can now write columns of type `str64`.

- Fread can now accept a list of files to read, or a glob pattern.

- Added `dt.lib.core.has_omp_support()` to check whether `datatable` was
  built with OMP support or not.

- Save per-column min/max information in the NFF format.


### Fixed

- Fix the source distribution (`sdist`) by including all the files that are
  required for building from source.

- Install no longer fails with `llvmlite 0.23.0` package.

- Fixed a stall in fread when using single-threaded mode with fill=True.



## [0.3.1][] — 2018-04-20

### Added

- Added ability to delete rows from a view Frame.

- Implement countna() function for `obj64` columns.

- New option `dt.options.core_logger` to help debug datatable.

- New Frame method `.materialize()` to convert a view Frame into a "real" one.
  This method is noop if applied to a non-view Frame.

- Several internal options to fine-tune the performance of sorting algorithm.

- Significantly improved performance of sorting doubles.

- fread can now read string columns that are larger than 2GB in size.

- fread can now accept a list/tuple of stypes for its `columns` parameter.

- improved logic for auto-assigning column names when they are missing.

- fread now supports reading files that contain NUL characters.

- Added global settings `options.frame.names_auto_index` and
  `options.frame.names_auto_prefix` to control automatic column name
  generation in a Frame.


### Changed

- When creating a column of "object" type, we will now coerce float "nan"
  values into `None`s.

- Renamed fread's parameter `strip_white` into `strip_whitespace`.

- Eliminated all `assert()` statements from C code, and replaced them with
  exception throws.

- Default column names, if none given by the user, are "C0", "C1", "C2", ...
  for both `fread` and `Frame` constructor.

- function-valued `columns` parameter in fread has been changed: if previously
  the function was invoked for every column, now it receives the list of all
  columns at once, and is expected to return a modified list (or dict / set /
  etc). Each column description in the list that the function receives carries
  the columns name and stype, in the future `format` field will also be added.


### Fixed

- fread will no longer consume excessive amounts of memory when reading a file
  with too many columns and few rows.

- fixed a possible crash when reading CSV file containing long string fields.

- fread: NA fields with whitespace were not recognized correctly.

- fread will no longer emit error messages or type-bump variables due to
  incorrectly recognized chunk boundaries.

- Fixed a crash when rbinding string column with non-string: now an exception
  will be thrown instead.

- Calling any stats function on a column of obj64 type will no longer result in
  a crash.

- Columns/rows slices no longer fail on an empty Frame.

- Fixed crash when materializing a view frame containing obj64 columns.

- Fixed erroneous grouping calculations.

- Fixed sorting of 1-row view frames.



## [0.3.0][] — 2018-03-19

### Added

- Method `df.tonumpy()` now has argument `stype` which will force conversion
  into a numpy array of the specific stype.

- Enums `stype` and `ltype` that encapsulate the type-system of the `datatable`
  module.

- It is now possible to fread from a `bytes` object.

- Allow columns to be renamed by setting the `names` property on the datatable.

- Internal "MemoryMapManager" will make datatable more robust when opening a
  frame with many columns on Linux systems. In particular, error 12 "not enough
  memory" should become much more rare now.

- Number of threads used by fread can now be controlled via parameter
  `nthreads`.

- It is now possible to supply string argument to `dt.DataTable` constructor,
  which in turn will try to interpret that argument via `fread`.

- `fread` can now read compressed `.xz` files.

- `fread` now automatically skips Ctrl+Z / NUL characters at the end of the
  file.

- It is now possible to create a datatable from string numpy array.

- Added parameters `skip_blank_lines`, `strip_white`, `quotechar` and `dec` to
  fread.

- Single-column files with blank lines can now be read successfully.

- Fread now recognizes \r\r\n as a valid line ending.

- Added parameters `url` and `cmd` to `fread`, as well as ability to detect URLs
  automatically. The `url` parameter downloads file from HTTP/HTTPS/FTP server
  into a temporary location and reads it from there. The `cmd` parameter
  executesthe provided shell command and then reads the data from the stdout.

- It is now possible to pass `file` objects to `fread` (or any objects exposing
  method `read()`).

- File path given to `fread` can now transparently select files within .zip
  archives. This doesn't work with archives-within-archives.

- GenericReader now supports auto-detecting and reading UTF-16 files.

- GenericReader now attempts to detect whether the input file is an HTML, and
  if so raises an exception with the appropriate error message.

- Datatable can now use either llvm-4.0 or llvm-5.0 depending on what the user
  has.

- fread now allows `sep=""`, causing the file to be read line-by-line.

- `range` arguments can now be passed to a DataTable constructor.

- datatable will now fall back to eager execution if it cannot detect LLVM
  runtime.

- simple Excel file reader.

- It is now possible to select columns from DataTable by type: `df[int]` selects
  all integer columns from `df`.

- Allow creating DataTable from list, while forcing a specific stype(s).

- Added ability to delete rows from a DataTable: `del df[rows, :]`

- DataTable can now accept pandas/numpy frames with columns of float16 dtype
  (which will be automatically converted to float32).

- .isna() function now works on strings too.

- `.save()` is now a method of `Frame` class.

- Warnings now have custom display hook.

- Added global option `nthreads` which control the number of Omp threads used
  by `datatable` for parallel execution. Example: `dt.options.nthreads = 1`.

- Add method `.scalar()` to quickly convert a 1x1 Frame into a python scalar.

- New methods `.min1()`, `.max1()`, `.mean1()`, `.sum1()`, `.sd1()`,
  `.countna1()` that are similar to `.min()`, `.max()`, etc. but return a scalar
  instead of a Frame (however they only work with a 1-column Frames).

- Implemented method `.nunique()` to compute the number of unique values in each
  column.

- Added stats functions `.mode()` and `.nmodal()`.


### Changed

- When writing "round" doubles/floats to CSV, they'll now always have trailing
  zero. For example, `[0.0, 1.0, 1e23]` now produces `"0.0,1.0,1.0e+23"`
  instead of `"0,1,1e+23"`.

- `df.stypes` now returns a tuple of `stype` elements (previously it was
  returning a list of strings). Likewise, `df.types` was renamed into
  `df.ltypes` and now it returns a tuple of `ltype` elements instead of strings.

- Parameter `colnames=` in DataTable constructor was renamed to `names=`. The
  old parameter may still be used, but it will result in a warning.

- DataTable can no longer have duplicate column names. If such names are given,
  they will be mangled to make them unique, and a warning will be issued.

- Special characters (in the ASCII range `\x00 - \x1F`) are no longer permitted
  in the column names. If encountered, they will be replaced with a dot `.`.

- Fread now ignores trailing whitespace on each line, even if ' ' separator is
  used.

- Fread on an empty file now produces an empty DataTable, instead of an
  exception.

- Fread's parameter `skip_lines` was replaced with `skip_to_line`, so that it's
  more in sync with the similar argument `skip_to_string`.

- When saving datatable containing "obj64" columns, they will no longer be
  saved, and user warning will be shown (previously saving this column would
  eventually lead to a segfault).

- (python) DataTable class was renamed into Frame.

- "eager" evaluation engine is now the default.

- Parameter `inplace` of method `rbind()` was removed: instead you can now rbind
  frames to an empty frame: `dt.Frame().rbind(df1, df2)`.


### Fixed

- `datatable` will no longer cause the C locale settings to change upon
  importing.

- reading a csv file with invalid UTF-8 characters in column names will no
  longer throw an exception.

- creating a DataTable from pandas.Series with explicit `colnames` will no
  longer ignore those column names.

- fread(fill=True) will correctly fill missing fields with NAs.

- fread(columns=set(...)) will correctly handle the case when the input contains
  multiple columns with the same names.

- fread will no longer crash if the input dataset contains invalid utf8/win1252
  data in the column headers (#594, #628).

- fixed bug in exception handling, which occasionally caused empty exception
  messages.

- fixed bug in fread where string fields starting with "NaN" caused an assertion
  error.

- Fixed bug when saving a DataTable with unicode column names into .nff format
  on systems where default encoding is not unicode-aware.

- More robust newline handling in fread (#634, #641, #647).

- Quoted fields are now correctly unquoted in fread.

- Fixed a bug in fread which occurred if the number of rows in the CSV file was
  estimated too low (#664).

- Fixed fread bug where an invalid DataTable was constructed if parameter
  `max_nrows` was used and there were any string columns (#671).

- Fixed a rare bug in fread which produced error message "Jump X did not finish
  reading where jump X+1 started" (#682).

- Prevented memory leak when using "PyObject" columns in conjunction with numpy.

- View frames can now be properly saved.

- Fixed crash when sorting view frame by a string column.

- Deleting 0 columns is no longer an error.

- Rows filter now works properly when applied to a view table and using "eager"
  evaluation engine.

- Computed columns expression can now be combined with rows expression, or
  applied to a view Frame.



## [0.2.2][] — 2017-10-18

### Added

- Ability to write DataTable into a CSV file: the `.to_csv()` method. The CSV
  writer is multi-threaded and extremely fast.

- Added `.internal.column(i).data_pointer` getter, to allow native code from
  other libraries to easily access the data in each column.

- Fread can now read hexadecimal floating-point numbers: floats and doubles.

- Csv writer will now auto-quote an empty string, and a string containing
  leading/trailing whitespace, so that it can be read by `fread` reliably.

- Fread now prints file sizes in "human-readable" form, i.e. KB/MB/GB instead
  of bytes.

- Fread can now understand a variety of "NaN" / "Inf" literals produced by
  different systems.

- Add option `hex` to csv writer, which controls whether floats will be written
  in decimal (default) or hexadecimal format.

- Csv writer now uses the "dragonfly" algorithm for writing doubles, which is
  faster than all known alternatives.

- It is now allowed to pass a single-row numpy array as an argument to
  `dt(rows=...)`, which will be treated the same as if it was a single-column
  array.

- Now `datatable`'s wheel will include libraries `libomp` and `libc++` on the
  platforms where they are not widely available.

- New `fread`'s argument `logger` allows the user to supply custom logging
  mechanism to fread. When this argument is provided, "verbose" mode is
  turned on automatically.


### Changed

- `datatable` will no longer attempt to distinguish between NA and NAN
  floating-point values.

- Constructing DataTable from a 2D numpy array now preserves shape of that
  array. At the same time it is no longer true that
  `arr.tolist() == numpy.array(DataTable(arr)).tolist()`: the list will be
  transposed.

- Converting a DataTable into a numpy array now also preserves shape. At the
  same time it is no longer true that `dt.topython() == dt.tonumpy().tolist()`:
  the list will be transposed.

- The internal `_datatable` module was moved to `datatable.lib._datatable`.


### Fixed

- `datatable` will now convert huge integers into double `inf` values instead
  of raising an exception.



## [0.2.1][] — 2017-09-11

### Added

- This CHANGELOG file.

- `sys.getsizeof(dt)` can now be used to query the size of the datatable in
  memory.

- A framework for computing and storing per-column summary statistics.

- Implemented statistics `min`, `max`, `mean`, `stdev`, `countna` for numeric
  and boolean columns.

- Getter `df.internal.rowindex` allows access to the RowIndex on the DataTable
  (for inspection / reuse).

- In addition to LLVM4 environmental variable, datatable will now also look for
  the `llvm4` folder within the package's directory.

- If `d0` is a DataTable, then `d1 = DataTable(d0)` will create its shallow
  copy.

- Environmental variable `DTNOOPENMP` will cause the `datatable` to be built
  without OpenMP support.


### Fixed

- Filter function when applied to a view DataTable now produces correct result.



## [0.2.0][] — 2017-08-30



## [0.1.0][] — 2017-04-13



[unreleased]: https://github.com/h2oai/datatable/compare/v0.9.0...HEAD
[0.9.0]: https://github.com/h2oai/datatable/compare/v0.8.0...v0.9.0
[0.8.0]: https://github.com/h2oai/datatable/compare/v0.7.0...v0.8.0
[0.7.0]: https://github.com/h2oai/datatable/compare/v0.6.0...v0.7.0
[0.6.0]: https://github.com/h2oai/datatable/compare/v0.5.0...v0.6.0
[0.5.0]: https://github.com/h2oai/datatable/compare/v0.4.0...v0.5.0
[0.4.0]: https://github.com/h2oai/datatable/compare/v0.3.2...v0.4.0
[0.3.2]: https://github.com/h2oai/datatable/compare/v0.3.1...v0.3.2
[0.3.1]: https://github.com/h2oai/datatable/compare/v0.3.0...v0.3.1
[0.3.0]: https://github.com/h2oai/datatable/compare/v0.2.2...v0.3.0
[0.2.2]: https://github.com/h2oai/datatable/compare/v0.2.1...v0.2.2
[0.2.1]: https://github.com/h2oai/datatable/compare/v0.2.0...v0.2.1
[0.2.0]: https://github.com/h2oai/datatable/compare/v0.1.0...v0.2.0
[0.1.0]: https://github.com/h2oai/datatable/tree/v0.1.0


[antorsae]: https://github.com/antorsae
[arno candel]: https://github.com/arnocandel
[carlosthinkbig]: https://github.com/CarlosThinkBig
[hawk berry]: https://github.com/hawkberry
[jonathan mckinney]: https://github.com/pseudotensor
[joseph granados]: https://github.com/g-eoj
[mateusz dymczyk]: https://github.com/mdymczyk
[megan kurka]: https://github.com/meganjkurka
[michael frasco]: https://github.com/mfrasco
[michal raška]: https://github.com/michal-raska
[nachigithub]: https://github.com/NachiGithub
[nishant kalonia]: https://github.com/nkalonia1
[oleksiy kononenko]: https://github.com/oleksiyskononenko
[olivier]: https://github.com/goldentom42
[pasha stetsenko]: https://github.com/st-pasha
[qiang kou]: https://github.com/thirdwing
[tom kraljevic]: https://github.com/tomkraljevic
[xiaomowu]: https://github.com/XiaomoWu
