<!---
  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
-->

# datatable

[![Build Status](https://travis-ci.org/h2oai/datatable.svg?branch=master)](https://travis-ci.org/h2oai/datatable)

This is a Python package for manipulating 2-dimensional tabular data structures
(aka data frames). It is close in spirit to [pandas] or [SFrame]; however we
put specific emphasis on speed and big data support. As the name suggests, the
package is closely related to R's [data.table] and attempts to mimic its core
algorithms and API.


Currently `datatable` is in the Alpha stage and is undergoing active
development. The API may be unstable; some of the core features are incomplete
and/or missing.


Python 3.5+ is required. There are no plans to support any earlier Python
versions.


## See also

* [Build instructions](https://github.com/h2oai/datatable/wiki/Build-instructions)
* [Changelog](https://github.com/h2oai/datatable/blob/master/CHANGELOG.md)


  [pandas]: https://github.com/pandas-dev/pandas
  [SFrame]: https://github.com/turi-code/SFrame
  [data.table]: https://github.com/Rdatatable/data.table
