datatable.Frame
---------------

.. xclass:: datatable.Frame
    :src: src/core/frame/py_frame.h Frame
    :doc: src/core/frame/py_frame.cc doc_Frame
    :notitle:

    Construction
    ------------

    .. list-table::
        :widths: auto
        :class: api-table

        * - :meth:`Frame(*args, **kws) <datatable.Frame.__init__>`
          - Construct the frame from various Python sources

        * - :func:`dt.fread(src) <datatable.fread>`
          - Read an external file and convert into a Frame.


    Convert into other formats
    --------------------------

    .. list-table::
        :widths: auto
        :class: api-table

        * - :meth:`.to_csv(file) <Frame.to_csv>`
          - Write the frame's data into CSV format.

        * - :meth:`.to_dict() <Frame.to_dict>`
          - Convert the frame into a Python dictionary, by columns.

        * - :meth:`.to_jay(file) <Frame.to_jay>`
          - Store the frame's data into a binary file in Jay format.

        * - :meth:`.to_list() <Frame.to_list>`
          - Return the frame's data as a list of lists, by columns.

        * - :meth:`.to_numpy() <Frame.to_numpy>`
          - Convert the frame into a numpy array.

        * - :meth:`.to_pandas() <Frame.to_pandas>`
          - Convert the frame into a pandas DataFrame.

        * - :meth:`.to_tuples() <Frame.to_tuples>`
          - Return the frame's data as a list of tuples, by rows.


    Properties
    ----------

    .. list-table::
        :widths: auto
        :class: api-table

        * - :data:`.key <Frame.key>`
          - The primary key for the Frame, if any.

        * - :data:`.ltypes <Frame.ltypes>`
          - Logical types (:class:`ltype`s) of all columns.

        * - :data:`.names <Frame.names>`
          - The names of all columns in the frame.

        * - :data:`.ncols <Frame.ncols>`
          - Number of columns in the frame.

        * - :data:`.nrows <Frame.nrows>`
          - Number of rows in the frame.

        * - :data:`.stype <Frame.shape>`
          - A tuple (number of rows, number of columns).

        * - :data:`.source <Frame.source>`
          - Where this frame was loaded from.

        * - :data:`.stype <Frame.stype>`
          - The common :class:`stype` for the entire frame.

        * - :data:`.stypes <Frame.stypes>`
          - Storage types (:class:`stype`s) of all columns.


    Methods
    -------

    .. list-table::
        :widths: auto
        :class: api-table

        * - :meth:`.cbind(*frames) <Frame.cbind>`
          - Append columns of other frames to this frame.

        * - :meth:`.colindex(name) <Frame.colindex>`
          - Find the position of a column in the frame by its name.

        * - :meth:`.copy() <Frame.copy>`
          - Create a copy of the frame.

        * - :meth:`.export_names() <Frame.export_names>`
          - Create python variables for each column of the frame.

        * - :meth:`.head() <Frame.head>`
          - Return the first few rows of the frame.

        * - :meth:`.materialize() <Frame.materialize>`
          - Make sure all frame's data is physically written to memory.

        * - :meth:`.rbind(*frames) <Frame.rbind>`
          - Append other frames at the bottom of the current.

        * - :meth:`.replace(what, with) <Frame.replace>`
          - Search and replace values in the frame.

        * - :meth:`.sort(cols) <Frame.sort>`
          - Sort the frame by the specified columns.

        * - :meth:`.tail() <Frame.tail>`
          - Return the last few rows of the frame.


    .. Auto-methods
    .. ------------

    .. ``countna()``, ``countna1()``, ``max()``, ``max1()``, ``mean()``,
    .. ``mean1()``, ``min()``, ``min1()``, ``mode()``, ``mode1()``,
    .. ``nmodal()``, ``nmodal1()``, ``nunique()``, ``nunique1()``,
    .. ``sd()``, ``sd1()``, ``sum()``, ``sum1()``.


    Special methods
    ---------------

    These methods are not intended to be called manually, instead they provide
    a way for :mod:`datatable` to interoperate with other Python modules or
    builtin functions.

    .. list-table::
        :widths: auto
        :class: api-table

        * - :meth:`.__copy__() <Frame.__copy__>`
          - Used by Python module :mod:`copy`.

        * - :meth:`.__deepcopy__() <Frame.__copy__>`
          - Used by Python module :mod:`copy`.

        * - :meth:`.__delitem__() <Frame.__delitem__>`
          - Method that implements the ``del DT[...]`` call.

        * - :meth:`.__getitem__() <Frame.__getitem__>`
          - Method that implements the ``DT[...]`` call.

        * - :meth:`.__getstate__() <Frame.__getstate__>`
          - Used by Python module :mod:`pickle`.

        * - :meth:`.__init__(...) <Frame.__init__>`
          - The constructor function.

        * - :meth:`.__iter__() <Frame.__iter__>`
          - Used by Python function :func:`iter()`, or when the frame
            is used as a target in a loop.

        * - :meth:`.__len__() <Frame.__len__>`
          - Used by Python function :func:`len`.

        * - :meth:`.__repr__() <Frame.__repr__>`
          - Used by Python function :func:`repr`.

        * - :meth:`.__reversed__() <Frame.__reversed__>`
          - Used by Python function :func:`reversed`.

        * - :meth:`.__setitem__() <Frame.__setitem__>`
          - Method that implements the ``DT[...] = expr`` call.

        * - :meth:`.__setstate__() <Frame.__getstate__>`
          - Used by Python module :mod:`pickle`.

        * - :meth:`.__sizeof__() <Frame.__sizeof__>`
          - Used by :func:`sys.getsizeof`.

        * - :meth:`.__str__() <Frame.__str__>`
          - Used by Python function :class:`str`.

        * - :meth:`._repr_html_() <Frame._repr_html_>`
          - Used to display the frame in `Jupyter Lab`_.

        * - :meth:`._repr_pretty_() <Frame._repr_pretty_>`
          - Used to display the frame in an :mod:`IPython` console.

    .. _`Jupyter Lab`: https://jupyterlab.readthedocs.io/en/latest/


.. toctree::
    :hidden:

    .__init__()      <frame/__init__>
    .__copy__()      <frame/__copy__>
    .__getstate__()  <frame/__getstate__>
    .__len__()       <frame/__len__>
    .__repr__()      <frame/__repr__>
    .__sizeof__()    <frame/__sizeof__>
    .__str__()       <frame/__str__>
    .cbind()         <frame/cbind>
    .colindex()      <frame/colindex>
    .copy()          <frame/copy>
    .export_names()  <frame/export_names>
    .head()          <frame/head>
    .key             <frame/key>
    .keys()          <frame/keys>
    .ltypes          <frame/ltypes>
    .materialize()   <frame/materialize>
    .names           <frame/names>
    .ncols           <frame/ncols>
    .nrows           <frame/nrows>
    .rbind()         <frame/rbind>
    .replace()       <frame/replace>
    .shape           <frame/shape>
    .sort()          <frame/sort>
    .source          <frame/source>
    .stype           <frame/stype>
    .stypes          <frame/stypes>
    .tail()          <frame/tail>
    .to_csv()        <frame/to_csv>
    .to_dict()       <frame/to_dict>
    .to_jay()        <frame/to_jay>
    .to_list()       <frame/to_list>
    .to_numpy()      <frame/to_numpy>
    .to_pandas()     <frame/to_pandas>
    .to_tuples()     <frame/to_tuples>
    .view()          <frame/view>
