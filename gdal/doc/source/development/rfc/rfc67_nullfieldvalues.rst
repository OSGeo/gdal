.. _rfc-67:

=======================================================================================
RFC 67 : Null values in OGR
=======================================================================================

Author: Even Rouault

Contact: even.rouault at spatialys.com

Status: Adopted, implemented

Implementation version: 2.2

Summary
-------

This RFC implement the concept of null value for the field of a feature,
in addition to the existing unset status.

Rationale
---------

Currently, OGR supports one single concept to indicate that a field
value is missing : the concept of unset field.

So assuming a JSon feature collection with 2 features would properties
would be { "foo": "bar" } and { "foo": "bar", "other_field": null }, OGR
currently returns that the other_field is unset in both cases.

What is proposed here is that in the first case where the "other_field"
keyword is totally absent, we use the current unset field concept. And
for the other case, we add a new concept of null field.

This distinction between both concepts apply to all GeoJSON based
formats and protocols, so GeoJSON, ElasticSearch, MongoDB, CouchDB,
Cloudant.

This also applies for GML where the semantics of a missing element would
be mapped to unset field and an element with a xsi:nil="true" attribute
would be mapped to a null field.

Changes
-------

OGRField
~~~~~~~~

The Set structure in the "raw field" union is modified to add a third
marker

::

       struct {
           int     nMarker1;
           int     nMarker2;
           int     nMarker3;
       } Set;

This is not strictly related to this work but the 3rd marker decreases
the likelihood of a genuine value to be misinterpreted as unset / null.
This does not increase the size of the structure that is already at
least 12 bytes large.

The current special value of OGRUnsetMarker = -21121 will be set in the
3 markers for unset field (currently set to the first 2 markers).

Similarly for the new Null state, the new value OGRNullMarker = -21122
will be set to the 3 markers.

OGRFeature
~~~~~~~~~~

The methods int IsFieldNull( int nFieldIdx ) and void SetNullField ( int
nFieldIdx ) are added.

The accessors GetFieldXXXX() are modified to take into account the null
case, in the same way as if they are called on a unset field, so
returning 0 for numeric field types, empty string for string fields,
FALSE for date time fields and NULL for list-based types.

A convenience method OGRFeature::IsFieldSetAndNotNull() is added to ease
the porting of existing code that used previously IsFieldSet() and
doesn't need to distinguish between the unset and null states.

C API
-----

The following functions will be added:

::


   int    CPL_DLL OGR_F_IsFieldNull( OGRFeatureH, int );
   void   CPL_DLL OGR_F_SetFieldNull( OGRFeatureH, int );

   int    CPL_DLL OGR_F_IsFieldSetAndNotNull( OGRFeatureH, int );

Lower-level functions will be added to manipulate directly the raw field
union (for use mostly in core and a few drivers), instead of directly
testing/ setting the markers :

::

   int    CPL_DLL OGR_RawField_IsUnset( OGRField* );
   int    CPL_DLL OGR_RawField_IsNull( OGRField* );
   void   CPL_DLL OGR_RawField_SetUnset( OGRField* );
   void   CPL_DLL OGR_RawField_SetNull( OGRField* );

SWIG bindings (Python / Java / C# / Perl) changes
-------------------------------------------------

The new methods will mapped to SWIG.

Drivers
-------

The following drivers will be modified to take into account the unset
and NULL state as distinct states: GeoJSON, ElasticSearch, MongoDB,
CouchDB, Cloudant, GML, GMLAS, WFS.

Note: regarding the GMLAS driver, the previous behavior to have both
xxxx and xxxx_nil fields when xxxx is an optional nillable XML elements
is preserved by default (can be changed through a configuration setting
in the gmlasconf.xml file). The rationale is that the GMLAS driver is
mostly used to convert to SQL capable formats that cannot distinguish
between the unset and null states, hence the need for the 2 dedicated
fields.

The CSV driver will be modified so that when EMPTY_STRING_AS_NULL open
option is specified, the new Null state is used.

All drivers that in their writing part test if the source feature has a
field unset will also test if the field is null.

For SQL based drivers (PG, PGDump, Carto, MySQL, OCI, SQLite, GPKG), on
reading a SQL NULL value will be mapped to the new Null state. On
writing, a unset field will not be mentioned in the corresponding
INSERT or UPDATE statement. Whereas a Null field will be mentioned and
set to NULL. On insertion, there will generally be no difference of
behavior, unless a default value is defined on the field, in which case
it will be used by the database engine to set the value in the unset
case. On update, a unset field will not see its content updated by the
database, where as a field set to NULL will be updated to NULL.

Utilities
---------

No direct changes, but as the OGRFeature::DumpReadable() method is
modified so that unset fields of features are no longer displayed, the
output of ogrinfo will be affected.

Documentation
-------------

All new methods/functions are documented.

Test Suite
----------

Core changes and updated drivers will be tested.

Compatibility Issues
--------------------

All code, in GDAL source code, and in calling external code, that
currently uses OGRFeature::IsFieldSet() / OGR_F_IsFieldSet() should also
be updated to used IsFieldNull() / OGR_F_IsFieldNull(), either to act
exactly as in the unset case, or add a new appropriate behavior. A
convenience method and function OGRFeature::IsFieldSetAndNotNull() /
OGR_F_IsFieldSetAndNotNull() is added to ease the porting of existing
code.

Failure to do so, the existing code will see 0 for numeric field types,
empty string for string fields, FALSE for date time fields and NULL for
list-based types.

On the write side, for the GeoJSON driver, in GDAL 2.1 or before, a
unset field was written as field_name: null. Starting with GDAL 2.2,
only fields explicitly set as null with OGR_F_SetFieldNull() will be
written with a null value. Unset fields of a feature will not be present
in the corresponding JSon feature element.

MIGRATION_GUIDE.TXT is updated to discuss those compatibility issues.

Related ticket
--------------

None

Implementation
--------------

The implementation will be done by Even Rouault (Spatialys) and be
sponsored by Safe Software.

The proposed implementation is available in
`https://github.com/rouault/gdal2/tree/rfc67 <https://github.com/rouault/gdal2/tree/rfc67>`__

Voting history
--------------

+1 from JukkaR, DanielM, HowardB and EvenR
