.. _rfc-31:

================================================================================
RFC 31: OGR 64bit Integer Fields and FIDs
================================================================================

Authors: Frank Warmerdam, Even Rouault

Contact: warmerdam@pobox.com, even dot rouault at spatialys.com

Status: Adopted, implemented in GDAL 2.0

Summary
-------

This RFC addresses steps to upgrade OGR to support 64bit integer fields
and feature ids. Many feature data formats support wide integers, and
the inability to transform these through OGR causes increasing numbers
of problems.

.. _64bit-fid-feature-index-and-feature-count:

64bit FID, feature index and feature count
------------------------------------------

Feature id's will be handled as type "GIntBig" instead of "long"
internally. This will include the nFID field of the OGRFeature. The
existing GetFID() and SetFID() methods on the OGRFeature use type long
and are changed to return (respectively accept) GIntBig instead. The
change of return type for GetFID() will require application code to
carefully adapt to avoid potential issues (for example if GetFID() is
used in printf-like expression). SetFID() change should be mostly
transparent. So the changes in the OGRFeature class are:

::

     GIntBig  GetFID();
     OGRErr   SetFID(GIntBig nFID );

At the C API level:

::

     GIntBig CPL_DLL OGR_F_GetFID( OGRFeatureH );
     OGRErr CPL_DLL OGR_F_SetFID( OGRFeatureH, GIntBig );

Note that the old interfaces using "long" are already 64bit on 64bit
operating systems (excluding Windows target compilers where long is
32bit even on 64bit builds), so there is little harm to applications
continuing to use these interfaces on 64bit operating systems.

A layer that can discover in a relatively cheap way that it holds
features with 64bit FID should advertise the OLMD_FID64 metadata item to
"YES", so ogr2ogr can pass the FID64 creation option to drivers that
support it.

The OGRLayer class allows several operations based on the FID. The
signature of these will be *altered* to accept GIntBig instead of long.
In theory this should not require any changes to application code since
long can be converted to GIntBig losslessly. However, all existing OGR
drivers require changes, including private drivers. This will also
result in a backwards incompatible change in the C ABI. While we are at
it, we want GetFeatureCount() to be able to return more than 2 billion
record (currently returning 32 bit integer), and thus it will return
GIntBig. Similarly to GetFID(), this change of return type will require
caution in application code.

So at the OGRLayer C++ class level:

::

       virtual OGRFeature *GetFeature( GIntBig nFID );
       virtual OGRErr      DeleteFeature( GIntBig nFID );
       virtual OGRErr      SetNextByIndex( GIntBig nIndex );
       virtual GIntBig     GetFeatureCount( int bForce = TRUE );

At the C API level :

::

     OGRFeatureH CPL_DLL OGR_L_GetFeature( OGRLayerH, GIntBig );
     OGRErr CPL_DLL OGR_L_DeleteFeature( OGRLayerH, GIntBig );
     OGRErr CPL_DLL OGR_L_SetNextByIndex( OGRLayerH, GIntBig );
     GIntBig CPL_DLL OGR_L_GetFeatureCount( OGRLayerH, int );

.. _64bit-fields:

64bit Fields
------------

New field types will be introduced for 64bit integers:

::

      OFTInteger64 = 12
      OFTInteger64List = 13

The OGRField union will be extended to include:

::

       GIntBig     Integer64;
       struct {
           int nCount;
           GIntBig *paList;
       } Integer64List;

The OGRFeature class will be extended with these new methods:

::

       GIntBig             GetFieldAsInteger64( int i );
       GIntBig             GetFieldAsInteger64( const char *pszFName );
       const int          *GetFieldAsInteger64List( const char *pszFName,
                                                  int *pnCount );
       const int          *GetFieldAsInteger64List( int i, int *pnCount );

       void                SetField( int i, GIntBig nValue );
       void                SetField( int i, int nCount, const GIntBig * panValues );
       void                SetField( const char *pszFName, GIntBig nValue )
       void                SetField( const char *pszFName, int nCount,
                                     const GIntBig * panValues )

At the C level, the following functions are added :

