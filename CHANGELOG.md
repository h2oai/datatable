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


This is an archive of Changelog up to and including version 0.8.0. For
more recent changes, visit the
[ReadTheDocs](https://datatable.readthedocs.io/en/latest/changelog.html)
website.








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
