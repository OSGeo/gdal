.. _rfc-46:

=======================================================================================
RFC 46: GDAL/OGR unification
=======================================================================================

Author: Even Rouault

Contact: even dot rouault at spatialys.com

Status: Adopted, implemented in GDAL 2.0

Summary
-------

In the 1.X series of GDAL/OGR, the GDAL/raster and OGR/vector sides are
quite different on some aspects even where there is no strong reason for
them to be different, particularly in the structure of drivers. This RFC
aims at unifying the OGR driver structure with the GDAL driver
structure. The main advantages of using the GDAL driver structure are :

-  metadata capabilities : description of driver, extensions, creation
   options, virtual IO capability ...
-  efficient driver identification and opening.

Similarly, OGR datasource and layer classes lack the metadata mechanisms
offered by the corresponding GDAL dataset and raster band classes.

Another aspect is that the separation between GDAL "datasets" and OGR
"datasources" is sometimes artificial. Various data containers can
accept both data types. The list of drivers that have a GDAL side and
OGR side is : SDTS, PDS, GRASS, KML, Spatialite/Rasterlite, GeoPackage
(raster side not yet implemented), PostGIS/PostGIS Raster, PDF, PCIDSK,
FileGDB (raster side not yet implemented). For applications that are
interested in both, this currently means to open the file twice with
different API. And for update mode, for file-based drivers, the updates
must be done sequentially to avoid opening a file twice simultaneously
in update mode and making conflicting changes.

Related RFCs
------------

There are a few related past RFCs that have never been adopted but
strongly relate to RFC 46 :

-  `RFC 10: OGR Open Parameters <./rfc10_ogropen>`__. All the
   functionality described in RFC 10 is included in RFC 46, mainly the
   GDALOpenEx() new API
-  `RFC 25: Fast Open <./rfc25_fast_open>`__. RFC 25 mentions avoiding
   to systematically listing the sibling files to the file being opened.
   This can now achieved in RFC 46 by lazy loading with
   GDALOpenInfo::GetSiblingFiles(). At least Identify() should not
   trigger GetSiblingFiles().
-  `RFC 36: Allow specification of intended on
   GDALOpen <./rfc36_open_by_drivername>`__. The new GDALOpenEx()
   accepts a list of a subset drivers that must be probed, as suggested
   by RFC36. The specification of the drivers on the command line of
   utilities could be easily done through a new option, but that's not in
   the scope of RFC 46.
-  `RFC 38: OGR Faster Open <./rfc38_ogr_faster_open>`__ is completely
   included in RFC 46 through the possibility of using
   Open(GDALOpenInfo*) in OGR drivers

Self-assigned development constraints
-------------------------------------

The changes should have moderate impact on the existing GDAL/OGR code
base, and particularly on most of its code, that lies in drivers.
Existing users of the GDAL/OGR API should also be moderately impacted by
the changes, if they do not need to use the new offered capabilities.

Core changes: summary
---------------------

-  OGRSFDriver extends GDALDriver.
-  Vector drivers can be implemented as GDALDriver.
-  OGRSFDriverRegistrar is a compatibility wrapper around
   GDALDriverManager for legacy OGRSFDriver.
-  OGRDataSource extends GDALDataSource.
-  GDALOpenEx() API is added to be able to open "mixed" datasets.
-  OGRLayer extends GDALMajorObject, thus adding metadata capability.
-  The methods of OGRDataSource related to layers are moved to
   GDALDataset, making it both a raster and vector capable container.
-  Performance improvements in GDALOpenInfo() mechanism.
-  New driver metadata item to describe open options (i.e. deprecate the
   use of configuration option).
-  New driver metadata item to describe layer creation options.

Core changes: details
---------------------

Drivers and driver registration
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

