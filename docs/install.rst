Installation
============

This section describes how to install Python ``datatable`` on various systems.

Prerequisites
-------------

Python 3.5 or newer is a prerequisite. You can check your python version via

.. code-block:: bash

   $ python --version

If you don't have Python 3.5 or later, you may want to download and install
the newest version of Python, and then create and activate a virtual
environment for that Python. For example:

.. code-block:: bash

   $ virtualenv --python=python3.6 ~/py36
   $ source ~/py36/bin/activate



Install on macOS and Linux
-----------------------------

Run the following command to install ``datatable`` on Mac OS X and Linux:

.. code-block:: bash

  $ pip install datatable



Install on Windows
------------------

As of version 0.11.0, ``datatable`` will also be available on Windows,
where the installation process will be identical to the one
for macOS and Linux.

Meanwhile, you can install the dev version of ``datatable``
from the pre-built wheels that are available on
`AppVeyor <https://ci.appveyor.com/project/h2oops/datatable/history>`__
To do so, simply click on the master build of your choice and
then navigate to ``Artifacts``. Copy the wheel URL that corresponds
to your version of Python and then install it as

.. code-block:: bash

  C:\> pip install YOUR_WHEEL_URL



Build from Source
-----------------

In order to install the latest development version of `datatable` directly
from GitHub, run the following command:

.. code-block:: bash

  $ pip install git+https://github.com/h2oai/datatable

Since ``datatable`` is written mostly in C++, you will need to have a C++
compiler on your computer. We recommend either `Clang 4+`, or `gcc 6+`,
however in theory any compiler that supports C++14 should work.



Build modified ``datatable``
----------------------------

If you want to tweak certain features of ``datatable``, or even add your
own functionality, you are welcome to do so.

1. First, clone ``datatable`` repository from GitHub:

  .. code-block:: bash

    $ git clone https://github.com/h2oai/datatable

2. Make ``datatable``:

  .. code-block:: bash

    $ make test_install
    $ make

3. Additional commands you may find occasionally interesting:

  .. code-block:: bash

   # Build a debug version of datatable (for example suitable for ``gdb`` debugging)
   $ make debug

   # Generate code coverage report
   $ make coverage

   # Build a debug version of datatable using an auto-generated makefile.
   # This does not work on all systems, but when it does it will work
   # much faster than standard "make debug".
   $ make fast



Troubleshooting
---------------

- If you get the error ``ImportError: This package should not be accessible on Python 3``, then you may have a ``PYTHONPATH`` environment variable that causes conflicts. See `this SO question <https://stackoverflow.com/questions/42214414/this-package-should-not-be-accessible-on-python-3-when-running-python3>`__ for details.

- If you see an error ``'Python.h' file not found``, then it means you have an incomplete version of Python installed. This is known to sometimes happen on Ubuntu systems. The solution is to run ``apt-get install python-dev`` or ``apt-get install python3.6-dev``.

- On OS X, if you are getting an error ``fatal error: 'sys/mman.h' file not found``, this can be fixed by installing the Xcode Command Line Tools:

  .. code-block:: bash

       $ xcode-select --install
