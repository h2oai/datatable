
PYTHON ?= python
OS := $(shell uname | tr A-Z a-z)
MODULE ?= .


.PHONY: all
all:
	$(MAKE) clean
	$(MAKE) build
	$(MAKE) install
	$(MAKE) test


.PHONY: clean
clean:
	rm -rf .cache
	rm -rf .eggs
	rm -rf build
	rm -rf dist
	rm -rf datatable.egg-info
	rm -f *.so
	find . -type d -name "__pycache__" -exec rm -rf {} +


.PHONY: mrproper
mrproper: clean
	git clean -f -d -x


.PHONY: build
build:
	$(PYTHON) setup.py build

.PHONY: install
install:
	$(PYTHON) -m pip install . --upgrade --no-cache-dir


.PHONY: uninstall
uninstall:
	$(PYTHON) -m pip uninstall datatable -y

.PHONY: test_install
test_install:
	$(PYTHON) -m pip install ${MODULE}[testing] --no-cache-dir

.PHONY: test
test:
	$(MAKE) test_install
	rm -rf build/test-reports 2>/dev/null
	mkdir -p build/test-reports/
	$(PYTHON) -m pytest -ra \
		--junit-prefix=$(OS) \
		--junitxml=build/test-reports/TEST-datatable.xml \
		tests


.PHONY: benchmark
benchmark:
	$(PYTHON) -m pytest -ra -v benchmarks


.PHONY: debug
debug:
	$(MAKE) clean
	DTDEBUG=1 \
	$(MAKE) build
	$(MAKE) install

.PHONY: build_noomp
build_noomp:
	DTNOOPENMP=1 \
	$(MAKE) build


.PHONY: bi
bi:
	$(MAKE) build
	$(MAKE) install


.PHONY: coverage
coverage:
	$(MAKE) clean
	DTCOVERAGE=1 \
	$(MAKE) build
	$(MAKE) install
	$(MAKE) test_install
	$(PYTHON) -m pytest \
		--benchmark-skip \
		--cov=datatable --cov-report=html:build/coverage-py \
		tests
	$(PYTHON) -m pytest \
		--benchmark-skip \
		--cov=datatable --cov-report=xml:build/coverage.xml \
		tests
	lcov --capture --directory . --output-file build/coverage.info
	# lcov --gcov-tool ./llvm-gcov.sh --capture --directory . --output-file build/coverage.info
	genhtml build/coverage.info --output-directory build/coverage-c
	mv .coverage build/
