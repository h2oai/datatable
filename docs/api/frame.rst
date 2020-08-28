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


    Special methods
    ---------------

    These methods are not intended to be called manually, instead they provide
    a way for :mod:`datatable` to interoperate with other Python modules or
    builtin functions.

    .. list-table::
        :widths: auto
        :class: api-table

        * - :meth:`__copy__() <Frame.__copy__>`
          - Used by Python module :mod:`copy`.

        * - :meth:`__deepcopy__() <Frame.__copy__>`
          - Used by Python module :mod:`copy`.

        * - :meth:`__delitem__()`
          - Method that implements the ``del DT[...]`` call.

        * - :meth:`__getitem__()`
          - Method that implements the ``DT[...]`` call.

        * - :meth:`__getstate__() <Frame.__getstate__>`
          - Used by Python module :mod:`pickle`.

        * - :meth:`__init__(...) <Frame.__init__>`
          - The constructor function.

        * - :meth:`__iter__() <Frame.__iter__>`
          - Used by Python function :func:`iter()`, or when the frame
            is used as a target in a loop.

        * - :meth:`.__len__()`
          - Used by Python function :func:`len`.

        * - :meth:`.__repr__()`
          - Used by Python function :func:`repr`.

        * - :meth:`.__reversed__()`
          - Used by Python function :func:`reversed`.

        * - :meth:`__setitem__()`
          - Method that implements the ``DT[...] = expr`` call.

        * - :meth:`__setstate__() <Frame.__getstate__>`
          - Used by Python module :mod:`pickle`.

        * - :meth:`.__sizeof__()`
          - Used by :func:`sys.getsizeof`.

        * - :meth:`.__str__()`
          - Used by Python function :class:`str`.

        * - :meth:`._repr_html_()`
          - Used to display the frame in `Jupyter Lab`_.

        * - :meth:`._repr_pretty_()`
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
    .replace()       <frame/replace>
    .shape           <frame/shape>
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
