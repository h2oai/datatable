# datatable
R's data.table for Python

Python 3.6 is required; later on we will add option to convert to 3.5
format as well. We will not provide support for any Python version
below 3.5.

## Run instructions

Inside the `datatable` folder run:
```bash
$ make
```
which does this:
```bash
$ python setup.py build
$ pip install . --upgrade
```

Note that you'll need to have Python 3.6 as your default python. Also,
you have to have write permissions to the python's `setup-packages`.
The easiest way is to set up a virtual environment:
```bash
$ virtualenv --python=python3.6 ~/py36
$ source ~/py36/bin/activate
```


## Troubleshooting

If you run into installation errors with `llvmlite` dependency, you
might need to build it manually. This involves the following:

  1. Check that you have `llvm 3.9.x` installed on your system:
     ```bash
     $ llvm-config --version
     ```
     If this produces an error, **OR** if the reported version is
     anything other than 3.9.*, then you'll need to download the correct
     LLVM binaries from http://releases.llvm.org/download.html#3.9.0
     After you unpack this distribution to a folder of your choice, set
     up an environment variable
     ```bash
     $ export LLVM_CONFIG = "/path/to/LLVM_3.9/bin/llvm-config"
     ```
     Check that the path is correct:
     ```bash
     $ $LLVM_CONFIG --version
     ```

  2. Install `llvmlite`:
     ```bash
     $ pip install llvmlite
     ```

  3. If you're getting error `"invalid deployment target for
     -stdlib=libc++ (requires OS X 10.7 or later)"`, then set the
     environment variable
     ```bash
     $ export MACOSX_DEPLOYMENT_TARGET = "10.7"
     ```
     (presumably your OS X version should be 10.7 or higher) and
     restart step 2.


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
     $ python -m pytest --cov=datatable --cov-report=html
     ```