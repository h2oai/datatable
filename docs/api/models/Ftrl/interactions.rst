
.. xattr:: datatable.models.Ftrl.interactions
    :src: src/core/models/py_ftrl.cc Ftrl::get_interactions Ftrl::set_interactions
    :cvar: doc_models_Ftrl_interactions
    :settable: new_interactions


    The feature interactions to be used for model training. This option is
    read-only for a trained model.

    Parameters
    ----------
    return: Tuple
        Current `interactions` value.

    new_interactions: List[List[str] | Tuple[str]] | Tuple[List[str] | Tuple[str]]
        New `interactions` value. Each particular interaction
        should be a list or a tuple of feature names, where each feature
        name is a column name from the training frame.

    except: ValueError
        The exception is raised when

        - trying to change this option for a model that has already been trained;
        - one of the interactions has zero features.
