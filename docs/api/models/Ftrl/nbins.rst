
.. xattr:: datatable.models.Ftrl.nbins
    :src: src/core/models/py_ftrl.cc Ftrl::get_nbins Ftrl::set_nbins
    :cvar: doc_models_Ftrl_nbins
    :settable: newnbins


    Number of bins to be used for the hashing trick.
    This option is read-only for a trained model.

    Parameters
    ----------
    return: int
        Current `nbins` value.

    new_nbins: int
        New `nbins` value, should be positive.

    except: ValueError
        The exception is raised when

        - trying to change this option for a model that has already been trained;
        - `new_nbins` value is not positive.
