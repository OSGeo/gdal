.. _rfc-38:

=========================================================================
RFC 38: OGR Faster Open (withdrawn)
=========================================================================

Author: Even Rouault

Contact: even dot rouault at spatialys.com

Status: Withdrawn.

Covered by `RFC 46: GDAL/OGR unification <./rfc46_gdal_ogr_unification>`__

Summary
-------

It is proposed that the OGR datasource opening mechanism relies on the
GDALOpenInfo class, already used by GDAL drivers, to speed-up datasource
opening. The speed-up is due to the fact that the file passed to
OGROpen() will be opened and stat'ed only once, whereas currently, it is
opened and closed as many times as there are OGR drivers. This should be
particularly beneficial for network filesystems, or when trying to open
a file that is not a OGR datasource at all.

E.g., trying to open a file that is not a OGR datasource currently
requires 45 file opening or stat operations :

::

   $ strace ogrinfo -ro NEWS 2>&1 | grep NEWS | wc -l
   45

It is expected that if/once all drivers are migrated, it will decrease
to 2 operations only.

Implementation
--------------

Similarly to GDALDriver, the OGRSFDriver class is extended to have a
pfnOpen member, that drivers will set to point to their own Open method.

::

   /* -------------------------------------------------------------------- */
   /*      The following are semiprivate, not intended to be accessed      */
   /*      by anyone but the formats instantiating and populating the      */
   /*      drivers.                                                        */
   /* -------------------------------------------------------------------- */
       OGRDataSource       *(*pfnOpen)( GDALOpenInfo * );

The OGRSFDriverRegistrar::Open() method is updated to call pfnOpen when
iterating over the drivers. When pfnOpen is not set, it will try to call
the Open() method of OGRSFDriver (which enables a progressive migration
of drivers).

Mainly for compatibility reasons, the virtual method Open() of
OGRSFDriver that is currently pure virtual, will now be a regular
virtual method, that will have a default implementation, that will try
to call pfnOpen.

The patch with the changes to OGR core is attached to this page.

Backward Compatibility
----------------------

Proposed additions will not have any impact on C binary compatibility.

C++ binary interface will be broken (due to the addition of a new member
in OGRSFDriver class and the Open() method changed from pure virtual to
virtual).

Source level compatibility will be preserved for third-party OGR
drivers.

Impact on drivers
-----------------

Existing drivers are *not* required to migrate to RFC38, but are
strongly encouraged to. New drivers *should* use RFC38 mechanism to
preserve the overall faster opening.

An example of the migration for a few drivers is attached to this page.

Timeline
--------

Even Rouault is responsible to implement this proposal. New API will be
available in GDAL 2.0. Most in-tree OGR drivers will be migrated to the
new mechanism.
