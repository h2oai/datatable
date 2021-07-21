
.. xattr:: datatable.models.Ftrl.model_type
    :src: src/core/models/py_ftrl.cc Ftrl::get_model_type Ftrl::set_model_type
    :cvar: doc_models_Ftrl_model_type
    :settable: new_model_type


    A type of the model `Ftrl` should build:

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
        The exception is raised when

        - trying to change this option for a model that has already been trained;
        - `new_model_type` value is not one of the following: `"binomial"`,
          `"multinomial"`, `"regression"` or `"auto"`.

    See also
    --------
    - :attr:`.model_type_trained` -- the model type `Ftrl` has build.
