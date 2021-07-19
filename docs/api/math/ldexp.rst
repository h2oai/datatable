
.. xfunction:: datatable.math.ldexp
    :src: src/core/expr/fbinary/math.cc resolve_fn_ldexp
    :cvar: doc_math_ldexp
    :signature: ldexp(x, y)

    Multiply ``x`` by 2 raised to the power ``y``, i.e. compute ``x * 2**y``.
    Column ``x`` is expected to be float, and ``y`` integer.
