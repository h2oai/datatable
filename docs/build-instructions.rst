Build from Source
-----------------

The key component needed for building the ``datatable`` package from source is the `Clang/Llvm <https://releases.llvm.org/download.html>`__ distribution. The same distribution is also required for building the ``llvmlite`` package, which is a prerequisite for ``datatable``. Note that the ``clang`` compiler which is shipped with MacOS is too old, and in particular it doesn't have support for the OpenMP technology.

Installing the ``Clang/Llvm`` distribution
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

1. Visit https://releases.llvm.org/download.html and **download** the most recent version of Clang/Llvm available for your platform (but no older than version 4.0.0).
2. Extract the downloaded archive into any suitable location on your hard drive.
3. Create one of the environment variables ``LLVM4`` / ``LLVM5`` / ``LLVM6`` (depending on the version of Clang/Llvm that you installed). The variable should point to the directory where you placed the Clang/Llvm distribution.

 For example, on Ubuntu after downloading ``clang+llvm-4.0.0-x86_64-linux-gnu-ubuntu-16.10.tar.xz`` the sequence of steps might look like:

 ::

    $ mv clang+llvm-4.0.0-x86_64-linux-gnu-ubuntu-16.10.tar.xz  /opt
    $ cd /opt
    $ sudo tar xvf clang+llvm-4.0.0-x86_64-linux-gnu-ubuntu-16.10.tar.xz
    $ export LLVM4=/opt/clang+llvm-4.0.0-x86_64-linux-gnu-ubuntu-16.10

You probably also want to put the last ``export`` line into your ``~/.bash_profile``.

Building ``datatable``
~~~~~~~~~~~~~~~~~~~~~~

1. Verify that you have Python 3.5 or above:

 ::

   $ python --V

 If you don't have Python 3.5 or later, you may want to download and install the newest version of Python, and then create and activate a virtual environment for that Python. For example:

 ::

   $ virtualenv --python=python3.6 ~/py36
   $ source ~/py36/bin/activate

2. Build ``datatable``:

 ::
  
   $ make build
   $ make install
   $ make test

3. Additional commands you may find occasionally interesting: 

 ::

   # Uninstall previously installed datatable
   make uninstall

   # Build a debug version of datatable (for example suitable for ``gdb`` debugging)
   make debug

   # Generate code coverage report
   make coverage

Troubleshooting
---------------

-  If you get an error like ``ImportError: This package should not be accessible on Python 3``, then you may have a ``PYTHONPATH`` environment variable that causes conflicts. See `this SO question <https://stackoverflow.com/questions/42214414/this-package-should-not-be-accessible-on-python-3-when-running-python3>`__ for details.

-  If you see errors such as ``"implicit declaration of function 'PyUnicode_AsUTF8' is invalid in C99"`` or ``"unknown type name 'PyModuleDef'"`` or ``"void function 'PyInit__datatable' should not return a value "``, it means your current Python is Python 2. Please revisit step 1 in the build instructions above.

-  If you are seeing an error ``'Python.h' file not found``, then it means you have an incomplete version of Python installed. This is known to sometimes happen on Ubuntu systems. The solution is to run ``apt-get install python-dev`` or ``apt-get install python3.6-dev``.

-  If you run into installation errors with ``llvmlite`` dependency, then your best bet is to attempt to install it manually before trying to build ``datatable``:

   ::

       $ pip install llvmlite

   Consult the ``llvmlite`` `Installation Guide <http://llvmlite.pydata.org/en/latest/admin-guide/install.html>`__ for additional information.

-  On OS-X, if you are getting an error ``fatal error: 'sys/mman.h' file not found`` or similar, this can be fixed by installing the Xcode Command Line Tools:

   ::

       $ xcode-select --install
