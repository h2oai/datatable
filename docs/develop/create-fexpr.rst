
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
          xassert(acol_.type().can_be_read_as<T>());
        }

        ColumnImpl* clone() const override {
          return new Column_Gcd(Column(acol_), Column(bcol_));
        }

        size_t n_children() const noexcept { return 2; }
        const Column& child(size_t i) { return i==0? acol_ : bcol_; }

        bool get_element(size_t i, T* out) {
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

First, declare a function with signature ``py::oobj(const py::PKArgs&)``. The
``py::PKArgs`` object here encapsulates all parameters that were passed to the
function, and it returns a ``py::oobj``, which is a simple wrapper around
python's ``PyObject*``.

.. code-block:: C++

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

    static const char* doc_gcd =
    R"(gcd(a, b)
    --

    Compute the greatest common divisor of `a` and `b`.

    Parameters
    ----------
    a, b: FExpr
        Only integer columns are supported.

    return: FExpr
        The returned column will have stype int64 if either `a` or `b` are
        of type int64, or otherwise it will be int32.
    )";

    DECLARE_PYFN(&py_gcd)
        ->name("gcd")
        ->docs(doc_gcd)
        ->arg_names({"a", "b"})
        ->n_positional_args(2)
        ->n_required_args(2);

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


Tests
-----

Any functionality must be properly tested. We recommend creating a dedicated
test file for each new function. Thus, create file ``tests/expr/test-gcd.py``
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

The final piece of the puzzle is the documentation. We've already written the
documentation for our function: the ``doc_gcd`` variable declared earlier.
However, for now this is only visible from python when you run ``help(gcd)``.
We also want the documentation to be visible on our official readthedocs
website, which requires a few more steps. So:

First, create file ``docs/api/dt/gcd.rst``. The content of the file should
contain just few lines:

.. code-block:: rst

    .. xfunction:: datatable.gcd
        :doc: src/core/fexpr/fexpr_gcd.cc doc_gcd
        :src: src/core/fexpr/fexpr_gcd.cc py_gcd
        :tests: tests/expr/test-gcd.py

In these lines we declare: in which source file the docstring can be found,
and what is the name of its variable. The documentation generator will be
looking for a ``static const char* doc_gcd`` variable in the source. Then
we also declare the name of the function which provides the gcd functionality.
The generator will look for a function with that name in the specified source
file and create a link to that source in the output doc file. Lastly, the
``:tests:`` parameter says which file contains tests dedicated to this
function, this will also become a link in the generated documentation.

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
