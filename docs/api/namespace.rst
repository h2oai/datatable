
.. xclass:: datatable.Namespace
    :src: src/core/expr/namespace.h Namespace
    :cvar: doc_Namespace

    A namespace is an environment that provides lazy access to columns of
    a frame when performing computations within
    :meth:`DT[i,j,...] <dt.Frame.__getitem__>`.

    This class should not be instantiated directly, instead use the
    singleton instances :data:`f <datatable.f>` and :data:`g <datatable.g>`
    exported from the :mod:`datatable` module.


    Special methods
    ---------------

    .. list-table::
        :widths: auto
        :class: api-table

        * - :meth:`.__getattribute__(attr)`
          - Access columns as attributes.

        * - :meth:`.__getitem__(item)`
          - Access columns by their names / indices.


.. toctree::
    :hidden:

    .__getitem__()        <namespace/__getitem__>
    .__getattribute__()   <namespace/__getattribute__>
