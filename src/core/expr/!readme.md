
# datatable expressions

All functionality involving column expressions (e.g. `f.A`) is implemented
via the following mechanism:

1. First, every expression generates an object of class `Expr`. The class
   itself is defined in `expr/expr.py`, and it contains just 3 fields:
   - `_op`: the numeric code of the operation;
   - `_args`: the tuple of arguments (these are usually `Expr`s too, but
     may also be numbers, strings, or anything else);
   - `_params`: the dictionary of additional parameters.

   Thus, this class stores an arbitrary function call `fn(*args, **kwds)`,
   where the function name is mapped into a numeric code.

   The C++ runtime knows how to recognize these python `Expr` objects, there
   is a dedicated method `.is_dtexpr()` in `py::_obj` for this purpose.

2. Inside the main `DT[]` call, all arguments are converted into C++
   `dt::expr::Expr` objects. The C++ `Expr` class is more versatile than its
   Python counterpart, it encapsulates objects of any types: ints, strings,
   slices, numpy arrays, pandas DataFrames, python Exprs, etc.

   The C++ `Expr` class contains only two fields: `head` and `inputs`. The
   `inputs` is a vector of children `Expr` objects, whereas `head` is the
   object that describes how the inputs should be interpreted. Thus, the
   "head" of an Expr roughly corresponds to its type.

   See the description of `Expr` and `Head` classes for more info.

3. When C++ `Expr` wraps a python `Expr` object, the `_op`, `_args` and
   `_params` are extracted; then `_args` are converted into `inputs`, while
   `_op` and `_params` are used to create the `head` of type `Head_Func`.
   In particular, we call `Head_Func::from_op()` static constructor (see
   "head_func.cc"). This constructor works as a factory, instantiating
   the appropriate virtual class based on the numeric op code.

   Some of the ops do not resolve fully at this point. For example, many
   unary (and binary) functions are implemented via a single class that
   serves multiple ops within its category.


## Creating new function

In order to implement a new function or operator that would work with
column expressions, the following steps must be taken:

1. Create a new opcode for your function. This opcode must be declared
   in two places: C++ "src/core/expr/op.h", and python "src/core/expr/expr.py".
   Obviously, the numeric values must match.

2. Declare the function/method. This can be done either in python,
   or in C++. In the end the function should construct and return a
   python `Expr` object, having the opcode from step 1, as well as the
   appropriate args and kwargs.

3. Import the new function in "src/datatable/__init__.py" or,
   if the function is a part of a specific datatable module,
   in the corresponding "src/datatable/YOUR_MODULE_NAME.py". Also,
   add the function name to the `__all__` tuple within the same file.

4. In file "src/core/expr/head_func.h" either declare the new class derived from
   `Head_Func`, or see if you can reuse one of the existing classes there.
   Then in file "src/core/expr/head_func.cc" add the factory method for resolving
   your numeric opcode into an object of appropriate class.

5. Implement the functionality of your new opcode -- either in a dedicated
   class, or reuse the existing classes in "funary", "fbinary", "fnary".
   In the latter case you would also need to add factory functions to
   "src/core/expr/funary/umaker.cc" or "src/core/expr/fbinary/bimaker.cc" or
   "src/core/expr/fnary/fnary.h".

