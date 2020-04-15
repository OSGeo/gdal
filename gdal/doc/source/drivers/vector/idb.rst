.. _vector.idb:

IDB
===

.. shortname:: IDB

.. build_dependencies:: Informix DataBlade

This driver implements support for access to spatial tables in IBM
Informix extended with the DataBlade spatial module.

When opening a database, its name should be specified in the form

::

   "IDB:dbname={dbname} server={host} user={login} pass={pass} table={layertable}".

The IDB: prefix is used to mark the name as a IDB connection string.

If the *geometry_columns* table exists, then all listed tables and named
views will be treated as OGR layers. Otherwise all regular user tables
will be treated as layers.

Regular (non-spatial) tables can be accessed, and will return features
with attributes, but not geometry. If the table has a "st_*" field, it
will be treated as a spatial table. The type of the field is inspected
to determine how to read it.

Driver supports automatic FID detection.

Driver capabilities
-------------------

.. supports_create::

.. supports_georeferencing::

.. supports_virtualio::

Environment variables
---------------------

-  **INFORMIXDIR**: It should be set to Informix client SDK install dir
-  **INFORMIXSERVER**: Default Informix server name
-  **DB_LOCALE**: Locale of Informix database
-  **CLIENT_LOCALE**: Client locale
-  **IDB_OGR_FID**: Set name of primary key instead of 'ogc_fid'.

For more information about Informix variables read documentation of
Informix Client SDK

Example
-------

This example shows using ogrinfo to list Informix DataBlade layers on a
different host.

::

   ogrinfo -ro IDB:"server=demo_on user=informix dbname=frames"
