.. _rfc-59.1:

=======================================================================================
RFC 59.1 : GDAL/OGR utilities as a library
=======================================================================================

Authors: Faza Mahamood, Even Rouault

Contact: fazamhd at gmail dot com, even.rouault at spatialys.com

Status: Adopted, implemented

Implementation version: 2.1

Summary
-------

This RFC defines how to expose GDAL/OGR C/C++ utilities as C callable
functions. The utility code is modified to call the new function. This
RFC gives a general frame and principles, demonstrated with a few
utilities, but aimed at being extended with other utilities.

Rationale
---------

There is a need for calling GDAL utilities from code without involving
system calls, to be able to work on in-memory datasets and use
progress/cancellation callback functions.

Changes
-------

A public header file gdal_utils.h is created which contains the public
declarations of GDAL utilities. The current header(still in progress)
can be found
`here <https://github.com/rouault/gdal2/blob/rfc59.1/gdal/apps/gdal_utils.h>`__.

Each utility has a function (XXXXOptionsNew) to create an option
structure from arguments specified as an array of strings. This function
also accepts as argument an extra semi-private structure used to
cooperate with the code of the command line utility itself.

For GDALInfo():

::

   /*! Options for GDALInfo(). Opaque type */
   typedef struct GDALInfoOptions GDALInfoOptions;
   typedef struct GDALInfoOptionsForBinary GDALInfoOptionsForBinary;

   /**
    * Allocates a GDALInfoOptions struct.
    *
    * @param papszArgv NULL terminated list of options (potentially including filename and open options too)
    *                  The accepted options are the one of the gdalinfo utility.
    * @param psOptionsForBinary (output) may be NULL (and should generally be NULL),
    *                           otherwise (gdalinfo_bin.cpp use case) must be allocated with
    *                           GDALInfoOptionsForBinaryNew() prior to this function. Will be
    *                           filled with potentially present filename, open options, subdataset number...
    * @return pointer to the allocated GDALInfoOptions struct.
    *
    * @since GDAL 2.1
    */
   GDALInfoOptions CPL_DLL *GDALInfoOptionsNew(char** papszArgv, GDALInfoOptionsForBinary* psOptionsForBinary);

   void CPL_DLL GDALInfoOptionsFree( GDALInfoOptions *psOptions );

   /**
    * Lists various information about a GDAL supported raster dataset.
    *
    * GDALInfoOptions* must be allocated and freed with GDALInfoOptionsNew()
    * and GDALInfoOptionsFree() respectively.
    *
    * @param hDataset the dataset handle.
    * @param psOptions the options structure returned by GDALInfoOptionsNew() or NULL.
    * @return string corresponding to the information about the raster dataset.
    * It must be freed using CPLFree().
    *
    * @since GDAL 2.1
    */
   char CPL_DLL *GDALInfo( GDALDatasetH hDataset, const GDALInfoOptions *psOptions );

Similarly for GDALTranslate():

::

   /*! Options for GDALTranslate(). Opaque type */
   typedef struct GDALTranslateOptions GDALTranslateOptions;
   typedef struct GDALTranslateOptionsForBinary GDALTranslateOptionsForBinary;

   GDALTranslateOptions CPL_DLL *GDALTranslateOptionsNew(char** papszArgv,
                                                         GDALTranslateOptionsForBinary* psOptionsForBinary);

   void CPL_DLL GDALTranslateOptionsFree( GDALTranslateOptions *psOptions );

   void CPL_DLL GDALTranslateOptionsSetProgress( GDALTranslateOptions *psOptions,
                                                 GDALProgressFunc pfnProgress,
                                                 void *pProgressData );

   GDALDatasetH CPL_DLL GDALTranslate(const char *pszDestFilename,
                                      GDALDatasetH hSrcDataset,
                                      const GDALTranslateOptions *psOptions,
                                      int *pbUsageError);

Similarly for GDALWarp():