-  The OGRSFDriver now extends GDALDriver and is meant as being a legacy
   way of implementing a vector driver. It is kept mainbly because, in
   the current implementation, not all drivers have been migrated to
   being "pure" GDALDriver. The CopyDataSource() virtual method has been
   removed since no in-tree drivers implement it. The inheritance to
   GDALDriver make it possible to manage vector drivers by the
   GDALDriverManager, and to be able to attach metadata to them, to
   document driver long name, link to documentation, file extension,
   datasource creation options with the existing GDAL\_DMD\_\* metadata
   items.

-  Drivers directly inheriting from GDALDriver (to be opposed to those
   inheriting from OGRSFDriver) should : - declare SetMetadataItem(
   GDAL_DCAP_VECTOR, "YES" ). - implement pfnOpen() for dataset opening
   - optionally, implement pfnCreate() for dataset creation. For vector
   drivers, the nBands parameter of Create() is supposed to be passed to
   0. - optionally, implement pfnDelete() for dataset deletion

-  The *C* OGR Driver API will still work with drivers that have been
   converted as "pure" GDALDrivers (this is not true of the C++ OGR
   Driver API). For example OGR_Dr_GetName() calls
   GDALDriver::GetDescription(), OGR_Dr_CreateDatasource() calls
   GDALDriver::Create(), etc...

-  The C++ definition of GDALDriver is extended with the following
   function pointers so that it can work with legacy OGRSFDriver.

::

       /* For legacy OGR drivers */
       GDALDataset         *(*pfnOpenWithDriverArg)( GDALDriver*, GDALOpenInfo * );
       GDALDataset         *(*pfnCreateVectorOnly)( GDALDriver*,
                                                    const char * pszName,
                                                    char ** papszOptions );
       CPLErr              (*pfnDeleteDataSource)( GDALDriver*,
                                                    const char * pszName );

::

   They are used by GDALOpenEx(), GDALDriver::Create() and GDALDriver::Delete()
   if the pfnOpen, pfnCreate or pfnDelete pointers are NULL. The OGRSFDriverRegistrar
   class has an implementation of those function pointers that calls the
   legacy C++ OGRSFDriver::Open(), OGRSFDriver::CreateDataSource() and
   OGRSFDriver::DeleteDataSource() virtual methods.

-  GDALDriver::Create() can accept nBands == 0 for a vector capable
   driver.

-  GDALDriver::DefaultCreateCopy() can accept a dataset with 0 bands for
   a vector capable driver, and if the output dataset has layer creation
   capability and the source dataset has layers, it copies the layers
   from the source dataset into the target dataset.

-  GDALDriver::Identify() now iterates over all kinds of drivers. It has
   been modified to do a first pass on drivers that have an
   implementation of Identify(). If no match is found, it does a second
   pass on all drivers and use the potentially slower Open() as the
   identification method.

-  Related to the above point, the implementations of
   GDALDriver::pfnIdentify function pointer used to return a boolean
   value to indicate if the passed GDALOpenInfo was a match for the
   driver. For some drivers, this was too restrictive so that they were
   able to implement Identify(). For example where the detection logic
   can return "yes, I definitely recognize that file", "no, it is not
   for me" or "I have not enough elements in GDALOpenInfo to be able to
   tell". That last state can now be advertized with a negative return
   value.

-  The OGRSFDriverRegistrar is trimmed down to be mostly a wrapper
   around GDALDriverManager. In particular, it does not contain any
   longer a list of drivers. The Open(), OpenShared(),
   ReleaseDataSource(), DeregisterDriver() and AutoLoadDrivers() methods
   are removed from the class. This change can have impact on C++ code.
   A few adaptations in OGR utilities have been done to accommodate for
   those changes. The RegisterDriver() API has been kept for legacy OGR
   drivers and it automatically sets SetMetadataItem( GDAL_DCAP_VECTOR,
   "YES" ). The GetDriverCount(), GetDriver() and GetDriverByName()
   methods delegate to GDALDriverManager and make sure to only take into
   account drivers that have the GDAL_DCAP_VECTOR capability. In the
   case a driver has the same name as GDAL and OGR driver, the OGR
   variant is internally prefixed with OGR\_, and GetDriverByName() will
   first try the OGR\_ variant. The GetOpenDSCount() and GetOpenDS()
   have now a dummy implementation returning 0/NULL. For reference,
   neither MapServer nor QGIS use those functions.

