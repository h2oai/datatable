<!---
  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
-->

# datatable

[![PyPi version](https://img.shields.io/pypi/v/pandas.svg)](https://pypi.org/project/datatable/)
[![License](https://img.shields.io/pypi/l/datatable.svg)](https://github.com/h2oai/datatable/blob/master/LICENSE)
[![Build Status](https://travis-ci.org/h2oai/datatable.svg?branch=master)](https://travis-ci.org/h2oai/datatable)

This is a Python package for manipulating 2-dimensional tabular data structures
(aka data frames). It is close in spirit to [pandas] or [SFrame]; however we
put specific emphasis on speed and big data support. As the name suggests, the
package is closely related to R's [data.table] and attempts to mimic its core
algorithms and API.

Currently `datatable` is in the Alpha stage and is undergoing active
development. The API may be unstable; some of the core features are incomplete
and/or missing. Python 3.5+ is required.


## Project goals

`datatable` started in 2017 as a toolkit for performing big data (up to 100GB)
operations on a single-node machine, at the maximum speed possible. Such
requirements are dictated by modern machine-learning applications, which need
to process large volumes of data and generate many features in order to
achieve the best model accuracy. The first user of `datatable` was [Driverless.ai].

The set of features that we want to implement with `datatable` is at least
the following:

* Column-oriented data storage.
* Native-C implementation for all datatypes, including strings. Packages such as
  pandas and numpy already do that for numeric columns, but not for strings.
* Support for date-time and categorical types. Object type is also supported,
  but promotion into object discouraged.
* All types should support null values, with as little overhead as possible.
* Data should be stored on disk in the same format as in memory. This will allow
  us to memory-map data on disk and work ob out-of-memory datasets transparently.
* Work with memory-mapped datasets to avoid loading into memory more data than
  necessary for each particular operation.
* Fast data reading from CSV and other formats.
* Multi-threaded data processing: time-consuming operations should attempt to
  utilize all cores for maximum efficiency.
* Efficient algorithms for sorting/grouping/joining.
* Expressive query syntax (similar to [data.table]).
* LLVM-based lazy computation for complex queries (code generated, compiled and
  executed on-the-fly).
* LLVM-based user-defined functions.
* Minimal amount of data copying, copy-on-write semantics for shared data.
* Use "rowindex" views in filtering/sorting/grouping/joining operators to avoid
  unnecessary data copying.
* Interoperability with pandas / numpy / pure python: the users should have
  the ability to convert to another data-processing framework with ease.
* Restrictions: Python 3.5+, 64-bit systems only.


## Installation

On MacOS systems installing datatable is as easy as
```
pip install datatable
```

On Linux you can install a binary distribution as
```
# If you have Python 3.5
pip install https://s3.amazonaws.com/h2o-release/datatable/stable/datatable-0.3.1/datatable-0.3.1%2Bmaster.61-cp35-cp35m-linux_x86_64.whl

# If you have Python 3.6
pip install https://s3.amazonaws.com/h2o-release/datatable/stable/datatable-0.3.1/datatable-0.3.1%2Bmaster.61-cp36-cp36m-linux_x86_64.whl
```

On all other platforms a source distribution will be needed.


## See also

* [Build instructions](https://github.com/h2oai/datatable/wiki/Build-instructions)
* [Changelog](https://github.com/h2oai/datatable/blob/master/CHANGELOG.md)


  [pandas]: https://github.com/pandas-dev/pandas
  [SFrame]: https://github.com/turi-code/SFrame
  [data.table]: https://github.com/Rdatatable/data.table
  [driverless.ai]: https://www.h2o.ai/driverless-ai/
