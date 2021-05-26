.. _rfc-53:

=======================================================================================
RFC 53: OGR not-null constraints and default values
=======================================================================================

Authors: Even Rouault

Contact: even dot rouault at spatialys.com

Status: Adopted, implemented in GDAL 2.0

Summary
-------

This RFC addresses handling of NOT NULL constraints and DEFAULT values
for OGR fields. NOT NULL constraints are useful to maintain basic data
integrity and are handled by most (all?) drivers that have SQL
capabilities. Default fields values may be used complementary or
independently of NOT NULL constraints to specify the value a field must
be assigned to if it is not provided when inserting a feature into the
layer.

NOT NULL constraint
-------------------

Up to now, OGR fields did not have NOT NULL constraints, i.e. fields in
layers/tables were created with the possibility for a field of a
feature/record to be unset (i.e. having a NULL value). This will still
be the default, i.e. fields are assumed to be nullable. The OGRFieldDefn
class is extended with a boolean attribute bNullable that defaults to
TRUE and can be set to FALSE to express a NOT NULL constraint (bNullable
has been preferred over bNotNullable to avoid confusion with double
negation). Drivers that can translate NOT NULL constraints in their
storage will use that attribute to determine if the field definition
must include a NOT NULL constraint. When opening a datasource, their
metadata will be inspected to set the nullable attribute properly, so
that round-tripping works.

The following methods are added to the OGRFieldDefn class

::

       int                 IsNullable() const { return bNullable; }

   /**
    * \brief Return whether this field can receive null values.
    *
    * By default, fields are nullable.
    *
    * Even if this method returns FALSE (i.e not-nullable field), it doesn't mean
    * that OGRFeature::IsFieldSet() will necessary return TRUE, as fields can be
    * temporary unset and null/not-null validation is usually done when
    * OGRLayer::CreateFeature()/SetFeature() is called.
    *
    * This method is the same as the C function OGR_Fld_IsNullable().
    *
    * @return TRUE if the field is authorized to be null.
    * @since GDAL 2.0
    */

       void                SetNullable( int bNullableIn ) { bNullable = bNullableIn; }

   /**
    * \brief Set whether this field can receive null values.
    *
    * By default, fields are nullable, so this method is generally called with FALSE
    * to set a not-null constraint.
    *
    * Drivers that support writing not-null constraint will advertise the
    * GDAL_DCAP_NOTNULL_FIELDS driver metadata item.
    *
    * This method is the same as the C function OGR_Fld_SetNullable().
    *
    * @param bNullableIn FALSE if the field must have a not-null constraint.
    * @since GDAL 2.0
    */

As this holds true for geometry fields, those 2 methods are also add to
the OGRGeometryFieldDefn class.

Note that adding a field with a NOT NULL constraint on a non-empty layer
is generally impossible, unless a DEFAULT value is associated with it.

The following method is added to the OGRFeature class :

::

       int                 Validate( int nValidateFlags, int bEmitError );

   /**
    * \brief Validate that a feature meets constraints of its schema.
    *
    * The scope of test is specified with the nValidateFlags parameter.
    *
    * Regarding OGR_F_VAL_WIDTH, the test is done assuming the string width must
    * be interpreted as the number of UTF-8 characters. Some drivers might interpret
    * the width as the number of bytes instead. So this test is rather conservative
    * (if it fails, then it will fail for all interpretations).
    *
    * This method is the same as the C function OGR_F_Validate().
    *
    * @param nValidateFlags OGR_F_VAL_ALL or combination of OGR_F_VAL_NULL,
    *                       OGR_F_VAL_GEOM_TYPE, OGR_F_VAL_WIDTH and OGR_F_VAL_ALLOW_NULL_WHEN_DEFAULT
    *                       with '|' operator
    * @param bEmitError TRUE if a CPLError() must be emitted when a check fails
    * @return TRUE if all enabled validation tests pass.
    * @since GDAL 2.0
    */

where nValidateFlags is a combination of :

