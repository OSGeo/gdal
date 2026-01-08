.. _rfc-109:

=====================================================================
RFC 109: Split of gdal_priv.h and addition of public C++ headers
=====================================================================

============== =============================================
Author:        Even Rouault
Contact:       even.rouault @ spatialys.com
Started:       2025-09-12
Status:        Adopted, implemented
Target:        GDAL 3.12
============== =============================================

Summary
-------

This RFC splits the content of :file:`gdal_priv.h`, the installed header
containing declaration for GDAL (raster) C++ classes, into finer grain exported headers.
It also adds 3 new exported headers, :file:`gdal_raster_cpp.h`, :file:`gdal_multidim_cpp.h`
and :file:`gdal_vector_cpp.h`, the first two ones being essentially the
content of current :file:`gdal_priv.h` header, but with a more engaging name.
Those changes are done in a fully backward compatible way: current external
users of :file:`gdal_priv.h` will not have any change to do in their source code.

Motivation
----------

As of today, :file:`gdal_priv.h` is a 5,600+ line long monolith including 35 classes
definitions. There are several disadvantages to that:

- it makes navigating the content of the file difficult for users.
- it may cause longer compilation times when only a subset of the functionality
  is needed.
- it causes a number of C++ standard or other GDAL header files to be included
  by default to fulfill the requirements of those classes, but that would not be
  necessary in third-party code wanting to use the GDAL C++ API.

Another issue is the name itself :file:`gdal_priv.h` which suggests that this is
a private API. While it is true that part of the content of that file is indeed
for GDAL private needs, the majority of it is of legitimate use by external
C++ users, hence it might be appropriate to offer a more engaging :file:`gdal_cpp.h`,
while still acknowledging that what it exposes might be subject to changes:
generally at least ABI changes at each feature release; sometimes API incompatible
changes occur.

Technical solution
------------------

The content of current :file:`gdal_priv.h` is spread over the following new
exported (installed) header files:

- :file:`gdal_multidomainmetadata.h`: ``GDALMultiDomainMetadata`` class definition
- :file:`gdal_majorobject.h`: ``GDALMajorObject`` class definition
- :file:`gdal_defaultoverviews.h`: ``GDALDefaultOverviews`` class definition
- :file:`gdal_openinfo.h`: ``GDALOpenInfo`` class definition
- :file:`gdal_gcp.h`: ``gdal::GCP class`` definition
- :file:`gdal_geotransform`: ``GDALGeoTransform`` class definition
- :file:`gdal_dataset.h`: ``GDALDataset`` class definition
- :file:`gdal_rasterblock.h`: ``GDALRasterBlock`` class definition
- :file:`gdal_colortable.h`: ``GDALColorTable`` class definition
- :file:`gdal_rasterband.h`: ``GDALRasterBand`` class definition
- :file:`gdal_computedrasterband.h`: ``GDALComputedRasterBand`` class definition
- :file:`gdal_maskbands.h`: ``GDALAllValidMaskBand``, ``GDALNoDataMaskBand``, ``GDALNoDataValuesMaskBand``, ``GDALRescaledAlphaBand`` class definitions (only a subset of out-of-tree drivers might need them)
- :file:`gdal_driver.h`: ``GDALDriver`` class definition
- :file:`gdal_drivermanager.h`: ``GDALDriverManager`` class definition
- :file:`gdal_asyncreader.h`: ``GDALAsyncRader`` class definition
- :file:`gdal_multidim.h`: definition all classes related to the multidimensional API: ``GDALGroup``, ``GDALAttribute``, ``GDALMDArray``, etc.
- :file:`gdal_pam_multidim.h`: ``GDALPamMultiDim`` and ``GDALPanMDArray`` class definitions
- :file:`gdal_relationship.h`: ``GDALRelationship`` class definition
- :file:`gdal_cpp_functions.h`: public (exported), driver-public (exported) and private (non-exported) C++ methods

Each of this file aims to include the minimum amount of headers (C++ standard
headers and GDAL specific headers) required to make it compile in a standalone
mode (and this is enforced by a CI check) and use forward class definitions as
much as possible.

Three new public entry points header files are added:

- :file:`gdal_raster_cpp.h`: includes all above files but :file:`gdal_multidim.h`
  and :file:`gdal_pam_multidim.h`
- :file:`gdal_multidim_cpp.h`: includes :file:`gdal_dataset.h`, :file:`gdal_drivermanager.h`,
  :file:`gdal_multidim.h` and :file:`gdal_pam_multidim.h`
- :file:`gdal_vector_cpp.h`: includes :file:`gdal_dataset.h`, :file:`gdal_drivermanager.h`,
  :file:`ogrsf_frmts.h`, :file:`ogr_feature.h` and :file:`ogr_geometry.h`

The existing :file:`gdal_priv.h` is modified as following:

- its current inclusion of non-strictly needed GDAL headers, such as CPL ones
  (:file:`cpl_vsi.h`, :file:`cpl_minixm.h`, etc.), GDAL ones (:file:`gdal_frmts.h`,
  :file:`gdalsubdatasetinfo.h`) and OGR ones (:file:`ogr_feature.h`) are kept
  by default. Users may define ``GDAL_PRIV_SKIP_OTHER_GDAL_HEADERS`` or ``GDAL_4_0_COMPAT``
  before including :file:`gdal_priv.h` to avoid including those files.

- its current inclusion of a number of C++ standard headers that might not all be
  needed is kept by default.  Users may define ``GDAL_PRIV_SKIP_STANDARD_HEADERS`` or ``GDAL_4_0_COMPAT``
  before including :file:`gdal_priv.h` to avoid including those files.

- and finally it includes the new :file:`gdal_raster_cpp.h` and :file:`gdal_multidim_cpp.h` files.

The end result is that this whole restructuring should not have any visible
effect on current users of :file:`gdal_priv.h`.

New users targeting only GDAL 3.12+ can now include at their convenience either
:file:`gdal_raster_cpp.h`, :file:`gdal_multidim_cpp.h`, :file:`gdal_vector_cpp.h`
or any of the new finer grain include files.

.. note::

    The ``GDALPluginDriverProxy`` class definition is moved to a GDAL private
    non-installed :file:`gdalplugindriverproxy.h` header, since it can only be used by deferred
    loading plugin drivers, which must thus be in-tree. This class was not
    CPL_DLL exported.

    The ``GDALAbstractBandBlockCache`` class definition is moved to a GDAL private
    non-installed :file:`gdal_abstractbandblockcache.h` header, since this is
    an implementation detail, that does not be accessed by users. This class was
    not CPL_DLL exported.


Backwards compatibility
-----------------------

Changes in this RFC aim at being backward compatible by default.

Documentation
-------------

Documentation under https://gdal.org/en/latest/api will be modified to mention
the new finer grain headers and entry points.

https://gdal.org/en/latest/tutorials/raster_api_tut.html will be modified to
mention the possibility of using the new headers.

Testing
-------

Existing continuous integration should be sufficient to test the non regression;

Related issues and PRs
----------------------

Candidate implementation: https://github.com/OSGeo/gdal/compare/master...rouault:gdal:gdal_priv_split?expand=1

Funding
-------

Funded by GDAL Sponsorship Program (GSP)

Voting history
--------------

+1 from PSC members MikeS, HowardB, DanielM, DanB, JavierJS, KurtS and me, and +0 from JukkaR
