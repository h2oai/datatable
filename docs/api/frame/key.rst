
.. xattr:: datatable.Frame.key
    :src: src/core/frame/key.cc Frame::get_key Frame::set_key
    :tests: tests/test-keys.py
    :settable: new_key
    :deletable:
    :cvar: doc_Frame_key

    The tuple of column names that are the primary key for this frame.

    If the frame has no primary key, this property returns an empty tuple.

    The primary key columns are always located at the beginning of the
    frame, and therefore the following holds::

        >>> DT.key == DT.names[:len(DT.key)]

    Assigning to this property will make the Frame keyed by the specified
    column(s). The key columns will be moved to the front, and the Frame
    will be sorted. The values in the key columns must be unique.

    Parameters
    ----------
    return: Tuple[str, ...]
        When used as a getter, returns the tuple of names of the primary
        key columns.

    new_key: str | List[str] | Tuple[str, ...] | None
        Specify a column or a list of columns that will become the new
        primary key of the Frame. Object columns cannot be used for a key.
        The values in the key column must be unique; if multiple columns
        are assigned as the key, then their combined (tuple-like) values
        must be unique.

        If `new_key` is `None`, then this is equivalent to deleting the
        key. When the key is deleted, the key columns remain in the frame,
        they merely stop being marked as "key".

    except: ValueError
        Raised when the values in the key column(s) are not unique.

    except: KeyError
        Raised when `new_key` contains a column name that doesn't exist
        in the Frame.

    Examples
    --------
    >>> DT = dt.Frame(A=range(5), B=['one', 'two', 'three', 'four', 'five'])
    >>> DT.key = 'B'
    >>> DT
    B     |     A
    str32 | int32
    ----- + -----
    five  |     4
    four  |     3
    one   |     0
    three |     2
    two   |     1
    [5 rows x 2 columns]