::

   /** Validate that fields respect not-null constraints.
    * Used by OGR_F_Validate().
    * @since GDAL 2.0
    */
   #define OGR_F_VAL_NULL           0x00000001

   /** Validate that geometries respect geometry column type.
    * Used by OGR_F_Validate().
    * @since GDAL 2.0
    */
   #define OGR_F_VAL_GEOM_TYPE      0x00000002

   /** Validate that (string) fields respect field width.
    * Used by OGR_F_Validate().
    * @since GDAL 2.0
    */
   #define OGR_F_VAL_WIDTH          0x00000004

   /** Allow fields that are null when there's an associated default value.
    * This can be used for drivers where the low-level layers will automatically set the
    * field value to the associated default value.
    * This flag only makes sense if OGR_F_VAL_NULL is set too.
    * Used by OGR_F_Validate().
    * @since GDAL 2.0
    */
   #define OGR_F_VAL_ALLOW_NULL_WHEN_DEFAULT       0x00000008

   /** Enable all validation tests.
    * Used by OGR_F_Validate().
    * @since GDAL 2.0
    */
   #define OGR_F_VAL_ALL            0xFFFFFFFF

Validation of NOT NULL constraints is generally let to the driver
low-level layer, so OGRFeature::Validate() is only useful on a few cases
(one of such case is the GML driver)

A new flag ALTER_NULLABLE_FLAG = 0x8 is added to be passed to
OGRLayer::AlterFieldDefn() so as to set or drop NULL / NOT-NULL
constraints (for drivers that implement it).

Drivers that handle NOT NULL constraint for regular attribute fields
should advertise the new GDAL_DCAP_NOTNULL_FIELDS and/or
GDAL_DCAP_NOTNULL_GEOMFIELDS driver metadata items.

Drivers that do not implement the OGRLayer::CreateGeomField() interface
(i.e. the ones that support single geometry field), but can create a
layer with a NOT NULL constraint on the geometry field can expose a
GEOMETRY_NULLABLE=YES/NO layer creation option.

Note: due to the way they are commonly written, the CreateField()
implementations of drivers that do not support NOT NULL constraint will
generally copy the value of the nullable flag, which may be a bit
misleading if querying the field definition just after having adding it
(the same holds true for width/precision as well).

All above methods are mapped into the C API :

::

     int    CPL_DLL OGR_Fld_IsNullable( OGRFieldDefnH hDefn );
     void   CPL_DLL OGR_Fld_SetNullable( OGRFieldDefnH hDefn, int );

   int                  CPL_DLL OGR_GFld_IsNullable( OGRGeomFieldDefnH hDefn );
   void                 CPL_DLL OGR_GFld_SetNullable( OGRGeomFieldDefnH hDefn, int );

   int    CPL_DLL OGR_F_Validate( OGRFeatureH, int nValidateFlags, int bEmitError );

Default field values
--------------------

Fields with NOT NULL constraints are sometimes accompanied with a
DEFAULT clause so as to be able to create a new feature without filling
all fields, while maintaining integrity. DEFAULT values can also be set
on nullable fields but for reasons exposed later it is recommended to
avoid that.

Drivers that can translate DEFAULT values in their storage will use that
attribute to determine if the field definition must include a DEFAULT
value. When opening a datasource, their metadata will be inspected to
set the default value attribute properly, so that round-tripping works.

There was an embryonic support for default values in GDAL 1.X but that
never got implemented beyond the getter/setter methods on OGRFieldDefn.
It relied on a "OGRField uDefault" member. The choice of OGRField
restricts the default values to be expressed with the type of the field,
but in some situations we want to be able to assign expressions or
special keywords for non-string fields. For example the SQL standard
defines CURRENT_TIMESTAMP for DateTime fields. So as to be general, we
have remove this uDefault member and replaced it with a "char\*
pszDefault" string.

The values that can be set as default values are :

-  literal string values enclosed in single-quote characters and
   properly escaped like: ``'Nice weather. Isn''t it ?'``
-  numeric values (unquoted)
-  reserved keywords (unquoted): CURRENT_TIMESTAMP, CURRENT_DATE,
   CURRENT_TIME, NULL