::

       GIntBig CPL_DLL OGR_F_GetFieldAsInteger64( OGRFeatureH, int );
       const GIntBig CPL_DLL *OGR_F_GetFieldAsInteger64List( OGRFeatureH, int, int * );
       void   CPL_DLL OGR_F_SetFieldInteger64( OGRFeatureH, int, GIntBig );
       void   CPL_DLL OGR_F_SetFieldInteger64List( OGRFeatureH, int, int, const GIntBig * );

Furthermore, the new interfaces will internally support setting/getting
integer fields, and the integer field methods will support
getting/setting 64bit integer fields so that one case can be used for
both field types where convenient (except GetFieldAsInteger64List() that
can only operate on Integer64List fields)

A GDAL_DMD_CREATIONFIELDDATATYPES = "DMD_CREATIONFIELDDATATYPES" driver
metadata item is added so as drivers to be able to declare the field
types they support on creation. For example "Integer Integer64 Real
String Date DateTime Time IntegerList Integer64List RealList StringList
Binary". Commonly used drivers will be updated to declare it.

OGR SQL
-------

A SWQ_INTEGER64 internal type is added so as to be able to map/from
OFTInteger64 fields. The int_value member of the swq_expr_node class is
extended from int to GIntBig (so both SWQ_INTEGER and SWQ_INTEGER64
refer to that member).

.. _python--java--c--perl-changes:

Python / Java / C# / perl Changes
---------------------------------

The following changes have been done :

-  GetFID(), GetFeatureCount() have been changed to return a 64 bit
   integer
-  SetFID(), GetFeature(), DeleteFeature(), SetNextByIndex() have been
   changed to accept a 64 bit integer as argument
-  GetFieldAsInteger64() and SetFieldInteger64() have been added
-  In Python, GetField(), SetField() can accept/return 64 bit values
-  GetFieldAsInteger64List() and SetFieldInteger64List() have been added
   (Python only, due to lack of relevant typemaps for other languages,
   but could potentially be done)

