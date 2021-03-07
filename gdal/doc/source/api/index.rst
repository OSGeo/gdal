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

       python_api_ref
       python_gotchas


   `Java API <../java/index.html>`_
   --------------------------------


   GDAL/OGR In Other Languages
   ++++++++++++++++++++++++++++

   There is a set of generic `SWIG <http://www.swig.org/>`__ interface files in the GDAL source tree (subdirectory swig) and a set of language bindings based on those. Currently active ones are 
   `CSharp <https://trac.osgeo.org/gdal/wiki/GdalOgrInCsharp/>`__, `Java <https://trac.osgeo.org/gdal/wiki/GdalOgrInJava>`__, `Perl <https://trac.osgeo.org/gdal/wiki/GdalOgrInPerl>`__, and `Python <https://trac.osgeo.org/gdal/wiki/GdalOgrInPython>`__.

   There are also other bindings that are developed outside of the GDAL source tree. These include bindings for `Go <https://github.com/lukeroth/gdal>`__, `Julia <https://github.com/JuliaGeo/GDAL.jl>`__, `Lua <https://trac.osgeo.org/gdal/wiki/GdalOgrInLua>`__, `Node.js <https://github.com/naturalatlas/node-gdal>`__, `Perl <https://metacpan.org/release/Geo-GDAL-FFI>`__, `PHP <http://dl.maptools.org/dl/php_ogr/php_ogr_documentation.html>`__, `R <http://cran.r-project.org/web/packages/rgdal/index.html>`__ (rgdal in SourceForge is out of date), and `Tcl <https://trac.osgeo.org/gdal/wiki/GdalOgrInTcl>`__. There are also more Pythonic ways of using the vector/OGR functions with `Fiona <https://github.com/Toblerity/Fiona>`__ and the raster/GDAL ones with `Rasterio <https://github.com/mapbox/rasterio>`__ (**note**: those offer APIs not strictly coupled the GDAL/OGR C/C++ API)

.. only:: latex

    API is omitted in this PDF document. You can consult it on
    https://gdal.org/api/index.html
