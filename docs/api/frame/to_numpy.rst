
.. xmethod:: datatable.Frame.to_numpy
    :src: src/core/frame/to_numpy.cc Frame::to_numpy
    :tests: tests/frame/test-to-numpy.py
    :cvar: doc_Frame_to_numpy
    :signature: to_numpy(self, type=None, column=None)

    Convert frame into a 2D numpy array, optionally forcing it into the
    specified type.

    In a limited set of circumstances the returned numpy array will be
    created as a data view, avoiding copying the data. This happens if
    all of these conditions are met:

      - the frame has only 1 column, which is not virtual;
      - the column's type is not string;
      - the `type` argument was not used.

    In all other cases the returned numpy array will have a copy of the
    frame's data. If the frame has multiple columns of different stypes,
    then the values will be upcasted into the smallest common stype.

    If the frame has any NA values, then the returned numpy array will
    be an instance of `numpy.ma.masked_array`.


    Parameters
    ----------
    type: Type | <type-like>
        Cast frame into this type before converting it into a numpy
        array. Here "type-like" can be any value that is acceptable to the
        :class:`dt.Type` constructor.

    column: int
        Convert a single column instead of the whole frame. This column index
        can be negative, indicating columns counted from the end of the frame.

    return: numpy.ndarray | numpy.ma.core.MaskedArray
        The returned array will be 2-dimensional with the same :attr:`.shape`
        as the original frame. However, if the option `column` was used,
        then the returned array will be 1-dimensional with the length of
        :attr:`.nrows`.

        A masked array is returned if the frame contains NA values but the
        corresponding numpy array does not support NAs.

    except: ImportError
        If the ``numpy`` module is not installed.
