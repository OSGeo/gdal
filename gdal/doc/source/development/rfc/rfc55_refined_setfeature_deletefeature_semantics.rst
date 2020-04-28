.. _rfc-55:

=======================================================================================
RFC 55: Refined SetFeature() and DeleteFeature() semantics
=======================================================================================

Authors: Even Rouault

Contact: even dot rouault at spatialys.com

Status: Adopted, implemented in GDAL 2.0

Summary
-------

This RFC refines the semantics of SetFeature() and DeleteFeature() so as
to be able to distinguish nominal case, attempts of updating/deleting
non-existing features, from failures to update/delete existing features.

Rationale
---------

Currently, depending on the drivers, calling SetFeature() or
DeleteFeature() on a non-existing feature may succeed, or fail. It is
generally not desirable that those functions return the OGRERR_NONE
code, as in most situations, it might be a sign of invalid input.
Therefore the OGRERR_NON_EXISTING_FEATURE return code is introduced so
that drivers can inform the calling code that it has attempted to update
or delete a non-existing feature.

Changes
-------

#define OGRERR_NON_EXISTING_FEATURE 9 is added to ogr_core.h

Updated drivers
~~~~~~~~~~~~~~~

The following drivers are updated to implement the new semantics:
PostgreSQL, CartoDB, SQLite, GPKG, MySQL, OCI, FileGDB, Shape, MITAB

Note: MSSQL could also likely be updated

Caveats
~~~~~~~

The behavior of the shapefile driver is a bit particular, in that, its
SetFeature() implementation accepts to recreate a feature that had been
deleted (and its CreateFeature() implementation ignores any set FID on
the passed feature to append a new feature). So
OGRERR_NON_EXISTING_FEATURE will effictively been returned only if the
FID is negative or greater or equal to the maximum feature count.

SWIG bindings (Python / Java / C# / Perl) changes
-------------------------------------------------

OGRERR_NON_EXISTING_FEATURE is added. All OGRERR_xxxx constants are
exposed to the Python bindings

Utilities
---------

No impact

Documentation
-------------

Documentation of SetFeature() and DeleteFeature() mentions the new error
code. MIGRATION_GUIDE.TXT updated with mention to below compatibility
issues.

Test Suite
----------

The test suite is extended to test the modified drivers. test_ogrsf also
tests the behavior of drivers updating/deleting non-existing features.

Compatibility Issues
--------------------

Code that expected update or deleting of non-existing features to
succeed will have to be updated.

Implementation
--------------

Implementation will be done by Even Rouault
(`Spatialys <http://spatialys.com>`__), and sponsored by `LINZ (Land
Information New Zealand) <http://www.linz.govt.nz/>`__.

The proposed implementation lies in the
"rfc55_refined_setfeature_deletefeature_semantics" branch of the
`https://github.com/rouault/gdal2/tree/rfc55_refined_setfeature_deletefeature_semantics <https://github.com/rouault/gdal2/tree/rfc55_refined_setfeature_deletefeature_semantics>`__
repository.

The list of changes:
`https://github.com/rouault/gdal2/compare/rfc55_refined_setfeature_deletefeature_semantics <https://github.com/rouault/gdal2/compare/rfc55_refined_setfeature_deletefeature_semantics>`__

Voting history
--------------

+1 from from DanielM, HowardB, JukkaR and EvenR
