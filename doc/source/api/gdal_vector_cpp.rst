.. _gdal_vector_cpp:

================================================================================
Entry point for C++ Vector API
================================================================================

Since GDAL 3.12, :source_file:`gcore/gdal_vector_cpp.h` is the include file
for all headers related to the C++ Vector API.

In earlier versions, the equivalent is to include :file:`gdal_priv.h`,
:file:`ogrsf_frmts.h`, :file:`ogr_feature.h` and :file:`ogr_geometry.h`

Note that the C++ API is also used by GDAL internals and may be subject from
time to time to backwards incompatible changes.
