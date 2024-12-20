.. option:: -oo <NAME>=<VALUE>

    Dataset open option (format specific).

    If a driver supports the ``OGR_SCHEMA`` open option, it can be used to
    partially or completely override the auto-detected schema (i.e. which
    fields are read, with which types, subtypes, length, precision etc.)
    of the dataset.

    The value of this option is a JSON string or a path to a JSON file that
    complies with the `OGR_SCHEMA open option schema definition <https://raw.githubusercontent.com/OSGeo/gdal/refs/heads/master/ogr/data/ogr_fields_override.schema.json>`_

