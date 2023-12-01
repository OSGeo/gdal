.. _java:

================================================================================
Java bindings
================================================================================

The GDAL project has SWIG generated Java bindings for GDAL and OGR.

Generally speaking the classes and methods mostly match those of the GDAL and OGR C++ classes. You can find the `​Javadoc <http://gdal.org/java>`__ of the API of the Java bindings.

Due to the fact the Java garbage collector works in a separate thread from the main thread, it is necessary to configure GDAL with multi-threading support, even if you do not use GDAL API from several Java threads.

How to build bindings
---------------------

Please consult the CMake :ref:`building_from_source_java` paragraph for CMake
options controlling how to enable the Java bindings and where to install its
artifacts

How to use the bindings
-----------------------

The result of the build of the Java bindings will be both a :file:`gdal.jar`
and a companion :file:`libgdalalljni.so` / :file:`libgdalalljni.dylib` /
:file:`gdalalljni.dll` native library. To limit potential compatibility problems,
you should ensure that gdal.jar and gdalalljni come from the same GDAL sources.

The native gdalalljni library, as well as the core libgdal library (and its
dependencies) should be accessible through the mechanism of the operating
system to locate shared libraries.
Typically on Linux, this means that the path to those libraries should be set
in the ``LD_LIBRARY_PATH`` environment variable (or in :file:`/etc/ld.so.conf`).
On MacOSX, it should be in the ``DYLD_LIBRARY_PATH`` environment variable.
And on Windows, in the ``PATH`` environment variable.

For example, to test on Linux that the bindings are working, you can lanch,
from the build directory:

::

    export LD_LIBRARY_PATH=$PWD:$PWD/swig/java:$LD_LIBRARY_PATH
    java -classpath $PWD/swig/java/gdal.jar:$PWD/swig/java/build/apps gdalinfo

On Windows:

::

    set PATH=%CD%;%CD%\swig\java;%PATH%
    java -classpath %CD%\swig\java\gdal.jar;%CD%\swig\java\build\apps gdalinfo


Maven Users
-----------

The Java bindings are available from the ​`Maven Central <http://search.maven.org/>`__ repository. All that is needed is to declare a dependency.


.. code-block:: xml


   <dependency>
      <groupId>org.gdal</groupId>
      <artifactId>gdal</artifactId>
      <version>3.8.0</version>
   </dependency>


Useful Links
------------

* ​`Javadoc <http://gdal.org/java>`__ of the API of the Java bindings.
* `gdalinfo.java <https://github.com/OSGeo/gdal/tree/master/swig/java/apps/gdalinfo.java>`__ Sample Java program similar to gdalinfo utility.
* `All Java sample programs <https://github.com/OSGeo/gdal/tree/master/swig/java/apps/>`__
* `Tamas Szekeres' Windows daily builds <http://www.gisinternals.com/sdk>`__ : Tamas Szekeres maintains a complete set of Win32 and Win64 binary packages that include the GDAL Java bindings. These packages are based on the current development and stable branches built from the GDAL source repository.
* `Image I/O-Ext <https://imageio-ext.dev.java.net/>`__ : The main core module of the project is gdalframework, a framework leveraging on GDAL via SWIG's generated JAVA bindings to provide support for a reach set of data formats. (**Note**: this framework doesn't necessarily ship the latest released GDAL version)
