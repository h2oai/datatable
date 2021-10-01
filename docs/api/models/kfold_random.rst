
.. xfunction:: datatable.models.kfold_random
    :src: src/core/models/kfold.cc kfold_random
    :tests: tests/models/test-kfold.py
    :cvar: doc_models_kfold_random
    :signature: kfold_random(nrows, nsplits, seed=None)

    Perform randomized k-fold split of data with `nrows` rows into
    `nsplits` train/test subsets. The dataset itself is not passed to this
    function: it is sufficient to know only the number of rows in order to decide
    how the data should be split.

    The train/test subsets produced by this function will have the following
    properties:

      - all test folds will be of approximately the same size `nrows/nsplits`;
      - all observations have equal ex-ante chance of getting assigned into each fold;
      - the row indices in all train and test folds will be sorted.

    The function uses single-pass parallelized algorithm to construct the
    folds.

    Parameters
    ----------
    nrows: int
        The number of rows in the frame that you want to split.

    nsplits: int
        Number of folds, must be at least `2`, but not larger than `nrows`.

    seed: int
        Seed value for the random number generator used by this function.
        Calling the function several times with the same seed values
        will produce same results each time.

    return: List[Tuple]
        This function returns a list of `nsplits` tuples `(train_rows, test_rows)`,
        where each component of the tuple is a rows selector that can be applied to
        to any frame with `nrows` rows to select the desired folds.


    See Also
    --------
    :func:`kfold()` -- Perform k-fold split.
