
.PHONY: all
all:
	$(MAKE) clean
	$(MAKE) build
	$(MAKE) test

.PHONY: build
build:
	python setup.py build
	pip install . --upgrade

.PHONY: clean
clean:
	rm -rf .cache
	rm -rf .eggs
	rm -rf build
	rm -rf datatable.egg-info
	rm -f *.so
	find . -type d -name "__pycache__" -exec rm -rf {} +

.PHONY: test
test:
	pytest

.PHONY: valgrind
valgrind:
	$(MAKE) clean
	VALGRIND=1 \
	$(MAKE) build

.PHONY: coverage
coverage:
	$(MAKE) clean
	WITH_COVERAGE=1 \
	$(MAKE) build
	python -m pytest --cov=datatable --cov-report=html:build/coverage-py
	python -m pytest --cov=datatable --cov-report=xml:build/coverage.xml
	lcov --capture --directory . --output-file build/coverage.info
	genhtml build/coverage.info --output-directory build/coverage-c
	mv .coverage build/
