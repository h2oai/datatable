
.. xfunction:: datatable.init_styles
    :src: src/core/frame/repr/html_styles.cc py_init_styles
    :cvar: doc_dt_init_styles
    :signature: init_styles()

    Inject datatable's stylesheets into the Jupyter notebook. This
    function does nothing when it runs in a normal Python environment
    outside of Jupyter.

    When datatable runs in a Jupyter notebook, it renders its Frames
    as HTML tables. The appearance of these tables is enhanced using
    a custom stylesheet, which must be injected into the notebook at
    any point on the page. This is exactly what this function does.

    Normally, this function is called automatically when datatable
    is imported. However, in some circumstances Jupyter erases these
    stylesheets (for example, if you run ``import datatable`` cell
    twice). In such cases, you may need to call this method manually.