The change in return type of GetFID() and GetFeatureCount() might cause
warnings at compilation time in some languages (Java YES, Python not
relevant, Perl/C# ?). All changes to existing methods will are an ABI
change for Java bytecode.

Utilities
---------

ogr2ogr and ogrinfo are updated to support the new 64bit interfaces.

A new option is added to ogr2ogr : -mapFieldType. Can be used like this
-mapFieldType Integer64=Integer,Date=String to mean that Integer64 field
in the source layer should be created as Integer, and Date as String.
ogr2ogr will also warn if attempting to create a field in an output
driver that advertises a GDAL_DMD_CREATIONFIELDDATATYPES metadata item
that does not mention the required field type. For Integer64 fields, if
it is not advertized in GDAL_DMD_CREATIONFIELDDATATYPES metadata item or
GDAL_DMD_CREATIONFIELDDATATYPES is missing, conversion to Real is done
by default with a warning. ogr2ogr will also query the source layer to
check if the OLMD_FID64 metadata item is declared and if the output
driver has the FID64 layer creation option. In which case it will set
it.

Documentation
-------------

New/modified API are documented. Updates in drivers with new
options/behaviours are documented. MIGRATION_GUIDE.TXT extended with a
section related to this RFC. OGR API updated.

File Formats
------------

As appropriate, existing OGR drivers have been updated to support the
new/updated interfaces. In particular an effort has been made to update
a few database drivers to support 64bit integer columns for use as
feature id, though they don't always create FID columns as 64bit by
default when creating new layers as this may cause problems for other
applications.

Apart from the mechanical changes due to interface changes, the detailed
list of changes is :

-  Shapefile: OFTInteger fields are created by default with a width of 9
   characters, so to be unambiguously read as OFTInteger (and if
   specifying integer that require 10 or 11 characters. the field is
   dynamically extended like managed since a few versions). OFTInteger64
   fields are created by default with a width of 18 digits, so to be
   unambiguously read as OFTInteger64, and extended to 19 or 20 if
   needed. Integer fields of width between 10 and 18 will be read as
   OFTInteger64. Above they will be treated as OFTReal. In previous GDAL
   versions, Integer fields were created with a default with of 10, and
   thus will be now read as OFTInteger64. An open option,
   DETECT_TYPE=YES, can be specified so as OGR does a full scan of the
   DBF file to see if integer fields of size 10 or 11 hold 32 bit or 64
   bit values and adjust the type accordingly (and same for integer
   fields of size 19 or 20, in case of overflow of 64 bit integer,
   OFTReal is chosen)
-  PG: updated to read and create OFTInteger64 as INT8 and
   OFTInteger64List as bigint[]. 64 bit FIDs are supported. By default,
   on layer creation, the FID field is created as a SERIAL (32 bit
   integer) to avoid compatibility issues. The FID64=YES creation option
   can be passed to create it as a BIGSERIAL instead. If needed, the
   drivers will dynamically alter the schema to extend a 32 bit integer
   FID field to 64 bit. GetFeatureCount() modified to return 64 bit
   values. OLMD_FID64 = "YES" advertized as soon as the FID column is 64
   bit.
-  PGDump: Integer64, Integer64List and 64 bit FID supported in
   read/write. FID64=YES creation option available.
-  GeoJSON: Integer64, Integer64List and 64 bit FID supported in
   read/write. The 64 bit variants are reported only if needed,
   otherwise OFTInteger/OFTIntegerList is used. OLMD_FID64 = "YES"
   advertized if needed
-  CSV: Integer64 supported in read/write, including the autodetection
   feature of field types.
-  GPKG: Integer64 and 64 bit FID supported in read/write. Conforming
   with the GeoPackage spec, "INT" or "INTEGER" columns are considered
   64 bits, whereas "MEDIUMINT" is considered 32 bit. OLMD_FID64 = "YES"
   advertized as soon as MAX(fid_column) is 64 bit. GetFeatureCount()
   modified to return 64 bit values.
-  SQLite: Integer64 and 64 bit FID supported in read/write. On write,
   Integer64 are createad as "BIGINT" and on read BIGINT or INT8 are
   considered as Integer64. However it might be possible that databases
   produced by other tools are created with "INTEGER" and hold 64 bit
   values, in which case OGR will not be able to detect it. The
   OGR_PROMOTE_TO_INTEGER64=YES configuration option can then be passed
   to workaround that issue. OLMD_FID64 = "YES" advertized as soon as
   MAX(fid_column) is 64 bit. GetFeatureCount() modified to return 64
   bit values.
-  MySQL: Integer64 and 64 bit FID supported in read/write. Similarly to
   PG, FID column is created as 32 bit by default, unless FID64=YES
   creation option is specified. OLMD_FID64 = "YES" advertized as soon
   as the FID column is 64 bit. GetFeatureCount() modified to return 64
   bit values.
-  OCI: Integer64 and 64 bit FID supported in read/write. Detecting
   Integer/Integer64 on read is tricky since there's only a NUMBER SQL
   type with a field width. It is assumed that if the width is <= 9 or
   if it is the unspecified value (38), then it is a Integer. On
   creation, OGR will set a width of 20 for OFTInteger64, so a NUMBER
   without decimal part and with a width of 20 will be considered as a
   Integer64.
-  MEM: Integer64 and 64 bit FID supported in read/write.
   GetFeatureCount() modified to return 64 bit values.
-  VRT: Integer64, Integer64List and 64 bit FID supported in read/write.
   GetFeatureCount() modified to return 64 bit values.
-  JML: Integer64 supported on creation (created as "OBJECT"). On read,
   returned as String
-  GML: Integer64, Integer64List and 64 bit FID supported in read/write.
   GetFeatureCount() modified to return 64 bit values.
-  WFS: Integer64, Integer64List and 64 bit FID supported in read/write.
   GetFeatureCount() modified to return 64 bit values.
-  CartoDB: Integer64 supported on creation. On read returned as Real
   (CartoDB only advertises a 'Number' type). GetFeatureCount() modified
   to return 64 bit values.
-  XLSX: Integer64 supported in read/write.
-  ODS: Integer64 supported in read/write.
-  MSSQLSpatial: GetFeatureCount() modified to return 64 bit values. No
   Integer64 support implemented although could likely be done.
-  OSM: FID is now always set even when sizeof(long) != 8
-  LIBKML: KML 'uint' advertized as Integer64.
-  MITAB: Change the way FID of Seamless tables are generated to make it
   more robust and accept arbitrary number of index tables made of an
   arbitrary number of features, by using full 64bit width of IDs

Test Suite
----------

The test suite is extended to test the new capabilities:

-  core SetField/GetField methods
-  updated drivers: Shapefile, PG, GeoJSON, CSV, GPKG, SQLite, MySQL,
   VRT, GML, XLSX, ODS, MITAB
-  OGR SQL
-  option -mapFieldType of ogr2ogr

Compatibility Issues
--------------------

Driver Code Changes
~~~~~~~~~~~~~~~~~~~

-  All drivers implementing SetNextByIndex(), DeleteFeature(),
   GetFeature(), GetFeatureCount() will need to change their prototype
   and do modest changes.

-  Drivers supporting CreateField() likely ought to be extended to
   support OFTInteger64 as an integer/real/string field if nothing else
   is available (and if bApproxOK is TRUE). ogr2ogr will convert
   Integer64 to Real if Integer64 support is not advertized

-  Drivers reporting FIDs via Debug statements, printf's or using
   sprintfs like statements to format them for output have been updated
   to use CPL_FRMT_GIB to format the FID. Failure to make these changes
   may result in code crashing. Due to the use of GCC annotation to
   advertise printf()-like formatting syntax in CPL functions, we are
   reasonably confident to have done the required changes in in-tree
   drivers (except in some proprietary drivers, like SDE, IDB, INGRES,
   ArcObjects, where this couldn't be compiled-checked). The same holds
   true for GetFeatureCount()

Application Code
~~~~~~~~~~~~~~~~

-  Application code may need to be updated to use GIntBig for FIDs and
   feature count in order to avoid warnings about downcasting.

-  Application code formatting FIDs or feature count using printf like
   facilities may also need to be changed to downcast explicitly or to
   use CPL_FRMT_GIB.

-  Application code may need to add Integer64 handling in order to
   utilize wide fields.

Behavioral Changes
~~~~~~~~~~~~~~~~~~

-  Wide integer fields that were previously treated as "real" or Integer
   by the shapefile driver will now be treated as Integer64 which will
   likely not work with some applications, and translation to other
   formats may fail.

Related tickets
---------------

-  `#3747 OGR FID needs to be 64
   bit <http://trac.osgeo.org/gdal/ticket/3747>`__
-  `#3615 Shapefile : A 10-digit value doesn't necessarily fit into a 32
   bit integer. <http://trac.osgeo.org/gdal/ticket/3615>`__
-  `#3150 Precision Problem for Numeric on OGR/OCI
   driver <http://trac.osgeo.org/gdal/ticket/3150>`__

Related topics out of scope of this RFC
---------------------------------------

The possibility of having a Numeric type that corresponds to the
matching SQL type, i.e. a decimal number with an arbitrary number of
significant figures has been considered. In OGR, this could be
implemented as a full type like Integer, Integer64 etc., or possibly as
a subtype of String (see `RFC 50: OGR field
subtypes <./rfc50_ogr_field_subtype>`__). The latter approach would be
easier to implement and mostly useful for lossless conversion between
database drivers (and shapefile). The former approach would require more
work, and would ideally involve OGR SQL support, which would require
supporting arithmetic of arbitrary length. The use cases for such a
numeric type have been considered marginal enough to let that aside for
now.

Implementation
--------------

Implementation will be done by Even Rouault
(`Spatialys <http://spatialys.com>`__), and sponsored by `LINZ (Land
Information New Zealand) <http://www.linz.govt.nz/>`__.

The proposed implementation lies in the "rfc31_64bit" branch of the
`https://github.com/rouault/gdal2/tree/rfc31_64bit <https://github.com/rouault/gdal2/tree/rfc31_64bit>`__
repository.

The list of changes :
`https://github.com/rouault/gdal2/compare/rfc31_64bit <https://github.com/rouault/gdal2/compare/rfc31_64bit>`__

Voting history
--------------

+1 from JukkaR, DanielM, TamasS, HowardB and EvenR
