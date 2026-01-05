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

    Arc

        A curved segment of a circle or other curve, often used in geographic data to represent smooth curved lines or boundaries between points.

    Band

        A single layer of data values within a raster dataset. In a single-band raster, each pixel contains one value - such as elevation in a :term:`DEM`.
        A multi-band raster contains multiple such layers; for example, satellite imagery may include three bands representing Red, Green, and Blue channels.

    Band Algebra

        A method for performing mathematical operations on one or more raster bands to produce a new output band or raster.
        Band algebra involves applying expressions - such as addition, subtraction, multiplication, division,
        or more complex functions—on pixel values across bands.

        .. seealso::
            :ref:`gdal_band_algebra`.

    Block Cache

        A memory cache that stores recently accessed blocks of data to reduce disk reads and improve performance.

    Boolean

        A data type representing one of two values: True or False, commonly used in logical operations and conditions.

    Coordinate Epoch

        The date tied to spatial coordinates in a dynamic reference frame, used to account for positional changes over time (e.g., due to tectonic motion).

    CPL

        Common Portability Library. Part of the C API that provides convenience
        functions to provide independence from the operating systems on which GDAL runs.
        Back in the early days of GDAL development, when cross-platform development
        as well as compatibility and standard conformance of compilers was a challenge,
        CPL proved necessary for smooth portability of GDAL/OGR.

        CPL, or parts of it, is used by some projects external to GDAL (eg. MITAB, libgeotiff).

    Credentials

        Information (such as a username and password or tokens) used to verify the identity of a user or system for authentication and access control.

    CRS

        Coordinate Reference System. A system that maps spatial data coordinates to real-world locations,
        combining a coordinate system with a reference surface like a projection or :term:`ellipsoid`.

    CSL

        C-String List. Prefix used by a number of functions in the C API, that deal with
        an array of NUL-terminated strings, the array itself being terminated by a NULL pointer.

    curl

        A command-line tool and library for transferring data with URLs, supporting protocols such as HTTP, HTTPS, FTP, and more.
        Commonly used for testing and interacting with web services.

    DEM

        Digital Elevation Model. A raster representation of the height of the earth's surface.

    Driver

        A software component that enables reading, writing, and processing of specific raster or vector data formats.

    DTM

        Digital Terrain Model. A raster representation of the bare ground surface of the earth, excluding natural and man-made
        objects such as vegetation and buildings.

    Ellipsoid

        A model used to approximate the Earth's shape in coordinate systems.

    Environment Variable

        A variable in the operating system that can influence the behavior of running processes or applications.

        .. seealso::
            :ref:`configoptions`.

    Escaped Character

        Refers to a character or sequence modified to prevent it from being interpreted as code or control instructions,
        allowing it to be treated as literal data.

    Euclidean Distance

        A measure of the straight-line distance between two points in space.

    EPSG

        European Petroleum Survey Group. A group of geospatial experts for the petroleum industry (1986–2005),
        now part of IOGP (International Association of Oil & Gas Producers). Known for creating the EPSG Geodetic Parameter Dataset,
        a widely used database of coordinate systems and geodetic parameters.

    File Handle

        An identifier used by an operating system to manage and access an open file during a program's execution.

    GEOS

        Geometry Engine - Open Source. GEOS is a C/C++ library for computational geometry with a focus on algorithms
        used in geographic information systems (GIS) software. GEOS started as a port of the Java Topology Suite (JTS).

        .. seealso::
            https://libgeos.org/

    Gridding

        The process of converting scattered or irregularly spaced data points into a regular, structured grid format.

        .. seealso::
            :ref:`gdal_grid_tut`.

    Georeference

        Linking data to real-world geographic coordinates so a location can be accurately mapped.


    Georeferencing

        See :term:`Georeference`.

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

    Library

        A collection of software routines, functions, or classes that can be used by programs to perform specific tasks
        without having to write code from scratch. Libraries help developers reuse code and simplify software development.

    Linestring

        A geometric object consisting of a sequence of connected points forming a continuous line, commonly used to represent
        linear features such as roads or rivers in spatial data.

    LRU Cache

        Least Recently Used Cache. A memory cache that stores a limited number of items, automatically discarding the
        least recently used entries to make space for new ones.

    Moving Average

        A method that smooths data by averaging values over a sliding window of data points.

        .. seealso::
            :ref:`gdal_grid_tut`.

    Multithreading

        A programming technique where multiple threads are executed concurrently within a single process,
        allowing parallel execution of tasks to improve performance and responsiveness.

    Normalizing

        The process of adjusting data values to a common scale. In raster analysis, normalizing is commonly used to
        scale pixel values to a defined range (such as 0 to 1) to facilitate comparison and visualization.

    Nearest Neighbor

        A method that finds the closest data point(s) to a given point, often used for classification or estimation based on similarity.

    OGC

        Open Geospatial Consortium. An international, non-profit organization that develops and promotes open standards
        for geospatial data and services.

        .. seealso::
            https://www.ogc.org/

    OGR

        Open GIS Reference. OGR used to stand for OpenGIS Simple Features Reference
        Implementation. However, since OGR is not fully compliant with the
        OpenGIS Simple Feature specification and is not approved as a reference
        implementation of the spec the name was changed to OGR Simple Features Library.
        The only meaning of OGR in this name is historical. OGR is also the
        prefix used everywhere in the source of the library for vector related
        functionality.

    OSR

        OGR Spatial Reference (OSR) - module that handles spatial reference systems and coordinate transformations.

        .. seealso::
            :ref:`osr_api_tut`.

    PAM

        Persistent Auxiliary Metadata. Metadata stored separately from the main raster data file,
        allowing additional information to persist without modifying the original file.

    Raster

        A type of spatial data used with GIS, consisting of a regular grid of points spaced at a set distance (the resolution);
        often used to represent heights (DEM) or temperature data.

    Raster Algebra

        See :term:`Band Algebra`.

    Resampling

        The process of changing the resolution or grid alignment of raster data by interpolating pixel values,
        used when scaling, reprojecting, or transforming images to maintain data quality.

    RGB

        An acronym for Red, Green, and Blue - the three primary colors of light used in digital imaging.

    Runtime

        The period during which a program or process is actively executing. It refers to the time from the start of a program's execution to its termination.

    Search Ellipse

        :term:`Search window` in :term:`gridding` algorithms specified in the form of rotated ellipse.

        .. seealso::
            :ref:`gdal_grid_tut`.

    Search Window

        A defined area within which data is analyzed or searched, often used in spatial analysis or image processing.

        .. seealso::
            :ref:`gdal_grid_tut`.

    Shell

        A command-line interface used to interact with an operating system by typing commands.

    Side-car Files

        Auxiliary files stored alongside a primary data file that contain metadata or additional information without
        altering the original file.

    SRS

        Spatial Reference System. A system that defines how spatial coordinates map to real-world locations.
        Often used interchangeably with :term:`CRS`, though CRS is the more precise term in modern geospatial standards.

    SSL

        Secure Sockets Layer. A security protocol that encrypts data transmitted over the Internet
        to ensure privacy and data integrity between a client and a server.

    stdout

        Standard output stream used by programs to display output data, typically shown on the console or terminal.

    strile

        A combination of "strip" and "tile". It is a block of raster data stored in a TIFF file, either as a row of pixels (strip) or a square/rectangle (tile).
        GDAL reads and writes each strile as a unit when working with Cloud Optimized GeoTIFFs (COGs).

        .. seealso::
            :ref:`raster.cog`.

    Swath

        A contiguous block or strip of raster data processed or read at one time.

    Thread

        A sequence of executable instructions within a program that can run independently, often used to perform tasks concurrently for better performance.

    Topology

        The study and representation of spatial relationships between geometric features, such as adjacency, connectivity, and containment,
        ensuring data integrity in GIS by defining how points, lines, and polygons share boundaries and connect.

    User-Agent

        A string sent by a web browser or client identifying itself to a web server, often including information
        about the software, version, and operating system.

    UTF8

        A character encoding that represents text using one to four bytes per character, and capable of encoding all Unicode characters. Also
        referred to a UTF-8.

    VRT

        Virtual Raster Tile: A lightweight XML-based GDAL format that references multiple rasters or vectors to create a
        virtual mosaic without duplicating data.

        .. seealso::
            :ref:`raster.vrt` and :ref:`vector.vrt`

    VSI

        Virtual System Interface. This is the name of the input/output abstraction layer
        that is implemented by different Virtual File Systems (VFS) to provide access
        to regular files, in-memory files, network accessible files, compressed files, etc.

        The VSI functions retain exactly the same calling pattern as the original
        Standard C functions they relate to, for the parts where they are common, and
        also extend it to provide more specialized functionality.

        .. seealso::
            :ref:`virtual_file_systems`.

    Warping

        The process of geometrically transforming raster data to align with a different coordinate system, projection,
        or spatial reference, often involving :term:`resampling` of pixel values.

        .. seealso::
            :ref:`gdalwarp`.

    WFS

        Web Feature Service, an :term:`OGC` standard that allows users to access geospatial vector data over the web.

        .. seealso::
            :ref:`vector.wfs`

    WKT

        Well-Known Text. Text representation of geometries described in the Simple Features for SQL (SFSQL) specification.

    WKT-CRS

        Well-Known Text for Coordinate Reference Systems. A text format that defines how to describe coordinate reference
        systems and transformations between them in a standardized way.
        See the `OGC WKT-CRS standard <https://www.ogc.org/standards/wkt-crs/>`__.

    WKB

        Well-Known Binary. Binary representation of geometries described in the Simple Features for SQL (SFSQL) specification.

    WMS

        Web Map Service, an :term:`OGC` standard that allows users to request and display georeferenced map images over the web.

        .. seealso::
            :ref:`raster.wms`


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