-  OGRRegisterAll() is now an alias of GDALAllRegister(). The past
   OGRRegisterAll() is now renamed OGRRegisterAllInternal() and called
   by GDALAllRegister(). So, GDALAllRegister() and OGRRegisterAll() are
   now equivalent and register all drivers.

-  GDALDriverManager has received a few changes :

   -  use of a map from driver name to driver object to speed-up
      GetDriverByName()
   -  accept OGR_SKIP and OGR_DRIVER_PATH configuration options for
      backward compatibility.
   -  The recommended separator for driver names in GDAL_SKIP is now
      comma instead of space (similarly to what OGR_SKIP does). This is
      to make it possible to define OGR driver names in GDAL_SKIP that
      have spaces in their names like "ESRI Shapefile" or "MapInfo
      File". If there is no comma in the GDAL_SKIP value, then space
      separator is assumed (backward compatibility).
   -  removal of GetHome()/SetHome() methods whose purpose seemed to
      define an alternate path for the search directory of plugins.
      Those methods only existed at the C++ level, and are redundant
      with GDAL_DRIVER_PATH configuration option

-  Raster-capable drivers should declare SetMetadataItem(
   GDAL_DCAP_RASTER, "YES" ). All in-tree GDAL drivers have been patched
   to declare it. But the registration code detects if a driver does not
   declare any of GDAL_DCAP_RASTER nor GDAL_DCAP_VECTOR, in which case
   it declares GDAL_DCAP_RASTER on behalf of the un-patched driver, with
   a debug message inviting to explicitly set it.

-  New metadata items :

   -  GDAL_DCAP_RASTER=YES / GDAL_DCAP_VECTOR=YES at driver level. To
      declare that a driver has raster/vector capabilities. A driver can
      declare both.
   -  GDAL_DMD_EXTENSIONS (with a final S) at driver level. This is a
      small evolution of GDAL_DMD_EXTENSION where one can specify
      several extensions in the value string. The extensions are
      space-separated. For example "shp dbf", "tab mif mid", etc... For
      ease of use, GDALDriver::SetMetadataItem(GDAL_DMD_EXTENSION) also
      sets the passed value as GDAL_DMD_EXTENSIONS, if it is not already
      set. So new code can always use GDAL_DMD_EXTENSIONS.
   -  GDAL_DMD_OPENOPTIONLIST at driver level. The value of this item is
      an XML snippet with a format similar to creation options.
      GDALOpenEx(), once it has identified with Identify() that a driver
      accepts the file, will validate the passed open option list with
      the authorized open option list. Below an example of such an
      authorized open option list in the S57 driver

::

   <OpenOptionList>
     <Option name="UPDATES" type="string-select"
       description="Should update files be incorporated into the base data on the fly" default="APPLY">
       <Value>APPLY</Value>
       <Value>IGNORE</Value>
     </Option>
     <Option name="SPLIT_MULTIPOINT" type="boolean"
       description="Should multipoint soundings be split into many single point "
                   "sounding features" default="NO" />
     <Option name="ADD_SOUNDG_DEPTH" type="boolean"
       description="Should a DEPTH attribute be added on SOUNDG features and "
                   "assign the depth of the sounding" default="NO" />
     <Option name="RETURN_PRIMITIVES" type="boolean"
       description="Should all the low level geometry primitives be returned as "
                   "special IsolatedNode, ConnectedNode, Edge and Face layers" default="NO" />
     <Option name="PRESERVE_EMPTY_NUMBERS" type="boolean"
       description="If enabled, numeric attributes assigned an empty string as a "
                   "value will be preserved as a special numeric value" default="NO" />
     <Option name="LNAM_REFS" type="boolean"
       description="Should LNAM and LNAM_REFS fields be attached to features "
                   "capturing the feature to feature relationships in the FFPT "
                   "group of the S-57 file" default="YES" />
     <Option name="RETURN_LINKAGES" type="boolean"
       description="Should additional attributes relating features to their underlying "
                   "geometric primtives be attached" default="NO" />
     <Option name="RECODE_BY_DSSI" type="boolean"
       description="Should attribute values be recoded to UTF-8 from the character "
                   "encoding specified in the S57 DSSI record." default="NO" />
   </OpenOptionList>

