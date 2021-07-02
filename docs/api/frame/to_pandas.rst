
.. xmethod:: datatable.Frame.to_pandas
    :src: src/core/frame/to_pandas.cc Frame::to_pandas
    :tests: tests/frame/test-to-pandas.py
    :cvar: doc_Frame_to_pandas
    :signature: to_pandas(self)

    Convert this frame into a pandas DataFrame.

    If the frame being converted has one or more key columns, those
    columns will become the index in the pandas DataFrame.

    Parameters
    ----------
    return: pandas.DataFrame
        Pandas dataframe of shape ``(nrows, ncols-nkeys)``.

    except: ImportError
        If the `pandas` module is not installed.
