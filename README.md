<!---
  Copyright 2018-2019 H2O.ai

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

# datatable

[![PyPi version](https://img.shields.io/pypi/v/datatable.svg)](https://pypi.org/project/datatable/)
[![License](https://img.shields.io/pypi/l/datatable.svg)](https://github.com/h2oai/datatable/blob/master/LICENSE)
[![Build Status](https://travis-ci.org/h2oai/datatable.svg?branch=master)](https://travis-ci.org/h2oai/datatable)
[![Documentation Status](https://readthedocs.org/projects/datatable/badge/?version=latest)](https://datatable.readthedocs.io/en/latest/?badge=latest)
[![Codacy Badge](https://api.codacy.com/project/badge/Grade/e72cadff26ed4ad68decd61b66b4c563)](https://www.codacy.com/app/st-pasha/datatable?utm_source=github.com&amp;utm_medium=referral&amp;utm_content=h2oai/datatable&amp;utm_campaign=Badge_Grade)

This is a Python package for manipulating 2-dimensional tabular data structures
(aka data frames). It is close in spirit to [pandas][] or [SFrame][]; however we
put specific emphasis on speed and big data support. As the name suggests, the
package is closely related to R's [data.table][] and attempts to mimic its core
algorithms and API.

Currently `datatable` is in the Alpha stage and is undergoing active
development. The API may be unstable; some of the core features are incomplete
and/or missing. Python 3.5+ is required.


## Project goals

`datatable` started in 2017 as a toolkit for performing big data (up to 100GB)
operations on a single-node machine, at the maximum speed possible. Such
requirements are dictated by modern machine-learning applications, which need
to process large volumes of data and generate many features in order to
achieve the best model accuracy. The first user of `datatable` was
[Driverless.ai][].

The set of features that we want to implement with `datatable` is at least
the following:

* Column-oriented data storage.

* Native-C implementation for all datatypes, including strings. Packages such
  as pandas and numpy already do that for numeric columns, but not for
  strings.

* Support for date-time and categorical types. Object type is also supported,
  but promotion into object discouraged.

* All types should support null values, with as little overhead as possible.

* Data should be stored on disk in the same format as in memory. This will
  allow us to memory-map data on disk and work on out-of-memory datasets
  transparently.

* Work with memory-mapped datasets to avoid loading into memory more data than
  necessary for each particular operation.

* Fast data reading from CSV and other formats.

* Multi-threaded data processing: time-consuming operations should attempt to
  utilize all cores for maximum efficiency.

* Efficient algorithms for sorting/grouping/joining.

* Expressive query syntax (similar to [data.table][]).

* LLVM-based lazy computation for complex queries (code generated, compiled
  and executed on-the-fly).

* LLVM-based user-defined functions.

* Minimal amount of data copying, copy-on-write semantics for shared data.

* Use "rowindex" views in filtering/sorting/grouping/joining operators to
  avoid unnecessary data copying.

* Interoperability with pandas / numpy / pure python: the users should have
  the ability to convert to another data-processing framework with ease.

* Restrictions: Python 3.5+, 64-bit systems only.


## Installation

On MacOS and Linux systems installing datatable is as easy as
```sh
pip install datatable
```

On all other platforms a source distribution will be needed. For more
information see [Build instructions](https://datatable.readthedocs.io/en/latest/install.html).


## See also

* [Build instructions](https://datatable.readthedocs.io/en/latest/install.html)
* [Changelog](https://github.com/h2oai/datatable/blob/master/CHANGELOG.md)
* [Documentation](https://datatable.readthedocs.io/en/latest/?badge=latest)


  [pandas]: https://github.com/pandas-dev/pandas
  [sframe]: https://github.com/turi-code/SFrame
  [data.table]: https://github.com/Rdatatable/data.table
  [driverless.ai]: https://www.h2o.ai/driverless-ai/
