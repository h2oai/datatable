#-------------------------------------------------------------------------------
# Copyright 2018-2020 H2O.ai
#
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the "Software"),
# to deal in the Software without restriction, including without limitation
# the rights to use, copy, modify, merge, publish, distribute, sublicense,
# and/or sell copies of the Software, and to permit persons to whom the
# Software is furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
# IN THE SOFTWARE.
#-------------------------------------------------------------------------------

PYTHON ?= python


.PHONY: clean
clean::
	rm -rf .cache
	rm -rf .eggs
	rm -rf build
	rm -rf dist
	rm -rf datatable.egg-info
	rm -rf htmlcov
	rm -f *.so
	rm -f src/datatable/lib/_datatable*.pyd
	rm -f src/datatable/lib/_datatable*.so
	rm -f src/datatable/_build_info.py
	rm -f .coverage
	find . -type d -name "__pycache__" -exec rm -rf {} +


.PHONY: extra_install
extra_install:
	$(PYTHON) -m pip install -r requirements_extra.txt


.PHONY: test_install
test_install:
	$(PYTHON) -m pip install -r requirements_tests.txt


.PHONY: docs_install
docs_install:
	$(PYTHON) -m pip install -r requirements_docs.txt


.PHONY: test
test:
	$(PYTHON) -m pytest -ra --maxfail=10 -Werror tests

# In order to run with Address Sanitizer:
#   $ make asan
#   $ DYLD_INSERT_LIBRARIES=/usr/local/opt/llvm/lib/clang/7.0.0/lib/darwin/libclang_rt.asan_osx_dynamic.dylib ASAN_OPTIONS=detect_leaks=1 python -m pytest
#
.PHONY: asan
asan:
	@$(PYTHON) ci/ext.py asan


.PHONY: build
build:
	@$(PYTHON) ci/ext.py build


.PHONY: debug
debug:
	@$(PYTHON) ci/ext.py debug


.PHONY: geninfo
geninfo:
	@$(PYTHON) ci/ext.py geninfo --strict


.PHONY: coverage
coverage:
	$(PYTHON) ci/ext.py coverage
	$(MAKE) test_install
	DTCOVERAGE=1 $(PYTHON) -m pytest -x \
		--cov=datatable --cov-report=html:build/coverage-py \
		tests
	DTCOVERAGE=1 $(PYTHON) -m pytest -x \
		--cov=datatable --cov-report=xml:build/coverage.xml \
		tests
	chmod +x ci/llvm-gcov.sh
	# Note: running `lcov` we should pass an absolute path to
	# `ci/llvm-gcov.sh`. The reason is that `lcov` invokes
	# `geninfo` that is trying to detect the base directory in the
	# `find_base_from_graph()` routine. This routine fails
	# for source files that have no function definitions, because
	# these files are excluded from the file graph,
	# where `find_base_from_graph()` does the search. Passing
	# `${CURDIR}/ci/llvm-gcov.sh` to `lcov` fixes the problem.
	lcov --gcov-tool ${CURDIR}/ci/llvm-gcov.sh --capture --directory . --no-external --output-file build/coverage.info
	genhtml --legend --output-directory build/coverage-c --demangle-cpp build/coverage.info
	mv .coverage build/


.PHONY: wheel
wheel:
	@$(PYTHON) ci/ext.py wheel


.PHONY: sdist
sdist:
	@$(PYTHON) ci/ext.py sdist
