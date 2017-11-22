## Changelog
The format is based on [Keep a Changelog](http://keepachangelog.com/)
and this project adheres to [Semantic Versioning](http://semver.org/).

### [Unreleased](https://github.com/h2oai/datatable/compare/HEAD...v0.2.2)
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
  data in the column headers.



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
