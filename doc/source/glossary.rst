.. _glossary:

================================================================================
Glossary
================================================================================

The GDAL glossary contains terms and acronyms found throughout the GDAL documentation

.. glossary::

    Affine Transformation

        A geometric change that preserves points, straight lines, and ratios, including scaling, rotating, or translating.

    API

        Application Programming Interface. A set of rules that lets different software programs communicate and share data or functions.

    DEM

        Digital Elevation Model. A raster representation of the height of the earth's surface.

    Euclidean Distance

        A measure of the straight-line distance between two points in space.

    EPSG

        European Petroleum Survey Group. A group of geospatial experts for the petroleum industry (1986–2005),
        now part of IOGP (International Association of Oil & Gas Producers). Known for creating the EPSG Geodetic Parameter Dataset,
        a widely used database of coordinate systems and geodetic parameters.

    Gridding

        The process of converting scattered or irregularly spaced data points into a regular, structured grid format.

        .. seealso::
            :ref:`gdal_grid_tut`.

    Georeference

        Linking data to real-world geographic coordinates so a location can be accurately mapped.

    Geotransform

        A set of parameters that defines how to convert pixel locations in an image to real-world geographic coordinates.

        .. seealso::
            :ref:`geotransforms_tut`.

    GNM

        Geographic Network Model. A GDAL abstraction for different existed network formats.

        .. seealso::
            :ref:`gnm_data_model`.

    Interpolation

        A mathematical and statistical technique used to estimate unknown values between known values.

        .. seealso::
            :ref:`gdal_grid_tut`.

    Hypsometric

        The measurement and representation of land elevation (or terrain height) relative to sea level.

    Moving Average

        A method that smooths data by averaging values over a sliding window of data points.

        .. seealso::
            :ref:`gdal_grid_tut`.

    Nearest Neighbor

        A method that finds the closest data point(s) to a given point, often used for classification or estimation based on similarity.

    Raster

        A type of spatial data used with GIS, consisting of a regular grid of points spaced at a set distance (the resolution);
        often used to represent heights (DEM) or temperature data.

    Search Ellipse

        :term:`Search window` in :term:`gridding` algorithms specified in the form of rotated ellipse.

        .. seealso::
            :ref:`gdal_grid_tut`.

    Search Window

        A defined area within which data is analyzed or searched, often used in spatial analysis or image processing.

        .. seealso::
            :ref:`gdal_grid_tut`.

    VSI

        Virtual System Interface. An interface for accessing files and datasets in non-filesystem locations, such as
        in-memory files, zip files, and over network protocols.

        .. seealso::
            :ref:`virtual_file_systems`.

    WKT

        Well-Known Text. Text representation of geometries described in the Simple Features for SQL (SFSQL) specification.

    WKT-CRS

        Well-Known Text for Coordinate Reference Systems. A text format that defines how to describe coordinate reference
        systems and transformations between them in a standardized way.
        See the `OGC WKT-CRS standard <https://www.ogc.org/standards/wkt-crs/>`__.

    WKB

        Well-Known Binary. Binary representation of geometries described in the Simple Features for SQL (SFSQL) specification.


Credits and Acknowledgments
---------------------------

Some definitions have been created with the help of the following resources.

+ `Introduction to Spatial Data and Using R as a GIS <https://github.com/nickbearman/intro-r-spatial-analysis/blob/main/glossary.tex>`__ by Nick Bearman,
  licensed under the `Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International <http://creativecommons.org/licenses/by-nc-sa/4.0/>`__
+ `The Good Docs Project Glossary Initiative <https://drive.google.com/drive/folders/1v5ir_VrR71RFxR8ipf9xmpIIH8muEvEK>`__,
  by various contributors to the project, used under the following `Terms of Use <https://www.thegooddocsproject.dev/terms-of-use>`__.
+ The `MapServer Glossary <https://mapserver.org/glossary.html>`__ covered by the `MapServer Licensing <https://mapserver.org/copyright.html>`__.

.. spelling:word-list::
    Bearman
