
.. xmethod:: datatable.Frame.__str__
    :src: src/core/frame/__repr__.cc Frame::m__str__

    Returns a string with the Frame's data formatted as a table, i.e.
    the same representation as displayed when trying to inspect the
    frame from Python console.

    Different aspects of the stringification process can be controlled
    via ``dt.options.display`` options; but under the default settings
    the returned string will be sufficiently small to fit into a
    typical terminal window. If the frame has too many rows/columns,
    then only a small sample near the start+end of the frame will be
    rendered.


    See Also
    --------
    - :meth:`.__repr__()`