-  datetime literal values enclosed in single-quote characters with the
   following defined format: 'YYYY/MM/DD HH:MM:SS[.sss]'
-  any other driver specific expression. e.g. for SQLite:
   (strftime('%Y-%m-%dT%H:%M:%fZ','now'))

The following methods are added/modified to the OGRFieldDefn class

::

       void                SetDefault( const char* );

   /**
    * \brief Set default field value.
    *
    * The default field value is taken into account by drivers (generally those with
    * a SQL interface) that support it at field creation time. OGR will generally not
    * automatically set the default field value to null fields by itself when calling
    * OGRFeature::CreateFeature() / OGRFeature::SetFeature(), but will let the
    * low-level layers to do the job. So retrieving the feature from the layer is
    * recommended.
    *
    * The accepted values are NULL, a numeric value, a literal value enclosed
    * between single quote characters (and inner single quote characters escaped by
    * repetition of the single quote character),
    * CURRENT_TIMESTAMP, CURRENT_TIME, CURRENT_DATE or
    * a driver specific expression (that might be ignored by other drivers).
    * For a datetime literal value, format should be 'YYYY/MM/DD HH:MM:SS[.sss]'
    * (considered as UTC time).
    *
    * Drivers that support writing DEFAULT clauses will advertise the
    * GDAL_DCAP_DEFAULT_FIELDS driver metadata item.
    *
    * This function is the same as the C function OGR_Fld_SetDefault().
    *
    * @param pszDefault new default field value or NULL pointer.
    *
    * @since GDAL 2.0
    */


       const char         *GetDefault() const;

   /**
    * \brief Get default field value.
    *
    * This function is the same as the C function OGR_Fld_GetDefault().
    *
    * @return default field value or NULL.
    * @since GDAL 2.0
    */


       int                 IsDefaultDriverSpecific() const;

   /**
    * \brief Returns whether the default value is driver specific.
    *
    * Driver specific default values are those that are *not* NULL, a numeric value,
    * a literal value enclosed between single quote characters, CURRENT_TIMESTAMP,
    * CURRENT_TIME, CURRENT_DATE or datetime literal value.
    *
    * This method is the same as the C function OGR_Fld_IsDefaultDriverSpecific().
    *
    * @return TRUE if the default value is driver specific.
    * @since GDAL 2.0
    */

SetDefault() validates that a string literal beginning with ' is
properly escaped.

IsDefaultDriverSpecific() returns TRUE if the value set does not belong
to one of the 4 bullets in the above enumeration. This is used by
drivers to determine if they can handle or not a default value.

Drivers should do some effort to interpret and reformat default values
in the above 4 standard formats so as to be able to propagate default
values from one driver to another one.

The following method is added to the OGRFeature class :

::

       void                FillUnsetWithDefault(int bNotNullableOnly,
                                                char** papszOptions );
   /**
    * \brief Fill unset fields with default values that might be defined.
    *
    * This method is the same as the C function OGR_F_FillUnsetWithDefault().
    *
    * @param bNotNullableOnly if we should fill only unset fields with a not-null
    *                     constraint.
    * @param papszOptions unused currently. Must be set to NULL.
    * @since GDAL 2.0
    */

It will replace unset fields of a feature with their default values, but
should rarely be used as most drivers will do that substitution
automatically in their low-level layer. CreateFeature() cannot be
trusted to automatically modify the passed OGRFeature object to set
unset fields to their default values. For that, an explicit GetFeature()
call should be issued to retrieve the record as stored in the database.

A new flag ALTER_DEFAULT_FLAG = 0x8 is added to be passed to
OGRLayer::AlterFieldDefn() so as to set, drop or modify default values
(for drivers that implement it)

Drivers that handle default values should advertise the new
GDAL_DCAP_DEFAULT_FIELDS driver metadata items.

Note: due to the way they are commonly written, the CreateField()
implementations of drivers that do not support default values will
generally copy the value of the default value string, which may be a bit
misleading if querying the field definition just after having adding it.

All above methods are mapped into the C API :

