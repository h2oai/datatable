
.. xattr:: datatable.models.LinearModel.model_type
    :src: src/core/models/py_linearmodel.cc LinearModel::get_model_type LinearModel::set_model_type
    :cvar: doc_models_LinearModel_model_type
    :settable: new_model_type


    A type of the model `LinearModel` should build:

    - `"binomial"` for binomial classification;
    - `"multinomial"` for multinomial classification;
    - `"regression"` for numeric regression;
    - `"auto"` for automatic model type detection based on the target column `stype`.

    This option is read-only for a trained model.

    Parameters
    ----------
    return: str
        Current `model_type` value.

    new_model_type: "binomial" | "multinomial" | "regression" | "auto"
        New `model_type` value.

    except: ValueError
        The exception is raised when trying to change this option
        for a model that has already been trained.
