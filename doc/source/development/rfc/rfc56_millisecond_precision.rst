.. _rfc-56:

=======================================================================================
RFC 56: OFTTime/OFTDateTime millisecond accuracy
=======================================================================================

Author: Even Rouault

Contact: even dot rouault at spatialys dot com

Status: Adopted, implemented

Version: 2.0

Summary
-------

This RFC aims at adding millisecond accuracy to OFTTime and OFTDateTime
fields, as a number of formats support it explicitly or implicitly :
MapInfo, GPX, Atom (GeoRSS driver), GeoPackage, SQLite, PostgreSQL, CSV,
GeoJSON, ODS, XLSX, KML (potentially GML too)...

Core changes
------------

The OGRField enumeration is modified as such :

::

   typedef union {
       [... unchanged ... ]

       struct {
           GInt16  Year;
           GByte   Month;
           GByte   Day;
           GByte   Hour;
           GByte   Minute;
           GByte   TZFlag; /* 0=unknown, 1=localtime(ambiguous), 
                              100=GMT, 104=GMT+1, 80=GMT-5, etc */
           GByte   Reserved; /* must be set to 0 */
           float   Second; /* with millisecond accuracy. at the end of the structure, so as to keep it 12 bytes on 32 bit */
       } Date;
   } OGRField;

So the "GByte Second" field is removed and replaced by a padding Byte
reserved for potential later uses. A "float Second" field is added.

On 32 bit builds, the size of OGRField is now 12 bytes instead of 8
bytes. On 64 bit builds, the size of OGRField remains 16 bytes.

New/modified methods
~~~~~~~~~~~~~~~~~~~~

OGRFeature::SetFieldAsDateTime() methods that took a int nSecond now
take a float fSecond parameter. The GetFieldAsDateTime() method that
took a int\* pnSecond is kept, and a new GetFieldAsDateTime() method
that takes a float\* pfSecond is added.

-  In OGRFeature class :

::

       int                 GetFieldAsDateTime( int i, 
                                        int *pnYear, int *pnMonth, int *pnDay,
                                        int *pnHour, int *pnMinute, int *pnSecond, 
                                        int *pnTZFlag ); /* unchanged from GDAL 1.X */
       int                 GetFieldAsDateTime( int i, 
                                        int *pnYear, int *pnMonth, int *pnDay,
                                        int *pnHour, int *pnMinute, float *pfSecond, 
                                        int *pnTZFlag ); /* new */
       void                SetField( int i, int nYear, int nMonth, int nDay,
                                     int nHour=0, int nMinute=0, float fSecond=0.f, 
                                     int nTZFlag = 0 ); /* modified */
       void                SetField( const char *pszFName, 
                                     int nYear, int nMonth, int nDay,
                                     int nHour=0, int nMinute=0, float fSecond=0.f, 
                                     int nTZFlag = 0 ); /* modified */

OGRFeature::GetFieldAsString() is modified to output milliseconds if the
Second member of OGRField.Date is not integral

OGRParseDate() is modified to parse second as floating point number.

The following utility functions have their signature modified to take a
OGRField (instead of the full year, month, day, hour, minute, second,
TZFlag decomposition) and accept decimal seconds as input/output :

::

   int CPL_DLL OGRParseXMLDateTime( const char* pszXMLDateTime,
                                    OGRField* psField );
   int CPL_DLL OGRParseRFC822DateTime( const char* pszRFC822DateTime,
                                       OGRField* psField );
   char CPL_DLL * OGRGetRFC822DateTime(const OGRField* psField);
   char CPL_DLL * OGRGetXMLDateTime(const OGRField* psField);

C API changes
~~~~~~~~~~~~~

Only additions :

::

   int   CPL_DLL OGR_F_GetFieldAsDateTimeEx( OGRFeatureH hFeat, int iField,
                                   int *pnYear, int *pnMonth, int *pnDay,
                                   int *pnHour, int *pnMinute, float *pfSecond,
                                   int *pnTZFlag );
   void   CPL_DLL OGR_F_SetFieldDateTimeEx( OGRFeatureH, int, 
                                          int, int, int, int, int, float, int );

Changes in drivers
------------------

The following drivers now accept milliseconds as input/output :

-  GeoJSON
-  CSV
-  PG
-  PGDump (output only)
-  CartoDB
-  GeoPackage
-  SQLite
-  MapInfo .tab and .mif
-  LIBKML
-  ODS
-  XLSX
-  GeoRSS (Atom format)
-  GPX

Changes in SWIG bindings
------------------------

Feature.GetFieldAsDateTime() and Feature.SetFieldAsDateTime() now
takes/returns a floating point number for seconds

Compatibility
-------------

This modifies the C/C++ API and ABI.

Output of above mentioned drivers will now include milliseconds if a
DateTime/Time field has such precision.

Related ticket
--------------

The need came from
`http://trac.osgeo.org/gdal/ticket/2680 <http://trac.osgeo.org/gdal/ticket/2680>`__
for MapInfo driver.

Documentation
-------------

All new/modified methods are documented. MIGRATION_GUIDE.TXT is updated
with a new section for this RFC.

Testing
-------

The various aspects of this RFC are tested:

-  core changes
-  driver changes

Implementation
--------------

Implementation will be done by Even Rouault
(`Spatialys <http://spatialys.com>`__).

The proposed implementation lies in the "subsecond_accuracy" branch of
the
`https://github.com/rouault/gdal2/tree/subsecond_accuracy <https://github.com/rouault/gdal2/tree/subsecond_accuracy>`__
repository.

The list of changes :
`https://github.com/rouault/gdal2/compare/subsecond_accuracy <https://github.com/rouault/gdal2/compare/subsecond_accuracy>`__

Voting history
--------------

+1 from DanielM, JukkaR and EvenR
