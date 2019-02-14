<!--
  Copyright 2018 H2O.ai

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
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

On MacOS systems installing datatable is as easy as
```sh
pip install datatable
```

On Linux you can install a binary distribution as
```sh
# If you have Python 3.5
pip install https://s3.amazonaws.com/h2o-release/datatable/stable/datatable-0.8.0/datatable-0.8.0-cp35-cp35m-linux_x86_64.whl

# If you have Python 3.6
pip install https://s3.amazonaws.com/h2o-release/datatable/stable/datatable-0.8.0/datatable-0.8.0-cp36-cp36m-linux_x86_64.whl
```

On all other platforms a source distribution will be needed. For more
information see [Build instructions](https://github.com/h2oai/datatable/wiki/Build-instructions).


## License

This project is licensed under the terms of [Apache License v2.0][]. The text
of this license is also available in the [LICENSE][] file.

All files in this repository that do not bear any boilerplate notices, should
be presumed to have the following copyright/license notice attached (in
satisfaction of the definition of "Work" from section 1 of the license):

```text
   Copyright 2018 H2O.ai

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
```


## See also

* [Build instructions](https://github.com/h2oai/datatable/wiki/Build-instructions)
* [Changelog](https://github.com/h2oai/datatable/blob/master/CHANGELOG.md)
* [Documentation](https://datatable.readthedocs.io/en/latest/?badge=latest)


[apache license v2.0]: http://www.apache.org/licenses/LICENSE-2.0
[license]: https://github.com/h2oai/datatable/LICENSE
[pandas]: https://github.com/pandas-dev/pandas
[sframe]: https://github.com/turi-code/SFrame
[data.table]: https://github.com/Rdatatable/data.table
[driverless.ai]: https://www.h2o.ai/driverless-ai/
