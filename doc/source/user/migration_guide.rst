.. _migration_guide:

================================================================================
Migration guide
================================================================================

From GDAL 3.12 to GDAL 3.13
---------------------------

- Changes impacting out-of-tree vector drivers:

  * :cpp:func:`GDALDataset::Close` takes now 2 input parameters ``(GDALProgressFunc pfnProgress, void *pProgressData)``,
    which may be nullptr.

- Changes impacting C++ users:

  * :cpp:func:`GDALMajorObject::SetMetadata` now takes a ``CSLConstList`` argument
    (this does not require code changes but is an opportunity for users to have better const safety)
  * :cpp:func:`GDALMajorObject::GetMetadata` and :cpp:func:`GDALGetMetadata`
    now return a ``CSLConstList`` argument.
    This will require users that stored the return value of those functions in
    ``char **`` to use ``CSLConstList`` instead. Such change is compatible with
    earlier GDAL versions.

From GDAL 3.11 to GDAL 3.12
---------------------------

- The following changes have been done to the "gdal" command line interface:

  * Sub-commands "buffer", "explode-collections", "make-valid", "segmentize",
    "simplify", "swap-xy" of "gdal vector geom" are now directly available
    under "gdal vector". Support for the old location is kept for 3.12, but
    will be definitely removed in 3.13.
  * Furthermore, sub-command "set-type" of "gdal vector geom" is renamed as
    "set-geom-type" and also placed under "gdal vector".
  * Progress bar is emitted to stdout by default unless --quiet/-q is specified.
  * For gdal raster info, gdal vector info and gdal vsi list, --format=text is
    now the default when those utilities are invoked from the command line.
    When they are invoked from the API, the default is still JSON.

- Changes impacting out-of-tree vector drivers:
  * :cpp:func:`GDALDataset::GetLayer` is now a const method that returns a ``const OGRLayer*``
  * :cpp:func:`GDALDataset::GetLayerCount` is now a const method
  * :cpp:func:`GDALDataset::TestCapability` is now a const method
  * :cpp:func:`OGRLayer::GetName` is now a const method
  * :cpp:func:`OGRLayer::GetGeomType` is now a const method
  * :cpp:func:`OGRLayer::GetLayerDefn` is now a const method that returns a ``const OGRFeatureDefn*``
  * :cpp:func:`OGRLayer::GetFIDColumn` is now a const method
  * :cpp:func:`OGRLayer::GetGeometryColumn` is now a const method
  * :cpp:func:`OGRLayer::GetSpatialRef` is now a const method that returns a ``const OGRSpatialReference*``
  * :cpp:func:`OGRLayer::TestCapability` is now a const method

- Changes impacting C++ users:

  * :cpp:func:`OGRLayer::GetSpatialRef()` now returns a ``const OGRSpatialReference*`` instead of a non-const one.
    In most situations, users can modify their code to store the returned value
    in a const pointer (which should be compatible with older GDAL versions).
    When they need to modify the reference count of the
    returned instance, const casting to a non-const pointer is the current
    recommended solution.

  * :cpp:func:`OGRFeature::GetDefnRef()` now returns a ``const OGRFeatureDefn*`` instead of a
    non-const one. Same recommendation as above.

- C API change: The :cpp:enum:`GDALRATFieldType` enumeration has been extended with 3 new
  values: :cpp:enumerator:`GDALRATFieldType::GFT_Boolean`,
  :cpp:enumerator:`GDALRATFieldType::GFT_DateTime`,
  :cpp:enumerator:`GFT_WKBGeometry`. Code that calls
  :cpp:func:`GDALRATGetTypeOfCol` may encounter those new values.

- The raw file capabilities (``VRTRawRasterBand``) of the VRT raster driver have
  been limited by default for security reasons. Consult
  :ref:`vrtrawrasterband_restricted_access` for more details.

- Methods :cpp:func:`GDALRasterAttributeTable::SetValue` now return a CPLErr instead of
  void. This will impact in particular out-of-tree drivers that implement those
  methods in a subclass of :cpp:class:`GDALRasterAttributeTable`.

