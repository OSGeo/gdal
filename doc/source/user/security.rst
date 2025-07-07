.. _security:

================================================================================
Security considerations
================================================================================

This page discusses some security issues that users or developers may face while
using GDAL. It can also serve as a check-list of items to take care of, for
developers coding new drivers.

While GDAL maintainers take a number of steps to limit the risk of software
vulnerabilities, GDAL has a large attack surface, because it deals with data in
a variety of file formats.

.. note::

    This page is not intended to be a comprehensive cook-book for being "safe" in all
    circumstances, and should be considered as work-in-progress
    that can benefit from continued feedback of people deploying GDAL.
    Although they are specialists of the GDAL code base, GDAL maintainers are not
    security experts.

Classes of potential vulnerabilities
------------------------------------

- Arbitrary code execution with the privileges of the user running the GDAL
  process.

- Theft, tampering or destruction of data.

- Denial of Service : abortion of a process, high consumption of CPU, memory,
  I/O resource.

- Unwanted network access.

Causes
------

- Software bugs - in GDAL code itself, or in third-party dependencies - related
  to code that processes user data. Common defects can be:

   * stack or heap buffer overflows, potentially leading to arbitrary code
     execution.

   * excessive memory allocation

   * infinite, or very long, loops in code

- Software functionalities themselves, depending on the context of use, can
  be a source of vulnerabilities. Consult :ref:`security_known_issues` below.

Situations at risk
------------------

The following are examples of situations where a user may process untrusted data
that has been crafted to trigger a vulnerability:

- web services accepting input files provided by a client,

- desktop use of GDAL where an hostile party manages to convince the victim
  user to process hostile data.

Mitigation
----------

- Upgrade to the latest versions of GDAL and its dependencies that might contain
  bug fixes for some vulnerabilities.

- Build GDAL with the hardening options of compilers, e.g. with -D_FORTIFY_SOURCE=2
  (some Linux distributions turn on it by default), to minimize the effect of
  buffer overflows.

- Restrict access to the part of the file system that are only needed. The ``​chroot``
  mechanism, or other sand-boxing solutions, might be a technical solution for this.

- Compile only the subset of drivers needed. The ``GDAL_ENABLE_DRIVER_{name}``
  or ``OGR_ENABLE_DRIVER_{name}`` CMake build options can be used for that purpose.

- Disable unneeded drivers at run-time by setting the :config:`GDAL_SKIP`
  configuration option.

- When network access is not needed, disable Curl at build time with the
  ``GDAL_USE_CURL=OFF`` CMake option.

- Process untrusted data in a dedicated user account with no access to other
  local sensitive data (data files, passwords, encryption/signing keys, etc...).
  Note that the mere act of opening a dataset with the GDAL/OGR API or utilities
  is a form of processing.

- For automatic services, place restrictions on the CPU time and memory
  consumption allowed to the process using GDAL. Check the options of your
  HTTP server.

- Check the raster dimensions and number of bands just after opening a GDAL
  driver and before processing it. Several GDAL drivers use the
  :cpp:func:`GDALCheckDatasetDimensions` and :cpp:func:`GDALCheckBandCount`
  functions to do early sanity checks, but user checks can also be useful.
  For example, a dataset with huge raster dimensions but with a very small file
  size might be suspicious (but not always, for example a VRT file, or highly
  compressed data...)

- Do not allow arbitrary arguments to be passed to command line utilities.
  In particular ``--config GDAL_DRIVER_PATH xxx`` or ``--config OGR_DRIVER_PATH xxx``
  could be used to trigger arbitrary code execution for someone with write
  access to some parts of the file system. Similarly, :config:`GDAL_VRT_ENABLE_PYTHON`
  or :config:`GDAL_VRT_PYTHON_TRUSTED_MODULES` could be used to trigger hostile
  Python code execution.

- When possible, avoid using closed-source dependency libraries that cannot be
  audited for vulnerabilities.

.. _security_known_issues:

Credential related issues
-------------------------

GDAL network virtual file systems can access credentials needed to grant access
to remote resources, from the environment or local files. Those secrets are
stored in RAM using normal mechanisms. Consequently they could potentially be
recovered by untrusted code that would run in the same process as GDAL code.

The same applies for connection strings to some drivers such as PostgreSQL, MySQL, etc.
which may include passwords.

Known issues in API
-------------------

