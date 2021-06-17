

.. xclass:: datatable.Frame
    :src: src/core/frame/py_frame.h Frame
    :doc: src/core/frame/py_frame.cc doc_Frame

    Construction
    ------------

    .. list-table::
        :widths: auto
        :class: api-table

        * - :meth:`Frame(*args, **kws) <dt.Frame.__init__>`
          - Construct the frame from various Python sources.

        * - :func:`dt.fread(src)`
          - Read an external file and convert into a Frame.

        * - :meth:`.copy()`
          - Create a copy of the frame.


    Properties
    ----------

    .. list-table::
        :widths: auto
        :class: api-table

        * - :attr:`.key`
          - The primary key for the Frame, if any.

        * - :attr:`.ltypes`
          - Logical types (:class:`dt.ltype`s) of all columns.

        * - :attr:`.meta`
          - The frame's meta information.

        * - :attr:`.names`
          - The names of all columns in the frame.

        * - :attr:`.ncols`
          - Number of columns in the frame.

        * - :attr:`.nrows`
          - Number of rows in the frame.

        * - :attr:`.stype`
          - A tuple (number of rows, number of columns).

        * - :attr:`.source`
          - Where this frame was loaded from.

        * - :attr:`.stype`
          - The common :class:`dt.stype` for the entire frame.

        * - :attr:`.stypes`
          - Storage types (:class:`dt.stype`s) of all columns.

        * - :attr:`.type`
          - The common type (:class:`dt.Type`) for the entire frame.

        * - :attr:`.types`
          - types (:class:`dt.Type`s) of all columns.


    Frame manipulation
    ------------------

    .. list-table::
        :widths: auto
        :class: api-table

        * - :meth:`frame[i, j, ...] <datatable.Frame.__getitem__>`
          - Primary method for extracting data from a frame.

        * - :meth:`frame[i, j, ...] = values <datatable.Frame.__setitem__>`
          - Update data within the frame.

        * - :meth:`del frame[i, j, ...] <datatable.Frame.__delitem__>`
          - Remove rows/columns/values from the frame.

        * - :meth:`.cbind(*frames)`
          - Append columns of other frames to this frame.

        * - :meth:`.rbind(*frames)`
          - Append other frames at the bottom of the current.

        * - :meth:`.replace(what, with)`
          - Search and replace values in the frame.

        * - :meth:`.sort(cols)`
          - Sort the frame by the specified columns.


    Convert into other formats
    --------------------------

    .. list-table::
        :widths: auto
        :class: api-table

        * - :meth:`.to_arrow()`
          - Convert the frame into an Arrow table.

        * - :meth:`.to_csv(file)`
          - Write the frame's data into CSV format.

        * - :meth:`.to_dict()`
          - Convert the frame into a Python dictionary, by columns.

        * - :meth:`.to_jay(file)`
          - Store the frame's data into a binary file in Jay format.

        * - :meth:`.to_list()`
          - Return the frame's data as a list of lists, by columns.

        * - :meth:`.to_numpy()`
          - Convert the frame into a numpy array.

        * - :meth:`.to_pandas()`
          - Convert the frame into a pandas DataFrame.

        * - :meth:`.to_tuples()`
          - Return the frame's data as a list of tuples, by rows.


    Statistical methods
    -------------------


    .. list-table::
        :widths: auto
        :class: api-table

        * - :meth:`.countna()`
          - Count missing values for each column in the frame.

        * - :meth:`.countna1()`
          - Count missing values for a one-column frame and return it as a scalar.

        * - :meth:`.max()`
          - Find the largest element for each column in the frame.

        * - :meth:`.max1()`
          - Find the largest element for a one-column frame and return it as a scalar.

        * - :meth:`.mean()`
          - Calculate the mean value for each column in the frame.

        * - :meth:`.mean1()`
          - Calculate the mean value for a one-column frame and return it as a scalar.

        * - :meth:`.min()`
          - Find the smallest element for each column in the frame.

        * - :meth:`.min1()`
          - Find the smallest element for a one-column frame and return it as a scalar.

        * - :meth:`.mode()`
          - Find the mode value for each column in the frame.

        * - :meth:`.mode1()`
          - Find the mode value for a one-column frame and return it as a scalar.

        * - :meth:`.nmodal()`
          - Calculate the modal frequency for each column in the frame.

        * - :meth:`.nmodal1()`
          - Calculate the modal frequency for a one-column frame and return it as a scalar.

        * - :meth:`.nunique()`
          - Count the number of unique values for each column in the frame.

        * - :meth:`.nunique1()`
          - Count the number of unique values for a one-column frame and return it as a scalar.

        * - :meth:`.sd()`
          - Calculate the standard deviation for each column in the frame.

        * - :meth:`.sd1()`
          - Calculate the standard deviation for a one-column frame and return it as a scalar.

        * - :meth:`.sum()`
          - Calculate the sum of all values for each column in the frame.

        * - :meth:`.sum1()`
          - Calculate the sum of all values for a one-column column frame and return it as a scalar.


    Miscellaneous methods
    ---------------------

    .. list-table::
        :widths: auto
        :class: api-table

        * - :meth:`.colindex(name)`
          - Find the position of a column in the frame by its name.

        * - :meth:`.export_names()`
          - Create python variables for each column of the frame.

        * - :meth:`.head()`
          - Return the first few rows of the frame.

        * - :meth:`.materialize()`
          - Make sure all frame's data is physically written to memory.

        * - :meth:`.tail()`
          - Return the last few rows of the frame.


    Special methods
    ---------------

    These methods are not intended to be called manually, instead they provide
    a way for :mod:`datatable` to interoperate with other Python modules or
    builtin functions.

    .. list-table::
        :widths: auto
        :class: api-table

        * - :meth:`.__copy__()`
          - Used by Python module :ext-mod:`copy`.

        * - :meth:`.__deepcopy__() <dt.Frame.__copy__>`
          - Used by Python module :ext-mod:`copy`.

        * - :meth:`.__delitem__()`
          - Method that implements the ``del DT[...]`` call.

        * - :meth:`.__getitem__()`
          - Method that implements the ``DT[...]`` call.

        * - :meth:`.__getstate__()`
          - Used by Python module :ext-mod:`pickle`.

        * - :meth:`.__init__(...)`
          - The constructor function.

        * - :meth:`.__iter__()`
          - Used by Python function :ext-func:`iter() <iter>`, or when the frame
            is used as a target in a loop.

        * - :meth:`.__len__()`
          - Used by Python function :ext-func:`len() <len>`.

        * - :meth:`.__repr__()`
          - Used by Python function :ext-func:`repr() <repr>`.

        * - :meth:`.__reversed__()`
          - Used by Python function :ext-func:`reversed() <reversed>`.

        * - :meth:`.__setitem__()`
          - Method that implements the ``DT[...] = expr`` call.

        * - :meth:`.__setstate__() <dt.Frame.__getstate__>`
          - Used by Python module :ext-mod:`pickle`.

        * - :meth:`.__sizeof__()`
          - Used by :ext-func:`sys.getsizeof`.

        * - :meth:`.__str__()`
          - Used by Python function :ext-class:`str`.

        * - ``._repr_html_()``
          - Used to display the frame in `Jupyter Lab`_.

        * - ``._repr_pretty_()``
          - Used to display the frame in an :ext-mod:`IPython` console.

    .. _`Jupyter Lab`: https://jupyterlab.readthedocs.io/en/latest/