- Virtual methods :cpp:func:`GDALDataset::GetGeoTransform` (resp. :cpp:func:`GDALDataset::SetGeoTransform` have
  been modified to accept ``GDALGeoTransform& gt`` (resp. ``const GDALGeoTransform& gt``)
  parameters instead of a pointer to 6 doubles. The new class :cpp:class:`GDALGeoTransform`
  is a thin wrapper around a ``std::array<double, 6>``. This change affects out-of-tree
  raster drivers.

From GDAL 3.10 to GDAL 3.11
---------------------------

- The introduction of the :source_file:`gcore/gdal_fwd.h` header that normalizes forward declarations
  of GDAL public opaque types may cause issues with downstream users of the
  GDAL API that would have redefined themselves those types, particularly when
  building against a GDAL built in DEBUG mode where the type aliases are stricter
  than in release mode.

- The ``OGRLayer::GetExtent(OGREnvelope*, int bForce)`` and
  ``OGRLayer::GetExtent(int iGeomField, OGREnvelope*, int bForce)`` methods are
  no longer virtual methods that are implemented by drivers, and the ``int bForce``
  parameter is now a ``bool bForce``.
  Drivers may implement the new ``OGRLayer::IGetExtent(int, OGREnvelope *, bool)`` protected
  virtual method. The public method checks that the
  iGeomField value is in range.
  Similarly for ``OGRLayer::GetExtent3D(int iGeomField, OGREnvelope3D*, int bForce)``
  which is now a user facing method (with the change that the ``int bForce`` is now a
  ``bool bForce``). Drivers may implement the new
  ``IGetExtent3D(int iGeomField, OGREnvelope3D*, bool bForce)`` protected virtual method.
  The public method checks that the iGeomField value is in range.

- The :cpp:func:`OGRLayer::SetSpatialFilter` and :cpp:func:`OGRLayer::SetSpatialFilterRect` methods are
  no longer virtual methods that are implemented by drivers. They now return
  OGRErr instead of void, and accept a ``const OGRGeometry*``). Drivers may implement
  the new ISetSpatialFilter(int iGeomField, const OGRGeometry*) protected virtual method.
  The public methods check that the iGeomField value is in range.

- GDAL drivers may now return raster bands with the new data types
  GDT_Float16 or GDT_CFloat16. Code that use the GDAL API must be
  ready to react to the new data type, possibly by doing RasterIO()
  requests with ``eBufType==GDT_Float32``, if they can't deal natively
  with Float16 values.

- If only a specific GDAL Minor version is to be supported, this must now be
  specified in the find_package call in CMake via a version range specification.