::

   - GDAL_DS_LAYER_CREATIONOPTIONLIST at dataset level. But can also be set at
     driver level because, in practice, layer creation options do not depend on the
     dataset instance.
     The value of this item is an XML snippet with a format similar to dataset creation
     options. 
     If specified, the passed creation options to CreateLayer() are validated
     against that authorized creation option list.
     Below an example of such an authorized open option list in the Shapefile driver.

::

   <LayerCreationOptionList>
     <Option name="SHPT" type="string-select" description="type of shape" default="automatically detected">
       <Value>POINT</Value>
       <Value>ARC</Value>
       <Value>POLYGON</Value>
       <Value>MULTIPOINT</Value>
       <Value>POINTZ</Value>
       <Value>ARCZ</Value>
       <Value>POLYGONZ</Value>
       <Value>MULTIPOINTZ</Value>
       <Value>NONE</Value>
       <Value>NULL</Value>
     </Option>
     <Option name="2GB_LIMIT" type="boolean" description="Restrict .shp and .dbf to 2GB" default="NO" />
     <Option name="ENCODING" type="string" description="DBF encoding" default="LDID/87" />
     <Option name="RESIZE" type="boolean" description="To resize fields to their optimal size." default="NO" />
   </LayerCreationOptionList>

.. _datasets--datasources:

Datasets / Datasources
~~~~~~~~~~~~~~~~~~~~~~

-  The main methods from OGRDataSource have been moved to GDALDataset :

::

       virtual int         GetLayerCount() { return 0; }
       virtual OGRLayer    *GetLayer(int) { return NULL; }
       virtual OGRLayer    *GetLayerByName(const char *);
       virtual OGRErr      DeleteLayer(int);

       virtual int         TestCapability( const char * ) { return FALSE; }

       virtual OGRLayer   *CreateLayer( const char *pszName, 
                                        OGRSpatialReference *poSpatialRef = NULL,
                                        OGRwkbGeometryType eGType = wkbUnknown,
                                        char ** papszOptions = NULL );
       virtual OGRLayer   *CopyLayer( OGRLayer *poSrcLayer, 
                                      const char *pszNewName, 
                                      char **papszOptions = NULL );

       virtual OGRStyleTable *GetStyleTable();
       virtual void        SetStyleTableDirectly( OGRStyleTable *poStyleTable );
                               
       virtual void        SetStyleTable(OGRStyleTable *poStyleTable);

       virtual OGRLayer *  ExecuteSQL( const char *pszStatement,
                                       OGRGeometry *poSpatialFilter,
                                       const char *pszDialect );
       virtual void        ReleaseResultSet( OGRLayer * poResultsSet );

       int                 GetRefCount() const;
       int                 GetSummaryRefCount() const;
       OGRErr              Release();

::

   The following matching C API is available :