::

   /*! Options for GDALWarp(). Opaque type */
   typedef struct GDALWarpAppOptions GDALWarpAppOptions;

   typedef struct GDALWarpAppOptionsForBinary GDALWarpAppOptionsForBinary;

   GDALWarpAppOptions CPL_DLL *GDALWarpAppOptionsNew(char** papszArgv,
                                                         GDALWarpAppOptionsForBinary* psOptionsForBinary);

   void CPL_DLL GDALWarpAppOptionsFree( GDALWarpAppOptions *psOptions );

   void CPL_DLL GDALWarpAppOptionsSetProgress( GDALWarpAppOptions *psOptions,
                                                 GDALProgressFunc pfnProgress,
                                                 void *pProgressData );
   void CPL_DLL GDALWarpAppOptionsSetWarpOption( GDALWarpAppOptions *psOptions,
                                                 const char* pszKey,
                                                 const char* pszValue );

   GDALDatasetH CPL_DLL GDALWarp( const char *pszDest, GDALDatasetH hDstDS, int nSrcCount,
                                  GDALDatasetH *pahSrcDS,
                                  const GDALWarpAppOptions *psOptions, int *pbUsageError );

Similarly for GDALVectorTranslate() (equivalent of ogr2ogr):

::

   /*! Options for GDALVectorTranslate(). Opaque type */
   typedef struct GDALVectorTranslateOptions GDALVectorTranslateOptions;

   typedef struct GDALVectorTranslateOptionsForBinary GDALVectorTranslateOptionsForBinary;

   GDALVectorTranslateOptions CPL_DLL *GDALVectorTranslateOptionsNew(char** papszArgv,
                                                         GDALVectorTranslateOptionsForBinary* psOptionsForBinary);

   void CPL_DLL GDALVectorTranslateOptionsFree( GDALVectorTranslateOptions *psOptions );

   void CPL_DLL GDALVectorTranslateOptionsSetProgress( GDALVectorTranslateOptions *psOptions,
                                                 GDALProgressFunc pfnProgress,
                                                 void *pProgressData );

   GDALDatasetH CPL_DLL GDALVectorTranslate( const char *pszDest, GDALDatasetH hDstDS, int nSrcCount,
                                  GDALDatasetH *pahSrcDS,
                                  const GDALVectorTranslateOptions *psOptions, int *pbUsageError );

For other utilities, see
`gdal_utils.h <http://svn.osgeo.org/gdal/trunk/gdal/apps/gdal_utils.h>`__

