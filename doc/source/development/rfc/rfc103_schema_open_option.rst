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

The schema document will allow both "Full" and "Patch" modes.
"Patch" mode will be the default and will allow partial overrides of the auto-detected fields types
while "Full" mode will replace all the auto-detected types with the ones specified in the schema.

The structure of the JSON document will be similar to the one produced by the `ogrinfo -json` command
with the notable exception that (for the scope of this RFC) only the information related to the type of
the fields will be considered.

The JSON schema for the OGR_SCHEMA open option will be as follows:

.. code-block:: json

    {
        "$schema": "http://json-schema.org/draft-07/schema#",
        "description": "Schema for OGR_SCHEMA open option",

        "oneOf": [
            {
                "$ref": "#/definitions/dataset"
            }
        ],

        "definitions": {
            "dataset": {
                "type": "object",
                "properties": {
                    "layers": {
                        "type": "array",
                        "items": {
                            "$ref": "#/definitions/layer"
                        }
                    }
                },
                "required": ["layers"],
                "additionalProperties": false
            },

            "schema_type": {
                "enum": ["Patch", "Full"],
                "default": "Patch"
            },

            "layer": {
                "type": "object",
                "properties": {
                    "name": {
                        "type": "string"
                    },
                    "schema_type": {
                        "$ref": "#/definitions/schema_type"
                    },
                    "fields": {
                        "type": "array",
                        "items": {
                            "$ref": "#/definitions/field"
                        }
                    }
                },
                "required": ["name", "fields"],
                "additionalProperties": false
            },

            "field": {
                "type": "object",
                "properties": {
                    "name": {
                        "type": "string"
                    },
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
                },
                "required": ["name", "type"],
                "additionalProperties": false
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
                "enum": ["None", "Boolean", "Int16", "Float32", "JSON", "UUID"]
            }
        }
    }

Here is an example of a schema document that will be used to override the fields types of a dataset using the default "Patch" mode:

.. code-block:: json

    {
    "fields": [
        {
        "name": "field1",
        "type": "String",
        "subType": "JSON"
        },
        {
        "name": "field2",
        "type": "Integer",
        "width":11,
        "precision":5
        }
    ]
    }


In case of multi-layered datasets, the schema will be specified as a list of layers, each with its own fields definition and Patch/Full mode:

.. code-block:: json

    {
    "layers":[
        {
        "name": "layer1",
        "schema_type": "Full",
        "fields": [
            {
            "name": "field1",
            "type": "string",
            "subType": "JSON"
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
        "schema_type": "Patch",
        "fields": [
          {
            "name": "field1",
            "type": "string",
            "subType": "JSON"
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

- If the schema is a valid JSON document but does not validates against the JSON schema, a critical error will be raised.

- If the schema contains a field that is not present in the dataset, a critical error will be raised.