::

   int    CPL_DLL GDALDatasetGetLayerCount( GDALDatasetH );
   OGRLayerH CPL_DLL GDALDatasetGetLayer( GDALDatasetH, int );
   OGRLayerH CPL_DLL GDALDatasetGetLayerByName( GDALDatasetH, const char * );
   OGRErr    CPL_DLL GDALDatasetDeleteLayer( GDALDatasetH, int );
   OGRLayerH CPL_DLL GDALDatasetCreateLayer( GDALDatasetH, const char *, 
                                         OGRSpatialReferenceH, OGRwkbGeometryType,
                                         char ** );
   OGRLayerH CPL_DLL GDALDatasetCopyLayer( GDALDatasetH, OGRLayerH, const char *,
                                           char ** );
   int    CPL_DLL GDALDatasetTestCapability( GDALDatasetH, const char * );
   OGRLayerH CPL_DLL GDALDatasetExecuteSQL( GDALDatasetH, const char *,
                                        OGRGeometryH, const char * );
   void   CPL_DLL GDALDatasetReleaseResultSet( GDALDatasetH, OGRLayerH );
   OGRStyleTableH CPL_DLL GDALDatasetGetStyleTable( GDALDatasetH );
   void   CPL_DLL GDALDatasetSetStyleTableDirectly( GDALDatasetH, OGRStyleTableH );
   void   CPL_DLL GDALDatasetSetStyleTable( GDALDatasetH, OGRStyleTableH );

::

   OGRDataSource definition is now reduced to :

::

   class CPL_DLL OGRDataSource : public GDALDataset
   {
   public:
                           OGRDataSource();

       virtual const char  *GetName() = 0;

       static void         DestroyDataSource( OGRDataSource * );
   };

::

   The existing OGR_DS_* API is preserved. The implementation of those functions
   casts the OGRDataSourceH opaque pointer to GDALDataset*, so it is possible to
   consider GDALDatasetH and OGRDataSourceH as equivalent from the C API point of
   view. Note that it is not true at the C++ level !

-  OGRDataSource::SyncToDisk() has been removed. The equivalent
   functionality should be implemented in existing FlushCache().
   GDALDataset::FlushCache() nows does the job of the previous generic
   implementation of OGRDataSource::SyncToDisk(), i.e. iterate over all
   layers and call SyncToDisk() on them.

-  GDALDataset has now a protected ICreateLayer() method.

::

       virtual OGRLayer   *ICreateLayer( const char *pszName, 
                                        OGRSpatialReference *poSpatialRef = NULL,
                                        OGRwkbGeometryType eGType = wkbUnknown,
                                        char ** papszOptions = NULL );

::

   This method is what used to be CreateLayer(), i.e. that drivers should
   rename their specialized CreateLayer() implementations as ICreateLayer().
   CreateLayer() is kept at GDALDataset level, but its implementation does a
   prior validation of passed creation options against an optional authorized
   creation option list (GDAL_DS_LAYER_CREATIONOPTIONLIST), before calling
   ICreateLayer() (this is similar to RasterIO() / IRasterIO() )
   A global pass on all in-tree OGR drivers has been made to rename CreateLayer()
   as ICreateLayer(). 

-  GDALOpenEx() is added to be able to open raster-only, vector-only, or
   raster-vector datasets. It accepts read-only/update mode,
   shared/non-shared mode. A list of potential candidate drivers can be
   passed. If NULL, all drivers are probed. A list of open options
   (NAME=VALUE syntax) can be passed. If the list of sibling files has
   already been established, it can also be passed. Otherwise
   GDALOpenInfo will establish it.

::

   GDALDatasetH CPL_STDCALL GDALOpenEx( const char* pszFilename,
                                    unsigned int nOpenFlags,
                                    const char* const* papszAllowedDrivers,
                                    const char* const* papszOpenOptions,
                                    const char* const* papszSiblingFiles );

::

   The nOpenFlags argument is a 'or-able' combination of the following values :

