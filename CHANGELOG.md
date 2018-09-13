<!---
  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
-->

## Changelog
The format is based on [Keep a Changelog](http://keepachangelog.com/)
and this project adheres to [Semantic Versioning](http://semver.org/).

### [Unreleased](https://github.com/h2oai/datatable/compare/HEAD...v0.6.0)
#### Added
- Frame can now be created from a list/dict of numpy arrays.
- Filters can now be used together with groupby expressions.
- fread's verbose output now includes time for opening the input file.
- Added ability to read/write Jay files.
- Frames can now be constructed via the keyword-args list of columns
  (i.e. `Frame(A=..., B=...)`).
- Implemented logical operators "and" `&` and "or" `|` for eager evaluator.
- Implemented integer division `//` and modulo `%` operators.
- Now a key column can be set on a Frame.
- Key column(s) are saved when the frame is saved into a Jay file.
- A Frame can now be naturally-joined with a keyed Frame.

#### Fixed
- bug in dt.cbind() where the first Frame in the list was ignored.
- bug with applying a cast expression to a view column.
- occasional memory errors caused by a lack of available mmap handles.
- memory leak in groupby operations.
- `names` parameter in Frame constructor is now checked for correctness.
- bug in fread with QR bump occurring out-of-sample.
- `import datatable` now takes only 0.13s, down from 0.6s.
- fread no longer wastes time reading the full input, if max_nrows option is used.
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
  that the column contains many non-ASCII characters.


### [v0.6.0](https://github.com/h2oai/datatable/compare/v0.6.0...v0.5.0) — 2018-06-05
#### Added
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

#### Fixed
- fread will no longer emit an error if there is an NA string in the header.
- if the input contains excessively long lines, fread will no longer waste time
  printing a sample of first 5 lines in verbose mode.
- fixed wrong calculation of mean / standard deviation of line length in fread
  if the sample contained broken lines.
- frame view will no longer get stuck in a Jupyter notebook.


### [v0.5.0](https://github.com/h2oai/datatable/compare/v0.5.0...v0.4.0) — 2018-05-25
#### Added
- rbind()-ing now works on columns of all types (including between any types).
- `dt.rbind()` function to perform out-of-place row binding.
- ability to change the number of rows in a Frame.
- ability to modify a Frame in-place by assigning new values to particular
  cells.
- `dt.__git_version__` variable containing the commit hash from which the
  package was built.
- ability to read .bz2 compressed files with fread.

#### Fixed
- Ensure that fread only emits messages to Python from the master thread.
- Fread can now properly recognize quoted NA strings.
- Fixed error when unbounded f-expressions were printed to console.
- Fixed problems when operating with too many memory-mapped Frames at once.
- Fixed incorrect groupby calculation in some rare cases.


### [v0.4.0](https://github.com/h2oai/datatable/compare/0.4.0...v0.3.2) — 2018-05-07
#### Added
- Fread now parses integers with thousands separator (e.g. "1,000").
- Added option `fread.anonymize` which forces fread to anonymize all user input
  in the verbose logs / error messages.
- Allow type-casts from booleans / integers / floats into strings.


### [v0.3.2](https://github.com/h2oai/datatable/compare/0.3.2...v0.3.1) — 2018-04-25
#### Added
- Implemented sorting for `str64` columns.
- write_csv can now write columns of type `str64`.
- Fread can now accept a list of files to read, or a glob pattern.
- Added `dt.lib.core.has_omp_support()` to check whether `datatable` was
  built with OMP support or not.
- Save per-column min/max information in the NFF format.

#### Fixed
- Fix the source distribution (`sdist`) by including all the files that are
  required for building from source.
- Install no longer fails with `llvmlite 0.23.0` package.
- Fixed a stall in fread when using single-threaded mode with fill=True.


### [v0.3.1](https://github.com/h2oai/datatable/compare/0.3.1...v0.3.0) — 2018-04-20
#### Added
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

#### Changed
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

#### Fixed
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


### [v0.3.0](https://github.com/h2oai/datatable/compare/0.3.0...v0.2.2) — 2018-03-19
#### Added
- Method `df.tonumpy()` now has argument `stype` which will force conversion into
  a numpy array of the specific stype.
- Enums `stype` and `ltype` that encapsulate the type-system of the `datatable`
  module.
- It is now possible to fread from a `bytes` object.
- Allow columns to be renamed by setting the `names` property on the datatable.
- Internal "MemoryMapManager" will make datatable more robust when opening a
  frame with many columns on Linux systems. In particular, error 12 "not enough
  memory" should become much more rare now.
- Number of threads used by fread can now be controlled via parameter `nthreads`.
- It is now possible to supply string argument to `dt.DataTable` constructor,
  which in turn will try to interpret that argument via `fread`.
- `fread` can now read compressed `.xz` files.
- `fread` now automatically skips Ctrl+Z / NUL characters at the end of the file.
- It is now possible to create a datatable from string numpy array.
- Added parameters `skip_blank_lines`, `strip_white`, `quotechar` and `dec` to fread.
- Single-column files with blank lines can now be read successfully.
- Fread now recognizes \r\r\n as a valid line ending.
- Added parameters `url` and `cmd` to `fread`, as well as ability to detect URLs
  automatically. The `url` parameter downloads file from HTTP/HTTPS/FTP server
  into a temporary location and reads it from there. The `cmd` parameter executes
  the provided shell command and then reads the data from the stdout.
- It is now possible to pass `file` objects to `fread` (or any objects exposing
  method `read()`).
- File path given to `fread` can now transparently select files within .zip archives.
  This doesn't work with archives-within-archives.
- GenericReader now supports auto-detecting and reading UTF-16 files.
- GenericReader now attempts to detect whether the input file is an HTML, and if so
  raises an exception with the appropriate error message.
- Datatable can now use either llvm-4.0 or llvm-5.0 depending on what the user has.
- fread now allows `sep=""`, causing the file to be read line-by-line.
- `range` arguments can now be passed to a DataTable constructor.
- datatable will now fall back to eager execution if it cannot detect LLVM runtime.
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
- New methods `.min1()`, `.max1()`, `.mean1()`, `.sum1()`, `.sd1()`, `.countna1()`
  that are similar to `.min()`, `.max()`, etc. but return a scalar instead of a
  Frame (however they only work with a 1-column Frames).
- Implemented method `.nunique()` to compute the number of unique values in each
  column.
- Added stats functions `.mode()` and `.nmodal()`.

#### Changed
- When writing "round" doubles/floats to CSV, they'll now always have trailing zero.
  For example, [0.0, 1.0, 1e23] now produce "0.0,1.0,1.0e+23" instead of "0,1,1e+23".
- `df.stypes` now returns a tuple of `stype` elements (previously it was returning
  a list of strings). Likewise, `df.types` was renamed into `df.ltypes` and now it
  returns a tuple of `ltype` elements instead of strings.
- Parameter `colnames=` in DataTable constructor was renamed to `names=`. The old
  parameter may still be used, but it will result in a warning.
- DataTable can no longer have duplicate column names. If such names are given,
  they will be mangled to make them unique, and a warning will be issued.
- Special characters (in the ASCII range `\x00 - \x1F`) are no longer permitted in
  the column names. If encountered, they will be replaced with a dot `.`.
- Fread now ignores trailing whitespace on each line, even if ' ' separator is used.
- Fread on an empty file now produces an empty DataTable, instead of an exception.
- Fread's parameter `skip_lines` was replaced with `skip_to_line`, so that it's
  more in sync with the similar argument `skip_to_string`.
- When saving datatable containing "obj64" columns, they will no longer be saved,
  and user warning will be shown (previously saving this column would eventually
  lead to a segfault).
- (python) DataTable class was renamed into Frame.
- "eager" evaluation engine is now the default.
- Parameter `inplace` of method `rbind()` was removed: instead you can now rbind
  frames to an empty frame: `dt.Frame().rbind(df1, df2)`.

#### Fixed
- `datatable` will no longer cause the C locale settings to change upon importing.
- reading a csv file with invalid UTF-8 characters in column names will no longer
  throw an exception.
- creating a DataTable from pandas.Series with explicit `colnames` will no longer
  ignore those column names.
- fread(fill=True) will correctly fill missing fields with NAs.
- fread(columns=set(...)) will correctly handle the case when the input contains
  multiple columns with the same names.
- fread will no longer crash if the input dataset contains invalid utf8/win1252
  data in the column headers (#594, #628).
- fixed bug in exception handling, which occasionally caused empty exception
  messages.
- fixed bug in fread where string fields starting with "NaN" caused an assertion error.
- Fixed bug when saving a DataTable with unicode column names into .nff format
  on systems where default encoding is not unicode-aware.
- More robust newline handling in fread (#634, #641, #647).
- Quoted fields are now correctly unquoted in fread.
- Fixed a bug in fread which occurred if the number of rows in the CSV file was
  estimated too low (#664).
- Fixed fread bug where an invalid DataTable was constructed if parameter `max_nrows`
  was used and there were any string columns (#671).
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



### [v0.2.2](https://github.com/h2oai/datatable/compare/v0.2.2...v0.2.1) — 2017-10-18
#### Added
- Ability to write DataTable into a CSV file: the `.to_csv()` method. The CSV writer
  is multi-threaded and extremely fast.
- Added `.internal.column(i).data_pointer` getter, to allow native code from other
  libraries to easily access the data in each column.
- Fread can now read hexadecimal floating-point numbers: floats and doubles.
- Csv writer will now auto-quote an empty string, and a string containing leading/
  trailing whitespace, so that it can be read by `fread` reliably.
- Fread now prints file sizes in "human-readable" form, i.e. KB/MB/GB instead of bytes.
- Fread can now understand a variety of "NaN" / "Inf" literals produced by different
  systems.
- Add option `hex` to csv writer, which controls whether floats will be written in
  decimal (default) or hexadecimal format.
- Csv writer now uses the "dragonfly" algorithm for writing doubles, which is faster
  than all known alternatives.
- It is now allowed to pass a single-row numpy array as an argument to `dt(rows=...)`,
  which will be treated the same as if it was a single-column array.
- Now `datatable`'s wheel will include libraries `libomp` and `libc++` on the platforms
  where they are not widely available.
- New `fread`'s argument `logger` allows the user to supply custom logging mechanism to
  fread. When this argument is provided, "verbose" mode is turned on automatically.

#### Changed
- `datatable` will no longer attempt to distinguish between NA and NAN floating-point values.
- Constructing DataTable from a 2D numpy array now preserves shape of that array. At the same
  time it is no longer true that `arr.tolist() == numpy.array(DataTable(arr)).tolist()`: the
  list will be transposed.
- Converting a DataTable into a numpy array now also preserves shape. At the same time it is
  no longer true that `dt.topython() == dt.tonumpy().tolist()`: the list will be transposed.
- The internal `_datatable` module was moved to `datatable.lib._datatable`.

#### Fixed
- `datatable` will now convert huge integers into double `inf` values instead of raising an exception.



### [v0.2.1](https://github.com/h2oai/datatable/compare/v0.2.1...v0.2.0) — 2017-09-11
#### Added
- This CHANGELOG file.
- `sys.getsizeof(dt)` can now be used to query the size of the datatable in memory.
- A framework for computing and storing per-column summary statistics.
- Implemented statistics `min`, `max`, `mean`, `stdev`, `countna` for numeric and boolean columns.
- Getter `df.internal.rowindex` allows access to the RowIndex on the DataTable (for inspection / reuse).
- In addition to LLVM4 environmental variable, datatable will now also look for the `llvm4` folder
  within the package's directory.
- If `d0` is a DataTable, then `d1 = DataTable(d0)` will create its shallow copy.
- Environmental variable `DTNOOPENMP` will cause the `datatable` to be built without OpenMP support.

#### Fixed
- Filter function when applied to a view DataTable now produces correct result.



### [v0.2.0](https://github.com/h2oai/datatable/compare/v0.2.0...v0.1.0) — 2017-08-30



### [v0.1.0](https://github.com/h2oai/datatable/tree/v0.1.0) — 2017-04-13
