
Creating a new FExpr
====================

The majority of functions available from ``datatable`` module are implemented
via the ``FExpr`` mechanism. These functions have the same common API: they
accept one or more ``FExpr``s (or `fexpr-like` objects) as arguments and
produce an ``FExpr`` as the output. The resulting ``FExpr``s can then be used
inside the ``DT[...]`` call to apply these expressions to a particular frame.

In this document we describe how to create such ``FExpr``-based function. In
particular, we describe adding the ``gcd(a, b)`` function for computing the
greatest common divisor of two integers.


C++ "backend" class
-------------------

The core of the functionality will reside within a class derived from the
class ``dt::expr::FExpr``. So let's create the file ``expr/fexpr_gcd.cc`` and
declare the skeleton of our class:

.. code-block:: C++

    #include "expr/fexpr_func.h"
    #include "expr/eval_context.h"
    #include "expr/workframe.h"
    namespace dt {
    namespace expr {

    class FExpr_Gcd : public FExpr_Func {
      private:
        ptrExpr a_;
        ptrExpr b_;

      public:
        FExpr_Gcd(ptrExpr&& a, ptrExpr&& b)
          : a_(std::move(a)), b_(std::move(b)) {}

        std::string repr() const override;
        Workframe evaluate_n(EvalContext& ctx) const override;
    };

    }}

In this example we are inheriting from ``FExpr_Func``, which is a slightly more
specialized version of ``FExpr``.

You can also see that the two arguments in ``gcd(a, b)`` are stored within the
class as ``ptrExpr a_, b_``. This ``ptrExpr`` is actually a typedef for
``std::shared_ptr<FExpr>``, which means that arguments to our ``FExpr`` are
also ``FExpr``s.

The first method that needs to be implemented is ``repr()``, which is
more-or-less equivalent to python's ``__repr__``. The returned string should
not have the name of the class in it, instead it must be ready to be combined
with reprs of other expressions:

.. code-block:: C++

    std::string repr() const override {
      std::string out = "gcd(";
      out += a_->repr();
      out += ", ";
      out += b_->repr();
      out += ')';
      return out;
    }

We construct our repr out of reprs of ``a_`` and ``b_``. They are joined with
a comma, which has the lowest precedence in python. For some other FExprs we
may need to take into account the precedence of the arguments as well, in
order to properly set up parentheses around subexpressions.

The second method to implement is ``evaluate_n()``. The ``_n`` suffix here
stands for "normal". If you look into the source of ``FExpr`` class, you'll see
that there are other evaluation methods too: ``evaluate_i()``, ``evaluate_j()``,
etc. However all of those are not needed when implementing a simple function.

The method ``evaluate_n()`` takes an ``EvalContext`` object as the argument.
This object contains information about the current evaluation environment. The
output from ``evaluate_n()`` should be a ``Workframe`` object. A workframe can
be thought of as a "work-in-progress" frame. In our case it is sufficient to
treat it as a simple vector of columns.

We begin implementing ``evaluate_n()`` by evaluating the arguments ``a_`` and
``b_`` and then making sure that those frames are compatible with each other
(i.e. have the same number of columns and rows). After that we compute the
result by iterating through the columns of both frames and calling a simple
method ``evaluate1(Column&&, Column&&)`` (that we still need to implement):

.. code-block:: C++

    Workframe evaluate_n(EvalContext& ctx) const override {
      Workframe awf = a_->evaluate_n(ctx);
      Workframe bwf = b_->evaluate_n(ctx);
      if (awf.ncols() == 1) awf.repeat_column(bwf.ncols());
      if (bwf.ncols() == 1) bwf.repeat_column(awf.ncols());
      if (awf.ncols() != bwf.ncols()) {
        throw TypeError() << "Incompatible number of columns in " << repr()
            << ": the first argument has " << awf.ncols() << ", while the "
            << "second has " << bwf.ncols();
      }
      awf.sync_grouping_mode(bwf);

      auto gmode = awf.get_grouping_mode();
      Workframe outputs(ctx);
      for (size_t i = 0; i < awf.ncols(); ++i) {
        Column rescol = evaluate1(awf.retrieve_column(i),
                                  bwf.retrieve_column(i));
        outputs.add_column(std::move(rescol), std::string(), gmode);
      }
      return outputs;
    }

The method ``evaluate1()`` will take a pair of two columns and produce
the output column containing the result of ``gcd(a, b)`` calculation. We must
take into account the stypes of both columns, and decide which stypes are
acceptable for our function:

