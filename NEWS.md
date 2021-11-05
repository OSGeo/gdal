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

* GDALInfo(): fix axis order issue in lon,lat corner coordinates, in particular when reading from a .aux.xml with a ProjectedCRS (#2195)
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
 * add support for Rotated pole LatLon projections (#7104)
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
 * JPGIS FGD v4: fix logic so that coordinate order reported is lon/lat (github #241)
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
 * ignore wpt/rtept/trkpt with empty content for lat or lon

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

## GDAL/OGR 1.11.0 - General Changes

Build(Unix):
 * add Unix configure support for SOSI
 * remove pointers to old ver of ingres library files
 * add --with-libjson-c configure option to build against external libjson-c (>= 0.11) (#4676)
 * compilation fixes for iOS (#5197, #5198)
 * update to autoconf 2.69
 * add pkg-config gdal.pc (#3470)
 * configure for FileGDB: add explicit linking to libfgdbunixrtl (requires FileGDB SDK >= 1.2) (#5215); also try .dylib extension (#5221)
 * fix so that Java installs are found on the MAC to enable the MDB driver (#5267)
 * fix compilation with recent MySQL versions (5.6 for example) (#5284)
 * support --with-jp2mrsid with standalone Kakadu with MRSID v8 or later
 * Fix parallel build in Python bindings (#5346)
 * PCIDSK: don't link against libjpeg if configured --without-jpeg
 * Update configure script to pick up ECW JP2 SDK 5.1 (#5390)
 * add a 'make install' target for the Java bindings  (#5424)
 * add Vagrant configuration

Build(Windows):
 * add option to generate VC project for x64 on makegdal_gen.bat
 * nmake.opt: add WITH_PDB=1 option to optionally generate .pdb file on Release builds (#5420)
 * add support for building the OGR SOSI driver as a plugin (#3638)
 * add support for building the HDF4 driver as plugin (#5294)
 * add support for MrSID v9
 * Remove makegdalXX.bat generated files

## GDAL 1.11.0 - Overview of Changes

Port:
 * vsisubfile: fix Eof() behavior to be POSIX compliant, so that the shapefile reader can read the last feature when using /vsitar (#5093)
 * vsicache: fix for 32bit binaries when file size is over 2GB (#5170)
 * vsicache: add optional nChunkSize and nCacheSize parameters to VSICreateCachedFile()
 * vsicurl: add CPL_VSIL_CURL_USE_HEAD config option to disable use of CURL HEAD for other services like mapbox (likely lame python http implementations)
 * vsitar: avoid infinite loop in case of invalid .tar structure
 * vsizip: fix path separator in CPLFormFilename
 * vsizip: allow additional extensions listed in CPL_VSIL_ZIP_ALLOWED_EXTENSIONS config option.
 * vsizip: improve UTF-8 support of filenames inside ZIP file (#5361)
 * vsizip: fix ZIP64 support
 * vsigzip: reset EOF flag when doing a Seek() to be POSIX compliant
 * curl: add .netrc support
 * Windows CPLGetSymbol(): avoid dialog boxes to pop up when a DLL or one of its dependencies does not exist (#5211)
 * Add CPLOPrintf() and CPLOvPrintf() functions for easy CPLString formatting
 * CPLBase64DecodeInPlace() : fix to be robust to malformed base64 strings
 * CPLQuadTree: add CPLQuadTreeInsertWithBounds() where the pfnGetBounds is not needed.
 * CPLQuadTree: fix potential infinite recursion when inserting several points with identical coordinates in the mode with limited bucket size
 * Protect concurrent calls to setlocale() by a mutex (#5366)

Core:
 * RFC 45: GDAL datasets and raster bands as virtual memory mapping
 * GDALRasterBand::GetHistogram(): ignore nodata values (#4750, #5289)
 * allow auto loading of drivers to be disabled via config option
 * PAM .aux.xml and VRT: serialize Z component of a GCP as 'Z' attribute,
   for consistency, instead of GCPZ that could not be read back previously.
   In reading code, try reading 'Z' and if not found try 'GCPZ' (#5326)
 * JPEG2000: Add GDALGeorefPamDataset and GDALJP2AbstractDataset classes and use
             them in JP2KAK, JP2ECW, JP2OpenJPEG, JPEG2000 and MrSID drivers so that PAM
             georeferencing consistently overrides internal georeferencing
 * GDALDataset::IRasterIO(): don't use BlockBasedRasterIO() when INTERLEAVE=PIXEL if the request band count is just 1
 * CopyWholeRaster(): make default GDAL_SWATH_SIZE to 1/4 of GDAL_CACHEMAX instead of hard-coded value of 10 MB
 * don't report empty RAT on GDALGetDefaultRAT() (#5232)
 * modify GDALGCPsToGeotransform() to do the regression in normalized coordinates to make the math more stable.
 * expose new GDALComposeGeoTransforms() function.
 * GDALDefaultOverviews::HaveMaskFile(): avoid fetching .ovr file
 * JPEG2000: Fix reading georeferencing from some JPEG2000 files with duplicated GeoTIFF JP2Box (#5249)
 * Cleanup raster block mutex (#5296)
 * Driver registration: move JPEG2000 (Jasper based) after MrSID JPEG2000 support

Algorithms:
 * warper: fix regression with lanczos resampling when yradius > xradius (#5058)
 * warper: Make GDALCreateGenImgProjTransformer2() and GDALCreateGenImgProjTransformer3() fail when the creation of the reprojection transformer fails
 * warper: Fix warping when input pixel size is too close to 0 (#5190)
 * warper: revise formula of cubic resampling kernel, and a few optimizations (#5209)
 * warper: added DST_METHOD and support for GCP and TPS dest
 * warper: add support for DST_METHOD=RPC
 * warper: fix mode and near resampling corner computation (#5311)
 * warper: GDALGenImgProjTransform(): don't set panSuccess[i] to 1 in the middle of the function, if an intermediate transform before has set the flag to 0
 * warper: fix cutline blending (#5343)
 * warper: Average/mode kernels: make them less sensitive to numerical precision issues (#5350)
 * warper: Average/mode kernels: avoid 'holes' when the source coordinates are in a reversed order from the target coordinates (#5433)
 * warper: provide prototypes and work around strict compiler requirements on some opencl platforms (#5400)
 * RPC: fix for computation of adfGTFromLL (#5395)
 * TPS: optimization for GCC x86_64 that make computation about twice faster with huge number of GCPs
 * TPS: when using Armadillo to solve the coefficients, use solve(A,B) instead of inv(A)xB to faster resolution
 * TPS: compute direct and inverse transformations in parallel when warping option NUM_THREADS or GDAL_NUM_THREADS config. options are set to > 1
 * Geoloc: fix wrong bilinear interpolation in GDALGeoLocTransform() (#5305)
 * Geoloc: fail transformation of coordinates that is located on a nodata place of the geoloc array
 * rasterize: preliminary support for MERGE_ALG=ADD for heatmaps
 * gdal_grid: Add AVX optimized version of GDALGridInverseDistanceToAPower2NoSmoothingNoSearch
 * fill_nodata: GDALFillNodata(): Fix use of uninitialized memory and integer overflows (#4010, #5203)
 * rpc: Fix out-of-bounds read in RPC dem cubic interpolation

Utilities:
 * gdalinfo: add -listmdd and -mdd all options (#5275)
 * gdal_translate: add a -exponent option to be used with -scale
 * gdal_translate: fix output file naming scheme in gdal_translate -sds (#5119)
 * gdal_translate: fix logic in detection non-gray color table level (#5245)
 * gdal_translate: add a -norat option
 * gdal_translate: don't add 0.1 when -scale is used with a dstmin equal to dstmax (useful to generate a raster with uniform color, i.e. scaleRatio = 0)
 * gdal_translate: use floor() to compute image coordinates from world coordinates when specifying -projwin (useful when extracting from left or top of upper-left corner, which generate negative image coordinates) (#5367)
 * gdaltindex: remove annoying warning 'Warning 1: Field location of width 255 truncated to 254' (#5121)
 * gdaltindex: add -src_srs_name and -src_srs_format to go with MapServer RFC100; add also a -f and -lyr_name options to be able to create a non-shapefile tileindex
 * gdalwarp: Fix segfault where metadata values were not being nullchecked properly during conflict resolution (#5069)
 * gdalwarp: honor -s_srs when using cutline (#5081)
 * gdalwarp: copy nodata values from source to dest if -dstnodata is not given ; add option to not set dest nodata with -dstnodata None (#5087)
 * gdalwarp: do not return a non-zero exit status for warnings
 * gdalwarp: prevent from copying statistics metadata (#5319)
 * gdal_rasterize: set the progress bar to 100% even when there's nothing to do
 * gdal_grid: add support for different types of geometries (#5341)
 * gdal_grid: add  -z_increase and -z_multiply options
 * gdaldem: check that value of -z, -s, -az and -alt is numeric
 * gdalbuildvrt: validate values of -srcnodata and -vrtnodata arguments
 * gdal2tiles.py: Corrected OpenLayers code to reflect fix to geodetic resolution factor
 * gdal2tiles.py: add --tmscompatible flag so as to produce 2 tiles at zoom level 0 in geodetic profile
 * rgb2pct.py: Use python tempfile logic to avoid permissions issues with cwd (#5079)
 * gdal_edit.py: add a -ro option for drivers refusing to use the dataset in update-mode.
 * gdal_calc.py: add --allBands options (#5388)
 * Add vsipreload.cpp that can be compiled as a shared library that can be LD_PRELOAD'ed as an overload of libc to enable VSI Virtual FILE API to be used with binaries using regular libc for I/O
 * Add the wcs_virtds_params.py sample utility to be able to set the MapServer WCS virtual dataset parameters from a tileindex with rasters of mixed SRS (linked to MapServer RFC100)
 * gdalcompare.py: move to scripts
 * gdalcompare.py: ensure image dimensions match
 * gdal_ls.py: Fix issue with UTF-8 characters

Multi driver changes:
 * JPEG2000 drivers: take into account PixelIsPoint in GeoJP2 boxes, and expose AREA_OR_POINT=Point (#5437)
 * JP2KAK, JP2ECW, JP2OpenJPEG, JPEG2000 CreateCopy(): take into account AREA_OR_POINT=Point if present to write GeoJP2 box (#5437)

AAIGRID:
 * revert DECIMAL_PRECISION and add SIGNIFICANT_DIGITS to CreateCopy() (#3732)

AIGRID:
 * Turn off errors that can be triggered if the info has no VAT table related with this coverage (#3031)

BAG driver:
 * Recognise falseNorthing=10000000 as UTM South (#5152)

DIMAP driver:
 * fix memleak in error-code path

DTED driver:
 * Speed optimization to be more friendly with CPU cache in GDAL_DTED_SINGLE_BLOCK=YES mode

ECW driver:
 * fix crash in GDALDeregister_ECW() with ECW SDK 5 called from GDALDestroy() (#5214)
 * fix issue with ECW_CLEVER optimization when nPixelSpace != sizeof eBufDataType (#5262)

Envisat driver:
 * implement more reliable way of extracting GCPs from Meris tie-points (#5423)
 * add DEM corrections of TP-ADS products when present (#5423)
 * workaround dateline discontinuity in GCPs so they can be used with GDAL warping transformers (#5423)

ERS driver:
 * fix wrong interpretation of RegistrationCellX/RegistrationCellY (#2612, #3056, #5075)

GeoRaster driver:
 * fix RPC support (#4038)
 * fix read error when reading from pyramids (#5076)
 * make regular table and secure file a default for RDT (#5127)
 * fix error when reading NBIT pyramid levels (#5199)
 * show the VAT as RAT (#5200)
 * fix reading and writing of statistics metadata (#5237)
 * add generate pyramid create options (#5288)
 * fix incorrect geotransform interpretation when there is no SRS (#5323)

GRASS driver:
 * fix compilation issues for GRASS 7

GRIB driver:
 * display temperature unit as deg Celsius in metadata (#3606)

GTiff driver:
 * when compiling against internal libtiff, in read-only mode, optimization to
   avoid fetching the whole Strip/TileCounts and Strip/TileOffsets arrays
 * add validation of source overview characteristics with COPY_SRC_OVERVIEWS (#5059)
 * convert invalid TIFFTAG_RESOLUTIONUNIT=0 to 1(Unknown) (#5069)
 * fix potential issues in gt_citation.cpp / CheckUTM()
 * upgrade internal libtiff to latest CVS
 * implement reading and writing of ICC profiles (#5246)
 * make SetColorInterpretation() round-trip with GetColorInterpretation();
   read color interpretation from PAM if it exists (overrides internal tiff color interpretation);
   set TIFFTAG_PHOTOMETRIC=PHOTOMETRIC_RGB if calling SetColorInterpretation() with R,G,B and no explicit PHOTOMETRIC creation option defined
 * gt_wkt_srs.cpp: fix compilation with external libgeotiff. The file is dependent of quite a few CPL stuff, don't try to pretend otherwise
 * implement GetVirtualMemAuto() for some formulations of TIFF files (RFC 45)
 * fix reading a single-strip TIFF file where the single strip is bigger than 2GB (32bit builds only) (#5403)
 * look for .tab file before .wld/.tfw

GTX driver:
 * Add nodata support (#4660)

HDF4 driver:
 * Skip "SceneLineNumber" table if present in the list of geolocation fields of
   ASTER L1A dataset.

HDF5 driver:
 * add support for ODIM H5 georeferencing method (#5032)
 * set SRS GEOGCS in all cases (reverts r25801 and closes #4160)
 * support HDF5 NATIVE_SCHAR type, subdatsets without PAM (#5088)
 * release all opened handles so the file is closed at dataset closing (#5103)
 * better deal with dimensions of CSK-L1A HDF5 subdatasets (#4227)
 * avoid segmentation fault when H5Sget_simple_extent_ndims() returns negative value (#5291)

HFA driver:
 * add minimally tested support for u2 and u4 data in basedata
 * use direct binning for thematic layers and real instead of integer for values (#5066)
 * add a HFA_COMPRESS_OVR config option to select whether to create compressed overviews (#4866)
 * fix rewriting of statistics in existing HFA file where base data value is 8-bit (#5175)
 * implement re-writing existing histogram in HFA file, after raster editing (#5176)
 * avoid segfaults when creating a Imagine dataset with an invalid WKT (#5258)
 * expose color columns in RAT as Integer with values in range [0-255] instead of Real with values [0-1] (#5362)
 * report histogram column as GFU_PixelCount instead of GFU_Generic (#5359)
 * ensure histogram column written as float for HFA when using RAT API (#5382)

Idrisi driver:
 * Improve coordinate system handling and min/max statistics (#4980)

IRIS driver:
 * add height information on bands; rename dataset metadata item CAPPI_HEIGHT --> CAPPI_BOTTOM_HEIGHT (#5104)
 * IRIS: add support for two bytes data (#5431)

JP2ECW driver:
 * fix problem with JP2 write with SDK v5
 * fix issue with ECW_CLEVER optimization when nPixelSpace != sizeof eBufDataType (#5262)
 * avoid writing dummy GeoJP2 box when source dataset has no georeferencing (#5306)

JP2KAK driver:
 * preliminary support for Kakadu V7.x
 * fix creation of unsigned int16 with reversible compression (#4050)
 * on Windows, use VSI cache for I/O by default, instead Kakadu own I/O layer
 * remove extension from 12bit to 16bit (#5328)

JP2OpenJPEG driver:
 * avoid 'Empty SOT marker detected: Psot=12.' warning to be repeated several times
 * add support for encoding GCPs in a GeoJP2 box (#5279)
 * avoid writing dummy GeoJP2 box when source dataset has no georeferencing (#5306)

JPEG driver:
 * add autodetection of bitmasks that are msb ordered (#5102)
 * avoid memory leak when GDALOpen'ing() a JPEG through a http:// URL, and make it possible to access its overviews
 * return YCbCrK raw data for YCbCrK JPEG in GDAL_JPEG_TO_RGB = NO mode (instead of CMYK as before) (#5097)
 * implement reading and writing of ICC profiles (#5246)
 * internal libjpeg: apply patch for CVE-2013-6629
 * allow fallback to PAM to read GCPs
 * give priority to PAM GeoTransform if it exists and other source of geotransform (.wld, .tab) also exists (#5352)

KMLSuperOverlay driver:
 * recognize an alternate structure for raster KMZ file made of a single doc.kml
    and tiles whose name pattern is kml_image_L{level}_{j}_{i}.{png|jpg}
 * fix horrible speed performance in Open() (#5094)
 * fix crash at dataset closing and inability to read some big PNG tiles (#5154)
 * fix to generate files validating against OGC KML 2.2 schema
 * put Style into conformity with ATC 7
 * remove Region in root KML (ATC 41)
 * add NAME and DESCRIPTION creation options; read them back as metadata
 * add ALTITUDE and ALTITUDEMODE creation options
 * directly write into .kmz file (instead of in temporary location)
 * correctly write directories entry in .kmz file
 * add progress callback

L1B driver:
 * report correct values for GCP (#2403)
 * report more GCPS than before
 * implement geolocation array
 * add fetching of record metadata in .csv file
 * add subdatasets with solar zenith angles, cloud coverage
 * recognize NOAA-9/14 datasets whose dataset name in TBM header is encoded in EBCDIC and not in ASCII (#2848)
 * support opening a few NOAA <= 9 datasets that have no dataset name in the TBM header

LCP driver:
 * better handling of projections (#3255)
 * add CreateCopy() (#5172)

MBTiles driver:
 * add write support
 * avoid failure when there's no tile at the center of the maximum zoom level (#5278)
 * add capability to open /vsicurl/https:// signed AWS S3 URLs

MEM driver:
 * Create(): use calloc() instead of malloc()+memset() for faster creation of huge in-memory datasets

NetCDF driver:
 * fix to read netcdf-4 files with UBYTE data (#5053)
 * fix reading large netcdf-4 files with chunking and DEFLATE compression
 * fix netcdf chunking when creating file with > 2 dims ; add CHUNKING creation option (#5082 )
 * fix duplicate nodata metadata when using CreateCopy() (#5084)
 * fix copying large metadata in netcdf driver (#5113)
 * fix netcdf geotransform detection (#5114)
 * fix netcdf driver irregular grids management (#5118 and #4513)
 * only call nc_close on a valid netcdf id when closing dataset
 * try and identify .grd (and .nc3) files in netcdf-4 format (#5291), so they are identified before the hdf5 driver

NITF driver:
 * fix to support reading horizontal and/or vertical mono-block uncompressed images, even when the number of columns is <= 8192 (#3263)
 * update NITF Series list with new entries from MIL-STD-2411_1_CHG-3.pdf (#5353)
 * allow JP2KAK to be used as the JPEG2000 compression engine in the CreateCopy() case (#5386)

PDF driver:
 * Avoid reporting a Poppler error as a GDAL error on some newer USGS GeoPDF files (#5201)
 * PDF writing: automatically adjust DPI in case the page dimension exceeds the 14400 maximum value (in user units) allowed by Acrobat (#5412)

PDS driver:
 * Parse correctly MISSING_CONSTANT = 16#FF7FFFFB# as a IEEE754 single precision float expressed in hexadecimal; add support for ENCODING_TYPE = ZIP (data file compressed in a ZIP); recognize IMAGE_MAP_PROJECTION as an object included in UNCOMPRESSED_FILE object (#3939)

PNG driver:
 * Implement reading and writing of ICC profiles (#5246)

PostgisRaster driver:
 * Speed-up dataset opening (#5046).
 * Multi-tile multi-band caching added.
 * Smarter use of the information advertized in raster_columns view.
 * Avoid full table scan in situations without PKID/GIST indices.
 * Use of quadtree.

Rasdaman driver:
 * caching of tiles for datasets with more than one band (#5298)
 * connections are now kept for a whole session (#5298)
 * fixing connection-string regex (#5298)
 * fixing possible memory leaks (#5298)

Rasterlite driver:
 * fix resolution check typo in rasterlite driver

Raw drivers:
 * implement GetVirtualMemAuto() (RFC 45)
 * IRasterIO(): add special behavior to avoid going to block based IO when the dataset has INTERLEAVE=PIXEL and is eligible to direct I/O access pattern
 * allow direct I/O access even if a small proportion of scanlines are loaded (improve QGIS use case where the overview display will load sparse scanlines, which would prevent direct I/O at full resolution afterwards)
 * fix optimized RasterIO() when doing sub-sampling with non standard buffer pixel offset (#5438)

RMF driver:
 * fix decompression of 24-bit RMF DEM (#5268)

RPFTOC driver:
 * fix potential crash on some datasets when selecting the color palette (#5345)

SAGA driver:
 * add read/write support for .prj files (#5316)

SRP driver:
 * read TRANSH01.THF file to establish subdatasets (#5297)

VRT driver:
 * Implement non-linear scaling with a power function (addition of Exponent, SrcMin, SrcMax, DstMin, DstMax sub-elements in <ComplexSource>)
 * Preserve 64bit integer image offsets (#5086)
 * Make sure that VRTSourcedRasterBand::AddMaskBandSource() takes into account specified window (#5120)
 * Make GDALAutoCreateWarpedVRT() return NULL when GDALSuggestedWarpOutput() fails
 * VRTDataset::IRasterIO(): use source DatasetRasterIO even if band count is 1
 * VRTWarped: avoid setting up relative paths for things that aren't file-like
 * make relativeToVRT=1 work with NITF_IM:, NETCDF:, HDF5:, RASTERLITE:

WCS driver:
 * ensure C locale is enforced before parsing floating point values

WMS driver:
 * accept 'WMS:http://server/?SRS=EPSG:XXXX' syntax to select the preferred SRS in which to fetch layers
 * CPLHTTPFetchMulti(): avoid doing a timeout-only select when there are no file descriptor to wait on (can happen when doing a file:// URL)
 * allow cache location to be specified with GDAL_DEFAULT_WMS_CACHE_PATH configuration option if not provided in the XML (#4540)
 * Update to be able to understand slight changes in formatting of JSon output of ArcGIS mapserver protocol

XYZ driver:
 * accept datasets that have missing values at beginning and/or end of lines, such as MNT250_L93_FRANCE.XYZ
 * fix detection when there are only integral values with comma field separator
 * reopen with 'rb' flags for Windows happyness

## OGR 1.11.0 - Overview of Changes

Core:
 * GEOS support: require GEOS >= 3.1.0 and use the _r API of GEOS to avoid issues with the global GEOS error handlers
 * exportToWkb(): ISO WKB generation with wkbVariant option (#5330)
 * geocoding: when getting several answers from server for a query, report geometries on second, third, etc.. feature, and not only first one (#5057)
 * allow auto loading of drivers to be disabled via config option
 * remove obsolete OGRGeometryFactory::getGEOSGeometryFactory()
 * OGRGeometryFactory::organizePolygons() in DEFAULT method: fix a case with 2 outer rings that are touching by the first point of the smallest one
 * OGRGeometryFactory::organizePolygons(): optimization in ONLY_CCW case
 * OGRGeometryFactory::organizePolygons(): Add an experimental mode : CCW_INNER_JUST_AFTER_CW_OUTER
 * OGRLineString::segmentize() : do not set 0 as z for interpolated points, but the z from the previous point
 * OGRLineString::setNumPoints(): add an optional argument to avoid zeroing the arrays
 * Add OGRLineString::setZ()
 * Add OGRLineString::Project() and OGRLineString::getSubline()
 * OGRPolygon: add stealExteriorRing() and stealInteriorRing(int iRing)
 * OGRLinearRing::isClockwise(): optimizations and make it work in a degenerated case when a vertex is used several times in the vertex list (#5342)
 * OGRLinearRing::isPointOnRingBoundary() : optimizations and take into account bTestEnvelope
 * Add OGR_G_SetPointCount and OGR_G_SetPoints functions to API C (#5357)
 * OGREnvelope3D::Contains(): fix incorrect test
 * Layer algebra: fix handling of method field mapping to output fields when output fields are precreated (#5089)
 * Layer algebra: when an error condition is skipped, call CPLErrorReset() (#5269)
 * OGRLayer::GetFeature(): make sure that the behavior is not influenced by
   attribute or spatial filters in the generic implementation;
   upgrade OGDI, PG, MySQL, MSSQLSpatial, OCI, SDE, PGeo, ODBC, WALK, IDB, SQLite and Ingres driver  (#5309)
 * introduce OGRLayer::FindFieldIndex() / OGR_L_FindFieldIndex() to lookup potentially laundered field names (RFC 42)
 * OGR SQL: upgrade to support RFC 41 (multiple geometry fields)
 * OGR SQL: more stricter checks
 * OGR SQL: make parsing error report a useful hint where the syntax error occurred
 * OGR SQL: fix thread-safety of swq_op_registrar::GetOperator() (#5196)
 * OGR SQL: support not explicitly specifying AS keyword for aliasing a column spec
 * OGR SQL: don't call CONCAT(a_column ...) or SUBSTR(a_column ...) as a_column
 * OGR SQL: validate that arguments of MAX, MIN, AVG, SUM, COUNT are columns and not any expression since this is not supported
 * OGR SQL: make AVG field definition a OFTReal
 * OGR SQL: implement MIN(), MAX() and AVG() on a date (#5333)
 * OGR SQL: fix SELECT * on a layer with a field that has a dot character (#5379)
 * SQL SQLITE dialect: Make it available to all OGR drivers that have a specialized ExecuteSQL() implementation

OGRSpatialReference:
 * Upgrade to EPSG 8.2 database
 * identify LCC_2SP instead of LCC_1SP if lat_0==lat_1 and lat_2 is present (#5191)
 * add a variety of linear units to proj4 parsing (#5370)
 * Fix crash in CleanupESRIDatumMappingTable() if it is called twice (#5090)
 * fix order of AXIS and UNIT nodes in a VERT_CS node (#5105)
 * ecw_cs.wkt: add missing TOWGS84[-168,-60,320,0,0,0,0] to NTF datum (#5145)
 * fix OGRSpatialReference::importFromProj4() to work with non-C locale (#5147)
 * morph central_latitude to latitude_of_origin in morphFromESRI() (#3191)
 * OGRProj4CT: avoid using proj when the 2 projections are actually identical (#5188)
 * add sanity checks in OGR_SRSNode::importFromWkt() (#5193)
 * VERT_CS: when importing from proj.4 put AXIS node after UNIT; COMPD_CS: when importing from EPSG:x+y, set a more meaningful name for the COMPD_CS node
 * OGRSpatialReference::Validate() : in addition to hand-validation, use WKT grammar from OGC 01-009 CT
 * preserve authority when importing +init=auth_name:auth_code (e.g. +init=IGNF:LAMB93)

Utilities:
 * ogrlineref: new utility to deal with linear geometries.
 * ogrinfo: upgrade to support RFC 41 (multiple geometry fields)
 * ogr2ogr: upgrade to support RFC 41 (multiple geometry fields)
 * ogr2ogr: bump default value for -gt from 200 to 20000 (#5391)
 * ogr2ogr: add -addfields option to add new fields found in a source layer into an existing layer ; add -unsetFieldWidth option to unset field with and precision; add -dim layer_dim option to force the coordinate dimension of geometries to match the one of the layer geometry type
 * ogr2ogr: Check that -t_srs is also specified when -s_srs is specified
 * ogr2ogr: add an explicit error message to report FID of feature that couldn't be inserted when CreateFeature() fails
 * ogr2ogr: make relaxed lookup optional and add a switch -relaxedFieldNameMatch to allow it (RFC 42)
 * ogr2ogr: make sure that the progress bar reaches 100% when converting OSM
 * ogr2ogr: make sure that target dataset is properly closed when a CreateFeature() fails (so that truncated shapefiles have their header file properly updated)
 * ogr_dispatch.py: Sample Python script to dispatch features into layers according to the value of some fields or the geometry type
 * ogrinfo.py: sync with ogrinfo (RFC 41)
 * ogr2ogr.py: port -nlt PROMOTE_TO_MULTI option from ogr2ogr.cpp (#5139)

CSV driver:
 * avoid erroneously reset of file content when opening in update mode a file without header (#5161)
 * upgrade to support RFC 41 in read/write (multiple geometry fields)
 * allow backslash doublequote to load (#5318)

DGN driver:
 * DGN writing: added polygon inner ring (holes) writing and MSLink writing (#5381)

DXF driver:
 * fix writing of 25D linestring where z is not constant (#5210)
 * fix writing of POLYLINE objects (#5217, #5210)
 * accept reading files starting with a TABLES section (#5307)
 * support reading 3DFACE and SOLID (#5380) entities
 * fix an error when processing clockwise circle arc (#5182)
 * avoid building an invalid polygon when edges cannot be reassembled: turn it into a multilinestring
 * use CPLAtof() instead of atof() to avoid issues with locales
 * fix linear approximation of circular and elliptic arc in HATCH boundaries (#5182)

DWG driver:
 * add support for reading AcDb3dPolyline (#5260)
 * fix linear approximation of circular and elliptic arc in HATCH boundaries (#5182)

FileGDB driver:
 * implement IgnoreFields API to speed-up a bit the conversion of a sub-set of fields when there's a huge amount of them (e.g. Tiger database).
 * when writing <Length> of an attribute, use size in bytes (#5192)
 * implement ref counting of the FileGDB SDK API' Geodatabase* object to avoid issues on Linux 64bit with interleaved opening and closing of databases (#4270)
 * honour update flag to determine which operations are allowed or not
 * add a driver global mutex to protect all calls as the FileGDB API SDK is not thread-safe at all
 * add a COLUMN_TYPES layer creation option to override default column types; support reading/writing XML column types
 * optimize GetFeatureCount() and GetExtent() when there are filters set
 * set the default width for string fields to 65536.
   The width can be configured with the FGDB_STRING_WIDTH configuration option
 * fix creation and writing of Binary fields; enable reading
 * add a CREATE_MULTIPATCH creation option

FME driver:
 * fix Linux compilation

GeoJSON driver:
 * recognize alternate formats such as the ones of https://code.google.com/p/election-maps/
 * add read support for TopoJSON
 * upgrade internal libjson-c to json-c 0.11 (#4676)
 * report integer values that are int64 as strings
 * add 3d support to esri geojson reader (#5219)
 * be less strict on looking for esri field type tag (#5219)
 * fix sometimes incorrect result (significant digit lost...) when using -lco COORDINATE_PRECISION=0
 * fix handling of huge coordinates when writing (#5377)

GeoRSS driver:
 * advertise OLCCreateField capability

GFT driver:
 * switch http to https for the oauth2 link to improve security

GML driver:
 * add support for multiple geometry columns (RFC 41)
 * add support for reading Finnish National Land Survey Topographic data (MTK GML)
 * add support for support Finnish NLS cadastral data and Inspire cadastral data.
 * add support for Czech RUIAN VFR format
 * add data/gml_registry.xml file to associate feature types with schemas.
 * extend .gfs syntax to be able to fetch OGR fields from XML attributes.
 * extend .gfs syntax to support multiple geometry columns, and define a geometry property name
 * autodiscover all XML attributes as OGR fields when creating .gfs file if GML_ATTRIBUTES_TO_OGR_FIELDS is set to YES (#5418)
 * allow the <ElementPath> in .gfs to have several components that give the full XML path
 * fix writing of .xsd file to avoid fid/gml_id being written as regular fields (#5142)
 * fix writing of global srsName attribute on the global boundedBy.Envelope when all layers have same SRS (#5143)
 * support for writing .gml/.xsd with fields of type StringList, RealList, IntegerList and support for parsing such .xsd files
 * when writing .xsd for a datasource that has fields of type StringList, RealList or IntegerList, advertise SF-1 profile in the .XSD schema
 * recognize xsd:boolean in XSD parsing and map it to String (#5384)
 * add STRIP_PREFIX and WRITE_FEATURE_BOUNDED_BY dataset creation option to help minimizing the size of GML files
 * don't write top <gml:boundedBy> in GML files with multiple layers of different SRS
 * fix segfault when reading a GML file with huge coordinates (#5148)
 * avoid opening our own .xsd files as valid datasources (#5149)
 * make driver thread-safe with Xerces
 * open successfully GML datasources with 0 layers (#249, #5205)
 * fix tweaking of DescribeFeatureType requests
 * support reading WFS 2.0 GetFeature documents with wfs:FeatureCollection as a wfs:member of the top wfs:FeatureCollection
 * fix for crash on certain xlink:href with GML_SKIP_RESOLVE_ELEMS=NONE (#5417)
 * GML geometry: fix duplicated points in GML_FACE_HOLE_NEGATIVE=YES mode (TopoSurface) (#5230)
 * GML geometry: accept CompositeSurface as a child of surfaceMembers (#5369)
 * GML geometry: join multilinestrings to linestrings in rings
 * GML geometry: correctly deal with MultiSurface of Surface of PolygonPatch where a PolygonPatch has only interior ring(s) and no exterior ring (#5421)
 * GML geometry: accept formulations of 'MULTIPOINT EMPTY, MULTILINESTRING EMPTY, MULTIPOLYGON EMPTY and GEOMETRYCOLLECTION EMPTY that are valid GML 3 (and accepted by PostGIS)
 * GML geometry: make use of cs, ts and decimal attributes of (deprecated) gml:coordinates element
 * GML geometry: accept XML header and comments

GPX driver:
 * advertise OLCCreateField capability

ILI driver:
 * add support for multiple geometry columns (RFC 41)
 * use IlisMeta model reader/writer instead of IOM
 * add layers for surface and area geometries

KML driver:
 * output KML that validates the ogckml22.xsd schema by placing <Schema> elements under the <Document> level (#5068)
 * in writing mode, avoid defining an extending schema for the name and description fields (related to #5208)

LIBKML driver:
 * various checks, fixes and improvements related to OGC KML 2.2 Abstract Test Suite
 * add support for reading <gx:Track> as a LINESTRING (#5095)
 * add support for writing and reading <snippet>
 * add support for writing atom:author, atom:link, phonenumber, Region,
   ScreenOverlay, 3D model, StyleMap
 * add support for reading and generating Camera object
 * add layer creation options to generate a LookAt element at layer level
 * if UPDATE_TARGETHREF dataset creation option is defined, a NetworkLinkControl/Update document will be created
 * add dataset creation options to generate a NetworkLinkControl element
 * add dataset and layer creation options LISTSTYLE_ICON_HREF and LISTSTYLE_TYPE
 * add support for writing a NetworkLink
 * add support for creating PhotoOverlay objects
 * add support for creating BalloonStyle elements
 * offer LIBKML_USE_SIMPLEFIELD configuration option can be set to NO to use Data element instead of SimpleField
 * add layer creation option FOLDER to optionally write layers as Folder instead of Document
 * add dataset and layer creation options NAME, VISIBILITY, OPEN, SNIPPET and DESCRIPTION
 * workaround bugs in pretty serializers
 * when writing a .kmz file, put layers .kml docs into a layers/ subdirectory
 * fix mem leaks, and use after free in kml2FeatureDef() (#5240)
 * create document with default namespace set to http://www.opengis.net/kml/2.2
 * when writing, consider empty strings as unset (useful when converting from CSV)
 * don't write empty <Style /> element if OGR_STYLE is empty string
 * transform multigeometry with one single component into single geometry
 * create libkml/ subdirectory entry in .kmz

MITAB driver:
 * support reading MIF file with no associated MID file (when MIF file advertises 'Columns 0') (#5141)

MSSQLSpatial driver:
 * Fix MSSQL to be aware of removed tables (#5071)
 * Eliminate the per table server access when identifying the spatial reference (#5072)
 * Improve detection of geometry column with MSSQL select layer (#4318)
 * Fix for an issue with multicolumn primary keys (#5155)
 * Add support for handling non spatial data tables (#5155)
 * Fix creation of spatial_ref_sys and geometry_columns tables (#5339)

MySQL:
 * robustness for huge coordinates in spatial filter

NAS driver:
 * also accept XML files that have NAS-Operationen_optional.xsd in header
 * make driver thread-safe with Xerces
 * warn when geometry cannot be saved

OCI driver:
 * fix make plugin option

ODBC driver:

OSM driver:
 * support relations with more than 2000 members in a OSM XML file (#5055)
 * make the driver work with PBF files produced by osmconvert.
 * osmconf.ini: report the waterway attribute for the lines layer (#5056)
 * add an option in osmconf.ini to enable creating a 'all_tags' field, combining both fields specifically identified, and other tags
 * always use quoting of key/values in other_tags field (#5096)
 * use alternative implementation of FindNode() that is a bit more efficient when process is CPU-bound
 * fix issue with attribute filtering
 * avoid GetNextFeature() to be blocked in certain conditions in non-interleaved mode (#5404)

PG driver:
 * upgrade to support RFC 41 in read/write (multiple geometry fields)
 * use ST_Estimated_Extent() on table layers if GetExtent() is called with force = 0 (#5427)
 * add a OGR_TRUNCATE configuration option (#5091)
 * let postgres name the constraints to avoid long name truncation resulting in conflicts (#5125)
 * map PG 'numeric' to OFTReal instead of OFTInteger
 * retrieve SRID from geometry value, if not found in geometry_columns (#5131)
 * fix ResolveSRID() when the current user has no select rights on geometry_columns table (#5131)
 * fix retrieval of SRID on a table without SRID constraint, and when the datasource is opened with ' tables=fake' (#5131)
 * robustness for huge coordinates in spatial filter
 * fix delete layer bug on PG 2.0 (#5349)
 * fix to be able to detect version of EntrepriseDB (#5375)
 * Fix UTF-8 encoded string length

PGDump driver:
 * upgrade to support RFC 41 (multiple geometry fields)
 * fix error when inserting a string that has backslash in it with PostgreSQL >= 9.1 (#5160)

REC driver:
 * fix runtime compatibility for Windows

Shapefile driver:
 * fix buffer overflow when creating a field of type Integer with a big width (#5135)
 * delete temporary .cpg file earlier in REPACK
 * fix leak of file descriptor in error code paths
 * speed optimizations when reading geometries from .shp (#5272)
 * add a 2GB_LIMIT=YES layer creation option (and SHAPE_2GB_LIMIT configuration option)
 * .sbn support: increase allowed max depth from 15 to 24 (#5383)

SOSI driver:
 * fix memory leaks, and crashes
 * advertise OLCCreateField capability

S57 driver:
 * make the driver thread-safe
 * remove use of MAX_CLASSES in favor of dynamic sizing (#5227)
 * fix GetFeature() on DSID layer

SQLite/Spatialite driver:
 * Make SQLite SQL dialect compatible with multiple geometry fields (note: however, multiple geom fields is not yet supported by OGR SQLite table layers)
 * implement SetAttributeFilter() on SQL result layers, to directly inject it as a WHERE clause when possible
 * add the OGR_SQLITE_PRAGMA configuration option
 * Spatialite: correctly set proj4text field of spatial_ref_sys when inserting a new SRS in the spatial_ref_sys table (#5174)
 * Spatialite: fix insertion in spatial_ref_sys to avoid issues with non-numeric authority codes (auth_srid)
 * Spatialite: make creation of database much faster with spatialite 4.1 by using InitSpatialMetaData(1) (#5270)
 * Spatialite: use thread-safe initialization for spatialite >= 4.1.2
 * avoid Spatialite views to emit (hidden) errors that cause troubles to MapServer OGR input driver (#5060)
 * attempt to make VirtualOGR (and thus sqlite dialect) work even with a libsqlite3 compiled with SQLITE_OMIT_LOAD_EXTENSION (on Unix only)
 * add warning when calling CreateField() with a field name that is 'ROWID' since it can cause corrupted spatial index
 * serialize StringList as non-truncated strings
 * detection of DateTime/Date/Time column type on the result of a max() / min() function
 * ensure fields of type DateTime/Date/Time are properly recognized if the table is empty (#5426)

VFK driver:
 * fix memory leak
 * change VFK header check (first line starts with '&H')
 * implement OGR_VFK_DB_DELETE configuration option
 * read all data blocks by default
 * fix GetFeatureCount()
 * don't use existing internal db when it's older then original vfk file

VRT driver:
 * add support for multiple geometry columns in OGRVRTLayer, OGRVRTUnionLayer and OGRVRTWarpedLayer (RFC 41)
 * add validation of OGR VRT document against the schema (if libXML2 available);
   can be disabled by setting GDAL_XML_VALIDATION configuration option to NO
 * make relativeToVRT=1 work with CSV:filename or GPSBABEL:driver:filename (#5419)

WFS driver:
 * WFS 2.0: be a good citizen and send TYPENAMES (with a S) for GetFeature request (but still TYPENAME for DescribeFeatureType)
 * accept TYPENAME(S) in URL with characters escaped by '%' (#5354)

XLS driver:
 * don't use sheetId attribute from workbook.xml to link a sheet name to its filename. The first sheet is sheet1.xml, etc...

XPlane driver:
 * accept V1000 APT files

## SWIG Language Bindings

All bindings:
 * include constraints.i in gdal.i so that NONNULL constraints are really applied
 * add Feature.SetFieldBinaryFromHexString()
 * add SpatialReference.EPSGTreatsAsNorthingEasting (#5385)
 * map OGR_L_GetStyleTable(), OGR_L_SetStyleTable(), OGR_DS_GetStyleTable(), OGR_DS_SetStyleTable()
 * Add OGR_STBL_AddStyle() and map all OGR_STBL_ C methods to SWIG

CSharp bindings:
 * Fix handling UTF8 strings in GDAL C# (#4971)
 * Add C# typemaps for RFC-39 functions (#5264)
 * Fix typo in OGRLayerAlg.cs (#5264)
 * Add missing typemaps for C# (#5265)

Java bindings:
 * run 'make ANDROID=yes' in swig/java to generate SWIG bindings that compile for Android (#5107)
 * fix linking issue that is encountered in non libtool builds with g++ 4.6
 * add multireadtest utility
 * respect JAVA_HOME set via configure

Perl bindings:
 * ColorTable method of ColorTable class was documented but ColorEntries was
   implemented. Implemented but deprecated ColorEntries.

Python bindings:
 * fix ref-counting of callable passed to gdal.PushErrorHandler() that could cause segfaults (#5186)
 * make gdal_array.NumericTypeCodeToGDALTypeCode accept numpy dtype arguments (#5223)
 * add default xoff,yoff,xsize,ysize values to ReadRaster calls
 * make ogr.CreateGeometryFromWkt() and SpatialReference.ImportFromWkt() properly validate their argument (#5302)

# GDAL/OGR 1.10.0 Release Notes (r23656 to r25905)

## In a nutshell...

 * New GDAL drivers:
   - ARG: read/write support for ARG datasets (#4591)
   - CTable2: read/write support for CTable2 datum grid shift format
   - DDS: write-only support for DirectDraw Surface format (#5017)
   - IRIS: read support for products generated by the IRIS weather radar software (#4854)
   - MAP:  read OziExplorer .map files (#3380)
   - MBTiles: read-only support for MBTiles rasters (needs libsqlite3)
 * New OGR drivers:
   - ElasticSearch: write-only support to write into ElasticSearch databases (needs libcurl)
   - ODS : read/write support for OpenOffice .ods (Open Document Spreadsheets) (needs libexpat)
   - OSM : read-only support for .osm / .pbf OpenStreetMap files
   - PDF: read/write support for vector/structured PDF files
   - XLSX: read/write support for MS Excel 2007 and later Open Office XML .xlsx spreadsheets (needs libexpat)
 * RFC 39: OGR Layer algebra methods : http://trac.osgeo.org/gdal/wiki/rfc39_ogr_layer_algebra
 * Add a SQL SQLite dialect : http://gdal.org/ogr/ogr_sql_sqlite.html
 * Make GDAL loadable as a SQLite3 extension (named VirtualOGR) (#4782)
 * /vsicurl_streaming/: new virtual file system handler designed to read in streaming mode dynamically generated files
 * GDAL API_PROXY mechanism to run GDAL drivers in a separate process: http://gdal.org/gdal_api_proxy.html
 * Significantly improved drivers : PDF, SQLite, JP2OpenJPEG
 * Add a geocoding client : http://gdal.org/ogr/ogr__geocoding_8h.html
 * Upgrade to EPSG 8.0 database

## New installed files

 * data/ozi_datum.csv
 * data/ozi_ellips.csv
 * data/osmconf.ini
 * include/cpl_progress.h
 * include/cpl_spawn.h
 * bin/gdalserver[.exe]

## Backward compatibility issues

Due to the 2-digit number '10' in GDAL 1.10, the GDAL_VERSION_NUM macro has been changed.
The new advised way of testing the GDAL version number (for GDAL 1.10 or later) at compilation time is :

#ifdef GDAL_COMPUTE_VERSION /* only available in GDAL 1.10 or later */
#if GDAL_VERSION_NUM >= GDAL_COMPUTE_VERSION(1,10,0)
///
#endif
#endif

Testing of previous versions is of course unchanged.

## GDAL/OGR 1.10.0 - General Changes

Build(Unix):
 * New optional dependencies : libpcre (for regular expressions support in SQLite), libxml2 (validation of GML files)
 * --with-python: make it work with python3, and also accept path to python binary as argument of --with-python (#4725)
 * Use nc-config to detect netcdf compilation and linking parameters (#4424)
 * Add frmts/vrt to CONFIG_CFLAGS for development version of gdal-config (needed for postgis 2.0 compilation)
 * Fix compilation failure with iconv on FreeBSD (#4525)
 * Make FileGDBAPI detection work with FileGDBAPI v1.1 and v1.2 (#4570)
 * Fix build on Gentoo with its custom zlib 1.2.6 with the OF macro removed
 * Mark man target as phony (#4629)
 * Add guess for the directory where to find openjdk on Ubuntu 12.04 (#4643)
 * Look for geotiff headers in /usr/include/libgeotiff too (#4706)
 * For install target, create gdalplugins subdirectory in $(DESTDIR)$(INST_LIB)/ (Unix, except MacOSX)
 * Better detection of OpenCL headers and library (#4665)
 * Changed libdap test to use dap-config to detect version when possible

Build(Windows):
 * (Preliminary) support to build INGRES
 * Make CPLGetErrorHandlerUserData() exported
 * Make OGDI include path overridable (to match OSGeo4W's default location)
 * Build and install plugins

## GDAL 1.10.0 - Overview of Changes

Port:
 * CPL Thread API: add condition API, modeled on POSIX pthread_cond_ API
 * Add CPLGetNumCPUs()
 * Deserialize various forms of textual representation for positive/negative infinity
 * Add routine to validate a XML file against its XSD schema (needs libxml2); 'optimize' it for GML files
 * CPLRecodeStub(): for Windows, provide an implementation of UTF8 <--> CPxxx conversions using Windows API
 * Make VSIFileManager::Get() thread-safe
 * Fix thread-safety of CPLOpenShared() (#4848)
 * Add CPLZLibDeflate() and CPLZLibInflate()
 * Add API for OAuth2 authentication protocol.
 * Curl: allows setting the CURLOPT_PROXYAUTH setting through GDAL_PROXY_AUTH=BASIC/NTLM/DIGEST/ANY,
   allow setting CURLOPT_HTTPAUTH through GDAL_HTTP_AUTH=BASIC/NTLM/GSSNEGOTIATE/ANY (#4998)
 * /vsicurl/ and /vsicurl_streaming/ : make it possible to cache the files in RAM with VSI_CACHE = TRUE
 * /vsizip/: fix handling of Eof() that could cause missed last feature(s) of zipped shapefiles (#4748)

Core:
 * Add a DMD_SUBDATASETS driver metadata, and advertise it in relevant drivers (#4902)
 * Fix statistics computation when nodata value is +/- infinity (#4506)
 * GDALNoDataMaskBand: implement IRasterIO() for an optimization in common use case (#4488)
 * GDALVersionInfo(): add BUILD_INFO version string
 * GMLJP2: Fix bad interpretation when Lat/Long (#4657)
 * Set nodata value when creating external overviews so that AVERAGE algorithm works as expected (#4679)
 * EXIFExtractMetadata() moved to gcore/gdalexif.cpp to make it usable for other drivers
 * Fix infinite recursion in GDALOpen() (#4835)
 * GDALRasterBand::IRasterIO() : optimize downsampling/upsampling code path
 * C API: make GDALSetDefaultRAT() accept a NULL RAT. All drivers are ready for that now.
 * GDALRasterBand::GetDefaultHistogram(): change how min and max bounds are computed in the non GDT_Byte case
 * GDALDataset::BlockBasedFlushCache(): fix crash when band has sub-blocking

Algorithms:
 * GSOC Image Correlator work (preliminary state)
 * Warp: divide Lanczos resampling time by at least a factor of 4.
 * Warp: add NUM_THREADS warping option to set the number of threads to use to parallelize the computation part of the warping
 * Warp: do not stop collecting chunks to operate on just because some subchunks fail (#4795)
 * Warp: add mode and average resampling methods (#5049)
 * OpenCL warper: handle errors in set_supported_formats(), fix memory leaks in error code paths, add detection of Intel OpenCL (by the way, Intel OpenCL seems to work properly only with a Float32 working data type)
 * OpenCL warper: fix segmentation fault related to source/destination validity masks (#4840)
 * Geoloc: do not trust pabSuccess in geolocation transformer (#4794)
 * Geoloc: add bilinear interpolation of coordinates from backmap (#4907)
 * Geoloc: add GDALTransformGeolocations() and SWIG binding
 * Add nearest neighbor and cubic interpolation of DEM in GDALRPCTransform (#3634).
   User can set RPC_DEMINTERPOLATION to near, bilinear or cubic to interpolate of input DEM file which set in RPC_DEM. The default interpolation is bilinear.
 * gdal_rasterize: fix problem identifying some connected-8 polygons (#4647)
 * gdal_grid: speed-up dramatically nearest neighbour search (with radius1 == radius2) by using a search quad tree
 * gdal_grid: parallelize processing by specifying the GDAL_NUM_THREADS configuration option (default to ALL_CPUS)
 * gdal_grid: for 'invdist' algorithm with default parameters, use SSE optimized version if available (at compile and runtime). Can be disabled with GDAL_USE_SSE=NO

Utilities:
 * General: make usage message more self-explanatory in case of bad option (#4973)
 * gdalmove.py: New application for "warping" an image by just updating its SRS and geotransform.
 * gdal_edit.py: promote it as an 'official' tool (#4963)
 * gdalwarp: add "-r average" and "-r mode" resampling methods (#5049)
 * gdalwarp: copy metadata and band information from first source dataset and detect for conflicting values, new options -nomd and -cvmd (#3898)
 * gdalwarp: optimization when (-tr and -te) or (-ts and -te) are specified (#4804)
 * gdalwarp: assign color interpretation of source bands to target dataset, in the case of target VRT (#4462)
 * gdalwarp: add -setci option to set the color interpretation of the bands of the target dataset from the source dataset
 * gdal_translate: accept -srcwin or -projwin values that fall partially or completely outside the source raster extent. Introduce -epo and -eco options to error out in those situations.
 * gdallocationinfo: add a -overview overview_level option to specify an overview level, instead of the base band
 * gdalsrsinfo: try to open with GDAL and OGR even if argument is not a file (#4493)
 * gdaldem: add a -combined option to the hillshade mode to compute combined hillshading (#4753)
 * gdaldem: fix color-relief output with driver that has only CreateCopy() capability, and when the source block dimensions are not multiple of the raster dimension (#4764)
 * gdaltindex: add -t_srs option, to transform all input source bounds to same SRS (#4773)
 * gdalbuildvrt: add -a_srs option
 * gdalbuildvrt: add -sd option to select subdataset by its number
 * gdalbuildvrt: add a -b flag (#4992)
 * gdalgrid: increase working buffer of gdal_grid binary to 16 MB
 * gdal_retile.py: Don't pass creation options to the MEM driver used for generating temporary datasets (#4532)
 * gdal_edit.py: make -a_srs option work properly by expanding the user input to WKT; support -a_srs ''
 * gdal_edit.py: add support for -gcp option
 * gdal2tiles.py: make KML output conformant with KML 2.2 (#4536)
 * gdal2tiles.py: OL 2.12 support (#4742)
 * gdal_polygonize.py: add -8 option to select 8 connectedness (#4655)
 * gdal_merge.py, gdalident.py: remove using of glob() API
 * gdal2xyz.py: fix output of -csv mode with multi-band raster
 * gdal_contour / gdal_rasterize / gdal_translate: accept numeric values in scientific format.
 * crs2crs2grid.py: New sample Python script
 * gdalcompare.py: New sample Python scrip to compare GDAL datasets
 * gdal_calc.py: add --co creation option flag (#4964)
 * gdaladdo: add a -b flag
 * pct2rgb.py: deal with color tables with more than 256 entries (#4905)

Multi driver changes:
 * Add support for reading .j2w, .jp2w and .wld files for JP2ECW, JP2MrSID, JP2OPENJPEG and JPEG2000 drivers (#4651)

AAIGrid:
 * Change float format string for AAIGrid to prevent pointless padding/decimals (#3732)

ACE2 driver:
 * Fix typo that prevented dataset to be opened with explicit /vsigzip/ (#4460)

ADRG driver:
 * Various fixes when opening ill-formed datasets.

BAG driver:
 * Fix serious problems with tiled images, particularly when not multiples of tile size (#4548)
 * Added capture of dateTime attribute
 * Support WKT (with Esri style VERTCS) spatial reference
 * Allow WGS84 spatial reference
 * Include compression method in metadata.

BT driver:
 * Fixes for huge files (>2GB) support (#4765)

CEOS2 driver:
 * Add various radarsat-1 related metadata fields (#4996)

DIMAP driver:
 * Check signature in METADATA.DIM, not just file existence
 * Fixed DIMAP2 driver to get the proper absolute path in a specific case

DTED driver:
 * Write the updated value of the partial cell indicator to the file (#4687)
 * Honour 'Longitude count' field of Data Record to deal properly with files with missing columns at the left and/or right of the file (#4711)

ECW driver:
 * Add support for ECW SDK 5.0
 * Improve picking performance on large datasets (#4790)
 * Use ECW SDK to do super-sampling for SDK >= 4.X
 * Expose 256x256 block dimension instead of scanline
 * Workaround a ECW SDK 3.3 bug, when doing a RasterIO() with the total number of bands, but not in the 1,2,..n order (#4234)
 * Add heuristics to detect successive band reading pattern (such as done by QGIS). Beneficial for ECWP

ENVI driver:
 * Add support for writing RPCs and GCPs
 * Add ability to access all ENVI header fields to the ENVI reader (#4735)
 * Write in the ENVI header metadata found in the ENVI metadata domain (#4957)
 * Fix reading of .sta file on 64bit Linux
 * Assume BSQ interleaving when 'interleave' keyword missing or unknown
 * Fix category names writing
 * Remove 'envi fft result' from the blacklist
 * Report wavelength and wavelength_units as band metadata (#3682)

ENVISAT driver:
 * Ported to VSI*L

GeoRaster driver:
 * Add spatialExtent and extentSRID create-options (#4529)
 * Fix JPEG quality not updated on metadata (#4552)
 * Search for RDT as regular table
 * Add support for RPC (#4038)

GIF driver:
 * Add support for giflib 4.2.0 (#4675) and giflib 5.0

GMT driver:
 * Make GMT driver thread-safe by adding a global mutex (since the netcdf library isn't thread-safe)

GTiff driver:
 * Internal libtiff and libgeotiff refreshed from upstream
 * Use EXTRASAMPLE_UNASSALPHA by default (behavior change w.r.t. previous GDAL versions) (#4733)
 * Add support for reading GeoEye *.pvl metadata files (#4465)
 * Lossless CreateCopy'ing() from a JPEG dataset
 * Read EXIF metadata in the EXIF metadata domain
 * Ensure that rowsperstrip is never larger than ysize (#4468)
 * Fix writing of RGBA pixel-interleaved JPEG-compressed TIFF (#4732)
 * Set color interpretation to GCI_PaletteIndex after calling SetColorTable() (#4547)
 * Conversion for 8-bit unpacked CMYK (PHOTOMETRIC_SEPARATED) to RGBA.
 * Maximize EPSG compatibility where PCS is defined (#4607)
 * Ensure that unusual units with an authority node are saved nicely (like EPSG:2066)
 * Add CT_HotineObliqueMercatorAzimuthCenter support
 * Fix PolarStereographic / 9829 support
 * Make sure that GetMetadata() initializes the value of GDALMD_AREA_OR_POINT item, if not already done (#4691)
 * When building overviews, if the image has already an internal mask, then build internal overviews for the mask implicitly
 * Better handling of SetMetadata(a_string) (#4816)
 * Use GTIFAllocDefn/GTIFFreeDefn with libgeotiff 1.4.1+
 * Add support for GEO_NORMALIZE_DISABLE_TOWGS84 (#3309)
 * Improve handling of description and offset/scale without reverting to .aux.xml
 * Workaround defects in libtiff 3.X when generating several overview levels at the same time
 * Special case where the EGM96 Vertical Datum code is misused as a Vertical CS code (#4922)
 * Support unsetting geotiff tags when calling SetGeoTransform([0,1,0,0,0,1]) and SetProjection('')
 * Rework how CSV files are searched w.r.t libgeotiff (#4994)

GRIB driver:
 * Report nodata value (#4433)
 * Fix fgetc signed/unsigned problem for Grib format VSI*L reader (#4603)
 * Avoid caching more than 100 MB in case of dataset with many bands (#4682)
 * uses meshLat as the latitude_of_origin parameter of LCC projection (#4807)

GSAG driver:
 * Fix hangs when reading truncated dataset (#4889)

GS7BG driver:
 * Implement Create() and CreateCopy() (#4707)

GTX driver:
 * Support reading old GTX files where datatype was Float64

GXF driver:
 * Avoid having big buffer on stack (#4852)
 * Avoid locale floating point parsing problems (similar to r24367).
 * Implement continued lines mechanism (#4873)
 * Fix various vulnerabilities / DoS

HDF4 driver:
 * Add ability to increase the maximum number of opened HDF4 files
 * Unix build: avoid issue with system hdfeos library
 * Ensure we do not try to use the grid tile api for non-tilesized chunks (#4672)
 * Preserve more Float32 attribute precision
 * Import HDF USGS GCTP angular parameters as radians
 * Restore conventional add_offset interpretation (#4891)
 * Be more careful about missing dimensions (#4900)
 * Make HDF4 driver thread-safe by adding a global mutex (since the HDF4 library isn't thread-safe)
 * Search for "coremetadata" attribute name instead of "coremetadata."

HDF5 driver:
 * Add support for COSMO-SkyMed metadata (#4160)

HFA driver:
 * Added BASEDATA support for EPT_u1 (#4537)
 * Fix crash on dataset closing when .ige file header is corrupted (#4596)
 * .aux overviews: avoid destroying existing overviews when asking twice in a row to build overviews for exactly the same overview levels (#4831)
 * Fix sizing of RAT string column maxwidth to include null char (#4867)
 * Fix segfault in HFAAuxBuildOverviews with selected bands (#4976)

INGR driver:
 * do not reduce tile size to image size (#4856)
 * Fix value inversion when reading type 9 (bitonal RLE) untiled files (#5030)
 * fix slowness and incorrect random reading with RLE datasets (#4965)
 * Enable reading bitonal rle files wider than 22784 (0x5900) pixels (#5030)
 * Add RESOLUTION metadata/option to read/write DPI (#5030)
 * Add write support for .rle (bitonal rle files) to test the above (#5030)

ISIS3 driver:
 * Ensure scaleFactor defaults to 1 (#4499)

JP2ECW driver:
 * Workaround conflict between ECW SDK deinitialization and GDAL deinitialization, as shown by gdaljp2ecw tests of imageio-ext (#5024)
 * Promote 1bit alpha band of a RGBA dataset to 8 bits to improve general user experience (can be turned off by setting GDAL_ECW_PROMOTE_1BIT_ALPHA_AS_8BIT to NO)

JP2KAK driver:
 * Capture Corder in metadata for user convenience
 * Fix writing of resolution box where the xresolution value was written instead of the yresolution one
 * Skip bands that have different data type when reading multiple bands in IReadBlock() (#4638)
 * Default to less than 250000 lines per tile (#5034)

JP2OpenJPEG driver:
 * Require OpenJPEG 2.0.0 now
 * Use several decoding threads when processing multi-tiles IRasterIO() requests
 * Add support for writing georeferencing
 * Read and write JP2 Res box and translate it from/to TIFFTAG_XRESOLUTION, TIFFTAG_YRESOLUTION and TIFFTAG_RESOLUTIONUNIT metadata items
 * Promote 1bit alpha band of a RGBA dataset to 8 bits to improve general user experience (can be turned off by setting JP2OPENJPEG_PROMOTE_1BIT_ALPHA_AS_8BIT to NO)

JPEG driver:
 * When there are no external overviews built, take advantage of the nature of JPEG compression to expose overviews of level 2, 4 and 8
 * Don't return junk content when requesting xml:XMP but no XMP metadata is present (#4593)
 * add a INTERNAL_MASK creation option to be able to disable appending the ZLib mask if not needed
 * add support for creating a JPEG loss-less file starting with the recent IJG libjpeg v9
   (with -co ARITHMETIC=yes -co BLOCK=1 -co COLOR_TRANSFORM=RGB1)

JPEG2000 driver:
 * do not accept by default source bands of type different from Byte, Int16 or UInt16 since they seem to cause crashes in libjasper.
   This can be overridden, at your own risk, by setting JPEG2000_FORCE_CREATION configuration option to YES (#5002)

KMLSuperOverlay driver:
 * Add read support
 * Remove bogus code that limited generation to one zoom level (#4527)
 * Set minLodPixels to 1 for zoom level 0 (#4721)
 * Fix bad placing of tiles with raster of the extent of a country or more (#4834)
 * Add FIX_ANTIMERIDIAN creation option (#4528)

L1B driver:
 * Add support for NOAA19, METOP-B and guess for METOP-C (#2352)

MG4Lidar driver:
 * Add UTF-8 filename support under Windows (#4612)

NetCDF driver:
 * Fix for gdal_rasterize (#4432)
 * Enable PAM for band histogram and statistics (#4244)
 * Add longitude_of_prime_meridian value to PRIMEM
 * Fix SetNoDataValue() - do not update when already set to new value (#4484)
 * Convert longitude values in [180,360] interval to [-180,180] (#4512) - override with config option GDAL_NETCDF_CENTERLONG_180=0
 * Support 2D GEOLOCATION arrays when a projected variable has coordinates attribute and supporting lon/at arrays (#4513)
 * Ignore coordinate and bounds variables (CF sections 5.2, 5.6 and 7.1) as raster bands, but expose them as subdatasets - this allows opening files with projected SRS (or dimension bounds) directly, without specifying the variable as a subdataset
 * Better support for Gaussian grids - store original latitude values in special Y_VALUES geolocation metadata item and use it for netcdf export (#4514)
 * Write multi-dimensional variables to a single variable (not one for each unrolled band) in CreateCopy() (#2581)
 * Fix handling of km units in netcdf driver and importFromProj4() (#4769)
 * Fix detection of 1 and 2 pixel width/height netcdf datasets (#4874)
 * Fix subdataset data type info (#4932)
 * Make netCDF driver thread-safe by adding a global mutex (since the netcdf library isn't thread-safe)

NITF driver:
 * nitf_spec.xml: add definition of ACFTB and AIMIDB TREs
 * Don't escape DESDATA for sizes >10mb (#4803)
 * Fix NITF creation when both BLOCKA and TRE are passed in (#4958)
 * Allow reading JPEG-in-NITF where JPEG stream dimensions are larger than NITF dimensions (#5001)
 * Support for cases with 2 LUTs

Northwood driver:
 * Fixes for huge files (>2GB) support (#4565, #4645)
 * NWT_GRD: don't advertise scale/offset as they are transparently applied in IReadBlock() (#5839).

PDF driver:
 * Add CreateCopy() support
 * Add update support for georeferencing and metadata
 * Add support for selective layer rendering (only with poppler backend)
 * Add GDAL_PDF_BANDS = 3 or 4 config option to select RGB or RGBA rendering; add GDAL_PDF_RENDERING_OPTIONS config option to enable selective feature rendering by combining VECTOR, BITMAP and TEXT values
 * Fix parsing of some georeferencing (r24022)
 * Recognized ISO georeferencing set at the image level (and not at the page level); expose such images as subdatasets (#4695)
 * Support Poppler 0.20 (and for current Poppler development version 0.23/0.24)
 * UTF-16 support
 * Report registration points as GCPs (OGC Best Practice)
 * Allow building driver with both Poppler and Podofo (testing purposes mostly)
 * Fix crashes on some PDF files with poppler >= 0.17.0 (#4520)
 * Improve rounding of raster dimensions (#4775)
 * With podofo, avoid launching the 'pdftoppm' process in a visible console on Windows (#4864)
 * Select neatline whose description is 'Map Layers' when it is found, to keep the best neatline for USGS PDF Topo
   and add GDAL_PDF_NEATLINE config. option to override that default value.
 * Improve detection of DPI for USGS Topo PDF to get the maximum raster quality (will increase
   reported dataset height and width)
 * Extract USGS Topo PDF embedded metadata in the EMBEDDED_METADATA domain

PNG driver:
 * Internal libpng upgraded to 1.2.50

PostgisRaster driver:
 * Implement CreateCopy and Delete (#4530)
 * Supports reading of tiled raster with irregular blocking and irregular pixel size
 * Cache postgres db connection.
 * Use PG environment variables as fallback when settings are not provided in the connection string. (#4533).
 * Do not report nodata value when there is none (#4414)
 * Removed dependency on the 'rid' column.
 * Fix to make SQL queries with un-rounded floating point string representations. (#4736)
 * Fix overview support

Rasterlite driver:
 * Support all resampling methods for internal overviews (#4740)
 * Fix overview support with multi-table datasets (#4568, #4737)
 * Add RASTERLITE_OVR_OPTIONS configuration option to specify options for the tiles of the internal overviews

RMF driver:
 * Fix incorrect zone number detection for Transverse Mercator (#4766)

RPFTOC driver:
 * Relax SanityCheckOK() to avoid rejecting valid CIB datasets (#4791)
 * Avoid selecting a color table that is full black
 * Add missing NITF series code 'TF' for 'TFC' (and fix typo in some other long descriptions) (#4776)

RS2 driver:
 * added various metadata fields (#4997)

SDTS driver:
 * Various fixes when opening ill-formed datasets.

SRP driver:
 * Various fixes when opening ill-formed datasets.

TIL driver:
 * Fix TIL driver using overview (#3482)
 * Add set projection and geotransformation for TILDataset

TSX driver:
 * Fix crashes in Identify() with certain filenames, and the file is empty or doesn't exist (#4622)

VRT driver:
 * VRTWarpedDataset: add INIT_DEST=0 if no INIT_DEST specified (#4571)
 * VRTFilteredSource: fix RasterIO() to take into account source and dest windows (#4616)
 * avoid crashes due to int overflow when dealing with requests filling a buffer larger than 2 GB (#4815)
 * VRTSourcedRasterBand: override ComputeRasterMinMax(), ComputeStatistics() and GetHistogram() to make them run on sources, only when there's one VRTSimpleSource covering the whole VRTSourcedRasterBand
 * solve issue when the VRT is a symlink and that the sources filenames are RelativeToVRT (#4999)
 * Fix relariveToVRT option in VRTRawRasterBand (#5033)

WCS driver:
 * Support version 1.1.2

WebP driver:
 * Allow reading/writing alpha channel (libwebp >= 0.1.4)
 * Add support for reading XMP metadata in the xml:XMP metadata domain

WMS driver:
 * Implement GetMetadataItem(Pixel_iCol_iLine, LocationInfo) to retrieve pixel attributes
 * Retrieve nodata, min and max values, defined per band or per dataset in the config file (#4613)
 * Add GetColorTable(), used by the TiledWMS mini driver (#4613)
 * Update TiledWMS mini-driver to support new variable substitution mechanism, min/max/nodata settings, color table support (#4613)
 * Add a <UserPwd> element in the XML service description file

XYZ driver:
 * avoid rescanning output file at end of CreateCopy()

## OGR 1.10.0 - Overview of Changes

Core:
 * Add OGRLayerDecorator class (decorator base class for OGRLayer),
 * Add OGRWarpedLayer class (on-the-fly reprojection of a base layer)
 * Add OGRUnionLayer class (on-the-fly concatenation of several base layers)
 * Add OGRFieldDefn::IsSame() and OGRFeatureDefn::IsSame()
 * Add OGRAbstractProxiedLayer, OGRProxiedLayer and OGRLayerPool classes
 * OGRGeometry: set SRS of geometries created via GEOS functions from the source geometry (idem for geometries returned by OGR_G_ForceXXXX() methods) (#4572)
 * OGRFeature: Add OGR_SETFIELD_NUMERIC_WARNING option to issue a warning when not fully
               numeric values are assigned to numeric fields. If the field type is Integer,
               then also warn if the long value doesn't fit on a int.
 * Add OGR_G_PointOnSurface() and add it to SWIG bindings
 * Add OGR_G_ForceToLineString / OGRGeometryFactory::forceToLineString to join
   connected segments in line strings.
 * Better implementation of getDimension() for OGRMultiPoint, OGRMultiLineString, OGRMultiPolygon and OGRGeometryCollection
 * Fix incorrect rounding in OGRFormatDouble that affected geometry WKT output (#4614)
 * OGRSQL: support UNION ALL of several SELECTs
 * OGRSQL: defer ORDER BY evaluation until necessary, so that a spatial filter can be taken into account after layer creation
 * OGRSQL: allow using indexes when OR or AND expressions are found in the WHERE clause
 * OGRSQL: fix incorrect result with more than 2 JOINs and SELECT with expressions with field names (#4521)
 * OGRSQL: fix 'SELECT MAX(OGR_GEOM_AREA) FROM XXXX' (#4633)
 * OGRSQL: fix invalid conversion from float to integer (#4634)
 * OGRSQL: fix behavior of binary operations when one operand is a NULL value
 * OGRDataSource::CopyLayer(): take into account field renaming by output driver (e.g. Shapefile driver that might truncated field names) (#4667)
 * OGRLayer::FilterGeometry() : speed-up improvement in some cases and  use GEOSPreparedIntersects() when available (r23953, r25268)
 * OGRLayer::SetNextByIndex(): return OGRERR_FAILURE if index < 0
 * OGRLineString::setPoint( int iPoint, OGRPoint * poPoint ) : avoid promoting the line to 25D if the point is only 2D (#4688)
 * OGRGeometry::Centroid(): make it work properly on POINT EMPTY with latest SVN geos version
 * Add reversePoints() method on linestring

OGRSpatialReference:
 * Add support for OGC URLs (#4752)
 * add dumpReadable() method on OGRSpatialReference
 * Differentiation between Hotine Oblique Mercator (aka Variant A) and Oblique Mercator (aka Variant B) (#104, #2745)
 * EPSG: Map methods 1028 and 1029 to normal equidistant cylindrical (#4589)
 * EPSG: add support for EPSG:5514 (Krovak East North)
 * EPSG: Add EPSGTreatsAsNorthingEasting() to deal with CRS with non-GIS friendly northing/easting axis order similarly as what was done with lat/long geographic SRS, and automatically do coord swapping in GML/WFS drivers in that case (#4329)
 * Ozi datum support: support all Ozi datums (#3929), support UTM projection
 * USGS: support USGS angular parameters in radians
 * MorphFromESRI() fix WKT : compare SPHEROID and PRIMEM parameters instead of names (#4673)
 * MorphToESRI(): Add common New Zealand GEOGCS values (#4849)
 * OSR ESRI: add GCS mapping name for ETRS89
 * ogr_srs_esri: Make InitDatumMappingTable() thread-safe
 * ecw_cs.wkt: Add entry for European Terrestrial Reference System 1989.
 * pci_datum.txt: Add entry for D894
 * accept "+proj=XXXX [...] +wktext" as a valid PROJ.4 string, even if projection is unhandled by OGR
 * recognize +proj=etmerc when importing from PROJ.4 string; and output +proj=etmerc when exporting Transverse_Mercator to PROJ.4 string if OSR_USE_ETMERC = YES (#4853)

Utilities:
 * ogr2ogr: add a -dim option to force the coordinate dimension to 2 or 3
 * ogr2ogr: accept -nlt PROMOTE_TO_MULTI to ease conversion from shapefiles to PostGIS, by auto-promoting polygons to multipolygons and linestrings to multilinestrings
 * ogr2ogr: add -gcp, -order n and -tps options to georeference ungeoreferenced vectors (#4604)
 * ogr2ogr: fix -select with shapefile output when specified field name case doesn't match source field name case (#4502)
 * ogr2ogr: correctly deal with filenames that begin with 'polygon' or 'multipolygon' as arguments of -clipsrc (#4590)
 * ogr2ogr: return non-zero exit code if the -sql triggers an error (#4870)
 * ogr2ogr: make -t_srs work when there's no per layer source SRS, but there's per feature SRS
 * ogr2ogr: add a -datelineoffset option  to provide users with capability to set different offsets than 170 to -170 (#4098)
 * ogr2ogr: add a -fieldmap option (#5021)
 * ogr2ogr: automatically rename duplicated field names of source layer so that the target layer has unique field names
 * ogrupdate.py: new sample script to update an OGR datasource from another one, by trying to identify matches between the 2 datasources
 * ogr_layer_algebra.py: new sample script to use OGR layer algebra operations

Multi driver changes:
 * Fix bad AND priority when spatial and attribute filter are combined in PG, MySQL, SQLite MSSQLSpatial and VRT drivers (#4507)

AVCE00 driver:
 * Fix GetFeatureCount() when an attribute or spatial filter is set

AVCBin driver:
 * Fix EOF test (#3031)

CSV driver:
 * Allow creating /vsimem/foo.csv
 * Detect and remove UTF-8 BOM marker if found (#4623)
 * Fix handling of empty column names in header (#4654)
 * Allow creating a new .csv file in a directory where there are invalid .csv
   files (#4824)
 * Use a trick so that the CSV driver creates valid single column files (#4824)
 * Add WRITE_BOM option to CSV driver to write UTF8 BOM for improved Excel/unicode compatibility (#4844)
 * Don't turn \r\n into \n in CSV field output, regardless of LINEFORMAT settings (#4452)
 * Don't left-pad numbers in CSV output when a width is set (#4469)
 * Add support for opening .tsv files, in particular the specific Eurostat .tsv files

DGN driver:
 * Add support to read and write font name in the style (#3392)

DXF driver:
 * Write HATCH (Polygon) compatible with other DXF viewers
 * Write layer geometry extent in file header (#4618)
 * Ignore Spline frame control points for VERTEX of POLYLINE (#4683)
 * Add color to POINT, INSERT and HATCH
 * Take into account extrusion vector to transform from OCS to WCS for MTEXT, TEXT, POINT, LINE, CIRCLE, ELLIPSE, ARC, SPLINE and HATCH (#4842)

FileGDB driver:
 * Add the FGDB_BULK_LOAD configuration option (#4420)
 * Do compulsory field name laundering. (#4458)
 * Add special SQL commands 'GetLayerDefinition a_layer_name' and 'GetLayerMetadata a_layer_name'
 * Implement SetFeature(), DeleteFeature() and DeleteField()
 * Fix inserting features in a layer of geometry type wkbNone
 * Define CLSID/EXTCLSID to fix Feature Class alias being ignored in ArcMap (#4477)
 * Use more sensible default values for tolerance and scale parameters (#4455)
 * Use ESRI SRS DB to find the WKT definition to use when creating a layer (#4838)
 * Fix the way empty geometries are written
 * Add read support for fields of type GlobalID or GUID (#4882)
 * Add XML_DEFINITION layer creation option
 * Support setting fields of type SmallInteger, Float and GUID

Geoconcept driver:
 * Partial support of relaxed GXT export syntax and fix when line is incomplete (#4983)

GeoJSON driver:
 * Fix HTTP HEADERS field send to server (#4546)
 * GeoJSON: write crs object as a FeatureCollection attribute in generated GeoJSON files (#4995); on read, strip AXIS nodes

Geomedia driver:
 * Fix loading of 'boundary' geometries when they are 2.5D or contain more than one inner rings (#4734)

GFT driver:
 * Use OAuth2 authentication (was ClientLogin in previous versions).

GML driver:
 * Add PREFIX and TARGET_NAMESPACE dataset creation options
 * Fix typo when writing geometry type name of MultiLineString in GML3 .xsd (#4674)
 * CreateFeature(): assign spatial ref when it is unset in source feature, even for GML2 case (fixes non-compliance with missing srsName on multi-geometries)
 * Make filtering of features based on OGR_GEOMETRY work (#4428)
 * Avoid point duplication concatenating GML curve segments (#4451)
 * Add special SQL command 'SELECT ValidateSchema()'
 * Allow reading srsDimension attribute when set on LineString element, and not on posList (#4663)
 * Partial support for reading GML 3.3 compat encoding profile, limited to <gmlce:SimplePolygon>, <gmlce:SimpleRectangle>, <gmlce:SimpleTriangle>, <gmlce:SimpleMultiPoint> elements
 * Support WFS GetFeature response document to be piped and opened with /vsistdin/
 * Support specifying connection string as 'filename.gml,xsd=some_filename.xsd' to explicitly provide a XSD
 * Improve detection of extent and srs for WFS 2.0
 * Allow ISO-8859-15 encoded files to be used by Expat parser (#4829)
 * Handle CompositeCurve like MultiCurve (for NAS)
 * Remove duplicate points in rings
 * Add OGR_ARC_MINLENGTH to limit the number of segments in interpolated arcs
 * When there are several geometries per feature, use geometry inside <XX:geometry> element for Inspire compatibility
 * Return per-feature SRS if there's no global SRS for a layer
 * Parse correctly <gml:outerBoundaryIs> when there are attributes in the element (#4934)
 * Recognize <gml:GeodesicString>
 * Recognize <gml:Envelope> elements (#4941)
 * Fix crash when reading CityGML attribute with empty string (#4975)

ILI driver:
 * Support for format codes (#3972)
 * ILI1: Use Topic name from model in itf output
 * ILI1: Recode ISO 8859-1 strings
 * ILI1: Various fixes related to enumerations
 * ILI2: Fix reading with models

Ingres driver:
 * Add effuser and dbpwd connection parameters.
 * Enhance EPSG search bath on WKT AUTH ids.
 * Use system defined sequence for fid instead of global (#4567)
 * Fix a name mixup when creating the table (#4567)

KML driver:
 * Report empty layers when there is only empty layers; don't error out on empty document (#4511)
 * Recognize file whose root element is <Document> and not <kml>

Idrisi driver:
 * Add support for reading attributes from .AVL / .ADC files
 * Ignore nodata value when computing min/max of CreateCopy()'ed dataset
 * Avoid setting unset values of mean and stddev to PAM (#4878)

LIBKML driver:
 * Add stylemap support
 * Add support for GroundOverlay reading (#4738)
 * Fix memory leak in OGRLIBKMLDataSource::FindSchema() when a schema is referenced by name and not by id (#4862)
 * Correct use of temporary variable for schema field names (#4883)
 * Update the layer class internal feature count when a new feature is added

MITAB driver:
 * Fix unwanted changes in data types while converting a datasource to MIF (#3853)
 * Fix incorrect handling of Mollweide projection (#4628)
 * Fix double free in OGRTABDataSource::Create() when exiting with error (#4730)
 * Add datum mapping between EPSG/authority codes and the MapInfo definitions. Falls back to old behavior of name/string matching. (#481)
 * Avoid negative zeros in TOWGS84 read from TAB file (#4931)

MSSQLSpatial driver:
 * Report DeleteFeature/DeleteLayer capabilities
 * Fix the parser to swap coordinates with geography data type (#4642)
 * Fix to read multipoint geometries correctly (#4781)
 * Fix to read 3D geometries correctly (#4806, #4626)
 * Use MSSQL catalog if geometry_columns doesn't exist (#4967)
 * Utilize OGRFieldDefn::IsIgnored() (#4534)
 * Remove requirement for identity fid column in MSSQL tables (#4438)

MySQL:
 * Enable auto reconnect to MySQL (#4819)
 * Reset field width and precision when converting from an unhandled field type to TEXT (#4951)

NAS driver:
 * Better support for wfsext:Replace (#4555)
 * Skip elements "zeigtAufExternes" and "objektkoordinaten" (fixes PostNAS #3 and #15)
 * Issue a warning when geometry is overwritten, invalid geometry is found or featureless geometry appears
 * Issue a debug message when a existing attribute is overwritten (to catch array)
 * Use forceToLineString() on line strings (fixes PostNAS #18)
 * Handle MultiCurve and CompositeCurve
 * Merge multilinestrings
 * Accept XML files that have AAA-Fachschema.xsd in header (and remove trailing whitespace)
 * Add EPSG:25833

NULL driver:
 * No-op output driver for debugging/benchmarking purpose (Not included in build process)

OCI driver:
 * Fix for index creation when layers are created (#4497)

ODBC driver:
 * Allow opening directly *non-spatial* MS Access .MDB databases (on Windows only)
 * Make SetAttributeFilter(NULL) work (#4821)
 * Add optimized GetFeatureCount() implementation

NTF driver:
 * Various fixes when opening ill-formed datasets.

OCI driver:
 * Fix issue with pre-existing tables with laundered names not being recognized (#4966)
 * Make sure the FID counter is correctly initialized when data is appended (#4966)
 * Better quoting of identifiers (#4966)

PGeo driver:
 * PGeo / Geomedia : remove heuristics that tried to identify if a MDB file belonged to the PGeo or Geomedia driver (#4498)

PG driver:
 * Add a 'COLUMN_TYPES' layer creation option (#4788)
 * Fix insertion of features with FID set in COPY mode (#4495)
 * Honour datasource read-only mode in CreateField(), CreateFeature(), SetFeature() and DeleteFeature() (#4620)
 * Avoid fatal error when the public schema is absent (#4611)
 * Differ SRS evaluation on SQL layers (#4644)
 * Optimize SRID fetching on SQL result layer; compatibility with PostGIS 2.0 by using ST_SRID (#4699, #4700)
 * Always fetch the SRS to attach it to feature geometry of result SQL layers.

PGDump driver:
 * Add a 'COLUMN_TYPES' layer creation option (#4788)

REC driver:
 * Add robustness checks against corrupted files

SDE driver:
 * Add support for CLOB and NCLOB data type (#4801)

Shapefile driver:
 * Add support for reading ESRI .sbn spatial index (#4719)
 * Add deferred layer loading
 * Implement auto-growing of string and integer columns
 * Add a special SQL command 'RESIZE table_name' to resize (shrink) fields to their optimum size, also available as a RESIZE=YES layer creation option for convenience
 * Recode field name from UTF-8 to DBF encoding in CreateField()
 * In creation, limit fields of type OFTString to a width of 254 characters (#5052)
 * Spatial index optimization (#4472)
 * Fix GetFeatureCount() when spatial filter set, especially on big-endian hosts (#4491)
 * Fixed wrong return value of OLCStringsAsUTF8 in OGRShapeLayer::TestCapability if GDAL was compiled without iconv support (#4650)
 * Support properly creating layers that include dot character
 * Avoid assert() if SetFeature() is called on a feature with invalid FID (#4727)
 * Correctly deal with .cpg files containing 8859xx string (#4743)
 * Make TestCapability(OLCFastFeatureCount) return TRUE when an attribute filter is set only if attribute indices can be used
 * Shapelib: Fix memory leaks in error code path of DBFCreateLL() (#4860)
 * Deal better with shapefile directories with foo.shp and FOO.DBF, particularly for REPACK support, and particularly for Windows OS (on Linux, foo.shp and FOO.DBF will be 2 different layers)
 * Delete temporary _packed.cpg file generated during REPACK of a layer whose .dbf has an accompanying .cpg file
 * In DeleteDataSource(), delete .cpg file if existing

S57 driver:
 * Various fixes when opening ill-formed datasets.
 * Preliminary support for FFPT/FFPC update records (#5028)
 * Add RECODE_BY_DSS suboption to OGR_S57_OPTIONS configuration option that can be set to YES so that the attribute values are recoded to UTF-8, from the character encoding specified in the S57 DSSI record (#5048, #3421, adapted from patch by julius6)

SQLite/Spatialite driver:
 * Add support for tables with multiple geometry columns (#4768)
 * Add (preliminary) support for SpatiaLite 4.0 database changes (#4784)
 * Make GDAL loadable as a SQLite3 extension (named VirtualOGR) (#4782)
 * Add support for OFTDateTime/OFTDate/OFTTime field types
 * Add a SRID layer creation option
 * Make REGEXP function available by using libpcre (#4823)
 * Add a COMPRESS_COLUMNS layer creation option to make string columns ZLib compressed
 * Implement minimal set of spatial functions if SpatiaLite isn't available
 * SpatiaLite: make use of spatial filter on result layers when the SQL expression is simple enough (no join, etc...) (#4508)
 * SpatiaLite: spatial index can be used even when linking against regular SQLite (#4632)
 * SpatiaLite: to improve performance, defer spatial index creation at layer closing or when a spatial request is done
 * SpatiaLite: use SpatiaLite 'layer_statistics' and 'spatialite_history' to cache the row count and extent of (spatial) layers
 * Spatialite: add support for reading Spatialite 4.0 statistics (filling them on the fly still not implemented)
 * SpatiaLite: for SpatiaLite 4.0, translate INIT_WITH_EPSG=NO into InitSpatialMetaData('NONE') to avoid filling the spatial_ref_sys table
 * On SQL result layers, report the SRS of the geometry of the first feature as the layer SRS
 * Deferred layer definition building for table and view layers
 * Speed-up opening of a result layer that has an ORDER BY
 * Cache GetExtent() result
 * Allow inserting empty feature
 * Return empty layer when SELECT returns 0 rows (#4684)
 * Add compatibility for newer SQLite versions when using the VFS layer (#4783)
 * Add missing column name quoting

TIGER driver:
 * Fix opening TIGER datasource by full file name (#4443)
 * Updated to use VSI*L

VFK driver:
 * SQLite is now a compulsory dependency for VFK
 * Store VFK data in SQLite (in-memory) database
 * Don't read whole file into buffer, but only on request
 * id property of data records in VFK file exceed int limit -> use GUIntBig for id values
 * Fix reading multi-line data records
 * Recode string feature properties - convert from cp-1250 to utf-8
 * Skip invalid VFK features

VRT driver:
 * Add <OGRVRTWarpedLayer> (on-the-fly reprojection of a base layer)
 * Add <OGRVRTUnionLayer> (on-the-fly concatenation of several base layers)
 * Add <FeatureCount>, <ExtentXMin>, <ExtentYMin>, <ExtentXMax>, <ExtentYMax> to <OGRVRTLayer>
 * Add an optional 'dialect' attribute on the <SrcSQL> element
 * Fix OGR VRT sensitive to whitespace and <?xml> nodes (#4582)
 * Optimizations to avoid feature translation when possible
 * Implement GetFIDColumn() (#4637)
 * Implement StartTransaction(), CommitTransaction() and RollbackTransaction() by forwarding to the source layer
 * Auto enable bAttrFilterPassThrough when possible

WFS driver:
 * WFS paging: change default base start index to 0 (was 1 before), as now clarified by OGC (#4504)
 * Accept several type names if the TYPENAME parameter is specified
 * Forward SQL ORDER BY clause as a WFS SORTBY for WFS >= 1.1.0
 * Fix spatial filter with WFS 2.0 GeoServer
 * Check that left-side of a binary operator in an attribute filter is a property name before submitting it to server-side
 * Major overhaul of URL-escaping
 * Fix issues when querying the WFSLayerMetadata and that one of the field contains double-quote characters (#4796)
 * Remove auto-added ACCEPTVERSIONS=1.0.0,1.1.1 - when none of VERSION or ACCEPTVERSIONS were specified - because it does not work with WFS 2.0 only servers
 * Automagically convert MAXFEATURES= to COUNT= if people still (wrongly) used it for WFS 2.0
 * Honour paging when running GetFeatureCount() and that RESULTTYPE=HITS isn't available (e.g. WFS 1.0.0) (#4953)
 * Optimize WFS 1.0 (or WFS 1.1.0 where RESULTTYPE=HITS isn't available) so that in some circumstances the GML stream
   downloaded is used to compute GetFeatureCount() and GetExtent() together
 * Fix segfault on non-GML output when there's SRS axis swapping but the feature has no geometry (#5031)

XLS driver:
 * Set FID to the row number in the spreadsheet software (first row being 1). In case OGR detects a header line, the first feature will then be assigned a FID of 2 (#4586)
 * Non-ascii path support for Windows (#4927)

## SWIG Language Bindings

All bindings:
 * Add VSIReadDirRecursive() (#4658)
 * Add a osr.CreateCoordinateTransformation(src, dst) method (and for Java, a static method CoordinateTransformation.CreateCoordinateTransformation(src, dst) (#4836)
 * Add ogr.ForceToLineString()
 * Clear error before OGR_Dr_Open() (#4955)
 * Add a SetErrorHandler(handler_name) method

CSharp bindings:
 * Add C# signature for FileFromMemBuffer that accepts byte array (#4701)

Java bindings:
 * Fix compilation issue with SWIG 2.0.6 on Java bindings (#4669)
 * New test application: ogrtindex.java
 * Fix values of gdalconst.DCAP_* and gdalconst.DMD_* constants (#4828)
 * Fix check for opaque colors in getIndexColorModel()

Perl bindings:
 * Specify module files to install
 * Return values have to be mortal, this was not the case in many instances.
 * New method Driver::Name aka GetName
 * doc target in GNUmakefile to call doxygen
 * Default to first band in GetRasterBand.
 * New method Geo::OGR::Layer::DataSource
 * New method Geo::OGR::Layer::HasField
 * Geometry method accepts geometries in Perl structures
 * Fixed a bug in FeatureDefn::create which changed the fields.
 * New experimental methods ForFeatures and ForGeometries.
 * InsertFeature, Tuple and Row methods use the Tuple and Row methods from Feature.
 * Do not use pattern my var = value if ...; as it seemingly may cause unexpected things.
   target_key is optional argument.
 * Allow setting geometry type with schema argument.
 * Fix incorrect behavior of Geo::OGR::Geometry method Points in the case of a Point (#4833)
 * Preserve the coordinate dimension in Move method

Python bindings:
 * setup.py: Changes to run without setuptools (#4693)
 * setup.py: Automatically run 2to3 for Python3
 * Define __nonzero__ on Layer object to avoid GetFeatureCount() being called behind our back when doing 'if a_layer:' (#4758)
 * Fix performance problem when instantiating Feature, especially with Python 3
 * Add RasterBand.ReadBlock(), mostly for driver testing
 * Reject strings when array of strings are expected
 * make gdal.PushErrorHandler() also accept a Python error handler function as an argument (#4993)
 * Fix Feature.ExportToJSon() to write the id attribute when it is 0 (the undefined value is NullFID ## -1)

# GDAL/OGR 1.9.0 Release Notes

## In a nutshell...

 * New GDAL drivers: ACE2, CTG, E00GRID, ECRGTOC, GRASSASCIIGrid, GTA, NGSGEOID, SNODAS, WebP, ZMap
 * New OGR drivers:  ARCGEN, CouchDB, DWG, EDIGEO, FileGDB, Geomedia, GFT, IDRISI, MDB, SEGUKOOA, SEGY, SVG, XLS
 * Significantly improved drivers: NetCDF
 * Encoding support for shapefile/dbf (#882)
 * RFC 35: Delete, reorder and alter field definitions of OGR layers
 * RFC 37: Add mechanism to provide user data to CPLErrorHandler (#4295)
 * gdalsrsinfo: new supported utility to report SRS in various form (supersedes testepsg)

## New installed files

 * data/nitf_spec.xml and data/nitf_spec.xsd

## Backward compatibility issues

 * GTiff: ensure false easting/northing in geotiff geokeys are treated as being in geosys units (#3901)
 * GRIB: Fix grid vs cell-center convention (#2637)
 * OGR SQL: with DISTINCT, consider null values are such, and not as empty string (#4353)

## GDAL/OGR 1.9.0 - General Changes

Build(Unix):
 * Add --with-rename-internal-libtiff-symbols and --with-rename-internal-libgeotiff-symbols
   flags in order to safely link against an external libtiff (3.X) and a GDAL built with
   internal libtiff (4.0) support (#4144)
 * Add --with-mdb --with-java,--with-jvm-lib, --with-jvm-lib-add-rpath options
 * Add --with-podofo, --with-podofo-lib, --with-podofo-extra-lib-for-test options
 * Add --with-armadillo
 * Update to libtool 2.4
 * Fix linking against static libkml (#3909)
 * Fix Xerces detection by using LIBS instead of LDFLAGS (#4195)
 * Check for .dylib too, when configuring MrSID SDK paths (#3910)
 * Fix wrong include order in GNUmakefile of GPX and GeoRSS drivers (#3948)
 * cpl_strtod.cpp: Enable android support (#3952).
 * ensure swig-modules depends on lib-target so make -j works with swig bindings
 * Change how we check for GEOS >= 3.1  (#3990)
 * Define SDE64 on at least x86_64 platforms (#4051)
 * Make ./configure --with-rasdaman=yes work (#4349)
 * MinGW cross compilation: clear GEOS_CFLAGS and XERCES_CFLAGS
   if headers found in /usr/include, do not use Unix 64 bit IO
 * MinGW build: define __MSVCRT_VERSION__ to 0x0601 if not already set

Build(Windows):
 * Move MSVC warning disabling to nmake.opt, add SOFTWARNFLAGS for external code
 * Use nmake.local (#3959)
 * cpl_config.h.vc: fix up so it also works with mingw (#3960)
 * Build testepsg utility by default when OGR is enabled (#2554)

## GDAL 1.9.0 - Overview of Changes

Port:
 * /vsigzip/ : Avoid reading beyond file size in case of uncompressed/stored files in zip (#3908)
 * /vsicurl/ : Better support for escaped and UTF-8 characters
 * /vsicurl/ : speed-up with a per-thread Curl connection cache
 * /vsicurl/ : read https directory listing
 * /vsicurl/ : look for GDAL_DISABLE_READDIR_ON_OPEN configuration option in
   Open() and Stat() to avoid trying fetching the directory file list
 * /vsicurl/ : fix performance problem when parsing large directory listings (#4164)
 * /vsicurl/ : recognize listing of Apache 1.3
 * /vsicurl/ : fix ReadDir() after reading a file on the same server
 * /vsicurl/ : fetch more info (size, date) when listing FTP or HTTP directories and save it in cache; use those info for ReadDir() and Stat()
 * /vsicurl/: accept 225 as a valid response code for FTP downloads (#4365)
 * /vsicurl/ : add CPL_VSIL_CURL_ALLOWED_EXTENSIONS configuration option that can be used to restrict files whose existence is going to be tested.
 * /vsitar/ : Recognize additional .tar files with slightly header differences
 * /vsizip/ : wrap the returned file handle in a BufferedReader
 * /vsizip/ : fix 1900 year offset for year returned by VSIStatL()
 * /vsizip and /vsitar: remove leading './' pattern at the beginning of filenames contained in the archive
 * /vsistdout_redirect/ : New virtual file system driver that has the same
   behavior as /vsistdout/ (write-only FS) except it can redirect the output to
   any VSIVirtualFile instead of only stdout (useful for debugging purposes)
 * Implement VSI*L read caching - useful for crappy io environments like Amazon
 * VSI*L: Add Truncate() virtual method and implement it for unix, win32 and /vsimem file systems
 * VSI*L: Add ReadMultiRange() virtual method to read several ranges of data in single call; add an optimized implementation for /vsicurl/
 * VSIFEofL(): make it more POSIX compliant.
 * Fine tune CPLCorrespondingPaths() for different basenames when paths involved.
 * VSIWin32FilesystemHandler::Open() : implement append mode.  Needed by ISIS2 driver with attached label (#3944)
 * CPLString: add case insensitive find operator (ifind)
 * RFC23: Add the iconv() based implementation of the CPLRecode() function (#3950)
 * Preliminary support for wchar_t with iconv recode (#4135)
 * Avoid calling setlocale if we are already in the C locale, or GDAL_DISABLE_CPLLOCALEC is TRUE (#3979)
 * CPLMiniXML: emit warnings when encountering non-conformant XML that is however accepted by the parser
 * add CPLBase64Encode(); move cpl_base64.h contents to cpl_string.h
 * Use CRITICAL_SECTION instead of Mutex on win32
 * CPLHTTPFetch(): Add a CLOSE_PERSISTENT option to close the persistent sessions
 * CPLHTTPFetch(): Add support for "NEGOTIATE" http auth mechanism
 * CPLHTTPFetch(): Add a CUSTOMREQUEST option
 * VSIBufferedReaderHandle: fix Eof()
 * Add CPLStringList class
 * Add CPLEmergencyError() - to call when services are too screwed up for normal error services to work (#4175)
 * CPLEscapeString(,,CPLES_URL) : don't escape dot character; fix escaping of characters whose code >= 128

Core:
 * Provide for ABI specific plugin subdirectories on all platforms
 * Force cleanup of datasets when destroying the dataset manager
 * Add a GDALDataset::CloseDependentDatasets() that can be used by GDALDriverManager::~GDALDriverManager() to safely close remaining opened datasets (#3954)
 * Add GDALRasterBand::ReportError() and GDALDataset::ReportError() to prepend dataset name (and band) before error message (#4242)
 * Fix performance problem when serializing huge color tables, metadata, CategoryNames and GCPs to VRT/PAM (#3961)
 * Be careful about Nan complex values getting histogram, avoid locale issues with statistics metadata
 * GDALRasterBand::IRasterIO() default implementation : don't try to use full-res band if I/O failed on the appropriate overview band (for WMS errors)
 * RasterIO: Return earlier when a write error occurred while flushing dirty block
 * GDAL_DISABLE_READDIR_ON_OPEN can be set to EMPTY_DIR to avoid reading the dir, but it set an empty dir to avoid looking for auxiliary files
 * Use sibling file list to look for .aux.xml, .aux, .ovr, world files, tab files
 * Add GDALFindAssociatedFile() (#4008)
 * PAM: Make sure GCPs loaded from a .aux.xml override any existing ones from other sources, like an .aux file
 * PAM: Add cloning of CategoryNames
 * PAM : PamFindMatchingHistogram() - fix floating-point comparison
 * GMLJP2: Use http://www.opengis.net/gml as the schemaLocation
 * GMLJP2: Support for capturing and writing page resolution in a TIFF compatible way (#3847)
 * GDALJP2Box::SetType() : remove byte-swapping so that SetType()/GetType() correctly round-trips. Do appropriate changes in JP2KAK and ECW drivers. (#4239)
 * GDALReplicateWord(): fix off-by-one error initialization (#4090)

Algorithms:
 * polygonize: Added GDALFPolygonize() as an alternative version of GDALPolygonize() using 32b float buffers instead of int32 ones. (#4005)
 * gdalwarp: take into account memory needed by DstDensity float mask (#4042)
 * rasterfill: create working file as a bigtiff if at all needed (#4088)
 * gdalrasterize: use double instead of float to avoid precision issues (#4292)

Utilities:
 * gdalsrsinfo: new supported utility to report SRS in various form (supersedes testepsg)
 * gdalinfo: add '-nofl' option to only display the first file of the file list
 * gdalinfo: add '-sd num' option to report subdataset with the specified number.
 * gdalinfo: add '-proj4' option to gdalinfo, to report a PROJ.4 string for the CRS
 * gdal_translate: propagate INTERLEAVE metadata to intermediate VRT dataset
 * gdal_translate: force quiet mode when writing to /vsistdout/
 * gdalwarp: Disable CENTER_LONG rewrapping for cutline (#3932)
 * gdalwarp: add -refine_gcps option to discard outliers GCPs before warping (#4143)
 * gdalwarp: add warning if user specifies several of -order, -tps, -rpc or -geoloc options
 * gdalwarp: speed-up when using -tps with large number of GCPs
 * gdalwarp: add support for optional use of libarmadillo to speed-up matrix inversion in -tps mode
 * gdalwarp: detect situations where the user will override the source file
 * gdallocationinfo: do not let one off-db pixel cause all the rest to be suppressed (#4181)
 * gdal_rasterize: fix half pixel shift when rasterizing points; make gdal_rasterize utility increase the computed raster extent by a half-pixel for point layers (#3774)
 * gdal_rasterize: when source datasource has a single layer, use it implicitly if none of -l or -sql is specified
 * nearblack: add -color option (#4085)
 * nearblack: improve detection of collar
 * nearblack: remove useless restrictions on number of bands for -setmask and -setalpha options (#4124)
 * gcps2vec.py: Fix command line parsing; Add SRS definition to created vector layer; Use Point geometry when dumping pixel/line coordinates.
 * gdal_merge.py: add support for -separate with multiband inputs (#4059)
 * gdal_merge.py: add a -a_nodata option (#3981)
 * gdal_proximity.py: -co option existed, but was unused...
 * gdal_fillnodata.py: add -co option
 * Add gdal_ls.py and gdal_cp.py as Python samples
 * Add new sample utility, gdal_edit.py, to edit in place various information of an existing GDAL dataset (projection, geotransform, nodata, metadata) (#4220)
 * gdalcopyproj.py: make it copy GCPs too
 * Add warning if a target filename extension isn't consistent with the output driver
 * Add --pause for convenient debugging, document it and --locale

Multi-driver topics:
 * Implement reading XMP metadata from GIF, JPEG, PNG, GTiff, PDF and the 5 JPEG2000 drivers. The XMP metadata is stored as raw XML content in the xml:XMP metadata domain (#4153)
 * Mark BT, DIPEx, ERS, FAST, GenBIN, GSC, GSBG, GSAG, GS7BG, JDEM, JP2ECW, PNM, RMF, TIL, WCS and WMS drivers as compatible with VSI virtual files
 * Port DOQ1, DOQ2, ELAS, Idrisi, L1B, NDF, NWT_GRD, NWT_GRC, USGSDEM to VSI virtual file API
 * PAM-enable BT and BLX drivers
 * Implement Identify() for AAIGrid, ACE2, DTED, NWT_GRD, NWT_GRC, WMS, WCS, JDEM and BSB drivers
 * Make GIF, JPEG and PNG drivers return a non NULL dataset when writing to /vsistdout/
 * HFA and GTiff: add explicit error message when trying to add external overviews when there are already internal overviews (#4044)
 * Initialize overview manager to support external overviews for AAIGRID, DIPX, ELAS, GXF, FIT, FITS, GMT, GRIB, GSAG, GSBG, GS7BG, ILWIS, L1B, LCP, Leveller, NWT_GRD, NWT_GRC, RIK, SDTS and SAGA

AAIGrid:
 * Make opening from /vsicurl/ work even when the server returns an empty file list

ACE2 driver:
 * New for GDAL/OGR 1.9.0
 * Read ACE2 DEM

AIG driver:
 * Support uncompressed integer files, new in ArcGIS 10 it seems (#4035)
 * Use color table from PAM if no native one (#4021)
 * Fallback to PAM mechanism for RAT (#4021)

BSB driver:
 * Parse the GD keyword in BSB_KNP to recognize European 1950 datum (#4247)
 * fix compilation issues with -DBSB_CREATE

CEOS2 driver:
 * avoid potential crash reading past end of string. (#4065)

CTG driver:
 * New for GDAL/OGR 1.9.0
 * Read USGS LULC Composite Theme Grid files

DIMAP driver:
 * Add support for DIMAP2
 * Check underlying raster for SRS. There are cases where HORIZONTAL_CS_CODE is empty and the underlying raster is georeferenced

E00GRID driver:
 * New for GDAL/OGR 1.9.0
 * Read Arc/Info Export E00 GRID

ECRGTOC driver:
 * New for GDAL/OGR 1.9.0
 * Read TOC.xml file of ECRG products

ECW driver:
 * Use a long refresh time for ecwp:// connections to ensure we get full resolution data, make configurable
 * Re-enable writing non8bit data in jpeg2000
 * Add implementation of an Async reader (4.x SDK)
 * Improve to support all /vsi stuff (#2344)
 * Ensure ECW_ENCODE_ values are applied for direct Create as well as CreateCopy
 * force adfGeoTransform[5] sign to negative. (#393)
 * Mark GDAL_DCAP_VIRTUALIO=YES when the driver is configured in read-only mode
 * Ensure we fallback to native geotransform if no pam override
 * Try to read projection info embedded in ECW file before reading the worldfile (#4046)
 * Add support for updating geotransform and projection info of a ECW file (#4220)
 * Fix ECW_CACHE_MAXMEM that was without effect and ECW_AUTOGEN_J2I that set an unrelated ECW parameter (#4308)
 * Allow to open a ECW file with invalid EPSG code from SWIG bindings (#4187)

EHdr driver:
 * Improve floating point detection (#3933)
 * Recognize MIN_VALUE and MAX_VALUE as found in ETOPO1 header
 * Try opening the .sch file for GTOPO30 or SRTM30 source file
 * Ignore bogus .stx file where min == nodata

EIR driver:
 * Add support for DATA_TYPE keyword

ENVI driver:
 * Add support for ESRI style coordinate system string (#3312)
 * Try to guess interleave mode from file extension, if interleave keyword is missing
 * Refuse to open unsupported types, but attempt to open everything else.

ENVISAT driver:
 * Correct dfGCPLine values for stripline products (#3160, #3709)
 * Fix checking of tie points per column for MERIS GCPs (#4086)
 * Report metadata from the ASAR ADS and GADS in the RECORDS metadata domain (#4105)
 * Read MERIS metadata (#4105)
 * Read data from ERS products in ENVISAT format (#4105)
 * Improved MERIS Level 2 bands detection (#4141 and #4142)

EPSILON driver:
 * Now require libepsilon 0.9.1 to build (now dual LGPL/GPL) (#4084)

ERS driver:
 * Use case insensitive find so case does not matter (#3974)
 * Handle case of 1 m pixel resolution when CellInfo is missing (#4067)
 * Implement ERSRasterBand::SetNoDataValue() (#4207)
 * Add support for DATUM, PROJ and UNITS creation option; report the values read from the .ers file in the ERS metadata domain (#4229)

GeoRaster driver:
 * Set nodata causes invalid XML metadata (#3893)
 * Fix SetStatistics() failure (#4072)
 * Fix default interleaving (#4071)
 * modelCoordinateLocation=CENTER default (#3266)
 * Cache block/level error in update (#4089)
 * Fix sequence.nextval not supported (Oracle 10g) (#4132)
 * change BLOCKING option to OPTIMALPADDING
 * fix 'cannot specify columns on insert create option' (#4206)
 * Fix ULTCoordinate Rows/Columns swapping (#3718)
 * Fix loading of small images, FlushCache issue (#4363)

GIF driver:
 * Make CreateCopy() more friendly with writing in /vsistdout/

GRIB driver:
 * Fix grid vs cell-center convention (#2637)
 * use /vsi for all jpeg2000 files now
 * Fix to allow GFS data to show up properly (#2550)
 * Added a ConfigOption in GRIB driver to not normalize units to metric when reading the data
 * Fixed grib1 & grib2 : pixel size precision introduces error for corner coordinates (#4287)

GTA driver:
 * New for GDAL/OGR 1.9.0
 * Read/write support for Generic Tagged Arrays

GTiff driver:
 * Ensure false easting/northing in geotiff geokeys are treated as being in geosys units.  Add GTIFF_LINEAR_UNITS=BROKEN config option to try and read old broken files, and logic to cover for older libgeotiffs when reading (#3901)
 * Add support for a special tag to keep track of properly written linear units (#3901)
 * Implement deferred directory chain scanning to accelerate simple opens
 * Make GTiff COPY_SRC_OVERVIEWS to deal with unusual source overview sizes (#3905)
 * Fix bug when using -co COPY_SRC_OVERVIEWS=YES on a multiband source with external overviews (#3938)
 * Add logic to fill out partial tiles on write in for jpeg images (#4096)
 * Updated to libtiff 4.0.0 final
 * Refresh with libgeotiff 1.4.0, to support for GeogTOWGS84GeoKey
 * Add support for Geocentric SRS
 * libtiff: Enable DEFER_STRILE_LOAD
 * Turn warning 'ASCII value for tag xx into more silent CPLDebug message
 * Overviews: Improve error reporting for >16bit images to JPEG compression
 * Use CPLAtof() for geotiff and epsg .csv file handling (#3886, #3979)
 * Lots of Imagine and ESRI PE string citation handling changes from 1.8-esri. Some citation related changes only compiled in if ESRI_SPECIFIC defined.
 * Give PAM information precedence over metadata from GeoTIFF itself.   Avoid unnecessary (default) writes of scale/offset.  Treat (0,1,0,0,0,-1) as a default geotransform as well as (0,1,0,0,0,1).
 * Migrate in some ESRI only logic for 1bit color tables, AdjustLinearUnits and default for 1bit data
 * Add a GTIFF_IGNORE_READ_ERRORS configuration option (#3994)
 * Lazy loading of RPC/RPB/IMD files (#3996)
 * Add mutex protection in GTiffOneTimeInit() to avoid occasional segfaults
 * Stop interpreting 4th band as alpha when not defined
 * Also list nSubType == FILETYPE_PAGE as subdatasets
 * CreateCopy(): copies category names from the source bands
 * Add capability of writing TIFFTAG_MINSAMPLEVALUE and TIFFTAG_MAXSAMPLEVALUE
 * Don't prevent from loading GTiff driver even if libtiff version mismatch detected (#4101)
 * Use GTIFF_ESRI_CITATION flag to disable writing special meaning ESRI citations
 * Optimize GTiffRGBABand implementation (#3476)
 * Add GTIFF_DIRECT_IO config. option that can be set to YES so that IRasterIO() reads directly bytes from the file using ReadMultiRange().
 * Use VSI_TIFFOpen() in GTIFFBuildOverviews() to make it work on virtual file systems
 * Treat _UNASSALPHA as alpha

GRASSASCIIGrid driver:
 * New for GDAL/OGR 1.9.0
 * Read GRASS ASCII grids (similar to ArcInfo ASCII grids)

GRIB driver:
 * Check for memory allocation failures

HDF4 driver:
 * Use larger object name buffer
 * Handle SWopen failures

HDF5 driver:
 * Fix HDF5/BAG handle/memory leaks (#3953)
 * Better error checking
 * Do not return NULL from getprojectionref() (#4076)
 * Identify datasets whose header starts with some XML content (#4196)
 * Fixed HDF5 variable length string attributes reading (#4228)

HFA driver:
 * Add support for writing RATs (#999)
 * Add support for reading 2bit compressed .img files (#3956)
 * Update EPRJ_ list based on input from Erdas, round trip Krovak and Mercator Variant A (#3958)
 * Major push to move projections from 1.6-esri into trunk in HFA driver (#3958)
 * Reinitialize RRDNamesList and ExternalRasterDMS (#3897)
 * Ensure the whole entry is reinitialized when writing proparams and datum (#3969)
 * Ensure PEString cleared if we aren't writing it (#3969)
 * Get nodata from an overview if not present on main band.  Set geotransform[1] and [5] to 1.0 if pixelSize.width/height are 0.0.  Improve error checking if MakeData() fails.
 * atof() changed to CPLAtofM() to avoid locale issues.
 * Altered Edsc_BinFunction column "Value" to "BinValues" and changed type.
 * Equirectangular StdParallel1 changed to LatitudeOfOrigin.
 * Logic to preserve psMapInfo->proName as the PROJCS name for UTM/StatePlane.
 * Special state plane zone handling.
 * Special wisconsin handling for some LCC and TM SRSes.
 * 1-bit null blocks to default to 1 in ESRI_BUILDs
 * Add support for GDA94 (#4025)

Idrisi driver:
 * Fix segfaults when fields are missing in .ref or .rdc files (#4100)
 * Fix problem with inverse flattening when reading a SRS of a sphere (#3757)

INGR driver:
 * Set NBITS for 1 bit bands

ISIS2/ISIS3 driver:
 * Various improvements to PDS related drivers (#3944)

JaxaPalsar driver:
 * Fixed datatype of ALOS PALSAR products Level 1.5 (#4136)
 * Fixed detection of unsupported PALSAR Level 1.0 products (#2234)

JPIPKAK driver:
 * Add try to in GetNextUpdatedRegion() to protect against kakadu exceptions (#3967)
 * Fixed a serious bug in the computation of fsiz and region. (#3967)

KMLSUPEROVERLAY driver :
 * Remove spaces between coordinates in coordinate triplets as mandated by KML 2.2 spec, to restore compatibility with Google Earth 6.1.0.5001 (#4347)

LAN driver:
 * Preliminary support for writing gis/lan files

MEM driver:
 * Add support for remembered histograms and PIXELTYPE

MG4Lidar driver:
 * Clamp nOverviewCount, some LiDAR files end up with -1 overviews

MrSID driver:
 * Initialize overview manager to enable RFC 15 mask band support (#3968)
 * Mark GDAL_DCAP_VIRTUALIO=YES when the driver is configured in read-only mode
 * Handle LTI_COLORSPACE_RGBA

NetCDF driver:
 * Set cylindrical equal area representation to the proper cf-1.x notation (#3425)
 * Fix precision issue in geotransform (#4200) and metadata
 * Add support for netcdf filetypes nc2(64-bit) and nc4 to netCDFDataset (#3890, #2379)
 * Add function Identify and IdentifyFileType() (#3890, #2379)
 * Temporarily disabling PAM for netcdf driver (#4244)
 * Make creation of geographic grid CF compliant (#2129)
 * Fixes for netcdf metadata export: duplication, Band metadata, int/float/double vs. char* and add_offset/scale_factor (#4211, #4204), double precision ( 4200)
 * Fix netcdf metadata import (float and double precision) (#4211)
 * Improve import of CF projection
 * Add netcdf history metadata (#4297)
 * CF-1.5 compatible export of projected grids (optional lon/lat export)
 * Fix LCC-1SP import and export (#3324)
 * Fix handling of UNITS import and export (#4402 and #3324)
 * Fix upside-down export and import of grids without projection and geotransform (#2129, #4284)
 * Support import of polar stereographic variant without standard parallel (#2893)
 * New driver options
 * Add simple progress indicator
 * Add support for netcdf-4, HDF4 and HDF5 (#4294 and #3166)
 * Add support for deflate compression
 * Add format support information and CreateOptionList to driver metadata
 * Add support for valid_range/valid_min/valid_max
 * Proper handling of signed/unsigned byte data
 * Add support for Create() function and significantly refactor code for export (#4221)
 * Improvements to CF projection support (see wiki:NetCDF_ProjectionTestingStatus)

NGSGEOID driver:
 * New for GDAL/OGR 1.9.0
 * Read NOAA NGS Geoid Height Grids

NITF driver:
 * Add a generic way of decoding TREs from a XML description file located in data/nitf_spec.xml
 * Add a new metadata domain xml:TRE to report as XML content the decoded TREs
 * Add NITF_OPEN_UNDERLYING_DS configuration option that can be set to FALSE to avoid opening the underlying image with the J2K/JPEG drivers
 * Support JP2KAK driver for jpeg2000 output, use /vsisubfile/ in all cases
 * NITFCreate(): deal with cases where image_height = block_height > 8192 or image_width = block_width > 8192 (#3922)
 * Add IREPBAND and ISUBCAT creation option (#4343)
 * Make sure scanline access is used only on single block image (#3926)
 * Add a NITF_DISABLE_RPF_LOCATION_TABLE_SANITY_TESTS configuration option that can be set to TRUE to blindly trust the RPF location table (#3930)
 * Correctly assign hemisphere for a ICORDS='U' NITF file with accompanying .nfw and .hdr files (#3931)
 * Make PAM available at band level for JPEG/JPEG2000 compressed datasets (#3985)
 * Read IMRFCA TRE for RPC info. Read CSEXRA TRE.
 * Read CSDIDA and PIAIMC TREs as metadata
 * Optional support for densifying GCPs and applying RPCs to them
 * Add GetFileList() that captures associated files with some NITF products.
 * Added ESRI only ExtractEsriMD() function.  Add raw header capture in NITF_METADATA domain in base64 encoded form.
 * Fetch NITF_DESDATA in segment data; decode specialized fields of XML_DATA_CONTENT and CSATTA DES
 * Truncate TRE name to 6 character (#4324)
 * Take into account the presence of comments when patching COMRAT for JPEG/JPEG2000 NITF (#4371)

NWT_GRD driver:
  * Fix interpolation of color when the maximum z value is below a threshold of the color scheme (#4395)

OPENJPEG driver:
 * Optimize decoding of big images made of a single block
 * Fallback to PAM to get projection and geotransform

PCIDSK driver:
 * Refresh PCIDSK SDK from upstream
 * Fix support for band description setting, add BANDDESCn creation option
 * Implement GetCategoryNames(), and color table from metadata for PCIDSK
 * Fix exception on files with bitmaps as bands in GetFileList()
 * Avoid closing and reopening file so we don't fracture the SysBMData with a metadata write
 * In read-only, if .pix is raster (resp. vector) only, then make sure that OGR (resp. GDAL) cannot open it

PDF driver:
 * Support linking against podofo library (LGPL) instead of poppler --> however
   pdftoppm binary from poppler distribution is needed for rasterization

PDS driver:
 * Add support for MISSING and MISSING_CONSTANT keywords for nodata values (#3939)
 * Add support for uncompressed images in the UNCOMPRESSED_FILE subdomain (#3943)
 * Add support for PDS_Sample/LineProjectOffset_Shift/Mult (#3940)
 * Preliminary qube write support (#3944)
 * Fix band offset computation in BSQ (#4368)

PNG driver:
 * Add compatibility with libpng >= 1.5.0 (#3914)
 * Upgrade internal libpng to 1.2.46

PNM driver:
 * Make it compatible with VSI virtual files

PostgisRaster driver:
 * Speed of PostGIS Raster driver improved. (#3228, #3233)

Rasterlite driver:
 * Robustness against buggy databases
 * Enable QUALITY creation option for WEBP tiles

RS2 driver:
 * Setup to properly support subdataset oriented metadata and overviews (#4006)
 * Allow opening subdatasets by passing in the folder (#4387)

SAGA driver:
 * Fix reading & writing .sdat files bigger than 2GB (#4104)
 * Use nodata value from source dataset in CreateCopy() (#4152)

SDE driver:
 * Break assumption that LONG==long (#4051)

SNODAS driver:
 * New for GDAL/OGR 1.9.0
 * Read Snow Data Assimilation System datasets

SRP driver:
 * Set Azimuthal Equidistant projection/geotransform info for ASRP north/south polar zones (#3946)
 * ASRP/USRP: fix skipping of padding characters on some datasets (#4254)

SRTMHGT driver:
 * Fix segfault in CreateCopy() if we cannot create the output file

Terralib driver:
 * Removed driver: was unfinished and is unmaintained (#3288)

TIL driver:
 * Implement GetFileList() (#4008)

TSX driver:
 * Add support for selecting a directory
 * Make Terrasar-X driver also open TanDEM-X data (#4390)
 * Fix memleaks

USGSDEM driver:
 * Ensure blocks read in C locale (#3886)

VRT driver:
 * Implement VRTDataset::IRasterIO() that can delegate to source Dataset::RasterIO() in particular cases
 * Implement GetMinimum() and GetMaximum()
 * GetFileList(): for /vsicurl/ resources, don't actually test their existence as it can be excruciating slow
 * VRTComplexSource: correctly deal with complex data type (#3977)
 * Fix 2 segfaults related to using '<VRTDataset', but with invalid XML, as the target filename of VRTDataset::Create()
 * Fix 'VRTDerivedRasterBand with ComplexSource and nodata value yields potentially uninitialized buffer' (#4045)
 * VRTDerivedRasterBand: Recognize PixelFunctionType and SourceTransferType options in AddBand() for  (#3925)
 * Copy GEOLOCATION metadata in CreateCopy().
 * VRTDerivedRasterBand: register pixel functions in a map for faster access (#3924)
 * VRT warped dataset: limit block size to dataset dimensions (#4137)

WCS driver:
 * Add time support (#3449)
 * Honour dimensionLimit restrictions on WCS request size.
 * Fetch projection from returned image file chunks if they have them (i.e. GeoTIFF).
 * Honour Resample option for WCS 1.0.
 * Include service url in GetFileList if ESRI_BUILD defined
 * Check validity of 'OverviewCount' parameter
 * Add support for getting the coverage offering details from the xml:CoverageOffering domain
 * Try to preserve the servers name for a CRS (WCS 1.0.0) (#3449).

WebP driver:
 * New for GDAL/OGR 1.9.0
 * Read/write GDAL driver for WebP image format

WMS driver:
 * Implementation of the OnEarth Tiled WMS minidriver (#3493)
 * Implementation of a VirtualEarth minidriver
 * Improve handling of .aux.xml files, ensure colorinterp support works for tiled wms server
 * Report subdatasets when being provided WMS:http://server.url (classic WMS), WMS:http://server.url?request=GetTileService or a url to a TMS server; recognize datasets specified as a pseudo GetMap request
 * Add capability to open the URL of a REST definition for a ArcGIS MapServer, like http://server.arcgisonline.com/ArcGIS/rest/services/World_Imagery/MapServer?f=json
 * Parse WMS-C TileSet info in VendorSpecificCapabilities of GetCapabilities
 * Implement CreateCopy() when source is a WMS dataset to serialize to disk the definition of a WMS dataset
 * WMS dataset : report INTERLEAVE=PIXEL
 * Make autodetection of TMS work with http://maps.qualitystreetmap.org/tilecache
 * Add capability to set Referer http header
 * Add a TMS-specific hack for some servers that require tile numbers to have exactly 3 characters (#3997)
 * Fix to make file:// URL to work
 * Add hack for OSGEO:41001
 * Fix GDALWMSRasterBand::IReadBlock() to avoid the ReadBlocks() optimization to become an anti-optimization in some use cases
 * Add service=WMS parameter if not already provided in <ServerUrl> (#4080)
 * Add options, ZeroBlockHttpCodes and ZeroBlockOnServerException, to control which http error codes should be considered as meaning blank tile (#4169)

ZMap driver:
 * New for GDAL/OGR 1.9.0
 * Read/write GDAL driver for ZMap Plus Grid format

## OGR 1.9.0 - Overview of Changes

Core:
 * RFC35: Add OGRLayer::DeleteField(), ReorderField(), ReorderFields() and AlterFieldDefn()
 * Avoid OGRLineString::addPoint( OGRPoint * poPoint ) to always force the geometry to be 3D (#3907)
 * Add a OGREnvelope3D object and getEnvelope( OGREnvelope3D * psEnvelope ) / OGR_G_GetEnvelope3D() method
 * Add OGR_G_SimplifyPreserveTopology() / OGRGeometry::SimplifyPreserveTopology()
 * OGR SQL: recognize optional ESCAPE escape_char clause
 * OGR SQL: allow NULL to be used as a value, so that 'SELECT *, NULL FROM foo works'
 * OGR SQL: Accept doublequoting of column_name in 'SELECT DISTINCT "column_name" FROM table_name' (#3966)
 * OGR SQL: OGRGenSQLResultsLayer: if the dialect is explicitly set to OGRSQL, don't propagate the WHERE clause of the SELECT to the source layer, but evaluate it instead at the OGRGenSQLResultsLayer level (#4022)
 * OGR SQL: Avoid error emission on requests such as 'SELECT MIN(EAS_ID), COUNT(*) FROM POLY'
 * OGR SQL: Avoid setting width/precision for AVG column
 * OGR SQL: Add a mechanism to delete a layer (DROP TABLE x)
 * OGR SQL: fix segfault when evaluating a 'IS NULL' on a float column (#4091)
 * OGR SQL: add support for new special commands : 'ALTER TABLE layername ADD COLUMN columnname columntype', 'ALTER TABLE layername RENAME COLUMN oldname TO new name', 'ALTER TABLE layername ALTER COLUMN columnname TYPE columntype', 'ALTER TABLE layername DROP COLUMN columnname'
 * OGR SQL: Add implicit conversion from string to numeric (#4259)
 * OGR SQL: Correctly parse big SQL statements (#4262)
 * OGR SQL: fix joining a float column with a string column (#4321)
 * OGR SQL: with DISTINCT, consider null values are such, and not as empty string (#4353)
 * OGR SQL: fix offset conversion for SUBSTR() (#4348)
 * Add OGR_G_GetPoints()
 * Fix parsing of WKT geometries mixing 2D and 3D parts
 * OGR_Dr_CopyDataSource() and OGRSFDriver::CopyDataSource() : make sure that the driver is attached to the created datasource (#4350)
 * OGRFeature::SetFrom() supports more conversions between field types.

OGRSpatialReference:
 * Update to EPSG 7.9 database
 * Add Geocentric SRS Support
 * Add support for Interrupted Goode Homolosine projection (#4060)
 * Add SRS vertical unit support
 * Add SetVertCS(), OSRSetVertCS(), SetCompound(), IsCompound() and target oriented set/get linear units functions
 * ESRI : Improve spheroid remapping (#3904)
 * ESRI: Compare whole names in RemapNameBasedOnKeyName() (#3965).
 * ESRI: addition of ImportFromESRIStatePlaneWKT and ImportfromESRIWisconsinWKT methods
 * ESRI: importFromESRI() : support POLYCONIC projection from old style files (#3983)
 * ESRI: importFromESRI() : support LAMBERT_AZIMUTHAL projection from old style files (#4302)
 * ESRI: fix EPSG:32161 mapping
 * ESRI: fix Stereo/Oblique_Stereo/Double_Stereo (bugs #1428 and #4267)
 * ESRI: fix projection parameter mapping for Orthographic projection (#4249)
 * ESRI: add optional fixing of TOWGS84, DATUM and GEOGCS with GDAL_FIX_ESRI_WKT config. option (#4345 and #4378)
 * ESRI: fix add Krassowsky/Krasovsky 1940 spheroid mapping
 * Add EPSG:102113 in the data/esri_extra.wkt file
 * Add Germany zone 1-5 in range 31491-31495 in the data/esri_extra.wkt file
 * fix NAD_1983_Oregon_Statewide_Lambert_Feet_Intl code
 * added/updated coordinates systems provided by IGNF (#3868)
 * ERM: add support for EPSG:n based coordinate systems (#3955)
 * ImportFromEPSG(): Add default support for various degree units without supporting .csv file.
 * ImportFromEPSG(): Add support for spherical LAEA (#3828)
 * ImportFromEPSG(): use CoLatConeAxis parameter to build Krovak azimuth parameter (#4223)
 * importFromURN(): support compound SRS
 * importFromURN(): accept 'urn:opengis:crs:' syntax found in some TOP10NL GML files
 * Add CRS: support and for importing two part ESRI PE SRS with VERTCS
 * SetFromUserInput() : recognize 'IGNF:xxx'
 * Ensure that the result of importFromEPSGA() always has keyword ordering fixed up (#4178)
 * exportToERM() : deal with GDA94 datum and its UTM projections (#4208)
 * Fix ESRI_DATUM_NAME for D_MOLDREF99 and D_Philippine_Reference_System_1992 (#4378)

Utilities:
 * ogr2ogr: Make 'ogr2ogr someDirThatDoesNotExist.shp dataSourceWithMultipleLayer' create a directory
 * ogr2ogr: make -overwrite/-append work with non-spatial tables created by GDAL 1.8.0;
 * ogr2ogr: take into account fields specified in the -where clause in combinations with -select to create the correct list of fields to pass to SetIgnoredFields() (#4015)
 * ogr2ogr: fix -zfield option so that the modified geometry properly reports coordinate dimension = 3. Also avoids it to be in the list of ignored field names
 * ogr2ogr: add -simplify option to simplify geometries
 * ogr2ogr: add a warning if the target filename has an extension or a prefix that isn't consistent with the default output format (i.e. Shapefile), but which matches another driver. Can be made quiet with -q option
 * ogrinfo/ogr2ogr: exit when SetAttributeFilter() fails, instead of silently going on (#4261)

Multi driver topics:
 * RFC35 : implementation in Shapefile, Memory and PG drivers (#2671)
 * DXF, EDIGEO, KML, LIBKML, Shapefile, SDE, SOSI: Mark as supporting UTF-8
 * BNA, CSV, GPX, KML, GeoRSS, GML, LIBKML, GeoJSON, PGDump : accept both /dev/stdout and /vsistdout/ as filenames; remove 'stdout' as a valid alias that could be used in some of them (#4225, #4226)

ARCGEN driver:
 * New for GDAL/OGR 1.9.0
 * Read-only OGR driver for Arc/Info Generate files

CouchDB driver:
 * New for GDAL/OGR 1.9.0
 * Read/write OGR driver for CouchDB / GeoCouch

CSV driver:
 * Add special recognition and handling for USGS GNIS (Geographic Names Information System) files
 * Directly recognize the structure of the allCountries file from GeoNames.org
 * Implement GetFeatureCount() to be a little bit faster
 * Accept /dev/stdout as a filename for CreateDataSource()
 * Fix handling of non-numeric values in numeric columns (NULL instead of 0)
 * Fix handling of column names with numbers
 * Recognize numeric fieldnames inside quotes
 * Accept real numbers with ',' as decimal separator when ';' is the field separator (CSV export in French locale)

DXF driver:
 * Add support for DXF_ENCODING config var and DWGCODEPAGE header field (#4008)
 * Added DXF_MERGE_BLOCK_GEOMETRIES
 * Treat ATTDEFs the same as TEXT entities
 * Implement hatch polyline and elliptical arc support, hatch fill, do not polygonize closed smoothed lines
 * Add handling of hidden/frozen/off layers.

DWG driver:
 * New for GDAL/OGR 1.9.0
 * Read DWG files through the use of Open Design Alliance Teigha Libraries

EDIGEO driver:
 * New for GDAL/OGR 1.9.0
 * Read files of French EDIGEO exchange format

FileGDB driver:
 * New for GDAL/OGR 1.9.0
 * Read/write support based on FileGDB API SDK

GeoJSON driver:
 * Support writing 3D lines and polygons
 * Add a bbox attribute with the geometry bounding box if WRITE_BBOX layer creation option is set (#2392)
 * Write bbox of FeatureCollection before features when file is seekable
 * Remove unsetting of FID that caused FID not at the last position of properties to be lost
 * Properly deal with null field values in reading and writing
 * Handle OFTIntegerList, OFTRealList and OFTStringList fields
 * Recognize other arrays as OFTString field
 * Fix assertion on unhandled ESRI json (#4056)
 * Fix segfault on feature where 'properties' member exists but isn't an object (#4057)
 * Better detection of OGR type for numeric JSON fields (#4082)
 * Add COORDINATE_PRECISION layer creation option to specify the maximum number of figures after decimal point in coordinates; set to 15 by default with smart truncation of trailing zeros (like done for WKT)
 * Add OGR_G_ExportToJsonEx() to accept a list of options
 * Add ability to detect geojson files without an extension (#4314)

Geomedia driver:
 * New for GDAL/OGR 1.9.0
 * Read-only driver to read Geomedia .MDB databases

GeoRSS driver:
 * Parse RSS documents without <channel> element

GFT driver:
 * New for GDAL/OGR 1.9.0
 * Read/write driver for Google Fusion Tables

GML driver:
 * Major performance improvement when reading large multi-layer GML files. See usage of new GML_READ_MODE configuration option
 * Support gml:xlink resolving for huge GML files through GML_SKIP_RESOLVE_ELEMS=HUGE (requires SQLite)
 * Add GML_GFS_TEMPLATE config option to specify a template .gfs file that can be used for several GML files with similar structure (#4380)
 * Be able to build the driver with support of both Expat and Xerces libraries and add ability of select one at runtime. For UTF-8 documents, we select Expat if it is available, because it is faster than Xerces
 * Expose fid or gml_id as feature fields if autodetected. This behavior can be altered by the GML_EXPOSE_FID / GML_EXPOSE_GML_ID configuration option.
 * Improve handling of .gml and .xsd produced by FME (in particular for CanVec GML)
 * Be able to open .gz file directly (like OS Mastermap ones), and read/write the .gfs file next to the .gz file
 * Fix segfault when encountering an invalid (or unhandled by OGR) geometry and when the axis order is lat/long (#3935)
 * GML3: use a new method to interpret Face objects (which requires GEOS support);
         old method available if GML_FACE_HOLE_NEGATIVE config. option set to YES (#3937)
 * GML3: support Curve as a valid child for curveProperty inside directEdge parsing (#3934)
 * GML3: don't force the linestring to be 3D when inverting its orientation during parsing of directedEdge (#3936)
 * GML3: accept <pointProperty> element in <gml:LineString> or <gml:LineStringSegment>
 * OGR_G_CreateFromGML(): accept <gml:coordinates> with coordinate tuples separated by comma and coordinate components separated by space
 * Recognized schemas with <complexType> inside <element>, such as the one returned by http://deegree3-demo.deegree.org:80/deegree-utah-demo/services
 * Write the Z component of bounding box for 25D geometries
 * Force layer geometry type to 3D when there's only a .xsd file and we detect a hint that the bounding box is 3D
 * Handle layers of type wkbNone appropriately (#4154)
 * Change format of (GML 2.1.1) FID generated from Fxxx to layer_name.xxx (where xxx is the OGR FID) to ensure uniqueness (#4250)
 * Accept 'GML3Deegree' as a valid value for the dataset creation option FORMAT, to produce a .XSD that should be better handled by Deegree3 (#4252), and 'GML3.2' to produce GML file and schema that validate against GML 3.2.1 schema.
 * Don't try to parse successfully a feature type in the .xsd if there are elements we don't know how to parse. Better to rely on the .gfs mechanism (#4328)
 * Fix bug in OGRAtof() that caused wrong parsing of coordinates in GML files written in scientific notation (#4399)

GMT driver:
 * Fix GetExtent() result that swallowed the first char of the minx bound (#4260)

IDRISI driver:
 * New for GDAL/OGR 1.9.0
 * Read Idrisi .VCT vector files

ILI1 driver:
 * Fix for missing geometry in ILI1

LIBKML driver:
 * Set the OGRStylePen unit type to pixel when reading <LineStyle>
 * Avoid ingesting zip files that are not valid kmz (#4003)
 * Do not use displayname to set the field name
 * Recognize <Data> elements of <ExtendedData> in case <ExtendedData> doesn't use a <SchemaData>
 * Fix mapping of the type attribute of <SimpleType> elements inside <Schema> to OGR field type (#4171)
 * Parse correctly kml docs containing only one placemark
 * Properly set the feature style string from a placemarks style
 * Improve OGRStyleLabel <-> KmlLabelStyle mapping
 * Combine styles from the style table and features styles when LIBKML_RESOLVE_STYLE=YES (#4231)
 * Check that string values put in fields are valid UTF-8 (#4300)

MDB driver:
 * New for GDAL/OGR 1.9.0
 * Read-only driver to read PGeo and Geomedia .MDB databases
 * Relies on using the Java Jackcess library (LGPL) through JNI.

MITAB driver:
 * Add support for reading google mercator from mapinfo (#4115)
 * Fixed problem of the null datetime values (#4150)
 * Fix problem with tab delimiter used in MIF files (#4257)

MSSQLSpatial driver:
 * Removing 'Initial Catalog' which is not supported in the ODBC SQL driver connection strings.
 * Allow to specify 'Driver' in MSSQL connection strings (#4393)
 * Fix for the IDENTITY INSERT problem with MSSQL Spatial (#3992)
 * Add more verbose warnings to the geometry validator
 * Fix for the schema handling problem with MSSQL Spatial (#3951)
 * Fix for the corrupt geometry report when using the ogr2ogr -sql option (#4149)

MySQL driver:
 * Recognize columns with types POINT, LINESTRING, etc. as geometry columns

NAS driver:
 * Add support for treating wfs:Delete as a special Delete feature with typeName and FeatureId properties
 * Handle empty files gracefully (#3809)
 * Preliminary support for SRS in NAS files, including 3GKn SRS
 * Implement special treatment for <lage> to be zero passed and string (NAS #9)
 * Add special handling of punktkennung (NAS #12)
 * Add special handling for artDerFlurstuecksgrenze (#4255)
 * Add support for wfsext:Replace operations (PostNAS #11)
 * Correct NASHandler::dataHandler() to avoid trimming non-leading white space

NTF driver:
 * Create and manage height field as floating point since some DTM products have floating point elevations.

OCI driver:
 * Added TRUNCATE layer creation option (#4000)
 * Clear errors after speculative dimension calls (#4001)
 * Fix multithreading related problems (#4039)
 * Ensure that AllocAndBindForWrite does not mess up if there are no general attributes (#4063)
 * Implement DeleteLayer(int) method
 * Ensure extents updated by SyncToDisk(), and that new features are merged into existing extents (#4079)

OGDI driver:
 * Fix GetFeature() that did not like switching between layers

PG driver:
 * Write geometries as EWKB by default to avoid precision loss (#4138)
 * Return the table columns in the order they are in the database (#4194)
 * Add a NONE_AS_UNKNOWN layer creation option that can be set to TRUE to force layers with geom type = wkbNone to be created as if it was wkbUnknown (PostGIS GEOMETRY type) to be able to revert to behavior prior to GDAL 1.8.0 (#4012)
 * Add EXTRACT_SCHEMA_FROM_LAYER_NAME layer creation option that can be set to OFF to disable analysis of layer name as schema_name.table_name
 * Add FID layer creation option to specify the name of the FID column
 * ogr2ogr: make sure that for a PG datasource, -nln public.XXX can also be used with -append
 * Fix CreateFeatureViaInsert() to emit 'INSERT INTO xx DEFAULT VALUES'
 * Fix handling of Nan with fields with non-zero width (#2112)
 * Use wrapper for PQexec() to use PQexecParams() instead in most cases
 * Add proper escaping of table and column names
 * OGR SQL: add proper column name escaping and quoting for PostgreSQL datasources
 * Launder single quote character in table name
 * Better reporting of error in case of failed ExecuteSQL()
 * Create field of type OFTString and width > 0, as VARCHAR(width) (#4202)
 * Add more compat with Postgis 2.0SVN (geometry_columns- #4217, unknown SRID handling)
 * Better behavior, in particular in error reporting, of ExecuteSQL() when passed with non-select statements, or with select statements that have side-effects such as AddGeometryColumn()

PGDump driver:
 * fix handling of Nan with fields with non-zero width (#2112)
 * Add CREATE_SCHEMA and DROP_TABLE layer creation option (#4033)
 * Fix crash when inserting a feature with a geometry in a layer with a geom type of wkbNone;
 * PG and PGDump: fix insertion of features with first field being a 0-character string in a non-spatial table and without FID in COPY mode (#4040)
 * Add NONE_AS_UNKNOWN, FID, EXTRACT_SCHEMA_FROM_LAYER_NAME layer creation options
 * Better escaping of column and table names
 * Create field of type OFTString and width > 0, as VARCHAR(width) (#4202)

PGeo driver:
 * Move CreateFromShapeBin() method to an upper level
 * Only try to open .mdb files that have the GDB_GeomColumns string
 * Decode Z coordinate for a POINTZM shape
 * Aad support for decoding multipoint/multipointz geometries
 * Fix setting of the layer geometry type
 * Add support for zlib compressed streams
 * Implement MultiPatch decoding

SDE driver:
 * Add support for decoding NSTRING fields (#4053)
 * Add support in CreateLayer() to clean up partially registered tables that aren't full spatial layers
 * Add logic to force envelope for geographic coordsys objects (#4054)
 * Add USE_STRING layer creation and configuration options information
 * Set SE_MULTIPART_TYPE_MASK for multipolygon layers (#4061).
 * Change how offset and precision are set for geographic coordinate systems to more closely match SDE

SEGUKOOA driver:
 * New for GDAL/OGR 1.9.0
 * Read files in SEG-P1 and UKOOA P1/90 formats

SEGY driver:
 * New for GDAL/OGR 1.9.0
 * Read files in SEG-Y format

Shapefile driver:
 * Encoding support for shapefile/dbf (#882)
 * Allow managing datasources with several hundreds of layers (#4306)
 * Lazy loading of SRS and lazy initialization of attribute index support
 * Use VSI*L API to access .qix spatial index files
 * Add special SQL command 'RECOMPUTE EXTENT ON layer_name' to force recomputation of the layer extent (#4027)
 * Faster implementation of GetFeatureCount() in some circumstances.
 * Fix crash in CreateField() if there is no DBF file
 * Fix add field record flushing fix (#4073)
 * Fix decoding of triangle fan in a multipatch made of several parts (#4081)
 * Refuse to open a .shp in update mode if the matching .dbf exists but cannot be opened in update mode too (#4095)
 * Recognize blank values for Date fields as null values (#4265)
 * Recognize 'NULL' as a valid value for SHPT creation option as documented
 * Check that we are not trying to add too many fields.
 * Support reading measure values as Z coordinate.

SQLite/Spatialite driver:
 * Spatialite: major write support improvements (creation/update of Spatialite DB now limited to GDAL builds with libspatialite linking)
 * Spatialite: add support for 3D geometries (#4092)
 * Spatialite: speed-up spatial filter on table layers by using spatial index table (#4212)
 * Spatialite: add support for reading Spatialite views registered in the views_geometry_columns
 * Spatialite: better support for building against amalgamated or not
 * Spatialite: when it exists, use srs_wkt column in spatial_ref_sys when retrieving/inserting SRS
 * Spatialite: add COMPRESS_GEOM=YES layer creation option to generate Spatialite compressed geometries
 * Spatialite: add support for VirtualXLS layers.
 * Spatialite: imported VirtualShape support, in particular it is now possible to open on-the-fly a shapefile as a VirtualShape with 'VirtualShape:shapefile.shp' syntax as a datasource
 * Implement RFC35 (DeleteField, AlterFieldDefn, ReorderFields)
 * Implement DeleteDataSource()
 * Implement DeleteFeature()
 * Implement SetFeature() by using UPDATE instead of DELETE / INSERT
 * Add capability to use VSI Virtual File API when needed (if SQLite >= 3.6.0)
 * Make CreateDataSource(':memory:') work
 * Enforce opening update/read-only mode to allow/forbid create/delete layers, create/update features (#4215)
 * Launder single quote character in table name; properly escape table name if no laundering (#1834)
 * Use ALTER TABLE ADD COLUMN by default to create a new field; older method can still be used by defining the OGR_SQLITE_USE_ADD_COLUMN config option to FALSE in order to provide read-compat by sqlite 3.1.3 or earlier
 * Fix bug in CreateField() : if there was already one record, the content of the table was not preserved, but filled with the column names, and not their values
 * Map 'DECIMAL' columns to OGR real type (#4346)
 * Add OGR_SQLITE_CACHE configuration option for performance enhancements
 * Try to reuse INSERT statement to speed up bulk loading.

SVG driver:
 * New for GDAL/OGR 1.9.0
 * Read only driver for Cloudmade Vector Stream files

S57 driver:
 * Add support for Dutch inland ENCs (#3881)
 * Allow up to 65536 attributes, use GUInt16 for iAttr (#3881)
 * Be cautious of case where end point of a line segment has an invalid RCID
 * Correct handling of update that need to existing SG2D into an existing feature without it (#4332)

VRT driver:
 * Do not try to read too big files
 * Lazy initialization of OGRVRTLayer
 * Don't set feature field when source feature field is unset

WFS driver:
 * Add preliminary support for WFS 2.0.0, but for now don't request it by default.
 * Increase performance of layer definition building by issuing a DescribeFeatureType request for several layers at the same time
 * Better server error reporting
 * Use the layer bounding box for EPSG:4326 layers (restricted to GEOSERVER for now) (#4041)
 * Add capability of opening a on-disk Capabilities document
 * Add special (hidden) layer 'WFSLayerMetadata' to store layer metadata
 * Add special (hidden) layer 'WFSGetCapabilities' to get the raw XML result of the GetCapabilities request
 * CreateFeature()/SetFeature(): use GML3 geometries in WFS 1.1.0 (make TinyOWS happy when it validates against the schema)
 * Make spatial filtering work with strict Deegree 3 servers
 * Fix reading when layer names only differ by their prefix

XLS driver:
 * New for GDAL/OGR 1.9.0
 * Read only driver for MS XLS files and relies on FreeXL library.

XPlane driver:
 * Port to VSI*L API

XYZ driver:
 * Ignore comment lines at the beginning of files

## SWIG Language Bindings

General :
 * RFC 30: Correct the signature of Datasource.CreateDataSource() and DeleteDataSource(),  gdal.Unlink() to accept UTF-8 filenames (#3766)
 * Add Band.GetCategoryNames() and Band.SetCategoryNames()
 * Add Geometry.GetPoints() (only for Python and Java) (#4016)
 * Add Geometry.GetEnvelope3D()
 * Add Geometry.SimplifyPreserveTopology()
 * Extend SWIG Geometry.ExportToJson() to accept a list of options (#4108)
 * Add (OGR)DataSource.SyncToDisk()
 * Add SpatialReference.SetVertCS(), IsSameVertCS(), IsVertical(), SetCompound(), IsCompound()
 * Add SpatialReference.SetIGH() (#4060)
 * RFC 35: Add OGRLayer.DeleteField(), ReorderField(), ReorderFields(), AlterFieldDefn()
 * Add gdal.VSIFTruncateL()

CSharp bindings:
 * Implement the typemap for utf8_path in C# (#3766)
 * Correcting the signature of OGRDataSource.Open to make the utf8 typemap to work (#3766)

Java bindings:
 * Turn the gdalJNI, gdalconstJNI and osrJNI into package private classes
 * Make Layer.GetExtent() return null when OGR_L_GetExtent() fails.

Perl bindings:
 * The "Points" method of Geometry was not accepting its own output in the case of a single point. It accepted only a point as a list. Now it accepts a point both as a list containing one point (a ref to a point as a list) and a point as a list.
 * Fixed UTF-8 support in decoding names (datasource, layer, field etc.).
 * Assume all GDAL strings are UTF-8, handle all changes in typemaps.
 * Additions to Perl bindings due to new developments etc: Layer capabilities, GeometryType  method for Layer, improved create, new Export and Set methods for SpatialReference.
 * Detect context in a typemap which returns a array, this now returns a list in list context; the change affects at least GetExtent and GetEnvelope methods, which retain backward compatibility though new and/or changed methods: FeatureDefn::Name, FeatureDefn::GeometryIgnored, FeatureDefn::StyleIgnored, Feature::ReferenceGeometry, Feature::SetFrom, FieldDefn::Ignored, Geometry::AsJSON
 * Perl typemaps: more correct manipulation of the stack, more cases where a list is returned in a list context, better handling of callback_data @Band::COLORINTERPRETATIONS, Band methods Unit, ScaleAndOffset, GetBandNumber, RasterAttributeTable method LinearBinning
 * Typemaps for VSIF{Write|Read}L, tests and docs for some VSI* functions.
 * Perl bindings: better by name / by index logic, some checks for silent failures, return schema as a hash if wanted, support ->{field} syntax for features, return list attributes as lists or listrefs as wished so that ->{field} works for lists too (API change)

Python bindings:
 * Improvements for ogr.Feature field get/set
 * Add hack to bring Python 3.2 compatibility
 * First argument of VSIFWriteL() should accept buffers for Python3 compat
 * Fix reference leak in 'typemap(in) char **dict'
 * Add gdal.VSIStatL()
 * swig/python/setup.py : fix for virtualenv setups (#4285)
 * Layer.GetExtent() : add optional parameter can_return_null that can be set to allow returning None when OGR_L_GetExtent() fails
 * Make gdal.VSIFSeekL(), gdal.VSIFTellL() and gdal.VSIFTruncateL() use GIntBig instead of long for compat with 32bit platforms
 * Add script to build the python extensions with Python 2.7 and a mingw32 cross-compiler under Linux/Unix

Ruby bindings:
 * Build SWIG Ruby Bindings against modern Ruby versions (1.8.7 and 1.9.2) (#3999)

# GDAL/OGR 1.8.0 release notes

(Note: Most changes/bugfixes between 1.7.0 and 1.8.0 that have already gone
to the 1.7.X maintenance releases are not mentioned hereafter.)

## In a nutshell...

* New GDAL drivers : GTX, HF2, JPEGLS, JP2OpenJPEG, JPIPKAK, KMLSUPEROVERLAY,
                     LOS/LAS, MG4Lidar, NTv2, OZI, PDF, RASDAMAN, XYZ
* New OGR drivers : AeronavFAA, ArcObjects, GPSBabel, HTF, LIBKML, MSSQLSpatial, NAS,
                    OpenAir, PDS, PGDump, SOSI, SUA, WFS
* Significantly improved OGR drivers : DXF, GML
* New implemented RFCs : RFC 7, RFC 24, RFC 28, RFC 29, RFC 30, RFC 33
* New utility : gdallocationinfo

## Backward compatibility issues

* MITAB driver: use "," for the OGR Feature Style id: parameter delimiter,
  not "." as per the spec. Known impacted application :
  MapServer (http://trac.osgeo.org/mapserver/ticket/3556)
* RFC 33 changes the way PixelIsPoint is handled for GeoTIFF (#3838,#3837)
* GML driver: write valid <gml:MultiGeometry> element instead of the non-conformant
  <gml:GeometryCollection>. For backward compatibility, recognize both syntax for
  the reading part (#3683)

## GDAL/OGR 1.8.0 - General Changes

Build(All):
 * Make sure that 'import gdal' can work in a --without-ogr build

Build(Unix):
 * Fix compilation on RHEL/Centos 64bit for expat and sqlite3 (#3411)
 * Update to autoconf 2.67 and libtool 2.2.6
 * During the external libtiff autodetection check whether library version is 4.0
   or newer, fallback to internal code otherwise. It is still possible to link
   with older libtiff using the explicit configure option (#3695)
 * Make --with-threads=yes the default
 * Allow using --with-spatialite=yes
 * Check /usr/lib64/hdf for RedHat 64bit

Build(Windows):
 * Change the default MSVC version to VS2008.

## GDAL 1.8.0 - Overview of Changes

Port:
 * RFC 7 : Use VSILFILE for VSI*L Functions (#3799)
 * RFC 30 : Unicode support for filenames on Win32
 * Implement Rename() for /vsimem
 * New virtual file system handlers :
    - /vsicurl/ : to read from HTTP or FTP files (partial downloading)
    - /vsistdin/ : to read from standard input
    - /vsistdout/ : to write to standard output
    - /vsisparse/ :mainly to make testing of large file easier
    - /vsitar/ : to read in .tar or .tgz/.tar.gz files
 * Add C API to create ZIP files
 * Add support for writable /vsizip/
 * Add VSIBufferedReaderHandle class that is useful to improve performance when
   doing backward seeks by a few bytes on underlying file handles for which
   backwardseeks are very slow, such as GZip handle
 * Add service for base64 decoding
 * CPL ODBC : Add transaction support (#3745)
 * CPL ODBC: Increase the default connection timeout to 30 sec
 * Add VSIStatExL() that has a flag to specify which info is really required
   (potential speed optimization on slow virtual filesystems such as /vsicurl)
 * Add VSIIsCaseSensitiveFS() to avoid ugly #ifndef WIN32 / #endif in the code of
   various drivers
 * Add Recode() convenience method to CPLString
 * HTTP downloader: add PROXY and PROXYUSERPWD options (and GDAL_HTTP_PROXY and
   GDAL_HTTP_PROXYUSERPWD configurations option) to allow request to go through a
   proxy server.

Core:
 * RFC 24: progressive/async raster reading
 * On Unix, add capability of opening the target of a symlink through GDALOpen()
   even if it not a real filename. Useful for opening resources expressed as
   GDAL virtual filenames in software offering only file explorers (#3902)
 * Assume anything less than 100000 for GDAL_CACHEMAX is measured in megabytes.
 * Read cartesian coordinates if applicable in GDALLoadOziMapFile().
 * Avoid being overly sensitive to numeric imprecision when comparing pixel
   value and nodata value in GDALRasterBand::ComputeStatistics()/
   ComputeRasterMinMax(), especially for GeoTIFF format where nodata is
   stored as text (#3573)
 * Better handling of NaN (not a number) (#3576)
 * Add C wrapper GDALSetRasterUnitType() for GDALRasterBand::SetUnitType() (#3587)
 * Add GDALLoadRPCFile() to read RPCs from GeoEye _rpc.txt files (#3639)
 * Allow GDALLoadRPB/RPC/IMDFile() to be called directly with the RPB/RPC/IMD
   filename
 * In GDAL cache block, use 64-bit variables for cache size
 * Add GDALSetCacheMax64(), GDALGetCacheMax64() and GDALGetCacheUsed64() (#3689)
 * Improve formatting of seconds in DecToDMS()
 * Support negative nPixelOffset values for RawRasterBands
 * GDALDatasetCopyWholeRaster(): improve performance in certain cases by better
   fitting to input/output block sizes
 * Add GDALRasterBandCopyWholeRaster()
 * Make sure band descriptions are properly captured and cloned (#3780)
 * GDALDataset/GDALRasterBand::CreateMaskBand(): invalidate pre-existing raster
   band mask that could be created lazily with GetMaskBand()/GetMaskFlags(),
   so that a later GetMaskBand() returns the newly created mask band
 * Overview computation : speed improvements in resampling kernels
 * Fix dereferencing of open datasets for GetOpenDatasets (#3871)
 * Add DllMain callback to set-up and tear-down internal GDAL library
   resources automatically (#3824)
 * List .aux file if it used in GDALPamDataset::GetFileList()
 * PAM dataset : try retrieving projection from xml:ESRI metadata domain

Algorithms:
 * rasterize: Burn the attribute value in ALL the bands during rasterization. (#3396)
 * geoloc : Allow using XBAND and YBAND with height == 1 in the case of a regular
   geoloc grid, suc h as for LISOTD_HRAC_V2.2.hdf (#3316)
 * GDALFillNodata(): improve&fix progress report
 * warper : Try to determine if we will need a UnifiedSrcDensity buffer
            when doing memory computations (#3515).
 * warper : GDALSuggestedWarpOutput2(): use more sample points around the edge
            of the raster to get more accurate result (#3742)
 * warper : added (preliminary) support for mask bands that aren't nodata or alpha
 * warper : integrate Google Summer of Code OpenCL implementation of warper
 * gdalgrid: Move ParseAlgorithmAndOptions from apps/gdal_grid.cpp to
             alg/gdalgrid.cpp (#3583)
 * RPCTransformer: take into account optional DEM file to extract elevation
   offsets (RPC_HEIGHT_SCALE and RPC_DEM transformation options added) (#3634)
 * GDALReprojectImage() : correctly assign nSrcAlphaBand and nDstAlphaBand (#3821)
 * gdalgrid : Properly initialize the first nearest distance in GDALGridNearestNeighbor().

Utilities :
 * gdallocationinfo : new
 * nearblack: add -setalpha option to add/set an alpha band + -of, -q, -co
 * nearblack: add -setmask option to use a mask band to mask the nodata areas
 * gdalbuildvrt: support stacking ungeoreferenced images when using -separate,
   provided they have the same size (#3432)
 * gdalbuildvrt: implement a check to verify that all color tables are identical
 * gdalbuildvrt: automatically create a VRT mask band as soon one of the sources
   has a dataset mask band (non-trivial = neither alpha, neither alldata, neither nodata)
 * gdalbuildvrt: use OSRIsSame() to check if all source raster have same SRS (#3856)
 * gdal_translate: Transfer GEOLOCATION in the -of VRT case if spatial
   arrangement of the data is unaltered
 * gdal_translate : add support for resizing datasets with mask bands
 * gdal_translate : add -mask option to add a mask band from an input band/mask band.
   Also extend syntax for the value of the -b option to allow specifying mask band as input band
 * gdal_translate : support '-a_nodata None' as a way of unsetting the nodata value
 * gdal_translate : invalidate statistics when using -scale, -unscale, -expand, -srcwin,
                    -projwin or -outsize and a new -stats option to force their (re)computation (#3889)
 * gdal_rasterize: Add capability of creating output file (#3505)
 * gdaldem: add a new option, -compute_edges, that enable gdaldem to compute
   values at image edges or if a nodata value is found in the 3x3 window,
   by interpolating missing values
 * gdaldem : add '-alg ZevenbergenThorne' as an alternative to Horn formula
   for slope, aspect and hillshade
 * gdaldem : support GMT .cpt palette files for color-relief (#3785)
 * gdalwarp: add -crop_to_cutline to crop the extent of the target dataset to
   the extent of the cutline
 * gdalwarp: add a -overwrite option (#3759)
 * gdal_grid : Properly use the spatial filter along with the bounding box.
 * epsg_tr.py: added -copy format for INGRES COPY command
 * hsv_merge.py: support RGBA dataset as well as RGB dataset, add -q and -of
   options, avoid using hillband when it is equal to its nodata value
 * val_repl.py: copy geotransform and projection from input dataset to output
   dataset
 * gdal_retile.py : assign color interpretation (#3821)
 * gdal_retile.py : add -useDirForEachRow option to create a different output structure (#3879)
 * Make gdal_translate and gdalwarp return non-zero code when block writing failed
   for some reason (#3708)
 * loslas2ntv2.py : new utility : .los/.las to NTv2 converter
 * gdal_calc.py : new utility
 * Add -tap option to gdal_rasterize, gdalbuildvrt, gdalwarp and gdal_merge.py
   to align on a standard grid (#3772)

AAIGRID driver:
 * Cast nodata value to float to be consistent with precision of pixel data in
   GDT_Float32 case; small optimization to avoid reading the first 100K when
   we know that the datatype is already Float32
 * Allow reading files where decimal separator is comma (#3668)
 * Detect 1e+XXX as a real value among integer values (#3657)
 * Add a AAIGRID_DATATYPE configuration option that can be set to Float64
 * speed-up CreateCopy(), particularly on windows, by buffering the output

AIGrid driver:
 * Support sparse sets of tile files {w,z}001???.adf (#3541)

BSB driver:
 * Capture extension lines for headers
 * Added UNIVERSAL TRANSVERSE MERCATOR, LCC and POLYCONIC handling (#3409)
 * provide an option (BSB_IGNORE_LINENUMBERS) to ignore line numbers as some
   generators do them wrong but the image is otherwise readable (#3776)
 * Avoid turning missing values to index 255 (#3777)

DODS driver:
 * Compilation fix to support libdap 3.10

DTED driver:
 * Add origin metadata in original format (#3413)
 * Report NIMA Designator field as 'DTED_NimaDesignator' metadata (#3684)
 * Fixes to read some weird DTED3 file

ECW driver:
 * Support building against 4.1 SDK (compat with older versions maintained) (#3676)
 * Add alpha support with 4.1 SDK, and various configuration options
 * Add pseudo powers of two overviews.

EHdr driver:
 * Improvements to deal with http://www.worldclim.org/futdown.htm datasets

ENVI driver:
 * Support tabulation character in .hdr files (#3741)
 * Support reading gzipped image file (#3849)

ERS driver:
 * Read "Units" child of the "BandId" node and set it as unit type for RasterBand.

FITS driver:
 * Accept files whose metadata list doesn't end with 'END' (#3822)

GeoRaster driver :
 * Suppress error when testing SRID code as EPSG (#3326)
 * Several improvements and fixes (#3424)
 * Deprecates JPEG-B compression (#3429)
 * Fix GetColorInterpretation() on RGBA's alpha channel (#3430)
 * Allows OS authentication (#3185)
 * Add support for Point Cloud, add transaction control wrapper
 * use OCI Bind to load VAT (#3277)
 * Change order of NODATA tag on XML metadata (#3673)
 * Add support for per band NoData value - Oracle 11g (#3673)
 * Add support to ULTCoordinate - (#3718)
 * Fix interleaving cache error (#3723)
 * Fix compress vs nbits order error (#3763)
 * Fix writing interleaved jpeg #3805 and reading default blocksize #3806
 * Add create option blocking=(YES,NO,OPTIMUM) #3807, also fix #3812

GRASS driver:
 * Update GDAL and OGR GRASS drivers to compile against GRASS 7.0SVN (#2953)

GTiff driver :
 * RFC 33 : Adjust PixelIsPoint handling (#3838,#3837)
 * Refresh internal libtiff with upstream
 * Refresh internal libgeotiff with upstream
 * Add PREDICTOR_OVERVIEW configuration option to set the predictor value for
   LZW or DEFLATE compressed external overviews; Also make sure that the
   predictor value gets well propagated in the case of internal overviews (#3414)
 * Add a COPY_SRC_OVERVIEWS creation option (for CreateCopy()) that copies
   existing overviews in the source dataset.
 * Make GetScale() and GetOffset() retrieve values from PAM if not available in
   internal metadata
 * Use GCP info from PAM if available
 * Support CreateCopy() on datasets with a color indexed channel and an alpha
   channel (#3547)
 * Allow reading geotransform when opening with GTIFF_DIR prefix (#3478)
 * Add a warning when clipping pixel values for odd-bits band
 * Make sure that 16bit overviews with jpeg compression are handled using 12bit
   jpeg-in-tiff (#3539)
 * Add GDAL_TIFF_OVR_BLOCKSIZE configuration option to specify block size used
   for overviews
 * Read RPCs from GeoEye _rpc.txt files (#3639)
 * Implement GetUnitType() and SetUnitType(); make sure to remove
   TIFFTAG_GDAL_METADATA tag if it existed before and there are no more
   metadata; fix to make sure we can unset offset & scale stored in PAM
 * Speed-up writing of blocks in case of multi-band 8 bit images
 * Support TIFF_USE_OVR config option to force external overviews
 * Add special ability to for xml:ESRI metadata into PAM
 * Try to detect build-time vs runtime libtiff version mismatch (*nix only)
 * Added logic to expand verticalcs using importFromEPSG() when possible
 * Create internal masks with deflate compression if available
 * Fix jpeg quality propagation (particularly remove warning when
   using a deflate compressed internal mask band with jpeg compressed main IFD)
 * Add support for JPEG_QUALITY_OVERVIEW configuration option for internal
   overviews when adding them after dataset reopening
 * auto-promote mask band to full 8 bits by default (unless
   GDAL_TIFF_INTERNAL_MASK_TO_8BIT is set TO FALSE).
 * add LZMA compression optional support (requires latest libtiff4 CVS HEAD)
 * Supporting writing compound coordinate systems.

GTX driver:
 * New for GDAL/OGR 1.8.0
 * Read NOAA .gtx vertical datum shift files.

GXF driver:
 * Cast nodata value to float to be consistent with precision of pixel data in
   GDT_Float32 case
 * Introduce a GXF_DATATYPE configuration option that can be set to Float64
 * Use GDALGetScanline() instead of GDALGetRawScanline() so that #SENS
   is applied to normally return things in conventional orientation as
   is assumed by the geotransform.  (#3816).

HDF4 driver:
 * Prevent reading nonexistent subdatasets
 * Allow reading 1D subdatasets, in particular for GEOLOC bands
 * Workaround strange test that swaps xsize, ysize and nbands for the particular
   case of the dataset of ticket #3316
 * Speed up access to HDF4_SDS datasets; allow multi-line block dimension for
   HDF4_EOS datasets (#2208)
 * HDF4_EOS_GRID : detect tile dimensions and use them as block size; increase
   HDF4_BLOCK_PIXELS default value to 1,000,000 (#3386)
 * Support reading of L1G MTL metadata (#3532)
 * Read as HDF if HDFEOS returned 0 datasets
 * Improve fetching the geolocation data in case of one-to-one mapping and
   abcence of dimension maps (#2079)
 * Properly set the GCP projection for MODIS Aerosol L2 Product.
 * Fetch scale/offset, unit type and descriptions for some HDF-EOS datasets.

HDF5 driver:
 * Avoid setting bogus projection if we don't get georeferencing from
   CreateProjections().  Avoid trying to operate if DeltaLat/Lon is zero.
   Avoid crashing on NULL poH5Object->pszPath in CreateMetadata().  (#3534)
 * Ensure backslashes are preserved in paths for UNC on win32 (#3851)

HF2 driver:
 * New for GDAL/OGR 1.8.0
 * Read and write HF2/HFZ heightfield raster

HFA driver:
 * Ensure that an .aux file created for overviews has AUX=YES set so a
   base raster will not be created.
 * Various robustness improvements (#3428)
 * Support pulling overviews from an .rrd file even if the .aux does not
   reference it (#3463)
 * avoid using empty names for layer, if we have one generate a fake name,
   use for overviews (#3570)
 * Add support for New Zealand Map Grid to HFA driver (#3613)
 * Support EPT_s8 in BASEDATA (#3819)
 * Substantial improvements for Rename/CopyFiles (#3897)

Idrisi driver:
 * Allow color items greater than maximum value (#3605)

JPEGLS driver:
 * New for GDAL/OGR 1.8.0
 * JPEG-LOSSLESS driver based on CharLS library

JP2KAK driver:
 * Allow quality as low as 0.01
 * Major restructuring, all reading now goes through DirectRasterIO (#3295)
 * Introduce YCC optimization
 * Ensure we fetch <= 8 bit images with their true precision (#3540)
 * Make JP2KAK_RESILIENT also turn off persist, and force sequential access (#4336)
 * Fix reading overviews via direct case (#4340)

JP2OpenJPEG:
 * New for GDAL/OGR 1.8.0
 * JPEG2 driver based on OpenJPEG library

JPIPKAK driver:
 * New for GDAL/OGR 1.8.0
 * JPIP driver based on Kakadu library

KMLSUPEROVERLAY driver :
 * New for GDAL/OGR 1.8.0
 * Added new plug-in GDAL Super-Overlay Driver. The driver allows converts
   raster (like TIF/GeoTIFF, JPEG2000, JPEG, PNG) into a directory structure
   of small tiles and KML files which can be displayed in Google Earth.

LOS/LAS driver:
 * New for GDAL/OGR 1.8.0
 * Read NADCON .los/.las Datum Grid Shift files

MG4Lidar driver
 * New for GDAL/OGR 1.8.0
 * Read MG4 Lidar point cloud data and expose it as a Raster. It depends on
   the current, freely-available-though-not-open-source MG4 Lidar SDK v1.1

MrSID:
 * Updated to support MrSID SDK v8.0.0 (compat with older versions maintained) (#3889)
 * Updated to support writing MG4/Raster (#3889)
 * Support reading projection from .met files accompanying NASA LandSat SID files

NetCDF driver:
 * Improve coordinate system support (#3425)
 * Add support for multiple standard_parallel tags to support LCC single
   standard parallel (#3324)
 * Add CF-1 spheroid tag support for netcdf driver
 * Add support for weather/climate files with pixel size in km
 * Attempt to fix flip image (#3575)
 * Add support for Scale and Offset (#3797)

NITF driver:
 * Ensure that igeolo corners are not messed up if irregular, keep center/edge
   of pixel location info (#3347)
 * Add capture of select RPF attribute metadata (#3413)
 * Carry raw IGEOLO and ICORDS through as metadata (#3419)
 * Added NITFPossibleIGEOLOReorientation() in an attempt to deal with
   files written with the IGEOLO corners out of order.
 * Implement readonly support for RSets (#3457)
 * Add capability of writing CGM segment as creation option (or from the source
   CGM metadata domain if no CGM= creation option); for consistency, also add
   the capability of writing TEXT segment as creation option, in addition to
   the existing capability of writing it from the source TEXT metadata domain (#3376)
 * Fix read out of buffer for NBPP < 8 and very small block size; fix decoding
   of NBPP=4 (#3517)
 * Add FILE_TRE creation option to write TRE content in XHD field of file header
 * Add SDE_TRE creation option to write GEOLOB and GEOPSB TREs. This is limited
   to geographic SRS, and to CreateCopy() for now
 * Allow using NITF header located in STREAMING_FILE_HEADER DE segment when
   header at beginning of file is incomplete
 * Improve NITF to NITF translation
 * Fetch TREs from DE segment
 * Support reading CSSHPA DES & extracting embedded shapefile
 * Support writing image comments (ICOM)
 * Add description for NITF file & image header fields in creation options XML
 * Accept A.TOC files with frame entries that have same (row,col) coordinates
 * Avoid erroring out when file or image user TRE size is just 3
 * Load subframe mask table if present (typically, for CADRG/CIB images with IC=C4/M4) (#3848)
 * A few hacks to accept some (recoverable) file inconsistencies (#3848)

NTv2 driver:
 * New for GDAL/OGR 1.8.0
 * Read&write NTv2 Datum Grid Shift files

OZI driver:
 * New for GDAL/OGR 1.8.0
 * Read OZI OZF2/OZFX3 files

PAUX driver:
 * Add support for INTERLEAVE option

PCIDSK2 driver:
 * PCIDSK SDK refreshed from upstream
 * Remove svn:external for pcidsk sdk; Copy it directly in GDAL tree
 * Support for reading and writing descriptions added to the PCIDSK SDK.
 * Add bitmap support
 * Support for reading/writing complex PCIDSK files through libpcidsk
 * Support worldfile if lacking internal georef (#3544)
 * Fix locking state at CPLThreadMutex creation (#3755)
 * Improved projection support.

PDF driver:
 * New for GDAL/OGR 1.8.0
 * Read Geospatial PDF (through poppler library), either encoded according
   to OGC Best practice or Adobe ISO32000 extensions.

PDS driver:
 * Support quoted SAMPLE_TYPE. Check for UNSIGNED in SAMPLE_TYPE for UInt16.
 * Support files where scanlines are broken over several records.
 * Support newline continuation
 * Recognize ENCODING_TYPE = "N/A" (N/A surrounded by double-quotes)
 * Take into account MINIMUM, MAXIMUM, MEAN and STANDARD_DEVIATION when
   available to set the statistics

PNG driver:
 * Update internal libpng to 1.2.44
 * Internal libpng : Make screwy MSPaint "zero chunks" only a warning,
   not error (#3416).
 * Added ZLEVEL creation option

PostGIS Raster (formerly WKTRaster driver):
 * Improved block reading and raster settings reading in WKT Raster driver
 * Functions SetRasterProperties and GetGeoTransform modified to allow both
   referenced and not referenced rasters
 * Connection string parsing simplified. Schema, table name and where clause
   can be passed with or without quotes
 * New parameter "mode"

RASDAMAN driver:
 * New for GDAL/OGR 1.8.0
 * Read rasters in rasdaman databases

RMF driver:
 * Significant improvements. Implemented decompression scheme typically
   used in DEM data.

SRP driver:
 * Relax strict equality test for TSI size for unusual products (#3862)

TerraSAR driver:
 * Enhancements related to GCPs handling (#3564).

USGSDEM driver:
 * Support non-standard DEM file (#3513)

VRT driver:
 * Preliminary Overview support on VRT bands (#3457)
 * Support for mask band : VRT may expose a mask band,
   and mask bands can be used as VRTRasterBand sources
 * Port to VSIF*L API; advertise GDAL_DCAP_VIRTUALIO=YES
 * Make format identification less strict (#3793)
 * Support for LocationInfo metadata item on bands

WCS driver:
 * Decode base64 encoded multipart data

WMS driver:
 * Adds a <UserAgent> optional parameter so that the user be able to provide
   its own useragent string for picky WMS servers (#3464)
 * Default color interpretation for wms driver (#3420)
 * Add UnsafeSSL setting (#3882)

XYZ driver:
 * New for GDAL/OGR 1.8.0
 * Read ASCII XYZ gridded datasets

## OGR 1.8.0 - Overview of Changes

Core:
 * RFC 28 : OGR SQL Generalized Expressions
 * RFC 29 : Support for ignoring fields in OGR
 * Add OGRLayer::GetName() and OGRLayer::GetGeomType() virtual methods,
   and their C and SWIG mappings (#3719)
 * On Unix, add capability of opening the target of a symlink through OGROpen()
   even if it not a real filename. Useful for opening resources expressed as
   GDAL virtual filenames in software offering only file explorers (#3902)
 * Expat based XML readers : add support for reading files with Windows-1252
   encoding
 * Use transactions in CopyLayer for better speed. (#3335)
 * OGRGeometry::importFromWkt() : allow importing SF-SQL 1.2 style WKT while
   preserving compatibility with previously recognized non conformant WKT (#3431)
 * Add C functions : OGR_G_ForceToPolygon(), OGR_G_ForceToMultiPolygon(),
                     OGR_G_ForceToMultiPoint() and OGR_G_ForceToMultiLineString()
 * Add C functions : OGR_G_Length(), OGR_G_Simplify(), OGR_G_Area(), OGR_G_Boundary()
                     OGR_G_SymDifference() and OGR_G_UnionCascaded()
 * Add C function: OGR_F_StealGeometry()
 * Move Centroid() method from OGRPolygon to OGRGeometry base class to be able
   to operate on various geometry types, and to be consistent with PostGIS
   ST_Centroid() capabilities and the underlying GEOS method
 * Make the GetStyleTable() SetStyleTable() SetStyleTableDirectly() methods on
   datasources and layers virtual (#2978)
 * Add OGRSFDriverRegistrar::DeregisterDriver() and OGRDeregisterDriver()
 * Improve detection of rounding errors when writing coordinates as text with
   OGRMakeWktCoordinate()
 * OGR SQL: allow comparing datetime columns in WHERE clause
 * OGR indexing: re-use .ind file in read-write mode when calling CreateIndex()
   but the index was opened as read-only (follow up of #1620); ensure that the
   .ind file is closed before being unlink()'ed
 * AssemblePolygon: ensure largest area ring is used as exterior ring (#3610)
 * OGRGeometryFactory::createFromGEOS() : preserve coordinate dimension
   (with GEOS >= 3.3) (#3625)
 * Allow calling transformWithOptions() with a NULL poCT
 * Improve wrapdateline, especially on LINESTRING
 * Fix getEnvelope() for OGRPolygon and OGRGeometryCollection to avoid taking
   into empty sub-geometries; Fix OGRLayer::GetExtent() to avoid taking into
   account empty geometries
 * Support attribute index scan with the sql 'IN' operator (#3686)
 * Add attribute index support for the sql queries in mapinfo tab format (#3687)
 * OGRGometry: add a swapXY() virtual method
 * Implement special field support for IsFieldSet
 * OGRLineString::transform() : allow partial reprojection if
   OGR_ENABLE_PARTIAL_REPROJECTION configuration option is set to YES (#3758)
 * Add OGR_G_ExportToGMLEx() that can take options to enable writing GML3
   geometries compliant with GML3 SF-0
 * OGRFeature::SetField() : support setting integer and real lists from a
   string in the format (n:value,value,value,...)

OGRSpatialReference:
 * Big upgrade to EPSG 7.4.1 with improved datum logic
 * Use PROJ 4.8.0 thread-safe functions if available to avoid global OGR PROJ4
   mutex when doing OGRProj4CT::TransformEx()
 * Support for defining VERT_CS and COMPD_CS from EPSG and from/to PROJ.4
 * Implement OGRSpatialReference:IsVertival() and
   OGRSpatialReference::IsSameVertCS() methods
 * add RSO gamma handling (proj #62)
 * TMSO support
 * Adjust handling of NAD27 to avoid towgs84 params, use +datum when no towgs84,
   do not emit +ellipse if +datum used (#3737)
 * exportToProj4() : add +towgs84= instead of +datum= if both information are
   available. This behavior can be turned off by setting
   OVERRIDE_PROJ_DATUM_WITH_TOWGS84=NO (#3450)
 * Add PROJ4_GRIDS EXTENSION as a way of preserving datum grids
 * ogr_srs_proj4: add a table for Prime Meridians; improve recognition of prime
   meridian to export them as names when possible with exportToProj4()
 * importFromProj4(): recognize +f= option
 * Correct handling of Mercator2SP from EPSG (#2744)
 * Make GetAxis() const
 * Improve axis orientation recognition for stuff like EPSG:3031
 * Fix Amersfoort (geotiff #22)
 * Panorama: Added zone number to the list of projection parameters list.
   Use the zone number when we need to compute Transverse Mercator projection.
 * Panorama: Use Pulkovo 42 coordinate system instead of WGS84 as a fallback
   if the CS is not specified.
 * ESRI: added support for Mercator in an old style file
 * Add Bonne, Gauss-Schreiber Transverse Mercator, Mercator (2SP), Two Point
   Equidistant and Krovak to the list of projections description dictionary.
 * SRS validation : various fixes
 * Exposure OSRImportFromERM() and OSRExportToERM() functions in C API.
 * PCI : Fix the transfer of scale for Stereographic Projection (#3840).
         Add support for Oblique Stereographic (SGDO) (#3841)
 * Substantially upgrade PCI datum conversions using PCI datum/ellips.txt files

Various drivers:
 * Improve behavior of DXF, VFK, GPX, SHAPE, PG, LIBKML, KML, VRT, CSV, GML,
   BNA, GeoRSS, GEOJSON drivers when LC_NUMERIC is not the C locale

Utilities:
 * ogr2ogr: copy datasources and layers style table
 * ogr2ogr: use OGRGeometryFactory::forceToMultiLineString() when -nlt
            MULTILINESTRING is specified
 * ogr2ogr: allow -wrapdateline if neither input or output srs is specified,
            but input layer srs is geographic
 * ogr2ogr: add -splitlistfields and -maxsubfields options to split fields of
            type IntegerList, RealList or StringList into as many subfields of
            single type as necessary.
 * ogr2ogr: accept None or Null as a special value of -a_srs to nullify the output SRS
 * ogr2ogr: ignore -overwrite options if the output datasource does not yet exist (#3825)
 * ogr2ogr: special case when output datasource is a existing single-file Shapefile :
            auto-fill the -nln argument if not specified (#2711)
 * ogr2ogr: add a -explodecollections option to split multi geometries into several features
 * ogr2ogr: add a -zfield option to set the Z coordinate of a 3D geometry from the value of
            a field

AeronavFAA driver:
 * New for GDAL/OGR 1.8.0

ArcObjects driver:
 * New for GDAL/OGR 1.8.0

BNA driver:
 * Ported to use VSIF*L API

CSV driver:
 * For files structured as CSV, but not ending with .CSV extension, the 'CSV:'
   prefix can be added before the filename to force loading by the CSV driver
 * Support reading airport data coming from http://www.faa.gov/airports/airport_safety/airportdata_5010
 * If a datasource is created with the extension .csv assume this should be
   the first layer .csv file instead of a directory
 * Skip empty lines (#3782)
 * Port to VSI*L API for read&write; support writing to /vsistdout/

DGN driver:
 * Correct computation of abyLevelsOccurring (#3554).

DODS driver:
 * Compilation fix to support libdap 3.10

DXF driver:
 * Smooth polyline entity support added
 * Read blocks as a distinct layer instead of inlining
 * Assemble file at end with changes to header template - so far only inserting
   new layer definitions
 * Support for writing user defined blocks
 * Add limited hatch support
 * Add support for writing linetypes.  Support using complete dxf files as
   the header or trailer template.
 * Apply the INSERT entity id to all features inserted in its place (#3817)
 * various fixes
 * Implement Win1252/utf8 conversion for dxf text
 * Fix issues with text angles, text escape and multiline text
 * add support for \U+xxxx unicode chars in labels

GeoJSON driver:
 * Add reader to parse JSON output of FeatureService following GeoServices REST
 * Read and write "id" member at feature object level
 * Various robustness fixes to avoid crashes
 * Fix combined spatial and attribute filtering (#3803)

GeoRSS driver:
 * Ported to use VSIF*L API
 * Recognize <gml:MultiPoint>, <gml:MultiLineString> and <gml:MultiPolygon>
 * Support reading GeoRSS inside <rdf:RDF>

GML driver:
 * Adding support for xlink:href. (#3630)
 * Add support for Polish TBD GML
 * Support reading <gml:Curve>, <gml:MultiCurve>, <gml:TopoCurve>,
   <gml:TopoSurface>, <gml:Ring>, <gml:Surface>, <gml:PolygonPatch>,
   <gml:pointMembers>, <gml:curveMembers>, <gml:surfaceMembers>
   <gml:Triangle>, <gml:Rectangle>, <gml:Tin/gml:TriangulatedSurface>,
   <gml:Arc>, <gml:Circle> elements in GML3 geometries
 * Recognize <gml:Solid> and <gml:CompositeSurface>, <gml:OrientableSurface> elements
   (dealt as an approximation as multipolygons, and not as volumes)
 * Add support for "complex structure flattening" of attributes, and OFTStringList,
   OFTRealList and OFTIntegerList field types in case of multiple occurrences of
   a GML element (such as UK Ordnance Survey Mastermap) (#3680)
 * Add support for CityGML generic attributes <stringAttribute>, <intAttribute> and
   <doubleAttribute>
 * Various improvements for better support of AIXM 5.1
 * Write and retrieve layer geometry type to/from .gfs file (#3680)
 * Support using the <GeometryElementPath> to retrieve the appropriate geometry in
   case several ones are available per feature
 * Use VSIF*L API for read&write
 * XSD reader : various improvements, in particular to support various types of schema
   returned by WFS DescribeFeatureType
 * XSD writer: change the default GeometryPropertyType to a more precise type name
   according to the layer geometry type
 * Write valid <gml:MultiGeometry> element instead of the non-conformant
   <gml:GeometryCollection>. For backward compatibility, recognize both syntax for
   the reading part (#3683)
 * Support reading SRS per layer when possible, and deal with urn:ogc:def:crs:EPSG::xxx
   geographic coordinate systems (as returned by WFS 1.1.0 for example) to restore
   (longitude, latitude) order (unless GML_INVERT_AXIS_ORDER_IF_LAT_LONG is set to NO)
   Also add a GML_CONSIDER_EPSG_AS_URN option that can be set to YES when EPSG:XXXX should
   be considered as urn:ogc:def:crs:EPSG::XXXX
 * Expose gml:id as a string field when reading <wfs:FeatureCollection>
 * Add dataset creation option FORMAT=GML3 to write GML3 SF-0 compliant data
 * Add dataset creation option SPACE_INDENTATION=YES/NO to optionally disable space indentation
   when writing GML.
 * Recognize GML answer of MapServer WMS GetFeatureInfo request
 * Fix datatype detection to fallback to Real when an integer cannot fit into a 32bit int (#3866)
 * GML/WFS : use SRS defined in global gml:Envelope if no SRS is set for any feature geometry

GMT driver:
 * Add support for multilinestring reading (#3802)

GPSBabel driver:
 * New for GDAL/OGR 1.8.0
 * Read/Write files supported by GPSBabel utility

GPX driver:
 * Port write side of the driver to VSIF Large API
 * Add LINEFORMAT dataset creation option
 * Allow writing track points and route points with their own attributes by
   writing point features in track_points and route_points layers

HTF driver:
 * New for GDAL/OGR 1.8.0
 * Read Hydrographic Transfer Format (HTF)

Ingres driver:
 * Implement support for spatial reference systems (atrofast, #3159)
 * Added support for GEOMETRYCOLLECTION as a generic geometry type, and
   fleshed out the layer creation logic to support all geometry types.

KML driver:
 * KML vertex output: avoid warning for coordinates just above 90 and 180
   degrees due to numerical imprecisions
 * Port to VSI*L API for write

LIBKML driver:
 * New for GDAL/OGR 1.8.0
 * Alternate KML driver relying on Google libkml

MITAB driver:
 * Use "," for the OGR Feature Style id: parameter delimiter, not "." as per
   the spec.
 * Synchronized with mitab CVS HEAD
 * Fixed crash when trying to get the same mitab feature twice

MSSQLSpatial driver:
 * New for GDAL/OGR 1.8.0
 * Read/write support for MS SQL Spatial databases

NAS driver:
 * New for GDAL/OGR 1.8.0
 * Reads the NAS/ALKIS format used for cadastral data in Germany

OCI driver:
 * Allows OS authentication (#3185)
 * Uppercase table_name on SQL queries #1960 - OCI: ogr2ogr with append option
   is not considering layer creation
 * Support creation of non-spatial tables (#3690)
 * Set MULTI_LOAD as default #3656, for new layer and update

OGDI driver:
 * Add OGR_OGDI_LAUNDER_LAYER_NAMES configuration option to simplify reported layer names
 * Fix GetFeatureCount() when used with SetAttributeFilter()

OpenAir driver:
 * New for GDAL/OGR 1.8.0
 * Read Special Use Airspace in OpenAir format

PCIDSK driver:
 * Implement creation/update and coordinate system support

PDS driver:
 * New for GDAL/OGR 1.8.0
 * Read NASA Planetary Data Systems TABLE objects

PGeo driver:
 * Add PGEO_DRIVER_TEMPLATE option

PGDump driver:
 * New for GDAL/OGR 1.8.0
 * To output PostgreSQL SQL dump (very similar to shp2pgsql utility)

PostgreSQL driver:
 * Add support for PostgreSQL >= 9.0 new binary data format
 * Use canonical (HEXEWKB) form to get geometry to speed-up feature retrieval.
   WKT-based retrieval can still be used if PG_USE_TEXT configuration option
   is set to YES
 * If the PG_USE_BASE64 configuration option is set to YES, geometries will be
   requested as BASE64 encoded EWKB instead of canonical HEX encoded EWKB.
   (useful when bandwidth is the limiting factor)
 * Don't instantiate layer defn at layer creation. This can speed up
   significantly database opening when they are many tables and the user just
   needs to fetch one with GetLayerByName().
 * Implement efficient OGRLayer::GetName() and OGRLayer::GetGeomType()
 * Allow creating layer with eType = wkbNone
 * Don't require to have found a layer in readonly mode to succeed in opening
   the datasource.
 * Add TEMPORARY (ON/OFF) layer creation option to create TEMPORARY tables
 * CreateLayer() : don't launder schema_name if passed string is schema_name.table_name,
   and when testing if the layer already exists prepend or remove the
   schema_name to the table_name when comparing to layer names
 * Handle Nan and Inf values for float types in INSERT, UPDATE and
   COPY SQL commands. (#3667)
 * Retrieve the FID of a newly inserted feature (#3744)
 * Remove use of deprecated PostGIS functions when running against PostGIS 2.0SVN

S57 driver:
 * Avoid crashing if there is a missing VRPT field in a vector record.

SOSI driver:
 * New for GDAL/OGR 1.8.0
 * Read Norwegian SOSI-standard

Shapefile driver:
 * Use VSI*L API for .prj file so it works in virtual circumstances (#3536).
 * CreateLayer(): Check that the layer doesn't already exist
 * Implement multipatch read support
 * Remove ESRI style spatial indexes from Shapefile on update via OGR (#2798)
 * Fix conflict between spatial and attribute indexes (#3722)
 * Create integer fields with unknown width as 10 characters instead of 11
   to avoid them getting immediately considered to be real when reopening (#2151)
 * Make 'ogr2ogr -overwrite dst.shp src.shp -nln dst' work when dst.shp already exists

SQLite driver:
 * Spatialite : use MBRIntersects operator instead of MBRWithin (#3810)
 * Spatialite: avoid executing some Spatialite functions several times when
   issuing SQL queries
 * Add a INIT_WITH_EPSG dataset creation option to fill the spatial_ref_sys
   table with content of EPSG CSV files (default to NO); several fixes to be
   robust to single quote characters in SRS strings
 * Fix to make CopyLayer() work when src layer is a SQL result layer (#3617)
 * Add OGR_SQLITE_SYNCHRONOUS configuration option that, when set to OFF,
   can speed up considerably write operations (e.g. on EXT4 filesystems),
   at the expense of extra robustness w.r.t system crashes.
 * Spatialite: when creating a spatialiate DB, add a srs_wkt column in the
   spatial_ref_sys table, as introduced in libspatialite 2.4.0
 * Implement the DeleteLayer() interface and report the ODsCDeleteLayer capability

SUA driver:
 * New for GDAL/OGR 1.8.0
 * Read Special Use Airspace in Tim Newport-Peace's format

VRT driver :
 * Fix GetExtent() on non VGS_Direct VRT layers (#3783)

WFS driver:
 * New for GDAL/OGR 1.8.0
 * WFS client that brings read & write (WFS-T) support for WFS 1.0.0 and 1.1.0

## SWIG Language Bindings

General :
 * Add Geometry.Length(), Geometry.Area(), Geometry.Simplify(), Geometry.UnionCascaded(),
   Geometry.SymDifference, Geometry.Boundary()
 * Add SpatialReference.GetUTMZone()
 * Add Geometry.ForceToPolygon(), Geometry.ForceToMultiPolygon(),
       Geometry.ForceToMultiPoint() and Geometry.ForceToMultiLineString()
 * Add Band.SetScale() Band.SetOffset(), and Band.SetUnitType()
 * Add ogr.RegisterDriver() and ogr.DeregisterDriver()
 * Move ogr.GeometryTypeToName() and ogr.GetFieldTypeName() from Java bindings
   to all bindings
 * Initialize return values of GetRasterStatistics() so that we know if they
   have been updated
 * Add Feature.SetFromWithMap()
 * Add gdal.GridCreate() (#3661)
 * Expose OSR GetSemiMajor(), GetSemiMinor(), GetInvFlattening() and
   ImportFromERM() functions

CSharp bindings:
 * Use the .NET Framework 2.0 transparency rules (level 1 transparency) for the
   VC2010 builds (#3559)
 * Fix GDAL_DMD_ and GDAL_DCAP_ constants for Csharp (#3601)

Java bindings:
 * Add GdalGrid.java, GDALContour.java
 * Add 'make test' target for Unix/Windows

Perl bindings:
 * Support polygons in TransformPoints.
 * Test for existence of capability before comparison (avoid unnecessary warning)
 * Added CAPABILITIES lists to driver, datasource and layer classes;
   Capabilities and TestCapability methods return and use strings as documented
   already earlier; added FIELD_TYPES, JUSTIFY_TYPES, GEOMETRY_TYPES and
   BYTE_ORDER_TYPES lists into appropriate classes
 * "create" constructor for FeatureDefn
 * Add aliases Equals and Intersects as mentioned in #3492
 * Changes to support RFC 30
 * Add bindings for ReadDir, Stat, FillNodata
 * Simple support for PostGIS HEX EWKB (remove/add SRID)
 * The Schema method of FeatureDefn returns a schema, where each field hash contains
   also key 'Index' and the field index as the value.

Python bindings:
 * Fix compilation of Python bindings with Python 3.X on 64 bit platform
 * Optimize Band.ReadRaster() and Dataset.ReadRaster() to avoid extra buffer
   copy; also add the capability to pass the result buffer such as
   result_buf = ' '; ReadRaster(0, 0, 1, 1, buf_obj = result_buf) (#3521)
 * NumPy Band.WriteArray() : use numpy object directly instead of converting to
   string
 * Band.ReadAsArray() : instantiate the numpy object before calling RasterIO()
   instead of creating it from a string
 * NumPy : add support for signed byte
 * Make sure that feat.SetField('field_name', double_value) goes through
   OGR_F_SetFieldDouble() instead of being first converted to string
 * Add an optional parameter can_return_null to Dataset.GetGeoTransform();
   when used and set to True, None is returned when GDALGetGeoTransform()
   returns CE_Failure (instead of the fake (0,1,0,0,0,1)); backward
   compatibility preserved when the parameter isn't specified
 * Avoid suppressing warnings and errors when exceptions are used (#3632)
 * Add gdalinfo.py, ogrinfo.py and ogr2ogr.py as sample scripts, direct ports
   of corresponding C/C++ utilities
 * Allow manipulating buffer > 2 GB on 64bit builds for ReadRaster() and WriteRaster()
 * Map gdal.GetCacheMax(), gdal.SetCacheMax() and gdal.GetCacheUsed() to the
   corresponding 64bit new API
 * Reset error status before new GDAL/OGR call when using gdal.UseExceptions() (#3077)
 * Changes to support RFC 30
 * Fix Feature.ExportToJson() (#3870)

# GDAL/OGR 1.7.0

(Some of the bug fixes mentioned below might also have gone into 1.6.X maintenance releases.)

## In a nutshell...

* New GDAL drivers : BAG, EPSILON, Northwood/VerticalMapper, R, Rasterlite,
                     SAGA GIS Binary, SRP (USRP/ASRP), EarthWatch .TIL, WKT Raster
* GDAL PCIDSK driver using the new PCIDSK SDK by default
* New OGR drivers : DXF, GeoRSS, GTM, PCIDSK and VFK
* New utility : gdaldem, gdalbuildvrt now compiled by default
* Add support for Python 3.X. Compatibility with Python 2.X preserved (#3265)
* Remove old-generation Python bindings.
* Significantly improved GDAL drivers: GeoRaster, GeoTIFF, HFA, JPEG2000 Jasper, JPEG2000 Kakadu, NITF
* Significantly improved OGR drivers: CSV, KML, SQLite (SpatiaLite support), VRT
* WARNING: incompatibility between MrSID GeoDSDK and libgeotiff 1.3.0 or internal libgeotiff on some platforms (see #3309)

## GDAL/OGR 1.7.0 - General Changes

Build (All) :
 * Add gdalbuildvrt to the list of utilities built by default (#2747)
 * Improve Mingw compatibility (#2649)
 * Add Expat read support for GML driver when Xerces is unavailable (#2999)
 * Fix GML and ILI reading problems with Xerces 3.x (#3156)
 * Add 8/12bit JPEG-in-TIFF support
 * Fix trunk compilation against libdap 3.9.X (#3105)

Build (Windows) :
 * Check for CURL_LIB instead of CURL_DIR
 * ensure OGR_ENABLED gets defined if INCLUDE_OGR_FRMTS set (#2784)
 * Change quoting in VCDIR and SETARGV to avoid likely problems.
 * added dll and target-lib targets
 * fix _findfirst handle type for win64 (#3035)
 * Add support to compile OGR-postgis as a plugin (#3125)
 * Trap failures in subdirectories and stop build

Build (Unix) :
 * Use proper object file names when building prerequisites lists (#1878)
 * Updated man page generation
 * Add new ./configure test to check that the GCC 4.1 built-in functions for atomic memory access are really available
 * Handle external libz (#2942)
 * Add support for 64bit file i/o on BSD systems, through fseeko/ftello
 * Add support for linking against libspatialite to benefit from spatial functions in SQL queries (#2666)
 * Fix support for --with-threads configure option on BSDs (tested on DragonFlyBSD 2.2.1)
 * Add support for autodetection of ogdi 3.2 in ./configure (#3007)
 * Remove additional dependency to libgdal.so added during linking in gdal/apps (#2970)
 * Improved ax_oracle_oci.m4 macro to handle libnnzXX for Oracle 10 and 11.
 * support using the Oracle Instant Client SDK
 * Make --with-ingres work with newer versions (#3126)
 * Search for alternative HDF4 flavor in HDF4 testing macro. Alternative HDF4
   (libmfhdfalt/libdfalt) build is NetCDF-compatible and used in Debian.
 * Support MacOSX "fat" binary building

Port :
 * Provide API and implementation for thread and SMP safe atomic increments (#2648)
 * Add /vsisubfile virtual file driver
 * Added gzip write implementation
 * VSI ZIP : Allow natural chaining of VSI drivers without requiring double slash
 * Add a shortcut when looking for .csv files that are already open
 * Add CPLSetThreadLocalConfigOption()
 * Add CPLIsUTF8() and CPLForceToASCII(); Use them in GML, KML, GPX and GeoRSS drivers (#2971)
 * Add CPLStrlcpy() and CPLStrlcat(), clones of BSD strlcpy() and strlcat() functions
 * Add CPLStrnlen()
 * Add CSLLoad2() and CPLReadLine2L() with max number of lines and chars per line allowed
 * cplkeywordparser.cpp: Support parsing IMD files with values of the form list of lists
 * odbc: Fixed the null terminators for columns of length (x*511)+1, where x>1. (#2727)
 * unix VSIF*L : reworked to avoid unnecessary seeks which can be expensive
 * added HTTPAUTH and USERPWD options for http fetch (#3091)

## GDAL 1.7.0 - Overview of Changes

Algorithms:
 * cutline : optimize by clipping cutline to region of interest (#2736)
 * cutline : avoid scanline to scanline blending problems (#2740)
 * rasterfill : substantially reworked
 * rasterfill : deprecate bConicSearch option - unused
 * rasterize : optimized
 * rasterize : Added GDALRasterizeLayersBuf() function to rasterize geometries directly
               into the supplied raster array.
 * rasterize : Add ALL_TOUCHED rasterize option (#2840)
 * rasterize : Added 3D support for rasterizing points and lines. (#3202)
 * rasterize : correct case of vertical/horizontal lines on raster right/bottom edge (#3268)
 * Added GDALCreateGenImgProjTransformer3() function
 * warp: Reduce destination file size, especially when it has compression (#1688)
 * warp: Fix crash when reprojecting to irrelevant SRS (#3079)
 * warp: avoid using the destination nodata value for integer datatypes (#3142)
 * warp: fix panDstValid generation, and avoid using it improperly (#3222)
 * warp: Restore support of reprojection of a lat-long image crossing 180E/180W longitude (#3206)
 * contour: Fix name of GDAL_CG_Create in contour.cpp (#2795)
 * contour: Generate contours with correct orientation (#3129)
 * gdalgeoloc: Improve geoloc backmap interpolation (#2501)
 * overview: added support for cubic convolution overviews
 * gdal_grid: 3 new metrics: data point count, average distance between data points
              and a grid node, average distance between data points.
 * gdal_grid: Properly cast the poOptions parameter in data metrics computation functions. (#3207)

Core :
 * Added mechanism to clear overviews if zero overviews requested. Implemented by GTiff and HFA drivers (#2915)
 * Support for overviews in subdatasets
 * Support for overviews in proxydb directory (#2432)
 * SetColorTable(NULL) is permitted and means delete (#2421)
 * Preserve NBITS and SIGNEDBYTE in CreateCopy() for supporting drivers (#2902)
 * GDALCopyWords() : performance optimizations
 * Add GDALCheckDatasetDimensions() and GDALCheckBandCount()
 * Add GDALGetColorInterpretationByName()
 * Use tiff .ovr for overviews if .aux is just metadata (#2854)
 * Add missing argument in function declaration for GDALRATTranslateToColorTable()
 * Do not use colortable for bit2grayscale overviews (#2914)
 * Support world files with blank lines (ESRI Merge)
 * Add worldfiles in GetFileList() (ESRI Merge)
 * Fix rpb/imd loading with a path (#3047)
 * Add support for using overviews in GDALDataset::BlockBasedRasterIO() (#3124)
 * Take into account SIGNEDBYTE for GetStatistics() & GetHistogram() (#3151)
 * Add GDALReadOziMapFile() and GDALLoadOziMapFile() to read projection
   and georeferencing information from OziExplorer .MAP files.
 * Added declarations for GDALLoadTabFile() and GDALReadTabFile()
 * Add missing case for CInt16 and CInt32 in GDALNoDataMaskBand and GDALNoDataValuesMaskBand

Utilities :
 * gdaldem: new for GDAL/OGR 1.7.0 (#2640)
 * gdalinfo:
    - add -norat switch
    - do not report RPC or GEOLOCATION metadata with -nomd
    - Use pretty wkt to display GCP projection
 * gdalwarp:
    - fix cutline blend distance setting (#2733)
    - in -te case, adjust the resolution after computing the image dimensions
    - improved cutline support (#2733, #2847, #2884, #2984)
    - avoid overwriting an existing destination file that cannot be opened in update mode with a new GTiff file
    - better heuristics to guess output extent when projection from lat/long world
      extent to other world global projections (#2305)
 * gdaltindex:
    - Avoid unnecessary error message in gdaltindex when creating a new shapefile (#2779)
    - Rewritten to use OGR API instead of ShapeLib API, so as to produce .prj files more easily (#982)
 * gdal_contour:
    - make -3d option work even after -fl option (#2793)
    - Call GDALGetProjectionRef() on the dataset, not the raster band (#3062)
 * gdalbuildvrt:
    - Add -separate, -allow_projection_difference, -te, -tr, -q, -addalpha options
    - Add -srcnodata and -vrtnodata options (#3254)
    - Add -hidenodata option (#3327)
    - Avoid accidental overwriting of a non VRT dataset due to reversed filename order
    - Fix -resolution lowest/highest (#3198)
 * gdaladdo: add -clean option (#2915)
 * gdaladdo: add -q option
 * gdal_grid: Add support for spatial filtering with -clipsrc option
 * gdal_translate: support translation of a dataset with subdatasets if the parent has bands
 * gdal_translate: Add 'gray' as a value of -expand rgb option
 * gdal_translate: Add -unscale commandline option
 * gdal_merge.py: Add progress report
 * gdal_vrtmerge.py: Fix -separate case (#2836)
 * gdal_vrtmerge.py: Write the <SourceProperties> element (#1985)
 * gdal_retile.py: add lanczos resampling (#2859)
 * gdal_fillnodata.py: ensure dstfile support works by copying source (#2866)
 * GDAL2Tiles: --srcnodata support + fixed KML rendering for -p raster
 * rgb2pct.py: Added ability to use a preexisting color table from a file (#2958)
 * pct2rgb.py and rgb2pct.py : Copy the GCPs and their projection to the target dataset.
 * classify.py: Fix order of args to numpy.ones() and numpy.zeros() (#3089)
 * hsv_merge.py: New sample script to greyscale as intensity into an RGB image,
                 for instance to apply hillshading to a dem colour relief.
 * support filename globbing for various Python scripts (#2783)
 * --formats will add 'v' in report on drivers that support virtual io
 * all utilities and scripts : consistently advertise -q as the official quiet
                               option, but accept both -q and -quiet (#3820)

Changes in various drivers :
 * Implement support for overviews on subdatasets for HDF4, HDF5, NetCDF, NITF, XPM, TERRAGEN, PCIDSK (#2719)
 * Add support for 64bit offsets in /vsisubfile, JPEG and JP2ECW drivers
 * External overviews support added to some drivers (JDEM, ...)
 * Avoid incorrect GEOGCS AXIS settings (#2713)
 * Use GDALCheckDatasetDimensions() and GDALCheckBandCount() in various drivers
 * Many memory leak fixes (HDF5, HKV, Leveler, MFF, NITF, RMF, JPEG2000, WCS ... drivers)
 * Many fixes to improve robustness against corrupt data in many drivers
 * Error out when trying to open a read-only dataset in update mode (#3147)
 * Ensure that the same JPEG2000 driver that has been used with CreateCopy() is used to re-open it (#1596)

ADRG driver:
 * Support PSP != 100 (#3193)

AIG driver:
 * Differ opening of the RAT only when GetDefaultRat() is called.
   Will improve performances and make less likely the error message of #3031

AAIGRID driver:
 * Fix bad reading of AAIGRID dataset whose last line doesn't have a linebreak character (#3022)
 * Make the ArcInfo ASCII driver more Mac-compatible. (#3212)

BAG driver :
 * New for GDAL/OGR 1.7.0

BLX driver:
 * Implement progress callback in CreateCopy() (#2830)

BMP driver:
 * Modify GetGeoTransform() to return geotransform based on the resolution
   information in the BMP header if nothing else is available (ESRI merge).
 * use pam functions properly for geotransform when not using world file

BSB driver:
 * Fix several issues with BSB files reading (#2782)
 * Handle properly more than 256 GCPs (#2777)
 * Add logic to chip GCPs around to avoid split over dateline problems (#2809)
 * Add logic to reproject GCPs into Mercator for mercator projected images.

DTED driver:
 * Re-enable DTED driver to recognize files not starting by a UHL record (#2951)

ECW driver:
 * Enable the JP2ECW driver to open JP2 files with the VSILAPI
 * Fix build with MSVC2008  (#2850)
 * Fix memory overwrite while zooming an ECW image (#2934)
 * Speed-up de-registration of the driver that can take up to 3 seconds (#3134)

EHDR driver:
 * Add color table update.  Add limited support for floating point
   files (.flt) (ESRI Merge)
 * added support for SIGNEDBYTE pixels (#2717)
 * Restructure stats handling so pam capture works right when stx write fails
 * improve a bit .clr reading (#3253)

ENVI driver:
 * Move RPC info into the RPC domain (#3063)
 * Converted to using VSI Large File API (#3274)
 * re-enabled complex support (#3174)

EPSILON driver:
 * New for GDAL/OGR 1.7.0
 * Mainly used coupled with Rasterlite

ERS driver:
 * Added PIXELTYPE support to report/create SIGNEDBYTE (#2902)
 * Give precedence to PAM SRS, and even check for .aux file.  (ESRI Merge)

FAST driver:
 * Support 7 bands (#3307)

Fujibas driver:
 * Fix to work on big-endian hosts

GenBin driver:
 * Implemented custom class for 1bit (U1) files (#2730)
 * Fix inverse flattening computation (#2755).
 * Added U2 and U4 support
 * Look for LSB, assuming MSB if not found (#2730)

GeoRaster driver:
 * Fix compression problems
 * Add MaskBand support
 * Support UNICODE metadata
 * Support cross database schema/user access
 * add COORDLOCATOR create option

GeoTIFF driver :
 * Add 8/12bit jpeg in tiff support
 * Add support for creating external BigTIFF overview files,
   with BIGTIFF_OVERVIEW configuration option. (#2785)
 * Add support for deleting a color table (#2421)
 * Add logic for Imagine citation parsing
 * Add logic for encoding and reading ESRI PE string from citation.
 * Add support for reading and writing vertical datum info from geotiff
 * Changes to units handling.  (#2755)
 * Optimize opening speed by deferring fetching the coordinate system till GetProjectionRef (#2957)
 * Optimize GTiffRasterBand::IReadBlock() for multi-band interleaved case.
 * Avoid unnecessary re-writing the TIFF directory (#3021)
 * Use official value for inverse flattening of the WGS84 ellipsoid (#2787)
 * Add metadata domain for XML documents (#2786)
 * Make GTiff driver friendly with files with huge number of bands and pixel interleaving (#2838)
 * Avoid precaching other bands if block cache size is not big enough to accommodate them (#2838)
 * Internal libtiff (4.0.0beta5) and libgeotiff (1.3.0beta) upgraded
 * use the SetCitationToSRS call for the PCSCitationGeoKey in a similar fashion to the GTCitationGeoKey (#2933)
 * NBITS set for GTiffOddBits.  YCbCr JPEG added as a compression type.
   generate MINISWHITE IMAGESTRUCTURE metadata item.  Set missing blocks
   to the nodata value if there is one.  (ESRI Merge)
 * Support GeoTIFF with only ProjectedCSTypeGeoKey defined (#3019)
 * External overviews: try to preserve the bit depth of the original image
 * Allow reading and creation of big all-in-one-strip 8bit TIFF (#3094)
 * Handle projection methods for Google Mercator special case (#3217)

GFF driver :
 * Fix support for big endian host (#2832)
 * Add pam, and overview support.  Switch to VSI*L API to support virtualio (#3014)

GIF driver :
 * Introduced a BIGGIF driver to handle GIF files without using the
   slurp into memory approach. (#2542)
 * CreateCopy() reports now progress
 * Replace internal libungif by giflib-4.1.6 (#1825)
 * Read projection and georeferencing from OziExplorer .MAP file if possible.

GRASS driver:
 * Add support for GRASS 7.0 GDAL and OGR plugins (#2953)
 * Use GRASS_GISBASE for GDAL GRASS driver instead of hard-coded path (#2721)

GRIB driver:
 * only scan for PDS templates in GRIB2 files (#2858)
 * Avoid dumping GribLen debug message if built with -DDEBUG.
 * Remove verbosity from GRIB driver (#2887)
 * Make GRIB detection thread safe (#3209)
 * Check that bands have the same dimensions (#3246)

GS7BG driver:
  * Recognize version 2 datasets (#3123)

HDF4 driver :
 * Allow HDF4 subdataset name to include Windows letter drive (#2823)

HDF5 driver :
 * subdatsets need to be numbered from 1 not 0 (#2462)
 * Block size recognition. (#2270)
 * Fix initial value for nGCPCount in HDF5ImageDataset (#2774)
 * Fixes to type classification, and to avoid listing subdatsets for unsupported pixel data types (#2941)
 * Mark PAM for not saving .aux.xml files at this level.  Directly open
   subdatasets in HDF5Dataset::Open() if there is only one subdataset.

HFA driver:
 * Support reading and evaluating 3rd order xforms (#2716)
 * Various improvements to SRS support, particularly to preserve PE
   compatibility.  (#2755)
 * Added HFAEntry::RemoveAndDestroy() method to remove nodes from tree (#2421)
 * Added support for deleting color tables (#2421)
 * Add a scaled progress monitor for HFADataset::IBuildOverviews()
 * Fix HFA u2 compression/decompression (ESRI merge)
 * Add support for reading compressed s8 HFA (#3152)
 * Defer opening overviews till they are first requested (#3155)
 * Support multiple excluded values (#3252)
 * added a variety of additional coordinate system based types missing in some files (#3262)
 * Various fixes (#2421, #2842, #2755, #3082, #2730)

Idrisi driver:
 * Writing text file in CRLF format (#3199)
 * forward porting esri changes + other changes
 * provide default values on Create() (#3243)

INGR driver:
 * Fix INGR driver that was failing on big endian hosts (#2898)
 * Fix RLE support (#3106)
 * Added overview building (#2904)

JPEG driver:
 * Enable the JPEG driver to read and create 12bit JPEG images when JPEG12_ENABLED=yes
 * Internal libjpeg: additional changes to ensure operation with IPP enabled apps (#2606,#2845)
 * JPEG read optimizations : differ extraction of EXIF metadata and internal maskband

JPEG2000 (JasPer) Driver:
 * Allow proper reading of JP2 images where dimensions are not multiple of 256 (#2399)
 * Add a virtual I/O interface to able to read from/write to JPEG2000-NITF files
 * Do not deregister jas_image_clearfmts() to avoid failure when gdal_translat'ing from JP2 streams
 * Add proper reading of YCbCr images as RGB
 * fix decoding of bit depth for BPCC and PCLR boxes

JP2KAK (Kakadu) Driver :
 * Fix band selection from ycbcr to rgb converted images in DirectRasterIO (#2732)
 * Support jpc vsisubfile streams
 * add handling of reversibly compressed data with 9 to 16 bits precision (#2964)
 * Modify transfer_bytes() buf32 case to offset/scale based on precision. (#2964)
   Fixed _WriteTile() lossless 16bit case to avoid improper 32K offset.
   Added support for NBITS image structure metadata, and creation option.
 * Added logic to limit tiles to 64K due to jpeg2000 limitation. (ESRI Merge)
 * Fix offsetting of 16U buf32 data (#3027)
 * Support 16u/16s imagery through DirectRasterIO interface (#3049)
 * Support external overviews as an override to internal overviews
 * Rework jp2kak support to use natural kakadu builds (Windows build)
 * ensure external overviews get used if available (#3276)
 * add preliminary multi-threading read support via DirectRasterIO()


LAN driver:
 * Give preference to PAM coordinate system since built-in info is very
  limited.  Fallback to PAM geotransform if internal missing. (ESRI Merge)

LCP driver:
 * Add projection file support (#3255)

MEM driver:
 * Allow creating bands of more than 2GB each if size_t is large enough.
 * Added GetInternalHandle() implementation to fetch band memory buffer

MrSID driver:
 * Implement faster resampling for 1:1 case
 * Improve stream implementation so it works for jp2 with v7
 * Make the JP2MrSID driver accept .ntf extension to allow reading jpeg2000
   datastream in NITF files
 * Avoid reporting large metadata objects.  Add MG version to metadata.

NetCDF driver:
 * Fix handling of pixel validity mask (#3112)
 * correct a problem with 5+ dimensional data access (#2583)
 * fix y flip detection in common case (#2654)
 * add support for subdataset names with Windows full path names, like NETCDF:D:\...

NITF driver:
 * Add support for reading & creating large (>4GB) NITF files.
 * Add support for NITF/JPEG2000 overviews (JP2KAK)
 * Add support for reading & creating 12bit JPEG compressed NITF files when JPEG12_ENABLED=yes
 * Add support for creating a NITF file with many bands and big TRE content
 * Add support for creating several uncompressed images in a NITF file (#2989)
 * Add support for creating M3 (masked multi-block JPEG compressed) images
 * Add support for unpacking a wider variety of pixel depths.
 * Add support for overriding IGEOLO with GEOLOB TRE precision georef (#3180)
 * Add support for for CFloat32 reading & writing (#2526)
 * Add support for reading and writing NITF file with large single block (#3263)
 * Allow Jasper driver to be used for NITF IC=C8 (JPEG2000) CreateCopy() if JP2ECW is not available
 * Allow JP2MrSID driver to be used for reading JPEG2000 datastreams in NITF
 * Avoid issues when reading M3 single block images
 * Fix CreateCopy() of multi block JPEG-NITF
 * Various bugfixes (#2940, #2912, #3029, #3088)
 * Support NITF file with a color table and JPEG2000 data content (#3110)

NWT_GRC / NWG_GRD drivers (Northwood/VerticalMapper) :
 * New for GDAL/OGR 1.7.0

OGDI driver:
 * improve finding of PROJ.4 include files for OGDI (#1242)

PCIDSK driver (old driver):
 * Added worldfile reading. Added PAM fallback for geotransform.
 * Added support for default overviews (i.e. .ovr or .rrd).  (ESRI Merge)
 * fail somewhat gracefully on compressed images

PCIDSK driver (new driver):
 * New for GDAL/OGR 1.7.0, using the PCIDSK SDK

PDS driver:
 * Transfer various keywords to metadata
 * Made keyword handler more in complaint with ODL (#2956)
 * Support detached files with an offset (#3177)
 * Support .LBL labelled compressed files

PNG driver :
 * Upgrade internal libpng to 1.2.35
 * Only write a world file if the source datasource has a geotransform
 * Allow writing a nodata value of 0 as the transparent color value (#3208)

R driver:
 * New for GDAL/OGR 1.7.0

Rasterlite driver
 * New for GDAL/OGR 1.7.0

RIK driver:
 * Improved error checking

SAGA GIS Binary driver:
 * New for GDAL/OGR 1.7.0

SDE driver :
 * Fix exporting ArcSDE raster results in a displaced image (#2063)

SRP driver (ASRP/USRP):
 * New for GDAL/OGR 1.7.0

SRTM driver :
 * Set GDALMD_AOP_POINT metadataitem (#1884)

TIL driver (EarthWatch .TIL) driver:
 * New for GDAL/OGR 1.7.0

VRT driver :
 * Honour the INIT_DEST warp option (#2724)
 * Improve performance of LUTs in VRTComplexSource from O(n) to O(log2(n)) (#3003)
 * Implement (advertized in doc) support for SetMetadataItem( "source_0", szFilterSourceXML, "vrt_sources" ) on a VRTSourcedRasterBand (#3052)
 * Implement GetFileList() to list the source files
 * Fix wrong initialization of destination buffer in VRTSourcedRasterBand::IRasterIO() in case of not standard pixel/line spacing. (#2867)

WCS driver:
 * do not try to parse HTML content, which is returned by some provider when the server doesn't exist
 * added HttpAuth and UserPwd options for authentication (#3091)

WKT Raster driver:
 * New for GDAL/OGR 1.7.0

WMS driver:
 * Support TMS/formatted URLs in WMS minidriver (#2878)
 * Be tolerant if we have required 3 bands and got 4, or the other way round
 * Declare a user agent string

## OGR 1.7.0 - Overview of Changes

Utilities:
 * ogrinfo: Preserve order of source layers specified on ogrinfo command line,
            and use GetLayerByName() which enables to read some hidden layers
            like public.<table> layers from a PG database (#2922, #2026)
 * ogr2ogr:
    - Add -clipsrc and -clipdst option to clip geometries to the specified extents
    - Add -fieldTypeToString option to conveniently cast any fields of given type to fields of type string (#2968)
    - Add -progress option for ogr2ogr to displaying progress (#2998)
    - Add -wrapdateline option to ogr2ogr to deal with geometries that cross 180 degree longitude (#3158)
    - Add -dialect flag to specify SQL dialect
    - Preserve order of source layers specified on command line (#2922)
    - -overwrite and -append now automatically imply -update (#3048)
    - Support converting to a format after field name "laundering" (#3247)
  * ogrtindex:
    - Skip layers whose schema does not match instead of terminating (#3141)
    - Add a -accept_different_schemas option for non-MapServer use cases (#3141)
    - Set SRS to tileindex when one is found in the tiles
  * ogr2vrt.py : new script that will create a VRT corresponding to a
    source datasource (sample script for the moment, not promoted officially)

Core :
 * Improved OGR feature style (#2875, #2808)
 * Considerable speed-up of ExportToWkt() for very large geometries
 * Added new OGR_GEOM_AREA special field (#2949)
 * ensure forceToMultiLineString() works for MultiPolygons (#2735)
 * Various fixes in OGR SQL engine (r16116, #2996, #2788, #3143, #3144)
 * Add OGREnvelope::Intersect()
 * Add OGR_G_ApproximateArcAngles() for ellipses
 * Fix crash on Ubuntu 8.10 in GetFieldAsString() because of (too) strict guard logic (#2896)
 * add field type max so we can iterate through all possible values
 * Avoid making a 2D5 geometry from a 2D only linestring when reprojecting

OGRSpatialReference :
 * Upgrade EPSG derived files to EPSG 7.1
 * Added support to operate on COMPD_CS coordinate systems
 * Added support for importing spatial reference definitions from the OziExplorer .MAP files.
 * Introduce static methods to destroy OGRSpatialReference and OGRCoordinateTransformation objects
 * Expose more of the axis orientation API to C
 * Add missing Eckert 1, 2, 3 and 5 projections
 * Fix typos in proj4 conversion for Wagner projections
 * Hack in EXTENSION nodes for Google Mercator (#3136)
 * Validates PROJCS with AXIS definitions (#2739)
 * Added support for urn:ogc:def:crs:OGC::CRS:84 (and CRS:83, CRS:27) per WMS spec.
 * Wide variety of improvements to preserve PE strings through a morphFromESRI()
   and morphToESRI() process (#2755)
 * Fix inversion of dictionary filename and GEOGCS/PROJCS name in OGRSpatialReference::exportToERM() (#2819)
 * Fix SpatialReference::IsSame() for LOCAL_CS case (#2849)
 * Fix bug in ImportFromXML that prevented from retrieving projection method
 * Accept both href and xlink:href in OGC XML
 * improve us foot translation handling (#2901)
 * OGRSpatialReference::importFromUrl() : add a default 10 second timeout to avoid waiting forever when remote server is stalled
 * ensure we can translate mercator1sp with non-zero origin to proj4 (#3026)
 * ensure scalefactor preserved in somerc translation (#3032)
 * SRS_ESRI: attempt to correct equidistant cylindrical parameter morph (#3036)
 * SRS_ESRI: improve plate_carree parameter morphing (#3036)
 * SRS_PCI : Fix PCI projection string handling for UTM
 * esri_extra.wkt: correct equidistant conic definitions (#3086)
 * SRS_PANORAMA : Added support for British National Grid and Pulkovo 1995 datums.
 * Improve recognition of WKT text strings when translating into proj4 hard-coded datum names, in particular nzgd49
   (also add ggrs87, carthage, hermannskogel, ire65); Fix ellipsoid parameters for modified airy (#3104)
 * OSRGetEllipsoidInfo() available for various OGR SRS modules
 * added support for OGC:CRS84 and similar srses from wms/wcs spec in SetFromUserInput (#3090)

BNA driver :
 * Fix output of BNA driver with polygons with inner ring (#2985)
 * Proper CRLF output on Windows (#3256)

CSV driver :
 * Add support for reading and writing CSV files with semicolon or tabulation
   as the field separator (#2925, #2136)
 * Add automatic treatment of WKT column as geometry
 * Add 'CREATE_CSVT' layer creation option

DXF driver :
 * New for GDAL/OGR 1.7.0

Geoconcept driver:
 * Fix 'private Class and SubClass headers are inverted' (#2919)
 * Fix error in writing 2.5D/3D export file (#2921)

GeoJSON driver:
 * updated JSON-C library to version 0.9
 * use VSIF*L API

GML driver :
 * Speed-up considerably parsing of GML coordinates in big geometries, in particular on Windows
 * Add support for gml3.1.1 srsDimension attribute, to deal with 3D geometries (#2311)
 * Support multiple <gml:pos> elements in linearrings of polygons (#3244)
 * Limited support for GML3
 * Support direct use of FIDs as long as they are all numeric or they have a completely fixed prefix (#1017)
 * Fix OGRGMLLayer::GetFeatureCount() if there's a .XSD file available (#2969)
 * Added support for out-of-band attributes on features (for NAS)
 * Adding the date field type to xsd writer and precision info for OFTReal fields. (#2857)

GPX driver:
 * Add GPX_SHORT_NAMES configuration option to make the GPX driver report shorter field names
   and avoid duplicated field names once translated to shapefile (#2966)
 * Write the <bounds> element (write only)
 * Avoid escaping XML content when writing <extensions>.
 * Add appropriate xmlns when detecting Garmin GPX extensions

GRASS driver:
 * Do not report 3D geometries for 2D GRASS maps (#3009)

GTM (GPSTrackMaker) driver :
 * New for GDAL/OGR 1.7.0 (#3113)

ILI driver :
 * Improved curve segmentation algorithm
 * ILI1: Support for multiple point geomtries
 * ILI1: Support Real and Integer column types

Ingres driver:
 * Utilize the new OGC based ingres capabilities (#3159)

KML driver:
 * Support reading MultiGeometry and layers with mixed geometry type.
 * Speed-up considerably the reading of huge KML geometries (#3005)
 * Speed-up considerably with huge number of layers
 * Moved the location of the Style element to match the OGC Schema (#2858)
 * Advertise 25D geometry type when relevant (#1803, #1853, #2181)
 * Relax KML driver about xmlns (#3004)

MySQL driver :
 * Fix mysql driver compilation with mysql 5.1/g++-4.3.2 (Mandriva 2009.1) (#2972)
 * Fixed bug MySQL driver truncating decimal places for double field type. (#2852)

OCI driver :
 * OCI varchar2 columns can be up to 4000 bytes (#2876)

ODBC driver :
 * make it slightly less likely that the srs_tablename parsing will interfere with complex DSNs.
 * support for schemas (#1969)

OGDI driver :
  * fix to avoid applying old spatial filter to unrelated layer
  * fix to force ResetReading() when changing current layer

PGEO driver :
 * Recognize more ESRI shape type constants. (#2991, #3100)
 * implement GetFIDColumn() and GetGeometryColumn() methods (#2694)

PostgreSQL driver:
 * Add support for tables with 'geography' column type introduced in PostGIS 1.5 (#3216)
 * Extend support of schemas in PG driver with 2 new options in the connection string:
   active_schema=foo and schemas=foo[,bar] (#522 and #525)
 * Implement OGRPGTableLayer::CreateFeature() by using UPDATE instead of DELETE + INSERT (#2557)
 * Implement SetNextByIndex() for layers of PG datasources (#3117)
 * Support PG 'real' data type in tables (#3006)
 * Speed-up PG database opening by avoiding 2 SQL requests per table
 * Avoid evaluating GetFieldIndex() on each field of each feature,
   which can be very expensive if the layer has many fields
 * allow ST_AsBinary with non binary connections
 * added a configuration option PG_SKIP_VIEWS.

GeoRSS driver :
 * New for GDAL/OGR 1.7.0 (#2726)

OCI driver:
 * support blob column binding

PCIDSK driver :
 * New for GDAL/OGR 1.7.0

Shape driver :
 * Handle duplicate field names in shapefile driver. (#3247)
 * Support for opening and handling .DBF files > 2 GB (#3011)
 * Optimize to use shape bounds for spatial test before organizing poly (#2775)
 * Support for alternate date format (#2746)
 * Improve/fix TestCapability() on OGRShapeLayer
 * Refreshed shapelib from upstream

S57 driver :
 * Fix incorrect return value of GetFeatureCount() on S57 SOUNDG layer when SPLIT_MULTIPOINT=ON;
   also avoid warning on that layer when ADD_SOUNDG_DEPTH=OFF (#3163)

SQLite driver:
 * Add creation and write support in SpatiaLite-compatible databases
 * Add SPATIAL_INDEX creation option for SpatiaLite tables if linked against
   libspatialite (default to YES)
 * Implement OGRSQLiteTableLayer::TestCapability(OLCFastFeatureCount)
 * Implement OGRSQLiteLayer::GetFIDColumn() and GetGeometryColumn()
 * Implement TestCapability(OLCRandomRead)
 * Add a SQLITE_LIST_ALL_TABLES configuration option to list all(non-spatial)
   tables into a SQLite DB even if there are spatial tables
 * Avoid reporting the primary key column as a regular column.
 * Better precision for double values in CreateFeature()

VFK driver:
  * New for GDAL/OGR 1.7.0

VRT driver:
 * Allow fast spatial filtering in the VGS_Direct case
 * Add support for CreateFeature(), SetFeature() and DeleteFeature() operations
 * Added field definition and style control
 * Added new vrt/schema creation capability (@dummy@ datasource, ogr2vrt.py script)
 * Implement 'SrcRegion' element
 * Add a 'reportSrcColumn' attribute to the 'GeometryField' to avoid reporting
   the x,y,wkt or wkb source fields in the VRT layer field definition
 * Forward TestCapability(), GetExtent(), SetNextByIndex() to source layer when possible

XPlane/Flightgear driver:
 * Improve handling of Bezier curves (#3030)
 * Support new file names used by XPlane 9.00 & later
 * Cut into 2 pieces airway segments that cross the antemeridian
 * Add new layer 'Stopway' that contains the shape of the stopway/blastpad/over-run of a runway
 * Recognize code 16 and 17 for seaplane bases and heliports and add a new field to APT layer

## SWIG Language Bindings

General:
 * Recommended SWIG version is 1.3.39
 * Added API :
    - GDAL :
        gdal.FilldoData(), gdal.FileFromMemBuffer(), gdal.Unlink()
        gdal.ApplyGeoTransform(), gdal.InvGeoTransform()
        Band.GetUnitType(), Band.GetBand()
        Band.ComputeStatistics(), Band.HasArbitraryOverviews()
        RasterAttributeTable.GetLinearBinning()  and SetLinearBinning()
        extend [Band|Dataset].[ReadRaster|WriteRaster] to accept pixel, line and band spacing parameters
    - OGR:
        ogr.GeneralCmdLineProcessor(), Geometry.Segmentize(), FieldDefn.GetTypeName(),
        Geometry.ApproximateArcAngles()
    - OSR :
        osr.ImportFromMICoordSys(), osr.ExportToMICoordSys(), SpatialReference.Clone()
        osr.EPSGTreatsAsLatLong(), osr.ImportFromEPSGA()
 * Make resampling an optional parameter for gdal.RegenerateOverview(),
   to be consistent with gdal.RegenerateOverviews()
 * NONNULL checks have been added to check various arguments of methods
 * add missing constants : DCAP_VIRTUALIO, color interpretations, OGR constants

CSharp bindings :
 * Add support for GetFieldAsStringList, GetFieldAsDoubleList and
   GetFieldAsIntegerList in the C# wrapper (#2839)
 * Support MSVC2008 builds with the csharp interface (#2862)
 * Change the dll mapping rules to support the recent MONO versions
 * Use GC pinned arrays instead of the double copy in the RasterIO functions (#3073)
 * Add typemaps to support custom CPLErrorHandler via C# swig bindings

Perl bindings :
 * in Polygonize make a local copy of the parameters as they are potentially edited

Python bindings :
 * Add support for Python 3.X. Compatibility with Python 2.X preserved (#3265)
 * Remove old-generation Python bindings.
 * Add Python binding's version description capabilities (#3137)
 * NUMPY : Make Band|Dataset.ReadAsArray() take into account preallocated array (#2658, #3028)
 * Various memory leaks fixed
 * Fix gdal.RegenerateOverviews(), Feature.GetFieldAsStringList(),
   Feature.GetFieldAsIntegerList(), Feature.GetFieldAsDoubleList(),
   Transform.TransformPoints and CoordinateTransformation.TransformPoints
 * Extend python TransformPoints typemap to accept any sequence (#3020)
 * Make Geometry iterable (#1886)

Java bindings (general changes):
 * Maintained again. A lot of changes to get them into clean state.
 * RasterIO API : added API for standard Java arrays in addition to DirectByteBuffer
 * Javadoc available at http://gdal.org/java

# GDAL/OGR 1.6.0

## GDAL/OGR 1.6.0 - General Changes

Build (Unix):
 * Added basic support for LDFLAGS
 * Try prefix/lib before prefix/src for proj.4 linking (#1345)
 * Allow specification of a python binary for --with-python (#2258)
 * Added NAS driver config support
 * Fixed Expat detection problem on MinGW (#2050)
 * Fix INST_DATA setting (/share/gdal instead of /share) (#2382)
 * Build MSGN driver on Unix-like platforms
 * Added MSG driver support to configure.in. EUMETSAT Wavelet Transform software is only detected on request, option --with-msg specified.
 * Improve cross-compilation
 * Fix linking with HDF4 library when configuring with --with-hdf4 or --with-hdf4=yes (#2602)
 * Fixes for compilation with GCC 4.3

Build (Windows)
 * Default to non-debug builds.  Use /GR in pre 1400 builds so that dynamic_cast doesn't just cause an blowout.  Use .pdb file with version embedded.
 * Make vc++ builds usable from mingw (#2216)
 * Updated nmake.opt for building with Visual C++ adding /W3 flag for release build and /W4 for debug build.
 * Add VS Makefile for GDAL and OGR DODS drivers (#2383)

Build (All)
 * Remove support for "Panorama" GIS SDK (#2669)

Port:
 * RFC 19: Added VSIMalloc2() and VSIMalloc3() API and use them in GDAL drivers
 * RFC 23: Added implementation of recode API
 * Added infrastructure to detect bad arguments to printf-like functions
 * Added CPLHashSet data structure
 * Added quad tree implementation derived from shapelib & mapserv
 * Added support for reading on-the-fly .gz files and .zip files (#1369)
 * Added CSLFindName()
 * Added two new flags to CSLTokenizeString2() function: CSLT_STRIPLEADSPACES and CSLT_STRIPENDSPACES to strip leading and ending spaces from the token.
 * Added CSVGetNextLine() to fetch next record based on just csv filename
 * Added CPL_ACCUM_ERROR_MSG=ON for CPLQuietErrorHandler
 * Added CPL_MAX_ERROR_REPORTS config option (#2409).
 * Added CPL_INLINE macro
 * Added UNREFERENCED_PARAM macro to cpl_port.h.
 * Added CPLGenerateTempFilename()
 * Improve performance of CPLParseXMLString from O(n*n) to O(n) where n is the number of siblings node
 * Fix bug with url encoding in CPLEscapeString() (#2314)
 * Various fixes in CPLList implementation (CPLListInsert and CPLListRemove) (#2134)
 * VSIMEM: added path normalization so everything is converted to forward slashes
 * VSIMEM: prevent file extension, or write/update to files opened in readonly mode
 * cpl_path.cpp: Add CPLAssert to check that the string inputs are not the result static buffer

Utilities:
 * Added a --utility_version that displays the version of GDAL used for compiling the utility and add runtime checks to see if GDAL library is compatible with the utility version

## GDAL 1.6.0 - Overview of Changes

Core :
 * RFC 22 : Added RPC and IMD support
 * Added support for computing statistics for datasets with arbitrary overviews in GDALRasterBand::ComputeStatistics()
 * Added Gaussian resampling in overview building (#2137)
 * Added Mode resampling in overview building (#2347)
 * Allow fast NONE overview generation (#2677)
 * Added in GDALRasterBand::GetRasterSampleOverview() and ComputeRasterMinMax() (#2148)
 * Preliminary gmljp2 specific changes to address axis orientation (#2131)
 * Added GDALProxyDataset and GDALProxyRasterBand abstract classes to be able to make proxy datasets and raster bands
 * Added a proxy dataset class, GDALProxyPoolDataset, that differ at the maximum the opening of the underlying dataset and keep the number of simultaneous opened underlying datasets under a limit (similar to what is done in the RPFTOC driver)
 * Migrate GDALRegenerateOverviews() to C API
 * Added GDALDestroyDriver()
 * Added special case in GDALCopyWholeRaster to be more friendly when writing a tiled compressed GeoTIFF (massive reduction of destination file size)
 * Added GDALRegenerateOverviewsMultiBand to process all the bands at the same time to optimize the generations of compressed pixel-interleaved overviews (such as JPEG-In-TIFF). Optimization triggered in some cases for external and internal GeoTIFF overviews. PHOTOMETRIC_OVERVIEW and INTERLEAVE_OVERVIEW config options added for external overviews. -ro option added to gdaladdo to generate external overviews for GeoTIFF. Result : divide by 2 to 3 the size of JPEG-In-TIFF overviews on big RGB datasets
 * Add a new class GDALNoDataValuesMaskBand to create a per-dataset nodata mask band (GMF_PER_DATASET | GMF_NODATA) when the metadata item NODATA_VALUES is found on the dataset (#2149)

 * Fix segfault when building overviews with --config USE_RRD YES (#2145)
 * PAM: save floating point nodata in IEEE floating point binary format
 * Fix division by zero in GDALGetRandomRasterSample (#2429)
 * GDALOpen: Use EOVERFLOW if defined otherwise use hardcoded likely values (#2437)
 * Replace implementation of arrays for maintaining the list of opened shared datasets by a CPLHashSet to avoid O(n*n) complexity
 * Fix GDALRasterBand::IRasterIO fails to read data from dataset when block cache is too small (#2457)
 * Modify GDALFindAssociatedAuxFile() to only select .aux files that have the same raster configuration as the target dataset (PxLxB).  (#2471).
 * When available use arbitrary overviews for computations in GDALRasterBand::ComputeRasterMinMax() and GDALRasterBand::GetHistogram().
 * Fix crash in GDALValidateCreationOptions when passed a creation option not in format key=value (#2499)
 * Fix 'GDALNoDataMaskBand::IReadBlock doesn't behave correctly when eWrkDT != eDataType' (#2504)
 * Use nodata masks when generating overviews (#2149)
 * Improve error propagation when GDALRasterBlock::Write() fails (#2524)
 * gdalnodatamaskband: add case for uint16 and uint32 as uint32 - fixes neg. nodata values for these
 * Add a special case for dealing with RasterIO expansion on writes (#773)
 * Add GDALValidateCreationOptions() checks in GDALDriver::Create() and GDALDriver::CreateCopy(). Can be disabled with GDAL_VALIDATE_CREATION_OPTIONS=NO
 * Optimization of GDALCopyWords for transfer from buffer of packed bytes to buffer of bytes with interleaving (#2536)
 * Use BlockBasedRasterIO in GDALDataset::IRasterIO for pixel-interleaved datasets when source and destination have the same size (#2536)
 * PAM: Allow empty category in .aux.xml  (#2562)
 * GDALDataTypeUnion(): Add missing GDT_CInt16 case that was triggering a CPLAssert(FALSE) (linked to #2564)
 * PAM: improve the find existing histogram logic to check approx and out of range
 * Fix validation of values for creation option parameters of type float
 * Fix memory leak related to PAM histograms
 * Restrict dataset sharing to a one thread by tracking owning pid (#2229)
 * rasterio.cpp: Handle >2GB memory arrays on a 64-bit build (#2199)

Algorithms:
 * Added GDALPolygonize() function
 * Added sieve filter
 * Add implementation of raster hole filler
 * Added proximity algorithm implementation
 * Added GDALRasterizeLayers() function to rasterize the all the features in the list of layers rather than individual geometries.
 * Added support for point geometries rasterization
 * Added line rasterization routine.
 * Added GDALCreateGenImgProjTransformer2()

 * warper: massive upgrade that fixes number of problems with Cubic Spline and Lanczos resamplers, multiple performance improvements.
 * Implement overview building for paletted rasterbands with average resampling by selecting the nearest entry after averaging on R,G,B components (#2408)
 * Fix destination coordinate system setting logic in GDALReprojectImage (#2231)
 * Modify GDALChecksum to give it a deterministic behavior when given a GDT_[C]Float[32|64] rasterband with NaN and Inf values. The result is backward compatible with previous implementations with finite values
 * Add options to RPC transformer, use for RPC_HEIGHT offset
 * TPS : fix uninitialized variables (#2300), fix wrong behavior with negative coordinates  (#2615)
 * gdalgeoloc.cpp : Fix crash in GDALCreateGeoLocTransformer if X_DATASET, etc... cannot be opened (#2434)
 * warper: Prevent crashes when srcAlphaBand and dstAlphaBand are wrong
 * Fix tiling in gdal_grid and output of geotransform when -txe and -tye not specified (#2508)
 * warper: Wait for the threads to complete before leaving GDALWarpOperation::ChunkAndWarpMulti() (#2518)
 * warper: When warping an RGBA image whose borders have alpha=0, avoid writing alpha=255 with bilinear, cubic, cubic spline resampling
 * warper: Properly set a resample window size for Cubic Spline kernel. (#2414)
 * gdalsimplewarp.cpp: fix pointer array allocation (#2586)

Utilities:
 * gdal_lut.py: New for 1.6.0. Sample app applying a greyscale lookup table
 * gdal_polygonize.py : New for 1.6.0
 * gdal_proximity.py : New for 1.6.0
 * gdal_sieve.py: New for 1.6.0
 * densify.py: New for 1.6.0. A generic Translator class for ogr2ogr-like operations that can be easily overridden for special translation operations
 * gdalflattenmask : New utility to merge regular data bands with the mask bands, for applications not being able to use the mask band concept. (Not compiled by default)
 * gdal2ogr: New for 1.6.0. to create an OGR datasource from the values of a GDAL dataset; May be useful to test gdal_grid and generate its input OGR file (Not compiled by default)

 * Fix crash in gdalenhance
 * Add -nln switch for gdal_contour
 * gdalgrid: Fixed zero search radius handling in nearest neighbor interpolation method.
 * gdalgrid: Added support for data metrics (minimum, maximum and range) computation.
 * gdalgrid: Added spatial filter support applied on the OGR data source
 * gdalgrid: Added ability to read values from the attribute field using the "-zfield" option.
 * gdalgrid: fix crash on features with NULL geometry or when no point geometry is found on a layer (#2424)
 * esri2wkt.py: Fix esri2wkt for NG python bindings (#2548)
 * Build testepsg utility by default when OGR is enabled (Ticket #2554).
 * gdaltranslate: new -expand rgb|rgba option to make color table expansion
 * gdaltindex: Use correct index variable name
 * gdal2tiles.py: Bug fix: switched axes in BoundingBox of tilemapresource.xml.
 * gdal2tiles.py: Bug fix: wrong Origin in tilemapresource.xml.
 * gdal2tiles.py: New version of GDAL2Tiles (SoC 2008 - GDAL2Tiles Improvements)
 * gdal_retile.py: Some minor enhancement optionally storing the  georeferencing data of created tiles in a csv file
 * gdal_vrtmerge.py: add support for NODATA
 * gdalinfo: Don't show RAT if -nomdd is used.
 * gdalinfo: Display checksums on overviews when -checksum is specified
 * gdalinfo: Display whether the mask band has overviews
 * ogr2ogr: reset -gt to 1 with -skipfailures, document -gt (#2409)
 * ogr2ogr: Output error messages on stderr to be consistent; Make error message about failed reprojection more clearer (hopefully); Advertise the use of -skipfailures in error message (#2588)
 * nearblack: Add support for scanning from top and bottom as well as from the sides.
 * Prevent crash in gdalwarpsimple utility and in GDALSimpleImageWarp() when source dataset has no raster band
 * gdal_rasterize: check that coordinates systems match (Ticket #1937)
 * gdalwarp: Add cutline support

Various drivers:
 * Reports GDAL_DCAP_VIRTUALIO=YES for drivers supporting it (#2193)
 * Add warnings in CreateCopy() methods for drivers not supporting color tables (#1939)
 * Simplify and harmonize how ESRI world file extensions are handled for BMP, GIF, JPEG, PNG and GTiff drivers (fix #1734)
 * Windows : enable bigtiff by default when using internal libtiff (#2257)
 * Added plugin building support for ECW, MrSID, HDF5, NetCDF, FITS and SDE drivers.
 * fix case of capabilities xml elements to match properly (#2322)
 * Add ALPHA creation option in the list of creation options

AAIGrid Driver:
 * Re-fix nodata test when determining AAIGrid data type (Ticket #2107).
 * fix yllcorner computation (#1794)
 * mark driver as supporting virtualio.
 * Fix wrong data type used to read the source band AAIGCreateCopy (#2369)
 * Add DECIMAL_PRECISION creation option for AAIGRID (#2430)

ADRG driver:
 * Initialize PAM to avoid creating .aux.xml file when gdalinfo an ADRG dataset
 * Prevent opening in update mode && fix reading of blocks in creation mode
 * Prevent error message coming from ADRG driver when trying to open in update mode a non-existing dataset
 * Avoid error reporting and subsequent failure on some DIGEST files where the last record ends in a non-standard way

BLX Magellan Topo driver:
 * New for 1.6.0 (#2254 and #2570)

BSB driver:
 * Add support for alternate palettes via config option
 * Fix Valgrind warning about read of uninitialized memory; Replace a CPLAssert by a test in case of corrupted dataset; Remove static buffer in BSBReadHeaderLine to improve thread-safety
 * Update BSB write support to use VSIF*L API (still disabled by default); fix palette handling (last color entry was lost); add GCP/Geotransform writing

COSAR driver:
 * Initialize integral variables to prevent failure of test condition in case file read operation fails; clean signed/unsigned mismatch warning.
 * Additional tests to protect against corrupted datasets

DIMAP driver:
 * Fix use of static CPL buffers
 * Implement GetFileList() and support for opening directory for DIMAP format
 * Add metadata at the raster band level by reading the Spectral_Band_Info tag

DODS driver:
 * Add using namespace libdap for version 3.8.2 (#2404)

DTED driver:
 * Add the GDAL_DTED_SINGLE_BLOCK config option to make a single block of a whole DTED file. This can speed-up a lot scanline oriented algorithms (#1909)
 * Add support for DTED products strictly following MIL-D-89020 that was buggy
 * Fix compilation without CPL
 * Improve thread safety

ECW driver:
 * Fix memory leaks (#2210)
 * Be more careful deciding what geotransforms to ignore (#1935)
 * Rename ecw plugin gdal_ECW_JP2ECW.so/dll (#2320)
 * Properly set default nPixelSpace and nLineSpace in ECWRasterBand::IRasterIO().
 * Added pixel data type checking in ECWCreateCopy() (#2593).

EHDR driver:
 * Port EHDR driver to large file API (by apetkov) (#2580)

EIR (Erdas Imagine Raw format) driver:
 * New for 1.6.0

ENVI driver:
 * fix problems with old/new state plane zone codes (#2227)

ERS driver:
 * add support for HeaderOffset keyword (#2598)

FAST driver:
 * Order the GCP in TL, TR, BR, BL order to benefit from the GDALGCPsToGeoTransform optimization
 * Add support for Euromap FAST datasets for IRS-1C/IRS-1D PAN/LISS3/WIFS (#2329)
 * Fix USGS projection decoding for EOSAT FAST format Rev C (#2381)
 * Add support for detection of FAST IRS P6 and CARTOSAT-1 band filenames

FITS driver:
 * Fix crash in FITS when dataset has metadata; Fix 2 minor Valgrind warnings (#2396)
 * Fix crash on int32 test case in fits.py for 64-bit GDAL build (#2579)

GeoRaster driver:
 * New for 1.6.0

GFF driver:
 * Close file pointer in dataset destructor

GIF driver:
 * Disable opening of large files which hang GDALOpen() (#2542)

GRASS driver:
 * Fix obvious memory leaks in GRASS driver (#2537)
 * fix to use G_free() instead of free() (#1983)

GRIB driver:
 * Moved from spike to trunk: grib now standard, but support --without-grib option

GSAG driver:
 * Prevent crash on huge number of rows
 * Prevent Valgrind warnings on bogus short GSAG files
 * Major update to correct upside problems, removing Create (#2224, #1616, #2191)

GS7BG driver:
 * Fixed geotransformation matrix calculation (#2132)
 * Properly read the header on big-endian system.
 * Fix bigendian support (#2172)

GTiff driver:
 * Add support for reading and writing embedded nodata masks of a TIFF file (TIFFTAG_SUBFILETYPE=FILETYPE_MASK)
 * Added SUBDATASETS support
 * Add the ability to create files with PHOTOMETRIC=PALETTE.
 * Add the ability to update palette on existing files (#2421)
 * Enforce PROFILE properly for Create (#1527)
 * Add support for reading a CMYK TIFF. By default, it will be opened with the RGBA interface (CMKY->RGBA translation done by libtiff. Said to be *very* crude), unless 'GTIFF_RAW:' is specified before the filename. In that later case, the CMYK bands will be presented. Also add support for translating to a CMYK TIFF too : the source dataset must have CMYK bands. No colorspace translation is done
 * Internal libtiff : refresh from upstream libtiff
 * Added GTiffSplitBitmapBand to treat one row 1bit files as scanline blocks (#2622)
 * Don't use GCS if it is less than 1 (#2183).
 * Modified so that the RGBA interface is not used for YCbCr mode JPEG compressed data.  Set JPEGCOLORMODE to RGB at the point a directory is read to avoid error report if doing it later during writing.  This fixes the GDAL 1.5.0 issues with writing YCbCr JPEG compressed data (#2189).
 * Fix memory leak in gt_wkt_srs.cpp
 * Prevent crash in GTiff driver in case we cannot GDALOpen the newly create-copied file
 * Fix buffer overflow when calling GTIFDirectoryInfo in GTIFGetOGISDefn (#2372)
 * add special handling for 24bit data which gets byteswapped by libtiff (#2361)
 * Replace hard-coded 3 byte increment by iPixelByteSkip in int24 gtiff decoding (#2361)
 * Cleaunup frmt/gtiff directory by removing unused files. Move TIFF_WriteOverview to gt_overview.cpp. Create gt_overview.h to declare TIFF_WriteOverview and GTIFFBuildOverviewMetadata
 * Add a ENDIANNESS creation option to GTiff driver for debug purpose mostly
 * Fix writing of blocks on TIFF files in non-native endianness (#2398)
 * Push extra bands of pixel interleaved data into block cache (#2435)
 * Improve integration with PAM metadata loading and saving (#2448)
 * Fix potential buffer overflow in GTIFAngleStringToDD (committed in upstream libgeotiff) - #2228
 * Fix GTiffOddBitsBand::IWriteBlock with GDT_UInt16/32; Error properly with GDT_Float32; Support creating files with NBITS>8; Handle NBITS=1 IReadBlock/IWriteBlock in GTiffOddBitsBand; Prevent subtle IReadBlock/IWriteBlock round-tripping bug for NBITS<8 (#2360)
 * Set the TIFFTAG_COMPRESSION compression before asking the default strip size, so that in the case of JPEG compression, the correct strip height is selected (either 8 or 16). Tested with libtiff-3.8.2 and internal libtiff
 * Prevent crash on tiff_ovr_9 when JPEG-In-TIFF support is not built
 * In GTiffDataset::Create(), set TIFFTAG_JPEGCOLORMODE=JPEGCOLORMODE_RGB when creating a TIFF with COMPRESS=JPEG and PHOTOMETRIC=YCBCR; In Crystalize(), backup the value of TIFFTAG_JPEGCOLORMODE and set it again after writing the directory (#2645)
 * Handle more gracefully the case where we open or create a TIFF file with a compression method not built in libtiff (use of TIFFIsCODECConfigured)
 * Don't fail when TIFFTAG_JPEGCOLORMODE tag cannot be read
 * IPP libjpeg compatibility changes (#2606)
 * ensure zip/jpeg quality is preserved in crystalize. (#2642)
 * support handling nodata via pam/aux mechanisms (#2505)
 * ensure TIFFFlush() is called in FlushCache() (#2512)
 * Replace Crystalize() by SetDirectory() in GTiffDataset::IBuildOverviews() so that 2 consecutive ds.BuildOverviews() calls work without needing to close and reopen the dataset in between
 * Prevent crash when disk is full
 * Add detection of big-endian bigtiffs when BIGTIFF_SUPPORT is *NOT* defined
 * Add missing ScaleAtCenter parameter for Hotine Oblique Mercator when writing the geotiff keys
 * Added logic to pre-clear geotiff tags when updating a file (#2546)
 * Add ExtraSample tag in overviews too (#2572)
 * Fix handling of non-degree angular units (#601)

GXF driver:
 * Add GXFRasterBand::GetNoDataValue (fix #835)
 * Avoid crash on bogus GXF file

HDF4 driver:
 * add support for projected NRL products (#2225)
 * make a block consist of several scanlines for SDS case to speed up (#2208)
 * Add H4ST prefix to names of HDF4SubdatasetType enumeration values. (#2296).
 * Remove useless and dangerous redefinition of sincos in HDF-EOS (#2494)
 * Added compatibility definitions for HDF 4.2 library (#2609)
 * Read HDF raster images containing in HDF-EOS datasets (#2656)

HDF5 driver:
 * Fix minor memory leaks and one incorrect memory usage in HDF5
 * implement support for 1.8+ hdf library versions (#2297)

HFA driver:
 * Avoid possible uninitialized variable usage in HFAWriteXFormStack()
 * Fix BASEDATA count value (preceding pointer) at 1 (#2144)
 * Incorporate generalization of EPT_f32 reduced precision handling (#1000)
 * Add missing creation options, fix doc to refer to COMPRESSED instead of COMPRESS (#2167)
 * remove static buffer to improve thread-safety
 * Read invalid blocks as nodata value if available.  Create new files with all blocks marked invalid.  Support writing to invalid blocks as long as there is already a pointer to valid data.  (#2427)
 * add support for writing 1, 2 and 4 bit data (#2436)
 * Attempt to preserve PROJCS name in sMapInfo.proName, and to capture it as the PROJCS name when reading.  This will hopefully preserve symbolic names like NAD_1983_StatePlane_Ohio_South_FIPS_3402_Feet instead of replacing them with something generic like "Lambert_Conformal_Conic" (#2422).
 * avoid reducing array sizes if writing them in random order (#2427)
 * Prevent writing out cached information after the file has been closed.  Loosely related to (#2524).
 * Error out gracefully and early on attempts to write to readonly file (#2524)
 * Open the dependent file(s) with same permissions as master (#2425)
 * Fix crash in HFACompress::compressBlock when compressing random data with m_nDataTypeNumBits >= 16 (#2525)
 * Fix reading of a non-initialized compressed HFA file (#2523)
 * Add FORCETOPESTRING, and ensure ProjectionX applied to all bands (#2243)
 * Added support for unique values color tables (#2419)

HTTP driver:
 * Fix HTTP driver when falling back to /tmp (#2363)

IDRISI driver:
 * Force min/max calculation on IWriteBlock
 * remove conditional from CreateColorRamp() call
 * Fix #2444 (lat/long) and #2442 (uppercase file extension)

ILWIS driver:
 * Modified to use VSI*L API for reading and writing.  Modify ReadBlock() so that data written on newly created datasets can still be read back.
 * Fix memory leaks in ILWIS driver
 * Avoid writing an ILWIS file to disk when it is a src_dataset. Design of responsible class (IniFile) is simplified, to prevent this from happening unintentionally.
 * Spend extra effort to find the most compact GDAL data-type for storing the ILWIS data; Added missing ILWIS-system domains to the list; Initialized variables before they are used; Added comments to code.
 * Solved unwanted rounding in the pixel size, that resulted in wrong map size calculation.

INGR driver:
 * Support splitting bitonal images into scanline blocks too (#1959)
 * Fix compilation of INGR driver on big-endian target (#2613)

ISIS3 driver:
 * fix earth model, already in meters, not kilometers! (#2321)
 * ensure we adjust first tile offset depending on band (#2573)

ISO8211 driver:
 * corrections to handle double byte attributes better (#1526)
 * add a -xml option to 8211dump utility; add a 8211createfromxml utility to generate a ISO8211 file from the output of 8211dump -xml
 * robustness fixes

JP2KAK driver:
 * Add VSI*L reading and writing (vsil_target) (#2255)
 * Remove KAKADU4 related ifdefs, we now assume at least KAKADU 4.2.
 * disable JPIP - not working with modern Kakadu

JPEG driver:
 * Added support for reading georeferencing from .tab files. Fixes #682.
 * Add support for reading images in CMYK and YCbCrK color spaces (#2443)
 * make sure bHasDoneJpegStartDecompress is set in Reset() (#2535)
 * Added fill/flush support compatible with IPP libjpeg (#2606)

LCP (FARSITE) driver:
 * New for 1.6.0

L1B driver:
 * Added support for NOAA-18(N) and METOP-2 datasets; tiny code refactoring.
 * L1B : add auto guess of data format when it is 2 spaces or empty string
 * The GAC GCPs are not tied to the center of pixel.
 * Serious code rewriting in order to read datasets without archive header

MEM driver:
 * Avoid failure when doing mem_driver->Create('MEM:::')

MrSID driver:
 * Use VSI Virtual File API in MRSID DSDK I/O routines. MrSID reading now
works through the VSI calls as any other GDAL driver.
 * Added support for MrSID DSDK 7.x (#2410)
 * Use int 32 types instead of long types for LTI_METADATA_DATATYPE_UINT32 and LTI_METADATA_DATATYPE_SINT32 metadata (#2629)

MSG driver:
 * Fixes and improvements to enable compilation with GCC 4.x (Ticket #2168).

NDF driver:
 * Support NDF2 files in other than the current directory (#2274)
 * Added somewhat improved coordinate system support (#2623)

NetCDF driver:
 * Handle very large attributes properly (#2196)
 * NETCDF plugin name doesn't correspond to the loader entry name causes an error in AutoLoadDrivers (#2464)
 * Fix allocation of panBandZLev (#2582)
 * Fix accidentally too large memory allocation (#2591)
 * Do not report char variables as subdataset (#2599)
 * Fix LAEA projection (#2584)

NITF driver:
 * Add support for RPB and IMD files
 * Handle NITF JPEG-compressed image with data mask subheader (IC=M3) multi-blocks (#2364)
 * Implement SetProjection for NITF (#2095)
 * Added support for decoding 12 bit images (#2532)
 * Added support for writing TEXT segments in CreateCopy()
 * Added support for writing arbitrary user defined TREs
 * Fix #2249 : shift when writing NITF color table with nColors < 256
 * Prevent crash with LUT entry count > 256
 * Disable unnecessary VSIFFlush() calls that slowdown writing on some systems
 * Apply untested RPC00A remapping (#2040)
 * Fix #2135 by narrowing workaround test made for #1750
 * Prevent crash on NITF file without image segment (#2362)
 * Additional fix for handling 1-bit uncompressed NITF images with NITF_IC=NM  (#1854)
 * Set IREP=RGB implicitly when the first 3 channels of an image are R,G,B (#2343)
 * Allocate one extra byte for the NULL terminating character when reading TEXT data in NITF file (#2366)
 * Fix 'adding BLOCKA TRE precludes writing IGEOLO' (#2475)
 * Add GDAL_DMD_CREATIONOPTIONLIST for NITF
 * Prevent crash when using a bad value for TRE creation option
 * Fallback to pam info for nodata (#2596)

PAUX driver:
 * Check for either generated spelling of AuxiliaryTarget (#2219)

PCRaster driver:
 * Add overview support
 * Added support for CSF version 1. Updated nodata values to be equal to gdal's internal ones.

PDS driver:
 * Add support for # style comments (#2176)
 * Improve PDS dataset identification & fixes image segment offset (#2397)
 * Add LSB_SIGNED_INTEGER

PGCHIP driver:
 * Many memory leak fixes & cleanups, add an extra parameter '%name=my_name' for handling several rasters in the same table, add support for reading&writing geotransform

PNM driver:
 * Fix potential buffer overflow in case of bad PNM file
 * Fix logical tests in PNM Identify (bug #2190)

RAW drivers:
 * manage RawRasterBand NODATA values at PAM level
 * RawRasterBand : add extra parameter bOwnsFP to enable the RawRasterBand to take ownership of the fpRaw so as to close it properly in its destructor
 * Fix crash in rawdataset.cpp with pixeloffset=0 (#2576)

RMF driver:
 * Do not forget to swap block size/offset table on big-endian archs. (#2169)
 * Added support for reading and writing extended header..
 * RMF driver can crash / corrupt stack when importing projection from Panorama (#2277)
 * Fixed error checking code returned by color table read function.
 * Added support for reading big endian variant of the RSW files.
 * Report units and dataset statistics (#2670)

RPFTOC driver:
 * Enable external overview building on RPFTOC subdatasets
 * Use new proxy API instead of RPFTOCGDALDatasetCache
 * Initialize PAM for RPFTocDataset
 * Implement GetFileList() for RPFTOCDataset and RPFTOCSubDataset

RS2 driver:
 * Added projection reading
 * Updates to RADARSAT-2 driver to account for tiled GeoTIFF images.
 * Capture all files for GetFileList().
 * Support selecting directory as well as product.xml to open the dataset.
 * Various other enhancements

SDTS driver:
 * Prevent infinite recursion in SDTSRasterReader::GetBlock when CEL0 file is truncated
 * SDTS DEM : Read metadata in the IDEN file

SGI driver:
 * Implemented SGI write support (always RLE)
 * Fix SGI driver that misidentified SRTMHGT files as SGI files (#2289)

Terragen Driver:
 * Fix overflow in implicit constant conversion (#2119)

Terralib driver:
 * New for 1.6.0

TSX driver:
 * Added support to extract GCPs from XML metadata for TerraSAR-X SSC products.
 * Provide an error message if the sceneInfo tag cannot be found in the TerraSAR-X image metadata.
 * Fix lat/lon inversion (whoops, #2565); expose additional metadata items

USGSDEM driver:
 * mark NTS and INTERNALNAME as legal options
 * make parser more permissive (#2348)
 * add missing ZRESOLUTION creation option in GDAL_DMD_CREATIONOPTIONLIST
 * USGSDEM: add precisions to creation options documentation; check that source dataset dimensions are at least 2x2 for CreateCopy()
 * USGSDEM: fix USGSDEMDecToPackedDMS when input is very close to an integer degree value

VRT driver:
 * Use VSIF Large API in VRTDataset::Open to fix #1070
 * recover from failure to create transformer (#2240)
 * Added LUT based transformation support to the VRTComplexSource
 * Extend the output of <SimpleSource> in a forward and backward compatible way, and make use of GDALProxyPoolDataset when possible
 * Add the <ColorTableComponent> element to <ComplexSource> to do color table expansion in the VRT
 * Fix failure when attempting to read a warped VRT made from a 3-band dataset with -dstalpha option (#2502)
 * In VRTDerivedRasterBand::IRasterIO() don't call RasterIO() on sources with 0,0 for nPixelSpace and nLineSpace as most sources, except VRTSimpleSource, don't translate them.
 * Allow empty category in VRT rasterband (#2562)
 * Use nodata in VRTKernelFilteredSource::FilterData (#1739)
 * Fix VRT average resampling when resampling factor > 100% (#1725)

WCS driver:
 * Improvements to identify Band field name
 * More fiddling with GetCoverage() bounding boxes.  Avoid half pixel bounding box shift south east.  When using GridOffset/GridStep values expand the bounding box out by 1% of a pixel to avoid "on edge" rounding issues.
 * Strip namespaces off DescribeCoverage response (early), and add a bug
workaround for GeoServer WCS 1.1 responses.
 * Correct wcs 1.1 band identification logic
 * URL encode format and coverage name.  Improve error recognition.
 * GridOffset should be top left corner of pixel center oriented bounds

WMS driver:
 * Add ClampRequests setting (#2450)
 * Fix WMS driver to make it work when ReadBlockFromFile() must deal with blocks already in block-cache but that are not the band to fill (#2647)
 * Add a <Timeout> option (#2646)

XPM driver:
 * Prevent crash when opening an XPM file with large file API


## OGR 1.6.0 - Overview of Changes

General:
  * RFC 21: OGR SQL type cast and field name alias (#2171)
  * Added support for outline color in OGRStyleLabel (#2480)
  * Added support for symbol outline color in OGR Style Strings (#2509)
  * Added geometry type merger
  * Added SetEquirectangular2()
  * Added SetLinearUnitsAndUpdateParameters() to C API
  * Add support to translate OGRPolygon to OGRMultiLineString
  * Add a segmentize() method to OGRGeometry to modify the geometry such it has no segment longer then the given distance; add a -segmentize option to ogr2ogr
  * Many performance fixes in OGRGeometryFactory::organizePolygons (#1217, #2428, #2589)
  * Changed OGRFeature::GetStyleString() to return the value of the OGR_STYLE field if no style string have been specified.
  * Ensure OpenShared sharing is only with same thread (#2229)
  * ogrfeaturestyle : OGRSTBrushAngle parameter should not be georeferenced.
  * Strip whitespaces at the start and end of parsed pairs of style elements in OGRStyleTool::Parse(). As per #1413.
  * Remove empty linestrings from multilinestring objects
  * Allow 'POINT EMPTY' in WKT (bug #1628)
  * Fix OGRGeometryCollection::getCoordinateDimension() (#2334)
  * Make OGRLineString::importFromWkb and OGRPolygon::importFromWkb with EMPTY geometries
  * Implement IsEmpty() for all geometries without using GEOS (for speed purpose, and also because GEOS 2.2.3 is buggy with multipolygons with empty polygon inside); Make exportToWkt() export a valid WKT when multipoints/linestrings/polygons have an empty geometry inside
  * Reintroduce OFTWideString and OFTWideStringList but mark them as deprecated (#2359).
  * Fixed segmentation fault in swq_select_finish_summarize when SQL query issued on layer without any attributes like empty shapefile (Ticket #2358).
  * Fix crash in OGRDataSource::ExecuteSQL with an empty SQL query (#2386)
  * Make OGRLayer::FilterGeometry more restrictive in the geometries it selects (#2454)
  * OGRStyleVector class and related stuff has been removed (#2070).

  * Fixed OGR SQL to properly escape single quotes in string literals (Ticket #2220).
  * Prevent an OGR driver from being registered several times (#2544)

Utilities:
 * Start on a dissolve utility based on ogr2ogr
 * Add --version and --licence options for OGR CLI utilities

OGRSpatialReference:
 * RFC 20: Axes methods
 * Upgrade to EPSG 6.17
 * Added support for "International Map of the World Polyconic" and "Wagner I-VII" projections.
 * Add EquidistantCylindricalSphere and GaussLabordeReunion (#2134)
 * Fix exportToProj() translation for OSGB36 (#2160)
 * ogr_srs_panorama.cpp : Fixed search in ellipsoid list
 * ogr_srs_pci.cpp : List of ellipsoids updated
 * Treat spherical mercator 1SP similarly to normal mercator 1sp (proj #9).
 * Ensure Clear() clears the bNormInfoSet flag and use Clear() from the various import methods to wipe old state (#2533).
 * add import/export for MITAB CoordSys
 * Added declarations for OSRImportFromMICoordSys()/OSRExportToMICoordSys(); make OSRImportFromPanorama()/OSRExportToPanorama() externally visible again.
 * Ensure rectified_grid_angle gets stripped for HOM projections (#2575)
 * ogr_srs_esri.cpp: Correct test of iRGAChild (#2575).
 * Recent EPSG releases seem to use PolarLongOrigin instead of ProjCenterLong for the Krovak projection parameters.  Handle either (#2559).
 * Test that input pointer is not NULL in OGRSpatialReference::importFromWkt().

AVC driver:
 * Make AVCE00 a distinct driver
 * Update from AVCE00 master, includes the fixes for #2495 (GCC warnings)
 * Detect compressed E00 input files and refuse to open them instead of crashing (#2513)
 * Avoid scanning the whole E00 input file in AVCE00ReadOpenE00() if the file does not start with an EXP line (#1989)

BNA driver:
 * Fix crash when trying to write features with empty geometries
 * Add support for Unix End-Of-Line characters on Windows

CSV driver:
 * Add support for writing the geometry of features through the new GEOMETRY layer creation option
 * Allow to define field width via .csvt text file (bug #2142)
 * Handle more gracefully CSV files with an empty column title (#2538)

DGN driver:
 * add some experimental linkage testing

DODS driver:
 * add using namespace libdap for version 3.8.2 (#2404)

Geoconcept Export driver:
 * New for 1.6.0

GeoJSON driver:
 * Fixed GeoJSON driver crash when writing features with null geometry (#2212)
 * GeoJSON: enabled read/write of 25D geometry types.
 * Improved GeoJSON driver to gracefully handle JSON strings that do not encode GeoJSON content.
 * Support GeoJSON 1.0 Spec CRS 'link' and 'name' members. (#2665)

GPX driver:
 * XML Datetime can be expressed without explicit timezone mention
 * <extensions> is valid inside <rtept> and <trkpt> too
 * Add support for GPX 1.0 reading
 * Handle degenerate and NULL geometries in creation mode
 * Remove noisy CPL_DEBUG message when GPX driver built without Expat and when the input file doesn't look like GPX (#2394)
 * Prevent GPX and KML drivers to read too much of a non GPX/KML file (#2395)
 * GPX writer: Remove leading spaces for a numeric field (#2638)

GML driver:
 * Support reading GML 3 posList geometry (#2311)
 * Add logic to potentially track geometry type (mostly for NAS just now)
 * Fix #2141 : GML driver recognizes improperly strings as integers
 * Do geometry element name test (IsGeometryElement) case sensitive to avoid false positives on property names, and such (#2215).
 * Fix memory bug in OGRGMLLayer::GetNextFeature() when using attribute filter (#2349)

GMT driver :
 * Remove spaces from numeric field values before writing to avoid unnecessary quoting.

GRASS driver:
 * Call Vect_close() in the OGR GRASS driver (#2537)

INGRES driver:
 * New for 1.6.0

Interlis 1 driver:
 * Support for SURFACE polygons spread over multiple OBJECTs Polygonize on demand. Generation of area layer
 * Fix a crash (#2201)
 * Fix memory leaks & apps/test_ogrsf correctness in OGRILI1 (#2203)
 * Prevent crash in OGRILI1DataSource::Open with an empty string (#2335)
 * Fixed column order detection for some Interlis 1 models (#2595)

Interlis 2 driver:
 * Fix memory usage and leaks in OGRILI2 (bug #2203)
 * Fix logic to detect ILI2 datasets (#2516)

KML driver:
 * Updated KML write driver to support KML v2.2.
 * Added support for "SchemaData" - typed KML fields that maintain feature data.
 * Support of date types, written out as strings (#2264)
 * Added automatic coordinate system transformation to WGS84 (the only CS that KML recognizes). (#2271)
 * Report XML parsing error in KML driver in a similar way it's done in the GPX driver
 * Speedup detection of KML documents
 * Use VSI Large File API
 * Corrected case on schemaUrl attribute and added the id attribute to the schema element. (#1897)

MITAB driver:
 * Upgraded to current dev version of MITAB - includes a number of TAB StyleString improvements
 * Support font point outline color

MySQL Driver:
 * Fix memory leaks in MySQL driver
 * Fix crash with very long WHERE clause in MySQL driver
 * Use assignSpatialReference for read features
 * Fix SRS cache in FetchSRS()
 * MYSQL: Add backquotes around table and column names to enable the use of reserved keywords (#2315)
 * Fix reporting of capabilities for OGRMySQLResultLayer
 * return proper results for various writing capabilities (#2184)

NTF driver:
 * Update for new meridian and strategi products (#2600)

OCI driver:
 * Fixed memory leaks in OCI driver reported by Linda Thompson (#2120)
 * Support for OCI + VRT to access non spatial data (#2202)
 * Prevent from calling CPLError when SDO_GEOMETRY is missing (non spatial) (#2202)
 * Added support for fields of type DATE and TIMESTAMP [WITH [LOCAL] TIME ZONE] as OFTDate and OFTDateTime.
 * An extra space is needed for the decimal separator when retrieving the numeric fields (#2350)
 * Improved OCI driver to query spatial extent of layer using SDO_GEOM_METADATA for better performance (Ticket #543).
 * OCI: Filter out MDSYS.CS_SRS entries with NULL value of WKTEXT.
 * add compound and stroked-arc read support

PG (Postgres/PostGIS) Driver:
 * Add the ability to specify a list of tables to be treated as layers with the 'tables=' connection string option (#1699)
 * Add SPATIAL_INDEX creation option to create GIST index. Turned ON by default (#959)
 * Add support for listing and correctly fetching geometry type of inherited spatial tables (#2558)
 * Add support for Postgis tables with multiple geometry columns (#1476)
 * Fixes to Postgres binary cursor mode and improvement/fixes to data types handling (#2312)
 * Implement efficient GetFeatureCount, SetSpatialFilter and GetExtent for OGRPGResultLayer
 * Apply spatial filter in OGRPGDataSource::ExecuteSQL()
 * Make binary cursor work with non-PostGIS geometry column
 * Fix memory leaks
 * Fix write outside of allocated buffer in OGR PG driver (#2303)
 * Use assignSpatialReference for read features
 * Fix geometry filter when there is no PostGIS geometry column
 * Fix getting the PK of a table with PostgreSQL <= 7.3; Fix CreateFeatureViaCopy when geometry column is not PostGIS geometry
 * Use the 'auth_srid' to avoid mismatches between OGR and PostGIS WKTs of EPSG codes (#2123)
 * Fix reporting of capabilities and handling of spatial and attribute filters by OGRPGResultLayer
 * Add PG_USE_POSTGIS to be able to disable PostGIS for debug purpose
 * Re-enable PQsetClientEncoding but set it to UNICODE now that the driver advertises OLCStringsAsUTF8
 * Replace use of risky sprintf by CPLString to avoid potential buffer overflows
 * In non PostGIS mode, skip tables of schema 'information_schema'
 * Allow VACUUM through ExecuteSQL() without a transaction (#2619).

PGEO driver:
 * correct testcapability results (#2601)
 * treat type 50 geometry as SHPT_ARC (#1484)

SDE driver:
 * Versioned editing/write support for SDE.

SQLite driver:
 * Added support for geometry_columns, and WKB support
 * Added preliminary FGF to geometry support
 * Added spatial_ref_sys support
 * Added preliminary support for spatialite geometries

SHAPE driver:
 * CreateField() now works on populated layers (#2672)
 * Cleanup to remove unused old classification code for multipolygons (#2174)
 * Fix error class in VSI_SHP_Error (#2177)
 * Fix crash with polygon with nParts == 0
 * Change SHAPE driver to return a NULL geometry instead of an empty OGRMultiPoint, OGRMultiLineString and OGRMultiPolygon (bug #2217)
 * Fix crashes on corrupted geometries (#2218 and #2610)
 * Fix crash when dealing with unhandled field types in shape driver, in DEBUG mode (#2309)
 * Add case for wkbMultiLineString and 25D in OGRShapeLayer::CreateFeature
 * Make SHPWriteOGRObject write a SHPT_NULL object for empty geometries and handle correctly multigeometries with empty geometries inside
 * Make sure field type set to OFTDate when OFTDataTime requested (#2474)
 * Implement OGRShapeDataSource::DeleteLayer() (#2561)

S57 driver:
 * Ensure SOUNDG in multipoint form is MultiPoint25D not 2D.
 * Print out contents of OGR_S57_OPTIONS environment variable if set (for debug
purposes).
 * Correct control for applying updates, now done in Ingest method.

VRT driver:
 * carry style string through VRT layer
 * Pass the envelope of the geometry as the spatial filter in the sub-query (#2214)
 * Add "shared" attribute on SrcDataSource to control sharing.  Default to OFF for SrcLayer layers, and ON for SrcSQL layers.  This endeavors to avoid conflicts of layer state. (#2229)

XPlane/Flightgear driver:
 * New for 1.6.0



## SWIG Language Bindings

SWIG General :
 * Added GetHistogram
 * Added SetLinearUnitsAndUpdateParameters
 * Added GetSubDatasets method on Dataset
 * Added SetEquirectangular2
 * Cast returned value to OGRDriverShadow in GetDriver method instead of OGRLayerShadow
 * Make it possible to skip adding the inline C functions into the wrapper
 * Fix SWIG ReadRaster_internal and DSReadRaster_internal may crash (#2140)
 * Modify GeneralCmdLineProcessor() to recognise that a <= 0 return result is special and means NULL should be returned indicating a need to terminate the calling application.
 * Added SetMetadataItem/GetMetadataItem
 * Added gdaltransformer wrapper for RFC 22
 * implement the BuildFromEdges function #2380
 * Added ComputeProximity
 * Added GDALRegenerateOverviews
 * Added GetFileList
 * Added GRA_Lanczos
 * Added gdal.Polygonize()
 * expose GDALDestroyDriverManager() to improve leak detection
 * Added RasterizeLayer() and SieveFilter()
 * If we receive an odd array type in BandWriteArray(), cast the array to float64 which we do support (#2285).

SWIG C# related changes:
 * Implement GDALProgressFunc callback for C# (fix for #2122)
 * Fixed that passing null as the options parameter of Driver.CreateCopy causes access violation (#2185).
 * Added GDALCreateCopy sample application
 * Support for signing the GDAL C# assemblies (#2186)
 * Added raster specific tests
 * Reworked the wrapper implementation
 * Added the bandMap parameter to the C# Dataset.ReadRaster and Dataset.WriteRaster API.
 * Added a C# sample to demonstrate the GDALDatasetRasterIO operations.
 * Added Band.GetHistogram to the C# bindings
 * Sample application for Band.GetHistogram.
 * Added the GDALAdjustContrast sample to demonstrate the image correction at the C# side.
 * Added Dataset.GetGCPs, Dataset.SetGCPs and GCPsToGeoTransform in the C# bindings (bugs #2426, #1986 and #1677
 * Added support for using OGR.Layer in Gdal.Polygonize
 * Changed the behavior to use OSR.SpatialReference and OSR.CoordinateTransformation instead of defining the same classes in the OGR namespace
 * Changed the scope from internal to public of the required functions

SWIG Python related changes:
 * Fix layer __getitem__ bug (#2187)
 * add some sugar to ogr.DataSource.DeleteLayer to be able to take in either an index or a layer name.  If you give it a name, it *will* loop through all of the layers on the datasource.  This might be expensive depending on the driver.
 * add date/time fetching support to the generic GetField implementation... note this is not real 'datetime' support yet
 * a typemap for taking in lists of GDAL objects #2458
 * don't always return 0 for OGRErrs #2498
 * Added GetDefaultHistogram() with Python implementation
 * support for mingw Windows builds
 * Link with gdal_i.lib instead of gdal.lib when building with MSVC compiler. (#2578)

SWIG Perl
 * Added a more verbose description to the error message if projection method test fails. Skip testing parameters of International Map of the World projection since it fails (a bug?).
 * driver's create method's 2nd parameter, if given, is a listref
 * Support Cygwin by adding -lstdc++ to LIBS if OS is cygwin
 * Add GetDriver method as an alias to _GetDriver for DataSource. This fixes an unnoticed side-effect of rewrapping GetDriver for root class OGR.
 * force name to be a string for _GetLayerByName and make default for name 0
 * fix Layer::Schema
 * use perl hash also for fields in schema (the new API was not really implemented), add Schema method also for Feature
 * croak in Geometry::create unless type, wkt, wkb, or gml given
 * do not call UseExceptions when booting OGR wrappers since it is only done once when booting GDAL (of which OGR is a part), do not include inline functions from cpl_exceptions.i into OGR wrappers
 * add exception support as for OGR
 * use geometry factory methods in Geometry::create; accept also only coordinate parameters (i.e. auto-add 0 if needed) in Point method for Point type
 * Add wrappers for field types datetime and lists. Do not use the overloaded (field name) versions of the get/set functions.
 * GetField and SetField methods, which check for goodness of the field (name, index) and support dates, times, datetimes, and lists. In Row and Tuple the field value may be a listref.
 * Support HEXWKB in Geo::OGR::Geometry::create.
 * As* methods for Geometry as aliases for ExportTo; SpatialReference->create constructor
 * made needed links from parameters to typemaps to make Get- and SetDefaultHistogram methods work in Perl
 * support for mingw Windows builds

SWIG Java:
 * removed colortable from java bindings for now (#2231)

# GDAL/OGR 1.5.0

## GDAL/OGR 1.5.0 - General Changes

Build:
 * CFG environment variable now ignored.  Instead set CFLAGS and CXXFLAGS
   environment variables to desired compilation options, or use --enable-debug
   for a debug build.  Default is "-g -O2" like most other packages.
 * Added --with-hide-internal-symbols to restrict exported API from .so files
   to be the GDAL public API (as marked with CPL_DLL).

Other:
 * OGR and GDAL C APIs now generally check for NULL objects and recover
   with an error report instead of crashing.


## GDAL 1.5.0 - Overview of Changes

Core:
 * Enable Persistent Auxiliary Metadata (.aux.xml) by default.
 * Support for "pam proxies" for files in read-only locations.
 * Create and !CreateCopy pre-Delete output existing dataset.
 * Added Identify() method on drivers (per RFC 11: Fast Format Identify)
 * Implement !GetFileList() on datasets (per RFC 12).
 * Implement Delete(), Rename(), Copy() based on !GetFileList() (per RFC 12).
 * vrtdataset.h, memdataset.h and rawdataset.h are now considered part of
   the public GDAL API, and will be installed along with gdal.h, etc.
 * Support nodata/validity masks per RFC 14: Band Masks.
 * Plugin drivers test for ABI compatibility at load time.
 * Creation flags can now be validated (this is used by gdal_translate)
 * Default block cache size changed to 40MB from 10MB.

Algorithms / Utilities:
 * gdal_grid: New utility to interpolate point data to a grid.
 * gdal2tiles.py is new for 1.5.0.
 * gdaltransform: stdin/stdout point transformer similar to PROJ.4 cs2cs.
 * gdalwarp: Several fixes related to destination "nodata" handling and
   nodata mixing in resampling kernels.
 * gdalwarp: Added Lanczos Windows Sinc resampling.
 * gdal_rasterize: added -i flag to rasterize all areas outside geometry.
 * gdalenhance: new utility for applying histogram equalization enhancements.
 * gdalmanage: Utility for managing datasets (identify, delete, copy, rename)
 * nearblack: Utility for fixing lossy compressed nodata collars.

Intergraph Raster Driver:
 * New for 1.5.0.

COSAR (TerraSAR-X) Driver:
 * New for 1.5.0.
 * SAR Format.

COASP Driver:
 * New for 1.5.0
 * SAR format produced by DRDC CASP SAR Processor.

GFF Driver:
 * New for 1.5.0

GENBIN (Generic Binary) Driver:
 * New for 1.5.0.

ISIS3 Driver:
 * New for 1.5.0.
 * Also PDS and ISIS2 driver improved substantially and all moved to frmts/pds

WMS Driver:
 * New for 1.5.0.

SDE Raster Driver:
 * New for 1.5.0.

SRTMHGT Driver:
 * New for 1.5.0.

PALSAR Driver:
 * New for 1.5.0.
 * SAR format.

ERS Driver:
 * New for 1.5.0.
 * ERMapper ASCII Header

HTTP Driver:
 * New for 1.5.0.
 * Fetches file by http and then GDALOpen()s.

GSG Driver:
 * New for 1.5.0.
 * Golden Software Surfer Grid.

GS7 Driver:
 * New for 1.5.0.
 * Golden Software Surfer 7 Binary Grid.

Spot DIMAP Driver:
 * New for 1.5.0.

RPFTOC Driver:
 * New for 1.5.0.

ADRG Driver:
 * New for 1.5.0.

NITF Driver:
 * Added support for writing JPEG compressed (IC=C3).
 * Added support for reading text segments and TREs as metadata.
 * Added support for 1bit images.
 * Added support for GeoSDE TRE for georeferencing.
 * Support PAM for subdatasets.
 * Improved NSIF support.
 * Support C1 (FAX3) compression.
 * Improved CADRG support (#913, #1750, #1751, #1754)

ENVI Driver:
 * Many improvements, particularly to coordinate system handling and metadata.

JP2KAK (Kakadu JPEG2000) Driver:
 * Now builds with libtool enabled.

GTIFF (GeoTIFF) Driver:
 * Now supports BigTIFF (read and write) with libtiff4 (internal copy ok).
 * Upgraded to include libtiff 4.0 (alpha2) as the internal option.
 * Support AVERAGE_BIT2GRAYSCALE overviews.
 * Produce pixel interleaved files instead of band interleaved by default.
 * Support TIFF files with odd numbers of bits (1-8, 11, etc).
 * Add ZLEVEL creation option to specify level of compression for DEFLATE method

GIF Driver:
 * Nodata/transparency support added.

JPEG Driver:
 * Support in-file masks.

AIGrid Driver:
 * Supports reading associated info table as a Raster Attribute Table.

HFA Driver:
 * Support MapInformation/xform nodes for read and write.
 * Support AVERAGE_BIT2GRAYSCALE overviews.
 * Support Signed Byte pixel type.
 * Support 1/2/4 bit pixel types.
 * Support PE_STRING coordinate system definitions.
 * Support nodata values (#1567)

WCS Driver:
 * Support WCS 1.1.0

DTED Driver:
 * Can now perform checksum verification.
 * Better datum detection.

HDF4 Driver:
 * Support PAM for subdatasets.

Leveller Driver:
 * Added write support.
 * Added v7 (Leveller 2.6) support.

## OGR 1.5.0 - Overview of Changes

General:
 * Plugin drivers test for ABI compatibility at load time.
 * SFCOM/OLEDB stuff all removed (moved to /spike in subversion).
 * Various thread safety improvements made.
 * Added PointOnSurface implementation for OGRPolygon.
 * Added C API interface to OGR Feature Style classes (RFC 18).

Utilities:
 * All moved to gdal/apps.

OGRSpatialReference:
 * Supports URL SRS type.
 * Upgraded to EPSG 6.13.
 * Operating much better in odd numeric locales.

BNA Driver:
 * New for 1.5.0.

GPX Driver:
 * New for 1.5.0.

GeoJSON Driver:
 * New for 1.5.0.

GMT ASCII Driver:
 * New for 1.5.0.

KML Driver:
 * Preliminary read support added.

DXF / DWG Driver:
 * Removed due to licensing issues with some of the source code.  Still
   available in subversion from under /spike if needed.

PG (Postgres/PostGIS) Driver:
 * Added support for recognising primary keys other than OGR_FID to use as FID.
 * Improved schema support.
 * Performance improvements related to enabling SEQSCAN and large cursor pages

Shapefile Driver:
 * Do not keep .shx open in read only mode (better file handle management).
 * Use GEOS to classify rings into polygons with holes and multipolygons if it is available.
 * Support dbf files larger than 2GB.

MySQL Driver:
 * Added support for BLOB fields.

MITAB (MapInfo) Driver:
 * Upgraded to MITAB 1.6.4.

Interlis Drivers:
 * Support datasources without imported Interlis TID
 * Remove ili2c.jar (available from http://home.gdal.org/dl/ili2c.jar
 * Support for inner rings in Surface geometries.
 * Support spatial and attribute filters.

## SWIG Language Bindings

 * The "Next Generation" Python SWIG bindings are now the default.
 * Python utility and sample scripts migrated to swig/python/scripts and
   swig/python/samples.
 * Added Raster Attribute Tables to swig bindings.
 * Added Geometry.ExportToKML
 * Added CreateGeometryFromJson
 * Added Geometry.ExportToJson

SWIG C# related changes:
 * Support for the enumerated types of the C# interface
 * C# namespace names and module names follows the .NET framework naming guidelines
 * Changed the names of the Windows builds for a better match with the GNU/Linux/OSX builds
 * The gdalconst assembly is now deprecated
 * GDAL C# libtool build support
 * !CreateFromWkb support
 * Dataset.!ReadRaster, Dataset.!WriteRaster support
 * Added support for Dataset.!BuildOverviews
 * More examples added

SWIG Python related changes:
 * Progress function callback support added.  You can use a Python function, or the standard GDALTermProgress variant
 * Sugar, sweet, sweet sugar.
    * ogr.Feature.geometry()
    * ogr.Feature.items()
    * ogr.Feature.keys()
 * doxygen-generated docstrings for ogr.py
 * geometry pickling
 * setuptools support
 * !PyPi http://pypi.python.org/pypi/GDAL/
 * setup.cfg for configuring major significant items (libs, includes, location of gdal-config0
 * support building the bindings from *outside* the GDAL source tree

SWIG Java:
 * SWIG Java bindings are orphaned and believed to be broken at this time.


# GDAL/OGR 1.4.0 - General Changes

Perl Bindings:
 - Added doxygen based documentation.

NG Python Bindings:
 - Implemented numpy support.

CSharp Bindings:
 - Now mostly operational.

WinCE Porting:
 - CPL
 - base OGR, OSR and mitab and shape drivers.
 - GDAL, including GeoTIFF, DTED, AAIGrid drivers
 - Added test suite (gdalautotest/cpp)

Mac OSX Port:
 - Added framework support (--with-macosx-framework)

## GDAL 1.4.0 - Overview Of Changes

WCS Driver:
 - New

PDS (Planetary Data Set) Driver:
 - New

ISIS (Mars Qubes) Driver:
 - New

HFA (.img) Driver:
 - Support reading ProjectionX PE strings.
 - Support producing .aux files with statistics.
 - Fix serious bugs with u1, u2 and u4 compressed data.

NITF Driver:
 - Added BLOCKA reading support.
 - Added ICORDS='D'
 - Added jpeg compression support (readonly)
 - Support multiple images as subdatasets.
 - Support CGM data (as metadata)

AIGrid Driver:
 - Use VSI*L API (large files, in memory, etc)
 - Support upper case filenames.
 - Support .clr file above coverage.

HDF4 Driver:
 - Added support for access to geolocation arrays (see RFC 4).
 - External raw raster bands supported.

PCIDSK (.pix) Driver:
 - Support METER/FEET as LOCAL_CS.
 - Fix serious byte swapping error on creation.

BMP Driver:
 - Various fixes, including 16bit combinations, and non-intel byte swapping.

GeoTIFF Driver:
 - Fixed in place update for LZW and Deflated compressed images.

JP2KAK (JPEG2000) Driver:
 - Added support for reading and writing gmljp2 headers.
 - Read xml boxes as metadata.
 - Accelerate YCbCr handling.

JP2MrSID (JPEG2000) Driver:
 - Added support for reading gmljp2 headers.

EHDR (ESRI BIL) Driver:
 - Support 1-7 bit data.
 - Added statistics support.

## OGR 1.4.0 - Overview of Changes

OGR SQL:
 - RFC 6: Added support for SQL/attribute filter access to geometry, and
   style strings.

OGRSpatialReference:
 - Support for OGC SRS URNs.
 - Support for +wktext/EXTENSION stuff for preserving PROJ.4 string in WKT.
 - Added Two Point Equidistant projection.
 - Added Krovak projection.
 - Updated support files to EPSG 6.11.

OGRCoordinateTransformation:
 - Support source and destination longitude wrapping control.

OGRFeatureStyle:
 - Various extensions and improvements.

INFORMIX Driver:
 - New

KML Driver:
 - New (write only)

E00 Driver:
 - New (read only)
 - Polygon (PAL) likely not working properly.

Postgres/PostGIS Driver:
 - Updated to support new EWKB results (PostGIS 1.1?)
 - Fixed serious bug with writing SRSes.
 - Added schema support.

GML Driver:
 - Strip namespaces off field names.
 - Handle very large geometries gracefully.

ODBC Driver:
 - Added support for spatial_ref_sys table.

SDE Driver:
 - Added logic to speed things up while actually detecting layer geometry types

PGeo Driver:
 - Added support for MDB Tools ODBC driver on linux/unix.

VRT Driver:
 - Added useSpatialSubquery support.


# GDAL/OGR 1.3.2 - General Changes

WinCE Porting:
 - Support for MS WinCE new for this release.

Java SWIG Bindings:
 - Preliminary support implemented.


## GDAL 1.3.2 - Overview of Changes

Locale:
 - Force numeric locale to "C" at a few strategic points.

Idrisi Driver:
 - New for 1.3.2.
 - Includes reading and writing.
 - Limited coordinate system support.

DIPEx Driver:
 - New for GDAL 1.3.2 (related to ELAS format).

Leveller Driver:
 - New for GDAL 1.3.2.

NetCDF Driver:
 - Improved autoidentification of x, y dimensions.
 - Improved CF support.

JPEG2000 (JasPer) Driver:
 - Use GDALJP2Metadata to support various kinds of georeferencing.

JPEG2000 (JP2KAK) Driver:
 - Support writing tiles outputs so that very large images can be written.

GeoTIFF Driver:
 - Report error when attempting to create >4GB uncompressed file.
 - Updated to latest libtiff, now supports "old jpeg" fairly well.
 - Improved support for subsampled YCbCr images.

Imagine (HFA) Driver:
 - Support reading affine polynomial transforms as geotransform.
 - Support overviews of different type than base band.
 - Support reading RDO style "nodata" indicator.

PCI Aux Driver:
 - Support projections requiring parameters.

MrSID Driver;
 - Fixed problem with writing files other than 1 or 3 bands.
 - Support ESDK 6.x.

BMP Driver:
 - Added support for 32bit images with bitfields compression.

DODS Driver:
 - Upgraded to support libdap 3.6.x.
 - Upgraded to support [-x][-y] to flip image.

gdal_rasterize Utility:
 - New for GDAL 1.3.2.
 - Rasterize OGR polygons into a raster.

## OGR 1.3.2 - Overview of Changes

OGRFeature:
 - Added support for OFTDate, OFTTime and OFTDateTime field types.
 - Also applied to a few drivers (shapefile, mysql, postgres)

OGRLayer:
 - GetFIDColumn() and GetGeometryColumn() added.

Generic OGR SQL:
 - Proper support for spatial and attribute filters installed on
   OGR SQL resultsets.

OGRSpatialReference:
 - Upgraded data files to EPSG 6.9

PostGIS Driver:
 - Include proj4text in new spatial_ref_sys entries.
 - Fixed support for very large queries.
 - Fixed DeleteLayer() implementation.
 - Added COPY support for accelerated loading.

MySQL Driver:
 - Added read and write support for Spatial types.
 - Support spatial_ref_sys and geometry_columns tables.
 - Various other improvements (dates, smallint, tinyint, etc)
 - More robust auto-detection of column types for layers
   created from SQL statements

ArcSDE Driver:
 - New for 1.3.2.
 - Read-only support for all geometry types.
 - Supports coordinate systems.
 - Requires SDE C API from ESRI.

Shapefile Driver:
 - Avoid posting errors when .dbf's without .shps are opened.
 - Added pseudo-SQL REPACK command after deleting features.
 - Implement DeleteFeature()

S-57 Driver:
 - Added support for Arcs.
 - Added special DSID_DSSI feature class to capture header info.

DGN Driver:
 - Support writing geometry collections.

DWG/DXF Driver:
 - New for OGR 1.3.2
 - Only supports writing DWG and DXF.
 - Depends on DWGdirect library.


# GDAL 1.3.1 - Overview of Changes

Next Generation SWIG Wrappers (GDAL and OGR):
 - Python, Perl and Ruby bindings considered to be ready to use.
 - C#, Java, PHP are at best initial prototypes.
 - Added configure options for most NG options.

PCRaster Driver:
 - libcsf is now included as part of GDAL.
 - PCRaster enabled by default on win32.
 - --with-pcraster=internal option now supported on unix (but not yet default)

VSI Virtualization:
 - The "large file API" (VSI*L) has been re-engineered to allow installing
   additional file handlers at runtime.
 - Added "in memory" VSI handler so that now any driver using VSI*L
   functions for data access can operate on in-memory files.
 - PNG, JPEG and GeoTIFF drivers upgraded to work with in-memory support.

Raster Attribute Tables:
 - Implemented new Raster Attribute Tables support.  See the
   GDALRasterAttributeTable class for more information.

Erdas Imagine Overviews:
 - Erdas Imagine driver upgraded to support building internal overviews.
 - Generic overview handler updated to support overviews in Erdas Imagine
   format for any file format.  Set USE_RRD config option to YES to enable.

gdalwarp:
 - Added proper support for "unified source nodata", so the -srcnodata
   switch works well.

RIK Driver:
 - New Swedish format driver implemented by Daniel Wallner.

JPEG Driver:
 - Substantial improvements to EXIF support.

MrSID Driver:
 - Updated with proper JPEG2000 support as JP2MRSID driver, including
   encoding with ESDK.
 - Updated to support MrSID Version 5.x SDKs.

PNG Driver:
 - Fixed serious bugs with 16bit file support.
 - Added NODATA_VALUES to identify RGB sets that indicate a nodata pixel.


## OGR 1.3.1 - Overview of Changes

Reference Counting:
 - OGRSpatialReference and OGRFeatureDefn now honour reference counting
   semantics.
 - Note that, especially for the OGRFeatureDefn, it is now critical that
   all drivers be careful with reference counting.  Any OGR drivers not in
   the core distribution will likely crash if not updated.

ESRI Personal Geodatabase Driver:
 - New driver implemented for ESRI Personal Geodatabase (.mdb) files.
 - Uses ODBC, enabled by default on win32.

ODBC Driver:
 - Updated to support binary fields.
 - Updated to support WKB geometry fields.
 - Updated to support DSN-less connections.

S57 Driver:
 - Added support for Inland Waterways, and Additional Military Layers profiles

# GDAL 1.3.0 - Overview of Changes

Multithreading:
 - Lots of work done to implement support for multiple threads reading
   from distinct GDALDataset objects at the same time.

GDALRasterBand / Persistent Auxiliary Metadata (PAM):
 - Support for preserving a variety of metadata in a supporting XML file.
 - GDALRasterBand now supports "remembering" histograms, and has a concept
   of the default histogram.
 - GDALRasterBand now supports remembering image statistics.
 - Disabled by default (set GDAL_PAM_ENABLED=YES to turn on).
 - Supported by *most* drivers with some caveats.

GDALCopyWords():
 - This function is a low level work horse for copying and converting pixel
   data in GDAL.  It has been substantially optimized by Steve Soule (Vexcel).

Next Generation Bindings:
 - Kevin Ruland and Howard Butler are working on reworked support for
   SWIG to generate Python, PHP, Java, C# and other language bindings for GDAL
   and OGR.

VB6 Bindings:
 - Now substantially complete, see VB6 directory.

HDF5 Driver:
 - New HDF5 driver implemented by Denis Nadeau.

RMF Driver:
 - New driver for Raster Matrix Format by Andrey Kislev.

MSGN (Meteosat Second Generation Native) Driver:
 - New driver implemented by Frans van der Bergh.

VRT Driver:
 - Fixed whopper of a memory leak in warped raster case.

NetCDF Driver:
 - Preliminary CF conventions support by Denis Nadeau.

NITF Driver:
 - NITF files between 2 and 4 GB in size can now be read and written.

JPEG Driver:
 - Added support for reading EXIF as metadata by Denis Nadeau.

DODS Driver:
 - Fixed up libdap 3.5.x compatibility.

JP2ECW (JPEG2000 ECW SDK) Driver:
 - Implemented support for new GML-in-JPEG2000 specification.
 - Implemented support for old MSI "worldfile" box.

JP2KAK (JPEG2000 Kakadu) Driver:
 - Implemented support for new GML-in-JPEG2000 specification.
 - Implemented support for old MSI "worldfile" box.

PCIDSK Driver:
 - tiled files now supported for reading.
 - overviews now supported for reading.

HFA (Imagine) Driver:
 - Supports creating internal overviews in very large files.
 - Support reading class names.
 - Support creating compressed files.

GeoTIFF Driver:
 - Support reading files with odd bit depths (i.e. 3, 12, etc).
 - Support 16/24bit floating point TIFFs (per Technote 3) (Andrey).
 - Support 12bit jpeg compressed imagery using libjpeg "MK1" library.

HDF4 Driver:
 - Added support for ASTER Level 1A, 1B and 2 products (Andrey).

## OGR 1.3.0 - Overview of Changes

OGRGeometry:
 - WKT (and GML) encoding now attempts to preserve pretty much full double
   precision.
 - geometries are now "coordinate dimension preserving" rather than dynamically
   figuring out dimension depending on whether Z is set.  So a geometry can
   now be 3D even if all z values are zero.
 - Fixed up proper EMPTY geometry support per standard.

GRASS Driver:
 - New driver for GRASS 6 vector data written by Radim Blazek.

Interlis Driver:
 - New driver for Swiss Interlis format from Permin Kalberer (SourcePole).

Shape Driver:
 - Fixed logic for degenerate polygons (Baumann Konstantin).

PostgreSQL/PostGIS Driver:
 - Implemented fast GetExtent() method (Oleg Semykin).
 - Implemented layer type from geometry_columns (Oleg Semykin).
 - Handle PostGIS 1.0 requirements for coordinate dimension exactness.
 - Handle EWKT type in PostGIS 1.0.
 - Generally PostGIS 0.x and 1.0 should now be supported fairly gracefully.
 - Added PostGIS "binary cursor" mode for faster geometry access.

VRT Driver:
 - Pass through attribute queries to underlying driver.
 - Pass through spatial queries as attribute filters on the underlying layer.

S57 Driver:
 - Added concept of supporting different profiles.
 - Added prototype AML profile support.

MySQL Driver:
 - Fixed for FID recognition (eg. mediumint).

GML Driver:
 - Various fixes for generated GML correctness (Tom Kralidis).

TIGER/Line Driver:
 - Added Tiger 2004 support.

Oracle Driver:
 - Use VARCHAR2 for fixed size string fields.
 - Use OCI_FID config variable when creating layers, and reading select results



# GDAL 1.2.6 - Overview of Changes

gdal_translate:
 - Added -sds switch to copy all subdatasets.

gdalwarp:
 - Added Thin Plate Spline support (-tps switch).

GDALRasterBand:
 - Now uses two level block cache allowing efficient access to files
   with a very large number of tiles.
 - Added support for YCbCr color space for raster band color interpretations.
 - Added AdviseRead() method - currently only used by ECW driver and OGDI
   drivers.

ILWIS Driver:
 - New driver for the raster format of the ILWIS software.

ECW Driver:
 - Updated to use ECW SDK 3.1 (older ECW SDK no longer supported!)

ECWJP2 Driver:
 - Added JPEG2000 support driver based on ECW/JPEG2000 SDK with a variety
   of features.

NITF Driver:
 - Added support for reading *and* writing JPEG2000 compressed NITF files
   using the ECW/JPEG2000 SDK.
 - Added ICHIPB support.

HDF Driver:
 - Add support for georeferencing from some additional metadata formats.
 - Fixed bug with multi-band HDF-EOS datasets.

MrSID Driver:
 - Driver can now be built as a plugin on win32.
 - Split out MrSID 3.x SDK support - not readily buildable now.
 - Implemented accelerated IO cases for MrSID 4.x SDK.
 - Support for writing MrSID files added (improved?)

Imagine Driver:
 - Fixed bug reading some large multiband Imagine files.
 - Added support for writing compressed files.

Win32 Builds:
 - Added versioning information to GDAL DLL.

L1B Driver:
 - Only return a reduced grid of control points.

IDA (WinDisp4) Driver:
 - New read/write driver for the Image Display and Analysis raster format
   used by WinDisp 4.

NDF (NLAPS) Driver:
 - Added NDF/NLAPS read driver for version 1 and 2.

MSG Driver:
 - Added support for the Metosat Second Generation raw file format.

GTiff Driver:
 - Added support for offset/scale being saved and loaded (special metadata).
 - Added Cylindrical Equal Area.
 - Added PROFILE creation option to limit extra tags.

PNG Driver:
 - Updated internal code for libpng to libpng 1.2.8.

## OGR 1.2.6 - Overview of Changes

OGRSFDriverRegistrar:
 - Added support for autoloading plugin drivers from ogr_<driver>.so.

ogr.py:
 - Geometry, and Feature now take care of their own reference counting and
   will delete themselves when unreferenced.  Care must still be taken to
   unreference all features before destroying the corresponding
   layer/datasource.
 - ogr.Feature fields can now be fetched and set directly as attributes.
 - Geometry constructor can now take various formats (wkt, gml, and wkb).
 - Added docstrings.
 - Added better __str__ methods on several objects.
 - Various other improvements.

OGRLayer:
 - Re-wrote generic spatial search support to be faster in case of rectangular
   filters.
 - Intersects() method now really uses GEOS.  This also affects all OGR
   layer spatial filtering (with non-rectangular filters).
 - Added SetNextByIndex() method on OGRLayer.

OGRSpatialReference:
 - Automatically generate +towgs84 from EPSG tables when translating to
   PROJ.4 if available and TOWGS84 not specified in source WKT.
 - Updated GML CRS translation to follow OGC 05-011 more closely.  Still
   incomplete but operational for some projections.
 - Added support for FIPSZONE State Plane processing for old ESRI .prjs.
 - Added Goode Homolosine support.
 - Added GEOS (Geostationary Satellite) support.

OCI (Oracle) Driver:
 - Added GEOMETRY_NAME creation option to control the name of the field to
   hold the geometry.

PostGIS Driver:
 - Fixed some problems with truncation for integer and float list fields.

Shapefile Driver:
 - Added support for MapServer style spatial index (.qix).

GML Driver:
 - Improved support for 3L0 (GML 3 - Level 0 profile) reading and writing.
   On read we can now use the .xsd instead of needing to build a .gfs file.


# GDAL 1.2.5 - Overview of Changes

gdalwarp Utility:
 - Added "proper" source and destination alpha support.

PCRaster Driver:
 - added write support, now consider ready for regular use.

MrSID Driver:
 - Initial support for writing to MrSID with encoding SDK.

GeoTIFF Driver:
 - Updated internal copy of libtiff to fix overview building ... really!
 - Fixed bug when writing south-up images.

## OGR 1.2.5 - Overview of Changes

OGRSpatialReference:
 - Added Bonne projection.

Docs:
 - Added OGR C++ API Tutorial (reading and writing).

PostGIS Driver:
 - Implemented SetFeature() and DeleteFeature() methods for in-place updates.

Oracle (OCI) Driver:
 - Fixed support for writing into Oracle 10g.
 - Fixed serious memory leak of geometries.
 - Fixed bug with 3D multipolygons.
 - Added support for selecting tables in the datasource name.


# GDAL 1.2.4 - Overview of Changes

gdalwarp:
  - Fixed some issues with only partially transformable regions.
  - Added Alpha mask generation support (-dstalpha switch).

HFA/Imagine Driver:
  - bug fix in histogram handling.
  - improved support for large colormaps.

Envi Driver:
  - Capture category names and colormaps when reading.

SAR CEOS Driver:
  - Added support for PALSAR/ALOS Polarimetric Datasets.

RadarSat 2 Driver:
  - New.  Reads RadarSat 2 Polarimetric datasets with a "product.xml" and
    imagery in TIFF files.

OGDI Driver:
  - Important bug fix for downsampled access.

GeoTIFF Driver:
  - Lots of libtiff upgrades, including some quite serious bug fixes.
  - Added better support for 16bit colormaps.
  - Write projection information even if we don't have a geotransform or GCPs.
  - Improved alpha support.
  - Generate graceful error message for BigTIFF files.

DODS Driver:
  - Almost completely reimplemented.   Uses chunk-by-chunk access.  Supports
    reading several bands from separate objects.  Some new limitations too.

NetCDF Driver:
  - Separated out a GMT NetCDF driver and a more generic but partially broken
    NetCDF driver (Radim).

JP2KAK Driver:
  - Added alpha support, including greyscale+alpha.

AirSAR Driver:
  - New, reads AirSAR Polarimetric Radar format.

## OGR 1.2.4 - Overview of Changes

epsg_tr.py:
  - Added escaping rules when generating PostGIS output.

tigerpoly.py:
  - Discard dangles and degenerate rings.

VRT Driver:
  - Fixed serious error in handling cleanup of VRT datasources, was often
   causing a crash.

SQLLite Driver:
  - Fixed substantial memory leaks.

MySQL Driver:
  - New readonly non-spatial MySQL driver implemented.

MITAB Driver:
  - Updated from upstream, several fixes.

TIGER/Line Driver:
  - Fixed serious bug with handling "full" records at end of .RT2 file.

OCI/Oracle Driver:
  - Added OCI_FID environment support to control FID selection.

OGRGeometry:
  - Added Centroid() implementation (from GEOS?)

# GDAL 1.2.3 - Overview of Changes

GeoTIFF Driver:
    - Fixed many missing compression codecs when built with the internal
      libtiff.
    - Modified driver metadata to only list available compression types.

DODS Driver:
    - Added support for OPeNDAP version after 3.4.x (use of opendap-config).

GRASS Driver:
    - Fixed support for building with grass57.

MrSID Driver:
    - Fixed support for MrSID Encoding SDK.

NITF Driver:
    - Fixed serious bug with non-square output files.


## OGR 1.2.3 - Overview of Changes

OGRSpatialReference:
    - Corrected memory leaks - OSRCleanup() cleans up temporary tables.
    - Fixed build problem with ogrct.cpp on Solaris.

TIGER Driver:
    - Improved generality of support for GDT files.

OGRGeometry:
    - Added getArea() method for rings, polygons and multipolygons.


# GDAL 1.2.2 - Overview of Changes

GRASS Driver:
    - Add Radim's version of the driver submitted by Radim.  This version
      uses GRASS 5.7 libraries directly instead of using libgrass.

DODS Driver:
    - Added support for spatial_ref, FlipX and FlipY .das info.

CPG Driver:
    - added new driver for Convair Polarimetric format.

HDF Driver:
    - Significant bugs fixed.

USGS DEM Driver:
    - Support writing UTM projected files.

PNG Driver:
    - Upgraded to libpng 1.2.6.

MrSID Driver:
    - Substantial performance improvements.
    - Support for DSDK 4.x
    - Support JPEG2000 files via MrSID SDK.

NITF Driver:
    - Support JPEG2000 compressed files (with Kakadu support)

ESRI BIL:
    - Support .clr color files.

VRT Driver:
    - Added support for describing raw files with VRTRawRasterBand.
    - Added support for virtual warped files with VRTWarpedRasterBand.

GeoTIFF Driver:
    - Fix support for 16bit image color tables.
    - Write ExtraSamples tag for files with more samples than expected
      in photometric interpretation.
    - External overviews now built for read-only files.

Erdas Imagine Driver:
    - Fixed support for compressed floating point layers.
    - Various other fixes for compatible with newer Imagine versions.
    - improved metadata handling.

gdal_merge.py:
    - sets projection on output file.

## OGR 1.2.2 - Overview of Changes

SQLite Driver:
    - New read/write driver implemented for SQLite databases.

CSV Driver:
    - New read/write driver implemented for comma separated value files.

S-57 Driver:
    - Substantial performance improvements.

ODBC Driver:
    - Arbitrary length field values now supported.

GEOS:
    - Integration a series of methods utilizing GEOS when available.  Note
      that Intersect() is still just an envelope comparison.

OGRSpatialReference:
    - Fixed Swiss Oblique Mercator support.

===========================================================================

# GDAL 1.2.1 - Overview of Changes

gdal_contour:
    - Now build and installed by default.

HDF4 Driver:
    - Added some degree of HDF-EOS support.  HDFEOS layer now part of GDAL.

DODS Driver:
    - Substantial fixes, support for flipped datasets.

HFA (Erdas Imagine) Driver:
    - Fixed bug with files between 2 and 4GB.
    - Capture statistics as metadata.

Erdas 7.x LAN/GIS Driver:
    - Newly implemented.

USGS DEM Driver:
    - Various fixes to creation support / CDED product.

NITF Driver:
    - Capture USE001 and STDIDC TREs as metadata.
    - Capture all sorts of header information as metadata.
    - Support geocentric corner coordinate specification.

MrSID Driver:
    - Support added for DSDK 4.0.x.

ECW Driver:
    - Added preliminary support for using 3.0 SDK for JPEG2000 support.
    - Fix oversampling assertion problem.

ArcInfo Binary Grids:
    - Added support for 0x01 and 0x20 block type.

## OGR 1.2.1 - Overview of Changes

OGRSpatialReference:
    - Various fixes related to prime meridians.

PostgreSQL/PostGIS Driver:
    - Added layer name laundering.
    - Launder names on by default.
    - Clean stale entries in geometry_columns table when creating a layer.
    - Support treating names views as layers.
    - Handle long command strings.

S57 Driver:
    - Fixed serious bugs with support for auto-applying update files.
    - Improvements to S57 writing support.

# GDAL 1.2.0 - Overview of Changes

Configuration:
    - Libtool used by default with Unix style builds.  Use --without-libtool
      to avoid this.
    - PROJ.4 can now be linked statically using --with-static-proj4.
    - Added --without-bsb option for those averse to legal risk.

DODS/OPeNDAP Driver:
    - Preliminary DODS (OPeNDAP) driver implemented (James Gallagher @ URI).

PCIDSK Driver:
    - PCIDSK read/write raster driver implemented (Andrey).

Erdas Imagine / HFA Driver:
    - Support recent Imagine versions (data dictionary changes).
    - Better logic to search for .rrd file locally.
    - Support creating files in the 2GB to 4GB size range.

GIF Driver:
    - Updated to libungif 4.1.0.
    - Various hacks to try and identify transparent colors better.

BMP Driver:
    - Handle 32bit BMPs properly.

HDF4 Driver:
    - Added proper support for multi-sample GR datasets.
    - Various fixes and improvements for specific product types.

GeoTIFF Driver:
    - Added PHOTOMETRIC option to control photometric interp of new files.

JPEG2000/Kakadu Driver:
    - Support reading/creating lossless 16bit files.
    - Updated to support Kakadu 4.1 library.

NITF Driver:
    - Implement support for IGEOLO="U" (MGRS/UTM) coordinates.
    - Added overview (as external GeoTIFF file) support.

MrSID Driver:
    - Support DSDK 4.2.x.

PNG Driver:
    - Support required byte swapping of 16bit PNG data.

FAST Driver:
    - lots of fixes, supports more datums and ellipsoids.

NetCDF Driver:
    - New driver implemented for netCDF support.
    - Pretty much tied to form of netCDF used in GMT for now.

VTerrain .bt Driver:
    - New driver for VTerrain .bt elevation format.

ECW Driver:
    - support supersampled reads efficiently.
    - special case for dataset level RasterIO() implemented for much better
      performance in some applications.

ESRI BIL (EHdr) Driver:
    - Support world files.

VRT Driver:
    - Implement filtering support.

GIO (Arc/Info Binary Grid via avgridio.dll):
   - Driver disabled ... to undependable.


Python:
    - Preliminary support for numarray in addition to numpy (Numeric).

Contouring:
    - New gdal_contour utility program implementing contour generation.
    - Underlying algorithm in gdal/alg.

Warping:
    - Improved support in GDALSuggestedWarpOutput() for "world" sized
      files that are only partially transformable.
    - Bicubic resampler improved.
    - What was gdalwarptest is now gdalwarp, and the old gdalwarp is now
      gdalwarpsimple.  The sophisticated warper is now the default.

Man Pages:
    - Man pages for GDAL utilities now being maintained and installed (Silke).

## OGR 1.2.0 - Overview of Changes

OGRSpatialReference:
   - Added methods for converting to/from GCTP representation.
   - Added HOM 2 points on centerline variant.

DODS (OPeNDAP) Driver:
   - Preliminary implementation.

TIGER/Line Driver:
   - Added support for GDT ASCII TIGER-like format.
   - Support TIGER/Line 2003 format.

S-57 Driver:
   - Preliminary export support implemented.
   - Support capture of FFPT (feature to feature) linkages.
   - Support capture of TOPI from VRPT.
   - Support capture of primitives as additional layers.

Shapefile Driver:
   - gdal/frmts/shapelib removed from GDAL source tree, now just a
     copy of required shapelib files are kept in gdal/ogr/ogrsf_frmts/shape.
   - Attempt identify polygons that are really multi-polygons and convert them
     into multi-polygons properly (Radim Blazek).
   - Create FID attribute in .dbf file if no attribute added by application.

GML Driver:
   - Lots of fixes and improvements for reading and writing.
   - Now writes a schema file by default.
   - Field types are set now when reading based on data found on first pass.
   - Added support for the various kinds of geometry collections.

DGN Driver:
   - Now using dgnlib 1.9 - this carries with it various new element types
     and some important bug fixes.

ODBC Driver:
   - New ODBC driver implemented.  Build by default on Windows, and buildable
     on Unix (with unixodbc).

VRT Driver:
   - New "virtual" OGR Datasource format implemented.
   - Configuration stored in XML control file.

Oracle (OCI) Driver:
   - support reading views.

OGR Core:
   - Added support for WKT EMPTY geometry objects (like "MULTIPOINT(EMPTY)").
   - Added DeleteFeature() method on OGRLayer class.

NTF Driver:
   - Support CHG_TYPE attribute for landline plus product.


# GDAL 1.1.9 - Overview of Changes

 o MrSID Driver: New for 1.1.9, read-only, includes good coordinate system
   support, and should be high performance.

 o ECW Driver: Now reads coordinate system information (but doesn't write).

 o HDF Driver: Added support for Hyperion Level 1, Aster Level 1A/1B/2, MODIS
   Level 1B(earth-view)/2/3, SeaWIFS Level 3.

 o L1B Driver: Now reads GCPs for georeferencing.

 o NITF Driver: Support for reading RPC, variety of bugs fixes for reading and
   writing.  Also some general RPC infrastructure added to GDAL.

 o JP2KAK Driver: Can be used with Kakadu 4.0.2 now.  Compatibility fixes
   for internal geotiff to improve compatibility with Mapping Science tools.
   Added palette support.

 o HFA (Imagine) Driver: Added read/write support for color table opacity.
   Added write support for large (spill) files.

 o "core" directory renamed to "gcore" to avoid confusing configure script.

 o Added support for GDAL_DATA environment variable to point to GDAL support
   data files (those in gdal/data directory).

 o Added GDALDataset::RasterIO() for more efficient reading of multiple bands
   in one request (in some cases anyways).

 o High performance warp api considered to be complete now, and substantially
   optimized.

 o gdal_merge.py: supported multiple bands, copying PCT.


## OGR 1.1.9 - Overview of Changes

 o Oracle Spatial: New generic read/write, and implemented highly optimized
   loading support.

 o Tiger driver: added support for TIGER/Line 2002 product.

 o GML driver:  now supports Xerces versions from 1.6 up to 2.3.  Lots of
   bugs fixes and improvements.   GML Geometry now in OGR core.

 o Improved support for translating to and from ESRI WKT, including a complete
   mapping between EPSG related ESRI datum names and OGR's expected names.

 o Improved support for alternate prime meridians in coordinate system code.

 o Shapefiles: Can write features with NULL geometry,

 o DGN: added 3d write support.

 o Implemented generic attribute indexing support (only used for shapefile
   at this point).  Use in SQL where clauses and ExecuteSQL().

 o WKT MULTIPOINT in/out formatting fixed.

 o Added SynToDisk() method on OGRDataset and OGRLayer.

 o Implemented "Web Coordinate Transformation Service" (ogr/wcts).

 o Implemented "in memory" format driver.

 o C API documented.


# GDAL 1.1.8 - Overview of Changes

 o Implemented HDF 4 read/write support. This includes HDF EOS reading.

 o Implemented Windows BMP read/write support.

 o Implemented NITF read/write support.

 o Implemented NOAA Polar Orbiter L1B format driver.

 o Implemented EOSAT FAST format driver.

 o Implemented a JasPer based JPEG2000 driver (several limitations).

 o Implemented a Kakadu based JPEG2000/GeoJP2(tm) driver (full featured, but
   Kakadu is not open source).

 o Implemented new 'gdalwarp' application for projection and GCP based image
   warping.  See gdal/alg for underlying algorithms.  Currently gdalwarp only
   supports 8 bit images and holds the whole source image in memory.

 o Implemented write support for ESRI ASCII Grids.

 o Lots of improvements to GeoTIFF driver.  Metadata writing, update of
   georeferencing, and support for writing PCS codes based on AUTHORITY fields
   in WKT.

 o Implemented support for uncompressed 1bit data in Erdas Imagine files,
   as well as generic metadata.

 o Fixed 0xFF compression support in the Arc/Info Binary Grid (AIG) driver.

 o Lots of improvements to BSB drive, including preliminary uncompressed
   output support, support for reading BSB 3.0 and GEO/NOS.

 o Lots of work on VRT format.

 o ECW: Fixed bug with reading a more than full resolution.

 o Envisat driver now supports AATSR TOA and MERIS data.

 o Fixes for nodata support in GRASS driver.

 o Added the --version and --formats options to many utility programs.

 o gdal_translate:
    - added -projwin flag to copy a window specified in projection coordinates.
    - added the -a_srs option to assign a user supplied SRS to output file.
    - translation with subsetting to any format now support (uses VRT inside).

 o Lots of metadata now attached to driver objects describing their
   capabilities.

 o Implemented GDALDestroyDriverManager() to ensure full memory cleanup of
   GDAL related resources.

 o Added a 'devinstall' target on Windows to support easy installation of
   include files and stub libraries on Windows.  Also many other improvements
   to Windows build.  Most options can be easily turned on and off from the
   nmake.opt file now.


## OGR 1.1.8 - Overview of Changes

 o Implemented support for writing 2D DGN files.   Added support for MSLINK
   and Text values available as attributes.

 o Implemented FMEObjects based read driver.

 o Implemented ExecuteSQL() method on OGRDataSource.  Generic code supports
   fairly full featured SELECT statements.

 o Various fixes to 3D shapefile support.

 o Fixes to binary representation for 2.5D geometries.  Fixed MULTIPOINT WKT
   geometry representation.

 o Upgraded OGRSpatialReference.importFromEPSG() to use the new EPSG 6.2.2
   tables instead of the old EPSG 4.x tables.

 o Many fixes to PostGIS driver, including special creation options for
   "laundering" field names to save tokens.

 o Many improvements to standards conformance of OGRSpatialReference WKT
   representation to the OGC Coordinate Transformations specification.  Still
   some quirks related to prime meridians and coordinate systems with units
   other than degrees.

 o Implemented support for Meridian 2 NTF files in NTF driver.  Better
   support for GENERIC_CPOLY geometries.

 o  Added support for [NOT] IN, [NOT] LIKE and IS [NOT] NULL predicates in
   WHERE clauses.

 o Implemented a C API for accessing OGR.

 o Implemented support for building OLE DB Provider with Visual Studio.NET
   (many changes in ATL templates).  Lots of other OLE DB improvements for
   better MapGuide compatibility.


# GDAL 1.1.7 - Overview of Changes

 o Add XPM (X11 Pixmap) format.

 o Added rough ENVI raster format read support.

 o Added --version support (and supporting GDALVersionInfo() function).

 o Special hooks for getting raw record data from sar ceos files and Envisat
   via the metadata api.

 o Upgraded TIFF/GeoTIFF support to CVS version ... includes new extension
   API and removes need for private libtiff include files entirely.

 o gdal_translate now has scaling option (-scale).

 o Added utility documentation.

## OGR 1.1.7 - Overview of Changes

 o Added Arc/Info binary coverage format read support.

 o Added ogrtindex for building MapServer compatible OGR tile indexes.

 o Added default implementation of GetFeature(fid) method on OGRLayer.

 o Shape driver now supports reading and creating free standing .dbf files
   for layers without geometry.

 o Added utility documentation.

 o Fixed major memory/file handle leak in SDTS access.

 o Added ADSK_GEOM_EXTENT support for OLE DB provider.

 o Ensure shapefiles written with correct polygon ring winding direction
   plus various other shapefile support fixes.

 o GML read/write working reasonable well, including use of .gfs files.


# GDAL 1.1.6 - Overview of Changes

 o Add > 2GB file support on Linux 2.4.

 o Implemented USGS DEM reading.

 o Implemented BSB Format (Nautical Chart Format) read support.

 o Preliminary implementation of Virtual Datasets (gdal/frmts/vrt).

 o Support for writing DTED files.

 o Some raw formats (i.e. PAux, HKV) support files larger than 2GB.

 o Add the AddBand() method on GDALDataset.

 o PAux: Added color table read support.

 o Various fixes to OGDI driver.

 o Stripped out the GDALProjDef related capabilities.  Superseded by
   OGRSpatialReference, and OGRCoordinateTransformation functionality.

 o Improved CEOS support, notable for ESA LANDSAT files, D-PAF ERS-1 and
   Telaviv ERS data.

 o geotiff: upgraded libtiff support to approximately libtiff 3.5.7.

 o DGN: Added support for complex shapes, shapes assembled from many elements.
   Various other improvements.


## OGR 1.1.6 - Overview of Changes

 o Fixed OGDI driver so that gltp urls with drive letters work properly on
   windows.

 o Many improvements to OLE DB provider during the process of making it
   compatible with the MapGuide (SDP) client.  These include implementing
   restrictions for schema rowsets, treating missing information like WKT
   coordinate systems as NULL fields, and setting ISLONG on geometry fields.
   Also made thread safe.

 o DGN: Threat SHAPE elements as polygons.  Set style information for text.
   Added 3D support for most elements.

 o Fixed bugs in WKT format for some OGR geometry types (i.e. multipoint).

 o Added support for morphing to/from ESRI WKT format for OGRSpatialReference.

 o NTF: Don't try to cache all the records from multiple files at once.

 o Added experimental XML SRS support ... not the final schema.  Added
   supporting "minixml" support to CPL.

 o PostGIS: Upgraded to PostGIS 0.6.  Added "soft transaction" semantics.
   Many create feature calls can now be part of one transaction.  Transactions
   are now a general OGR concept although only implemented for PostGIS.

 o Added transform() and transformTo() methods for reprojecting geometries and
   added user options for this in ogr2ogr.

 o Very preliminary GML read/write support.  Needs Xerces C++ XML parser for
   read support.

# GDAL 1.1.5 New Features

o AIGrid:
- Return nodata value.

o OGDI:
- Added format user documentation.
- Added Sub Dataset support.
- Utilize OGDI 3.1 style capabilities metadata.

o SAR_CEOS:
- Added support for Alaska SAR Toolbox naming convention.
- Read map projection record for corner GCPs.

o PNG Driver:
- read/write support for transparency via colortable and nodata value.

o Erdas Imagine (HFA) Driver:
- Added support for reading external large image files.
- Added support for uncompressed, but reduced precision blocks.

o GIF Driver:
- Added .wld world file support.
- Added transparency read support.
- Upgraded to libungif 4.x.

o JPEG Driver:
- Added .wld world file support.

o PAux Driver:
- Added limited gcp and projection read support.

o GeoTIFF Driver:
- Added specialized support for 1 bit files.
- Upgraded world file reading (added .wld files), use
GDALReadWorldFile().

o JDEM Driver is new (Japanese DEM format).

o FujiBAS Driver is new.

o ERMapper ECW Driver is new.

o GDAL Bridge: upgraded to include new entry points, like GCP access and
nodata api.

o gdal_translate: added the -not_strict option.

o GDALGetRandomRasterSample(): Return magnitude for random samples.

o Added use of CPL_CVSID macro in most source files. Running the RCS ident
command on any GDAL executable or shared library should now give a listing
of most object file versions from which it was built.

o Various improvements so that static builds will work under Cygwin.

o Various improvements so that builds can be done on MacOS X.

o Overviews: Implement AVERAGE_MAGPHASE option for complex image overviews.

o Added support for sub datasets to gdalinfo, core api and OGDI raster driver.

o The size of the GDAL cache can now be overridden with the GDAL_CACHEMAX
environment variable (measured in MB).

o Added Driver implementation tutorial to documentation.

o Added apps/gdaltindex.c - application for building tile indexed raster
datasets suitable for use with UMN MapServer.


## GDAL 1.1.5 Significant Bug Fixes

o SAR_CEOS:
- Don't try to get GCPs from scanlines with no prefix data.

o GeoTIFF:
- Fixed handling of RGBA band ordering on big endian systems.
- Fixed bugs in overview generation, especially when updating in place.

o gdal-config should work properly in all situations now.

o JPEG Driver: improved magic number tested to avoid ignoring some jpeg files.

o FITS Driver: lots of fixes and improvements.


## OGR 1.1.5 New Features

o Implemented support for attribute query filters (SetAttributeFilter())
on OGRLayer, provided SWQ based implementation, plugged into all
drivers and added hooks to ogrinfo.

o MapInfo Driver:
- Added accelerated spatial query support.
- Upgraded to current MITAB source as of GDAL release date.

o S-57 Driver:
- Added support for applying S-57 updates automatically.

o SDTS Driver:
- Added ENID and SNID to line features.
- Return coordinate system in WKT instead of PROJ.4 format.

o Shapefile Driver:
- Auto determine shapefile type from first object written.
- Added good support for NULL shapes, and NULL attribute fields.
- Added support for .prj files (read and write).

o PostgreSQL Driver:
- Added PostGIS support.
- Pass attribute queries through to PostgreSQL.

o NTF Driver:
- Added support for GTYPE 5 geometries (a type of arc).
- Added support for GEOMETRY3D records in indexed (generic) datasets.

o TIGER/Line Driver:
- Added write support.
- Improved read support for TIGER 2000.

o OLE DB Provider:
- Added support for spatial queries via ICommand parameters.
- Added support for attribute queries by parsing out WHERE clause.
- In general substantial rework and extensions were made to make it
work with ESRI and AutoDesk clients.

o Added gdal/data/stateplane.txt - a test file with one line per state plane
zone for applications wanting to present options to users.

o Install ogrsf_frmts.a on install if building with OGR support enabled.

o Reports layer extents in ogrinfo.

## OGR 1.1.5 Significant Bug Fixes

o OGRSpatialReference:
- Fix bug with extracting linear units from EPSG derived definitions.
- Fixed bug translating LCC from EPSG to WKT (importFromEPSG()).
- Improved IsSame() test for GEOGCS now.
- Fixed crash if PROJECTION missing from PROJCS definition.

o S-57:
- Improve recovery from corrupt line geometries.
- Read objects as generic if the object class is not recognised.
- Handle LIST attributes as a string, instead of as a single int.

o NTF:
- Fixed circle conversion to polylines to close the circle properly.
- Upped MAX_LINK to 5000 to handle much more complex geometries.

o DGN:
- Don't include elements with the complex bit set in extents
computations.

o OGRGeometry:
- Fixed WKT format (import and export) for various container types.
- WKT import fixed for coordinates, and Z coordinates.
