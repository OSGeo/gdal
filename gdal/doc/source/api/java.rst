.. _java:

================================================================================
Java bindings
================================================================================

The GDAL project has SWIG generated Java bindings for GDAL and OGR.

Generally speaking the classes and methods mostly match those of the GDAL and OGR C++ classes. You can find the `​Javadoc <http://gdal.org/java>`__ of the API of the Java bindings for
GDAL 1.7.0 and later releases.

Due to the fact the Java garbage collector works in a separate thread from the main thread, it is necessary to configure GDAL with multi-threading support,
even if you do not use GDAL API from several Java threads.

The minimum version of Java required to build the Java bindings is Java 7


Maven Users
-----------

The Java bindings are available from the ​`Maven Central <http://search.maven.org/>`__ repository. All that is needed is to declare a dependency.


.. code-block:: xml


   <dependency>
      <groupId>org.gdal</groupId>
      <artifactId>gdal</artifactId>
      <version>1.11.2</version>
   </dependency>


Useful Links
------------

* ​`Javadoc <http://gdal.org/java>`__ of the API of the Java bindings for GDAL 1.7.0 and later releases.
* `gdalinfo.java <https://github.com/OSGeo/gdal/tree/master/gdal/swig/java/apps/gdalinfo.java>`__ Sample Java program similar to gdalinfo utility (use API from Java bindings of GDAL 1.7.0dev)
* `All Java sample programs <https://github.com/OSGeo/gdal/tree/master/gdal/swig/java/apps/>`__
* `Tamas Szekeres' Windows daily builds <http://www.gisinternals.com/sdk>`__ : Tamas Szekeres maintains a complete set of Win32 and Win64 binary packages (compiled with VC2003/VC2005/VC2008/VC2010) that include the GDAL Java bindings. These packages are based on the current development and stable branches built from the GDAL SVN daily. The -devel packages are based on the development version (1.9.0dev at time of writing), and the -stable packages are based on the latest stable branch (1.8 at time of writing)
* `Image I/O-Ext <https://imageio-ext.dev.java.net/>`__ : The main core module of the project is gdalframework, a framework leveraging on GDAL via SWIG's generated JAVA bindings to provide support for a reach set of data formats. (**Note**: this framework doesn't necessarily ship the latest released GDAL version)
* `GdalOgrInJavaBuildInstructions <https://trac.osgeo.org/gdal/wiki/GdalOgrInJavaBuildInstructions>`__ : Complete instructions for building GDAL's Java bindings from scratch on **Windows**.
* `GdalOgrInJavaBuildInstructionsUnix <https://trac.osgeo.org/gdal/wiki/GdalOgrInJavaBuildInstructionsUnix>`__ : Build instructions for java on Unix/Linux?/MinGW.
