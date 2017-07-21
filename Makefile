
PYTHON ?= python
PIP ?= pip
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

.PHONY: build
build:
	$(PYTHON) setup.py build

.PHONY: install
install:
	$(PIP) install . --upgrade

.PHONY: test
test:
	rm -rf build/tests 2>/dev/null
	mkdir -p build/tests/
	pytest --junit-prefix=$(OS) --junitxml=build/tests/TEST-datatable.xml

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
	$(PYTHON) -m pytest --cov=datatable --cov-report=html:build/coverage-py
	$(PYTHON) -m pytest --cov=datatable --cov-report=xml:build/coverage.xml
	lcov --capture --directory . --output-file build/coverage.info
	genhtml build/coverage.info --output-directory build/coverage-c
	mv .coverage build/