.. code-block:: C++

    #include "stype.h"

    Column evaluate1(Column&& a, Column&& b) const {
      SType stype1 = a.stype();
      SType stype2 = b.stype();
      SType stype0 = common_stype(stype1, stype2);
      switch (stype0) {
        case SType::BOOL:
        case SType::INT8:
        case SType::INT16:
        case SType::INT32: return make<int32_t>(std::move(a), std::move(b), SType::INT32);
        case SType::INT64: return make<int64_t>(std::move(a), std::move(b), SType::INT64);
        default:
            throw TypeError() << "Invalid columns of types " << stype1 << " and "
                << stype2 << " in " << repr();
      }
    }

    template <typename T>
    Column make(Column&& a, Column&& b, SType stype0) const {
      a.cast_inplace(stype0);
      b.cast_inplace(stype0);
      return Column(new Column_Gcd<T>(std::move(a), std::move(b)));
    }

As you can see, the job of the ``FExpr_Gcd`` class is to produce a workframe
containing one or more ``Column_Gcd`` virtual columns. This is where the actual
calculation of GCD values will take place, and we shall declare this class too.
It can be done either in a separate file in the `core/column/` folder, or
inside the current file `expr/fexpr_gcd.cc`.

.. code-block:: C++

    #include "column/virtual.h"

    template <typename T>
    class Column_Gcd : public Virtual_ColumnImpl {
      private:
        Column acol_;
        Column bcol_;

      public:
        Column_Gcd(Column&& a, Column&& b)
          : Virtual_ColumnImpl(a.nrows(), a.stype()),
            acol_(std::move(a)), bcol_(std::move(b))
        {
          xassert(acol_.nrows() == bcol_.nrows());
          xassert(acol_.stype() == bcol_.stype());
          xassert(acol_.can_be_read_as<T>());
        }

        ColumnImpl* clone() const override {
          return new Column_Gcd(Column(acol_), Column(bcol_));
        }

        size_t n_children() const noexcept override { return 2; }
        const Column& child(size_t i) const override { return i==0? acol_ : bcol_; }

        bool get_element(size_t i, T* out) const override {
          T a, b;
          bool avalid = acol_.get_element(i, &a);
          bool bvalid = bcol_.get_element(i, &b);
          if (avalid && bvalid) {
            while (b) {
              T tmp = b;
              b = a % b;
              a = tmp;
            }
            *out = a;
            return true;
          }
          return false;
        }
    };


Python-facing ``gcd()`` function
--------------------------------

Now that we have created the ``FExpr_Gcd`` class, we also need to have a python
function responsible for creating these objects. This is done in 4 steps:

First, declare a function with signature ``py::oobj(const py::XArgs&)``. The
``py::XArgs`` object here encapsulates all parameters that were passed to the
function, and it returns a ``py::oobj``, which is a simple wrapper around
python's ``PyObject*``.

.. code-block:: C++

    #include "documentation.h"
    #include "python/xargs.h"

    static py::oobj py_gcd(const py::XArgs& args) {
      auto a = args[0].to_oobj();
      auto b = args[1].to_oobj();
      return PyFExpr::make(new FExpr_Gcd(as_fexpr(a), as_fexpr(b)));
    }

This function takes the python arguments, if necessary validates and converts
them into C++ objects, then creates a new ``FExpr_Gcd`` object, and then
returns it wrapped into a ``PyFExpr`` (which is a python equivalent of the
generic ``FExpr`` class).

In the second step, we declare the signature and the docstring of this
python function:

.. code-block:: C++

    DECLARE_PYFN(&py_gcd)
        ->name("gcd")
        ->docs(dt::doc_dt_gcd)
        ->arg_names({"a", "b"})
        ->n_positional_args(2)
        ->n_required_args(2);

The variable ``doc_gcd`` must be declared in the common "documentation.h"
file:

.. code-block::

    extern const char* doc_dt_gcd;

The actual documentation should be written in a separate ``.rst`` file (more
on this later), and then it will be added into the code during the compilation
stage via the auto-generated file "documentation.cc".

At this point the method will be visible from python in the ``_datatable``
module. So the next step is to import it into the main ``datatable`` module.
To do this, go to ``src/datatable/__init__.py`` and write

