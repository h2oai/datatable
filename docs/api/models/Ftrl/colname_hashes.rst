
.. xattr:: datatable.models.Ftrl.colname_hashes
    :src: src/core/models/py_ftrl.cc Ftrl::get_colname_hashes
    :cvar: doc_models_Ftrl_colname_hashes


    Hashes of the column names used for the hashing trick as
    described in the :class:`Ftrl <dt.models.Ftrl>` class description.

    Parameters
    ----------
    return: List[int]
        A list of the column name hashes.

    See also
    --------
    - :attr:`.colnames` -- the column names of the
      training frame, i.e. the feature names.
