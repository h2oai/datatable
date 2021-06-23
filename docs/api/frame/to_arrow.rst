
.. xmethod:: datatable.Frame.to_arrow
    :src: src/core/frame/to_arrow.cc Frame::to_arrow
    :tests: tests/frame/test-to-arrow.py
    :cvar: doc_Frame_to_arrow
    :signature: to_arrow(self)

    Convert this frame into a ``pyarrow.Table`` object. The ``pyarrow``
    module must be installed.

    The conversion is multi-threaded and done in C++, but it does
    involve creating a copy of the data, except for the cases when the
    data was originally imported from Arrow. This is caused by differences
    in the data storage formats of datatable and Arrow.

    Parameters
    ----------
    return: pyarrow.Table
        A ``Table`` object is always returned, even if the source is a
        single-column datatable Frame.

    except: ImportError
        If the `pyarrow` module is not installed.
