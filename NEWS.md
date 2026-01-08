# GDAL/OGR 3.12.1 Release Notes

GDAL 3.12.1 is a bugfix release.

## Build

* mkgdaldist.sh: add pytest.ini to gdalautotest distribution

## GDAL 3.12.1

### Port

* VSIZipFilesystemHandler::GetFileInfo(): extra sanity check to avoid later
  huge memory allocations (ossfuzz#457877771)
* /vsicurl/: fix a redirect to a URL ending with a slash followed by a 403

### Algorithms

* Rasterize: avoid integer overflows on huge geometry coordinates
* GDALFPolygonize(): make it handle 64-bit float rasters on their native
  precision, and not Float32 (#13526)

### Core

* GDALGeoTransform::Apply(const OGREnvelope &, GDALRasterWindow&): avoid
  integer overflows
* ComputeRasterMinMaxLocation(): fix on all inf/-inf rasters
* GDALProxyPoolDataset::GetGCPSpatialRef(): fix nullptr dereference
* Deferred plugin loading: add GDAL_DCAP_UPDATE to cached driver metadata items
  in proxy driver, to avoid plugins to be unnecessarily loaded
* GDALJP2Metadata::CollectGMLData(): avoid memory leak on malformed documents
* JPEG2000 writer: remove NUL terminated byte at end of payload of 'lbl ' and
  'xml ' boxes
* ComputeStatistics(): fix imprecise result on stddev on Float64 with SSE2/AVX2
  optimization (3.12.0 regression) (#13543)
* ComputeStatistics(): increase precision for mean and stddev computation on
  Float32 to Float64 (3.12.0 regression)

### Raster utilities

* gdalinfo -json output: fix stac:transform coefficient order (#13358)
* gdalinfo -json: fix setting [stac][raster:bands][0][nodata] for
  floating-point datasets
* GDALZonalStats: Fix error for certain polygons outside raster extent (#13376)
* gdal raster zonal-stats: avoid integer overflows on geometries with huge
  coordinate values
* gdal raster tile: fix stalling in --parallel-mode=spawn with CPL_DEBUG=ON on
  Windows (#13390)
* gdal mdim convert: fix specifying multiple values for --group, --subset,
  --scale-axes
* gdal raster convert: avoid error message when outputting to /vsistdout/ (#13400)
* gdal raster calc / VRTDerivedRasterBand: fix computation/transfer data type
  with ComplexSource (#13409)
* gdal raster compare/info/tile: make them work properly in a pipeline where the
  input dataset is provided not in the pipeline string
* gdal vector make-valid: avoid skipping 3d geometries (#13425)
* ogr2ogr/VectorTranslate: fix (and improve) selectFields support in Arrow code
  path (#13401)
* gdal vector check-geometry: add include-field option
* gdal raster color-map/gdaldem color-relief: fix crash when color map is invalid
  and outputting to a format without CreateCopy capability
* GDALGeosNonStreamingAlgorithmDataset: Avoid crash with multiple input layers
* gdal vector sql: fix --overwrite-layer (#13516)
* gdal raster calc: allow to specify input files as nested pipelines (#13493)
* gdalwarp: fix artifacts (related to chunked processing) when using -r sum
  resampling (#13539)

### Raster drivers

GTI driver:
 * accept tiles with south-up orientation (auto-warp them to north-up) (#13416)
 * allow 'stac_extensions' field (in addition to 'stac_version') to be a marker
   for STAC GeoParquet
 * STAC GeoParquet: make it ready for top-level 'bands' object and eo v2.0 stac
   extension
 * only rewrite URLs (like gs:// --> /vsigs/) for a STAC collection catalog
   (#12900)

GTiff driver:
 * avoid a warning to be emitted when created RRD overviews

HDF5 driver:
 * do not report GCPs on swath geolocation fields, but report GEOLOCATION

netCDF driver:
 * fix potential linking issue in GDAL_REGISTER_DRIVER_NETCDF_FOR_LATER_PLUGIN=ON
   mode (#13410)

NITF driver:
 * fix reading extended header TREs (#13510)

STACIT driver:
 * do not emit initial pagination request with a '{}' body

VRT driver:
 * Add "name" attribute to source types in xml schema
 * with nearest neighbor, round source coordinates as we do in generic
   GDALRasterBand::IRasterIO() (#13464)
 * disable multithreading on neighbouring sources not perfectly aligned on an
   integer output coordinate, to avoid non deterministic pixel output

Zarr driver:
 * fix one way of opening Kerchunk Parquet reference stores
 * avoid excessive processing time on corrupted NCZarr datasets
   (ossfuzz#459241526)

## OGR 3.12.1

### Core

* OGR_G_ExportToJson(): add ALLOW_MEASURE, ALLOW_CURVE, COORDINATE_ORDER
  options (#13366)

### OGRSpatialReference

* OGRSpatialReference::FindBestMatch(): fix potential nullptr deref

### Vector drivers

GML driver:
 * GML2OGRGeometry_XMLNode(): fix perf issue on srsName repeated many times
   (ossfuzz#453226763)
 * reader: fix reading 3D geometries with a 3D srsName but without
   srsDimension='3'
 * reader: set geometry column name when there are several geometry elements,
   but the last one (whih is the one we read) is always the same

GPKG driver:
 * writer: avoid (non fatal) error message when creating a layer with a derived
   geographic CRS

Parquet driver:
 * Arrow/Parquet: add a LISTS_AS_STRING_JSON=YES/NO open option (#13448)
 * fix SetIgnoredFields() on files with fields of type list of structure
   (#13338)

Shapefile driver:
 * reader/organizePolygons(): dramatically improve performance when input has
   several 100,000 of rings (qgis/QGIS#63826)
 * fix potential nullptr deref on corrupted CRS in GetSpatialRef()
   (ossfuzz#458229990)

## SWIG bindings

* Increment FeatureDefn refcount on Feature.GetDefnRef
* Python bindings: avoid warning with Python 3.14 when VSIFile constructor
  throws an exception
* Python bindings: add compatilibity with Python 3.13+ free-standing/no-gil
  builds
* Python bindings: add 'progress' keyword argument to gdal.alg.xxxx() methods
* Python bindings: avoid gdal.alg.X public symbols to have non relevant
  suggestions

# GDAL/OGR 3.12.0 "Chicoutimi" Release Notes

GDAL/OGR 3.12.0 is a feature release.
Those notes include changes since GDAL 3.11.0, but not already included in a
GDAL 3.11.x bugfix release.

## In a nutshell...

* New 'gdal' command line interface capabilities:
  - Add 'gdal raster as-features' (#12970)
  - Add 'gdal raster blend' (port of hsv_merge.py + regular alpha blending)
  - Add 'gdal raster compare' (port of gdalcompare.py) (#12757)
  - Add 'gdal raster neighbors' (#12768)
  - Add 'gdal raster nodata-to-alpha' (#12524)
  - Add 'gdal raster pansharpen' (port of gdal_pansharpen.py)
  - Add 'gdal raster proximity' (#12350)
  - Add 'gdal raster rgb-to-palette' (port of rgb2pct.py)
  - Add 'gdal raster update'
  - Add 'gdal raster zonal-stats'
  - Add 'gdal vector check-coverage'
  - Add 'gdal vector check-geometry'
  - Add 'gdal vector clean-coverage'
  - Add 'gdal vector index' (port of ogrtindex)
  - Add 'gdal vector layer-algebra' (port of ogr_layer_algebra.py)
  - Add 'gdal vector make-point'
  - Add 'gdal vector partition'
  - gdal vector pipeline: add limit step
  - Add 'gdal vector set-field-type'
  - Add 'gdal vector simplify-coverage'
  - Add 'gdal mdim mosaic' (#13208)
  - Add 'gdal dataset' port of 'gdal manage'
  - Make 'gdal pipeline' support mixed raster/vector pipelines
  - Pipeline: add support for nested pipeline (#12874)
  - Pipeline: add support for a 'tee' step (#12874)
  - Move 'gdal vector geom XXXX' utilities directly under 'gdal vector'
  - Rename 'gdal vector geom set-type' as 'gdal vector set-geom-type'
  - gdal raster reproject: add a -j/--num-threads option and default to ALL_CPUS
  - Make 'gdal raster fill-nodata/proximity/sieve/viewshed' pipeline-able
  - gdal raster mosaic/stack: allow it to be the first step of a raster pipeline
  - gdal pipeline: allow to run an existing pipeline and override/add parameters
  - Improved Bash completion
  - Python bindings: add a dynamically generated 'gdal.alg' module
    (e.g. ``gdal.alg.raster.convert(input="in.tif", output="out.tif")``)
  - Many other improvements to existing utilities (see below)

* VRT pixel functions: Add mean, median, geometric_mean, harmonic_mean, mode
   (#12418), and handle NoData values
* Add C/C++/Python API for [raster band algebra](https://gdal.org/en/latest/user/band_algebra.html):
  arithmetic operators, comparison operators, AsType(),
  gdal::abs()/sqrt()/log10()/log()/min()/max()/mean()/IfThenElse() functions
* Add MiraMon raster driver: read-only support (#12293)
* ADBC driver: support for ADBC BigQuery driver (requires BigQuery ADBC driver)
* JSONFG driver: add read/write support for curve and measured geometries;
  update to 0.3.0 spec
* Parquet: add update support using OGREditableLayer mechanism
* Add C++ public header files for raster functionality
* Security: avoid potential path traversal in several drivers
* Various code linting, static code analyzer fixes, etc.
* Significant automation of the release process
* Add Docker attestation (#13066)
* Bump of shared lib major version

## New installed files

* Include files:
gdalalgorithm_c.h
gdalalgorithm_cpp.h
gdal_asyncreader.h
gdal_colortable.h
gdal_computedrasterband.h
gdal_cpp_functions.h
gdal_dataset.h
gdal_defaultoverviews.h
gdal_driver.h
gdal_drivermanager.h
gdal_gcp.h
gdal_geotransform.h
gdal_majorobject.h
gdal_maskbands.h
gdal_multidim_cpp.h
gdal_multidim.h
gdal_multidomainmetadata.h
gdal_openinfo.h
gdal_pam_multidim.h
gdal_rasterband.h
gdal_rasterblock.h
gdal_raster_cpp.h
gdal_relationship.h
gdal_vector_cpp.h

* Resource files:
grib2_table_4_2_0_22.csv
grib2_table_4_2_20_3.csv
grib2_table_4_2_2_7.csv

## Backward compatibility issues

See [MIGRATION_GUIDE.TXT](https://github.com/OSGeo/gdal/blob/release/3.12/MIGRATION_GUIDE.TXT)

## Build changes

* CMake: unset GDAL/OGR_ENABLE_DRIVER_xxxx when setting
  GDAL_BUILD_OPTIONAL_DRIVERS=ON/OGR_BUILD_OPTIONAL_DRIVERS=ON the first time
* Add /MP to MSVC flags (parallel compilation)
* Add a CMake GDAL_VRT_ENABLE_RAWRASTERBAND=ON/OFF option, default ON, that
  can be set to OFF to disable VRTRawRasterBand support
* CMake: install missing completions
* Add v18 of Microsoft ODBC Driver for SQL Server to CMake module
* Remove unused macros from generated cpl_config.h
* add a GDAL_ENABLE_ALGORITHMS boolean variable to disable algorithms beneath
 'gdal'
* PDF driver: add compatibility with Poppler 25.10 and PoDoFo 1.0
* GdalGenerateConfig.cmake: improve generator expression handling
* GdalGenerateConfig.cmake: revise link lib flattening

## Internal libraries

* Add third_party/libdivide

## GDAL 3.12.0 - Overview of Changes

### Port

* cpl_port.h: remove obsolete and unused CPL_CVSID() macro (#11433)
* VSICurlHandle::AdviseRead() (GTIFF multithreading /vsicurl/ reading):
  implement retry strategy (#12426)
* Rename VSIE_AWSxxx constants to VSIE_xxx, and CPLE_AWSxxx to CPLE_xxx; rename
  AWSError to ObjectStorageGenericError
* Add VSIErrorNumToString()
* VSIToCPLError(): include error number string in error message
* /vsis3/: add support for directory buckets
* /vsis3/: add credential_process support for AWS authentication (#13239)
* /vsis3/: fix issue when doing a new connection using EC2 credentials would
  go through WebIdentity (#13272)
* /vsis3/: set VSIError on Stat() operations (and others too)
* /vsis3/: retrieve path specific options in ReadDir()
* /vsiaz/ and /vsiadls/: set VSI error codes
* Add CPLFormatReadableFileSize()
* (some) implementation of GetDiskFreeSpace() for /vsis3/, /vsigs/ and /vsiaz/
* VSI Win32 GetCanonicalFilename(): make it expend output buffer if needed
* VSI Win32: implement OpenDir()
* /vsigs/: make UnlinkBatch() and GetFileMetadata() work when OAuth2 bearer is
  passed through GDAL_HTTP_HEADERS
* /vsiswift/: pass HTTP options for GetFileList() operation
* /vsiwebhdfs/: pass HTTP options for GetFileList(), Unlink() and Mkdir()
  operations
* Unix/Win32/Sparse/Archive VSI: make sure that Close() can be called multiple
  times, and is called by destructor
* add a VSIVirtualFileSystem::CreateOnlyVisibleAtCloseTime() method, and add a
  Linux and Windows implementation of it
* CPLJSONDocument::Save(): propagate failure to write file
* Add CPLGetCurrentThreadCount()
* CPLGetRemainingFileDescriptorCount(): add a FreeBSD and netBSD implementation
* CPLGetExecPath(): implement it on NetBSD
* Add CPLHasPathTraversal() and CPLHasUnbalancedPathTraversal()
* CPLSpawnAsync() Win32: properly quote arguments that need it
* VSI archive (/vsizip/, /vsiar/) performance improvements with large number of
  files (#12939)
* vsis3/vsigs/vziaz/vsiwebhdfs/vsiswift/vsicurl: add path traversal protection
  when parsing result of ListDir
* /vsis3/ (and /vsigs/, /vsiaz/, etc.): by default squash '/./' and '/../'
  sequences in path, unless the GDAL_HTTP_PATH_VERBATIM config option is set
  (#13040)
* Unix and Win32 file I/O: honour CancelCreation() even for files created by
  regular Open()
* Win32: make Rename() able to rename on top of existing dest file
* VSIFOpenEx2L(): add a CACHE=ON/OFF option to disable caching of file after
  file closing (for /vsicurl and similar)
* /vsicurl/: do not update cached file properties if cURL returned a non-HTTP
  error
* /vsicurl/: make HTTP directory listing more robust (#13293)
* VSIGlob(): fix when argument contains no directory, like 'byte*.tif'
* /vsizip/: add file size related sanity checks to avoid huge mem allocs on
  corrupted files (ossfuzz#452384655)

### Core

Improvements:
* Raster Attribute Table: add GFT_Boolean, GFT_DateTime and GFT_WKBGeometry
* C++ API: add GDALGeoTransform class, and modify GDALDataset::GetGeoTransform()
  /SetGeoTransform() to use it
* C and SWIG API: add GDALAlgorithmArgGetDefaultAs[Boolean|Integer|Double|String
  |IntegerList|DoubleList|StringList]
* Add GDAL_DMD_MAX_STRING_LENGTH driver metadata item
* Add GDAL_DCAP_APPEND driver capability (#12899)
* Add GDAL_DCAP_CREATE_ONLY_VISIBLE_AT_CLOSE_TIME driver capability
* Add GDAL_DCAP_REOPEN_AFTER_WRITE_REQUIRED and GDAL_DCAP_CAN_READ_AFTER_DELETE
* Add GDAL_DCAP_UPSERT and use it where relevant
* Add GDALDataset::GetLayerIndex()
* Add GDALAlgorithm::AddGeometryTypeArg(), AddAppendLayerArg(),
  AddOverwriteLayerArg(), AddAbsolutePathArg(), IsHidden(), WarnIfDeprecated(),
  AddStdoutArg()
* GDALAlgorithm::ParseCommandLineArguments(): make it accept <INPUT>
  <ONE-OR-MORE>... <OUTPUT> positional arguments (useful for 'gdal raster
  pansharpen')
* Add GDALAlgorithmArgIsHidden(), and rename IsOnlyForCLI() to IsHiddenForAPI()
* GDALVectorNonStreamingAlgorithmDataset: Set ADVERTIZE_UTF8 if applicable
* Add GDALRasterBand::SplitRasterIO()
* GDALRegenerateOverviewsMultiBand(): avoid progress percentage to go beyond 1.0
  in some cases
* Add GDALRescaleGeoTransform()
* Add GDALDataset::AddOverviews() to add existing overview datasets (#12614)
* gdal raster hillshade/slope/aspect/tpi/tri: make sure it keeps a reference on
  source dataset, for more robust working in streaming mode
* Make GDALRasterAttributeTable::SetValue() methods return a CPLErr
* GDALRasterBand::ReadRaster(): instantiate template for GFloat16
* GDALRasterBand: Add IterateWindows() (#12674)
* Add GDALDataset::GetExtent() and GetExtentWGS84LongLat(), corresponding C API
  and bind to SWIG
* GDALCopyWords(): perf improvements for some packed conversions
* GDALCopyWords(): implement efficient conversion of 8 Float16->Float32/Float64
  conversions using SSE2
* GDALCopyWords(): speed-up Double->Byte conversion
* gdal::TileMatrixSet: add a exportToTMSJsonV1() method
* Add GDALDatasetMarkSuppressOnClose() and expose it to SWIG
* gdal_minmax_element.hpp: add support for GDT_Float16
* gdal_minmax_element.hpp: SSE2 optimization for int64_t/uint64_t
* Add GDALGetGDALPath()
* ComputeStatistics(): speed-up on Intel SSE2 for number of data types

Fixes:
* GDALOpenEx(): changes so that opening PG:xxx triggers a message about missing
  plugin when it is not installed
* Enhance behavior/error messages regarding deferred plugins (in particular
  PostgreSQL)
* GDALGetOutputDriversForDatasetName(): improve error message when matching on
  prefix and not extension
* GDALGetOutputDriversForDatasetName(): turn warning into error if plugin
  driver detected but not available, and take it into account in
  GDALVectorTranslate()
* GDALAlgorithm: rename AddNodataDataTypeArg() method to AddNodataArg()
* GDALAlgorithm: Raise error on malformed list arguments
* GDALAlgorithm::GetArg(): change suggestionAllowed default to false, e.g. to
  avoid weird error messages in auto-completion when a filename is close to the
  'like' argument name
* GDALDataTypeUnion(): adjust to avoid Byte + CInt32 to go to CFloat64
* Overview generation: limit external file size in
  GDALRegenerateOverviewsMultiBand (#12392)
* Overview generation: avoid potential integer overflows with huge reduction
  factors

### Multidimensional API

* Add GDALExtendedDataType::GetRAT() and GDALGroup::GetDataTypes()
* Add GDALDataset::AsMDArray()
* GDALMDArray::AsClassicDataset(): accept a "attribute" key in BAND_METADATA and
  BAND_IMAGERY_METADATA options. Also accept fully qualified path for attributes
  in "attribute" or in "${/path/to/attribute}"
* Performance improvements in Float16 <--> Float32/64 conversions when F16C
  instruction set is enabled
* GDALMDArrayMask::IRead(): avoid potential issue when bufferDataType is not a
  numeric data type
* add GDALMDArray::GetRawBlockInfo() / GDALMDArrayGetRawBlockInfo() and implement
  it in HDF5, netCDF, ZARR and VRT
* Fix GetView(["::-1"]) on a dimension of size 1

### GNM

* GNMDBDriver: tighten Identify method

### Algorithms

* alg/: replace some uses of CPLCalloc() by VSI_MALLOC_VERBOSE()
* GDALChecksumImage(): avoid potential int overflow
* Geoloc transformer: add a GEOLOC_NORMALIZE_LONGITUDE_MINUS_180_PLUS_180
  transformer option to force  apply longitude 180 normalization (#6582)
* Warper: make sure mode resampling doesn't alter values for [U]Int[32|64]
* Warper: make mode resampling NaN aware
* Warper: when source nodata is set and per-dataset mask band exists, ignore the
  later, and raise a warning message (#13074)
* RasterIO/overview mode resampling: properly takes into account NaN for
  Float16/CFloat16
* rasterio/overview: fix handling of large float values (FLT_MAX/DBL_MAX) for
  average, RMS, cubic, cubicspline, lanczos

### Raster Utilities

* gdal: enable --progress by default, and add --quiet to shut it off (#12712)
* gdal: when outputting to /vsistdout/, automatically turn on --quiet
* gdal: Homogenize input/output layer name arg (#12984)
* gdal info: make it output text by default
* gdal raster/vector info and gdal vsi list: change default output format to text
  when invoked from command line (#12712)
* gdal raster calc: handle NoData (#12610)
* gdal raster calc: add --flatten option
* gdal raster calc: add a --dialect=muparser|builtin, where builtin can be used
  to compute a single band from all bands of a single input dataset
* gdal raster calc: check that formula is correct in RunStep() even when writing
  to VRT, GDALG or stream
* gdal raster calc: detect providing twice the same input name
* gdal raster calc: check input names are valid identifiers
* gdal raster calc: optimize sum of all sources as using builtin sum()
* gdal raster clip: add a --window <column>,<line>,<width>,<height> argument
* gdal raster/vector clip: make them work with --like dataset being a PostgreSQL
  (vector) dataset (#13091)
* gdal raster clip: do not emit warning about being outside of window when
  specifying --allow-bbox-outside-source
* gdal raster color-map: write directly to output file if possible
* gdal raster convert: fix issue with comma in input dataset name (#13255)
* gdal raster edit: add a --gcp option
* gdal raster edit: add a --unset-metadata-domain option
* gdal raster footprint: accept output dataset passed as object
* gdal raster hillshade: write directly to output file if possible
* gdal raster hillshade: implement SSE2 optimization on Float32 input data type
* gdal raster index: emit error if using --dst-crs and non existing input dataset
* gdal raster index: add --skip-errors (#12806)
* gdal raster mosaic: support VRT pixel functions (--pixel-function and
  --pixel-function-arg arguments)
* gdal raster mosaic/stack: add a --absolute-path option
* gdal raster overview add: add an --overview-src option
* gdal raster overview add: add a --creation-option/--co argument to properly
  pass overview creation options (#12446)
* gdal raster overview add: make it available in a pipeline
* gdal raster pipeline: make --band validation work for steps
* gdal raster polygonize: improve performance when using GPKG output (#13169)
* gdal raster reclassify: validate mappings in case VRT or GDALG output is used
* gdal raster reproject: avoid going through VRT when writing to final file is
  possible
* gdal raster resize: add a --resolution argument (#13259)
* gdal raster tile: speed-up generation of (max zoom) tiles in PNG format
* gdal raster tile: reduce consumption of cached source tiles when computing
  overview tiles
* gdal raster tile: improve a bit the efficiency of multithreading by reducing
  the number of jobs to approximately the number of threads (#12661)
* gdal raster tile: add a --parallel-method=fork on non-Windows OS
* gdal raster tile: add a --parallel-method=spawn switch
* gdal raster tile: do not pass potential sensitive information on command line
  of child processes
* gdal raster tile: only use a new thread for a minimum of 100 tiles/thread
* gdal raster tile: generate a stacta.json file (Spatio-Temporal Asset Catalog
  Tiled Assets)
* gdal raster tile: honour progress callback asking for process interruption
* gdal raster tile: allow it to be the last part of a pipeline
* gdal raster viewshed: add angular, pitch and minimum distance masking (#12457)
* gdal raster/vector pipeline: do not show hidden algorithms in usage
* gdal vsi list/copy/move/sync/delete: display object storage errors (#12467)
* Allow raster/vector info to be used as last step of a pipeline (#12767)
* gdal completion: avoid multiple completion of dataset/filenames
* gdal completion: add completion for --layer argument of 'gdal vector info' and
  'gdal vector pipeline read'
* gdal completion: only propose long name when completing '-' or '--' (#12465)
* take into account config options for Bash completion
* Make 'gdal raster reproject --resampling=?' or
  'gdal vector info my.gpkg --layer=?' output possible values
* gdalbuildvrt: add -write_absolute_path (#12401)
* gdalbuildvrt: when using -addalpha and and a source that has a per-dataset
  mask band, use that one for the alpha content (instead of using the source
  raster footprint as opaque)
* buildvrt / raster mosaic with pixel function: use smaller transfer type when
  possible
* gdalinfo: make it display content of archive when opening a /vsirar/
  (or /vsi7z/) without inner path specified
* gdal_translate: avoid int overflow on dataset whose at least one dimension is
  INT_MAX
* gdal_translate: do not preserve NODATA_VALUES metadata item when subsetting/
  reordering bands
* gdalwarp: avoid double->int overflows when computing target dataset size
* gdalwarp: avoid warning when using -novshift (#13313)
* gdaldem: INTERPOL(): avoid issues with large values
* gdaldem/gdal raster hillshade/slope/aspect/roughness/tpi/tri streaming:
  expose overviews if source has overviews
* gdalmdimtranslate: make 'gdalmdimtranslate multiband.tif out.nc/.vrt/.zarr
  -array <array_name>' generate a 3D array
* gdal2tiles: fix 'memory leak' (actually static memory not deallocated)
* Add hidden alias 'gdal [raster|vector] translate' for 'gdal [raster|vector]
  convert', and 'gdal raster warp' for 'gdal raster reproject
* gdal_viewshed: set lower bound of DEM to input raster (#12758)
* gdal_viewshed: pooled lines (code refactoring) (#13099)
* gdalmdimtranslate: propagate block size when possible
* gdalenhance: error out if attempting VRT output
* gdalenhance: do not transfer statistics from input to output (#13298)

### Raster drivers

Multi drivers: implement Close()

BAG driver:
 * creation: avoid integer overflow in progress computation if there are more
   than 2 billion blocks...

COG driver:
 * in STATISTICS=YES mode, always recompute statistics and do not use the source
   ones
 * write OVERVIEW_RESAMPLING in IMAGE_STRUCTURE metadata domain, and expose it
   when present (#13014)

DTED:
 * add configuration option DTED_ASSUME_COMPLIANT for the user to opt out of
   DTED 2's compliment conversion if the values are less than -16000 (#12299)

EHdr/ENVI:
 * clamp color table components to [0,255] to avoid issues down the road

GeoRaster driver:
 * Support long schema/table/column name (#12290)
 * Add OCI connection pool (#12980)

GRIB2 driver:
 * add four GRIB1 ECMWF tables (210, 215, 217, 218) to read a CAMS global
   atmospheric composition forecasts product
 * GRIB2: update tables to https://github.com/wmo-im/GRIB2 v34

GTI driver:
 * STAC GeoParquet: do s3:// -> /vsis3/ substitution (#12900)
 * allow to specify a SQL request instead of just a layer/table name to
   return tile features

GTiff driver:
 * add support for reading and writing in the GDAL_METADATA TIFF tag
 * GetFileList(): list .vat.dbf file
 * make GetInternalHandle() only return the TIFF handle if the parameter of the
   call is TIFF_HANDLE
 * fix serialization/deserialization of metadata in a json:XXXX metadata domain
   in the GDAL_METADATA TIFF tag
 * in CreateCopy(), serialize json:ISIS3 and json:VICAR metadata domains inside
   the GDAL_METADATA TIFF tag
 * SRS reader: try to identify the CRS from PROJ db and its name, when it has no
   code
 * only write COG ghost zone if both COPY_SRC_OVERVIEWS and TILED are set
 * update internal libtiff to 4.7.1

HDF5 driver:
 * do not try opening datasets with one dimension size > INT_MAX (BAG as well)
 * support HDF5EOS formulation in VIIRS snow cover products (VNP10A1) products
   (#12941)
 * fix path issues when reading GEOLOCATION from .aux.xml (#12824)

HEIF driver:
 * implement request_range() reading callback, in particular for /vsicurl/
   access (libheif >= 1.19.0, #12239)

ISCE driver:
 * do not emit error about .xml file not accessible

ISIS3 driver:
 * adds a _data member in json:ISIS3 metadata that contains the offline content
   as hexadecimal encoded.
 * ISIS3 -> PDS4 GeoTIFF: propagate json:ISIS3 metadata
 * gdal_translate/gdalwarp: add a GDALHistory section to json:ISIS3 metadata if
   it might be invalidated
 * change mapping of PVL to JSON for 'Object = <xxx>' or 'Group = <xxx>' that
   have a Name attribute to be keyed with '<xxx>_<Name> (#13023)

JPEG driver:
 * fix GetMetadataDomainList()
 * Read metadata and subdataset for DJI thermal images in JPEG APP3s
 * enable PAM for FLIR/DJI thermal images

LIBERTIFF driver:
 * speed-up decompression on Predictor=3 on Float32 datatype

MEM driver:
 * BuildOverviews(): set geotransform and SRS from source dataset
 * improve RasterIO() performance in some cases

MRF driver:
 * Improved overview progress reporting (#12459)
 * Add QB3_BAND_MAP option

netCDF driver:
 * take into account valid_range in x/y indexing variables, only if GMT
   node_offset attribute is present (#12865)

NITF driver:
 * add a RPF_SHOW_ALL_ATTRIBUTES config option to dump all RPF attributes
   (mostly for debugging / extreme advanced users)
 * nitf_spec.xml: add definition of RPFHDR and very partial of RPFIMG
 * add support for reading part of RPF components in RPFIMG TRE

PDF driver:
 * update PDFium backend to rev7391
 * properly override FlushCache() instead of no longer existing SyncToDisk()

PDS4 driver:
 * update schema to PDS4 1O00
 * read hexadecimal values in missing_constant element
 * add support for Int64/UInt64 raster data types
 * write missing_constant value as hexadecimal for Float32 and Float64
 * read invalid_constant as nodata value when missing_constant missing
 * be more robust to untypical axis names

PNG driver:
 * implement read/write support for background color through 'BACKGROUND_COLOR'
   dataset metadata item (#12732)
 * allow ZLEVEL=0 for no compression (#12834)

Sentinel2 driver:
 * support L1B products with geolocation arrays (MPC Sen2VM)

S10x drivers:
 * add support for decoding custom CRS

S102 driver:
 * add full read support for S102 Ed 3.0 (i.e. managing multiple feature
   instance groups)
 * recognize boolean and date featureAttributeTable fields with proper GDAL
   types

S104 driver:
 * add read support for S104 Ed 2.0 (i.e. managing multiple feature instance
   groups)
 * report verticalCS in metadata

S111 driver:
 * add read support for S111 Ed 2.0 (i.e. managing multiple feature instance
   groups)
 * report verticalCS in metadata

STACTA driver:
 * recognize gs://, az://, azure:// as URL template prefixes, as well as our
   /vsi ones
 * allow reading tiles in WEBP and JPEGXL format
 * when regular /vsicurl/ access does not work, try to remap to /vsis3/, /vsigs/
   or /vsiaz/

TileDB driver:
 * add support for /vsiaz/ (#12666)

VRT driver:
 * VRT pixel functions: Add mean, median, geometric_mean, harmonic_mean, mode
   (#12418)
 * VRT pixel functions: Handle NoData values
 * VRT pixel function min() and max(): allow an optional 'k' constant
 * VRT pixel function: optimize 'sum' for common data types
 * VRT pixel function: 'sum': accept one single input
 * VRT pixel function: SSE2 optimization for  'sum', 'mean', 'min', 'max'
 * add 'transpose' option for for vrt:// connection (#12366)
 * add 'fmod' function for muparser expressions
 * VRTDerivedBand expressions: Add _CENTER_X_ and _CENTER_Y_ built-in variables
 * VRTDerivedBand: Add argmin, argmax pixel functions
 * VRTDerivedBand: Make source_names work like other builtin arguments (#12400)
 * VRTDerivedRasterBand::IRasterIO(): select a smaller source transfer type that
   the output buffer type if possible
 * VRTDerivedRasterBand::IRasterIO(): do not zero/nodata initialize source buffer
   when not needed
 * VRTDataset::CheckCompatibleForDatasetIO(): work better with anonymous datasets
 * VRTSimpleSource::UnsetPreservedRelativeFilenames(): only do something if
   m_bRelativeToVRTOri is set
 * Add a GDAL_VRT_ENABLE_RAWRASTERBAND=YES/NO (default YES) config option that
   can be set to NO to disable VRTRawRasterBand at run-time
 * VRTRawRasterBand: add a GDAL_VRT_RAWRASTERBAND_ALLOWED_SOURCE config option
   to restrict allowed sources, and make it default to
   SIBLING_OR_CHILD_OF_VRT_PATH (breaking change)
 * ComputeStatistics(): when there's a single source and we can use its
   statistics (after recomputing them), strictly preserve values (#12650)
 * multidim: allow opening a multi-band classic dataset, and add support for
   serialization of null values in a VRTMDArraySourceInlinedValues
 * multidim: respect creation order for arrays and groups
 * multidim: add support for array BlockSize
 * multidim: fix serialization of sources w.r.t relativeToVRT attribute
 * Raise error if pixel function provided without VRTDerivedRasterBand

WEBP driver:
 * add read and creation support for .wld worldfiles (#13025)

WMS driver:
 * add a mini-driver for International Image Interoperability Framework (IIIF)
   Image API 3.0

WMTS driver:
 * URL encode strings before passing to CURL

Zarr driver:
 * support opening directly .zarray, .zgroup, .zmetadata, zarr.json files
 * fix creating a /vsizip/ file (#12790)
 * make GDALDataset::Close() report errors when write failure occurs (#12790)
 * Kerchunk JSON: assorted set of fixes and improvements
 * recognize STAC proj:epsg and proj:wkt2 attributes to expose the CRS from
   EOPF Sentinel Zarr Samples Service datasets
 * Zarr V3: on creation with CHUNK_MEMORY_LAYOUT=F, no longer write
   "order":"F", but the permutation array instead

## OGR 3.12.0 - Overview of Changes

### Core

* Add OGR_G_CreateFromEnvelope()
* Add OGRGeometry::ConstrainedDelaunayTriangulation()
* OGRUnionLayer::GetFeatureCount(): avoid potential int64 overflow
* OGRLayer::SetNextByIndex(invalid_index): return OGRERR_NON_EXISTING_FEATURE
  and make sure following calls to GetNextFeature() return nullptr (#12832)
* OGRLayerArrow: fix reading time64[us]/time64[ns] type when OGR field type is
  OFTTime
* Add const correctness for GDALDataset::GetLayer()/GetLayerCount()/
  TestCapability() and OGRLayer::GetName()/GetGeomType()/GetLayerDefn()/
  GetSpatialRef() and TestCapability()
* GDALDataset: make 'GetLayer(int) const' return a 'const OGRLayer*', and add a
 'OGRLayer *GetLayer(int)' non const override
* GDALDataset::GetSpatialRef(): make it work on vector datasets (#12956)
* Make virtual methods OGRLayer::GetFIDColumn()/GetGeometryColumn() const
* OGRSchemaOverride: allow using '*' wildcard for layer name, and
  'srcType'/'srcSubType' as an alternative to field name when patching
* OGRUnionLayer: improve performance of GetFeature() once a full scan has
  already been made
* OGRGeometryFactory::forceTo(): fix potential nullptr dereference
* OGRCircularString::segmentize(): fix crash/read-heap-overflow on M geometries
  (#13303)
* OGRGeometryFactory::transformWithOptions(): avoid warnings when options are
  set but doing geographic->projected transformation (#13310)

### OGRSpatialReference

* OSRGetCRSInfoListFromDatabase(): report celestial body name
* Add OSRGetCelestialBodyName() / OGRSpatialReference::GetCelestialBodyName()

### Vector Utilities

* gdal vector clip: Don't explode result if input was multipart (#13210)
* gdal vector convert / gdal vector pipeline ! write: add --skip-errors (#12794)
* gdal vector convert: do not default to shapefile if output file name has no
  .shp extension
* gdal vector convert: support updating existing dataset and providing output
  open options
* gdal vector write/convert: add --upsert (#13006)
* gdal vector edit: add --unset-fid
* gdal vector info: don't make it output features in text mode,
  unless --features is specified
* gdal vector info: add a --limit=<FEATURE-COUNT> option (#12876)
* gdal vector info: do not implicitly set -al with -json (#12510)
* gdal vector info --update: deprecate it in favor of 'gdal vector sql --update'
* gdal vector reproject: fix reprojecting from polar CRS to geographic
  coordinates (#13222)
* gdal vector sql: add a --update mode to modify in-place a dataset (##12466)
* ogr2ogr: avoid int64 addition overflow on huge feature counts
* ogr2ogr -clipsrc/-clipdst: adjust feature geometry type to match layer
  geometry type (#13178)
* ogrtindex: replace implementation by use of gdal vector index
* ogrmerge.py: validate value of -s_srs/-t_srs/-a_srs

### Vector drivers

ADBC driver:
 * add support for ADBC BigQuery driver
 * implement a loading function for DuckDB even when OGR_ADBC_HAS_DRIVER_MANAGER
   is off
 * do not try to open in update mode
 * implement deferred loading of layers (when SQL open option not specified)
 * error out on non-existing database with DuckDB (#1368)

Arrow/Feather driver:
 * make it able to recognize the CRS when geoarrow.wkb extension is there without
  'geo' metadata
 * make it work properly when geoarrow.wkb extension is loaded

Arrow/Parquet:
 * handle list of binary and binary as JSON content (struct/list_of_list/
   array_of binary)

CSV driver:
 * update FID to 64 bit
 * Allow to use the PIPE as separator for layer creation
 * Add a HEADER=YES/NO layer creation option (#9772)

DXF driver:
 * add support for reading Transparency / group code 440
 * add write support for true color (code group 420) and transparency
   (code group 440)
 * add read/write support for HATCH background color (bc), brush id (id),
   angle (a), scaling factor (s)
 * further refine ByBlock and ByLayer color handling
 * add read support for AutoCAD Binary DXF format
 * add a special "ogr2ogr out_ascii.dxf in_binary.dxf" code path for
   direct translation

ESRIJSON driver:
 * recognize esriFieldTypeDateOnly, esriFieldTypeTimeOnly,
   esriFieldTypeBigInteger, esriFieldTypeGUID and esriFieldTypeGlobalID data
   types (#13093)
 * better recognize some ESRIJSON files

GML driver:
 * Expose SKIP_CORRUPTED_FEATURES and SKIP_RESOLVE_ELEMS open options (#12770)
 * Support CityGML3 Shell (#12789)
 * avoid returning features with duplicated FIDs (#3532)

GPKG driver:
 * implement UpdateFieldDomain() and DeleteFieldDomain()
 * avoid undefined behavior when appending to a layer with a (wrong)
   feature_count = INT64_MAX

MapML driver:
 * Change MapML elements for their custom-element counter-part

MBTiles driver:
 * improve guessing of field type

MEM driver:
 * Allow creating layer from an OGRFeatureDefn
 * declare field subtypes Boolean Int16 Float32 JSON UUID

MiraMonVector driver:
 * fix uninitialized access

MVT driver:
 * fix reading files with 0-byte padding (#13268)
 * writer: fix encoding some polygons with almost flat inner rings (#13305)
 * reader: auto-fix badly oriented inner rings (#13305)

OAPIF driver:
 * recognize 'itemCount' element in Collection description

Parquet driver:
 * add support for reading/writing Parquet GEOMETRY data type (libarrow >= 21)
 * add a COMPRESSION_LEVEL layer creation option (#12639)
 * writer: fix SQLite3 error when using SORT_BY_BBOX=YES but writing no features
   (#13328)

PDS4 driver:
 * writer: create table layers in the same directory as the .xml file and using
   its base name as a prefix

PGDUMP driver:
 * Add SKIP_CONFLICTS layer creation option

PMTiles:
 * gdal vsi list/copy: fixes so that gdal_ls.py/gdal_cp.py can be replaced by
   gdal vsi list/copy

Shapefile driver:
 * SHPCreateLL()/DBFCreate(): make error message contains full filename (in case
   it is very long)
 * Shape: workaround bug in PROJ BoundCRS::identify for CRS based on
   'NTF (Paris)' (qgis/qgis#63787)
 * Resync internal shapelib with 1.6.2

SQLite driver:
 * make REGEXP behave like official extension regarding NULL
 * advertise OLCStringsAsUTF8 (#12962)

## SWIG Language Bindings

### All languages

* map Band.GetSampleOverview() to GDALGetRasterSampleOverviewEx()
* Enable kwargs for mdim CreateDimension and CreateGroup
* make AddFieldDomain() to emit errors / exceptions
* Guard against null input to SuggestedWarpOutput
* Avoid returning garbage values from Geometry.GetPoints

### CSharp

* add Utf8 bytes conversion for name on FieldDefn creation
* add Utf8 bytes conversion on FieldDefn GetName method
* Add SpatialReference.FindMatches (#12578)
* Get rid of SWIG csharp warnings (#12596)
* Add missing typemaps (#12642)
* Update string marshaling code to use utf-8 native strings (#12546)

### Python

* Add np.bool to map of gdal codes (#12528)
* Don't promote a boolean array to float64 when trying to write the data
* accept a band as input for Driver.CreateCopy()
* hide methods that should not be public
* Add Band.BlockWindows()
* coerce config options to strings
* Docstring Updates for Stub Generation (#13198)
* Utilities as a function: consistently handle options as string (#13274)

# GDAL/OGR 3.11.5 Release Notes

GDAL 3.11.5 is a bugfix release.

## Build

* Fix Clang 21 -Wunnecessary-virtual-specifier warnings
* Add compatibility with Poppler 25.10 (support for older versions kept)
  (#13173)

## GDAL 3.11.5

### Port

* /vsis3/: fix issue when doing a new connection using EC2 credentials would go
 through WebIdentity (#13272)

### Algorithms

* InitializeDestinationBuffer(): do not return CE_Failure when emitting warning
  about INIT_DEST=NO_DATA without nodata, to make sure to 0 initialize the dest
  buffer (#13026)

### Raster core

* GDALAlgorithm: re-arrange argument validation so that
  'gdal raster create --bbox=' doesn't crash (#13112)
* GDALMDArrayRegularlySpaced::IRead(): avoid potential unsigned integer
  overflow
* Multidim: make CreateSlicedArray() also slice indexing variables of
  dimensions (#13119)
* Multidim: Fix ``GetView(["::-1"])`` on a dimension of size 1

### Raster utilities

* GDALInfo(): fix crash on datasets not linked to a driver (#13106)
* gdaldem: fix wrong results on non north-up src ds with aspect/tpi/tri
  (and on rotated for hillshade/slope/roughness) (#13100)
* Fix crash on 'gdal_translate -of COG -b 1 -b 2 -b 3 -b mask
  RGBmask_with_ovr.tif out.tif', and tag mask band turned as regular one as
  alpha (#13183)

### Raster drivers

PDF driver:
 * Properly override FlushCache() instead of no longer existing SyncToDisk()

GTI driver:
 * make sure that a non readable source causes IRasterIO() to fail (#13212)

GTiff driver:
 * fix crash when setting color interpretation on newly created mask band

HDF4 driver:
 * skip long/lat values at nodata when creating GCPs (#13207)

LIBERTIFF driver:
 * fix reading a RGB pixel-interleaved file into a RGBA pixel-interleaved
   buffer (#13193)

VRT driver:
 * fix slowness when downsampling from VRTs with explicit resampling=nearest
   (qgis/QGIS#63293)
 * Pansharpening: make sure VRTPansharpenedRasterBand of overviews inherit the
   nodata value from the full res band
 * VRTMDArraySourceFromArray::Read(): fix various issues when reading with a
   negative step (#13236)

Zarr driver:
 * Kerchunk JSON: assorted set of fixes and improvement for datasets such as in
   https://noaa-nodd-kerchunk-pds.s3.amazonaws.com/index.html#nos/cbofs/
 * avoid infinite recursion on archives with hostile object names
 * emit an error when reading from a JSON/Kerchunk reference store and one of
   the pointed file cannot be opened (#13126)

## OGR 3.11.5

### Vector core

* OGRParseDate(): make it accept leap seconds
* Geometry reprojection: fix issues with polar to geographic reprojection
  (#13222)
* OGRBuildPolygonFromEdges(): return multipolygon when appropriate (fixes
  reading some DXF HATCH) (#13230)
* OGRGeometryFactory::forceTo(): fix potential nullptr dereference

### Vector utilities

* gdal vector reproject: fix reprojecting from polar CRS to geographic
  coordinates (#13222)

### Vector drivers

ADBC driver:
 * error out on non-existing database with DuckDB (#13168)

DXF driver:
 * fix taking into account ENCODING open option (#13224)

ESRIJSON driver:
 * recognize esriFieldTypeDateOnly, esriFieldTypeTimeOnly,
   esriFieldTypeBigInteger, esriFieldTypeGUID and esriFieldTypeGlobalID data
   types
 * JSON variant detection heuristics: better recognize some ESRIJSON files

GML driver:
 * add support for gml:TimeInstantType (#13120)

GMLAS driver:
 * add support for gml:TimeInstantType (#13120)

GPKG driver:
 * optimize GetNextArrowArray() on SQL result layers that return 0 row (#13041)

MapInfo driver:
 * .tab: fix support of px vs pt for pen width, including fractional point
   width (#13064)

MBTiles driver
 * improve guessing of field type (#13232)

MVT driver:
 * fix reading files with 0-byte padding (#13268)

WFS driver:
 * make spatial filter be forwarded to server even if we don't understand the
   XSD schema (#13120)

## SWIG bindings

* Guard against null input to SuggestedWarpOutput (#13054)
* fix non-freeing of dataset created with CreateVector()

# GDAL/OGR 3.11.4 Release Notes

GDAL 3.11.4 is a bugfix release.

## Build

* Install missing symlinks for completions of a few missing utilities, and
  remove ones that are no longer installed
* CMake: fix checks for CMAKE_SYSTEM_PROCESSOR on non Windows platforms
* Various compiler and cppcheck warning fixes
* Add option to disable libavif version check

## GDAL 3.11.4

### Port

* AWS: Fix aws sso cache file location and region parameter (#12064)
* /vsis3/: retrieve path specific options in ReadDir()
* /vsiaz/: fix ReadDir() with AZURE_NO_SIGN_REQUEST=YES
* /vsirar/: fix Read() that can return a negative value when opening a rar made
  of a single file with /vsirar/the.rar (#12944)

### Algorithms

* Warp: use UInt16 nearest neighbor warping specific code path
* Warp: fix error when reprojecting large raster (such as WMTS with global
  extent) (#12965)
* Warp: avoid inserting CENTER_LONG when warping whole >=360 longitude range
  to WebMercator (#13017)

### Raster core

* GDALNoDataMaskBand::IRasterIO(): fix corruption when reading from Byte band
  and nLineSpace > nBufXSize (3.10.0 regression)
* RAT: Fix invalid memory access in ValuesIO
* GetDefaultHistogram(): fix error (on non Byte type) when min=max (#12851)
* GDALMDArray::AsClassicDataset(): fix crash on 1D-array when iYDim is invalid
  (#12855)
* GDALAntiRecursionStruct: fix so that a std::map doesn't increase out of
  control (#12931)
* RasterIO/overview mode resampling: properly takes into account NaN for
  Float16/CFloat16

### Raster utilities

* gdal_translate: display full synopsis in case of error (#12763)
* gdalmdiminfo: fix crash on a null string attribute
* gdalwarp: for TPS warping, use -wo SOURCE_EXTRA=5 by default (#12736)
* gdal_footprint: fail if there is a simplification error and there is a single
  input feature (#12724)
* Make 'gdal mdim info' return 0 when there is no error (#12796)
* gdal_viewshed: set lower bound of DEM to input raster (#12758)
* gdal info: fix --help and 'gdal info i_do_not_exist --format=text' (#12812)

### Raster drivers

BT driver:
 * Restored (was removed in 3.11.0) (qgis/QGIS#63015)

COG driver:
 * fix creation with complex data types (#12915)

ENVI driver:
 * warn/error out if samples/lines/bands are greater than INT_MAX (#12781)

GTI driver:
 * fix erroneous removal of contributing source in some cases

GTiff driver:
 * fix creating a R,G,B,Nir file without explicit PHOTOMETRIC creation option
 * SRS reader: fix misidentification of vertical datum NAVD88 (as 'Derived
   California Orthometric Heights of 1988 epoch 2025') with PROJ 9.7dev
   database

GTiff/COG drivers:
 * emit warnings when using JXL_DISTANCE/JXL_ALPHA_DISTANCE without
   JXL_LOSSLESS=NO

HDF5 driver:
 * multidim: fix reading array with non-default stride
 * fix path issues when reading GEOLOCATION from .aux.xml (#12824)

JPEGXL driver:
 * Make 'gdal_translate non_byte.jxl byte.jxl -ot Byte' work properly

KMLSuperOverlay driver:
 * fix creating datasets using extended-length path on Windows (#12601)

netCDF driver:
 * make LIST_ALL_ARRAYS=YES work on datasets that have no 2D array (#12793)

RCM driver:
 * remove illegal calls to CPLFree() in error code path

## OGR 3.11.4

### Vector utilities

* gdal vector concat: allow to concat more than 1000 files

### Vector drivers

Arrow/Parquet drivers:
 * implement Close() and call it from destructor, so that delete on a dataset
   properly flushes

CSV driver:
 * fix opening directory with .csv and .prj files (#12728)
 * fix file descriptor leak in one case

GML driver:
 * takes into account JGD2024 CRS from recent Japan's Fundamental Geospatial
   Data (FGD) (#12897)

GPKG driver:
 * fix random crash in GetNextArrowArrayAsynchronous() (#12934)

MongoDB driver:
 * add compatibility with >=mongo-cpp-driver-4

OCI driver:
 * fix varchar2 type length

SQLite driver:
 * make REGEXP behave like official extension regarding NULL handling

## Python bindings

* Dataset/Band.WriteArray(): fix writing of arrays with a 0-stride (#12913)

## Java Bindings

* restore -fno-strict-aliasing
* avoid double free with Band.GetDataset().Close() (#12764)

# GDAL/OGR 3.11.3 Release Notes

GDAL 3.11.3 is a bugfix release.

PG driver:
 * restore string truncation that was broken in 3.11.1

# GDAL/OGR 3.11.2 Release Notes

GDAL 3.11.2 is a bugfix release.

## Build

* sqlite_rtree_bulk_load.c: add missing stdlib.h include

## GDAL 3.11.2

### Port

* Unix/Win32/Sparse/Archive VSI: make sure that Close() can be called multiple
  times, and is called by destructor
* /vsizip/: fix memory leak when opening a SOZip-enabled file (pyogrio#545)

### Core

* GDALAlgorithmArg::Serialize(): fix serialization of list arguments with
  SetPackedValuesAllowed(false)
* GetHistogram(), ComputeRasterMinMax(), ComputeStatistics(): fix wrong use of
  GetLockedBlockRef() that could cause crashes with the MEM driver
* Statistics computation: avoid warnings with large Int64/UInt64 nodata value
  not exactly representable as double (#12628)

### Raster utilities

* gdalwarp: fix reprojecting to COG (3.11.0 regression)
* gdallocationinfo: handle properly nodata values (3.10.0 regression fix)

### Raster drivers

AAIGrid/GRASSASCII/ISG drivers:
 * avoid excessive memory allocation on truncated/corrupted/hostile file
   (#12648)

BMP driver:
 * Create(): avoid nullptr dereference on too wide image

GSAG driver:
 * re-added (#12695)

GSBG/GS7BG drivers:
 * Create/CreateCopy(): stricter validation of raster dimension, to avoid int
   overflow (GS7BG), or floating-point division by zero (both)

LIBERTIFF driver:
 * fix reading WEBP-compressed RGBA images where at least one tile/
   strip has the alpha component omitted due to being fully opaque

netCDF driver:
 * properly recognize axis of 'rhos' variable for PACE OCI products
 * improve detection of X,Y axis in 3D variables thanks to the presence of a
   geolocation array

PNG driver:
 * fix caching of other bands that only worked if reading band 1 (#12713)

VRT driver:
 * expose all overviews of a single-source dataset, whatever their size
   (#12690)
 * VRTPansharpen: make virtual overview generation more tolerant to different
   number of overviews in source bands

## OGR 3.11.2

### Core

* OGRParseDate(): do not round second=59.999999 to 60.0 but 59.999
* OGRParseDate(): avoid potential out-of-bounds read in OGRPARSEDATE_OPTION_LAX
  mode (#12720)

### OGRSpatialReference

* Coordinate transformation: fix when one of the CRS has an EPSG code that is
  actually a ESRI one

### Vector utilities

* ogrinfo/ogr2ogr/gdal vector sql/etc.: raise the max size of a @filename
  argument from 1 MB to 10 MB (#12672)

### Vector drivers

S57 driver:
 * fix nullptr dereference on invalid dataset when GDAL_DATA not set

# GDAL/OGR 3.11.1 Release Notes

GDAL 3.11.1 is a bugfix release.

## Build

* Do not include cpl_float.h in gdal_priv.h, to avoid issues on Windows with
  min/max macros (#12338)
* gdal_minmax_element.hpp: fix build issue with ARM32 Neon optimizations
* Python bindings: fix build error on Windows with setuptools 80.0
* Python bindings: Add -isysroot when building (macos)
* LIBERTIFF: fix build failure with GCC 12 (#12464)
* cpl_vsi_virtual.h: add missing include (PDAL/PDAL#4742)
* Fix build with libcxx 19.1.7 on OpenBSD (#12619)
* Fix build of Python bindings with latest SWIG master (4.4.0dev)

## GDAL 3.11.1

### Port

* ZIP creation: do not warn on filenames within ZIP that are non-Latin1 (#12292)
* /vsis3/ with CPL_VSIL_USE_TEMP_FILE_FOR_RANDOM_WRITE=YES: unlink temporary file
  immediately after creation (on Unix)
* VSICurlHandle::AdviseRead() (GTIFF multithreading /vsicurl/ reading):
  implement retry strategy (#12426)
* /vsis3_streaming/: fix memory leak when retrying request
* /vsizip/ / /vsitar/: make it respect bSetError / VSI_STAT_SET_ERROR_FLAG flag,
  in particular when used with /vsicurl/ (#12572)
* /vsigs/: make UnlinkBatch() and GetFileMetadata() work when OAuth2 bearer is
  passed through GDAL_HTTP_HEADERS
* /vsiswift/: pass HTTP options for GetFileList() operation
* /vsiwebhdfs/: pass HTTP options for GetFileList(), Unlink() and Mkdir() operations

### Algorithms

* Warping: do not emit warning when specifying -wo NUM_THREADS (3.11.0 regression)
* GDALCreateHomographyTransformer(): emit CPLError()s when failures happen (#12435)
* GDALChecksumImage(): avoid potential int overflow

### Core

* gdalmdiminfo_output.schema.json: add missing data types
* GDALAlgorithm: fix reading a .gdalg.json dataset with 'GDALG' input format
  option specified (#12297)
* GDALAlgorithm: Refuse NaN for arguments with specified valid ranges
* GDALAlgorithm::ProcessDatasetArg(): do not emit error when QuietDelete() runs
* GDALAlgorithm: Raise error on malformed list arguments
* GDALAlgorithm: better propagate m_calledFromCommandLine flag
* GDALAlgorithm::Run(): make sure the progress function emits a CE_Failure
  if interrupted
* GDALRegenerateOverviewsMultiBand(): raise threshold to go to on-disk temporary
  file (#12303)
* GDALRegenerateOverviews() / GDALRegenerateOverviewsMultiBand(): avoid
  potential integer overflows on very large rasters/block size
* Overview building: avoid potential integer overflows with huge reduction factors
* GDALOpenEx(): changes so that opening PG:xxx triggers a message about missing
  plugin when it is not installed
* GDALGetOutputDriversForDatasetName(): improve error message when matching on
  prefix and not extension
* GDALGetOutputDriversForDatasetName(): turn warning into error if plugin driver
  detected but not available, and take it into account in GDALVectorTranslate()

### Utilities

* gdalmanage: fix wrong order of src/dest datasets for 'rename' and 'copy'
* gdaladdo: fix error message about --partial-refresh-from-source-timestamp only
  working with VRT (works with GTI too)
* gdaladdo: do not hide message about IGNORE_COG_LAYOUT_BREAK for COG, and
  automatically set if on -clean as this doesn't break the layout
* gdal raster overview add: allow -r none
* gdal raster overview add/delete: do not hide message about
  IGNORE_COG_LAYOUT_BREAK for COG, and automatically set if on -clean as
  this doesn't break the layout
* gdalmdimtranslate: ensure valid axis values in transpose
* gdal raster tile: port --excluded-values, --excluded-values-pct-threshold,
  --nodata-values-pct-threshold from gdal2tiles
* gdal raster tile: fix error when generating overview tiles that may happen
  for some input raster extents (#12452)
* gdal raster calc: make formulas with a comma, like "--calc=sum(A,B)", work
  on command line
* gdal CLI: fix handling of --format (#12411)
* gdal CLI: take into account config options for Bash completion
* gdalwarp to format without Create() support: create temporary file with
  CPLGenerateTempFilenameSafe() instead in final directory (which might be /vsis3/)
* gdal raster/vector pipeline: make --help act on the last pipeline step (#12445)
* gdal raster hillshade/slope/aspect/tpi/tri: make sure it keeps a reference on
  source dataset, for more robust working in streaming mode
* gdal2tiles.py: fix isfile() on non-existent /vsi file (e.g. --resume mode on
  /vsis3/) (#12453)
* gdal_translate: avoid int overflow on dataset whose at least one dimension is
  INT_MAX
* gdalwarp: avoid double->int overflows when computing target dataset size
* gdalwarp -te + -te_srs: better compute target extent in target CRS using
  OGRCoordinateTransformation::TransformBounds() (#12583)
* gdal raster calc: make input argument required (#12555)
* gdal-bash-completion: add compatibility for zsh
* gdalinfo JSON output: return integer nodata value of integer bands as integer
* gdalinfo JSON output: attach 'rat' object to 'band',
* gdalinfo JSON outpu: do not emit wgs84Extent/extent on non-georeferenced image
* add missing elements in gdalinfo_output.schema.json (#12637, #12638)

### Raster drivers

Multi-driver fixes:
 * AVIF, GRIB, COG, JPEG, netCDF, NITF, VRT, ZARR, GPKG, ARROW, PARQUET:
   fix multithreading race in getting driver metadata (#12389)

EHdr driver:
 * Create(): avoid int overflows

GDALG driver:
 * do not Open() in update mode

GRIB driver:
 * deal with longitudes > 10 radians

GSBG driver:
 * Restore GSBG (Golden Software Surfer Binary Grid 6.0) support that was removed
   in 3.11.0

GTiff driver:
 * GTiffDataset::SubmitCompressionJob(): avoid crashing on lack of memory
 * GetFileList(): list .vat.dbf file

JPEG driver:
 * Fix subdomain of FLIR metadata for RelativeHumidity item
 * FLIR metadata: expose IRWindowTransmission as such, instead of overwriting
   IRWindowTemperature value.
 * Read FLIR thermal image stored as 16-bit PNG with little-endian byte order
   (#12539)

HDF5/BAG drive:
 * do not try opening datasets with one dimension size > INT_MAX

HFA driver:
 * do not emit warning when reading NaN nodata value

LIBERTIFF driver
 * ReadBlock(): fix unsigned integer overflow (ossfuzz #421943270)

MFF driver:
 * avoid potential int overflow

netCDF driver:
 * support reading SRS/geotransfrom from Rotated Latitude Longitude grid mapping
   without ellipsoid definition

S102 driver:
 * fix opening products with no uncertainty
 * fix retrieval of NoData value when there is only a depth component

VRT driver:
 * reclassify: Avoid crash with empty mapping
 * ExprPixelFunc(): avoid potential undefined behavior
 * VRTProcessedDataset::Init(): fix memleak in one case
 * VRT pixel function: do not crash when 'expression' pixel function argument
   is missing (ossfuzz #427499233)

WCS driver:
 * avoid out-of-bounds read (oss-fuzz #416429855)

WMS driver:
 * add a mini-driver for International Image Interoperability Framework (IIIF)
   Image API 3.0

WMTS driver:
 * honor <Accept> when sending GetCapabilities response (#12354)
 * handle conflict between Accept from WMTS XML document and
   GDAL_HTTP_HEADERS/GDAL_HTTP_HEADER_FILE (#12354)

## OGR 3.11.1

### Core

* OGRexportToSFCGAL(): fix for 3D geometries for SFCGAL >= 1.5.2
* OGRUnionLayer::GetFeatureCount(): avoid potential int64 overflow

### OGRSpatialReference

* OGRSpatialReference destructor: avoid memory leak when called from
  GDALThreadLocalDatasetCache destructor

### Utilities

* ogr2ogr: avoid int64 addition overflow on huge feature counts
* ogrmerge.py: validate value of -s_srs/-t_srs/-a_srs
* gdal vector rasterize: fix --co
* gdal vector grid: avoid segfault if first feature has no geometry

### Vector drivers

ADBC driver:
 * do not try to open in update mode

Arrow/Parquet driver:
 * avoid wrong data type casts that make UBSAN unhappy

GeoPackage driver:
 * GDALGeoPackageDataset::AddFieldDomain(): fix error message
 * avoid undefined behavior when appending to a layer with a (wrong)
  feature_count = INT64_MAX

GML driver:
 * geometry parser: recognize '<gml:Curve><gml:segments/></gml:Curve>' as
   LINESTRING EMPTY
 * GML2OGRGeometry_XMLNode_Internal(): fix emission of error message

IDF driver:
 * do not use deprecated Memory driver

LIBKML driver:
 * advertise Date/Time/DateTime/Integer64 (mapped as string) as types on
   creation; do proper mapping of bool (#12292)

MiraMonVector driver:
 * Fixing decimal figures error

OAPIF driver:
 * recognize 'itemCount' element in Collection description

OpenFileGDB driver:
 * writer: error out if creating a range field domain whose minimum and/or
   maximum value is missing

Parquet driver:
 * initialize arrow::compute for Arrow >= 21
 * better error message with libarrow <= 18 when file cannot be opened

PG/PGDump drivers:
 * fix truncation of identifiers with UTF-8 characters (#12532)

Shapefile driver:
 * GetNextArrowArray(): fix memleak when FID column is not requested

SQLite driver:
 * REGEXP: fix memory leak in PCRE2 implementation

VRT driver:
 * ogrvrt.xsd: update to support as well MULTIPOLYGON in SrcRegion

WFS driver:
 * fix memory leak on IN SQL keyword

## SWIG bindings

* map Band.GetSampleOverview() to GDALGetRasterSampleOverviewEx()
* Python bindings: fix issues with None bounds for ogr.CreateRangeFieldDomain()
  and ogr.CreateRangeFieldDomainDateTime() (#12564)
* make AddFieldDomain() to emit errors / exceptions
* CSharp interface: Add SpatialReference.FindMatches (#12578)

# GDAL/OGR 3.11.0 "Eganville" Release Notes

GDAL/OGR 3.11.0 is a feature release.
Those notes include changes since GDAL 3.10.0, but not already included in a
GDAL 3.10.x bugfix release.

## In a nutshell...

Highlight:

* [RFC 104](https://gdal.org/en/latest/development/rfc/rfc104_gdal_cli.html):
  Adding a "gdal" front-end command line interface.
  - See the [list of commands](https://gdal.org/en/latest/programs/index.html#gdal-application)
  - Includes new "gdal raster calc" and "gdal raster resclassify" utilities.
  - "gdal raster tile", C++ port of gdal2tiles, runs faster (3x to 6x in some cases)
  - Includes "gdal vsi list/copy/delete/move/sync" (ports of Python sample scripts)
  - Includes "gdal driver {driver_name}" for driver-specific commands.
  - Includes smart Bash autocompletion
  - Includes C, C++, Python API
* Add [GDALG](https://gdal.org/en/latest/drivers/vector/gdalg.html) (GDAL
  Streamed Algorithm Format) driver: reading of on-the-fly / streamed vector
  dataset replaying compatible "gdal" command lines (kind of VRT).

Other topics

* [RFC 100](https://gdal.org/en/latest/development/rfc/rfc100_float16_support.html):
  Support float16 type
* [RFC 102](https://gdal.org/en/latest/development/rfc/rfc102_embedded_resources.html):
  Embedding resource files into libgdal
* [RFC 103](https://gdal.org/en/latest/development/rfc/rfc103_schema_open_option.html):
  Add a OGR_SCHEMA open option to selected OGR drivers
* [RFC 105](https://gdal.org/en/latest/development/rfc/rfc105_safe_path_manipulation_functions.html):
  Add and use safe path manipulation functions
* [RFC 106](https://gdal.org/en/latest/development/rfc/rfc106_update_metadata.html):
  Metadata items to reflect driver update capabilities
* [RFC 107](https://gdal.org/en/latest/development/rfc/rfc107_igetextent_isetspatialfilter.html):
  Add OGRLayer::IGetExtent() and OGRLayer::ISetSpatialFilter()
* Add read-only [OGR ADBC](https://gdal.org/en/latest/drivers/vector/adbc.html)
 (Arrow Database Connectivity) driver, in particular
  with support for DuckDB or Parquet datasets (if libduckdb also installed)
* Add [LIBERTIFF](https://gdal.org/en/latest/drivers/raster/libertiff.html)
  driver: native thread-safe read-only GeoTIFF reader
* Add read-only [RCM](https://gdal.org/en/latest/drivers/raster/rcm.html)
 (Radarsat Constellation Mission) raster driver
* Add read-only AIVector (Artificial intelligence powered vector) driver
* VRT Pixel Functions: Add function to evaluate arbitrary expression (#11209)
* Substantially improved drivers: ZARR
* GeoPackage: change default version to GeoPackage 1.4 on creation (#7870)
* [RFC 108](https://gdal.org/en/latest/development/rfc/rfc108_driver_removal_3_11.html):
  - Removed raster drivers: BLX, BT, CTable2, ELAS, FIT, GSAG (Golden Software ASCII grid),
    GSBG (Golden Software 6.0 binary grid), JP2Lura, OZI OZF2/OZFX3,
    Rasterlite (v1), R object data store (.rda), RDB, SDTS, SGI, XPM,DIPex
  - Removed vector drivers: Geoconcept Export, OGDI (VPF/VMAP support), SDTS,
    SVG, Tiger, UK. NTF
  - Removed write support in following drivers: Interlis 1, Interlis 2, ADRG,
    PAux, MFF, MFF2/HKV, LAN, NTv2, BYN, USGSDEM, ISIS2
* Removed OpenCL warper
* OGR "Memory" driver deprecated, and aliased to the "MEM" driver.
  Its functionality is merged into the "MEM" driver that has raster, vector
  and multidimensional support.
* Various code linting, static code analyzer fixes, etc.
* Bump of shared lib major version

## New installed files

* Headers: gdal_fwd.h, gdal_typetraits.h, gdal_minmax_element.hpp, gdalalgorithm.h
* Binaries: gdal, gdalenhance
* Data files: gdal_algorithm.schema.json
* Man pages: a lot related to the gdal subcommands

## New optional dependencies

* [muparser](https://github.com/beltoforion/muparser) is *strongly* recommended
  to be added as a build and runtime dependency, to provide nominal support for
  C++ VRT expressions.
* [exprtk](https://github.com/ArashPartow/exprtk) may be added as a build
  dependency (this is a header-only library) to provide support for advanced
  C++ VRT expressions. Note that it causes an increase in libgdal size of about
  8 MB. exprtk support is recommended to be an addition to muparser support, not
  as a replacement.

## Backward compatibility issues

See [MIGRATION_GUIDE.TXT](https://github.com/OSGeo/gdal/blob/release/3.11/MIGRATION_GUIDE.TXT)

## Build

* CMake: add EMBED_RESOURCE_FILES and USE_ONLY_EMBEDDED_RESOURCE_FILES options
  (RFC 102)
* CMake: fix build if disabling only OpenFileGDB driver
* CMake: Export -DGDAL_DEBUG as PUBLIC for debug builds
* CMake: add USE_PRECOMPILED_HEADERS option (default OFF)
* CMake: Switch from SameMinorVersion to SameMajorVersion compatibility
* CMake: Fix PATHS keyword
* CMake: Use NAMES_PER_DIR where needed
* CMake: Export GDAL library targets
* Fix compiler warning with libxml 2.14.0
* Fix compiler warning with libarrow 20.0

## Docker

* Add options to add Oracle, ECW, MrSID drivers to ubuntu-full amd64 (disabled
  by default)
* Enable Bash completion for GDAL command line utilities

## GDAL 3.11.0

### Port

* Use CPLParseMemorySize for CPL_VSIL_CURL_CACHE_SIZE and CPL_VSIL_CURL_CHUNK_SIZE
* HTTP: retry on "SSL connection timeout" Curl error
* /vsicurl/: add a VSICURL_QUERY_STRING path specific option
* /vsicurl?: accept header.<key>=<value> to specify HTTP request headers
 (#11503)
* /vsicurl/: add GDAL_HTTP_MAX_CACHED_CONNECTIONS and
  GDAL_HTTP_MAX_TOTAL_CONNECTIONS config options (#11855)
* /vsicurl_streaming/: follow HTTP 303 See Other redirect that is used by
 cloudfront
* /vsigs/: make GetFileMetadata('/vsigs/bucket', NULL, NULL) work if using
  OAuth2 auth
* /vsis3/: advertise AWS_S3_ENDPOINT, and allow use of http:// / https://
  prefixing in it
* /vsis3/: advertise AWS_PROFILE in options
* /vsis3/, /vsigs/, /vsiaz/: better error message when credentials aren't found
* /vsis3/, /vsigs/, /vsiaz/: fix VSIOpenDir() recursive starting from FS prefix
* /vsiaz/: fix Stat('/vsiaz/')
* Add a CPLIsInteractive() function to determine if a file refers to a terminal
* GDALTermProgress: display estimated remaining time for tasks >= 10 seconds
* GDALTermProgress(): implement OSC 9;4 progress reporting protocol as supported
  by ConEmu, Windows Terminal and other terminals
* CPLDebug: Accept values of YES,TRUE,1
* Add CPLIsDebugEnabled()
* CPLBinaryToHex(): make it return empty string if out of memory happened
* Add VSIGlob() to find files using wildcards '*', '?', '['
* Add CPLGetKnownConfigOptions()
* Add CPLErrorOnce() and CPLDebugOnce()
* Add CPLTurnFailureIntoWarningBackuper and CPLErrorAccumulator classes
* Add CPLQuietWarningsErrorHandler()
* CPLGetPhysicalRAM(): cache result to avoid multiple file openings on repeated
  calls
* infback9: Fix potential vulnerable cloned functions. (#12244, CVE-2016-9840)
* Add VSIMove()
* VSIMkdirRecursive(): fix VSIMdirRecursive('dir_name') (without slash)
* CPLStrdod(): fix setting *endptr for values *starting* with inf/nan/etc. special values

### Core

* Add public gdal_typetraits.h header with gdal::CXXTypeTraits<T> and
  gdal::GDALDataTypeTraits<T>
* Add gdal_minmax_element.hpp public header, that can also be vendored, to find
  the min/max elements in a buffer (qgis/QGIS#59285)
* Add a gdal_fwd.h public header with forward definitions of GDAL/OGR/OSR C
  opaque types
* Add GDALRasterComputeMinMaxLocation / GDALRasterBand::ComputeMinMaxLocation,
  and map it to SWIG
* Add convenience GDALDataset::GeolocationToPixelLine() and
  GDALRasterBand::InterpolateAtGeolocation()
* Block cache: Allow memory units in GDAL_CACHEMAX
* Use sse2neon.h to provide Intel SSE optimizations to ARM Neon CPUs in gcore,
  PNG, GTI, overview, warp code paths
* Raster API: error out on GDT_Unknown/GDT_TypeCount in a nmber of places (#11257)
* create class gdal::VectorX to do easy vector operations
* gdaldrivermanager.cpp: Do not look for plugins in GetRealDriver when
  GDAL_NO_AUTOLOAD set (#11332)
* RAT XML serialization: add text representation of field and usage
* GDALMDArray::AsClassicDataset(): allow to set band IMAGERY metadata domain
  with a BAND_IMAGERY_METADATA option
* Add GDALTranspose2D() for fast 2D matrix transposition
* Add more explicit error message when attempting to write an implicit mask band
* Add GDALGroup::GetMDArrayFullNamesRecursive() and corresponding C and SWIG API
* Add GDAL_DCAP_CREATE_SUBDATASETS capability, and declare it in drivers that
  support APPEND_SUBDATASET=YES creation option
* Add GDALIsValueInRangeOf()
* Add GDALRasterBand::SetNoDataValueAsString()
* Fix ComputeRasterMinMax() and ComputeStatistics() on a raster with all
  values to infinity
* GDALComputeOvFactor(): try to return a power-of-two when possible (#12227)
* Register KEA deferred plugin before HDF5
* Improve error messages when creating a file with a known (deferred loading)
  driver but not installed
* tilematrixset: add definitions of WorldMercatorWGS84Quad,
  PseudoTMS_GlobalMercator, GoogleCRS84Quad
* GDALDriver::QuietDelete(): allow removing directories, except for containers
  of MapInfo and shapefiles

### Algorithms

* Transformer/warper: New Transform type: Homography (#11940)
* Warper: fix inconsistent replacement of valid value that collides with the
  dstnodata value (#11711)
* Warper: Add MODE_TIES warp option
* Warper: Mode resampling: Use src pixel coverage and allow mode of -1
* Warper: Optimize GWKLanczosSinc() and GWKLanczosSinc4Values()
* Warper: optimize speed of Lanczos resampling in Byte case without any
  masking/alpha
* Warper: improve (a bit) performance of multi-band bicubic warping
* Warper: improve performance of Byte/UInt16 multi-band bicubic warping with
  XSCALE=1 and YSCALE=1 on SSE2 / x86_64
* Warper: Guard against some invalid values of INIT_DEST
* Warper: add a transformer option ALLOW_BALLPARK=NO to disallow use of PROJ
  ballpark operations
* Warper: add a transformer option ONLY_BEST=AUTO/YES/NO
* GDALWarpOperation::ValidateOptions(): validate warp options against list of
  known ones, and emit warning if unknown found
* GDALRasterizeLayers(): do not emit warning about missing SRS if the target
  raster dataset has no SRS
* GDALCreateGenImgProjTransformer2(): validate transformer options against list
  of known ones, and emit warning if unknown found
* GDALCreateGenImgProjTransformer2(): recognize
  [SRC_SRS_|DST_SRS_][_DATA_AXIS_TO_SRS_AXIS_MAPPING|_AXIS_MAPPING_STRATEGY]
* GDALCreateGenImgProjTransformer2(): take into account HEIGHT_DEFAULT as the
  default value for RPC_HEIGHT if none of RPC_HEIGHT and RPC_DEM are specified
* GDALGridParseAlgorithmAndOptions(): accept 'radius' as a known option for
  invdist
* GDALGridParseAlgorithmAndOptions(): do propagate min_points_per_quadrant/
  max_points_per_quadrant when redirecting from invdist to invdistnn
* Viewshed: warn if a nodata value has been found in the input raster

### Utilities

* gdalenhance: promoted to documented and regression tested utility
* gdal_rasterize: allow -q in library mode even if it has no effect (#11028)
* gdalbuildvrt: add a -co switch (#11062)
* gdalbuildvrt: Emit warning on invalid value for -vrtnodata
* gdalbuildvrt: add '-resolution same' mode to check all source rasters have the
  same resolution
* gdalbuildvrt: Add '-resolution compatible' mode
* gdallocationinfo: Allow querying raster corner points (#12087)
* nearblack: avoid hardcoded logic related to alpha band processing to 4 bands
* gdal_contour: Do not include min/max when polygonize (#11673)
* gdal_translate: fail on invalid -scale values
* gdal_translate: expand output to include pixels partially within -projwin
  (#12100)
* gdal_translate: transform entire bounds of -projwin, not just two corners
  (#12100)
* gdal_translate: check that -append / APPEND_SUBDATASET=YES is supported
* gdal_translate: only copy nodata value if it can be exactly represented
* gdalwarp: fail on invalid -et values
* gdalwarp: in term progress, only display the short filename of the file being
  processed
* gdalwarp: speed-up warping to COG with heavy transformers (TPS / geoloc)
  by avoiding to instantiate it twice (#12170)
* gdalwarp: Emit error on invalid -srcnodata, -dstnodata
* gdalwarp: Guard against numeric parsing failures in INIT_DEST
* gdaldem: hillshade and slope: automatically set scale for geographic and
  projected CRS; add -xscale and -yscale
* gdal_footprint: fix -lyr_name on a newly created dataset
* gdal_rasterize: fix/simplify vertical/horizontal detection
* add a gdal_minmax_location.py sample/unofficial script
* gdal2tiles: apply srcnodata values in non-reprojected datasets
* gdal2tiles: remap PIL (Python Imaging Library)' 'antialias' resampling to GDAL
  Lanczos (#12030)
* gdal2tiles: fix wrong .kml file name and content at base resolution when --xyz is used
* gdal2tiles: lealeft HTML: Enable the generated tile layer by default
* gdal2tiles: change __version__ to be gdal.
* rgb2pct: add --creation-option (#12031)
* Python sample scripts: make --version return 0 for gdal_cp/gdal_ls/gdal_rm
 /gdal_rmdir.py
* gdal2xyz: support writing into a VSI*L file, including /vsistdout/ (#11766)
* gdalmdimtranslate: add error message when output format cannot be guessed
  (#11847)
* gdalcompare: do not emit exception when all pixels are invalid (#12137)
* Removed (unofficial) applications: gdalwarpsimple, ogrdissolve

### Raster drivers

AVIF driver:
 * add read-only GeoHEIF support. Requires libavif master (#11333)

BMP driver:
 * fix reading files with BITMAPV4/BITMAPV5 headers (3.4 regression) (#12196)

COG driver:
 * add support for INTERLEAVE=BAND and TILE creation option (hyperspectral
   use cases)
 * creation option spec: add min/max to a number of integer options

DIMAP driver:
 * read FWHM values from DIMAP2 XML files (PNEO products) (#11075)
 * PleiadesMetadataReport: take into account the center height of the scene and
   report it in a HEIGHT_DEFAULT metadata item of the RPC metadata domain
 * do not report SRS on products that have no geotransform (i.e. primary with
   RPC only) (#12112)

ECW driver:
 * fix build with VC17 and ECW SDK >= 5.1

GRIB driver:
 * apply a heuristics to auto-fix wrong registration of latitudeOfFirstGridPoint
   in products from Tokyo center

GTI driver:
 * STACGeoParquet: add support for proj:code, proj:wkt2, proj:projjson

GTiff driver:
 * add support to read RAT stored in ArcGIS style .tif.vat.dbf auxiliary file
 * GTiff/gdalwarp/COG: preserve pre-multiplied alpha information from source
   TIFF (#11377)
 * GTiff/COG writer: add hint about NBITS creation option to specify to be
   compatible of PREDICTOR creation option (#11457)
 * creation option spec: add min/max to a number of integer options
 * when using COMPRESS=JXL, switch to use DNG 1.7 compress=52546 value
 * support creating Float16, and read it as Float16 (instead of Float32 with
   NBITS=16)
 * Internal libtiff: resync with upstream

HDF4 driver:
 * hdf-eos: disable lots of code unused by GDAL

HEIF driver:
 * add tile reading. Requires libheif 1.19 (#10982)
 * add CreateCopy support (#11093)
 * add read-only GeoHEIF support. Requires libheif 1.19 (#11333)

HTTP driver:
 * Avoid warning with 'ogr2ogr out http://example.com/in.gpkg'

JPEGXL driver:
 * add support for reading Float16 (as Float32)

Leveller driver:
 * Fix for 64-bit platform compatibility (#12166)
 * Leveller: Increased highest supported document version number from 9 to 12.
   (#12191)

MBTiles driver:
 * Fix update with WEBP compression

MEM driver:
 * set IMAGE_STRUCTURE metadata for BAND INTERLEAVE

MRF driver:
 * Fix JPEG max size and caching relative path handling (#11943)
 * DeflateBlock(): avoid potential buffer overrun

netCDF driver:
 * add support to identify a geolocation array for a variable that lacks a
   'coordinates' attribute
 * add a LIST_ALL_ARRAYS open option that defaults to NO (#11913)
 * Use GeoTransform attribute to prevent precision loss (#11993)

NITF driver:
 * represent SAR products with I,Q bands as single complex band

PDF driver:
 * remove write support for GEO_ENCODING=OGC_BP
 * PDF composition creation: remove unneeded limitation on non-rotated rasters
 * fix nullptr dereference on invalid file (ossfuzz#376126833)
 * Update to PDFium 7047. For PDFium support, requires building against
   https://github.com/rouault/pdfium_build_gdal_3_11

PLMOSAIC driver:
 * Use a unique user-agent string to isolate usage of driver

RS2 driver:
 * changes related to NITF complex datasets
 * make its Identify() method not to recognize RCM (RADARSAT Constellation
   Mission) datasets that also use product.xml

SAFE driver:
 * do not recognize manifest.safe file from RCM (RADARSAT Constellation Mission)

Sentinel2 driver:
 * make the driver ready for S2C_ filenames

STACIT driver:
 * STAC 1.1 support (#11753)

VRT driver:
 * VRT pixel functions: Add function to evaluate arbitrary expression (#11209)
 * VRT pixel functions: remove limitation to at least 2 sources for min/max builtin functions
 * VRT pixel functions: add 'reclassify' pixel function (#12232)
 * VRT pixel functions: Allow mul and sum to apply constant factor to a single
   band
 * allow to use a <VRTDataset> instead of a <SourceFilename> inside a
   <SimpleSource> or <ComplexSource> (avoid <![CDATA[some stuff]]> tricks)
 * avoid artifacts on source boundaries when the VRT has an alpha or mask band,
   and non-nearest neighbour resampling is involved
 * VRTProcessedDataset: allow to define a OutputBands element to specify number
   of output bands and their data type (#11430)
 * VRTProcessedDataset: reduce block size when amount of RAM for temp buffers
   would be too large
 * VRTProcessedDataset: use GDALTranspose2D()
 * Implement VRTProcessedDataset::IRasterIO()
 * VRT processed dataset: unscale source raster values (#11623)
 * VRT: RasterIO(): propagate errors from worker thread to main thread (#11904)
 * propagate user overridden blocksize to (non-external) overviews, such as
   defined by OverviewList

ZARR driver:
 * update to latest Zarr V3 specification, including 'zstd' codec
 * add support for Kerchunk JSON and Parquet reference stores
 * read raster: error out if opening a chunk blob failed because of a CPLError()
 * Zarr v2: add support for 'shuffle', 'quantize', 'fixedscaleoffset' filters
 * Zarr v2: add support for 'imagecodecs_tiff' decompressor
 * report COMPRESSOR and FILTERS in structural metadata
 * report array dimension in subdataset list (#11906, #11913)
 * add a LIST_ALL_ARRAYS open option that defaults to NO report array dimension in subdataset list; add a LIST_ALL_ARRAYS open option that defaults to NO

## OGR 3.11.0

### Core

* Add OGRFieldDefn::SetGenerated()/IsGenerated()
* OGRFeature: Add SetGeometry overloads taking unique_ptr<OGRGeometry>
* OGRGeometryFactory: Add createFromWkt overload returning unique_ptr
* OGRGeometry classes: implement move constructor and move assignment operator
* OGRGeometry classes: make clone() detect out-of-memory and return null
* Add OGRPolygon::OGRPolygon(double x1, double y1, double x2, double y2)
* Add OGRPolygon::OGRPolygon(const OGREnvelope &envelope) constructor
* Add a OGRGeometryFactory::GetDefaultArcStepSize() method to get value of
  OGR_ARC_STEPSIZE config option
* OGRGeometryToHexEWKB(): return empty string if out of memory happened
* SQL expression tree to string: add missing parenthesis that could make
  further evaluation of operator priority wrong
* OGRLayer::GetArrowStream(): add a DATETIME_AS_STRING=YES/NO option
* various micro-optimizations in OGRFormatDouble()
* SFCGAL: Use WKB instead of WKT (#11006)
* GML2OGRGeometry: report gml:id of geometry in error messages (#11582)
* Add OGR_GT_GetSingle() and map it to SWIG
* SQLite dialect: fix when a field has the same name as the FID column

### OGRSpatialReference

* Add OSRGetAuthorityListFromDatabase() to get the list of CRS authorities used
  in the PROJ database.
* Make all Transform methods of OGRCoordinateTransformation and
  GDALTransformerFunc return FALSE as soon as one point fails to transform
  (#11817)

### Utilities

* ogr2ogr: check successful parsing of -simplify and -segmentize
* ogr2ogr: output warning hinting when -relatedFieldNameMatch can be used
* ogr2ogr: GPKG/FlatGeoBuf -> other format: in Arrow code path, use
  DATETIME_AS_STRING to preserve origin timezone (#11212)
* ogr2ogr: transfer relationships (when possible) from source dataset to target
  dataset
* ogr2ogr: do not warn when writing to .gdb without explicit OpenFileGDB/FileGDB
  output driver specification
* ogr2ogr: emit a warning if different coordinate operations are used during the
  warping of the source dataset
* ogr2ogr: add a -ct_opt switch to specify coordinate operation options:
  ALLOW_BALLPARK=NO, ONLY_BEST=YES or WARN_ABOUT_DIFFERENT_COORD_OP=NO
* ogrlineref: make -f LIBKML to work without warning (#11719)
* ogrtindex: fix error message when specifying incorrect output driver
* GDALVectorInfo(): do not crash if psOptions == nullptr

### Vector drivers

CSV driver:
 * add OGR_SCHEMA open option (RFC 103)
 * emit error when unbalanced double quotes are detected (#11845)

DXF driver:
 * add creation option to set $INSUNITS and $MEASUREMENT DXF system variables
   (#11423)
 * interpret INSERT blocks with row count or column count equal to 0 as 1
   (#11591)
 * handle MultiPoints in writer (#11686)
 * Don't hide block entities on layer 0 when that layer is frozen
 * support reading WIPEOUT entities (#11720)

FileGDB driver:
 * remove native update and creation support; forward it to OpenFileGDB driver

GeoJSON driver:
 * add a FOREIGN_MEMBERS=AUTO/ALL/NONE/STAC open option
 * writing: optimize speed of json_double_with_precision()
 * issue more warnings when invalid constructs are found (#11990)
 * do not advertise ODsCMeasuredGeometries, OLCMeasuredGeometries and
   ODsCMeasuredGeometries

GML driver:
 * add OGR_SCHEMA open option (RFC 103)
 * optimize speed of WriteLine()
 * writer: detect and report write error
 * gml:CircleByCenterPoint(): return a 5-point CIRCULARSTRING for compliance
   with ISO/IEC 13249-3:2011 (#11750)

GMLAS driver:
 * automatically map CityGML 2.0 namespace URI to OGC well known location when
   there is no schemaLocation
 * Speed up parsing of large datasets with large schemas

GPKG driver:
 * change default version to GeoPackage 1.4 on creation (#7870)
 * optimize speed of 'DELETE FROM table_name', especially on ones with RTree
 * fix querying relationships immediately after creating tables
 * GPKG and SQLite: Fix out of sync (not restored) fields after a ROLLBACK
  (#11609)
 * call sqlite3_errmsg() in error messages (#12247)

KML driver:
 * when reassembling multi-line text content, use newline character instead of
   space (#12278)

LIBKML driver:
 * fix error when creating a Id field of type integer (#11773)

MiraMonVector driver:
 * assorted set of fixes (#12039)

MySQL driver:
 * Remove deprecated reconnect option for mysql >= 80034 (#11842)

MVT driver:
 * allow generating tilesets with more than 1 tile at zoom level 0 (#11749)

NGW driver:
 * Add http request timeouts (connect timeout, timeout, retry count, retry timeout).
 * Add where clause to SQL delete command and unit test.
 * Add coded field domain support and unit test.
 * Add COG support.
 * Add web map and basemap layer support as TMS sources.
 * Add alter field support (name, alternative name, domain, comment).

OpenFileGDB driver:
 * change long name to 'ESRI FileGeodatabase (using OpenFileGDB)' (#11079)
 * writer: fix creation relationships immediately after creating tables
 * make CreateLayer(wkbTINZ) work properly
 * add support for CREATE_MULTIPATCH=YES layer creation option for compatibility
   with FileGDB driver
 * accept /vsizip/some_random_filename.zip where content is directly at
   top-level (#11836)

Parquet driver:
 * honor MIN()/MAX() field alias in OGRSQL (#12072)

PG driver:
 * implement OGRLayer::FindFieldIndex()
 * add escaping syntax for TABLES open option to allow table names with open
   parenthesis (#11486)
 * detect out-of-memory on large geometries

PGDump driver:
 * detect out-of-memory on large geometries

S57 driver:
 * fix nullptr deref on invalid file when S57 data resource files are missing
   (ossfuzz#415669422)

Shapefile driver:
 * ogr2ogr to Shapefile: write DateTime as ISO8601 string, both in Arrow and
   non-Arrow code paths
 * avoid undue warnings about loss of precision when writing some real values
  (#12154)

SQLite driver:
 * add OGR_SCHEMA open option (RFC 103)
 * Support SAVEPOINT (#11695)
 * SQLite/GPKG: run PRELUDE_STATEMENTS after end of initialization, in
   particular after Spatialite loading (#11782)

TopoJSON driver:
 * do not advertise Z capabilities
 * read a top level 'crs' member (#12216)

VFK driver:
 * Fix invalid parcel "banana" geometries (#3376, #11688)

XODR driver:
 * check that a .xodr filename is a 'real' file, and do not try to import empty
   PROJ4 string

## SWIG Language Bindings

* Add Driver.CreateVector()

### CSharp bindings

* Add VSIGetMemFileBuffer

### Python bindings

* Accept CRS definition in osr.SpatialReference constructor
* Add osgeo.gdal.VSIFile class
* Add a osgeo.gdal_fsspec module that on import will register GDAL VSI file
  system handlers as fsspec AbstractFileSystem
* Fix typo in handling of Translate widthPct, heightPct
* Add relatedFieldNameMatch parameter to gdal.VectorTranslate()
* Avoid losing error message (#11795)
* Honour GDAL_PYTHON_BINDINGS_WITHOUT_NUMPY=YES/1/ON/TRUE or NO/0/OFF/FALSE
  (#11853)
* gdal.UseExceptions(): do not emit ModuleNotFound message when numpy not
  available
* Accept NumPy types in Driver.Create
* Support os.PathLike inputs to Driver.Rename, Driver.CopyFiles
* Expose gdal_translate -epo and -eco from Python
* Add Dataset.ReadAsMaskedArray()
* Add mask_resample_alg arg to ReadAsArray methods
* fix compatibility issue with SWIG 4.3.1 and PYTHONWARNINGS=
* avoid deprecation warning with setuptools >= 77.0.3

# GDAL/OGR 3.10.3 Release Notes

GDAL 3.10.3 is a bugfix release.

## Build

* Fix build with -DWIN32_LEAN_AND_MEAN
* Fix warnings when building against Poppler 25.03.00

## GDAL 3.10.3

### Port

* cpl_http: retry "SSL connection timeout"
* /vsigs/ (and /vsiaz/): invalidate cached state of files/directories when
  changing authentication parameters (#11964)

### Algorithms

* Pansharpen: avoid I/O errors when extent of PAN and MS bands differ by less
  than the resolution of the MS band (#11889)
* Warp: fix reprojecting on empty source window with nodata != 0 and MEM driver
  (#11992)

### Utilities

* gdaldem: allow -az zero or negative

### Raster drivers

AVIF driver:
 * remove limitation that prevented from reading images bigger than 10 MB

GRIB2 driver:
 * fix reading Transverse Merctor with negative easting/falsing (#12015)
 * also fix reading it with scale factor != 0.9996

GTiff driver:
 * fix 3.10.1 regression when reading a file just created in multi-threaded
   mode with compression

MBTiles driver:
 * Fix update with WEBP compression

MRF driver:
 * allow deflate expansion

PDF driver:
 * Fix default value for DPI open option description

## OGR 3.10.3

### Core

* SQLite dialect: make it compatible with SQLite 3.49.1 in SQLITE_DQS=0 mode

### OGRSpatialReference

* ogrct.cpp: fix potential crash in multi-threaded execution (#11860)

### Utilities

* ogr2ogr: fix -upsert with a GPKG source

### Vector drivers

CSV driver:
 * fix parsing of 64 bit integers above 2^53

GeoJSON driver:
 * fix detection of features starting with
   {"geometry":{"type":"xxxxx","coordinates":[... (qgis/QGIS#61266)

GMLAS driver:
 * fix reading multiple values of a StringList field that is a repeated element
   (#12027)

GPKG driver:
 * make it compatible with SQLite 3.49.1 in SQLITE_DQS=0 mode
 * fix SetNextByIndex() followed by GetNextFeature() without explicit call to
   GetLayerDefn() (#11974)

MiraMonVector driver:
 * fix error: Unexpected Non-nullptr Return
 * fixing a word in Catalan language
 * writing VRS in metadata file + error in the zMin, zMax values of the header

MSSQLSpatial driver:
 * Fix creation of metadata tables related to "dbo"

OpenCAD driver:
 * add missing std:: qualifiers

WFS driver:
 * fix crash with GetFeatureCount() and client-side filters (#11920)

# GDAL/OGR 3.10.2 "Gulf of Mexico" Release Notes

GDAL 3.10.2 is a bugfix release.

## Build

* Fix build against Poppler 25.02.00 (#11804)
* explicitly set CMAKE_CXX_SCAN_FOR_MODULES=0
* Fix build in infback9 on MacOS and unity builds
* Move swig/python/data/template_tiles.mapml to gcore/data to avoid issues
  with Conda Forge builds (#11745)

## GDAL 3.10.2

### Port

* Fix read heap-buffer-overflow on nested /vsitar/ calls (ossfuzz #388868487)
* fix cppcheck nullPointerOutOfMemory

### Core

* GDALGCPsToGeoTransform(): return FALSE when invalid geotransform is generated
  (#11618)

### Utilities

* gdal_rasterize: Also accept doubles for -ts (#11829)

### Raster drivers

PLMOSAIC driver:
 * Use a unique user-agent string to isolate usage of driver

SNAP_TIFF driver:
 * third_party/libertiff: avoid issue with invalid offline tags with value
   offset at zero (ossfuzz #388571282)

STACIT driver:
 * add STAC 1.1 support (#11753)
 * Identify(): accept if at least 2 of 'proj:transform', 'proj:bbox' or
   'proj:shape' are present

WMS driver:
 * Update ESRI WMS links in documentation

## OGR 3.10.2

### Core

* Fix GeodesicLength() that was quite severely broken as working only on closed
  linestrings (3.10.0 regression)

### Vector utilities

* ogr2ogr: fix -clipsrc/-clipdst when a input geometry is within the envelope of
  a non-rectangle clip geometry, but doesn't intersect it (3.10.0 regression)
  (#11652, #10341)
* ogrtindex: fix error message when specifying incorrect output driver
* ogrlineref: make -f LIBKML to work without warning (#11719)

### Vector drivers

CSV driver:
 * fix parsing files with double-quote inside a field value (#11660)

DXF driver:
 * interpret INSERT blocks with row count or column count equal to 0 as 1
   (#11591)

Geoconcept driver:
 * fix potential double-free on creation error code

GML driver:
 * gml:CircleByCenterPoint(): return a 5-point CIRCULARSTRING for compliance
   with ISO/IEC 13249-3:2011 (#11750)

MiraMonVector driver:
 * Fix memory leak with oss-fuzz #393742177 scenario

MVT driver:
 * allow generating tilesets with more than 1 tile at zoom level 0 (#11749)
 * avoid infinite recursion on opening on hostile file (ossfuzz #391974926)

Parquet driver:
 * fix compiler deprecation warning with libarrow 19.0

## Python bindings

* fix wrong comment in documentation (#11631)
* on Debian, fix install target with non-Debian provided python version (#11636)
* Avoid losing error message (#11795)

# GDAL/OGR 3.10.1 Release Notes

GDAL 3.10.1 is a bugfix release.

### Build

* CMake: FindDotnet.cmake: remove obsolete cmake_minimum_required()
* CMake: fix swig/csharp/CMakeLists.txt compatibility with CMake 3.31
* CMake: use add_compile_options() instead of setting CMAKE_CXX_FLAGS for
  -fno-finite-math-only (#11286)
* Set GDAL_DEV_SUFFIX to the pre-release suffix if a corresponding Git tag was
  found.
* PDF: fix build issue on CondaForge build infrastructure (gcc 13.3)
* Fix issues in cpl_vsil_win32.cpp with latest mingw64

## GDAL 3.10.1

### Port

* CPLDebug: Accept values of YES,TRUE,1 (#11219)
* /vsiaz/: ReadDir(): be robust to a response to list blob that returns no blobs
  but has a non-empty NextMarker
* /vsis3/ / AWS: implement support for AWS Single-Sign On (AWS IAM Identity
  Center) (#11203)
* /vsicurl/: fix to allow to read Parquet partitionned datasets from public
  Azure container using /vsicurl/ (#11309)
* CPLGetPath()/CPLGetDirname(): make them work with /vsicurl? and URL encoded
  (#11467)
* Fix CPLFormFilename(absolute_path, ../something, NULL) to strip the relative
  path

### Algorithms

* GDALContourGenerateEx(): return CE_None even if the raster is at constant
  value (3.10.0 regression, #11340)

### Core

* fix memory leak when calling GDALAllRegister(), several times, on a deferred
  loaded plugin that is absent from the system (rasterio/rasterio#3250)
* GDALRasterBlock: make sure mutex is initialize in repeated calls to
  GDALAllRegister / GDALDestroyDriverManager (#11447)
* Do not sort items in IMD metadata domain (#11470)
* GDALDataset::BuildOverviews(): validate values of decimation factors

### Raster utilities

* gdalinfo: bring back stdout streaming mode that went away during argparse
  refactor
* gdalinfo: fix bound checking for value of -sds argument
* gdaldem: fix help message for subcommands
* gdaltindex: restore -ot option accidentally removed in GDAL 3.10.0 (#11246)
* gdaladdo: validate values of decimation factors

### Raster drivers

GTI driver:
 * make it work with STAC GeoParquet files that don't have a assets.image.href
   field (#11317)
 * STAC GeoParquet: make it recognize assets.XXX.proj:epsg and
   assets.XXX.proj:transform
 * attach the color table of the sample tile to the GTI single band
 * generalize logic for reading STAC GeoParquet eo:bands field to any asset
   name, and handle all 'common_names' values
 * parse central wavelength and full width half max metadata items from STAC
   GeoParquet eo:bands field
 * read scale and offset from STAC GeoParquet raster:bands field
 * STACGeoParquet: add support for proj:code, proj:wkt2, proj:projjson (#11512)
 * advertise SRS open option

GTiff driver:
 * detect I/O error when getting tile offset/count in multi-threaded reading
   (#11552)
 * CacheMultiRange(): properly react to errors in VSIFReadMultiRangeL() (#11552)
 * internal overview building: do not set PHOTOMETRIC=YCBCR when
   COMPRESS_OVERVIEW != JPEG
 * mask overview: fix vertical shift in internal mask overview computation
   (#11555)
 * JXL: add support for Float16 and Compression=52546 which is JPEGXL from DNG
   1.7 specification
 * Internal libtiff: fix writing a Predictor=3 file with non-native endianness

HDF4 driver:
 * fix REMQUOTE implementation that caused valgrind to warn about overlapping
   source and target buffers
 * hdf-eos: fix lots of Coverity Scan warnings and disable lots of code unused
   by GDAL

HTTP driver:
 * re-emit warnings/errors raised by underlying driver

KEA driver:
 * use native chunksize for copying RAT (#11446)

netCDF driver:
 * add a GDAL_NETCDF_REPORT_EXTRA_DIM_VALUES config option (#11207)
 * multidim: report correct axis order when reading SRS with EPSG geographic
   CRS + geoid model
 * fix warning message mentioning Time dimension instead of Vertical

OCI driver:
 * add a TIMESTAMP_WITH_TIME_ZONE layer creation option, and ogr2ogr tweaks

PNG driver:
 * fix reading 16-bit interlaced images (on little-endian machines)

TileDB driver:
 * remove dir only if it exists (#11485)

VRT driver:
 * fix reading from a CFloat32/CFloat64 ComplexSource in a non-complex-type
   buffer (3.8.0 regression, refs rasterio/rasterio#3070)
 * processed dataset: Read scale and offset from src dataset
 * data/gdalvrt.xsd: fix schema to reflect that <Array> can be a child of
   <ArraySource>

WMTS driver:
 * for geographic CRS with official lat,lon order, be robust to bounding box
   and TopLeftCorner being in the wrong axis order and emits a warning (#11387)

Zarr driver:
 * fix incorrect DataAxisToSRSAxisMapping for EPSG geographic CRS (#11380)

## OGR 3.10.1

### Core

* OGRWarpedLayer: do not use source layer GetArrowStream() as this would skip
  reprojection

### OGRSpatialReference

* importFromEPSG(): tries with ESRI when it looks like an ESRI code, but with
  a warning when that succeeds (#11387)

### Vector utilities

* ogrinfo: command line help text fixes (#11463)
* ogr2ogr: fix 'ogr2ogr out.parquet in.gpkg/fgb/parquet -t_srs {srs_def}'
  optimized code path (3.10.0 regression)
* ogr2ogr: fix crash with -ct and using Arrow code path (e.g source is
  GeoPackage) (3.10.0 regression) (#11348)
* GDALVectorTranslate(): fix null-ptr dereference when no source driver
* ogrlineref: fix double-free on 'ogrlineref --version'

### Vector drivers

DXF driver:
 * use Z value for SPLINE entities (#11284)
 * writer: do not set 0 as the value for DXF code 5 (HANDLE) (#11299)

FlatGeobuf driver:
 * writing: in SPATIAL_INDEX=NO mode, deal with empty geometries as if there
   were null (#11419)
 * writing: in SPATIAL_INDEX=NO mode, accept creating a file without features

GeoJSON(-like) drivers:
 * combine value of GDAL_HTTP_HEADERS with Accept header that the driver set
   (#11385)

GeoJSON driver:
 * do not generate an empty layer name when reading from /vsistdin/ (#11484)

GML driver:
 * add support for AIXM ElevatedCurve (#4600, #11425)
 * honour SWAP_COORDINATES=YES even when the geometry has no SRS (#11491)
 * gml:CircleByCenterPoint: correctly take into account radius.uom for projected CRS

GPKG driver:
 * make CreateCopy() work on vector datasets (#11282)
 * make sure gpkg_ogr_contents.feature_count = 0 on a newly created empty table
   (#11274)
 * fix FID vs field of same name consistency check when field is not set (#11527)

LVBAG driver:
 * only run IsValid() if bFixInvalidData

MapInfo driver
 * .tab: support .dbf files with deleted columns (#11173)

MVT driver:
 * emit warning when the maximum tile size or feature count is reached and the
   user didn't explicitly set MAX_SIZE or MAX_FEATURES layer creation options
   (#11408)

OpenFileGDB driver:
 * be robust to unusual .gdbindexes files with weird/corrupted/not-understood
   entries (#11295)

Parquet driver:
 * writer: write page indexes

PG driver:
 * avoid error when the original search_path contains something like '"",
   something_else' (#11386)

OGR VRT:
 * fix SrcRegion.clip at OGRVRTLayer level (#11519)
 * accept SrcRegion value to be any geometry type as well as SetSpatialFilter()
   (#11518)

## Python bindings

* add a colorInterpretation argument to gdal.Translate() and fixes a copy&paste
  issue in the similar argument of gdal.TileIndex()
* swig/python/setup.py.in: fix exception when building a RC git tag

## Java bindings

* add byte[] org.gdal.gdal.GetMemFileBuffer(String filename) (#11192)
* avoid double free when calling Dataset.Close() (#11566)

# GDAL/OGR 3.10.0 Release Notes

GDAL/OGR 3.10.0 is a feature release.
Those notes include changes since GDAL 3.9.0, but not already included in a
GDAL 3.9.x bugfix release.

## In a nutshell...

* [RFC 101](https://gdal.org/en/latest/development/rfc/rfc101_raster_dataset_threadsafety.html):
  Raster dataset read-only thread-safety
* New read/write [AVIF](https://gdal.org/en/latest/drivers/raster/avif.html)
  raster driver
* New read-only [SNAP_TIFF](https://gdal.org/en/latest/drivers/raster/snap_tiff.html)
  raster driver for Sentinel Application Processing GeoTIFF files
* New OGR read-only [XODR](https://gdal.org/en/latest/drivers/vector/xodr.html)
  driver for OpenDRIVE (#9504)
* Code linting and security fixes
* Bump of shared lib major version

## New optional dependencies

* [libavif](https://github.com/AOMediaCodec/libavif) for AVIF driver
* [libopendrive](https://github.com/DLR-TS/libOpenDRIVE) for XODR driver

## Backward compatibility issues

See [MIGRATION_GUIDE.TXT](https://github.com/OSGeo/gdal/blob/release/3.10/MIGRATION_GUIDE.TXT)

## Build

* add html, man, latexpdf, doxygen, doxygen_check_warnings, clean_doc targets
  (require doc/ subdirectory to be re-added) (#5484)
* Java and CSharp bindings: do not build sample/tests programs if
  BUILD_TESTING=OFF (#9857)
* Allow following drivers to be built in new -DSTANDALONE=ON mode: MrSID,
  JP2KAK, OCI, Arrow, Parquet, JP2OPENJPEG, TileDB, ECW, GeoRaster
* Internal zlib: update to 1.3.1
* Internal libpng: update to 1.6.43
* Add scripts/check_binaries.py to detect unknown binaries and run it in CI

## General changes

* Reduce excessive precision %.18g to %.17g
* Replace MIT license long text with 'SPDX-License-Identifier: MIT' (#10903)

## GDAL 3.10.0

### Port

* Add VSIMemGenerateHiddenFilename() and use it extensively in whole code base
* Add VSICopyFileRestartable() to allow restart of upload of large files
* Add VSIMultipartUploadXXXXX() functions for multi-part upload
* VSICopyFile(): detect error when reading source file, and delete output file
  on error
* VSI CopyFile() implementations: make them actually robust to pszSource==nullptr
* VSIMallocAligned(): make behavior more predictable when nSize == 0
* CPLHTTPFetch(): add a RETRY_CODES option (GDAL_HTTP_RETRY_CODES config option)
  (#9441)
* /vsicurl/: honor 'Cache-Control: no-cache' header
* /vsicurl/: no longer forward Authorization header when doing redirection to
  other hosts.
* /vsiaz/: add BLOB_TYPE=BLOCK and CHUNK_SIZE options to VSIFOpenEx2L()
* /vsis3/: avoid emitting a CPLError() when bSetError=false and access=r+b
* /vsigs/: make sure access token with GOOGLE_APPLICATION_FILE in authorized_user
  mode is cached
* /vsimem/: make Read() error for a file not opened with read permissions
* /vsimem/: more efficient SetLength()
* /vsicrypt/: if opening in write-only mode, do so on the underlying file as well
* /vsigzip/: sanitize Eof() detection
* /vsigzip/: Read(): detect attempts to read more than 4 GB at once
* Add VSIFErrorL() and VSIFClearErrL(), and implement them in file systems
* VSIVirtualHandle: add a Interrupt() method and implement in in /vsicurl/ (and
  related filesystems)
* VSIWin32Handle: Read(): handle cleanly nSize * nCount > UINT32_MAX
* Add VSIToCPLErrorWithMsg()
* CPLHTTPFetch(): Add support for GDAL_HTTP_VERSION=2PRIOR_KNOWLEDGE
* Add a cpl::contains(container, value) helper
* CPLFormFilename()/CPLGetDirname()/CPLGetPath(): make it work with
  'vsicurl/http://example.com?foo' type of filename, to fix Zarr driver
* Thread pool: Use std::function as pool job and job queue function (#10505, #10628)
* CPLHTTPGetOptionsFromEnv(): fallback to non-streaming filename with no path
  specific option has been found on the streaming one
* CPLSpawn() (unix): correctly return the exit() code of the process
* CPLRecode(): make ISO-8859-2 and -15 and CP437/CP1250/CP1251/CP1252 to UTF-8
  always available

### Core

* Add new GCI_ constants in particular for infra-red and SAR, and standardize
  a band-level IMAGERY metadata domain with "CENTRAL_WAVELENGTH_UM" and "FWHM_UM"
* Add GDALRasterBand::InterpolateAtPoint() and GDALRasterInterpolateAtPoint()
* GDALRasterBand: add convenience ReadRaster() methods accepting std::vector<>
* GDALIdentifyDriverEx(): pass nIdentifyFlags to GDALOpenInfo, so that drivers
  can see which type of dataset is asked for
* GDALIdentifyDriverEx(): transmit papszAllowedDrivers to Identify()
* Deprecate GDALDestroyDriverManager() and OGRCleanupAll()
* GDALNoDataMaskBand::IRasterIO(): speed optimization
* Fix GDALDataTypeUnion() to check that the provided value fits into eDT
* Add GDALIsValueExactAs()
* GDALFindDataTypeForValue(): fix for integer values that can't be represented
  as UInt64
* GDALDataset::RasterIO() / GDALDatasetRasterIO[Ex](): accept a const int*
  panBandList, instead of a int*
* Make GDALDataset::IRasterIO() implementations ready to switch to use
  const int* panBandList (enabled only if -DDGDAL_BANDMAP_TYPE_CONST_SAFE)
* Add GDALOpenInfo::IsSingleAllowedDriver()
* GDALDriver::QuietDeleteForCreateCopy(): do not set error state when attempting
  to open datasets
* Rasterband methods (histogram, statistics): make them compatible of a dataset
  with more than 2 billion blocks
* gdal_priv_templates.hpp: implement GDALIsValueInRange<[u]int64_t]>, and add GDALIsValueExactAs<>
* GDALMDArray::GetResampled(): take into account good_wavelengths array for EMIT
  dataset orthorectification
* EXIF reader: strip trailing space/nul character in ASCII tags
* GDALDatasetCopyWholeRaster(): fix SKIP_HOLES=YES in the interleaved case that
  failed to detect holes
* GDALDriver::DefaultCreateCopy(): recognize SKIP_HOLES=YES option and forward
  it to GDALDatasetCopyWholeRaster()
* Multidimensional API: add GDALMDArray::GetMeshGrid()
* Multidim: AsClassicDataset(): make it able to retrieve dataset metadata from PAM
* JSON TileMatrixSet parser: accept crs.uri and crs.wkt encodings (#10989)

### Algorithms

* Polygonize: optimizations to reduce runtime
* Polygonize: make it catch out of memory situations
* GDALRasterizeGeometries(): various robustness fixes
* GDALPansharpenOperation::PansharpenResampleJobThreadFunc(): make it more robust
* Contour: include minimum raster value as contour line when it exactly matches
  the first level (#10167)
* GDALWarpResolveWorkingDataType(): ignore srcNoDataValue if it doesn't fit into
  the data type of the source band
* Warper: fix shifted/aliased result with Lanczos resampling when XSCALE < 1 or
  YSCALE < 1 (#11042)
* Warper: optimize speed of Lanczos resampling in Byte case without any
  masking/alpha
* Warper: make sure to check that angular unit is degree for heuristics related
  to geographic CRS (#10975)

### Utilities

* gdalinfo: add -nonodata and -nomask options
* gdalinfo/ogrinfo: make --formats -json work (#10878)
* gdalbuildvrt: add a -co switch (#11062)
* gdal_translate: use GDALIsValueExactAs<> to check range of [u]int64 nodata
* gdal_viewshed: multi-threaded optimization (#9991)
* gdal_viewshed: support observers to left, right, above and below of input
  raster (#10264, #10352)
* gdal_viewshed: address potential issues with line-based interpolation (#10237)
* gdal_viewshed: add support for cumulative viewshed (#10674)
* gdal_contour: allow to use -fl with -i and -e (#10172)
* gdal_contour: add a -gt option to define the transaction flush interval
 (default it to 100,000) (#10729)
* gdallocationinfo: add -r resampling switch
* gdalwarp: allow specifying units of warp memory (#10976)
* Use GDALArgumentParser for gdal_contour, gdallocationinfo, gdaltindex,
  ogrtindex, gdal_footprint, gdal_create, gdalmdiminfo, gdalmdimtranslate,
  gdaldem, gdalmanage, ogrlineref, gdal_rasterize
* GDALArgumentParser: support sub-parser
* --formats option: detail abbreviation codes
* crs2crs2grid.py: Update to work with current data output format from HTDP (#2655)

### Raster drivers

Multi-driver changes:
 * BAG, GTI, HDF5, OGCAPI, netCDF, S102/S104/S111, STACTA, STACIT, TileDB,
   VICAR, WMS, WMTS, XYZ:
   relax identification checks when papszAllowedDrivers[] contains only the
   driver name

Derived driver:
 * make it optional

DIMAP driver:
 * for PNEO products, use the new color interpretations for the NIR, RedEdge
   and DeepBlue/Coastal bands

DTED driver:
 * added metadata items Security Control and Security Handling (#10173)

EEDA/EEDAI drivers:
 * add a VSI_PATH_FOR_AUTH open option to allow using a /vsigs/ path-specific
   GOOGLE_APPLICATION_CREDENTIALS option

GeoPackage driver:
 * Implement ALTER TABLE RENAME for rasters (#10201)
 * use GDALIsValueInRange<> to avoid potential undefined behavior casts
 * in CreateCopy() without resampling, use IGetDataCoverageStatus() on the source
   dataset
 * detect non-RGBA tiles fully at 0 and do not write them
 * implement IGetDataCoverageStatus()

GRIB driver:
 * display hint to speed-up operations when using gdalinfo on a remote GRIB2
   file with an .idx side car file

GTI driver:
 * multi-threaded IRasterIO() for non-overlapping tiles
 * recognize STAC GeoParquet catalogs
 * implement GetDataCoverageStatus()
 * emit warning if bounding box from the tile's feature extent in the index does
   not match actual bounding box of tile

GTiff driver:
 * fix nMaxBits computation for Float32/Float64
 * DiscardLsb(): use GDALIsValueExactAs<> to check range of nodata
 * handle TIFF color map that uses a 256 multiplication factor and add open
   option COLOR_TABLE_MULTIPLIER
 * clearer error when attempting to create external JPEG compressed overviews on
   dataset with color table
 * honor GDAL_DISABLE_READDIR_ON_OPEN when working on a dataset opened with
   OVERVIEW_LEVEL open option
 * better error messages when trying to create too-large untiled JPEG/WEBP
   compressed files (or with huge tiles)
 * when reading a unrecognized value of color interpretation in the GDAL_METADATA,
   expose it in a COLOR_INTERPRETATION metadata item
 * use main dataset GetSiblingFiles() for overviews
 * SRS writer: do not use EPSG:4326 if angular unit is not degree
 * make driver optional (but must be explicitly disabled)
 * tif_jxl: writer: propagate DISTANCE/QUALITY setting to extra channel
 * fix memory leak when trying to open COG in update mode
 * Internal libtiff: resync with upstream

HEIF driver:
 * correctly initialize PAM for multiple images per file
 * make simple identification more robust (#10618)
 * make it possibly accept AVIF files (depends on libheif capabilities)

HFA driver:
 * SRS reading: strip TOWGS84 when datum name is known, and use FindBestMatch()
   to try to find known SRS (#10129)
 * make driver optional and buildable as a plugin

HTTP driver:
 * make parsing of Content-Disposition header aware of double quotes around
   filename=

ISIS3 driver:
 * Create(): open file(s) in wb+ mode if possible

ISG driver:
 *  Parse dms in ISG format

JP2ECW driver:
 * report JPEG2000 tile size as GDAL block size for ECW SDK >= 5.1
   (up to 2048x2048 and dataset dimensions)

JP2KAK driver:
 * add Cycc=yes/no creation option to set YCbCr/YUV color space and set it to
   YES by default (#10623)

JP2Lura driver:
 * fix Identify() method
 * planned for removal in GDAL 3.11

JPEG driver:
 * make sure not to expose DNG_UniqueCameraModel/DNG_CameraSerialNumber if the
   corresponding EXIF tags are set at the same value
 * add support for reading Pix4DMapper CRS from XMP

JPEGXL driver:
 * writer: propagate DISTANCE/QUALITY setting to extra channel (#11095)
 * writer: allow a minimum DISTANCE of 0.01, max of 25 and revise QUALITY to
   DISTANCE formula (#11095)

KMLSuperOverlay driver:
 * recognize <GroundOverlay> with <gx:LatLonQuad> forming a rectangle (#10629)

MEM driver:
 * disable opening a dataset with MEM::: syntax by default (unless
   GDAL_MEM_ENABLE_OPEN build or config option is set)

MRF driver:
 * (~twice) faster LERC V1 encoding (#10188)
 * Add 64bit int support (#10265)
 * Allow open of MRF-in-TAR as MRF (#10331)
 * MRF/LERC: check that values are within target type range before casting
 * enable QB3_FTL mode when available (#10753)

netCDF driver:
 * multidim: add OpenMDArray() options to set up nc_set_var_chunk_cache()
 * use GDALIsValueExactAs<> to check range of [u]int64 nodata
 * do not override GDALPamDataset::TrySaveXML() to allow proper mixing of
   classic and multidim PAM metadata
 * make sure CreateMetadataFromOtherVars() doesn't set PAM dirty flag
 * simplify identification logic by just checking runtime availability of
   HDF4/HDF5 drivers.
 * CreateCopy(): fix taking into account NETCDF_DIM_EXTRA when source dataset is
   not georeferenced
 * do not emit error when longitude axis unit is degrees_east and latitude axis
   unit is degrees_north (#11009)

NITF driver:
 * Create()/CreateCopy(): make sure that provided ABPP creation option is zero
   left-padded
 * add an alias of existing ABPP creation option as NBITS; propagate NBITS/ABPP
   creation option to JPEG2000 drivers (#10442)
 * remove ABPP field as NBITS IMAGE_STRUCTURE metadata item, instead of NBPP
 * CreateCopy(): support Blue,Green,Red[,other] color interpretation ordering
   (#10508)
 * when built as plugin, make sure to register the RPFTOC and ECRGTOC drivers too

OGCAPI driver:
 * combine CURL error message and data payload (when it exists) to form error
   message

OpenFileGDB driver:
 * Identify: if nOpenFlags == GDAL_OF_RASTER, and the dataset is local, check
   file extensions to better identify
 * use GDALIsValueInRange<> to avoid potential undefined behavior casts

PDF driver:
 * PDFium backend: update to PDFium 6677

PDS4 driver:
 * Create(): open file(s) in wb+ mode if possible
 * flip image along horizontal axis when horizontal_display_direction = Right
   to Left (#10429)

S102 driver:
 * GDALPamDataset::XMLInit(): preserve DerivedDataset nodes that affect S102
   PAM saving
 * add (limited) support for IHO S102 v3.0 specification (#10779)

SAFE (Sentinel1):
 * report a FOOTPRINT metadata item
 * report failure to opening a band dataset as a warning

STACIT driver:
 * correctly return the STACIT driver as the dataset's driver, instead of VRT
 * support top-level Feature in addition to FeatureCollection

SRTMHGT driver:
 * add support for reading/wrinting 0.5 deg resolution datasets (#10514)

TileDB driver:
 * implement GetNoDataValue()/SetNoDataValue()
 * implement BuildOverviews() (for dataset created with CREATE_GROUP=YES)
 * Default to CREATE_GROUP=YES when creating datasets
 * allow to potentially read/write more than 2GB at once
 * IRasterIO(): fix checks related to pixel/line/band spacing for optimized code paths
 * CopySubDatasets(): support very large number of blocks and very large block size
 * CreateCopy(): return nullptr in error cases
 * CreateCopy(): do not force returned dataset to be in read-only mode
 * IRasterIO(): avoid reading out-of-raster with TILEDB_ATTRIBUTE creation option,
   when the block size is not a multiple of the raster size

VICAR driver:
 * handle VICAR header being located beyond 2 GB within PDS3 file

VRT driver:
 * multi-thread IRasterIO() in 'simple' situations
 * reduce mutex contention, particularly useful for multithreading of remote
   sources
 * GetDataCoverageStatus(): implement special case on a single source covering
   the whole dataset
 * GDALAutoCreateWarpedVRT(): ignore src nodata value if it cannot be represented
   in the source band data type
 * VRTComplexSource::RasterIO(): speed-up pixel copy in more cases
 * Warped VRT: instantiate overview bands in a lazy fashion for faster execution
 * VRTWarpedDataset::IRasterIO(): avoid a memory allocation and pixel copy when
   possible
 * VRTWarpedDataset::IRasterIO(): optimize I/O when requesting whole image at a
   resolution that doesn't match an overview
 * allow it to be partially disabled if GDAL built with GDAL_ENABLE_VRT_DRIVER=OFF
 * use GDAL_OF_VERBOSE_ERROR flag in vrt:// and pansharpened modes

WMTS driver:
 * when reading a WMTS capabilities file, use in priority Operation.GetCapabilities.DCP.HTTP
   to retrieve the URL (#10732)
 * try to be robust to servers using CRS instead of SRS in 1.1.1 mode (#10922)

XYZ driver:
 * add COLUMN_ORDER open option

Zarr driver:
 * Zarr V2 creation: fix bug when creating dataset with partial blocks and need
   to re-read them in the writing process when compression is involved (#11016)
 * Allow int64 attributes (#10065)
 * SerializeNumericNoData(): use CPLJSonObject::Add(uint64_t) to avoid
   potential undefined behavior casts
 * Create(): remove created files / directories if an error occurs (#11023)

## OGR 3.10.0

### Core

* OGRSQL and SQLite dialect: add STDDEV_POP() and STDDEV_SAMP() aggregate functions
* SQL SQLite dialect: fix translation of "x IN (NULL)" with "recent"
  (at least > 3.31.1) versions of SQLite3
* OGRSQL: fix compliance of NOT and IN operators regarding NULL values
* OGRSQL: SQL expression tree to string: add missing parenthesis that could make
  further evaluation of operator priority wrong
* OGRSQL: add SELECT expression [AS] OGR_STYLE HIDDEN to be able to specify
  feature style string without generating a visible column (#10259)
* OGRSQL: use Kahan-Babuska-Neumaier algorithm for accurate SUM()
* OGRSQL: avoid going through string serialization for MIN(), MAX(), SUM(),
  AVG() on numeric fields
* OGRSQL: do not query geometry column(s) when not needed
* SQLite SQL dialect: add MEDIAN, PERCENTILE, PERCENTILE_CONT and MODE
  ordered-set aggregate functions
* SQLite/GPKG: extend gdal_get_pixel_value()/gdal_get_layer_pixel_value()
  to support an interpolation method
* SQLite/GPKG: Add ST_Length(geom, use_ellipsoid)
* GetNextArrowArray() generic implementation: avoid calling
  VSI_MALLOC_ALIGNED_AUTO_VERBOSE() with a zero size
* Arrow reading: generic code path (as used by GeoJSON): fix mis-handling of
  timezones
* OGRFeature: optimizations while accessing field count
* OGRFeature: SetXXX() methods: more informative warning messages reporting
  field name and value for out-of-range values
* Add OGRGeometry::BufferEx() method
* Add OGRGeometry::hasEmptyParts()/removeEmptyParts()
* Add OGRCurve::reversePoints(), and deprecated OGRLinearRing::reverseWindingOrder()
* Add OGRGeometryCollection::stealGeometry()
* Add OGR_G_GeodesicLength() and OGRCurve/OGRSurface/OGRGeometryCollection::get_GeodesicLength()
* make OGR_G_GetLength() work on surfaces, suming the length of their
  exterior and interior rings.
* OGR geometry classes: add a bool return type for methods that can fail
* OGR geometry classes: mark default constructors directly in .h and removed
  useless overridden destructor for better code generation
* MakeValid(METHOD=STRUCTURE): make sure to return a MULTIPOLYGON if input is
  MULTIPOLYGON (#10819)
* GML geometry reader: add support for gml:OrientableCurve (#10301)
* GenSQL layer: implement OLCFastGetArrowStream when underlying layer does and
  for very simple SQL statements
* WKT geometry importer: accept (non conformant) PointZ/PointZM without space as
  generated by current QGIS versions
* OGRLineString::SetPoint(): avoid int overflow if passing iPoint = INT_MAX
* OGRCurveCollection::addCurveDirectly(): avoid int overflow if adding to a
  already huge collection
* OGRGeometryCollection::addGeometryDirectly(): avoid int overflow if adding to
  a already huge collection
* OGRGeometryFactory::transformWithOptions(): in WRAPDATELINE=YES mode, return a
  multi polygon if a input multi polygon has been provided (#10686)
* OGRGeometryFactory::transformWithOptions(): deal with polar or anti-meridian
  discontinuities when going from projected to (any) geographic CRS
* OGRProjCT::TransformWithErrorCodes(): speed-up by avoiding OSRGetProjTLSContext()
  when possible
* Add OGRWKBTransform() for in-place coordinate transformation of WKB geometries
* OGR_GreatCircle_ API: do not hardcode Earth radius
* ogr_core.h: suppress warning when building with -Wpedantic for C < 23 (#2322)

### OGRSpatialReference

* OGRSpatialReference::FindMatches(): improve when input SRS doesn't have
  expected axis order
* OGRSpatialReference::EPSGTreatsAsLatLong()/EPSGTreatsAsNorthingEasting():
  remove the check on the EPSG authority
* Add OSRGetAuthorityListFromDatabase() to get the list of CRS authorities used
  in the PROJ database.

### Utilities

* ogr2ogr: add -skipinvalid to skip features whose geometry is not valid w.r.t
  Simple Features
* ogr2ogr: error out if WKT argument of -clipsrc/-clipdst is an invalid geometry
  (#10289)
* ogr2ogr: speed-up -clipsrc/-clipdst by avoiding GEOS when possible
* ogr2ogr: speed-up -t_srs in Arrow code path using multi-threaded coordinate
  transformation
* ogr2ogr: optim: call GetArrowStream() only once on source layer when using
  Arrow interface
* ogr2ogr: fix -explodecollections on empty geometries (#11067)
* validate_gpkg.py: make it robust to CURRENT_TIMESTAMP instead of 'now'

### Vector drivers

Multi-driver changes:
 * Arrow, CSV, ESRIJSON, JSONFG, GeoJSON, GeoJSONSeq, GML, GTFS, LVBAG, NAS,
   OAPIF, TopoJSON:
   relax identificationchecks when papszAllowedDrivers[] contains only the
   driver name
 * GeoJSON like driver: avoid fetching unrecognized HTTP dataset more than once

Arrow ecosystem:
 * Arrow/Parquet/generic arrow: add write support for arrow.json extension
 * Add a Arrow VSI file system (for libarrow >= 16.0) allowing to use GDAL
   VSI file systems as libarrow compatible file systems.
 * Add (minimum) support for libarrow 18.0.0

Arrow driver
 * add read support for StringView and BinaryView (but not in OGR generic Arrow
   code)
 * use recommended item names for GeoArrow [multi]line, [multi]polygon, multipoint

CSV driver:
 * error out if invalid/inconsistent value for GEOMETRY layer creation option
   (#10055)
 * allow inf, -inf and nan as numeric values
 * emit warning when reading invalid WKT (#10496)

CSW driver:
 * make it buildable as plugin

DGN driver:
 * add ENCODING open option and creation option (#10630)

DXF driver:
 * add a DXF_CLOSED_LINE_AS_POLYGON=YES/NO configuration option (#10153)

ESRIJSON driver:
 * make it able to parse response of some CadastralSpecialServices APIs (#9996)
 * use 'alias' field member to set OGR alternative field name

FileGDB/OpenFileGDB drivers:
 * update (and unify) list of reserved keywords that can't be used for column
   and table names (#11094)

JSONFG driver:
 * avoid Polyhedron/Prism geometry instantiation during initial scan

GeoJSON driver:
 * make it (and companion TopoJSON, ESRIJSON, GeoJSONSeq) optional (but must be
   explicitly disabled) and buildable as plugin
 * avoid false-positive identification as TopoJSON

GeoJSONSeq driver:
 * add a WRITE_BBOX layer creation option

GML driver:
 * XSD parser: fix to resolve schema imports using open option USE_SCHEMA_IMPORT
   (#10500)
 * make it buildable as plugin if NAS driver is explicitly disabled
 * add a GML_DOWNLOAD_SCHEMA config option matching the DOWNLOAD_SCHEMA open
   option (and deprecate undocumented GML_DOWNLOAD_WFS_SCHEMA)

GPKG driver:
 * prevent from creating field with same name, or with the name of the geometry
   field
 * CreateField(): check we are not reaching the max number of fields
 * SQLite/GPKG: turn on SQLite 'PRAGMA secure_delete=1' by default

HANA driver:
 * Add support for REAL_VECTOR type (#10499)
 * Add support for fast extent estimation (#10543)

KML driver:
 * make it optional and buildable as plugin
 * writer: generate a Placemark id

LIBKML driver:
 * writer: validate longitude, latitude to be in range (#10483)
 * writer: set name of NetworkLink from NAME layer creation option (#10507)
 * writer: dump feature when its geometry cannot be written (#10829)
 * on reading of directory KML datasets, don't consider the root doc.kml as a layer

MapInfo driver:
 * make it optional and buildable as plugin
 * implement read/write support for MapInfo logical type to OGR OFSTBoolean
 * Add UTF-8 encoding
 * Disable table fields "laundering" for non-neutral charset
 * Add 'STRICT_FIELDS_NAME_LAUNDERING' creation option
 * better deal with EPSG:3301 'Estonian Coordinate System of 1997'

Miramon driver:
 * various memory leak fixes on corrupted datasets
 * fix a case of mutirecord (lists) in some fields (#11148)

OAPIF driver:
 * combine CURL error message and data payload (when it exists) to form error
   message (#10653)
 * make it buildable as plugin (independently of WFS driver)
 * add a DATETIME open option (#10909)

OCI driver:
 * OCI: use TIMESTAMP(3) and tweak NLS_TIME[STAMP][_TZ]_FORMAT to accept
   milliseconds (#11057)

ODBC driver:
 * add GDAL_DMD_LONGNAME

OpenFileGDB driver:
 * add partial read-only support for tables with 64-bit ObjectIDs
 * more informative warning message when opening a dataset with a .cdf file and
   FileGDB driver isn't there
 * error out explicitly when attempting to create an index on multiple columns

OSM driver:
 * add a \[general\] section at top of osmconf.ini to make it INI compliant (and
   Python's configparser friendly)
 * actually reserve memory for /vsimem/ temp files

Parquet driver:
 * dataset (multi-file typically) mode: enable use of bounding box columns
   for spatial filter; optimize spatial filtering
 * dataset mode: implement SetIgnoredFields() and SetAttributeFilter()
 * dataset mode: detect bbox geometry column when opening current Overture Maps
 * dataset mode: make sure all files are closed before closing the GDALDataset

PDF driver:
 * reader: fixes to handle recursive resources, /OC property attached to a
   XObjet and an empty UTF-16 layer name (#11034)

PostgreSQL driver:
 * OGR_PG_SKIP_CONFLICTS: optionally insert with ON CONFLICT DO NOTHING (#10156)
 * avoid error when the original search_path is empty

Shapefile driver:
 * make it optional (but must be explicitly disabled) and buildable as plugin
 * Shapelib: resync'ed with upstream

SQLite driver:
 * run deferred table creation before StartTransaction
 * avoid some potential O(n^2) issues with n=field_count

TileDB driver:
 * use GEOM_WKB type when creating geometry columns with TileDB >= 2.21

VRT driver:
 * OGRWarpedVRT: use faster SetFrom() implementation (#10765)
 * UnionLayer: avoid some potential O(n^2) issues with n=field_count

WFS driver:
 * make it buildable as plugin

## SWIG bindings

* Python/Java: replace sprintf() with snprintf() to avoid warnings on OSX
* fix memleak in gdal.GetConfigOptions()

### Python bindings

* generate launcher shell/bat scripts for Python scripts in /swig/python/bin
* make GetStatistics() and ComputeStatistics() return None in case of error (#10462)
* Make ogr.DataSource a synonym of gdal.Dataset
* Remove ogr.Driver
* do not emit warnings about not having used UseExceptions() if run under
  gdal.ExceptionMgr()
* avoid gdal.ExceptionMgr() to re-throw a GDAL exception already caught under it
* avoid exception emitted and caught under gdal.ExceptionMgr() to cause later issues
* Python scripts: use local exception manager, instead of global UseExceptions()
* check validity of GDALAccess flag passed to gdal.Open()
* make MDArray.Write(array_of_strings) work with a 0-d string variable
* Avoid linear scan in gdal_array.NumericTypeCodeToGDALTypeCode (#10694)
* Dataset.Close(): invalidate children before closing dataset
* __init__.py: remove calls to warnings.simplefilter() (#11140)
* fix compatibility issue with SWIG 4.3.0 and PYTHONWARNINGS=error

### Java bindings

* Make sure a valid UTF-8 string is passed to NewStringUTF()
* OGR module: add various xxxxAsByteArray() method that return a byte[] when
  content is not UTF-8 (#10521, #10630)

# GDAL/OGR 3.9.3 Release Notes

GDAL 3.9.3 is a bugfix release.

## Build

* Java bindings: remove unneeded dependency on Java AWT
* Use the right header for std::endian cpl_conv.cpp (C++20 compilation)
* Fix build failure with upstream netcdf caused by _FillValue macro renaming

## GDAL 3.9.3

### Port

* /vsitar/: fix support of /vsitar/ of /vsitar/ (#10821)
* CPLGetValueType(): do not recognize '01' as integer, but as string
  (Toblerity/Fiona#1454)

### Algorithms

* Geoloc array: fix bad usage of path API that resulted in temporary files
  not being created where expected (#10671)
* GDALCreateGeoLocTransformer(): fix inverted logic to decide for a debug
  message
* GDALCreateGeoLocTransformer(): increase threshold to use GTiff geoloc
  working datasets to 24 megapixels (#10809)
* GDALGeoLocDatasetAccessors: use smaller, but more, cached tiles (#10809)
* Warper: fix too lax heuristics about antimeridian warping for Avg/Sum/Q1/
  Q3/Mode algorithms (#10892)

### Core

* Fix GDALDataTypeUnion() to check that the provided value fits into eDT
* GDALRegenerateOverviewsMultiBand(): make sure than when computing large
  reduction factors (like > 1024) on huge rasters does not lead to excessive
  memory requirements
* Overview: fix nearest resampling to be exact with all data types (#10758).
  Also make sure that for other resampling methods, the working data type
  is large enough (e.g using Float64 for Int32/UInt32/Int64/UInt64).

### Raster utilities

* gdal_translate/GDALOverviewDataset: fix half-pixel shift issue when
  rescaling RPC (#10600)
* gdaldem color-relief: fix issues with entry at 0 and -exact_color_entry
  mode, and other issues
* gdalwarp: fix crash/infinite loop when using -tr one a 1x1 blank raster
  (3.8.0 regression)
* gdalwarp: be more robust to numerical instability when selecting overviews
  (#10873)
* gdal_contour: Fix regression when fixed level == raster max (#10854)

### Raster drivers

DIMAP driver:
 * emit verbose error message if not able to open image file (#10928)

GeoRaster driver:
 * Preserve quote in the connection string to the GeoRaster driver so that
   Oracle Database wallet can be supported (#10869)

GRIB:
 * adjust longitude range from \[180, xxx\] to \[-180, xxx\] (#10655)

GTiff driver:
 * do not query TIFFTAG_TRANSFERFUNCTION if m_nBitsPerSample > 24 (#10875)
 * fix to not delete DIMAP XML files when cleaning overviews on a DIMAP2
   GeoTIFF file with external overviews

JPEG driver:
 * Fix inverted handling of GDAL_ERROR_ON_LIBJPEG_WARNING

JP2KAK driver:
 * fix data corruption when creating multi-band tiled with the stripe
   compressor code path (#10598)

KEA driver:
 * fix overview writing

MrSID driver:
 * prevent infinite recursion in IRasterIO() in some cases (#10697)

netCDF driver:
 * honour BAND_NAMES creation option in CreateCopy() (#10646)

NITF driver:
 * properly take into account comma-separated list of values for JPEG2000
   QUALITY when JPEG2000_DRIVER=JP2OpenJPEG (#10927)
 * fix parsing of CSCSDB DES

OpenFileGDB raster:
 * do not generate debug 'tmp.jpg' file when reading JPEG tiles

PDF driver:
 * avoid 'Non closed ring detected' warning when reading neatlines from OGC
   Best Practice encoding

TileDB driver:
 * make Identify() method return false if passed object is not a directory

VRT driver:
 * VRTSourcedRasterBand::IRasterIO(): initialize output buffer to nodata
   value if VRT band type is Int64 or UInt64
 * VRTComplexSource::RasterIO(): use double working data type for more
   source or VRT data types
 * VRTComplexSource::RasterIO(): speed-up pixel copy in more cases (#10809)
 * VRTProcessedDataset: fix issue when computing RasterIO window on
   auxiliary datasets on right-most/bottom-most tiles

## OGR 3.9.3

### Core

* Make OGRSFDriver::TestCapability(ODrCCreateDataSource) work with
  defered-loaded drivers (#10783)
* MEM layer: fix UpdateFeature() that didn't mark the layer as updated,
  which caused GeoJSON files to not be updated (qgis/QGIS#57736)

### Vector drivers

FlatGeobuf driver:
 * Fix reading of conformant single-part MultiLineString (#10774)

GeoPackage driver:
 * OGR_GPKG_FillArrowArray_Step(): more rigorous locking

GMLAS driver:
 * make it robust to XML billion laugh attack

JSONFG driver:
 * accept coordRefSys starting with https://www.opengis.net/def/crs/

NAS driver:
 * make it robust to XML billion laugh attack

OpenFileGDB driver:
 * add missing GetIndexCount() in FileGDBTable::CreateIndex
 * fix writing a Int32 field with value -21121
 * exclude straight line segments when parsing arcs (#10763)

Parquet driver:
 * fix crash when using SetIgnoredFields() + SetSpatialFilter() on
   GEOMETRY_ENCODING=GEOARROW layers with a covering bounding box
   (qgis/QGIS#58086)

PostgreSQL/PGDump drivers:
 * properly truncates identifiers exactly of 64 characters (#10907)

PostgreSQL driver:
 * ensure current user has superuser privilege beore attemption to create
   event trigger for metadata table (#10925)

Shapefile driver:
 * Add new shapelib API functions to the symbol rename header

SQLite/GPKG drivers:
 * fix potential double-free issue when concurrently closing datasets when
   Spatialite is available

## Python bindings

* Silence SWIG 'detected a memory leak' message (#4907)
* fix passing a dict value to the transformerOptions argument of gdal.Warp()
  (#10919)

# GDAL/OGR 3.9.2 Release Notes

GDAL 3.9.2 is a bugfix release.

## Build

* Fix compilation against openssl-libs-3.2.2-3 of fedora:rawhide
* Fix compilation against libarchive-3.3.3-5 as shipped by RHEL 8 (#10428)
* Fix -Wnull-dereference warnings of gcc 14.2

## GDAL 3.9.2

### Port

* CPLFormFilename()/CPLGetDirname()/CPLGetPath(): make it work with
  'vsicurl/http://example.com?foo' type of filename, to fix Zarr driver

### Core

* GDALCopyWords(): Fix double->uint64 when input value > UINT64_MAX that was
  wrongly converted to 0

### Raster utilities

* gdalinfo_output.schema.json: pin stac-extensions/eo to v1.1.0
* gdal_translate: fix -a_nodata to accept '-inf' as input (3.9.0 regression)
* gdalwarp: fix -srcnodata/-dstnodata to accept negative value in first position
  (3.9.0 regression)
* gdalbuildvrt: -fix -srcnodata/-vrtnodata to accept negative value in first
  position (3.9.0 regression)
* gdallocationinfo: avoid extra newline character in -valonly mode if coordinate
  is outside raster extent (3.9.0 regression)
* gdal_rasterize: on a int64 band, set nodata value as int64 (#10306)
* gdal_rasterize: restrict to defaulting to Int64 raster data type only if
  output driver supports it (and the burned field is Int64)
* gdal_retile: error out with clear message when trying to retile a file with a
  geotransform with rotation terms, or several input files with inconsistent SRS
  (#10333)
* gdallocationinfo: in -E echo mode, always report input coordinates, not pixel,line
* gdal2tiles: update links in generate_leaflet(), remove OSM Toner (#10304)
* gdal2tiles: uUse correct OpenStreetMap tile url (openstreetmap/operations#737)
* gdal2tiles: fix exception with --nodata-values-pct-threshold but not
  --excluded-values on a multi-band raster

### Raster drivers

COG driver:
 * properly deal with mask bands w.r.t resampling when generating JPEG output,
   or when input has a mask band (3.5 regression) (#10536)

GTI driver:
 * start looking for OVERVIEW_<idx>_xxxx metadata items at index 0
 * automatically add overviews of overviews, unless the OVERVIEW_LEVEL=NONE open
   option is specified

GTiff driver:
 * make SetNoDataValue(double) work on a Int64/UInt64 band (#10306)

KEA driver:
 * don't derive from PAM classes (#10355)

JPEG driver:
 * ReadFLIRMetadata(): avoid potential infinite loop

netCDF driver:
 * multidim: fix use-after-free on string variables in ReadOneElement()

NITF driver:
 * 12-bit JPEG writer: fix crash if raster width > block width (#10441)

OGCAPI driver:
 * do not emit 'Server does not support specified IMAGE_FORMAT: AUTO' when
   opening a vector layer with MVT tiles only
 * fix reading encodingInfo/dataType for Coverage API

SRTMHGT driver:
 * add support for 0.5 deg resolution datasets (#10514)

VRT driver:
 * fix reading from virtual overviews when SrcRect / DstRect elements are missing

WMTS driver:
 * make sure not to request tiles outside of tile matrix / tile matrix limits,
   if the dataset extent goes beyond them
 * clip layer extent with union of extent of tile matrices (#10348)

## OGR 3.9.2

### Core

* WKT geometry importer: accept (non conformant) PointZ/PointZM without space as
  generated by current QGIS versions
* OGR SQL: do not make backslash a special character inside single-quoted
  strings, to improve compliance with SQL92 (#10416)
* OGRSQL: return in error when 'Did not find end-of-string character' is emitted
  (#10515)

### OGRSpatialReference

* EPSGTreatsAsNorthingEasting(): make it work correctly with compound CRS

### Vector drivers

GeoJSON driver:
 * writer: make sure geometry is reprojected even when it is invalid after
   coordinate rounding (3.9.0 regression) (qgis/QGIS#58169)

GML driver:
 * writer: fix missing SRS in Featurecollection's boundedBy element (3.9.1
   regression) (#10332)

GMLAS driver:
 * avoid crash on a OSSFuzz generated xsd (ossfuzz#70511)

GTFS:
 * fix error when applying an attribute filter on a field whose advertized data
   type is not string (duckdb/duckdb_spatial#343)

JSONFG driver:
 * fix reading non-FeatureCollection documents larger than 6,000 bytes (#10525)

LIBKML driver:
 * fix writing a .kmz to cloud storage (#10313)
 * fix LIBKML_RESOLVE_STYLE=YES when reading a KML file with dataset-level
   styles (3.9.1 regression) (qgis/qgis#58208)
 * fix reading/writing style URL in KMZ files (#10478)

MiraMonVector driver:
 * fix oss-fuzz#70860

OAPIF driver:
 * fix resolving of relative links (#10410)

OpenFileGDB driver:
 * fix attribute filter when an equality comparison is involved with a field
   with an index created with a LOWER(field_name) expression (#10345)

OSM driver:
 * avoid creating /vsimem/osm_importer directory (#10566)

PG driver:
 * fix ogr2ogr scenarios to PostgreSQL when there are several input layer names
   like schema_name.layer_name (3.9.0 regression)
 * fix support for geometry/geography OID in range between 2^31 and 2^32 (#10486)

PG/PGDump drivers:
 * make sure spatial index name is unique, even if several tables have a long
   enough prefix (#10522, #7629)

Shapefile driver:
 * fix recognizing an empty string as a NULL date (#10405)
 * fix off-by-one write heap buffer overflow when deleting exactly 128, 232,
   etc. features and repacking (#10451)

XLSX driver:
 * support documents whose XML elements have a prefix (duckdb
/duckdb_spatial#362)

## Python bindings

* fix typos in gdal.Footprint() help message
* make MDArray.Write(array_of_strings) work with a 0-d string variable

# GDAL/OGR 3.9.1 Release Notes

GDAL 3.9.1 is a bugfix release.

## Build

* PDF: fix build with PoDoFo >= 0.10 with C++20 compilation (#9875)
* PDF: fix build against PoDoFo with MSYS2 UCRT64 and CLANG64 (#9976)
* Fix compiler errors in C++20 and C++23 modes
* Fix compiler warnings with gcc 14
* CMake: add infrastructure so we can build plugins that can be loaded against
  a libgdal that has been without any support for them.
  Note: this is an ABI break for in-tree drivers, with deferred loading capability, when built as plugin. That is such a driver built against
  GDAL 3.9.0 won't be able to be loaded by GDAL 3.9.1.

## GDAL 3.9.1

### Port

* /vsis3/: include AWS_CONTAINER_CREDENTIALS_TOKEN in container credentials
 flow (#9881)
* VSIStatL(): allow trailing slash on mingw64 builds

### Algorithms

* Warper: relax longitude extend check to decide whether we can insert a
  CENTER_LONG wrapping longitude
* GCPTransformer: accept only 2 points for order=1 transformer (#10074)
* GDALContourGenerateEx(): validate LEVEL_INTERVAL option (#10103)
* GDALTranslate(): use directly generated VRT even if BLOCKXSIZE/BLOCKYSIZE
  creation options are specified
* GDALSieveFilter(): avoid assert() when all pixels are masked (3.9.0 regression)
  (raster/rasterio#3101)
* Overview generation: fix multi-threaded bug, resulting in locks/crashes with
  GeoPackage in particular (#10245)

### Core

* GDALIdentifyDriverEx(): transmit papszAllowedDrivers to Identify() (#9946)
* GDALNoDataMaskBand::IRasterIO(): fix crash on memory allocation failure
  (rasterio/rasterio#3028)
* Rasterband methods (histogram, statistics): make them compatible of a dataset
  with more 2 billion blocks

### Raster utilities

* Add argument name after 'Too few arguments' error (#10011)
* gdalwarp: fix help message for -wm (#9960)
* gdalinfo text output: avoid weird truncation of coordinate epoch when
  its value is {year}.0
* Python utilities: avoid UseExceptions() related deprecation warning when
  launched from launcher shell scripts (#10010)

### Raster drivers

AAIGRID driver:
 * fix forcing datatype to Float32 when NODATA_value is nan and pixel values
   look as ints

ESRIC driver:
 * ingest more bytes to be able to identify more datasets (#10006)
 * properly takes into account minLOD for .tpkx (#10229)
 * validate tile size
 * add a EXTENT_SOURCE open option and default to FULL_EXTENT (#10229)

GRIB driver:
 * make .idx reading compatible of /vsisubfile/ (#10214)

GTiff driver:
 * fix reading angular projection parameters in non-degree unit
   (typically grads), when reading projection from ProjXXXXGeoKeys (#10154)
 * fix writing angular projection parameters in non-degree unit (typically
   grads), when writing ProjXXXXGeoKeys
 * multithreaded reading: avoid emitting tons of warnings when ExtraSamples tag
   is missing
 * writer: limit number of GCPs in GeoTIFF tag to 10922 to avoid overflowing
   uint16_t (#10164)

HDF5 driver:
 * add support for libhdf5 >= 1.14.4.2 when built with Float16
 * make logic to assign per-band attribute more generic

netCDF driver:
 * netCDFAttribute::IWrite(): fix writing an array of (NC4) strings, from a
   non-string array
 * try to better round geotransform values when read from single-precsion
   lon/lat float arrays

STACIT driver:
 * add a OVERLAP_STRATEGY opening option to determine how to handle sources
   fully overlapping others, and taking into account nodata values
 * honour MAX_ITEMS=0 as meaning unlimited as documented
 * implement paging using POST method

VRT driver:
 * fix processing of LUT where the first source value is NaN
 * fix serialization of separatable kernel in VRTKernelFilteredSource (#10253)

Zarr driver:
 * SerializeNumericNoData(): use CPLJSonObject::Add(uint64_t) to avoid potential
   undefined behavior casts

## OGR 3.9.1

### Core

* OGRSQL: validate column name in COUNT(field_name) and error out if it
  doesn't exist (#9972)
* OGR SQL: fix crash when the ON expression of a JOIN contains OGR special
  fields (in particular feature id)
* OGR layer algebra: honour PROMOTE_TO_MULTI=YES for Points
* OGRFeature::SetField(int, double): avoid UndefinedBehavior when passing NaN
* OGRFeature::SetField(int, GIntBig): avoid UndefinedBehavior when passing value
  close to INT64_MAX
* OGRLayer::WriteArrowBatch(): avoid UndefinedBehavior when trying to convert
  NaN to Int64

### OGRSpatialReference

* importFromESRI() Arc/Info 7 .prj: fix importing LAMBERT_AZIMUTHAL with a
  'radius of the sphere of reference'
* workaround bug of PROJ < 9.5 regarding wrong conversion id for UTM south
* importFromUSGS(): use plain WGS84 datum with WGS84 ellipsoid, and identify
  EPSG code

### Vector utilities

* ogrinfo: fix error message when not specifying a filename
* ogrinfo text output: avoid weird truncation of coordinate epoch when
  its value is {year}.0
* ogr2ogr: do not call CreateLayer() with a geom field defn of type wkbNone
  (#10071)
* ogr2ogr: error out if GCP transform creation fails (#10073)

### Vector drivers

AVCBin driver:
 * avoid integer overflow (ossfuzz #68814)

DXF driver:
 * avoid slight numeric imprecision with closed lwpolyline and arcs (#10153)

ESRIJSON driver:
 * do not set width on DateTime field
 * fix paging when URL contains f=pjson instead of f=json (#10094)

FileGDB driver:
 * be robust to be called with a geometry field definition of type wkbNone
   (#10071)

FlatGeoBuf driver:
 * more explicit error message in case of feature geometry type != layer
   geometry type (#9893)

GeoJSON driver:
 * do not recognize GeoJSON files with a featureType feature property as JSONFG
   (#9946)
 * make it possible with -if/papszAllowedDrivers to force opening a JSONFG file
   with the GeoJSON driver
 * implement IUpdateFeature() that was broken up to now
 * declare missing capabilities DELETE_FIELD, REORDER_FIELDS,
   ALTER_FIELD_DEFN_FLAGS

GML driver:
 * fix memory leak due do xlink:href substitution in a non-nominal case
   (ossfuzz #68850)
 * GML_SKIP_RESOLVE_ELEMS=HUGE: keep gml:id in .resolved.gml file
 * avoid assertion due to trying to load existing .gfs file after reading
   .resolved.gml (#10015)
 * fix issue with nested elements with identical names in
   GML_SKIP_RESOLVE_ELEMS=HUGE mode (#10015)
 * make sure SRS is detected when using GML_SKIP_RESOLVE_ELEMS=HUGE on a AIXM
   file, otherwise GML_SWAP_COORDINATES might not work correctly

JSONFG driver:
 * dataset identification: recognize more JSON-FG specification version

LIBKML driver:
 * fix handling of styleUrl element referencing to an external document (#9975)
 * fix performance issue when writing large files (#10138)

MiraMonVector driver:
 * adding which polygon cycles the linestring in the linestring metadata information (#9885)
 * Adding more matches in the look-up table MM_m_idofic.csv
 * Fixing error in linestring (from not polygon) metadata file
 * Robustness fixes (ossfuzz #68809)
 * fix a case of sensitive comparison
 * fix MMResetFeatureRecord
 * fix accepted metadata versions/subversions

MapInfo:
 * .tab: AlterFieldDefn(): fix data corruption when altering (for example
   renaming) a Integer/Float32 field of size 4 (or Integer64/Float64 field of
   size 8
 * avoid potential double-free (ossfuzz#69332, ossfuzz#69334)

netCDF driver:
 * writer: use CF-1.8 with FORMAT=NC4 and GEOMETRY_ENCODING=WKT

ODS driver:
 * implement IUpdateFeature() that was broken up to now
 * declare missing capabilities DELETE_FIELD, REORDER_FIELDS,
   ALTER_FIELD_DEFN_FLAGS
 * fix CreateFeature() implementation when a FID is set

OpenFileGDB driver:
 * writer: .gdbtable header must be rewritten when updating an existing feature
   at the end of file (otherwise resulting file might not be readable by
   Esri software)
 * detect and try to repair corruption of .gdbtable header related to above item
 * BuildSRS(): do not use CPLErrorReset()

Parquet driver:
 * GeoParquet: always write version=1.1.0

PDF driver:
 * fix wrong order of matrix multiplication for 'cm' operator... (#9870)
 * make ExploreContentsNonStructured() be able to parse OGCs as generated
   by ArcGIS 12.9 (fixes #9870)
 * just ignore unknown objects referenced by /Do (#9870)

PMTiles driver:
 * writer: fix crash in ogr2ogr (or Arrow based workflows) from GeoPackage/
   GeoParquet to PMTiles (#10199)

PG driver:
 * avoid errors related to ogr_system_tables.metadata when user has not enough
   permissions (#9994)
 * really honor OGR_PG_ENABLE_METADATA=NO in SerializeMetadata()

SQLite/GPKG drivers
 * detect UNIQUE constraints expressed as a ', CONSTRAINT name UNIQUE (column_name)'
   at the end of CREATE TABLE (qgis/QGIS#57823)

XLSX driver:
 * implement IUpdateFeature() that was broken up to now
 * declare missing capabilities DELETE_FIELD, REORDER_FIELDS,
   ALTER_FIELD_DEFN_FLAGS
 * fix CreateFeature() implementation when a FID is set

## Python bindings

* do not emit warnings about not having used [Dont]UseExceptions() if run under
  gdal.ExceptionMgr()
* avoid gdal.ExceptionMgr() to re-throw a GDAL exception already caught under it
* avoid exception emitted and caught under gdal.ExceptionMgr() to cause later
  issues
* Avoid crash when using orphaned subgeometry (#9920)
* make them compatible of SWIG 4.3.0dev


# GDAL/OGR 3.9.0 Releases Notes

GDAL/OGR 3.9.0 is a feature release.
Those notes include changes since GDAL 3.8.0, but not already included in a
GDAL 3.8.x bugfix release.

## In a nutshell...

* [RFC 96](https://gdal.org/development/rfc/rfc96_deferred_plugin_loading.html):
  Deferred C++ plugin loading
* [RFC 97](https://gdal.org/development/rfc/rfc97_feature_and_fielddefn_sealing.html):
  OGRFeatureDefn, OGRFieldDefn and OGRGeomFieldDefn "sealing"
* [RFC 98](https://gdal.org/development/rfc/rfc98_build_requirements_gdal_3_9.html):
  Build requirements for GDAL 3.9
* [RFC 99](https://gdal.org/development/rfc/rfc99_geometry_coordinate_precision.html):
  Geometry coordinate precision
* Add [S104](https://gdal.org/drivers/raster/s104.html) (Water Level Information
  for Surface Navigation Product) and
  [S111](https://gdal.org/drivers/raster/s111.html) (Surface Currents Product)
  raster read-only drivers (required libhdf5)
* Add raster [GTI](https://gdal.org/drivers/raster/gti.html) (GDAL Raster Tile
  Index) driver to support catalogs with huge number of sources.
* Add vector [MiraMonVector](https://gdal.org/drivers/vector/miramon.html)
  read/creation driver (#9688)
* Deprecated ARG driver has been removed (#7920)
* Code linting

## Build

* CMake: add ``[GDAL|OGR]_REGISTER_DRIVER_<driver_name>_FOR_LATER_PLUGIN``
  variables (RFC 96)
* CMake: Bump max compatible version to 3.28
* CMake: add a way of defining an external deferred driver by setting one or
  several ADD_EXTERNAL_DEFERRED_PLUGIN_XXX CMake variables (RFC 96)
* CMake: error out if a driver has been asked as a plugin, but conditions are not met
* CMake: rework PROJ detection
* CMAKE_UNITY_BUILD=YES builds are possible, but not recommended for production
* gdal.cmake: set -DDEBUG for CMAKE_BUILD_TYPE=Debug for Windows builds as well
* CMake: add GDAL_FIND_PACKAGE_OpenJPEG_MODE and GDAL_FIND_PACKAGE_PROJ_MODE
  variables
* FindSQLite3.cmake: avoid repeating finding `SQLite3_INCLUDE_DIR`/
 `SQLite3_LIBRARY` if existed.
* Add compatibility for Intel Compiler 2024.0.2.29

## Backward compatibility issues

See [MIGRATION_GUIDE.TXT](https://github.com/OSGeo/gdal/blob/release/3.8/MIGRATION_GUIDE.TXT)

## Changes in installed files

* data/MM_m_idofic.csv: new
* data/gdalvrtti.xsd: new
* data/pci_datum.txt and data/pci_ellips.txt: updated (#8034)
* include/cpl_minizip_ioapi.h, cpl_minizip_unzip.h, cpl_minizip_zip.h:
  removed. They use since 3.5 an header that is not installed, so were unusable

## GDAL 3.9.0 - Overview of Changes

### Port

* /vsicurl: add ANYSAFE & BEARER to auth methods (#8683)
* /vsicurl/: re-emit HTTP error code next times we try opening a resource that
  failed the first time (#8922)
* /vsicurl/: add a VSICURL_PC_URL_SIGNING path-specific option to enable
  Planetary Computer URL signing only on some URLs
* /vsicurl/: Read(): emit error message when receiving HTTP 416 Range Not
  Satisfiable error
* Add VSIVirtualHandle::GetAdviseReadTotalBytesLimit()
* cpl_http: retry "Connection reset by peer"
* Add a VSICURLMultiCleanup() that runs with SIGPIPE ignored (#9677)
* /vsiaz/: fix RmdirRecursive() on an empty directory with just the
  .gdal_marker_for_dir special marker file
* /vsis3/: include region to build s3.{region}.amazonaws.com host name (#9449)
* /vsigs/: fix RmdirRecursive() on an empty directory
* /vsigs/ UnlinkBatch(): make sure that path config options are checked for the
  deleted files and not '/vsigs/batch/
* /vsiaz/ UnlinkBatch(): make sure that path config options are checked for the deleted files
* Add VSIVirtualHandle::Printf()
* Add VSIRemovePluginHandler() to enable removal of virtual filesystems (#8772)
* No longer alias CPLMutex, CPLCond and CPLJoinableThread to void in non-DEBUG
  builds (ABI change)
* Win32 extended filenames (starting with "\\?\"): various fixes; add support
  for network UNC paths
* Add VSIGetDirectorySeparator() to return the directory separator for the
  specified path
* Add CPLXMLNodeGetRAMUsageEstimate()
* CPLCreateOrAcquireMutexEx(): fix warning about lock-order inversion (#1108)
* Win32 Stat(VSI_STAT_EXISTS_FLAG): improve performance (#3139)
* Add CPLDebugProgress() to display a debugging message indicating a progression
* CSLConstList/CPLStringList: add iterating facilities and better
  ``std::vector<std::string>`` operability
* Add CPLStringList::front(), back() and clear()
* Add CPLStrtodM()
* CPLStrtod(): set errno=0 when no value has been parsed to conform to POSIX
* Add CPLUTF8ForceToASCII()
* VSICACHE: avoid EOF read (#9669)

### Core

* GDALRATValuesIOAsString(): fix wrong type for papszStrList argument
* Add GDALDataset::DropCache() (#8938)
* Add GDALDataset::UnMarkSuppressOnClose() and IsMarkSuppressOnClose() (#8980)
* Add GDALGetOutputDriversForDatasetName()
* Modify the logic of selection of overviews for non-nearest resampling; add a
  GDAL_OVERVIEW_OVERSAMPLING_THRESHOLD config option (#9040)
* GDALMDArray::AsClassicDataset(): make it possible to use overviews
* GDALOpen(): change error message when a dataset isn't recognized (#9009)
* GDALDeserializeGCPListFromXML(): validate value of GCP Pixel,Line,X,Y,Z
  attributes
* PAM: only unset GPF_DIRTY flag
* GDALGetCacheMax64(): fix warning about lock-order inversion (#1837)
* Add gdal::GCP class
* QuietDeleteForCreateCopy(): forward source dataset open options (#9422)

### Multidimensional API

* Add GDALCreateRasterAttributeTableFromMDArrays() to return a virtual Raster
  Attribute Table from several GDALMDArray's
* fix wrong DataAxisToSRSAxisMapping
* GDALMDArray::AsClassicDataset(): make it possible to use overviews

### Algorithms

* Implement basic Line-of-sight algorithm (#9506, #9050)
* Warper: fix artifacts when reprojecting from long/lat to ortho (#9056)
* GDALSuggestedWarpOutput2(): ortho->long/lat: limit extent to
  [lon_0-90,lon_0+90] even when poles are included
* Warper: limit artifacts when doing ortho->long/lat (provided that srcNodata is
  set)
* GDALSuggestWarpOutput(): make it return original potential non-square pixel
  shape for a south-up oriented dataset (#9336)
* Warper: add a EXCLUDED_VALUES warping option to specify pixel values to be
  ignored as contributing source pixels during average resampling. And
  similarly, add a NODATA_VALUES_PCT_THRESHOLD warping option for nodata/
  transparent pixels.
* GDALFillNodata(): add a INTERPOLATION=NEAREST option
* GDALChecksumImage(): read by multiple of blocks for floating-point bands to
  improve performance

### Utilities

* Add GDALArgumentParser class to extend p-ranav/argparse framework
* Use GDALArgumentParser for: gdaladdo, gdalinfo, gdal_translate, gdalwarp,
  gdal_grid, gdal_viewshed, gdalbuildvrt, nearblack, ogrinfo, ogr2ogr, sozip
* Add support for ``--config <key>=<value>`` syntax
* gdaladdo: reuse previous resampling method (from GTiff RESAMPLING metadata
  item) if not specifying -r and overview levels if not specifying them
* gdaladdo: make --partial-refresh-from-source-timestamp work on GTI datasets
* gdal_contour: fix lowest min value in polygonize mode (#9710)
* gdalbuildvrt: add a -nodata_max_mask_threshold option
* gdal_create: copy GCPs present in the input file
* gdal_edit: add -a_coord_epoch option
* gdal_footprint: fix -split_polys and -convex_hull to not consume the argument
  specified just after them
* gdal_footprint: write source dataset path in a 'location' field (#8795)
* gdal_grid: error out on in invalid layer name, SQL statement or failed where
  condition (#9406)
* gdallocationinfo: make it output extra content at end of input line, and
  add -E echo mode, and -field_sep option (#9411)
* gdalmdimtranslate: add -arrayoptions
* gdalinfo: do not call GDALGetFileList() if -nofl is specified (#9243)
* gdal_translate: add -dmo option, for domain-metadata items (#8935)
* gdal_translate -expand rgb[a]: automatically select Byte output data type if
  not specified and color table compatible of it (#9402)
* gdaltransform: make it output extra content at end of input line, and add
  -E echo mode, and -field_sep option (#9411)
* gdalmanage: make --utility_version work
* gdal_polygonize.py: handle error if creation of destination fails
* gdal_viewshed: add support for south-up source datasets (#9432)
* gdalwarp: progress bar tunings
* gdalwarp: emit error message when transformation isn't inversible (#9149)
* gdalwarp: fix performance issue when warping to COG (#9416)
* gdalwarp: make target resolution independent from source extent when -te is
  specified (#9573)
* allow passing a WKT geometry as -cutline value, and add -cutline_srs (#7658)
* sozip: support generic command line options
* --formats option: append list of extensions (#8958)
* Make gdaltindex a C callable function: GDALTileIndex()
* gdaltindex: add -overwrite, -vrtti_filename, -tr, -te, -ot, -bandcount,
  -nodata, -colorinterp, -mask, -mo, -recursive, -filename_filter,
  -min_pixel_size, -max_pixel_size, -fetch_md, -lco options
* gdal2tiles: added support for JPEG output
* gdal2tiles: Fix case where --exclude still writes fully transparent tiles
  (#9532)
* gdal2tiles: add --excluded-values, --excluded-values-pct-threshold and
  --nodata-values-pct-threshold options
* gdal2tiles: support an input dataset name not being a file (#9808)
* gdal2xyz: Change -srcwin parameter type to integer.
* Python sample scripts: add gdalbuildvrtofvrt.py (#9451)
* Python utilities: do not display full traceback on OpenDS failures (#9534)
* gdalinfo: suggest trying ogrinfo if appropriate, and vice-versa
* gdalinfo and ogrinfo -json: add newline character at end of JSON output

### Raster drivers

Updates affecting multiple drivers:
 * All drivers depending on external libraries that can be built as plugins
   have been updated to implement RFC 96 deferred plugin loading
 * Drivers that can now be built as plugins if using external libraries and
   not vendored/internal libraries: GIF, JPEG, MRF, NITF, RAW drivers (as a
   single plugin), PDS related drivers (as a single plugin)
 * do not export GMLJP2 if the SRS isn't compatible (#9223): JP2KAK, JP2ECW,
   JP2Lura, JP2OPENJPEG

AAIGRID driver:
 * fix reading file whose first value is nan (#9666)

BAG driver:
 * make sure to report a subdataset for the bathymetry coverage when we expose
   one(s) for the georeferenced metadata

GeoPackage driver:
 * support maximum level up to 29 or 30; add a ZOOM_LEVEL creation option

GTiff driver:
 * Internal libtiff: resync with upstream
 * Use (future) libtiff 4.6.1 TIFFOpenOptionsSetMaxSingleMemAlloc()
 * add JXL_LOSSLESS_OVERVIEW, JXL_EFFORT_OVERVIEW, JXL_DISTANCE_OVERVIEW and
   JXL_ALPHA_DISTANCE_OVERVIEW configuration options (#8973)
 * overviews: generalize saving resampling method in a RESAMPLING metadata item
 * friendlier error message when attempting to create JXL compressed file with
   unsupported type/bits_per_sample
 * deal with issues with multi-band PlanarConfig=Contig, LERC and NaN values
   (#9530)
 * make BuildOverviews() in update mode work when there is an external .msk file
 * no longer condition CreateInternalMaskOverviews() behavior to
   GDAL_TIFF_INTERNAL_MASK being unset or set to YES
 * change default value of GDAL_TIFF_INTERNAL_MASK config option to YES
 * multi-threaded decoding: fix potential mutex deadlock
 * MultiThreadedRead(): make it take into account AdviseRead() limit to reduce
   the number of I/O requests (#9682)

HDF5 driver:
 * multidim: fix crash on reading compound data type with fixed-length strings
 * multidim: implement GDALMDArray::GetBlockSize() and GetStructuralInfo()
 * improve performance of band IRasterIO() on hyperspectral products
 * support HDF5EOS metadata where the GROUP=GridStructure has an empty sub
   GROUP=Dimension (like for AMSR-E/AMSR2 Unified L3 Daily 12.5 km)
 * implement GetOffset() and GetScale() for netCDF metadata items add_offset
   and scale_factor

JP2KAK driver:
 * make result of RasterIO() consistent depending if it is called directly or
   through VRT, with non-nearest upsampling (#8911)
 * refactor how overviews work

JP2OpenJPEG driver:
 * CreateCopy(): limit number of resolutions taking into account minimum block
   width/height

JPEG driver:
 * ReadXMPMetadata(): only stop looking for XMP marker at Start Of Scan
 * ReadFLIRMetadata(): stop on Start-Of-Scan marker (speeds up opening large
   remote JPEG files)
 * CreateCopy(): emit warning/error message before progress bar (#9441)

MRF driver:
 * BuildOverviews: bail out when no overviews are needed
 * emit warning when attempting to clean internal overviews (#9145)
 * Use ZSTD streaming API for compression: faster and better compression (#9230)

netCDF driver:
 * use the SRS (its geographic part) if found in the file, instead of the
   hardcoded WGS84 string, for the GEOLOCATION.SRS metadata item (#9526)
 * Add BAND_NAMES creation option
 * fix writing of metadata items whose value has an equal sign ["9702)

NITF driver:
 * in xml:DES metadata domain field, expand content of XML_DATA_CONTENT DES as
   XML in a <xml_content> sub-node of >field name=DESDATA> instead as a Base64
   value
 * fix undefined behavior when using NITFGetField() several times in the same
   statement

OGCAPI driver:
 * OGCAPI Maps: support image formats beyond PNG/JPG as well as enables content
   negotiation. (#9231, #9420)

PDF driver:
 * correctly initialize PAM when opening a subdataset (specific page e.g.)
 * PDFium backend: update to support (and require) PDFium/6309
 * deal with the situation where a multipage PDF has layers with same name but
   specific to some page(s)
 * Fix build with Poppler 24.05

S102 driver:
 * add support for spatial metadata of the QualityOfSurvey group
 * read nodata value from dataset metadata

Sentinel2 driver:
 * include 10m AOT and WVP bands in 10m subdataset (#9066)

TileDB driver:
 * Remove use of deprecated API, and bump minimum version to 2.15
 * Added tiledb metadata fields to easily tag array type
 * be able to read datasets converted with 'tiledb-cf netcdf-convert'
 * make its identify() method more restrictive by not identifying /vsi file
   systems it doesn't handle
 * multidim: prevent infinite recursion when opening an array in a group where
   there are 2 or more 2D+ arrays
 * multidim: OpenMDArray(): fix opening an array, member of a group, which has
   no explicit name

VRT driver:
 * add a VRTProcessedDataset new mode to apply chained processing steps that
   apply to several bands at the same time. Currently available algorithms:
   LocalScaleOffset (for dehazing), BandAffineCombination, Trimming, LUT
 * vrt:// connection: add a_nodata, sd_name, sd options
 * add a NoDataFromMaskSource source: replaces the value of the source with
   the value of the NODATA child element when the value of the mask band of
   the source is less or equal to the MaskValueThreshold child element.
 * VRT serialization: emit a warning if RAM usage of XML serialization reaches
   80% of RAM (#9212)
 * VRTWarpedDataset: add an optimized IRasterIO() implementation

WMS driver:
 * change logic to set gdalwmscache directory to honor in particular
   XDG_CACHE_HOME (#8987)
 * Use GDAL_HTTP_TIMEOUT

Zarr driver:
 * Add capability to read CRS from CF1 conventions

ZMap driver:
 * support reading variant of format where there is no newline character at
   end of column

## OGR 3.9.0 - Overview of Changes

### Core

* OGRSQL: Support SELECT * EXCLUDE(...)
* OGRSQL: add UTF-8 support for LIKE/ILIKE (for layers declaring
  OLCStringsAsUTF8) (#8835)
* Add OGRLayer::GetExtent3D() (#8806)
* OGRLayer: Have CreateField/CreateGeomField take const OGRFieldDefn
  /OGRGeomFieldDefn* argument (#8741)
* OGRFeatureDefn, OGRFieldDefn, OGRGeomFieldDefn: add Seal() and Unseal()
  methods
* Fix swq_select::Unparse()
* ExecuteSQL(): add a warning if the dialect name isn't recognized (#8843)
* ExecuteSQL() with OGRSQL and SQLITE dialects: get OLCStringsAsUTF8 from
  underlying layer (#9648)
* ExecuteSQL() OGRSQL and SQLITE dialects: error out if SetSpatialFilter() fails
  (#9623)
* OGRGeometryFactory::forceTo(): make it honour dimensionality of eTargetType
  (in particular fix POLYGON -> LINESTRING or POLYGON -> LINESTRINGZ) (#9080)
* OGRGeometryCollection::importFromWkb(): fix reading corrupted wkb with mixed
  2D and 3D geoms (#9236)
* RFC99: Add C++ OGRGeomCoordinatePrecision class and corresponding C / SWIG API
* Change prototype of GDALDataset::ICreateLayer() to take a
  ``const OGRGeomCoordinatePrecision*``
* Add OGRGeometry::SetPrecision() / OGR_G_SetPrecision(), as wrapper of
  GEOSGeom_setPrecision_r()
* Add a OGRGeometry::roundCoordinates() method
* OGRFeature: add SerializeToBinary() / DeserializeFromBinary()
* OGRFeature::SetField(): add warnings when detecting some lossy conversions (#9792)
* Add OGR_G_GeodesicArea() / OGRSurface::get_GeodesicArea()
* SQLite SQL dialect: implement ST_Area(geom, use_ellipsoid)
* Add OGR_L_GetDataset() and implement GetDataset() in all drivers with creation
  support
* Arrow array: fix decoding of ``date32[days]`` values before Epoch
 (Arrow->OGRFeature), and fix rounding when encoding such values
 (OGRFeature->Arrow) (#9636)
* OGRLayer::WriteArrowBatch(): add tolerance for field type mismatches if int32/int64/real;
  Also add an option IF_FIELD_NOT_PRESERVED=ERROR to error out when lossy conversion occurs. (#9792)
* OGRLayer::SetIgnoredFields(): make it take a CSLConstList argument instead of
  const char*

### OGRSpatialReference

* Add OGRSpatialReference::exportToCF1() and importFromCF1()
* Add OSRIsDerivedProjected() / OGRSpatialReference::IsDerivedProjected()
* OGRCoordinateTransformation::Transform(): change nCount parameter to size_t
  (C++ API only for now) (#9074)
* OGRProjCT::TransformWithErrorCodes(): Improve performance of axis swapping
  (#9073)
* OSR_CT: fix ``SetDataAxisToSRSAxisMapping([-2, 1])``` on target SRS
* exportToXML(): error out on unsupported projection method (#9223)
* Add OSRSetFromUserInputEx() and map it to SWIG (#9358)
* Add std::string OGRSpatialReference::exportToWkt(
  const char* const* papszOptions = nullptr) const
* OGR_CT: use PROJJSON internally rather than in WKT:2019 (#9732)

### Utilities

* ogrinfo: add a -extent3D switch
* ogrinfo: output coordinate resolution in -json mode
* ogrinfo: add -limit option (#3413)
* ogr2ogr: do not copy private layers (would for example break SQLite ->
  SQLite/GPKG)
* ogr2ogr: force -preserve_fid when doing GPX to GPKG (#9225)
* ogr2ogr: propagate input coordinate precision
* ogr2ogr: add options -xyRes, -zRes, -mRes and -unsetCoordPrecision
* ogr2ogr: make -select tokenize only on comma (and not on space), and honour
  double-quoting (#9613)
* Remove ogr2ogr.py sample utilities (gdal.VectorTranslate() is a more powerful
  replacement)

### Vector drivers

Updates affecting multiple drivers:
 * Drivers updated for RFC 97 feature definition sealing: GPKG, Shape,
   OpenFileGDB, MITAB, MEM, GeoJSON, JSONFG, TopoJSON, ESRIJSON, ODS, XLSX, PG
 * Drivers updated for RFC99 geometry coordinate precision: GML, GeoJSON,
   GeoJSONSeq, JSONFG, GPKG, CSV, OpenFileGDB, FileGDB, OGR VRT
 * All drivers depending on external libraries that can be built as plugins
   have been updated to implement RFC 96 deferred plugin loading
 * GetExtent3D() implemented in Shapefile, Arrow, Parquet, PostgreSQL, GeoJSON,
   GeoPackage drivers

OGR SQLite/SQLite/GPKG: add UTF-8 support for case-insensitive LIKE (#8835)

Arrow/Parquet drivers:
 * GetArrowSchema(): potential fix when there are ignored fields and the FID
   column is not the first one
 * Read/write support for GeoArrow (struct-based) encoding
 * silently ignore column of type null in GetArrowSchema/GetArrowArray
 * fix crash when reading geometries from a geometry column with a
   pyarrow-registered extension type
 * handle fields with a pyarrow-registered extension type
 * preliminary/in-advance read support for future JSON Canonical Extension
 * OGRCloneArrowArray(): add missing support for 'tss:' Arrow type

CSV driver:
 * parse header with line breaks (#9172)

DGN (v7) driver:
 * emit explicit error when attempting to open a DGNv8 file and the DGNv8
   driver is not available (#9004)

FileGDB/OpenFileGDB drivers:
 * fix co-operation between the 2 drivers to make sure .cdf is opened with
   FileGDB

FileGDB driver:
 * remove warning 'Empty Spatial Reference'

FlatGeobuf driver:
 * add support for reading and writing layer title, description and metadata
 * CreateFeature(): error out if a string context is not valid UTF-8 (#7458)

GeoJSON driver:
 * OGRGeoJSONDriverIdentify: return -1 when unsure
 * writer: add FOREIGN_MEMBERS_FEATURE and FOREIGN_MEMBERS_COLLECTION layer
   creation options
 * reader: accept a {"type": "Polygon","coordinates": []} as a representation
   of POLYGON EMPTY

GeoPackage driver:
 * fixes to make most operations compatible with PRAGMA foreign_keys=1 (#9135)
 * writer: set Z/M columns of gpkg_geometry_columns when there are Z/M
   geometries in a 2D declared layer
 * Read relationships defined using foreign key constraints
 * CreateFeature(): allow creating a feature with FID=0 (#9225)
 * Add DISCARD_COORD_LSB layer creation option
 * map Null CRS to a new srs_id=99999, and add a SRID layer creation option
 * add a LAUNDER=YES/NO layer creation option (default: NO)
 * fix random error in threaded RTree creation, particularly on Mac M1 ARM64

GeoRaster driver:
 * Added GENSTATS options, security fixes, and prevent failing when password is
   near expiration (#9290)

GML driver:
 * writer: honour geometry field name and isnullable passed to ICreateLayer()

GMLAS driver:
 * faster retrieval of GML and ISO schemas by using zip archives
 * use CPLHTTPFetch() instead of /vsicurl_streaming/ to allow alternate HTTP
   downloader (for QGIS 3.36 enhanced WFS provider)

LIBKML driver:
 * improve generation of internal ids for layer names starting with digits
  (#9538)

netCDF driver:
 * writer: make it more robust to empty geometries

OpenFileGDB driver:
 * add support for Integer64, Date, Time and DateTimeWithOffset data types
   added in ArcGIS Pro 3.2 (#8862)
 * writer: set xml shape type to "esriGeometryPolyline" for LineString/
   MultiLineString (#9033)

OSM driver:
 * properly escape special characters with TAGS_FORMAT=JSON open option (#9673)

Parquet driver:
 * support reading and writing layer metadata
 * Read/write support for covering.bbox struct columns for spatial
   filtering, add SORT_BY_BBOX=YES/NO layer creation option (GeoParquet 1.1)
 * Read/write support for GeoArrow (struct-based) encoding (GeoParquet 1.1)
 * add more configuration options for Parquet dataset case, and default to using
   custom I/O layer (#9497)
 * ogr2ogr/Parquet: add hack to avoid super lengthy processing when using -limit
   with a Parquet dataset source (#9497)
 * make it recognize bbox field from Overture Maps 2024-01-17-alpha.0 and
   2024-04-16-beta.0 releases
 * fix ResetReading() implementation, when using the ParquetDataset API and
   when there's a single batch
 * fix opening single file Parquet datasets with the ParquetDataset API when
   using PARQUET:filename.parquet

PGDUMP driver:
 * add a LAUNDER_ASCII=YES/NO (default NO) layer creation option

PMTiles driver:
 * handle decompressing data with a compression ratio > 16 (#9646)

PostgreSQL driver:
 * serialize GDAL multidomain metadata of layer as XML in a
   ogr_system_tables.metadata table
 * be robust to inexact case for schema when creating a layer
   'schema_name.layer_name' (#9125)
 * remove support for PostgreSQL < 9 and PostGIS < 2 (#8937)
 * add a LAUNDER_ASCII=YES/NO (default NO) layer creation option

Shapefile driver:
 * Shapelib: resync with upstream
 * GetNextArrowArray(): add specialized implementation restricted to situations
   where only retrieving of FID values is asked
 * make it recognize /vsizip/foo.shp.zip directories
 * add read/write support for DBF Logical field type mapped to OGR OFSTBoolean

VFK driver:
 * Fix solving circle parcel geometry (#8993)

## SWIG Language Bindings

All bindings:
 * add osr.SpatialReference.ImportFromCF1(), ExportToCF1(), ExportToCF1Units()
 * expose Geometry.UnaryUnion()
 * expose OLCFastWriteArrowBatch
 * add gdal.IsLineOfSightVisible()

CSharp bindings:
 * Exposes additional Dataset methods (#918, #9398)

Java bindings:
 * bump minimum version to Java 8

Python bindings:
 * lots of improvements to documentation
 * add a pyproject.toml with numpy as a build requirement (#8926, #8069)
 * pyproject.toml: use numpy>=2.0.0rc1 for python >=3.9 (#9751)
 * bump setuptools requirement to >= 67.0
 * define entry_points.console_scripts (#8811)
 * add RasterAttributeTable::ReadValuesIOAsString, ReadValuesIOAsInteger,
   ReadValuesIOAsDouble, RemoveStatistics()
 * implement ``__arrow_c_stream__()`` interface for ogr.Layer
 * add a ogr.Layer.GetArrowArrayStreamInterface() method
 * add a ogr.Layer.WriteArrow() method consuming ``__arrow_c_stream__`` or
   ``__arrow_c_array__`` interfaces (#9132)
 * Invalidate layer ref when Dataset closes
 * Accept str arg in FeatureDefn.GetFieldDefn
 * Add Band.ReadAsMaskedArray
 * Make gdal/ogr.GeneralCmdLineProcessor accept int, os.PathLike
 * Avoid crash when accessing closed file

# GDAL/OGR 3.8.5 Release Notes

GDAL 3.8.5 is a bugfix release.

## Build

* Disable my_test_sqlite3_ext in static builds
* Fix false-positive -Wformat-truncation with clang 18 on fedora:rawhide CI
* cpl_vsil_unix_stdio_64.cpp: avoid compiler warning related to ftello()
* Fix compiler crash on gcore/overview.cpp with ICC 2024.0.2.29 (#9508)
* CMake: Fix FindGEOS to remove use of deprecated exec_program()
* CMake: fix NumPy detection when Intel MKL library is installed
* CMake: add modern Apple OS (visionOS|tvOS|watchOS) support (#9550)
* Minimal support for TileDB 2.21 to avoid build & test issues

## GDAL 3.8.5

### Port

* /vsiaz/: handle properly BlobEndpoint ending with a slash (#9519)

### Core

* QuietDeleteForCreateCopy(): forward source dataset open options (#9424)
* Overview/RasterIO resampling: fix infinite looping when nodata has a big
  absolute value (#9427)

### Utilities

* gdalinfo_output.schema.json: add comment about size and proj:shape ordering
* gdalinfo -json/gdal.Info(format='json'): avoid error/exception on engineering
  CRS (#9396)
* gdalwarp: cutline zero-width sliver enhancement: avoid producing invalid
  polygons
* gdal2tiles.py: fix exception when -v flag is used and overview tiles are
  generated (3.7.0 regression) (#9272)
* gdalattachpct.py: fix it when output file is a VRT (#9513)

### Raster drivers

DIMAP driver:
 * add radiometric metadata

ERS driver:
 * avoid 'Attempt at recursively opening ERS dataset' when the .ers file
   references a .ecw (#9352)

GPKG driver:
 * avoid invalid use of pointer aliasing that caused ICC 2024.0.2.29 to
   generate invalid code (#9508)

GRIB driver:
 * avoid floating-point issues with ICC 2024.0.2.29 (#9508)

GTiff driver:
 * fix read error/use-after-free when reading COGs with mask from network
   storage (#9563)

JP2OpenJPEG driver:
 * CreateCopy(): limit number of resolutions taking into account minimum block
   width/height (#9236)

OGCAPI driver:
 * fix potential use-after-free on vector tiled layers

VRT driver:
 * VRTDerivedRasterBand: Support Int8, (U)Int64 with Python pixel functions
 * VRT/gdal_translate -of 200% 200%: make sure that the synthetized virtual
   overviews match the dimension of the source ones when possible
 * VRTPansharpenedDataset: allow to specify <OpenOptions> for <PanchroBand> and
   <SpectralBand>

## OGR 3.8.5

### Core

* OGRGeometry::getCurveGeometry(): avoid failures when building some compound
  curves with inferred circular strings (#9382)
* OGRLayer::GetArrowSchema(): remove potential unaligned int32_t writes
* CreateFieldFromArrowSchema(): don't propagate native subtype if we have to
  use a fallback main type

### Vector drivers

Arrow/Parquet driver:
 * fix inverted logic regarding spatial filtering of multipolygon with GeoArrow
   interleaved encoding

FlatGeoBuf driver:
 * Make sure vendored flatbuffers copy has a unique namespace
 * implement OGRLayer::GetDataset() (#9568)

GMLAS driver:
 * fix crash when reading CityGML files (r-spatial/sf#2371)

GPKG driver:
 * Ensure that mapping tables are inserted into gpkg_contents
 * Ensure that tables present in gpkgext_relations can be read

ILI2 driver:
 * emit an error and not just a warning when creating a dataset without a model
   file

ODS driver:
 * declare OLCStringsAsUTF8 on newly created layers

OpenFileGDB driver:
 * Correctly use "features" as related table type (instead of "feature")
 * writer: fix corrupted maximum blob size header field in some SetFeature()
   scenarios (#9388)
 * avoid issue with -fno-sanitize-recover=unsigned-integer-overflow with recent
   clang

Parquet driver:
 * avoid potential assertion/out-of-bounds access when a subset of row groups
   is selected

PMTiles driver:
 * fix 'Non increasing tile_id' error when opening some files (#9288)

Shapefile driver:
 * Fix bug when reading some .sbn spatial indices

XLSX driver:
 * declare OLCStringsAsUTF8 on newly created layers

## Python bindings

* gdal.Translate()/gdal.Warp()/etc.: make sure not to modify provided options[]
  array (#9259)
* Fix gdal.Warp segfault with dst=None

# GDAL/OGR 3.8.4 Release Notes

GDAL 3.8.4 is a bugfix release.

## Build

* FindECW.cmake: make it work for Windows 32-bit builds (#9106)
* Restore use of gmtime_r and localtime_r; extend to ctime_r; use Windows
  variants too
* FindSQLite3.cmake: improve detection of static libsqlite3.a (#9096)
* bash_completion installation: Allow the project_binary_dir to contain a
  whitespace

## Docker build recipes

* docker/ubuntu-full/Dockerfile: update to Arrow 15.0.0
* docker/ubuntu-full/Dockerfile: pin libarrow-acero-dev version (#9183)
* docker/ubuntu-full/Dockerfile: disable AVX2 when building TileDB

## GDAL 3.8.4

### Port

* /vsisparse/: fix Stat() on files larger than 4 GB on 32-bit builds
* CPLAtof()/CPLStrtod(): recognize again INF and -INF
* /vsicurl/: fix potential multithreaded crash when downloading the same region
  in parallel and that the download fails

### Core

* PAM: only unset GPF_DIRTY flag
* GDALOverviewDataset: avoid setting SetEnableOverviews(false) during lifetime
  of object. Just do it transiently

### Utilities

* gdalinfo: do not emit errors if corner coordinate reprojection fails
* gdalwarp: do not enable blank line detection when -tap and -te are specified
 (#9059)

### Raster drivers

BMP driver:
 * fix reading images larger than 4GB (3.4.0 regression)

EEDA/EEDAI driver:
 * use 'crsWkt' element

GRIB driver:
 * degrib: use gmtime_r() or gmtime_s() when possible

netCDF driver:
 * use VSILocalTime()

PCIDSK driver:
 * PCIDSK SDK: use ctime_r() or ctime_s() when possible

PDF driver:
 * correctly initialize PAM when opening a subdataset (specific page for
   example)

VRT driver:
 * VRTPansharpenedRasterBand::GetOverviewCount(): robustify against potential
   failure of GDALCreateOverviewDataset()

WMS driver:
 * fix nullptr dereference on invalid document (ossfuzz #65772)

## OGR 3.8.4

### Core

* ExecuteSQL(dialect=SQLite): support 'SELECT\n' for example (#9093)
* OGRGeometryFactory::createGeometry(): do not assert on wkbUnknown input

### Utilities

* ogr2ogr: Arrow code path: take into account -limit parameter for
  MAX_FEATURES_IN_BATCH

### Vector drivers

GeoRSS, GPX, JML, KML, LVBAG, SVG, XLSX drivers:
 * harmonize on a 8192 byte large parsing buffer on all platforms

Arrow/Parquet driver:
 * add (minimum) support for libarrow 15.0
 * MapArrowTypeToOGR(): make the code robust to potentially new entries in the
   arrow::Type enumeration

CAD driver:
 * Internal libopencad: use localtime_r() or localtime_s() when possible

CSV driver:
 * do not quote numeric fields even if STRING_QUOTING=ALWAYS (3.8.1 regression)
   (#55808)

GMLAS driver:
 * recognize GeometricPrimitivePropertyType

LIBKML driver:
 * fix crash on a gx:Track without when subelements (qgis/qgis#55963)

MSSQLSpatial driver:
 * Fix BCP performance problem (#9112)

MySQL driver:
 * fix/workaround server-side spatial filtering when SRS is geographic with
   MySQL >= 8 (qgis/QGIS#55463)

ODS driver:
 * fix parsing of large cells on Windows (at least with mingw64) with recent
   expat 2.6.0 release

PDF driver:
 * vector stream parser: correctly parse structures like "[3 3.5] 0 d"

PDS driver:
 * fix compilation with Emscripten version 3.1.7

SQLite driver:
 * OGR2SQLITE_Setup(): robustify against potential crashing scenario

### Python bindings

* remove run of 'python -m lib2to3' that is a no-op, given that lib2to3 is
  removed in python 3.13 (#9173)

# GDAL/OGR 3.8.3 Release Notes

GDAL 3.8.3 is a bugfix release.

## Build

* infback9: fix various build issues with clang 17
* fix build with sqlite 3.36.x (#9021)
* fix build of optional utility gdal2ogr

## GDAL 3.8.3

### Port

* AWS S3: add explicit error message when IMDS fails and we know we are on EC2

### Utilities

* gdalbuildvrt: in -separate mode, only use ComplexSource if needed

### Raster drivers

VRT driver:
* VRTDerivedRasterBand::IRasterIO(): fix potential multiplication overflow

## OGR 3.8.3

### OGRSpatialReference

* CoordinateTransformation::TransformBounds(): fix polar stereographic (
  including pole) to Web Mercator (#8996)

### Utilities

* ogr2ogr: do not use ArrowArray interface if -clipsrc, -clipdst, -gcp or
  -wrapdateline are specified (#9013)

### Vector drivers

* OGRArrowArrayHelper::SetBoolOn(): fix wrong bit shift computation.
  Affects ogr2ogr from GPKG/FlatGeoBuf to something else) (#8998)

GPKG driver:
 * Fix error message that sometimes occurred with multi-threaded ArrowArray
   interface, and add OGR_GPKG_NUM_THREADS config option. (#9018, 9030)

GeoJSON driver:
  * Internal libjson: resync random_seed.c with upstream, and use getrandom()
    implementation when available (#9024)

# GDAL/OGR 3.8.2 Release Notes

GDAL 3.8.2 is a bugfix release.

## GDAL 3.8.2

### Port

* /vsis3/: takes into account AWS_CONTAINER_CREDENTIALS_FULL_URI environment
  variable (#8858)
* cpl_safemaths.hpp: fix compilation with clang targeting Windows (#8898)
* CPLGetPhysicalRAM(): fix getting right value when running inside Docker on a
  cgroups v1 system (like Amazon Linux 2) (#8968)

### Algorithms

* Rasterization: avoid burning pixel that we only touch (with an empty
  intersection) (#8918)

### Utilities

* gdal_footprint: return an error if the requested output layer doesn't exist
* gdal_translate: avoid useless extra GDALOpen() call on a target GeoRaster
* pct2rgb.py: emit explicit exception when source file has no color table (#8793)

### Raster drivers

HDF5 driver:
 * classic 2D API: handle char,ushort,uint,int64,uint64 attributes when
   reading them as double
 * multidim: better warning when nodata value is out of range

JPEGXL driver:
 * add compatibility with latest libjxl git HEAD

NGSGEOID driver:
 * make dataset identification robust to NaN values (#8879)

OGCAPI driver:
 * make it robust to missing 'type' on 'self' link (#8912)

STACTA driver:
 * use GDAL_DISABLE_READDIR_ON_OPEN=EMPTY_DIR instead of
   CPL_VSIL_CURL_ALLOWED_EXTENSIONS
 * use STAC Raster extension to get number of bands, their data type, nodata
   value, scale/offset, units, and avoid fetching a metatile
 * add support for upcoming STAC 1.1 which merges eo:bands and raster:bands
   into bands

netCDF, HDF4, HDF5:
 * SubdatasetInfo API: fix various issues (#8869, #8881)

VRT driver:
 * VRTComplexSource: fix excessive RAM usage with many sources (#8967, 3.8.0 regression)

### OGR 3.8.2

### Core

* OGRGeometryFactory::transformWithOptions(): fix WRAPDATELINE=YES on
  multipoint geometries (#8889)
* OGRSpatialReference::importFromUrl(): changes to no longer use a
  'Accept: application/x-ogcwkt' header
* OSRPJContextHolder: call pthread_atfork() once for the process, and
  re-enable it for MacOS
* OGRWKBIntersectsPessimisticFixture: handle all geometry types

### Utilities

* ogrinfo: really honours -if (refs #8590)
* ogr2ogr: implement -if

### Vector drivers

* PMTiles: Correct extension for temporary mbtiles file

## Python bindings

* gdal.Footprint(): add a minRingArea option
* fix build/install when there's a gdal-config from a pre-installed version in
  the PATH (#8882)
* add missing reference increment on Py_None in error case of
  Geometry.GetPoints() (#8945)

# GDAL/OGR 3.8.1 Release Notes

GDAL 3.8.1 is a bugfix release.

## Build

* CMake: add gdalinfo bash-completion file to list of installed files
* Fix build error with libxml2 2.12
* CMake: make GDAL_USE_LIBKML and GDAL_USE_OPENJPEG honor GDAL_USE_EXTERNAL_LIBS
* Detect failure in installation of the Python bindings

## GDAL 3.8.1

### Port

* CSLLoad2(): remove CPLErrorReset()

### Core

* RasterIO: fix subpixel shift when reading from overviews with non-nearest
  resampling
* GDALOverviewDataset::IRasterIO(): use parent dataset when possible for more
  efficiency

### Algorithms

* Inverse TPS transformer: speed improvement in gdalwarp use case (#8672)

### Utilities

* gdalwarp -of COG: use target SRS from -co TILING_SCHEME when specified (#8684)
* gdalwarp: add a heuristic to clamp northings when projecting from geographic
  to Mercator (typically EPSG:3857) (#8730)
* gdal_rasterize: fix inverse rasterization of polygon nested inside another
  one. Requires GEOS enabled build (#8689)
* gdal_footprint: fix -ovr on RGBA datasets (#8792)
* gdal_footprint: fix wrong taking into account of alpha band (#8834)
* gdal_footprint: fix taking into account of individual bands that have nodata
* gdal_sieve.py/gdalattachpct.py/gdalcompare.py/gdalmove.py:
  make sure --version and --help return 0 error code (#8717)

### Raster drivers

BSB driver:
 * fix opening datasets with errant 0x1A character in header (#8765)

COG driver:
 * avoid warnings when converting from world coverage to EPSG:3857
 * for JPEG compression, convert single band+alpha as single band JPEG +
   1-bit mask band

KEA driver:
 * Create(): error out if passing a /vsi file. avoids crashes (#8743)

MRF driver:
 * Avoid crashes when no overviews can be generated (#8809)

MSGN driver:
 * fix memleak in error code path

GeoTIFF driver:
* multithreaded reader/writer: in update scenarios, do not force serialization
  to disk of dirty blocks that intersect the area of interest to read (#8729)
* SRS reader: include VertCRS name from EPSG in CompoundCRS name if there's no
  citation geokey

VRT driver:
 * VRTSourcedRasterBand: serialize approximate statistics inside .vrt when
   there are overviews

## OGR 3.8.1

### Core

* PostFilterArrowArray(): various fixes to pass libarrow full validation checks
 (#8755)
* Add OGRCloneArrowArray()
* WriteArrowArray(): fix wrong taking into account of struct offset

### Utilities

* ogr2ogr: fix GPKG to shapefile with the -preserve_fid flag (#8761,
  3.8.0 regression)
* ogr2ogr: fix GPKG -> Shapefile when field names are truncated (#8849,
  3.8.0 regression)

### Vector drivers

Arrow/Parquet driver:
 * use OGRCloneArrowArray() for safer filtering

CSV driver:
 * CSV writer: do not quote integer fields by default (only if
   STRING_QUOTING=ALWAYS is specified)

GML driver:
 * SaveClasses(): fix memleak in error code path (ossfuzz#63871)

GPKG driver:
 * fix SetFeature()/UpdateFeature()/DeleteFeature() on views with INSTEAD OF
   triggers (#8707)
 * sqlite_rtree_bulk_load.c: fix memleak in error code path
 * fix adding field comments after alternative name
 * Add a OGRPARSEDATE_OPTION_LAX option to OGRParseDate() and use it when
   reading GPKG files (#8759)
 * fix GetNextArrowArray() when there are more than 125 columns (affects
   ogr2ogr from such GPKG) (#8757)

GPX driver:
 * make detection of extensions element more robust (#8827)

OAPIF driver:
 * add INITIAL_REQUEST_PAGE_SIZE open option (#4556)

PMTiles driver:
 * avoid undefined-shift when zoom level is too big (ossfuzz#64234,
   ossfuzz#64404)

S57 driver:
 * stricter dataset identification to avoid recognize S-101 datasets we don't
   handle

Shapefile driver:
 * fix spurious warning when reading polygons (#8767)
 * recognize '      0' as a null date
 * fix writing an invalid "0000/00/00" date

SQLite driver:
 * fix SRS retrieval of a SELECT layer from a non-Spatialite DB with a point
   geometry column (#8677)

## Python bindings

* GetArrowStreamAsNumPy(): fix missing offset when reading fixed size list of
  string
* Fix installation issue with Python 3.12 on Debian
* Python bindings: add a combineBands option to gdal.Footprint()

# GDAL/OGR 3.8.0 Releases Notes

GDAL/OGR 3.8.0 is a feature release.
Those notes include changes since GDAL 3.7.0, but not already included in a
GDAL 3.7.x bugfix release.

## In a nutshell...

* Add [JSONFG](https://gdal.org/drivers/vector/jsonfg.html) read/write vector
  driver for OGC Features and Geometries JSON.
* Add [PMTiles](https://gdal.org/drivers/vector/pmtiles.html) read/write vector
  driver for PMTiles v3 datasets containing MVT PBF tiles
* Add [S102](https://gdal.org/drivers/raster/s102.html) raster read-only driver
  for S-102 bathymetric products (depends on libhdf5)
* Add [gdal_footprint](https://gdal.org/programs/gdal_footprint.html) utility:
  compute the footprint of a raster file, taking into account nodata/mask band,
  and generating polygons/multipolygons corresponding to areas where pixels are
  valid (#6264)
* Python bindings: various enhancements to reduce the number of "gotchas"
  related to inter-object ownership relationships, and a few syntaxic sugar
  enhancements
* Arrow interface: improve spatial and attribute filtering on read side;
  add write side with OGRLayer::WriteArrowBatch()
* GeoPackage: much faster spatial index creation (~ 3-4 times faster)
* ARG driver deprecated: will be removed in 3.9.0

## Backward compatibility issues

See [MIGRATION_GUIDE.TXT](https://github.com/OSGeo/gdal/blob/release/3.8/MIGRATION_GUIDE.TXT)

## New optional dependencies

* libaec to enable CCSDS Adaptive Entropy Coding decompression in the GRIB driver

## Build

* emit better error message when a raster driver cannot be enabled
  because of OGR_BUILD_OPTIONAL_DRIVERS=OFF
* improve static linking for CURL and EXPAT
* Java bindings: change default installation directory of JNI shared
  library and control it with GDAL_JAVA_JNI_INSTALL_DIR
* automatically enable SQLite driver if -DOGR_ENABLE_DRIVER_GPKG=ON
* Don't use libjpeg if disabled (and libjpeg-turbo >= 3.0 available) (#8336)
* gdal.pc generation: use CMAKE_INSTALL_INCLUDEDIR/CMAKE_INSTALL_LIBDIR for
  includedir/libdir (#8012)
* gdal-config: add a --plugindir switch (#8012)

## GDAL 3.8.0 - Overview of Changes

### Port

* Add third_party/fast_float header library for fast string->double conversion
  for CPLStrtodDelim()
* /vsimem/: make it safe to use in multi-threaded scenarios
* /vsimem/: implicitly create parent directories when creating file
* CPLParseXML(): do not call CPLErrorReset()
* CPLJSon: add setters for uint64_t
* Add CPLJSONArray::AddNull() and CPLJSONObject constructor with primitive types
* VSIUnixStdioHandle / VSIWin32Handle: make Close() be callable multiple times
  to be friendly with VSIVirtualHandleUniquePtr
* /vsicurl/: avoid the same region to be downloaded at the same time from
  concurrent threads (#8041)
* /vsicurl/ / /vsicurl_streaming/: recognize IGNORE_FILENAME_RESTRICTIONS=YES
  open option to skip any extension based filtering (#8162)
* /vsicurl/: emit warnings if invalid values of CPL_VSIL_CURL_CHUNK_SIZE and
  CPL_VSIL_CURL_CACHE_SIZE are used (#8499)
* /vsicurl_streaming/: implement retry strategy if GDAL_HTTP_MAX_RETRY is set
* /vsis3_streaming/ and the like: implement ReadDir() by forwarding to
  non-streaming filesystem (#8191)
* IVSIS3LikeFSHandler::CopyFile(): retry with non-streaming source for more
  robustness
* /vsis3/ and /vsioss/: less error prone management of redirects / region
  discovery
* /vsiaz/: do not append trailing slash for directories deduced from
  .gdal_marker_for_dir special file
* /vsiaz/: implement server-side copy from /vsis3/, /vsigs/, /vsiadls/,
  /vsicurl/ to /vsiaz
* /vsiaz/: fix CopyObject() when source and target are both /vsiaz/ but in
  different buckets
* /vsiaz/: update to version 2020-12-06 for GetSignedURL(), limit to https and
  use blob resource type
* /vsi network file system: support r+ access under
  CPL_VSIL_USE_TEMP_FILE_FOR_RANDOM_WRITE=YES by copying initial content of
  remove file locally
* VSISync() network to network: implement SyncStrategy::TIMESTAMP
* Add VSIGetCanonicalFilename()
* CPLvsnprintf(): deal with '%.*f' formatting
* VSIFilesystemHandler: remove 'virtual' qualifier from ReadDir(), which now
  forwards to ReadDirEx(); modify all implementations to implement ReadDirEx()
* Add /vsicached? virtual file system, as another way of doing the same as
  VSICreateCachedFile() / VSI_CACHE=YES
* VSIFilesystemHandler::CopyFile(): detect wrong target file size w.r.t source
  file size
* /vsi7z/: Accept ArcGIS Pro Project Packages extension
* cpl_vsil_win32: check return value of FlushFileBuffers(), and open file with
  GENERIC_WRITE only if access=w or wb
* cpl_vsil_win32: implement a WRITE_THROUGH=YEs option to pass to VSIFOpenEx2L()

### Core

* GDALIdentifyDriverEx() and GDALDriver::QuietDelete(): do not call
  CPLErrorReset()
* GDALDataset::Open(): take into account open options for OF_SHARED mode (#7824)
* PAM .aux.xml: read geotransform from Esri GeodataXform.CoeffX/CoeffY elements
 (qgis/qgis#53125)
* Add a DCAP_FLUSHCACHE_CONSISTENT_STATE capability, and fix GeoJSON, JSONFG,
  GPKG drivers to implement it
* Overview/RasterIO resampling: use Float64 as working data type for Float64
  input data (#8187)
* Add GDALRegisterPlugins function to register all/only plugins (#8447)
* GDALDriver::DefaultCreateCopy(): in non-strict mode, turn (ignored) errors as
  warnings and do not silence them
* Add a GDALPamDataset::SetDerivedDatasetName() method, and use it to be able to
  save statistics of datasets returned by GDALMDArray::AsClassicDataset()
* Add GDALGetSubdatasetInfo() and related functions  (#8155)

### Multidimensional API

* add Rename() methods. Implemented in MEM, netCDF, Zarr drivers
* add DeleteGroup(), DeleteMDArray(), DeleteAttribute()
  methods. Implemented in MEM, netCDF (for attributes) and Zarr drivers
* GDALDatasetFromArray: fix write/update support
* GDALMDArrayMask: take into account attributes at construction time
* GetMask(): add a UNMASK_FLAGS option
* GDALMDArray::ComputeStatistics(): add option to set actual_range in netCDF
  and Zarr when computing statistics
* GDALMDArray::AsClassicDataset(): add a LOAD_EXTRA_DIM_METADATA_DELAY option,
  and expose it in Zarr driver
* GDALMDArray::AsClassicDataset(): allow to map band indexing arrays as band
  metadata items with BAND_METADATA option
* Add GDALGroup::SubsetDimensionFromSelection()

### Algorithms

* Warp average resampling: using Weighted incremental algorithm mean for numeric
  stability
* Warper: auto-enable OPTIMIZE_SIZE warping option when reasonable (#7761)
* Geoloc transformer: warn if the input dataset is larger in width or height
  compared to the geoloc array (#7751)
* GDALCreateGenImgProjTransformer2(): deal with discontinuity of GCPs around
  antimeridian (#8371)
* TPS transformer: use an iterative method to refine the inverse transformation
  (#8572)

### Utilities

* gdaladdo: add options to partially refresh existing overviews:
  ``--partial-refresh-from-source-timestamp,``,
  ``--partial-refresh-from-projwin <ulx> <uly> <lrx> <lry>``,`
  ``--partial-refresh-from-source-extent <filename1,...,filenameN>``
* gdal_translate: emit warning when -a_scale/-a_offset + -unscale is specified
 (#7863)
* gdal_translate: GTiff, COG, VRT, PNG and JPEG drivers recognize
  COPY_SRC_MDD=AUTO/YES/NO and SRC_MDD=domain_name creation options
* gdal_translate: add -a_gt option to assign geotransform (#8248)
* gdal_translate -scale: change dstMax value from 255.999 to 255
* gdal_translate: when specifying -srcwin, preserve source block size in the
  temporary VRT if srcwin top,left is a multiple of the block size
* gdalwarp: in -tr mode (without -ts / -te), detect blank edge lines/columns
  before warping and remove them (#7905)
* gdalwarp: fix cutline processing when warping with a cutline geometry in UTM
  1/60 crossing the antimeridian, on a raster in long/lat SRS (#8163)
* gdalwarp: tune usage to allow both -s_coord_epoch and -t_coord_epoch
* gdalwarp: fix error when using -ct and -cutline (master only), and actually
  use the -ct when possible if sourceCRS != targetCRS and targetCRS == cutlineCRS
* gdalwarp: do not enter a specific COG optimized code path when some of its
  preconditons are not met (#8655)
* gdalmdiminfo: output details of indexing variables that can be accessed only
  from the array (typically for TileDB dimension labels)
* nearblack: add "-alg floodfill" to select a flood fill algorithm, to address
  concave areas.
* gdalmdimtranslate: add support for resample=yes array spec option
* gdalbuildvrt: make -separate option process all bands of input datasets,
  unless -b is specified (#8584)
* gdal_polygonize: add a -overwrite switch (#7913)
* gdal_rasterize_lib: fix error messages
* gdaltransform: add -s_coord_epoch and -t_coord_epoch
* gdalcompare.py: multiple enhancements and new command line options
* gdal_pansharpen.py: avoid error trying to generate relative path
* Add validate_geoparquet.py sample Python script
* gdalinfo.py: use math.isnan() (#8196)
* gdalinfo.py: fix wrong order of long,lat corner coordinates (#8199)
* make --help and --help-general available in all utilities (#3816)

### Raster drivers

AAIGRID driver:
 * writing: remove leading space on each line (#8344)

ARG driver:
 * mark it deprecated, removal planned for GDAL 3.9 (#7920)

BAG driver:
 * increase the efficiency of getting depth&uncertainty values from the
   refinement grids
 * use low-resolution grid as the last overview level in MODE=RESAMPLED_GRID
 * add a MODE=INTERPOLATED mode using mostly bilinear interpolation of the
   refinement grid nodes

COG driver:
 * only update mode if the IGNORE_COG_LAYOUT_BREAK=YES open option is specified
   (#7735)
 * add a STATISTICS=AUTO/YES/NO creation option and forward gdal_translate
   -stats to it (#8169)
 * Lerc: add a MAX_Z_ERROR_OVERVIEW creation option to separately control the
   error threshold of overviews w.r.t the one of the full resolution image

ENVI driver:
 * warn if assigning different nodata value to different bands
 * support Int64 and UInt64

ESRIC driver:
 * Implement ESRI Tile Package (.tpkx) support (#7799)

GRIB driver:
 * allow opening files with invalid Earth of shape (#7811)
 * implement CCSDS Adaptive Entropy Coding decompression. Requires libaec (#8092)
 * only emit a CPLDebug() instead of a message on stdout when there are trailing
   bytes (#8574)
 * GRIB2 SRS writing: add support for Rotated LatLong grids (fixes #8536)

GTiff driver:
 * Performance improvement: avoid using block cache when writing whole blocks
   (up to about twice faster in some scenarios)
 * GTiff multi-threaded reader: catch errors emitted in worker threads and
   re-emit them in main thread
 * Internal libtiff: WebP codec: turn exact mode when creating lossless files
   to avoid altering R,G,B values in areas where alpha=0 (#8038)
 * Lerc: add a MAX_Z_ERROR_OVERVIEW creation option to separately control the
   error threshold of overviews w.r.t the one of the full resolution image
 * SRS writer: write Projected 3D built as a pseudo-compound in .aux.xml
 * Internal libtiff and libgeotiff: resynchronization with upstream

HDF5 driver:
 * optimize code-paths for RasterIO() without resampling
 * multidim: speed-up very slow cases of IRead()

ISG driver:
 * make it able to read headers > 1024 bytes
 * take into ISG format 2.0

JPEG driver:
 * allow QUALITY down to 1
 * redirect JPEG 'output message' to GDAL debug messages
 * only take into account first Exif directory found

MBTiles driver:
 * Add WEBP support (#8409)

MEM driver:
 * allocate a single buffer for band-interleaved data

netCDF driver:
 * on reading, set NETCDF_DIM_xxx band metadata items in on-demand way (helps
   with network accesses)
 * do not set NETCDF_DIM_xxxx_VALUES dataset metadata items for variables of
   unlimited dimensions on network access for performance reasons
 * better error message when reading from /vsi is not possible (#8398, #8378)
 * renormalize CRS and geotransform to metric, typically for EUMETSAT OSI SAF
   products. Add a PRESERVE_AXIS_UNIT_IN_CRS=YES/NO open option
 * add support for EMIT band data ordering and geolocation array (
   using glt_x/glt_y for multidimensional API)

NITF driver:
 * add support for CSCSDB (Common Sensor Covariance Support Data) DES from
   GLAS/GFM SDEs

OpenFileGDB raster:
 * add support for FileGDB v9 raster datasets
 * add a RASTER_DATASET metadata item with the name of the RasterDataset (#8427)

PDF driver:
 * PDFium backend: update to support (and require) PDFium/5952
 * Poppler backend: implement overviews by adjusting the DPI value (#8233)
 * PoDoFo backend: add support for PoDoFo >= 0.10.0 (#8356)
 * increase threshold to detect tile size and band count (#8236, #8240)
 * Various robustness fixes

PRF driver:
 * add associated PRJ file read

Sentinel2 driver:
 * additional metadata (#8379)

TileDB driver:
 * add read/write multidimensional support (requires libtiledb >= 2.15)
 * TileDBRasterBand::IRasterIO(): use correct band indexing
 * read/write Int8, Int64 and UInt64
 * add capability to read arbitrary (i.e. not created by GDAL) 2D/3D dense
   array (provided it uses uint64 dimension)
 * Add support for TileDB 2.17

VRT driver:
 * add `norm_diff` (#8081), `min` and `max` pixel functions (#8292)
 * ignore <OverviewList> when external .vrt.ovr is present, as documented and
   intended
 * vrt:// connection string: add `projwin`, `projwin_srs`, `tr`, `r`,
   `srcwin`, `a_gt`, `oo`, `scale`, `unscale`, `a_coord_epoch`, `nogcp`, `eco`,
   `epo`
 * VRTComplexSource (scaling typically): make sure to take into account
   constraints from VRTRasterBand data type in RasterIO() (rather than just
   taking into account output buffer data type)
 * IRasterIO(): avoid edge effects at sources boundaries when downsampling with
   non-nearest resampling
 * VRTMDArraySourceFromArray: fix taking into account relativeToVRT=1
 * allow a <ArraySource> element containing a 2D multidimensional array as a
   VRTRasterBand source and through the use of a <DerivedArray> make it possible
   to create a 2D array from a 3D or more multidimensional one, by slicing,
   transposing, resampling, gridding, etc.
 * VRTComplexSource: perf improvement: add specialization when only NODATA for
   Byte/UInt16/Int16 data types
 * VRTSimpleSource::GetFileList(): do not issue a stat() as it may be slow on
   network drives
 * VRTSourcedRasterBand::GetMinimum/GetMaximum(): limit to 1 second max when
   iterating over sources
 * VRTSourcedRasterBand::GetMinimum/GetMaximum(): use STATISTICS_MINIMUM/MAXIMUM
   metadata first

WCS driver:
 * remove non-standard 'FORMAT' parameter from 'DescribeCoverage' requests
   (#8381)

Zarr driver:
 * allow update support in classic mode
 * Zarr V3: update to current specification (breaks backward compatibility)
 * implement GDALDriver::Rename(), Delete() and CopyFiles()
 * ignore filename restrictions when reading tile data files (#8162)
 * add MULTIBAND=YES/NO, DIM_X and DIM_Y open options (#8237)
 * classic raster API: write multi-band datasets as Zarr 3D arrays (writing
   them as several 2D arrays as in GDAL 3.7 can be asked with the
   SINGLE_ARRAY=NO creation option)
 * fix writing partial tiles

## OGR 3.8.0 - Overview of Changes

### Core

* exportToGEOS(): do not drop M dimension with GEOS >= 3.12
* Add OGR_G_IsClockwise() and map it to SWIG
* core and ogr2ogr: add logic so that ogr2ogr can try a driver specific
  implementation of GDALVectorTranslate()
* OGRParseDate(): restrict valid times to HH:MM:SS(.sss) with at least 2 figures
  (#8150)
* Add OGR_F_DumpReadableAsString
* ArrowArray interface: make PostFilterArrowArray() deal with attribute filter,
  and enable that in Parquet&Arrow drivers
* OGRLayer::GetArrowStream(): do not issue ResetReading() at beginning of
  iteration, but at end instead, so SetNextByIndex() can be honoured
* ArrowStream interface: make TIMEZONE="unknown", "UTC", "(+|:)HH:MM" or any
  other Arrow supported value as an option of the generic implementation
* Add OGRLayer::WriteArrowBatch()
* ArrowArray: implement fast 'FID IN (...)' / 'FID = ...' attribute filter in
  generic GetNextArrowArray(), and use it for FlatGeoBuf one too (when it has a
  spatial index) (#8590)
* GetArrowStream(): support a GEOMETRY_METADATA_ENCODING=GEOARROW option (#8605)
* GetNextArrowArray() implementations: automatically adjust batch size of list/
  string/binary arrays do not saturate their capacity (2 billion elements)
* OGRGeometry classes: add addGeometry()/addRing()/addCurve() methods accepting
  a std::unique_ptr
* OGRLineString/Polygon/MultiPolygon/MultiLineString: make it possible to run
  importFromWkb() on the same object and limiting the number of dynamic memory
  (re)allocations
* organizePolygons: Remove handling of nonpolygonal geometries
* OGRGeometryFactory::transformWithOptions() WRAPDATELINE=YES: remove heuristics
  about points exactly at +/- 180 (#8645)
* gml2ogrgeometry: reject empty <gml:Triangle/>
* OGRGF_DetectArc(): harden tolerance when detecting consecutive arcs to avoid
  incorrect arc center computation (#8332)
* OGR SQL: allow MIN() and MAX() on string fields
* SQLite dialect: when the underlying layer has a FID column name, enable the
  user to use it as an alias of ROWID
* SQLite dialect: error out with explicit message on unsupported commands (#8430)
* Add OGRFieldDefn::GetTZFlag()/SetTZFlag(), and OGR_TZFLAG_ constants
* GDALDataset::ICreateLayer(): now takes a const OGRSpatialReference* instead of
  a OGRSpatialReference*. Affects out-of-tree drivers (#8493)
* OGR Python drivers: support WKB geometries

### OGRSpatialReference

* Add OGRCoordinateTransformationOptions::SetOnlyBest() /
  OCTCoordinateTransformationOptionsSetOnlyBest() (#7753)
* OGRProjCT::Transform(): do not emit generic error message if a specific one
  has already been emitted
* SetFromUserInput(): add support for urn:ogc:def:coordinateMetadata (PROJ >=
  9.4)
* SetFromUserInput(): recognize 'EPSG:XXXX@YYYY' (PROJ >= 9.4)
* Add OSRHasPointMotionOperation() (PROJ >= 9.4)
* OGR_CT: handle point motion operations (PROJ >= 9.4)

### Utilities

* ogrinfo: speed-up string concatenation
* ogrinfo: add support for DateTime field domains
* ogrinfo: emit distinct error message if the file doesn't exist or can't be
  opened (#8432)
* ogrinfo: output timezone flag
* ogr2ogr: calls FlushCache() (#8033)
* ogr2ogr: tune usage to allow both -s_coord_epoch and -t_coord_epoch
* ogr2ogr: better deal when reprojecting curve geometries to a non-curve
  geometry type (#8332)
* ogr2ogr: use Arrow interface in reading and writing when possible
* make -select '' work (or gdal.VectorTranslate(selectFields=[]))

### Vector drivers

Arrow/Parquet driver:
 * emit ARROW:extension:name=ogc.wkb in Feature field metadata, and return it
   also through GetArrowStream() for Parquet
 * implement faster spatial filtering with ArrowArray interface
 * optimize attribute filter on FID column
 * support/reading nested list/map datatypes as JSON
 * implement full spatial filtering (not just bbox intersection)
 * reading and writing: use field TZFlag
 * implement WriteArrowBatch() specific implementation
 * support LargeString and LargeBinary for geometry columns (read support only)

CSV driver:
 * reader: change to use separator with the most occurrences (#7831)
 * and add a SEPARATOR=AUTO/COMMA/SEMICOLON/TAB/SPACE/PIPE open option (#7829)
 * implement GetFileList() and return .csvt if used (#8165)

DGN driver:
 * CreateFeature(): fix crash on empty geometries (ossfuzz#56771)

DXF driver:
 * Preserve attributes in nested block insertions

ESRIJSON driver:
 * add support for esriFieldTypeSingle and esriFieldTypeDate data types

FlatGeoBuf driver:
 * GetNextArrowArray(): implement full spatial filtering (not just bbox
   intersection)

GeoJSON driver:
 * add AUTODETECT_JSON_STRINGS layer creation option (#8391)
 * writer: when writing with limited coordinate precision, run MakeValid() to
   avoid creating invalid geometries
 * reading: set field TZFlag
 * writer: in RFC7946 mode, refine logic to determine if a multipolygon spans
   over the antimeridian to write correct bbox (qgis/qgis#42827)
 * writer: use faster file write() primitive and detect write() errors

GeoJSONSeq driver:
 * add AUTODETECT_JSON_STRINGS layer creation option (#8391)
 * writer: use faster file write() primitive and detect write() errors

GeoPackage driver:
 * use much faster creation of RTree with a in-memory RTree building
 * speed-up HasMetadataTables() on dataset with many layers
 * speed-up unique constraint discovery on dataset with many layers
 * more efficient retrieval of layer extent from RTree content
 * deal with DateTime fields without milliseconds or seconds, as allowed by
   GeoPackage 1.4 (#8037)
 * add a DATETIME_PRECISION layer creation option (#8037)
 * implement SetNextByIndex() on table layers (by appending
   'OFFSET -1 LIMIT index')
 * make GetArrowStream() honour SetNextByIndex()
 * add a CRS_WKT_EXTENSION=YES/NO dataset creation option to force addition of
   definition_12_063 column
 * add a METADATA_TABLES creation option to control creation of system metadata
   tables
 * GetNextArrowArray(): implement full spatial filtering (not just bbox
   intersection)
 * GetNextArrowArray(): only do multi-threaded prefetch if more than 1 GB RAM
   available
 * make invalid attribute filter to cause error in main thread (so Python
   binding can emit an exception)
 * GPKG / SQLite dialect: improve detection of geometry columns when first row
   is NULL (#8587)
 * make GetFeatureCount() do full geometry intersection and not just bounding
   box (#8625)

GPX driver:
 * add a CREATOR dataset creation option

HANA driver:
 * Set sessionVariable:APPLICATION in connection string
 * Support connections using a user store key (#7946)

Memory driver:
 * add a FID layer creation option to specify the FID column name

MVT/MBTiles driver:
 * take into account tileStats metadata item to decide if a field of type
   'number' might be Integer or Integer64
 * MVT writer: clamp generated tile x, y coordinates to \[0,(1<<z)-1\]

NAS driver:
 * remove unused nas relation layer and remove GML driver's out-of-band
   attribute handling only used for it
 * support gfs @ notation for attributes to handle attributes for codelists
   (fixes norBIT/alkis-import#65)
 * Fix (and refactor) update operations for GID7

MySQL driver:
 * fix compliance issues with test_ogrsf

OAPIF driver:
 * bump default limit to 1000 and honor schema from API (#8566)

ODS driver:
 * add FIELD_TYPES and HEADERS open options (#8028)

OpenFileGDB driver:
 * add support for DateTime field domains
 * expose layer alias name in ALIAS_NAME layer metadata item

Parquet driver:
 * emit GeoParquet 1.0.0 version number
 * add a COORDINATE_PRECISION layer creation option
 * add fast implementation of Arrow Array interface when requesting WKT as WKB
 * make Parquet driver recognize a geometry column if it has
   ARROW:extension:name=ogc.wkb/ogc.wkt field metadata
 * add a GEOM_POSSIBLE_NAMES and CRS open options for wider compatibility with
   datasets not following GeoParquet dataset-level metadata
 * fix ExecuteSQL() MIN/MAX optimization on a UINT32 field on a Parquet 2 file
 * optimize SELECT MIN(FID), MAX(FID)
 * restrict FID column detection to Int32/Int64 data types
 * use statistics to skip row groups that don't match attribute filter (#8225)
 * use statistics of bbox.minx/miny/max/maxy fields (as found in Ouverture Maps
   datasets) to implement fast GetExtent()

PDF driver:
 * (minimal) take into account BMC operator to correctly handle BMC/EMC pairs
   w.r.t BDC/EMC ones (#8372)
 * Read vector unstructured: take into account OCMD constructs as found in
   recent USGS GeoPDFs (#8372)
 * ignore non-relevant StructTreeRoot in most recent USGS GeoPDFs (#8372)

PGDump driver:
 * use faster file write() primitive

PostgreSQL driver:
 * do not override search_path when not needed (#8641)

Shapefile driver:
 * use VSIGetCanonicalFilename() in GetFileList() (#8164)
 * be tolerant with .prj with lon, lat axis order (fixes #8452)

TileDB driver:
 * GetNextArrowArray(): implement full spatial filtering (not just bbox
   intersection)

XLSX driver:
 * add FIELD_TYPES and HEADERS open options (#8028)

WFS driver:
 * Don't issue STARTINDEX if feature count is small (#8146)
 * do not emit twice DescribeFeatureType request on servers with complex
   features and a single layer
 * ExecuteSQL(): skip leading spaces that could cause the rest of the function
   to malfunction
 * correctly paginate when number of features is lower than page size (#8653)
 * use numberMatched when present to avoid last empty GetFeature request, and
   set GetFeatureCount()

## SWIG Language Bindings

All bindings:
 * Map multidimensional API Rename() methods
 * Increment FeatureDefn ref count on ogr.Layer.GetLayerDefn()
 * add ogr.CreateRangeFieldDomainDateTime() and Domain.GetMinAsString()
   /GetMaxAsString()
 * Expose GetConfigOptions()
 * add gdal.SuggestedWarpOutput()
 * Expose GDALClose

Java bindings:
 * add Read/WriteRaster abilities for GDT_UInt64 & GDT_Int64 (#7893)
 * make multidimensional API usable (#8048)
 * implement ogr.CreateCodedFieldDomain() and FieldDomain.GetEnumeration()
  (#8085)
 * eliminate some deprecations / compiler warnings (#8055)
 * Add FieldDefn::GetFieldType() compatibility method

Python bindings:
 * Allow passing options as dict
 * Return context manager from Create, CreateDataSource
 * Use ogr.DataSource and gdal.Dataset as context managers
 * make gdal.Group.GetGroupNames() and GetMDArrayNames() return an empty list instead of None
 * detect invalid use of ReleaseResultSet() (#7782)
 * Invalidate band and layer refs when dataset closes
 * Invalidate mask, overview references after dataset close
 * Invalidate refs from CreateLayer on datasource close
 * Invalidate refs from CopyLayer on datasource close
 * Avoid crash when using dataset after Destroy or Release
 *  Avoid crashes when using orphaned Geometry refs
 * GetArrowStreamAsNumPy(): various fixes
 * Fix gdal.config_options to prevent migration of config options in and out of
   thread-local storage (#8018)
 * accept numpy.int64/float64 arguments for xoff, yoff, win_xsize, win_ysize,
   buf_xsize, buf_ysize arguments of ReadAsArray() (#8026)
 * make setup.py check that libgdal version >= python bindings version (#8029)
 * fix CoordinateTransform.TransformPoint(sequence of 3 or 4 values)
 * Use DumpReadable for Feature __repr__
 * add a outputGeotransform option to gdal.Translate()
 * Accept os.PathLike arguments where applicable
 * make Feature.SetField(field_idx_or_name, binary_values) work
 * add feature.SetFieldBinary(field_idx_or_name, binary_values)
 * throw exceptions (when enabled) on gdal_array.OpenArray()/OpenNumPyArray()/
   OpenMultiDimensionalNumPyArray()
 * improve performance of CSLFromPySequence() on large sequences
   (like > 100,000 strings)
 * Allow ExecuteSQL context manager to work on empty datasources
 * make sure that CPL_DEBUG=ON and gdal.UseExceptions() work fine during
   gdal.VectorTranslate() (and similar) (#8552)

# GDAL/OGR 3.7.3 Release Notes

GDAL 3.7.3 is a bugfix release.

## Build

* CheckDependentLibraries.cmake: don't use libjpeg if disabled
* frmts/mrf/CMakeLists.txt: ignore HAVE_JPEGTURBO_DUAL_MODE_8_12 if
  GDAL_USE_JPEG is OFF (tune #8336)
* FindPostgreSQL.cmake: Update the list of released PgSQL versions
* Add support for PoDoFo >= 0.10.0 (#8356)
* Add support for TileDB 2.17
* Fix build with -DRENAME_INTERNAL_SHAPELIB_SYMBOLS=OFF (#8532)

## GDAL 3.7.3

### Algorithms

* Rasterize: avoid burning the same pixel twice in MERGE_ALG=ADD mode within
  a same geometry (#8437)
* OpenCL warp: scan multiple devices for each platform (#8540)
* GDALSerializeReprojectionTransformer(): fallback to WKT2 for SourceCRS/
  TargetCRS if WKT1 cannot be used

### Port

* VSIReadDirRecursive(): make it work properly with a trailing slash
  (especially on /vsizip/) (#8474)
* /vsi7z/: accept ArcGIS Pro Project Packages

### Utilities

* gdal_translate -ovr: properly rescale RPC (#8386)
* gdal_translate: when specifying -srcwin, preserve source block size in the
  temporary VRT if srcwin top,left is a multiple of the block size
* gdal2tiles: raise explicit exception if bad value for --s_srs (#8331)

### Raster drivers

ENVI driver:
 * skip leading spaces at front of metadata keys

GIF driver:
 * Internal giflib: fix memleak on animations (#8380)
 * Internal giflib: fix memleak on truncate files (#8383)

GTiff driver:
 * multithreaded reading/writing: fix a deadlock and/or data corruption
   situation (#8470)
 * DirectIO(): fix integer overflow when requesting more than 2 GB at once
 * Support writing a CRS of known code but unknown GeoTIFF projection method
   (#8611). Support also a CRS of unknown code but that can be encoded as
    ESRI_WKT if GEOTIFF_KEYS_FLAVOR=ESRI_PE creation option is used
 * JXL codec: fix wrong use of memcpy() in decoding, and add memcpy()
   optimizations

MBTiles driver:
 * BuildOverviews(): correctly set minzoom metadata (#8565)

netCDF driver:
 * multidim: add write support for Int8 data type (#8421)

VRT driver:
 * IRasterIO(): avoid edge effects at sources boundaries when downsampling
   with non-nearest resampling

WCS driver:
 * remove 'FORMAT' parameter from 'DescribeCoverage' requests

## OGR 3.7.3

### Core

* OGRSQL: fix incorrect interaction of LIMIT clause and SetNextByIndex()(#8585)
* OGRWKBGetBoundingBox(): properly initialize envelope

### OGRSpatialReference

* TransformBounds(): do not emit errors when trying to reproject
  points at poles
* ogr_proj_p.cpp: disable pthread_atfork() optimization on MacOS (#8497)

### Utilities

* ogr2ogr: fix -gt 0 to disable transactions
* ogr2ogr: make -select '' work (or gdal.VectorTranslate(selectFields=[]))
* ogr2ogr: fix preserve_fid and explodecollections incompatible options (#8523)

### Vector drivers

Arrow/Parquet drivers:
 * fix crash on ArrowArray interface when there is a fid column

CSV driver:
 * avoid extra comma at end of header line with GEOMETRY=AS_XYZ and a single
   attribute (#8410)

FlatGeoBuf driver:
 * ArrowStream: return a released array when there are no rows (#8509)

GeoPackage driver:
 * GPKG SQL layer GetNextArrowArray(): fix wrong array size when spatial
   filtering occurs
 * ArrowStream: immediately return a release array when there are no rows
   (#8509)
 * declare OGR_GPKG_GeometryTypeAggregate_INTERNAL only when needed

GPKG/SQLITE drivers:
 * declare relevant SQL functions as SQLITE_INNOCUOUS (#8514)
 * turn PRAGMA trusted_schema = 1 when loading Spatialite (#8514)
 * turn PRAGMA trusted_schema = 1 if RTree+update mode or if views use
   Spatialite functions (#8514)

OAPIF driver:
 * make comparison of rel case insensitive to account for describedBy ->
   describedby change

OpenFileGDB driver:
 * fix opening of .gdb directories whose last component starts with 'a' and
   is 18 character long... (#8357)

NTF driver:
 * fix assertion on corrupted dataset (ossfuzz#63531)

Shapefile driver:
 * Reader: correct bad (multi)polygons as written by QGIS < 3.28.11 with
   GDAL >= 3.7 (qgis/qgis#54537)

SQLite driver:
 * ignore 'SRID=' layer creation option (qgis/QGIS#54560)

XLSX driver:
 * do not write empty 'cols' element on empty layers, and change heuristics to
   detect 'default' empty layers from intended empty ones (qgis/qgis#42945)

## Python bindings

* GetArrowStreamAsNumPy(): fix reading fixed size list arrays that were
  ignoring the parent offset (affects Parquet)
* GetArrowStreamAsNumPy(): fix reading fixed width binary that were misusing
  the offset (affects Parquet)

# GDAL/OGR 3.7.2 Release Notes

GDAL 3.7.2 is a bugfix release.

## Build

* Ccache.cmake: fix warning
* XLS: avoid compiler warning when building against freexl 2.0
* FindSPATIALITE.cmake: updated for SpatiaLite 5.1.0
* CMakeLists.txt: update maximum range to 3.27
* fix building test_ofgdb_write (#8321)

## GDAL 3.7.2

### Port

* /vsiaz/: fix cached URL names when listing /vsiaz/
* /vsiaz/: add options to pass object_id/client_id/msi_res_id in IMDS
  authentication requests (AZURE_IMDS_OBJECT_ID, AZURE_IMDS_CLIENT_ID,
  AZURE_IMDS_MSI_RES_ID)
* /vsiaz/: implement Azure Active Directory Workload Identity authentication,
  typically for Azure Kubernetes

### Core

* TileMatrixSet::parse(): add support for OGC 2D Tile Matrix Set v2 (#6882)

### Algorithms

* Warper: do not modify bounds when doing geographic->geographic on a dataset
  with world extent but not in [-180,180] (#8194)
* RMS resampling: avoid potential integer overflow with UInt16 values
* GDALChecksumImage(): fix 3.6.0 regression regarding integer overflow on
  images with more than 2 billion pixels (#8254)

### Utilities

* gdalinfo -json output: emit a stac['proj:epsg'] = null object when emitting
  proj:wkt2 or proj:projjson (#8137)
* gdalmdimtranslate: fix wrong output dimension size when using syntax like
  '-array name=XXX,view=[::factor_gt_1]'
* gdal2tiles: fix exception with dataset in EPSG:4326 with longitudes > 180 in
  WebMercator profile (#8100)
* gdal_retile.py: allow gaps in input files larger than grid size (#8260)

### Raster drivers

GeoPackage driver:
 * GDALGeoPackageRasterBand::GetMetadata(): fix use after free
 * fix missing GRID_CELL_ENCODING metadata item when there is other metadata
 * remove .aux.xml file in Delete()

GTiff driver:
 * fix reading .tif + .tif.aux.xml file with xml:ESRI SourceGCPs without
   TIFFTAG_RESOLUTIONUNIT (#8083)

HDF5 driver:
 * more efficient metadata collection (no functional change)
 * deal with int64/uint64 attributes
 * remove trailing space in multi-valued metadata items
 * remove dataset name prefix in band level metadata
 * address Planet's datacube band-specific metadata

NITF driver:
 * fix MIN/MAX_LONG/LAT when reading RPC00B
 * add support for CSCSDB (Common Sensor Covariance Support Data) DES from
   GLAS/GFM SDEs
 * nitf_spec.xml: corrections to CSEXRB TRE

OGCAPI driver:
 * make it work when the media type of links (expected to be application/json)
   is missing, using Accept content negotiation (#7970)
 * do not try to use the 'uri' member of a tilematrixset definition document
 * reproject bounding box from CRS84 to tile matrix set CRS
 * skip too small overview levels
 * remove erroneous taking into account of tilematrixset limits

STACIT driver:
 * correctly process asset 'href' starting with 'file://' (#8135)
 * make it tolerant to missing proj:epsg if proj:wkt2 or proj:projjson are
   provided (#8137)
 * apply vsis3 protocol to s3:// items

WEBP driver:
 * fix build against libwebp < 0.4.0 (#8111)

Zarr driver:
 * Zarr V2: fix duplicate array listing when both a 'foo' file and 'foo/'
   directory exist on the object storage (#8192)

## OGR 3.7.2

### Core

* Fix ExecuteSQL(dialect='SQLITE', spatialFilter=...) on a Memory datasource
* OGRCurve::isClockwise(): fix wrong result when lowest rightmost vertex is the
  first one (#8296)

### Utilities

* ogrinfo: return non-zero ret code if -sql failed (#8215)
* ogrinfo_output.schema.json: reflect actual output of ogrinfo (#8300)
* ogr2ogr: support -nlt GEOMETRY -nlt CONVERT_TO_LINEAR (#8101)

### Vector drivers

HANA driver:
 * Fix incorrect detection of table name for view columns
 * Don't process layers that are not in TABLES list

OGCAPI driver:
 * vector tiles: avoid potential infinite time to establish layer definition

OpenFileGDB driver:
 * make Open() to fail if requested to open in update mode and that files are
   in read-only (qgis/QGIS#53715)

Shapefile driver:
 * make CreateLayer() + GetFileList() list the .prj file when it exists (#8167)

WFS driver:
 * do not surround selected fields names in PROPERTYNAME with open and close
   parenthesis (#8089)

## Python bindings

* fix typo on slopeFormat parameter (#8154)
* osgeo_utils: align base.get_extension() and util.DoesDriverHandleExtension()
  on C++ versions (#8277)

# GDAL/OGR 3.7.1 Release Notes

GDAL 3.7.1 is a bugfix release.

## New installed files

* data/gfs.xsd and data/gml_registry.xsd

## Build

* CMake: only try to detect HDF5 CXX component if KEA is detected
* CMake: capture dependency of MBTiles driver to MVT
* CMake: add a EXPAT_USE_STATIC_LIBS hint (#7955)
* CMake: add a CURL_USE_STATIC_LIBS hint (#7955)
* minified_zutil.h: fix build error on Gentoo and derivatives (#7739)
* Fix build error with MSYS64 UCRT64 with gcc 13 (#7909)
* FlatGeobuf: fix build error when new Abseil headers are present (#7894)
* Fix build against latest libjxl master

## GDAL 3.7.1

### Port

* CPLBloscDecompressor(): fix logic error when providing an output buffer
  larger than needed
* Fix issues on big endian hosts: recode from UTF-16
* RequiresUnixPathSeparator(): takes into account /vsihdfs/ and /vsiwebhdfs/
  (#7801)
* /vsiadls/: make it take into account Azure SAS token
* /vsiaz/: fix truncated Authorization header with very large SAS tokens

### Core

* Multidim GDALGroup::CopyFrom(): modify logic to copy first indexing variables.
  Fixes issue with gdalmdimtranslate from Zarr to netCDF
* RawRasterBand::IRasterIO(): avoid harmless unsigned integer overflow
  (ossfuzz #57371)
* GDALDatasetPool: Make VRT pool size clamping more lenient (#7977)
* PAM rasterband: on .aux.xml reading do not systematically set (offset,scale)
  to (0,1) and similarly for unit type (#7997)

### Algorithms

* Warp (average,mode,min,max,med,Q1,Q3): fix issue on edge of target valid area
  when oversampling (#7733)
* Warp average resampling: using Weighted incremental algorithm mean for numeric
  stability
* GDALWarpResolveWorkingDataType(): take into account data type of both source
  and target dataset

### Utilities

* gdallocationinfo: set exit code to 1 as soon as one coordinate is off the file
  (#7759)
* gdal_rasterize: issue explicit error message when specifying invalid layer
  name, and catch absence of bounds (#7763)
* gdalsrsinfo: when -e flag is passed, report properly non EPSG authorities
  (#7833)
* gdalwarp: make -cutline to work again with PostGIS datasources
  (fixes 3.7.0 regression, #8023)
* gdal_calc.py: allow "none" or a float for --NoDataValue (#7796)
* gdal_calc.py: make --hideNoData imply --NoDataValue=none as documented (#8009)

### Raster drivers

DIMAP driver:
 * correctly set RPC origin for SPOT, PHR and PNEO (#7726)

DIPEx driver:
 * fix memleak in error code path (ossfuzz 57478)

ERS driver:
 * avoid integer overflow (ossfuzz 57472)

GTiff driver:
 * multi-threaded reader: avoid error when reading PlanarConfiguration=Separate
   and ExtraSamples > 0 (fixes #7921)
 * Multi-threaded reader: fix crash/read errors when reading a
   PlanarConfiguration=Separate file band per band (rasterio/rasterio#2847)
 * SRS writer: make sure to write EPSG codes only in VerticalDatumGeoKey and
   VerticalUnitsGeoKey, and write VerticalCSTypeGeoKey = KvUserDefined,
   VerticalDatumGeoKey = KvUserDefined and VerticalCitationGeoKey=CRS name when
   the vertical CRS code is unknown (#7833)
 * SRS reader: try to recover the vertical datum from the name of the vertical
   CRS if we only know it (#7833)
 * SRS reader: try to retrieve the EPSG code of a CompoundCRS if the one of its
   horizontal and vertical part is known (#7982)
 * SPARSE_OK=YES: recognize negative floating point 0 as 0 (#8025)
 * do not emit 'WEBP_LEVEL is specified, but WEBP_LOSSLESS=YES' warning when
   only WEBP_LOSSLESS=YES is specified

HDF5 driver:
 * multidim: deal with _FillValue attribute of different type as variable
   (e.g. for NASA GEDI L2B products)

ISG driver:
 * relax tolerance check to accept GEOIDEAR16_20160419.isg

JP2OpenJPEG driver:
 * avoid hard crash in multi-threaded writing mode when opj_write_tile()
   fails (#7713)

netCDF driver:
 * multidim: fix issues with data vs definition mode
 * multidim: deal with _FillValue attribute of different type as variable
   (e.g. for NASA GEDI L2B products)
 * parse coordinates attribute from Sentinel-3 Synergy product which uses
   comma instead of space

OGCAPI driver:
 * Fix TILES raster API not working
 * make sure OGR_ENABLE_DRIVER_GML is set when the driver is built

OpenFileGDB driver:
 * correctly deal with rasters whose block_origin_x/y != (eminx, emaxy) (#7794)
 * do not emit error if SRS is unknown (#7794)
 * reduce debug traces on raster datasets

SAFE driver:
 * fix non-contiguous number in subdataset names (#7939)

Zarr driver:
 * avoid GetMDArrayNames() to report duplicates
 * fix SRS DataAxisToSRSAxisMapping

WMTS driver:
 * workaround buggy TileMatrix.TopLeftCorner of one server (#5729)

## OGR 3.7.1

### Core

* OGRGeometryFactory::createGeometry(): take into account Z/M flags
* SQL SQLite: fix error on field of type String with GetWidth() > 0 and a
  field domain

### Vector drivers

CSV driver:
 * do not allow CREATE_CSVT=YES with /vsistdout/ (#7699), and make it possible
   to use GEOMETRY_NAME with /vsistdout/ (#7700)

DGN driver:
 * CreateFeature(): fix crash on empty geometries (ossfuzz 56771)

DXF driver:
 * show constant ATTDEFs as text

FileGDB driver:
 * OGRCreateFromShapeBin(): accept empty polygon (#7986)

GeoJSON driver:
 * Internal libjson: use locale insensitive CPLStrtod() to parse floating point
   numbers (qgis/qgis#52731)

GeoPackage driver:
 * ArrowArray interface: make it tolerant to Spatialite geometries
 * fix handling of geometry column from a view, when it is the result of
   computing, and not just simple selection from a table
 * define a 'gdal_spatialite_computed_geom_column' extension when a view has
   its geometry column being the result of Spatialite SQL function
 * implement GeoPackage 1.4 RTree triggers, and add a VERSION=1.4 dsco (#7823)

GML driver:
 * fix 3.7.0 regression when reading files with several layers, whose features
   have gml:boundedBy elements and the geometry element name is different
   between the different layers (#7925)
 * add a USE_BBOX=YES open option to allow using gml:boundedBy as feature
   geometry (#7947) to revert back to < 3.7.0 behavior by default

GTFS driver:
 * Fix date parsing

MITAB driver:
 * fix reading CRS with LCC_2SP and non-metre unit (#7715)

MSSQL driver:
 * fix crash in ogr2ogr when BCP is enabled (#7787)
 * fix GEOGRAPHY clockwise vertex order (#1128)
 * fix cannot write binary field in BCP mode (#3040)

MVT driver:
 * writing: improve dealing with polygon inner rings (#7890)
 * writing: fix wrong winding order for polygons after MakeValid()

MySQL driver:
 * add support for inserting new SRS in MySQL >= 8
   INFORMATION_SCHEMA.ST_SPATIAL_REFERENCE_SYSTEMS table (#7781)

netCDF driver:
 * fix crash on invalid layer (ossfuzz 58469)

OpenFileGDB driver:
 * allows to modify a record with a GlobalID field without regenerating it
 * correctly read POINT EMPTY (#7986)

Parquet driver:
 * fix crash when calling SetIgnoredFields() after SetAttributeFilter()
   (qgis/QGIS#53301)
 * do not hang on empty RecordBatch (#8042)

SDTS driver:
 * fix reading of polygon geometries (#7680)

Shapefile driver:
 * in creation, uses w+b file opening mode instead of wb followed by r+b, to
   support network file systems having sequential write only and when using
   CPL_VSIL_USE_TEMP_FILE_FOR_RANDOM_WRITE=YES (#7801)
 * SHPRestoreSHX: update SHX content length even if error occurred
 * Internal Shapelib: update list of symbols to rename to avoid conflicts with
   external shapelib (microsoft/vcpkg#32070)

TileDB driver:
 * add TILE_Z_EXTENT layer creation option (#7683)
 * fully apply spatial filter when using ArrowArray interface
 * FillPrimitiveListArray(): fix data corruption with spatial filter

## Python bindings

* take into account Int64/UInt64 in gdal.Array.Write(array(...))
* do not pass deprecated -py3 SWIG switch for SWIG >= 4.1
* GetArrowStreamAsNumPy(): fix wrong retrieval of content when
  ArrowArray::offset != 0 on a field of primitive type (integer, float)
* works around issue with SWIG 4.1 related to module unloading (#4907)

# GDAL/OGR 3.7.0 Releases Notes

GDAL/OGR 3.7.0 is a feature release.
Those notes include changes since GDAL 3.6.0, but not already included in a
GDAL 3.6.x bugfix release.

## In a nutshell...

* [RFC 87](https://gdal.org/development/rfc/rfc87_signed_int8.html): Add
  GDT_Int8 support
* [RFC 88](https://gdal.org/development/rfc/rfc88_googletest.html): switch
  to GoogleTest framework for C++ tests (#3525)
* [RFC 89](https://gdal.org/development/rfc/rfc89_sql_logging_callback.html):
  SQL query logging callback (#6967)
* [RFC 90](https://gdal.org/development/rfc/rfc90_read_compressed_data.html):
  Direct access to compressed raster data
* [RFC 91](https://gdal.org/development/rfc/rfc91_dataset_close.html):
  GDALDataset::Close() method
* [RFC 93](https://gdal.org/development/rfc/rfc93_update_feature.html):
  OGRLayer::UpdateFeature() method
* [RFC 94](https://gdal.org/development/rfc/rfc94_field_precision_width_metadata.html):
  Numeric fields width/precision metadata
* ogrinfo: make it accessible through a new GDALVectorInfo() C API call, and
  a -json switch
* Add read-only raster driver NOAA_B to read NOAA GEOCON/NADCON5 .b grids
* Add read-only raster driver NSIDCbin for Sea Ice Concentrations (#7263)
* Add read-only vector GTFS (General Transit Feed Specification) driver
* TileDB: add read/write vector side
* Add support for [SOZip](https://sozip.org) (Seek Optimized ZIP) with enhanced
  /vsizip/ virtual file system and a new sozip utility
* OpenFileGDB: add read-only support for raster datasets (.gdb v10)
* PNG: 1.7-2.0x speed-up in whole image decompression with libdeflate on
  Intel/AMD CPUs. Benefits GPKG, MRF drivers
* [RFC 69](https://gdal.org/development/rfc/rfc69_cplusplus_formatting.html):
  C++ code reformatting
* Code linting and security fixes
* Remove any traces of Rasdaman driver, now moved to OSGeo/gdal-extra-drivers
  repository (#4808)

## Backward compatibility issues

See [MIGRATION_GUIDE.TXT](https://github.com/OSGeo/gdal/blob/release/3.7/MIGRATION_GUIDE.TXT)

## New installed files

* data/gfs.xsd: XML schema for .gfs files (#6655)
* data/gml_registry.xsd: new file with XML schema of gml_registry.xml (#6716)
* data/ogrinfo_output.schema.json: to validate ogrinfo -json output
* data/gdalinfo_output.schema.json: to validate gdalinfo -json output (fixes #6850)
* data/grib2_table_4_2_0_21.csv
* data/grib2_table_4_2_2_6.csv
* bin/sozip

## Build

* make BUILD_JAVA/CSHARP/PYTHON_BINDINGS default value dependent on the
  presence of requirements, and error out if those variables are set but
  requirements are missing
* Python bindings: remove generated files and require SWIG to be present
* Fix build with -DOGR_ENABLE_DRIVER_GML=OFF (#6647)
* make it possible to build on Linux if linux/fs.h is missing by explicitly
  setting ACCEPT_MISSING_LINUX_FS_HEADER
* Add support for (future) libjpeg-turbo 2.2 with its 8/12 bit dual mode (#6645)
* PDF driver with PDFium support must be built against PDFium from
  https://github.com/rouault/pdfium_build_gdal_3_7
* No longer alias VSILFILE* to FILE* in non-DEBUG builds (#6790)
* Add build option for using static Arrow/Parquet build (#7082)
* Quote variables for INTERFACE_INCLUDE_DIRECTORIES / IMPORTED_LOCATION
* Enable OpenCL at build-time, but disable it at runtime by default unless
  the USE_OPENCL warping option or GDAL_USE_OPENCL config option is set (#7224)
* Fix MSVC x64 builds with /arch:AVX2 (#7625)

## New optional dependencies

* libarchive for new /vsi7z/ and /vsirar/ virtual file systems

## GDAL 3.7.0 - Overview of Changes

### Port

* /vsizip/: add read support for Deflate64 (#7013)
* Add read-only /vsi7z/ and /vsirar/ virtual file systems (depends on libarchive)
* Make it possible to specify all HTTP related configuration options as
  path-specific options with VSISetPathSpecificOption()
* VSIGSFSHandler::UnlinkBatch(): avoid potential nullptr deref
* /vsiaz/: implement UnlinkBatch()
* /vsis3/: add CPL_VSIS3_CREATE_DIR_OBJECT configuration option
* /vsigs/ allow GDAL_HTTP_HEADERS config option to be used as authentication method if contains at least a line starting with "Authorization:"
* /vsihdfs/: fix ReadDir() and EOF flag (#7632)
* add a robust CPLParseKeyValueJson() function (#6753)
* HTTP: set default User-Agent header to GDAL/x.y.z and add a
  CPLHTTPSetDefaultUserAgent() function (#6376)
* HTTP: CUSTOMREQUEST option overrides POST while send form
* /vsicurl/ / CPLHTTPFetch(): add a GDAL_HTTP_NETRC_FILE config option
* /vsicurl/ / CPLHTTPFetch(): add options to support SSL client certificates
* VSICurlHandle::ReadMultiRange(): avoid potential infinite loop
* Add VSIVirtualHandle::AdviseRead() virtual method and implement it in /vsicurl
* Add VSICopyFile()
* CPLCopyFile(): remap onto VSICopyFile()
* Implement CopyFile() for VSIZipFilesystemHandler
* Implement GetFileMetadata() for /vsizip/
* CPLDefaultFindFile: Warn if file not found and GDAL_DATA not defined
* Add VSIDuplicateFileSystemHandler() (for remote stores identifcal to popular
  ones, but with different settings).
* Make C type VSILFILE an alias of C++ VSIVirtualHandle (and make it a struct
  for that purpose) (#6643)
* Add a CPLGetErrorHandler() function
* Add VSIVirtualHandleUniquePtr type, unique pointer of VSIVirtualHandle that
  calls the Close() method

### Core

* ComputeRasterMinMax(), ComputeStatistics(), GetHistogram(): take into account
  mask band (and not only nodata value)
* Modify GDALFlushCache() and GDALDataset::FlushCache() to return CPLErr
  instead of void
* Add GDALDataset::Close() virtual method, call it from GDALClose() and make
  GDALClose() return a CPLErr
* GDALDataset:: add a SetBand() method that takes a GDALRasterBand unique_ptr
* RawRasterBand: cache GDAL_ONE_BIG_READ value for RasterIO (#6726)
* RawRasterBand: add IsValid() and Create() methods that return a unique_ptr
* RawDataset::RasterIO(): add optimization when reading from a BIP dataset to
  a BIP buffer (#6819)
* RAW: fix performance issue when reading files with very small width (#1140)
* Raw drivers: check RawRasterBand validity
* LoadPythonAPI(): take into account Python 3.12
* NASAKeywordHandler: add a Parse() method and change Ingest() return type to be bool
* Add GDALMDArray::GetGridded() / GDALMDArrayGetGridded()
* Add GDALMDArray::Resize() / GDALMDArrayResize(). Implement it in MEM, netCDF
  and Zarr drivers
* Support blocks > 2GB in GDALAllValidMaskBand and GDALNoDataValuesMaskBand
* GDALVersionInfo("BUILD_INFO"): report CURL_ENABLED=YES and CURL_VERSION=x.y.z
* Add CPLSubscribeToSetConfigOption() to subscribe to config option settings
* GoogleMapsCompatible tiling scheme: Increase max zoom level from 24 to 30
* JP2 structure dump: fix interpretation of METH field in COLR JP2 box
* Enable SSE2/AVX2 optims on 32bit MSVC builds if /arch:AVX2 is defined (#7625)

### Algorithms

* Polygonizer: switch implementation to Two-Arm Chains EdgeTracing Algorithm,
  which is much faster in some cases (#7344)
* Prefix ParseAlgorithmAndOptions() public symbol with GDALGrid for proper
  namespacing, and add #define alias for API compat (but ABI breakage)
* Pansharpening: require geotransform on panchromatic and multispectral bands.
  Remove undocumented and somewhat broken MSShiftX and MSShiftY options
* Warper: fix issue with insufficiently large source window, visible with RPC
  DEM warping (#7491)

### Utilities

* gdalwarp: add -srcband (aliased to -b) and -dstband options
* gdalwarp: preserve source resolution by default when no reprojection is
  involved (behavior change), and add '-tr square' to use previous behavior
* gdalwarp: better error message when not providing enough values after a switch
  (#7086)
* gdal_rasterize: support @filename for -sql option (#7232)
* gdal_rasterize: add -oo switch for open options (#7329)
* gdal_grid: add -oo switch for open options (#7329)
* gdal_polygonize.py: use transactions to speed-up writing
* gdal_polygonize.py: add a -lco option, and fix -o (#7374)
* gdalbuildvrt: implement numerically stable averaging of resolution (#7502)
* gdal2tiles: use logging module instead of print() for verbose output (#4894)
* gdal2tiles: update doctype to html format (#7631)
* gdal2tiles: uses GDALTermProgress() for progress bar
* gdal_calc: uses GDALTermProgress() for progress bar (#7549)
* gdal_fillnodata: fix parsing of -co option
* validate_gpkg.py: make it work better on examples from ngageoint repositories
* gdal_cp.py: use gdal.CopyFile()
* C/C++ command line utilities: take into account GDALClose() error code

### Raster drivers

ARG driver:
 * add support for int64/uint64

COG driver:
 * relax a bit the tolerance when computing tile number
 * propagate NUM_THREADS to warping (#7479)
 * add NBITS creation option (#7361)

ERS driver:
 * support GDA2020

GeoPackage driver:
 * load/save band statistics in GPKG metadata tables or PAM .aux.xml

GRIB driver:
 * update tables to wmo-im/GRIB2@v30
 * g2clib: allow negative longitudes in grid templates Lon/Lat, Rotated,
   Stretched, Stretched & Rotated, LAEA (#7456) and Mercator
 * fix GetNoDataValue() on band > 1 when there's a bitmap section (#7649)

GTiff driver:
 * add a JXL_ALPHA_DISTANCE creation option, e.g. to have lossless alpha and
   lossy RGB.
 * call VSIVirtualHandle::AdviseRead() in multithreaded read implementation
 * implement GetCompressionFormats() and ReadCompressedData()
 * add minimum support for reading CRS from ESRI's .xml side car file (#7187)
 * use libtiff >= 4.5 reentrant error handlers (when available) (#6659)
 * make sure that band description in PAM overrides the one coming from
   GDAL_METADATA tag
 * Internal libtiff: resync with upstream

HFA driver:
 * add a DISABLEPESTRING=YES creation option to disable use of ArcGIS PE String
   (#1003)

HDF5 driver:
 * add generic support for HDF-EOS5 grids and swaths (#7117)

JP2OpenJPEG driver:
 * add workaround for dop10 orthophotos wrong colorspace

JPEG driver:
 * add a APPLY_ORIENTATION=YES open option to take into account EXIF_Orientation
 * advertise JPEG_QUALITY metadata item in IMAGE_STRUCTURE domain
 * use ReadCompressedData(), implement lossless copy from JPEGXL that has
   JPEG reconstruction box
 * add a LOSSLESS_COPY creation option
 * add support for reading lossless 8-bit JPEG if using libjpeg-turbo >= 2.2
 * change behavior to return an error and not just a warning with reading a
   truncated file
 * Internal libjpeg: decompressor: initialize Huffman tables to avoid issues
   with some FileGDB raster

JPEGXL driver:
 * add a ALPHA_DISTANCE creation option, e.g. to have lossless alpha and
   lossy RGB.
 * add a APPLY_ORIENTATION=YES open option to take into account EXIF_Orientation
 * advertise COMPRESSION_REVERSIBILITY=LOSSY when there is a JPEG reconstruction
   box
 * add a LOSSLESS_COPY creation option
 * implement creation from JPEGXL content
 * fix lossless copy of JPEG with zlib compressed mask band
 * implement GetCompressionFormats() and ReadCompressedData()

MEM driver:
 * implement GetCoordinateVariables() from coordinates attribute
 * Adds 'SPATIALREFERENCE' element to the DSN format (#7272)

MRF driver:
 * make it use PNG driver for decompression of 8-bit images

netCDF driver:
 * add a ASSUME_LONGLAT open option (#6195)
 * add heuristics to detect invalid validity range when scale_factor is
   present (#7167)
 * report geolocation array for NASA L2 ocean colour products (#7605)

NITF driver:
 * nitf_spec.xml: add definition for subheader of CSATTB, CEEPHB and CSSFAB DES
   of GLAS/GFM
 * add capability of decoding DES data part as XML fields and add descriptors
   for CSATTB, CEEPHB and CSSFAB DES
 * add VALIDATE and FAIL_IF_VALIDATION_ERROR open options
 * fix bug that prevents adding subsequent TREs after a HEX TRE (#6827)

OGCAPI driver:
 * define raster/vector scope of open options
 * Passed uri to TileMatrixSet::parse
 * Updated uris of well-known tile matrix sets

PCIDSK driver:
 * support Web Mercator projection (#7647)

PDF driver:
 * skip JP2ECW driver if ECW_ENCODE_KEY required but not found

PNG driver:
 * 1.7-2.0x speed-up in whole image decompression with libdeflate on Intel/AMD CPUs

RMF driver:
 * Add scale, name, frame support.
 * Add vert CS write support.
 * Implement GetSuggestedBlockAccessPattern

VRT driver:
 * add 'a_offset', 'a_scale', 'a_srs', 'a_ullr', 'expand', 'exponent', 'gcp',
  'if', 'ot', 'outsize', 'ovr', 'scale' options for vrt:// connection string
 * add BLOCKXSIZE and BLOCKYSIZE creation options
 * Fix excessive RAM usage when reading a VRT made of single-tiled JPEG2000
   files read with the JP2OpenJPEG driver
 * implement GetCompressionFormats() and ReadCompressedData()
 * derived band: add a <SkipNonContributingSources>true optional element
   to discard non contributing sources (#7223)
 * VRTPansharpened: avoid issue when querying overviews when PAN and MS bands
   have significant different spatial extent
 * serialize NODATA/NoDataValue elements with double precision (#7486)
 * fix warning regarding with OpenShared with vrt://http://example.com/test.jp2
   with the JP2OpenJPEG driver

WEBP driver:
 * implement GetCompressionFormats() and ReadCompressedData()

## OGR 3.7.0 - Overview of Changes

### Core

* Add OGRLayer::UpdateFeature() and OGR_L_UpdateFeature() (RFC 93).
  Implement it in Memory, GPKG, MongoDBv3, PG
* OGRFeatureDefn: add GetFields() and GetGeomFields() for easier C++ iteration
* OGRFieldDefn: add GetComment() / SetComment() methods
* OGRFeature/OGRGeometry: add a DumpReadable method that outputs to a string
* Add GDAL_DMD_ILLEGAL_FIELD_NAMES, and feel it for OpenFileGDB, FileGDB,
  PostgreSQL
* Add GDAL_DMD_RELATIONSHIP_RELATED_TABLE_TYPES: list of standard related table
  types recognized by the driver, and feel it for OpenFileGDB, FileGDB and GPKG
* Add GDAL_DMD_CREATION_FIELD_DEFN_FLAGS metadata
* Add DCAP_FEATURE_STYLES_READ and DCAP_FEATURE_STYLES_WRITE capabilities
* Add ALTER_ALTERNATIVE_NAME_FLAG for use changing a field's alternative name
  when calling OGR_L_AlterFieldDefn
* Add ALTER_COMMENT_FLAG for altering field comments via OGR_L_AlterFieldDefn
* Add OGRLayer::GetSupportedSRSList() and SetActiveSRS()
* OGRToOGCGeomType(): add options to control output
* GenSQL: fix SetAttributeFilter() when dialect=OGRSQL and not forwarding the
  initial where clause to the source layer (#7087)
* OGR SQL: do not emit error message when comparing a NULL datetime
* OGRFeature::SetField(string argument): for bool, recognize 0/false/off/no as
  false and 1/true/on/yes as true
* Add OGRFeature::GetFieldAsISO8601DateTime() (#7555)
* Geometry WKT import: accept nan as a value, for parity with PostGIS and GEOS
* GDALDataset::CopyLayer(): copy source layer metadata, unless the COPY_MD=NO
  option is specified
* Add OGRGeometry::UnaryUnion() / OGR_G_UnaryUnion()
* SQL SQLite parser: correctly take into account statements like
  'SELECT ... FROM json_each(...)' (#7464)
* Make OGRGeometry::getSpatialReference() return a const OGRSpatialReference*
* Make OGRGeomFieldDefn::GetSpatialRef() return a const OGRSpatialReference*

### OGRSpatialReference

* importFromWkt(): take into account COORDINATEMETADATA[] (PROJ >= 9.2)
* update hard-coded definition of OGC:CRS84 to include the ID
* OSR_Panorama: Add some spatial references (GSK 2011, etc.)
* OSR_Panorama: Fix TM zone for projections with negative central meridian
* OSR_Panorama: Fix import from invalid data
* SetFromUserInput(): skip leading white space (#7170)
* Make OGRCoordinateTransformation::GetSourceCS() and GetTargetCS() return a
  const OGRSpatialReference* (#7377)
* OSRImportFromEPSG(): emit warning message about deprecated CRS substitution
  (#7524)
* Allow CPLSetConfigOption('PROJ_DATA', ...) to work by making it call
  OSRSetPROJSearchPaths()

### Utilities

* ogrinfo: make it accessible through a new GDALVectorInfo() C API call
* ogrinfo: add a -json switch
* ogrinfo: output CRS supported list
* ogrinfo: output relationships
* ogr2ogr: use SetActiveSRS() when possible when -t_srs is used
* ogr2ogr: make conversion from GML2 to GPKG work without explicit -lco
  FID=some_name_different_than_fid
* ogr2ogr: add a -dateTimeTo option to convert datetime between timezones (#5256)
* ogr2ogr: Improve performance of -clipsrc and -clipdst (#7197)
* ogr2ogr: LoadGeometry(): use UnaryUnion()
* ogrmerge.py: add optimization for GPKG -> GPKG non-single layer case (up to
  10x faster)

### Vector drivers

Arrow driver:
 * add support for getting/setting field alternative name and comment in
   gdal:schema extension

CSV driver:
 * recognize pipe separator and .psv extension for read (#6811)
 * fix GetFeatureCount() to work correctly with spatial and attribute filters
 * allow reading single column file (#7595)

CSW driver:
 * Add 'title' as query-able property

FileGDB driver:
 * do not set Length/Precision from OGR width/precision for floating-point data
   types (#7283)
 * correct ObjectID field to have a Length of 4.

FlatGeobuf driver:
 * decrease memory usage when inserting lots of features
 * speed-up writing of DateTime/Date values
 * avoid crash when writing huge geometry
 * Support reading/writing field comments in field description metadata (#7598)

GML driver:
 * Use geometry in boundedBy element if there are no geometry properties
 * use srsDimension on top gml:Envelope as the default one (#6986)
 * GML geometry parsing: don't promote to 3D a 2D <gml:Box>
 * deal with only <gml:null> in boundedBy element
 * fix reading CityGML Lod2 with xlink:href in gml:Solid as found in German
   datasets (qgis/QGIS#51647)
 * add support for getting/setting field comment

GMLAS driver:
 * add BoundingShapeType as a known geometry type

GPKG driver:
 * Implement relationship creation, deletion and update support
 * add direct read and create support for .gpkg.zip files
 * SQLite/GPKG: move PRELUDE_STATEMENTS evaluation just after database opening
 * add a gdal_get_pixel_value() SQL function.
 * add a SetSRID() SQL function
 * speed-up writing of DateTime/Date values
 * allow opening filenames >= 512 characters
 * do not register non-spatial layers on creation if there are already
   unregistered non-spatial layers (qgis/qgis#51721)
 * avoid potential int overflows / crash on huge geometries
 * make SQL function ogr_layer_Extent() available (#7443)
 * Map field alternative name with "name" attribute from gpkg_data_columns table
 * Map field comment with "description" attribute from gpkg_data_columns table
 * hide implicit relationships from NGA GeoInt and Spatialite system tables
 * add ST_EnvIntersects() for faster spatial filtering when there is no
   spatial index
 * minimum (read) support for non-standard multiple geometry columns per table

GPX driver:
 * add capability to read & write content of <metadata> element (#7190)

LVBAG driver:
 * fix std::find() test

MITAB driver:
 * add support for LargeInt (Integer64) data type (#7162)

NAS driver:
 * make it trigger only if NAS_GFS_TEMPLATE config option is set (#7529)

MongoDBv3 driver:
 * avoid get_utf8() deprecatation warning with mongocxx 3.7.0

netCDF driver:
 * add support for getting/setting alternative name and comment

OAPIF driver:
 * Add support for OGC API Features - Part 2 CRS extension (ie ability to work
   with non WGS 84 CRS)
 * Add CRS/PREFERRED_CRS open options to control the active CRS
 * Add a SERVER_FEATURE_AXIS_ORDER open option
 * Implement GetSupportedSRSList() and SetActiveSRS()

OCI driver:
 * improve round-tripping of EPSG CRS (#7551)

OpenFileGDB driver:
 * Optimise writing of large geometries
 * allow CreateField() with OBJECTID as the column name (#51435)
 * make Delete() method to remove the directory (#7216)
 * remove traces of dealing with field precision (#7283)
 * correct ObjectID field to have a Length of 4.
 * take into account SpatialReference.VCSWKID/LatestVCSWKID for compound CRS
 * relax test to detect broken .spx

OGR_VRT:
 * add support for reading alternative name and comment from VRT XML

OSM driver:
 * add a tags_format=json osmconf.ini setting and TAGS_FORMAT=JSON open option,
   as an alternative for HSTORE for other_tags/all_tags fields (#7533)

Parquet driver:
 * add support for getting/setting field alternative name and comment in
   gdal:schema extension

PG driver:
 * Add WKBFromEWKB() for a slightly faster OGRGeometryFromEWKB()
 * fix TEMPORARY layer creation option
 * Add SKIP_VIEWS open option to replace PG_SKIP_VIEWS config option
 * Remove PG_USE_TEXT config option
 * use standard_conforming_strings=ON
 * add support for getting/setting/altering field comments (#7587)
 * truncate table names larger than 63 characters (#7628)

PGDump driver:
 * fix TEMPORARY layer creation option
 * add GEOM_COLUMN_POSITION layer creation option and allow empty FID= (#7482)
 * fix escaping of schema and table name (#7497)
 * add support for setting field comments
 * truncate table names larger than 63 characters (#7628)

Shapefile driver:
 * writer: do no use SHPRewindObject() for [Multi]Polygon layers, but use the
   input OGRGeometry structure to deduce the winding order (#5315)
 * writer: prevent potential overflows on 64-bit platforms on huge geometries
 * writer: optimize MultiLineString writing

SQLite driver:
 * Implement AddRelationship support
 * add a gdal_get_pixel_value() SQL function
 * allow opening filenames >= 512 characters
 * make SQL function ogr_layer_Extent() available (#7443)
 * Spatialite: remove support for libspatialite < 4.1.2

WFS driver:
 * implement GetSupportedSRSList() and SetActiveSRS()

## SWIG Language Bindings

All bindings:
 * add gdal.VectorInfo()
 * fix GDT_TypeCount value (affects C# and Java bindings)
 * add gdal.GetNumCPUs() and gdal.GetUsablePhysicalRAM()
 * add gdal.CopyFile()
 * fix syntax error that fail with SWIG 4.1

CSHARP bindings:
 * add SkiaSharp (#6957)
 * Add missing wrappers for BuildVRT and MultiDimTranslate (#7517)

Python bindings:
 * Emit FutureWarning when exceptions are not explicitly enabled or disabled.
   Turning on exceptions by default is planned for GDAL 4.0
 * Make UseExceptions() on one of gdal/ogr/osr module affect all of them
 * add gdal/ogr/osr.ExceptionMgr() Context Manager for handling Python exception
   state (#6637)
 * add gdal.config_option() and gdal.config_options() context manager
 * add gdal.quiet_errors() context manage
 * make ogr.Open() and ogr.OpenShared() work with verbose error when exceptions
   are enabled
 * gdal.VectorTranslate: add missing extra options (#6486)
 * Adapt various utilities for exceptions enabled: gdal_merge.py,
   ogr_layer_algebra.py, ogr_merge, gdalinfo.py, ogr2ogr.py
 * __init__.py: more robust handling of PATH (cf rasterio/rasterio#2713)
 * do not make gdal.PushErrorHandler()/PopErrorHandler() sensitive to the GDAL
   error context
 * Make GetArrowStreamAsNumPy() handle large lists, strings and binaries
 * make Dataset.ExecuteSQL() usable as a context manager to automatically
   release the layer (#7459)
 * GetArrowStreamAsNumPy(): optimization to save memory on string fields with
   huge strings compared to the average size

# GDAL/OGR 3.6.4 Release Notes

GDAL 3.6.4 is a bugfix release.

## GDAL 3.6.4

### Port

* userfaultfd: avoid it to stall on 32bit and test real working of syscall in
  CPLIsUserFaultMappingSupported()

### Core

* RawRasterBand::FlushCache(): avoid crash in some situations
* RawRasterBand::IRasterIO(): fix wrong byte swapping in Direct IO multiline
  writing code path
* RawRasterBand::IRasterIO(): fix optimized code path that wrongly triggered
  on BIL layout
* RawRasterBand::IRasterIO(): avoid reading and writing too many bytes
* RawRasterBand::IRasterIO(): fix floating-point issues with ICC that could
  result in wrong lines/cols being read/written

### Algorithms

* Rasterize all touched: tighten(decrease) the tolerance to consider that edge
  of geometries match pixel obundaries (#7523)

### Utilities

* gdal_translate: fix crash when specifying -ovr on a dataset that has no
  overviews (#7376)
* gdalcompare.py: correctly take into account NaN nodata value (#7394)
* gdal2xyz.py: fix -srcnodata and -dstnodata options (#7410)
* gdal2tiles: update 'ol-layerswitcher' widget to v4.1.1 (#7544)

### Raster drivers

GTiff driver:
 * correctly read GCPs from ArcGIS 9 .aux.xml when TIFFTAG_RESOLUTIONUNIT=3
  (pixels/cm) (#7484)

HDF5 driver:
 * fix detecting if HDF5 library is thread-safe (refs #7340)

LCP driver:
 * CreateCopy(): fix crash on negative pixel values (#7561)

MRF driver:
 * restore SetSpatialRef() that was wrongly deleted in 3.6.0

netCDF driver:
 * restore capability of reading CF-1.6-featureType vector layers even if the
   conventions >= CF 1.8, and improve featureType=trajectory by adding the
   time attribute (fixes #7550)

## OGR 3.6.4

### Core

* OGRSQL: fix 'SELECT ... WHERE ... AND ... AND ... AND ... UNION ALL ...'
  (#3395)
* OGRUnionLayer::GetExtent(): do not emit error on no-geometry layer
* OGREditableLayer::IUpsertFeature(): fix memleak

### OGRSpatialReference

* Fix OGRSpatialReference::SetProjCS() on an existing BoundCRS;
  affects GeoTIFF SRS reader (fixes gdal-dev/2023-March/057011.html)

### Utilities

* ogr2ogr: fix and automate conversion from list types to String(JSON) when the
  output driver doesn't support list types but String(JSON) (#7397)

### Vector drivers

CSV driver:
 * CSVSplitLine(): do not treat in a special way double quotes that appear in
   the middle of a field

FlatGeobuf driver:
 * improve handling of null geoms (#7483)

GeoPackage driver:
 * Update definition of gpkg_data_columns to remove unique constraint on "name"

OpenFileGDB driver:
 * fix write corruption when re-using freespace slots in some editing scenarios
   (#7504)
 * relax test to detect broken .spx
 * CreateField(): in approxOK mode, do not error out if default value of a
   DateTime field is CURRENT_TIMESTAMP, just ignore it with a warning (#7589)

OSM driver:
 * Fix handling of closed_ways_are_polygons setting in osmconf.ini (#7488)

S57 driver:
 * s57objectclasses.csv: apply S-57 Edition 3.1 Supplement No. 2

SQLite driver:
 * GDAL as a SQLite3 loadable extension: avoid crash on Linux

# GDAL/OGR 3.6.3 Release Notes

GDAL 3.6.3 is a bugfix release.

## Build

* CMake: Fix integration of find_package2()
* CMake: avoid HDF4 CMake error with Windows paths with spaces
* CMake: quote variables for INTERFACE_INCLUDE_DIRECTORIES / IMPORTED_LOCATION
* CMake: fix wrong test when GDAL_SET_INSTALL_RELATIVE_RPATH is set
* CMake: issue an error when the user explicitly asks for a condition-dependent
  driver and the condition is not met
* CMake: add include to FindSQLite3.cmake
* fix uclibc build without NPTL
* zlib: Add ZLIB_IS_STATIC build option
* FindCryptoPP.cmake: properly take into account _LIBRARY_RELEASE/_DEBUG (#7338)
* FindPoppler.cmake: check that Poppler private headers are available (#7352)

## GDAL 3.6.3

### Port

* CPLGetPhysicalRAM(): take into account current cgroup (v1 and v2)
* CPLGetPhysicalRAM(): take into account MemTotal limit from /proc/meminfo
* /vsicurl/: fix CPL_VSIL_CURL_USE_HEAD=NO mode (#7150)
* Avoid use of deprecated ZSTD_getDecompressedSize() function with libzstd 1.3+
* cpl_vsil_crypt.cpp: fix build isse on Windows (#7304)

### Algorithms

* GDALPolygonizeT(): add sanity check
* GDALRasterPolygonEnumeratorT::NewPolygon(): check memory allocation to avoid
  crash (#7027)
* Warper: do not use OpenCL code path when pafUnifiedSrcDensity is not null
  (#7192)
* Warper: optimize a bit when warping a chunk fully within the cutline
* Geoloc inverse transformer: fix numeric instability when quadrilaterals are
  degenerate to triangles (#7302)

### Core

* GDALProxyPoolRasterBand::FlushCache(): fix for ref counting when calling
  FlushCache() on GDALProxyPoolMaskBand or GDALProxyPoolOverviewRasterBand
* VirtualMem: Fix mremap() detection with clang 15, and disable
  HAVE_VIRTUAL_MEM_VMA if HAVE_5ARGS_MREMAP not found

### Utilities

* gdal_translate: make -colorinterp work on a source band that is a mask band
* gdalmdimtranslate: do not require VRT driver to be registered (#7021)
* gdalmdimtranslate: fix subsetting in the situation of dataset of #7199
* gdalwarp: fix vshift mode when vertical unit of dstSrs is non-metric
* gdalwarp: overview choice: fix longitude wrap problem (#7019)
* gdalwarp: allow up to inaccuracy in cropline coordinates up to 0.1% of a
  pixel size for rounding (#7226)
* gdalsrsinfo: fix crash on 'gdalsrsinfo IAU:2015:49902 -o xml'
* gdal_retile.py: fix wrong basename for .aux.xml files (#7120)
* gdallocationinfo: fix issue with VRTComplexSource and nodata (#7183)
* gdal_rasterize: ignore features whose Z attribute is null/unset (#7241)

### Raster drivers

BMP driver:
 * Make sure file is created at proper size if only calling Create() without
   writing pixels (#7025)
 * Create(): add checks and warnings for maximum dimensions

COG driver:
 * avoid warning message when using -co COMPRESS=WEBP -co QUALITY=100 (#7153)

DIMAP driver:
 * optimize performance of dataset RasterIO()

GRIB driver:
 * fix reading South Polar Stereographic from GRIB1 datasets (#7298)
 * degrib: replace use of sprintf() with snprintf()

GTiff driver:
 * GTiffJPEGOverviewBand::IReadBlock(): remove hack that causes read errors in
   some circumstances
 * do not use implicit JPEG overviews with non-nearest resampling
 * fix generation of external overviews on 1xsmall_height rasters (#7194)

GTX driver:
 * fix (likely harmless in practice) integer overflow (ossfuzz#55718)

HDF5 driver:
 * add a GDAL_ENABLE_HDF5_GLOBAL_LOCK build option to add a global lock when
   the HDF5 library is not built with thread-safety enabled (#7340)

HFA driver:
 * ERDAS Imagine SRS support: various fixes: Vertical Perspective projection,
   LCC_1SP, Mercator_2SP, Eqirectanglar, Hotine Obliqe Mercator Azimuth Center

JPEG driver:
 * Correctly read GCPS when an .aux.xml sidecar has GeodataXform present in the
   ESRI metadata element instead of root element

JPEGXL driver:
 * CreateCopy(): fix memory leak when writing georeferencing

MBTiles driver:
 * fix nullptr deref when calling GetMetadata() on a dataset returned by
   Create() (#7067)

netCDF driver:
 * quote variable name in subdataset list if it contains a column character
   (#7061)
 * report GEOLOCATION metadata for a lon/lat indexed variable where lon and/or
   lat has irregular spacing
 * netCDFDimension::GetIndexingVariable(): be more restrictive
 * resolve variable names beyond the parent group (#7325)

NITF driver:
 * update CLEVEL to appropriate values when using compression / multiple image
   segments
 * fix bug that prevents adding subsequent TREs after a HEX TRE (#6827)

PDF driver:
 * skip JP2ECW driver if ECW_ENCODE_KEY required but not found

TileDB driver:
 * fix compatibility with tiledb 2.14

VRT:
 * warp: fix issue when warping a Float32 raster with nodata = +/- FLOAT_MAX

ZMap creation:
 * fix potential truncation of nodata value (#7203)

## OGR 3.6.3

### Core

* OGRSQL: fix crash when comparing integer array fields (#6714)
* OGRSQL: fix SetAttributeFilter() when dialect=OGRSQL and not forwarding the
  initial where clause to the source layer (#7087)

### Utilities

* ogr2ogr: fix -clipsrc/-clipdst when clip dataset has SRS != features's
  geometry (#7126)

### Vector drivers

GeoJSON driver:
 * avoid duplication of FID in streaming parser (#7258)
 * declare GDAL_DCAP_MEASURED_GEOMETRIES and ODsCMeasuredGeometries
 * fix mixed type field not flagged as JSON if first is a string (#7313)
 * writer: take into account COORDINATE_PRECISION for top bbox (#7319)
 * writer: fix json mixed types roundtrip (#7368)

GeoJSONSeq driver:
 * fix writing to /vsigzip/ (#7130)

GeoPackage driver:
 * avoid issue with duplicated column names in some cases (#6976)
 * GetNextArrowArray(): fix retrieving only the geometry (geopandas/pyogrio#212)
 * restore async RTree building for 1st layer (broken by GDAL 3.6.2)

GML driver:
 * fix CurvePolygon export of CompoundCurve and CircularString child elements
   (#7294)

HANA driver:
 * fix DSN open option

MITAB driver:
 * handle projection methods 34 (extended transverse mercator) and 35 (Hotine
   Oblique Mercator) (#7161)
 * Fix possible crash on NULL feature text (#7341)
 * Fix a typo at MITABGetCustomDatum

NAS driver:
 * fix file descriptor leak in error code path

OpenFileGDB driver:
 * fix performance issue when identifying a CRS
 * detect broken .spx file with wrong index depth (qgis/qgis#32534)
 * index reading: avoid integer overflow on index larger than 2 GB
 * allow CreateField() with OBJECTID as the column name (qgis/qgis#51435)
 * make Delete() method to remove the directory (fixes #7216)

Shapefile driver:
 * fix adding features in a .dbf without columns (qgis/qgis#51247)
 * make sure eAccess = GA_Update is set on creation (#7311)

## SWIG bindings

* add missing OLCUpsertFeature constant

## Python bindings

* fix setup.py dir-list issue on macOS

# GDAL/OGR 3.6.2 Release Notes

GDAL 3.6.2 is a bugfix release.

## General

[RFC69](https://gdal.org/development/rfc/rfc69_cplusplus_formatting.html):
Whole code base C/C++ reformatting

## Build

* Avoid warning with curl >= 7.55 about CURLINFO_CONTENT_LENGTH_DOWNLOAD being
  deprecated
* Avoid warning with curl >= 7.87 about CURLOPT_PROGRESSFUNCTION being
  deprecated
* fix nitfdump build against external libtiff (#6968)
* fix compilation with gcc 4.8.5 of Centos 7.9 (#6991)

## Data files

* tms_MapML_CBMTILE.json: fix wrong matrixWidth value (#6922)

## GDAL 3.6.2

### Port

* CPLGetUsablePhysicalRAM(): take into account RSS limit (ulimit -m) (#6669)
* CPLGetNumCPUs(): take into sched_getaffinity() (#6669)

### Algorithms

* Warp: fix crash in multi-threaded mode when doing several warping runs with
  the same WarpOperation
* RasterizeLayer: prevent out-of-bounds index/crash on some input data (#6981)

### Core

* gdal_pam.h: workaround for code including it after windows.h

### Raster drivers

AAIGRID driver:
 * fix CreateCopy() of source raster with south-up orientation (#6946)

BAG driver:
 * conform to the final BAG georeferenced metadata layer specification (#6933)

ESRIC driver:
 * Fix DCAP_VECTOR metadata

JPEGXL driver:
 * advertise COMPRESSION_REVERSIBILITY=LOSSY when there is a JPEG
   reconstruction box

netCDF driver:
 * deal with files with decreasing latitudes/north-up orientation and presence
   of actual_range attribute (#6909)

VRT driver:
 * VRTSourcedRasterBand: replace potentially unsafe cpl::down_cast<> by
   dynamic_cast<>

## OGR 3.6.2

### Core

* OGRGenSQLResultsLayer::GetFeatureCount(): fix it to return -1 when base layer
  also returns -1 (#6925)
* OGRXercesInstrumentedMemoryManager::deallocate(): avoid (likely harmless)
  unsigned integer overflow in error code path.
* GPKG/SQLite dialect: fix issues when SQL statement provided to ExecuteSQL()
  starts with space/tabulation/newline (#6976)
* ArrowArray generic: FillBoolArray(): avoid out-of-bounds write access

### Utilities

* ogr2ogr: make -nln flag with GeoJSON output even if a name exists in input
  GeoJSON (#6920)
* ogr2ogr: silent reprojection errors related to IsPolarToWGS84() in
  OGRGeometryFactory::transformWithOptions()

### Drivers

GML driver:
 * fix recognizing WFS GetFeature response with a very long initial XML element
   (#6940)
 * default srsDimension to 3 for CityGML files (#6989)
 * fix incorrect behavior when using GFS_TEMPLATE open option with a .gfs that
   refers to FeatureProperty/FeaturePropertyList fields (#6932)

GeoPackage driver:
 * fix threaded RTree building when creating several layers (3.6.0 regression),
   by disabling async RTree building
 * avoid SQLite3 locking in CreateLayer() due to RemoveOGREmptyTable()

NGW driver:
 * remove DCAP_ items set to NO (#6994)

Parquet driver:
 * update to read and write GeoParquet 1.0.0-beta.1 specification (#6646)

Selafin driver:
 * Fix DCAP_VECTOR metadata

# GDAL/OGR 3.6.1 Release Notes

GDAL 3.6.1 is a bugfix release. It officially retracts GDAL 3.6.0 which
could cause corruption of the spatial index of GeoPackage files it created
(in tables with 100 000 features or more):
cf https://github.com/qgis/QGIS/issues/51188 and
https://github.com/OSGeo/gdal/pull/6911. GDAL 3.6.1 fixes that issue. Setting
OGR_GPKG_ALLOW_THREADED_RTREE=NO environment variable (at generation time)
also works around the issue with GDAL 3.6.0. Users who have generated corrupted
GeoPackage files with 3.6.0 can regnerate them with 3.6.1 with, for example,
"ogr2ogr out_ok.gpkg in_corrupted.gpkg" (assuming a GeoPackage file with vector
content only)

## Build

* Fix build with -DOGR_ENABLE_DRIVER_GML=OFF (#6647)
* Add build support for libhdf5 1.13.2 and 1.13.3 (#6657)
* remove RECOMMENDED flag to BRUNSLI and QB3. Add it for CURL (cf
  https://github.com/spack/spack/pull/33856#issue-1446090634)
* configure.cmake: fix wrong detection of pread64 for iOS
* FindSQLite3.cmake: add logic to invalidate SQLite3_HAS_ variables if
  the library changes
* detect if sqlite3 is missing mutex support
* Fix build when sqlite3_progress_handler() is missing
* do not use Armadillo if it lacks LAPACK support (such as on Alpine)
* make it a FATAL_ERROR if the user used -DGDAL_USE_ARMADILLO=ON and it
  can't be used
* Fix static HDF4 libraries not found on Windows
* Internal libjpeg: rename extra symbol for iOS compatibility (#6725)
* gdaldataset: fix false-positive gcc 12.2.1 -O2 warning about truncation
  of buffer
* Add minimal support for reading 12-bit JPEG images with libjpeg-turbo
  2.2dev and internal libjpeg12
* Fix detection of blosc version number
* Add missing includes to fix build with upcoming gcc 13

## GDAL 3.6.1

### Port

* CPLGetExecPath(): add MacOSX and FreeBSD implementations; prevent
  potential one-byte overflow on Linux&Windows
* /vsiaz/: make AppendBlob operation compatible of Azurite (#6759)
* /vsiaz/: accept Azure connection string with only BlobEndpoint and
  SharedAccessSignature (#6870)
* S3: fix issue with EC2 IDMSv2 request failing inside Docker container
  with default networking

### Algorithms

* warp: Also log number of chunks in warp operation progress debug logs (#6709)
* Warper: use exact coordinate transformer on source raster edges to avoid
  missing pixels (#6777)

### Utilities

* gdalbuildvrt: make -addalpha working when there's a mix of bands with or
  without alpha (#6794)
* gdalwarp: fix issue with vertical shift, in particular when CRS has US
  survey foot as vertical unit (#6839)
* gdalwarp: speed-up warping with cutline when the source dataset or
  processing chunks are fully contained within the cutline (#6905)
* validate_gpkg.py: make it work with SRID=-1 in geometry blobs

### Core

* GDALMDReader: avoid possible stack overflow on hostile XML metadata
  (ossfuzz #53988)

### Raster drivers

GeoRaster driver:
 * add internal OCI connection support to vsilocilob which is used only
    by the GeoRaster driver. (#6654)

GPKG driver:
 * implement setting the nodata value for Byte dataset (#1569)

GTiff driver:
 * DISCARD_LSB: reduce range of validity to 0-7 range for Byte to avoid
   unsigned integer overflow if 8. (ossfuzz #53570)
 * if CRS is DerivedProjected, write it to PAM .aux.xml file (#6758)
 * SRS reader: do not emit warning when reading a projected CRS with GeoTIFF
   keys override and northing, easting axis order (related to #6905)

netCDF driver:
 * fix exposing geotransform when there's x,y and lat,lon coordinates and
   the CRS is retrieved from crs_wkt attribute (#6656)

HDF4 driver:
 * fix regression of CMake builds, related to opening more than 32 simultaneous
   HDF4_EOS files (#6665)

OGCAPI driver:
 * update for map api; also for tiles but not working properly due to
   churn in tilematrixset spec (#6832)

RMF driver:
 * Implement GetSuggestedBlockAccessPattern

SAR_CEOS driver:
 * fix small memleak

XYZ driver:
 * support more datasets with rather sparse content in first lines (#6736)

## OGR 3.6.1

### Core

* OGRArrowArrayHelper::SetDate(): simplify implementation
* OGRSpatialReference::importFromWkt(): fix compatibility with PROJ master
  9.2.0dev for DerivedProjectedCRS
* OGR layer algebra: make sure result layer has no duplicated field names
  (#6851)

### Utilities

* ogr2ogr: densify points of spatial filter specified with -spat_srs to
  avoid reprojection artifacts
* ogr2ogr: discard features whose intersection with -clipsrc/-clipdst
  result in a lower dimensionality geometry than the target layer geometry
  type (#6836)
* ogr2ogr: add warning when -t_srs is ignored by drivers that
  automatically reproject to WGS 84 (#6859)
* ogr2ogr: make sure an error in GDALClose() of the output dataset result
  in a non-zero return code (https://github.com/Toblerity/Fiona/issues/1169)

### Vector drivers

CSV driver:
 * accept comma as decimal separator in X_POSSIBLE_NAMES, Y_POSSIBLE_NAMES
   and Z_POSSIBLE_NAMES fields

FileGDB driver:
 * avoid crash in the SDK if passing incompatible geometry type (#6836)

FlatGeoBuf driver:
 * speed-up writing of DateTime/Date values

GPKG driver:
 * fix corruption of spatial index on layers with >= 100 000 features,
   with the default background RTree building mechanism introduced in
   3.6.0 (https://github.com/qgis/QGIS/issues/51188, #6911) when flushing
   transactions while adding features (triggered by ogr2ogr). See announcement
   at top of release notes of this version.
 * avoid nullptr dereference on corrupted databases
 * add support for reading tables with generated columns (#6638)
 * fix bad performance of ST_Transform() by caching the
   OGRCoordinateTransformation object
 * improve multi-threaded implementation of GetNextArrowArray() on tables
   with FID without holes and when no filters are applied (full bulk
   loading)
 * FixupWrongRTreeTrigger(): make it work with table names that need to be
   quoted (https://github.com/georust/gdal/issues/235)
 * Fix opening /vsizip//path/to/my.zip/my.gpkg with NOLOCK=YES open option
 * speed-up writing of DateTime/Date values, and fix writing DateTime with
   milliseconds with a locale where the decimal point is not dot, and when
   spatialite is not loaded

MITAB driver:
 * add support for 'Philippine Reference System 1992' datum

MSSQLSPATIAL driver:
 * Get UID and PWD from configuration options (#6818)

OpenFileGDB driver:
 * do not use buggy .spx spatial index found in some datasets
   (geopandas/geopandas#2253)

Parquet driver:
 * make sure that ArrowLayer destructor is available (for plugin building)

PCIDSK driver:
 * advertise missing capabilities

PGDump driver:
 * Fix support for the TEMPORARY layer creation option

PostgreSQL driver:
 * avoid error when inserting single feature of FID 0 (#6728)
 * Fix support for the TEMPORARY layer creation option

SOSI driver:
 * do not advertise GDAL_DCAP_CREATE_FIELD as it is not implemented

SQLite driver:
 * Fix relationships determined through foreign keys have tables reversed
 * Use 'features' as related table type instead of 'feature' to match
   gpkg/filegdb

VDV driver:
 * make creation of temporary .gpkg files more robust on some platforms

WFS driver:
 * do not remove single or double quote character in a LIKE filter (also
   applies to CSW driver)

## SWIG bindings:
 * add gdal.GetNumCPUs() and gdal.GetUsablePhysicalRAM()

## CSharp bindings

* Default to dotnet 6 (#6843)

## Python bindings

* make Geometry.__str__() use ExportToIsoWkt() (#6842)
* setup.py: improve numpy fixing (#6700)
* add a 'python_generated_files' target that facilitate generation of bindings without building the lib

# GDAL/OGR 3.6.0 Release Notes

Those notes include changes since GDAL 3.5.0, but not already included in a GDAL 3.5.x bugfix release.

## In a nutshell...

* CMake is the only build system available in-tree. autoconf and nmake build systems have been removed
* OpenFileGDB: write and update support (v10.x format only), without requiring any external dependency, with same (and actually larger) functional scope as write side of the FileGDB driver
* [RFC 86](https://gdal.org/development/rfc/rfc86_column_oriented_api.html): Column-oriented read API for vector layers.
  Implemented in core, Arrow, Parquet, GPKG and FlatGeoBuf drivers
* Add read/write raster [JPEGXL driver](https://gdal.org/drivers/raster/jpegxl.html) for standalone JPEG-XL files. Requires libjxl
* Add KTX2 and BASISU read/write raster drivers for texture formats. Require (forked) basisu library
* Vector layer API: table relationship discovery & creation, Upsert() operation
* GeoTIFF: add multi-threaded read capabilities (reqiures NUM_THREADS open option or GDAL_NUM_THREADS configuration option to be set)
* Multiple performance improvements in GPKG driver
* ogr_layer_algebra.py: promoted to official script (#1581)
* Code linting and security fixes
* Bump of shared lib major version

## New optional dependencies

* libjxl: for JPEGXL driver (it was already a potential dependency in past versions, when using internal libtiff, to get the JXL TIFF codec)
* libarrow_dataset: for Parquet driver
* [QB3](https://github.com/lucianpls/QB3): for QB3 codec in MRF driver
* [basisu](https://github.com/rouault/basis_universal/tree/cmake): required for KTX2 and BASISU drivers

## New installed files

* bin/ogr_layer_algebra.py
* include/ogr_recordbatch.h

## Removed installed files

None

## Backward compatibility issues

See [MIGRATION_GUIDE.TXT](https://github.com/OSGeo/gdal/blob/release/3.6/MIGRATION_GUIDE.TXT)

## Build changes

Enhancements:
 * Add version suffix to DLL when compiling for MinGW target
 * Add a -DBUILD_WITHOUT_64BIT_OFFSET advanced option (#5941)
 * Add a USE_ALTERNATE_LINKER option
 * Build iso8211 library conditionally to drivers requiring it

Fixes:
 * Fix build without PNG (#5742) and JPEG (#5741)
 * Various changes for CHERI-extended architectures such CHERI-RISC-V or Arm Morello with sizeof(void*) == 16
 * FindMono.cmake: fix setting 'CSHARP_MONO_INTERPRETER_', to avoid having to run CMake twice
 * swig/python/CMakeLists.txt: fix SWIG_REGENERATE_PYTHON mode
 * FindNetCDF.cmake: fix when running on Ubuntu 16.04 regarding erroneous detection of netcdf_mem.h
 * Remove uses of std::regex (#6358)
 * honour CMAKE_INSTALL_RPATH for Python bindings, but only if it is an absolute path. (#6371)
 * Python: fix to allow building in ubuntu 18.04 (#6443)
 * Fixed building position independent static lib
 * Fix issues when building/installing in directories with spaces, at least on Unix.
 * fix LIBKML linking on Windows Conda
 * make sure to register EEDAI driver when built as a plugin
 * fix Win32 csharp build (#6620)

## Internal libraries

* flatbuffers: updated
* internal libtiff: resynchroinzation with upstream
* internal libpng: use __UINTPTR_TYPE__ for png_ptruint when available

## GDAL 3.6.0 - Overview of Changes

### Port

New features:
 * Add CPLIsASCII()
 * /vsis3/: Provide credentials mechanism for web identity token on AWS EKS (#4058)
 * /vsis3/: support source_profile in .aws/config pointing to a profile with a web_identity_token_file (#6320)
 * /vsistdin/: make size of buffered area configurable (#751)
 * add VSIIsLocal(), VSISupportsSequentialWrite() and VSISupportsRandomWrite()
 * Configuration file: add a ignore-env-vars=yes setting (#6326) in a \[general\] leading section
 * Add a cpl::ThreadSafeQueue<> class

Enhancements:
 * VSIFileFromMemBuffer(): allow anonymous files
 * CPLCheckForFile(): do not request file size
 * /vsicurl/: when CPL_CURL_VERBOSE is enabled, log as CPLDebug() message the error message from the server
 * /vsicurl / CPLHTTPFetch(): add GDAL_HTTP_HEADERS configuration option (#6230)
 * VSIVirtualHandle: add a PRead() method for thread-safe parallel read and implement it in /vsimem/ and Unix virtual file system
 * /vsis3/: make CPL_VSIS3_USE_BASE_RMDIR_RECURSIVE a path-specific configuration option
 * Make GDAL_DISABLE_READDIR_ON_OPEN a path-specific configuration option
 * Add VSISetPathSpecificOption() / VSIGetPathSpecificOption() / VSIClearPathSpecificOptions().
   Deprecate VSISetCredential() / VSIGetCredential() / VSIClearCredentials()
 * /vsicurl/ and other network file systems: add a DISABLE_READDIR_ON_OPEN=YES/NO VSIFOpenEx2L() option
 * Add a VSIFilesystemHandler::SupportsRead() method
 * Add GDAL_HTTP_TCP_KEEPALIVE/GDAL_HTTP_TCP_KEEPIDLE/GDAL_HTTP_TCP_KEEPINTVL configuration options to control TCP keep-alive functionality
 * Make CPLODBCSession and CPLODBCStatement member variables 'protected' (#6314)

Bugfixes:
 * cpl_config.h: Don't use __stdcall on MinGW
 * CPL recode: fix issues with iconv library integrated in musl C library
 * CPLWorkerThreadPool::SubmitJob(): avoid potential deadlock when called from worker thread
 * /vsicurl/: fix caching of first bytes of the files

### Core

New features:
 * Add a GDALRasterBand::GetSuggestedBlockAccessPattern() method, implement it in GTiff, JPEG, PNG, PDF drivers and use it in GDALCopyWholeRasterGetSwathSize().
 * GDALJP2Box/GDALJP2Metadata: add support for reading/writing JUMBF box
 * Add a GDALDeinterleave() function, to copy values from a pixel-interleave buffer to multiple per-component,
  and add SSE2/SSSE3 optimizations for a few common scenarios like Byte/UInt16 3/4 components.
  Use it in GTiff and MEM drivers.
 * C API change: make GDALComputeRasterMinMax() return CPLErr instead of void (#6300)
 * Add a GDAL_DMD_MULTIDIM_ARRAY_OPENOPTIONLIST constant

Enhancements:
 * GDALRasterBand::ComputeRasterMinMax(): add optimized implementation for Byte and UInt16 data types (~10 times faster)
 * Multidim API: significantly enhance performance of reading transposed arrays for netCDF/HDF5
 * GDALVersionInfo(): report if it is a debug build in --version output, and report compiler version in BUILD_INFO output
 * /vsicurl/: cache the result of several collections/URL signing requests to Planetary Computer
 * GDALCopyWholeRasterGetSwathSize(): aim for a chunk that is at least tall as the maximum of the source and target block heights
 * GDALGetJPEG2000Structure(): add JP2_BOXES, CODESTREAM_MARKERS, STOP_AT_SOD, ALLOW_GET_FILE_SIZE options
 * GDALDataset::BuildOverviews/IBuildOverviews(): add a CSLConstList papszOptions parameter
 * GDALDataset::CreateLayer(): honor GDAL_VALIDATE_CREATION_OPTIONS (#6487)

Breaking changes:
 * Remove use of compatibility wrappers _GetProjectionRef / _GetGCPProjection / _SetProjection / _SetGCPs (#6186)

Bugfixes:
 * EXIFCreate(): fix writing of EXIF_UserComment
 * GDALPamDataset::TrySaveXML(): do not set error if a subdataset name is set but the .aux.xml doesn't exist (#5790)
 * GDALOpen(): make recursive opening of dataset more reliable when papszAllowedDrivers is passed
 * GetHistogram(): Support 64 bit images (#6059)
 * GetHistogram(): deal with undefined behavior when raster values are at infinity, or with pathological min/max bounds
 * GDALPamRasterBand::SetOffset()/SetScale(): set the bOffsetSet/bScaleSet even if the value provided is the default offset/scale
 * GDALDataset/GDALRasterBand::BuildOverviews/IBuildOverviews(): fix const correctness of panOverviewList and panBandList arguments
 * Overview: tighten GAUSS and MODE to be exactly those names, and not starting with them
 * Overview building: fix MODE resampling on large datasets (#6587)

### Algorithms

Enhancements:
 * Transformer: add SRC_GEOLOC_ARRAY and DST_GEOLOC_ARRAY transformer options
 * GDALChecksumImage(): make it return -1 in case of error

### Utilities

New features:
 * gdalinfo: add a STAC section to `gdalinfo -json` output (#6265)
 * gdal_translate: add a -ovr <level|AUTO|AUTO-n|NONE> flag (#1923)
 * gdal2tiles.py: add WEBP support with --tiledriver option
 * gdalmdiminfo & gdalmdimtranslate: add -if (input format) flag (#6295)
 * gdal_grid: add 'radius' parameter to invdist, nearest, averge and metrics algorithm, to set radius1 and radius2 at the same time
 * gdal_grid: add per-quadrant search capabilities for invdistnn, average, and metrics algorithms
 * ogr_layer_algebra.py: promoted to official script (#1581)

Enhancements:
 * gdal2tiles.py: short circuit overview tile creation for --resume ahead of processing base tiles
 * gdal2tiles.py: refactor transparent file check in overview creation
 * gdalsrsinfo: use wkt2_2019 name instead of wkt2_2018
 * gdal_viewshed: use -cc 1.0 as default for non-Earth CRS (#6278)
 * nearblack: skip erosion when pixel at edge is valid
 * gdal_grid: produce north-up images
 * gdal_grid: add validation of algorithm parameters and warn when a unknown parameter is specified
 * gdal_grid: add a nSizeOfStructure leading structure member in GDALGridXXXXOptions structure, as a way to detect ABI issues when adding new parameters

Bugfixes:
 * gdalwarp: modify 'sum' resampling to preserve total sum
 * gdalwarp: fix issue with wrong resolution when reprojecting between geographic CRS with source extent slightly off [-180,180]
 * gdalwarp: fix artifacts around antimeridian for average/mode/min/max/med/q1/q3/sum/rms resampling (#6478)
 * gdal2tiles.py: remove PIL deprecation warning by replacing ANTIALIAS with LANCZOS
 * gdal2tiles: allow oversampling in -p raster mode (fixes #6207)
 * gdal2xyz.py: fix parsing of -b option (#5984)
 * gdal_rasterize: fix ALL_TOUCHED on polygons whose boundaries coordinates are aligned on pixels (#6414)

### gdal_utils package

* standardized return codes (#5561). Return 2 when utilities called without argumen

### Raster drivers

ADRG driver:
 * add SRP pixel spacing value (SRP_PSP) to the dataset metadata

COG driver:
 * add a OVERVIEW_COUNT creation option to control the number of overview levels (#6566)
 * add DMD_EXTENSIONS metadata item (#6073)
 * properly set lossy WEBP compression when QUALITY_OVERVIEW < 100 but QUALITY = 100 (#6550)

COSAR driver:
 * handle version 2 files that contain half-foat samples (#6289)

ECRGTOC driver:
 * fix error on RasterIO() when GDAL_FORCE_CACHING=YES is set

ECW driver:
 * strip off boring Kakadu and OpenJPEG COM marker comments

ENVI driver:
 * implement 'default bands' to read/write R,G,B and gray color interpretation (#6339)
 * implement Get/Set Scale/Offset from ENVI 'data gain values'/'data offset values' (#6444)
 * use OGRSpatialReference::FindBestMatch() on reading to find a matching known CRS (#6453)

GPKG driver:
 * in CreateCopy() mode for Byte data, save the band count in a IMAGE_STRUCTURE metadata domain to be able to re-open the file with the appropriate number of bands
 * default to PNG storage for single band dataset (qgis/QGIS#40425)
 * writer: write fully set tiles as soon as possible to decrease pressure on block cache

GTiff driver:
 * add multi-threaded read capabilities (reqiures NUM_THREADS open option or GDAL_NUM_THREADS configuration option to be set)
 * JXL codec: support more than 4 bands in INTERLEAVE=PIXEL mode (#5704)
 * JXL codec: preserve Alpha color interpretation when the Alpha band does not immediately follow color bands (e.g. R,G,B,undefined,Alpha), and fix decoding of such files
 * add a WEBP_LOSSLESS_OVERVIEW=YES/NO configuration option (#6439)
 * report a COMPRESSION_REVERSIBILITY=LOSSLESS (possibly)/LOSSY metadata item in IMAGE_STRUCTURE for WEBP and JXL compression
 * read/write JPEGXL and WEBP compression parameters (for main dataset only) in IMAGE_STRUCTURE metadata domain of GDAL_METADATA tag
 * avoid potential crash on creation in a disk full situation
 * fix reading a CompoundCRS of a LocalCS/EngineeringCS, and avoid warnings on writing (#5890)
 * report codec name (or code) when opening a file with a unhandled code
 * WEBP: avoid unnecessary temporary buffer creation and copy (most of changes are in libtiff itself)
 * force INTERLEAVE=PIXEL for internal overviews when using WEBP compression
 * avoid SetMetadata() to cancel effect of SetGeoTransform() (#6015)
 * refuse to open files with SampleFormat=IEEEFP and BitsPerSample != 16, 24, 32 or 64
 * SRS import: better deal when angular unit of the GEOGCS[] of the PROJCS[] doesn't match the one from the database
 * SRS export: avoid error when exporting a Projected 3D CRS (#6362)
 * honour COMPRESS_OVERVIEW and INTERLEAVE_OVERVIEW for internal overviews (#6344)
 * CreateCopy(): fix marking alpha channels that are not the last one (#6395)

HDF5 driver:
 * multidim: fix crash on 'gdalmdiminfo HDF5:autotest/gdrivers/data/netcdf/alldatatypes.nc'

JP2KAK driver:
 * use kdu_multi_analysis class for tile encoding, instead of very low level kdu_analysis
 * use kdu_stripe_compressor whenever the required buffer size is < CACHE_MAX / 4, otherwise fallback to kdu_multi_analysis
 * add (at least build) support for versions down to 7.3

All JPEG2000 drivers:
 * report a COMPRESSION_REVERSIBILITY=LOSSLESS/LOSSLESS (possibly)/LOSSY metadata item in IMAGE_STRUCTURE domain

JP2OpenJPEG driver:
 * for reversible compression, write a hint in the COM marker if the compression is lossy or not, and use it on reading

JPEG-XL driver:
 * NEW!
 * The JPEG-XL format is supported for reading, and batch writing (CreateCopy()), but not update in place.
  The driver supports reading and writing:
    - georeferencing: encoded as a GeoJP2 UUID box within a JUMBF box.
    - XMP in the xml:XMP metadata domain
    - EXIF in the EXIF metadata domain
    - color profile in the COLOR_PROFILE metadata domain.

KEA driver:
 * add support for 64 bit nodata functions

MRF driver:
 * Add QB3 compression (#5824)

netCDF driver:
 * handle variables of type NC_SHORT with _Unsigned=true as GDT_UInt16 (#6352)
 * do not report metadata of indexing variables of dimensions not used by the variable of interest (#6367)
 * fix 2 issues with netCDF 4.9.0 of msys2-mingw64 (#5970)
 * multidim: workaround crash with using same file in 2 different threads (each thread with its own dataset object) (#6253)
 * ignore 'missing_value' when it is a non-numeric string
 * multidim: use 'fill_value' attribute as an alternative for nodata, and add a USE_DEFAULT_FILL_AS_NODATA=YES array open option
 * allow NETCDF:"/vsicurl_streaming/http[s]://example.com/foo.nc":variable_name (#6610)

NITF driver:
 * do not put PAM metadata in a Subdataset node of .aux.xml file if there's a single dataset (3.4.0 regression) (#5790)
 * avoid excessive memory allocation on broken files (ossfuzz#52642)
 ù fix crash when reading all metadata from a file without image segment, and allow creating such file
 * add support for writing a TRE_OVERFLOW DES
 * nitf_spec.xml: lower minlength for CSEPHA

PDF driver:
 * avoid PROJ error when reading a CRS with a EPSG code that is actually a ESRI one (#6522)

PNG driver:
 * report cause when unable to create file

RMF driver:
 * backup error state before min-max computation at FlushCache
 * Ext header size checks improved

VRT driver:
 * optimize speed of statistics and minmax computation when the VRT is a mosaics of non-overlapping simple sources
 * ComputeStatistics(): for mosaicing case, enable it to be multi-threaded if GDAL_NUM_THREADS is set
 * take into account open options when sharing sources (#5989)

WEBP driver:
 * report a COMPRESSION_REVERSIBILITY=LOSSLESS/LOSSY metadata item in IMAGE_STRUCTURE

## OGR 3.6.0 - Overview of Changes

### Core

New features:
 * OGRLayer: add Arrow C stream based batch retrieval (RFC 86)
 * Add OGRLayer::Upsert() operation support (#6199). Implement it in MongoDBv3, ElasticSearch MEM, GPKG drivers
 * Add OGR_G_ConcaveHull(), using GEOS >= 3.11 GEOSConcaveHull_r(), and map it to SWIG
 * Add a OGRLayer::AlterGeomFieldDefn() / OGR_L_AlterGeomFieldDefn() to change geometry field definitions. Implement in MEM, Shapefile, GPKG, PG, OpenFileGDB drivers
 * Add GDALRelationship class for describing a relationship between two tables,
   and related API for retrieving the relationship
   names and relationships in a dataset.
   Implement discovery in FileGDB, OpenFileGDB, PGeo and GPKG drivers, SQLite
 * Add API for relationship creation/deletion/update.
   Implement in OpenFileGDB driver
 * Add OGRLayer::GetGeometryTypes(). This method iterates over features to retrieve their geometry types.
   This is mostly useful for layers that report a wkbUnknown geometry type.
   Specialized implementation in GPKG and PG drivers.
 * Add a GDAL_DMD_ALTER_GEOM_FIELD_DEFN_FLAGS driver metadata item
 * Add DCAP_CREATE_LAYER for drivers which have support for layer creation
 * Add DCAP_DELETE_LAYER for drivers which have support for layer deletion
 * Add DCAP_DELETE_FIELD for drivers which have support for field deletion
 * Add DCAP_REORDER_FIELDS for drivers which have support for field reordering
 * Add GDAL_DMD_ALTER_FIELD_DEFN_FLAGS for drivers which describe the flags supported for a driver by the AlterFieldDefn API
 * Add DCAP_CURVE_GEOMETRIES for drivers which support curved geometries
 * Add DCAP_MEASURED_GEOMETRIES for drivers which support measured geometries
 * Add driver capability for DCAP_Z_GEOMETRIES
 * Add OLCZGeometries (equivalent to OLCMeasuredGeometries for Z support)
 * Add ODsCZGeometries datasource capability flag
 * Add driver metadata for DMD_GEOMETRY_FLAGS. Contains a list of (space separated) flags which reflect the geometry handling behavior of a driver.
   Supported values are currently "EquatesMultiAndSingleLineStringDuringWrite", "EquatesMultiAndSinglePolygonDuringWrite".
 * Add OGRParseDateTimeYYYYMMDDTHHMMSSZ() and OGRParseDateTimeYYYYMMDDTHHMMSSsssZ()
 * Add GDAL_DMD_SUPPORTED_SQL_DIALECTS driver metadata.

Enhancements:
 * Make isClockwise() available at the OGRCurve level
 * Export OSRStripVertical() function in C API
 * OGRSimpleCurve point iterator: make its modification instant on the parent curve (#6215)

Bugfixes:
 * OGRFeature::FillUnsetWithDefault(): do not set driver-specific default values on unset numeric fields
 * OGR_SM_InitStyleString(): make it work with a @style_name argument (#5555)
 * Fix loss of split/merge policy when cloning field domains
 * OGRSQL: fix GetFeature() to return a feature such that GetFeature(fid).GetFID() == fid (#5967)
 * OGRGeometry::UnionCascaded(): avoid crash with GEOS < 3.11 on empty multipolygon input

### OGRSpatialReference

New features:
 * Add a OGRSpatialReference::FindBestMatch() method

Enhancements:
 * Warping/coordinate transformation performance improvements
 * OSRGetProjTLSContext(): make it faster on Linux by saving getpid() system call
 * OGRSpatialReference::SetFromUserInput(): allow using strings like EPSG:3157+4617 where the 'vertical CRS' is actually the geographic CRS, to mean ellipsoidal height, which is supported in recent PROJ versions
 * Improve OGRCoordinateTransformation::TransformBounds error handling (#6081)
 * OGRSpatialReference: evaluate OSR_DEFAULT_AXIS_MAPPING_STRATEGY config option at each object construction (#6084)

Bugfixes:
 * Avoid issues with PROJJSON with id in members of datum ensemble
 * OGRSpatialReference::GetTargetLinearUnits(): fix getting linear units from a CompoundCRS of a LocalCS/EngineeringCS (#5890)

### Utilities

New features:
 * ogr2ogr: add -upsert option

Bugfixes:
 * ogr2ogr: make sure geometry column name is going through laundering when outputting to PG/PGDump (#6261)
 * ogr2ogr: take into account -limit when -progress is used

### Vector drivers

All drivers:
 * Add some missing DCAP_VECTOR capabilities to drivers

Arrow/Parquet drivers:
 * implement faster SetAttributeFilter() for simpler filters.
   Things like "col =/!=/>/>=/</<= constant", "col IS NULL", "col IS NOT NULL", possibly combined with AND.

CSV driver:
 * make AUTODETECT_SIZE_LIMIT=0 open option to scan the whole file, including beyond 2 GB (for non-streaming input) (#5885)
 * fix width autodetection

DXF driver:
 * Support files between 2 GB and 4 GB in size
 * Prevent buffer from sometimes splitting CRLF newlines in MLEADER entities

FlatGeoBuf driver:
 * make CreateLayer() to fail if output file cannot be
 created

FileGDB driver:
 * handle Shape_Area/Shape_Length fields on reading/writing
 * avoid crash when reading layer with AliasName with XML special characters (issue with embedded libxml2 in SDK), and fallback to OpenFileGDB driver to reliably retrieve it (#5841)
 * Report relationships

GeoJSONSeq driver:
 * add support for appending features to an existing file (#2080)

GML driver:
 * make FORCE_SRS_DETECTION=YES open option work with multiple geometry columns (#6392)
 * read <gml:description>, <gml:identifier>, <gml:name> fields in a feature (qgis/QGIS#42660)
 * OGRMergeGeometryTypesEx(): do not consider different type of MultiGeometries (ie MultiPoint, MultiLineString, MultiPolygon) as being mergeable as GeometryCollections (#6616)

GMLAS driver:
 * be robust to GML schemas being pointed to a location different from http://schemas.ogc.net/

GPKG driver:
 * Do not list layers referenced in gpkg_contents but that have no corresponding table (qgis/qgis#30670)
 * Performance improvement in reading features
 * Performance improvement in reading DateTime fields
 * Performance improvement: do not request ignored fields
 * Micro optimizations to improve CreateFeature() speed
 * Performance improvement: implement background RTree creation in bulk insertion into a new table
 * Implement a fast ST_Area() method
 * optimization to remove bbox filtering when the spatial filter is larger than the layer extent
 * remove code path specific to SQLite < 3.7.8 (PROJ requires SQLite >= 3.11)
 * avoid integer overflow when trying to insert strings larger > 2 GB
 * preliminary non-user-visible support for Related Tables Extension
 * fix issue with ST_MakeValid() when the SQLite driver runs before GPKG on Alpine Linux
 * Report relationships, through FOREIGN KEY constraints, and Related Tables extension.
 * properly update gpkg_ogr_contents on INSERT OR REPLACE statements
 * do not warn about http://ngageoint.github.io/GeoPackage/docs/extensions extensions in read-only mode
 * Rename layer: take into account QGIS layer_styles extension
 * add compatibility with GPKG 1.0 gpkg_data_column_constraints table

GRIB driver:
 * fix crash and invalid metadata when processing index .idx file with sub-messages (#6613)

HANA driver:
 * pending batches are not flushed when layer is destroyed
 * reset prepared statements when creating new field
 * fix transaction support
 * execute pending batches from other operations
 * properly handle special characters in connection string

LIBKML driver:
 * writer: add automatic reprojection to EPSG:4326 (#6495)

MITAB driver:
 * implements writing Text objects for Point geometries with LABEL style string (#6149)

ODS driver:
 * make it possible to open file without .ods extension if prefixed with ODS: (#6375)

OpenFileGDB driver:
 * Add write support
 * handle Shape_Area/Shape_Length fields on reading
 * fix use of indexes on strings when the searched value is longer than the max indexed string, or ending with space
 * Report relationships

Parquet driver:
 * add basic support for reading partitionned datasets
 * add CREATOR option
 * do not write statistics for WKB geometry columns
 * make sure 'geo' metadata is embedded in ARROW:schema so that partitioned reading works fine
 * implement SetNextByIndex()
 * make it honour GDAL_NUM_THREADS, and assume min(4, ALL_CPUS) as default value

PG driver:
 * make GEOM_TYPE layer creation option be taken into account by CreateGeomField() (instead of always assuming geometry)

PGDump driver:
 * avoid extraneous harmless spaces in CREATE TABLE statements

S57 driver:
 * resource files: fix missing punctuation (#6000)

Selafin driver:
 * remove likely broken logic in handing /vsigzip/foo.gz filenames

SQLite driver:
 * Report relationships, through FOREIGN KEY constraints
 * SQLiteVFS: fix semantics of xOpen(SQLITE_OPEN_CREATE) that could cause to wrongly truncate an attached database

VFK driver:
 * add support for UTF-8 (VFK 6.0 switched from ISO-8859-2 to UTF-8)

XLSX driver:
 * make it possible to open file without .xlsx extension if prefixed with XLSX: (#6375)
 * improve detection to recognize even if no XLSX: prefix or .xlsx extension

## SWIG Language Bindings

All bindings:
 * Add SWIG bindings for OGR_L_AlterGeomFieldDefn()
 * fix SpatialReference.GetLinearUnitsName() to use OSRGetLinearUnits() to retrieve the name
 * add SpatialReference.StripVertical()
 * Create alias versions with/without GDAL_ prefix for c#/java constants
 * add inverseCT optional parameter to CoordinateOperation.SetOperation(), and add CoordinateOperation.GetInverse()
 * make Band.ComputeStatistics() kwargs
 * add options argument to Dataset.BuildOverviews()
 * fix GDT_TypeCount value (affects C# and Java bindings)

Python bindings:
 * bindings for Arrow Batch functionality
 * add numpy to extras_require option of setup.py
 * add an optional can_return_none=True parameter to Band.ComputeRasteMinMax() to make it return None in case of error. Otherwise, return (nan, nan) (#6300)

# GDAL/OGR 3.6.4 Release Notes

GDAL 3.6.4 is a bugfix release.

## GDAL 3.6.4

### Port

* userfaultfd: avoid it to stall on 32bit and test real working of syscall in
  CPLIsUserFaultMappingSupported()

### Core

* RawRasterBand::FlushCache(): avoid crash in some situations
* RawRasterBand::IRasterIO(): fix wrong byte swapping in Direct IO multiline
  writing code path
* RawRasterBand::IRasterIO(): fix optimized code path that wrongly triggered
  on BIL layout
* RawRasterBand::IRasterIO(): avoid reading and writing too many bytes
* RawRasterBand::IRasterIO(): fix floating-point issues with ICC that could
  result in wrong lines/cols being read/written

### Algorithms

* Rasterize all touched: tighten(decrease) the tolerance to consider that edge
  of geometries match pixel obundaries (#7523)

### Utilities

* gdal_translate: fix crash when specifying -ovr on a dataset that has no
  overviews (#7376)
* gdalcompare.py: correctly take into account NaN nodata value (#7394)
* gdal2xyz.py: fix -srcnodata and -dstnodata options (#7410)
* gdal2tiles: update 'ol-layerswitcher' widget to v4.1.1 (#7544)

### Raster drivers

GTiff driver:
 * correctly read GCPs from ArcGIS 9 .aux.xml when TIFFTAG_RESOLUTIONUNIT=3
  (pixels/cm) (#7484)

HDF5 driver:
 * fix detecting if HDF5 library is thread-safe (refs #7340)

LCP driver:
 * CreateCopy(): fix crash on negative pixel values (#7561)

MRF driver:
 * restore SetSpatialRef() that was wrongly deleted in 3.6.0

netCDF driver:
 * restore capability of reading CF-1.6-featureType vector layers even if the
   conventions >= CF 1.8, and improve featureType=trajectory by adding the
   time attribute (fixes #7550)

## OGR 3.6.4

### Core

* OGRSQL: fix 'SELECT ... WHERE ... AND ... AND ... AND ... UNION ALL ...'
  (#3395)
* OGRUnionLayer::GetExtent(): do not emit error on no-geometry layer
* OGREditableLayer::IUpsertFeature(): fix memleak

### OGRSpatialReference

* Fix OGRSpatialReference::SetProjCS() on an existing BoundCRS;
  affects GeoTIFF SRS reader (fixes gdal-dev/2023-March/057011.html)

### Utilities

* ogr2ogr: fix and automate conversion from list types to String(JSON) when the
  output driver doesn't support list types but String(JSON) (#7397)

### Vector drivers

CSV driver:
 * CSVSplitLine(): do not treat in a special way double quotes that appear in
   the middle of a field

FlatGeobuf driver:
 * improve handling of null geoms (#7483)

GeoPackage driver:
 * Update definition of gpkg_data_columns to remove unique constraint on "name"

OpenFileGDB driver:
 * fix write corruption when re-using freespace slots in some editing scenarios
   (#7504)
 * relax test to detect broken .spx
 * CreateField(): in approxOK mode, do not error out if default value of a
   DateTime field is CURRENT_TIMESTAMP, just ignore it with a warning (#7589)

OSM driver:
 * Fix handling of closed_ways_are_polygons setting in osmconf.ini (#7488)

S57 driver:
 * s57objectclasses.csv: apply S-57 Edition 3.1 Supplement No. 2

SQLite driver:
 * GDAL as a SQLite3 loadable extension: avoid crash on Linux

# GDAL/OGR 3.6.3 Release Notes

GDAL 3.6.3 is a bugfix release.

## Build

* CMake: Fix integration of find_package2()
* CMake: avoid HDF4 CMake error with Windows paths with spaces
* CMake: quote variables for INTERFACE_INCLUDE_DIRECTORIES / IMPORTED_LOCATION
* CMake: fix wrong test when GDAL_SET_INSTALL_RELATIVE_RPATH is set
* CMake: issue an error when the user explicitly asks for a condition-dependent
  driver and the condition is not met
* CMake: add include to FindSQLite3.cmake
* fix uclibc build without NPTL
* zlib: Add ZLIB_IS_STATIC build option
* FindCryptoPP.cmake: properly take into account _LIBRARY_RELEASE/_DEBUG (#7338)
* FindPoppler.cmake: check that Poppler private headers are available (#7352)

## GDAL 3.6.3

### Port

* CPLGetPhysicalRAM(): take into account current cgroup (v1 and v2)
* CPLGetPhysicalRAM(): take into account MemTotal limit from /proc/meminfo
* /vsicurl/: fix CPL_VSIL_CURL_USE_HEAD=NO mode (#7150)
* Avoid use of deprecated ZSTD_getDecompressedSize() function with libzstd 1.3+
* cpl_vsil_crypt.cpp: fix build isse on Windows (#7304)

### Algorithms

* GDALPolygonizeT(): add sanity check
* GDALRasterPolygonEnumeratorT::NewPolygon(): check memory allocation to avoid
  crash (#7027)
* Warper: do not use OpenCL code path when pafUnifiedSrcDensity is not null
  (#7192)
* Warper: optimize a bit when warping a chunk fully within the cutline
* Geoloc inverse transformer: fix numeric instability when quadrilaterals are
  degenerate to triangles (#7302)

### Core

* GDALProxyPoolRasterBand::FlushCache(): fix for ref counting when calling
  FlushCache() on GDALProxyPoolMaskBand or GDALProxyPoolOverviewRasterBand
* VirtualMem: Fix mremap() detection with clang 15, and disable
  HAVE_VIRTUAL_MEM_VMA if HAVE_5ARGS_MREMAP not found

### Utilities

* gdal_translate: make -colorinterp work on a source band that is a mask band
* gdalmdimtranslate: do not require VRT driver to be registered (#7021)
* gdalmdimtranslate: fix subsetting in the situation of dataset of #7199
* gdalwarp: fix vshift mode when vertical unit of dstSrs is non-metric
* gdalwarp: overview choice: fix longitude wrap problem (#7019)
* gdalwarp: allow up to inaccuracy in cropline coordinates up to 0.1% of a
  pixel size for rounding (#7226)
* gdalsrsinfo: fix crash on 'gdalsrsinfo IAU:2015:49902 -o xml'
* gdal_retile.py: fix wrong basename for .aux.xml files (#7120)
* gdallocationinfo: fix issue with VRTComplexSource and nodata (#7183)
* gdal_rasterize: ignore features whose Z attribute is null/unset (#7241)

### Raster drivers

BMP driver:
 * Make sure file is created at proper size if only calling Create() without
   writing pixels (#7025)
 * Create(): add checks and warnings for maximum dimensions

COG driver:
 * avoid warning message when using -co COMPRESS=WEBP -co QUALITY=100 (#7153)

DIMAP driver:
 * optimize performance of dataset RasterIO()

GRIB driver:
 * fix reading South Polar Stereographic from GRIB1 datasets (#7298)
 * degrib: replace use of sprintf() with snprintf()

GTiff driver:
 * GTiffJPEGOverviewBand::IReadBlock(): remove hack that causes read errors in
   some circumstances
 * do not use implicit JPEG overviews with non-nearest resampling
 * fix generation of external overviews on 1xsmall_height rasters (#7194)

GTX driver:
 * fix (likely harmless in practice) integer overflow (ossfuzz#55718)

HDF5 driver:
 * add a GDAL_ENABLE_HDF5_GLOBAL_LOCK build option to add a global lock when
   the HDF5 library is not built with thread-safety enabled (#7340)

HFA driver:
 * ERDAS Imagine SRS support: various fixes: Vertical Perspective projection,
   LCC_1SP, Mercator_2SP, Eqirectanglar, Hotine Obliqe Mercator Azimuth Center

JPEG driver:
 * Correctly read GCPS when an .aux.xml sidecar has GeodataXform present in the
   ESRI metadata element instead of root element

JPEGXL driver:
 * CreateCopy(): fix memory leak when writing georeferencing

MBTiles driver:
 * fix nullptr deref when calling GetMetadata() on a dataset returned by
   Create() (#7067)

netCDF driver:
 * quote variable name in subdataset list if it contains a column character
   (#7061)
 * report GEOLOCATION metadata for a lon/lat indexed variable where lon and/or
   lat has irregular spacing
 * netCDFDimension::GetIndexingVariable(): be more restrictive
 * resolve variable names beyond the parent group (#7325)

NITF driver:
 * update CLEVEL to appropriate values when using compression / multiple image
   segments
 * fix bug that prevents adding subsequent TREs after a HEX TRE (#6827)

PDF driver:
 * skip JP2ECW driver if ECW_ENCODE_KEY required but not found

TileDB driver:
 * fix compatibility with tiledb 2.14

VRT:
 * warp: fix issue when warping a Float32 raster with nodata = +/- FLOAT_MAX

ZMap creation:
 * fix potential truncation of nodata value (#7203)

## OGR 3.6.3

### Core

* OGRSQL: fix crash when comparing integer array fields (#6714)
* OGRSQL: fix SetAttributeFilter() when dialect=OGRSQL and not forwarding the
  initial where clause to the source layer (#7087)

### Utilities

* ogr2ogr: fix -clipsrc/-clipdst when clip dataset has SRS != features's
  geometry (#7126)

### Vector drivers

GeoJSON driver:
 * avoid duplication of FID in streaming parser (#7258)
 * declare GDAL_DCAP_MEASURED_GEOMETRIES and ODsCMeasuredGeometries
 * fix mixed type field not flagged as JSON if first is a string (#7313)
 * writer: take into account COORDINATE_PRECISION for top bbox (#7319)
 * writer: fix json mixed types roundtrip (#7368)

GeoJSONSeq driver:
 * fix writing to /vsigzip/ (#7130)

GeoPackage driver:
 * avoid issue with duplicated column names in some cases (#6976)
 * GetNextArrowArray(): fix retrieving only the geometry (geopandas/pyogrio#212)
 * restore async RTree building for 1st layer (broken by GDAL 3.6.2)

GML driver:
 * fix CurvePolygon export of CompoundCurve and CircularString child elements
   (#7294)

HANA driver:
 * fix DSN open option

MITAB driver:
 * handle projection methods 34 (extended transverse mercator) and 35 (Hotine
   Oblique Mercator) (#7161)
 * Fix possible crash on NULL feature text (#7341)
 * Fix a typo at MITABGetCustomDatum

NAS driver:
 * fix file descriptor leak in error code path

OpenFileGDB driver:
 * fix performance issue when identifying a CRS
 * detect broken .spx file with wrong index depth (qgis/qgis#32534)
 * index reading: avoid integer overflow on index larger than 2 GB
 * allow CreateField() with OBJECTID as the column name (qgis/qgis#51435)
 * make Delete() method to remove the directory (fixes #7216)

Shapefile driver:
 * fix adding features in a .dbf without columns (qgis/qgis#51247)
 * make sure eAccess = GA_Update is set on creation (#7311)

## SWIG bindings

* add missing OLCUpsertFeature constant

## Python bindings

* fix setup.py dir-list issue on macOS

# GDAL/OGR 3.6.2 Release Notes

GDAL 3.6.2 is a bugfix release.

## General

[RFC69](https://gdal.org/development/rfc/rfc69_cplusplus_formatting.html):
Whole code base C/C++ reformatting

## Build

* Avoid warning with curl >= 7.55 about CURLINFO_CONTENT_LENGTH_DOWNLOAD being
  deprecated
* Avoid warning with curl >= 7.87 about CURLOPT_PROGRESSFUNCTION being
  deprecated
* fix nitfdump build against external libtiff (#6968)
* fix compilation with gcc 4.8.5 of Centos 7.9 (#6991)

## Data files

* tms_MapML_CBMTILE.json: fix wrong matrixWidth value (#6922)

## GDAL 3.6.2

### Port

* CPLGetUsablePhysicalRAM(): take into account RSS limit (ulimit -m) (#6669)
* CPLGetNumCPUs(): take into sched_getaffinity() (#6669)

### Algorithms

* Warp: fix crash in multi-threaded mode when doing several warping runs with
  the same WarpOperation
* RasterizeLayer: prevent out-of-bounds index/crash on some input data (#6981)

### Core

* gdal_pam.h: workaround for code including it after windows.h

### Raster drivers

AAIGRID driver:
 * fix CreateCopy() of source raster with south-up orientation (#6946)

BAG driver:
 * conform to the final BAG georeferenced metadata layer specification (#6933)

ESRIC driver:
 * Fix DCAP_VECTOR metadata

JPEGXL driver:
 * advertise COMPRESSION_REVERSIBILITY=LOSSY when there is a JPEG
   reconstruction box

netCDF driver:
 * deal with files with decreasing latitudes/north-up orientation and presence
   of actual_range attribute (#6909)

VRT driver:
 * VRTSourcedRasterBand: replace potentially unsafe cpl::down_cast<> by
   dynamic_cast<>

## OGR 3.6.2

### Core

* OGRGenSQLResultsLayer::GetFeatureCount(): fix it to return -1 when base layer
  also returns -1 (#6925)
* OGRXercesInstrumentedMemoryManager::deallocate(): avoid (likely harmless)
  unsigned integer overflow in error code path.
* GPKG/SQLite dialect: fix issues when SQL statement provided to ExecuteSQL()
  starts with space/tabulation/newline (#6976)
* ArrowArray generic: FillBoolArray(): avoid out-of-bounds write access

### Utilities

* ogr2ogr: make -nln flag with GeoJSON output even if a name exists in input
  GeoJSON (#6920)
* ogr2ogr: silent reprojection errors related to IsPolarToWGS84() in
  OGRGeometryFactory::transformWithOptions()

### Drivers

GML driver:
 * fix recognizing WFS GetFeature response with a very long initial XML element
   (#6940)
 * default srsDimension to 3 for CityGML files (#6989)
 * fix incorrect behavior when using GFS_TEMPLATE open option with a .gfs that
   refers to FeatureProperty/FeaturePropertyList fields (#6932)

GeoPackage driver:
 * fix threaded RTree building when creating several layers (3.6.0 regression),
   by disabling async RTree building
 * avoid SQLite3 locking in CreateLayer() due to RemoveOGREmptyTable()

NGW driver:
 * remove DCAP_ items set to NO (#6994)

Parquet driver:
 * update to read and write GeoParquet 1.0.0-beta.1 specification (#6646)

Selafin driver:
 * Fix DCAP_VECTOR metadata

# GDAL/OGR 3.6.1 Release Notes

GDAL 3.6.1 is a bugfix release. It officially retracts GDAL 3.6.0 which
could cause corruption of the spatial index of GeoPackage files it created
(in tables with 100 000 features or more):
cf https://github.com/qgis/QGIS/issues/51188 and
https://github.com/OSGeo/gdal/pull/6911. GDAL 3.6.1 fixes that issue. Setting
OGR_GPKG_ALLOW_THREADED_RTREE=NO environment variable (at generation time)
also works around the issue with GDAL 3.6.0. Users who have generated corrupted
GeoPackage files with 3.6.0 can regnerate them with 3.6.1 with, for example,
"ogr2ogr out_ok.gpkg in_corrupted.gpkg" (assuming a GeoPackage file with vector
content only)

## Build

* Fix build with -DOGR_ENABLE_DRIVER_GML=OFF (#6647)
* Add build support for libhdf5 1.13.2 and 1.13.3 (#6657)
* remove RECOMMENDED flag to BRUNSLI and QB3. Add it for CURL (cf
  https://github.com/spack/spack/pull/33856#issue-1446090634)
* configure.cmake: fix wrong detection of pread64 for iOS
* FindSQLite3.cmake: add logic to invalidate SQLite3_HAS_ variables if
  the library changes
* detect if sqlite3 is missing mutex support
* Fix build when sqlite3_progress_handler() is missing
* do not use Armadillo if it lacks LAPACK support (such as on Alpine)
* make it a FATAL_ERROR if the user used -DGDAL_USE_ARMADILLO=ON and it
  can't be used
* Fix static HDF4 libraries not found on Windows
* Internal libjpeg: rename extra symbol for iOS compatibility (#6725)
* gdaldataset: fix false-positive gcc 12.2.1 -O2 warning about truncation
  of buffer
* Add minimal support for reading 12-bit JPEG images with libjpeg-turbo
  2.2dev and internal libjpeg12
* Fix detection of blosc version number
* Add missing includes to fix build with upcoming gcc 13

## GDAL 3.6.1

### Port

* CPLGetExecPath(): add MacOSX and FreeBSD implementations; prevent
  potential one-byte overflow on Linux&Windows
* /vsiaz/: make AppendBlob operation compatible of Azurite (#6759)
* /vsiaz/: accept Azure connection string with only BlobEndpoint and
  SharedAccessSignature (#6870)
* S3: fix issue with EC2 IDMSv2 request failing inside Docker container
  with default networking

### Algorithms

* warp: Also log number of chunks in warp operation progress debug logs (#6709)
* Warper: use exact coordinate transformer on source raster edges to avoid
  missing pixels (#6777)

### Utilities

* gdalbuildvrt: make -addalpha working when there's a mix of bands with or
  without alpha (#6794)
* gdalwarp: fix issue with vertical shift, in particular when CRS has US
  survey foot as vertical unit (#6839)
* gdalwarp: speed-up warping with cutline when the source dataset or
  processing chunks are fully contained within the cutline (#6905)
* validate_gpkg.py: make it work with SRID=-1 in geometry blobs

### Core

* GDALMDReader: avoid possible stack overflow on hostile XML metadata
  (ossfuzz #53988)

### Raster drivers

GeoRaster driver:
 * add internal OCI connection support to vsilocilob which is used only
    by the GeoRaster driver. (#6654)

GPKG driver:
 * implement setting the nodata value for Byte dataset (#1569)

GTiff driver:
 * DISCARD_LSB: reduce range of validity to 0-7 range for Byte to avoid
   unsigned integer overflow if 8. (ossfuzz #53570)
 * if CRS is DerivedProjected, write it to PAM .aux.xml file (#6758)
 * SRS reader: do not emit warning when reading a projected CRS with GeoTIFF
   keys override and northing, easting axis order (related to #6905)

netCDF driver:
 * fix exposing geotransform when there's x,y and lat,lon coordinates and
   the CRS is retrieved from crs_wkt attribute (#6656)

HDF4 driver:
 * fix regression of CMake builds, related to opening more than 32 simultaneous
   HDF4_EOS files (#6665)

OGCAPI driver:
 * update for map api; also for tiles but not working properly due to
   churn in tilematrixset spec (#6832)

RMF driver:
 * Implement GetSuggestedBlockAccessPattern

SAR_CEOS driver:
 * fix small memleak

XYZ driver:
 * support more datasets with rather sparse content in first lines (#6736)

## OGR 3.6.1

### Core

* OGRArrowArrayHelper::SetDate(): simplify implementation
* OGRSpatialReference::importFromWkt(): fix compatibility with PROJ master
  9.2.0dev for DerivedProjectedCRS
* OGR layer algebra: make sure result layer has no duplicated field names
  (#6851)

### Utilities

* ogr2ogr: densify points of spatial filter specified with -spat_srs to
  avoid reprojection artifacts
* ogr2ogr: discard features whose intersection with -clipsrc/-clipdst
  result in a lower dimensionality geometry than the target layer geometry
  type (#6836)
* ogr2ogr: add warning when -t_srs is ignored by drivers that
  automatically reproject to WGS 84 (#6859)
* ogr2ogr: make sure an error in GDALClose() of the output dataset result
  in a non-zero return code (https://github.com/Toblerity/Fiona/issues/1169)

### Vector drivers

CSV driver:
 * accept comma as decimal separator in X_POSSIBLE_NAMES, Y_POSSIBLE_NAMES
   and Z_POSSIBLE_NAMES fields

FileGDB driver:
 * avoid crash in the SDK if passing incompatible geometry type (#6836)

FlatGeoBuf driver:
 * speed-up writing of DateTime/Date values

GPKG driver:
 * fix corruption of spatial index on layers with >= 100 000 features,
   with the default background RTree building mechanism introduced in
   3.6.0 (https://github.com/qgis/QGIS/issues/51188, #6911) when flushing
   transactions while adding features (triggered by ogr2ogr). See announcement
   at top of release notes of this version.
 * avoid nullptr dereference on corrupted databases
 * add support for reading tables with generated columns (#6638)
 * fix bad performance of ST_Transform() by caching the
   OGRCoordinateTransformation object
 * improve multi-threaded implementation of GetNextArrowArray() on tables
   with FID without holes and when no filters are applied (full bulk
   loading)
 * FixupWrongRTreeTrigger(): make it work with table names that need to be
   quoted (https://github.com/georust/gdal/issues/235)
 * Fix opening /vsizip//path/to/my.zip/my.gpkg with NOLOCK=YES open option
 * speed-up writing of DateTime/Date values, and fix writing DateTime with
   milliseconds with a locale where the decimal point is not dot, and when
   spatialite is not loaded

MITAB driver:
 * add support for 'Philippine Reference System 1992' datum

MSSQLSPATIAL driver:
 * Get UID and PWD from configuration options (#6818)

OpenFileGDB driver:
 * do not use buggy .spx spatial index found in some datasets
   (geopandas/geopandas#2253)

Parquet driver:
 * make sure that ArrowLayer destructor is available (for plugin building)

PCIDSK driver:
 * advertise missing capabilities

PGDump driver:
 * Fix support for the TEMPORARY layer creation option

PostgreSQL driver:
 * avoid error when inserting single feature of FID 0 (#6728)
 * Fix support for the TEMPORARY layer creation option

SOSI driver:
 * do not advertise GDAL_DCAP_CREATE_FIELD as it is not implemented

SQLite driver:
 * Fix relationships determined through foreign keys have tables reversed
 * Use 'features' as related table type instead of 'feature' to match
   gpkg/filegdb

VDV driver:
 * make creation of temporary .gpkg files more robust on some platforms

WFS driver:
 * do not remove single or double quote character in a LIKE filter (also
   applies to CSW driver)

## SWIG bindings:
 * add gdal.GetNumCPUs() and gdal.GetUsablePhysicalRAM()

## CSharp bindings

* Default to dotnet 6 (#6843)

## Python bindings

* make Geometry.__str__() use ExportToIsoWkt() (#6842)
* setup.py: improve numpy fixing (#6700)
* add a 'python_generated_files' target that facilitate generation of bindings without building the lib

# GDAL/OGR 3.5.0 Release Notes

## In a nutshell...

* [RFC 84](https://gdal.org/development/rfc/rfc84_cmake.html):
  Addition of a CMake build system, which deprecates the existing
  autoconf/automake and nmake build systems, that will be removed
  in GDAL 3.6.0. Users are encouraged to adopt the new CMake build system.
  Documentation of the CMake build system is at
  https://gdal.org/build_hints.html.
* Add GDT_Int64 and GDT_UInt64 data types and handle them in MEM, GTiff, netCDF and Zarr drivers
* Add read/write OGR Parquet (Apache Parquet) and 'Arrow' (Apache Arrow IPC File/Feather or stream) drivers. Only in CMake builds
* Add OGR HANA database driver. Only in autoconf & cmake builds
* Removed drivers: RDA, JPEG2000 (Jasper-based), CharLS, MG4 LIDAR, FujiBAS, IDA, INGR, ARCGEN, ArcObjects, CouchDB, Cloudant, DB2, FME, Geomedia, MDB (Java Jackess based), GTM, Ingres, MongoDB (old one. MongoDBv3 is the one to use now), REC, Walk, GMT raster, DODS raster and vector
* GDAL and OGR GRASS drivers moved to https://github.com/OSGeo/gdal-grass repository
* Tiger: remove deprecated write side of the driver (#4216)
* Remove deprecated SWIG Perl bindings
* Code linting and security fixes
* Bump of shared lib major version

## New optional dependencies

* odbc-cpp-wrapper (https://github.com/SAP/odbc-cpp-wrapper): for SAP Hana driver
* Apache arrow-cpp (https://github.com/apache/arrow/tree/master/cpp) libraries: for Parquet and Arrow drivers

## New installed files

* lib/gdalplugins/drivers.ini: list of (known) drivers in the order they must be registered, for deterministic behavior with plugins
* shared/gdal/grib2_*.csv: resource files for GRIB driver

## Removed files

* Remove deprecated testepsg utility (#3993)

## Backward compatibility issues

See [MIGRATION_GUIDE.TXT](https://github.com/OSGeo/gdal/blob/release/3.5/MIGRATION_GUIDE.TXT)

## Build changes

Build(all):
 * Drop support for non-reentrant external libqhull
 * support libhdf5 1.13.0 (#5061)
 * Support latest Poppler versions (requires C++17)
 * Support tiledb >= 2.7 (requires C++17)
 * Updates for IJG libjpeg-9e
 * Require using https://github.com/rouault/pdfium_build_gdal_3_5 for PDF PDFium support
 * cpl_config.h: remove lots of unused defines, and severely restrict what we export in non-GDAL compilation mode

Build(autoconf/automake):
 * fix detection of OpenEXR >= 3 (#4766)
 * Add support for PCRE2 (to replace deprecated PCRE) (#4822)
 * Add support for external libqhull_r (#4040)
 * add a --with-qhull-pkgname=qhull_r/qhullstatic_r option to select with qhull package to use
 * generate test_ogrsf by default (but not installed)
 * change default of RENAME_INTERNAL_LIBTIFF/LIBGEOTIFF/SHAPELIB_SYMBOLS to yes
 * when building against internal libjpeg, prefix by default libjpeg symbols with gdal_ (#4948)
 * when building against internal libpng, prefix internal libpng symbols with gdal_ (#5303)
 * m4/acinclude.m4: fix detection of fseeko/ftello on netBSD
 * detect xlocale.h to use LC_NUMERIC_MASK on Mac (#5022)
 * move generated headers to a generated_headers subdirectory to allow in a same git checkout, to continue to do autoconf in-source-tree builds as well as cmake out-of-source-tree build

Build(nmake):
 * nmake.opt: fix wrong variable name in example MSODBCSQL_LIB
 * nmake.opt: add a HAVE_ATLBASE_H variable that can be set to NO

## Internal libraries

* Internal zlib: update to 1.2.12 (#5587)
* Internal libtiff: resync with upstream
* Internal libgeotiff: resync with upstream
* Internal libpng: fix memleak on corrupted file (ossfuzz #44486)
* Internal libjson: update to 0.15.0
* Internal libLerc: Prevent LERC out of bounds access (#5598)
* Internal libqhull: update to qhull_r 2020.2 (8.0.2)

## GDAL 3.5.0 - Overview of Changes

### Port

* CPLRecode(): fix recoding between UTF-8 and CP_ACP/CP_OEMCP on Windows build that have iconv support
* CPLRecodeIconv(): avoid potential unsigned integer overflow (ossfuzz#41201)
* Fix out of bounds read in CPLRecodeFromWCharIconV() (#5542)
* Add VSISetCredential() to set /vsis3, /vsigs, /vsiaz ... credentials for a given file prefix
* VSICurl: Print response code for failed range requests
* /vsis3/: allow setting AWS_PROFILE to a profile that uses IAM role assumption
* IVSIS3LikeFSHandler::CopyFile(): always take into account ret code of CopyObject()
* VSISync() onto /vsis3/: allow x-amz- headers to be specified for object creation
* /vsis3/: ignore object class DEEP_ARCHIVE in addition to GLACIER, and add CPL_VSIL_CURL_IGNORE_STORAGE_CLASSES to configure which object classes should be ignored
* /vsis3/: make sure file properties of /vsis3_streaming/foo are invalidated when /vsis3/foo ones are
* /vsis3/ with GDAL_DISABLE_READDIR_ON_OPEN: do not hide when accessing non existing file (#1900)
* VSISync(): fix sync'ing from /vsis3/, /vsigs/, /vsiaz/ to local disk, when the source contains implicit directories
* /vsiaz/: read credentials from ~/.azure/config as an additional fallback method. Add AZURE_STORAGE_SAS_TOKEN configuration option and deprecate AZURE_SAS
*  /vsiaz/: allow authorization through an access token specified with the AZURE_STORAGE_ACCESS_TOKEN config option (to be used with AZURE_STORAGE_ACCOUNT)
* /vsiaz/: fix handling of BlobEndpoint in connection string, and fix signing of requests to handle a directory part in the endpoint
* /vsiaz/: implement container creation/destruction with VSIMkdir()/VSIRmdir()
* /vsiaz/: add compatibility with Azurite emulator
* /vsiadls/: add missing call to InvalidateParentDirectory()
* /vsigs/: Support type=user JSON file for authentication
* /vsigs/: fix upload of files > 4 MB in HTTP 1.1 (#5267)
* /vsigs/, /vsiaz/, /vsiadls/: allow x-goog- / x-ms-/ headers to be specified for object creation
* ZIP support: avoid warnings on MacOS
* CPLZLibInflate(): workaround issue with /opt/intel/oneapi/intelpython/latest/lib/libz.so.1 from intel/oneapi-basekit Docker image
* /vsizip/: fix Eof() detection on stored (ie not compressed) files inside the zip (#5468)
* /vsitar/: fix reading .tar.gz files when the size of the uncompressed .tar file is a multiple of 65536 bytes (#5225)
* CPLGetCurrentDir(): use _wgetcwd() on Windows to get a UTF-8 filename
* CSLTokenizeString2(): make it work with strings > 2 GB
* CPLLoadConfigOptionsFromFile(): add a \[credentials\] section to load VSI credentials
* /vsimem/: VSIFTruncateL(): make sure to zeroize beyond the truncated area

### Core

* Embedded Python: fixes to load Conda and mingw64 python on Windows
* Embedded python: list Python 3.10
* GDALDriver::QuietDelete(): take into account papszAllowedDrivers argument
* GDALDataset::MarkSuppressOnClose(): delete auxiliary files (#4791)
* GDALPamRasterBand::CloneInfo(): deal correctly with NaN nodata to avoid generating useless .aux.xml file (#4847)
* External overviews: automatically turn PLANARCONFIG_CONTIG for WebP overviews
* GDALDatasetCopyWholeRaster(): clarify INTERLEAVE option meaning and raises warning if invalid value is specified (#4909)
* CreateCopy(): propagate INTERLEAVE from source data (#4911)
* Add GDALPamDataset::DeleteGeoTransform() (#4877)
* Metadata readers: remove thread-unsafe use of localtime()
* Pleiades metadata reader: fix to handle RPC for Pleiades Neo (#5090)
* GeoEye metadata reader: avoid potential write stack-buffer overflow on long basename files (#5506)
* Overview generation: fix Cubic resampling on boundaries between valid and transparent areas (#5016)
* make GDALProxyRasterBand::RefUnderlyingRasterBand() / UnrefUnderlyingRasterBand() const. May affect out-of-tree drivers
* Add GDALRasterBand::IsMaskBand() and GetMaskValueRange() virtual methods
* Avoid CPLError() on number of drivers when PAM is disabled
* RawRasterBand::AccessBlock(): do not early return in case of truncated block

### Algorithms

* Geoloc transformer: fix inverse transform to be exact (#5520)
* Geoloc transformer: make it usable with arbitrary large geolocation arrays, using temporary GTiff storage (#5520)
* gdal_grid: add facility to search points included in search ellipse for GDALDataMetrics (minimum, maximum, range, count, average_distance, average_distance_pts) in the already implemented quadtree data structure (#5530)
* Rasterization of polygons: avoid underflow/overflow of output data type
* Rasterize: use rounding to integers of floating-point value, and handle +/- inf for floating point rasters
* Rasterize: make it possible to burn a Int64 attribute into a Int64 raster in a lossless way
* Warp kernel: avoid writing outside allocated buffer if more threads than needed are allocated
* warper: better guess output bounds when warping from a rotated pole projection that include poles
* transformer: implement DST_METHOD=GEOLOC_ARRAY
* avoid using any resampling kernel when doing just a subsetting of the source image (alignment on pixel boundaries) (#5345)
* GDALBuildVRT(): in -separate mode, add support for sources such as MEM dataset or non-materialized VRT files

### Utilities

* gdalinfo --build: report PROJ version of build and runtime
* gdaladdo -clean: remove overviews of mask (#1047)
* gdalwarp -crop_to_cutline: relax epsilon to avoid extremely long loop (#4826)
* gdalwarp: do not emit "Point outside of projection domain" / "tolerance condition error" (#4934)
* gdalwarp: avoid useless use of CHECK_WITH_INVERT_PROJ when computed bounds are at the 'edges' of the geographic domain
* gdal_translate/gdalwarp: do not delete DIM_/RPC_ auxiliary XML files on converting a jp2 file to a tif file (#5633)
* gdalbuildvrt: change logic to check homogeneous number of bands (#5136)
* gdalsrsinfo: emit message when replacement of deprecated CRS occurs, with hint to set OSR_USE_NON_DEPRECATED=NO configuration option to avoid that (#5106)
* gdalsrsinfo: return a non-zero exit code when specified SRS fails to load (#5201)
* gdalbuildvrt: add a -strict/-non_strict flag, and in strict mode consider non-existing datasets as a failure (#4755)
* gdal_pansharpen.py: display usage if not enough filenames provided
* gdal_polygonize.py: remove use of Unicode double quote characters
* gdal_polygonize.py: make -8 switch work again (#5000)
* gdal_sieve.py: fix exception when source dataset has no nodata value (3.4.0 regression) (#4899)
* gdal_calc.py: support wildcard for filenames, and use 3D arrays instead of list for multiple filenames per alpha
* gdal_calc.py: raise error when overwriting behavior is apparently wished but not specified (#5270)
* gdal2tiles: make --no-kml option takes effect (#4940)
* gdal2tiles: XML escape input filename (#5032)
* gdal2tiles: remove/fix broken links in generated files
* gdal2tiles: implement parallel generation of overview tiles (#5052)
* gdal2tiles: lower chunksize value to allow parallel processing when generating ~ 100 tiles
* gdal2tiles: fix issue with multiprocessing and the gdal2tiles launcher script on Windows and Python >= 3.8 (#4951)
* gdal2tiles: support mpi4py multi-node parallelism (#5309)
* gdal2tiles: detect write error when creating tiles, and allow outputting to /vsi filesystems (#3382, #5370)
* gdal2tiles: do not generate .aux.xml files on overview tiles
* bash-completion: fixes and update gdal-bash-completion.sh

### gdal_utils package

* Make scripts executable
* Add entry points for gdal-utils package (#5281)

### Raster drivers

AAIGRID driver:
 * add support for 'null' as can be generated by D12 software (#5095)

BAG driver:
 * fix 'too many refinement grids' error (#3759)

BMP driver:
 * harden identify checks to avoid misidentification of other datasets (#4713)

BYN driver:
 * remove validation of nTideSys and nPtType fields

COG driver:
 * only create RGB JPEG with mask if 4-band is alpha (#4853)
 * fix potential generation failure when main imagery has overview and mask none
 * output exactly square pixels when using -co TILING_SCHEME (#5343)
 * add a ZOOM_LEVEL creation option (#5532)

DIMAP driver:
 * avoid warning when extracting metadata from unsupported band_id
 * register metadata on all 6 PNEO bands
 * validate raster dimensions to avoid further issues (ossfuzz #46762)

ECW driver:
 * fix test failures with ECW 5.5
 * do not try to open UInt32 JPEG2000 if the SDK is buggy (#3860)
 * fix non-nearest upsampling on multi-band datasets (#5288)
 * Added source read by swath to improve encoder performance

ERS driver:
 * Add support for comments in ERS files (#4835)

ESRIC driver:
 * Fix bundle file name

FITS driver:
 * fix non-conformant use of &v[0] that crashes with clang 14 -O2

GeoRaster driver:
 * fix build without JPEG

GRIB driver:
 * GRIB2: mode degrib hard-coded tables to .csv files in resource files
 * GRIB2: merge content from WMO tables at https://github.com/wmo-im/GRIB2 with DEGRIB ones
 * fix writing negative longitude of natural origin for Transverse Mercator, and fix reading it for TMerc, LCC, ACEA and LAEA
 * fix thread-safetey of errSprintf() (#4830)
 * avoid read heap buffer overflow due to inappropriate split-and-swap on dataset with weird georeferencing (#5290, ossfuzz #41260, ossfuzz #41637)
 * consider longitudes that slightly exceed 360° as 360° for Split&Swap mode (#5496)
 * degrib: partial resynchroinzation with degrib 2.25 (degrib-20200921)
 * fix use of uninitialized memory on some datasets (#5290)
 * multidim: fix crash when a .idx file is present (#5569)

GTiff driver:
 * explicitly enable strip choping to avoid issues with cmake libtiff builds
 * JXL codec: use non-deprecated methods and update for compatibility with latest state of libjxl
 * only emit warnings on libgeotiff PROJ errors (#4801)
 * SRS reading: add warning when CRS definition from geokeys is inconsistent with EPSG, and a GTIFF_SRS_SOURCE=EPSG/GEOKEYS configuration option to alter the SRS (#5399)
 * avoid warning with >= 5 bands and JPEG compression
 * LERC overview related improvements (#4848): MAX_Z_ERROR_OVERVIEW, ZLEVEL_OVERVIEW, ZSTD_LEVEL_OVERVIEW configuration options added
 * propagate SPARSE_OK to overviews (#4932), and add SPARSE_OK_OVERVIEW configuration option.
 * fix performance issue when reading transfer functions (#4923)
 * avoid huge memory allocation when generating overviews on large single-band 1-bit tiled files (#4932)
 * make SetGCPs(), SetGeoTransform(), SetSpatialRef(), SetNoDataValue(), SetMetadata(Item)() write to PAM .aux.xml on read only files (#4877)
 * add support for reading/writing color table from/into PAM .aux.xml (#4897)
 * do not warn about buggy Sentinel1 geotiff files use a wrong 4326 code for the ellipsoid
 * fix DISCARD_LSB with nodata value (#5097)
 * GTIFWktFromMemBufEx / GTIFMemBufFromSRS: use OSRSetPROJSearchPaths()  (#5184). Affects GeoJP2 encoding/decoding
 * fix exposing WEBP_LOSSLESS option
 * early checks for PREDICTOR settings, and update internal libtiff to support PREDICTOR=2 for 64-bit samples (rasterio/rasterio#2384)
 * remove limitation to 32,000 bytes when writing the GDAL metadata tag (#4116)
 * Create(): better detection of threshold when to switch to BigTIFF for tiled images (#5479)
 * unset geotransform from non-PAM source if PAM defines GCPs, and PAM is the priority source
 * fix crash when building overviews and computing approx stats (#5580)

HDF5 driver:
 * fix issue when netCDF and/or HDF5 drivers built as plugins with multidim datasets
 * detect Matlab .mat HDF5-based, or other files with HDF5 superblock at offset 512

HFA driver:
 * Fix "Pulkovo 1942" datum write to IMG files

JP2KAK driver:
 * add support for reading/writing Int32/UInt32 data types
 * Add support for Creversible & RATE creation option  (#5131)

JP2OpenJPEG driver:
 * add a STRICT=YES/NO open option to allow decoding some broken files (requires OpenJPEG 2.5)

JPEG driver:
 * make sure that max memory usage check is done in all code paths that require it

KEA driver:
 * print error message when opening of kea file fails

MRF driver:
 * Fix padding space logic (#5096)
 * Add LERC2 padding when encoding
 * Adjust PNG limits (#5347)
 * Allow using external libLerc (#5386)

MSG driver:
 * fix/workaround MSVC warnings

netCDF driver:
 * handle 'crs_wkt' attribute
 * always use WKT when found, without comparing with CF params (#4725)
 * limit SetFromUserInput() use to non file input
 * disable filename recoding to ANSI on Windows for netCDF >= 4.8
 * add WRITE_GDAL_VERSION and WRITE_GDAL_HISTORY creation option
 * add a VARIABLES_AS_BANDS=YES/NO open option
 * allow update mode of raster datasets
 * implement SetMetadataItem()/SetMetadata()
 * avoid warnings when CreateCopy() a non-georeferenced dataset, and opening a 1x1 non-georeferenced dataset
 * add a IGNORE_XY_AXIS_NAME_CHECKS=YES open option (qgis/QGIS#47158)
 * recognize x/y axis from GMT generated files as geospatial axis (#5291, qgis/QGIS#47158, qgis/QGIS#45704)
 * read CF attributes giving CRS component names (#5493)

NITF driver:
 * Add ISO-8859-1 decoding for file and image header metadata
 * avoid PROJ error to be emitted in Create() when ICORDS=N/S (#5563)
 * CADRG polar zone! CRS definition aligned with ADRG and SRP (#5656)
 * RPF.toc: skip plausibility check for Overviews and Legends; disable some checks for polar zones (#5654)

PCIDSK driver:
 * fix write heap-buffer-overflow (ossfuzz #41993)

PDS4 driver:
 * write conformant Equirectangular when input raster is a geographic CRS

PDF driver:
 * Named NEATLINE extraction from ISO32000 style Geospatial PDF (#5504)

RRASTER driver:
 * add support for CRS WKT2 (#5473)

SAGA driver:
 * implement SetNoDataValue() (#5147)

SENTINEL2 driver:
 * identify zipped S2 datasets from file content (#5505)

TGA driver:
 * fix reading images with runs crossing scanlines (#5168)

TileDB driver:
 * fix crash when creating array from subdatasets fails
 * fix handling of relative paths on Windows
 * avoid warnings about deprecated functions

USGSDEM driver:
 * fix reading datasets with 1025 byte records ending with linefeed (#5007)

VICAR driver:
 * avoid undefined behavior on empty container (ossfuzz #46650)

VRT driver:
 * Add div, polar, exp pixel functions. Improvements in sum, mul, inv (#5298)
 * pixel function: implements metadata, and replace_nodata and scale builtin functions (#5371)
 * Warped VRT: detect inconsistent block size between dataset and bands (#4714)
 * Warped VRT: do not serialize block size at band level, since already serialized at dataset level (#4714)
 * Warped VRT: advertise INTERLEAVE=PIXEL for faster processing of multiband
 * Warped VRT: fix issue with blocks without sources and alpha band (#4997)
 * VRTWarpedDataset::ProcessBlock(): fix issue in the unlikely situation where a block would be > 4 GB
 * Warped VRT opening: do not open the source dataset with GDAL_OF_SHARED
 * fix serialization of relativeToVRT=1 when mix of relative and absolute paths for source and VRT (#4983)
 * GDALAutoCreateWarpedVRTEx(): avoid potential crash in case of error
 * allow setting VRT description after CreateCopy('', src_ds)
 * fix serialization of relativeToVRT=1 when mix of relative and absolute paths for source and VRT
 * ComputeSourceWindow(): round source coordinates when very close (#5343)

WMTS driver:
 * disable clipping with TileMatrixSetLimits by default when using layer extent and add open options to control that behavior (#4461)

XYZ driver:
 * Fix incorrect failure to open ASCII-file due to floating point comparison

Zarr driver:
 * be robust to duplicated array and group names in NCZarr metadata (ossfuzz #40949)
 * fix nullptr dereference on array with zero-dim and Fortran order (ossfuzz #46717)

## OGR 3.5.0 - Overview of Changes

### Core

New features:
* Add layer renaming capacity, and implement it in GPKG, PG, FileGDB and Shapefile drivers
* Add GDALDataset::GetFieldDomainNames method to retrieve all field domain names which are available for a dataset. Implemented in OpenFileGDB, FileGDB, GPKG drivers
* Add GDALDataset::DeleteFieldDomain for deleting an existing field domain. Implemented in MEM driver
* Add GDALDataset::UpdateFieldDomain for replaciong the definition of an existing field domain. Implemented in MEM driver
* Add driver metadata for GDAL_DCAP_FIELD_DOMAINS and GDAL_DMD_CREATION_FIELD_DOMAIN_TYPES
* Add OGR_F_StealGeometryEx to steal more than the 1st geometry of a feature (#5115)
* OGRFeature: add fast/unsafe field getters/setters
* OGREnvelope: add == and != comparison operators
* OGRFeature: add Reset() method

Bug fixes:
* OGRSimpleCurve::segmentize(): avoid too many memory reallocation (#4826)
* OGR SQLite dialect: avoid being confused by an attribute and geometry field of same name
* OGRFormatFloat(): handle nan and inf
* OGRFeature::Equal(): fix when a Real or RealList field contains NaN
* OGRCurvePolygon::checkRing(): make it accept non-closed rings by default (#5488)
* OGRCreateFromShapeBin(): do not report M if all NaN coordinates
* OGRGeometryFactory::forceTo(): fix dimensionality with empty geometries
* Multipoint WKT import: accept MULTIPOINT Z in non-backeted mode, like what PostGIS outputs
* OGR_F_SetFieldRaw() / OGRFeature::SetField( const char*, const OGRField *): fix const correctness of OGRField argument

### OGRSpatialReference

* OGRSpatialReference::GetName(): workaround a PROJ 8.2.0 bug for BoundCRS
* add a OGR_CT_PREFER_OFFICIAL_SRS_DEF config option (fixes https://github.com/OSGeo/PROJ/issues/2955)
* OGRSpatialReference::importFromEPSG(): document OSR_USE_NON_DEPRECATED=NO configuration option (#5106)
* OGRSpatialReference::SetFromUserInput(): make it work with 'IAU:XXXX' and 'IAU:2015:XXXX' (when using PROJ >= 8.2)
* OGRSpatialReference::SetFromUserInput(): do not emit error message on unrecognized string when ALLOW_FILE_ACCESS=NO (Toblerity/Fiona#1063)
* Add a OSR_DEFAULT_AXIS_MAPPING_STRATEGY configuration option
* OGRProjCT: add missing copying of epoch related fields in copy constructor
* Add OGRSpatialReference::GetOGCURN()

### Utilities

* ogr2ogr: propagate error in final CommitTransaction() to status code of the utility (#5054)
* ogr2ogr: make detection of FID layer creation option more robust
* ogr2ogr: fix cutting of geometries in projected coordinates intersecting antimeridian, in some configurations
* ogr2ogr: avoid (in-memory) copying of source feature to target feature when possible
* ogr2ogr: make 'ogr2ogr [-f PostgreSQL] PG:dbname=.... source [srclayer] -lco OVERWRITE=YES' work as if -overwrite was specified (#5640)

### Vector drivers

CAD driver:
 * libopencad: fix crash on corrupted datasets (ossfuzz #45943, ossfuzz #46887, osffuzz #45962)

CSV driver:
 * when the .csvt indicates WKT and in default KEEP_GEOM_COLUMNS=YES mode, prefix the geometry field name with 'geom_'

DXF driver:
 * Do not copy final DXF when MarkSuppressOnClose was called.
 * Fix long line handling in edge case

DWG driver:
 * Add block attributes to entities layer (#5013)
 * support AcDbFace reading (#5034)
 * read block attributes as feature fields (#5055)
 * add DWG_ALL_ATTRIBUTES to get all attributes (#5103)

DWG/DGNv8 driver:
 * avoid potential crash with ODA 2022 on Linux

ElasticSearch driver:
 * do not try to open reserved .geoip_databases to avoid error message with recent Elastic versions
 * add a AGGREGATION open option (#4962)
 * support GetLayerByName() with multiple layers and/or wildcard (#4903)

ESRIJSON driver:
 * fix dimensionality of PolygonZ

FileGDB driver:
 * add support for writing field domains
 * implement database compaction on 'REPACK' SQL command
 * workaround a crash involving binary field and CDF datasets
 * fix crash related to feature of FID 2147483647

FlatGeoBuf driver:
 * If CRS WKT detected to be non UTF-8, force it to ASCII (#5062)
 * catch exception on GetFeature() on corrupted index
 * fix GetFeature() when featuresCount != 0 and indexNodeSize == 0
 * in SPATIAL_INDEX=NO mode, write a meaningful featureCount and extent in the header

GeoJSON driver:
 * reader: expose EPSG:4979 as CRS when none is specified and 3D geoms are found
 * use OGRSpatialReference::GetOGCURN() to handle compound CRS when writing 'crs' member
 * writer: in RFC7946=YES mode, reproject to EPSG:4979 if source CRS is 3D
 * report OFSTJSON subfield type for properties we can't map to a native OGR type, and better handling of mixed content in properties (#3882)

GMLAS driver:
 * be robust to leading spaces in <gml:coordinates> element (#5494)

GMT driver:
 * allow writing to /vsistdout/ (#4993)

GPKG driver:
 * fix nullptr dereference on corrupted databases with sqlite >= 3.35
 * deal explicitly with CPL_VSIL_USE_TEMP_FILE_FOR_RANDOM_WRITE=YES for /vsi network file systems (#5031)
 * when adding a 'epoch' column to gpkg_spatial_ref_sys, one must use the 'gpkg_crs_wkt_1_1' extension instead of 'gpkg_crs_wkt'
 * DeleteField(): use ALTER TABLE ... DROP COLUMN if sqlite >= 3.35.5, and run foreign_key_check only if foreign_keys is ON (fixes qgis/qgis#47012)
 * AlterFieldDefn(): use ALTER TABLE ... RENAME COLUMN when only renaming is to be done and sqlite >= 3.26.0, and do not run integrity_check in that situation (fixes qgis/qgis#47012)
 * intercept 'ALTER TABLE table RENAME COLUMN src_name TO dst_name' and 'ALTER TABLE table DROP COLUMN col_name'
 * add a NOLOCK=YES option to open a file without any lock (for read-only access) (helps fixing qgis/qgis#2399)
 * add ST_MakeValid() SQL function if not linking against Spatialite or if Spatialite lacks ST_MakeValid()

GPSBabel driver:
 * Allow identifying tcx (gtrnctr) files directly

ILI driver:
 * IMDReader: fix various potential crashes on invalid input

MapInfo driver:
 * allow reading MID/MIF files with lines up to 1 million bytes (#3943)
 * fix parsing .mid files with newline character in string field (#4789)
 * .tab writing: correctly detect datum when creating a layer from a WKT2 CRS string (#5217)
 * Add GSK2011 and PZ90.11 to list of ellipsoids (#5541)
 * add WindowsBalticRim / CP1257 charset mapping (#5608)

MSSQL driver:
 * fix build warnings on Linux
 * GetLayerDefn(): fix potential memory leak
 * DeleteLayer(): fix read after free
 * fix issue when inserting strings in bulk copy mode on Linux
 * make GetNextFeature() end bulk copy mode
 * CreateFeature in bulk copy mode: make it use feature geometry SRID when set
 * disable bulk mode when a UUID field is found, as this doesn't work currently
 * do not set field width when reading smallint/int/bigint/float/real columns, and correctly roundtrip smallint/Int16 (#3345)

MVT driver:
 * writing: using MakeValid() when possible on polygon output

ODS driver:
 * avoid huge memory allocations on files abusing repeated cells (ossfuzz #40568)
 * avoid crashing 'floating-point exception' when evaluating -2147483648 % -1 (ossfuzz #41541)

OpenFileGDB driver:
 * correctly parse raster fields of type == 2 which are inlined binary content (#4881)
 * do not report extent with NaN values
 * fix crash related to feature of FID 2147483647
 * fix dimensionality of MULTILINESTRING M geometries
 * add support for reading (non-default) UTF16 encoding for strings

OSM driver:
 * add a railway attributes to lines (#5141)

PGeo driver:
 * Always request numeric column values as numeric types (#4697)

PostgreSQL driver:
 * skip all leading whitespace in SQL statements. (#4787)
 * error out if non UTF-8 content is transmitted in COPY mode, when client_encoding=UTF8 (#4751)
 * propagate errors in deferred EndCopy() call in CommitTransaction() (#5054)
 * make ogr2ogr -lco GEOM_TYPE=geography work when the source layer has a named geometry column, and also reject creating geography columns with a SRS != EPSG:4326 (#5069)
 * support other geographic SRS than EPSG:4326 for geographic type (#5075)

S57 driver:
 * enable recoding to UTF-8 by default (RECODE_BY_DSSI=YES open option) (#4751)
 * handle more than 255 updates to a feature (#5461)
 * only apply an update if update.UPDT == 1 + previous.UPDT, or update.EDTN == 0 (#5461)

Selafin driver:
 * fix time step count when none dataset in the file

Shapefile driver:
 * better deal with ETRS89 based CRS with TOWGS84\[0,0,0,0,0,0,0\]
 * writer: avoid considering rings slightly overlapping as inner-outer rings of others (#5315)
 * fix perf issue when writing multilinestring with lots of parts (#5321)
 * consider rings at non-constant Z as outer rings (#5315)
 * fix unlikely nullptr dereference (#5635)

SOSI driver:
 * avoid segfault on invalid geometries (#5502)

SQLite driver:
 * add a STRICT=YES layer creation option to create tables as SQLite >= 3.37 STRICT tables
 * workaround MacOS system SQLite non-default settings that cause issues with WAL and AlterFieldDefn() when patching CREATE TABLE DDL
 * deal explicitly with CPL_VSIL_USE_TEMP_FILE_FOR_RANDOM_WRITE=YES for /vsi network file systems (#5031)
 * DeleteField(): use ALTER TABLE ... DROP COLUMN if sqlite >= 3.35.5, and run foreign_key_check only if foreign_keys is ON (refs qgis/qgis#47012)
 * AlterFieldDefn(): use ALTER TABLE ... RENAME COLUMN when only renaming is to be done and sqlite >= 3.26.0
 * fix crash when doing select load_extension('libgdal.so') from a statically linked sqlite3 console application
 * add a OGR_SQLITE_LOAD_EXTENSIONS configuration option
 * fix VirtualShape support with spatialite 5.0.1 or older and sqlite 3.38.0

TopoJSON driver:
 * fix duplicate 'id' field, and other potential issues when reading fields (3.4.0 regression)

VFK driver:
 * avoid crash when a SQLite3 statement fails

WFS driver:
 * if COUNT is present in WFS >= 2, use it as the page size

## SWIG Language Bindings

All bindings:
 * add missing CPLES_SQLI constant (#4878)
 * add gdal.DataTypeUnion()
 * Geometry.ExportToWkb()/ExportToIsoWkb(): use Intel order by default to avoid useless byte swapping
 * use OGR_G_CreateFromWkbEx() in ogr.CreateGeometryFromWkb() to be able to handle > 2 GB WKB

C# Bindings:
 * Re-target C# bindings apps to netcoreapp2.2 (#4792)
 * Switch default platform target to AnyCPU #1368

Python bindings:
 * for Windows and Python >= 3.8, automatically add the first path with (lib)gdal*.dll with os.add_dll_directory()
 * Change GetFieldAsBinary to use VSIMalloc. (#4774)
 * add xmp=True/False argument to gdal.Translate()
 * fix/workaround setuptools 60.0 and Debian --install-layout
 * replace unsafe use of tempfile.mktemp() by mkstemp()
 * utilities-as-lib: propagate warnings to custom error handler when UseExceptions is on (#5136)
 * remove all traces of distutils
 * make sure GetUseExceptions() doesn't clear the error state (#5374)
 * make feature.GetField() returns a bool for a (type=OFTInteger, subtype=OFSTBool) field

# GDAL/OGR 3.4.3 Release Notes

## GDAL 3.4.3 - Overview of Changes

### Port

* /vsizip/: fix Eof() detection on stored (ie not compressed) files inside the zip (#5468)
* /vsiaz/: do not sign request in AZURE_NO_SIGN_REQUEST mode even when we have credentials
* /vsimem/: VSIFTruncateL(): make sure to zeroize beyond the truncated area
* Fix memory leak in VSICurlClearCache() (#5457)
* Fix out of bounds read in CPLRecodeFromWCharIconV() (#5542)

### Core

* GeoEye metadata reader: avoid potential write stack-buffer overflow on long basename files (#5506)

### Utilities

* gdalbuildvrt: fix potential out-of-bounds access when using -b option
* gdalwarp: fix issue with vertical shift mode when only one of the CRS is 3D, with PROJ 9.1

### Raster drivers

FITS driver:
 * fix crash at runtime with clang 14 -O2

GRIB driver:
 * consider longitudes that slightly exceed 360° as 360° for Split&Swap mode (#5499)
 * multidim: fix crash when a .idx file is present (#5569)

GTiff driver:
 * SRS reading: add warning when CRS definition from geokeys is inconsistent with EPSG, and a GTIFF_SRS_SOURCE=EPSG/GEOKEYS configuration option to alter the SRS (#5399)
 * avoid hang when trying to create a larger than 4GB classic TIFF, and switch to BigTIFF before that happens
 * fix crash when building overviews and computing approx stats (#5580)

MRF driver:
 * Prevent LERC out of bounds access (#5598)

NITF driver:
 * avoid PROJ error to be emitted in Create() when ICORDS=N/S (#5563)

VICAR driver:
 * avoid undefined behavior on empty container (ossfuzz #46650)

## OGR 3.4.3 - Overview of Changes

### Core

* OGREnvelope: ignore fp warnings for operator== like IsInit()
* GML geometry importer / GMLAS: be robust to leading spaces in <gml:coordinates> element (#5494)
* Multipoint WKT import: accept MULTIPOINT Z in non-backeted mode, like what PostGIS outputs

### Utilities

* ogr2ogr: fix cutting of geometries in projected coordinates intersecting antimeridian, in some configurations

### Vector drivers

ESRIJSON driver:
 * fix dimensionality of PolygonZ

FlatGeoBuf driver:
 * catch exception on GetFeature() on corrupted index
 * fix GetFeature() when featuresCount != 0 and indexNodeSize == 0

GPKG driver:
 * fix opening files in NOLOCK=YES mode that contains '?' or '#' (fixes qgis/QGIS#48024)

MITAB driver:
 * add WindowsBalticRim / CP1257 charset mapping (#5608)

OpenFileGDB:
 * fix dimensionality of MULTILINESTRING M geometries

S57 driver:
 * fixes related to update files (#5461)

SOSI driver:
 * avoid segfault on invalid geometries (#5502)

# GDAL/OGR 3.4.2 Release Notes

## Build

* Fix build against Poppler > 21 (#5071)
* Fix build against libhdf5 1.13.0 (#5061)
* Fix build with tiledb >= 2.7 (#5409)

## GDAL 3.4.2 - Overview of Changes

### Port

* Google cloud - Support type=authorized_user JSON file for authentication
* /vsigs/: fix upload of files > 4 MB in HTTP 1.1 (#5267)
* /vsitar/: fix reading .tar.gz files when the size of the uncompressed  .tar file is a multiple of 65536 bytes (#5225)
* VSICurl: Print response code for failed range requests

### Core

* Pleiades metadata reader: fix to handle RPC for Pleiades Neo (#5090)
* Driver manager: make sure oMapNameToDrivers.size() == nDrivers when querying a non existing driver
* Overview generation: fix Cubic resampling on boundaries between valid and nodata areas.

### Algorithms

* Rasterization of polygons: avoid underflow/overflow of output data type
* warper: better guess output bounds when warping from a rotated pole projection that include poles

### Utilities

* gdal_calc.py: raise error when overwriting behavior is apparently wished but not specified (#5270)
* gdalbuildvrt: change logic to check homogeneous number of bands (#5136)
  - if no explicit band list is specified, all source datasets must have
    the same number of bands (previously we checked that the second,
    third, etc. datasets had at least a number of bands greater or equal
    than the first one, which made things order dependent).
  - if an explicit band list is specified, we tolerate a different number of bands, provided that datasets have at least all requested bands.
* gdalbuildvrt: add a -strict/-non_strict flag, and in strict mode consider non-existing datasets as a failure (#4755)
* gdalsrsinfo: emit message when replacement of deprecated CRS occurs
* gdalsrsinfo: return a non-zero exit code when specified SRS fails to load (#5201)
* gdal2tiles: remove/fix broken links in generated HTML files
* gdal2tiles: fix issue with multiprocessing and the gdal2tiles launcher script on Windows and Python >= 3.8 (#4951)
* gdal2tiles: detect write error when creating tiles, and allow outputting to /vsi filesystems (#3382, #5370)

### Raster drivers

BAG driver:
 * fix 'too many refinement grids' error (#3759)

BSB driver:
 * Add missing kap file extension in driver metadata

BYN driver:
 * remove validation of nTideSys and nPtType fields

COG driver:
 * output exactly square pixels when using -co TILING_SCHEME (#5343)
 * and emit warning messages when using RES or EXTENT options in that mode.

DIMAP driver:
 * register metadata on all 6 PNEO bands (#5420)

ECW driver:
 * fix non-nearest upsampling on multi-band datasets (#5288)

ESRIC driver:
 * Fix bundle file name

FileGDB driver:
 * workaround a crash involving binary field and CDF datasets

GTiff driver:
 * fix DISCARD_LSB with nodata value (#5097)
 * Updates for IJG libjpeg-9e
 * GTIFWktFromMemBufEx / GTIFMemBufFromSRS: use OSRSetPROJSearchPaths() (#5187). Affects GeoJP2 encoding/decoding
 * fix exposing WEBP_LOSSLESS option
 * remove limitation to 32,000 bytes when writing the GDAL metadata tag (#4116)

GRIB driver:
 * fix write heap-buffer-overflow when using 'split and swap column' mode when the split column is not equal to width / 2 (#5290)

HFA driver:
 * Fix "Pulkovo 1942" datum write to IMG files

MRF driver:
 * Fix padding space logic (#5096)
 * Adjust PNG limits (#5347)

netCDF driver:
 * add a IGNORE_XY_AXIS_NAME_CHECKS=YES open option (refs qgis/QGIS#47158)
 * recognize x/y axis from GMT generated files as geospatial axis (#5291, qgis/QGIS#47158, qgis/QGIS#45704)

PNG driver:
 * Internal libpng: fix memleak on corrupted file.

TGA driver:
 * fix reading images with runs crossing scanlines (#5168)

WMTS driver:
 * disable clipping with TileMatrixSetLimits by default

## OGR 3.4.2 - Overview of Changes

### OGRSpatialReference

* OGRSpatialReference::SetFromUserInput(): make it work with 'IAU:XXXX'

### Vector drivers

CSV driver:
 * Fix issues with attribute and geometry fields of same name

FlatGeobuf driver:
 * If CRS WKT detected to be non UTF-8, force it to ASCII (#5062)

GPKG:
 * deal explicitly with CPL_VSIL_USE_TEMP_FILE_FOR_RANDOM_WRITE=YES for /vsi network file systems (#5031)
 * use ALTER TABLE ... RENAME/DROP COLUMN when possible
 * when adding a 'epoch' column to gpkg_spatial_ref_sys, one must use the 'gpkg_crs_wkt_1_1' extension instead of 'gpkg_crs_wkt'
 * add a NOLOCK=YES option to open a file without any lock (for read-only access) (helps fixing qgis/qgis#23991, but requires QGIS changes as well)

MapInfo driver:
 * .tab writing: correctly detect datum when creating a layer from a WKT2 CRS string (#5217)

PG driver:
 * ogr2ogr/PG: propagate errors in CommitTransaction() (#5054)
 * support other geographic SRS than EPSG:4326 for geographic type (#5075)

Shapefile driver:
 * writer: fix speed issue when writing multilinestring with lots of parts (#5321)
 * writer: fixes for slightly overlapping parts in a multipolygon, and non-horizontal multipolygon Z (#5315)

SQLite:
 * deal explicitly with CPL_VSIL_USE_TEMP_FILE_FOR_RANDOM_WRITE=YES for /vsi network file systems (#5031)
 * use ALTER TABLE ... RENAME/DROP COLUMN when possible
 * SQLite SQL dialect: Fix issues with attribute and geometry fields of same name
 * fix crash when doing select load_extension('libgdal.so') from a statically linked sqlite3 console application
 * fix VirtualShape support with spatialite 5.0.1 or older and sqlite 3.38.0

## SWIG bindings

CSharp bindings:
 * Switch default platform target to AnyCPU (#1368)

Python bindings:
 * utilities-as-lib: propagate warnings to custom error handler when UseExceptions is on (#5136)
 * make sure GetUseExceptions() doesn't clear the error state (#5374)

# GDAL/OGR 3.4.1 Release Notes

## Build

* configure.ac: fix detection of OpenEXR >= 3 (#4766)
* Add support for PCRE2 (to replace deprecated PCRE) (#4822)
* nmake.opt: fix wrong variable name in example MSODBCSQL_LIB
* Add support for external libqhull_r (#4040)

## GDAL 3.4.1 - Overview of Changes

### Algorithms

* viewshed.cpp: remove hidden bidirectional unicode character in code comment

### Port

* CPLRecodeIconv(): avoid potential unsigned integer overflow (ossfuzz #41201)
* CPLRecode(): ZIP support: avoid warnings on MacOS
* IVSIS3LikeFSHandler::CopyFile(): always take into account ret code of CopyObject()
* CPLZLibInflate(): workaround issue with /opt/intel/oneapi/intelpython/latest/lib/libz.so.1 from intel/oneapi-basekit Docker image

### Core

* GDALDataset::MarkSuppressOnClose(): delete auxiliary files (#4791)
* GDALPamRasterBand::CloneInfo(): deal correctly with NaN nodata to avoid generating useless .aux.xml file (#4847)
* External overviews: automatically turn PLANARCONFIG_CONTIG for WebP overviews

### Utilities

* gdaladdo -clean: remove overviews of mask (#1047)
* gdalwarp -crop_to_cutline: fix extremely long loop (#4826)
* gdalwarp: do not emit "Point outside of projection domain" / "tolerance condition error" (#4934)
* gdal_polygonize.py: remove use of Unicode double quote characters
* gdal_polygonize.py: make -8 switch work again (#5000)
* gdal_sieve.py: fix exception when source dataset has no nodata value (3.4.0 regression) (#4899)
* gdal2tiles: making --no-kml option takes effect (#4936)
* gdal2tiles: XML escape input filename (#5032)

### Raster drivers

COG driver:
 * only create RGB JPEG with mask if 4-band is alpha (#4853)
 * fix potential generation failure when main imagery has overview and mask none

ECW driver:
 * do not try to open UInt32 JPEG2000 if the SDK is buggy (#3860)

ERS driver:
 * Add support for comments in ERS files (#4835)

GRIB driver:
 * fix thread-safetey of errSprintf()
 * avoid read heap buffer overflow due to inappropriate split-and-swap on dataset with weird georeferencing (ossfuzz #41260, #41637)
 * fix writing negative longitude of natural origin for Transverse Mercator

GTiff driver:
 * only emit warnings on libgeotiff PROJ errors (#4801)
 * Initialize jxl structures with zeros.
 * LERC overview related improvements (#4848):
   + COMPRESS_OVERVIEW configuration option now honours LERC_DEFLATE and LERC_ZSTD for external overviews
   + MAX_Z_ERROR_OVERVIEW configuration option is added for LERC compressed internal and external overviews.
   + ZLEVEL_OVERVIEW configuration option is added for DEFLATE/LERC_DEFLATE compressed internal and external overviews.
   + ZSTD_LEVEL_OVERVIEW configuration option is added for ZSTD/LERC_ZSTD compressed internal and external overviews.
 * fix performance issue when reading transfer functions (#4923)
 * propagate SPARSE_OK to overviews (#4932)
 * avoid huge memory allocation when generating overviews on large single-band 1-bit tiled files (#4932)
 * make SetGCPs(), SetGeoTransform(), SetSpatialRef(), SetNoDataValue(), SetMetadata[Item]() write to PAM .aux.xml on read only files (#4877)
 * add support for reading/writing color table from/into PAM .aux.xml (#4897)
 * do not warn about buggy Sentinel1 geotiff files use a wrong 4326 code for the ellipsoid
 * Internal libtiff: fix issue with rewrite-in-place logic of libtiff #309

KEA driver:
 * print error message when opening of kea file fails

NITF driver:
 * Add ISO-8859-1 decoding for file and image header metadata

PCIDSK driver:
 * fix write heap-buffer-overflow (ossfuzz #41993)

PDS4 driver:
 * write conformant Equirectangular when input raster is a geographic CRS

USGSDEM driver:
 * fix reading datasets with 1025 byte records ending with line feed (#5007)

VRT driver:
 * Warped VRT: fix issue with blocks without sources and alpha band (#4997)

XYZ driver:
 * Fix incorrect failure to open ASCII-file due to floating point comparison

Zarr driver:
 * be robust to duplicated array and group names in NCZarr metadata, which could lead to performance issues
 * fixes 'runtime error: reference binding to null pointer of type 'unsigned char' (ossfuzz #41517)

## OGR 3.4.1 - Overview of Changes

### OGRSpatialReference

* OGR_CT: add a OGR_CT_PREFER_OFFICIAL_SRS_DEF config option

### Vector drivers

DXF driver:
 * Do not copy final DXF when MarkSuppressOnClose was called

DWG/DGNv8 drivers:
 * avoid potential crash with ODA 2022 on Linux when driver unloading doesn't occur properly
 * DWG: Add block attributes to entities layer (#5013)

ElasticSearch driver:
 * do not try to open reserved .geoip_databases to avoid error message with recent Elastic versions

GPKG driver:
 * fix nullptr dereference on corrupted databases with sqlite >= 3.35

MSSQL driver:
 * fix compilation warnings on Linux
 * avoid GetLayerDefn() to return null, and call it in GetExtent()
 * DeleteLayer(): remove suspicious test 'strlen(pszTableName) == 0' that causes a read after free
 * fix issue when inserting strings in bulk copy mode on Linux
 * make GetNextFeature() end bulk copy mode
 * CreateFeature in bulk copy mode: make it use feature geometry SRID when set
 * disable bulk mode when a UUID field is found, as this doesn't work currently
 * do not set field width when reading smallint/int/bigint/float/real columns, and correctly roundtrip smallint/Int16 (#3345)

ODS driver:
 * avoid crashing 'floating-point exception' when evaluating -2147483648 % -1 (ossfuzz #41541)

OpenFileGDB driver:
 * correctly parse raster fields of type == 2 which are inlined binary content (#4881)

PG driver:
 * skip all leading whitespace in SQL statements. (#4787)

Selafin driver:
 * fix time step count when none dataset in the file

Shape driver:
 * better deal with ETRS89 based CRS with TOWGS84[0,0,0,0,0,0,0]

SQLite driver:
 * Various fixes for MacOS system SQLite

S57 driver:
 * enable recoding to UTF-8 by default (RECODE_BY_DSSI=YES open option) (#4751)

TopoJSON driver:
 * fix duplicate 'id' field, and other potential issues when reading fields

WFS driver:
 * if COUNT is present in WFS >= 2, use it as the page size. Also clarify doc

## SWIG Language Bindings

All bindings:
 * add missing CPLES_SQLI constant (#4878)

CSharp bindings:
 * Re-target apps to netcoreapp2.2, fix broken build (#4792)

Java bindings:
 * add_javadoc.c: robustness fixes and fix crash on MacOS

Python bindings:
 * Change GetFieldAsBinary to use VSIMalloc. (#4774)
 * install target: fix/workaround for setuptools 60.0 and Debian --install-layout

# GDAL/OGR 3.4.0 Release Notes

## In a nutshell...

* [RFC 81](https://gdal.org/development/rfc/rfc81_coordinate_epoch.html):
  Support for coordinate epochs in geospatial formats.
  Implemented in FlatGeoBuf, GeoPackage, MEM, VRT
* New GDAL drivers:
  - [Zarr](https://gdal.org/drivers/raster/zarr.html):
    read/write support for ZarrV2 (and experimental V3), using 2D classic raster
    API or multidimensional API:
  - [STACIT](https://gdal.org/drivers/raster/stacit.html):
    Spatio-Temporal Asset Catalog Items as virtual mosaics
* Other improvements:
  - number of enhancements in file system operations of /vsigs/
  - NITF: additions to comply with NITF Version 2.1 Commercial Dataset
    Requirements Document (NCDRD)
  - ODBC and PGeo: multiple fixes and improvements
  - SAFE (Sentinel1): multiple improvements related to SLC/calibration (change
    subdataset naming)
  - multidimensional API: caching, and other improvements
* Code linting and security fixes
* Bump of shared lib major version
* MDB driver (Java based) mark as deprecated. Planned for removal for GDAL 3.5.
  ODBC driver is the preferred solution (with up-to-date MDBTools library on
  non-Windows platforms)
* Writing side of Tiger driver deprecated and will be removed in GDAL 3.5
* Remainder: DODS, JPEG2000(Jasper), JPEGLS, MG4LIDAR, FUJIBAS, IDA, INGR and
  vector driver ARCGEN, ArcObjects, CLOUDANT, COUCHDB, DB2, DODS, FME, GEOMEDIA,
  GTM, INGRES, MONGODB, REC, WALK are planned for removal in GDAL 3.5. As well
  as Perl bindings

## New optional dependencies

* libjxl (master) when using internal libtiff for JPEG-XL support in TIFF
* liblz4, libblosc: recommended for Zarr driver (as well as existing libzstd, liblzma)
* libbrunsli for JPEG-XL support in MRF driver (as well as existing libzstd)

## New installed files

 * include/cpl_compressor.h

## Removed installed files

 * no longer build testepsg utility by default, which has been superseded by
   gdalsrsinfo for many years. It will be finally removed in GDAL 3.5

## Backward compatibility issues

See [MIGRATION_GUIDE.TXT](https://github.com/OSGeo/gdal/blob/release/3.4/gdal/MIGRATION_GUIDE.TXT)

## Build changes

Build(General):
 * Changes to build against Xerces-c 4.0dev

Build(Unix):
 * configure file no longer in git. autogen.sh must be run
 * configure: fix explicit disabling of geos/sfcgal (#3782)
 * configure.ac: also check netcdf presence by using static linking
 * fix lack of compilation of nasakeywordhandler.o with some old/non-gnu versions of grep (#3836)
 * configure.ac: Add compatibility with autoconf 2.71. Set 2.69 as minimum version
 * #include local files with quotes, and for Unix build, use -iquote instead of -I (#4091)
 * Use pkg-config for libdap when dap-config is not available.

## GDAL 3.4.0 - Overview of Changes

### Port
 * Add cpl_compressor.h API
 * Add VSIAbortPendingUploads() to abort pending multipart uploads on /vsis3/ and /vsigs/
 * /vsiaz/: avoid appending same chunk several times
 * /vsigs/: switch to using S3-style multipart upload API, which is now supported
 * /vsigs/: enable parallel multipart upload
 * /vsigs/: add support for reading/writing ACLs through GetFileMetadata/SetFileMetadata with the ACL domain
 * /vsigs/: implement UnlinkBatch() (efficient implementation only works with OAuth2 authentication)
 * /vsigs/: implement RmdirRecursive() (efficient implementation only works with OAuth2 authentication)
 * /vsigs/: add support for GS_USER_PROJECT config option to specify the project ID to bill for requester pays access
 * /vsigs/: add support for GS_NO_SIGN_REQUEST=YES
 * /vsigs/: implement SetFileMetadata(filename, metadata, 'HEADERS')
 * /vsiswift/: Fix openstack swift endpoint detection, add application_credential authentication support (#3960)
 * /vsiswift/: properly cache stat() results
 * VSISwiftFSHandler::Stat(): fix potential nullptr dereference (ossfuzz #37906)
 * /vsis3/ , /vsiaz/: properly set mode field into file property cache
 * /vsicurl/: fix to be able to read /vsis3/zarr-demo/store/spam
 * /vsicurl/: make it work without explicit CPL_VSIL_CURL_USE_HEAD=FALSE with .earthdata.nasa.gov/ URLs (#4641)
 * /vsicurl/: fix crash with curl 7.79.0 to get file size when retrieving headers only
 * /vsizip/: fix memory leak in error code path (ossfuzz #29715)
 * /vsitar/: support prefixed filename of ustar .tar archives (#4625)
 * /vsitar/ etc: avoid potential long processing time on invalid filenames in archive (ossfuzz #30312, ossfuzz #39129)
 * IVSIS3LikeFSHandler::Sync(): add missing lock
 * VSIS3FSHandler::GetOptions(): fix non-XML conformity
 * cpl_odbc.cpp: fix segfault in debug message due to nullptr dereference
 * CPLODBCSession::ConnectToMsAccess(): try various DSN template combinations because the combo odbcinst 2.3.7 from Ubuntu 20.04 + self compiled mdbtools-0.9.3 doesn't like double quoting of DBQ
 * MDBTools driver installation: do not error out if /etc/odbcinst.ini contains an entry for Access, and also add /usr/lib64/odbc as a path to please Fedora
 * Add CPLJSONObject::DeleteNoSplitName()
 * CPLJSONDocument::Load(): increase max document size to 100 MB
 * CPLJSONDocument::LoadMemory(): fix parsing 'true' and 'false'
 * Add CPLQuadTreeRemove()
 * VSIGetMemFileBuffer(): delay destroying VSIMemFile on bUnlinkAndSeize (#4158)
 * Fix VSIMemHandle::Read(buffer,nSize=0,nCount=1) to return 0
 * minixml parser: fix performance issue on very large processing instructions. (ossfuzz #37113)
 * VSIOpenDir(): add a PREFIX option to select only filenames starting with the specified prefix
 * Unix file system: implement OpenDir() with a NAME_AND_TYPE_ONLY option
 * VSIReadDirRecursive(): use VSIOpenDir() / VSIGetNextDirEntry()
 * VSIStatExL(): add a VSI_STAT_CACHE_ONLY flag to avoid attempting any network access
 * CPLCreateUserFaultMapping(): better error message when userfaultfd system call fails due to permission issue
 * userfaultfd: make it work with kernel >= 5.11 without special OS settings
 * CPL logging mechanism: fix writing non-ASCII UTF-8 filenames on Windows (#4492)
 * VSI cache: fix crash when a Read() to the underlying file fails (#4559)

### Core
 * Add a GDAL_DCAP_COORDINATE_EPOCH driver capability set by drivers which support storing/retrieving coordinate epoch
 * fixed issue with GDALLoadRPBFile failing when RPB file does not contain errBias or errRand fields
 * CPLKeywordParser / IMD parser: accept .IMD files with non-properly quoted values (#4037)
 * NASAKeywordHandler: fix performance issues (ossfuzz #39092)
 * IMD writer: properly quote values that have spaces (#4037)
 * ComputeStatistics(): tiny speed improvements for SSE2 Byte/UInt16 cases
 * Overview generation: use per band mask with nodatavalue and average (#3913)
 * GMLJP2v2 writing: fix axis order issue for gml:Envelope (#3866)
 * Overview/rasterio: fix RMS on large Float32 values
 * Overview/rasterio: RMS resampling: add AVX2 implementation for Float and UInt16
 * Overview building: fix crash possibility if user interrupting multi-threaded computation
 * GDALDriver::DefaultCreateCopy(): preserve coordinate epoch
 * PAM: support coordinate epoch as a SRS.coordinateEpoch attribute (RFC 81)
 * GDALDataset::BlockBasedRasterIO(): fixed progress calculation
 * GDALDataset::BlockBasedRasterIO(): fix when an overview is selected and non-integer window coordinates are involved (fix issue with GeoRaster driver. #4665)
 * GetIndexColorTranslationTo(): fix non-exact color matching when palettes don't have the same number of entries (#4156)
 * Block cache: fix potential infinite loop in case of high concurrency and multi-threaded writing (#3848)
 * GDALDataset::GetFileList(): avoid cyclic calls on odd/corrupted datasets (ossfuzz #37460)
 * GDALOpenInfo(): fix issue when passing a directory on netBSD and potentially other BSDs
 * DumpJPK2CodeStream(): dump COC, QCD and QCC segments, and add HTJ2K specificities
 * GDALGetJPEG2000Structure(): limit default number of lines in output to avoid excessive memory and processing time (ossfuzz #14547)
 * GDALMDReaderPleiades::LoadRPCXmlFile(): fix nullptr dereference on non-conformant file (ossfuzz #38998)
 * GDALDataset::FlushCache() and GDALRasterBand::FlushCache(): add a bool bAtClosing argument (fixes #4652)
 * Make GDALRasterBand::TryGetLockedBlockRef() public

### Multidimensional API
 * Add GDALMDArray::IsRegularlySpaced() and GDALMDArray::GuessGeoTransform()
 * Add GDALMDArray::GetResampled() to get a resampled / reprojected view
 * Add a GDALMDArray::GetCoordinateVariables() method
 * Add a GDALMDArray::Cache() method to cache an array/view into a .gmac side car file
 * Add GDALMDArray::IsCacheable() so that drivers can disable attempts at opening cache file on main dataset
 * API: add JSON as a possible subtype of (string) extended data type
 * GDALRasterBand::AsMDArray(): fix data axis to SRS Axis mapping in the return of GetSpatialRef()
 * GDALMDArray::AsClassicDataset(): fix RasterIO() when buffer data type is not native data type
 * Add GDALPamMDArray and GDALPamMultiDim classes to be able to serialize/deserialize metadata (SRS for now) from GDALMDArray
 * Add GDALExtendedDataTypeCreateStringEx() and map it to SWIG

### Algorithms
 * Warper: Eliminate thread lock timeouts on Windows (#3631)
 * Warper: prevent potential int overflows for > 2 GB work buffers
 * Warper: force SAMPLE_GRID=YES when target output includes one pole (#4064)
 * Warper: OpenCL: remove extra double quote character in resampling kernel (#4404)
 * Warper: avoid selecting Float64 working data type with a Float32 raster and a nodata value compatible of it (#4469)
 * geolocation transformer: use GDALFillNodata() to fill holes in the backmap
 * GDALSuggestedWarpOutput2(): better guess of bounds when transforming with a geolocation array
 * gdalgeoloc: fix if the backmap would be larger than 2 Giga pixels
 * gdalgeoloc: backmap generation: prevent averaging that would lead to unexpected results
 * gdalgeoloc: improve backmap hole filling
 * Polygonize: fix self intersection polygon when raster image contains complex holes (#1158, #3319)
 * Deprecate GDALOpenVerticalShiftGrid() and GDALApplyVerticalShiftGrid() that are no longer needed. Will be removed in GDAL 4.0

### Utilities
 * gdalinfo: report coordinate epoch (RFC 81)
 * gdalinfo: avoid displaying thousands of filename in /vsizip/ if open fails
 * gdal_translate: add -a_coord_epoch switch (RFC 81)
 * gdalwarp: allow vertical shift when using EPSG codes (#4566)
 * gdalwarp: preserve coordinate epoch in output SRS (RFC 81)
 * gdalwarp: add a -s_coord_epoch/_t_coord_epoch switches (RFC 81)
 * Fix gdalwarp / exporting to WKT when a projection that isn't supported in WKT1 is involved (#4133)
 * gdalwarp: allow reprojecting a rater with geotransform = (0,1,0,0,0,-1) (#4147)
 * gdalwarp: slightly change default behavior of unified src band masking when a pixel is evaluate to the nodata value on all bands (#4253)
 * gdal_contour: fix bad coordinate order when outputting from raster with geographic CRS to KML driver (#3757)
 * gdal_viewshed: fix incorrect calculation if target height is specified (#4381)
 * gdal_viewshed: Fix incorrect progress reporting (#4390)
 * gdal_viewshed: change default value of -cc (curvature coefficient) to 0.85714, standard value for atmospheric refraction (#4415)
 * gdal_polygonize.py: fix output driver guessing
 * gdal_fillnodata.py: copy pixel data after metadata
 * gdal_fillnodata.py: fix -si option (#4192)
 * gdal_fillnodata.py: remove broken -nomask option. It has likely never worked, and was equivalent to the default mode
 * gdal_sieve.py: preserve NoData value when output to a new file
 * gdal2tiles: fix tile grid origin of openlayers with -p geodetic --tmscompatible --xyx as arguments
 * gdal2tiles: avoid potential race when creating directories (#4691)
 * gdal_pansharpen.py: display usage if not enough filenames provided
 * gdalmdiminfo: add dimension_size and block_size in output
 * gdalmdimtranslate: add -oo switch to specify open option of source dataset

### gdal_utils package
 * Various linting and improvements in gdal_utils package
 * Create ogrmerge(), gdalproximity(), gdal_sieve(), rgb2pct(), pct2rgb(), gdal_fillnodata(), gdal_polygonize() functions
 * fix GetOutputDriversFor for ext == 'nc' (#4498)

### Raster drivers

AIGRID driver:
 * avoid integer overflow (ossfuzz #31766)
 * fix crash when accessing twice to a broken tile (#4316)

BAG driver:
 * ignore pixel at nodata value when updating min/max attributes (#4057)

BMP driver:
 * harden identify checks to avoid misidentification of other datasets (#4713)

CEOS2 driver:
 * Add ASF Compatibility (#4026)

COG driver:
 * Default to LZW compression (#4574)
 * Add LZMA to compressions list
 * don't crash if GDALCreateGenImgProjTransformer2() fails
 * Fix error messages when creating JPEG compressed COGs from RGBA

DIMAP driver:
 * expose virtual overviews (#4400)
 * fix integer overflows (ossfuzz #39027, #39795)
 * fix potential performance issue in SetMetadataFromXML() (ossfuzz #39183)
 * avoid excessive memory allocation / almost infinite loop on corrupted dataset (ossfuzz #39324)

ECW driver:
 * Enable reading ECW files from virtual file systems for ECW SDK 3.3

EEDAI driver:
 * Point at highvolume EE API endpoint by default

EIR driver:
 * avoid excessive memory allocation on small files (ossfuzz #9408)

ENVI driver:
 * Open(): use .hdr as an additional extension in priority over a replacement one (#4317)
 * use newer Get/SetSpatialRef API to avoid performance issue (ossfuzz #38295)

ENVISAT driver:
 * avoid potential buffer overflow (CID 1074533)

ERS driver:
 * avoid long open delay on bogus datasets that reference themselves (ossfuzz #36701)
 * Fix off-by-one error when reading ERS header

EXR driver:
 * fix build against OpenEXR 3.0.1 (#3770)

GeoRaster driver:
 * Fix the Oracle DB connection when user and pwd are empty, while Oracle wallet is used. (#3768)
 * Set the SRID value for the extent when flushing the metadata to the database. (#4106)
 * fix issue when user name is not provided in connection string (#4460)
 * Add owner checking in the query (#4565)

GRIB driver:
 * use sidecar .idx file and lazy initialization (#3799)
 * implement transparent [0,360] to [-180,180] translation, in reading and writing (#4524, #3799)
 * add/fill a few GRIB1 ECMWF table to read a ERA5 product (#3874)
 * turn debug messages about unknown GRIB1 table as warnings
 * fix stack read buffer overflow (ossfuzz #38610)
 * fix wrong check related to memory allocation limit (ossfuzz #38894)
 * fix potential crashes on big GRIB1 files on low-memory condition (ossfuzz #38971)
 * multidim: catch potential exception in case of out-of-memory situation

GTiff driver:
 * add support for coordinate epoch with a new CoordinateEpochGeoKey=5120 GeoKey (RFC 81)
 * Enable JPEG-in-TIFF for 12 bit samples (for internal libtiff), whatever libjpeg is used for 8 bit sample support
 * Add support for JPEG-XL codec when using internal libtiff, through libjxl master (#4573)
 * fix overviews with NaN nodata value
 * when reading NoData value of Float32 dataset, cast it to float to avoid overprecision (#3791)
 * Fix SetDefaultRAT() (#3783)
 * signedbyte: do not write 0 values for tiles/strips entirely at a negative nodata value (#3984)
 * Set color interpretation on overview bands (#3939)
 * SRS reading: ignore vertical information when one of the tag has a value in the private user range (#4197)
 * write georeferencing info to PAM .aux.xml when using PROFILE=BASELINE
 * do not try to export a DerivedGeographic CRS to GeoTIFF, but fallback to PAM
 * fix corruption in TIFF directory chaining when altering nodata value after IFD crystallization (#3746)
 * Update internal libtiff to latest upstream
 * limit error message count, mainly with Fax3 decoder, to avoid performance issues (ossfuzz #38317)

HDF5 driver:
 * multidim: fix crash when extracting a field from a compound HDF5 data type
 * fix issue when netCDF and/or HDF5 drivers built as plugins with multidim datasets

HFA driver:
 * enable to read GCPs from PAM .aux.xml (#4591)

Idrisi driver:
 * Add support of Cylindrical Equal Area (#3750)

JP2KAK driver:
 * fix Unix build with Kakadu 8.2 (#4306)
 * accept files with .jhc extension (HTJ2K codestreams)
 * report Corder metadata item in IMAGE_STRUCTURE domain instead of default domain

JP2OpenJPEG driver:
 * add a TLM=YES creation option to write Tile-part Length Marker (requires openjpeg >= 2.5)

JPEG driver:
 * fix wrong detection of bit ordering (msb instead of lsb) of mask band (#4351)

KMLSuperOverlay driver:
 * Allow reading kml documents with a GroundOverlay element directly inside a Document element (#4124)

MEM driver:
 * Open(): add support for the 'GEOTRANSFORM' options
 * multidim: faster copying when data type conversion is involved
 * multidim: add MEMAbstractMDArray::SetWritable, IsModified and SetModified methods

MRF driver:
 * add JPEG-XL (brunsli) support (#3945)
 * add ZStd compression (#4177)

MSG driver:
 * fix race condition in satellite number lookup (#4153)

NetCDF driver:
 * expose Sentinel5 verbose metadata in json:ISO_METADATA/ESA_METADATA/EOP_METADATA/QA_STATISTICS/GRANULE_DESCRIPTION/ALGORITHM_SETTINGS metadata domains
 * use new PROJ method 'rotated pole netCDF CF convention'
 * add write support for rotated pole method
 * be more restrictive on axis to decide if to advertise a geotransform.
 * fix wrong offsetting by -360 of longitude values when huge nodata value found at beginning or end of line
 * fix reading MODIS_ARRAY.nc file that has non CF-compliant way of expressing axis and CRS (#4075)
 * support more possibilities for content of 'crs' attribute in some opendatacube datasets
 * accept degree_north/degree_east (degree singular) as acceptable unit for latitude/longitude axis (#4439)
 * accept "degrees_E"/"degrees_N" as valid axis names (#4581)
 * Accept HDF5 files if HDF5 driver is not available and libnetcdf has support for them
 * properly deal with Polar Stereographic in reading and writing (#4144)
 * parse correctly valid_range/valid_min/valid_max attributes when they are of non-integer type (#4443)
 * no longer do filename recoding with netCDF >= 4.8 on Windows
 * multidim: make opening /vsimem/ file work when netCDF MEM API is available (avoids using userfaultfd)
 * multidim: make /vsi access work with netcdf >= 4.7
 * multidim: avoid crash when getting data type of a char variable with 2 dimensions.
 * multim: GetBlockSize(): fix heap buffer write overflow on 2D char variables (ossfuzz #39258)
 * multidim: fix use of coordinates attribute to find the indexing variable of a dimension

NITF driver:
 * add PROFILE=NPJE_VISUALLY_LOSSLESS/NPJE_NUMERICALLY_LOSSLESS with JP2OpenJPEG driver, to comply with STDI-0006 NITF Version 2.1 Commercial Dataset Requirements Document (NCDRD)
 * fix writing of COMRAT field in JPEG/JPEG2000 mode, when there are FILE TREs
 * expose ISUBCAT for each band (if non-empty) (#4028)
 * add capability to add several images in a file
 * extend nitf_spec.xml to contain DES subheader user-data fields definitions for CSATTA DES, CSSHPA DES, CSSHPB DES and XML_DATA_CONTENT
 * Slightly modify xml:DES output, such that field names no longer have the NITF_ prefix

PCIDSK driver:
 * fix retrieval of RPC metadata (3.3 regression) (#4118)
 * avoid big memory allocation and processing time (ossfuzz #37912)
 * avoid long processing time by limiting the number of bands (ossfuzz #38885)

PDF driver:
 * update PDFium dependency to pdfium/4627. Requires use of https://github.com/rouault/pdfium_build_gdal_3_4 PDFium fork
 * avoid potential nullptr dereference on corrupted datasets (ossfuzz #28635)
 * Poppler backend: limit CPU time when reading extremely corrupted file. (ossfuzz #37384)

PDS4 driver:
 * update template to CART_1G00_1950 version
 * add support for reading PositiveWest longitudes, MapRotation and Oblique Cylindrical
 * add write support for Oblique Cylindrical
 * fix opening of some .xml files with large number of <?xml-model> tags
 * accept optional <Reference_List> element in template.
 * fixes for compatibility with sp:Spectral_Characteristics in template.
 * projection writing: honor LONGITUDE_DIRECTION=Positive West by negating longitudes in projection parameters

PNG driver:
 * disable Neon optimizations for internal libpng as we don't include the needed source file (fix build issue)

PostgisRaster driver:
 * support for libpq services (#3820)
 * do not append generated application_name in subdataset names

RMF driver:
 * Fix delta compression with non-initialized elevation min-max (#4399)

SAFE driver:
 * multiple improvements related to SLC/calibration (change subdataset naming)

SRTMHGT driver:
 * support .hgts files from NASADEM SHHP. Also support auxiliary files with .err, .img, .num, .img.num and .swb extensions from other NASADEM products (#4239)

TileDB driver:
 * fix crash when creating array from subdatasets fails
 * fix handling of relative paths on Windows

TGA driver:
 * fix heap buffer overflow (ossfuzz #35520)

VICAR driver:
 * add read/write support for GeoTIFF encoding of CRS and geotransform

VRT driver:
 * Support named arguments to C++ derived band pixel functions (#4049)
 * Add interpolate_linear pixel function
 * use source floating point coordinates when valid (#4098)
 * make SourceProperties element no longer needed on reading for deferred opening of sources
 * avoid potential very deep call stack if SourceFilename contains empty string (ossfuzz #37530)
 * Add a per-thread mechanism to count call depth and use it in VRT driver to avoid infinite recursive calls on corrupted datasets (ossfuzz #37717)
 * GDALDatasetPool: avoid potential crash during destruction (#4318)
 * VRT pixelfunc: make it work with buffers larger than 2 giga pixels
 * fix issue with sources with nodata and OverviewList resampling='average' that triggers a 'recursion error' message
 * GDALAutoCreateWarpedVRTEx(): do not set padfSrcNoDataReal/padfDstNoDataReal if already set in passed-in options (rasterio/rasterio#2233)
 * avoid recursion issue on ComputeStatistics()/.ComputeRasterMinMax()/GetHistogram() with implicit overviews and approximation enabled (#4661)

WCS driver:
 * fix issue with negative coordinates (#4550)

WebP driver:
 * Support 'exact' option to preserve the RGB values under transparent areas.

WMS driver:
 * Add an option to disable the cache (#4097)
 * use correct default bbox according to the SRS (#4094)

WMTS driver:
 * Add an option to disable the cache (#4097)
 * do not take into account a TileMatrixSetLink that points to a non-existing TileMatrixSet

ZMap driver:
 * undeprecate it since we got feedback it is still useful (#4086)

## OGR 3.4.0 - Overview of Changes

### Core
 * Add support for reporting hiearchical organization of vector layers, and implement it in FileGDB/OpenFileGDB drivers. (Trac #5752)
 * Add GDALGroup::GetVectorLayerNames() and OpenVectorLayer(), and corresponding C and SWIG API
 * Geometry::MakeValid(): add option to select the GEOS 3.10 'structure' method
 * Support multiline geometry WKT input
 * OGRGeometryFactory::createFromGeoJson(): Support importing GeoJSON fragments in non-null terminated strings (#3870)
 * OGR SQL: fix wrong evaluation of attribute filter when a OR with >= 3 clauses is nested inside in another OR (#3919)
 * OGR SQL: Support ISO-8601 literal dates with IN operator (#3978)
 * OGRFeature::GetFieldAsSerializedJSon(): fix nullptr dereference on empty string list (#3831)
 * Add OGRGetGEOSVersion() and map it to SWIG (#4079)
 * Add GDAL_DCAP_MULTIPLE_VECTOR_LAYERS capability (#4135)
 * Add virtual method GDALDataset::IsLayerPrivate

### OGRSpatialReference
 * Improve OGRProjCT::Clone() and export OGRCoordinateTransformation::Clone() to C API (#3809)
 * Export OCTGetSourceCS(), OCTGetTargetCS() and OCTGetInverse() to C API (#3857)
 * Add OGRSpatialReference::IsDynamic() to check if a CRS is a dynamic one
 * Add OGRSpatialReference::SetCoordinateEpoch()/GetCoordinateEpoch()
 * Coordinate transformation: take into account SRS coordinate epoch if the coordinate tuple has no time value
 * Fix crash in AutoIdentifyEPSG/GetEPSGGeogCS() on some WKTs (#3915)
 * Fix wrong quoting for WKT spatial CS type
 * Add DERIVEDPROJCRS to WKT keywords for OGRSpatialReference::SetFromUserInput
 * Support GetLinearUnits for ellipsoidal and spherical coordinate systems that have a height axis (#4030)
 * AutoIdentifyEPSG(): do not add AUTHORITY node if the axis order of the geographic CRS contradicts the one from EPSG (#4038)
 * OGR_CT: fix transformation between CRS that only differ by axis order (related to #4038)
 * OGR_CT: when a WKT def contradicts its AUTHORITY node, use the WKT def (relates to #4038)
 * OGR_CT: deal with bogus NaN coordinates as output of PROJ (#4224)
 * OGR_CT with OGR_CT_OP_SELECTION=BEST_ACCURACY: fix for non-Greenwich prime meridian
 * OGR_CT: take into account PROJ_NETWORK=ON in OGR_CT_OP_SELECTION=BEST_ACCURACY/FIRST_MATCHING modes
 * OGRSpatialRef::SetFromUserInput(): add options to disallow file and network access, and use it various drivers (ossfuzz #35453)
 * importFromCRSURL() / importFromWMSAUTO() / importFromURN(): avoid excessive memory allocation and processing time on invalid input (ossfuzz #37922 and #38134)
 * OGRSpatialReference::GetAxesCount(): fix on CompoundCRS of BoundCRS
 * fix retrieving projection parameters from WKT1 name on a Projected 3D CRS (OSGeo/PROJ#2846)
 * Fix EPSGTreatsAsLatLong with compoundCRS (#4505)
 * OSRProjTLSCache: avoid use of exceptions (#4531)
 * exportToWkt(): use WKT2 for DerivedProjectedCRS (#3927)
 * Add OSRSetPROJEnableNetwork() / OSRGetPROJEnableNetwork(), and expose them to SWIG
 * Adds OCTTransformBounds to the C and SWIG API to transform a bounding box (#4630)

### Utilities
 * ogrinfo: report hiearchical organization of layers
 * ogrinfo: report SRS coordinate epoch (RFC 81)
 * ogrinfo: Report private layers with a "[private]" suffix
 * ogr2ogr: add a -a_coord_epoch/-s_coord_epoch/_t_coord_epoch switches (RFC 81)
 * ogr2ogr: fix quadratic performance if many source field names are identical (ossfuzz #37768)

### Vector drivers

Multiple drivers:
 * geomedia, walk: Check sql tables for required tables before trying to query them when trying to open a .mdb dataset

CSV driver:
 * improve concatenation performance of multi-line records (ossfuzz #37528)
 * OGREditableLayer (affects CSV driver): fix quadratic performance when creating field (ossfuzz #37768)
 * limit to 2000 fields in opening by default. (ossfuzz #39095)

DGN driver:
 * avoid potential buffer overflow (CID 1074498)
 * replace undefined behavior 1 << 31 by 1U << 31. Not sure of the consequences

DXF driver:
 * Improved handling of very long lines of text in DXF file (#3909)
 * Correctly handle hidden (invisible) INSERTs

DWG driver:
 * Make Closed AcDb2dPolyline and AcDb3dPolyline to Closed OGRLineString (#4264)

ESRIJSON driver:
 * make 'ESRIJSON:http://' connection string scroll results

FlatGeoBuf driver:
 * add support for coordinate epoch (RFC 81)
 * fix memory leaks when reading invalid geometries (ossfuzz #37752, #37834, #38166)
 * set minimum alignment to 8 bytes

FileGDB driver:
 * report hiearchical organization of layers
 * Fix reading of field domains when data source is wrapped in OGRDataSourceWithTransaction
 * Ensure tables which are present in the catalog but not listed in GDB_Items can be read by the driver (#4463)
 * faster CreateLayer() w.r.t SRS identification
 * Implement Identify method

GeoJSON driver:
 * writer: emit error msg when geometry type is not supported in GeoJSON (#4006)
 * recognize URL of likely OAPIF items response page (#4601)

GML driver:
 * reading: fix reading compound curves with gml:ArcByCenterPoint and in a projected CRS in northing/easting axis order (#4155)
 * writer: use GML 3.2 as the new default version (instead of GML 2)
 * read/write srsName as a XML comment in .xsd
 * store geometry element name as geometry column name when reading without .gfs/.xsd (#4386)
 * avoid read heap-buffer-overflow (ossfuzz #35363)
 * avoid performance issue on too nested structure (ossfuzz #21737)
 * fix warning when reading .gfs file with a polyhedral surface
 * expose field names in a more consistent orders when some feature lacks some fields (qgis/QGIS#45139)

GMLAS driver:
 * avoid uncaught exception when opening corrupted .xsd
 * use our own network accessor to honour GDAL_HTTP_TIMEOUT (ossfuzz #37356)
 * fix potential crash on XMLString::transcode() failure (ossfuzz #37459)
 * fix potential crash in ProcessSWEDataArray() (ossfuzz #37474)
 * improve performance of field name truncation (ossfuzz #37836)
 * fix likely quadratic performance in LaunderFieldNames() (ossfuzz #39797)
 * error out on huge mem allocation / processing time in Xerces (ossfuzz #38073)
 * fix performance issue (ossfuzz #38707)

GPKG driver:
 * add support for coordinate epoch (RFC 81)
 * accept CreateField with a column with the fid column name and type=real, width=20, precision=0 (qgis/qgis#25795)
 * fix wrong gpkg_metadata_reference_column_name_update trigger (qgis/qgis#42768)
 * fix for compatibility with SQLite 3.36.0 and views (#4015)
 * add error message when prepare fails
 * capitalize cartesian as Cartesian for SRS of id -1 'Undefined Cartesian coordinate reference system' (#4468)

GPX driver:
 * avoid potential integer overflow on writing (ossfuzz #30937)

ILI driver:
 * Fix non contiguous curves for COORD/ARC sequences (#3755)

KML driver:
 * add read support for non-conformant MultiPolygon/MultiLineString/MultiPoint elements (#4031)

LIBKML driver:
 * add read support for non-conformant MultiPolygon/MultiLineString/MultiPoint elements (#4031)

LVBAG driver:
 * add field definition for "gerelateerdewoonplaats" (#4161)
 * add Field verkorteNaam to OpenbareRuimte FT (#4286)

Memory driver:
 * add support for coordinate epoch (RFC 81)

MITAB driver:
 * Fix mapping of mapinfo symbol numbers to corresponding OGR symbol IDs (#3826)

MongoDB3 driver:
 * fix deprecation warnings with mongocxx >= 3.6

MVT driver:
 * use 'number' instead of wrong 'numeric' when generating metadata tilestats (#4160)
 * reader: be tolerant to broken points generated by Mapserver

OCI driver:
 * Don't try to create new layer with TRUNCATE option (#4027)

ODBC driver:
 * Fix incorrect ODBC type mapping for a number of SQL types
 * Optimise testing if mdb is a PGeo/Walk/Geomedia database
 * Add support for LIST_ALL_TABLES open option
 * Update driver metadata
 * Move MDB Tools access driver installation to  CPLODBCDriverInstaller, and ensure driver installation is attempted when trying to open an MDB file using the ODBC driver
 * Add identify support
 * Add GetLayerByName override which can retrieve private layers by name
 * Implement IsLayerPrivate support

ODS driver:
 * limit maximum number of fields to 2000 by default (ossfuzz #7969)

OGR_GMT driver:
 * fix performance issue. (ossfuzz #38158)

OpenFileGDB driver:
 * report hiearchical organization of layers
 * fix reading of raster fields
 * Add a "LIST_ALL_TABLES" open option
 * Support detection of private layers
 * Correctly list non-spatial, non-private tables which are not present in the GDB_Items table (#4463)

PDS4 vector:
 * add a LINE_ENDING=CRLF/LF layer creation option for DELIMITED and CHARACTER tables
 * fix quadratic performance when creating fields (ossfuzz #37768)
 * avoid warning about wrong field subtype when parsing ASCII_Boolean
 * fix issue with layer names with non alphanum characters or leading digit

PG driver:
 * support postgresql:// connection strings (#4570)

PGeo driver:
 * add search path for libmdbodbc on Ubuntu 20.04
 * use SELECT COUNT(*) for GetFeatureCount() with mdbtools only if it appears to be working (#4103)
 * Make identification of system tables case insensitive
 * Add more internal tables to block list
 * Always promote polygon or line layers to multi-type geometries (#4255)
 * Correctly reflect whether layers have m values in the layer types
 * Correctly return failure when getting extent for non-spatial pgeo select layer
 * Correctly set no geometry type for pgeo select queries with no shape column
 * Add support for field domains (#4291)
 * Add support for retrieving layer definition and metadata with special "GetLayerDefinition table_name" and "GetLayerMetadata table_name" SQL queries
 * Add LIST_ALL_TABLES open option
 * Implement IsLayerPrivate()
 * Add GetLayerByName override which can retrieve private layers by name (#4361)
 * Add identify method (#4357)

PGDump driver:
 * fix performance issue on huge number of columns (ossfuzz #38262)
 * set limit to number of created fields to 1600, consistently with PostgreSQL

PLScenes driver:
 * add PSScene and SkySatVideo item types to conf, and update other item types
 * raster side: make it work with Sentinel2L1C and Landsat8L1G

Shapefile driver:
 * fix wrong SRS when reading a WGS 84 SRS with a TOWGS84[0,0,0,0,0,0,0] (#3958)
 * Communicate why the file size cannot be reached when appending features (#4140)

SQLite/Spatialite driver:
 * fix crash when calling GetMetadataItem() with domain==nullptr (qgis/qgis#43224)
 * fix for compatibility with SQLite 3.36.0 and views (#4015)
 * Implement IsLayerPrivate()
 * support reading tables WITHOUT ROWID (#3884)
 * Add missing SQL datatypes

Tiger driver:
 * tag writing side as deprecated and for removal in 3.5 (#4215)

VRT driver:
 * add support for coordinate epoch (RFC 81)
 * Fix spatial filter for geometry column names with space
 * fix performance issue (ossfuzz #38629)

WasP driver:
 * fix likely wrong condition in line merging (spotted by cppcheck)

WFS driver:

XLS driver:
 * Set OLCStringsAsUTF8 capability to TRUE

## SWIG Language Bindings

All bindings:
 * add VSI_STAT_SET_ERROR_FLAG constant
 * Map CPLSetThreadLocalConfigOption() and CPLGetThreadLocalConfigOption()

Python bindings:
 * add MDArray.GetNoDataValueAsString() / SetNoDataValueString()
 * change gdal.EscapeString() to return a bytearray() when input is bytes(), to make it compatible with backslash escaping of non-ASCII content
 * fix GDALPythonObjectFromCStr() to return a bytes() object rather than a corrupted unicode string when invalid UTF-8 sequences are found
 * make GDALAttribute.Write() accept a dictionary for JSON attributes, and make GDALAttribute.Read() return a dictionary for a JSON attribute
 * fix resampleAlg=gdal.GRA_Sum/Med/Min/Max/Q1/Q3 in gdal.Warp()
 * Fix inversion between GRA_ and GRIORA_ enums in gdal.Warp()/Translate()/BuildVRT()
 * fix bilinear, cubic, cubicspline resampling for gdal.BuildVRT()
 * make GetMetadata(None) work
 * fix MDArray.ReadAsArray() with CInt16/CInt32 data type
 * fix memleak in gdal|ogr|osr.DontUseExceptions()
 * fix crash when reading null strings with MDArray.Read()
 * catch potential exception thrown by SWIG_AsCharPtrAndSize()
 * Build windows python wrappers with -threads argument
 * Python >= 3.8 on Windows: automatically add path with [lib]gdal*.dll with os.add_dll_directory()
 * Python >= 3.8 import: fix exception when PATH has nonexistent paths on Windows (#3898)
 * fix behavior of SetMetadata() when a value is of type bytes (#4292)
 * no longer use distutils (deprecated since python 3.10) when setuptools is available (#4334)
 * remove call to __del__() in Geometry.Destroy(), which no longer exists in SWIG 4

# GDAL/OGR 3.3.0 Release Notes

## In a nutshell...

* RFC 77 (https://gdal.org/development/rfc/rfc77_drop_python2_support.html): Drop Python 2 support in favor of Python 3.6 (#3142)
* RFC 78 (https://gdal.org/development/rfc/rfc78_gdal_utils_package.html): Add a gdal-utils Python package
* New driver:
  - STACTA: raster driver to read Spatio-Temporal Asset Catalog Tiled Assets
* Add /vsiadls/ virtual file system for Azure Data Lake Storage Gen2
* Improved drivers: DIMAP, NITF
* Number of improvements in Python bindings
* Add automatic loading of configuration options from a file
* Add support for enumerated, constraint and glob field domains in MEM, FileGDB/OpenFileGDB and GeoPackage drivers
* Deprecation:
  - Disable by default raster drivers DODS, JPEG2000(Jasper), JPEGLS, MG4LIDAR, FUJIBAS, IDA, INGR, ZMAP and vector driver ARCGEN, ArcObjects, CLOUDANT, COUCHDB, DB2, DODS, FME, GEOMEDIA, GTM, INGRES, MONGODB, REC, WALK at runtime, unless the GDAL_ENABLE_DEPRECATED_DRIVER_{drivername} configuration option is set to YES. Those drivers are planned for removal in GDAL 3.5
  - Perl bindings are deprecated. Removal planned for GDAL 3.5. Use Geo::GDAL::FFI instead
* Removal of BNA, AeronavFAA, HTF, OpenAir, SEGUKOOA, SEGY, SUA, XPlane, BPG, E00GRID, EPSILON, IGNFHeightASCIIGrid, NTV1 drivers. Moved to (unsupported) https://github.com/OSGeo/gdal-extra-drivers repository.
* Continued code linting (cppcheck, CoverityScan, etc.)
* Bump of shared lib major version

## Backward compatibility issues

See MIGRATION_GUIDE.txt

## GDAL/OGR 3.3.0 - General Changes

General:
 * fix build with recent gcc/clang

Build(Unix):
 * Support CharLS 2.1 on Debian as well. (#3083)
 * disable LERC on big-endian hosts, as it is not big-endian ready
 * gdal-config (non installed): add -I/gnm in CFLAGS
 * fix compilation failure with gcc < 5 in Elasticsearch driver
 * configure: Also save LDFLAGS when checking compatibility.
 * configure: Ensure --with-geos/sfcgal fail if unavailable.
 * configure: check presence of linux/fs.h for Linux builds
 * configure: Fix gdal compilation when using proj-8.0.0 and libtiff with static jpeg support
 * GDALmake.opt.in: in non-libtool LD_SHARED builds, do not link applications against libgdal dependencies, but only against libgdal itself

Build(Windows):
 * add missing makefile.vc for heif

## GDAL 3.3.0 - Overview of Changes

Port:
 * GOA2GetRefreshToken(): avoid nullptr dereference in some cases
 * /vsicurl/: for debug messages, use the debug key of derived filesystem, such as 'S3' instead of 'VSICURL'
 * /vsicurl/: fix issue when trying to read past end of file.
 * /vsicurl/: use less restrictive check for S3-like signed URLs to be usable by OpenIO (#3703)
 * /vsicurl/: fix handling of X-Amz-Expires type of signed URLs (#3703)
 * VSICurlStreamingClearCache(): fix cache clearing for /vsiaz_streaming/
 * /vsimem/, /vsisubfile/: fix memleak if destroying the C++ handle with delete instead of using VSIFCloseL() (ossfuzz #28422 #28446)
 * /vsiaz/: fetch credentials from Azure Active Directory when run from a Azure VM when AZURE_STORAGE_ACCOUNT is set.
 * /vsiaz/: add SAS query string in HTTP Get Range requests.
 * /vsiaz/: implement GetFileMetadata() / SetFileMetadata()
 * /vsiaz/: fix Stat('/vsiaz/container') when shared access signature is enabled
 * /vsis3/, /vsiaz/, /vsigs/: set Content-Type from filename extension for a few select file types
 * /vsis3/ sync: improve upload performance by setting the default number of threads and increase the chunk size
 * VSICachedFile::Read(): avoid division by zero if nSize ## 0 (#3331)
 * Make creation of global hCOAMutex thread-safer on Windows (#3399)
 * Add VSIFOpenEx2L() to be able to specify headers such as Content-Type or Content-Encoding at file creation time
 * CPLHTTP: Allow to set GSSAPI credential delegation type with GSSAPI_DELEGATION option / GDAL_GSSAPI_DELEGATION config. option
 * CPLHTTPGetNewRetryDelay(): match 'Operation timed out' curl error
 * CPLDebug: in CPL_TIMESTAMP mode, display also elapsed time since first trace
 * Use _stricmp / _strnicmp for Windows EQUAL / EQUALN() macros

Core:
 * Add automatic loading of configuration options from a file (located in {sysconfdir}/gdal/gdalrc or $(USERPROFILE)/.gdal/gdalrc)
 * RasterIO: massive speed-up of nearest-neighbour downsampling when the downsampling factor is a integer value
 * RasterIO: speed-up average downsampling when the downsampling factor is 2 and with Byte/UInt16/Float32 data type
 * GDALDataset::BlockBasedRasterIO(): make it take into account floating-point window coordinates, as GDALRasterBand::IRasterIO() generic case does  (#3101)
 * GDALOpenEx(): supports OVERVIEW_LEVEL=NONE to indicate overviews shouldn't be exposed
 * GDALCopyWholeRasterGetSwathSize(): take into account COMPRESSION IMAGE_STRUCTURE metadata item at dataset level (helps for DIMAP)
 * cpl_userfaultfd.cpp: enable to disable it at runtime with CPL_ENABLE_USERFAULTFD=NO
 * Python embedding: fix symbol conflicts with python library (#3215)
 * Python embedding: fix loading in QGIS/Windows (or any process with more than 100 modules)
 * Python embedding: make it work on Ubuntu 20.04 when python-is-python3 package that symlinks python to python3 is installed
 * GDALUnrolledCopy_GByte: improve base SSE2 implementation so that specialized SSSE3 is no longer needer
 * RPC: add parameters ERR_BIAS and ERR_RAND (#3484)

Multidim API:
 * CreateCopy() multidim: allow to provide array-level creation options by prefixing them with "ARRAY:"
 * [Set/Get][Offset/Scale](): extend to get/set storage data type. Add 'Ex' suffixed C API functions for that
 * GDALMDArrayUnscaled: implement Write() to be able to get from unscaled values to raw values
 * GDALMDArrayUnscaled::IWrite(): speed optimization when writing to netCDF 4
 * GDALGroup::CopyFrom(): add ARRAY:AUTOSCALE=YES and ARRAY:AUTOSCALE_DATA_TYPE=Byte/UInt16/Int16/UInt32/Int32 creation options
 * CreateCopy(): allow IF(NAME={name}) to be a full qualified name
 * GDALExtendedDataType::Create(): do not allow creating compound data type with no component

Algorithms:
 * Add RMS (Quadratic Mean) subsampling to RasterIO, overviews and warp kernel (#3210)
 * Warping: improve performance related to PROJ usage for warping of small rasters
 * Warper ComputeSourceWindow(): avoid potential integer overflow with a ill-behaved transformer/inverse projection
 * Warper: fix assertion/crash in debug mode in GWKCheckAndComputeSrcOffsets() in some circumstances
 * Pansharpening: fix wrong band assignment when input multispectral bands not in order 1,2,... and NoData set as PansharpeningOptions and not present in input bands
 * internal_libqhull/poly.c: avoid int overflow on 32bit
 * Fix failure in overview generation for certain raster sizes and overview factor, on raster with color table in particular (#3336)
 * GDALSuggestedWarpOutput2(): avoid potential crash if an approximate transformer is provided

Utilities:
 * gdalinfo: add -approx_stats in synopsis (#3296)
 * gdal_translate -tr: make non-nearest resampling honour the specified resolution to avoid potential sub-pixel misalignment when the spatial extent, resolution and dimensions in pixels don't perfectly match
 * gdal_translate: preserve source block size when not subsetting, and preserve as well COMPRESSION IMAGE_STRUCTURE metadata item
 * gdal_translate / gdalwarp: do not copy CACHE_PATH metadata item from WMS driver
 * gdalwarp: make -of COG work with multiple input sources when BigTIFF temporary output is needed (#3212)
 * gdalwarp: preserve scale/offset settings of source bands to output (#3232)
 * gdalwarp: address (one situation of) geometry invalidity when reprojecting the cutline to the source image
 * gdal_create: add a -if option to specify a prototype input file
 * gdal_rasterize: make -i work again (#3124)
 * gdaldem TRI: add a -alg Riley option to use the Riley 1999 formula, for terrestrial use cases (#3320), and make it the new default
 * gdal2tiles: Allow automatic max zoom when min zoom is specified
 * gdal2tiles: change from cdn.leafletjs.com to unpkg.com for leaflet .css and .js (#3084)
 * gdal2tiles: take into account --xyz in leafleft output (#3359)
 * gdal_calc: multiple improvements (nodata, multiple inputs in the same alpha, checks, extent with union/intersection logic, color table, ...)
 * gdal_edit: Correctly handle the error of parameter '-scale' with no number given
 * gdal_ls.py: display file mode when available
 * gdal2xyz.py: various improvements (nodata, band selection, etc.)
 * pct2rgb.py and rgb2pct.py: added support for an input pct_filename
 * gdalattachpct.py: new utility from attachpct.py sample
 * validate_gpkg.py: various improvements
 * add gdallocationinfo.py sample script

Resource files:
 * tms_MapML_CBMTILE.json: fix it to use correct resolution / scaleDenominator. Unfortunately this makes it non-usable by gdal2tiles, but still by the COG driver

Multi driver changes:
 * gdal2tiles/COG/MBTiles/GeoPackage: adjustments for EPSG:3857 output (due to PROJ 8 changes)

BAG driver:
 * fix inversion of width and height in XML metadata (#3605)

BLX driver:
 * fix potential free of uninitialized variable in case memory allocation would fail

BYN driver:
 * relax checks in header bytes to allow products with some unset/invalid fields we don't use

COG driver:
 * allow customising overview compression type (#3453)

DAAS driver:
 * fix pixel retrieval of dataset with UInt16 data type and one mask band (#3061), and also use pixelType from bands[] instead of deprecated top-level one

DIMAP driver:
 * add support for VHR 2020 Multispectral Full Spectrum products
 * add support for multiple components / subdatasets
 * set source block size to DIMAP VRT band (helps for performance with JP2KAK)

ECW driver:
 * fix build with original ECW SDK 5.4 (#2776)

ENVI, Ehdr, and other "raw" drivers:
 * lower memory requirements for BIP interleave, and improve efficiency (#3213)

FIT driver:
 * reject negative value PAGESIZE creation option (ossfuzz #26596)

FITS driver:
 * apply vertical mirroring on reading/writing
 * add support for creation and update of binary tables
 * display more informative error message when opening raster dataset in vector mode, or vice-verca
 * avoid stack smash overflow with 32 bit Linux builds in GetRawBinaryLayout()
 * set physical filename when opening subdataset

GRIB driver:
 * fix reading subfields reusing the bitmap of a previous one (#3099)
 * degrib: avoid erroneous unit conversion when section 4 has vertical values coordinates (#3158)
 * degrib: add surface type 150 = Generalized Vertical Height Coordinate (#3158)
 * Update MRMS Local Table to v12 (#3328)
 * fix writing of ComplexPacking with nodata values and a single valid value (#3352)

GTiff driver:
 * Internal libtiff resync with libtiff 4.3.0. Includes:
   - tif_lerc.c: fix encoding of datasets with NaN values (#3055)
   - TIFFStartStrip(): avoid potential crash in WebP codec when using scanline access on corrupted files (ossfuzz #26650)
 * Export JPEG compression tags to metadata
 * Report PREDICTOR in IMAGE_STRUCTURE dataset metadata domain (when it is set)
 * avoid setting compoundCRS name to 'unknown' when the GTCitationGeoKey is absent
 * better parse VerticalCitationGeoKey that is in the form 'VCS Name = foo|...'
 * rely on libtiff for LERC codec instead of internal libtiff-only codec previously
 * avoid GetSpatialRef() to return non-NULL on dataset with GCPs (#3642)

HDF5 driver:
 * fix reading files whose HDF5 signature is not at offset 0 (#3188)

HFA driver:
 * fix reading SRS with NAD83 favors, like 'NAD83(CORS96)'

JP2KAK driver:
 * JP2KAKCreateCopy(): add validation of BLOCKXSIZE/BLOCKYSIZE to avoid Coverity warning about division by zero (CID 1086659, 1086660)

JPEG driver:
 * add support for reading FLIR (infrared) metadata and thermal image
 * read XMP tag from EXIF and expose its content in xml:XMP metadata domain
 * CreateCopy(): add a warning when writing a non-CMYK 4-band dataset, and improve doc
 * switch internal libjpeg internal memory allocator to default (malloc/free) (#3601)

LercLib (third_party):
 * fix portability issue on non-Intel platforms

MRF driver:
 * Restore raw Lerc1 detection (#3109)
 * Support LERC1 compression improvements with non-finite values (#3335)

netCDF driver:
 * fix reading netCDF4/HDF5 files whose HDF5 signature is not at offset 0 (#3188)
 * accept relative variations in X/longitude and Y/latitude spacing of up to 0.2% for float variables or 0.05% for double. Add a GDAL_NETCDF_IGNORE_EQUALLY_SPACED_XY_CHECK config option to ignore those checks if needed (#3244, #3663)
 * open /vsimem/ files without requiring Linux userfaultfd mechanism
 * Set scope for raster/vector-only options in driver metadata
 * multidim: add a GROUP_BY=SAME_DIMENSION option to GetGroupNames()
 * change the 'No UNIDATA NC_GLOBAL:Conventions attribute' from a warning to a debug message

NITF driver:
 * Add support for SNIP TREs: CSRLSB, CSWRPB, RSMAPB, RSMDCB, RSMECB, SECURA and SNSPSB
 * Add xml:DES metadata domain and DES creation option (#3153)
 * Add support for ISO8859-1 fields in NITF TREs

NWT_GRD driver:
 * fix portability issue on non-Intel platforms

OGCAPI driver:
 * add missing Windows build support
 * fixes to handle links without type, in the coverage code path (Rasdaman use case)
 * fix when coverage uses a compoundCRS with a time component (#3665)

PCIDSK driver:
 * resynchronization of PCIDSK SDK with the internal repository maintained by PCI
 * make GetMetadataItem() returns the same const char* for a given key (while SetMetadata/SetMetadataItem is not called)

PDS4 driver:
 * update value of <parsing_standard_id> for TIFF/BigTIFF to what is expected by PDS4 IM (#3362)

RMF driver:
 * Better support for sparse files. Fill null tiles with NoData value.
 * Fix portability issues on non-Intel platforms

TileDB driver:
 * Add support for array versioning (#3293)
 * do not try to identify /vsis3/ files ending with .tif
 * add /vsigs/ support

VRT driver:
 * add a <UseMaskBand>true</UseMaskBand> element of <ComplexSource> to allow compositing of overlapping sources (#1148)
 * AddBand() support BLOCKXSIZE and BLOCKYSIZE options; serialize/deserialize block size as attributes of VRTRasterBand element
 * VRTPansharpenedDataset: fix crash when the spectral bands have no nodata value, but we have one declared in PansharpeningOptions, and when the VRTPansharpenedDataset exposes overviews (#3189)
 * close sources at dataset closing (#3253)

WCS driver:
 * fix memory leak in error code path (ossfuzz #28345)

WMS driver:
 * tWMS: improves usability with NASA GIBS services (#3463)
 * properly deal with northing,easting ordered CRS in WMS 1.3.0 when using connection string being a GetMap URL request (#3191)
 * add option to specify HTTP Accept header
 * Use noDataValue on empty blocks (#3375)
 * Let pass expected NoDataValue to MRF for reading data (#3388)
 * Update default values for VirtualEarth minidriver

WMTS driver:
 * Map CURLE_FILE_COULDNT_READ_FILE to 404 (#2941)

XYZ driver:
 * support reading datasets organized by columns, such as the ones of https://www.opengeodata.nrw.de/produkte/geobasis/hm/dgm1_xyz/dgm1_xyz

## OGR 3.3.0 - Overview of Changes

Core:
 * Add UUID string field subtype
 * OGRFeature::GetFieldAsString() and GeoJSON output: do not output Float32 with excessive precision
 * IsPolarToWGS84(): make detection of polar projections more specific
 * OGR SQL: fix potential crash or incorrect result on multi column ORDER BY (#3249)
 * Expose prepared geometry API to C and in SWIG bindings
 * OGRGeometry::ExportToJson(): takes into account CRS and axis order to swap
 * OGRShapeCreateCompoundCurve(): fix memory leak in error code path. (ossfuzz #28923)
 * Add OGR_G_Normalize() and bind it to SWIG (#3506)
 * OGRPoint: make it empty when x or y is NaN (refs #3542)
 * Avoid SWIG generating exception on OGRGeometry::IsValid (#3578)
 * OGRGeometry::exportToWkt(): rebustify implementation against out-of-memory, and make it slightly more efficient for some huge geometries
 * OGRSimpleCurve: fix copy constructor / assignment operator of empty Z/M geometries in some cases
 * OGRGeometry: make sure that the return type of clone() for all classes is their own type
 * OGRGeometry: make getGeometryRef() in subclasses return a better type
 * OGRGeometry: Handle WKB > 2 GB. Add OGR_G_WkbSizeEx() and OGR_G_CreateFromWkbEx()
 * Add OGRSimpleCurve::removePoint()

OGRSpatialReference:
 * Fix exportToWkt() after morphToESRI() on Geographic/Projected 3D CRS.
 * GetEPSGGeogCS(): make it use database lookups if needed (and avoid misidentification of 'NAD83(CORS96)' for example
 * exportToWKT(): add a ALLOW_ELLIPSOIDAL_HEIGHT_AS_VERTICAL_CRS=YES option to allow export of Geographic/Projected 3D CRS in WKT1_GDAL as CompoundCRS with a VerticalCRS being an ellipsoidal height (for LAS 1.4)
 * Add OGRCoordinateTransformation::TransformWithErrorCodes() and OCTTransform4DWithErrorCodes() to get PROJ (>= 8) error codes
 * Add OCTCoordinateTransformationOptionsSetDesiredAccuracy() and OCTCoordinateTransformationOptionsSetBallparkAllowed(), and map them to SWIG
 * GetEPSGGeogCS(): fix when projected CRS can't be exported to WKT2
 * SetFromUserInput() allow https:// when loading CRS from opengis.net (#3548)
 * Add Get/Set for PROJ auxiliary database filenames (#3590)
 * Delegate to PROJ (>= 8.1) import from OGC URN and URL (OSGeo/PROJ#2656)

Utilities:
 * ogrinfo: report field domain type and add '-fielddomain name' switch
 * ogr2ogr: add -emptyStrAsNull option
 * ogr2ogr: add propagation of field domains
 * ogr2ogr: add a -resolveDomains switch

AmigoCloud driver:
 * Fix date/datetime field type handling.
 * Fix json encoder to handle nested strings. (#3483)
 * Fix SQL delete (#3512)

CAD driver:
 * fixes for big endian hosts

DGN driver:
 * avoid assert() in case of very low memory condition (ossfuzz #27006)

DGN and DGNv8 driver:
 * Support for reading User Data Linkage data (#3089)

DXF driver:
 * Skip hatch polyline segments with one vertex

Elasticsearch driver:
 * support WKT geo_shape encoding, and make GetExtent() use server-side query for geo_shape if ES >= 7.8 and XPack is installed
 * fix GetFeatureCount() when SetAttributeFilter() is a ElasticSearch JSON filter
 * add open options for timeouts and maximum number of features

FlatGeobuf driver:
 * add editable capabilities
 * fix crash when writing a geometry collection with an empty polygon (ossfuzz #29291)
 * fix crash in GetFileList() on a dataset opened in update mode
 * fix crash when writing features larger than 32 KB

GeoPackage driver:
 * Add support for GeoPackage 1.3 (creation still defaults to 1.2)
 * Fix handling of invalid SRS ID (#3286)
 * always write milliseconds in a DATETIME field for strict compliance with the spec (#3423)
 * no longer create triggers on gpkg_metadata and gpkg_metadata_reference
 * Take into account OGR_CURRENT_DATE config option in gpkg_metadata_reference table (fixes #3537)
 * Remove creation support for data_type='aspatial' legacy tables
 * fix update of gpkg_metadata_reference when renaming or dropping a column, and this table references it

GeoJSON driver:
 * writer: avoid invalid .0 suffix to be added to numeric values like 1eXX (#3172)
 * writer: avoid CPLDebug() messages when writing BBOX in RFC7946 mode
 * reader: accept files starting with {"bbox":...,"features":[... (#1537)
 * reader: fix opening of file containing a Feature object and starting with a large properties (fixes #3280)

GML driver:
 * fix layer extent with wrong axis order in some cases (#3091)
 * recognize AIXM ElevatedSurface to be able to proper axis swapping (#3091)
 * GML geometry parsing: handle km unit for radius of arcs (#3118)
 * fix nullptr dereference on invalid GML (ossfuzz #28040)

LVBAG driver:
 * Update to the new XSD schema (#3324)
 * Minor fixes (#3467, #3462)
 * Convert MultiPolygon to Polygon (#3581)

MapInfo driver:
 * Support for font and custom symbols (#3081)
 * Explicitly set pen cap and join params for pen tools

NAS driver:
 * don't skip prescan, if NAS_GFS_TEMPLATE is given, but still don't     write .gfs

NGW driver:
 * Add extensions support.
 * Add alternative field name support.

OAPIF driver:
 * report url of STAC asset items

ODBC driver:
 * Remove text trimming behavior

PGeo driver:
 * don't silently ignore rows with NULLs for Memo fields

PostGIS driver:
 * on reading, instantiate SRS from EPSG when possible instead of srtext to avoid axis order issues, and strip TOWGS84 on known datum (#3174)
 * when detecting srid from table contents, ignore null geometries.

Shapefile driver:
 * writer: do not write coordinates with non-finite values (#3542)

SQLite driver:
 * recognize col decltype 'INTEGER_OR_TEXT' as a string
 * better detection of view column types

SXF driver:
 * fix reading on big-endian architectures

WFS driver:
 * fix CreateFeature() for WFS 2 (#3323). And for WFS 2, make CreateFeature() and SetFeature() use GML 3

XLSX driver:
 * fix read cells with inline formatting (#3729)

## SWIG Language Bindings

All bindings:
 * avoid nullptr deref if providing some NULL arguments to gdal.Debug() and gdal.GOA2xxxxx() methods
 * fix return type of Dataset.SetSpatialRef()
 * fix expansion of ODsCRandomLayerWrite constant
 * add missing GRA_Sum constant (#3724)

CSharp bindings:
 * Replace ToInt32 calls with ToInt64 to prevent an ArithmeticException when executing against large images in a 64-bit context.
 * Sign csharp assemblies as part of the netcore build process, verify signature during tests (#1368) (#3332)
 * fix 'make' target on Linux/Mac

Java bindings:
 * Add CoordinateTransformation.TransformPointWithErrorCode()

Python bindings:
 * Add Dataset.WriteArray()
 * Make Dataset.ReadRaster() and ReadAsArray() accept floating-point coordinates (#3101)
 * Make methods such as WriteRaster() accept a bytearray object
 * Return bytearray object as return of ReadRaster(), ReadBlock(), VSIFReadL(), MDArray.Read(), ...
 * Add a band_list optional argument to Dataset.ReadAsArray()
 * Make methods such as WriteRaster() accept any memoryview compatible of PyBUF_SIMPLE
 * Make ogr.GetFieldAsBinary(), geom.ExportTo[Iso]Wkb() return a bytearray instead of a bytes object
 * Make ReadRaster() and ReadBlock() accept an existing buf_obj buffer
 * Make ReadAsArray(buf_obj = ar) fail if ar.flags.writable is False
 * Make WriteRaster() accept a numpy array (when used without the buf_ overriders)
 * Allow list or array types as input of MDArray.Write() for 1D arrays
 * Prepare all samples scripts for reuse
 * Add osgeo_utils.auxiliary submodules for auxiliary functions.
 * [epsg_tr|esri2wkt|gcps2vec|gcps2wld|gdal_auth|gdalchksum|gdalident|gdalimport|mkgraticule].py - move undocumented untested utils to the `samples` subfolder where they belong
 * Implement correct behavior for copy.copy() and copy.deepcopy() on a Geometry object (#3278)
 * Add CoordinateTransformation.TransformPointWithErrorCode()
 * Remove deprecated NumPy aliases for standard types.
 * Better validation of Band.ReadAsArray() input shape (#3466)
 * Dictionary typemap: accept key/value not being strings
 * Add batch_creator.py, a Windows batch file creator
 * Fix 'make generate' target on Unix builds (#3696)

# GDAL/OGR 3.2.0 Release Notes

## In a nutshell...

 * New GDAL drivers:
   - ESRIC: ESRI bundle cache read-only driver (#2663)
   - HEIF: read-only driver for HEIF/HEIC file. Requires libheif
   - OGCAPI: tiles/maps/coverage raster/vector experimental driver
   - TGA: read/only driver to read TGA image file format
 * New OGR drivers:
   - LVBAG: read-only support for Dutch LVBAG/Kadaster 2.0 vector format
 * New utilities:
   - gdal_create: to create/initialize a new raster file
 * Other improvements:
   - Multi-threaded overview computation (if GDAL_NUM_THREADS set)
   - COG driver: TILING_SCHEME creation option
   - OpenFileGDB driver: add support for using spatial indexes
   - BAG driver: multiple improvements
   - FITS driver: multiple improvements (MEF and binary table support)
   - NITF driver: support for SNIP TREs
   - OGRFieldDefn: support UNIQUE constraint
   - OGRFieldDefn: support a AlternativeName (alias) property (#2729)
   - Python bindings: move implementation of scripts (except gdal2tiles) in osgeo.utils package to be reusable
   - Faster GTIFF Deflate compression/decompression through libdeflate (if using internal libtiff or libtiff > 4.1.0)
 * Removed functionality:
   - Python bindings: old-style "import gdal" is no longer available. Use "from osgeo import gdal" instead
   - API_PROXY mechanism: likely never used for real usage.
   - Removal of GDAL and OGR ArcSDE drivers

## Backward compatibility issues

See MIGRATION_GUIDE.txt

## New installed files

 * data/tms_NZTM2000.json
 * data/tms_LINZAntarticaMapTileGrid.json
 * data/tms_MapML_APSTILE.json
 * data/tms_MapML_CBMTILE.json
 * data/template_tiles.mapml

## GDAL/OGR 3.2.0 - General Changes

General:
 * fix building against Jasper 2.0.19 and 2.0.21
 * Add optional libdeflate dependency for faster Deflate compression/decompression

Build(Unix):
 * GNUmakefile: split long line to avoid 32K character limitation on MSYS2
 * configure: Configure proj dependencies before proj (#2512)
 * configure. add $with_proj_extra_lib_for_test n LIBS when detecting PROJ when no path is specified (#2672)
 * configure: fix CharLS 2.1 detection on case insensitive filesystems (#2710)
 * configure: fix --with-hdf4 and --disable-all-optional-drivers (#2740)
 * GNUmakefile: make 'all' target an alias of the default one to avoid potential double build of OGR objects (fixes #2777)
 * configure: add ODA lib requirements to compile GDAL with ODA 2021 (#2812)
 * configure: fix detection of minor version number of Poppler with the new YY.MM.X numbering scheme (#2823)
 * configure: fix detection of Spatialite 5 build against PROJ >= 6 (#2826)
 * configure: fix detection of libtiff and libjpeg on mingw (fixes #2881)
 * configure: fix linking order for Informix libraries
 * configure: support CharLS 2.1 on Debian as well. (#3083)
 * configure: disable LERC on big-endian hosts, as it is not big-endian ready
 * fix compilation issue of gdallinearsystem.cpp on Slackware 14.2 (#2883)
 * Update scripts/gdal-bash-completion.sh

Build(Windows):
 * nmake.opt: mention shell32.lib in PROJ_LIBRARY
 * nmake.opt: Add a PYEXEC var to specify the python executable when building GDAL/bindings on Windows.
 * nmake.opt: add hint about adding ole32.lib to PROJ_LIBRARY for PROJ 7.1 when static linking (#2743)
 * only expand CPL_DLL to__declspec(dllexport) when building GDAL (shared configuration) (#2664)
 * add missing Windows build support for EXR driver
 * generate_vcxproj.bat: add vs2019 compatibility (#2676)
 * fix build in AVC driver in VSIL_STRICT_ENFORCE mode

## GDAL 3.2.0 - Overview of Changes

Port:
 * cpl_json.h: change the Type and PrettyFormat 'enum' to 'enum class', so that Double doesn't conflict with a C enum from OGDI include files
 * cpl_json.h: add an iterator on array items, and fix const correctness of LoadUrl() method
 * Add logging capabilities for /vsicurl, /vsis3 and the like. Add VSINetworkStatsGetAsSerializedJSON() and VSINetworkStatsReset(), CPL_VSIL_NETWORK_STATS_ENABLED and CPL_VSIL_SHOW_NETWORK_STATS config options
 * /vsis3/ and other cloud file system: allow random-write access (for GTiff creation in particular) through temporary local file when CPL_VSIL_USE_TEMP_FILE_FOR_RANDOM_WRITE config option is set to YES
 * /vsis3/: recognize AWS_PROFILE in addition to obsolete AWS_DEFAULT_PROFILE (#2470)
 * /vsis3/: use IMDSv2 protocol to retrieve IAM role and credentials
 * /vsis3/: add CPL_VSIS3_USE_BASE_RMDIR_RECURSIVE=YES for some S3-like APIs do not support DeleteObjects
 * /vsis3/: additional retry on failures on write operations
 * VSIS3FSHandler::SetFileMetadata(): fix memleak
 * Improved AWS EC2 detection on Windows
 * /vsicurl/: defer reading of CPL_VSIL_CURL_CHUNK_SIZE and CPL_VSIL_CURL_CACHE_SIZE config options until a /vsicurl/ access (or other network filesystem) is done
 * /vsiaz/: do not consider directory absence as an error situation for Rmdir()
 * /vsiaz/: fix OpenDir()/NextDirEntry() that returned S_IFDIR for regular files
 * /vsiaz/: update to latest version of API (2019-12-12)
 * /vsiaz/: add a AZURE_NO_SIGN_REQUEST=YES config option for unsigned public buckets and AZURE_SAS to provided Shared Access Signature. Both to be used with AZURE_STORAGE_ACCOUNT (#2766)
 * /vsiaz/: ReadDir(): fix caching of file properties with space in the name
 * VSISync(): implement chunking of large files for /vsiaz/ upload when CHUNK_SIZE and NUM_THREADS are set
 * VSISync(): split large files on download/upload for /vsis3/ and /vsiaz/ (#2786)
 * VSISync(): add a SYNC_STRATEGY=OVERWRITE to always overwrite target file
 * RmdirRecursive(): use OpenDir() in recursive mode to have more efficient directory deletion on /vsiaz/ for example
 * /vsimem/: fix normalization of slashes in filenames to avoid potential infinite loop in VSIDirGeneric::NextDirEntry()
 * VSI plugin: add caching option (#2901)
 * VSI plugin: add callback to explicitly list sibling files (#2980)
 * CPLWorkerThreadPool: add capability to create several job queues
 * CPLStrtodDelim(): recognize '1.#SNAN' as a NaN value
 * CPLAtoGIntBigEx(): use strtoll() when available since POSIX doesn't guarantee atoll() will return ERANGE (and libmusl for example does not return it)
 * CPLIsFilenameRelative(): treat "scheme://.." filenames as absolute
 * Add GDAL_CURL_CA_BUNDLE environment variable (#3025)
 * CPLHTTPFetch: fix reading FORM_FILE_PATH on Windows (#2971)
 * Add CPLHTTP[Push|Pop]FetchCallback() and CPLHTTPSetFetchCallback() callback mechanisms to replace curl implementation (to be used by QGIS)
 * VSI_CACHE: do not trust unreliable file size from underlying layer as in fixes #3006
 * CPLMultiPerformWait(): use curl_multi_poll() for curl >= 7.66

Core:
 * Overview building: add multi-threading of resampling computations when GDAL_NUM_THREADS config option is set
 * add a TileMatrixSet class to parse OGC Two Dimensional Tile Matrix Set 17-083r2 JSON encoded definitions, and add NZTM2000 and LINZAntarticaMapTileGrid definitions
 * Add GDALAutoCreateWarpedVRTEx function to GDAL API to accept extra options for transformer (#2565)
 * add a global thread pool mechanism
 * GDALDataset::IRasterIO(): Fixes wrong IO of subpixel shifted window (#2057)
 * GDALDriver::QuietDelete(): partially revert 3.1 commit f60392c8
 * gdal_priv.h: export EXIFExtractMetadata() for plugin uses
 * DumpJPK2CodeStream(): fix reported offset of EOC marker when PSOT = 0 (#2724)
 * Overview generation: fix progress percentage when using USE_RRD=YES (#2722)
 * reader_geo_eye.cpp and reader_pleiades.cpp: avoid potential issue with overuse of per-thread CPLPath static buffers
 * GDALDataset::IRasterIO(): make it try overviews when non-nearest resampling is done before doing RasterIOResampled() on full resolution dataset
 * Workaround issue with UTF-8 precomposed vs decomposed encodings on MacOS filesystems that affect sidecar file discovery (#2903)
 * GDALBuildVRT(): add support for sources such as MEM dataset or non-materialized VRT files
 * GDALDataset::BlockBasedRasterIO(): make it take into account floating-point window coordinates, as GDALRasterBand::IRasterIO() generic case does  (#3101)

Multidim API:
 * GetMask(): use underlying parent data type as much as possible, instead of double, to avoid potential performance issues
 * GetMask(): optimize when we know the mask is always at 1, and when output buffer is contiguous Byte
 * Add GDALGroupOpenMDArrayFromFullname() and GDALGroupOpenGroupFromFullname(), and exose them to SWIG (#2790)
 * Add GDALGroup::ResolveMDArray() and map it to C and SWIG
 * Add GDALMDArrayGetStatistics(), GDALMDArrayComputeStatistics() and GDALDatasetClearStatistics()
 * Make sliced and transpose arrays access the attributes of their source array
 * add GDALMDArrayAdviseRead() and implement it in netCDF driver for DAP datasets

Algorithms:
 * Warper/transformer: avoid error about invalid latitude when warping a dataset in Geographic CRS whose north/south lat is > 90deg (#2535)
 * Warper: use gcore global thread pool when doing multithreaded operations
 * Warper: fix average resampling that lead to very wrong results in some circumstances (#2665) (3.1.0 regression)
 * Warper: ComputeSourceWindow(): modify extra source pixel computation in anti-meridian warping situations
 * Warper: fix computation of kernel resampling width when wrapping across the antimeridian
 * Warper: emit error message instead of assertion if cutline is not a (multi)polygon (#3037)
 * RPC transformer: Add RPC_DEM_SRS option to override SRS of RPC_DEM. (#2680)
 * TPS transformer: fix handling of duplicated GCPs (fixes #2917)
 * Polygonize: make sure not to use dummy geotransform

Utilities:
 * gdalinfo, gdal_translate, gdalwarp: add a -if switch to be able to specify input driver(s) (#2641)
 * gdalinfo: json output: report nan/inf values as a string instead of invalid JSON
 * gdalinfo: json output: do not escape forward slash
 * gdal_translate (and GTiff driver): copy XMP metadata unless -noxmp is specified (#3050)
 * gdal_viewshed: adjust computation of observer position (#2599)
 * gdaldem: ignore 'nv' entry in color file if there is no nodata value in input file
 * gdalwarp: fix crash if warping a dataset without source or target CRS when -ct is specified (#2675)
 * gdalwarp: improve logic for selecting input overview when target bounds and resolution are specified
 * gdalwarp: better guess of target resolution when target extent is specified (#2754)
 * gdalbuildvrt: for easier understanding, and replicate gdal_translate -of VRT behavior, clamp xSize/ySize of SrcRect/DstRect if outside raster dimensions. Not a fix per se
 * gdalbuildvrt: add support for automatically declaring virtual overviews in very restricted situations
 * gdalbuildvrt: fix -srcnodata / -vrtnodata handling in -separate mode (#2978)
 * gdal_grid: Addition of -tr option (#2839)
 * gdal_contour: major speed up in polygon mode (#2908)
 * gdal2tiles: make general cmd line switches like --formats work without exception (#2522)
 * gdal2tiles: fix issue in nativezoom computation with --profile=raster on a raster whose size is below the tile size
 * gdal2tiles: fix --xyz with -p raster, fix KML generation with --xyz (#2463) and update OpenLayers export to OpenLayers 6.3.1
 * gdal2tiles: add -w mapml output, and possibilities to use custom tiling scheme / profile
 * gdal2tiles.py: fix generation of tiles at high zoom levels when input is small (#2896)
 * gdal2tiles.py: make sure configuration options specified with --config are passed to worker processes (#2950)
 * gdal2tiles.py: fix --profile=raster on a non-georeferenced image (#2998)
 * gdal2tiles: change from cdn.leafletjs.com to unpkg.com for leaflet .css and .js (#3084)
 * gdal_merge.py: fix rounding of source coordinates, when they are very close to an integer, which would otherwise result in a one-pixel shift
 * gdal_calc: support multiple calc arguments to produce a multiband file (#3003)
 * gdal_calc.py: raise exception in case of I/O error. Fixes QGIS #36867
 * gdal_sieve.py: do not write geotransform to the target if the source doesn't have one (#2830)
 * gdalcompare.py - export def find_diff(golden_file, new_file, check_sds=False) into a function for reuse
 * gdalcompare.py: fix floor division in Python 3
 * validate_cloud_optimized_geotiff.py: update to support SPARSE_OK=YES files

Multi driver changes:
 * Driver metadata: fix XML errors in option declarations and add testing (#2656)
 * JPEG2000 drivers: extent signature for JPEG2000 codestream to avoid false positive detections.

LERC:
 * Fixed a bug in Lerc2 decoder (https://github.com/Esri/lerc/pull/129)
 * Fixed a bug in Lerc1 decoder (https://github.com/Esri/lerc/pull/121)

AAIGrid:
 * mention dataset name in errors (#3051)

BAG driver:
 * add support for reading georeferenced metadata
 * remove support for interpolated resampled raster
 * add support for COUNT VALUE_POPULATION strategy
 * support reading other single resolution layers on the root node
 * add support for Create()/warp operation
 * add support for reading the tracking list (with OGR API)
 * fix reporting of vertical CRS
 * fix missing nodes at right and top edges of supergrids in resampling mode (#2737)
 * allow it to be used in multidimensional mode through the generic HDF5 driver
 * fix for big endian arch
 * avoid crash on non-standard dataset

COG driver:
 * add support for using custom TILING_SCHEME using OGC 17-083r2 JSON encoded definitions
 * add ZOOM_LEVEL_STRATEGY creation option
 * add WARP_RESAMPLING and OVERVIEW_RESAMPLING to override general RESAMPLING option (#2671)
 * write information about tiling scheme in a TILING_SCHEME metadata domain
 * add a SPARSE_OK=YES option to create sparse files
 * TIFF/COG: report content of the header ghost area in metadata
 * skip reprojection when source dataset matches reprojection specifications
 * fix rounding issue when computing overview dimensions
 * fix crash if passing an invalid (warp) resampling
 * fix crash when source dataset is non-Byte/non-UInt16 with a color table (fixes #2946)

DIMAP driver:
 * fix loading when R1C1 tile is not present (#2999)

DTED driver:
 * add support for EGM08 for dted metadata "E08" (#3047)

E00grid driver:
 * avoid recursive call in _GetNextSourceChar(). Fixes ossfuzz#25161

ECW driver:
 * fix related to network files with SDK >= 5.5 (#2652)

ENVI driver:
 * write nodata value in 'data ignore value' header field
 * add support for writing south-up / rotation=180 datasets

FIT driver:
 * reject negative value PAGESIZE creation option (ossfuzz#26596)

FITS driver:
 * add support for reading multiple-extension FITS files through subdatasets
 * add support for reading binary tables
 * initialize default geotransform
 * do not emit error if no georeferencing is found

GPKG driver:
 * perf improvement: when inserting more than 100 features in a transaction in an existing layer, switch to a deferred insertion strategy for spatial index entries
 * on reading of gridded coverage data with PNG tiles, select -FLT_MAX as the nodata value (#2857)
 * add support for using custom TILING_SCHEME using OGC 17-083r2 JSON encoded definitions
 * increase limitation of number of tables to 10000, and make it configurable through OGR_TABLE_LIMIT config option as for vector tables
 * more robust and simple logic to build overviews and compute overview factor (#2858)
 * make ST_Transform() fallback to EPSG code when no SRS with the given srs_id can be found in gpkg_spatial_ref_sys
 * CreateSpatialIndex(): slight optimization by bumping batches to 500K features
 * make SELECT DisableSpatialIndex(...) run faster
 * fix when writing a tile with uniform negative values or values > 65535 (when nodata is set) in PNG tiles (#3044)
 * fix GDAL 3.0 regression regarding some update scenarios (#2325)

GRASS driver:
 * simplified to GRASS GIS 7+ only (#2945)
 * fix reading GRASS groups (#2876)

GRIB driver:
 * correctly report PDS template number for messages with subgrids (#3004)
 * avoid rejecting valid product due to security check
 * fix retrieval of nodata value for GRIB1 products (GDAL 3.1 regression, #2962)
 * fix reading subfields reusing the bitmap of a previous one (GDAL 3.1 regression,  #3099)
 * Degrib g2clib: rename symbols of our internal modified copy (#2775)

GTiff driver:
 * add read support for overviews/masks referenced through TIFFTAG_SUBIFD (#1690)
 * add WEBP_LEVEL_OVERVIEW config option to set WebP compression level on overviews
 * fix wrong direction for half-pixel shift with GCPs and PixelIsPoint convention
 * for Geodetic TIFF grids (GTG), report the 'grid_name' metadata item in the subdataset description
 * fix reading/writing GEO_METADATA TIFF tag on big-endian
 * fix importing WGS_1984_Web_Mercator / ESRI:102113 (#2560)
 * use gcore global thread pool when doing multithreaded operations
 * allow multi-threaded JPEG compression. This can help a bit
 * fix potential crash when generating degenerate 1x1 overviews
 * in CreateCopy() mode, avoid closing and re-opening the file handle
 * add earlier check to bail out when attempting JPEG compression with paletted image
 * LERC codec: do not write TIFFTAG_LERC_PARAMETERS several times as it cause spurious directory rewrites, and breaks for example COG creation
 * LERC codec: fix encoding of datasets with NaN values (#3055)
 * SRS reader: interpret infinite value in GeogInvFlatteningGeoKey as 0 (fixes PROJ #2317)
 * support hidden SHIFT_ORIGIN_IN_MINUS_180_PLUS_180=YES open option used by GDALOpenVerticalShiftGrid()
 * Internal libtiff: updated to latest upstream master version
 * Internal libgeotiff: updated to latest upstream master version

HDF4 driver:
 * do not report SDS when there are EOS_SWATH or EOS_GRID in it. Add LIST_SDS open option
 * multidim: fix issue when reading transposed array, and duplicate attribute names (#2848)

HDF5 driver:
 * add support for new 'CSK 2nd generation' (CSG) (#2930)
 * multidim: fixes for big endian host
 * multidim: fix performance issue when reading from sliced array

HFA driver:
 * do not report TOWGS84 when reading SRS with WGS84, NAD27 or NAD83 datums (unless OSR_STRIP_TOWGS84 config option is set to NO) (fixes QGIS #36837)

ISCE driver:
 * avoid crashing division by zero on corrupted datasets. Fixes ossfuzz#24252

ISIS3 driver:
 * make sure that in-line label size is at least 65536 bytes (#2741)

JP2KAK driver:
 * add ORGtparts creation option

JP2OpenJPEG driver:
 * add support for generating files with PLT marker segments (OpenJPEG > 2.3.1)
 * add support to enable multi-threaded encoded (OpenJPEG > 2.3.1)
 * writer: acquire input data in background thread
 * fix reading overviews on Sentinel2 PVI files (343x343 size, with 8x8 tiles) (#2984)

LCP driver:
 * add extension checking in Identify() (#2641)

MRF driver:
 * add support for interleaved LERC (#2846)
 * LERC V1 support for NaN and other bug fixes (#2891)
 * missing data return on initial caching of nodata tiles (#2913)
 * Fixes for Create() (#2923)

NetCDF driver:
 * GrowDim(): fix issue with non-ASCII filename on Windows
 * fix setting offset and scale in CreateCopy()
 * allow a NETCDF:http://example.com/my.nc DAP dataset to be opened
 * multidim: fix performance issue when reading from sliced array
 * multidim: optimize reading into a data type 'larger' than the native one
 * multidim: identify the indexing variable of a dimension through the 'coordinates' attribute of other variables (#2805)
 * multidim: add CHECKSUM and FILTER creation options. Make SetRawNoDataValue() use nc_def_var_fill()
 * multidim: fix retrieval of missing_attribute, etc... when reading mask

NITF driver:
 * add support for various TREs for Spectral NITF Implementation Profile (SNIP): MATESA, GRDPSB, BANDSB, ACCHZB, ACCVTB, MSTGTA, PIATGB, PIXQLA, PIXMTA, CSEXRB, ILLUMB, CSRLSB, CSWRPB
 * Add nested variable support in xml:TRE

PAux driver:
 * avoid ingesting large binary unrelated files (found when investigating #2722)

PCRaster driver:
 * fix Create() mode by propagating eAccess = GA_Update (#2948)

PDF driver:
 * update to pdfium/4272 with https://github.com/rouault/pdfium_build_gdal_3_2

PDS driver:
 * take into account FIRST_STANDARD_PARALLEL for Mercator projection (#2490)

RMF driver:
 * Better support for sparse files. Fill null tiles with NoData value.

RS2 driver:
 * remove support for CharLS compression since it is removed from upstream librasterlite2

SAFE driver:
 * deal correctly with WV swaths (#2843)

TileDB driver:
 * support for pixel interleave and single formats (#2703)

TSX driver:
 * fix issue with reading dataset in .zip file on Windows (#2814)

VICAR driver:
 * fix for Basic compression and non-Byte type on big endian host
 * avoid potential null-dereference on corrupted dataset. Fixes ossfuzz#24254

VRT driver:
 * Add support for explicit virtual overviews. Can be built with gdaladdo --config VRT_VIRTUAL_OVERVIEW YES (#2955)
 * VRTDataset::IRasterIO(): allow source overviews to be used when non-nearest resampling is used, and the VRT bands don't expose overviews (#2911)
 * fix VRTRasterBand nodata handling when creating implicit overviews (#2920)
 * round src/dst coordinates to integer within 1e-3 margin
 * prevent potential infinite recursion in VRTDataset::IRasterIO()

WMS driver:
 * Add a GDAL_MAX_CONNECTIONS config option
 * WMS cache: add a <CleanTimeout> in <Cache> XML configuration (#2966)

WMTS driver:
 * Add support for DataType tag in service description XML (#2794)

## OGR 3.2.0 - Overview of Changes

Core:
 * Add unique constraints to OGRFieldDefn in core, GML, PG, PGDump, GPKG, SQLite and VRT drivers (#2622)
 * Add support for field AlternativeName to OGRFieldDefn, read alias in openfilegdb driver (#2729)
 * Add GDALDataset::AbortSQL (#2953)
 * OGRFeature::GetFieldAsString(): remove 80 character limitation when formatting string/integer/real lists (#2661)
 * Add OGRLayerUniquePtr and OGRExpatUniquePtr aliases (#2635)
 * OGRSQL: take into account second part of arithmetic expression to correctly infer
 result type.
 * OGRSQL: support constructs 'A AND B AND C ... AND N' with many successive AND ((#2989)
 * OGRSQL: Fixed buffer overflow in BuildParseInfo for SQL query when joining multiple tables that each have implicit FID columns.
 * ogr_geometry.h: export OGRWktOptions class (#2576)
 * ogr_swq.h: type nOperation as swq_op

OGRSpatialReference:
 * Make GetAuthorityCode('PROJCS') work on a WKT1 COMPD_CS with a VERT_DATUM type = 2002 (Ellipsoid height)
 * Make OSRGetPROJSearchPaths() return the value set by OSRSetPROJSearchPaths()
 * exportToProj4(): make it add +geoidgrids= when possible (needs PROJ 7.2)
 * Add OSRDemoteTo2D() and expose it to SWIG
 * fix GetUTMZone() to work on 3D projected CRS
 * exportToWkt(): accept FORMAT=WKT2_2019 (alias of WKT2_2018)
 * fixes to avoid crashes with datum ensemble objects (needs PROJ 7.2)
 * Avoid warnings in GetProjTLSContextHolder() when PROJ resource path is not already set (PROJ #2242)
 * ogr_proj_p.cpp: make sure init() is called in OSRPJContextHolder() constructor to avoid potential use of default NULL PROJ context (#2691)
 * fix issue with PROJ context and OSRCleanup() (#2744)
 * Fix exportToWkt() after morphToESRI() on Geographic/Projected 3D CRS. But only works with PROJ 7.2

Utilities:
 * ogrinfo: report field unique constraint
 * ogrinfo: report field alternative name
 * ogrinfo: report SUBDATASETS domain
 * ogrinfo/ogr2ogr: fix issues with -sql @filename where SQL comments are not at start of line (#2811)
 * ogr2ogr: bump default value of -gt to 100 000

Multi driver changes:
 * Avoid copy&paste implementations of GetNextFeature() relying on GetNextRawFeature() through OGRGetNextFeatureThroughRaw class
 * make sure GetNextFeature() always return nullptr after the first time it did (ie no implicit ResetReading()) in GPKG, SQLite, PCIDSK, MSSQLSpatial and MySQL driver
 * better support for layers with field names differing by case in OGR SQL, SQL SQLite and VRT

CSV driver:
 * do not try to read .csvt if CSV filename has no extension (#3006)

DXF driver:
 * Propagate PaperSpace field from INSERTs to subfeatures
 * Fix wrong transformer composition for ASM entities
 * do not rely on tail recursion and avoid potential big stack calls when eliminated 999 comment lines. Fixes ossfuzz#22668

Elasticsearch driver:
 * make OVERWRITE_INDEX=YES work properly by re-creating the index afterwards

ESRIJSON driver:
 * fix GetFeatureCount() and GetExtent()

FileGDB/OpenFileGDB drivers:
 * qualify DateTime values with UTC timezone when <IsTimeInUTC>true</IsTimeInUTC> is present in layer metadata (#2719)

FileGDB driver:
 * generate layer definition XML with HasSpatialIndex=true to better reflect reality.
 * add support for reading and writing field alternative names
 * simplify spatial filtering, and apply full intersection in GetFeatureCount() instead of BBOX one, so as to be similar to OpenFileGDB

FlatGeoBuf driver:
 * Metadata extensions (#2929)
 * fix illegal use of std::vector (#2773)
 * make GetExtent() work on feature write (#2874)

GeoJSON driver:
 * fix opening of file starting with {"coordinates" (#2720)
 * fix opening of file starting with {"geometry":{"coordinates" (#2787)
 * RFC7946 writer: fix processing of geometry that covers the whole world (#2833)
 * writer: use JSON_C_TO_STRING_NOSLASHESCAPE when available to avoid escaping forward slash, and very partial resync of internal libjson-c to get it

GML driver:
 * add a WRITE_GFS=AUTO/YES/NO open option (#2982)
 * writer: correctly format OFTDate and OFTDateTime fields (#2897)
 * hugefileresolver: add missing xmlns:xlink to make Xerces parser happy
 * fix typo in VFR GFS files BonitovaneDilRizeniId -> BonitovanyDilRizeniId (#2685)
 * when encountering XML issue, defer emission of error message until we return a NULL feature, so as to avoid to confuse ogr2ogr (#2730)
 * XSD parser: recognized unsignedLong data type
 * avoid 'Destination buffer too small' error to be emitted on /vsicurl_streaming/ URLs with filters coming from the WFS driver
 * fix layer extent with wrong axis order in some cases (#3091)
 * recognize AIXM ElevatedSurface to be able to proper axis swapping (#3091)

GMLAS driver:
 * avoid running out of file descriptors in case of big number of layers
 * fix so as to get same unique ids on big-endian arch

GPKG driver:
 * add a PRELUDE_STATEMENTS open option that can be used for example to attach other databases
 * add a DATETIME_FORMAT=WITH_TZ/UTC dataset creation option (defaults to WITH_TZ) to specify how to deal with non-UTC datetime values (#2898)
 * hide view "geometry_columns"
 * fix wrong RTree _update3 trigger on existing files (QGIS #36935)

GRASS driver:
 * simplified to GRASS GIS 7+ only (#2945)

GTM driver:
 * on write, do not consider TZFlag=1 (localtime) as a timezone value (refs #2696)
 * on write, take into correctly timezone value to convert to UTC (refs #2696)

LIBKML driver:
 * do not advertise RandomWrite capability (unless on a update layer, when datasource is created with UPDATE_TARGETHREF creation option) (fixes QGIS 39087)

MDB driver:
 * fix warning when parsing 'false' boolean value

MITAB driver:
 * fix reading and writing of Transverse Mercator projections based on KKJ
 * .tab: fix writing empty/null Time fields (#2612)
 * fix reading and writing of non-metre linear units
 * Support for font and custom symbols in mitab (#3081)

MVT driver:
 * fix 'random' failures in test_ogr_mvt_point_polygon_clip() by sorting sub-directory names, and also revise logic to attribute FID when reading directories (#2566)
 * writing: fix crashes in multi-threading mode (#2764)

NAS driver:
 * add support for new GID7 updates
 * also filter for wfs:member (as in GID7)
 * do not try to write a .gfs file when NAS_GFS_TEMPLATE is specified

netCDF driver:
 * simple geometries: fix for big-endian

OAPIF driver:
 * avoid re-adding user query parameters if they are found in URLs returned by the API (#2873)
 * do not list raster or coverage collections
 * support opening of a collection when its URL is non-standard such as in MOAW workflows
 * fix memory leak when reading schema from .xsd

ODBC driver:
 * Allow mdb files to be opened with the generic ODBC driver on non-windows platforms
 * Fix w.r.t fallback to alternative Access ODBC driver name
 * Fix DSN string construction for Windows Access ODBC driver, template candidate preference order (#2878)
 * Correctly handle datetime fields provided by the mdbtools ODBC driver
 * Read MS Access databases with ACCDB and STYLE extensions (#2880)

OCI driver:
 * fix server 12.2 version detection

ODS driver:
 * do not create files with Zip64 extension, to avoid compatibility issue with LibreOffice Calc
 * avoid potential deep call stack in formula evaluation. Fixes ossfuzz#22237

OpenFileGDB driver:
 * add support for reading .spx spatial index file
 * more reliable .gdbtable header reading

OSM driver:
 * remove limitation to 10000 nodes per way (#849)
 * Replace hard-coded tag filter with variable
 * Optionally disable early tag filtering
 * Don't filter out explicit attributes (#2603)

PDF driver:
 * write correctly attribute object dictionary when there is no field to write, and read back correctly broken files we generated before (#2624)

PDS4 driver:
 * fix potential double free if RenameFileTo() fails

PGeo driver:
 * Quote DBQ value in PGEO driver template to avoid issues opening MDB paths with spaces
 * Fixes to automatic ODBC driver installation (#2838)
 * Read non-spatial tables (#2889)

PG driver:
 * Make ogr2ogr -f PostgreSQL work when using PG:service= syntax (actually a workaround in GDALDriver::Create())
 * take into potential generated columns (PostgreSQL >= 12) to avoid issuing INSERT, UPDATE or COPY statements with them
 * PG (and PostgisRaster driver): set the application name in PostgreSQL connections (#2658)
 * apply standard libpq parsing rules for connection parameters for our custom connection parameters (schemas, active_schemas, tables) (#2824)

PGDump driver:
 * change default value of POSTGIS_VERSION layer creation option to 2.2

SEGY driver:
 * avoid opening FITS files

Shapefile driver:
 * do not claim to support DateTime data type
 *  when several candidate SRS are found with confidence >= 90%, take the one from EPSG (contributes to fixes QGIS #32255)
 * SHPRestoreSHX: fix for (64 bit) big endian

S57 driver:
 * apply update to DSID_EDTN field (#2498)
 * report attributes tagged as list in S57 dictionaries as StringList fields. Add a LIST_AS_STRINGLIST open option that can be set to OFF to restore GDAL < 3.2 behavior (#2660)

SXF driver:
 * fixes for big-endian

SQLite/Spatialite driver:
 * add a PRELUDE_STATEMENTS open option that can be used for example to attach other databases

WFS driver:
 * avoid /vsicurl_streaming/ URL to be truncated in case of big filter (but the server might reject it)

XLSX driver:
 * do not create files with Zip64 extension, to avoid compatibility issue with LibreOffice Calc
 * fix numeric precision issue when reading datetime that could lead to an error of 1 second (#2683)

## SWIG Language Bindings

All bindings:
 * OGRDriver.CopyDataSource(): check that source dataset is not NULL
 * validate range of resample_alg algorithm (#2591)
 *  expose CPLSetCurrentErrorHandlerCatchDebug() as gdal.SetCurrentErrorHandlerCatchDebug()

CSharp bindings:
 * Add build support for .NET core (#1368)
 * SWIG 4.0 compatibility (#2802)
 * Adding typemaps C# for wrapper_GDALWarpDestDS and wrapper_GDALWarpDestName (#2621)
 * Expose Dataset.GetSpatialRef() (#2620)
 * Expose OSR.GetCRSInfoListFromDatabase (#1665)
 * Fixed implementation of Utf8BytesToString (#2649)

Java bindings:
 * update minimum source/target Java version to 7 to please JDK 11 (#2594)
 * make 'install' target copy maven artifacts (gdal-X.Y.Z.jar) to /usr/share/java

Perl bindings:

Python bindings:
 * Move implementation of scripts in osgeo.utils package to be reusable
 * Fix Python2 install to be synchronous and report all errors (#2515)
 * add a colorSelection='nearest_color_entry'/'exact_color_entry' argument to gdal.DEMProcessing()
 * accept string as value for gdal.Translate() metadataOptions and creationOptions argument when providing single option (#2709)
 * gdal.Info(): make options=['-json'] work properly
 * remove use of deprecated PyObject_AsReadBuffer() function
 * makefile.vc: remove '-modern -new_repr' on python target for SWIG 4 compatibility
 * add GDALMDArray.shape attribute and GDALMDArray.ReadAsMaskedArray() method
 * make Dataset.ReadRaster() and Dataset.ReadAsArray() accept floating-point coordinates (#3101)

# GDAL/OGR 3.1.0 Release Notes

## In a nutshell...

 * Implement RFC 75: support for multidimensional arrays in MEM, VRT, netCDF, HDF4, HDF5 and GRIB drivers. Read/write for MEM and netCDF. Read/only for others. Add gdalmdiminfo and gdalmdimtranslate utilities.
 * Implement RFC76: add capability of writing vector drivers in Python
 * New GDAL drivers:
   - COG: write-only, for Cloud Optimized GeoTIFF
   - EXR: read/write driver, relying on OpenEXR library
   - ISG: read-only, for geoid models of the International Service for the Geoid
   - RDB: read-only, for RIEGL Database .mpx RDB 2 files (#1538) (needs proprietary SDK)
 * New OGR drivers:
   - FlatGeoBuf: read-support and creation (#1742)
   - MapML: read/write driver for experimental web spec
 * Improved drivers:
  - OAPIF driver (renamed from WFS3): updated to OGC API - Features 1.0 core spec
  - GTiff: improve performance of internal overview creation
  - GTiff: GeoTIFF 1.1 support
  - Shapefile driver: add read/creation/update support for .shz and .shp.zip
  - netCDF vector: read/write support for CF-1.8 Encoded Geometries (#1287)
  - VICAR: multiple improvements and write support (#1855)
  - DDS: add read support
 * Other improvements:
   - gdalwarp: accept output drivers with only CreateCopy() capabilities
   - gdal_viewshed: new utility for viewshed algorithm
 * Remove GFT driver now that the online service no longer exists (#2050)
 * New Sphinx-based documentation
 * Multiple security related fixes (ossfuzz)
 * Continued code linting (cppcheck, CoverityScan, etc.)
 * Compatibility with GDAL 3.0:
    - C and C++ API: backward compatible changes
    - C ABI: backward compatible changes
    - C++ ABI: modified
    - Functional changes: see MIGRATION_GUIDE.TXT

## GDAL/OGR 3.1.0 - General Changes

Build(Unix):
 * use pkg-config for libxml2 detection (#2173)
 * fix detection of libpq in a non-standard place (#1542)
 * do not use absolute path in linking command. Helps Mac OS and cygwin builds (#2075)
 * Enable Bash completions and control installation
 * GDALmake.opt.in: silence datarootdir warning
 * Doc: allow user full control over installation directory
 * fix JVM detection for HDFS support on MacOS (#2313)
 * Remove #define HOST_FILLORDER from cpl_config.h (#2345)
 * Added search for proj library in lib64 directory.
 * configure: strip -L/usr/lib and similar from netCDF, MySQL, GEOS and SFCGAL lib path (#2395)
 * configure: remove useless -lproj from --with-spatialite detection

Build(Windows):
 * parametrize number of CPUs for parallel builds with CPU_COUNT variable (#1922)
 * add HDF5_H5_IS_DLL variable to switch the scenario when HDF5 is built as a DLL (#1931)
 * add POSTFIX that defaults to _d for GDAL .dll, .lib and .pdb for DEBUG builds (#1901)
 *  Fix issues with thread_local and C++ objects that don't work well with DLL on Windows

All:
 * Support Poppler 0.82, 0.83, 0.85

## GDAL 3.1.0 - Overview of Changes

## Algorithms

* Warper: add sum resampling method (#1437)
* Average resampling (warp and overview/translate): use weighted average for border source pixels
* GDALReprojectImage(): properly take into account source/target alpha bands
* GDALCreateReprojectionTransformerEx(): do not emit error if reverse transformation fails, and fix crash when trying to use null reverse transformation
* Warper: fix GDAL 2.3 regression in a situation with source nodata value, multiple bands and nearest resampling where the logic to detect which source pixels are nodata was inverted (#1656)
* GWKAverageOrModeThread(): reject invalid source pixels for average/q1/q3/mode/min/max resampling (#2365)
* Multithreaded warper: make sure a transformer object is used by the thread which created it (#1989). This workarounds a PROJ bug also fixed per https://github.com/OSGeo/PROJ/pull/1726
* Contour: fix SegmentMerger list iterator skipping and out of bounds error (#1670)
* Contour: fix (over) precision issue when comparing pixel value to NoData on Float32 rasters (#1987)
* Contour: add sanity checks for interval based contouring, in case the dataset contains extreme values regarding the settings, which would lead to a lot of memory allocations / too large computation time
* TPS warper: enhance precision without armadillo support (#1809)
* RPC warper: fix issue when source image has a geotransform (#2460)
* GDALRasterizeGeometries(): fix potential integer overflow / memory allocation failure, depending on GDAL_CACHEMAX and raster dimensions (#2261)
* Rasterize: speed optimization for geometry collections (#2369)
* GDALContourGenerate(): propagate raster acquisition error (#2410)

## Port

* /vsitar/: support >100 character file names (#1559)
* /vsitar/: accept space as end of field terminator
* /vsigz/: fix seeking within .gz/.tgz files larger than 2 GB (#2315)
* /vsicurl (and derived filesystems): fix concurrency issue with multithreaded reads (#1244)
* /vsicurl/: avoid downloading one extra block when the end offset is just at a chunk boundary
* /vsicurl/: fix CPL_VSIL_CURL_ALLOWED_EXTENSIONS with query string (#1614)
* /vsicurl/: allow 'Connection timed out' CURL errors as candidate for HTTP retry
* /vsicurl/: GetFileSize(): when HEAD request does not return Content-Length header, retry with GET
* /vsis3/: for a long living file handle, refresh credentials coming from EC2/AIM (#1593)
* /vsis3/: invalidate cached non-existing file is AWS_ config options are changed in the meantime (#2294)
* /vsis3/ /vsigs/ /vsiaz/: implement Rename() first doing a copy of the original file and then deleting it
* /vsis3/ and similar: add a NUM_THREADS option to Sync() for parallelized copy
* AWS: Fix error in loading ~/.aws/config file (#2203)
* VSISync(): when copying from /vsis3/ to /vsis3/ (or /vsigs/->/vsigs/, /vsiaz/->/vsiaz/), use CopyObject() method to use server side copy
* VSISync(): make file copying from /vsis3/ actually use /vsis3_streaming/ to reduce number of GET requests
* VSISync(): add a CHUNK_SIZE option to Sync() to split large objects and get parallelization of their download and upload
* Add VSIUnlinkBatch() for batch deletion of files, and add optimized /vsis3/ implementation
* Add efficient VSIRmdirRecursive() implementation for /vsis3/
* Add VSIGetFileMetadata()/VSISetFileMetadata() and implement them to get/set HTTP headers, and AWS S3 object tagging
* /vsis3/ and other network filesystems: avoid useless network requests when we already got a directory listing (#2429)
* /vsiswift/: V3 authentication method, handling auth token expiration
* /vsimem/: make Rename() error if destination file is not in /vsimem/
* /vsizip/ writing: in ZIP64 mode, also advertise 45 as the version in the central directory (avoids a warning from 'zip' utility)
* Add CPLCanRecode function and use it in MITAB, Shape and SXF drivers to decide when to advertise UTF-8 capability
* CPLConfigOptionSetter: only reset thread-locale value, not global one
* CPLJSONObject::GetType(): return Long when the value doesn't fit on a int32
* CPLJSON: distinguish Null type from Unknown/invalid type
* CPLEscapeString(): escape double-quote for CPLES_URL
* Add a CPLCondTimedWait()
* cpl_safemaths.hpp: safe + and * for GUInt64
* Add CPLJSonStreamingWriter class
* Add cpl_error_internal.h with logic with error accumulator
* Add VSIOverwriteFile()
* Add CPLLaunderForFilename()
* cpl_error.h: add a CPLDebugOnly() macro that expands to CPLDebug() only for DEBUG builds
* QuietDelete: support expliciting the drivers to use

## Core

* Block cache: fix corruption on multithreaded write on datasets (#2011)
* GDALInvGeoTransform(): make it work with scale and rotation/skew coefficients of small absolute value (#1615)
* GDALCopyWholeRasterGetSwathSize(): fix potential int overflows for big values of GDAL_SWATH_SIZE or GDAL_CACHEMAX
* PAMRasterBand: add presence flag for Offset and SetScale so that GetOffset()/GetScale() properly reports if they are defined in PAM
* GDALDefaultOverviews::BuildOverviews(): fix typo in detection of 1x1 overview (#1730)
* Fix precision loss at GDALResampleChunkC32R with complex data
* Fix precision loss at GDALComputeBandStats with complex data
* Proxy pool: Load band block sizes if not provided at creation. Fixes floating point exception on copy overviews from PRF dataset to destination dataset.
* GTiff and PAM: allow serializing WKT2 for SRS using non-WKT1 compatible projections such as Vertical Perspective (#1856)
* RasterIO(): fix non-neareset resampling over nodata blocks (#1941)
* Overview dataset (-oo OVERVIEW_LEVEL): expose mask if the source dataset has a mask with overviews
* GDALUnrolledCopy<GByte,2,1>: fix SSE2-only implementation (when SSSE3 is not available, on older AMD CPUs)
* PAM: support reading GCPs from ESRI GeodataXform in .aux.xml files
* Make it possible to call BuildOverviews() on a dataset returned by GDALBuildVRT()
* GDALDataset::SetProjection(): re-allow the use of PROJ4 strings (#2034)

## Utilities

* GDALInfo(): fix axis order issue in long,lat corner coordinates, in particular when reading from a .aux.xml with a ProjectedCRS (#2195)
* gdal_translate: Make 'gdal_translate foo.tif foo.tif.ovr -outsize 50% 50% -of GTiff' work
* gdal_translate: clamp/round source nodata value when not compatible of the target data type. Was already done when using -a_nodata, but not with implicit copy (#2105)
* gdalwarp: accept output drivers with only CreateCopy() capabilities
* gdalwarp: adjust nodata values, passed with -srcnodata/-dstnodata, and close to FLT_MAX to exactly it (#1724)
* gdalwarp: fix wrong axis order when using source/target CRS being a geographic3D CRS such as EPSG:4979, and with vertical shift grid application (#1561, GDAL 3.0 regression)
* gdal_contour: remove explicit width/precision=12/3 of the elev field (#1487)
* gdal_contour: turn on quiet mode if output dataset is standard output (refs #2057)
* gdaldem: avoid potential integer overflow in color-relief mode (#2354)
* gdal_calc.py: fixed NaN-streaking in output images when the --allBands option is given to tiled images
* gdal_polygonize.py: fix outputting to geojson without explicit -f switch (#1533)
* gdalcompare.py: take into account mask bands
* gdal_retile.py: add resume option (#1703)
* gdallocationinfo: emit verbose error when dataset cannot be opened (#1684)
* gdallocationinfo and gdaltransform: print a hint when values are expected from the command line and stdin is an interactive terminal (refs #1684)
* Python utilities: fix GetOutputDriverFor() when multiple drivers found (#1719)
* gdal2tiles.py: add remaining resample methods
* gdal2tiles.py: add option for setting the tile size (#2097)
* gdal2tiles.py: add --xyz option to generate tiles in the OSM Slippy Map standard (#2098)
* gdal2tiles.py: show warning when running against non-Byte input (#1956)
* gdal2tiles.py: update cache calculation (#2020)
* gdal2tiles.py: check that min zoom <= max zoom (#2161)
* gdal2tiles.py: ignore nodata values that are not in the range of the band data type
* gdal2tiles.py: fix hang when --s_srs specified but image lacks georeferencing
* gdal_translate / gdalwarp / ogrct: allow dealing with non-WKT1 representable SRS (#1856)
* gdal_edit.py: add a -units switch
* gdal_edit.py: add -a_ulurll switch
* gdal_fillnodata.py/GDALFillNodata: fix crash when smooth_iterations is used, and with some progress functions such as the one used by Python (#1184)
* Python scripts and samples: use python3 for shebang (#2168)

## Sample scripts

* Add tile_extent_from_raster.py: sample script to generate the extent of each raster tile in a overview as a vector layer
* Add gdal_remove_towgs84.py script

## GDAL drivers

Multiple drivers:
 * GTiff, GPKG, MBTiles, PostgisRaster drivers: share lock of overview dataset with parent dataset (#1488)
 * HDF5 and netCDF: fix crash when reading attributes of type string of variable length with NULL values
 * CTable2/LOSLAS/NTv1/NTv2: document in metadata that positive longitude shift values are towards west
 * Revise raster drivers GDAL_DMD_HELPTOPIC
 * JP2KAK and JP2OPENJPEG: fix to read images whose origin is not (0,0)
 * Strip TOWGS84 when datum is known, in GTiff, Spatialite and GPKG drivers

BAG driver:
 * modify way georeferencing is read (particularly pixel sizes and origin) (#1728)
 * Backward compatibility for metadata reading for BAG < 1.5 (#2428)

BSB driver:
 * Report PLY coordinates as a WKT POLYGON in a BSB_CUTLINE metadata item

BYN driver:
 * fix nodata value for Int32 encoded products

DAAS driver:
 * accept 4-band RGBA PNG response even when a single band is requested

DIMAP driver:
 * don't look inside Dataset_Components if Raster_Data is present
 * avoid reparsing xml if already a product dimap
 * ignore missing strip xml file

DDS driver:
 * add read support

DTED driver:
 * Add DTED_APPLY_PIXEL_IS_POINT environment switch  (#2384)
 * emit a CE_Failure instead of a CE_Warning in case of checksum verification failure (#2410)
 * support E96 as well as MSL for COMPD_CS (#2422)

EEDA driver:
 * fix startTime / endTime comparisons that were incomplete in #1506

FITS driver:
 * fix memory leaks

ENVI driver:
 * add read support for reading GCPs (#1528), and fix off-by-one offset on line,pixel on reading GCP
 * fix potential use of invalid pointer on some unusual std::string implementations (#1527)
 * preserve 'byte order' on update (#1796)

GPKG driver:
 * support opening subdataset of /vsicurl/ files (#2153)

GTiff driver:
 * improve performance of internal overview creation (#1442)
 * in COPY_SRC_OVERVIEWS=YES, interleave mask with imagery, and add leader/trailer to strile
 * optimize read of cloud-optimized geotiffs
 * do not generate a TIFFTAG_GDAL_METADATA with color interpretation information for JPEG YCbCr compression
 * make -co NUM_THREADS produce reproducible output
 * make overview blocksize defaults to same as full-resolution
 * move deferred tile/strip offset/bytecount loading to libtiff
 * make WEBP_LEVEL to be honored in Create() mode (fixes #1594)
 * PushMetadataToPam(): early exit when PAM is disabled, to avoid error messages
 * remove support for libtiff 3.X
 * set a LAYOUT=COG metadata item in the IMAGE_STRUCTURE metadata domain of the dataset when the hidden feature declarations typical of a COG file are found
 * fix memory leak with -co APPEND_SUBDATASET=YES
 * fix error message for NBITS != 16 and Float32
 * add explicit error message when trying to create a too big tiled TIFF file (refs #1786)
 * ensure GDAL PROJ context is used for all libgeotiff functions (requires internal libgeotiff / libgeotiff 1.6)
 * make sure that GetMetadataDomainList() doesn't return EXIF when there's no EXIF metadata
 * GTIFGetOGISDefn: avoid querying UOM length info when it is KvUserDefined to avoid an error to be emitted
 * on CRS reading, avoid unsetting of EPSG code when ProjLinearUnitsGeoKey = Linear_Foot_US_Survey and the CRS also uses that unit (#2290)
 * do not write in GeoTIFF keys non-standard projections
 * do not write TOWGS84 that come from EPSG codes, unless GTIFF_WRITE_TOWGS84=YES is explicitly set
 * GTiff writing: workaround PROJ 6.3.0 bug when writing a EPSG:4937 ETRS89 Geog3D CRS
 * GTiff writing: do not write by default EPSG:3857/WebMercator as a ESRI PE string. Fixes 3.0 regression
 * avoid crash on single-component file with Whitepoint and PrimaryChromaticities tags
 * libtiff: BigTIFF creation: write TileByteCounts/StripByteCounts tag with SHORT or LONG when possible
 * Internal libgeotiff: set UOMLength from GeogLinearUnits (for geocentric CRS) (#1596)
 * Internal libgeotiff: upgrade to libgeotiff 1.6.0dev to support OGC GeoTIFF 1.1
 * Internal libtiff: resync with internal libtiff (post 4.1.0)

GRIB driver:
 * do not do erroneous K->C unit conversion for derived forecasts whose content is not a temperature, but a derived quantity, such as spread
 * avoid erroneous K->C conversion for Dew point depression
 * update GRIB tables to degrib 2.24
 * add missing entries in MeteoAtmoChem table
 * add more values from Table 4.5 in Surface type table
 * add support for template 4.48 Optical Properties of Aerosol
 * add support for rotated lat-lon grids
 * avoid quadratic performance on GRIB2 datasets using subgrids within a single GRIB message (#2288)

GXF driver:
 * avoid closing the file pointer before being sure this is a GXF driver, otherwise this can prevent the opening of some raw format files (#1521)

HDF4 driver:
 * fix retrieval of non-string swath and grid attributes
 * fix GR support for non-Byte rasters
 * remove broken attribute reading
 * fixes related to color table

HDF5 driver:
 * avoid error report of the HDF5 library when _FillValue attribute is missing
 * Make GH5_FetchAttribute(CPLString) more robust and able to cope with variable-length string
 * type detection: only detect complex data type if the component names start with r/R and i/I (refinement of #359)
 * add a way to open datasets split over several files using the 'family' driver
 * fix reading single dimension dataset (#2180)

HFA driver:
 * fix writing of compressed file when a RLE run count is in the [0x4000,0x8000[ range or [0x400000, 0x800000[ (#2150)

IGNFHeightASCIIGrid driver:
 * fix to read RASPM2018.mnt grid

IRIS driver:
 * make identification more restrictive to avoid false-positive identification of raw binary formats such as ENVI (#1961)

ISIS3 driver:
 * extract band name from BandBin group, and wavelength/bandwidth (#1853)
 * preserve label in PAM .aux.xml when copying to other formats (#1854)
 * add support for PointPerspective projection (#1856)
 * add support for Oblique Cylindrical projection (#1856)

JP2ECW driver:
 * add support for ECWJP2 SDK 5.5

JP2OpenJPEG driver:
 * Fix multi-threading race condition (#1847)
 * fix reading overviews, when tiled API is used, and the dimensions of the full resolution image are not a multiple of 2^numresolutions (#1860)
 * fix to return the proper number of bytes read when we read more than 2 GB at once (fixes https://github.com/uclouvain/openjpeg/issues/1151)

JP2KAK driver:
 * fix issue with multi-threaded reads
 * NMAKE: Allow users to tweak/append extras to KAKINC (#1584)

JPEG driver:
 * fix further calls to RasterIO after reading full image at full resolution (#1947)
 * tune sanity check for multiple-scan (QGIS #35957)
 * in case of multiscan image and implicit overviews, limit memory consumption

KEA driver:
 * return error when deleting metadata item rather than crashing (#1681)
 * Backport thread safety fixes and nullptr tests from standalone driver (#2372)

LOSLAS driver:
 * add support for .geo geoid models

MEM driver:
 * Set access mode to the one required in Open()

MRF driver:
 * fix relative file name detection
 * relax TIFF tile format rules on read
 * Accept any known projection encoding
 * use PNG library for swapping
 * fixes caching MRF issue

MrSID driver:
 * add MRSID_PLATFORM to frmts/mrsid/nmake.opt

netCDF driver:
 * improve performance when reading chunked netCDF 4 bottom-up files (read-only)
 * correctly parse grid_mapping attribute in expanded form
 * allow "radian" value for the X/Y axis units
 * very partial workaround for an issue in libnetcdf 4.6.3 and 4.7.0 with unlimited dimensions (refs https://github.com/Unidata/netcdf-c/pull/1442)
 * NCDFIsUserDefinedType(): make it work for types in subgroups
 * Disregard valid range if min > max (#1811)
 * for byte (signed/unsigned) variables, do not report a nodata value if there's no explicit _FillValue/missing_value as recommended by the netCDF specs. And for other data types use nc_inq_var_fill() to get the default value
 * do not report nodata if NOFILL is set
 * fix bottom-up identification with negative scale_factor for y axis (#1383)
 * fix issue when opening /vsicurl/http[s]://example.com/foo.nc filenames (#2412)
 * Fix support of WKT1_GDAL with netCDF rotated pole formulation

NGW driver:
 * Add support for additional raster types and QGIS styles.
 * Add more server side attribute filters.

NITF driver:
 * skip bad UDID data (#1578)

NTv2 driver:
 * add support for the Canadian NAD83(CRSR)v7 / NAD83v70VG.gvb velocity grid

OZI driver:
 * fix axis order issue with georeferencing (3.0 regression)

PDF driver:
 * update to use newer versions of pdfium

PDS3 driver:
 * report the label in a json:PDS metadata domain
 * fix support of Oblique Cylindrical (#1856)
 * ix opening of datasets with BSQ organization (or single band), where one band is larger than 2 GB (2.3 regression)
 * nasakeywordhandler: fixes to be able to read some labels with metadata items whose value is a list on several lines
 * add a GDAL_TRY_PDS3_WITH_VICAR configuration option that can be set to YES so that PDS3 datasets that contain a VICAR label are opened by the VICAR driver

PDS4 driver:
 * update CART schema to 1D00_1933 and PDS to 1C00 (#1851)
 * fix reading side of Mercator and Orthographic
 * add a CREATE_LABEL_ONLY=YES creation option, and create a <Header> element (#1832)
 * add write support for LAEA projection

PNG driver:
 * Update internal libpng to 1.6.37

PNM driver:
 * emit warning if creating file with non-standard extension

RasterLite2 driver:
 * fix build against latest librasterlite2, and require it

RMF driver:
 * Add support for PZ-90.11 and GSK-2011 coordinate systems
 * Read vertical datum info
 * Add translation vertical CS ID to dataset's spatial reference

RS2 driver:
 * add half-pixel shift to reported GCP line and column numbers (#1666)

Sentinel2 driver:
 * Add support for exposing Level-2A AOT, WVP, SCL, CLD, SNW data in SAFE_COMPACT format (#2404)

TileDB driver:
 * add capability to define co-registered attributes per band
 * set row-major reads and removed adviseread (#1479)
 * added support for raster band metadata
 * TILEDB_LIBS added for windows build
 * partial updates to existing arrays
 * support reading tiles in update mode (#2185)
 * use array metadata to store xml (#2103)
 * redirect vsis3 calls to tiledb s3 direct calls
 * Flush cache in block order for global writes (#2391)

TSX driver:
 * add PAZ support

USGSDEM driver:
 * fix reading FEMA generated Lidar datasets whose header is 918 bytes large

VICAR driver:
 * Support FORMAT=HALF, DOUB and COMP
 * Support big-endian order for integer & floating point values
 * Support VAX floating-point order
 * Support BIP and BIL organizations
 * Ignore binary label records (NBL), and properly skip binary prefixes (NBB)
 * fix reading of EOL labels with non BSQ organizations, and possible confusion with LBLSIZE of EOL overwriting the main one
 * improvements in label reading, and report label in a json:VICAR metadata domain
 * read binary prefixes as OGR layer
 * add read support for BASIC and BASIC2 compression methods
 * add write support
 * remove obsolete END-OF-DATASET_LABEL, CONVERSION_DETAILS and PIXEL-SHIFT-BUG metadata items that dates back to 1.X era

VRT driver:
 * add 'vrt://{gdal_dataset}?bands=num1,...,numN' syntax as a convenient way of creating a on-the-fly VRT with a subset of bands
 * VRT warp: do not fail if a block has no corresponding source pixels (#1985)
 * VRT Python: also probe libpython3.Xm.so.1.0 (#1660)
 * VRT Python: add Python 3.8 compatibility
 * avoid erroneous pixel request do be done with KernelFilteredSource
 * VRTRawRasterBand: add GetVirtualMemAuto interface to enable mmap io
 * fix requesting a downsampling version of the mask band of a source that has masks and overviews
 * VRT pansharpening: fix crash when NoData is set and input multispectral bands are separate files (#2328)
 * fix IGetDataCoverageStatus() that can cause issue for the use case of https://lists.osgeo.org/pipermail/gdal-dev/2020-April/051889.html
 * gdalvrt.xsd: add 'dataAxisToSRSAxisMapping' attribute for GCPList element (#2378)

WCS driver:
 * pass user-supplied progress function to curl HTTP request when using DirectRasterIO

WMS driver:
 * IReadBlock(): limit number of tiles downloaded at once
 * Use curl_multi_wait instead of socket API (#1789)
 * AGS: Remove unused parameters from url

## OGR 3.1.0 - Overview of Changes

## Core

* Support API-level precision/round for geometry WKT (#1797)
* OGR SQL: make LIKE behave in a case sensitive way from now
* OGR SQL: support constructs 'A OR B OR C ... OR N' with many successive OR
* SQL SQLite: do not propagate 'IS / IS NOT value' constructs to OGR SQL
* SQL SQLite: add support for ST_MakeValid() using OGRGeometry::MakeValid() if not exposed by Spatialite already
* Rename swq.h->ogr_swq.h and install it, mark SQL query parse API with
CPL_UNSTABLE_API macro (#1925)
* SQLite dialect: fix issue when using JOIN on a layer without fast filter count capability
* OGRSimpleCurve::getPoints() with XYZM: fix wrong stride used for M array
* OGRSimpleCurve: fix reversePoints() and addSubLineString() to take into account M dimension
* Add OGR_G_CreateGeometryFromEsriJson() and map it to SWIG
* OGRLinearRing::isPointOnRingBoundary(): fix incomplete test that could falsely return true if the point was aligned with a segment, but not between the nodes. Impact correct reconstruction of holes in shapefile driver
* OGRGeometryFactory::ForceTo(): fix crash when forcing a MultiPolygon Z/M/ZM to a CompoundCurve (#2120)
* OGRGeometryFactory::forceToMultiLineString(): fix for a GeometryCollection of LineStringZ/M/ZM (#1873)
* OGRGeometryRebuildCurves(): only try to reconstruct curved geometry if one of the input geometries has really a non-linear portion
* curveFromLineString(): make sure to exactly close the compound curve if the input curve is itself closed
* GDALDataset::CopyLayer(): fix crash when using DST_SRSWKT option. And also set SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER)
* Add OGR_G_RemoveLowerDimensionSubGeoms()

## OGRSpatialReference

* Revise how SRS methods deal with TOWGS84. Add OSR_ADD_TOWGS84_ON_IMPORT_FROM_EPSG, OSR_ADD_TOWGS84_ON_EXPORT_TO_PROJ4 and OSR_ADD_TOWGS84_ON_EXPORT_TO_WKT1 configuration options. See https://github.com/OSGeo/gdal/commit/cc02dc4397c7ec43ec4c4b842e5faabe16c54904 for details
* SetFromUserInput(): add capability to import PROJJSON
* add ExportToPROJJSON() (PROJ >= 6.2)
* GetAxis(): make it work with a compound CRS (#1604)
* Coordinate transformation: fix invalid output with some input coordinates in EPSG:4326 to EPSG:3857 transformation (3.0 regression)
* Coordinate transformation: Coordinate transformation: add a OGR_CT_OP_SELECTION=PROJ/BEST_ACCURACY/FIRST_MATCHING config option. Defaults to PROJ for PROJ >= 6.3
* importFromWkt(): emit a CPLError() in case of import failure (#1623)
* Add OSRGetAxesCount() to C API and SWIG bindings
* Add OSRPromoteTo3D() and map it to SWIG (PROJ >= 7) (#1852)
* importFromESRI(): acept COMPD_CS (#1881)
* add an internal cache for importFromEPSG() and importFromWkt(). Helps performance for MapServer PROJ6 migration
* Add support for Vertical Perspective projection (#1856)
* Add a OSRGetPROJSearchPaths() function and a SWIG osr.GetPROJVersionMicro()
* Fix use-after-free issue when destroying a OGRSpatialReference object in a thread when another thread has created it but has been destroy in-between
* Coordinate transformation: make it work with hacky WKT1 rotated pole from netCDF driver
* Add OGR_GeomTransformer_XXXX API that wraps OGRGeometryFactory::transformWithOptions() and expose it to SWIG (fixes #1911)
* Fix PROJ usage across fork() calls (#2221)
* OGRCoordinateTransformation: correctly deal when transforming CRS that includes +lon_wrap= or similar qualifiers
* Add OSRIsDerivedGeographic()
* EPSGTreatsAsNorthingEasting(): fix it to properly deal with Polar projected CRS with northing,easting order

## Utilities

* ogrinfo: add a -nogeomtype switch
* ogrinfo: fix to output WKT2 SRS by default. Was done correctly for several geometry column, but not single one
* ogr2ogr: emit better error message when using -f VRT
* ogr2ogr: improve performance of -explodecollections on collections with big number of parts
* ogr2ogr: avoid non-relevant warning 'Input datasource uses random layer reading, but output datasource does not support random layer writing' when converting one single layer
* ogr2ogr: allow to combine -nlt CONVERT_TO_LINEAR and -nlt PROMOTE_TO_MULTI (#2338)
* ogr2ogr: add a -makevalid switch (requires GEOS 3.8 or later) and expose it in Python as makeValid=True options of gdal.VectorTranslate()

## OGR drivers

Multiple drivers:
 * GML/WFS: add support for Date, Time and DateTime fields
 * GeoJSONSeq & TopoJSON: avoid false positive detection and errors on unrelated http[s]:// filenames

AmigoCloud driver:
 * Modify amigocloud URL endpoint.

AVCE00 driver:
 * fix alternance from sequential to per-FID reading

CAD driver:
 * Fix read ellipse and arc (#1886)

DXF driver:
 * Re-add some header and trailer elements that caused compatibility issues with recent Autocad versions (#1213)
 * Generate correct HATCH boundary elliptical arc segments for certain start/end angles
 * Skip "Embedded Object" sections in 2018 version DXFs
 * Specify maximum gap between interpolated curve points, configurable with OGR_ARC_MAX_GAP
 * do not error out if trying to create a OGR_STYLE field
 * fix handling of SPLINE whose first knot is at a very very close to zero negative (#1969)
 * do not discard Z value when reading a HATCH (#2349)
 * write (100, AcDbEntity) before (8, layer_name) (#2348)
 * Correct handling of "off" and "frozen" layers in blocks (#1028)

DWG driver:
 * add support for ODA 2021.2 (Windows builds)

ElasticSearch driver:
 * rename driver to Elasticsearch (s lowercase for search)
 * update geometry type name for Points
 * Enable support for Elasticsearch 7 (#1246)
 * Set 'application/json' in RunRequest() with POST (#1628)
 * GeoJSON type field should be mapped as text in ES>=5
 * add FORWARD_HTTP_HEADERS_FROM_ENV open option to pass HTTP headers down to the ES server
 * translate constructs like CAST(field_name AS CHARACTER[(size)]) = 'foo' to ES query language

ESRIJson driver:
 * attempt identification of SRS from database entries (#2007)
 * do not require a 'geometry' member to be present in a feature

GeoJSON driver:
 * on writing, format OFTDate and OFTDateTime as ISO 8601 strings rather than OGR traditional formatting
 * add a DATE_AS_STRING open option that can be set to YES to disable autodetection of date/time/datetime
 * Advertise UTF-8 encoding of strings (#2151).
 * report 3D layer geometry types (#1495)
 * fix recognizing some documents with members sorted alphabetically (#1537)
 * avoid SetFeature() to repeat first feature when looping over features (#1687)
 * use VSIOverwriteFile() to fix update of file on Windows (fixes https://github.com/qgis/QGIS/issues/28580)
 * Transform MAX_OBJECT_SIZE to runtime environment option - OGR_GEOJSON_MAX_OBJ_SIZE
 * on reading of a file that use crs.name = urn:ogc:def:crs:OGC:1.3:CRS84, report EPSG:4326 as we used to do in GDAL 2 (#2035)
 * in writing mode, implement GetExtent() (#2269)
 * Add ID_GENERATE option for generating missing feature ids (#2406)

GeoJSONSeq driver:
 * make Open() return successfully only if at least one feature is detected

GML driver:
 * support reading standalone geometry (#2386)
 * fix axis order issue when decoding AIXM ElevationPoint (#2356)
 * fix axis order issue with gml:CircleByCenterPoint and gml:ArcByCenterPoint (# 2356)
 * fix handling of angles with ArcByCenterPoint and urn:ogc:def:crs:EPSG::4326, and compound curves made of a sequence of straight lines and ArcByCenterPoint in a <segments> (#2356)

GPKG driver:
 * change default value of OGR_GPKG_FOREIGN_KEY_CHECK to NO, so as to avoid issues in downstream software
 * insert more accurate spatial extent in gpkg_contents
 * on layer creation, check if the SRS is consistent with its advertise AUTHORITY/ID, and if not do not use official EPSG entries (#1857)
 * allow parsing datetime serialized as OGR strings, and emit warnings when unrecognized content is found (#2183)
 * when writing a layer of geometry wkbUnknown, make sure to set gpkg_geometry_columns.z/m to 2 when there are geometries with Z/M values (#2360)

KML driver:
 * set OAMS_TRADITIONAL_GIS_ORDER for SRS returned on returned layers

LIBKML driver:
 * make it accept /vsigzip/foo.kml.gz files (#1743)

MITAB driver:
 * Add friendly layer name (description) support.
 * Fix creation of long field names in local encoding (#1617)
 * don't left truncate numeric values in the .dat when the field formatting is incompatible, but error out (#1636)
 * Add custom datum/spheroid parameters export
 * Update WindowsLatin2 definition (#1571)
 * identify correctly GDA2020 datum
 * do not set (by default) TOWGS84 when reading a known Datum
 * cleanup management of update flag (#2170)

MSSQLSpatial driver:
 * Fix handling empty geometries (#1674)
 * Fix handling mixed geometries (#1678)
 * avoid GetExtent() to mess with GetNextFeature() statement, so that ogrinfo -al works properly

MVT driver:
 * make CONF option accept a filename as well
 * reduce memory usage when processing big geometries (#1673)

MySQL driver:
 * Add SRID to geometry when creating layer table (#1015)
 * use INFORMATION_SCHEMA.ST_GEOMETRY_COLUMNS to get SRSId

NAS driver:
 *  disable generation of .gfs file in read-only file systems (or intended to be), similarly to GML driver

netCDF driver:
 * add read/write support for CF-1.8 Encoded Geometries
 * add support for vector products Sentinel3 SRAL MWR Level 2

NGW driver:
 * Fix dataset access mode
 * Add support for layers geometry types with Z

OAPIF driver (previously WFS3):
 * updated to OGC API - Features 1.0 core spec (#1878)
 * correctly handle user query string parameters in connection URL (#1710)
 * add persistent curl session for better performance
 * add capability to use a XML Schema to build the layer field definition and geometry type
 * add capability to use JSON schema
 * add support for rel=queryables and cql-text
 * add a IGNORE_SCHEMA=YES/NO open option
 * implement filter-lang=json-filter-expr
 * avoid issues with double slash when building a /collections URL

OSM driver:
 * make error message hopefully clearer and more complete (#2100)

PDF driver:
 * fix reading polygon with holes and Bezier curves (#1932)
 * add a GDAL_PDF_LAUNDER_LAYER_NAMES configuration option
 * fix reading strings with escape sequences

PostgreSQL driver:
 * add support for PostgreSQL 12 (#1692)
 * add support for PG:service=xxx syntax and SERVICE open option (#2373)
 * support PostGIS schema installed in non-public schema (#2400)
 * do not attempt to create VARCHAR(n) columns with n >= 10485760 (#1508)
 * be more restrictive when deducing non-nullability of columns in SQL result layers (#1734)

Shape driver:
 * add read/creation/update support for .shz and .shp.zip
 * identify a EPSG code if the confidence is >= 90 (https://github.com/OSGeo/PROJ/issues/1799)
 * better deal with empty .shp+.shx and SHAPE_RESTORE_SHX (#1525)
 * launder layer name to get Windows compatible filename
 * try to better deal with polygons with parts touching on an edge (which is illegal simple features) (#1787)
 * fix crash when creating a layer with a wkbNone geometry type but a SRS (3.0 regression)
 * include fseek() optimization of https://github.com/OSGeo/shapelib/pull/3
 * expose .dbf and .cpg source encodings in the SHAPEFILE metadata domain
 * correctly update layer extent when first feature is a point at (0,0) (#2269)

S57 driver:
 * s57objectclasses.csv: add missing TXTDSC attribute for DRYDOC class (#1723)
 * s57objectclasses.csv: add wtwdis and unlocd for distance marks from IENC (#2123)
 * s57objectclasses.csv: add PICREP attribute to LNDMRK object class
 * Added handling of ISDT when using updates (#895)
 * Added creation of additional field "ATTF" when missing while updating (#2249)

SOSI driver:
 * Append values from duplicate fields when setting new appendFieldsMap open option (#1794)

SQLITE driver:
 * fix crash in loading sqlite extensions on iOS (#1820)
 * fix conversion of geometry collections (and derived types) in geometry collections as Spatialite geometries, by flattening the structure
 * cleanup management of update flag (#2162)
 * GetSpatialiteGeometryHeader(): fix bug regarding detection of empty geometries

SXF driver:
 * Add open options with RSC file name
 * Use SXF_LAYER_FULLNAME from dataset open options
 * Use SXF_SET_VERTCS from dataset open options
 * Add OGRSXFLayer::CanRecode and check it for OLCStringsAsUTF8 capability
 * Add driver identify function (#1607)

VFK driver:
 * use a faster implementation of VFKDataBlockSQLite::LoadGeometryPolygon()

VRT driver:
 * set OAMS_TRADITIONAL_GIS_ORDER for LayerSRS (#1975)

WaSP driver:
 * on creation, make sure the layer geometry type set on the feature definition is wkbLineString25D

WFS driver:
 * Support FlatGeobuf as WFS outputformat (#2135)
 * skip silently GeoServer EPSG:404000 dummy CRS

XLSX driver:
 * add support for .xlsm extension

## SWIG Language Bindings

All:
 * add osr.SetPROJSearchPath(path) that can be used since setting PROJ_LIB from C# does no work (#1647)

Python bindings:
 * add 'add' option to gdal.Rasterize
 * add hint&workaround for ImportError on Windows Python >= 3.8
 * add compatibility with SWIG 4.0 (#1702)
 * build modules in parallel
 * honour gdal.UseExceptions() in numpy related methods (gdalnumeric module) (#1515)
 * update to SWIG 3.0.12 to have better error message (#1677)
 * make the feature iterator on the layer call ResetReading()
 * Removed calls to deprecated imp module (#2264)
 * Add numpy as extras_require dependency (#2158)
 * emit exception is osr.SpatialReference(wkt) fails, even if in non-UseExceptions() mode, to avoid later cryptic exception (#2303)
 * NUMPYDataset::Open() / gdal_array.OpenArray(): honour writable flag of the numpy array to decide update status of GDAL dataset
 * fix invalid check for Dataset.ReadAsArray(buf_obj=some_array, interleave='pixel') scenario

# GDAL/OGR 3.0.0 Release Notes

## In a nutshell...

 * Implement RFC 73: Integration of PROJ6 for WKT2, late binding capabilities, time-support and unified CRS database. PROJ >= 6 is now a build requirement
    https://trac.osgeo.org/gdal/wiki/rfc73_proj6_wkt2_srsbarn
 * New GDAL drivers:
  - DAAS: read driver for Airbus DS Intelligence Data As A Service
  - TileDB: read/write driver for https://www.tiledb.io (#1402)
 * New OGR drivers:
  - MongoDBv3: read/write driver using libmongocxx v3.4.0 client (for MongoDB >= 4.0)
 * Improved drivers:
   - FITS: read/write support for scale, offset and CRS
   - netCDF: read support for groups
   - PDF: add a COMPOSITION_FILE creation option to generate a complex document
   - PDS4: subdataset creation support, read/write table/vector support
 * Support for minimal builds on Unix (#1250)
 * Add a docker/ directory with Dockerfile for different configurations
 * Continued code linting

## New installed files

 * Resource file: pdfcomposition.xsd

## Removed installed files

 * Removal of resource files related to EPSG and ESRI CRS databases: compdcs.csv, coordinate_axis.csv, datum_shift.csv, ellipsoid.csv, esri_epsg.wkt, esri_extra.wkt, esri_Wisconsin_extra.wkt, gcs.csv, gcs.override.csv, gdal_datum.csv, geoccs.csv, pcs.csv, pcs.override.csv, prime_meridian.csv, projop_wparm.csv, unit_of_measure.csv, vertcs.csv, vertcs.override.csv

## Backward compatibility issues

See MIGRATION_GUIDE.txt

## GDAL/OGR 3.0.0 - General Changes

Build(Unix):
 * Allow internal drivers to be disabled (#1250)
 * Fix build with OpenBSD which doesn't support RLIMIT_AS (#1163)
 * Fix MacOS build failures due to json-c
 * Poppler: require pkg-config
 * PostgreSQL: Switch from pg_config to pkg-config (#1418)
 * fix build --without-lerc (#1224)
 * fix netcdf_mem.h detection in netcdf 4.6.2 (#1328)
 * Fix build --with-curl --without-threads (#1386)

Build(Windows):
 * nmake.opt: remove unicode character at line starting with '# 4275' that apparently cause build issues with some MSVC versions (#1169)

All:
 * PROJ >= 6.0 is a required external dependency
 * libgeotiff >= 1.5 should be used for builds with external libgeotiff
 * Poppler: drop support for Poppler older than 0.23.0
 * Poppler: add support for 0.72.0, 0.73.0, 0.75.0, 0.76.0

## GDAL 3.0.0 - Overview of Changes

Port:
 * Add capability to define external VSI virtual file systems from C API (#1289)
 * MiniXML: Fix wrong node order when calling CPLAddXMLAttributeAndValue() after CPLCreateXMLElementAndValue()
 * /vsicurl/: ReadMultiRange(): use default implementation if there is a single range (#1206)
 * /vsicurl/: ignore proxy CONNECT response headers (#1211)
 * /vsicurl/: automatically detect signed URLs where host ends with a port number; also detect signed URLs as created with the AWS4-HMAC-SHA256 method (#1456)
 * /vsizip/: Add config option to create zip64 extra fields by default (#1267)
 * /vsis3/, /vsigs/, /vsiaz/: add HTTP retry logic in writing code paths
 * Fix data race in VSIFileManager::Get
 * cpl_zipOpenNewFileInZip3: fix memory leak in error code path. Fixes https://bugs.chromium.org/p/oss-fuzz/issues/detail?id=13103.
 * VSIGZipWriteHandleMT: avoid potential deadlock in case of error
 * Fix assertion in CPLGetValueType when testing non-ASCII chars
 * /vsihdfs/: fix Read() when more than one hdfsRead call is needed (#1349)
 * Fix potential buffer overflow in CPLODBCSession::Failed (#1390)
 * /vsitar/: handle .tar file header with space padding instead of 0 for file size (#1396)

Core:
 * Support blocks larger than 2 billion pixels/bytes
 * Make CPLHaveRuntimeSSSE3() and CPLHaveRuntimeAVX() use GCC constructor functions
 * Move RawDataset base class to gcore/ (#1268)
 * RasterBand/Dataset::RasterIO(): enforce access mode on write
 * PAM: preserve existing metadata when setting new one (#1430)
 * RawDataset: use generic RasterIO() implementation when non-nearest resampling is asked (#1301)
 * DumpJPK2CodeStream(): dump PLT and POC markers

Algorithms:
 * RPC transformer: test success code of GDALRPCTransform() in GDALCreateRPCTransformer()
 * RPC transformer: add a RPC_FOOTPRINT transformer option to provide a polygon in long/lat space where the RPC is valid, and also make gdalwarp use GDALSuggestedWarpOutput2() to restrict the bounding box of the output dataset
 * GDALFillNodata(): fix wrong comparison in QUAD_CHECK() macro: nNoDataVal is only assigned to target_y values (#1228)
 * GDALFillNodata(): reinitialize panLastY array to nNoDataVal before bottom to top pass (#1228)
 * GDALFillNodata(): do an extra iteration to reach the maximum search distance in all quadrants (#1228)
 * GDALRasterizeLayersBuf():support any GDAL data type for buffer,and pixel and line spaceing arguments
 * GDALResampleChunk32R_Mode: performance improvement
 * Rasterize with MERGE_ALG=ADD: avoid burning several times intermediate points of linestrings (#1307)
 * rasterize: fix crash when working buffer is larger than 2GB (#1338)

Utilities:
 * gdal_translate: add "-nogcp" option (#1469)
 * gdal_contour: remove explicit width/precision=12/3 of the elev field (#1487)
 * gdaldem hillshade: add -igor option (#1330)
 * gdalwarp -crop_to_cutline: do not round computed target extent to be aligned on the grid of the source raster if -tr is set (restore partially pre 2.4 behavior) (#1173)
 * gdalwarp: assume -tap when using -crop_to_cutline, -tr and -wo CUTLINE_ALL_TOUCHED=TRUE, so as to avoid issues with polygons smaller than 1x1 pixels (#1360)
 * gdal2tiles: give local tile layer and basemap layers same min/max zoom levels as generated tile cache
 * gdal2tiles: fix breakage of openlayers.html getURL() javascript function, introduced in GDAL 2.3.3 / 2.4.0 (#1127)
 * gdal2tiles: prevent accidental copy of full GeoTIFF into temporary .vrt file
  gdal2tiles: Refactor and fix multiprocessing completion handling
 * gdal_fillnodata.py: preserve color interpretation and table
 * gdal_fillnodata.py: do not set geotransform if source doesn't have one
 * gdalsrsinfo: do not silence errors when calling SetFromUserInput()
 * gdal_retile: Use nodata value from origin dataset
 * gdal_edit.py: allow setting band-specific scale and offset values (#1444)
 * validate_cloud_optimized_geotiff.py: check if file is only greater than 512px (#1403)
 * validate_cloud_optimized_geotiff.py: report headers size

Multidriver fixes:
  * GTiff, GPKG, MBTiles, PostgisRaster drivers: ensure that main dataset and overviews share the same lock, so as to avoid crashing concurrent access (#1488)

ADRG driver:
 * modified to ensure that there is no confliction between ADRG and SRP when opening a .gen file (#953)

AIGRID / AVCBin:
 * fix filename case adjustment that failed on /vsi filesystems (#1385)

BAG driver:
 * fix potential nullptr deref on corrupted file

COSAR driver:
 * avoid out-of-bound write on corrupted dataset. Fixes https://bugs.chromium.org/p/oss-fuzz/issues/detail?id=12360.

EEDA driver:
 * report 'path' metadata in 'path' field

GPKG driver:
 * allow negative srs_id values in gpkg_spatial_ref_sys
 * reduce memory requirements for cached tiles

GTiff diver:
 * supports tiles/strips larger than 2GB
 * add APPEND_SUBDATASET=YES capability to create subdataset / new TIFF page to an existing file
 * only report scale/offset deduced from ModelTiepointTag and ModelPixelScaleTag if the SRS has a vertical component (and thus currently if GTIFF_REPORT_COMPD_CS is set) (https://issues.qgis.org/issues/20493)
 * TIFF Lerc: properly initialize state after Create() so that BuildOverviews() succeed (#1257)
 * emit merged consecutive multi-range reads (#1297)
 * add warnings when using unsupported combination of internal mask+external overview, and fix COPY_SRC_OVERVIEWS=YES so that it does not copy ALL_VALID masks (#1455)
 * do not generate a TIFFTAG_GDAL_METADATA with color interpretation information for JPEG YCbCr compression
 * Internal libtiff and libgeotiff: resync with upstream

FITS driver:
 * new functions for Scale Offset and FITS World Coordinate System read and write (#1298)

GeoRaster driver:
 * Fix memory leaks

HDF5 driver:
 * support reading blocks larger than 2GB
 * fix handling of attributes of type SCHAR, UCHAR, USHORT and UINT (https://github.com/mapbox/rasterio/issues/1663)
 * detect nodata from netCDF _FillValue (#1451)
 * add more strict checks for accepting datasets for GCP, and handle nodata in GCP too (#1451)

IGNFHeightAsciiGrid driver:
 * add support for RAF18.mnt

JPEG driver:
 * fix GDAL 2.3.0 performance regression when decoding JPEG (or GPKG using JPEG) images (#1324)

KEA driver:
 * add support for /vsi file systems

KMLSuperOverlay driver:
 * report color table of single overlay datasets, and also handle some variation in the KML structure (https://issues.qgis.org/issues/20173)

MRF driver:
 * sparse index and internal resampling fixes
 * fix integer overflow. Fixes https://bugs.chromium.org/p/oss-fuzz/issues/detail?id=13974

MrSID driver:
 * fix potential crash when a zoom level cannot be opened

netCDF driver:
 * implement support for NetCDF-4 groups on reading (#1180)
 * support complex data types (#1218)
 * fix crash when opening a dataset with an attribute of length 0 (#1303)
 * fix IWriteBlock() to support non-scanline blocks, and use the chunk size
 * better deal with datasets indexed with unusual order for x/y dimensions
 * avoid inappropriate shift by -360 when attribute axis=X is set (#1440)

NGW driver:
 * Add CreateCopy function

NITF driver:
 * avoid harmless floating point division by zero. Fixes https://bugs.chromium.org/p/oss-fuzz/issues/detail?id=12844.

PCIDSK driver:
 * avoid uint overflow and too big memory allocation attempt. Fixes https://bugs.chromium.org/p/oss-fuzz/issues/detail?id=12893

PDF driver:
 * Add a COMPOSITION_FILE creation option to generate a complex document
 * And a gdal_create_pdf.py sample script
 * Fix selection of Poppler PDF layers with duplicate names (#1477)
 * avoid division by zero when generating from vector content whose bounding box is almost a horizontal or vertical line. Fixes https://bugs.chromium.org/p/oss-fuzz/issues/detail?id=13408

PDS3:
 * fix decoding of band interleaved images (such as for CRISM HSP) (#1239)
 * add support for ENCODING_TYPE=DCT_DECOMPRESSED (#1457)

PDS4 driver:
 * add subdataset creation support, and getting/setting the band unit
 * update template and code to PDS v1B00 schema versions

VRT driver:
 * Python pixel functions: add shared object name for python 3.7
 * VRT: in case of no SourceProperties, do not use global shared datasets, but only shared to the owning VRTDataset, to avoid potential reference cycles and annoying related issues. Fixes https://bugs.chromium.org/p/oss-fuzz/issues/detail?id=13476.

WCS driver:
 * avoid potential out-of-bound access

WMS driver:
 * use proper JSon parsing for ESRI MapServer document (#1416)
 * Add url parameters escaping to ArcGIS Server minidriver
 * avoid warning when a wms cache doesn't exist

XPM driver:
 * fix read heap buffer overflow on corrupted image. Fixes https://bugs.chromium.org/p/oss-fuzz/issues/detail?id=13455.

XYZ driver:
 * add creation options DECIMAL_PRECISION and SIGNIFICANT_DIGITS like with AAIGrid
 * fix regression regarding header lines that are not X,Y,Z (#1472)

## OGR 3.0.0 - Overview of Changes

Core:
 * Add OGR_G_MakeValid() (requires GEOS 3.8)
 * change prototye of OGRFeature::SetField( int iField, int nBytes, GByte *pabyData ) to ( ... , const void* pabyData), and same for OGR_F_SetFieldBinary().
 * Polyhedral surface: fix importFromWKT to properly fix Z/M flag
 * OGRBuildPolygonFromLines: avoid generating effectively duplicate points
 * OGRBuildPolygonFromEdges(): improve performance. https://bugs.chromium.org/p/oss-fuzz/issues/detail?id=13798
 * OGRGeometryFactory::transformWithOptions(): if WRAPDATELINE=YES, test that the geometry SRS is geographic
 * OGRGeometryFactory::GetCurveParameters(): fix assertion when coordinates are very near 0
 * Expat XML parsing: add OGR_EXPAT_UNLIMITED_MEM_ALLOC=YES config option to workaround failure for very specific cases
 * OGRLineString::segmentize(): fix issues when segment length is divisible by maxlength (#1341)
 * OGR SQL: limit recursion in swq_expr_node::Check(). Fixes https://bugs.chromium.org/p/oss-fuzz/issues/detail?id=13540
 * OGRGeometry / SFCGAL: fix dead code, memory leaks and potential nullptr deref.

OGRSpatialReference:
 * Deep rework due to RFC 73 integration
 * SRS_WKT_WGS84 macro replaced by SRS_WKT_WGS84_LAT_LONG
 * Add OSRSetPROJSearchPaths(), OSRExportToWktEx(), OSRGetName(), OSRIsSameEx(), OSRGetCRSInfoListFromDatabase(), OSRGetAreaOfUse(),OSRGetAxisMappingStrategy(), OSRSetAxisMappingStrategy(), OSRGetDataAxisToSRSAxisMapping()
 * Add OCTNewCoordinateTransformationOptions(), OCTCoordinateTransformationOptionsSetOperation(), OCTCoordinateTransformationOptionsSetAreaOfInterest(), OCTDestroyCoordinateTransformationOptions(), OCTNewCoordinateTransformationEx(), OCTTransform4D()
 * Remove OSRFixupOrdering(), OSRFixup(), OSRStripCTParms(), OCTProj4Normalize(), OCTCleanupProjMutex(), OPTGetProjectionMethods(), OPTGetParameterList(), OPTGetParameterInfo()

Utilities:
 * ogrinfo: report TITLE metadata in summary layer listing
 * ogr2ogr: for drivers supporting ODsCCreateGeomFieldAfterCreateLayer, do not create geometry column if -nlt none
 * ogrmerge.py: add shared='1' to speed-up -single mode with many layers

CARTO driver:
 * Overwrite tables in single transactions
 * Improve documentation and warnings around CARTODBFY (#1445)

CSW driver:
 * fix crash when geometry parsing fails (#1248)

DODS driver:
 * fixes related to memory leaks and null pointer dereferences

DXF driver:
 * support RGB true color values
 * fix the coloring of ByBlock entities inserted via a ByLayer INSERT
 * fix double-free issue in case of writing error. Fixes https://bugs.chromium.org/p/oss-fuzz/issues/detail?id=13516.

FileGDB/OpenFileGDB drivers:
 * be robust when winding order of outer ring is incorrect (#1369)

Elasticsearch driver:
 * Fixed index comparison bug when a index have at least one mapping
 * Fix _mapping url for Elasticsearch 7 compatibility

GeoJSON driver:
 * speed-up random reading with GetFeature() by storing a map FID->(start,size) to retrieve performance similar to GDAL 2.2 or before (https://issues.qgis.org/issues/21085)
 * report 3D layer geometry types (#1495)

GeoJSONSeq driver:
 * be more robust to invalid objects, and fixes performance issue on corrupted files. Fixes https://bugs.chromium.org/p/oss-fuzz/issues/detail?id=13770.

GML driver:
 * write SRSName element in .gfs when parsing a GML file with srsName only on top-level boundedBy element (#1210)
 * Add "FeatureType" to list of suffixes recognized by XSD parser (#1313)
 * GML/WFS: add minimum support for 'hexBinary' type (as string) (#1375)

GMLAS driver:
 * avoid null pointer dereference on some schemas
 * do not use space as separator for schema filename in XSD open option (#1452)

GMT driver:
 * use file extension based detection to accept files without header (#1461)

MongoDB driver:
 * fix related to filters in GetFeature()

MITAB driver:
 * Add encode/decode feature labels to/from UTF-8 encoding while MIF file read/write (#1151)
 * .tab: fix deleting a feature without geometry (#1232)
 * adapt dynamically default projection bounds to false_easting/false_northing values (#1316)
 * avoid potential assertion or stack buffer overflow on corrupted .ind files. Fixes https://bugs.chromium.org/p/oss-fuzz/issues/detail?id=11999
 * prevent potential infinite recursion on broken indexes. Fixes https://bugs.chromium.org/p/oss-fuzz/issues/detail?id=12739.
 * TAB_CSLLoad(): fix performance issue. Fixes https://bugs.chromium.org/p/oss-fuzz/issues/detail?id=13588
 * avoid long processing on corrupted .mif files. Fixes https://bugs.chromium.org/p/oss-fuzz/issues/detail?id=13152

MSSQLSpatial driver:
 * Add support for curve geometries (#1299)
 * Add option to expose the FID column as a feature attribute (#1227)
 * Adding improved extent queries
 * Fix extent calculation for geography type, take care of invalid geometries

NGW driver:
 * Fix get children API. Add authorization support to create dataset options
 * Add support for feature extensions in OGRFeature native data
 * Add feature query via chunks, attribute and spatial filter support
 * Add resource type and parent identifier to metadata
 * Fix batch update features
 * Add JSON_DEPTH open option
 * Add check forbidden field names, check duplicate field names.

OCI driver:
 * Add MULTI_LOAD to open options (#1233)
 * Fix memory leaks

ODS driver:
 * allow opening tables with empty cells with huge values of columns-repeated attribute at end of line (#1243)
 * avoid potential null pointer dereference when writing to corrupted filename. Fixes https://bugs.chromium.org/p/oss-fuzz/issues/detail?id=12976.

OGDI driver:
 * switch incorrect order for east/west bounds of spatial filter

PDS4 driver:
 * add read/write support for tables (vector support)

PGDump driver:
 * in WRITE_EWKT_GEOM=YES non-default mode, export geometries to ISO WKT so as to be able to export XYZM (#1327)
 * Fix emitted SQL when UNLOGGED=ON

PLScenes driver:
 * update plscenesconf.json with SkySatCollect and add missing fields for PSOrthoTile

SDTS driver:
 * error out if too many errors are raised to avoid timeout in oss-fuzz. Fixes https://bugs.chromium.org/p/oss-fuzz/issues/detail?id=13819.

Selafin driver:
 * avoid null pointer dereference on corrupted files. Fixes https://bugs.chromium.org/p/oss-fuzz/issues/detail?id=12356.

Shapefile driver:
* DeleteLayer(): make it delete .cpg, .sbn, .sbx, .qpj and other sidecar files (#1405)
 * speed-up creation of lots of fields with name collisions. Fixes https://bugs.chromium.org/p/oss-fuzz/issues/detail?id=13065

SQLite driver:
 * set sqlite3_busy_timeout, handle SQLITE_BUSY during tile read (#1370)
 * close database before freeing the spatialite context. Fix crashes on dataset closing, with VirtualShape and recent spatialite versions
 * Spatialite: fix update of geometry_columns_statistics when extent goes to infinity (#1438)

S57 driver:
 * avoid long procession on corrupted datasets. Fixes https://bugs.chromium.org/p/oss-fuzz/issues/detail?id=13238

SXF driver:
 * Fix wrong EPSG codes range (#1278)
 * Fix memory leaks in SetVertCS()

VFK driver:
 * fix curved geometries being ignored (#1351)

WFS3 driver:
 * handle paging with missing type for rel:next, and better deal with user:pwd in URL
 * use 'id' attribute of collection items, if 'name' not available

## SWIG Language Bindings

All bindings:
 * add Geometry::MakeValid()

Python bindings:
 * fix Dataset.ReadAsRaster() on CInt16 data type (#82)
 * adding overviewLevel option to WarpOptions
 * add noGCP options to gdal.Translate()

# GDAL 2.x and older

Consult [NEWS-2.x.md](NEWS-2.x.md)
