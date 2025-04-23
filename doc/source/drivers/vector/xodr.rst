.. _vector.xodr:

XODR -- OpenDRIVE Road Description Format
=========================================

.. versionadded:: 3.10

.. shortname:: XODR

.. build_dependencies:: libOpenDRIVE >= 0.6.0, GEOS

This driver provides read access to road network elements of ASAM OpenDRIVE datasets.

`ASAM OpenDRIVE <https://www.asam.net/standards/detail/opendrive/>`_ is an open industry format for lane-detailed description of road networks, commonly used in applications of the automotive and transportation systems domains. It bundles polynomial, continuous road geometry modelling with all necessary topological links and semantic information from traffic-regulating infrastructure (signs and traffic lights).

Driver capabilities
-------------------

.. supports_georeferencing::

Specification version
---------------------

The currently supported OpenDRIVE version is 1.4 and depends on what is provided by libOpenDRIVE_. 

.. _libOpenDRIVE: https://github.com/pageldev/libOpenDRIVE/

Supported OpenDRIVE elements
++++++++++++++++++++++++++++

The XODR driver exposes OpenDRIVE's different road elements as separate layers by converting geometric elements into 3-dimensional OGR geometry types. The following _`layer types` are currently implemented:

* *ReferenceLine*: Road reference line (``<planView>``) as :cpp:class:`OGRLineString`.
* *LaneBorder*: Outer road lane border as :cpp:class:`OGRLineString`.
* *Lane*: Polygonal surface (TIN) of the lane mesh as :cpp:class:`OGRTriangulatedSurface`.
* *RoadMark*: Polygonal surface (TIN) of the road mark mesh as :cpp:class:`OGRTriangulatedSurface`.
* *RoadObject*: Polygonal surface (TIN) of the road object mesh as :cpp:class:`OGRTriangulatedSurface`.
* *RoadSignal*: Polygonal surface (TIN) of the road signal mesh as :cpp:class:`OGRTriangulatedSurface`.

Spatial reference
-----------------

By definition, OpenDRIVE geometries are always referenced in a Cartesian coordinate system which defaults to having its origin at ``(0, 0, 0)``. If real-world coordinates are used, the OpenDRIVE header provides a PROJ.4 string with the definition of a projected spatial reference system:

::

  <header ...>
    <geoReference><![CDATA[+proj=tmerc +lat_0=0 +lon_0=9 +k=0.9996 +x_0=500000 +y_0=0 +datum=WGS84 +units=m +no_defs]]></geoReference>
  </header>

The XODR driver uses this PROJ definition as spatial reference for creation of all OGR geometry layers. 

Limitations
-----------

The supported content encoding of OpenDRIVE XML files is limited to what pugixml is able to automatically guess (see `4.6. Encodings <https://pugixml.org/docs/manual.html#loading.encoding>`_). The default fallback encoding is UTF-8.

Open options
------------

The following open options can be specified
(typically with the ``-oo name=value`` parameters of :program:`ogrinfo` or :program:`ogr2ogr`):

-  .. oo:: EPSILON
      :choices: <float>
      :default: 1.0

      Epsilon value ``> 0.0`` for linear approximation of continuous OpenDRIVE geometries. A smaller value results in a finer sampling. This parameter corresponds to libOpenDRIVE's ``eps`` parameter.

-  .. oo:: DISSOLVE_TIN
      :choices: YES, NO
      :default: NO

      Whether to dissolve triangulated surfaces. By setting this option to YES, the TIN layers *Lane* and *RoadMark* of geometry type :cpp:class:`OGRTriangulatedSurface` will be simplified to single, simple :cpp:class:`OGRPolygon` geometries. This performs a :cpp:func:`UnaryUnion` which dissolves boundaries of all touching triangle patches and thus yields a slimmer dataset which often suffices for basic GIS usage. Be aware that this dissolving step increases processing time significantly.
      Layer *RoadSignal* will be dissolved to a simple :cpp:class:`OGRPoint`.

Examples
--------

