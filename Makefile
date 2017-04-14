
all:
	$(MAKE) clean
	$(MAKE) build

build:
	python setup.py build
	pip install . --upgrade

clean:
	rm -rf .cache
	rm -rf .eggs
	rm -rf build
	rm -rf datatable.egg-info
	rm -rf htmlcov
	rm -f .coverage
	rm -f *.so
	find . -type d -name "__pycache__" -exec rm -rf {} +
