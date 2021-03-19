.. _gdalogrin_csharp:

================================================================================
GDAL/OGR In CSharp
================================================================================

The GDAL project (primarily Tamas Szekeres) maintains SWIG generated CSharp bindings for GDAL and OGR. Generally speaking the classes and methods mostly match

those of the GDAL and OGR C++ classes, but there is currently no CSharp specific documentation beyond this site. The CSharp bindings are also usable from other
.NET languages, such as VB .Net.

The CSharp interface has been build upon a common ground as the other SWIG generated wrappers (like Perl, Python, Java, PHP and Ruby). In this regard the class names
and class member names along with the method signatures are driven by the GDAL+SWIG conventions, and might not follow the conventional .NET naming guidelines.
However, one can easily identify the matching members in the GDAL/OGR API documentation.

The GDAL/OGR CSharp classes use the .NET P/Invoke mechanism for the communication between the managed and unmanaged code. Every class implements the IDisposable
interface to control the finalization of the underlying unmanaged memory referenced by every the wrapper class.

Supported platforms
-------------------

Currently the interface is compilable and supports the various Win32 and Win64 platforms targeting the Microsoft.NET and the MONO frameworks. The interface is also
compilable under the GNU Linux/OSX systems for the MONO framework. The compilation steps for the various platforms have been added to the ​`GDAL buildbot <http://buildbot.osgeo.org:8500/>`__ and tested
regularly.

Related Documents
-----------------

* `GDAL/OGR CSharp interface versions <http://trac.osgeo.org/gdal/wiki/GdalOgrCsharpVersions>`__
* `GDAL/OGR CSharp Raster Operations <http://trac.osgeo.org/gdal/wiki/GdalOgrCsharpRaster>`__
* `GDAL/OGR CSharp Compilation <http://trac.osgeo.org/gdal/wiki/GdalOgrCsharpCompile>`__
* `Using the GDAL/OGR CSharp interface <http://trac.osgeo.org/gdal/wiki/GdalOgrCsharpUsage>`__

Useful Links
------------

* A variety of example programs in CSharp are available at the `/swig/csharp/apps <https://github.com/OSGeo/gdal/tree/master/gdal/swig/csharp/apps>`__ folder of the GDAL project tree.

* This project make images tiles (Superoverlay method) for Google Earth ​`GdalToTiles C# <https://archive.codeplex.com/?p=gdal2tilescsharp>`__

FWTools
-------

One way to get the CSharp bindings is to use FWTools 1.2.0+ for windows. To use these it should be sufficient to add the assemblies in FWTools\csharp to your project,
and to ensure that FWTools\bin is in your path. See ​`FWTools: Open Source GIS Binary Kit for Windows and Linux <http://fwtools.maptools.org>`__


Windows Build SDKs
------------------

Tamas Szekeres maintains ​`build SDK packages <http://www.gisinternals.com/sdk>`__ in order to compile GDAL from the sources on Windows. The build system provides daily
build binary packages for the latest stable and development versions.