::

   const char CPL_DLL *OGR_Fld_GetDefault( OGRFieldDefnH hDefn );
   void   CPL_DLL OGR_Fld_SetDefault( OGRFieldDefnH hDefn, const char* );
   int    CPL_DLL OGR_Fld_IsDefaultDriverSpecific( OGRFieldDefnH hDefn );

   void   CPL_DLL OGR_F_FillUnsetWithDefault( OGRFeatureH hFeat,
                                              int bNotNullableOnly,
                                              char** papszOptions );

SWIG bindings (Python / Java / C# / Perl) changes
-------------------------------------------------

The following additions have been done :

-  SetNullable(), IsNullable() added on FieldDefn class
-  SetNullable(), IsNullable() added on GeomFieldDefn class
-  Validate() added on Feature class
-  SetDefault(), GetDefault(), IsDefaultDriverSpecific() available on
   FieldDefn class
-  FillUnsetWithDefault() added on Feature class

Utilities
---------

ogrinfo has been updated to expose NOT NULL constraints and DEFAULT
values. e.g.

::

   Geometry Column 1 NOT NULL = WKT
   Geometry Column 2 NOT NULL = geom2
   id: Integer (0.0) NOT NULL DEFAULT 1234567
   dbl: Real (0.0) NOT NULL DEFAULT 1.456
   str: String (0.0) NOT NULL DEFAULT 'a'
   d: Date (0.0) NOT NULL DEFAULT CURRENT_DATE
   t: Time (0.0) NOT NULL DEFAULT CURRENT_TIME
   dt: DateTime (0.0) NOT NULL DEFAULT CURRENT_TIMESTAMP
   dt2: DateTime (0.0) NOT NULL DEFAULT '2013/12/11 01:23:45'

2 news options have been added to ogr2ogr :

-  -forceNullable to remove NOT NULL constraint (NOT NULL constraints
   are propagated by default from source to target layer)
-  -unsetDefault to remove DEFAULT values (DEFAULT values are propagated
   by default from source to target layer)

Unless it is explicitly specified, ogr2ogr will also automatically set
the GEOMETRY_NULLABLE=NO creation option to target layers that support
it, if the source layer has its first geometry field with a NOT NULL
constraint.

Documentation
-------------

New/modified API are documented.

File Formats
------------

The following OGR drivers have been updated to support the new
interfaces.

-  PG: supports NOT NULL (for attribute and multiple geometry fields)
   and DEFAULT on creation/read. AlterFieldDefn() implementation
   modified to support ALTER_NULLABLE_FLAG and ALTER_DEFAULT_FLAG.
-  PGDump: supports NOT NULL (for attribute and multiple geometry
   fields) and DEFAULT on creation.
-  CartoDB: supports NOT NULL (for attribute and single geometry fields)
   and DEFAULT on creation. Supported also on read with authenticated
   login only (relies on queries on PostgreSQL system tables)
-  GPKG: supports NOT NULL (for attribute and its single geometry field)
   and DEFAULT on creation/read. GEOMETRY_NULLABLE layer creation added.
-  SQLite: supports NOT NULL (for attribute and multiple geometry
   fields. Support for multiple geometry fields has been added recently
   per #5494) and DEFAULT on creation/read. AlterFieldDefn()
   implementation modified to support ALTER_NULLABLE_FLAG and
   ALTER_DEFAULT_FLAG.
-  MySQL: supports NOT NULL (for attribute fields only) and DEFAULT on
   creation/read.
-  OCI: supports NOT NULL (for attribute and its single geometry field)
   and DEFAULT on creation/read. GEOMETRY_NULLABLE layer creation added.
-  VRT: supports NOT NULL (for attribute and multiple geometry fields)
   and DEFAULT on read, through new attributes "nullable" and "default"
   (driver documentation and data/ogrvrt.xsd updated)
-  GML: supports NOT NULL (for attribute and multiple geometry field) on
   creation/read. DEFAULT not truly supported (no way to express it in
   .xsd AFAIK), but on creation, unset fields with a NOT NULL constraint
   and DEFAULT values will be filled by using FillUnsetWithDefault() so
   as to generate valid XML.
-  WFS: supports NOT NULL (for attribute fields only) on read
-  FileGDB: supports NOT NULL (for attribute and its single geometry
   field) on read/write. GEOMETRY_NULLABLE layer creation added. DEFAULT
   supported for String,Integer and Real fieds on creation/read (with
   some bugs/weird behavior seen in FileGDB SDK and E$RI tools,
   workarounded by using the OpenFileGDB driver in problematic
   cases...). DEFAULT supported for DateTime on read, but unsupported on
   creation to bug in FileGDB SDK.
-  OpenFileGDB: supports NOT NULL (for attribute and its single geometry
   field) and DEFAULT on read

MSSQLSpatial could probably support NOT NULL / DEFAULT, but has not been
updated as part of this work.

Test Suite
----------

The test suite is extended to test:

-  all new methods of OGRFieldDefn, OGRGeomFieldDefn and OGRFeature in
   ogr_feature.py
-  updated drivers: PG, PGDump, CartoDB, GPKG, SQLite, MySQL, OCI, VRT,
   GML, FileGDB, OpenFileGDB
-  new options of ogr2ogr, and default behavior with NOT NULL / DEFAULT
   propagation

Compatibility Issues
--------------------

This RFC should cause few compatibility issues.

Regarding API, the existing OGRFieldDefn::SetDefault() has been changed
and GetDefaultRef() has been removed. Impact should be low as this
wasn't used in any drivers, was documented as being prone to be removed
in the future, and so was unlikely to be used in applications either
(there was no C binding)

When not using the new API, behavior should remain unchanged w.r.t GDAL
1.X when operating on layers created by GDAL. If reading layers created
by other tools, then NOT NULL and/or DEFAULT can be read, and
propagated. We cannot exclude that propagation of NOT NULL / DEFAULT can
cause problems in some situations. In which case the new options of
ogr2ogr will revert to a behavior that was the one of the GDAL 1.X era.

Related topics out of scope of this RFC
---------------------------------------

There might be an ambiguity between a field that has not been set and a
field that is set to NULL. Both concepts are not distinguished in OGR
currently, but most RDBMS are able to make such a distinction.

Consider the 2 following statements :

::

   INSERT INTO mytable (COL1) VALUES (5)
   INSERT INTO mytable (COL1, COL2) VALUES (5, NULL)

They are not equivalent when COL2 has a default value.

The behavior of the modified drivers by this RFC is to *NOT* emit NULL
at CreateFeature() time when a field is unset, so that the low-level
layer of the driver can replace it with its default value if it exists.
This is generally the wished behavior.

If explicit NULL insertion is wanted, then using SetFeature() afterwards
might be needed, if supported by the drivers (some drivers will likely
not force unset OGR fields to be NULL when composing an UPDATE
statement), otherwise with a direct SQL UPDATE statement.

In fact, this confusion between unset or NULL hurts only in the case of
fields that are nullable and have a DEFAULT value. If making sure to
always associate DEFAULT with NOT NULL, then it becomes a non-issue as
the database would refuse explicit NULL values.

Solving the confusion would require to add a new state to an
instantiated field within a feature to distinguish explicit NULL from
unset, but this would have deep impact in drivers and application code.

Implementation
--------------

Implementation will be done by Even Rouault
(`Spatialys <http://spatialys.com>`__), and sponsored by `LINZ (Land
Information New Zealand) <http://www.linz.govt.nz/>`__.

The proposed implementation lies in the "rfc53_ogr_notnull_default"
branch of the
`https://github.com/rouault/gdal2/tree/rfc53_ogr_notnull_default <https://github.com/rouault/gdal2/tree/rfc53_ogr_notnull_default>`__
repository.

The list of changes :
`https://github.com/rouault/gdal2/compare/rfc53_ogr_notnull_default <https://github.com/rouault/gdal2/compare/rfc53_ogr_notnull_default>`__

Voting history
--------------

+1 from JukkaR, DanielM and EvenR
