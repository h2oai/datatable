
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

It is also **recommended** to :ref:`Create a Python Virtual Environment<PVE>`.




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

.. _Install Dev Version:

Install latest dev version
--------------------------

If you wish to test the latest version of ``datatable`` before it has been
officially released, then you can use one of the binary wheels that we build
as part of our Continuous Integration process. 

There are two sources available;

- For macOS and Linux please use our `S3 repository`_. 

- For Windows installations, you can find the latest wheels within our
  `AppVeyor`_ build system.


Install from the Amazon S3 Repository (macOS & Linux)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
First it is **recommended** to :ref:`Create a Python Virtual Environment<PVE Linux & macOS>`.

Both the most current and historical development wheels for macOS & Linux can 
be found at our `S3 repository`_.

- To install the most current development version, **scroll to the bottom** of 
  the page to find the latest links, and then download or copy the URL of a
  wheel that corresponds to your Python version and platform. 

  .. image:: ../_static/amazon_s3_repo.png
    :scale: 25 %
    :alt: datatable S3 binary wheel links

- This wheel can be installed with ``pip`` as usual:

.. code-block:: console

    $ pip install https://AWS S3 DATATABLE BUILD WHEEL URL.whl

Install from AppVeyor (Windows)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

First it is **recommended** to :ref:`Create a Python Virtual Environment<PVE Windows>`.

- To find the latest build wheels for datatable, go to datatable's `AppVeyor`_ 
  CI/CD Instance and select the platform you're installing for:

  .. image:: ../_static/appveyor-platform.png
    :scale: 25 %
    :alt: datatable appveyor platform links

- next, select **"Artifacts"** and **right/option-click** on the filename for 
  your desired build wheel.
- then, Select "Copy Link Address" from your browser's context menu to copy 
  the wheel's URL

  .. image:: ../_static/appveyor-artifacts.png
    :scale: 25 %
    :alt: datatable appveyor artifact links

- Finally, install using :code:`pip install` by pasting the copied url.

.. code-block:: doscon

    C:\> pip install https://APPVEYOR DATATABLE BUILD WHEEL URL.whl


.. _Install from Source:

Installation from Source
------------------------

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

Build for Linux
^^^^^^^^^^^^^^^
- Install GCC for your distribution:

  - Debian/Ubuntu:

  .. code-block:: console

    $ sudo apt install build-essential

  - RHEL/CentOS:

  .. code-block:: console

    $ sudo dnf group install "Development Tools"

    # or

    $ sudo yum groupinstall "Development tools"

- **Recommended:** :ref:`Create a Python Virtual Environment<PVE Linux & macOS>`.

- Then install datatable directly from github from using 
  :code:`pip install git+https://...`

.. code-block:: console

  $ pip install git+https://github.com/h2oai/datatable


Build for macOS
^^^^^^^^^^^^^^^

- For macOS you will need to install the ``XCode Command Line Tools``
- run :code:`xcode-select --install`` from your terminal and confirm the 
  prompts for download and installation of the xcode command-line tools.

.. code-block:: console

  $ xcode-select --install

- Then install datatable directly from github from using 
  :code:`pip install git+https://...`

- **Recommended:** :ref:`Create a Python Virtual Environment<PVE Linux & macOS>`.

.. code-block:: console

  $ pip install git+https://github.com/h2oai/datatable

Build for Windows
^^^^^^^^^^^^^^^^^

- In order to install the MSVC C++ compiler, you need to download and install 
  `Visual Studio`_ or `Visual Studio Community Edition`_ and choose the option 
  `Desktop Development with C++`, then select **install**.

- Next, your shell must have the MSVC development tools available. To enable
  them, you must run the MS Dev environment setup scripts (usually located in
  the :code:`...\\Common7\\Tools\\` directory of your Visual Studio installation 
  directory)

  - e.g. :code:`C:\\Program Files\\Microsoft Visual Studio\\2022\\Community\\Common7\\Tools\\`

- Depending on your shell, run the VS CLI environment setup script:

  - Powershell

  .. code-block:: powershell

    > "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\Launch-VsDevShell.ps1"

  - CMD or Anaconda Prompt

  .. code-block:: doscon

    C:\> "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\LaunchDevCmd.bat"

