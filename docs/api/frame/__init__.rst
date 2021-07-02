
.. xmethod:: datatable.Frame.__init__
    :src: src/core/frame/__init__.cc Frame::m__init__
    :tests: tests/frame/test-create.py
    :cvar: doc_Frame___init__
    :signature: __init__(self, _data=None, *, names=None, types=None,
                         type=None, **cols)

    Create a new Frame from a single or multiple sources.

    Argument `_data` (or `**cols`) contains the source data for Frame's
    columns. Column names are either derived from the data, given
    explicitly via the `names` argument, or generated automatically.
    Either way, the constructor ensures that column names are unique,
    non-empty, and do not contain certain special characters (see
    :ref:`name-mangling` for details).

    Parameters
    ----------
    _data: Any
        The first argument to the constructor represents the source from
        which to construct the Frame. If this argument is given, then the
        varkwd arguments `**cols` should not be used.

        This argument can accept a wide range of data types; see the
        "Details" section below.

    **cols: Any
        Sequence of varkwd column initializers. The keys become column
        names, and the values contain column data. Using varkwd arguments
        is equivalent to passing a ``dict`` as the `_data` argument.

        When varkwd initializers are used, the `names` parameter may not
        be given.

    names: List[str|None]
        Explicit list (or tuple) of column names. The number of elements
        in the list must be the same as the number of columns being
        constructed.

        This parameter should not be used when constructing the frame
        from `**cols`.

    types: List[Type] | Dict[str, Type]
        Explicit list (or dict) of column types. The number of elements
        in the list must be the same as the number of columns being
        constructed.

    type: Type | type
        Similar to `types`, but provide a single type that will be used
        for all columns. This option cannot be used together with `types`.

    return: Frame
        A :class:`Frame <datatable.Frame>` object is constructed and
        returned.

    except: ValueError
        The exception is raised if the lengths of `names` or `types`
        lists are different from the number of columns created, or when
        creating several columns and they have incompatible lengths.


    Details
    -------
    The shape of the constructed Frame depends on the type of the source
    argument `_data` (or `**cols`). The argument `_data` and varkwd
    arguments `**cols` are mutually exclusive: they cannot be used at the
    same time. However, it is possible to use neither and construct an
    empty frame::

        >>> dt.Frame()       # empty 0x0 frame
        >>> dt.Frame(None)   # same
        >>> dt.Frame([])     # same

    The varkwd arguments `**cols` can be used to construct a Frame by
    columns. In this case the keys become column names, and the values
    are column initializers. This form is mostly used for convenience;
    it is equivalent to converting `cols` into a `dict` and passing as
    the first argument::

        >>> dt.Frame(A = range(7),
        ...          B = [0.1, 0.3, 0.5, 0.7, None, 1.0, 1.5],
        ...          C = ["red", "orange", "yellow", "green", "blue", "indigo", "violet"])
        >>> # equivalent to
        >>> dt.Frame({"A": range(7),
        ...           "B": [0.1, 0.3, 0.5, 0.7, None, 1.0, 1.5],
        ...           "C": ["red", "orange", "yellow", "green", "blue", "indigo", "violet"]})
           |     A        B  C
           | int32  float64  str32
        -- + -----  -------  ------
         0 |     0      0.1  red
         1 |     1      0.3  orange
         2 |     2      0.5  yellow
         3 |     3      0.7  green
         4 |     4     NA    blue
         5 |     5      1    indigo
         6 |     6      1.5  violet
        [7 rows x 3 columns]

    The argument `_data` accepts a wide range of input types. The
    following list describes possible choices:

    ``List[List | Frame | np.array | pd.DataFrame | pd.Series | range | typed_list]``
        When the source is a non-empty list containing other lists or
        compound objects, then each item will be interpreted as a column
        initializer, and the resulting frame will have as many columns
        as the number of items in the list.

        Each element in the list must produce a single column. Thus,
        it is not allowed to use multi-column `Frame`s, or
        multi-dimensional numpy arrays or pandas `DataFrame`s.

            >>> dt.Frame([[1, 3, 5, 7, 11],
            ...           [12.5, None, -1.1, 3.4, 9.17]])
               |    C0       C1
               | int32  float64
            -- + -----  -------
             0 |     1    12.5
             1 |     3    NA
             2 |     5    -1.1
             3 |     7     3.4
             4 |    11     9.17
            [5 rows x 2 columns]

        Note that unlike `pandas` and `numpy`, we treat a list of lists
        as a list of columns, not a list of rows. If you need to create
        a Frame from a row-oriented store of data, you can use a list of
        dictionaries or a list of tuples as described below.

    ``List[Dict]``
        If the source is a list of `dict` objects, then each element
        in this list is interpreted as a single row. The keys
        in each dictionary are column names, and the values contain
        contents of each individual cell.

        The rows don't have to have the same number or order of
        entries: all missing elements will be filled with NAs::

            >>> dt.Frame([{"A": 3, "B": 7},
            ...           {"A": 0, "B": 11, "C": -1},
            ...           {"C": 5}])
               |     A      B      C
               | int32  int32  int32
            -- + -----  -----  -----
             0 |     3      7     NA
             1 |     0     11     -1
             2 |    NA     NA      5
            [3 rows x 3 columns]

        If the `names` parameter is given, then only the keys given
        in the list of names will be taken into account, all extra
        fields will be discarded.

    ``List[Tuple]``
        If the source is a list of `tuple`s, then each tuple
        represents a single row. The tuples must have the same size,
        otherwise an exception will be raised::

            >>> dt.Frame([(39, "Mary"),
            ...           (17, "Jasmine"),
            ...           (23, "Lily")], names=['age', 'name'])
               |   age  name
               | int32  str32
            -- + -----  -------
             0 |    39  Mary
             1 |    17  Jasmine
             2 |    23  Lily
            [3 rows x 2 columns]

        If the tuples are in fact `namedtuple`s, then the field names
        will be used for the column names in the resulting Frame. No
        check is made whether the named tuples in fact belong to the
        same class.

    ``List[Any]``
        If the list's first element does not match any of the cases
        above, then it is considered a "list of primitives". Such list
        will be parsed as a single column.

        The entries are typically `bool`s, `int`s, `float`s, `str`s,
        or `None`s; numpy scalars are also allowed. If the list has
        elements of heterogeneous types, then we will attempt to
        convert them to the smallest common stype.

        If the list contains only boolean values (or `None`s), then it
        will create a column of type `bool8`.

        If the list contains only integers (or `None`s), then the
        resulting column will be `int8` if all integers are 0 or 1; or
        `int32` if all entries are less than :math:`2^{31}` in magnitude;
        otherwise `int64` if all entries are less than :math:`2^{63}`
        in magnitude; or otherwise `float64`.

        If the list contains floats, then the resulting column will have
        stype `float64`. Both `None` and `math.nan` can be used to input
        NA values.

        Finally, if the list contains strings then the column produced
        will have stype `str32` if the total size of the character is
        less than 2Gb, or `str64` otherwise.

    ``typed_list``
        A typed list can be created by taking a regular list and
        dividing it by an stype. It behaves similarly to a simple
        list of primitives, except that it is parsed into the specific
        stype.

            >>> dt.Frame([1.5, 2.0, 3.87] / dt.float32).type
            Type.float32

    ``Dict[str, Any]``
        The keys are column names, and values can be any objects from
        which a single-column frame can be constructed: list, range,
        np.array, single-column Frame, pandas series, etc.

        Constructing a frame from a dictionary `d` is exactly equivalent
        to calling `dt.Frame(list(d.values()), names=list(d.keys()))`.

    ``range``
        Same as if the range was expanded into a list of integers,
        except that the column created from a range is virtual and
        its creation time is nearly instant regardless of the range's
        length.

    ``Frame``
        If the argument is a :class:`Frame <datatable.Frame>`, then
        a shallow copy of that frame will be created, same as
        :meth:`.copy()`.

    ``str``
        If the source is a simple string, then the frame is created
        by :func:`fread <datatable.fread>`-ing this string.
        In particular, if the string contains the name of a file, the
        data will be loaded from that file; if it is a URL, the data
        will be downloaded and parsed from that URL. Lastly, the
        string may simply contain a table of data.

            >>> DT1 = dt.Frame("train.csv")
            >>> dt.Frame("""
            ...    Name    Age
            ...    Mary     39
            ...    Jasmine  17
            ...    Lily     23 """)
               | Name       Age
               | str32    int32
            -- + -------  -----
             0 | Mary        39
             1 | Jasmine     17
             2 | Lily        NA
            [3 rows x 2 columns]

    ``pd.DataFrame | pd.Series``
        A pandas DataFrame (Series) will be converted into a datatable
        Frame. Column names will be preserved.

        Column types will generally be the same, assuming they have a
        corresponding stype in datatable. If not, the column will be
        converted. For example, pandas date/time column will get converted
        into string, while `float16` will be converted into `float32`.

        If a pandas frame has an object column, we will attempt to refine
        it into a more specific stype. In particular, we can detect a
        string or boolean column stored as object in pandas.

    ``np.array``
        A numpy array will get converted into a Frame of the same shape
        (provided that it is 2- or less- dimensional) and the same type.

        If possible, we will create a Frame without copying the data
        (however, this is subject to numpy's approval). The resulting
        frame will have a copy-on-write semantics.

    ``pyarrow.Table``
        An arrow table will be converted into a datatable Frame, preserving
        column names and types.

        If the arrow table has columns of types not supported by datatable
        (for example lists or structs), an exception will be raised.

    ``None``
        When the source is not given at all, then a 0x0 frame will be
        created; unless a `names` parameter is provided, in which
        case the resulting frame will have 0 rows but as many columns
        as given in the `names` list.