SWIG bindings (Python / Java / C# / Perl) changes
-------------------------------------------------

All bindings
~~~~~~~~~~~~

For all bindings, the above functions are mapped to SWIG with :

::


   struct GDALInfoOptions {
   %extend {
       GDALInfoOptions(char** options) {
           return GDALInfoOptionsNew(options, NULL);
       }

       ~GDALInfoOptions() {
           GDALInfoOptionsFree( self );
       }
   }
   };

   %rename (InfoInternal) GDALInfo;
   char *GDALInfo( GDALDatasetShadow *hDataset, GDALInfoOptions *infoOptions );

::

   struct GDALTranslateOptions {
   %extend {
       GDALTranslateOptions(char** options) {
           return GDALTranslateOptionsNew(options, NULL);
       }

       ~GDALTranslateOptions() {
           GDALTranslateOptionsFree( self );
       }
   }
   };

   %rename (TranslateInternal) wrapper_GDALTranslate;
   %newobject wrapper_GDALTranslate;

   %inline %{
   GDALDatasetShadow* wrapper_GDALTranslate( const char* dest,
                                         GDALDatasetShadow* dataset,
                                         GDALTranslateOptions* translateOptions,
                                         GDALProgressFunc callback=NULL,
                                         void* callback_data=NULL);

::

   struct GDALWarpAppOptions {
   %extend {
       GDALWarpAppOptions(char** options) {
           return GDALWarpAppOptionsNew(options, NULL);
       }

       ~GDALWarpAppOptions() {
           GDALWarpAppOptionsFree( self );
       }
   }
   };

   /* Note: we must use 2 distinct names since there's a bug/feature in swig */
   /* that doesn't play nicely with the (int object_list_count, GDALDatasetShadow** poObjects) input typemap */

   %inline %{
   int wrapper_GDALWarpDestDS( GDALDatasetShadow* dstDS,
                               int object_list_count, GDALDatasetShadow** poObjects,
                               GDALWarpAppOptions* warpAppOptions,
                               GDALProgressFunc callback=NULL,
                               void* callback_data=NULL),
   %}

   %newobject wrapper_GDALWarpDestName;

   %inline %{
   GDALDatasetShadow* wrapper_GDALWarpDestName( const char* dest,
                                                int object_list_count, GDALDatasetShadow** poObjects,
                                                GDALWarpAppOptions* warpAppOptions,
                                                GDALProgressFunc callback=NULL,
                                                void* callback_data=NULL),
   %}

::


   struct GDALVectorTranslateOptions {
   %extend {
       GDALVectorTranslateOptions(char** options) {
           return GDALVectorTranslateOptionsNew(options, NULL);
       }

       ~GDALVectorTranslateOptions() {
           GDALVectorTranslateOptionsFree( self );
       }
   }
   };

   /* Note: we must use 2 distinct names since there's a bug/feature in swig */
   /* that doesn't play nicely with the (int object_list_count, GDALDatasetShadow** poObjects) input typemap */

   %inline %{
   int wrapper_GDALVectorTranslateDestDS( GDALDatasetShadow* dstDS,
                                          GDALDatasetShadow* srcDS,
                               GDALVectorTranslateOptions* options,
                               GDALProgressFunc callback=NULL,
                               void* callback_data=NULL);
   %}

   %newobject wrapper_GDALVectorTranslateDestName;

   %inline %{
   GDALDatasetShadow* wrapper_GDALVectorTranslateDestName( const char* dest,
                                                GDALDatasetShadow* srcDS,
                                                GDALVectorTranslateOptions* options,
                                                GDALProgressFunc callback=NULL,
                                                void* callback_data=NULL);
   %}

For other utilities, see
`gdal.i <http://svn.osgeo.org/gdal/trunk/gdal/swig/python/gdal.i>`__

Python bindings
~~~~~~~~~~~~~~~

For Python bindings, convenience wrappers are created to allow
specifying options in a more user friendly way.

::

   def InfoOptions(options = [], format = 'text', deserialize = True,
            computeMinMax = False, reportHistograms = False, reportProj4 = False,
            stats = False, approxStats = False, computeChecksum = False,
            showGCPs = True, showMetadata = True, showRAT = True, showColorTable = True,
            listMDD = False, showFileList = True, allMetadata = False,
            extraMDDomains = None):
       """ Create a InfoOptions() object that can be passed to gdal.Info()
           options can be be an array of strings, a string or let empty and filled from other keywords."""


   def Info(ds, **kwargs):
       """ Return information on a dataset.
           Arguments are :
             ds --- a Dataset object or a filename
           Keyword arguments are :
             options --- return of gdal.InfoOptions(), string or array of strings
             other keywords arguments of gdal.InfoOptions()
           If options is provided as a gdal.InfoOptions() object, other keywords are ignored. """

gdal.Info() can be used either with setting the attributes of
gdal.InfoOptions() or inlined arguments of gdal.Info(). Arguments can be
specified as array of strings, command line syntax or dedeicated
keywords. So various combinations are possible :

::

       options = gdal.InfoOptions(format = 'json', computeChecksum = True)
       gdal.Info(ds, options)

::

       options = gdal.InfoOptions(options = ['-json', '-checksum'])
       gdal.Info(ds, options)

::

       options = gdal.InfoOptions(options = '-json -checksum')
       gdal.Info(ds, options)

::

       gdal.Info(ds, format = 'json', computeChecksum = True)

::

       gdal.Info(ds, options = ['-json', '-checksum'])

::

       gdal.Info(ds, options = '-json -checksum')

::

   def TranslateOptions(options = [], format = 'GTiff',
                 outputType = GDT_Unknown, bandList = None, maskBand = None,
                 width = 0, height = 0, widthPct = 0.0, heightPct = 0.0,
                 xRes = 0.0, yRes = 0.0,
                 creationOptions = None, srcWin = None, projWin = None, projWinSRS = None, strict = False,
                 unscale = False, scaleParams = None, exponents = None,
                 outputBounds = None, metadataOptions = None,
                 outputSRS = None, GCPs = None,
                 noData = None, rgbExpand = None,
                 stats = False, rat = True, resampleAlg = None,
                 callback = None, callback_data = None):
       """ Create a TranslateOptions() object that can be passed to gdal.Translate()
           Keyword arguments are :
             options --- can be be an array of strings, a string or let empty and filled from other keywords.
             format --- output format ("GTiff", etc...)
             outputType --- output type (gdal.GDT_Byte, etc...)
             bandList --- array of band numbers (index start at 1)
             maskBand --- mask band to generate or not ("none", "auto", "mask", 1, ...)
             width --- width of the output raster in pixel
             height --- height of the output raster in pixel
             widthPct --- width of the output raster in percentage (100 = original width)
             heightPct --- height of the output raster in percentage (100 = original height)
             xRes --- output horizontal resolution
             yRes --- output vertical resolution
             creationOptions --- list of creation options
             srcWin --- subwindow in pixels to extract: [left_x, top_y, width, height]
             projWin --- subwindow in projected coordinates to extract: [ulx, uly, lrx, lry]
             projWinSRS --- SRS in which projWin is expressed
             strict --- strict mode
             unscale --- unscale values with scale and offset metadata
             scaleParams --- list of scale parameters, each of the form [src_min,src_max] or [src_min,src_max,dst_min,dst_max]
             exponents --- list of exponentiation parameters
             outputBounds --- assigned output bounds: [ulx, uly, lrx, lry]
             metadataOptions --- list of metadata options
             outputSRS --- assigned output SRS
             GCPs --- list of GCPs
             noData --- nodata value (or "none" to unset it)
             rgbExpand --- Color palette expansion mode: "gray", "rgb", "rgba"
             stats --- whether to calcule statistics
             rat --- whether to write source RAT
             resampleAlg --- resampling mode
             callback --- callback method
             callback_data --- user data for callback
       """

   def Translate(destName, srcDS, **kwargs):
       """ Convert a dataset.
           Arguments are :
             destName --- Output dataset name
             srcDS --- a Dataset object or a filename
           Keyword arguments are :
             options --- return of gdal.InfoOptions(), string or array of strings
             other keywords arguments of gdal.TranslateOptions()
           If options is provided as a gdal.TranslateOptions() object, other keywords are ignored. """

::


   def WarpOptions(options = [], format = 'GTiff', 
            outputBounds = None,
            outputBoundsSRS = None,
            xRes = None, yRes = None, targetAlignedPixels = False,
            width = 0, height = 0,
            srcSRS = None, dstSRS = None,
            srcAlpha = False, dstAlpha = False, 
            warpOptions = None, errorThreshold = None,
            warpMemoryLimit = None, creationOptions = None, outputType = GDT_Unknown,
            workingType = GDT_Unknown, resampleAlg = None,
            srcNodata = None, dstNodata = None, multithread = False,
            tps = False, rpc = False, geoloc = False, polynomialOrder = None,
            transformerOptions = None, cutlineDSName = None,
            cutlineLayer = None, cutlineWhere = None, cutlineSQL = None, cutlineBlend = None, cropToCutline = False,
            copyMetadata = True, metadataConflictValue = None,
            setColorInterpretation = False,
            callback = None, callback_data = None):
       """ Create a WarpOptions() object that can be passed to gdal.Warp()
           Keyword arguments are :
             options --- can be be an array of strings, a string or let empty and filled from other keywords.
             format --- output format ("GTiff", etc...)
             outputBounds --- output bounds as (minX, minY, maxX, maxY) in target SRS
             outputBoundsSRS --- SRS in which output bounds are expressed, in the case they are not expressed in dstSRS
             xRes, yRes --- output resolution in target SRS
             targetAlignedPixels --- whether to force output bounds to be multiple of output resolution
             width --- width of the output raster in pixel
             height --- height of the output raster in pixel
             srcSRS --- source SRS
             dstSRS --- output SRS
             srcAlpha --- whether to force the last band of the input dataset to be considered as an alpha band
             dstAlpha --- whether to force the creation of an output alpha band
             outputType --- output type (gdal.GDT_Byte, etc...)
             workingType --- working type (gdal.GDT_Byte, etc...)
             warpOptions --- list of warping options
             errorThreshold --- error threshold for approximation transformer (in pixels)
             warpMemoryLimit --- size of working buffer in bytes
             resampleAlg --- resampling mode
             creationOptions --- list of creation options
             srcNodata --- source nodata value(s)
             dstNodata --- output nodata value(s)
             multithread --- whether to multithread computation and I/O operations
             tps --- whether to use Thin Plate Spline GCP transformer
             rpc --- whether to use RPC transformer
             geoloc --- whether to use GeoLocation array transformer
             polynomialOrder --- order of polynomial GCP interpolation
             transformerOptions --- list of transformer options
             cutlineDSName --- cutline dataset name
             cutlineLayer --- cutline layer name
             cutlineWhere --- cutline WHERE clause
             cutlineSQL --- cutline SQL statement
             cutlineBlend --- cutline blend distance in pixels
             cropToCutline --- whether to use cutline extent for output bounds
             copyMetadata --- whether to copy source metadata
             metadataConflictValue --- metadata data conflict value
             setColorInterpretation --- whether to force color interpretation of input bands to output bands
             callback --- callback method
             callback_data --- user data for callback
       """

   def Warp(destNameOrDestDS, srcDSOrSrcDSTab, **kwargs):
       """ Warp one or several datasets.
           Arguments are :
             destNameOrDestDS --- Output dataset name or object
             srcDSOrSrcDSTab --- an array of Dataset objects or filenames, or a Dataset object or a filename
           Keyword arguments are :
             options --- return of gdal.InfoOptions(), string or array of strings
             other keywords arguments of gdal.WarpOptions()
           If options is provided as a gdal.WarpOptions() object, other keywords are ignored. """

::


   def VectorTranslateOptions(options = [], format = 'ESRI Shapefile', 
            accessMode = None,
            srcSRS = None, dstSRS = None, reproject = True,
            SQLStatement = None, SQLDialect = None, where = None, selectFields = None, spatFilter = None,
            datasetCreationOptions = None,
            layerCreationOptions = None,
            layers = None,
            layerName = None,
            geometryType = None,
            segmentizeMaxDist= None,
            callback = None, callback_data = None):
       """ Create a VectorTranslateOptions() object that can be passed to gdal.VectorTranslate()
           Keyword arguments are :
             options --- can be be an array of strings, a string or let empty and filled from other keywords.
             format --- output format ("ESRI Shapefile", etc...)
             accessMode --- None for creation, 'update', 'append', 'overwrite'
             srcSRS --- source SRS
             dstSRS --- output SRS (with reprojection if reproject = True)
             reproject --- whether to do reprojection
             SQLStatement --- SQL statement to apply to the source dataset
             SQLDialect --- SQL dialect ('OGRSQL', 'SQLITE', ...)
             where --- WHERE clause to apply to source layer(s)
             selectFields --- list of fields to select
             spatFilter --- spatial filter as (minX, minY, maxX, maxY) bounding box
             datasetCreationOptions --- list of dataset creation options
             layerCreationOptions --- list of layer creation options
             layers --- list of layers to convert
             layerName --- output layer name
             geometryType --- output layer geometry type ('POINT', ....)
             segmentizeMaxDist --- maximum distance between consecutive nodes of a line geometry
             callback --- callback method
             callback_data --- user data for callback
       """

   def VectorTranslate(destNameOrDestDS, srcDS, **kwargs):
       """ Convert one vector dataset
           Arguments are :
             destNameOrDestDS --- Output dataset name or object
             srcDS --- a Dataset object or a filename
           Keyword arguments are :
             options --- return of gdal.InfoOptions(), string or array of strings
             other keywords arguments of gdal.VectorTranslateOptions()
           If options is provided as a gdal.VectorTranslateOptions() object, other keywords are ignored. """

::


   def DEMProcessingOptions(options = [], colorFilename = None, format = 'GTiff',
                 creationOptions = None, computeEdges = False, alg = 'Horn', band = 1,
                 zFactor = None, scale = None, azimuth = None, altitude = None, combined = False,
                 slopeFormat = None, trigonometric = False, zeroForFlat = False,
                 callback = None, callback_data = None):
       """ Create a DEMProcessingOptions() object that can be passed to gdal.DEMProcessing()
           Keyword arguments are :
             options --- can be be an array of strings, a string or let empty and filled from other keywords.
             colorFilename --- (mandatory for "color-relief") name of file that contains palette definition for the "color-relief" processing.
             format --- output format ("GTiff", etc...)
             creationOptions --- list of creation options
             computeEdges --- whether to compute values at raster edges.
             alg --- 'ZevenbergenThorne' or 'Horn'
             band --- source band number to use
             zFactor --- (hillshade only) vertical exaggeration used to pre-multiply the elevations.
             scale --- ratio of vertical units to horizontal.
             azimuth --- (hillshade only) azimuth of the light, in degrees. 0 if it comes from the top of the raster, 90 from the east, ... The default value, 315, should rarely be changed as it is the value generally used to generate shaded maps.
             altitude ---(hillshade only) altitude of the light, in degrees. 90 if the light comes from above the DEM, 0 if it is raking light.
             combined --- (hillshade only) whether to compute combined shading, a combination of slope and oblique shading.
             slopeformat --- (slope only) "degree" or "percent".
             trigonometric --- (aspect only) whether to return trigonometric angle instead of azimuth. Thus 0deg means East, 90deg North, 180deg West, 270deg South.
             zeroForFlat --- (aspect only) whether to return 0 for flat areas with slope=0, instead of -9999.
             callback --- callback method
             callback_data --- user data for callback
       """

   def DEMProcessing(destName, srcDS, processing, **kwargs):
       """ Apply a DEM processing.
           Arguments are :
             destName --- Output dataset name
             srcDS --- a Dataset object or a filename
             processing --- one of "hillshade", "slope", "aspect", "color-relief", "TRI", "TPI", "Roughness"
           Keyword arguments are :
             options --- return of gdal.InfoOptions(), string or array of strings
             other keywords arguments of gdal.DEMProcessingOptions()
           If options is provided as a gdal.DEMProcessingOptions() object, other keywords are ignored. """

::

   def NearblackOptions(options = [], format = 'GTiff', 
            creationOptions = None, white = False, colors = None,
            maxNonBlack = None, nearDist = None, setAlpha = False, setMask = False,
            callback = None, callback_data = None):
       """ Create a NearblackOptions() object that can be passed to gdal.Nearblack()
           Keyword arguments are :
             options --- can be be an array of strings, a string or let empty and filled from other keywords.
             format --- output format ("GTiff", etc...)
             creationOptions --- list of creation options
             white --- whether to search for nearly white (255) pixels instead of nearly black pixels.
             colors --- list of colors  to search for, e.g. ((0,0,0),(255,255,255)). The pixels that are considered as the collar are set to 0
             maxNonBlack --- number of non-black (or other searched colors specified with white / colors) pixels that can be encountered before the giving up search inwards. Defaults to 2. 
             nearDist --- select how far from black, white or custom colors the pixel values can be and still considered near black, white or custom color.  Defaults to 15.
             setAlpha --- adds an alpha band if the output file.
             setMask --- adds a mask band to the output file.
             callback --- callback method
             callback_data --- user data for callback
       """

   def Nearblack(destNameOrDestDS, srcDS, **kwargs):
       """ Convert nearly black/white borders to exact value.
           Arguments are :
             destNameOrDestDS --- Output dataset name or object
             srcDS --- a Dataset object or a filename
           Keyword arguments are :
             options --- return of gdal.InfoOptions(), string or array of strings
             other keywords arguments of gdal.NearblackOptions()
           If options is provided as a gdal.NearblackOptions() object, other keywords are ignored. """

::

   def GridOptions(options = [], format = 'GTiff',
                 outputType = GDT_Unknown,
                 width = 0, height = 0,
                 creationOptions = None,
                 outputBounds = None,
                 outputSRS = None,
                 noData = None,
                 algorithm = None,
                 layers = None,
                 SQLStatement = None,
                 where = None,
                 spatFilter = None,
                 zfield = None,
                 z_increase = None,
                 z_multiply = None,
                 callback = None, callback_data = None):
       """ Create a GridOptions() object that can be passed to gdal.Grid()
           Keyword arguments are :
             options --- can be be an array of strings, a string or let empty and filled from other keywords.
             format --- output format ("GTiff", etc...)
             outputType --- output type (gdal.GDT_Byte, etc...)
             width --- width of the output raster in pixel
             height --- height of the output raster in pixel
             creationOptions --- list of creation options
             outputBounds --- assigned output bounds: [ulx, uly, lrx, lry]
             outputSRS --- assigned output SRS
             noData --- nodata value
             algorithm --- e.g "invdist:power=2.0:smoothing=0.0:radius1=0.0:radius2=0.0:angle=0.0:max_points=0:min_points=0:nodata=0.0"
             layers --- list of layers to convert
             SQLStatement --- SQL statement to apply to the source dataset
             where --- WHERE clause to apply to source layer(s)
             spatFilter --- spatial filter as (minX, minY, maxX, maxY) bounding box
             zfield --- Identifies an attribute field on the features to be used to get a Z value from. This value overrides Z value read from feature geometry record.
             z_increase --- Addition to the attribute field on the features to be used to get a Z value from. The addition should be the same unit as Z value. The result value will be Z value + Z increase value. The default value is 0.
             z_multiply - Multiplication ratio for Z field. This can be used for shift from e.g. foot to meters or from  elevation to deep. The result value will be (Z value + Z increase value) * Z multiply value.  The default value is 1.
             callback --- callback method
             callback_data --- user data for callback
       """

   def Grid(destName, srcDS, **kwargs):
       """ Create raster from the scattered data.
           Arguments are :
             destName --- Output dataset name
             srcDS --- a Dataset object or a filename
           Keyword arguments are :
             options --- return of gdal.InfoOptions(), string or array of strings
             other keywords arguments of gdal.GridOptions()
           If options is provided as a gdal.GridOptions() object, other keywords are ignored. """

::

   def RasterizeOptions(options = [], format = None, 
            creationOptions = None, noData = None, initValues = None,
            outputBounds = None, outputSRS = None,
            width = None, height = None,
            xRes = None, yRes = None, targetAlignedPixels = False,
            bands = None, inverse = False, allTouched = False,
            burnValues = None, attribute = None, useZ = False, layers = None,
            SQLStatement = None, SQLDialect = None, where = None,
            callback = None, callback_data = None):
       """ Create a RasterizeOptions() object that can be passed to gdal.Rasterize()
           Keyword arguments are :
             options --- can be be an array of strings, a string or let empty and filled from other keywords.
             format --- output format ("GTiff", etc...)
             creationOptions --- list of creation options
             outputBounds --- assigned output bounds: [minx, miny, maxx, maxy]
             outputSRS --- assigned output SRS
             width --- width of the output raster in pixel
             height --- height of the output raster in pixel
             xRes, yRes --- output resolution in target SRS
             targetAlignedPixels --- whether to force output bounds to be multiple of output resolution
             noData --- nodata value
             initValues --- Value or list of values to pre-initialize the output image bands with.  However, it is not marked as the nodata value in the output file.  If only one value is given, the same value is used in all the bands.
             bands --- list of output bands to burn values into
             inverse --- whether to invert rasterization, ie burn the fixed burn value, or the burn value associated  with the first feature into all parts of the image not inside the provided a polygon.
             allTouched -- whether to enable the ALL_TOUCHED rasterization option so that all pixels touched by lines or polygons will be updated, not just those on the line render path, or whose center point is within the polygon.
             burnValues -- list of fixed values to burn into each band for all objects. Excusive with attribute.
             attribute --- identifies an attribute field on the features to be used for a burn-in value. The value will be burned into all output bands. Excusive with burnValues.
             useZ --- whether to indicate that a burn value should be extracted from the "Z" values of the feature. These values are added to the burn value given by burnValues or attribute if provided. As of now, only points and lines are drawn in 3D.
             layers --- list of layers from the datasource that will be used for input features.
             SQLStatement --- SQL statement to apply to the source dataset
             SQLDialect --- SQL dialect ('OGRSQL', 'SQLITE', ...)
             where --- WHERE clause to apply to source layer(s)
             callback --- callback method
             callback_data --- user data for callback
       """

   def Rasterize(destNameOrDestDS, srcDS, **kwargs):
       """ Burns vector geometries into a raster
           Arguments are :
             destNameOrDestDS --- Output dataset name or object
             srcDS --- a Dataset object or a filename
           Keyword arguments are :
             options --- return of gdal.InfoOptions(), string or array of strings
             other keywords arguments of gdal.RasterizeOptions()
           If options is provided as a gdal.RasterizeOptions() object, other keywords are ignored. """

::

   def BuildVRTOptions(options = [],
                       resolution = None,
                       outputBounds = None,
                       xRes = None, yRes = None,
                       targetAlignedPixels = None,
                       separate = None,
                       bandList = None,
                       addAlpha = None,
                       resampleAlg = None,
                       outputSRS = None,
                       allowProjectionDifference = None,
                       srcNodata = None,
                       VRTNodata = None,
                       hideNodata = None,
                       callback = None, callback_data = None):
       """ Create a BuildVRTOptions() object that can be passed to gdal.BuildVRT()
           Keyword arguments are :
             options --- can be be an array of strings, a string or let empty and filled from other keywords..
             resolution --- 'highest', 'lowest', 'average', 'user'.
             outputBounds --- output bounds as (minX, minY, maxX, maxY) in target SRS.
             xRes, yRes --- output resolution in target SRS.
             targetAlignedPixels --- whether to force output bounds to be multiple of output resolution.
             separate --- whether each source file goes into a separate stacked band in the VRT band.
             bandList --- array of band numbers (index start at 1).
             addAlpha --- whether to add an alpha mask band to the VRT when the source raster have none.
             resampleAlg --- resampling mode.
             outputSRS --- assigned output SRS.
             allowProjectionDifference --- whether to accept input datasets have not the same projection. Note: they will *not* be reprojected.
             srcNodata --- source nodata value(s).
             VRTNodata --- nodata values at the VRT band level.
             hideNodata --- whether to make the VRT band not report the NoData value.
             callback --- callback method.
             callback_data --- user data for callback.
       """

   def BuildVRT(destName, srcDSOrSrcDSTab, **kwargs):
       """ Build a VRT from a list of datasets.
           Arguments are :
             destName --- Output dataset name
             srcDSOrSrcDSTab --- an array of Dataset objects or filenames, or a Dataset object or a filename
           Keyword arguments are :
             options --- return of gdal.InfoOptions(), string or array of strings
             other keywords arguments of gdal.BuildVRTOptions()
           If options is provided as a gdal.BuildVRTOptions() object, other keywords are ignored. """

Utilities
---------

Utilities are modified to call the respective function.

Documentation
-------------

All new methods/functions are documented.

Test Suite
----------

gdal.Info method is tested in
`test_gdalinfo_lib.py <http://svn.osgeo.org/gdal/trunk/autotest/utilities/test_gdalinfo_lib.py>`__.

gdal.Translate method is tested in
`test_gdal_translate_lib.py <http://svn.osgeo.org/gdal/trunk/autotest/utilities/test_gdal_translate_lib.py>`__

gdal.Warp method is tested in
`test_gdalwarp_lib.py <http://svn.osgeo.org/gdal/trunk/autotest/utilities/test_gdalwarp_lib.py>`__

gdal.VectorTranslate method is tested in
`test_ogr2ogr_lib.py <http://svn.osgeo.org/gdal/trunk/autotest/utilities/test_ogr2ogr_lib.py>`__

gdal.DEMProcessing method is tested in
`test_gdaldem_lib.py <http://svn.osgeo.org/gdal/trunk/autotest/utilities/test_gdaldem_lib.py>`__

gdal.Nearblack method is tested in
`test_nearblack_lib.py <http://svn.osgeo.org/gdal/trunk/autotest/utilities/test_nearblack_lib.py>`__

gdal.Grid method is tested in
`test_gdal_grid_lib.py <http://svn.osgeo.org/gdal/trunk/autotest/utilities/test_gdal_grid_lib.py>`__

gdal.Rasterize method is tested in
`test_gdal_rasterize_lib.py <http://svn.osgeo.org/gdal/trunk/autotest/utilities/test_gdal_rasterize_lib.py>`__.

gdal.BuildVRT method is tested in
`test_gdalbuildvrt_lib.py <http://svn.osgeo.org/gdal/trunk/autotest/utilities/test_gdalbuildvrt_lib.py>`__.

Compatibility Issues
--------------------

None expected. Command line utilities will keep the same interface. It
will be checked by ensuring their tests in autotest/utilities still
pass.

Related ticket
--------------

Implementation
--------------

Implementation will be done by Faza Mahamood and Even Rouault

The proposed implementation for gdalinfo and gdal_translate lies in the
"rfc59.1" branch of the
`https://github.com/rouault/gdal2/tree/rfc59.1 <https://github.com/rouault/gdal2/tree/rfc59.1>`__.

Voting history
--------------

+1 from DanielM and EvenR
