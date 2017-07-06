# datatable
R's data.table for Python

Python 3.6 is required; later on we will add option to convert to 3.5
format as well. We will not provide support for any Python version
below 3.5.

## Run instructions

Make sure that you have the
[Clang + Llvm-4.0](http://releases.llvm.org/download.html#4.0.0) combined package
installed even if you have both clang and llvm installed at version 4.0 already.
Python seems to need them together in one path. Then set up environment variable
`LLVM4` so that it points to the location of this package.

On Ubuntu that means:
  1. Click the link above and using your browser download the pre-built binary; e.g. `clang+llvm-4.0.0-x86_64-linux-gnu-ubuntu-16.10.tar.xz`
  2. Move that `.tar.xz` file to `/opt`
  3. `cd /opt`
  4. `sudo tar xvf clang+llvm-4.0.0-x86_64-linux-gnu-ubuntu-16.10.tar.xz`
  5. `export LLVM4=/opt/clang+llvm-4.0.0-x86_64-linux-gnu-ubuntu-16.10`

You will also need to have Python 3.6 as your default python. The easiest way
is to set up a virtual environment:
```bash
$ virtualenv --python=python3.6 ~/py36
$ source ~/py36/bin/activate
```
If you get an error like `ImportError: This package should not be accessible on Python 3. Either you are trying to run from the python-future src folder or your installation of python-future is corrupted` see: <https://stackoverflow.com/questions/42214414/this-package-should-not-be-accessible-on-python-3-when-running-python3>.

After that, go into the `datatable` folder and run:
```bash
$ make
```
This will compile that `datatable` package and pip-install it into your system.


## Troubleshooting

* If you see errors such as `"implicit declaration of function
'PyUnicode_AsUTF8' is invalid in C99"` or `"unknown type name 'PyModuleDef'"` or
`"void function 'PyInit__datatable' should not return a value`" -- it means you
are running under an old version of Python. Please switch to Python3.5+ as
described above (verify this by typing `python --version` in the console).

* If you are seeing an error that `'Python.h' file not found`, then it means
you have an incomplete version of Python installed. This is known to sometimes
happen on Ubuntu systems. The solution is to run `apt-get install python-dev`
or `apt-get install python3.6-dev`.

* If you run into installation errors with `llvmlite` dependency, you
might need to build it manually. This involves the following:

  1. If you're getting error `"invalid deployment target for
     -stdlib=libc++ (requires OS X 10.7 or later)"`, then set the
     environment variable
     ```bash
     $ export MACOSX_DEPLOYMENT_TARGET="10.7"
     ```
     (presumably your OS X version should be 10.7 or higher) and
     restart step 2.

  2. If you're getting error about missing
     `"llvmlite/binding/libllvmlite.dylib"`, then it is a
     [known problem](https://github.com/Rdatatable/data.table/pull/2084) of
     `llvmlite` package. You may try to install from this location:
     ```bash
     pip install -i https://pypi.anaconda.org/sklam/simple llvmlite
     ```


## Testing

  1. Make sure that `pytest` is installed in your virtualenv:
     ```bash
     $ pip install pytest
     $ pip install pytest-cov
     ```
  2. Run the tests:
     ```bash
     python -m pytest
     ```
  3. Generate coverage report:
     ```bash
     $ make coverage
     ```
