.. _rfc-4:

=========================================================================
RFC 4: Geolocation Arrays
=========================================================================

Author: Frank Warmerdam

Contact: warmerdam@pobox.com

Status: Development

Summary
-------

It is proposed that GDAL support an additional mechanism for geolocation
of imagery based on large arrays of points associating pixels and lines
with geolocation coordinates. These arrays would be represented as
raster bands themselves.

It is common in AVHRR, Envisat, HDF and netCDF data products to
distribute geolocation for raw or projected data in this manner, and
current approaches to representing this as very large numbers of GCPs,
or greatly subsampling the geolocation information to provide more
reasonable numbers of GCPs are inadequate for many applications.

Geolocation Domain Metadata
---------------------------

Datasets with geolocation information will include the following dataset
level metadata items in the "GEOLOCATION" domain to identify the
geolocation arrays, and the details of the coordinate system and
relationship back to the original pixels and lines.

-  SRS: wkt encoding of spatial reference system.
-  X_DATASET: dataset name (defaults to same dataset if not specified)
-  X_BAND: band number within X_DATASET.
-  Y_DATASET: dataset name (defaults to same dataset if not specified)
-  Y_BAND: band number within Y_DATASET.
-  Z_DATASET: dataset name (defaults to same dataset if not specified)
-  Z_BAND: band number within Z_DATASET. (optional)
-  PIXEL_OFFSET: pixel offset into geo-located data of left geolocation
   pixel
-  LINE_OFFSET: line offset into geo-located data of top geolocation
   pixel
-  PIXEL_STEP: each geolocation pixel represents this many geolocated
   pixels.
-  LINE_STEP: each geolocation pixel represents this many geolocated
   lines.

In the common case where two of the bands of a dataset are actually
latitude and longitude, and so the geolocation arrays are the same size
as the base image, the metadata might look like:

::

   SRS: GEOGCS...
   X_BAND: 2
   Y_BAND: 3
   PIXEL_OFFSET: 0
   LINE_OFFSET: 0
   PIXEL_STEP: 1
   LINE_STEP: 1

For AVHRR datasets, there are only 11 points (note, the more recent NOAA
AVHRR datasets have 51 points), but for every line. So the result for a
LAC dataset might look like:

::

   SRS: GEOGCS...
   X_DATASET: L1BGCPS:n12gac10bit.l1b
   X_BAND: 1
   Y_DATASET: L1BGCPS:n12gac10bit.l1b
   Y_BAND: 2
   PIXEL_OFFSET: 25
   LINE_OFFSET: 0
   PIXEL_STEP: 40
   LINE_STEP: 1

This assumes the L1B driver is modified to support the special access to
GCPs as bands using the L1BGCPS: prefix.

Updating Drivers
----------------

1. HDF4: Client needs mandate immediate incorporation of geolocation
   array support in the HDF4 driver (specifically for swath products).
   (complete)
2. HDF5: Some HDF5 products include geolocation information that should
   be handled as arrays. No timetable for update.
3. AVHRR: Has 11/51 known locations per-scanline. These are currently
   substantially downsampled and returned as GCPs, but this format would
   be an excellent candidate for treating as geolocation arrays. Planned
   in near future.
4. Envisat: Envisat raw products use geolocation information currently
   subsampled as GCPs, good candidate for upgrade. No timetable for
   update.
5. netCDF: NetCDF files can have differently varying maps in x and y
   directions, which are represented as geolocation arrays when they are
   encoded as CF conventions "two-dimensional coordinate variables". See
   the netcdf driver page for details.
6. OPeNDAP: Can have differently varying maps in x and y directions
   which could be represented as geolocation arrays when they are
   irregular. No timetable for update.

Changes to Warp API and gdalwarp
--------------------------------

Introduce a new geolocation array based transformation method, following
the existing GDALTransformer mechanism. A geolocation array transformer
will be created with the following function call. The "char \**" array
is the list of metadata from the GEOLOCATION metadata domain.

::

    void *GDALCreateGeoLocTransformer( GDALDatasetH hBaseDS, 
                                       char **papszGeolocationInfo,
                                       int bReversed );

This transformer is currently partially implemented, but in a manner
that potentially uses a great deal of memory (twice the memory needed
for the geolocation arrays), and with still dubious correctness, but
once approved this will be fixup up to at least be correct, though
likely not efficient for the time being.

The GDALGenImgProjTransformer will be upgraded to instantiate the GeoLoc
transformer (instead of an affine, gcp, or rpc transformer) if only
geolocation information is available (done). However, the current
GDALCreateGenImgProjTransformer() function does not provide a mechanism
to select which transformation mechanism is used. So, for instance, if
an affine transform is available it will be used in preference to
geolocation data. If bGCPUseOK is TRUE, gcps will be used in preference
to geolocation data.

The gdalwarp program currently always sets bGCPUseOK to TRUE so there is
no means for gdalwarp users select use of geolocation data in preference
to gcps. Some modification to gdalwarp may be needed in the future in
this regard.

Preserving Geolocation Through Translation
------------------------------------------

| ''How do we preserve access to geolocation information when
  translating a dataset? Do applications like gdal_translate need
  special handling?
| Placement of the geolocation data in a special metadata domain means
  it won't be transferred in default translations.''
