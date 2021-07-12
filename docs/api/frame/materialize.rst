
.. xmethod:: datatable.Frame.materialize
    :src: src/core/frame/py_frame.cc Frame::materialize
    :cvar: doc_Frame_materialize
    :signature: materialize(self, to_memory=False)

    Force all data in the Frame to be laid out physically.

    In datatable, a Frame may contain "virtual" columns, i.e. columns
    whose data is computed on-the-fly. This allows us to have better
    performance for certain types of computations, while also reducing
    the total memory footprint. The use of virtual columns is generally
    transparent to the user, and datatable will materialize them as
    needed.

    However, there could be situations where you might want to materialize
    your Frame explicitly. In particular, materialization will carry out
    all delayed computations and break internal references on other
    Frames' data. Thus, for example if you subset a large frame to create
    a smaller subset, then the new frame will carry an internal reference
    to the original, preventing it from being garbage-collected. However,
    if you materialize the small frame, then the data will be physically
    copied, allowing the original frame's memory to be freed.

    Parameters
    ----------
    to_memory: bool
        If True, then, in addition to de-virtualizing all columns, this
        method will also copy all memory-mapped columns into the RAM.

        When you open a Jay file, the Frame that is created will contain
        memory-mapped columns whose data still resides on disk. Calling
        ``.materialize(to_memory=True)`` will force the data to be loaded
        into the main memory. This may be beneficial if you are concerned
        about the disk speed, or if the file is on a removable drive, or
        if you want to delete the source file.

    return: None
        This operation modifies the frame in-place.
