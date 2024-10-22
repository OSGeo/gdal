.. _rfc-103:

===================================================================
RFC 103 add a OGR_SCHEMA open option to selected OGR drivers
===================================================================

=============== =============================================
Author:         Alessandro Pasotti
Contact:        elpaso at itopen.it
Status:         Draft
Created:        2024-10-22
=============== =============================================

Summary
-------

This RFC enables users to specify a OGR_SCHEMA open option in the OGR
drivers that support it.

The new option will be used to override the auto-detected fields types.

Motivation
----------

Several OGR drivers must guess the attribute data type: CSV, GeoJSON, SQLite,
the auto-detected types are not always correct and the user has no way to
override them at opening tim.

For the details please see the discussion attached to the issue: https://github.com/OSGeo/gdal/issues/10943

Implementation
--------------

A new reserved open option named OGR_SCHEMA will be added to the following drivers:

- CSV
- GeoJSON
- SQLite
- GML

The option will be used to specify the schema in the form of a JSON document (or a path to a JSON file).

The schema document will not need to to contain a definition for every field and it will allow partial overrides of the auto-detected fields types.

The structure of the JSON document will be similar to the one produced by the `ogrinfo -json` command
with the notable exception that (for the scope of this RFC) only the type of the fields will be considered:

.. code-block:: json

    {
    "fields": [
        {
        "name": "field1",
        "type": "string"
        },
        {
        "name": "field2",
        "type": "integer",
        "width":11,
        "precision":5
        }
    ]
    }


In case of multi-layered datasets, the schema will be specified as a list of layers:

.. code-block:: json

    {
    "layers":[
        {
        "name": "layer1",
        "fields": [
            {
            "name": "field1",
            "type": "string"
            },
            {
            "name": "field2",
            "type": "integer",
            "width":11,
            "precision":5
            }
        ]
        },
        {
        "name": "layer2",
        "fields": [
            {
            "name": "field1",
            "type": "string"
            },
            {
            "name": "field2",
            "type": "integer",
            "width":11,
            "precision":5
            }
        ]
        }
    ]
    }


The new option will be used by applications such as `ogr2ogr` to override the auto-detected fields types.

A preliminary draft of the implementation can be found at:
https://github.com/elpaso/gdal/commits/enhancement-gh10943-fields-schema-override/


Errors and warnings
-------------------

- If the schema is not a valid JSON document, a critical error will be raised.

- If the schema is a valid JSON document but does not contain the expected fields or it is a no-op
  (does not contain any actionable instruction), a warning will be raised and the schema will be ignored.

- Additional JSON properties will be ignored while parsing the schema.

- If the schema contains a field that is not present in the dataset, a warning will be raised and the field will be ignored.
