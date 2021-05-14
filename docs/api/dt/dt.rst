
.. xdata:: datatable.dt
	:src: src/datatable/__init__.py dt

	This is the :mod:`datatable` module itself.

	The purpose of exporting this symbol is so that you can easily
	import all the things you need from the datatable module in one
	go::

		>>> from datatable import dt, f, g, by, join, mean

	Note: while it is possible to write

	.. code-block:: python

		>>> test = dt.dt.dt.dt.dt.dt.dt.dt.dt.fread('test.jay')
		>>> train = dt.dt.dt.dt.dt.dt.dt.dt.dt.dt.dt.dt.dt.fread('train.jay')

	we do not in fact recommend doing so (except possibly on April 1st).
