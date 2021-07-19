
.. xfunction:: datatable.math.fmod
    :src: src/core/expr/fbinary/math.cc resolve_fn_fmod
    :cvar: doc_math_fmod
    :signature: fmod(x, y)

    Floating-point remainder of the division x/y. The result is always
    a float, even if the arguments are integers. This function uses
    ``std::fmod()`` from the standard C++ library, its convention for
    handling of negative numbers may be different than the Python's.