::

   /* Note: we define GDAL_OF_READONLY and GDAL_OF_UPDATE to be on purpose */
   /* equals to GA_ReadOnly and GA_Update */

   /** Open in read-only mode. */
   #define     GDAL_OF_READONLY        0x00
   /** Open in update mode. */
   #define     GDAL_OF_UPDATE          0x01

   /** Allow raster and vector drivers. */
   #define     GDAL_OF_ALL             0x00

   /** Allow raster drivers. */
   #define     GDAL_OF_RASTER          0x02
   /** Allow vector drivers. */
   #define     GDAL_OF_VECTOR          0x04
   /* Some space for GDAL 3.0 new types ;-) */
   /*#define     GDAL_OF_OTHER_KIND1   0x08 */
   /*#define     GDAL_OF_OTHER_KIND2   0x10 */

   /** Open in shared mode. */
   #define     GDAL_OF_SHARED          0x20

   /** Emit error message in case of failed open. */
   #define     GDAL_OF_VERBOSE_ERROR   0x40

::

   The existing GDALOpen(), GDALOpenShared(), OGROpen(), OGROpenShared(),
   OGR_Dr_Open() are just wrappers of GDALOpenEx() with appropriate open flags.
   From the user point of view, their behavior is identical to the existing one,
   i.e. GDALOpen() family will only returns datasets of drivers with declared raster
   capabilities, and similarly with OGROpen() family with vector.

-  GDALOpenInfo class. The following changes are done :

   -  the second argument of the constructor is now nOpenFlags instead
      of GDALAccess, with same semantics as GDALOpenEx(). GDALOpenInfo
      uses the read-only/update bit to "compute" the eAccess flag that
      is heavily used in existing drivers. Drivers with both raster and
      vector capabilities can use the GDAL_OF_VECTOR/GDAL_OF_RASTER bits
      to determine the intent of the caller. For example if a caller
      opens with GDAL_OF_RASTER only and the dataset only contains
      vector data, the driver might decide to not open the dataset (if
      it is a read-only driver. If it is a driver with update
      capability, it should do that only if the opening is done in
      read-only mode).
   -  the open options passed to GDALOpenEx() are stored into a
      papszOpenOptions member of GDALOpenInfo, so that drivers can use
      them.
   -  the "FILE\* fp" member is transformed into "VSILFILE\* fpL". This
      change is motivated by the fact that most popular drivers now use
      the VSI Virtual File API, so they can now directly use the fpL
      member instead of re-opening again the file. A global pass on all
      in-tree GDAL drivers that used fp has been made.
   -  A VSIStatExL() was done previously to determine the nature of the
      file passed. Now, we optimistically begin with a VSIFOpenL(),
      assuming that in most use cases the passed filename is a file. If
      the opening fails, VSIStatExL() is done to determine the nature of
      the filename.
   -  If the requested access mode is update, the opening of the file
      with VSIFOpenL() is done with "rb+" permissions to be directly
      usable.
   -  The papszSiblingFiles member is now private. It is accessed by a
      GetSiblingFiles() method that does the ReadDir() on demand. This
      can speed up the Identify() method that generally does not require
      to know sibling files.
   -  A new method, TryToIngest(), is added to read more than the first
      1024 bytes of a file. This is useful for a few vector drivers,
      like GML or NAS, that must fetch a bit more bytes to be able to
      identify the file.

Layer
~~~~~

-  OGRLayer extends GDALMajorObject. Drivers can now define layer
   metadata items that can be retrieved with the usual
   GetMetadata()/GetMetadateItem() API.

-  The GetInfo() method has been removed. It has never been implemented
   in any in-tree drivers and has been deprecated for a long time.

Other
~~~~~

-  The deprecated and unused GDALProjDefH and GDALOptionDefinition types
   have been removed from gdal.h

-  GDALGeneralCmdLineProcessor() now interprets the nOptions
   (combination of GDAL_OF_RASTER and GDAL_OF_RASTER) argument as the
   type of drivers that should be displayed with the --formats option.
   If set to 0, GDAL_OF_RASTER is assumed.

-  the --formats option of GDAL utilities outputs whether drivers have
   raster and/or vector capabilities

