
.. xattr:: datatable.options.fread.log.anonymize
    :src: src/core/csv/reader.cc get_anonymize set_anonymize
    :cvar: doc_options_fread_log_anonymize
    :settable: value

    This option controls logs anonymization that is useful in production
    systems, when reading sensitive data that must not accidentally leak
    into log files or be printed with the error messages.

    Parameters
    ----------
    return: bool
        Current `anonymize` value. Initially, this option is set to `False`.

    value: bool
        New `anonymize` value. If `True`, any snippets of data being read
        that are printed in the log will be first anonymized by converting
        all non-zero digits to `1`, all lowercase letters to `a`,
        all uppercase letters to `A`, and all unicode characters to `U`.
        If `False`, no data anonymization will be performed.
