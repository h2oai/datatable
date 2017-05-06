
.PHONY: all
all:
	$(MAKE) clean
	$(MAKE) build

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
	rm -rf htmlcov
	rm -rf pycov
	rm -rf ccov
	rm -f .coverage
	rm -f coverage.xml
	rm -f coverage.info
	rm -f *.so
	find . -type d -name "__pycache__" -exec rm -rf {} +

.PHONY: valgrind
valgrind:
	$(MAKE) clean
	VALGRIND=1 \
	$(MAKE) build