- Translate OpenDRIVE road *ReferenceLine* elements (``<planView>``) to :ref:`Shapefile <vector.shapefile>` using :program:`ogr2ogr`. The desired :ref:`layer type <layer types>` which is to be extracted from the dataset is specified as the last parameter of the function call. 

  ::

    ogr2ogr -f "ESRI Shapefile" CulDeSac.shp CulDeSac.xodr ReferenceLine

- Convert the whole OpenDRIVE dataset with all its different layers into a :ref:`GeoPackage <vector.gpkg>` using:

  ::

    ogr2ogr -f "GPKG" CulDeSac.gpkg CulDeSac.xodr

- Convert the whole OpenDRIVE dataset with custom parameters :oo:`EPSILON` and :oo:`DISSOLVE_TIN` into a :ref:`GeoPackage <vector.gpkg>`:

  ::

    ogr2ogr -f "GPKG" CulDeSac.gpkg CulDeSac.xodr -oo EPSILON=0.9 -oo DISSOLVE_TIN=YES

Convenient usage through docker image 
-------------------------------------

To use the XODR driver through a docker image, first build the image from the corresponding docker directory 
    
  ::

    cd <gdal>/docker/ubuntu-full/
    docker build -t gdal/xodr -f Dockerfile .

For general usage information refer to `GDAL Docker images <https://github.com/OSGeo/gdal/tree/master/docker#usage>`__. Usage examples:

- Use :program:`ogrinfo` to extract detailed information about a local `xodr` file by mounting your current working directory (`$PWD`) containing the file into the Docker container:
  
  ::

    docker run --rm -v ${PWD}:/home -it gdal/xodr ogrinfo /home/<file>.xodr

- Use :program:`ogr2ogr` to convert a local `xodr` file into any other supported OGR output format. The result will be automatically available in your host machine's working directory which is mounted into the container: 
  
  ::

    docker run --rm -v ${PWD}:/home -it gdal/xodr ogr2ogr -f "GPKG" /home/<file>.gpkg /home/<file>.xodr


Alternatively, you can run a docker container that enables using the XODR driver in an isolated workspace from within the container 
    
    ::

      docker run --name <container_name> -it gdal/xodr /bin/bash


General building notes
----------------------

Building of the driver as plugin is tested to work on

* Ubuntu 24.04 using GCC
* Windows 10 using GCC 13.1.0 (with MCF threads) + MinGW-w64 11.0.0 (MSVCRT runtime), which is obtainable from `WinLibs <https://winlibs.com/>`_.

Ensure to meet the following driver dependencies:

* PROJ
* GEOS
* libOpenDRIVE_ as shared library (built with CMake option ``-DBUILD_SHARED_LIBS=ON``)

Then, after checking out GDAL sources with this driver extension, create the build directory:

  ::

    cd <gdal>
    mkdir build
    cd build

From the build directory configure CMake to activate our XODR driver as plugin:

  ::

    cmake .. -DOGR_ENABLE_DRIVER_XODR_PLUGIN=TRUE -DOpenDrive_DIR=/path/to/libOpenDRIVE/installdir/cmake/

.. note:: The :file:`cmake/` path is usually automatically created when installing libOpenDRIVE and contains the necessary configuration files for inclusion into other project builds.

Now, build GDAL and install it:

  ::

    cmake --build .
    cmake --build . --target install

Afterwards you will find a new shared library file :file:`{path/to/GDAL/installdir}/lib/gdalplugins/ogr_XODR`.

Verifying a successful build
++++++++++++++++++++++++++++

Check if XODR driver is found:

  ::
    
    cd <gdal>/build/
    ./apps/ogrinfo --format XODR

This should print basic capabilities of the driver:

  ::

    Format Details:
      Short Name: XODR
      Long Name: OpenDRIVE - Open Dynamic Road Information for Vehicle Environment
      Supports: Vector
      Supports: Open() - Open existing dataset.
    <OpenOptionList>
      ...
    </OpenOptionList>

If you are on Linux, depending on your environment, you might experience linker errors like: 

  ::

    ERROR 1: libOpenDrive.so: cannot open shared object file: No such file or directory

In such cases ensure that your environment variable ``LD_LIBRARY_PATH`` points to the corresponding install directories of libOpenDRIVE and GDAL and run ``ldconfig`` afterwards.
