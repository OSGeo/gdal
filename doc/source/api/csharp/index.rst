.. _csharp:

================================================================================
C# bindings
================================================================================

The GDAL project (primarily Tamas Szekeres) maintains SWIG generated C# bindings for GDAL and OGR.

Generally speaking the classes and methods mostly match those of the GDAL and OGR C++ classes, but there is currently no C# specific documentation beyond this site.

The C# bindings are also usable from other .NET languages, such as VB.Net.

The C# interface has been built upon the same libraries as the other SWIG generated wrappers (like Perl, Python, Java, PHP and Ruby). Therefore, the class names,
class member names, and the method signatures are driven by the GDAL+SWIG conventions and might not follow the conventional .NET naming guidelines.
However, one can easily identify the matching members in the GDAL/OGR API documentation.

The GDAL/OGR C# classes use the .NET P/Invoke mechanism for the communication between the managed and unmanaged code. Every class implements the IDisposable
interface to control the finalization of the underlying unmanaged memory referenced by every the wrapper class.

Supported platforms
-------------------

Currently the interface is compilable on and supports:

* the various Win32 and Win64 platforms targeting the Microsoft.NET and the MONO frameworks, 
* GNU Linux/OSX systems using the MONO framework, and
* Unity systems on Windows, OSX and Linux (currently only the MONO framework and not IL2CPP).

Getting GDAL for C#
-------------------

There are a number of ways to get the C# bindings, including but not limited to:

* The `gisinternals <http://www.gisinternals.com/sdk>`__ site, see below under "Windows Build SDK",
* The `Conda package <https://anaconda.org/conda-forge/gdal-csharp>`__, see instructions below
* The gdal.netcore NuGet package, see link below, and
* For Unity, there is a UPM package that installs GDAL, `available from here <https://openupm.com/packages/com.virgis.gdal/?subPage=readme>`__ (available on Windows, Mac and Linux)

(all of these are community supported)


Related Documents
-----------------
   .. toctree::
       :maxdepth: 1

       csharp_compile_legacy
       csharp_compile_cmake
       csharp_raster
       csharp_vector
       csharp_usage
       csharp_conda


Useful Links
------------

* A variety of example programs in CSharp are available at the `/swig/csharp/apps <https://github.com/OSGeo/gdal/tree/master/swig/csharp/apps>`__ folder of the GDAL project tree.

* The Conda Feedstock 

* A simple (as is) build engine of GDAL 3.2 library for .NET Core. `MaxRev-Dev/gdal.netcore <https://github.com/MaxRev-Dev/gdal.netcore>`__ 

* The `ViRGiS project <https://www.virgis.org/>`__ makes extensive use of GDAL in c# in a Unity environment.

(Please add your project to this section)


Windows Build SDKs
------------------

Tamas Szekeres maintains `build SDK packages <http://www.gisinternals.com/sdk>`__ in order to compile GDAL from the sources on Windows. The build system provides daily
build binary packages for the latest stable and development versions.
