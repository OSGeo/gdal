.. _rfc-50:

=======================================================================================
RFC 50: OGR field subtypes
=======================================================================================

Author: Even Rouault

Contact: even dot rouault at spatialys dot com

Status: Adopted, implemented in GDAL 2.0

Summary
-------

This RFC aims at adding the capability of specifying sub-types to OGR
fields, like boolean, 16 bit integers or 32 bit floating point values.
The sub-type of a field definition is an additional attribute that
specifies a hint or a restriction to the main type. The subtype can be
used by applications and drivers that know how to handle it, and can
generally be safely ignored by applications and drivers that do not.

Core changes
------------

Field subtypes
~~~~~~~~~~~~~~

The OGRFieldSubType enumeration is added :

::

   /**
    * List of field subtypes. A subtype represents a hint, a restriction of the
    * main type, that is not strictly necessary to consult.
    * This list is likely to be extended in the
    * future ... avoid coding applications based on the assumption that all
    * field types can be known.
    * Most subtypes only make sense for a restricted set of main types.
    * @since GDAL 2.0
    */
   typedef enum
   {
       /** No subtype. This is the default value */        OFSTNone = 0,
       /** Boolean integer. Only valid for OFTInteger and OFTIntegerList.*/
                                                           OFSTBoolean = 1,
       /** Signed 16-bit integer. Only valid for OFTInteger and OFTIntegerList. */
                                                           OFSTInt16 = 2,
       /** Single precision (32 bit) floatint point. Only valid for OFTReal and OFTRealList. */
                                                           OFSTFloat32 = 3,
                                                           OFSTMaxSubType = 3
   } OGRFieldSubType;

New attributes and methods
~~~~~~~~~~~~~~~~~~~~~~~~~~

-  In OGRFieldDefn class :

::

       OGRFieldSubType     eSubType;

       OGRFieldSubType     GetSubType() { return eSubType; }
       void                SetSubType( OGRFieldSubType eSubTypeIn );
       static const char  *GetFieldSubTypeName( OGRFieldSubType );

OGRFeature::SetField() will check that the passed value is in the
accepted range for boolean and int16 subtypes. If not, it will emit a
warning and correct/clamp the value to fit the subtype.

C API changes
~~~~~~~~~~~~~

Only additions :

::

   OGRFieldSubType CPL_DLL OGR_Fld_GetSubType( OGRFieldDefnH );
   void   CPL_DLL OGR_Fld_SetSubType( OGRFieldDefnH, OGRFieldSubType );
   const char CPL_DLL *OGR_GetFieldSubTypeName( OGRFieldSubType );
   int CPL_DLL OGR_AreTypeSubTypeCompatible( OGRFieldType eType,
                                             OGRFieldSubType eSubType );

Changes in OGR SQL
------------------

-  Subtypes are preserved when a field name (or \*) is specified in the
   list of fields of a SELECT
-  CAST(xxx AS BOOLEAN) and CAST(xxx AS SMALLINT) are now supported.
-  The field list of a SELECT can now accept boolean expressions, such
   as "SELECT x IS NULL, x >= 5 FROM foo"
-  The WHERE clause of a SELECT can now accept boolean fields, such as
   "SELECT \* FROM foo WHERE a_boolean_field"

Changes in drivers
------------------

-  GeoJSON: can read/write OFSTBoolean
-  GML: can read/write OFSTBoolean, OFSTInt16 and OFSTFloat32
-  CSV: can read/write OFSTBoolean (explicitly with CSVT or with
   autodetection), OFSTInt16 and OFSTFloat32 (explicitly with CSVT)
-  PG: can read/write OFSTBoolean, OFSTInt16 and OFSTFloat32
-  PGDump: can write OFSTBoolean, OFSTInt16 and OFSTFloat32
-  GeoPackage: can read/write OFSTBoolean, OFSTInt16 and OFSTFloat32
-  SQLite: can read/write OFSTBoolean and OFSTInt16
-  SQLite dialect: can read/write OFSTBoolean, OFSTInt16 and OFSTFloat32
-  FileGDB: can read/write OFSTInt16 and OFSTFloat32
-  OpenFileGDB: can read OFSTInt16 and OFSTFloat32
-  VRT: 'subtype' property added to be able to handle any subtype.

Changes in utilities
--------------------

-  ogrinfo: the output of ogrinfo is slightly modified in presence of a
   subtype. A field with a non-default subtype will be described as
   "field_type(field_subtype)". For example

::

   Had to open data source read-only.
   INFO: Open of `out.gml'
         using driver `GML' successful.

   Layer name: test
   Geometry: None
   Feature Count: 2
   Layer SRS WKT:
   (unknown)
   short: Integer(Int16) (0.0)
   b: Integer(Boolean) (0.0)
   OGRFeature(test):0
     short (Integer(Int16)) = -32768
     b (Integer(Boolean)) = 1

Changes in SWIG bindings
------------------------

Addition of :

-  ogr.OFSTNone, ogr.OFSTBoolean, ogr.OFSTInt16 and ogr.OFSTFloat32
-  ogr.GetFieldSubTypeName()
-  FieldDefn.GetSubType()
-  FieldDefn.SetSubType()

Compatibility
-------------

This should have no impact on read-only operations done by applications.
Update operations could be impacted if an out-of-range value for the
subtype is written (but such a behavior probably already caused issues,
either ignored or notified by the backend)

Documentation
-------------

All new methods are documented. Driver documentation is updated when
necessary.

Testing
-------

The various aspects of this RFC are tested:

-  core changes
-  OGR SQL changes
-  driver changes

Implementation
--------------

Implementation will be done by Even Rouault
(`Spatialys <http://spatialys.com>`__), and sponsored by
`CartoDB <https://cartodb.com>`__.

The proposed implementation lies in the "ogr_field_subtype" branch of
the
`https://github.com/rouault/gdal2/tree/ogr_field_subtype <https://github.com/rouault/gdal2/tree/ogr_field_subtype>`__
repository.

The list of changes :
`https://github.com/rouault/gdal2/compare/ogr_field_subtype <https://github.com/rouault/gdal2/compare/ogr_field_subtype>`__

Voting history
--------------

+1 JukkaR, TamasS, FrankW and EvenR
