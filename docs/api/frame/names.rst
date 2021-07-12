
.. xattr:: datatable.Frame.names
    :src: src/core/frame/names.cc Frame::get_names Frame::set_names
    :settable: new_names
    :deletable:
    :cvar: doc_Frame_names

    The tuple of names of all columns in the frame.

    Each name is a non-empty string not containing any ASCII control
    characters, and jointly the names are unique within the frame.

    This property is also assignable: setting ``DT.names`` has the effect
    of renaming the frame's columns without changing their order. When
    renaming, the length of the new list of names must be the same as the
    number of columns in the frame. It is also possible to rename just a
    few of the columns by assigning a dictionary ``{oldname: newname}``.
    Any column not listed in the dictionary will keep its old name.

    When setting new column names, we will verify whether they satisfy
    the requirements mentioned above. If not, a warning will be emitted
    and the names will be automatically :ref:`mangled <name-mangling>`.

    Parameters
    ----------
    return: Tuple[str, ...]
        When used in getter form, this property returns the names of all
        frame's columns, as a tuple. The length of the tuple is equal to
        the number of columns in the frame, :attr:`.ncols`.

    new_names: List[str?] | Tuple[str?, ...] | Dict[str, str?] | None
        The most common form is to assign the list or tuple of new
        column names. The length of the new list must be equal to the
        number of columns in the frame. Some (or all) elements in the list
        may be ``None``'s, indicating that that column should have
        an auto-generated name.

        If ``new_names`` is a dictionary, then it provides a mapping from
        old to new column names. The dictionary may contain less entries
        than the number of columns in the frame: the columns not mentioned
        in the dictionary will retain their names.

        Setting the ``.names`` to ``None`` is equivalent to using the
        ``del`` keyword: the names will be set to their default values,
        which are usually ``C0, C1, ...``.


    except: ValueError | KeyError
        .. list-table::
            :widths: auto
            :class: api-table

            * - :exc:`dt.exceptions.ValueError`
              - raised If the length of the list/tuple `new_names` does not match the
                number of columns in the frame.

            * - :exc:`dt.exceptions.KeyError`
              - raised If `new_names` is a dictionary containing entries that do not
                match any of the existing columns in the frame.

    Examples
    --------
    >>> DT = dt.Frame([[1], [2], [3]])
    >>> DT.names = ['A', 'B', 'C']
    >>> DT.names
    ('A', 'B', 'C')
    >>> DT.names = {'B': 'middle'}
    >>> DT.names
    ('A', 'middle', 'C')
    >>> del DT.names
    >>> DT.names
    ('C0', 'C1', 'C2)