.. toctree::
    :hidden:

    .__init__()      <frame/__init__>
    .__copy__()      <frame/__copy__>
    .__delitem__()   <frame/__delitem__>
    .__getitem__()   <frame/__getitem__>
    .__getstate__()  <frame/__getstate__>
    .__iter__()      <frame/__iter__>
    .__len__()       <frame/__len__>
    .__repr__()      <frame/__repr__>
    .__reversed__()  <frame/__reversed__>
    .__setitem__()   <frame/__setitem__>
    .__sizeof__()    <frame/__sizeof__>
    .__str__()       <frame/__str__>
    .cbind()         <frame/cbind>
    .colindex()      <frame/colindex>
    .copy()          <frame/copy>
    .countna()       <frame/countna>
    .countna()       <frame/countna1>
    .export_names()  <frame/export_names>
    .head()          <frame/head>
    .key             <frame/key>
    .keys()          <frame/keys>
    .ltypes          <frame/ltypes>
    .materialize()   <frame/materialize>
    .max()           <frame/max>
    .max1()          <frame/max1>
    .mean            <frame/mean>
    .mean1           <frame/mean1>
    .meta            <frame/meta>
    .min             <frame/min>
    .min1            <frame/min1>
    .mode            <frame/mode>
    .mode1           <frame/mode1>
    .names           <frame/names>
    .ncols           <frame/ncols>
    .nmodal          <frame/nmodal>
    .nmodal1         <frame/nmodal1>
    .nrows           <frame/nrows>
    .rbind()         <frame/rbind>
    .replace()       <frame/replace>
    .shape           <frame/shape>
    .sort()          <frame/sort>
    .source          <frame/source>
    .stype           <frame/stype>
    .stypes          <frame/stypes>
    .tail()          <frame/tail>
    .to_arrow()      <frame/to_arrow>
    .to_csv()        <frame/to_csv>
    .to_dict()       <frame/to_dict>
    .to_jay()        <frame/to_jay>
    .to_list()       <frame/to_list>
    .to_numpy()      <frame/to_numpy>
    .to_pandas()     <frame/to_pandas>
    .to_tuples()     <frame/to_tuples>
    .type            <frame/type>
    .types           <frame/types>
    .view()          <frame/view>
