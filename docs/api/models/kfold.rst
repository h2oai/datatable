
.. xfunction:: datatable.models.kfold
    :src: src/core/models/kfold.cc kfold
    :tests: tests/models/test-kfold.py
    :cvar: doc_models_kfold
    :signature: kfold(nrows, nsplits)

    Perform k-fold split of data with `nrows` rows into `nsplits` train/test
    subsets. The dataset itself is not passed to this function:
    it is sufficient to know only the number of rows in order to decide
    how the data should be split.

    The range `[0; nrows)` is split into `nsplits` approximately equal parts,
    i.e. folds, and then each `i`-th split will use the `i`-th fold as a
    test part, and all the remaining rows as the train part. Thus, `i`-th split is
    comprised of:

      - train rows: `[0; i*nrows/nsplits) + [(i+1)*nrows/nsplits; nrows)`;
      - test rows: `[i*nrows/nsplits; (i+1)*nrows/nsplits)`.

    where integer division is assumed.

    Parameters
    ----------
    nrows: int
        The number of rows in the frame that is going to be split.

    nsplits: int
        Number of folds, must be at least `2`, but not larger than `nrows`.

    return: List[Tuple]
        This function returns a list of `nsplits` tuples `(train_rows, test_rows)`,
        where each component of the tuple is a rows selector that can be applied
        to any frame with `nrows` rows to select the desired folds.
        Some of these row selectors will be simple python ranges,
        others will be single-column Frame objects.

    See Also
    --------
    :func:`kfold_random()` -- Perform randomized k-fold split.
