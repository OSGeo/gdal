# GDAL/OGR 2.4.0 Release Notes

## In a nutshell...

 * New GDAL drivers:
   - BYN: read/write support for Natural Resources Canada's Geoid binary format
   - EEDAI: read-only driver for Google Earth Engine Data API
   - IGNFHeightASCIIGrid: read-only driver to read IGN-France height correction ASCII grids
   - NGW: NextGIS Web read-only driver
   - NTv1: read-only driver for NTv1 datum shift grids
 * New OGR drivers:
   - EEDA: read-only driver for Google Earth Engine Data API
   - GeoJSONSeq: read/creation support of new-line or record-separator separated GeoJSON features (#378)
   - NGW: NextGIS Web read-write driver
 * Improved drivers:
   - BAG: add read support for variable-resolution grids, and write support for single-resolution grids
   - GTiff driver: add Lerc and WebP codecs
   - PostgisRaster: add support for out-db rasters
   - RMF
   - MSSQLSpatial
 * RFC 72: Make GDAL Python autotest suite use pytest framework
 * Add /vsihdfs/ virtual file system handler for Hadoop File System (via libhdfs)
 * Add /vsiwebhdfs/ read-write virtual file system for Web Hadoop File System REST API
 * gdal_contour rewriting: speed optimizations and capability to compute polygon isosurfaces.
 * Remove PHP and Ruby bindings.
 * Continued code linting in C++, Python scripts, Shell scripts and autotest

## Backward compatibility issues
 * The value of COMPRESSION_ZSTD used for ZStd-in-TIFF compression has been changed. ZStd-compressed TIFF files produced by GDAL 2.3.0 will not be readable

## GDAL/OGR 2.4.0 - General Changes

Build(Unix):
 * configure: error out when --enable-pdf-plugin is used with --with-libtool since frmts/pdf/GNUmakefile isn't ready for that (#556)
 * Fix compilation in C++17 mode with older ogdi headers
 * Fix the datadir in gdal.pc.
 * re-install cpl_vsi_error.h
 * update GRASS drivers to support GRASS 7.4.0 (#639, #633)
 * configure: use CXXFLAGS when CXX is used (#693)
 * GNUmakfile: fix dependency of install target (#707)
 * configure: fix 12 bit JPEG-in-TIFF support (#716)
 * configure: Remove additional '$' in front of '${CXX}' to fix ECW5 detection
 * configure: use ogdi.pc if available
 * Set minimum pkg-config version to 0.21
 * fix potential link errors when using internal libgif and internal libpng but headers of those libraries are available in the system in different versions than our internal ones (#938)

Build(Windows):
 * nmake.opt: allow install into paths with spaces
 * MBTiles driver: fix issue in Makefile
 * Fix HDF4 Plugin build for Visual C++ (#624)
 * NMAKE: copy gdal pdb to $(LIBDIR) in libinstall target
 * NMAKE: Enable friendlier static library builds to allow an external `DLLBUILD = 0`
 * nmake.opt: allow DEBUG=0 to be set (#703)
 * Do not include DllMain() in static library builds
 * add support for JPEGLS driver

All:
 * Add support for Poppler 0.64, 0.69, 0.71
 * avoid compilation error when compiling GMT's gmtdigitize.c that defines _XOPEN_SOURCE to empty (#590)
 * Fix build against PDFium (#612)
 * Add support for MySQL 8.0

## GDAL 2.4.0 - Overview of Changes

Port:
 * Add multi-threaded compression to /vsigzip/ and /vsizip/
 * /vsizip/: create ZIP64 when needed
 * /vsizip/: encode filename in Unicode when needed also in local file header
 * /vsigzip/: allow seeking to beginning of file, despite decompression error
 * /vsicurl/: extend retry logic to HTTP 500 and HTTP 400 with RequestTimeout, emit a CE_Warning if code != 400 and != 404
 * /vsicurl/: fix parsing of HTML file listing that got broken in GDAL 2.3.0
 * /vsicurl/ and derived: implement a LRU cache for file properties (instead of ever growing cache)
 * /vsicurl/ and derived: implement a LRU cache for directory content listing
 * /vsicurl/: make GetCurlMultiHandleFor() more thread-safe
 * HTTP: added curl cookiefile and cookiejar variables (fixes #1000)
 * /vsioss/: fix support of filenames with spaces
 * /vsizip/: output explicit error message when encountering a unsupported file compression method
 * /vsis3/: fix VSIStatL() on a directory (#603)
 * /vsis3/: take into account AWS_CONTAINER_CREDENTIALS_RELATIVE_URI for ECS instances (#673)
 * /vsis3/: honour CPL_VSIL_CURL_ALLOWED_EXTENSIONS configuration option (#995)
 * /vsis3/: ignores files with GLACIER storage class in directory listing, unless CPL_VSIL_CURL_IGNORE_GLACIER_STORAGE=NO
 * /vsiaz/: support BlobEndpoint element in AZURE_STORAGE_CONNECTION_STRING such as found in Azurite (#957)
 * Add a VSICurlPartialClearCache(const char* filenameprefix) function to partially clear the /vsicurl/ and related caches; and bind it to SWIG
 * Add VSISync() to synchronize source and target files/directories
 * /vsitar/: support headers with fields using star base-256 coding (#675)
 * Add VSIOpenDir/VSIGetNextDirEntry/VSICloseDir and provide efficient recursive implementation for /vsis3/, /vsigs/, /vsioss/ and /vsiaz/
 * Detect Amazon EC2 instances that use the newer hypervisor. Deprecates CPL_AWS_CHECK_HYPERVISOR_UUID and replaces with CPL_AWS_AUTODETECT_EC2
 * CPLString class: rework visibility of exported symbols for Visual Studio (rework of #321) (#636)
 * CPLGetPhysicalRAM(): take into account cgroup limitation (Docker use case), and rlimit (#640)
 * CPLGetNumCPUs(): take into account cgroup limitation.
 * ODBC: Improve Fetch() error handling.
 * ODBC: get multiple ODBC error messages.
 * ODBC: Fetch wide-char strings on UNIX (#839)
 * Fix buffer overflow in GDALDefaultCSVFilename with GDAL_NO_HARDCODED_FIND (#683)
 * VSIZipFilesystemHandler::Open: Fix leaks of poVirtualHandle (#699)
 * Fix date-time formatting for /vsigs/, /vsiaz/ and /vsioss/ protocols with non-C locales
 * CPLQuadTreeGetAdvisedMaxDepth(): avoid int overflow. Fixes https://bugs.chromium.org/p/oss-fuzz/issues/detail?id=9248
 * CPLJSonStreamingParser: make it error on invalid array constructs (#970)
 * Add a GDAL_HTTPS_PROXY configuration option to selectively setup proxy for https only connections (#972)
 * Add template class CPLAutoClose (#952)
 * Make VSIToCPLError() handle the generic VSIE_AWSError (#1007)
 * Propagate error handler user data correctly (#1098)
 * CPLOpenSSLCleanup(): reset callbacks to nullptr to avoid potential segfault

Core:
 * SetStatistics(): write a STATISTICS_APPROXIMATE=YES metadata item if bApproxOK=true, and take it into account in GetStatistics() (trac #4857, trac #4576)
 * Add percentage of valid pixels to metadata when computing raster band statistics (#698)
 * Overview creation: avoid creating too many levels, and fix related heap buffer overflow (#557,#561)
 * Overview: fix wrong computation of source pixel indices for AVERAGE and pixel-interleave bands
 * Lanczos rasterio/overview/warp: do not compute target pixel if there are too many missing source pixels, to avoid weird visual effects depending on if valid source pixels match positive or negative kernel weights
 * Overview / RasterIO resampling: do not use nodata value as a valid output value
 * RawRasterBand: only accept VSILFILE*
 * Add alpha mask flag for alpha band in Uint16 One band dataset (#742)
 * GetMaskBand(): do not use a GDALNoDataMaskBand when nodata value is out of range (#754)
 * Pleiades metadata reader: Add more strict check (#431)
 * Statistics/minmax computation: on a float32 raster, be more tolerant when the nodata is slightly larger than +/- FLOAT_MAX
 * GDALNoDataMaskBand: improve performance in downsampling cases
 * GDALDestroy(): no longer call it automatically on GCC/CLang (non-MSVC) builds
 * GDALGetJPEG2000Structure(): avoid excessive memory allocation. Fixes https://bugs.chromium.org/p/oss-fuzz/issues/detail?id=8268
 * Implement GDALAllValidMaskBand::ComputeStatistics(). Fixes https://bugs.chromium.org/p/oss-fuzz/issues/detail?id=9309
 * PAMDataset: avoid illegal down_cast to GDALPamRasterBand. Fixes https://bugs.chromium.org/p/oss-fuzz/issues/detail?id=9358
 * GDALOpenEx(): improve anti recursion detection. Fixes https://bugs.chromium.org/p/oss-fuzz/issues/detail?id=9391
 * GDALResampleChunk32R_Convolution: avoid invalid left shift. Fixes https://bugs.chromium.org/p/oss-fuzz/issues/detail?id=9477
 * Gauss resampling: fix potential read heap buffer overflow in corner cases. Fixes https://bugs.chromium.org/p/oss-fuzz/issues/detail?id=9783
 * GDALDefaultOverviews::OverviewScan(): avoid potential infinite recursion. Fixes https://bugs.chromium.org/p/oss-fuzz/issues/detail?id=10153
 * GDALPamRasterBand::CloneInfo(): do not clone empty RAT

Algorithms:
 * OpenCL wrapper: fix memory leak
 * Warper: add complex nodata handling in average/min/max modes
 * TPS solver: improve numerical stability, for non Armadillo builds, for points not centered on (0,0)
 * GCP polynomial interpolation: fix bug where worst_oultier always assumed polynomial order 2
 * GCP polynomial interpolation: fix bug where remove_ouliers used the parameters of the reverse transformation
 * GDALGridLinear(): speed-up search of triangle for points outside of the triangulation
 * gdal_grid linear: avoid artifacts with degenerate triangles (#638)
 * GDALPansharpenOperation::Initialize(): validate value of GDAL_NUM_THREADS (CID 1393944)
 * GDALFillNodata(): add NODATA option
 * GDALDEMProcessing(): fix null pointer dereference if psOptionsIn == nullptr (#931)

Utilities:
 * gdal_translate: make -stats option work with -co COPY_SRC_OVERVIEWS=YES (#792)
 * gdal_translate: fix RPC correction when using -srcwin with negative offsets (#827)
 * gdalwarp: automatically enable SKIP_NOSOURCE=YES when it is safe to do so
 * gdalwarp: make -crop_to_cutline stick to source pixel boundaries when no raster reprojection is involved, to avoid unnecessary resampling or resolution change
 * gdalwarp -r average: better deal with south-up oriented datasets (#778)
 * gdalwarp: improve robustness of computation of source raster window for a given target raster window (#862)
 * gdalwarp: allow to create bottom-up grid with -te xmin ymax ymin ymin
 * gdalwarp: fix crash when warping on an existing dataset with less bands as needed
 * gdal_contour: speed optimizations and capability to compute polygon isosurfaces.
 * gdal_contour: add amin and amax parameter for gdal_contour to be used with option -p
 * gdal_contour: avoid out-of-memory situation (#594)
 * gdal_contour: fix GDAL 2.3 regression with fixed interval contouring that resulted in discontinuities in contour lines (#889)
 * gdal_merge.py: deal with NaN values
 * gdal_retile.py: fix rounding issues when computing source and target regions (#670)
 * gdal_calc.py: add --optfile switch
 * gdal2tiles: fix wrong computation of min zoom level in some cases (#730)
 * gdal2tiles: add -x option for skipping transparent tile generation
 * gdal2tiles: fix performance issue by caching source dataset; GDALAutoCreateWarpedVRT()
 * gdal2tiles: fix issue with out-of-range nodata values (#770)
 * gdal2tiles: restore GDAL < 2.3 behavior when output directory is not explicitly specified (#795)
 * gdal2tiles: fix --force-kml (#809)
 * gdal_edit.py: add -setstats to set "fake" statistics (#819)
 * gdal_edit.py: add -unsetrpc option to gdal_edit.py, and fix GTiff driver to be able to clear RPC
 * gdal_grid: fix -clipsrc from a vector datasource (broken at least since GDAL 2.1)
 * gdalenhance: avoid potential nullptr dereference (CID 1394096)
 * make sure that --config is early evaluated for config options such as CPL_VSIL_CURL_CHUNK_SIZE that are read early

Multi driver changes:
 * HFA and KEA: better support for writing RATs (trac #4903)
 * Fix creation of large enough datasets with drivers EHdr, ENVI and ISCE that failed due to inappropriate check on file size whereas the file wasn't filled yet (#705, 2.3.0 regression)

BAG driver:
 * avoid warnings when reading georeferencing
 * get datetime
 * safer retrieval of variable extents
 * add read support for variable-resolution grids, and write support for single-resolution grids

E00GRID driver:
 * correctly parse projection sections that have lines with tildes (#894)

EHdr driver:
 * only write .stx if bApproxOK=false (#514)

ENVI driver:
 * support reading truncated datasets (#915)

ERS driver:
 * fix quadratic performance in parsing .ers header. Fixes https://bugs.chromium.org/p/oss-fuzz/issues/detail?id=8404
 * avoid excessive memory allocation. Fixes https://bugs.chromium.org/p/oss-fuzz/issues/detail?id=8497
 * avoid potential stack overflow. Fixes https://bugs.chromium.org/p/oss-fuzz/issues/detail?id=8614
 * prevent infinite recursion. Fixes https://bugs.chromium.org/p/oss-fuzz/issues/detail?id=8744
 * avoid potential bad cast. Fixes https://bugs.chromium.org/p/oss-fuzz/issues/detail?id=8745

FIT driver:
 * avoid excessive block size on creation. Fixes https://bugs.chromium.org/p/oss-fuzz/issues/detail?id=8292
 * error out in CreateCopy() on failed I/O on source dataset. Fixes https://bugs.chromium.org/p/oss-fuzz/issues/detail?id=8338

GeoRaster driver:
 * Fix the issue by freeing the temporary lobs created by readCLob() and writeCLOB().

GPKG driver:
 * fix memleak if I/O error occurs on write
 * retrieve original raster file when using gdal_translate -co APPEND_SUBDATASET=YES with other gdal_translate switches
 * copy source metadata when using TILING_SCHEME
 * properly delete gridded coverage raster layers

GRIB driver:
 * replace DataSource and derived classes with VSILFILE directly for > 4GB file support on Windows
 * turn printf() warning as CPLDebug() messages
 * read and write missing data values correctly for complex packing when original data is integer (#1063)
 * g2clib: avoid int overflow. Fixes https://bugs.chromium.org/p/oss-fuzz/issues/detail?id=8194
 * g2clib: avoid potential out of bound access (CID 1393528)
 * degrib: fix floating point division by zero. Fixes https://bugs.chromium.org/p/oss-fuzz/issues/detail?id=9407.
 * degrib: avoid potential floating point division by zero. Fixes https://bugs.chromium.org/p/oss-fuzz/issues/detail?id=10291
 * add UNIT[] node to SRS on reading, so that is valid

GTiff driver:
 * Add TIFF Lerc codec (in GTiff driver itself)
 * Add TIFF WebP codec (in libtiff)
 * save XMP on field TIFFTAG_XMLPACKET (#767)
 * fix retrieving mask band of overview band when the mask is external. Fixes -co COPY_SRC_OVERVIEWS=YES of such datasets (#754)
 * improve progress report in CreateCopy() when there is a mask (#935)
 * improve performance reading multi-band 1-bit data. Fixes https://bugs.chromium.org/p/oss-fuzz/issues/detail?id=7840.
 * internal libtiff: updated to libtiff 4.0.10
 * internal libgeotiff: resync with upstream.
 * workaround bug in currently released libgeotiff versions, where when rewriting a ASCII key with a string value longer than the original value (#641)
 * allow the use of PREDICTOR with ZSTD compression
 * avoid various memory corruptions in case of some corrupted file. Fixes https://bugs.chromium.org/p/oss-fuzz/issues/detail?id=8488
 * load PAM if not already done when GetDefaultRAT() is called
 * fix missing #ifdef causing compilation failure due to missing bTryCopy (#946)

HDF4 driver:
 * quote swath and field names if needed (if they contain spaces, column, quotes) in HDF4_EOS subdataset names

HDF5 driver:
 * add VSI functionality (#786)
 * fix reading variable names with single character (#622)
 * fix HDF5 object leak (thus preventing file closing) on datasets with variable length attributes (#933)

HFA driver:
 * fix floating point division by zero. Fixes https://bugs.chromium.org/p/oss-fuzz/issues/detail?id=9201
 * fix division by zero. Fixes https://bugs.chromium.org/p/oss-fuzz/issues/detail?id=10190

HTTP driver:
 * do not immediately delete a file used by the JP2OpenJPEG driver

ILWIS driver:
 * Fix performance issue on creation with big number of bands. Fixes https://bugs.chromium.org/p/oss-fuzz/issues/detail?id=9062

IRIS driver:
 * add UNIT[] node to SRS on reading, so that is valid
 * avoid infinite loop. Fixes https://bugs.chromium.org/p/oss-fuzz/issues/detail?id=8256 and 8439

ISCE driver:
 * do not try to stat() mainfile.xml if mainfile does not exist

ISIS3 driver:

FITS driver:
 * allow reading/writing beyond 2 billion pixels limit

JP2OpenJPEG driver:
 * allow YCC for non-Byte datasets; and allow 4-band MCT with openjpeg >= 2.2
 * add CODEBLOCK_STYLE creation option for OpenJPEG >= 2.3
 * add support for generating and using external overviews

JPEG driver:
 * slightly improve performance of whole RGB image loading with pixel-interleaved buffer
 * Internal libjpeg: Avoid integer overflow on corrupted image in decode_mcu_DC_first() (https://bugs.chromium.org/p/oss-fuzz/issues/detail?id=9447)

JPEGLS driver:
 * Add support for CharLS 2 (#632)

MRF driver:
 * Add TestBlock(), skip empty areas when building overviews
 * Fix detection of Lerc2 data
 * Resync with upstream LercLib and put it in third_party/LercLib

NetCDF driver:
 * add VSI functionality on Linux (#786)
 * add support for longitude values wrapping at 180deg of longitude (#1114)
 * avoid use of uninitialized variable when reading blocks in creation mode

NGSGEOID driver:
 * report a CRS that conforms to the official publications for GEOID2012 and USGG2012 datasets (#1103)

NITF driver:
 * avoid excessive processing time on corrupted files. Fixes https://bugs.chromium.org/p/oss-fuzz/issues/detail?id=8972
 * avoid heap-buffer-overflow for VQ compression. Fixes https://bugs.chromium.org/p/oss-fuzz/issues/detail?id=9467

PCIDSK driver:
 * add back support for creating external overviews, removed years ago when switching to the new PCIDSK SDK (#887)

PCRaster driver:
 * libcsf: avoid potential out of bound access (CID 1074445)

PDF driver:
 * Remove forced use of libstdc++ for macOS when building plugin (#888)

PDS driver:
 * deal with detached labels whose line spacing is not a multiple of record size (#955)
 * add support for reading CRISM images
 * add support for ^QUBE = number for multi-band images

PDS4 driver:
 * fix georeferencing reading/writing to use pixel corner convention (#735)
 * add UNIT[] node to SRS on reading, so that is valid

PostgisRaster driver:
 * add support for out-db rasters (Trac #3234)
 * use ST_BandFileSize of PostGIS 2.5 when available for outdb_resolution=client_side_if_possible
 * improve performance of line by line reading; add performance hints section in the doc
 * fix CreateCopy() when PostGIS is not in public schema
 * add quoting of identifiers

PRF driver:
 * Fix Photomod x-dem files georeference

RasterLite2 driver:
 * fail on Create() that is not supported

RDA driver:
 * enable support for DG RDA Image Reference string

RMF driver:
 * Add support JPEG compressed RMF datasets (#691)
 * Add optional projection import/export from EPSG code (#701)
 * Create compressed datasets: LZW, DEM, JPEG (#732)
 * Cache decoded tile to improve performance of interleaved access
 * Parallelize data compression, add internal tile write-cache, add compressed overviews support (#748)
 * Fix 4-bit dataset reading
 * Expose NBITS to metadata

RS2 driver:
 * avoid potential memleak (CID 1393537, CID 1393534, CID 1074387)

SENTINEL2 driver:
 * Add support of S2x_MSIL2A files (#1069)

SGI driver:
 * writer: avoid out-of-bound buffer access. Fixes https://bugs.chromium.org/p/oss-fuzz/issues/detail?id=8889

SIGDEM driver:
 * be more robust against excessive memory allocation attempt
 * avoid floating point division by zero. Fixes https://bugs.chromium.org/p/oss-fuzz/issues/detail?id=11220

SDTS driver:
 * avoid long processing time on corrupted dataset. Fixes https://bugs.chromium.org/p/oss-fuzz/issues/detail?id=11219

SRP driver:
 * ASRP/USRP: allow opening files padded with 0x5E / ^ character without emitting error (#838)

USGSDEM driver:
 * fix reading of Benicia.dem and Novato.dem (trac #4901, #583)
 * optimize I/O access a bit
 * avoid potential out-of-bounds access (CID 1393532)
 * avoid int overflow. Fixes https://bugs.chromium.org/p/oss-fuzz/issues/detail?id=9475

VRT driver:
 * GDALCreateWarpedVRT(): automatically set GCI_AlphaBand on the alpha band
 * data/gdalvrt.xsd: add GDALRasterAttributeTable (#818)
 * VRTComplexSource: make sure that min and max values in case of exponential resampling are properly computed
 * ComputeStatistics(): optimize when nodata is set on a single SimpleSource
 * Fix issue when opening VRT with large number of bands (#1048)
 * deal with serialized nodata value that is slightly outside Float32 validity range (#1071)
 * fix source window computation that caused sub-pixel shift with non-nearest resampling
 * fix potential int overflow on invalid VRT

WCS driver:
 * Parse envelopes with time periods. Improve error reporting. Fix one SUBDATASETS metadata thing. More metadata from Capabilities to metadata. Print some URLs in debug mode. Unique subset params in URLs. Add time domain interval to metadata.
 * GeoServer does not like primary subsets to have postfixes. Fix service dirty issue. Add GeoServer TimeDomain coverage metadata. Do not put service parameter into subdataset name and use generic coverage parameter
 * deal with GDALPamDataset::GetMetadata returning nullptr. (#648)

WMTS driver:
 * avoid issue with reprojection of layer extent into TileMatrixSet SRS
 * fix issue with inappropriate zoom level being selected causing integer overflow in raster dimension computation
 * fix potential off-by-one pixel when compositing the underlying WMS/TMS source into the final raster

## OGR 2.4.0 - Overview of Changes

Core:
 * Add JSON field subtype for String fields
 * OGR SQL: avoid int overflow on -(-9223372036854775808) evaluation. Fixes https://bugs.chromium.org/p/oss-fuzz/issues/detail?id=8717
 * OGR SQL: evaluation modulo operator on floating point values as a floating point modulo. Fixes https://bugs.chromium.org/p/oss-fuzz/issues/detail?id=8761.
 * OGR SQL: swq_expr_node::Evaluate(): avoid too deep recursion. Fixes https://bugs.chromium.org/p/oss-fuzz/issues/detail?id=8836
 * OGRLineString::TransferMembersAndDestroy(): fix crashing issue with M component. Fixes https://bugs.chromium.org/p/oss-fuzz/issues/detail?id=8773.
 * OGRGeometry::exportToGEOS(): fix potential out-of-bounds write on some GeometryCollection with TIN/PolyhedralSurface (#688)
 * OGRGeometryFactory::organizePolygons(): improve performance for polygons with many consecutive identical nodes. Fixes https://bugs.chromium.org/p/oss-fuzz/issues/detail?id=9618
 * Fix IsValid() for a TRIANGLE with GEOS, but without SFCGAL
 * morphToESRI(): fix a heap user-after-free.
 * OGRFeature::SetField( int iField, const char * const * papszValues ): avoid potential invalid access to pauFields[iField].StringList.paList
 * SQLite dialect: fix when ROWID is used in WHERE clause and the source layer has a real FID column name
 * GML geometry parser: recognize MultiGeometry.geometryMembers syntax (refs https://issues.qgis.org/issues/19571)
 * OGRGetXMLDateTime(): Interpret TZFlag correctly (#996)
 * OGRFeatureStyle: Restore font field at OGRStyleSymbol

OGRSpatialReference:
 * importFromEPSG(): append ' (deprecated)' at end of deprecated GCS and GEOCCS (#646)
 * ogr_opt.cpp: fix wrong values and add missing values in papszProjectionDefinitions[]
 * importFromProj4/exportToProj4: fix typo in the PROJ method name of InternalMapOfTheWorldPolyconic which is imw_p
 * Krovak: explicit that alpha and Pseudo_standard_parallel_1 are hardcoded in PROJ
 * SetNormProjParm(): avoid division by zero. Fixes https://bugs.chromium.org/p/oss-fuzz/issues/detail?id=10588
 * FindMatches(): if the input SRS has a EPSG code, check that its definition and the EPSG one actually matches (#990)

Utilities:
 * ogr2ogr: reject -append, -select options together
 * ogr2ogr: speed-up in case of big number of field name clashes. Fixes https://bugs.chromium.org/p/oss-fuzz/issues/detail?id=8960
 * ogr2ogr: make -clipsrc work when output dataset has no geometry field (#943)
 * ogrlineref: fix tolerance for not geographic spatial reference
 * ogrmerge.py: avoid exception in error code path of GetOutputDriverFor()
 * ogrmerge.py: fix issue with non-ASCII characters (fixes #1067)

Multi driver changes:
 * KML/LIBKML: robustify for out-of-memory conditions (fixes https://issues.qgis.org/issues/19989)
 * XLSX / ODS: avoid harmless warning in some cases when guessing column data types

AVCE00 driver:
 * avoid perforance issues on huge lines. Fixes https://bugs.chromium.org/p/oss-fuzz/issues/detail?id=8284
 * fix performance issue on reading PRJ section. Fixes https://bugs.chromium.org/p/oss-fuzz/issues/detail?id=9061

BNA driver:
 * refuse to open existing file in update mode, since it causes later crashes when attempting to add new features (https://issues.qgis.org/issues/18563)
 * avoid long processing. Fixes https://bugs.chromium.org/p/oss-fuzz/issues/detail?id=8754
 * more efficient building of polygons. Fixes https://bugs.chromium.org/p/oss-fuzz/issues/detail?id=10951

CAD driver:
 * Fix read objects map. Upgrade version of libopencad to 0.3.4. (#677)
 * Fix wrong OGRCircularString construction from CADCircle. (#736)
 * avoid integer overflow. Fixes https://bugs.chromium.org/p/oss-fuzz/issues/detail?id=8385
 * Fix buffer overflow on skip read. Fixes https://bugs.chromium.org/p/oss-fuzz/issues/detail?id=9107

Carto driver:
 * Use new /sql/copy-from end point for writing (#715)
 * fix ICreateFeatureCopy() with unset fields

CSV driver:
 * in writing, use WKT instead of actual geometry column name if GEOMETRY=AS_WKT mode is used without CREATE_CSVT=YES (fixes #660)
 * writer: limit to 10000 fields to avoid performance issues. Fixes https://bugs.chromium.org/p/oss-fuzz/issues/detail?id=9336 (8449 as well)
 * avoid endless loop when iterating and updating features (#919)

DXF driver:
 * add PaperSpace field (Trac #7121)
 * allow attributes with spaces in the tag
 * Correctly handle non-uniformly-weighted spline HATCH boundaries (#1011)
 * avoid null pointer dereference when DXF_MAX_BSPLINE_CONTROL_POINTS is hit. Fixes https://bugs.chromium.org/p/oss-fuzz/issues/detail?id=8254.
 * Fix reporting of wrong line numbers in errors (fixes #726)
 * Don't crash when spline control point limit is reached
 * fix memory leak in case of attempt to write GeometryCollection of unsupported type

EDIGEO driver:
 * reading multipolygons (trac #6955, #711)

ElasticSearch driver:
 * add compatibility with ES v6.0
 * add a USERPWD open option
 * add lazy loading of layers
 * add a LAYER open option
 * skip xpack indices, and do not emit 503 error when listing unauthorized layers
 * add a INDEX_DEFINITION layer creation option
 *allow several geometry fields of type GEO_POINT to be created

ESRIJson driver:
 * parse documents that lack 'geometryType' member (#914)

GeoJSON driver:
 * fix type deduction when there is a Feature.id of type string and Feature.properties.id of type int. The later has precedence over the former (arbitrary decision) (#669)
 * properly flush the file in SyncToDisk() in append situations (https://issues.qgis.org/issues/18596)
 * parse '{"type": "GeometryCollection", "geometries": []}' as empty geometrycollection
 * increase max memory allowed to parse a single feature (#807)
 * remove topojson from extensions recognized by the driver
 * add partial support for field names differing by case (#1057)
 * RFC7946 writer: clip and offset geometries outside [-180,180] (#1068)
 * no longer write NaN/Infinity values by default (#1109)

GeoRSS driver:
 * avoid excessive processing time. Fixes https://bugs.chromium.org/p/oss-fuzz/issues/detail?id=9263

GML driver:
 * avoid fetching SRS from http
 * fix potential memory leak in case of duplicated name of geometry fields in .gfs
 * improve performance for large number of attributes. Fixes https://bugs.chromium.org/p/oss-fuzz/issues/detail?id=9038.

GPKG driver:
 * add read/write support for JSON field subtype
 * speed up GetExtent() on huge tables with rtree. Refs https://issues.qgis.org/issues/18402
 * take into ROLLBACK TO SAVEPOINT to invalidate cached feature count
 * make sure to not invalidate POSIX advisory locks
 * remove useless check that encoding is UTF-8 (#793)
 * fix typo in gpkg_metadata_reference_column_name_update trigger definition
 * optimize table renaming by avoiding to drop the spatial index, but just renaming it

IDF driver:
 * use a temporary SQLite database (when driver available) for files larger than 100 MB
 * add support for Z coordinate (#964)

LIBKML driver:
 * add support for reading several schemas for the same layer (#826)
 * make edition of existing file work (https://issues.qgis.org/issues/18631)
 * implement OGRLIBKMLLayer::SyncToDisk() to fix https://issues.qgis.org/issues/18631
 * workaround weird issue with OSGeo4W and newline characters in <coordinate> element (fixes https://issues.qgis.org/issues/19215)

MITAB driver:
 * fix potential use of uninitialized memory
 * improve performance of adding many fields in a .tab. Fixes https://bugs.chromium.org/p/oss-fuzz/issues/detail?id=8559
 * fix geometry corruption when editing some datasets (#817)
 * fix writing .tab when field name has invalid characters in it (#924)
 * Expose font family to feature style symbol ID

MSSQLSpatial driver:
 * Add support for MSODBCSQL (#1136)
 * Fix layer-schema separation problem (#586)
 * Fix bulk copy for multiple layers (#619)
 * Accept datetime values (#841)
 * Don't truncate string values on Unix (#843)
 * Create 3D features (#852)
 * enforce read-only/update mode for CreateFeature/SetFeature/DeleteFeature
 * fix retrieval of geometry column on Linux for SQL result layers
 * Use only valid SRIDs; Create features preserving SRID (#860)
 * Fix geometry parser with M values (#1051)
 * Assign new ID following an INSERT (#1052)
 * Adding configuration option MSSQLSPATIAL_ALWAYS_OUTPUT_FID (#1101)

MVT driver:
 * writer: do not ignore Z/M/ZM geometries
 * writer: make it possible to output to /vsizip/output.zip out-of-the-box
 * disable check on 'extent' field in identifiation method, which rejected tiles with extent > 16384
 * avoid recursion on opening. Fixes https://bugs.chromium.org/p/oss-fuzz/issues/detail?id=10226

MySQL:
 * add support for MySQL 8.0

NAS driver:
 * speed-up in case of huge number of attributes. Fixes https://bugs.chromium.org/p/oss-fuzz/issues/detail?id=7977

OCI driver:
 * Ensure table Dims and GTYPE are retrieved for the correct table (#629)

OGR_GMT driver:
 * avoid performance issue when opening layer with big number of fields. Fixes https://bugs.chromium.org/p/oss-fuzz/issues/detail?id=8259.

OGR_PDS driver:
 * avoid int overflow. Fixes https://bugs.chromium.org/p/oss-fuzz/issues/detail?id=9136

OpenFileGDB driver:
 * Catch a NaN in FileGDBDoubleDateToOGRDate to prevent undefined behavior. (#740)
 * fix potential crash on corrupted datasets. Fixes https://bugs.chromium.org/p/oss-fuzz/issues/detail?id=11313

OSM driver:
 * allow parsing files with up to 10 000 nodes per way (#849)
 * avoid array overflow with ways with many tags. Relates to https://bugs.chromium.org/p/oss-fuzz/issues/detail?id=9618

PCIDSK driver:
 * defer writing of segment header to improve performance when creating huge number of fields. Fixes https://bugs.chromium.org/p/oss-fuzz/issues/detail?id=8191
 * fix performance issue when inserting in layer with huge number of fields. Fixes https://bugs.chromium.org/p/oss-fuzz/issues/detail?id=8335.

PDF driver:
 * fix the parser of drawing instructions that had issues with array objects

PG driver:
 * add SPATIAL_INDEX=SPGIST/BRIN layer creation options (and PGDump as well) (#892)
 * add read/write support for JSON field subtype

PGDump driver:

PLScenes driver:
 * fix /vsicurl/ raster download
 * fix scene activation
 * add ground_control field in layer definition

Shapefile driver:
 * avoid being dependent on correctness of file size field in .shp
 * fix corruption when deleting a field from a .dbf without records (#863)
 * Add CP1251 codepage name synonym (ANSI 1251) for DBF files.

S57 driver:
 * add S57_AALL, S57_NALL, S57_COMF, S57_SOMF creation options (#810)

SOSI driver:
 * fix memory leaks / null pointer dereference

SQLite/Spatialite driver:
 * avoid SetFeature() to reset the iterator (#964)
 * Spatialite: read table name in its original case (#1060)
 * do not run spatial index creation in rollback code

VFK driver:
 * create index on ID column only for selected (geometry-related) layers (#498)
 * create db indices after inserting data (#498)
 * create indices before resolving geometry
 * new open option - include filename field (#564)
 * speed up sequential feature access
 * fix leak of unfinalized statement (#634)
 * fix file check on Windows with large files (#637)
 * fix big int overflow, force text attributes (PODIL_CITATEL/PODIL_JMENOVATEL) to avoid int64 overflow (#672)
 * fix missing geometry for SBPG layer (#710)
 * missing fields in update mode gfs (#734)

VRT driver:
 * revise logic for handling the <FID> element (or when it is omitted) (#941)

WFS driver:
 * avoid potential bad cast. Fixes https://bugs.chromium.org/p/oss-fuzz/issues/detail?id=9800

WFS3 driver:
 * update to current version (May 2018) of the API draft (#626)
 * add USERPWD open option

XLSX driver:
 * avoid stack buffer overflow is creating too many fields. Fixes https://bugs.chromium.org/p/oss-fuzz/issues/detail?id=8198
 * avoid timeout. Fixes https://bugs.chromium.org/p/oss-fuzz/issues/detail?id=8225
 * fix null pointer dereference. Fixes https://bugs.chromium.org/p/oss-fuzz/issues/detail?id=8286

## SWIG Language Bindings

All bindings:
 * use a dedicated VSILFILE class to avoid type mismatch (#601)
 * reload drivers if GDAL_SKIP/OGR_SKIP is defined with --config in gdal/ogr.GeneralCmdLineProcessor()
 * Add VSIFFlushL()
 * Add VSIErrorReset()

CSharp bindings:
 * GDALCreateCopy.cs sample code: Correct misspelling in info message (#647)
 * SWIG 2.x and 3.x compatibility (#824)

Java bindings:
 * update to Java 1.6 requirement to please 'ant maven' target
 * Fix JNI library bundling.

Perl bindings:
 * fix make dependencies (#43)

Python bindings:
 * Python logging improvements: add gdal.ConfigurePythonLogging() function  (#1017)
 * added support for band / pixel interleave for numpy array
 * fixed NULL check on python's swig VSIFReadL (#572)
 * fix dataset[slice] access (#574)
 * added support for reading vsi data as memorybuffer and writing buffers via VSIFWriteL
 * setup.py: add more prominent warning when numpy is not available (#822)
 * for utilities as library functions, when gdal.UseExceptions() is enabled, do not emit Python exceptions when the operation is reported as successful
 * update import path
 * avoid crash in ReadRaster() under low memory condition (#1026)

# GDAL/OGR 2.3.0 Release Notes

Note: due to the change of SCM during the development, #XXXX still refers to
Trac tickets. GitHub tickets are explicitly indicated with github #XXXX

## In a nutshell...

 * New GDAL drivers:
   - PDS4: read/write
   - RDA: DigitalGlobe Raster Data Access (read-only)
 * New OGR drivers:
   - MVT: add read/write driver for Mapbox Vector Tile standalone files or tilesets
   - ESRIJson and TopoJSON: read-only, split from existing code of the GeoJSON driver
   - WFS3: *experimental* read-only driver
 * RFC 68: Make C++11 a build requirement
    https://trac.osgeo.org/gdal/wiki/rfc68_cplusplus11
 * RFC 70: Guess output format from extension of output filename, if no explicit format passed to C++ or Python utilities
    https://trac.osgeo.org/gdal/wiki/rfc70_output_format_guess
 * RFC 71: Move to GitHub for source code repository and issue tracker.
    https://trac.osgeo.org/gdal/wiki/rfc71_github_migration
 * Significantly improved drivers:
    - MBTiles (vector tiles support)
    - DXF
    - GRIB (GRIB2 write support)
    - WCS (support WCS 2.0)
 * Improvements in network-based /vsi handlers: /vsicurl, /vsis3, /vsigs. Add:
     - /vsiaz for Microsoft Azure Blobs
     - /vsioss for Alibaba Cloud Object Storage Service
     - /vsiswift/ for OpenStack Swift object storage
 * Command line utilities: use Unicode main on Windows to avoid issues with non-ASCII characters (#7065)
 * Update to EPSG v9.2 (#7125)
 * Update data/esri_extra.wkt and add data/esri_epsg.wkt, taken from https://github.com/Esri/projection-engine-db-doc (Apache v2 license) (#2163)
 * Add support for PROJ.5 new API (requires proj 5.0.1 or later). PROJ 4.X is still supported.
 * More than 1000 fixes for issues/vulnerabilities found by OSS-Fuzz
 * Remove raster OGDI driver (vector OGDI driver still there) (#7087)

## New installed files

 * data/pds4_template.xml
 * data/esri_epsg.wkt

## Backward compatibility issues

See MIGRATION_GUIDE.TXT

## GDAL/OGR 2.3.0 - Build changes

Build(Unix):
 * refresh config.{guess,sub} from latest upstream; upgrade to libtool 2.4.6 files
 * add a --with-rename-internal-shapelib-symbols option that defaults to yes if --with-hide-internal-symbols is set; and make -with-rename-internal-libtiff/libgeotiff-symbols also defaults to yes when --with-hide-internal-symbols is set
 * add a --with-charls switch (enabled by default) to compile that JPEGLS driver
 * make --without-static-proj and --with-fgdb an error when filegdb (>= 1.5) embeds proj.4 symbols
 * add --with-zstd switch (for GTiff ZStd compressino with internal libtiff)
 * add support for ECW SDK 5.4, by detecting if we must link against the newabi or oldabi link
 * fix detection of 64bit file API with clang 5 (#6912)
 * GNUmakefile: add a static-lib and install-static-lib targets
 * use .exe extension when building with mingw64* toolchains (#6919)
 * Pass --silent to libtool in compile (C and C++), link, install and clean modes.
 * Limit number of installed cpl*.h files installed to a fixed list.
 * configure / m4/acinclude.m4: replace use of CCFLAGS by plain CFLAGS (github #529)
 * configure / m4/acinclude.m4: require 'long long' type (github #530)

Build(Windows):
 * always build the PDF driver, even when none of poppler/podofo/pdfium are available, in which case it is write-only (#6938)
 * add new targets bindings, bindings_install and bindings_clean that depend on the new BINDINGS option in nmake.opt (#6948)
 * for Kakadu, add capability to build as a plugin, and make it possibly to link only against the Kakadu .lib/.dll instead of incorporating some of its .obj (#6940)
 * nmake.opt: Ensure PDB is included in release DLL if WITH_PDB requested (#7055)
 * nmake.opt: use /MDd for OPTFLAGS for DEBUG=1 builds (#7059)
 * nmake.opt: avoid some settings to be defined unconditionally (#5286)
 * nmake.opt: add configuration to enable openssl (which is needed for thread-safe curl use)

Build(All):
 * fix compilation error with Crypto++ 7.0.0 (github #541)

Developer corner:
 * Add scripts/setdevenv.sh to setup env variables needed for running GDAL without installing it

## GDAL 2.3.0 - Overview of Changes

Port:
 * Add CPLJSONDocument/Object/Array - C++ thin wrapper around json-c library. (github #282)
 * /vsicurl/: fix occasional inappropriate failures in Read() with some combinations of initial offset, file size and read request size (#6901)
 * /vsicurl/: add a CPL_VSIL_CURL_NON_CACHED configuration option, so as to be able to specify filenames whose content must not be cached after dataset closing
 * /vsicurl/: honour GDAL_HTTP_MAX_RETRY and GDAL_HTTP_RETRY_DELAY config options. Add extended filename syntax to pass options use_head, max_retry, retry_delay and list_dir.
 * /vsicurl/: enable redirection optimization on signed URLs of Google Cloud Storage. Helps for the PLScenes driver (fixes #7067)
 * /vsicurl/ and derived filesystems: redirect ReadDir() to ReadDirEx() (#7045)
 * /vsicurl/ and related file systems: add compatibility with HTTP/2 (requires recent enough curl, built against nghttp2). Can be controlled with the GDAL_HTTP_VERSION=1.0/1.1/2/2TLS
 * /vsicurl/: fix 2.2 regression regarding retrieval of file size of FTP file (#7088)
 * /vsicurl/: when stat'ing a file, fallback from HEAD to GET if the server issues a 405 error
 * Add a VSICurlClearCache() function (bound to SWIG as gdal.VSICurlClearCache()) to be able to clear /vsicurl/ related caches (#6937)
 * CPLHTTPSetOptions(): use SearchPathA() for curl-ca-bundle.crt on Windows. See https://github.com/curl/curl/issues/1538
 * CPLHTTPFetch() / vsicurl: add retry on HTTP 429, and add exponential backoff logic for retry delay
 * CPLHTTPFetch(): when openssl is enabled, and used by libcurl, use openssl thread safety mechanism to avoid potential crashes in multithreading scenarios
 * CPLHTTPFetch(): add a SSL_VERIFYSTATUS option / GDAL_HTTP_SSL_VERIFYSTATUS configuration option to check OCSP stapling
 * CPLHTTPFetch(): add a USE_CAPI_STORE option / GDAL_HTTP_USE_CAPI_STORE configuration option to use certificates from the Windows certificate store
 * Ignore SIGPIPE that may arose during curl operations (mostly when using OpenSSL for TLS)
 * Add CPLHTTPMultiFetch() and CPLMultiPerformWait()
 * /vsis3/: support reading credentials from ~/.aws/credentials, ~/.aws/config or IAM role on EC2 instances
 * /vsis3/: properly handle cases where a directory contains a file and subdir of same names; implement Mkdir() and Rmdir()
 * /vsis3/: fix Seek(Tell(), SEEK_SET) fails if current position is not 0 (#7062)
 * /vsis3/: properly handle 307 TemporaryRedirection (#7116)
 * /vsis3/: fix support of bucket names with dot in them (#7154)
 * /vsis3/: make multipart upload work with Minio
 * /vsigs/: add new authentication methods using OAuth2 refresh token or service account or Google Compute Engine VM authentication, or using ~/.boto file
 * /vsigs/: add write, Unlink(), Mkdir() and Rmdir() support
 * /vsigs/: allow authentication to be done with the GOOGLE_APPLICATION_CREDENTIALS configuration option pointing to a JSon file containing OAuth2 service account credentials
 * /vsis3/ and /vsigs/: take into account user provided x-amz- / x-goog- HTTP headers with GDAL_HTTP_HEADER_FILE
 * Fix CPLReadDirRecursive() to behave properly on /vsis3/ buckets that have foo (file) and foo/ (sub-directory) entries (#7136)
 * /vsis3/: add a AWS_NO_SIGN_REQUEST=YES configuration option to disable request signing (#7205)
 * /vsis3, /vsigs, /vsioss, /vsiaz: fix support of non-ASCII characters in keys (#7143)
 * Add VSIGetActualURL(), typically to expand /vsis3/ paths to full URLs, and bind it to SWIG as gdal.GetActualURL()
 * Add VSIGetSignedURL()
 * Add VSIGetFileSystemsPrefixes() and VSIGetFileSystemOptions()
 * CPLFormFilename() / CPLProjectRelativeFilename(): add /vsis3 and similar file systems to the list of filesystems requiring unix separator (github #281)
 ù Make CPLFormFilename() properly work with http[s:]// filenames
 * Add a CPLGetErrorCounter() function that can be used to test if new errors have been emitted
 * Add cpl_safemaths.hpp to detect integer overflows (#6229)
 * /vsigzip/: avoid trying to write a .gz.properties file on a /vsicurl/ file (#7016)
 * CPLStrtod(): parse string like '-1.#IND0000000' as NaN instead of -1 (seen when looking at refs #7031, but does not fix it)
 * Fix CPLCopyTree() that doesn't properly on MSVC 2015 (and possibly other platforms) (#7070)
 * /vsimem/: to improve Posix compliance, do not make Seek() after end of file error out in read-only mode
 * cpl_config.h.vc: define HAVE_LONG_LONG 1; cpl_port.h remove MSVC specific logic for int64 (github #264)
 * /vsisparse/: make Read() detect end of file
 * GDALVersionInfo("BUILD_INFO"): report if GEOS is available
 * Add VSIMkdirRecursive() and VSIRmdirRecursive()
 * Add CPLGetHomeDir()
 * CPLSetErrorHandler(): avoid later crashes when passing a null callback (github #298)
 * CPLHTTPParseMultipartMime(): make it format the pasMimePart[].papszHeaders in a standard key=value format without EOL
 * CPLString: avoid std::string symbols to be exported with Visual Studio (#7254)
 * I/O on Android: add support for 64-bit file operations if API level >= 24 (Android 7.0 or later) (github #339)

Core:
 * add GDALDataTypeIsFloating, GDALDataTypeIsSigned, GDALDataTypeUnionWithValue, GDALFindDataType, GDALFindDataTypeForValue (github #215). Add GDALDataTypeIsInteger()
 * Cleanup ARE_REAL_EQUAL() and GDALIsValueInRange() (#6945, #6946)
 * Various SSE2/AVX2 optimizations for GDALCopyWords()
 * GDALGCPsToGeoTransform(): add GDAL_GCPS_TO_GEOTRANSFORM_APPROX_OK=YES and GDAL_GCPS_TO_GEOTRANSFORM_APPROX_THRESHOLD=threshold_in_pixel configuration option (#6995)
 * RawDataset::IRasterIO(): don't assume all bands are RawRasterBand
 * GDALOpenInfo: make number of bytes read at opening configurable with GDAL_INGESTED_BYTES_AT_OPEN
 * GDALCopyWholeRasterGetSwathSize(): try to use at least 10 MB for swath size
 * GDALDatasetCopyWholeRaster(), GDALRasterBandCopyWholeRaster(), GDALCreateCopy(): always call AdviseRead() on the full extent of the source dataset (#7082)
 * make DefaultCreateCopy() copy RAT
 * Generate gcore/gdal_version.h from git date and sha for a dev version (Unix builds only)
 * Add GDALDataset::Open()
 * Add C++ iterators for layers, bands and features in GDALDataset
 * External .ovr: make sure that ExtraSamples tag is written
 * Overview creation: avoid creating too many levels, and fix related heap buffer overflow (github #557)
 * SetStatistics(): write a STATISTICS_APPROXIMATE=YES metadata item if bApproxOK=true, and take it into account in GetStatistics() (#4857,#4576)

Algorithms:
 * Contour: make sure no 3D geometry is created unless -3d switch is defined (#336)
 * Warper: revise/improver how working data type is inferred from other parameters
 * Warper: when operating on single-band, skip target pixels whose source center pixel is nodata (2.2 regression, #7001)
 * Warper: avoid blocking when number of rows is 1 and NUM_THREADS > 1 (#7041). Also limit the number of threads so that each one processes at least 65536 pixels
 * Warper: use AdviseRead() when source chunks are sufficiently compact (#7082)
 * Warper: fix rounding error in scale factor computation (github #273)
 * Warper: use panSrcBands[0] in the single band case (regression fix, github #295)
 * Warper: add very special case to handle situation where input raster edge touches dateline, but proj.4 transformation involves a discontinuity (#7243)
 * Geoloc transformer: fix systematic pixel shift (github #244)
 * RPC transformer: set output coordinates to HUGE_VAL when failure occurs, so that a following coordinate transformation can detect the error too (#7090)
 * RPC transformer: return NULL at instantiation if the specified RPC_DEM file cannot be opened
 * Export GDALRegisterTransformDeserializer() and GDALUnregisterTransformDeserializer() (#5392)
 * GDALRasterize(): avoid hang in some cases with all_touched option (#5580)
 * Optimize GDALResampleConvolutionVertical() and GDALPansharpenOperation::WeightedBroveyPositiveWeightsInternal() for SSE2 / AVX
 * Overview / resampling: speed-up bicubic upsampling for SSE2
 * GDALGrid() with linear algorithm: avoid assertions/segmentation fault when GDALTriangulationFindFacetDirected() fails (#7101)
 * GDALComputeProximity(): fix int32 overflow when computing distances on large input datasets (#7102)
 * GDALAllRegister(): make sure that all drivers that need to look for sidecar files are put at the end

Utilities:
 * GDAL and OGR C++ and Python utilities: accept -f or -of to specify output format
 * --optfile: re-parse inlined content with GDALGeneralCmdLineProcessor(), in particular to support --config key value in option file
 * gdalsrsinfo: for consistency with other output, do not quote the proj.4 string output
 * gdal_rasterize: fix segfault when rasterizing onto a raster with RPC (#6922)
 * gdal_rasterize: add a -to option to specify transformer options
 * gdal_rasterize / GDALRasterizeGeometries(): optimize rasterization for large number of small geometries (#5716)
 * gdal_rasterize: fix crash in some situations with ALL_TOUCHED option (#7176)
 * gdaladdo: support not specifying explicitly overview factors, and add -minsize option
 * gdal_translate: add -a_scale / -a_offset (#7093)
 * gdal_translate: add -colorinterp / -colorinterp_X options
 * DefaultCreateCopy()/gdal_translate: do not destroy target file in case of failed copy wen using -co APPEND_SUBDATASET=YES (#7019)
 * gdal_translate: make -b mask[,xx] use the appropriate band data type (#7028)
 * gdal_translate property copy RAT (or not-copy RAT when -norat is specified)
 * gdalwarp: make -crop_to_cutline works when RPC transform is involved
 * gdalwarp: for RPC warping add a few extra source pixels by default
 * gdalwarp: -crop_to_cutline: reduce number of iterations to find the appropriate densification (#7119)
 * gdalwarp: do not set implicitly nodata on destination dataset when -dstalpha is specified (#7075)
 * gdalwarp: display errors (such as invalid open options) on successful opening of destination dataset
 * gdalwarp: fix "-dstnodata inf" (#7097)
 * gdalwarp: fix when several input datasets with different SRS are specified, and no explicit target SRS is provided (#7170)
 * gdalwarp: make sure to try to redefine the destination nodata value from the source nodata even if the newly created dataset has already set a default nodata value (#7245)
 * gdalwarp: improve progress meter when using multiple source files. For GDALWarp() function, make sure that the progress goes monotonically from 0 to 1. (#352)
 * gdal2tiles.py: fix GDAL 2.2 regression where tilemapresource.xml was no longer generated (#6966)
 * gdal2tiles: add --processes=intval option to parallelize processing (#4379)
 * gdalinfo --format / ogrinfo --format: report extra metada items in a 'Other metadata items:' section (#7007)
 * gdalinfo: make sure to display geodetic coordinates always in degree (and not potentially in another unit such as grad) (#4198)
 * gdalinfo: report 'Mask Flags: PER_DATASET NODATA' when NODATA_VALUES metadata item is specified
 * gdal_edit.py: add a -colorinterp_X red|green|blue|alpha|gray|undefined option to change band color interpretation
 * gdal_contour: return with non-0 code if field creation or contour generation failed  (#7147)
 * gdal_retile.py: fix failure if the filename contains % (percent) symbol (#7186)
 * gdalbuildvrt: make warnings about heterogeneous projection/band characteristics more explicit (#6829)
 * gdalbuildvrt: add support for band scale and offset (#3221)
 * gdal_fillnodata.py, gdal_pansharpen.py, gdal_polygonize.py, gdal_proximity.py, gdal_sieve.py, rgb2pct.py: avoid potential problem on Windows in verbose mode (github #458)

Sample Python scripts:
 * Add gdal_mkdir.py, gdal_rm.py and gdal_rmdir.py samples scripts
 * gdalcopyproj.py: fix use of GCP related API (github #255)
 * ogr2vrt.py: automatically set relativeToVRT=1 for input and output filenames givn in relative form in the same directory

Multi driver changes:
 * tag (and do needed changes) CALS, FUJIBAS, PAUX, SGI, RS2, GXF, TERRAGEN, Rasterlite, CPG, MSGN, Leveller as supporting GDAL_DCAP_VIRTUALIO

AIGRID:
 * fix handling on raw 32-bit AIG blocks (#6886)

BT driver:
 * make GetNoDataValue()/SetNoDataValue() use PAM

DIMAP driver:
 * do not report dummy geotransform (see https://lists.osgeo.org/pipermail/gdal-dev/2018-January/048014.html)

DTED driver:
 * Support VerticalCS for DTED and SRTM drivers when REPORT_COMPD_CS config option is set (github #237)

ECW driver:
 * fix Windows compilation against old ECW SDK and VS < 2015 (#6943)
 * make AdviseRead() to store its call parameters, and only do the actual work in RunDeferredAdviseRead() if TryWinRasterIO() determines that the IRasterIO() parameters are compatible of the AdviseRead() ones (#7082)
 * data/ecw_cs.wkt: fix PRIMEM of MONTROME (#2340)

EHdr driver:
 * support reading/writing .clr as/from RAT (#3253)
 * only write .stx if bApproxOK=false (github #514)

ENVI driver:
 * support 'major frame offsets' keyword (#7114)

ERS driver:
   add extension metadata (github #320)

GeoPackage driver:
 * update from 'tiled gridded extension' to now OGC approved 'tiled gridded coverage data extension' (OGC 17-066r1) (#7159)
 * avoid corruption of gpkg_tile_matrix when building overviews, down to a level where they are smaller than the tile size (#6932)
 * fix opening subdatasets with absolute filenames on Windows (https://issues.qgis.org/issues/16997)
 * fix possible assertion / corruption when creating a raster GeoPackage (#7022)
 * properly handle non-0 nodata value in edge tiles, especially with TILING_SCHEME creation option
 * do not write empty tiles for Float32 data type
 * speed-up statistics retrieval on non-Byte datasets (#7096)
 * make DELLAYER:rastertable / DROP TABLE rastertable delete the table and all references to it (#7013)
 * create single tiled TIFF tiles if they are not bigger than 512x512 pixels
 * avoid multi-threading issues when creating TIFF tiles with GDAL_NUM_THREADS defined.
 * fix overview creation with big overview factors on some datasets

GeoRaster driver:
 * handle memory allocation failures (#6884)
 * add support for GCP (#6973)

GTiff driver:
 * add support for ZSTD compression/decompression (requires internal libtiff, or libtiff HEAD)
 * when IRasterIO() realizes that several blocks are going to be needed, use MultiRangeRead() interface for /vsicurl/ related file systems to get data in parallel
 * change default value of BIGTIFF_OVERVIEW to be IF_SAFER (github #231)
 * make sure that -co PHOTOMETRIC=RGB overrides the color interpretation of the first 3 bands of the source datasets (#7064)
 * allow modifying color interpretation on existing file opened in update mode
 * Internal libtiff: resync with upstream HEAD (post 4.0.9)
 * Internal libgeotiff: resync with upstream HEAD: use ProjScaleAtCenterGeoKey for CT_Mercator if ProjScaleAtNatOriginGeoKey is not set (github #296)
 * fix compilation without BIGTIFF_SUPPORT (#6890)
 * fix reading subsampled JPEG-in-TIFF when block height = 8 (#6988)
 * when reading a COMPD_CS (and GTIFF_REPORT_COMPD_CS=YES), set the name from the GTCitationGeoKey (#7011)
 * on reading use GeogTOWGS84GeoKey to override the defaults TOWGS84 values coming from EPSG code (#7144)
 * when writing SRS, do not drop EXTENSION PROJ4 node if the projection is unknown (#7104)
 * make it accept to write SetGeoTransform([0,1,0,0,0,1]) as a ModelTransformationTag, and remove particular cases with the [0,1,0,0,0,1] geotransform (#1683)
 * warn when SetNoDataValue() is called on different bands with different values (#2083)
 * add a GTIFF_HONOUR_NEGATIVE_SCALEY=YES config option that can be set to honour negative ScaleY value in GeoPixelScale tag according to the GeoTIFF specification (#4977)
 * read/write Z dimension for ModelTiepointTag and ModelPixelScaleTag and translate it into/from band scale and offset, when there's a SRS with a vertical component (#7093)
 * fix reading PCSCitationGeoKey (#7199)
 * add support for reading and writing TIFF_RSID and GEO_METADATA GeoTIFF DGIWG tags
 * use consistently multiplication/division by 257 when converting between GDAL [0,255] range to TIFF [0,65535] range for color map values (#2213)
 * don't write <GDALMetadata> colorinterp when writing a file with a color table
 * copy georeferencing info to PAM if the profile is not GeoTIFF

GRIB driver:
 * add GRIB2 write support
 * update to degrib 2.14 and g2clib 1.6.0
 * add support for GRIB2 template 4.32 (github #249)
 * add support for GRIB2 template '4.32 Simulate (synthetic) Satellite Product'
 * add support for GRIB2 template 4.40 (Analysis or forecast at a horizontal level or in a horizontal layer at a point in time for atmospheric chemical constituents)
 * update table 4.2-0-7 current (github #274)
 * add support for GRIB1 products with non-zero NV (number of vertical coordinate parameters) field in GDS (NV is just ignored) (#7104)
 * add support for Rotated pole LatLong projections (#7104)
 * adjust the longitude range to be close to [-180,180] when possible for products whose left origin is close to 180deg. Can be controlled with the GRIB_ADJUST_LONGITUDE_RANGE=YES/NO config option, that defaults to YES  (#7103)
 * advertise .grb2 and .grib2 extensions in metadata
 * expose product discipline of GRIB2 products in GRIB_DISCIPLINE (#5108)
 * add a GRIB_PDS_TEMPLATE_ASSEMBLED_VALUES metadata items which expose a slightly higher view of GRIB_PDS_TEMPLATE_NUMBERS
 * speed-up GetNoData() implementation of GRIB2 files by avoiding decompressing the data
 * GRIB2: add support for Transverse Mercator, Albers Equal Area, Lambert Azimuthal projections, fixes in Mercator and Polar Stereographic support, adjustment for GS80 and WGS84 datums
 * correctly read Mercator as Mercator_2SP when stdparallel1 != 0
 * GRIB2: add support for Grid point data - IEEE Floating Point Data template 5.4 encoding
 * GRIB2: report Section 1 / Identification section as GRIB_IDS metadata item
 * fix decimal_scale_factor != 0 handling with nbits = 0 for simple packing and JP2K compression/decompression, and PNG compression.
 * GRIB2: don't error out on a unhandled template in Section 4
 * remove TDLPack support

GSAG driver:
 * fix reading issue that could cause a spurious 0 value to be read and shift all following values (#6992)

GTX driver:
 * do not emit error when opening with GDAL_PAM_ENABLED=NO (#6996)

GRC driver:
 * Fix handling of alpha values in GRC color table (#6905)
 * Handle case of 0-len GRC class names (#6907)

HDF5 driver:
 * Added CInt16, CInt32, CFloat32, CFloat64 support (github #359)
 * HDF5 driver as a plugin: register the BAG driver as well (#5802)

HF2 driver:
 * creation: copy source information (including nodata) into PAM if needed (#6885)
 *  fix reading tiles that are 1-pixel wide (2.1 regression, #6949)

HFA driver:
 * add GDAL_HFA_OVR_BLOCKSIZE configuration option to control HFA overviews block size (github #292)

HTTP driver:
 * do not open the underlying dataset with GDAL_OF_SHARED, to avoid later assertion

ISIS3 driver:
 * make sure that -co USE_SRC_HISTORY=NO -co ADD_GDAL_HISTORY=NO results in remove of History section (#6968)
 * fix logic to initialize underlying GeoTIFF file in IWriteBlock(), and implement Fill() (#7040)

JP2ECW driver:
 * add NBITS creation option, and automatically select codestream output for .j2k extension

JP2KAK driver:
 * add support fr Kakadu 7.A (#7048)
 * fix lossless compression with NBITS != 8 for Byte and NBITS != 16 for UInt16
 * use tile dimensions as block size up to 2048x2048 (#6941)
 * make write side honour .j2k extension to generate only codestream

JP2OpenJPEG driver:
 * add support for OpenJPEG 2.2 (#7002) and 2.3 (#7074). Drop support for openjpeg 2.0
 * fix performance issues with small images with very small tile size, such as some Sentinel2 quicklooks (#7012)
 * emit warning if GMLJP2v2 explicitly requested but georeferencing implemented
 * allow YCC for non-Byte datasets; and allow 4-band MCT with openjpeg >= 2.2

JPEG driver:
 * Add compatibility with libjpeg-turbo 1.5.2 that honours max_memory_to_use
 * add capability to write EXIF and GPS tags in a EXIF segment
 * Internal libjpeg: provide implementation of tmpfile() that works better on Windows (#1795)
 * avoid mis-identification of some SRTMHGT files as JPEG

JPEG2000 driver:
 * add NBITS creation option, and automatically select codestream output for .j2k extension

KEA driver:
 * add some additional metadata items (STATISTICS_HISTOBINVALUES and STATISTICS_HISTONUMBINS) (#6892)

L1B driver:
 * support reading NOAA18 datasets (#7115)

MBTiles driver:
 * add read/write support for Mapbox vector tiles
 * support opening and creating datasets with tiles whose dimension is not 256 (#7233)
 * default to opening as RGBA (#6119)

MEM driver:
 * Avoid Create(foo.tif) / CreateCopy(foo.tif) on the MEM or Memory drivers to delete a 'real' foo.tif file
 * add RAT support

MRF driver:
 * Add Zen chunk support
 * Open option to ignore decompression errors and missing data files
 * delay data file creation when NOCOPY is set
 * Identify MRF metadata passed as filename
 * Initialize PAM better on CopyCreate, enable external mask.
 * Fix for coordinates when opening single overview level
 * Use input mask if available to filter input data when creating JPEG compressed files.
 * Add open option to select Z-slice, also Z-slice selection in the metadata file

MrSID driver:
 * add support for LTI_COLORSPACE_GRAYSCALEA and LTI_COLORSPACE_GRAYSCALEA_PM color
 spaces

NetCDF driver:
 * avoid vector vs raster variable confusion that prevents reading Sentinel3 datasets, and avoid invalid geolocation array to be reported (#6974)
 * fix raster read as nodata with Byte datatype, (valid_range={0,255} or _Unsigned = True) and negative _FillValue (#7069)
 * be more tolerant on the formatting of standard parallel (space separated instead of {x,y,...} syntax), and accept up to 2/1000 error on spacing to consider a regular grid, to be able to read files provided by the national weather institute of Netherlands (KNMI) (#7086)
 * on creation, attach grid_mapping attribute to all bands
 * netCDF: support UTF-8 filenames on Windows (#7065)
 * add support for reading files in rotated pole projection (#4285)
 * behave correctly when an extra dimension of a variable has a corresponding 1D variable of different names (#7165)
 * netCDF: fix bad interaction of SetNoDataValue() and SetGeoTransform()/SetProjection() for NC4/NC4C modes (#7245)

NITF driver:
 * add support for NITF CCINFA TRE (github #232)
 * data/nitf_spec.xml: fix location of PIAPRD TRE (github #234)
 * make sure that BLOCKA_ or TRE=BLOCKA= creation option override source NITF metadata; add a USE_SRC_NITF_METADATA=YES/NO creation option; make sure that gdal_translate doesn't preserve source BLOCKA when georeferencing is modified
 * fix swapped lines and samples in IMRFCA (github #289)
 * allow to read single-block JPEG2000 compressed images with one dimension > 8192 pixels (fixes #407)

PCIDSK driver:
 * sort overviews (#7100)

PCRaster driver:
 * fix fseek/ftell for large files on Windows (#322)

PDF driver:
 * add support for Poppler 0.58 (#7033)
 * round to upper integer when computing a DPI such that page size remains within limits accepted by Acrobat (#7083)
 * do not emit 'Cannot find GPTS object' on VP.Measure objects whose Subtype != GEO

PDS driver:
 * map STEREOGRAPHIC with fabs(lat)=90 to Polar_Stereographic (#6893)

PLMosaic driver:
 * update to use the new Basemaps and Mosaics API
 * take into account BBOX for XYZ tiles

PNM driver:
 * add .pgm and .ppm extensions in metadata

RMF driver:
 * add RMF native overviews support (github #266)
 * fix raster garbage values while reading sparse RMF files (github #267)
 * add native overview build (github #275)
 * fix NoData value update (github #312)
 * fix elevation units write and update (github #314)

RRASTER driver:
 * add support for reading creator and created metadata items, band names and color table / RAT
 * add update and create/createcopy support

SAGA driver:
 * add support for .sg-grd-z files (github #228)

Sentinel2 driver:
 * make sure that the True Color Image subdatset really uses the R,G,B bands and not the R,R,R (#7251)
 * add support for direct opening of .zip files of new safe_compact L1C products (#7085)

SNODAS driver:
 * accept header lines up to 1024 characters (github #506)

SRTMHGT driver:
 * set appropriate description when opening a .hgt.zip file
 * recognizes the .hgt.gz extension (#7016)
 * add support for reading .SRTMSWBD.raw.zip files (GRASS #3246)

USGSDEM driver:
 * properly handle southern hemisphere UTM projections (#344)

VICAR driver:
 * optimize nodata handling, and map STEREOGRAPHIC with fabs(lat)=90 to Polar_Stereographic (#6893)

VRT driver:
 * add option for separable kernel (github #216)
 * warn if band attribute of VRTRasterBand is not the one expected
 * implement FlushCache()
 * fix use of VRTs that point to the same source in multi-threaded scenarios (#6939)
 * Warped VRT: correctly take into account cutline for implicit overviews; also avoid serializing a duplicate CUTLINE warping options in warped .vrt (#6954)
 * Warped VRT: fix implicit overview when output geotransform is not the same as the transformer dst geotransform (#6972)
 * fix IGetDataCoverageStatus() in the case of non-simple sources, which unbreaks gdalenhance -equalize (#6987)
 * re-apply shared='0' on sources if existing in original VRT when rewriting it due to invalidation
 * avoid error being emitted when opening a VRTRawRasterBand in a .zip files (#7056)
 * implement VRTDataset::AdviseRead() (in the particular case of a single source) (#7082)
 * allow to incorporate a warped VRT as CDATA in the SourceFileName field of a regular VRT
 * VRTDerivedRasterBand: fix detection of Python runtime already loaded when more than 100 modules are linked. Fixes QGIS3 use case (#7213)
 * add RAT support
 * for consistency, make sure that VRT intermediate datatype demotion is done, e.g that a VRT band of type Byte, with a source of type Float32, requested as Float32 buffer involves Float32 -> Byte -> Float32 conversions
 * VRTDataset::AddBand(): honour 'PixelFunctionLanguage' option (github #501)

WCS driver:
 * add support for WCS 2.0
 * add caching
 * add various open options

WMS driver:
 * ArcGIS miniserver: use latestWkid and wkt metadata (#6112)
 * avoid AdviseRead() to download too many tiles at once (#7082)
 * recognize /ImageServer?f=json ESRI endpoints
 * Add support for maximum size and expiration time and unique per dataset name for WMS cache

XYZ driver:
 * fix 2.2.0 regression where the driver hangs on some dataset with missing samples (#6934)
 * support non numeric characters when there is an header line (#7261)

## OGR 2.3.0 - Overview of Changes

Core:
 * OGRLayer::FilterGeom(): make sure a feature with a null or empty geometry never matches a spatial filter (#7123)
 * OGRSimpleCurve::get_LinearArea(): return 0 on a non-closed linestring (#6894)
 * OGRFeature::SetField: Improve the setting for OFTInteger fields from a double.
 * OGR_G_TransformTo(): emit error message when source geometry has no SRS
 * OGR API SPY: various fixes
 * OGRCurve::get_isClosed(): do not take into account M component (#7017)
 * OGRLineString::setPoint() and addPoint() with a OGRPoint* argument. properly takes into account ZM, Z and M dimensions (#7017)
 * OGRMultiSurface: refuse to add a triangle to it. (github #357)
 * OGRPolyhedralSurface: importFromWkt(): remove support for invalid constructs, that contrary to comments are not supported by PostGIS
 * Fix OGR[Curve]Polygon::Intersects(OGRPoint*) to return true when point is on polygon boundary (#7091)
 * Emit an explicit error message for OGRGeometry::IsValid(), IsSimple(), IsRing() when GEOS is not available
 * Make OGRGeometry::assignSpatialReference() a virtual method, and make implementation recursively assign the SR to child geometries (#7126)
 * Expose OGRGeometry::swapXY() as OGR_G_SwapXY() in C API (#7025)
 * importFromWkt(): fix import of GEOMETRYCOLLECTION ending with POINT EMPTY or LINESTRING EMPTY (#7128, 2.1 regression)
 * OGRSQL: avoid silent cast so that int64->int overflow is better signaled
 * OGRSQL: accept using the real FID column name (in addition to the special 'FID' alias) (#7050)
 * Add GDAL_DCAP_NONSPATIAL capability to ODS, REC, XLS and XLSX drivers
 * Add GDAL_DCAP_FEATURE_STYLES capability to CAD, DGN, DWG, DXF, EDIGEO, JML, KML, lIBLML, MITAB, OpenAIR and VRT drivers
 * Add DMD_CREATIONFIELDDATASUBTYPES metadata type (github #278)
 * OGRParseDate(): only accept seconds up to 60 included for leap seconds (#6525)
 * Fix OGRPolygon::IsPointOnSurface() broken with polygons with holes
 * OGRFormatDouble(): add a OGR_WKT_ROUND config option that can be set to FALSE to disable the heuristics that remove trailing 00000x / 99999x patterns (#7188)
 * Add C++ iterators for feature in layer and field values in features.
 * Add C++ iterators for "sub-parts" of geometry classes
 * Const-correctness fixes in signatures of methods of OGRFeatureDefn, OGRFeature, OGRFieldDefn and OGRGeomFieldDefn classes.
 * Fix constness of OGR_G_CreateFromWkb(), OGR_G_CreateFromFgf(), OGR_G_ImportFromWkb() and related C++ methods
 * Fix const correctness of OGRGeometry::importFromWkt() and OGRGeometryFactory::createFromWkt(), and add compatibility wrappers

OGRSpatialReference:
 * Update to EPSG v9.2 (#7125)
 * Update data/esri_extra.wkt and add data/esri_epsg.wkt, taken from https://github.com/Esri/projection-engine-db-doc (Apache v2 license) (#2163)
 * Add support for PROJ.5 new API (requires proj 5.0.1 or later)
 * Add a OSRFindMatches() function to look for equivalent SRS in the EPSG database, map it to SWIG Python. Enhance gdalsrsinfo to use it
 * Add OGRSpatialReference::convertToOtherProjection() (bound to C as OSRConvertToOtherProjection() and to SWIG) to transform between different equivalent projections (currently Merc_1SP <--> Merc_2SP and LCC_1SP <--> LCC_2SP) (#3292)
 * Fix OGRSpatialReference::IsSame() to return FALSE when comparing EPSG:3857 (web_mercator) and EPSG:3395 (WGS84 Mercator) (#7029)
 * importFromProj4(): implement import of Hotine Oblique Mercator Two Points Natural Origin, and fix OGRSpatialReference::Validate() for that formulation (#7042)
 * morphFromESRI(): fix remapping from DATUM = D_S_JTSK + PRIMEM = Ferro to OGC DATUM System_Jednotne_Trigonometricke_Site_Katastralni_Ferro
 * Add alternate simpler signature for OGRGeometryFactory::createFromWkt()
 * Add importFromWkt(const char**)

Utilities:
 * ogrmerge.py: fix '-single -o out.shp in.shp' case (#6888)
 * ogrmerge.py: allow using wildchar '*' in source names and use glob.glob to expand it
 * ogrmerge.py: correctly guess vrt format
 * ogrmerge.py: error when outputting to a existing vrt if -overwrite_ds is not specified
 * ogrmerge.py: use ogr.wkbUnknown to fix -nlt GEOMETRY
 * ogr2ogr: fix crash when using -f PDF -a_srs (#6920)
 * ogr2ogr: make -f GMT work again (#6993)
 * ogr2ogr: make it fail if GetNextFeature() returned NULL with an error
 * ogr2ogr: honour -select when using -addfiels
 * ogr2ogr: preserve source geometry field name for output drivers that support GEOMETRY_NAME layer creation option (such as GPKG)
 * ogr2ogr: add support for circularstring, compoundcurve, curvepolygon, multicurve and multisurface in SetZ()
 * GDALVectorTranslate(): fix converting from Memory to Memory datasources (#7217)
 * gnmmanage: fix crash on shape files with linestring and multilinestring geometries mixing
 * ogrtindex: fix crash when using -f SQLITE -t_srs XXXX (#7053)
 * ogrinfo: always open in read-only mode, unless -sql without -ro is specified
 * ogrinfo/ogr2ogr with -sql @filename: remove lines starting with '--' (github #459)

Multi driver changes:
 * port/tag ILI, NAS, DGN, NTF, SDTS, OGR_GMT, Geoconcept, AVCE00, AVCBin to VirtualIO API

Amigocloud driver:
 * Fixed data field types (github #246)
 * Output list of datasets if dataset id is not provided.
 * Implemented waiting for job to complete on the server. This will improve write to AmigoCloud reliability.
 * Add HTTP user agent (github #263)
 * add OVERWRITE open option (github #268)

AVCE00 driver:
 * Make sure AVCE00 opens .e00 files and not AVCBIN one

AVCBin driver:
 * fix 2.1 regression regarding attributes fetching (#6950)

Carto driver:
 * fix append mode by retrieving the sequence name for the primary key (#7203)
 * fix insertion with ogr2ogr -preserve_fid (#7216)
 * fix missing features when iterating over a SQL result layer with pagination (#6880)

CSV driver:
 * add a STRING_QUOTING=IF_NEEDED/IF_AMBIGUOUS/ALWAYS layer creation option, that defaults to IF_AMBIGUOUS (quote strings that look like numbers)
 * temporarily disable spatial and attribute filter when rewriting CSV file (#7123)
 * fix autodetection of types when a Real value is followed by a Integer64 one (#343)

DGNv8 driver:
 * add support for building against static library (#7155)

DXF driver:
 * Support LEADER and MULTILEADER entities (#7111, #7169)
 * Support HELIX and TRACE entities
 * Support spline hatch boundaries
 * Support MLINE entity
 * Support rows/columns within INSERT
 * Add support for block attributes (ATTRIB entities) (#7139)
 * Support text styles (#7151)
 * Support cylinders (CIRCLE entities with thickness)
 * set dx and dy to DXF LABEL style string on TEXT and MTEXT objects (github #225)
 * fix reading of hatches with boundaries that contain elliptical arcs (#6971)
 * fix reading files where INSERT is the last entity and DXF_MERGE_BLOCK_GEOMETRIES is false (#7006)
 * fix ordering of vertices in a SOLID entity (#7089)
 * only apply certain escaping rules to the text of the MTEXT object (#7047)
 * Handle all known MTEXT escapes; allow user to turn off text unescaping (fixes #7122)
 * do not apply the OCS transformation for MTEXT (#7049)
 * do not output dx and dy in MTEXT style strings (#7105)
 * apply DXF codepage encoding while decoding ExtendedEntity field (#5626)
 * allow user to set the tolerance used when joining parts of a hatch boundary with a DXF_HATCH_TOLERANCE config option, and default to 1e-7 of the extent of the geometry (#7005)
 * refactor out block insertion to its own function, and transform insertion point of an INSERT entity into OCS (#7077)
 * on write side, force 'defpoints' layer to be non-displayed (#7078)
 * entities on layer 0 within a block should inherit the style of the layer the INSERT is on. Do that for entities with a PEN style string for now. (#7099)
 * OCS fixes for HATCH and INSERT entities (#7098)
 * defer inlining of blocks until actually required (#7106)
 * Read DIMSTYLEs and block record handles
 * Handle linetype scales (#7129)
 * Improved support for DIMENSION entities (#7120)
 * handle ByBlock colors (#7130)
 * Fix parsing of complex linetypes (#7134)
 * Correct color for ByBlock text features; respect hidden objects (#5592, #7099, #7121)
 * Make ACAdjustText also transform text width and offset (#7151)
 * Honor block base points (#5592)
 * Use BRUSH style tool instead of PEN when reading polygonal SOLIDs and leader arrowheads; add PEN style string to 3DFACE
 * Specify ground units for TEXT dx and dy style values; interpret dx and dy in the writer
 * Fix bug in text style property retrieval; handle text color in DIMENSION fallback renderer
 * Write text styles and HATCH elevations; fix output of invalid extent values (#7183)
 * Allow access to unused entity group codes with RawCodeValues field (github #226)
 * Reduce the size of header.dxf and trailer.dxf (tested with AutoCAD, QCAD Teigha, QCAD dxflib, FME)
 * Make the writer aware of linetype scaling
 * Don't force elevation to zero in smooth polylines (#7200)
 * Always output all edges in polyface mesh, and add styling (#7219)
 * Remove incorrect OCS transformation from MLINE entities
 * Introduce 3D extensible mode to allow translation of 3D entities

ESRIJson driver:
 * "New": extracted from code previously in GeoJSON driver
 * avoid endless looping on servers that don't support resultOffset (#6895)
 * use 'latestWkid' in priority over 'wkid' when reading 'spatialReference' (github #218)
 * recognize documents that lack geometry fields (#7071)
 * do not attempt paging requests on a non-HTTP resource
 * recognize documents starting with a very long fieldAliases list (#7107)

FileGDB driver:
 * remove erroneous ODsCCreateGeomFieldAfterCreateLayer capability declaration (github #247)
 * add LAYER_ALIAS layer creation option (#7109)
 * FileGDB / OpenFileGDB: attempt to recover EPSG code using FindMatches()
 * FileGDB / OpenFileGDB: linearize Bezier curves with a more reasonable number of points

GeoJSON driver:
 * Add support for json-c v0.13 (github #277)
 * support loading (true) GeoJSON files of arbitrary size (#6540)
 * in update mode, support appending features, in some cases, without ingesting features of existing file into memory
 * writer: accept writing ZM or M geometry by dropping the M component (#6935)
 * fix 2.2 regression regarding lack of setting the FeatureCollection CRS on feature geometries (fixes https://github.com/r-spatial/sf/issues/449#issuecomment-319369945)
 * make sure that -lco WRITE_NAME=NO works even if the native data has a 'name' attribute
 * ignore 'properties' member of features whose value is null or empty array
 * fix writing of id with RFC7946=YES (#7194)
 * add ID_FIELD and ID_TYPE layer creation options

GeoPackage driver:
 * default to version 1.2 (#7244)
 * speed-up loading of databases with hundreds of layers
 * make driver robust to difference of cases between table_name in gpkg_contents/gpkg_geometry_columns and name in sqlite_master (#6916)
 * fix feature count after SetFeature() with sqlite < 3.7.8 (#6953)
 * do not error out if extent in gpkg_contents is present but invalid (#6976)
 * avoid GetFeature() to hold a statement, that can cause database locking issues (https://issues.qgis.org/issues/17034)
 * emit warning when feature geometries are not assignable to layer geometry type
 * remove UNIQUE keyword for PRIMARY KEY of gpkg_metadata (doesn't hurt, but just to align with the examples of DDL in the GeoPackage spec)
 * fix logic to detect FID on SQL result layer when several FID fields are selected (#7026)
 * fix handling of spatial views
 * do not try to update extent on gpkg_contents after GetExtent() on a empty layer of a datasource opened in read-only mode
 * when inserting a SRS with a non-EPSG code, then avoid using srs_id that could be later assigned to a EPSG code
 * when deleting a layer, delete from gpkg_metadata metadata records that are only referenced by the table we are about to drop
 * fix incorrect rtree_<t>_<c>_update3 trigger statement (https://github.com/opengeospatial/geopackage/issues/414)
 * fix definition of extensions
 * only create metadata table if needed, and register it as extension
 * declare gpkg_crs_wkt extension when using GPKG_ADD_DEFINITION_12_063=YES config option, and do not emit warning when finding it declared

GeoRSS driver:
 * fix detection of field type (#7108)

GML driver:
 * CreateGeometryFromGML(): accept gml:Arc with odd number of points > 3, even if they are invalid
 * decode gml:Solid as PolyhedralSurface (#6978)
 * JPGIS FGD v4: fix logic so that coordinate order reported is long/lat (github #241)
 * a GML_FEATURE_COLLECTION=YES/NO dataset creation option
 * fix FORCE_SRS_DETECTION=YES effect on feature count and SRS reporting on gml files with .gfs (#7046)
 * do not try to open some kml files (#7061)
 * do not report gml:name / gml:description of features as layer metadata

GMLAS driver:
 * get the srsName when set on the srsName of the gml:pos of a gml:Point (#6962)
 * CityGML related fixes: better take into account .xsd whose namespace prefix is not in the document, fix discovery of substitution elements, handle gYear data type (#6975)
 * properly interpret SolidPropertyType (#6978)
 * handle GML geometries in elements that are a substitutionGroup of an element (#6990)
 * make sure to try namespaces that are indirectly imported (helps in the case of parsing the result of a WFS GetFeature request)
 * take into account gYearMonth XML data type (https://github.com/BRGM/gml_application_schema_toolbox/issues/46)
 * implement resolution of internal xlink:href='#....' references (https://github.com/BRGM/gml_application_schema_toolbox/issues/31)
 * Recognize gml:Envelope as a Polygon geometry (https://github.com/BRGM/gml_application_schema_toolbox/issues/56#issuecomment-381967784)

GMT:
 * fix creation of several layers

IDB driver:
 * optimize spatial query using spatial index and st_intersects function (#6984)
 * close connection at dataset closing (#7024)

ILI driver:
 * declare OLCCreateField and OLCSequentialWrite capabilities

Ingres driver:

JML driver:
 * add support for reading and write SRS from gml:Box element (#7215)

KML driver:
 * KML / LIBKML: read documents with an explicit kml prefix (#6981)
 * KML/LIBKML: make sure layer names are unique (QGIS github#5415)

LIBKML driver:
 * fix reading placemarks in toplevel container, when it has subfolders (#7221)
 * improve open performance on huge number of layers

MDB driver:
 * fix multi-thread support (https://issues.qgis.org/issues/16039)

MITAB driver:
 * support for additional datums (#5676)
 * add support for Reseau_National_Belge_1972 / EPSG:31370 on writing (#6903)
 * add support for SWEREF99 datum (#7256)
 * fix EPSG datum code for NTF Paris
 * support units as code instead of string when reading a coordsys string from a .mif (#3590)
 * add read/write support between MapInfo text encodings and UTF-8. Add ENCODING layer/dataset creation option (github #227)
 * do not emit error if the .ind file is missing, just a debug message (#7094)
 * accept creating files with LCC_1SP SRS (#3292)
 * support 'CREATE INDEX ON layer_name USING field_name' (if called after field creation and before first feature insertion)
 * avoid reporting invalid extent on empty .mif
 * report aspatial .tab as wkbNone geometry type
 * export BRUSH(fc:#.....) with mapinfo brush pattern 2 (solid fill)

mongoDB driver:
 * compilation fix on Windows

MSSQLSpatial driver:
 * Fix issues on x64 (#6930)
 * Implement EXTRACT_SCHEMA_FROM_LAYER_NAME layer creation option.
 * properly format DELETE statement with schema (#7039)

MySQL:
 * fix compilation issue with MariaDB 10.1.23 (#6899) and MariaDB 10.2 (#7079)
 * make sure geometry column name is properly set (GDAL 2.0 regression, github #397)

NAS driver:
 * [NAS/GML] try reversing non contiguous curves
 * Allow usage of a GFS template file for type assignment and mapping of element paths to attributes and remove arbitrary hacks that used to avoid attribute name conflicts:
 * New option: NAS_GFS_TEMPLATE: specify gfs template
 * New option: NAS_NO_RELATION_LAYER: skip unused alkis_beziehungen (also makes progress available)
 * Multiple geometries per layer
 * NAS_INDICATOR updated to "NAS-Operationen;AAA-Fachschema;aaa.xsd;aaa-suite"
 * use single geometry column name from input (like GMLAS)
 * accept unqualified attribute name in wfs:Update

NTF driver:
 * fix regression introduced by fix for https://bugs.chromium.org/p/oss-fuzz/issues/detail?id=2166 causing some valid records to be skipped (#7204)

NULL driver:
 * Generalize OGR NULL development driver as a GDAL raster+vector one; enable to compile it --with-null

OCI driver:
 * initialize in multi-threaded compatible mode (https://issues.qgis.org/issues/17311)
 * support for extended max_string_size (#7168)

ODBC driver:
 * deal with table names that require double quoting (#7242)

ODS driver:
 * add read/write support for Boolean sub datatype

OSM driver:
 * performance improvements
 * increase string buffer for osm XML files (#6964)
 * OGR_SQL/OSM: avoid OSM driver warning about attribute filter not being taken into account when issuing a 'select * from X limit Y' request on a OSM datasource

OpenFileGDB driver:
 * properly read GeneralPolygon with M component whose all values are set to NaN (#7017)

PDF driver:
 * make OGR_PDF_READ_NON_STRUCTURED=YES configuration option work with documents without layers (#6359)
 * Improvements to text and dash pattern support in writer (#7185)

PDS driver:
 * fix reading 8-byte IEEE_REAL fields (github #570)

PG driver:
 * do not be confused by a 'geometry' table in a non-PostGIS enabled database (#6896)
 * PG/PGDump: make sure serial sequence is updated at layer closing/end-of-copy if we inserted features with fixed ids (#7032)

PLScenes driver:
 * remove support for V0 and V1 API. Only Data V1 is kept (#6933)
 * add SkySatScene

SEGY driver:
 *report more attributes from Segy Standard Trace Header (github #315)

Shapefile driver:
 * Fix GetFeatureCount() to properly take into account spatial filter when attribute filter also in effect (#7123)
 * use VSIStatL() in Create() to properly work with /vsimem/ directories (#6991)
 * fix regression affecting GDAL 2.1.3 or later, 2.2.0 and 2.2.1, when editing the last shape of a file and growing it, and then appending a new shape, which resulted in corruption of both shapes (#7031)
 * hide shapelib symbols on Unix
 * improvements auto-identification of EPSG codes from input SRS, using OSRFindMatches()
 * Improve guidance on use of SHAPE_RESTORE_SHX in SHPOpenLL() (#7246)

SQLite/Spatialite driver:
 * SQLite/GPKG: report SQLITE_HAS_COLUMN_METADATA=YES as driver metadata item when it is available (#7007)
 * escape integer primary key column name on table creation (#7007)
 * don't invalidate statistics when running a PRAGMA (https://issues.qgis.org/issues/17424)
 * SQLite dialect: avoid erroring out if source layer has a OGR_STYLE field
 * SQLite dialect: support SQLite 3.21, and LIKE, <>, IS NOT, IS NOT NULL, IS NULL and IS operators (fixes #7149)
 * SQLite/GPKG: avoid parsing of OGR_SQLITE_JOURNAL to stop on JOURNAL_MODE keyword

VFK driver:
 * fix collecting linestrings for sbp layer
 * collect geometries also for zvb layer
 * Improve VFK parser logic (github #365)
 * add option to suppress geometry (github #371)

VRT driver:
 * increase maximum size limit to 10 MB (instead of 1MB), and add config option to be able to force loading if above that

XLS driver:
 * workaround opening filenames with incompatible character set on Windows (https://issues.qgis.org/issues/9301)

XLSX driver:
 * avoid 'ogr2ogr -f XLSX out.xlsx in.shp -preserve_fid' to overwrite the first record. Note however that FID preservation itself doesn't really work with XLSX (#6994)
 * fix non working detection of Date/Time fields in some documents (#7073)
 * fix opening of documents with x: namespace in xl/workbook.xml (#7110)
 * fix misdetection of field type if first cell blank (github #291)
 * fix updating multi layer document where existing layers where dropped (#7225)
 * add read/write support for Boolean sub datatype
 * on writing, use %.16g formatting for floating point numbers (#7230)

## SWIG Language Bindings

All bindings:
 * Expose gdal.VSIFEofL()
 * Expose gdal.GRA_Max, GRA_Min, GRA_Med, GRA_Q1 and GRA_Q3 (#7153)
 * Expose Geometry.SwapXY()
 * Expose Geometry.RemoveGeometry()
 * Expose SpatialReference.SetMercator2SP()
 * Expose Band.AdviseRead() and Dataset.AdviseRead()

CSharp bindings:

Java bindings:
 * create single JNI shared library (gdalalljni.so/dll) (github #286)

Perl bindings:
 * Keep parent geometry alive if an object is created from a child geometry. Experimental methods Features and Geometries. Catch an error in storing a geometry into a feature. Prefer GeometryCount over GetGeometryCount and Geometry over GetGeometryRef.
 * Set INSTALLSITEMAN1DIR and INSTALLSITEMAN3DIR according to what is in GDALmake.opt if INSTALL_BASE is set. (#6142)
 * ignore no data cells in ClassCounts and Reclassify for real valued rasters.
 * support for number line (or decision tree) classifiers also for integer bands.
 * Set error handler separately in all modules (required by Strawberry Perl build).

Python bindings:
 * add scripts to python module (#342)
 * make sure that errors in Open() related functions, that do not prevent dataset opening in C/C++, do not either prevent it in Python (#7094)
 * avoid hang when calling gdal/ogr.UseExceptions()/DontUseExceptions() in wrong order (#6891)
 * accept callback = 0 since SWIG generates it as the default argument of BandRasterIONumPy(). (github #219)
 * fix 2.2.0 regression preventing use of callback function in Band.ComputeStatistics() (#6927)
 * fix reference count issue on gdal.VSIStatL() when it returns None, that can cause Python crashes if None refcount drops to zero
 * avoid potential cross-heap issue on Windows with numpy (https://trac.osgeo.org/osgeo4w/ticket/466)
 * fix potential alignment issues when reading double values with ReadRaster() or ReadBlock()
 * fix issue with PyInstaller (#7044)
 * add a addAlpha option to gdal.DEMProcessing()
 * fix for include files when building .cpp files (github #258)
 * make gdal.Transform() return None in case of an error (instead of an invalid Transformer object)
 * emit exceptions if VSI_RETVAL methods (such as gdal.Rename()) fail and gdal.UseExceptions() is enabled
 * add addFields and forceNullable options to gdal.VectorTranslate()
 * for command line utilities as functions, serialize number of floating-point arguments with higher precision
 * Fix python 3.x package installation in custom prefixes (#6671)
 * lots of PEP8-related fixes in scripts and autotest
 * add NULL checks after PyUnicode_AsUTF8String() in case the unicode cannot be translated to UTF-8 and workaround SWIG bug to avoid crashes with Python 3. (github #356)
 * make sure gdal.VectorTranslate() and gdal.Grid() open their source dataset with a vector driver (github #449)
 * make 'for field_val in feature' work (github #451)

# GDAL/OGR 2.2.0 Release Notes

## In a nutshell...

 * New GDAL/raster drivers:
   - DERIVED driver: read-support. Expose subdatasets in a a new metadata domain, called DERIVED_SUBDATASETS
   - JP2Lura driver: read/create support for JPEG-2000 images using Luratech JP2 Library
   - PRF: add read-only support for PHOTOMOD PRF file format driver (github #173)
   - RRASTER driver: read-support .grd/.gri files handled by the R 'raster' package (#6249)
 * New OGR/vector drivers:
    - CAD driver: read support for DWG R2000 files (GSoC 2016 project)
    - DGNv8 driver: read-write support for DGN 8.0 format (using Teigha ODA libraries)
    - GMLAS driver: read-write support. XML/GML driver driven by Application Schemas.
 * New utility script: ogrmerge.py to merge several vector datasets into a single one
 * New /vsigs/ and /vsigs_streaming/ virtual file systems to read Google Cloud Storage non-public files
 * Significantly improved drivers:
  - NWT_GRD: write support (#6533)
  - FileGDB/OpenFileGDB: add support to read curve geometries (#5890)
  - VRT derived band: add the capability to define pixel functions in Python
  - Add read support for RasterLite2 coverages in SQLite driver
  - GPKG: implement tiled gridded elevation data extension
  - ISIS3: add write support and improve read support
 * RFC 63: Add GDALRasterBand::GetDataCoverageStatus() and implement it in GTiff and VRT drivers
        https://trac.osgeo.org/gdal/wiki/rfc63_sparse_datasets_improvements
 * RFC 64: Triangle, Polyhedral surface and TIN
        https://trac.osgeo.org/gdal/wiki/rfc64_triangle_polyhedralsurface_tin
   ==> this RFC introduces potential backward incompatible behavior.
        Consult MIGRATION_GUIDE.txt
 * RFC 66: OGR random layer read/write capabilities
        https://trac.osgeo.org/gdal/wiki/rfc66_randomlayerreadwrite
 * RFC 67: add null field state for OGR features, in addition to unset fields
        https://trac.osgeo.org/gdal/wiki/rfc67_nullfieldvalues
   ==> this RFC introduces potential backward incompatible behavior.
        Consult MIGRATION_GUIDE.txt
 * Upgrade to EPSG database v9.0 (#6772)
 * Python bindings: Global Interpreter Lock (GIL) released before entering GDAL native code (for all, in GDAL module and a few ones in ogr like ogr.Open())
 * Continued major efforts on sanitization of code base
 * Remove bridge and vb6 bindings (#6640)
 * GNM built by default

## Installed files

 * Removed: data/s57attributes_aml.csv data/s57attributes_iw.csv data/s57objectclasses_aml.csv data/s57objectclasses_iw.csv
 * Added plscenesconf.json, gmlasconf.xsd

## Backward compatibility issues

See MIGRATION_GUIDE.TXT

## GDAL/OGR 2.2.0 - General Changes

Build(Unix):
 * improve detection of packaged libfyba (SOSI) --with-sosi, as in Ubuntu 16.04 (#6488)
 * Sort files in static library to make the build reproducible (#6520)
 * fix libqhull include path when it is /usr/local/include/libqhull (#6522)
 * FileGDB: compilation fix on Linux in C++11 mode
 * configure: make pdfium detection not fail if there are just warnings. And make configure fail if --with-pdfium was required but failed (#6653)
 * Make ./configure --with-xerces fail if not found
 * Don't install script documentation in INST_BIN (github #157)
 * configure: auto-detect webp without requiring explicit --with-webp
 * configure: use pkg-config for HDF5 detection so that works out of the box on recent Ubuntu
 * auto-detect JDK 8 on Ubuntu
 * MDB: allow libjvm.so to be dlopen'd with --with-jvm-lib=dlopen (Unix only, github #177)
 * configure: delete temporary directories on the mac
 * configure: make sure --with-macosx-framework is correctly defined
 * configure: error out if --with-ld-shared is specified (#6769)
 * configure: remove bashism.
 * configure: fix --without-mrf (#6811)
 * configure: take into account CXXFLAGS and LDFLAGS in a few more cases (cryptopp, podofo, libdap)
 * Vagrant: all lxc and Hyper-V provider support; use vagrant-cachier for package caching
 * configure: update DWG support to work with Teigha libraries
 * Internal libgeotiff: hide symbols in --with-hide-internal-symbols mode
 * Shape: do not export Shapelib symbols for builds --with-hide-internal-symbols (#6860)

Build(Windows):
 * Try to avoid confusion between libqhull io.h and mingw own io.h (#6590)
 * update script to generate most recent Visual C++ project files (#6635)
 * fix broken and missing dependencies in makefile.vc
 * add a way to use an external zlib (github #171)
 * Rename makegdal_gen.bat to generate_vcxproj.bat
 * generate_vcxproj.bat: Set correct value of PlatformToolset property based on specified Visual C++ version. Add NMAKE command line parameters: MSVC_VER based on specified Visual C++ version, DEBUG=1 WITH_PDB=1 to Debug build configuration.
 * generate_vcxproj.bat: generate project for autotest/cpp (Ticket #6815)
* Add WIN64=1 to NMAKE command line options.
 * Add HDF4_INCLUDE option (#6805)
 * Add MSVC compiler option /MP to build with parallel processes.
 * Add ZLIB_LIB missing from EXTERNAL_LIBS

Build(all):
 * make Xerces 3.1 the minimal version
 * drop support for PostgreSQL client library older than 7.4, or non security maintained releases older than 8.1.4, 8.0.8, 7.4.13, 7.3.15

## GDAL 2.2.0 - Overview of Changes

Port:
 * Export VSICreateCachedFile() as CPL_DLL so as to enable building JP2KAK as a plugin
 * Added possibility to find GDAL_DATA path using INST_DATA definition without execution GDALAllRegister if GDAL_DATA placed in version named directory on Linux (#6543)
 * Unix filesystem: make error message about failed open to report the filename (#6545)
 * File finder: Remove hardcoded find location (/usr/local/share/gdal) (#6543)
 * Win32 filesystem handler: make Truncate() turn on sparse files when doing file extension
 * Add VSIFGetRangeStatusL() and VSISupportsSparseFiles()
 * GDAL_NO_HARDCODED_FIND compilation option (#6543,#6531) to block file open calls (for sandboxed systems)
 * Add VSIMallocAligned(), VSIMallocAlignedAuto() and VSIFreeAligned() APIs
 * /vsizip / /vsitar: support alternate syntax /vsitar/{/path/to/the/archive}/path/inside/the/tar/file so as not to be dependent on file extension and enable chaining
 * Optimize opening of /vsitar/my.tar.gz/my_single_file
 * /vsizip/ : support creating non-ASCII filenames inside a ZIP (#6631)
 * VSI Unix file system: fix behavior in a+ mode that made MRF caching not work on Mac and other BSD systems
 * Fix deadlock at CPLWorkerThreadPool destruction (#6646)
 * Windows: honour GDAL_FILENAME_IS_UTF8 setting to call LoadLibraryW() (#6650)
 * CPLFormFilename(): always use / path separator for /vsimem, even on Windows
 * /vsimem/: add trick to limit the file size, so as to be able to test how drivers handle write errors
 * /vsimem/: fix potential crash when closing -different- handles pointing to the same file from different threads (#6683)
 * CPLHTTPFetch(): add MAX_FILE_SIZE option
 * CPLHTTPFetch(): add a CAINFO option to set the path to the CA bundle file. As a fallback also honour the CURL_CA_BUNDLE and SSL_CERT_FILE environment variables used by the curl binary, which makes this setting also available for /vsicurl/, /vsicurl_streaming/, /vsis3/ and /vsis3_streaming/ file systems (#6732)
 * CPLHTTPFetch(): don't disable peer certificate verification when doing https (#6734)
 * CPLHTTPFetch(): cleanly deal with multiple headers passed with HEADERS and separated with newlines
 * CPLHTTPFetch(): add a CONNECTTIMEOUT option
 * CPLHTTPFetch(): add a GDAL_HTTP_HEADER_FILE / HEADER_FILE option.
 * CPLHTTPSetOptions(): make redirection of POST requests to still be POST requests after redirection (#6849)
 * /vsicurl/: take CPL_VSIL_CURL_ALLOWED_EXTENSIONS into account even if GDAL_DISABLE_READDIR_ON_OPEN is defined (#6681)
 * /vsicurl/: get modification time if available from GET or HEAD results
 * /vsis3/: add a AWS_REQUEST_PAYER=requester configuration option (github #186)
 * CPLParseXMLString(): do not reset error state
 * Windows: fix GetFreeDiskSpace()
 * Fix GetDiskFreeSpace() on 32bit Linux to avoid 32bit overflow when free disk space is above 4 GB (#6750)
 * Fix CPLPrintUIntBig() to really print a unsigned value and not a signed one
 * Add CPL_HAS_GINT64, GINT64_MIN/MAX, GUINT64_MAX macros (#6747)
 * Add CPLGetConfigOptions(), CPLSetConfigOptions(), CPLGetThreadLocalConfigOptions() and CPLSetThreadLocalConfigOptions() to help improving compatibility between osgeo.gdal python bindings and rasterio (related to https://github.com/mapbox/rasterio/pull/969)
 * MiniXML serializer: fix potential buffer overflow.

Core:
 * Proxy dataset: add consistency checks in (the unlikely) case the proxy and underlying dataset/bands would not share compatible characteristics
 * GDALPamDataset::TryLoadXML(): do not reset error context when parsing .aux.xml
 * PAM/VRT: only take into account <Entry> elements when deserializing a <ColorTable>
 * GDALCopyWords(): add fast copy path when src data type == dst data type == Int16 or UInt16
 * GetVirtualMemAuto(): allow USE_DEFAULT_IMPLEMENTATION=NO to prevent the default implementation from being used
 * Nodata comparison: fix test when nodata is FLT_MIN or DBL_MIN (#6578)
 * GetHistogram() / ComputeRasterMinMax() / ComputeStatistics(): better deal with precision issues of nodata comparison on Float32 data type
 * Fast implementation of GDALRasterBand::ComputeStatistics() for GDT_Byte and GDT_UInt16 (including use of SSE2/AVX2)
 * Driver manage: If INST_DATA is not requested, do not check the GDAL_DATA variable.
 * Make sure that GDALSetCacheMax() initialize the raster block mutex (#6611)
 * External overview: fix incorrect overview building when generating from greater overview factors to lower ones, in compressed and single-band case (#6617)
 * Speed-up SSE2 implementation of GDALCopy4Words from float to byte/uint16/int16
 * Add SSE2 and SSSE3 implementations of GDALCopyWords from Byte with 2,3 or 4 byte stride to packed byte
 * GDALCopyWords(): SSE2-accelerated Byte->Int32 and Byte->Float32 packed conversions
 * Fix GDALRasterBand::IRasterIO() on a VRT dataset that has resampled sources, on requests such as nXSize == nBufXSize but nXSize != dfXSize
 * GDALRasterBand::IRasterIO(): add small epsilon to floating-point srcX and srcY to avoid some numeric precision issues when rounding.
 * Add GDALRasterBand::GetActualBlockSize() (#1233)
 * Fix potential deadlock in multithreaded writing scenarios (#6661)
 * Fix thread-unsafe behavior when using GetLockedBlock()/MarkDirty()/DropLock() lower level interfaces (#6665)
 * Fix multi-threading issues in read/write scenarios (#6684)
 * Resampled RasterIO(): so as to get consistent results, use band datatype as intermediate type if it is different from the buffer type
 * Add GDALIdentifyDriverEx() function (github #152)
 * GDALOpenInfo: add a papszAllowedDrivers member and fill it in GDALOpenEx()
 * GDALDefaultOverviews::BuildOverviews(): improve progress report
 * Average and mode overview/rasterio resampling: correct source pixel computation due to numerical precision issues when downsampling by an integral factor, and also in oversampling use cases (github #156)
 * Overview building: add experimental GDAL_OVR_PROPAGATE_NODATA config option that can be set to YES so that a nodata value in source samples will cause the target pixel to be zeroed. Only implemented for AVERAGE resampling right now
 * GDALValidateOptions(): fix check of min/max values
 * GMLJP2 v2: update to 2.0.1 corrigendum and add capability to set gml:RectifiedGrid/gmlcov:rangeType content. Set SRSNAME_FORMAT=OGC_URL by default when converting to GML. Add gml:boundedBy in gmljp2:GMLJP2RectifiedGridCoverage
 * GMLJP2 v2: ensure KML root node id unicity when converting annotations on the fly. When generating GML features, make sure that PREFIX and TARGET_NAMESPACE are unique when specifying several documents.

Algorithms:
 * RPC transformer: speed-up DEM extraction by requesting and caching a larger buffer, instead of doing many queries of just a few pixels that can be costly with VRT for example
 * GDALDeserializeRPCTransformer(): for consistency, use the same default value as in GDALCreateRPCTransformer() if <PixErrThreshold> is missing (so use 0.1 instead of 0.25 as before)
 * TPS solver: when Armadillo fails sometimes, fallback to old method
 * GDALCreateGenImgProjTransformer2(): add SRC_APPROX_ERROR_IN_SRS_UNIT, SRC_APPROX_ERROR_IN_PIXEL, DST_APPROX_ERROR_IN_SRS_UNIT, DST_APPROX_ERROR_IN_PIXEL, REPROJECTION_APPROX_ERROR_IN_SRC_SRS_UNIT and REPROJECTION_APPROX_ERROR_IN_DST_SRS_UNIT transformer options, so as to be able to have approximate sub-transformers
 * Fix GDAL_CG_Create() to call GDALContourGenerator::Init() (#6491)
 * GDALContourGenerate(): handle the case where the nodata value is NaN (#6519)
 * GDALGridCreate(): fix hang in multi-threaded case when pfnProgress is NULL or GDALDummyProgress (#6552)
 * GDAL contour: fix incorrect oriented contour lines in some rare cases (#6563)
 * Warper: multiple performance improvements for cubic interpolation and uint16 data type
 * Warper: add SRC_ALPHA_MAX and DST_ALPHA_MAX warp options to control the maximum value of the alpha channel. Set now to 65535 for UInt16 (and 32767 for Int16), or to 2^NBITS-1. 255 used for other cases as before
 * Warper: avoid undefined behavior when doing floating point to int conversion, that may trigger exception with some compilers (LLVM 8) (#6753)
 * OpenCL warper: update cubicConvolution to use same formula as CPU case (#6664)
 * OpenCL warper: fix compliance to the spec. Fix issues with NVidia opencl (#6624, #6669)
 * OpenCL warper: use GPU based over CPU based implementation when possible, use non-Intel OpenCL implementation when possible. Add BLACKLISTED_OPENCL_VENDOR and PREFERRED_OPENCL_VENDOR to customize choice of implementation

Utilities:
 * gdalinfo -json: fix order of points in wgs84Extent.coordinates (github #166)
 * gdalwarp: do not densify cutlines by default when CUTLINE_BLEND_DIST is used (#6507)
 * gdalwarp: when -to RPC_DEM is specified, make -et default to 0 as documented (#6608)
 * gdalwarp: improve detection of source alpha band and auto-setting of target alpha band. Automatically set PHOTOMETRIC=RGB on target GeoTIFF when input colors are RGB
 * gdalwarp: add a -nosrcalpha option to wrap the alpha band as a regular band and not as the alpha band
 * gdalwarp: avoid cutline densification when no transform at all is involved (related to #6648)
 * gdalwarp: fix failure with cutline on a layer of touching polygons (#6694)
 * gdalwarp: allow to set UNIFIED_SRC_NODATA=NO to override the default that set it to YES
 * gdalwarp: fix -to SRC_METHOD=NO_GEOTRANSFORM -to DST_METHOD=NO_GEOTRANSFORM mode (#6721)
 * gdalwarp: add support for shifting the values of input DEM when source and/or target SRS references a proj.4 vertical datum shift grid
 * gdalwarp: fix crash when -multi and -to RPC_DEM are used together (#6869)
 * gdal_translate: when using -projwin with default nearest neighbour resampling, align on integer source pixels (#6610)
 * gdal_translate & gdalwarp: lower the default value of GDAL_MAX_DATASET_POOL_SIZE to 100 on MacOSX (#6604)
 * gdal_translate: avoid useless directory scanning on GeoTIFF files
 * gdal_translate: make "-a_nodata inf -ot Float32" work without warning
 * gdal_translate: set nodata value on vrtsource on scale / unscale / expand cases (github #199)
 * GDALTranslate(): make it possible to create a anonymous target VRT from a (anonymous) memory source
 * gdaldem: speed-up computations for src type = Byte/Int16/UInt16 and particularly for hillshade
 * gdaldem hillshade: add a -multidirectional option
 * GDALDEMProcessing() API: fix -alt support (#6847)
 * gdal_polygonize.py: explicitly set output layer geometry type to be polygon (#6530)
 * gdal_polygonize.py: add support for -b mask[,band_number] option to polygonize a mask band
 * gdal_rasterize: make sure -3d, -burn and -a are exclusive
 * gdal_rasterize: fix segfaults when rasterizing into an ungeoreferenced raster, or when doing 'gdal_rasterize my.shp my.tif' with a non existing my.tif (#6738)
 * gdal_rasterize: fix crash when rasterizing empty polygon (#6844)
 * gdal_grid: add a smoothing parameter to invdistnn algorithm (github #196)
 * gdal_retile.py: add a -overlap switch
 * gdal2tiles.py: do not crash on empty tiles generation (#6057)
 * gdal2tiles.py: handle requested tile at too low zoom to get any data (#6795)
 * gdal2tiles: fix handling of UTF-8 filenames (#6794)
 * gdal2xyz: use %d formatting for Int32/UInt32 data types (#6644)
 * gdal_edit.py: add -scale and -offset switches (#6833)
 * gdaltindex: emit warning in -src_srs_format WKT when WKT is too large
 * gdalbuildvrt: add a -oo switch to specify dataset open options

Python samples:
 * add validate_cloud_optimized_geotiff.py
 * add validate_gpkg.py

Multi-driver:
 * Add GEOREF_SOURCES open option / GDAL_GEOREF_SOURCES config. option to all JPEG2000 drivers and GTiff to control which sources of georeferencing can be used and their respective priority

AIGRID driver:
 * fix 2.1.0 regression when reading statistics (.sta) file with only 3 values, and fix <2.1 behavior to read them in LSB order (#6633)

AAIGRID driver:
 * auto-detect Float64 when the nodata value is not representable in the Float32 range

ADRG driver:
 * handle north and south polar zones (ZNA 9 and 18) (#6783)

ASRP driver:
 * fix georeferencing of polar arc zone images (#6560)

BPG driver:
* declare GDALRegister_BPG as C exported for building as a plugin (#6693)

DIMAP driver:
 * DIMAP: for DIMAP 2, read RPC from RPC_xxxxx.XML file (#6539)
 * DIMAP/Pleiades metadata reader: take into tiling to properly shift RPC (#6293)
 * add support for tiled DIMAP 2 datasets (#6293)

DODS driver:
 * fix crash on URL that are not DODS servers (#6718)

DTED driver:
 * correctly create files at latitudes -80, -75, -70 and -50 (#6859)

ECW driver:
 * Add option ECW_ALWAYS_UPWARD=TRUE/FALSE  to work around issues with "Downward" oriented images (#6516).

ENVI driver:
 * on closing, pad image file with trailing nul bytes if needed (#6662)
 * add read/write support for rotated geotransform (#1778)

GeoRaster driver:
 * fix report of rotation (#6593)
 * support for JP2-F compression (#6861)
 * support direct loading of JPEG-F when blocking=no (#6861)
 * default blocking increased from 256x256 to 512x512 (#6861)

GPKG driver:
 * implement tiled gridded elevation data extension
 * add VERSION creation option
 * check if transaction COMMIT is successful (#6667)
 * fix crash on overview building on big overview factor (#6668)
 * fix crash when opening an empty raster with USE_TILE_EXTENT=YES
 * fix gpkg_zoom_other registration

GTiff driver:
 * support SPARSE_OK=YES in CreateCopy() mode (and in update mode with the SPARSE_OK=YES open option), by actively detecting blocks filled with 0/nodata about to be written
 * When writing missing blocks (i.e. non SPARSE case), use the nodata value when defined. Otherwise fallback to 0 as before.
 * in FillEmptyTiles() (i.e. in the TIFF non-sparse mode), avoid writing zeroes to file so as to speed up file creation when filesystem supports ... sparse files
 * add write support for half-precision floating point (Float32 with NBITS=16)
 * handle storing (and reading) band color interpretation in GDAL internal metadata when it doesn't match the capabilities of the TIFF format, such as B,G,R ordering (#6651)
 * Fix RasterIO() reported when downsampling a RGBA JPEG compressed TIFF file (#6943)
 * Switch search order in GTIFGetOGISDefn() - Look for gdal_datum.csv before datum.csv (#6531)
 * optimize IWriteBlock() to avoid reloading tile/strip from disk in multiband contig/pixel-interleave layouts when all blocks are dirty
 * fix race between empty block filling logic and background compression threads when using Create() interface and NUM_THREADS creation option (#6582)
 * use VSIFTruncateL() to do file extension
 * optimize reading and writing of 1-bit rasters
 * fix detection of blocks larger than 2GB on opening on 32-bit builds
 * fix saving and loading band description (#6592)
 * avoid reading external metadata file that could be related to the target filename when using Create() or CreateCopy() (#6594)
 * do not generate erroneous ExtraSamples tag when translating from a RGB UInt16, without explicit PHOTOMETRIC=RGB (#6647)
 * do not set a PCSCitationGeoKey = 'LUnits = ...' as the PROJCS citation on reading
 * fix creating an image with the Create() interface with BLOCKYSIZE > image height (#6743)
 * fix so that GDAL_DISABLE_READDIR_ON_OPEN = NO / EMPTY_DIR is properly honoured and doesn't cause a useless directory listing
 * make setting GCPs when geotransform is already set work (with warning about unsetting the geotransform), and vice-versa) (#6751)
 * correctly detect error return of TIFFReadRGBATile() and TIFFReadRGBAStrip()
 * in the YCBCR RGBA interface case, only expose RGB bands, as the alpha is always 255
 * don't check free disk space when outputting to /vsistdout/ (#6768)
 * make GetUnitType() use VERT_CS unit as a fallback (#6675)
 * in COPY_SRC_OVERVIEWS=YES mode, set nodata value on overview bands
 * read GCPs in ESRI <GeodataXform> .aux.xml
 * explicitly write YCbCrSubsampling tag, so as to avoid (latest version of) libtiff to try reading the first strip to guess it. Helps performance for cloud optimized geotiffs
 * map D_North_American_1927 datum citation name to OGC North_American_Datum_1927 so that datum is properly recognized (#6863)
 * Internal libtiff. Resync with CVS (post 4.0.7)
 * Internal libtiff: fix 1.11 regression that prevents from reading one-strip files that have no StripByteCounts tag (#6490)

GRASS driver:
 * plugin configure: add support for GRASS 7.2 (#6785)
 * plugin makefile: do not clone datum tables and drivers (#2953)
 * use Rast_get_window/Rast_set_window for GRASS 7 (#6853)

GRIB driver:
 * Add (minimalistic) support for template 4.15 needed to read Wide Area Forecast System (WAFS) products (#5768)
 * **Partial** resynchronization with degrib-2.0.3, mostly to get updated tables (related to #5768)
 * adds MRMS grib2 decoder table (http://www.nssl.noaa.gov/projects/mrms/operational/tables.php) (github #160)
 * enable PNG decoding on Unix (#5661, github #160)
 * remove explicitly JPEG2000 decompression through Jasper and use generic GDAL code so that other drivers can be triggered
 * fix a few crashes on malformed files

GTX driver:
 * add a SHIFT_ORIGIN_IN_MINUS_180_PLUS_180 open option

HDF4 driver:
 * Fixed erroneous type casting in HDF4Dataset::AnyTypeToDouble() that breaks reading georeferencing and other metadata

HDF5 driver:
 * correct number of GCPs to avoid dummy trailing (0,0)->(0,0,0) and remove +180 offset applied to GCP longitude. Add instead a heuristics to determine if the product is crossing the antimeridian, and a HDF5_SHIFT_GCPX_BY_180 config option to be able to override the heuristics (#6666)

HFA driver:
 * fix reading and writing of TOWGS84 parameters (github #132)
 * export overview type from HFA files to GDAL metadata as OVERVIEWS_ALGORITHM (github #135)
 * make .ige initialization use VSIFTruncateL() to be faster on Windows
 * add support for TMSO and HOM Variant A projections (#6615)
 * Add elevation units read from HFA files metadata (github #169)
 * set binning type properly according to layerType being thematic or not (#6854)

Idrisi driver:
 * use geotransform of source dataset even if it doesn't have a SRS (#6727)
 * make Create() zero-initialize the .rst file (#6873)

ILWIS driver:
 * avoid IniFile::Load() to set the bChanged flag, so as to avoid a rewrite of files when just opening datasets

ISCE driver:
 * fix computation of line offset for multi-band BIP files, and warn if detecting a wrong file produced by GDAL 2.1.0 (#6556)
 * fix misbehaviour on big endian hosts
 * add support for reading and writing georeferencing (#6630, #6634)
 * make parsing of properties case insensitive (#6637)

ISIS3 driver:
 * add write support
 * add mask band support on read
 * get label in json:ISIS3 metadata domain

JPEGLS driver:

JP2ECW driver:
 * fix crash when translating a Float64 raster (at least with SDK 3.3)

JP2KAK driver:
 * add support for Kakadu v7.9.  v7.8 should not be used.  It has a bug fixed in v7.9
 * catch exceptions in jp2_out.write_header()

JP2OpenJPEG driver:
 * add a USE_TILE_AS_BLOCK=YES open option that can help with whole image conversion
 * prevent endless looping in openjpeg in case of short write
 * for single-line organized images, such as found in some GRIB2 JPEG2000 images, use a Wx1 block size to avoid huge performance issues (#6719)
  * ignore warnings related to empty tag-trees.

JPIPKAK driver:
 * fix random crashes JPIP in multi-tread environment (#6809)

KEA driver:
 * Add support for Get/SetLinearBinning (#6855)

KMLSuperOverlay driver:
 * recognize simple document made of GroundOverlay (#6712)
 * Add FORMAT=AUTO option. Uses PNG for semi-transparent areas, else JPG. (#4745)

LAN driver:
 * remove wrong byte-swapping for big-endian hosts

MAP driver:
 * change logic to detect image file when its path is not absolute

MBTiles driver:
 * on opening if detecting 3 bands, expose 4 bands since there might be transparent border tiles (#6836)
 * fix setting of minzoom when computing overviews out of order
 * do not open .mbtiles that contain vector tiles, which are not supported by the driver

MEM driver:
 * disable R/W mutex for tiny performance increase in resampled RasterIO
 * add support for overviews
 * add support for mask bands

MRF driver:
 * bug fix in PNG and JPEG codecs
 * Fixing a problem with setting NoData for MRFs generated with Create
 * fix plugin building (#6498)
 * rename CS variable so as to avoid build failure on Solaris 11 (#6559)
 * Allow MRF to write the data file directly to an S3 bucket.
 * Allow relative paths when MRF is open via the metadata string.
 * Add support for spacing (unused space) between tiles. Defaults to none.
 * Read a single LERC block as an MRF file.

MSG driver:
 * fix incorrect georeference calculation for msg datasets (github #129)

NetCDF driver:
 * add support for reading SRS from srid attribute when it exists and has content like urn:ogc:def:crs:EPSG::XXXX (#6613)
 * fix crash on datasets with 1D variable with 0 record (#6645)
 * fix erroneous detection of a non-longitude X axis as a longitude axis that caused a shift of 360m on the georeferencing (#6759)
 * read/write US_survey_foot unit for linear units of projection
 * apply 'add_offset' and 'scale_factor' on x and y variables when present, such as in files produced by NOAA from the new GOES-16 (GOES-R) satellite (github #200)
 * add a HONOUR_VALID_RANGE=YES/NO open option to control whether pixel values outside of the validity range should be set to the nodata value (#6857)
 * fix crash on int64/uint64 dimensions and variables, and add support for them (#6870)

NITF driver:
 * add support for writing JPEG2000 compressed images with JP2OpenJPEG driver
 * fix writing with JP2KAK driver (emit codestream only instead of JP2 format)
 * fix setting of NBPR/NBPC/NPPBH/NPPBV fields for JPEG2000 (fixes #4322); in JP2ECW case, make sure that the default PROFILE=NPJE implies 1024 block size at the NITF level
 * implement creation of RPC00B TRE for RPC metadata in CreateCopy() mode
 * add support for reading&writing _rpc.txt files
 * nitf_spec.xml: Add support for MTIRPB TRE in NITF image segment. Also makes minor change to BLOCKA to include default values (github #127)
 * nitf_spec.xml: add IMASDA and IMRFCA TREs
 * GetFileList(): Small optimization to avoid useless file probing.

NWT_GRD:
 * detect short writes

OpenJPEG driver:
 * support direct extracting of GeoRaster JP2-F BLOB (#6861)

PCIDSK driver:
 * handle Exceptions returned from destructor and check access rights in setters (github #183)

PDF driver:
 * implement loading/saving of metadata from/into PAM (#6600)
 * implement reading from/writing to PAM for geotransform and projection (#6603)
 * prevent crashes on dataset reopening in case of short write

PLScenes driver:
 * add a METADATA open option

PostgisRaster driver:
 * fix potential crash when one tile has a lower number of bands than the max of the table (#6267)

R driver:
 * fix out-of-memory (oom) with corrupt R file

Raw drivers:
 * prevent crashes on dataset closing in case of short write

RMF driver:
 * fix wrong counter decrement that caused compressed RMF to be incorrectly decompressed (github #153)
 * fix load/store inversion of cm and dm units in MTW files (github #162)
 * fix reading nodata for non-double data type (github #174)

ROIPAC driver:
 * add support for reading/writing .flg files (#6504)
 * fix computation of line offset for multi-band BIP files, and warn if detecting a wrong file produced by GDAL >= 2.0.0 (#6591)
 * fix for big endian hosts

RS2 driver:
 * add support for reading RPC from product.xml

SAFE driver:
 * fix handling of SLC Products by providing access to measurements as subdatasets (#6514)

Sentinel2 driver:
 * add support for new "Safe Compact" encoding of L1C products (fixes #6745)

SQLite driver:
 * Add read support for RasterLite2 coverages in SQLite driver

SRTMHGT driver:
 * open directly .hgt.zip files
 * accept filenames like NXXEYYY.SRTMGL1.hgt (#6614)
 * handle files for latitude >= 50 (#6840)

VRT driver:
 * add default pixel functions: real, imag, complex, mod, phase, conj, etc... for complex data types (github #141)
 * avoid useless floating point values in SrcRect / DstRect (#6568)
 * avoid buffer initialization in RasterIO() when possible (replace ancient and likely broken concept of bEqualAreas)
 * make CheckCompatibleForDatasetIO() return FALSE on VRTDerivedRasterBands (#6599)
 * VRT warp: fix issue with partial blocks at the right/bottom and dest nodata values that are different per band (#6581)
 * fix performance issue when nodata set at band level and non-nearest resampling used (#6628)
 * VRTComplexSource: do temp computations on double to avoid precision issues when band data type is Int32/UInt32/CInt32/Float64/CFloat64 (#6642)
 * VRT derived band: add the capability to define pixel functions in Python
 * CreateCopy(): detect short writes
 * Fix linking error on VRTComplexSource::RasterIOInternal<float>() (#6748)
 * avoid recursion in xml:VRT metadata (#6767)
 * prevent 'Destination buffer too small' error when calling GetMetadata('xml:VRT') on a in-memory VRT copied from a VRT
 * fix 2.1 regression that can cause crash in VRTSimpleSource::GetFileList() (#6802)

WMS driver:
 * Added support for open options to WMS minidrivers
 * Refactored the multi-http code to make it possible to do range requests.
 * Added a minidriver_mrf, which reads from remote MRFs using range requests.
 * Made the minidriver_arcgis work with an ImageService, not only MapService.
 * Added static cache of server response.
 * Allow tiledWMS to work in off-line mode by including the server response in the .wms file itself.
 * honour GDAL_HTTP_USERAGENT config option when it is set and <UserAgent> is missing (#6825)
 * WMS/WMTS: better deal with tiles with different band count (grayscale, gray+alpha, palette, rgb, rgba) (github #208)
 * Make HTTPS options apply to tWMS minidriver init, better HTTP error reporting

WMTS driver:
 * do not take into account WGS84BoundingBox/BoundingBox that would be the result of the densified reprojection of the bbox of the most precise tile matrix
 * add TILEMATRIX / ZOOM_LEVEL open options
 * accept tiles of small dimensions (github #210)

XYZ driver:

## OGR 2.2.0 - Overview of Changes

Core:
 * Layer algebra: Add KEEP_LOWER_DIMENSION_GEOMETRIES=YES/NO option to Intersection, Union and Identity.
Default is yes, but it is set to no unless result layer is of geom type unknown.
If set to no, result features which would have lower dim geoms are skipped
if operating on two layers with same geom dim.
 * Fix crash/corrupted values when running importFromWkb() on POLYGON M/POLYGON ZM geometries (#6562)
 * Add OGR_WKT_PRECISION config option that defaults to 15 to select the number of decimals when outputting to WKT
 * Make OGRFeature::SetField(string) accept JSon serialized arrays for the String/Integer/Integer64/RealList types; add reciprocal OGRFeature::GetFieldAsSerializedJSon() for those types
 * OGRGeometryFactory::transformWithOptions(): better deal with reprojection from polar projection to WGS84, and projections crossing the antimeridian to WGS84, by splitting geometries prior to reprojection (#6705)
 * LinearRing transformTo(): force last point to be identical to first one in case it is not.
 * GML geometry parsing: avoid 'Cannot add a compound curve inside a compound curve' error (#6777)
 * OGR SQL: fix IN filtering on MapInfo indexed columns (#6798)
 * OGR SQL: add support for LIMIT and OFFSET keywords
 * OGR SQL: add comparisons on date / datetime (#6810)
 * OGR SQL: increase efficiency of DISTINCT operator
 * OGREnvelope: change initialization to fix issue when getting MULTIPOINT(0 0,1 1) envelope (#6841)
 * OGRParse: fix parsing logic to avoid false positive detection of string as datetime (#6867)

OGRSpatialReference:
 * Upgrade to EPSG database v9.0 (#6772)
 * OGRCT: upgrade LIBNAME of mingw and cygwin to libproj-9.dll and cygproj-9.dll to be up-to-date with proj 4.9.X (recommended method is using ./configure --with-static-proj4 instead) (#6501)
 * importFromESRI(): fix import of multi line MERCATOR SRS (#6523)
 * morphToESRI(): correctly compute standard_parallel_1 of Mercator(2SP) projection from scale factor of Mercator(1SP) (#6456, #4861)
 * exportToProj4(): recognize explicit OSR_USE_ETMERC=NO to avoid using etmerc with proj >= 4.9.3
 * importFromProj4(): do not set a AUTHORITY node for strings like '+init=epsg:XXXX +some_param=val'
 * importFromProj4(): be robust with missing proj.4 epsg dictionary when importing '+init=epsg:xxxx +other_parm=value'
 * AutoIdentifyEPSG(): add identification of EPSG:3995 (Arctic Polar Stereographic on WGS84) and EPSG:3031 (Antarctic Polar Stereographic on WGS84)
 * OGRCoordinateTransformation: avoid potential bugs in proj.4 on NaN input
 * importFromEPSG(): take into account DX,DY,DZ,RX,RY,RZ,DS columns in pcs.csv to add per-PCS TOWGS84 overrides (geotiff #52)
 * Coordinate transformation: prevent unnecessary coordinate transformations (github #184, #185)

Utilities:
 * ogr2ogr: do not return error on ogr2ogr --utility_version
 * ogr2ogr: keep -append and -overwrite when -update follows
 * ogr2ogr: fix heuristics to detect likely absence of '-f' (#6561)
 * ogr2ogr: do not emit a warning when updating/overwriting a destination dataset that is not a Shapefile and if -f is not specified (#6561)
 * ogr2ogr: make overwriting of lots of PG tables less prone to PostgreSQL 'out of shared memory' errors, by committing transactions after each table recreation
 * ogr2ogr: prevent 'ogr2ogr same.shp same.shp' (#1465)
 * ogr2ogr: add a -limit option to limit the number of features read per layer
 * GDALVectorTranslate(): imply update mode if access mode not explicitly specified and hDstDS != NULL (#6612)
 * ogrlineref: Fix crash if no new layer name provided
 * ogrtindex: add -src_srs_name, -src_srs_format and -t_srs option to allow indexing files that have different projections

Multidriver:
 * PGeo/FileGDB/OpenFileGDB: OGRCreateFromShapeBin(): do not error out on empty lines/polygons
 * GPKG/SQLite/PG/FileGDB/MEM: properly set/reset field subtype with AlterFieldDefn() (#6689)
 * DXF, GeoJSON, GML, KML, LIBKML, ODS, Shape, XLSX: report operating system error if file creation fails (#2253)

AmigoCloud driver:
 * add option to receive an AmigoCloud API key in the connection string, fix page size (github #137)

Carto:
 * renamed from CartoDB
 * fix CartoDB'fication() by removing manual creation of the_geom_webmercator mercator, and also attach created sequence to table.cartodb_id (#6565)

CSV driver:
 * add read/write support for String/Integer/Integer64/RealList types as serialized JSon arrays

DGN driver:
 * avoid crash with -ftrapv on 250K_NF44NE_Area_V7.dgn (relates to #6806)
 * use coordinate delta encoding from the attribute records array (6806)

DXF driver:
 * sanitize layer name on export to avoid forbidden characters
 * reader: convert GeometryCollection to MultiPoint, MultiLineString or MultiPolygon when possible (QGIS #10485)
 * add font of TEXT and MTEXT to OGR style string (https://github.com/OSGeo/gdal/pull/198)

DWG driver:
 * compilation fixes with Teigha 4.2.2

ElasticSearch driver:
 * add support for ElasticSearch 5.0 and fix 2.X support (#6724)
 * implement translation from SQL to E.S. query language

FileGDB driver:
 * add support to read curve geometries (#5890)
 * support opening '.' directory

GeoJSON driver:
 * writer: add a RFC7946=YES creation option (#6705)
 * read and write 'name' and 'description' members at FeatureCollection level
 * fix field type detection when first value of a field is null (#6517)
 * improve/fix field type promotion
 * fix wrong behavior when there's a 'id' at Feature level and 'id' or 'ID' field in properties (#6538)
 * in case top level id is a negative integer, put the value in a 'id' attribute (#6538)
 * ESRI Json reader: support multilinestring from esriGeometryPolyline
 * ESRI Json reader: do not set field width of 2147483647 (#6529)
 * ESRI Json reader: support reading M and ZM geometries
 * Add CPL_json_object_object_get() and use it, to avoid deprecation warnings on json_object_object_get()
 * TopoJSON reader: sanitize invalid polygons (such as found in the 'TopoJSON' layer of http://bl.ocks.org/mbostock/raw/4090846/us.json)
 * writer: fix segfaults on NULL geometry with -lco WRITE_BBOX=YES (#6698)
 * writer: fix crash if NATIVE_MEDIA_TYPE creation option is specified alone
 * Add support of TopoJSON without 'transform' element (github #192)
 * don't set SRS if 'crs' set to null (github #206)

GML driver:
 * remove hack for CityGML regarding forcing srsDimension to 3 if not specified. Instead add a GML_SRS_DIMENSION_IF_MISSING config option that can be set to 3 if needed (#6597)
 * consider srsName with URL like 'http://www.opengis.net/def/crs/EPSG/0/' as following EPSG axis order. Add SWAP_COORDINATES=YES/NO/AUTO (and GML_SWAP_COORDINATES configuration option) to override all auto guessing (#6678)
 * add a SRSNAME_FORMAT=SHORT/OGC_URN/OGC_URL dataset creation option
 * OGR_G_ExportToGMLEx(): add a COORD_SWAP option
 * Writer: make ogr:FeatureCollection be a substitutionGroup of AbstractFeature for GML 3.2 output, so as to be compatible of GMLJP2 v2
 * GML and NAS: improve error reporting (mention feature id and gml_id) when parsing fails because of geometry decoding error
 * GML and NAS: add GML_SKIP_CORRUPTED_FEATURES and NAS_SKIP_CORRUPTED_FEATURES configuration options to avoid stopping reading a layer because of a corrupted geometry
 * Add support for Japanese GML FGD v4.1 format (github #204)

GPKG driver:
 * list all tables/views by default (useful for non spatial tables not registered as aspatial). Add ASPATIAL_VARIANT layer creation option to select the way how to create a non-spatial table as aspatial.
 * Create non-spatial tables by default conforming to GPKG v1.2 'attributes'
 * use OGR_CURRENT_DATE config option if defined as the value of the 'last_change' of column of 'gpkg_contents' so as to be able to have binary identical output
 * update last_change column when content of raster or vector tables has changed
 * do not emit error when running ExecuteSQL() with a spatial filter on an empty layer (#6639)
 * update schema to reflect CURRENT_TIMESTAMP -> 'now' changes (github #155)
 * better table and column quoting.
 * Robustify layer deletion (make it accessible through DROP TABLE xxx and DELLAYER:xxx syntax) and renaming
 * implement DeleteField(), AlterFieldDefn(), ReorderFields()
 * make GetExtent() save extent if not already cached
 * add special SQL 'RECOMPUTE EXTENT ON layer_name' to force recomputation of extent
 * check identifier unicity on layer creation
 * add possibility to disable foreign key check by setting OGR_GPKG_FOREIGN_KEY_CHECK=NO config option
 * add HasSpatialIndex(tblname,geomcolname) SQL function
 * don't show Spatialite vgpkg_ virtual tables (#6707)
 * SQLite/GPKG: add explicit error message when trying to open a read-only WAL-enabled database (#6776)
 * SQLite/GPKG: make sure when closing a WAL-enabled database opened in read-only mode to reopen it in read-write mode so that the -wal and -shm files are removed (#6776)
 * make GetFeature() works on non conformant tables that have no integer primary key field (#6799), and be robust to non standard column types
 * remove triggers related to metadata tables that cause issues and have been removed by latest revisions of the spec.
 * declare feature id column of features tables and tile pyramid user data tables as NOT NULL (#6807)
 * add a gpkg_ogr_contents table to store feature count.
 * speed-up GetFeatureCount() with only a spatial filter set
 * improve column recognition for SQL result layer
 * add/override ST_Transform() and SridFromAuthCRS() from Spatialite to make them work with gpkg_spatial_ref_sys
 * add ImportFromEPSG()
 * make ST_Min/MaxX/Y(), ST_SRID(), ST_GeometryType() and ST_IsEmpty() work with Spatialite geometries
 * improve performance of spatial index creation and use on multi-gigabyte databases
 * better support of spatial views, by adding a special behavior if a column is named OGC_FID
 * avoid potential denial of services by adding LIMIT clauses
 * slightly more efficient implementation of GetExtent() if extent in gpkg_contents is empty
 * create a dummy 'ogr_empty_table' features table in case we have no 'features' or 'tiles' table, so as to be conformant with Req 17 of the GeoPackage specification
 * add DEFAULT '' to metadata column of gpkg_metadata table
 * accept opening a .gpkg without vector content and without gpkg_geometry_columns table in vector mode if we also open in update mode; remove capability of opening a .gpkg without vector content but with gpkg_geometry_columns table in vector mode if we only open in read-only mode; fix creation of a vector layer in a database if it initially lacks a gpkg_geometry_columns table
 * fix appending a raster to a vector database without pre-existing raster support tables
 * add minimalistic support for definition_12_063 column in gpkg_spatial_ref_sys, so that insertion of new SRS doesn't fail
 * use GEOMETRYCOLLECTION instead of GEOMCOLLECTION for SQL and gpkg_geometry_columns.geometry_type_name
 * do not warn if gpkg_metadata extension declared

GPX driver:
 * ignore wpt/rtept/trkpt with empty content for lat or long

ILI driver:
 * ILI1: fix crash in JoinSurfaceLayer() when the multicurve of the feature of the poSurfaceLineLayer layer is empty (#6688)
 * ILI1: make polygon reconstruction in Surface layers robust to curves not in natural order (#6728)
 * ILI2: assign FID to features (#6839)
 * ILI2: fix crashing bug in Create() if model file not specified

KML driver:
 * add a DOCUMENT_ID datasource creation option to set the id of the root <Document> node

LIBKML driver:
 * fix crash when reading <gx:TimeStamp> or <gx:TimeSpan> elements (#6518)
 * add a DOCUMENT_ID datasource creation option to set the id of the root <Document> node
 * emit style related errors as warnings to make datasets openable by SWIG bindings (#6850)

MITAB driver:
 * limit (width, precision) of numeric fields on creation to (20,16) for compatibility with MapInfo (#6392)
 * add support for oblique stereographic (#6598)
 * Adds the authority code for Irish national grid (Ireland_1965) (github #149)
 * fix spelling for Euref_89 and add EPSG code (#6816)

MSSQLSpatial driver:
 * Fix bulk insert with table names containing spaces (#6527)
 * Build optional mssql plugin with SQL Native Client support for MSSQL Bulk Copy
 * Fix MSSQL select layer to recognize geometry column with sqlncli (#6641)

MySQL driver:
 * fix spatial filtering on recent mysql by adding a SRID in the rectangle geometry
 * do not force NOT NULL constraint on geometry field if no spatial index is used

NAS driver:
 * support multiple 'anlass' in updates

NWT_GRD:
 * add write support (#6533)

OCI driver:
 * Add options for faster feature loading (#6606)
 * add WORKSPACE open option
 * correctly handle OFTInteger64 case in loader layer (bug found by cppcheck multiCondition)
 * support for long identifiers (up to 128 long) when running of 12.2 or + (#6866)
 * OCILOB VSIL driver: new driver to streams in and out of Oracle BLOB as a GDAL large virtual file system (#6861)

ODS driver:
 * fix FID filtering (#6788)

OGDI driver:
 * make GetNextRawFeature() report an error when it is not end of layer
 * better error reporting when the layer list cannot be established
 * catch non-fatal OGDI errors emitted by OGDI 3.2.0 if OGDI_STOP_ON_ERROR environment variable is set to NO, and emit them as CPLError()s
 * display OGDI error message if opening fails

OpenFileGDB driver:
 * do not error out on geometries that have a declared M array but that is missing (#6528)
 * add support to read curve geometries (#5890)
 * transcode UTF-16 strings found in column names, alias, etc... to UTF-8 (instead of using only their ASCII byte) (#6544)
 * do not emit an error on a empty table whose declaration has M settings (#6564)
 * support opening '.' directory
 * improve detection of some form of TINs from MULTIPATCH, and for MultiPatch layers, try to select a better geometry type for those layers (#5888)
 * fix bug when field description offset is beyond 4GB (#6830)

OSM driver:
 * fix 'too many tags in relation' error when parsing .osm files
 * allow key=value entries in closed_ways_are_polygons= configuration (#6476)
 * allow OSM_SQLITE_CACHE config option to be greater than 2047

PG driver:
 * fix insertion of binary/bytea content in non-copy mode (#6566)
 * fix errors caused by missing geometry_columns/spatial_ref_sys tables in non PostGIS databases, that prevent reading more than 500 features (QGIS #10904)
 * avoid errors with field default expressions like 'foo'::text (#6872)

PLScenes driver:
 * add HTTP retry logic (#6655)
 * V0 API: workaround limitations on filtering on image_statistics.image_quality (#6657)
 * add support for Data V1 API

S57 driver:
 * Fix ogr2ogr -f S57 (#6549)
 * fix crashes if the s57objectclasses.csv resource file contains invalid lines
 * remove data/s57attributes_aml.csv data/s57attributes_iw.csv data/s57objectclasses_aml.csv data/s57objectclasses_iw.csv and move their content into main s57attributes.csv and s57objectclasses.csv files (#6673)
 * Update s57 attributes and object classes according to s-57 reference (github #202)
 * add POSACC and QUAPOS fields for geometric primitive layers (github #205)

SDE driver:
 * rename driver to OGR_SDE. Fix build (#6714)

SEGY driver:
 * Accept SEGY files with ASCII headers that have nul terminated strings (#6674)

Shapefile driver:
 * auto-repack by default at dataset closing and FlushCache()/SyncToDisk() time. Controlled by AUTO_REPACK open and layer creation options (that default to YES)
 * generate .dbf end-of-file 0x1A character by default. Add DBF_EOF_CHAR layer creation options / open options to control that behavior
 * writing: use strerrno() for better error messages (QGIS #13468)
 * change REPACK implementation on Windows to be robust to remaining file descriptors opened on the .shp/.shx/.dbf (#6672, QGIS #15570)
 * Fix issue in DBFCloneEmpty() one a one field DBF not yet written to disk
 * add call to AutoIdentifyEPSG() when reading a .prj
 * support reading .dbf with substantial padding after last field definition.
 * when rewriting the geometry of the last record in the .shp, do it at the file offset it previously used (#6787)

SOSI driver:
 * make registration of driver work again (2.1.0 regression) (#6500)
 * update to latest version of https://github.com/kartverket/gdal. Make SOSI driver support more geometry types (including curved geometries) plus provides some improvements on attribute-type mapping. Fix some memory errors/leaks. Disable by default non working creation code (#6503)

SQLite/Spatialite driver:
 * do not emit error when running ExecuteSQL() with a spatial filter on an empty layer (#6639)
 * add read/write support for String/Integer/Integer64/RealList types as serialized JSon arrays
 * Spatialite: avoid crash when creating layer with geom_type = wkbCurve (fixes #6660)
 * Spatialite: do not report some BLOB columns as geometry columns of tables/views (when found before the geometry column(s)) (#6695, #6659)
 * fix update of features with multiple geometry columns (#6696)
 * speed-up dataset closing when creating a database with many spatial layers
 * Spatialite: avoid spatial views to cause layers 'layer_name(geometry_name)' to be publicly listed (#6740)
 * Spatialite: speed-up creation of database with INIT_WITH_EPSG=NO that is slow without transaction (not sure why as the table is empty...)
 * use AUTOINCREMENT for feature id column
 * allow OGR_SQLITE_CACHE to be set to > 2047 (MB) without overflowing
 * SQLite/GPKG: use SQLITE_OPEN_NOMUTEX flag to open databases.
 * GPKG/SQLite: fix ExecuteSQL() to work with a statement with 2 SELECT and ORDER BY clause (#6832)
 * SQLite/GPKG: change default page_size to 4096 bytes.
 * Update layer statistics for Spatialite 4 DB (#6838)
 * Remove traces of support of SQLite < 3.6.0
 * SQLite dialect: properly quote column names when needed (github #214)

VFK driver:
 * allow reading multiple VFK files into single DB support amendment VFK files
 * recreate DB in the case that it's outdated (VFK DB created by previous versions of GDAL)
 * allow reading DB as valid datasource (#6509)
 * new tables in backend SQLite database (geometry_columns/spatial_ref_sys) to enable reading DB datasource by SQLite driver
 * new configuration option OGR_VFK_DB_READ to enable reading backend database by VFK driver

VRT driver:
 * add support for 'm' attribute in PointFromColumns mode (#6505)

WFS driver:
 * invalidate underlying layer when SetIgnoredFields() is called (QGIS #15112)
 * don't crash on empty <Keyword/> declaration (#6586)
 * fix potential nullptr dereference on dataset without layer (github #179)

XLSX driver:
 * only list worksheets (and no charts) as layers (#6680)

## SWIG Language Bindings

All bindings:
 * map osr.GetTargetLinearUnits() (#6627)
 * allow wkbCurve/wkbSurface as valid values for the geom type of GeomFieldDefn object
 * map GDALIdentifyDriverEx()

Java bindings:
 * Fix SWIG Java bindings for GNM (#6434)
 * Fix crash on GetDefaultHistogram() if the C++ method returns an error (#6812)

Perl bindings:
 * return value always from GetGeomFieldIndex (#6506)
 * the Warp method requires a list of datasets (#6521)
 * when 'use bigint' is in effect, int var is a ref.
 * Separate the module building in the CPAN distribution and in
the GDAL source tree. The CPAN distribution will be
developed at https://github.com/ajolma
 * Fix the Extension method in Driver per RFC 46
 * The Inv method of GeoTransform did not return a new object.
Allow a single point in Apply method of GeoTransform.
 * Test for existence of PDL and require it if available.
 * allow decimation/replication in Piddle i/o.
 * support resampling in ReadTile.
 * Polygonize: Require explicit 8 for Connectedness to set 8-connectedness and allow 8CONNECTED as an option.
 * use Safefree to free memory allocated in Perl (#6796)

Python bindings:
 * release the GIL before entering GDAL native code (for all, in GDAL module and a few ones in ogr like ogr.Open())
 * add outputType option to gdal.Rasterize()
 * fix build issues when CXX is defined in the environment
 * gdal.VectorTranslate(): add spatSRS option
 * when enabling ogr.UseExceptions(), use the GDAL error message in the exception text (if available), when the exception is linked to an error value in the OGRErr return code
 * gdal.VectorTranslate(): accept a single string as value of the layers option (instead of iterating over each of its characters)
 * Regenerate Python bindings with SWIG 3.0.8 to avoid issue with Python 3.5. Add backward compatibility in Band.ComputeStatistics() to accept 0/1 as input instead of the expected bool value (#6749)
 * fix gdal.DEMProcessingOptions(zeroForFlat=True) (#6775)
 * fix 'import osgeo.gdal_array' with python3 and SWIG 3.0.10 (#6801)
 * allow gdal.FileFromMemBuffer() to use buffer > 2GB (#6828)
 * accept unicode strings as field name argument in Feature (like SetField, GetField, etc...) and FeatureDefn methods

# GDAL/OGR 2.1.0 Release Notes

## In a nutshell...

 * New GDAL/raster drivers:
    - CALS: read/write driver for CALS Type I rasters
    - DB2 driver: read/write support for DB2 database (Windows only)
    - ISCE: read/write driver (#5991)
    - MRF: read/write driver (#6342)
    - SAFE: read driver for ESA SENTINEL-1 SAR products (#6054)
    - SENTINEL2: read driver for ESA SENTINEL-2 L1B/LC1/L2A products
    - WMTS: read driver for OGC WMTS services
 * New OGR/vector drivers:
    - AmigoCloud: read/write support for AmigoCloud mapping platform
    - DB2 driver: read/write support for DB2 database (Windows only)
    - MongoDB: read/write driver
    - netCDF: read/write driver
    - VDV: read/write VDV-451/VDV-452 driver, with specialization for the
           Austrian official open government street graph format
 * Significantly improved drivers:
    - CSV: new options, editing capabilities of existing file
    - ElasticSearch: read support and support writing any geometry type
    - GeoJSON: editing capabilities of existing file, "native data" (RFC 60) support
    - MBTiles: add raster write support. fixes in open support
    - PDF: add PDFium library as a possible back-end.
    - PLScenes: add support for V1 API
    - VRT: on-the-fly pan-sharpening
    - GTiff: multi-threaded compression for some compression methods
 * Port library: add /vsis3/, /vsis3_streaming/, /vsicrypt/ virtual file systems
 * RFC 26: Add hash-set band block cache implementation for very larger rasters (WMS, WMTS, ...)
        http://trac.osgeo.org/gdal/wiki/rfc26_blockcache
 * RFC 48: Geographical networks support (GNM)
        https://trac.osgeo.org/gdal/wiki/rfc48_geographical_networks_support
 * RFC 58: Add DeleteNoDataValue():
        https://trac.osgeo.org/gdal/wiki/rfc58_removing_dataset_nodata_value
 * RFC 59.1: Make GDAL/OGR utilities available as library functions:
        https://trac.osgeo.org/gdal/wiki/rfc59.1_utilities_as_a_library
   For gdalinfo, gdal_translate, gdalwarp, ogr2ogr, gdaldem, nearblack, gdalgrid,
   gdal_rasterize, gdalbuildvrt
   Available in C, Python, Perl and Java bindings.
 * RFC 60: Improved round-tripping in OGR
        https://trac.osgeo.org/gdal/wiki/rfc60_improved_roundtripping_in_ogr
   Implemented in GeoJSON driver
 * RFC 61: Support for measured geometries.
        https://trac.osgeo.org/gdal/wiki/rfc61_support_for_measured_geometries
   Implemented in Shapefile, PostgreSQL/PostGIS, PGDump, MEM, SQLite, GeoPackage,
   FileGDB, OpenFileGDB, CSV, VRT
 * Upgrade to EPSG database v8.8
 * General sanitization pass to clean-up code, fix a lot of compiler warnings,
   as well as issues pointed by static code analyzers, such as Coverity Scan
   (credits to Kurt Schwehr for tackling a large part of them) or CLang Static Analyzer.
 * Fixes in a number of drivers to be more robust against corrupted files (most found
   with American Fuzzy Lop): RIK, INGR, Northwood, HF2, CEOS, GTiff, GXF, BMP,
   NITF, HFA, VRT, FIT, CEOS2, NWT_GRD/NWT_GRC, MITAB, RPFTOC, DBF/Shape, XYZ, VFK, DXF,
   NAS, GSAG, GS7BG, OpenFileGDB, RMF, AIGRID, OpenAIR, EHDR, ISO8211, FAST,
   USGSDEM, DGN, SGI, OpenJPEG, PCRaster, BSB, ADRG, SRP, JPEG, Leveller, VICAR, PCIDSK, XPM
   as well as in portability library (CPL), algorithms and raster core.
 * Driver removed:
    - Google Maps Engine (GME)  (#6261)

## New installed files
 * gdal_pansharpen.py
 * data/vdv452.xml
 * data/vdv452.xsd
 * data/netcdf_config.xsd
 * /path/where/bash-completion/scripts/are/installed/gdal-bash-completion.sh

## Backward compatibility issues

See MIGRATION_GUIDE.TXT

## GDAL/OGR 2.1.0 - General Changes

Build(Unix):
 * Allow plugin for HDF4, HDF5, GeoRaster, FileGDB, OCI and PG
 * Add a --with-gnm option to enable GNM
 * Add --enable-lto switch to turn on link time optimization (GCC >= 5)
 * Add --with-spatialite=dlopen --with-spatialite-soname=libspatialite.so[.X] syntax to allow linking against spatialite through dlopen() mechanism (#6386)
 * Add a lot of warning options when available in the compiler.
 * Change detection test of podofo to work with podofo 0.9.3
 * Fix gdalserver compilation with recent GNU libc (such as in Arch Linux) (#6073)
 * Add pkg-config support for libkml fork (#6077)
 * Update config.sub and config.guess to their latest upstream versions so as to be able to detect new architectures (android, ppcle64, etc..) (#6101)
 * Fix python package installation in custom prefixes (#4563)
 * configure: check that CXX is really a working compiler (#4436)
 * Build support for Kakadu 7.7
 * thinplatespline.cpp: avoid using optimized version of VizGeorefSpline2DBase_func4() with ICC versions that fail on it (#6350)
 * Add bash completion for GDAL/OGR utilities and scripts (#6381)
 * Add support for ODBC autodetection with mingw64 (#6000)
 * Remove macos 9 and older support (#6133).
 * Remove dist_docs, burnpath and pszUpdableINST_DATA in gcore/gdaldrivermanager.cpp as they are have not been used for a while (#6140).
 * Removed --without-ogr configure flag (#6117).  Always build with OGR.

Build(Windows):
 * Add support for Visual Studio 2015 (a.k.a MSVC_VER=1900 or VC 14)
 * Add KAKFLAGS to nmake.opt with KDU_{MAJOR,MINOR,PATCH}_VERSION define-s.
 * nmake.opt: make it less error prone to define SETARGV when paths include spaces (#6032)
 * nmake.opt: add CL.EXE compiler option /FC to display full path of source code file in diagnostics
 * Remove WinCE support (#6133)
 * nmake.opt: add /I flag to the INSTALL command so that xcopy will be smart enough to create a directory when copying files and avoid prompting for user input (https://github.com/OSGeo/gdal/pull/95)
 * Enable mssql spatial blugin build, use BCP as the default option for the sqlncli enabled builds
 * Add support to find MrSID 9.5 dll
 * Fix Windows build with recent MySQL versions and Visual Studio 2015 (#6457)

Build(all):
 * Compatibility with C++11 and C++14
 * Optional crypto++ dependency for /vsicrypt/ support
 * Optional mongocxx dependency for MongoDB support

Other:
 * Disable copy constructor and assignment operators in classes OGRFieldDefn, OGRGeomFieldDefn, OGRFeature, GDALMultiDomainMetadata, GDALDefaultOverviews, GDALOpenInfoGDALDataset, GDALRasterBlock, GDALRasterBand and GDALDriver (#6100)

## GDAL 2.1.0 - Overview of Changes

Port:
 * Add /vsicrypt/ virtual file system for reading/creating/update encrypted files on the fly, with random access capabilities
 * Add /vsis3/ and /vsis3_streaming/ virtual file systems to read/write objects from AWS S3 buckets
 * /vsizip/: avoid returning the previous file list of an already opened .zip if it has changed afterwards (#6005)
 * /vsizip/: use CP437 on Windows when ICONV support is available (#6410)
 * /vsimem/: implement append mode (#6049)
 * /vsistdin/: fix caching of first 1MB / VRT driver: read XML content from /vsistdin/ in a streaming compatible way (#6061)
 * /vsistdout/: flush when closing the handle (#6149)
 * Add VSIReadDirEx() with a limit on the number of files before giving up and corresponding VSIFilesystemHandler::ReadDirEx() virtual function
 * cpl_vsil_cache: rework to be able to work on very large files on 32bit systems
 * Add CPLThreadLocaleC class to use thread-specific locale settings (when available)
 * VSIWin32Handle::Flush(): add VSI_FLUSH config option that can be set to TRUE to force FlushFileBuffers(). (hack related to #5556)
 * Fix VSIL append mode in Windows (#6065)
 * Make CPLCreateMutexEx() support CPL_MUTEX_REGULAR; and fix CPL_MUTEX_ADAPTIVE to avoid continuing in code path for creation of recursive mutex
 * Add CPLWorkerThreadPool API
 * Add CPLGetThreadLocalConfigOption()
 * Fix CPL_LSBINT16PTR() and CPL_LSBINT32PTR() to work with non-byte pointer (#6090)
 * CPLRecodeStub(): add special case for CP437 -> UTF-8 when input is only printable ASCII
 * CPLHTTPFetch(): add LOW_SPEED_TIME and LOW_SPEED_LIMIT options
 * CPLGetValueType(): improve heuristics to avoid detecting some WKB strings as real numbers (#6128)
 * Add CPL_SHA256* and CPL_HMAC_SHA256 functions
 * Make CPLVirtualMemFileMapNew() work on all systems having mmap(), and thus GeoTIFF GTIFF_VIRTUAL_MEM_IO optimization too
 * Add VSI_MALLOC_VERBOSE() and similar macros to output an error message in case of failed alloc
 * CPLParseXMLString(): limit depth of elements to 10000
 * Win32 file management: handle files whose total file path length is greater than 255 characters in Open() and Stat()
 * Make CPLFormFilename(absolute_path, "..", NULL) truncate when possible
 * Add VSIGetDiskFreeSpace()
 * Implement CPLIsInf() for Solaris 11
 * Add a CPL_FINAL macro that expands to C++11 final keyword when C++11 is enabled, and use it in a few places
 * Avoid CPLEscapeString (CPLES_URL) encoding some characters unnecessarily (#5526)
 * Add CPLTestBool(), CPLTestBoolean(), CPLFetchBool()
 * Fix CPLGetValueType() to recognize D1 as a string and not a real number (#6305)
 * CPLFormFilename(): use '/' directory separator on Windows on /vsicurl_streaming/ files (#6310)
 * Add hack enabled by -DDEBUG_BOOL to detect implicit int->bool conversions that dislike MSVC (#6325)
 * Add hack to '#define NULL nullptr' when NULL_AS_NULLPTR is defined. Must be used together with -std=c++11 -Wzero-as-null-pointer-constant with GCC to detect misuses of NULL/nullptr (#6327)
 * Add VSIError mechanism to store errors related to filesystem calls, and use it for /vsis3/. Add new CPLE_ error numbers. (https://github.com/OSGeo/gdal/pull/98)
 * Fix CPLsscanf() to be conformant regarding how space/tab/... separators are handled, so as to fix OGR GMT to be able to read coordinates separated by tabulations (#6453)

Core:
 * Change default value of GDAL_CACHEMAX to 5% of usable physical RAM
 * Allow open options name to be prefixed by @ to be silently ignored when not existing in driver options (RFC 60)
 * Honour NBITS metadata item when doing RasterIO() with non-nearest resampling (#6024)
 * GDALClientServer: pass open options to INSTR_Open call
 * Improve performance of GDALCopyWords() float->byte/int16/uint16 by using SSE2
 * Decorate prototypes of RasterIO() related operations with CPL_WARN_UNUSED_RESULT
 * Avoid deadlock when writing 2 datasets in 2 threads (#6163)
 * Remove obsolete symbols __pure_virtual(), GDALCreateProjDef(), GDALReprojectToLongLat(), GDALReprojectFromLongLat() and GDALDestroyProjDef()
 * Remove obsolete non-template-based implementation of GDALCopyWords(). We don't support anymore such compilers
 * GDALJP2Box::ReadBoxData(): do not abort if memory allocation fails
 * Add GDALAdjustValueToDataType() in C API, and use it in GDALTranslate() and GDALWarp()
 * GDALDefaultOverviews::CreateMaskBand(): fix logic error related to writing per-band mask
 * Block cache: fix excessive memory consumption when dealing with datasets with different block sizes (#6226)
 * EXIFExtractMetadata(): fix potential 1-byte write buffer stack overflow
 * MDReader: do no attempt reading side-car files on /vsisubfile/ (#6241)
 * GDALCheckBandCount(): allow by default a maximum number of bands to 65536
 * GDALOpenInfo: add StealSiblingFiles() and AreSiblingFilesLoaded() methods
 * GDALOpenInfo::GetSiblingFiles(): give up after GDAL_READDIR_LIMIT_ON_OPEN (default=1000) files in the directory
 * GDALDefaultOverviews: add TransferSiblingFiles() method
 * GDALDriver::DefaultCreateCopy(): fix logic related to propagation of NBITS / PIXELTYPE metadata items as creation options
 * Reorder driver registration a bit so that formats with efficient identification are probed first
 * Add GDALIdentifyEnum (GDAL_IDENTIFY_UNKNOWN, GDAL_IDENTIFY_FALSE or GDAL_IDENTIFY_TRUE) for return values of Identify()
 * GDALLoadRPCFile(): load .rpc files from Ikonos products that have extra end-of-line character (#6341)
 * Export GDALRegenerateOverviewsMultiBand() symbol, but mostly for plugins (#6342)
 * Add GDAL_NO_AUTOLOAD to compile out the body of AutoLoadDriver (#6380)
 * Avoid ComputeStatistics(), GetHistogram() and ComputeRasterMinMax() to use only the first column of blocks in approximation mode for a raster whose shape of blocks is a square (#6378)
 * Add GDALGetDataTypeSizeBits() and GDALGetDataTypeSizeBytes().
 * GDALReadOziMapFile(): fix reading Ozi external files from virtual file systems (https://github.com/OSGeo/gdal/pull/114)
 * Add CPLSetCurrentErrorHandlerCatchDebug() to enable custom error handlers not to intercept debug messages

Algorithms:
 * RPC transformer: provide SSE2 accelerated transformer
 * RPC: fix off-by-half pixel computation of (pixel, line), and in bilinear and bicubic RPC DEM interpolation; fix off-by-one pixel registration for Pleiades RPC (#5993)
 * RPC: make RPCInverseTransformPoint() check convergence has been reached (#6162)
 * RPC DEM: optimize when DEM is in NAD83 or any other geodetic CS that transform as a no-op to WGS 84
 * RPC DEM: take into account vertical datum of the DEM when present to convert DEM elevations to ellipsoidal heights (#6084)
 * RPC DEM: do fallback cubic -> bilinear and bilinear -> near on DEM edges
 * RPC DEM: improve inverse transformer to validate error threshold and improve convergence (#6162, #6377)
 * RPC: fix issues with dateline (#6472)
 * TPS solver: discard duplicated GCP to avoid avoidable error, warning when 2 GCPs have same (pixel,line) but different (X,Y) or the reverse
 * Warper: rework multithreaded computations to use a thread pool rather than forking threads each time
 * Warper: avoid really excessive processing time for some warping with target areas completely off the source raster (especially when involving RPC) (#6182)
 * Warper: CreateKernelMask(): fix potential 32 bit integer overflow when using warp memory value > 2GB (#6448)
 * gdal_grid: add linear interpolation algorithm
 * gdal_grid: add invdistnn algorithm, variation on the existing inverse distance weighting algorithm with quadtree to search for points only in the neighborhood (#6038)
 * gdal_grid: fix crash in optimized mode with GCC 4.4 on 64bit (#5987)
 * gdal_grid: compile gdalgrid AVX optimization for Windows when supported by compiler
 * Add GDALTriangulationXXXX() API through libqhull
 * Sieve filter: fix crash on nodata polygons (#6096)
 * Sieve filter: improvement to walk through the biggest neighbour chain until we find a polygon larger than the threshold (#6296)
 * GDALFPolygonize(): factor implementation with integer case
 * GDALComputeMedianCutPCT(): fix to make it work with rasters with more than 2 billion pixels (#6146)
 * Overview: Make average and gauss methods aware of transparent color table entries (#6371)

Utilities:
 * gdalinfo: workaround bug in proj <= 4.9.1 on datasets with a SRS with a vertical shift grid (#6083)
 * gdal_translate: detect more reliably if specified bands are not in default order
 * gdal_translate: fix -a_nodata with negative values on rasters that have PIXELTYPE=SIGNEDBYTE; fix preserving PIXELTYPE=SIGNEDBYTE with VRT
 * gdal_translate: fix random behavior when -scale is used without source bounds (#6455)
 * gdal_rasterize: do on-the-fly reprojection of input vector onto output raster
 * gdal_rasterize: rasterize: always create output with 1/2 cell buffer of input geometry envelope (#6058)
 * gdal_rasterize: add the -dialect option
 * gdal_rasterize: accept NaN as a valid value for -init / -burn (#6467)
 * gdalwarp: add -doo option to specify open options of (existing) output dataset
 * gdalwarp: if RPC_DEM warping option is specified, use exact transformer by default (#5993)
 * gdalwarp: make it honour nodata value of existing dataset (if -dstNoData isn't explicitly specified)
 * gdalwarp: do not propagate STATISTICS_ of second or following source datasets
 * gdalwarp: do not emit warning when using -cutline with a SRS and the source raster has RPC or GEOLOCATION
 * gdalwarp: check that the cutline is valid after transformation/reprojection
 * gdalwarp: better deal when dealing with a mix of RGB and RGBA datasets as input
 * gdalwarp: fix -srcnodata to not put garbage values as target dstnodata (#6315)
 * gdalwarp: densify cutline to avoid invalid geometry after reprojection to source raster, especially in the RPC case (#6375)
 * gdalserver: add a -nofork mode (Unix only for now), so that multiple clients can connect to the same dataset. Useful for safe 'concurrent' updates
 * add gdal_pansharpen.py script
 * gdal2tiles.py: fix error on a raster with less than 3 bands that resulted in a 'IndexError: list index out of range'
 * gdal2tiles.py: Change EPSG:3785 / EPSG:900913 into EPSG:3857 (#5622)
 * gdal2tiles.py: add Leaflet template (https://github.com/OSGeo/gdal/pull/71)
 * gdal2tiles.py: add -q switch for quiet mode
 * gdaldem: correctly deal with NaN as nodata value (#6066)
 * gdaldem color-relief: deal with the case of repeated entries with the same value and the input raster has pixels that match that value exactly (#6422)
 * gdaladdo: emit error message if passed an invalid dataset name (#6240)
 * gdaladdo: do not silence warnings when opening in update mode, if the open is successful
 * gdalbuildvrt: fix potential crash when using -b switch (#6095)
 * gdalbuildvrt: accept nan as value for -srcnodata and -vrtnodata
 * gdalbuildvrt: return non zero return code if the flush of the VRT to disk failed
 * gdal_merge.py: takes again into account -n flag (#6067)
 * gdalbuildvrt / gdal_translate / VRT: use floating point values for source and destination offsets and sizes (#6127)
 * gdalmove.py: fix to run with GDAL 2.0 gdal.InvGeoTransform() signature
 * gdal_calc.py: Add * from gdalnumeric to gdal_calc.py eval namespace again, to fix 2.0 regression that made for example 'log10(A)' to no longer work (https://github.com/OSGeo/gdal/pull/121)

Python samples:
 * gdalpythonserver.py: update to protocol 3

AAIGRID:
 * when writing with floating-point values, ensure at least one value has a decimal point to avoid issues with some readers (#6060)

BMP driver:
 * BMP: avoid too big color table allocation in case of wrong iClrUsed value

BLX driver:
 * converted to support VirtualIO

ECRG driver:
 * change subdataset definition to make sure that they only consist of frames of same scale (#6043)
 * fix base34 decoding and Windows filename handling (#6271)

ECW driver:
 * use wide char Open API on Windows when GDAL_FILENAME_IS_UTF8=YES (https://github.com/OSGeo/gdal/pull/70)
 * fix reading of ECW in /vsi file systems (with SDK >= 4) (#6482)

GeoRaster driver:
 * fix deflate read error depending on endianness (#6252)

GIF driver:
 * libgif: partial resync with giflib master (but keep ABI of 4.1.6)
 * fix crash when CreateCopy a source with one color in the color table

GMT driver:
 * validate raster dimensions on opening, and acquire mutex in dataset destructor

GPKG driver:
 * write support: fix various issues in update scenarios when interacting with the GDAL block cache that could result in lost/corrupted band data to be written in tiles (#6309)
 * write support: fix potential use of freed sqlite temporary DB handle when generating overviews with partial tiles (#6335)
 * write support: fix potential crash in scenarios when block cache is full (#6365)
 * write support: fix inversion of row/column in one of the SQL request involved in partial tiles management (#6339)
 * fix generation of files with 1-band paletted input dataset. Also improve the logic to retrieve color palette when forcing BAND_COUNT=1 on opening (#6450)

GTiff driver:
 * add NUM_THREADS creation and open options to enable multi-threaded compression
 * fix GTiffDataset::IsBlockAvailable() wrong behavior when compiling against internal libtiff, when a BigTIFF file has a TileByteCounts with LONG/4-byte counts and not LONG8 (#6001)
 * Correctly take into account overridden linear units for a geotiff with a EPSG PCS code (#6210, #4954)
 * make VirtualMemIO() work with non native endianness
 * improve VirtualMemIO() performance in tiled Contig read to single band copy
 * improve single band tiled VirtualMemIO reading
 * improve DirectIO() to work on tiled uncompressed geotiff, for non-resampling and nearest resampling cases. Also improve performance of resampling cases on un-tiled files
 * fix DirectIO() mode with complex types and inverted endianness (#6198)
 * optimize writing of 12-bit values
 * implement lazy loading of .aux.xml and .tfw/.wld
 * Internal libtiff: update to CVS HEAD post libtiff 4.0.6
 * make SetColorInterpretation(GCI_AlphaBand) work on a 5 band or more GeoTIFF (#6102)
 * automatically set PHOTOMETRIC=RGB if manually assigning color interpretation Red,Green,Blue to band 1,2,3 before directory crystallization (#6272)
 * add GEOTIFF_KEYS_FLAVOR=ESRI_PE creation option to write EPSG:3857 in a ESRI compatible way (#5924)
 * call XTIFFInitialize() in LibgeotiffOneTimeInit() as the former isn't thread-safe, so better call it from the later which is thread-safe (#6163)
 * fix reading and writing angular units different from degree (namely arc-second, arc-minute, grad, gon and radian) (#6171)
 * do not use VirtualMemIO optimization on compressed /vsimem/ files (#6195)
 * correctly set GTRasterTypeGeoKey=RasterPixelIsPoint if AREA_OR_POINT=Point but there is no SRS set (#6225)
 * improve a bit error detection when writing
 * check free space before writing (only for big, non sparse, uncompressed)
 * do not read large 'one row' JBIG compressed files with the scanline API (#6264)
 * Fix SetMetadata() to properly clear existing PAM metadata (complement to #5807)
 * prevent potential out of bounds read/write to TIFFTAG_EXTRASAMPLES (#6282)
 * do not use first directory as potential mask, to avoid assertion in GTiffDataset::SetDirectory() (#6287)
 * reject files with strips/tiles/scanlines bigger than 2 GB to avoid 32 bit integer overflow.
   Also in case of files with Contig PlanarConfiguration do not make reading one block for band 2 OK when reading for band 1 issued an error (#6288)
 * GTIFFWriteDirectory(): avoid memory leak of codec related memory (#2055)
 * Make ALPHA=NO in CreateCopy() cancel alpha color interpretation even if present in source raster
 * fix problem with implicit overviews of JPEG-compressed files (#6308)
 * fix compilation problem with internal libtiff if DEFER_STRILE_LOAD isn't defined (which is not the default configuration) (https://github.com/OSGeo/gdal/pull/90)
 * use more appropriate error message when 4GB threshold is reached with external overviews, and try to make doc of BIGTIFF=IF_NEEDED/IF_SAFER clearer (#6353)

HDF4 driver:
 * Skip quotation mark when parsing HDF-EOS metadata.

HDF5 driver:
 * add Komsat Mission ID to possible value for HDF5 SAR product (https://github.com/OSGeo/gdal/pull/103)

HF2 driver:
 * fix reading side of the driver to work on architectures where char is unsigned, like PPC or ARM (#6082)

HFA driver:
 * when reading projection, preserve EPSG code if AutoIdentifyEPSG() identified the SRS, even if a PE string is present (#6079)
 * keep TOWGS84 even when using ESRI PE string (#6158)
 * fix crashes on corrupted files (#6208, #6286)

INGR driver:
 * check that RLE decoding produced the expected number of bytes and error out otherwise; test that 'random' line seeking actually works

JPEGLS driver:
 * fix build error (#6430)

JP2ECW driver:
 * honour psExtraArg->eResampleAlg when upsampling (#6022)

JP2KAK driver:
 * honour psExtraArg->eResampleAlg when upsampling (#6022)
 * try rounded dimensions to decide if the data is being requested exactly at a sub-resolution
 * support v7.7 on Unix (additional changes potentially needed on Windows)

JP2OpenJPEG driver:
 * Safer multi-threaded use

JPEG driver:
 * add USE_INTERNAL_OVERVIEWS open option (default to TRUE as in recent versions, can be set to FALSE to hide internal overviews

KMLSuperOverlay driver:
 * fix 2.0 regression with some RasterIO() requests involving resampling (#6311)
 * recognize datasets that have a intermediate <Folder> that forms a <Document><Folder><Region/><GroundOverlay/> structure (#6343)

LCP driver:
 * fix wrong use of endianness macros to fix behavior on big-endian hots

Leveller driver:
 * update to v9 read support (#5632,)

MBTiles driver:
 * add raster write support
 * fix so that datasets whose lowest min zoom level is 16 or above are recognized
 * be robust to invalid 'bounds' at dataset opening (#6458)

MEM driver:
 * avoid 32 bit overflows

NetCDF driver:
 * add support for reading NC4 unsigned short attributes and variables (#6337) * read correctly nodata values in [128,255] range for (unsigned) Byte data type (#6175)
 * implement Get/SetUnitType() using the standard units attribute (https://github.com/OSGeo/gdal/pull/96)
 * optimize IReadBlock() and CheckData() handling of partial blocks in the x axis by re-using the GDAL block buffer instead of allocating a new temporary buffer for each block (#5950)
 * full read/write support for new NetCDF4 types NC_UBYTE, NC_USHORT, NC_UINT and NC_STRING for variables (except for NC_STRING) and attributes (https://github.com/OSGeo/gdal/pull/99)
 * add support for the geostationary projection (#6030)
 * fix one byte heap write overflow in NCDFTokenizeArray() (#6231)
 * fix potential buffer overflows with uses of nc_inq_varname(), nc_inq_attname() and nc_get_att_text() (#6227)
 * validate that gridmapping:GeoTransform has 6 values (#6244)
 * fix wrong use of deallocator when writing a GEOLOCATION array, and other issues
 * limit number of bands reported to 32768 by default
 * validate raster dimensions
 * validate content of NC_GLOBAL#GDAL variable (#6245)

NGSGEOID driver:
 * make it work on > 2GB file

NITF driver:
 * data/nitf_spec.xml: Add CSCCGA, MENSRB, SENSRB, STREOB, ENGRDA, EXPLTB and PATCHB TREs (https://github.com/OSGeo/gdal/pull/81, #6285, https://github.com/OSGeo/gdal/pull/86)
 * fix parser to properly deal with variable length items not in first nesting level

Northwoord driver:
 * fix computation of intermediate color table values on non-Intel platforms (#6091)

NTv2 driver:
 * support reading/writing/appending to files with big-endian order (cf https://github.com/OSGeo/proj.4/issues/345)

OpenJPEG driver:
 * do not expose block dimensions larger than dataset dimensions to avoid wasting memory (#6233)

PCIDSK driver:
 * Remove the old driver (#6172)

PDF driver:
 * add PDFium library as a possible back-end. Initial support contributed by Klokan Technologies GmbH  (http://www.maptiler.com/)
 * workaround a bug of PoDoFo 0.9.0 by avoiding loading of vector content in raster-only mode (doesn't prevent the crash if reading the vector layers) (#6069)
 * make OGC BP registration work with media box where bottom_y is negative and top_y = 0 (in non rotated case)
 * make OGC BP registration work with media box where min_x != 0 (in non-rotated case)
 * correctly take into account non-meter linear units with OGC BP encoding, and add support for US FOOT (#6292)

PDS driver:
 * change default values of PDS_SampleProjOffset_Shift and PDS_LineProjOffset_Shift to 0.5 (#5941)
 * fix nodata value for UInt16 to be 0 (#6064)
 * accept 'ODL_VERSION_ID = ODL3' in header (#6279)

PGChip driver:
 * driver removed from sources

PLScenes driver:
 * PLScenes V0: avoid opening raster to generate dummy .aux.xml

PNG driver:
 * Support writing 1, 2 or 4 bit single band
 * Add NBITS creation option
 * fix XML of creation option list
 * Update internal libpng to 1.2.56

PostgisRaster driver:
 * avoid Identify() to recognize OGR PostgreSQL connection strings with schemas option and cause loud 'QuietDelete' (#6034)

Raw drivers:
 * better support for direct read of more than 2GB in single gulp (untested though)

RIK driver:
 * fix Identify() method to recognize again non-RIK3 RIK datasets (#6078)

RMF driver:
 * implement GetNoDataValue()
 * add read/write access to new RMF format for files larger than 4 Gb (version=0x201) (https://github.com/OSGeo/gdal/pull/11)

ROIPAC driver:
 * Support offset and scale band (#6189)

VICAR driver:
 * change PDS_SampleProjOffset_Shift and PDS_LineProjOffset_Shift default values to 0.5 (#5941)
 * fix loss of precision in scale and offset

VRT driver:
 * expose implicit 'virtual' overviews for VRT whose bands are made of a single SimpleSource/ComplexSource
 * gdalvrt.xsd: Add capitalized versions of true and false (#6014)
 * GetSingleSimpleSource(): check there's a single source (#6025)
 * honour VRTRasterBand NBITS metadata with SimpleSource and ComplexSource
 * properly take into account nodata value declared at VRT band level when doing resampling with non-nearest
 * honour relativeToVRT when using AddBand() to add a VRTRawRasterBand (https://github.com/OSGeo/gdal/pull/67)
 * VRT warp: fix crash with implicit overviews and destination alpha band (#6081)
 * make GetDefaultHistogram() on a sourced raster band save the result in the VRT (#6088)
 * serialize NODATA and NoDataValue items with %.16g, e.g. so as to be able to hold large int32 nodata values (#6151)
 * VRTSourcedRasterBand: make ComputeRasterMinMax() and ComputeStatistics() forward bApproxOK to overview band (useful for implicit overviews)
 * make CreateCopy() preserve NBITS metadata item
 * avoid loading sibling file list if not available
 * VRT raw: don't truncate last figure of ImageOffset if there are left space padding (#6290)
 * VRTWarpedDataset::SetMetadataItem(): fix crash when calling with name=SrcOvrLevel and value=NULL (#6397)
 * Warped VRT: fix deadlock in situation where warped VRT datasets are read in multiple threads and the block cache reaches saturation. Also add a GDAL_ENABLE_READ_WRITE_MUTEX config option that can be set to NO to disable the read/write mutex mechanism in cases where it would deadlock (#6400)

WMS driver:
 * add a IIP (Internet Imaging Protocol) minidriver
 * limit number of zoom levels for ArcGIS MapServer JSon (#6186)
 * determine a resolution that will not result in a number that is larger than the maximum size of an integer.  Any value that exceeds the maximum size of an integer will raise an invalid dataset dimensions error. (https://github.com/OSGeo/gdal/pull/89)

XYZ driver:
 * be more robust to not exactly equal X and Y spacing (#6461)

## OGR 2.1.0 - Overview of Changes

Core:
 * Add OGREditableLayer class to add editing capabilities to drivers with none or limited editing capabilities
 * OGRGeometry: add DelaunayTriangulation() method (GEOS >= 3.4)
 * OGRGeometry and derived classes: implement copy constructor and assignment operator (#5990)
 * OGRGeometry: Fix result of Equals on POINT EMPTY with POINT(0 0)
 * OGRFeature SetField(): more type conversions allowed, particularly with array types
 * OGRFeature::SetGeometry()/SetGeometryDirectly(): make it work when passed geometry is the currently installed geometry (#6312)
 * OGR SQL: do not silently skip NULL values in the first records when evaluating a SELECT DISTINCT (#6020)
 * OGR SQL: correctly sort NULL values in first positions (#6155)
 * OGR SQL: fix CAST(x AS bigint) to return an evaluated int64 node, and not int32 (#6479)
 * OGR SQL: handle 'fid' as Integer64 in where clause, and allow CAST(fid AS bigint) in selected columns (#6484)
 * Add OGRUpdateFieldType()
 * Decorate a few functions/methods of the OGR layer API with CPL_WARN_UNUSED_RESULT
 * WKT export: use 15 significant figures, instead of 15 figures after decimal point (#6145)
 * WKT export: do not append .0 after non-finite values (#6319)
 * Fix typo in definition of name of OGR_FD_ReorderFieldDefns (final s was missing)
 * OGRLayer::SetIgnoredFields(): properly reset state of non first geometry fields (#6283)
 * Make OGRLayer::SetSpatialFilter(GetSpatialFilter()) work with non empty spatial filter (#6284)
 * OGRLayerDecorator: add missing CreateGeomField()
 * OGRLayer::Erase(): do not discard input geometries that have no intersection with method layer (#6322)
 * OGRLayer::Erase(): Speedup = ~70%
 * Add OGRPreparedGeometryContains()
 * Use prepared geometry intersects as pretest in layer Intersection, Union, and Identity methods if requested.
   Use prepared geometry containment as pretest in layer Intersection method if requested.
 * Bail out from layer algebra methods if GEOS calls fail and not SKIP_FAILURES.
 * OGR_G_SetPoints(): error out if padfX or padfY == NULL, do not change coordinate dimension to 3D when pabyZ == NULL, fix optimization on linestring to call setPoints() only if the strides are the ones of a double, not 0 as incorrectly done before (#6344)
 * OGRParseDate(): more strict validation to reject invalid dates (#6452)

OGRSpatialReference:
 * Upgrade to EPSG database v8.8
 * Add support for SCH (Spherical Cross-track Height) projection
 * Optimize reprojection typically between WGS84 based SRS and WebMercator
 * Correctly transform Mercator_2SP and _1SP to ESRI Mercator, and back from ESRI Mercator to Mercator_2SP (#4861)
 * No longer enforce C locale if running against latest proj that is locale safe (4.9.2 or later)
 * EPSGGetPCSInfo(): use pcs.override.csv in priority over pcs.csv to read projection name, UOM, UOMAngle, GeogCS, etc... (#6026)
 * morphToESRI(): use GCS_WGS_1972 as GCS name for EPSG:4322 (#6027)
 * morphToESRI(): use Mercator_Auxiliary_Sphere projection for EPSG:3857. morphFromESRI(): map Mercator_Auxiliary_Sphere to EPSG:3857 (#5924)
 * Align hard-coded WKT of well known GCS definitions of WGS84, WGS72, NAD27 and NAD83 with the WKT of their EPSG def (#6080)
 * morphFromESRI(): special case with PROJCS name 'WGS_84_Pseudo_Mercator' (#6134)
 * OSR C API: fix declarations of OSRSetAxes() and OSRSetWagner(), and add missing OSRSetHOMAC(), OSRSetMercator2SP() and OSRSetTPED() (#6183)
 * Recognize EPSG 9835 method (Lambert Cylindrical Equal Area (Ellipsoidal)), needed for EPSG:6933 PCS for example
 * importFromProj4/exportToProj4(): rework linear unit conversion between WKT name/values and proj4 unit name, and extend its scope in WKT to proj4 conversions
 * OSR ESRI .prj: add support for reading custom ellipsoid in Parameters line
 * on import of +proj=geos, if +sweep=x is used then store it as a proj4 extension node (#6030)

Utilities:
 * ogrinfo / ogr2ogr: implement @filename syntax for -sql and -where
 * ogr2ogr: prevent the -gt setting from overriding transaction group size of 1 set by skipfailures earlier (#2409)
 * ogr2ogr: warn if -zfield field does not exist in source layer
 * ogr2ogr -skip: rollback dataset transaction in case of failure (#6328)
 * ogr2ogr: fix -append with a source dataset with a mix of existing and non existing layers in the target datasource (#6345)
 * ogr2ogr: imply quiet mode if /vsistdout/ is used as destination filename
 * ogr2ogr: make -dim and -nlt support measure geometry types

CartoDB:
 * fix GetNextFeature() on a newly create table (#6109)
 * defer 'CartoDBfycation' at layer closing
 * optimize feature insertion with multiple rows INSERT

CSV driver:
 * add editing capabilities of existing files
 * add X_POSSIBLE_NAMES, Y_POSSIBLE_NAMES, Z_POSSIBLE_NAMES, GEOM_POSSIBLE_NAMES and KEEP_GEOM_COLUMNS open options
 * add HEADERS open option to force OGR to handle numeric column names. (PR #63)
 * add EMPTY_STRING_AS_NULL=YES/NO open option
 * implement compatibility enhancements for GeoCSV specification (#5989)
 * fix detection of TAB delimiter in allCountries.csv when the first line has a comma (#6086)
 * fix issues with leading single quote, and missing first line after ResetReading(), when parsing allCountries.txt (#6087)
 * speed-up GetFeatureCount() on allCountries.txt
 * on CreateDataSource() with a .csv name, do not try to open other existing .csv files in the directory
 * make CreateGeomField() returns OGRERR_NONE in case of success instead of OGRERR_FAILURE (#6280)
 * avoid adding trailing comma in header line when writing 'WKT,a_single_field'

DGN driver:
 * add partial 3D transformation support for cell headers

DXF driver:
 * detect files without .dxf extension (#5994)
 * fix handling of ELLIPSE with Z extrusion axis = -1 (#5705)
 * take into account full definition of spline entity (degree, control points, weights and knots) when stroking splines (#6436)
 * better handling of various object coordinate systems found in dxf files for point, line, polyline, spline and ellipse entities. Add anchor position to text styles. Remove polygon/polyface mesh parsing from polyline entity (#6459)

ElasticSearch driver:
 * use get /_stats instead of /_status for ElasticSearch 2.0 compatibility (#6346)

FileGDB driver:
 * make CreateFeature() honour user set FID, and implement more fine grained transaction for Linux/Unix
 * give a hint of using FileGDB SDK 1.4 is FileGDB compression is used

GeoJSON driver:
 * Add editing capabilities of existing files
 * Add ARRAY_AS_STRING=YES open option
 * Use '%.17g' formatting by default for floating-point numbers and add SIGNIFICANT_FIGURES layer creation option (#6291)
 * add a json_ex_get_object_by_path() function
 * fix crash on null / non-json object features (#6166)
 * serialize string values that are valid JSon dictionary or array as it (ie do not quote them)
 * make sure there's enough space to write the FeatureCollection bbox (#6262). Also avoid duplicating FeatureCollection bbox if source has one (trunk only)
 * Export POINT EMPTY as having a null geometry, instead as of being POINT(0 0) (#6349)
 * Do not 'promote' a null field to OFTString type if it had another type before (#6351)

GME driver:
 * Driver removed.  Maps Engine being shut down at the end of January 2016.

GML driver:
 * VFR: add new attribute DatumVzniku (v1.6)
 * VFR: fix ST_UVOH type handling
 * VFR: fix ZpusobyOchrany attributes (data types and names)
 * VFR: fix CisloDomovni attributes (Integer->IntegerList)
 * VFR: fix TEA attributes of StavebniObjekty
 * add NAMESPACE_DECL=YES option to OGR_G_ExportToGMLEx() to add xmlns:gml=http://www.opengis.net/gml or http://www.opengis.net/gml/3.2 declaration; Also accept GML2 or GML32 as valid valiues for FORMAT option (#6214)
 * serialize in .gfs file the name of the geometry element when it is 'geometry' since this is a particular case (#6247)
 * fix logic error in BuildJointClassFromScannedSchema() (#6302)

GPKG driver:
 * make it accept files with non standard extension if they still have the correct application_id (#6396); also accept the .gpkx extension that may be used for extended geopackages
 * emit warning when generating a database without .gpkg/.gpkx extension (#6396)
 * as GPKG 1.1 uses a different application_id, emit a more specific warning if the application id starts with GPxx (but is not GP10). Add GPKG_WARN_UNRECOGNIZED_APPLICATION_ID config option to avoid the warning
 * correct scope of gpkg_geom_XXXXX extensions to be read-write, and allow reading geometry types CURVE or SURFACE
 * avoid trying to insert a gpkg_geom_XXXX extension if already done (#6402)
 * writer: implement strategy to flush partial_tiles temporary database when it becomes too big (#6462)
 * writer: when writing to GoogleMapsCompatible tiling scheme, better deal with source rasters in EPSG:4326 with latitude = +/-90 (#6463)
 * fix generation of files with 1-band paletted input dataset. Also improve the logic to retrieve color palette when forcing BAND_COUNT=1 on opening (#6450)

GPX driver:
 * fix crash when parsing a 'time' extension element at route/track level (2.0 regression, #6237)

ILI driver:
 * ILI1: Support for Surface polygon rings spread over multiple geometry records
 * ILI1: add string TID support (https://github.com/OSGeo/gdal/pull/91)
 * Fix crash with models using types derived from INTERLIS
 * Fix memory leaks (#6178)

JML driver:
 * remove arbitrary limitation preventing from reading geometries with <gml:coordinates> larger than 10 MB (#6338)

KML driver:
 * fix crash on KML files without content but with nested folders (#6486)

LIBKML driver:
 * for documents without folder, use document name when available as name of layer (#6409)

Memory driver:
 * add support for sparse feature IDs
 * add ADVERTIZE_UTF8 layer creation option

MITAB driver:
 * Add support for block sizes other than 512 bytes in .map files, for MapInfo 15.2 compatibility (#6298)
 * write correct datum id for EPSG:3857
 * read MID files with TAB delimiter and empty first field (#5405)
 * use projection code 29 when exporting non-Polar Lambert Azimuthal Equal Area (#5220)
 * fix crashes when parsing invalid MIF geometries (#6273)

MSSQLSpatial driver:
 * Implement MSSQL bulk insert (#4792)
 * do not treat a primary key that is not of integer type as the FID (#6235)

NTF driver:
 * fix potential buffer overflows when reading too short lines (#6277)

ODBC driver:
 * remove limitations to 500 columns

ODS driver:
 * fix loss of precision in formula computation

OpenFileGDB driver:
 * do not emit warning if SDC/CDF table detected and that FileGDB driver is present
 * fix min/max on columns without indices (#6150)
 * build correct geometry for a multi-part wkbMultiLineStringZ (#6332)
 * add support for reading SHPT_GENERALPOINT (#6478)

OSM driver:
 * correct fields ids for the (non frequently used) Node message
 * do not override 'our' osm_id (the node, way or relation id) with a tag named 'osm_id' (#6347)
 * properly deal with polygons in other_relations geometrycollection (#6475)

PG driver:
 * Add PRELUDE_STATEMENTS and CLOSING_STATEMENTS open option to be for example able to specify options, like statement_timeout, with pg_bouncer
 * Fix 2.0 regression when overwriting several existing PostGIS layers with ogr2ogr (#6018)
 * Update PG, PGDump and CartoDB drivers to correctly export POINT EMPTY for PostGIS 2.2
 * avoid resetting error potentially emitted by ExecuteSQL() (#6194)
 * sanitize management of quoting for FID column at layer creation
 * fix to get SRID on result layer with PostGIS 2.2
 * in copy mode (the default on layer creation), do not truncate the concatenated string list to the field width (#6356)
 * make such that GEOMETRY_NAME layer creation option is honoured in ogr2ogr when the source geometry field has a not-null constraint (#6366)
 * read and set DESCRIPTION metadata item from/into pg_description system table; add DESCRIPTION layer creation option
 * support int2[] and numeric[] types, better map float4[] type
 * remove code that was intended to handled binary cursors as it cannot be triggered
 * fix append of several layers in PG_USE_COPY mode and within transaction (ogr2ogr -append use case) (#6411)

PGDump driver:
 * fix issue with case of ogc_fid field in case the FID  layer creation option is not set by user or by ogr2ogr (related to #6232)
 * in copy mode (the default on layer creation), do not truncate the concatenated string list to the field width (#6356)
 * make such that GEOMETRY_NAME layer creation option is honoured in ogr2ogr when the source geometry field has a not-null constraint (#6366)
 * set DESCRIPTION metadata item from/into pg_description system table; add DESCRIPTION layer creation option

Shapefile driver:
 * accept opening standalone .dbf files whose header length is not a multiple of 32 bytes (#6035)
 * fix REPACK crash on shapefile without .dbf (#6274)
 * add capability to restore/build a missing .shx file when defining SHAPE_RESTORE_SHX to TRUE (#5035)
 * avoid CreateLayer() to error out when passed wkbUnknown | wkb25D (#6473)

SQLite/Spatialite driver:
 * support file:xxx URI syntax (derived from patch by joker99, #6150)
 * fix heuristics in OGRSQLiteSelectLayer::GetExtent() to not be used when there's a sub SELECT (#6062)
 * fix crash on GetLayerByName('non_existing_table(geom_column)') (#6103)
 * fix OGRSQLiteSelectLayerCommonBehaviour::GetBaseLayer() to no longer 'eat' consecutive characters in layer name (#6107)
 * Spatialite: turn debug messages warning about update not being supported because of missing or too old spatialite version as errors, and return NULL to the caller (#6199)
 * fix memleak in OGRSQLiteTableLayer destructor when updating geometry_columns_time
 * VFS: increase mxPathname to 2048 by default, and provide OGR_SQLITE_VFS_MAXPATHNAME config option to be able to configure that higher if that would be needed. Useful when dealing with very long names like /vsicurl/.... with AWS S3 security tokens
 * VFS: do not probe -wal files on /vsicurl/

SXF driver:
 * fix wrong use of endianness macros to fix behavior on big-endian hots
 * add recoding from CP1251 for TEXT attribute that is now decoded
 * fix various issues (#6357)

VRT driver:
 * implement CloseDependentDatasets()
 * fix editing with 'direct' geometry mode which could cause attribute column to be empty (#6289)
 * fix crash with a OGRVRTWarpedLayer using a source layer that would have non geometry column (unlikely to happen currently as this would require explicit disabling it, but more likey with following commit that createe VRT non-spatial layer implicitly when the source is non-spatial, linked to #6336)
 * avoid creating an implicit wkbUnknown geometry field when the source has no geometry column and there's no XML elements related to geometry fields (#6336)

XLSX driver:
 * fix reading sheets with more than > 26 columns and 'holes' (#6363)

XPlane driver:
 * extend ICAO identifiers to 5 digits (#6003)

## SWIG Language Bindings

All bindings:
 * add a options parameter to gdal.ReprojectImage() to pass warp options
 * Change ReadRaster and WriteRaster to use GIntBig and the *IOEx-methods
 * prevent NULL file pointer from being passed to VSIF*L functions
 * make gdal.Rename() accept Unicode strings
 * add SpatialReference.GetAxisName() and SpatialReference.GetAxisOrientation() (#6441)
 * add SpatialReference.GetAngularUnitsName() (#6445)

Java bindings:
 * Fix typemap for input parameter of type GIntBig (fixes GetFeature(long), DeleteFeature(long), etc...) (#6464)
 * Bump minimal java version to 1.5 in case SWIG generates anotations (#6433, patch by Bas Couwenberg)
 * GNUmakefile: add -f in rm commands
 * GNUmakefile: add support for all hardening buildflags

Perl bindings:
 * Fix #6050: string formatting in croak.
 * Perl Makefile.PL: add support for all hardening buildflags (#5998)
 * use strict and warnings in overridden constructors.
 * add $VERSION to ogr_perl.i (OGR.pm), which is required by pause.perl.org.
 * Add some basic module info for CPAN.
 * Bugfix for Geo::OGR::Feature->new().
 * Add many utility level algorithms as methods to various classes.
 * New class for XML stuff
 * New Makefile.PL, which can download and build GDAL. This allows automatic testing of the CPAN module.
 * Wrap VSIStdoutSetRedirection and allow creating datasets via an object, which can write and close.
 * Geo::OGR::Driver and Geo::OGR::DataSource are now Perl wrappers for respective GDAL classes.
 * Add to the error stack also errors from the bindings
 * Fix sending utf8 from Perl to GDAL. Should also remove some "uninitialized value" warnings.
 * ReadTile and WriteTile methods for Dataset, ReadTile accepts now tile size and scaling algorithm.
 * Improved Parent - Child management.
 * Improved support for 64bit ints.
 * Measures support in Geometry class.
 * Many new tests

Python bindings:
 * make Feature.ExportToJson() output boolean value for a boolean field
 * support floating point coordinates for the source windows of Band.ReadRaster() and Band.ReadAsArray()
 * fix build with SWIG 3.0.6 (#6045)
 * make gdal.OpenEx() throw a Python exception in case of failed open when exceptions are enables with gdal.UseExceptions() (#6075)
 * Disable opening a NumPy dataset with a filename returned by gdal_array.GetArrayFilename(() unless GDAL_ARRAY_OPEN_BY_FILENAME is set to TRUE
 * disable the warning about using deprecated wkb25DBit constant as it uses a trick that prevents the bindings from being used by py2exe / pyinstaller (#6364)

# GDAL/OGR 2.0 Release Notes

## In a nutshell...

 * New GDAL drivers:
    - BPG: read-only driver for Better Portable Graphics format (experimental, no build support)
    - GPKG: read/write/update capabilities in the unified raster/vector driver
    - KEA: read/write driver for KEA format
    - PLMosaic: read-only driver for Planet Labs Mosaics API
    - ROI_PAC: read/write driver for image formats of JPL's ROI_PAC project (#5776)
    - VICAR: read-only driver for VICAR format
 * New OGR drivers:
    - Cloudant: read/write driver for Cloudant service
    - CSW: read-only driver for OGC CSW (Catalog Service for the Web) protocol
    - JML: read/write driver for OpenJUMP .jml format
    - PLScenes: read-only driver for Planet Labs Scenes API
    - Selaphin: read/write driver for the Selaphin/Seraphin format (#5442)
 * Significantly improved drivers: CSV, GPKG, GTiff, JP2OpenJPEG, MapInfo file, PG, SQLite
 * RFC 31: OGR 64bit Integer Fields and FIDs (trac.osgeo.org/gdal/wiki/rfc31_ogr_64)
   In OGR core, OGR SQL, Shapefile, PG, PGDump, GeoJSON, CSV, GPKG, SQLite, MySQL,
   OCI, MEM, VRT, JML, GML, WFS, CartoDB, XLSX, ODS, MSSQLSpatial, OSM, LIBKML, MITAB
 * RFC 46: GDAL/OGR unification ( http://trac.osgeo.org/gdal/wiki/rfc46_gdal_ogr_unification)
     - GDAL and OGR PDF drivers are unified into a single one
     - GDAL and OGR PCIDSK drivers are unified into a single one
 * RFC 49: Add support for curve geometries (http://trac.osgeo.org/gdal/wiki/rfc49_curve_geometries)
   In OGR core, and GML, NAS, PostgreSQL, PGDUMP, GPKG, SQLite, VFK, VRT, Interlis drivers
 * RFC 50: Add support for OGR field subtypes (http://trac.osgeo.org/gdal/wiki/rfc50_ogr_field_subtype)
   In OGR core, OGR SQL, swig bindings, CSV, FileGDB, GeoJSON, GML, GPKG, OpenFileGDB, PG, PGDump, SQLite, VRT
 * RFC 51: RasterIO() improvements : resampling and progress callback (http://trac.osgeo.org/gdal/wiki/rfc51_rasterio_resampling_progress)
 * RFC 52: Stricter SQL quoting (http://trac.osgeo.org/gdal/wiki/rfc52_strict_sql_quoting)
 * RFC 53: OGR not-null constraints and default values (http://trac.osgeo.org/gdal/wiki/rfc53_ogr_notnull_default)
   In OGR core, OGR SQL, PG, PGDump, CartoDB, GPKG, SQLite, MySQL, OCI, VRT, GML, WFS, FileGDB, OpenFileGDB and MSSQLSpatial
 * RFC 54: Dataset transactions (https://trac.osgeo.org/gdal/wiki/rfc54_dataset_transactions)
   In PG, GPKG, SQLite, FileGDB and MSSQLSpatial
 * RFC 55: refined SetFeature() and DeleteFeature() semantics.
   In GPKG, Shape, MySQL, OCI, SQLite, FileGDB, PG, CartoDB, MITAB and MSSQL
 * RFC 56: OFTTime/OFTDateTime millisecond accuracy ( https://trac.osgeo.org/gdal/wiki/rfc56_millisecond_precision )
 * RFC 57: 64bit histogram bucket count ( https://trac.osgeo.org/gdal/wiki/rfc57_histogram_64bit_count )
 * Upgrade to EPSG v8.5 database
 * Fix locale related issues when formatting or reading floating point numbers (#5731)

## New installed files
 * data/gdalvrt.xsd: XML schema of the GDAL VRT format

## Backward compatibility issues

See MIGRATION_GUIDE.TXT

## GDAL/OGR 2.0 - General Changes

Build(Unix):
 * Fix for cpl_recode_iconv.cpp compilation error on freebsd 10 (#5452)
 * Fix pthread detection for Android
 * Fix in Armadillo detection test (#5455)
 * Fix detection of OCI by changing linking order to please modern GCC (#5550)
 * Fix test to accept MariaDB 10.X as valid MySQL (#5722)
 * Make sure $(GDAL_INCLUDE) is first to avoid being confused by GDAL headers of a previous version elsewhere in the include path (#5664)
 * Always use stat rather than stat64 for Mac OSX in AC_UNIX_STDIO_64. (#5780, #5414).
 * Add support for ECW SDK 5.1 (#5390)
 * Do not enable Python bindings if PYTHON env variable is set without --with-python being explicitly specified (#5956)

Build(Windows):
 * PDF: fix compilation issue with Visual Studio 2012 (#5744)
 * PDF: Add support to compile the pdf driver as plugin (#5813)
 * Add support for MrSID 9.1 SDK (#5814)
 * when building netCDF, HDF4, HDF5 as plugins, call registration of 'sub-drivers' GMT, HDF4Image and HDF5Image (#5802)

Build(all):
 * Ruby bindings: disable autoconf and makefile support (#5880)
 * Fix compilation errors with json-c 0.12 (#5449)
 * Fix compilation error in alg/gdalgrid.cpp when AVX is available, but not SSE (#5566)

## GDAL 2.0 - Overview of Changes

Port:
 * Introduce a more generic lock API (recursive mutex, adaptive mutex, spinlock)
 * Add types for CPLMutex, CPLCond and CPLJoinableThread (only enforced in -DDEBUG mode)
 * Add CPLGetPhysicalRAM() and CPLGetUsablePhysicalRAM()
 * CPLSpawn() on Windows: quote arguments with spaces in them (#5469)
 * /vsigzip/: avoid infinite loop when reading broken .gml.gz file (#5486)
 * /vsizip/ : fix bug that caused premature end of file condition with some read patterns (#5530)
 * /vsizip/ on >4GB zips: accept .zip declare 0 disks (#5615)
 * /vsitar/: remove useless validation test that prevents from opening valid .tar files (#5864)
 * /vsistdout/: add VSIStdoutSetRedirection() for compatibility with MapServer FCGI (https://github.com/mapserver/mapserver/pull/4858)
 * /vsimem/: update st_mtime and return it with Stat()
 * /vsimem/: in update mode, when seeking after end of file, only extend it if a write is done
 * /vsimem/: Make Rename() on a directory also rename filenames under that directory (#5934)
 * /vsicurl/: manage redirection from public URLs to redirected AWS S3 signed urls, with management of the expiration. Enabled by default. Can be disabled if CPL_VSIL_CURL_USE_S3_REDIRECT=NO (#6439)
 * /vsicurl/: avoid reading after end-of-file and fix failure when reading more than 16MB in a single time (#5786)
 * /vsicurl/: by default do not use HEAD request when detecting a AWS S3 signed URL
 * Allow CPL_VSIL_CURL_ALLOWED_EXTENSIONS to be set to special value {noext}
 * VSIWin32Handle::Flush(): no-op implementation is sufficient to offer same guarantee as POSIX fflush() (#5556)
 * Unix VSIL: reset eof in all cases in Seek()
 * Windows plugins: complementary fix to #5211 to avoid error dialog box when there are dependency problems (#5525)
 * Fix VSIReadDirRecursive() recursing on the parent or current directory (#5535)
 * cpl_error: obfuscate password
 * HTTP: set CURLOPT_NOSIGNAL if available (#5568)
 * Add COOKIE option to CPLHTTPFetch() (#5824)
 * CPLHTTPFetch(): add retry logic in case of 502, 503 and 504 errors with the GDAL_HTTP_MAX_RETRY (default: 0)and GDAL_HTTP_RETRY_DELAY (default: 30 s) config options (#5920)
 * Fix stack corruption upon thread termination with CPLSetThreadLocalConfigOption on Windows 32 bit (#5590)
 * cpl_csv: Stop probing for csv/horiz_cs.csv. (#5698)
 * vsipreload: implement clearerr() and readdir64() (#5742)
 * CPLsetlocale(): return a string that is thread-locale storage to avoid potential race in CPLLocaleC::CPLLocaleC() (#5747)
 * CPLHexToBinary(): faster implementation (#5812)
 * CPLAcquireMutex(): improve performance on Windows (#5986)

Core:
 * Add imagery (satellite or aerial) metadata support (Alos, DigitalGlobe, Eros, GeoEye, OrbView, Landsat, Pleiades, Resurs-DK1, Spot/Formosat).
 * Reduce lock contention on the global cache mutex and make it possible to use spin lock instead with GDAL_RB_LOCK_TYPE=SPIN
 * Block cache: make block cache manager safe with respect to writing dirty blocks (#5983)
 * EXIF reader: fix memleak in error code path
 * EXIF reader: add missing validation for some data types (#3078)
 * Fix crash in GDALPamRasterBand::SerializeToXML() when saving an empty RAT (#5451)
 * ComputeStatistics(): use Welford algorithm to avoid numerical precision issues when computing standard deviation (#5483)
 * Fix crashing issue with TLS finalization on Unix (#5509)
 * GDALJP2Metadata::CreateGMLJP2(): use EPSGTreatsAsLatLong() and EPSGTreatsAsNorthingEasting() to determine if axis swapping is needed (#2131)
 * GDALJP2AbstractDataset: implement GetFileList() to report .wld/.j2w if used
 * GMLJP2: be robust when parsing GMLJP2 content that has nul character instead of \n (#5760)
 * GMLJP2: add missing rangeParameters element to validate against GMLJP2 schema (#5707)
 * GMLJP2: write non null bounding box at root of FeatureCollection (#5697)
 * GMLJP2: SRS export as GML: output XML definition of a SRS as a GML 3.1.1 compliant Dictionary (#5697)
 * GMLJP2: when setting GDAL_JP2K_ALT_OFFSETVECTOR_ORDER=TRUE write it as a XML comment so that we can interpret the OffsetVector elements correctly on reading
 * GMLJP2: when parsing a GMLJP2 box, accept srsName found on gml:RectifiedGrid if not found on origin.Point, so as to be compatible with the example of DGIWG_Profile_of_JPEG2000_for_Georeferenced_Imagery.pdf (#5697)
 * GMLJP2: add compatibility with GMLJP2 v2.0 where SRS is expressed as CRS URL
 * GMLJP2: on reading, don't do axis inversation if there's an explicit axisName requesting easting, northing order (#5960); also strip axis order in reported SRS
 * JP2Boxes: add null terminated byte to GDAL XML, XML or XMP boxes
 * Add GDALGetJPEG2000Structure() (#5697)
 * GDALMultiDomainMetadata::XMLInit(): when importing XML metadata, erase the existing document to replace it with the new one
 * Metadata: fix correct sorting of StringList / metadata (#5540, #5557)
 * Make GetMaskBand() work with GDT_UInt16 alpha bands (#5692)
 * Fix 32bit overflow in GDALRasterBand::IRasterIO() and  GDALDataset::BlockBasedRasterIO() (#5713)
 * RasterIO: small optimization in generic RasterIO() implementation to avoid loading partial tiles at right and/or bottom edges of the raster when they are going to be completely written
 * Fix crash when calling GetTiledVirtualMem() on non-Linux platform (#5728)
 * Add GDAL_OF_INTERNAL flag to avoid dataset to be registered in the global list of open datasets
 * GDALDriver::CreateCopy(): accept _INTERNAL_DATASET=YES as creation option, so as to avoid the returned dataset to be registered in the global list of open datasets
 * Implement GDALColorTable::IsSame()
 * GDALPamDataset: do not serialize dataset metadata unless it has been set through GDALDataset::SetMetadata() or GDALDataset::SetMetadataItem()
 * GDALLoadTabFile: add TAB_APPROX_GEOTRANSFORM=YES/NO configuration option to decide if an approximate geotransform is OK (#5809)
 * Optimize copy efficiency from tiled JPEG2000 images
 * Avoid fetching remote non-existing resources for sidecar files, when using /vsicurl/ with a URL that takes arguments (#5923)
 * Use GDALCanFileAcceptSidecarFile() in GDALMDReaderManager::GetReader()

Algorithms:
 * RPC transformer: fix near interpolation in RPC DEM (#5553)
 * RPC transformer: take into account nodata in RPC DEM (#5680)
 * RPC transformer: add RPC_DEM_MISSING_VALUE transformer option to avoid failure when there's no DEM at the transformed point (#5730)
 * RPC transformer: in DEM mode, implement optimization, in specific conditions (input points at same longitude, DEM in EPSG:4326) to extract several elevations at a time
 * TPS transformer: fix crash if the forward or backward transform cannot be computed (#5588)
 * OpenCL warper: remove unused variable in bilinear resampling that can cause compilation error (#5518)
 * OpenCL wrapper: fix code compilation with NVIDIA OpenCL (#5772)
 * Overview: Fix and speed-up cubic resampling in overview computation to take into account scaling factor (#5685)
 * Overview: ignore alpha=0 values when compute an average overview of an alpha band; and also avoid memory errors when calling GetMaskBand()/GetMaskFlags() after overview computation if GetMaskXXX() has been called before (#5640)
 * Overview: avoid crash when computing overview with a X dimension much smaller than Y dimension (#5794)
 * GDALRegenerateOverviewsMultiBand(): fix stride calculation error with certain raster dimensions (#5653)
 * Warper: numerous speed optimizations (SSE2 specific code, more fast code paths, ...)
 * Warper: fix Cubic and Bilinear resampling to work correctly with downsizing (#3740)
 * Warper: fix and optimize CubicSpline
 * Warper: regardless of the warping memory limit, add heuristics to determine if we must split the target window in case the 'fill ratio' of the source dataset is too low (#3120)
 * Warper: accept warping options METHOD=NO_GEOTRANSFORM and DST_METHOD=NO_GEOTRANSFORM to run gdalwarp on ungeoreferenced images
 * Warper: fix GDALSuggestedWarpOutput() wrong extent in some circumstances (e.g. dataset of big dimension with world coordinates) (#5693)
 * Warper: fix integer overflow when reprojecting into an area with (part of) bounds completely outside of the source projection (#5789)
 * Warper: add min,max,med,q1 and q3 resampling algorithms (#5868)
 * Warper: add a SRC_COORD_PRECISION warping option to help getting more reproducible output when -wm parameter changes (#5925)
 * Warper: fix failure in GDALSuggestedWarpOut2() when top-left and bottom-right corners transform to the same point (#5980)
 * GDALReprojectImage(): takes into account nodata values set on destination dataset
 * Median cut and dithering: optimizations and enhancements to deal with 8-bit precision (only if using internal interface for now)
 * rasterfill: add option to specify driver to use for temporary files
 * Polygonize: speed optimization: do not try to build the polygon for pixels that are masked by the mask band (i.e. alpha, nodata, etc...). Can considerably speed-up processing when the nodata outline forms a very complex polygon

Utilities:
 * gdalinfo: display extra metadata domains attached to band, and refactor code a bit (#5542)
 * gdalinfo: add -oo option per RFC 46
 * gdalinfo: add -json switch (partial implementation of RFC 44)
 * gdaladdo: add -oo option per RFC 46
 * gdaladdo: add warning when subsampling factor 1 specified
 * gdal_translate: add -oo option per RFC 46
 * gdal_translate: add -r and -tr options per RFC 51
 * gdal_translate: add a -projwin_srs option to be able to express -projwin coordinates in another SRS than the one of the dataset
 * gdal_translate: support -'outsize avalue 0' or '-outsize 0 avalue' to preserve aspect ratio
 * gdal_translate: avoid preserving statistics when changing data type in situations where clamping can occur
 * gdal_translate: adjust RPC metadata (pixel/line offset/scale) when subsetting/rescaling, instead of just discarding it
 * gdal_translate: don't recopy band units if rescaling or unscaling is involved (#3085)
 * gdal_translate: increase GDAL_MAX_DATASET_POOL_SIZE default value to 450. (#5828)
 * gdal_translate: preserve NBITS image structure metadata when possible
 * gdalwarp: add -oo option per RFC 46
 * gdalwarp: add -te_srs option to specify -te in a SRS which isn't the target SRS
 * gdalwarp: add a -ovr option to select which overview level to use, and default to AUTO. Also add a generic OVERVIEW_LEVEL=level open option, and make it available in standard VRT (#5688)
 * gdalwarp: initialize destination dataset to no_data value when automatically propagating source nodata (#5675)
 * gdalwarp: only apply INIT_DEST when processing the first input dataset (#5387)
 * gdalwarp: increase GDAL_MAX_DATASET_POOL_SIZE default value to 450. (#5828)
 * gdalwarp: do not preserve NODATA_VALUES metadata item in output dataset if adding an alpha channel with -dstalpha
 * gdalwarp: fix '-dstnodata none' to avoid read of uninitialized values (#5915)
 * gdalwarp: make -crop_to_cutline densify cutline in source SRS before reprojecting it to target SRS (#5951)
 * gdaldem: avoid too large files to be produced when using -co COMPRESS=xxxx -co TILED=YES (#5678)
 * gdallocationinfo: add -oo option
 * gdaltransform: add a -output_xy flag to restrict output coordinates to 'x y' only
 * gdal_grid: use nodata= parameter in the algorithm string to determine the nodata value to set on the band (#5605)
 * gdal_grid: fix crash in optimized mode with GCC 4.4 on 64bit (#5987)
 * gdalbuildvrt: add a -r option to specify the resampling algorithm
 * gdal_edit.py: add -unsetstats option (and fix -a_nodata to run on all bands, ant not just first one)
 * gdal_edit.py: add -stats and -approx_stats flags (patch by mwtoews, #5805)
 * gdal_edit.py: change -mo add metadata to existing one; add new option -unsetmd to clean existing metadata
 * gdal_edit.py: add -oo to specify open options
 * gdal_retile.py: fix to make it work with input images of different resolutions (#5749)
 * gdal_retile.py: implement progress bar (#5750)
 * gdal_merge.py: add timing information in verbose output
 * gdal_merge.py: take into account alpha band to avoid writing zones of source images that are fully transparent (#3669)
 * gdal2tiles.py: fix inverted long/lat in BoundingBox and Origin elements of tilemapresource.xml (#5336)
 * pct2rgb.py: make it work with color tables with less than 256 entries (#5555)
 * gdal_fillnodata.py: FillNodata: copy no data value to destination band when creating a dataset (if available) (#4625)
 * gdal_proximity.py: add a -use_input_nodata flag
 * gdalcompare.py: add options to suppress selected comparisons
 * gdalcompare.py: takes into account differences in overview bands
 * gdalcompare.py: compute difference on float to avoid integer underflow
 * epsg_tr.py: change to make it possible to export GEOCCS and COMPD_CS to proj.4 epsg and PostGIS spatial_ref_sys.sql files

Python samples:
 * Added swig/python/samples/jpeg_in_tiff_extract.py
 * Added dump_jp2.py
 * Added validate_jp2.py
 * Added build_jp2.py
 * Added gcps2ogr.py
 * tolatlong.py: report error when operating on a non-georeferenced dataset

AAIGRID:
 * Fix formatting string (#5731)

BAG driver:
 * change nodata value for uncertainty band to 1e6 (#5482)

BMP driver:
 * back out r17065 change that inferred georeferencing based on the resolution information in the BMP header (#3578)

DIMAP driver:
 * DIMAP 2: handle the case where the Raster_Data element is in main file (#5018, #4826)
 * DIMAP 2: fix to extract geodetic SRS (#5018, #4826)
 * DIMAP 2: fix to extract geotransform from JPEG2000 file if not available in XML (#5018, #4826)

DDS driver:
 * Add ETC1 compression format support
 * Header correction for worldwind client

ECW driver:
 * correctly assign color interpretation to bands if order is unusual

ENVI driver:
 * avoid generating potentially corrupted .hdr files when opening in update mode; Write 'Arbitrary' instead of 'Unknown' as the projection name for an undefined SRS (#5467)
 * when writing, consider that LOCAL_CS SRS is like ungeoreferenced (#5467)

ERS driver:
 * reset RasterInfo.RegistrationCellX/Y if setting a new geotransform on an updated .ers file (#5493)
 * fix SetProjection() (#5840)

GeoRaster driver:
 * fix Oracle SRID authority (#5607)
 * fix user-defined SRID issue (#5881)
 * new SRID search (#5911)

GIF driver:
 * add compatibility with giflib 5.1 (#5519)
 * fix crash on images without color table (#5792)
 * fix reading of interlaced images with giflib >= 5.0
 * validate the size of the graphic control extension block (#5793)
 * implement GetFileList() to report worldfile

GRASS driver:
 * GRASS 7.0.0 support (#5852)

GRIB driver:
 * avoid divide by zero while setting geotransform on 1xn or nx1 grib file (#5532)
 * allow writing PDS template numbers to all bands (#5144)

GTiff driver:
New capabilities:
 * for JPEG-in-TIFF, use JPEG capabilities to decompress fast overview levels 2,4 and 8, to generate 'hidden' overviews used by RasterIO()
 * add DISCARD_LSB creation option (lossy compression) to be best used with PREDICTOR=2 and LZW/DEFLATE compression
 * when GTIFF_DIRECT_IO=YES is enabled, performance improvements in GTiffRasterBand::DirectIO() with Byte dataset and Byte buffer
 * implement dataset DirectIO()
 * add GTIFF_VIRTUAL_MEM_IO=YES/NO/IF_ENOUGH_RAM configuration option so that RasterIO() can rely on memory-mapped file I/O (when possible and supported by the OS)
 * support reading and created streamable files
 * for JPEG-compressed TIFF, avoid quantization tables to be emitted in each strip/tile and use optimized Huffman coding by default
 * avoid SetNoDataValue() to immediately 'crystallize' the IFD
 * allow unsetting TIFFTAG_SOFTWARE, TIFFTAG_DOCUMENTNAME, etc... by removing them from metadata list or passing None as a value of SetMetadataItem() (#5619)
 * allow lossless copying of CMYK JPEG into JPEG-in-TIFF
 * set alpha on target by default when translating from Grey+Alpha
 * Internal overviews: for near, average, gauss and cubic, and pixel interleaving, make sure to use the same code path for compressed vs uncompressed overviews (#5701)
 * add RPCTXT=YES creation option to write sidecar _RPC.TXT file
 * internal libtiff updated to upstream libtiff 4.0.4beta
 * internal libgeotiff updated to upstream libgeotiff SVN head
 * speed optimization on write (at least in Vagrant) (#5914)
 * use importFromEPSG() when ProjectedCSTypeGeoKey is available (#5926)
 * on reading better deal with a few ESRI formulations of WebMercator (#5924)
Fixes:
 * when overriding metadata in update mode, make sure to clear it from PAM file (#5807)
 * fix handling of Mercator_2SP (#5791)
 * avoid TIFF directory to be written (at end of file) when creating a JPEG-in-TIFF file
 * for a paletted TIFF with nodata, set the alpha component of the color entry that matches the nodata value to 0, so as gdal_translate -expand rgba works properly
 * fix to make band SetMetadata(NULL) clear band metadata (#5628)
 * fix error message when requesting a non existing directory
 * check data type with PHOTOMETRIC=PALETTE
 * be robust to out-of-memory conditions with SplitBand and SplitBitmapBand
 * avoid using optimized JPEG --> JPEG-in-TIFF path if INTERLEAVE=BAND is specified with a 3-band JPEG
 * to make Python bindings happy, avoid emitting CE_Failure errors due to libtiff errors when we still manage to open the file (#5616)
 * avoid crash when reading GeoTIFF keys if the stored key type isn't the one expected
 * make sure to call libgeotiff gtSetCSVFilenameHook() method when linking against external libgeotiff
 * avoid/limit DoS with huge number of directories
 * clean spurious spaces when reading values from _RPC.TXT
 * serialize RPC in PAM .aux.xml file if using PROFILE != GDALGeoTIFF and RPB = NO
 * fix clearing of GCPs (#5945)
 * avoid generated corrupted right-most and bottom-most tiles for 12-bit JPEG-compressed (#5971)
 * make sure to use scanline write API when writing single-band single-strip 1-bit datasets

GRASS driver:
 * fix compilation issues against GRASS 7

HDF4 driver:
 * Add configuration support to be able to open more files simultaneously
 * Add class suffix to the parameter name when parsing HDF-EOS objects.
 * Fix AnyTypeToDouble() to use proper type (int instead of long) to work with DFNT_INT32/DFNT_UINT32 on 64-bit Linux (#5965)
 * MODIS: Set more correct values for PIXEL_/LINE_ OFFSET/STEP by comparing longitude and latitude subdatasets dimensions with main subdataset dimensions

HDF5 driver:
 * avoid opening BAG files in the case HDF5 and BAG are plugins, and HDF5 is registered before BAG

HFA driver:
 * fix recognition of Hotine Mercator Azimuth Center in Imagine format (and Swisstopo GeoTIFF) (#5551)
 * fix various hangs on invalid files
 * read projection even when it does not contain datum information (#4659)

HTTP driver/wrapper:
  * make it work with vector files too
  * fix handling of non VSI*L file on Windows

IRIS driver:
 * add support for the SHEAR data type (#5549)

JPEG2000 driver:
 * do expansion of 1-bit alpha channel to 8-bit by default. Can be controlled with the 1BIT_ALPHA_PROMOTION open option (default to YES)
 * add GMLJP2 creation option
 * add GMLJP2V2_DEF creation option to create a GMLJP2 v2 box
 * Add capability of reading GMLJP2 v2 embedded feature collections and annotations
 * Add read/write support for RPC in GeoJP2 box (#5948)

JP2ECW driver:
 * add metadata about JPEG2000 codestream and boxes (#5408)
 * Add 1BIT_ALPHA_PROMOTION open option (default to YES) to control expansion of 1-bit alpha channel to 8-bit
 * correctly assign color interpretation to bands if order is unusual
 * add WRITE_METADATA and MAIN_MD_DOMAIN_ONLY creation options to write GDAL metadata, JP2 XML boxes or XMP box
 * add GMLJP2V2_DEF creation option to create a GMLJP2 v2 box
 * Add capability of reading GMLJP2 v2 embedded feature collections and annotations
 * Add read/write support for RPC in GeoJP2 box (#5948)

JP2KAK driver:
 * Compatibility with Kakadu v7.5 (#4575, #5344)
 * Handle Kakadu version 7 allocator.finalize (#4575)
 * fix bug in vsil_target::end_rewrite() that prevented TLM index to be generated (#5585)
 * Add FLUSH in creation option XML (#5646)
 * Do expansion of 1-bit alpha channel to 8-bit by default. Can be controlled with the 1BIT_ALPHA_PROMOTION open option (default to YES)
 * add GMLJP2V2_DEF creation option to create a GMLJP2 v2 box
 * Add capability of reading GMLJP2 v2 embedded feature collections and annotations
 * Add read/write support for RPC in GeoJP2 box (#5948)

JP2OpenJPEG driver:
 * Support writing arbitrary number of bands. (#5697)
 * Generate cdef box when transparency is needed. Add NBITS, 1BIT_ALPHA and ALPHA creation options (#5697)
 * add INSPIRE_TG (for conformance with Inspire Technical Guidelines on Orthoimagery), PROFILE, JPX and GEOBOXES_AFTER_JP2C creation option (#5697)
 * add GMLJP2V2_DEF creation option to create a GMLJP2 v2 box
 * Add capability of reading GMLJP2 v2 embedded feature collections and annotations
 * add PRECINCTS creation option (#5697)
 * allow several quality values to be specified with QUALITY creation option. Add TILEPARTS, CODEBLOCK_WIDTH and CODEBLOCK_HEIGHT options (#5697)
 * support reading&writing datasets with unusual order of band color interpretation (#5697)
 * add WRITE_METADATA and MAIN_MD_DOMAIN_ONLY creation options to write GDAL metadata, JP2 XML boxes or XMP box (#5697)
 * add support for reading/writing/updating IPR box (from/into xml:IPR metadata domain) (#5697)
 * add YCC creation option to do RGB->YCC MCT, and turn it ON by default (#5634)
 * add USE_SRC_CODESTREAM=YES experimental creation option, to reuse the codestream of the source dataset unmodified
 * support reading & writing images with a color table (#5697)
 * support update mode for editing metadata and georeferencing (#5697)
 * add compatibility with OpenJPEG 2.1 (#5579)
 * fix warning when reading a single tile image whose dimensions are not a multiple of 1024 (#5480)
 * Add 1BIT_ALPHA_PROMOTION open option (default to YES) to control expansion of 1-bit alpha channel to 8-bit
 * Add read/write support for RPC in GeoJP2 box (#5948)
 * limit number of file descriptors opened

JPEG driver:
 * use EXIF overviews if available
 * add EXIF_THUMBNAIL creation option to generate an EXIF thumbnail
 * use optimized Huffman coding to reduce file size
 * add support for reading and writing COMMENT
 * optimize whole image reading with dataset IRasterIO()
 * report non-fatal libjpeg errors as CE_Warning (or CE_Failure if GDAL_ERROR_ON_LIBJPEG_WARNING = TRUE) (#5667)
 * in compressor, increase default val of max_memory_to_use to 500MB

JPIPKAK driver:
 * avoid symbol collision with kdu_cpl_error_message from JP2KAK driver
 * reset the bNeedReinitialize flag after a timeout (#3626)

KMLSuperOverlay driver:
 * fix truncated raster on 32 bit builds (#5683)
 * make Identify() more restrictive to avoid false positives

L1B driver:
 * add support for little-endian LRPT datasets (#5645)
 * expose band mask when there are missing scanlines (#5645)
 * expose WGS-84 or GRS-80 datum if read from header record (#5645)

MAP driver:
 * retrieve the image filename in a case insensitive way (#5593)

MBTiles driver:
 * better detection of 4 bands dataset and take into account alpha component of color table in RasterIO() (#5439)
 * avoid wrong detection of 3 bands when finding paletted PNG in /vsicurl mode (#5439)
 * fix dimension computation when opening a single tile dataset
 * better computation of extent from min/max of tile coordinates, for a single zoom level
 * use standard EPSG:3857 origin to fix a ~8m shift (#5785)

MEM driver:
 * implement optimized versions of raster band and dataset IRasterIO()

MSG driver:
 * fix compilation problem (#5479)
 * fix memory leaks (#5541)

NetCDF driver:
 * Force block size to 1 scanline for bottom-up datasets if nBlockYSize != 1 (#5291)
 * Fix computation of inverse flattening (#5858)
 * In case the netCDF driver is registered before the GMT driver, avoid opening GMT files
 * Fix crash on opening a NOAA dataset (#5962)

NGSGEOID driver:
 * make Identify() more restrictive

NITF driver:
 * deal correctly with JPEG2000 NITF datasets that have a color table inboth Image Subheader and JP2 boxes, and for drivers that don't do color table expension
 * HISTOA TRE: put definition of TRE in conformance with STDI-0002 (App L page 14) and STDI-0006 (Page 57) (#5572)

OGDI driver:
 * Remove OGDIDataset::GetInternalHandle (#5779).

OZI driver:
 * remove .map header detection from Identify() since this is actually handled by the MAP driver

PCIDSK driver:
 * close dataset in case of exception in PCIDSK2Dataset::LLOpen() (#5729)

PCRaster driver:
 * Align libcsf code with PCRaster raster format code (#5843)
 * Implement Create() (#5844)
 * Improve handling of no-data value (#5953)

PDF driver:
 * add compatibility with Poppler 0.31.0
 * in the OGC Best practice case, switch rotational terms of the geotransform matrix (gt[2] and gt[4])
 * in the OGC Best practice case, handle rotations of 90 and 270 degrees.
 * advertise LAYERS metadata domain
 * deal with OHA- datum (Old Hawaiian)
 * fix compilation problem with Podofo on Windows (patch by keosak, #5469)
 * add sanity check on page count

PDS driver:
 * Added support for SPECTRAL_QUBE objects used e.g. by THEMIS instrument of Mars Odyssey spacecraft.

PNG driver:
 * add creation options to write metadata in TEXT/iTXt chunks
 * optimize whole image reading with dataset IRasterIO()
 * Internal libpng: update to 1.2.52

PostgisRaster driver:
 * Fix read of metadata for tables with multiple raster cols (#5529)

Rasterlite driver:
 * accept space in filename

RPFTOC driver:
 * add tweak for weird relative directory names in the A.TOC file (#5979)

VRT driver:
 * add handling of a shared='0' attribute on <SourceFilename> to open sources in non-shared mode, and VRT_SHARED_SOURCE config option that can be set to 0 in case the shared attribute isn't there (#5992)
 * VRT warp: make selection of source overview work (#5688)
 * VRT warp: expose as many overviews in warped dataset as there are in source dataset, and make warped VRT honour -ovr parameter of gdalwarp (#5688)
 * make sure nodata value set on VRT raster band is taken into account in statistics computation (#5463)
 * fix ComputeStatistics() on VRT that are a sub-window of source dataset (#5468)
 * VRT raw: fix corrupted serialization on Windows (#5531)
 * implement heuristics to determine if GetMinimum()/GetMaximum() should use the implementation of their sources of not. Can be overridden by setting VRT_MIN_MAX_FROM_SOURCES = YES/NO (#5444)
 * VRT warp: avoid to warp truncated blocks at right/bottom edges, so that scale computation is correct
 * fix RasterIO() to be able to fill buffers larger than 2GB (#5700)
 * fix performance problem when serializing into XML a big number of sources
 * do not output empty <Metadata> node on VRTDataset and VRTRasterBand elements
 * fix rounding of output window size on VRTSimpleSource (#5874)
 * add trick to make relativeToVRT works for a VRT-in-VRT
 * add more checks to CheckCompatibleForDatasetIO() to avoid issues with overview bands (#5954)
 * preserves relative links on reserialization of existing VRT (#5985)

USGSDEM driver:
 * take into account horizontal unit = ft in the UTM case (#5819)

TIL driver:
 * fix half pixel shift in geo registration (#5961)

WEBP driver:
 * Lazy uncompressed buffer allocation and optimize band-interleaved IRasterIO() for whole image reading

WMS driver:
 * Add support for ArcGIS server REST API
 * fix to make GDAL_DEFAULT_WMS_CACHE_PATH configuration option work as expected (#4540)
 * move the WMS layer name encoding to be done before the sub datasets URLs are created.

XYZ driver:
 * fix back line seeking with datasets that have not the same number of values per lines (#5488)
 * deal with lines that have missing values (but still regularly spaced)

## OGR 2.0 - Overview of Changes

Core:
 * OGRPolygon::importFromWkt(): fix memleak when importing broken 2.5D polygon
 * Fix OGRFeature::SetGeometryDirectly() and SetGeomFieldDirectly() to free the passed geometry even if the method fails (#5623)
 * OGR SQL: Add hstore_get_value(hstore, key) function
 * OGR SQL: sanitize how we deal with field names expressed as table_name.field_name and "fieldname.with_point_inside". By default, use standard quoting rules, and be tolerant when there's no ambiguity
 * OGR SQL: support arbitrary boolean expression on ON clause of a JOIN
 * OGR SQL: accept AS keyword in 'FROM table_name AS alias' clause
 * OGR SQL: don't consider backslash-doublequote as an escape sequence when inside a single-quoted string literal
 * Add OGR_API_SPY mechanism (http://www.gdal.org/ograpispy_8h.html)
 * Make OGRParseDate() recognize ISO 8601 format
 * ogr_core.h: only ignore -Wfloat-equal for IsInit() and not for the rest of the file and files that include it (#5299)
 * OGR layer algebra: properly initialize field maps to avoid Valgrind warnings in OGRLayer::Update() (#5778)
 * Make OGR_F_SetFieldBinary() set OFTString fields, mostly for testing purposes
 * OGR_G_CreateGeometryFromJson(): attach a WGS84 SRS to the returned geometry if the json object has no 'crs' member (#5922)

OGRSpatialReference:
 * Upgrade to EPSG v8.5 database
 * Proj.4 import: for HOM, make sure +no_off/no_uoff is preserved, and change default value of gamma parameter to be the same as alpha (#5511)
 * Proj.4 export: export Aitoff, Winkel I, Winkel II, Winkel-Tripel, Craster, Loximuthal, Quartic Authalic
 * Adding support for Mercator_Auxiliary_Sphere without AUTHORITY SECTIONS (#3962)
 * Add QSC (Quadrilateralized_Spherical_Cube) projection, compatible with PROJ 4.9
 * Various fixes to put EXTENSION node before AUTHORITY and make it pass Validate() (#5724)
 * importFromEPSG()/exportToProj4(): avoid precision loss in TOWGS84 parameters, e.g. on Amersfoort / RD EPSG:4289 (https://trac.osgeo.org/proj/ticket/260)
 * Add OSRCalcInvFlattening() and OSRCalcSemiMinorFromInvFlattening(), and use them in various places (#5858)
 * Remove deprecated variant of OGRSpatialReference::importFromOzi() (#5932)

Utilities:
 * ogrinfo: add -oo option per RFC 46
 * ogrinfo: display dataset and layer metadata. Add -nomdd, -listmdd, -mdd all|domain options, like in gdalinfo. OGR VRT: add dataset and layer metadata support
 * ogrinfo: add -nocount and -noextent options
 * ogr2ogr: add -oo and -doo options per RFC 46
 * ogr2ogr: add -spat_srs option
 * ogr2ogr: turn string value to one element list if destination field is stringlist
 * ogr2ogr: fix problem with SRS when copying layers with multiple geometry columns with different SRS (#5546)
 * ogr2ogr: add special case for -update and GPKG and input=output
 * ogr2ogr: when copying a layer that has a source integer/integer64 field with same name as target FID column, avoid creating it into target layer and use its content as the FID value (#5845)
 * ogr2ogr: in non-append mode, if target layer has a FID creation option, reuse source FID name and source FIDs, unless -unsetFid is specified (#5845)
 * ogr2ogr: copy source dataset and layer metadata, unless disabled by -nomd. Additional dataset metadata can be set with -mo KEY=VALUE
 * ogr2ogr: add -ds_transaction to force dataset transactions, mainly for FileGDB driver
 * ogr2ogr: fix crash with -clipdst when a reprojection fails before (#5973)
 * ogrlineref: fix project if reper lies on first point or last point of line
 * ogr_layer_algebra.py: for Update, Clip and Erase, only creates attribute of input layer by default (#5976)

Other:
 * OGR WCTS removed from tree

Cross driver changes:
 * MSSQLSpatial and GPKG: use standardized 'GEOMETRY_NAME' option name. Add GEOMETRY_NAME to SQLite (#5816)
 * FileGDB and MySQL: use standardized 'FID' option name. SQLite: add a FID layer creation option (#5816)
 * SQLite, GPKG, PG, PGDump: in a newly created table, allow to create a integer field with same name of FID column (#5845)

BNA driver:
 * fix segfault when calling GetNextFeature() on a write-only layer

CartoDB:
 * add CARTODBFY layer creation option
 * launder layer and column names by default (#5904)
 * enable by default batch insertion of features in update mode
 * on a newly created layer, send new features created by CreateFeature() by chunks of a maximum size of 15 MB (configurable through CARTODB_MAX_CHUNK_SIZE).
 * implement deferred field creation
 * support boolean type
 * register tables with cdb_cartodbfytable()
 * fix creation of features with Date/DateTime/Time values
 * fix for multi-user account, and optimization for SQL layers
 * implement TestCapability() and CreateDataSource() similarly to PostgreSQL, i.e. redirect to Open() in update mode
 * accept a user column to have the same name of the FID (cartodb_id)
 * do automatic polygon->multipolygon promotion at creation time
 * in authenticated mode, retrieve all column information, including spatial info, default value and primary key in one single statement
 * use integer primary key of tables, when available, to scroll faster among features instead of using OFFSET/LIMIT (#5906)

CSV driver:
 * add optional field type detection with AUTODETECT_TYPE=YES open option
 * add QUOTED_FIELDS_AS_STRING open option that default to NO. So by default, if AUTODETECT_TYPE=YES, the content of quoted fields will be tested for real, integer,... data types
 * fix to avoid truncation of WKT geometries to 8000 characters (#5508)
 * fix segfault when reading allCountries.txt of geonames.org (#5668)
 * accept space as separator as input/output, and add MERGE_SEPARATOR=YES/NO open option

DXF driver:
 * improve TestCapability(ODsCCreateLayer)

FileGDB driver:
 * add layer creation option to set CONFIGURATION_KEYWORD
 * avoid error message when failing to import SRS from WKID code (might be an ESRI code for example)
 * do not reject features with null geometry
 * use LatestWKID when available (#5638)
 * avoid emitting error when opening a FileGDB v9, so that OpenFileGDB can be tried to open it, in the case FileGDB is a plugin (#5674)
 * fix CreateFeature() to work when a esriFieldTypeGlobalID field is not set
 * report width of string fields (#5806)
 * add compatibility with FileGDB SDK v1.4
 * enable bulk load on newly created layers

GeoJSON driver:
 * implement Date/Time/DateTime field type detection
 * expose a 'id' object, of type string, directly on Feature object (not in its properties) as a field
 * add FLATTEN_NESTED_ATTRIBUTES and NESTED_ATTRIBUTE_SEPARATOR open options
 * TopoJSON: establish layer schema from objects properties (#5870)
 * implement automatic scrolling through result sets of ArcGIS GeoServices Feature Service (#5943)
 * accept and skip UTF-8 BOM (#5630)
 * ESRIJson: parse correctly rings of esriGeometryPolygon objects to build correct Polygon or MultiPolygon (#5538)
 * avoid truncation of real numbers on reading (#5882)
 * internal libjson-c: Fix to read floating point numbers in non C locale (#5461)
 * improve TestCapability(ODsCCreateLayer)
 * make string comparison for authority name case insensitive so as to recognize lowercase 'epsg' (#4995)
 * support reading Feature without geometry field

GeoRSS driver:
 * fix to parse ATOM feed documents with atom: namespace (#5871)

GME driver:
 * Added fixes discovered while using v.in.ogr and v.out.ogr in GRASS

GML driver:
 * add XSD=filename open option
 * add FORCE_SRS_DETECTION, INVERT_AXIS_ORDER_IF_LAT_LONG, CONSIDER_EPSG_AS_URN, READ_MODE, EXPOSE_GML_ID, EXPOSE_FID, DOWNLOAD_SCHEMA and REGISTRY open options
 * Fix bug that prevented multiple instances of the reader with Xerces backend (#5571)
 * parse correctly GML geometries whose srsDimension attribute is on top-level geometry element and not on posList (#5606)
 * add datasource option SRSDIMENSION_LOC=GEOMETRY to be able to write srsDimension attribute on top level geometry element, default on posList unchanged (#5066)
 * add support for reading layers resulting from a WFS 2.0 join query
 * read/write top <gml:description> and <gml:name> as DESCRIPTION and NAME metadata items. Also add GML_ID, DESCRIPTION and NAME creation options
 * support to reader response to CSW GetRecords queries
 * Fix incorrect geometry cast when reading GML topogeometries (#5715)
 * VFR: fix ST_Z type (changes) -- list all layers
 * VFR: include also non-spatial (removed) features (ZaniklePrvky) in type ST_Z (changes)
 * VFR: use String when 32-bit integer was not wide enough
 * VFR: add support for UVOH file type
 * VFR: add missing support for OriginalniHraniceOmpv geometry
 * VFR: update GFS files to RFC31 (OGR 64bit Integer Fields and FIDs)
 * update RUIAN GFS files: add missing GMLFeatureClasses to OB type (SpravniObvody, Mop, Momc)
 * add support for parsing .xsd with a <choice> of polygonProperty and multiPolygonProperty
 * remove wrong case insensitive comparison related to gml_registry.xml use
 * various fixes to better deal with ArcByCenterPoint() as found in FAA AIXML files
 * make Expat parser accept trailing nul characters
 * correctly record path to attribute in case of attribute located on a nested element when .gfs is created with GML_ATTRIBUTES_TO_OGR_FIELDS=YES
 * fix GML_ATTRIBUTES_TO_OGR_FIELDS=YES to work correctly with xlink:href too (#5970
 * make GML_EXPOSE_GML_ID to be honoured on WFS documents

GPKG driver:
 * add support for non-spatial layers via the gdal_aspatial extension (#5521)
 * add support for creating spatial index
 * add layer metadata read/write support
 * implement ST_GeometryType(), GPKG_IsAsisgnable() and ST_SRID() to be compatible with Geometry Type Triggers and SRS ID Triggers Extensions
 * on creation, use GEOMCOLLECTION (instead of GEOMETRYCOLLECTION) (#5937)
 * make SELECT expressions passed to ExecuteSQL() be evaluated by SQLite
 * make it possible to use spatialite 4.2.0 SQL functions
 * add a 'INDIRECT_SQLITE' dialect that goes through the VirtualOGR mechanism (e.g. for compat with older Spatialite)
 * allow table names with dash character (#5472)
 * emit warning when required extensions are not implemented
 * disable PRAGRAM integrity_check by default, since it can be expensive on big files
 * read-only support for tables without integer primary key
 * fix Date and DateTime support
 * implement TEXT(maxwidth) type in read and creation
 * implement deferred table creation
 * fix reporting of geometry type for 2.5D (previous behavior had the effect to turn to wkbUnknown)
 * put correct value (1) in gpkg_geometry_columns for 2.5D tables (#5481)
 * fix component geometry type of 3D MultiGeometries (#5629)
 * fix GetExtent() crash on layers without extent set in gpkg_contents (#5471)
 * avoid leak when a table has more than one FID column
 * accept spatial tables whose geometry field is declared as BLOB
 * recognize both GeomCollection and GeometryCollection as possible values, until GeoPackage SWG clears what is the official value
 * escape all column names in SQL (#5520)
 * accept geometries with Spatialite format, that can be returned with issuing a SQL request using spatialite functions
 * enable Spatialite 4.3 'amphibious' mode to avoid explicit cast to Spatialite geometries

GPSBabel driver:
 * automatically open .igc files, implement Identify() and add open options
 * advertise creation option

GTM driver:
 * declare OLCCreateField and OLCSequentialWrite capabilities

IDRISI driver:
 * fix support for multi-ring polygons (#5544)

ILI driver:
 * Use Ili1TransferElement written by ili2c 4.5.5 and newer
 * Fix crash in polygon geometry reading
 * Fix reading SURFACE polygons with multiple rings
 * Fix reading tables with polygon type
 * Support curve geometries for ILI1 and ILI2.
 * Add a MODEL open option

ISIS3 driver:
 * fix to recognize IsisCube.Mapping.LatitudeType = Planetocentric (#5717)

KML driver:
 * fix segfault when calling GetNextFeature() on a write-only layer

LIBKML driver:
 * add support for reading gx:MultiTrack
 * rework libkml singleton factory management (#5775)

MITAB driver:
 * add support for append/update/delete operations on .tab files (#5652)
 * add support for CreateField() on non empty file, AlterFieldDefn() and DeleteField() for .TAB (#5652)
 * implement SyncToDisk() for TAB layers (#5652)
 * convert to use of VSI*L FILE API (#5558)
 * don't write field width for integer fields in .mif, which is incompatible with MapInfo (#3853)
 * report OLCCreateField for .mif files (#5477)
 * fix opening .mif file without .mid file (#5570)
 * swap StdParallel1 and stdParallel2 if necessary on LCC projections (https://github.com/mapgears/mitab/issues/1)
 * take into account scale/bounds to properly round coordinates (https://github.com/mapgears/mitab/issues/2)
 * add MITAB_BOUNDS_FILE configuration option to specify a file with projection bounds (https://github.com/mapgears/mitab/issues/3)
 * add BOUNDS layer creation option (#5642)
 * refactor import/export of MIF coordsys to use the TAB code; take into account MITAB_BOUNDS_FILE to add Bounds to the CoordSys string
 * close polygon rings when reading Region from MIF file (#5614)
 * fix segfault in CreateFeature() if passing an invalid OGR feature style string (#1209)

MSSQLSpatial driver:
 * Implement SPATIAL_INDEX layer creation option for MSSQL (#5563)
 * Implement support for WKB geometry upload (#5682)
 * Fix schema handling in MSSQL driver (#5401)
 * Fix spatial geometry field handling (#5474)
 * Bind string fields to unicode string columns in the database (#5239)
 * Fix recognizing image columns as geometry columns for the select layers. (#5498)
 * Fix issue when creating non-spatial table (#5696)
 * Fix to read metadata if the tables are specified in the connection string (#5796)
 * Fix crash if the tablename is specified in the connection string (#5826)
 * Include geometry column name in Update statement (#5930)
 * Implement FID layer creation option (#5816)
 * Fix issue when removing an MSSQL spatial layer

MySQL:
 * thread-safe initialization of mysql client library (#5528)

NAS driver:
 * implement wfs:update (adds new context 'update' and fields "endet" and "anlass" to "delete" layer).
 * also assign xlink:href attributes as layer attribute (not only in "alkis_beziehungen" layer; #5372)
 * fix filtering on OGR_GEOMETRY special field
 * make chevrons configurable by NAS_INDICATOR

OCI driver:
 * add a ADD_LAYER_GTYPE=YES/NO layer creation option that defaults to YES to enforce a layer geometry type and is used to retrieve the layer geometry type when listing layers (#3754)
 * Fix FID (multi_load=off, OGRNullFID) - start with 1 (not -1) (#5454)
 * use VARCHAR2 instead of VARCHAR for unsupported types
 * Fix "ORA-00972: identifier is too long" error (#5466)
 * Fix memory leaks (#5599)
 * Fix creation of date and datetime fields (#5600)
 * initialize member variable to avoid UpdateLayerExtents() to be called randomly on non spatial tables (#5376)
 * avoid spatial index to be created each time SyncToDisk() is called
 * fix memory leak in DeleteLayer(const char*)
 * fix reading of 2D geometries that were always turned as 3D
 * in layers returned by ExecuteSQL(), only expose geometry column if there's one
 * force NLS_NUMERIC_CHARACTERS to ". " (#5709)

ODBC driver:
 * try alternate DSN template for 64bit ODBC
 * make ODBC driver honour PGEO_DRIVER_TEMPLATE config. option (and also MDB_DRIVER_TEMPLATE in case PGEO_DRIVER_TEMPLATE isn't defined) (#5594)

ODS driver:
 * fix export of OFTDate fields that were exported as string

OpenAir driver:
 * tweak detection logic to read beyond first 10KB bytes when needed (#5975)

OpenFileGDB driver:
 * add compatibility with .gdbtable files bigger than 4 GB (#5615)
 * support opening files with ConfigurationKeyword=MAX_FILE_SIZE_4GB or MAX_FILE_SIZE_256TB (#5615)
 * fix occasional write-after-end-of-buffer (#5464)
 * avoid error message when failing to import SRS from WKID code (might be an ESRI code for example)
 * fix spatial filter with GeneralPolygon shapes (#5591)
 * fix for reading GDB with string fields with a default value length > 127 (#5636)
 * better handling of certain definitions of raster columns
 * use LatestWKID when available (#5638)
 * increase accepted size for field description zone up to 10 MB (#5660)
 * fix ResetReading() on SQL layer with ORDER BY on indexed column (#5669)
 * add support for non spatial GDB v9 tables (#5673)
 * improve error reporting when file exists but cannot be opened due to permission problem (#5838)
 * report width of string fields (#5806)
 * try to deal more gracefully with inconsistent nValidRecordCount vs nTotalRecordCount values (#5842)
 * report 25D layer geometry type on FileGDB v9 tables when relevant
 * optimize sequential reading of sparse layers
 * avoid warning when opening a00000004.gdbtable
 * disable feature count optimization with IS NOT NULL on an index column

OSM driver:
 * add mechanism to compute fields from other fields/tags with SQL expressions. Apply it for z_order on lines layer
 * fix random crash, particularly on MacOSX (#5465)
 * add CONFIG_FILE, USE_CUSTOM_INDEXING, COMPRESS_NODES, MAX_TMPFILE_SIZE and INTERLEAVED_READING open options

PG driver:
 * use COPY mode by default (unless PG_USE_COPY is set to NO) when inserting features in a newly create table (#5460)
 * add UNLOGGED=YES/NO layer creation option to create unlogged tables (improved version of patch by Javier Santana, #4708)
 * implement deferred loading of table list, to optimize ExecuteSQL() (#5450)
 * implement optimization for spatial table listing for PostGIS 2.x
 * implement deferred creation of tables to capture all attribute and geometry column creations into a single CREATE TABLE statement (#5547)
 * change "No field definitions found" from fatal error to debug
 * when creating a table and filling it, avoid re-reading the table definition from PG system tables (#5495)
 * better handling of SRS authority name different than EPSG (authority code must still be integral)
 * fix crash when writing a StringList with 0 element (#5655)
 * emit errors instead of debug messages when postgres issues an error (#5679)
 * fix to make ExecuteSQL('CREATE DATABASE foo') work
 * fix regression that prevented to retrieve more than 500 features from a connection with tables= parameter and on a SQL result layer (#5837)
 * PG/PGDump: fix truncation of fields to work with multi-byte UTF-8 characters (#5854)

PGDump driver:
 * switch to DROP_TABLE=IF_EXISTS by default (#5627)
 * fix crash when writing a StringList with 0 element (#5655)

PGeo driver:
 * try alternate DSN template for 64bit ODBC

REC driver:

Shapefile driver:
 * add SPATIAL_INDEX layer creation option (#5562)
 * support .prj files with UTF-8 BOM
 * fill 'date of last update' header with current time instead of dummy date, and add a DBF_DATE_LAST_UPDATE layer creation option to override this with a fixed date (#3919)
 * fix reading of shapefiles whose .shx is non conformant (#5608)
 * fix writing values up to 2^53 in OFTReal fields with 0 decimal places (#5625)
 * delete implicit FID field as soon as we CreateField a real one
 * GetExtent(): don't trust extent in header if it contains Not-A-Number values (#5702)
 * make REPACK compact .shp if SetFeature() is called and changes one geometry size (#5706)
 * add check not to cut unicode character while cut the string lengnt for field max length during SetFeature
 * avoid reading whole .shx at open time for /vsicurl/
 * add SHAPE_REWIND_ON_WRITE configuration option that can be set to NO to disable correction of ring winding order on write. Useful when dealing with MultiPolygon that are MultiPatch objects in fact (#5888)
 * Make ENCODING layer creation option proprietary over SHAPE_ENCODING config. option

SOSI driver:
 * remove error noise (#5710)

S57 driver:
 * various compliance fixes in ISO8211 and S57 writer (#5798)
 * make it possible to set LNAM_REFS=OFF as advertized in the doc

SQLite/Spatialite driver:
 * SQLite/Spatialite: add support for multiple geometry column tables, accordingly with RFC 41 (#5494)
 * SQLite SQL dialect: Add hstore_get_value(hstore, key) function
 * remove 'T' suffix when formatting the content of a Date field (#5672)
 * optimize CreateFeature() when fields can be null or not null from one feature to another one
 * Spatialite: improve insertion performance by disabling triggers and doing the job ourselves
 * Fix segmentation fault when executing OGR2SQLITE_Register() when compiling against sqlite 3.8.7 (#5725)
 * make GetFIDColumn() work when run as first method call (#5781)
 * emit warning when reading text values in a integer/real field (possible since SQLite has no strong typing)
 * support reading date/datetime from Julian day floating point representation
 * SQLite dialect: fix insertion in geometry_columns of table names that are not in upper-case (#6483)

SUA driver:
 * tweak detection logic to read beyond first 10KB bytes when needed (#5975)

SXF driver:
 * Fix SXF file version check (#5456)
 * Fix wrong Miller Cylindrical projection string
 * Fix encoding issues (#5647)
 * Fix extract z value to OGRGeometry
 * Fix case sensitivity of RSC file

VFK driver:
 * recode also header values
 * process DKATUZE from header properly
 * handle also duplicated records
 * check attribute 'parametry_spojeni'
 * speed-up GetFeatureCount()
 * fix reading properties. Escape characters for SQL
 * change SRS from EPSG 2065 to 5514
 * fix reading SBP datablock (fix mismatch when reading from file and db)

VRT driver:
 * do not propagate ignoring of x and y cols of a PointFromColumns to the source layer (#5777)
 * add an optional 'name' attribute on FID element, so as to be able to force the report of a FID column name even if it is not exposed as a regular field (related to #5845)
 * handle optional <OpenOptions><OOI key='key'>value</OOI></OpenOptions> to specify open options
 * do not enable passthrough filtering if redefining <FID> (#6480)

Tiger driver:
 * Fix potential buffer underflow when providing /vsistdin/ to Tiger driver (#5567)

WAsP driver:
 * added options and changed output precision to match WAsP Map Editor
 * improve TestCapability(ODsCCreateLayer)

WFS driver:
 * automatically enable paging if WFS 2.0 capabilities report paging support
 * evaluate SELECT with JOIN on server-side for a Join-capable WFS 2.0 server
 * add a TRUST_CAPABILITIES_BOUNDS open option, that can be set to YES to trust layer bounds declared in GetCapabilities response, for faster GetExtent() runtime (#4041)
 * add INVERT_AXIS_ORDER_IF_LAT_LONG, CONSIDER_EPSG_AS_URN and EXPOSE_GML_ID open options
 * add capability to use spatial functions ST_xxxxx() as server-side filters
 * add dataset and layer metadata
 * allow SELECT with several ORDER BY columns
 * report name of geometry column
 * Add COOKIE option (#5824)
 * when parsing a layer schema without geometry from the GML .xsd, do not expose a geometry field at the WFS layer level (#5834)

XLSX driver:
 * fix column numbering when there are more than 26 columns (#5774)

XPlane driver:
 * add support for Taxi Location 1300 record found in V1000

## SWIG Language Bindings

All bindings:
 * bind GDALGetBandDataset() as Band.GetDataset()
 * add Feature.GetFieldAsBinary()

Java bindings:
 * Pass eRWFlag to allow both reading or writing.  Write was broken in DatasetRasterIO().
 * updates to generate maven artifacts

Perl bindings:
 * The breaking changes are described in more detail in swig/perl/Changes-in-the-API-in-2.0.
 * More comprehensive use of strings as constants (such as capabilities); they are also taken from bindings, which added new ones, and not hard-coded.
 * New classes (e.g., VSIF, GeoTransform, GeomFieldDefn) and new methods (e.g., constant lists, Dataset::SpatialReference).
 * Much improved documentation and switch to Doxygen::Filter::Perl.
 * New test codes.
 * Errors are confessed with stack trace and often caught earlier with better messages.
 * Use of attributes is deprecated and methods have been added to replace them.
 * Multiple geometry fields have necessitated some changes in schema and field related methods.
 * More support for named parameters (i.e., hashes as arguments).
 * NoDataValue: set max float if undef is given.
 * Unit: set '' if undef is given.
 * Also other changes that will also remove some "use of uninitialized value in subroutine entry" warnings.
 * Automatic handling of SQL result layers.
 * Fix issue with index attribute for field meta data (schema) (#5662)
 * Warn if attempt to create non-integer column for colors.
 * Remove prefix GCP from GCP swig made attributes.

Python bindings:
 * add optional buf_xsize, buf_ysize and buf_type parameters to Dataset.ReadAsArray() and Dataset.LoadFile(), and use dataset RasterIO for better efficiency
 * avoid generating Python exception when PyString_FromStringAndSize() fails and GDAL errors as Python exceptions are disabled
 * Band.ReadRaster() and Dataset.ReadRaster(): clear the buffer in case there are holes in it due to odd spacings specified by the user
 * Fix hang of Python in case of repeated call to gdal/ogr.UseExceptions() and CE_Warning emitted (#5704)
 * for Python 2, accept unicode string as argument of Feature.SetField(idx_or_name, value) (#4608)
 * for Python 2, accept Unicode strings to be passed as key and/or value of the dictionary passed to SetMetadata() (#5833)
 * fix processing error of ogr_python.i with SWIG 3 (#5795)
 * NUMPY driver: avoid returning CE_None in GetGeoTransform() when there's no geotransform set (#5801)
 * Make GetFieldAsBinary() work with OFTString fields
 * For Python3 compat, make Feature.GetField() use GetFieldAsBinary() if GetFieldAsString() fails (#5811)

# GDAL/OGR 1.11.0 Release Notes

## In a nutshell...

 * New GDAL drivers:
    - KRO: read/write support for KRO KOKOR Raw format
 * New OGR drivers:
    - CartoDB : read/write support
    - GME (Google Map Engine) : read/write support
    - GPKG (GeoPackage): read-write support (vector part of the spec.)
    - OpenFileGDB: read-only support (no external dependency)
    - SXF driver: read-only support
    - WALK : read-only support
    - WasP : read-write support
 * Significantly improved drivers: GML, LIBKML
 * RFC 40: enhanced RAT support (#5129)
 * RFC 41: multiple geometry fields support
 * RFC 42: OGR Layer laundered field lookup
 * RFC 43: add GDALMajorObject::GetMetadataDomainList() (#5275)
 * RFC 45: GDAL datasets and raster bands as virtual memory mapping
 * Upgrade to EPSG 8.2 database

## New installed files
 * bin/ogrlineref
 * lib/pkgconfig/gdal.pc
 * gdalcompare.py
 * data/gml_registry.xml
 * data/inspire_cp_CadastralParcel.gfs
 * data/inspire_cp_BasicPropertyUnit.gfs
 * data/inspire_cp_CadastralBoundary.gfs
 * data/inspire_cp_CadastralZoning.gfs
 * data/ruian_vf_ob_v1.gfs
 * data/ruian_vf_st_v1.gfs
 * data/ogrvrt.xsd

## Backward compatibility issues

# GDAL 1.x

Consult [NEWS-1.x.md](NEWS-1.x.md)