- The following methods
  ``OGRCoordinateTransformation::Transform(size_t nCount, double *x, double *y, double *z, double *t, int *pabSuccess)`` and
  ``OGRCoordinateTransformation::TransformWithErrorCodes(size_t nCount, double *x, double *y, double *z, double *t, int *panErrorCodes)``
  are modified to return
  FALSE as soon as at least one point fails to transform (to be consistent with
  the other form of ``Transform()`` that doesn't take a "t" argument), whereas
  previously they would return FALSE only if no transformation was found. When
  FALSE is returned the ``pabSuccess[]`` or ``panErrorCodes[]`` arrays indicate which
  point succeeded or failed to transform.

  The ``GDALTransformerFunc`` callback and its implementations (``GenImgProjTransformer``,
  ``RPCTransformer``, etc.) are also modified to return FALSE as soon as at least
  one point fails to transform.

From GDAL 3.9 to GDAL 3.10
--------------------------

- The OGR SQL parser has been modified to evaluate NULL values in boolean
  operations similarly to other SQL engines (SQLite, PostgreSQL, etc.). Previously,
  with a foo=NULL field, expressions ``foo NOT IN ('bar')`` and ``foo NOT LIKE ('bar')``
  would evaluate as true. Now the result is false (with the NULL state being
  propagated to it). Concretely, to get the same results as in previous versions,
  the above expressions must be rewritten as ``foo IS NULL OR foo NOT IN ('bar')``
  and ``foo IS NULL OR foo NOT LIKE ('bar')``.

- MEM driver: opening a dataset with the ``MEM:::`` syntax is now disabled by
  default because of security implications. This can be enabled by setting the
  GDAL_MEM_ENABLE_OPEN build or configuration option. Creation of a 0-band MEM
  dataset, and using the :cpp:func:`GDALDataset::AddBand` method with the DATAPOINTER,
  PIXELOFFSET and LINEOFFSET options is the recommended way. For example, like
  in https://github.com/OSGeo/gdal/blob/e32a2fde41a555b7948cece9ab9b4e979138e7dd/gcore/rasterio.cpp#L1534-L1576

- The Erdas Imagine (HFA) and Derived drivers are now optional drivers. Users
  building with GDAL_BUILD_OPTIONAL_DRIVERS=OFF may need to explicitly enable
  them with GDAL_ENABLE_DRIVER_HFA=ON and GDAL_ENABLE_DRIVER_DERIVED=ON.
  The MapInfo, OGR_VRT and KML drivers are now an optional driver. Users
  building with OGR_BUILD_OPTIONAL_DRIVERS=OFF may need to explicitly enable
  them with OGR_ENABLE_DRIVER_TAB=ON, OGR_ENABLE_DRIVER_VRT=ON and
  OGR_ENABLE_DRIVER_KML=ON.

- User code using :cpp:func:`VSIFEofL` to potentially to end read loops should also test
  the return code of the new :cpp:func:`VSIFErrorL` function. Some virtual file systems
  that used to report errors through ``VSIFEofL`` now do through ``VSIFErrorL``.

- Out-of-tree implementations of :cpp:class:`VSIVirtualHandle`:
  2 new required virtual methods must be implemented: ``int Error()``, and
  ``void ClearErr()`` following POSIX semantics of ``ferror()`` and ``clearerr()``.
  This is to distinguish ``Read()`` that returns less bytes than requested because
  of an error (``Error() != 0``) or because of end-of-file (``Eof() != 0``)

  The ``VSIFilesystemPluginCallbacksStruct`` structure is extended with 2
  corresponding optional (but recommended to be implemented to reliably detect
  reading errors) callbacks "error" and "clear_err".

- Python bindings: :py:meth:`osgeo.gdal.Band.GetStatistics` and
  :py:meth:`osgeo.gdal.Band.ComputeStatistics` now
  return a None value in case of error (when exceptions are not enabled)

- New color interpretation (``GCI_xxxx``) items have been added to the
  :cpp:enum:`GDALColorInterp` enumeration.
  Code testing color interpretation may need to be adapted.

From GDAL 3.8 to GDAL 3.9
-------------------------

- Out-of-tree vector drivers:

  * :cpp:func:`OGRLayer::CreateField` now takes a ``const OGRFieldDefn*`` instead of a
    ``OGRFieldDefn*``.
  * :cpp:func:`OGRLayer::CreateGeomField` now takes a ``const OGRGeomFieldDefn*`` instead of
    a ``OGRGeomFieldDefn*``.
  * :cpp:func:`GDALDataset::ICreateLayer` has a new prototype, due to :ref:`RFC 99 "Geometry
    coordinate precision <rfc-99>` changes.

    The fastest migration path is from:

    ::

        OGRLayer *
             MyDataset::ICreateLayer(const char* pszLayerName,
                                     const OGRSpatialReference *poSpatialRef,
                                     OGRwkbGeometryType eGType, char **papszOptions)
        {
            ...
        }

    to

    ::

        OGRLayer *
             MyDataset::ICreateLayer(const char *pszLayerName,
                                     const OGRGeomFieldDefn *poGeomFieldDefn,
                                     CSLConstList papszOptions)
        {
            const auto eGType = poGeomFieldDefn ? poGeomFieldDefn->GetType() : wkbNone;
            const auto poSpatialRef =
                poGeomFieldDefn ? poGeomFieldDefn->GetSpatialRef() : nullptr;
            ...
        }

- Sealed feature and field definition (RFC 97). A number of drivers now "seal"
  their layer definition, which might cause issue to user code currently
  mis-using setters of OGRFeatureDefn, OGRFieldDefn or OGRGeomFieldDefn on such
  instances.
  The drivers that have been updated to seal their layer definition are:
  GeoPackage, PostgreSQL, Shapefile, OpenFileGDB, MITAB, Memory, GeoJSON, JSONFG,
  TopoJSON, ESRIJSON, ODS, XLSX.

- OGRLayer::SetIgnoredFields() now accepts a ``CSLConstList papszIgnoredFields``
  instead of a ``const char** papszIgnoredFields``

From GDAL 3.7 to GDAL 3.8
-------------------------

- Out-of-tree vector drivers:

  * :cpp:func:`GDALDataset::ICreateLayer()` now takes a ``const OGRSpatialReference*`` instead
    of a ``OGRSpatialReference*``. Drivers should clone the passed SRS if they need
    to keep it.

- The /vsimem virtual file system is modified to automatically create parent
  directories when a file is created. (e.g., creating /vsimem/parent/child.txt
  would cause the directory /vsimem/parent to be created.) If the parent
  directory cannot be created because the file /vsimem/parent exists, file
  creation will now fail.

- In SWIG bindings, the function FileFromMemBuffer now returns an error code
  if the file could not be created.


From GDAL 3.6 to GDAL 3.7
-------------------------

- Following RFC 87, PIXELTYPE=SIGNEDBYTE in IMAGE_STRUCTURE metadata domain is
  no longer reported by drivers that used to do it. The new GDT_Int8 data type
  is now reported.
  On writing, the PIXELTYPE=SIGNEDBYTE creation option is preserved in drivers
  that used to support it, but is deprecated and external code should rather use
  the GDT_Int8 data type.

- The VSILFILE* type is no longer aliased to FILE* in builds without the DEBUG
  define (that is production builds). External code that used FILE* with
  GDAL VSI*L API has to be changed to use VSILFILE*.
  This aliasing dates back to old times where both types were indifferently
  used in the code base. In the mean time, this was cleaned up. But there was a
  drawback of having VSILFILE* being either a dedicated type or an alias of
  FILE* depending whether DEBUG is defined, especially with the C++ API, for
  people building their plugins with DEBUG and running them against a non-DEBUG
  GDAL build, or the reverse.

- GDALFlushCache() and GDALDataset::FlushCache() are modified to return a CPLErr
  error code instead of void. Affects out-of-tree drivers.

- A Close() virtual method is added to GDALDataset per RFC 91. Out-of-tree
  drivers with write support are encouraged to implement it for error
  propagation.

- Pansharpening now requires that panchromatic and multispectral bands have
  valid geotransform (in early versions, it was assumed in the case of missing
  geotransform that they covered the same geospatial extent).
  The undocumented VRT pansharpened MSShiftX and MSShiftY options (and the
  corresponding C++ ``GDALPansharpenOptions::dfMSShiftX`` and ``dfMSShiftY`` members)
  have been removed, due to using the inverted convention as one would expect,
  and being sub-par solution compared to using geotransform to correlate pixels
  of panchromatic and multispectral bands.

- OGRCoordinateTransformation::GetSourceCS() and GetTargetCS() now returns
  a const OGRSpatialReference*

- OGRGeometry::getSpatialReference() now returns a const OGRSpatialReference*

- OGRGeomFieldDefn::GetSpatialRef() now returns a const OGRSpatialReference*

From GDAL 3.5 to GDAL 3.6
-------------------------

- Out-of-tree vector drivers:

  * GDALDataset::IBuildOverviews(): parameters ``panOverviewList`` and ``panBandList``
    are now of type 'const int*' (previously 'int*')
    Added a CSLConstList papszOptions member.
  * GDALRasterBand::BuildOverviews(): parameter ``panOverviewList`` is now of
    type 'const int*' (previously 'int*')
    Added a CSLConstList papszOptions member.
  * Compatibility layers of GDAL 3.0 ``_GetProjectionRef()``, ``_GetGCPProjectionRef()``,
    ``_SetProjection()``, ``_SetGCPs()`` have been removed

From GDAL 3.4 to GDAL 3.5
-------------------------

- GDAL drivers may now return raster bands with the new data types GDT_Int64 or
  GDT_UInt64.

- Make GDALProxyRasterBand::RefUnderlyingRasterBand() / UnrefUnderlyingRasterBand() const. May affect out-of-tree drivers

From GDAL 3.3 to GDAL 3.4
-------------------------

- Out-of-tree vector drivers:

    * :cpp:class:`OGRFeatureDefn` protected member variables have been changed.

      - ``(nFieldCount, papoFieldDefn)``        ==> ``std::vector<std::unique_ptr<OGRFieldDefn>> apoFieldDefn{}``
      - ``(nGeomFieldCount, paoGeomFieldDefn)`` ==> ``std::vector<std::unique_ptr<OGRGeomFieldDefn>> apoGeomFieldDefn{}``

    * ``OGRFeatureDefn::AddGeomFieldDefn( OGRGeomFieldDefn *, bCopy = FALSE )`` is
      replaced by ``AddGeomFieldDefn( std::unique_ptr<OGRGeomFieldDefn>&& )``

    * :cpp:func:`GDALDataset::FlushCache` and :cpp:func:`GDALRasterBand::FlushCache` now takes a ``bool bAtClosing`` argument.
      That argument is set to true when FlushCache() is called from the dataset/band destructor.
      This can be used as a hint, for example to avoid doing extra work if the dataset is marked
      for deletion at closing. Driver implementing that method should propagate the argument to the
      base implementation when calling it.

From GDAL 3.2 to GDAL 3.3
-------------------------

- Python bindings:

  * Python 2 is no longer supported per RFC 77. Python 3.6 or later required

  * "osgeo.utils" was replaced by "osgeo_utils" (more details: see RFC78)

  * The following undocumented, untested utility scripts are no longer installed as system scripts and were moved
    from "gdal/swig/python/gdal-utils" to: "gdal/swig/python/gdal-utils/samples":

    - ``epsg_tr.py``
    - ``esri2wkt.py``
    - ``gcps2vec.py``
    - ``gcps2wld.py``
    - ``gdal_auth.py``
    - ``gdalchksum.py``
    - ``gdalident.py``
    - ``gdalimport.py``
    - ``mkgraticule.py``

    In order to import sample script X (i.e. `epsg_tr`) as a module, use: `from osgeo_utils.samples import X`.
    In order to run it as a script run: `python -m osgeo_utils.samples.X`.

  * packaging:

    - "gdal/swig/python/samples" moved to: "gdal/swig/python/gdal-utils/osgeo_utils/samples"
    - "gdal/swig/python/scripts" moved to: "gdal/swig/python/gdal-utils/scripts"

- gdaldem TRI: default algorithm has been changed to use Riley et al. 1999. Use -alg Wilson to select the algorithm used previously
- Disable by default raster drivers DODS, JPEG2000(Jasper), JPEGLS, MG4LIDAR, FUJIBAS, IDA, INGR, ZMAP and vector driver ARCGEN, ArcObjects, CLOUDANT, COUCHDB, DB2, DODS, FME, GEOMEDIA, GTM, INGRES, MONGODB, REC, WALK at runtime, unless the GDAL_ENABLE_DEPRECATED_DRIVER_{drivername} configuration option is set to YES. Those drivers are planned for complete removal in GDAL 3.5
- Perl bindings are deprecated. Removal planned for GDAL 3.5. Use ``Geo::GDAL::FFI`` instead
- Removal of BNA, AeronavFAA, HTF, OpenAir, SEGUKOOA, SEGY, SUA, XPlane, BPG, E00GRID, EPSILON, IGNFHeightASCIIGrid, NTV1 drivers. Moved to (unsupported) https://github.com/OSGeo/gdal-extra-drivers repository.

From GDAL 3.1 to GDAL 3.2
-------------------------

- Python bindings: old-style, deprecated for many years, import method of
  importing the gdal module through "import gdal" is no longer available.
  "from osgeo import gdal" must now be used. This holds true for the ``ogr``, ``osr``,
  ``gdalconst`` and ``gdalnumeric`` modules.


From GDAL 3.0 to GDAL 3.1
-------------------------

- OGR SQL: the 'LIKE' operator is now case sensitive by default. ILIKE (supported
  in previous versions) must be used for case insensitive comparison. The
  OGR_SQL_LIKE_AS_ILIKE configuration option can be set to YES to make LIKE behave
  in a case insensitive way as in previous versions.

From GDAL 2.x to GDAL 3.0
-------------------------

Consult `https://github.com/OSGeo/gdal/blob/v3.0.4/gdal/MIGRATION_GUIDE.TXT <https://github.com/OSGeo/gdal/blob/v3.0.4/gdal/MIGRATION_GUIDE.TXT>`__

