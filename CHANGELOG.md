## Changelog
The format is based on [Keep a Changelog](http://keepachangelog.com/)
and this project adheres to [Semantic Versioning](http://semver.org/).

### [Unreleased](https://github.com/h2oai/datatable/compare/v0.2.0...HEAD)
#### Added
- Getter `df.internal.rowindex` allows access to the RowIndex on the DataTable (for inspection / reuse).
- Implemented statistics `min`, `max`, `mean`, `stdev`, `countna` for numeric and boolean columns.
- A framework for computing and storin g per-column summary statistics.
- `sys.getsizeof(dt)` can now be used to query the size of the datatable in memory.
- This CHANGELOG file.

#### Fixed
- Filter function when applied to a view DataTable now produces correct result.

### [v0.2.0](https://github.com/h2oai/datatable/compare/v0.2.0...v0.1.0) — 2017-08-30

### [v0.1.0](https://github.com/h2oai/datatable/tree/v0.1.0) — 2017-04-13
