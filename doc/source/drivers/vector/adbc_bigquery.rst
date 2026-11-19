.. _vector.adbc_bigquery:

ADBC BigQuery -- Google BigQuery through Arrow Database Connectivity
====================================================================

.. versionadded:: 3.12

.. shortname:: ADBC

.. build_dependencies:: adbc-driver-manager

See :ref:`vector.adbc` for generalities on the ADBC driver.

The ADBC driver supports read and write operations to BigQuery tables,

Runtime requirements
--------------------

`adbc_driver_bigquery <https://github.com/apache/arrow-adbc/tree/main/c/driver/bigquery>`__
must be available in a path from which shared libraries can be loaded (for example
by setting the ``LD_LIBRARY_PATH`` environment variable on Linux,
``DYLD_LIBRARY_PATH`` on MacOS X or ``PATH`` on Windows). Note that
compilation of that driver requires a Go compiler.

Dataset connection string
-------------------------

The ``ADBC:`` connection string must be provided, as well with the
:oo:`BIGQUERY_PROJECT_ID` open option and at least one of
:oo:`BIGQUERY_JSON_CREDENTIAL_STRING` or :oo:`BIGQUERY_JSON_CREDENTIAL_FILE`.

Dataset open options
--------------------

|about-open-options|
The following open options are supported:

-  .. oo:: ADBC_DRIVER
      :choices: <string>

      ADBC driver name. Must be set to ``adbc_driver_bigquery`` for BigQuery
      support. It is not necessary to explicitly set it, if at least one of
      the below open option starting with ``BIGQUERY\_``  is set.

- .. oo:: SQL
      :choices: <string>

      A SQL-like statement recognized by the driver, used to create a result
      layer from the dataset.

- .. oo:: BIGQUERY_PROJECT_ID
      :choices: <string>

      Required.
      Name of the Google `project ID <https://support.google.com/googleapi/answer/7014113?hl=en#>`__
      This can also be set as a configuration option.

- .. oo:: BIGQUERY_DATASET_ID
      :choices: <string>

      Optional if providing the SQL open option. Required otherwise.
      Name of the BigQuery `dataset ID <https://cloud.google.com/bigquery/docs/datasets-intro?hl=en>`__
      This can also be set as a configuration option.

- .. oo:: BIGQUERY_JSON_CREDENTIAL_STRING
      :choices: <string>

      JSON string containing Google credentials. This is typically the content
      of the :file:`$HOME/.config/gcloud/application_default_credentials.json` file
      generated with ``gcloud auth application-default login``.
      Mutually exclusive with BIGQUERY_JSON_CREDENTIAL_FILE. One of them required.
      This can also be set as a configuration option.

- .. oo:: BIGQUERY_JSON_CREDENTIAL_FILE
      :choices: <string>

      Filename containing Google credentials. It may be created with
      ``gcloud auth application-default login``, which will generate a
      :file:`$HOME/.config/gcloud/application_default_credentials.json` file.
      Mutually exclusive with BIGQUERY_JSON_CREDENTIAL_STRING. One of them required.
      This can also be set as a configuration option.

- .. oo:: ADBC_OPTION_xxx
      :choices: <string>

      Custom ADBC option to pass to AdbcDatabaseSetOption(). Options are
      driver specific.
      For example ``ADBC_OPTION_uri=some_value`` to pass the ``uri`` option.

- .. oo:: PRELUDE_STATEMENTS
      :choices: <string>

      SQL-like statement recognized by the driver that must be executed before
      discovering layers. Can be repeated multiple times.
      For example ``PRELUDE_STATEMENTS=INSTALL spatial`` and
      ``PRELUDE_STATEMENTS=LOAD spatial`` to load DuckDB spatial extension.

Write support
-------------

The following operations are supported (provided appropriate permissions are set
on server side):

- creating a new table
- deleting a table
- adding a new field to a table
- adding a new feature to a table
- updating an existing feature in a table
- deleting an existing feature from a table.

Note that feature insertion is done one feature at a time, so it is only appropriate
for creation of a few features. Use BigQuery bulk import capabilities when
importing a substantial number of features.

Layer creation options
----------------------

|about-layer-creation-options|
The following layer creation options are available:

-  .. lco:: FID
      :default: ogc_fid

      Column name to use for the OGR FID (integer primary key in the table).
      Can be set to empty to ask for not creating a primary key (in which case
      operations like updating or deleting existing features will not be available
      through :cpp:func:`OGRLayer::SetFeature` and :cpp:func:`OGRLayer::DeleteFeature`)

Examples
--------

.. example::
   :title: Listing available tables from a BigQuery dataset

   .. code-block:: bash
 
       gdal vector info ADBC: \
           --oo BIGQUERY_PROJECT_ID=my_project_id \
           --oo BIGQUERY_DATASET_ID=my_dataset \
           --oo BIGQUERY_JSON_CREDENTIAL_FILE=$HOME/.config/gcloud/application_default_credentials.json

.. example::
   :title: Displaying the results of a SQL statement

   .. code-block:: bash
 
       gdal vector info ADBC: --features \
           --oo BIGQUERY_PROJECT_ID=my_project_id \
           --oo BIGQUERY_JSON_CREDENTIAL_FILE=$HOME/.config/gcloud/application_default_credentials.json
           --oo "SQL=SELECT * FROM my_dataset.my_table ORDER BY area DESC LIMIT 10"

.. example::
   :title: Converting a subset of a BigQuery table to a GeoPackage file

   .. code-block:: bash

      gdal vector sql --input=ADBC: --output=out.gpkg \
          "--sql=SELECT * FROM my_dataset.my_table WHERE country = 'France'" \
          --oo BIGQUERY_PROJECT_ID=my_project_id \
          --oo BIGQUERY_JSON_CREDENTIAL_FILE=$HOME/.config/gcloud/application_default_credentials.json

.. example:: 
   :title: Converting a GeoPackage file to a BigQuery table (provided the input table(s) use geographic coordinates):

   .. code-block:: bash

      gdal vector convert --update --input=my.gpkg --output=ADBC: \
          --output-oo BIGQUERY_PROJECT_ID=my_project_id \
          --output-oo BIGQUERY_DATASET_ID=my_dataset \
          --output-oo BIGQUERY_JSON_CREDENTIAL_FILE=$HOME/.config/gcloud/application_default_credentials.json

.. example::
   :title: Converting a shapefile file to a BigQuery table, doing prior reprojection to WGS 84 (EPSG:4326), and renaming the layer

   .. code-block:: bash

      gdal vector pipeline read my.gpkg ! \
          reproject --dst-crs=EPSG:4326 ! \
          write ADBC: \
              --update \
              --output-oo BIGQUERY_PROJECT_ID=my_project_id \
              --output-oo BIGQUERY_DATASET_ID=my_dataset \
              --output-oo BIGQUERY_JSON_CREDENTIAL_FILE=$HOME/.config/gcloud/application_default_credentials.json \
              --output-layer newlayer

See Also
--------

* `adbc_driver_bigquery <https://github.com/apache/arrow-adbc/tree/main/c/driver/bigquery>`__
