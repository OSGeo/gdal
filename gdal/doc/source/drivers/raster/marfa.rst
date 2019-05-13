.. _raster.marfa:

MRF -- Meta Raster Format
=========================

Integrated in GDAL >= 2.1. Available as plugin for prior versions.

Access to a indexed heap of regular tiles (blocks). Controlled by an xml
file, usually organized as a pyramid of overviews, with level zero being
the full resolution image. None, PNG, JPEG, ZLIB tile packing are
implemented

For file creation options, see "gdalinfo --format MRF"

Links
-----

-  `MRF User
   Guide <https://github.com/nasa-gibs/mrf/blob/master/src/gdal_mrf/frmts/mrf/MUG.md>`__
-  `MRF
   Specification <https://github.com/nasa-gibs/mrf/blob/master/spec/mrf_spec.md>`__
-  `Source repository
   nasa-gibs/mrf <https://github.com/nasa-gibs/mrf>`__