-  the --format option of GDAL utilities outputs GDAL_DMD_EXTENSIONS,
   GDAL_DMD_OPENOPTIONLIST, GDAL_DS_LAYER_CREATIONOPTIONLIST.

-  OGRGeneralCmdLineProcessor() use GDALGeneralCmdLineProcessor()
   implementation, restricting --formats to vector capable drivers.

Changes in drivers
------------------

-  OGR PCIDSK driver has been merged into GDAL PCIDSK driver.

-  OGR PDF driver has been merged into GDAL PDF driver.

-  A global pass has been made to in-tree OGR drivers that have to open
   a file to determine if they recognize it. They have been converted to
   GDALDriver to accept a GDALOpenInfo argument and they now use its
   pabyHeader field to examine the first bytes of files. The number of
   system calls realated to file access (open/stat), in order to
   determine that a file is not recognized by any OGR driver, has now
   dropped from 46 in GDAL 1.11 to 1. The converted drivers are :
   AeronavFAA, ArcGEN, AVCBin, AVCE00, BNA, CSV, DGN, EDIGEO, ESRI
   Shapefile, GeoJSON, GeoRSS, GML, GPKG, GPSBabel, GPX, GTM, HTF, ILI1,
   ILI2, KML, LIBKML, MapInfo File, MySQL, NAS, NTF, OpenAIR, OSM, PDS,
   REC, S57, SDTS, SEGUKOOA, SEGY, SOSI, SQLite, SUA, SVG, TIGER, VFK,
   VRT, WFS

-  Long driver description is set for most OGR drivers.

-  All classes deriving from OGRLayer have been modified to call
   SetDescription() with the value of
   GetName()/poFeatureDefn->GetName(). test_ogrsf tests that it is
   properly set.

-  Following drivers are kept as OGRSFDriver, but their Open() method
   does early extension/prefix testing to avoid datasource object to be
   instantiated : CartoDB, CouchDB, DXF, EDIGEO, GeoConcept, GFT, GME,
   IDRISI, OGDI, PCIDSK, PG, XPlane.

-  Identify() has been implemented for CSV, DGN, DXF, EDIGEO, GeoJSON,
   GML, KML, LIBKML, MapInfo File, NAS, OpenFileGDB, OSM, S57, Shape,
   SQLite, VFK, VRT.

-  GDAL_DMD_EXTENSION/GDAL_DMD_EXTENSIONS set for following drivers:
   AVCE00, BNA, CSV, DGN, DWG, DXF, EDIGEO, FileGDB, Geoconcept,
   GeoJSON, Geomedia, GML, GMT, GPKG, GPX, GPSTrackMaker, IDRISI Vector,
   Interlis 1, Interlis 2, KML, LIBKML, MDB, MapInfo File, NAS, ODS,
   OpenFileGDB, OSM, PGDump, PGeo, REC, S57, ESRI Shapefile, SQLite,
   SVG, WaSP, XLS, XLSX, XPlane.

-  Document dataset and layer creation options of BNA, DGN, FileGDB,
   GeoConccept, GeoJSON, GeoRSS, GML, GPKG, KML, LIBKML, PG, PGDump and
   ESRI Shapefile drivers as GDAL_DMD_CREATIONOPTIONLIST /
   GDAL_DS_LAYER_CREATIONOPTIONLIST.

-  Add open options AAIGRID, PDF, S57 and ESRI Shapefile drivers.

-  GetFileList() implemented in OpenFileGDB, Shapefile and OGR VRT
   drivers.

-  Rename datasource SyncToDisk() as FlushCache() for LIBKML, OCI, ODS,
   XLSX drivers.

-  Use poOpenInfo->fpL to avoid useless file re-opening in GTiff, PNG,
   JPEG, GIF, VRT, NITF, DTED.