- :cpp:func:`OGRSpatialReference::SetFromUserInput` accepts URLs by default

Known issues in drivers
-----------------------

General issues
+++++++++++++++

- Drivers do not always use file extensions to determine which file must be
  handled by which driver (this is a feature in most situations). But,
  for example, a VRT file might be disguised as a .tif, .png, or .jpg file.
  So you cannot know which driver will handle a file by just looking at its
  extension. Using ``gdalmanage identify the.file``
  (or :cpp:func:`GDALIdentifyDriver`) can be a way to know the
  driver without attempting a full open of the file, but, drivers not having a
  specialized implementation of the Identify() method will fall back to the Open()
  method.

- Drivers depending on third-party libraries whose code has been embedded in GDAL.
  Binary builds might rely on the internal version, or the external version.
  If using the internal version, they might use an obsolete version of the
  third-party library that might contain known vulnerabilities. Potentially
  concerned drivers are GTiff (libtiff, libgeotiff), PNG (libpng), GIF (giflib),
  JPEG (libjpeg), PCRaster (libcsf), GeoJSON (libjson-c), MRF (liblerc).
  An internal version of ZLib is also contained in GDAL sources.
  Packagers of GDAL are recommended to use the external version of the libraries
  when possible, so that security upgrades of those dependencies benefit to GDAL.

- Drivers using GDALOpen()/GDALOpenEx()/OGROpen() internally cause other drivers
  to be used (and their possible flaws exploited), without it being obvious at
  first sight. VRT, MBTiles, KMLSuperOverlay, RasterLite, PCIDSK, PDF, RPFTOC,
  RS2, WMS, WCS, WFS, OAPIF, OGCAPI and GTI are examples of drivers with this
  behavior.

- Drivers depending on downloaded data (HTTP, WMS, WCS, WFS, OAPIF, OGCAPI,
  STACIT, STACTA, etc.).

- XML based drivers: they might be subject to denial of service by
  `​billion laugh-like <https://en.wikipedia.org/wiki/Billion_laughs_attack>`__
  attacks. Existing GDAL XML based drivers are thought to take defensive measures
  against such patterns (starting with GDAL 3.9.3 for LVBAG and GMLAS drivers)

- SQL injections: services that would accept untrusted SQL requests could trigger
  SQL injection vulnerabilities in database-based drivers.


​GDAL MEM driver
++++++++++++++++

The opening syntax ``MEM:::DATAPOINTER=<memory_address>,PIXELS=<number>,LINES=<number>``
can access any memory of the process. Feeding it with a random access can cause
a crash, or a read of unwanted virtual memory. The MEM driver is used by various algorithms
and drivers in creation mode (which is not vulnerable to the DATAPOINTER issue),
so completely disabling the driver might be detrimental to other areas of GDAL.
It is possible to define the GDAL_NO_OPEN_FOR_MEM_DRIVER *compilation* flag to
disable the ``MEM:::DATAPOINTER``` syntax only.

​GDAL PDF driver
++++++++++++++++

The OGR_DATASOURCE creation option accepts a file name. So any OGR datasource,
and potentially any file (see OGR VRT) could be read through this option, and
its content embedded in the generated PDF.
Similarly for the COMPOSITION_FILE creation option.

​GDAL VRT driver
++++++++++++++++

It can be used to access any valid GDAL dataset. If a hostile party, with
knowledge of the location on the filesystem of a valid GDAL dataset, convinces
a user to run gdal_translate a VRT file and give it back the result,
it might be able to steal data. That could potentially be able for a web service
accepting data from the user, converting it, and sending back the result.

The VRTRawRasterBand mechanism can read any file (not necessarily a
valid GDAL dataset) accessible, which can extend the scope of the above
mentioned issue. Consult :ref:`vrtrawrasterband_restricted_access` for more details.

The VRTDerivedRasterBand mechanism can use Pixel functions written in Python,
directly embedded in a VRT file, or pointing to external Python code. By
default this mechanism is disabled, to avoid arbitrary code execution.
Consult :ref:`raster_vrt_security_implications` for more details.

/vsicurl/ (and associated network-capable virtual file systems) filenames can be
used, thus causing remote data to be downloaded.

​GDAL GTI driver
++++++++++++++++

Same issues as the GDAL VRT driver.

​OGR VRT driver
+++++++++++++++

Similar issues as the GDAL VRT driver. ``<SrcSQL>`` could be used to modify data.