- Lastly, On windows, datatable uses an environment variable to find the
  MSVC compiler, so you must set that variable.

  - Powershell

  .. code-block:: powershell

    > $env:DT_MSVC_PATH="$env:VSINSTALLDIR"+"VC\Tools\MSVC\"

  - CMD or Anaconda Prompt

  .. code-block:: doscon

    C:\> set DT_MSVC_PATH=%VSINSTALLDIR%VC\Tools\MSVC\

- Finally, you can install datatable directly from github from using 
  :code:`pip install git+https://...`

- **Recommended:** :ref:`Create a Python Virtual Environment<PVE Windows>`.

.. code-block:: console

  $ pip install git+https://github.com/h2oai/datatable




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

``ModuleNotFoundError: No module named 'gendoc'``
  Check your python version with ``python --version``. if you are running 
  python3.10 or above, you will need to :ref:`install the latest dev version<Install Dev Version>`
  or :ref:`build from source<Install from Source>`.

.. _PVE:

Appendix: Creating a Python Virtual Environment
-----------------------------------------------

.. _PVE Linux & macOS:

Linux & macOS
^^^^^^^^^^^^^

- Create a new project directory and change to it

.. code-block:: console

  $ mkdir myproject && cd myproject && pwd

- Create a Python Virtual Environment and activate it

.. code-block:: console

  $ python -m venv venv && source venv/bin/activate

- Confirm Python Path and Version

.. code-block:: console

  $ which python && python --version


.. _PVE Windows:

Windows Powershell
^^^^^^^^^^^^^^^^^^

- Launch a Powershell Terminal
  - Run as Administrator (right-click on shortcut and click Run as Admin...)
- Verify your desired Python version:

.. code-block:: powershell

  > python --version

- Your output should reflect your python version if you clicked the checkbox 
  when installing python.
  - If not, you can run the following to add Python to your system's Path. 
  - Once complete, you will need to relaunch powershell as admin.

.. code-block:: powershell

  > [Environment]::SetEnvironmentVariable("Path", "$env:Path;C:\Program Files\Python310")

- Next, you will need to modify the Execution Policy to allow Python's venv
  scripts to execute:

.. code-block:: powershell

  > Set-ExecutionPolicy Unrestricted -Force

- Now that the Powershell setup is complete, you're ready to create a
  Python virtual Environment:

  - Create a new directory and change to it:

  .. code-block:: powershell

    > New-Item -Path ".\" -Name "myproject" -ItemType "directory"; Set-Location .\myproject\

  - Create a Virtual Environment and activate it:

  .. code-block:: powershell

    > python -m venv venv; .\venv\Scripts\Activate.ps1

  - Confirm Path and Python Version:

  .. code-block:: powershell

    > Get-Command python | select Source; python --version


Windows CMD
^^^^^^^^^^^

- Launch a CMD Prompt
  - Run as Administrator (right-click on shortcut and click Run as Admin...)
- Verify your desired Python version:

.. code-block:: doscon

  C:\> python --version

- Your output should reflect your python version if you clicked the checkbox 
  when installing python.
  - If not, you can run the following to add Python to your system's Path.

.. code-block:: doscon

  C:\> setx /M PATH "%PATH%;C:\Program Files\Python310"


- Now that the CMD/DOS setup is complete, you're ready to create a
  Python virtual Environment:

  - Create a new directory and change to it:

  .. code-block:: doscon

    C:\> mkdir myproject && cd myproject\

  - Create a Virtual Environment and activate it:

  .. code-block:: doscon

    C:\> python -m venv venv && .\venv\Scripts\activate.bat

  - Confirm Path and Python Version:

  .. code-block:: doscon

    C:\> where python && python --version


.. rubric:: Footnotes

.. [#v11] Since version v0.11.0


.. Other links

.. _`end of life`: https://endoflife.date/python

.. _`PyPI`: https://pypi.org/

.. _`AppVeyor`: https://ci.appveyor.com/project/h2oops/datatable/

.. _`S3 repository`: https://h2o-release.s3.amazonaws.com/datatable/index.html

.. _`StackOverflow`: https://stackoverflow.com/questions/tagged/py-datatable

.. _`GitHub`: https://github.com/h2oai/datatable/issues

.. _`Visual Studio`: https://visualstudio.microsoft.com/vs/

.. _`Visual Studio Community Edition`: https://visualstudio.microsoft.com/vs/community/
