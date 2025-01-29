.. _rfc-106:

===================================================================
RFC 106: Metadata items to reflect driver update capabilities
===================================================================

============== =============================================
Author:        Even Rouault
Contact:       even.rouault @ spatialys.com
Started:       2025-01-22
Status:        Adopted, implemented
Target:        GDAL 3.11
============== =============================================

Summary
-------

This RFC adds two new driver metadata items, GDAL_DCAP_UPDATE and
GDAL_DMD_UPDATE_ITEMS, to better inform applications about update capabilities
of drivers.

Motivation
----------

There are use cases where an existing dataset must be updated, for example to
add georeferencing to an existing raster, adding/updating metadata. Currently
there is no obvious way to know if a driver supports at all updating without
trying to open it in update mode, and when that works, trying the operation
and checking its return status (and in case of failure, one cannot know for sure
if the failure is due to the operation not being supported at all, or for some
other reason, without checking the text of the error message)
Some applications, e.g QGIS as discussed in https://github.com/qgis/QGIS/pull/60208
have to use workarounds, such as having an allow-list of drivers known to work,
and this is generally not desirable.
This could also be used by the new "gdal raster edit" utility (https://gdal.org/en/latest/programs/gdal_raster_edit.html)
to provide better error message when a user attempts to perform an operation
that is not supported.

Technical solution
------------------

A first addition is a GDAL_DCAP_UPDATE capability whose value must be set
to "YES" when a driver offers some update capabilities.

.. code-block:: c

    /** Capability set by a driver that supports the GDAL_OF_UPDATE flag and offers
     * at least some update capabilities.
     * Exact update capabilities can be determined by the GDAL_DMD_UPDATE_ITEMS
     * metadata item
     * @since GDAL 3.11
     */
    #define GDAL_DCAP_UPDATE "DCAP_UPDATE"


This offers the same type of capability discovery as the existing GDAL_DCAP_CREATE
or GDAL_DCAP_CREATECOPY items.


An additional item is provided to get more details on the exact capabilities:

.. code-block:: c

    /* List of (space separated) items that a dataset opened in update mode supports
     * updating. Possible values are:
     * - for raster: "GeoTransform" (through GDALDataset::SetGeoTransform),
     *   "SRS" (GDALDataset::SetSpatialRef), "GCPs" (GDALDataset::SetGCPs()),
     *   "NoData" (GDALRasterBand::SetNoDataValue),
     *   "ColorInterpretation" (GDALRasterBand::SetColorInterpretation()),
     *   "RasterValues" (GF_Write flag of GDALDataset::RasterIO() and GDALRasterBand::RasterIO()),
     *   "DatasetMetadata" (GDALDataset::SetMetadata/SetMetadataItem), "BandMetadata"
     *   (GDALRasterBand::SetMetadata/SetMetadataItem)
     * - for vector: "Features" (OGRLayer::SetFeature()), "DatasetMetadata",
     *   "LayerMetadata"
     *
     * No distinction is made if the update is done in the native format,
     * or in a Persistent Auxiliary Metadata .aux.xml side car file.
     *
     * @since GDAL 3.11
     */
    #define GDAL_DMD_UPDATE_ITEMS "DMD_UPDATE_ITEMS"


Note that the feature list is far from being exhaustive. It is restricted to
what is thought to be the most useful.

A new ``GDALDataset::ReportUpdateNotSupportedByDriver(const char* pszDriverName)``
static method will be added to avoid all drivers to have to compose their own
error message when they don't support update.

So current patterns like:

.. code-block:: c++

    if (poOpenInfo->eAccess == GA_Update)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "The GSC driver does not support update access to existing "
                 "datasets.");
        return nullptr;
    }

will become:

.. code-block:: c++

    if (poOpenInfo->eAccess == GA_Update)
    {
        ReportUpdateNotSupportedByDriver("GSC");
        return nullptr;
    }


Impact on drivers
-----------------

Drivers that have update capabilities will be modified to fill the new metadata
items. I don't guarantee I'll have the motivation to go into all the esoteric
drivers, so if you care about them, dear reader, that will be let as a pull
request exercise to you.

Out-of-scope
------------

A functionality that does not exist currently would be to offer the capability
to open formats like PNG, maybe with a new GDAL_OF_UPDATE_PAM flag, to be able to
update information such as geotransform, SRS, metadata that goes into Persistent
Auxiliary Metadata .aux.xml side car files, which is not possible currently,
although that such information is written during CreateCopy() operation.

Regarding the detailed capabilities of GDAL_DMD_UPDATE_ITEMS, one could also
imagine they could potentially be useful in a Create()/CreateCopy() context, but
that would probably require creating one/two distinct new metadata items, e.g.
GDAL_DMD_CREATE_ITEMS / GDAL_DMD_CREATECOPY_ITEMS, because many drivers only
support updating a subset of what they are able to take into account at CreateCopy()
time. In the absence of a concrete obvious need for that, this is out of scope for now.
If that was going to be implemented, consistency with the label of the items
listed in GDAL_DMD_UPDATE_ITEMS would have to be sought.

Considered alternatives / discussion
------------------------------------

For detailed capabilities, adding new capability flags to
:cpp:func:`GDALDataset::TestCapability()` could have been considered, but for
use cases where one wants to present a subset of drivers with a given capability,
this is not adequate as it requires to have already opened a driver.

Backward compatibility
----------------------

No impact.

Testing
-------

test_ogrsf is modified to check that if a layer declares the OLCReadWrite
capabilities it also reports GDAL_DCAP_UPDATE and the "Features" item in
GDAL_DMD_UPDATE_ITEMS

autotest/gcore/misc.py is modified so that the dataset it creates is opened
in update mode if GDAL_DCAP_UPDATE is declared and tests that the operations
specific of "GeoTransform", "SRS", "NoData", "DatasetMetadata", "BandMetadata"
and "RasterValues" do not fail when those items are declared.

autotest/gcore/test_driver_metadata.py is modified to validate that if GDAL_DCAP_UPDATE
is declared, GDAL_DMD_UPDATE_ITEMS is also declared, and vice versa. It also
validates that the items declared in GDAL_DMD_UPDATE_ITEMS are the ones allowed
(to avoid typos in driver metadata).

Documentation
-------------

Nothing specific, just Doxygen generated documentation from above proposed
additions in :file:`gdal.h`.

Related issues and PRs
----------------------

* Bug that triggered this PR: https://github.com/qgis/QGIS/pull/60208

* Candidate implementation: https://github.com/OSGeo/gdal/pull/11718

Funding
-------

Co-funded by GDAL Sponsorship Program (GSP) and QGIS bug-fixing program.

Voting history
--------------

+1 from PSC members JavierJS, JukkaR, SeanG and EvenR


.. below is an allow-list for spelling checker.

.. spelling:word-list::
    pszDriverName
