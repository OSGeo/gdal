.. _rfc-10:

================================================================================
RFC 10: OGR Open Parameters (not implemented)
================================================================================

Author: Andrey Kiselev

Contact: dron@ak4719.spb.edu

Status: Development, *not* implemented

Summary
-------

It is proposed that OGRSFDriver::Open() and OGRSFDriverRegistrar::Open()
calls should be changed to accept additional parameter containing
arbitrary additional parameters supplied by caller. OGROpenEx() function
will be introduced to map this new functionality into C interface. In
addition it is proposed to add an "update" flag to
OGRSFDriverRegistrar::Open() call to avoid using
OGRSFDriverRegistrar::OpenShared() method.

Open parameters
---------------

Sometimes it is needed to pass additional information to OGR driver
along with the name of the dataset to be opened. It can be, for example,
the style table name (some drivers allow to choose from the various
style tables) or any other additional data. The old method for doing
this was to encode the extra info in the dataset name string. It was
inconvenient approach, so it proposed to use separate parameter in
OGRSFDriver::Open() and OGRSFDriverRegistrar::Open() calls representing
open options, just like it is implemented in
OGRDataSource::CreateLayer() call.

It is supposed that open options will be supplied in form of NAME=VALUE
pairs forming the string list.

In addition to options parameter the special "shared" flag will be added
to OGRSFDriverRegistrar::Open() call, so there will be no need in
separate OGRSFDriverRegistrar::OpenShared() method.

Implementation
--------------

All Open() functions will be changed in the following way:

::

   static OGRDataSource *
   OGRSFDriverRegistrar::Open( const char * pszName, int bUpdate,
                               OGRSFDriver ** ppoDriver,
                   int bShared = FALSE,
                   char **papszOptions = NULL );


::

   OGRDataSource *
   OGRSFDriverRegistrar::OpenShared( const char * pszName, int bUpdate,
                                     OGRSFDriver ** ppoDriver )
       { return Open( pszName, bUpdate, ppoDriver, TRUE, NULL ); }

::

   virtual OGRDataSource
   OGRSFDriver::*Open( const char *pszName, int bUpdate=FALSE,
                       char **papszOptions = NULL ) = 0;

The last change needs to be propagated in all OGR drivers. The change
itself is pretty simple: one additional parameter should be added to
function definition. But it has impact on third-party OGR drivers: they
are not source compatible anymore and should be changed too.

Also appropriate C functions will be added:

::

   OGRDataSourceH OGROpenEx( const char *pszName, int bUpdate,
                             OGRSFDriverH *pahDriverList,
                 int bShared, char **papszOptions );

::

   OGRDataSourceH OGR_Dr_OpenEx( OGRSFDriverH hDriver, const char *pszName, 
                                 int bUpdate, char **papszOptions );

New Options for OGR Utilities
-----------------------------

Proposed functionality will be available in OGR utilities ogr2ogr and
ogrinfo via the '-doo NAME=VALUE' ("Datasource Open Option") format
specific parameter.

Backward Compatibility
----------------------

Proposed additions will not have any impact on C binary compatibility.
C++ binary interface will be broken, source level compatibility will be
broken for third-party OGR drivers only. There will be no impact for
high-level applications on source level.

Responsibility and Timeline
---------------------------

Andrey Kiselev is responsible to implement this proposal. New API will
be available in GDAL 1.5.0.
