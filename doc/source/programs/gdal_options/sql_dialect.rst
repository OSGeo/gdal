.. option:: --dialect <DIALECT>

    SQL dialect.

    By default the native SQL of an RDBMS is used when using
    ``gdal vector sql``. If using ``sql`` as a step of ``gdal vector pipeline``,
    this is only true if the step preceding ``sql`` is ``read``, otherwise the
    :ref:`OGRSQL <ogr_sql_dialect>` dialect is used.

    If a datasource does not support SQL natively, the default is to use the
    ``OGRSQL`` dialect, which can also be specified with any data source.

    The :ref:`sql_sqlite_dialect` dialect can be chosen with the ``SQLITE``
    and ``INDIRECT_SQLITE`` dialect values, and this can be used with any data source.
    Overriding the default dialect may be beneficial because the capabilities of
    the SQL dialects vary.

    Supported dialects can be checked with ``gdal --format``. For example:

    .. code-block::

        $ gdal --format "PostgreSQL"
        [...]
        Supported SQL dialects: NATIVE OGRSQL SQLITE
        [...]

        $ gdal --format "ESRI Shapefile"
        [...]
        Supported SQL dialects: OGRSQL SQLITE
        [...]