.. code-block:: python

    from .lib._datatable import (
        ...
        gcd,
        ...
    )
    ...
    __all__ = (
        ...
        "gcd",
        ...
    )

To recap
~~~~~~~~
In this section we did the following:

  - Added a line ``extern const char* doc_dt_gcd;`` in file ``documentation.h``;

  - Added two lines to import/export ``gcd`` function in file ``src/datatable/__init__.py``;

  - Created file ``src/core/fexpr_gcd.cc`` with the following content:

.. code-block:: C++

    #include "column/virtual.h"
    #include "documentation.h"
    #include "expr/fexpr_func.h"
    #include "expr/eval_context.h"
    #include "expr/workframe.h"
    #include "python/xargs.h"
    #include "stype.h"
    namespace dt {
    namespace expr {


    template <typename T>
    class Column_Gcd : public Virtual_ColumnImpl {
      private:
        Column acol_;
        Column bcol_;

      public:
        Column_Gcd(Column&& a, Column&& b)
          : Virtual_ColumnImpl(a.nrows(), a.stype()),
            acol_(std::move(a)), bcol_(std::move(b))
        {
          xassert(acol_.nrows() == bcol_.nrows());
          xassert(acol_.stype() == bcol_.stype());
          xassert(acol_.can_be_read_as<T>());
        }

        ColumnImpl* clone() const override {
          return new Column_Gcd(Column(acol_), Column(bcol_));
        }

        size_t n_children() const noexcept override { return 2; }
        const Column& child(size_t i) const override { return i==0? acol_ : bcol_; }

        bool get_element(size_t i, T* out) const override {
          T a, b;
          bool avalid = acol_.get_element(i, &a);
          bool bvalid = bcol_.get_element(i, &b);
          if (avalid && bvalid) {
            while (b) {
              T tmp = b;
              b = a % b;
              a = tmp;
            }
            *out = a;
            return true;
          }
          return false;
        }
    };


    class FExpr_Gcd : public FExpr_Func {
      private:
        ptrExpr a_;
        ptrExpr b_;

      public:
        FExpr_Gcd(ptrExpr&& a, ptrExpr&& b)
          : a_(std::move(a)), b_(std::move(b)) {}


        std::string repr() const override {
          std::string out = "gcd(";
          out += a_->repr();
          out += ", ";
          out += b_->repr();
          out += ')';
          return out;
        }

        Workframe evaluate_n(EvalContext& ctx) const override {
          Workframe awf = a_->evaluate_n(ctx);
          Workframe bwf = b_->evaluate_n(ctx);
          if (awf.ncols() == 1) awf.repeat_column(bwf.ncols());
          if (bwf.ncols() == 1) bwf.repeat_column(awf.ncols());
          if (awf.ncols() != bwf.ncols()) {
            throw TypeError() << "Incompatible number of columns in " << repr()
                << ": the first argument has " << awf.ncols() << ", while the "
                << "second has " << bwf.ncols();
          }
          awf.sync_grouping_mode(bwf);

          auto gmode = awf.get_grouping_mode();
          Workframe outputs(ctx);
          for (size_t i = 0; i < awf.ncols(); ++i) {
            Column rescol = evaluate1(awf.retrieve_column(i),
                                      bwf.retrieve_column(i));
            outputs.add_column(std::move(rescol), std::string(), gmode);
          }
          return outputs;
        }

        Column evaluate1(Column&& a, Column&& b) const {
          SType stype1 = a.stype();
          SType stype2 = b.stype();
          SType stype0 = common_stype(stype1, stype2);
          switch (stype0) {
            case SType::BOOL:
            case SType::INT8:
            case SType::INT16:
            case SType::INT32: return make<int32_t>(std::move(a), std::move(b), SType::INT32);
            case SType::INT64: return make<int64_t>(std::move(a), std::move(b), SType::INT64);
            default:
                throw TypeError() << "Invalid columns of types " << stype1 << " and "
                    << stype2 << " in " << repr();
          }
        }

        template <typename T>
        Column make(Column&& a, Column&& b, SType stype0) const {
          a.cast_inplace(stype0);
          b.cast_inplace(stype0);
          return Column(new Column_Gcd<T>(std::move(a), std::move(b)));
        }
    };


    static py::oobj py_gcd(const py::XArgs& args) {
      auto a = args[0].to_oobj();
      auto b = args[1].to_oobj();
      return PyFExpr::make(new FExpr_Gcd(as_fexpr(a), as_fexpr(b)));
    }

    DECLARE_PYFN(&py_gcd)
        ->name("gcd")
        ->docs(dt::doc_dt_gcd)
        ->arg_names({"a", "b"})
        ->n_positional_args(2)
        ->n_required_args(2);

    }}  // namespace dt::expr::