-  HTTP driver: declared as GDAL_DCAP_RASTER and GDAL_DCAP_VECTOR
   driver.

-  RIK: implement Identify()

-  Note: the compilation and good working of the following OGR drivers
   (mostly proprietary) could not be tested: ArcObjects, DWG, DODS, SDE,
   FME, GRASS, IDB, OCI, MSSQLSpatial(compilation OK, but not runtime
   tested)

Changes in utilities
--------------------

-  gdalinfo accepts a -oo option to define open options
-  ogrinfo accepts a -oo option to define open options
-  ogr2ogr accepts a -oo option to define input dataset open options,
   and -doo to define destination dataset open options

Changes in SWIG bindings
------------------------

-  Python and Java bindings:

   -  add new GDALDataset methods taken from OGRDataSource :
      CreateLayer(), CopyLayer(), DeleteLayer(), GetLayerCount(),
      GetLayerByIndex(), GetLayerByName(), TestCapability(),
      ExecuteSQL(), ReleaseResultSet(), GetStyleTable() and
      SetStyleTable().
   -  make OGR Driver, DataSource and Layer objects derive from
      MajorObject

-  Perl and CSharp: make sure that it still compiles but some work would
   have to be done by their mainteners to be able to use the new
   capabilities

Potential changes that are *NOT* included in this RFC
-----------------------------------------------------

"Natural" evolutions of current RFC :

-  Unifying the GDAL MEM and OGR Memory drivers.
-  Unifying the GDAL VRT and OGR VRT drivers.

Further unification steps :

-  Source tree changes to move OGR drivers from ogr/ogrsf_frmts/ to
   frmts/ , to move ogr/ogrsf_frmts/generic/\* to gcore/\*, etc...
-  Documentation unification (pages with list of drivers, etc...)
-  Renaming to remove traces of OGR namespace : OGRLayer -> GDALLayer,
   etc...
-  Kill --without-ogr compilation option ? It has been preserved in a
   working state even if it embeds now ogr/ogrsf_frmts/generic and
   ogr/ogrsf_frmts/mitab for conveniency.
-  Unification of some utilities : "gdal info XXX", "gdal convert XXX"
   that would work on all kind of datasets.

Backward compatibility
----------------------

GDALDriverManager::GetDriverCount(), GetDriver() now returns OGR
drivers, as well as GDAL drivers

The reference counting in GDAL datasets and GDAL 1.X OGR datasources was
a bit different. It starts at 1 for GDAL datasets, and started at 0 for
OGR datasources. Now that OGRDataSource is basically a GDALDataset, it
starts at 1 for both cases. Hopefully there are very few users of the
OGR_DS_GetRefCount() API. If it was deemed necessary we could restore
the previous behavior at the C API, but that would not be possible at
the C++ level. For reference, neither MapServer nor QGIS use
OGR_DS_GetRefCount().

Documentation
-------------

A pass should be made on the documentation to check that all new methods
are properly documented. The OGR general documentation (especially C++
API Read/Write tutorial, Driver implementation tutorial and OGR
architecture) should be updated to reflect the changes.

Testing
-------

Very few changes have been made so that the existing autotest suite
still passes. Additions have been made to test the GDALOpenEx() API and
the methods "imported" from OGRDataSource into GDALDataset.

Version numbering
-----------------

Although the above describes changes should have very few impact on
existing applications of the C API, some behavior changes, C++ level
changes and the conceptual changes are thought to deserve a 2.0 version
number.

Implementation
--------------

Implementation will be done by Even Rouault.

The proposed implementation lies in the "unification" branch of the
`https://github.com/rouault/gdal2/tree/unification <https://github.com/rouault/gdal2/tree/unification>`__
repository.

The list of changes :
`https://github.com/rouault/gdal2/compare/unification <https://github.com/rouault/gdal2/compare/unification>`__

Voting history
--------------

+1 from JukkaR, FrankW, DanielM, TamasS and EvenR.
