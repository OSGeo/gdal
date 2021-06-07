.. _api:

================================================================================
API
================================================================================

.. only:: not latex

   `Full Doxygen output <../doxygen/index.html>`_
   ----------------------------------------------

   C API
   -----

   .. toctree::
       :maxdepth: 1

       cpl
       raster_c_api
       vector_c_api
       gdal_alg
       ogr_srs_api
       gdal_utils

   C++ API
   -------

   Raster API
   +++++++++++++++++++++++++++

   .. toctree::
       :maxdepth: 1

       gdaldriver_cpp
       gdaldataset_cpp
       gdalrasterband_cpp
       gdalwarp_cpp

   Vector API
   +++++++++++++++++++++++++++

   .. toctree::
       :maxdepth: 1

       ogrfeature_cpp
       ogrfeaturestyle_cpp
       ogrgeometry_cpp
       ogrlayer_cpp

   Spatial reference system API
   ++++++++++++++++++++++++++++

   .. toctree::
       :maxdepth: 1

       ogrspatialref

   Multi-dimensional array API
   +++++++++++++++++++++++++++

   .. toctree::
       :maxdepth: 1

       gdalgroup_cpp
       gdaldimension_cpp
       gdalabstractmdarray_cpp
       gdalmdarray_cpp
       gdalattribute_cpp
       gdalextendeddatatype_cpp

   Miscellaneous C++ API
   ++++++++++++++++++++++++++++

   .. toctree::
       :maxdepth: 1

       cpl_cpp
       gnm_cpp

   Python API
   ----------

   .. toctree::
       :maxdepth: 1

       python
       python_api_ref
       python_gotchas
       python_samples


   `Java API <../java/index.html>`_
   --------------------------------


   GDAL/OGR In Other Languages
   ---------------------------

   There is a set of generic `SWIG <http://www.swig.org/>`__ interface files in the GDAL source tree (subdirectory swig) and a set of language bindings based on those. Currently active ones are:

   .. toctree::
       :maxdepth: 1

       csharp/index
       java/index

   There are also other bindings that are developed outside of the GDAL source tree (**note**: those offer APIs not strictly coupled the GDAL/OGR C/C++ API). These include bindings for

      .. toctree::
       :maxdepth: 1

       Go <https://github.com/lukeroth/gdal>
       Julia <https://github.com/JuliaGeo/GDAL.jl>
       Lua <https://trac.osgeo.org/gdal/wiki/GdalOgrInLua>
       Original Node.js bindings <https://github.com/naturalatlas/node-gdal>
       Node.js fork with full Promise-based async and TypeScript support <https://www.npmjs.com/package/gdal-async>
       Perl <https://metacpan.org/release/Geo-GDAL-FFI>
       PHP <http://dl.maptools.org/dl/php_ogr/php_ogr_documentation.html>
       R <http://cran.r-project.org/web/packages/rgdal/index.html>

    .. warning::
        For Perl, since GDAL 3.5 the link `Perl <https://trac.osgeo.org/gdal/wiki/GdalOgrInPerl>`__ is deprecated, use above link instead.

        For R, rgdal in SourceForge is out of date.



    There are also more Pythonic ways of using the vector/OGR functions with

      .. toctree::
       :maxdepth: 1

       Fiona <https://github.com/Toblerity/Fiona>
       Rasterio <https://github.com/mapbox/rasterio>

    There is a more idiomatic Golang way of using the raster functions with

      .. toctree::
       :maxdepth: 1

       Godal <https://github.com/airbusgeo/godal>

.. only:: latex

    API is omitted in this PDF document. You can consult it on
    https://gdal.org/api/index.html
