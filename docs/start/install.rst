
Installation
============

This page describes how to install ``datatable`` on various systems.


Prerequisites
-------------

Python 3.5+ is required, although we recommend Python 3.6 or newer for best
results. You can check your python version via

.. xcode:: shell

    $ python --version
    Python 3.6.6


In addition, we recommend using ``pip`` version 20.0+, especially if you're
planning to install datatable from source, or if you are on a Unix machine.

.. xcode:: shell

    $ pip --version
    pip 19.3.1 from /home/pasha/py36/lib/python3.6/site-packages/pip (python 3.6)

    $ pip install pip --upgrade
    Collecting pip
      Using cached https://files.pythonhosted.org/.../pip-20.1.1-py2.py3-none-any.whl
    Installing collected packages: pip
      Found existing installation: pip 19.3.1
        Uninstalling pip-19.3.1:
          Successfully uninstalled pip-19.3.1
    Successfully installed pip-20.1.1



Basic installation
------------------

On most platforms ``datatable`` can be installed directly from `PyPI`_ using
``pip``:

.. xcode:: shell

    $ pip install datatable

The following platforms are supported:

- macOS
- Linux x86_64
- Linux ppc64le

Windows wheels cannot be installed from PyPI just yet, but they will be
available in the next release. In the meanwhile, ``datatable`` development
wheels can be installed.



Installing latest dev version
-----------------------------

Meanwhile, you can install the dev version of ``datatable`` on Windows
from the pre-built wheels that are available on
`AppVeyor <https://ci.appveyor.com/project/h2oops/datatable/history>`__.
To do so, simply click on the master build of your choice and
then navigate to ``Artifacts``. Copy the wheel URL that corresponds
to your version of Python and then install it as:

.. xcode:: winshell

   C:\> pip install YOUR_WHEEL_URL



Build from source
-----------------

In order to install the latest development version of `datatable` directly
from GitHub, run the following command:

.. xcode:: shell

   $ pip install git+https://github.com/h2oai/datatable

Since ``datatable`` is written mostly in C++, you will need to have a C++
compiler on your computer. We recommend either `Clang 4+`, or `gcc 6+`,
however in theory any compiler that supports C++14 should work.



Build modified ``datatable``
----------------------------

If you want to tweak certain features of ``datatable``, or even add your
own functionality, you are welcome to do so.

1. First, clone ``datatable`` repository from GitHub:

   .. xcode:: shell

      $ git clone https://github.com/h2oai/datatable

2. Make ``datatable``:

   .. xcode:: shell

      $ make test_install
      $ make

3. Additional commands you may find occasionally interesting:

   .. xcode:: shell

     $ # Build a debug version of datatable (for example suitable for ``gdb`` debugging)
     $ make debug

     $ # Generate code coverage report
     $ make coverage

     $ # Build a debug version of datatable using an auto-generated makefile.
     $ # This does not work on all systems, but when it does it will work
     $ # much faster than standard "make debug".
     $ make fast



Troubleshooting
---------------

- If you get the error ``ImportError: This package should not be accessible on
  Python 3``, then you may have a ``PYTHONPATH`` environment variable that
  causes conflicts. See `this SO question`_ for details.

- If you see an error ``'Python.h' file not found``, then it means you have an
  incomplete version of Python installed. This is known to sometimes happen on
  Ubuntu systems. The solution is to run ``apt-get install python-dev`` or
  ``apt-get install python3.6-dev``.

- On macOS, if you are getting an error ``fatal error: 'sys/mman.h' file not
  found``, this can be fixed by installing the Xcode Command Line Tools:

  .. xcode:: shell

     $ xcode-select --install


.. _this SO question: https://stackoverflow.com/questions/42214414/this-package-should-not-be-accessible-on-python-3-when-running-python3

.. _`PyPI`: https://pypi.org/
