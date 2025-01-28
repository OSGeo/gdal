.. _rfc-103:

===================================================================
RFC 103 add a OGR_SCHEMA open option to selected OGR drivers
===================================================================

=============== =============================================
Author:         Alessandro Pasotti
Contact:        elpaso at itopen.it
Status:         Implemented
Created:        2024-10-22
=============== =============================================

Summary
-------

This RFC enables users to specify a OGR_SCHEMA open option in the OGR
drivers that support it.

The new option will be used to override the auto-detected fields types and to rename detected fields.

Motivation
----------

Several OGR drivers must guess the attribute data type: CSV, GeoJSON, SQLite,
the auto-detected types are not always correct and the user has no way to
override them at opening time.

A secondary goal is to allow users to rename fields at opening time: some drivers
have limitations regarding field names and have specific laundering rules that
may yield field names that are not ideal for the user.

For the details please see the discussion attached to the issue: https://github.com/OSGeo/gdal/issues/10943

Implementation
--------------

A new reserved open option named OGR_SCHEMA will be added to the following drivers,
chosen because they are the most likely to benefit from the field type override feature:

- CSV
- GeoJSON
- SQLite
- GML

Additional drivers may benefit from the field rename feature while are not usually
affected by the field type guessing issue and may be added later to the supported drivers.

The option will be used to specify the schema in the form of a JSON document (or a path to a JSON file).

The schema document will allow both "Full" and "Patch" modes.
"Patch" mode will be the default and will allow partial overrides of the auto-detected fields types
and names while "Full" mode will produce a layer with only the fields specified in the schema.

The structure of the JSON document has been largely inspired by the one produced by the `ogrinfo -json` command
with the notable exception that (for the scope of this RFC) only the information related to the type and and
name of the fields will be considered.

The JSON schema for the OGR_SCHEMA open option will be as follows:

.. code-block:: json

    {
    "$schema": "https://json-schema.org/draft/2020-12/schema",
    "description": "Schema for OGR_SCHEMA open option",
    "oneOf": [
        {
        "$ref": "#/definitions/dataset"
        }
    ],
    "definitions": {
        "schemaType": {
        "enum": [
            "Patch",
            "Full"
        ]
        },
        "dataset": {
        "type": "object",
        "properties": {
            "layers": {
            "type": "array",
            "description": "The list of layers contained in the schema",
            "items": {
                "$ref": "#/definitions/layer"
            }
            }
        },
        "required": [
            "layers"
        ],
        "additionalProperties": false
        },
        "layer": {
        "type": "object",
        "properties": {
            "name": {
            "description": "The name of the layer",
            "type": "string"
            },
            "schemaType": {
            "description": "The type of schema operation: patch or full",
            "$ref": "#/definitions/schemaType"
            },
            "fields": {
            "description": "The list of field definitions",
            "type": "array",
            "items": {
                "$ref": "#/definitions/field"
            }
            }
        },
        "required": [
            "name",
            "fields"
        ],
        "additionalProperties": false
        },
        "field": {
        "description": "The field definition",
        "additionalProperties": true,
        "type": "object",
        "properties": {
            "name": {
            "type": "string"
            }
        },
        "anyOf": [
            {
            "type": "object",
            "properties": {
                "type": {
                "$ref": "#/definitions/fieldType"
                },
                "subType": {
                "$ref": "#/definitions/fieldSubType"
                },
                "width": {
                "type": "integer"
                },
                "precision": {
                "type": "integer"
                }
            }
            },
            {
            "description": "The new name of the field",
            "newName": {
                "type": "string"
            },
            "required": [
                "newName"
            ]
            }
        ],
        "required": [
            "name"
        ]
        },
        "fieldType": {
        "enum": [
            "Integer",
            "Integer64",
            "Real",
            "String",
            "Binary",
            "IntegerList",
            "Integer64List",
            "RealList",
            "StringList",
            "Date",
            "Time",
            "DateTime"
        ]
        },
        "fieldSubType": {
        "enum": [
            "None",
            "Boolean",
            "Int16",
            "Float32",
            "JSON",
            "UUID"
        ]
        }
    }
    }

Here is an example of a schema document that will be used to override the fields type and the name of a dataset using the default "Patch" mode:

.. code-block:: json

    {
    "layers":[
        {
        "name": "layer1",
        "schemaType": "Full",
        "fields": [
            {
            "name": "field1",
            "type": "String",
            "subType": "JSON"
            },
            {
            "name": "field2",
            "newName": "new_field2"
            }
        ]
        },
        {
        "name": "layer2",
        "schemaType": "Patch",
        "fields": [
          {
            "name": "field1",
            "type": "String",
            "subType": "JSON"
          },
          {
            "name": "field2",
            "newName": "new_field2"
          }
        ]
      }
    ]
    }


The new open option will be useful for applications such as `ogr2ogr` (as a more powerful alternative to the ``-mapFieldType`` switch) to override the auto-detected fields types and to override the auto-detected (and possibly laundered) field names.

A note will be added to the application documentation to explain the new option.


Errors and warnings
-------------------

- If the schema is not a valid JSON document, a critical error will be raised.

- If the schema is a valid JSON document but does not validates against the JSON schema, a critical error will be raised.

- If the schema contains a field that is not present in the dataset, a critical error will be raised.


Related issues and PRs
----------------------

- Candidate implementation (GML): https://github.com/OSGeo/gdal/pull/11334

- PRs for the other drivers:
    - https://github.com/OSGeo/gdal/pull/11479: CSV
    - https://github.com/OSGeo/gdal/pull/11464: GeoJSON
    - https://github.com/OSGeo/gdal/pull/11499: SQLite

Voting history
--------------

+1 from PSC members JukkaR, JavierJS, HowardB and EvenR
