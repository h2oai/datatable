
PYTHON ?= python
OS := $(shell uname | tr A-Z a-z)

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

.PHONY: test
test:
	rm -rf build/test-reports 2>/dev/null
	mkdir -p build/test-reports/
	$(PYTHON) -m pytest --junit-prefix=$(OS) --junitxml=build/test-reports/TEST-datatable.xml -rxs tests

.PHONY: valgrind
valgrind:
	$(MAKE) clean
	VALGRIND=1 \
	$(MAKE) build
	$(MAKE) install

.PHONY: coverage
coverage:
	$(MAKE) clean
	WITH_COVERAGE=1 \
	$(MAKE) build
	$(MAKE) install
	$(PYTHON) -m pytest --cov=datatable --cov-report=html:build/coverage-py tests/
	$(PYTHON) -m pytest --cov=datatable --cov-report=xml:build/coverage.xml tests/
	lcov --gcov-tool ./llvm-gcov.sh --capture --directory . --output-file build/coverage.info
	genhtml build/coverage.info --output-directory build/coverage-c
	mv .coverage build/