Run ``make debug`` in order to compile the code and check that there are no
compilation errors or warnings. It's always better to compile in debug mode
during development, as this mode enables in-code assertions and also makes
debugging easier in case something goes wrong.




Tests
-----

Any functionality must be properly tested. We recommend creating a dedicated
test file for each new function. Thus, create file ``tests/dt/test-gcd.py``
and add some tests in it. We use the ``pytest`` framework for testing. In this
framework, each test is a single function (whose name starts with ``test_``)
which performs some actions and then asserts the validity of results.

.. code-block:: python

    import pytest
    import random
    from datatable import dt, f, gcd
    from tests import assert_equals  # checks equality of Frames
    from math import gcd as math_gcd

    def test_equal_columns():
        DT = dt.Frame(A=[1, 2, 3, 4, 5])
        RES = DT[:, gcd(f.A, f.A)]
        assert_equals(RES, dt.Frame([1, 1, 1, 1, 1]/dt.int32))

    @pytest.mark.parametrize("seed", [random.getrandbits(63)])
    def test_random(seed):
        random.seed(seed)
        n = 100
        src1 = [random.randint(1, 1000) for i in range(n)]
        src2 = [random.randint(1, 100) for i in range(n)]
        DT = dt.Frame(A=src1, B=src2)
        RES = DT[:, gcd(f.A, f.B)]
        assert_equals(RES, dt.Frame([math_gcd(src1[i], src2[i])
                                     for i in range(n)]))

When writing tests try to test any corner cases that you can think of. For
example, what if one of the numbers is 0? Negative? Add tests for various
column types, including invalid ones.


Documentation
-------------

The final piece of the puzzle is the documentation. We've already created
variable ``doc_gcd`` earlier, which will ensure that documentation will
be visible from python when you run ``help(gcd)``. However, the primary
place where people look for documentation is on a dedicated readthedocs
website, and this is where we will be adding the actual content.

So, create file ``docs/api/dt/gcd.rst``. The content of the file could
be something like this:

.. code-block:: rst

    .. xfunction:: datatable.gcd
        :src: src/core/expr/fexpr_gcd.cc py_gcd
        :tests: tests/dt/test-gcd.py
        :cvar: doc_dt_gcd
        :signature: gcd(a, b)

        Compute the greatest common divisor of `a` and `b`.

        Parameters
        ----------
        a, b: FExpr
            Only integer columns are supported.

        return: FExpr
            The returned column will have stype int64 if either `a` or `b` are
            of type int64, or otherwise it will be int32.


In these lines we declare:

  - the name of the function which provides the gcd functionality (this is
    presented to the user as the "src" link in the generated docs);

  - the name of the file dedicated to testing this functionality, this will
    also become a link in the generated documentation;

  - the name of the C variable declared in "documentation.h" which should
    be given a copy of the documentation, so that it can be embedded into
    python;

  - the main signature of the function: its name and parameters (with defaults
    if necessary).

This RST file now needs to be added to the toctree: open the file
``docs/api/index-api.rst`` and add it into the ``.. toctree::`` list at the
bottom, and also add it to the table of all functions.

Lastly, open ``docs/releases/v{LATEST}.rst`` (this is our changelog) and write
a brief paragraph about the new function:

.. code-block:: rst

    Frame
    -----
    ...

    -[new] Added new function :func:`gcd()` to compute the greatest common
      divisor of two columns. [#NNNN]

The ``[#NNNN]`` is a link to the GitHub issue where the ``gcd()`` function
was requested.


Submodules
----------

Some functions are declared within submodules of the datatable module. For
example, math-related functions can be found in ``dt.math``, string functions
in ``dt.str``, etc. Declaring such functions is not much different from what
is described above. For example, if we wanted our ``gcd()`` function to be
in the ``dt.math`` submodule, we'd made the following changes:

- Create file ``expr/math/fexpr_gcd.cc`` instead of ``expr/fexpr_gcd.cc``;

- Instead of importing the function in ``src/datatable/__init__.py`` we'd
  have imported it from ``src/datatable/math.py``;

- The test file name can be ``tests/math/test-gcd.py`` instead of
  ``tests/expr/test-gcd.py``;

- The doc file name can be ``docs/api/math/gcd.rst`` instead of
  ``docs/api/dt/gcd.rst``, and it should be added to the toctree in
  ``docs/api/math.rst``.



Debugging
---------

Sometimes the code we wrote doesn't work exactly as expected. This could
manifest in either program crashes with a segmentation fault, or simply an
exception or assertion error being thrown. In both cases you can troubleshoot
the problem by using the ``lldb`` tool (on macOS) or ``gdb`` (on Unix). Both
have very similar API, at least for simple commands. Please refer to
`this cheatsheet <lldb-commands>`_ for more advanced cases.

Next, you want to have a code sample that causes the problem. It can be
either in a separate python file, or in a pytest test-case, or maybe it's
small enough that you're willing to type it by hand in the console.
Regardless, the first step is to start the ``lldb``:

.. code-block:: console

    $ lldb python
    (lldb) target create "python"
    warning: (x86_64) /Users/pasha/py36/bin/python empty dSYM file detected, dSYM was created with an executable with no debug info.
    Current executable set to 'python' (x86_64).
    (lldb)

Now you want to set up breakpoints for your code. For example, ``b Error``
will put a breakpoint on any exception being raised by the code (because
``Error`` is the name of our function that is responsible for exception
handling). Likewise, you can set a breakpoint at any other function, or
even at a specific line in a file:

.. code-block:: console

    (lldb) b Error
    Breakpoint 1: no locations (pending).
    WARNING:  Unable to resolve breakpoint to any actual locations.
    (lldb) b Column_Gcd<int>::get_element
    (lldb) b py_gcd
    (lldb) b fexpr_gcd.cc:44

Obviously, you don't need to set up all these breakpoints: only those that
you think might be useful for catching the problem. And if your code fails
with a segfault, then no break points needed at all: lldb will stop on a
segfault anyways.

After the breakpoints have been set, run the code sample that you prepared.
If it's in a file ``ouch.py``, run ``r ouch.py``. If it's in a pytest case,
run ``r -m pytest path/to/test.py::testcase``. If it's in your head,
simply run ``r`` and then type in python's prompt.

Note that when your program actually imports datatable, lldb will display
a message that breakpoints that you have set up earlier were succesfully
added:

.. code-block:: python

    >>> from datatable import dt, f, gcd
    1 location added to breakpoint 1

If you don't see such message, then you probably misspelled breakpoint's
name and it won't work. Press :kbd:`Ctrl+C` to break out of python back
into lldb, issue a breakpoint command again, and then press ``c`` to
continue.

If you did everything correctly, then when your program invokes the
function with a breakpoint, lldb will stop and allow you to look around
and then proceed manually.

.. code-block:: console

    >>> from datatable import dt, f, gcd
    1 location added to breakpoint 1
    >>> DT = dt.Frame(A=range(10), B=range(0, 20, 2))
    >>> DT[:, gcd(f.A, f.B)]
    Process 76557 stopped
    * thread #1, queue = 'com.apple.main-thread', stop reason = breakpoint 1.1
        frame #0: 0x0000000104323388 _datatable.cpython-36m-darwin.so`dt::expr::Column_Gcd<int>::get_element(this=0x0000000101814730, i=0, out=0x00007ffeefbfe454) const at fexpr_gcd.cc:38:21
       35
       36       bool get_element(size_t i, T* out) const override {
       37         T a, b;
    -> 38         bool avalid = acol_.get_element(i, &a);
       39         bool bvalid = bcol_.get_element(i, &b);
       40         if (avalid && bvalid) {
       41           while (b) {
    Target 0: (python) stopped.
    (lldb)

At this point you can multiple commands to examine the execution stack
and to step through the program:

  - ``bt 20``: show program's backtrace (up to 20 lines, optional)
  - ``f 3``: go to execution frame 3
  - ``v``: display contents of all local variables
  - ``p expr``: display contents of expression ``expr``
  - ``n``: step over the current command
  - ``s``: step into the current command
  - ``c``: continue execution until the next breakpoint
  - ``thread list``: list all execution threads
  - ``t 5``: switch to thread number 5
  - ``q`` or :kbd:`Ctrl+D` to exit lldb.

Obviously, there are `many more <lldb-commands>`_ commands available, but
these are the most basic.


.. _`lldb-commands`: https://lldb.llvm.org/use/map.html
