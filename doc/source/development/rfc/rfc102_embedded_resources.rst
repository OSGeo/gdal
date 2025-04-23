.. _rfc-102:

===================================================================
RFC 102: Embedding resource files into libgdal
===================================================================

============== =============================================
Author:        Even Rouault
Contact:       even.rouault @ spatialys.com
Started:       2024-Oct-01
Status:        Adopted, implemented
Target:        GDAL 3.11
============== =============================================

Summary
-------

This RFC uses C23 ``#embed`` pre-processor directive, when available,
to be able to optionally embed GDAL resource files directly into libgdal.

A similar `PROJ RFC-8 <https://github.com/OSGeo/PROJ/pull/4274>`__ has been
submitted for PROJ to embed its :file:`proj.db` and :file:`proj.ini` files.

Motivation
----------

Some parts of GDAL core, mostly drivers, require external resource files located
in the filesystem. Locating these resource files is difficult for use cases where
the GDAL binaries are relocated during installation time.
One such case could be the GDAL embedded in Rasterio or Fiona binary wheels where :config:`GDAL_DATA` must be set to the directory of the resource files.
Web-assembly (WASM) use cases come also to mind as users of GDAL builds where
resources are directly included in libgdal.

Technical solution
------------------

The C23 standard includes a `#embed "filename" <https://en.cppreference.com/w/c/preprocessor/embed>`__
pre-processor directive that ingests the specified filename and returns its
content as tokens that can be stored in a unsigned char or char array.

Getting the content of a file into a variable is as simple as the following
(which also demonstrates adding a nul-terminating character when this is needed):

.. code-block:: c

    static const char szPDS4Template[] = {
    #embed "data/pds4_template.xml"
        , '\0'};

Compiler support
----------------

Support for that directive is still very new. clang 19.1 is the
first compiler which has a release including it, and has an efficient
implementation of it, able to embed very large files with minimum RAM and CPU
usage.

The development version of GCC 15 also supports it, but in a non-optimized way
for now. i.e. trying to include large files, of several tens of megabytes could
cause significant compilation time, but without impact on runtime. This is not
an issue for GDAL use cases, and there is intent from GCC developers to improve
this in the future.

Embedding PROJ's :file:`proj.db` of size 9.1 MB with GCC 15dev at time of writing takes
18 seconds and 1.7 GB RAM, compared to 0.4 second and 400 MB RAM for clang 19,
which is still reasonable (Generating :file:`proj.db` itself from its source .sql files
takes one minute on the same system).

There is no timeline for Visual Studio C/C++ at time of writing (it has been
`requested by users <https://developercommunity.visualstudio.com/t/Add-support-for-embed-as-voted-into-the/10451640>`__)

To be noted that currently clang 19.1 only supports ``#embed`` in .c files, not
C++ ones (the C++ standard has not yet adopted this feature). So embedding
resources must be done in a .c file, which is obviously not a problem since
we can easily export symbols/functions from a .c file to be available by C++.

New CMake options
-----------------

Resources will only be embedded if the new ``EMBED_RESOURCE_FILES`` CMake option
is set to ``ON``. This option will default to ``ON`` for static library builds
and if `C23 ``#embed`` is detected to be available. Users might also turn it to ON for
shared library builds. A CMake error is emitted if the option is turned on but
the compiler lacks support for it.

A complementary CMake option ``USE_ONLY_EMBEDDED_RESOURCE_FILES`` will also
be added. It will default to ``OFF``. When set to ON, GDAL will not try to
locate resource files in the GDAL_DATA directory burnt at build time into libgdal
(``${install_prefix}/share/gdal``), or by the :config:`GDAL_DATA` configuration option.

Said otherwise, if ``EMBED_RESOURCE_FILES=ON`` but ``USE_ONLY_EMBEDDED_RESOURCE_FILES=OFF``,
GDAL will first try to locate resource files from the file system, and
fallback to the embedded version if not found.

The resource files will still be installed in ``${install_prefix}/share/gdal``,
unless ``USE_ONLY_EMBEDDED_RESOURCE_FILES`` is set to ON.

Impacted code
-------------

- gcore: embedding LICENSE.TXT, and tms_*.json files
- frmts/grib: embedding GRIB2 CSV files
- frmts/hdf5: embedding bag_template.xml
- frmts/nitf: embedding nitf_spec.xml
- frmts/pdf: embedding pdf_composition.xml
- frmts/pds: embedding pds4_template.xml and vicar.json
- ogr/ogrsf_frmts/dgn: embedding seed_2d.dgn and seed_3d.dgn
- ogr/ogrsf_frmts/dxf: embedding header.dxf and leader.dxf
- ogr/ogrsf_frmts/gml: embedding .gfs files and gml_registry.xml
- ogr/ogrsf_frmts/gmlas: embedding gmlasconf.xml
- ogr/ogrsf_frmts/miramon: embedding MM_m_idofic.csv
- ogr/ogrsf_frmts/osm: embedding osm_conf.ini
- ogr/ogrsf_frmts/plscenes: embedding plscenesconf.json
- ogr/ogrsf_frmts/s57: embedding s57*.csv files
- ogr/ogrsf_frmts/sxf: embedding default.rsc
- ogr/ogrsf_frmts/vdv: embedding vdv452.xml

Considered alternatives
-----------------------

Including resource files into libraries has been a long-wished feature of C/C++.
Different workarounds have emerged over the years, such as the use of the
``od -x`` utility, GNU ``ld`` linker ``-b`` mode, or CMake-based solutions such
as https://jonathanhamberg.com/post/cmake-file-embedding/

We could potentially use the later to address non-C23 capable compilers, but
we have chosen not to do that, for the sake of implementation simplicity. And,
if considering using the CMake trick as the only solution, we should note that
C23 #embed has the potential for better compile time, as demonstrated by clang
implementation.

Backward compatibility
----------------------

Fully backwards compatible.

C23 is not required, unless EMBED_RESOURCE_FILES is enabled in GDAL.

Documentation
-------------

The 2 new CMake variables will be documented.

Testing
-------

The existing fedora:rawhide continuous integration target, which has now clang
19.1 available, will be modified to test the effect of the new variables.

Local builds using GCC 15dev builds of https://jwakely.github.io/pkg-gcc-latest/
have also be successfully done during the development of the candidate implementation

Related issues and PRs
----------------------

- https://github.com/OSGeo/gdal/issues/10780

- `GDAL candidate implementation <https://github.com/OSGeo/gdal/pull/10972>`__

- `PROJ RFC-8 Embedding resource files into libproj <https://proj.org/en/latest/community/rfc/rfc-8.html>`__

Voting history
--------------

+1 from PSC members JukkaR, JavierJS, KurtS, HowardB and EvenR
