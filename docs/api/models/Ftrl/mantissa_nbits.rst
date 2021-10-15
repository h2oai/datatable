
.. xattr:: datatable.models.Ftrl.mantissa_nbits
    :src: src/core/models/py_ftrl.cc Ftrl::get_mantissa_nbits Ftrl::set_mantissa_nbits
    :cvar: doc_models_Ftrl_mantissa_nbits
    :settable: new_mantissa_nbits


    Number of mantissa bits to take into account for hashing floats.
    This option is read-only for a trained model.

    Parameters
    ----------
    return: int
        Current `mantissa_nbits` value.

    new_mantissa_nbits: int
        New `mantissa_nbits` value, should be non-negative and
        less than or equal to `52`, that is a number of
        mantissa bits in a C++ 64-bit `double`.

    except: ValueError
        The exception is raised when

        - trying to change this option for a model that has already been trained;
        - `new_mantissa_nbits` value is negative or larger than `52`.
