.. _about:

================================================================================
What is GDAL?
================================================================================

.. descriptionstartshere

GDAL is a translator library for raster and vector geospatial data formats that is released under an MIT style Open Source :ref:`license` by the `Open Source Geospatial Foundation`_. As a library, it presents a single raster abstract data model and single vector abstract data model to the calling application for all supported formats. It also comes with a variety of useful command line utilities for data translation and processing. The `NEWS`_ page describes the December 2025 GDAL/OGR 3.12.1 release.

Who is GDAL for?
----------------

GDAL is designed for anyone working with geospatial raster or vector data, including:

* GIS users converting data between formats (GeoTIFF, NetCDF, Shapefile, GeoPackage, etc.)
* Researchers processing satellite or aerial imagery
* Developers building geospatial applications in Python, C/C++, Java, or other languages
* Data engineers integrating geospatial workflows into larger data pipelines

GDAL is widely used in both open source and commercial software, and serves as the
core geospatial engine for many well-known GIS tools.

What can you do with GDAL?
--------------------------

Common GDAL use cases include:

* Converting between hundreds of raster and vector formats
* Reprojecting data between coordinate reference systems
* Cropping, resampling, and mosaicking large raster datasets
* Inspecting dataset metadata and structure
* Accessing geospatial data programmatically through stable APIs

Most users interact with GDAL through its command-line utilities
such as ``gdal_translate``, ``gdalwarp``, and ``ogr2ogr``,
or via the Python bindings for scripting and automation.

.. only:: html

.. note::

    The GDAL project is currently soliciting feedback to help focus activities.
    We would highly appreciate you fill in the `survey <https://gdal.org/survey/>`__ that will
    provide guidance about priorities for the program's resources (open until end of December 2025).
    Five T-shirts will be distributed to randomly chosen respondents who leave their email!

   |offline-download|

.. image:: ../images/OSGeo_project.png
   :alt:   OSGeo project
   :target:  `Open Source Geospatial Foundation`_

.. _`Open Source Geospatial Foundation`: http://www.osgeo.org/
.. _`NEWS`: https://github.com/OSGeo/gdal/blob/v3.12.1/NEWS.md

See :ref:`software_using_gdal`

.. |DOI| image:: ../images/zenodo.5884351.png
   :alt:   DOI 10.5281/zenodo.5884351
   :target: https://doi.org/10.5281/zenodo.5884351

You may quote GDAL in publications by using the following Digital Object Identifier: |DOI|
