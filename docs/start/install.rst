
Installation
============

This page describes how to install ``datatable`` on various systems.



Prerequisites
-------------

Python 3.7+ is required. Generally, we will support each version of Python
until its official `end of life`_. You can verify your python version via

.. code-block:: console

    $ python --version
    Python 3.7.10

In addition, we recommend using ``pip`` version 20.3+, especially if you're
planning to install datatable from the source, or if you are on a Unix machine.

.. code-block:: console

    $ pip install pip --upgrade
    Collecting pip
      Using cached pip-21.1.2-py3-none-any.whl (1.5 MB)
    Installing collected packages: pip
      Attempting uninstall: pip
        Found existing installation: pip 20.2.4
        Uninstalling pip-20.2.4:
          Successfully uninstalled pip-20.2.4
    Successfully installed pip-21.1.2

There are no other prerequisites. Datatable does not depend on any other python
module [#v11]_, nor on any non-standard system library.



Basic installation
------------------

On most platforms ``datatable`` can be installed directly from `PyPI`_ using
``pip``:

.. code-block:: console

    $ pip install datatable

The following platforms are supported:

- **macOS**

  Datatable has been tested to work on macOS 10.12 (Sierra), macOS 10.13
  (High Sierra), macOS 10.15 (Catalina), macOS 11 (BigSur) and 
  macOS 12 (Monterey).

- **Linux x86_64 / ppc64le**

  For ``x86_64`` and ``ppc64le`` architectures we produce binary wheels 
  that are tagged as ``manylinux_2_17``. Consequently, they will
  work with your Linux distribution if it is compatible with this tag.
  Please refer to :pep:`600` for details.

- **Windows**

  Windows wheels are available for Windows 10 or later.



Install latest dev version
--------------------------

If you wish to test the latest version of ``datatable`` before it has been
officially released, then you can use one of the binary wheels that we build
as part of our Continuous Integration process.

If you are on Windows, then pre-built wheels are available on `AppVeyor`_.
Click on a green main build of your choice, then navigate to the "Artifacts"
tab, copy the wheel URL that corresponds to your Python version, and finally
install it as:

.. code-block:: doscon

    C:\> pip install YOUR_WHEEL_URL

For macOS and Linux, development wheels can be found at our `S3 repository`_.
Scroll to the bottom of the page to find the latest links, and then download
or copy the URL of a wheel that corresponds to your Python version and
platform. This wheel can be installed with ``pip`` as usual:

.. code-block:: console

    $ pip install YOUR_WHEEL_URL



Build from source
-----------------

In order to build and install the latest development version of ``datatable``
directly from GitHub, run the following command:

.. code-block:: console

   $ pip install git+https://github.com/h2oai/datatable

Since ``datatable`` is written mostly in C++, your computer must be set up for
compiling C++ code. The build script will attempt to find the compiler
automatically, searching for GCC, Clang, or MSVC on Windows. If it fails, or
if you want to use some other compiler, then set environment variable ``CXX``
before building the code.

Datatable uses C++14 language standard, which means you must use the compiler
that fully implements this standard. The following compiler versions are known
to work:

- Clang 5+;
- GCC 6+;
- MSVC 19.14+.



Install datatable in editable mode
----------------------------------

If you want to tweak certain features of ``datatable``, or even add your
own functionality, you are welcome to do so. This section describes how
to install datatable for development process.

1. First, you need to fork the repository and then :ref:`clone it locally
   <local-setup>`:

   .. code-block:: console

      $ git clone https://github.com/your_user_name/datatable
      $ cd datatable

2. Build ``_datatable`` core library. The two most common options are:

   .. code-block:: console

      $ # build a "production mode" datatable
      $ make build

      $ # build datatable in "debug" mode, without optimizations and with
      $ # internal asserts enabled
      $ make debug

   Note that you would need to have a C++ compiler in order to compile and
   link the code. Please refer to the previous section for compiler
   requirements.

   On macOS you may also need to install Xcode Command Line Tools.

   On Linux if you see an error that ``'Python.h' file not found``, then it
   means you need to install a "development" version of Python, i.e. the one
   that has python header files included.

3. After the previous step succeeds, you will have a ``_datatable.*.so`` file
   in the ``src/datatable/lib`` folder. Now, in order to make ``datatable``
   usable from Python, run

   .. code-block:: console

      $ echo "`pwd`/src" >> ${VIRTUAL_ENV}/lib/python*/site-packages/easy-install.pth

   (This assumes that you are using a virtualenv-based python. If not, then
   you'll need to adjust the path to your python's ``site-packages``
   directory).

4. Install additional libraries that are needed to test datatable:

   .. code-block:: console

       $ pip install -r requirements_tests.txt
       $ pip install -r requirements_extra.txt
       $ pip install -r requirements_docs.txt

5. Check that everything works correctly by running the test suite:

   .. code-block:: console

       $ make test

Once these steps are completed, subsequent development process is much simpler.
After any change to C++ files, re-run ``make build`` (or ``make debug``) and
then restart python for the changes to take effect.

Datatable only recompiles those files that were modified since the last time,
which means that usually the compile step takes only few seconds. Also note
that you can switch between the "build" and "debug" versions of the library
without performing ``make clean``.



Troubleshooting
---------------

Despite our best effort to keep the installation process hassle-free, sometimes
problems may still arise. Here we list some of the more frequent ones, where we
know how to resolve them. If none of these help you, please ask a question on
`StackOverflow`_ (tagging with ``[py-datatable]``), or file an issue on
`GitHub`_.

``pip._vendor.pep517.wrappers.BackendUnavailable``
  This error occurs when you have an old version of ``pip`` in your environment.
  Please upgrade ``pip`` to the version 20.3+, and the error should disappear.

``ImportError: cannot import name '_datatable'``
  This means the internal core library ``_datatable.*.so`` is either missing
  entirely, is in a wrong location, or has the wrong name. The first step
  is therefore to find where that file actually is. Use the system ``find``
  tool, limiting the search to your python directory.

  If the file is missing entirely, then it was either deleted, or installation
  used a broken wheel file. In either case, the only solution is to rebuild or
  reinstall the library completely.

  If the file is present but not within the ``site-packages/datatable/lib/``
  directory, then moving it there should solve the issue.

  If the file is present and is in the correct directory, then there must be a
  name conflict. In python run::

    >>> import sysconfig
    >>> sysconfig.get_config_var("SOABI")
    'cpython-36m-ppc64le-linux-gnu'

  The reported suffix should match the suffix of the ``_datatable.*.so`` file.
  If it doesn't, then renaming the file will fix the problem.

``Python.h: no such file or directory`` when compiling from source
  Your Python distribution was shipped without the ``Python.h`` header file.
  This has been observed on certain Linux machines. You would need to install
  a Python package with a ``-dev`` suffix, for example ``python3.7-dev``.

``fatal error: 'sys/mman.h' file not found`` on macOS
  In order to compile from source on mac computers, you need to have Xcode
  Command Line Tools installed. Run:

  .. code-block:: console

     $ xcode-select --install

``ImportError: This package should not be accessible``
  The most likely cause of this error is a misconfigured ``PYTHONPATH``
  environment variable. Unset that variable and try again.




.. rubric:: Footnotes

.. [#v11] Since version v0.11.0


.. Other links

.. _`end of life`: https://endoflife.date/python

.. _`PyPI`: https://pypi.org/

.. _`AppVeyor`: https://ci.appveyor.com/project/h2oops/datatable/history

.. _`S3 repository`: https://h2o-release.s3.amazonaws.com/datatable/index.html

.. _`StackOverflow`: https://stackoverflow.com/questions/tagged/py-datatable

.. _`GitHub`: https://github.com/h2oai/datatable/issues
