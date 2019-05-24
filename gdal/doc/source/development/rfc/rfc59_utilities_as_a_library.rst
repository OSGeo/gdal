.. _rfc-59:

=======================================================================================
RFC 59 : GDAL/OGR utilities as a library
=======================================================================================

Authors: Faza Mahamood

Contact: fazamhd at gmail dot com

Status: Retracted in favor of :ref:`rfc-59.1`

Summary
-------

This RFC defines new function for each GDAL utility. The new utility
functions can be used to work on in-memory datasets. The utility code is
modified to call the new function. This RFC gives a general frame and
principles, demonstrated with gdalinfo, but aimed at being extended with
other utilities.

Rationale
---------

There is need for calling GDAL utilities from the code. But this
involves using system calls and cannot work on in-memory datasets.

Changes
-------

New library libgdalutils is created. Both Unix and Windows build have
been modified to take into account the new lib. The GDAL utilities are
modified to use the new functions. New header file gdal_utils.h is
created which contains the public declarations of GDAL utilities. The
current header(still in progress) can be found
`here <https://github.com/fazam/gdal/blob/gdalinfo/gdal/apps/gdal_utils.h>`__.

::


       char CPL_DLL *GDALInfo( GDALDatasetH hDataset, GDALInfoOptions *psOptions );

       GDALInfoOptions CPL_DLL *GDALInfoOptionsNew( void );

       void CPL_DLL GDALInfoOptionsAddExtraMDDomains( GDALInfoOptions *psOptions,
                                                      const char *pszDomain );

       void CPL_DLL GDALInfoOptionsSetExtraMDDomains( GDALInfoOptions *psOptions,
                                                      char **papszExtraMDDomains );

       void CPL_DLL GDALInfoOptionsFree( GDALInfoOptions *psOptions );

::

   GDALDatasetH CPL_DLL GDALTranslate(const char *pszDest, GDALDatasetH hSrcDataset, GDALTranslateOptions *psOptions, int *pbUsageError)

   GDALDatasetH CPL_DLL GDALWarp( const char *pszDest, GDALDatasetH hDstDS, int nSrcCount,
                          GDALDatasetH *pahSrcDS, GDALWarpAppOptions *psOptions, int *pbUsageError )

   GDALDatasetH CPL_DLL OGR2OGR( const char *pszDest, GDALDatasetH hDstDS, GDALDatasetH hSrcDS,
                                 OGR2OGROptions *psOptions, int *pbUsageError )

SWIG bindings (Python / Java / C# / Perl) changes
-------------------------------------------------

For Python bindings only, new functions Info(), Translate() and Warp()
are added in the gdal module which uses the new GDALInfo(),
GDALTranslate() and GDALWarp() function respectively. Translate() is
added in the ogr module which uses the new OGR2OGR() function.

gdal.Info() can be used either with setting the attributes of
gdal.InfoOptions() or inlined arguments of gdal.Info().

::


       options = gdal.InfoOptions()
       
       options.format = gdal.INFO_FORMAT_TEXT
       options.deserialize = True
       options.computeMinMax = False
       options.reportHistograms = False
       options.reportProj4 = True
       options.stats = False
       options.approxStats = True
       options.sample = False
       options.computeChecksum = False
       options.showGCPs = True
       options.showMetadata = True
       options.showRAT = False
       options.showColorTable = True
       options.listMDD = False
       options.showFileList = True
       options.allMetadata = TRUE
       options.extraMDDomains = ['TRE']
       
       gdal.Info(ds, options, deserialize = True)

::


       gdal.Info(ds, options = None, format = _gdal.INFO_FORMAT_TEXT, deserialize = True,
                computeMinMax = False, reportHistograms = False, reportProj4 = False,
                stats = False, approxStats = True, sample = False, computeChecksum = False,
                showGCPs = True, showMetadata = True, showRAT = True, showColorTable = True,
                listMDD = False, showFileList = True, allMetadata = False,
                extraMDDomains = None)

       gdal.Translate(destName, srcDS, options = None, format = 'GTiff', quiet = True,
                outputType = GDT_Unknown, maskMode = _gdal.MASK_AUTO, bandList = None,
                oXSizePixel = 0, oYSizePixel = 0, oXSizePct = 0.0, oYSizePct = 0.0,
                createOptions = None, srcWin = [0,0,0,0],strict = False,
                unscale = False, scaleParams = None, exponent = None,
                uLX = 0.0, uLY = 0.0, lRX = 0.0, lRY = 0.0, metadataOptions = None,
                outputSRS = None, GCPs = None, ULLR = [0,0,0,0], setNoData = False,
                unsetNoData = False, noDataReal = 0.0, rgbExpand = 0, maskBand = 0,
                stats = False, approxStats = False, errorOnPartiallyOutside = False,
                errorOnCompletelyOutside = False, noRAT = False, resampling = None,
                xRes = 0.0, yRes = 0.0, projSRS = None)
       
       gdal.Warp(destNameOrDestDS, srcDSOrSrcDSTab, options = None, minX = 0.0, minY = 0.0, maxX = 0.0,
                maxY = 0.0, xRes = 0.0, yRes = 0.0, targetAlignedPixels = False, forcePixels = 0,
                forceLines = 0, quiet = True, enableDstAlpha = False, enableSrcAlpha = False,
                format = 'GTiff', createOutput = False, warpOptions = None, errorThreshold = -1,
                warpMemoryLimit = 0.0, createOptions = None, outputType = GDT_Unknown,
                workingType = GDT_Unknown, resampleAlg = GRA_NearestNeighbour,
                srcNodata = None, dstNodata = None, multi = False, TO = None, cutlineDSName = None,
                cLayer = None, cWHERE = None, cSQL = None, cropToCutline = False, overwrite = False,
                copyMetadata = True, copyBandInfo = True, MDConflictValue = '*',
                setColorInterpretation = False, destOpenOptions = None, OvLevel = -2)

       ogr.Translate(destNameOrDestDS, srcDS, options = None, accessMode = _ogr.ACCESS_CREATION,
                skipFailures = False, layerTransaction = -1, forceTransaction = False,
                groupTransactions = 20000, FIDToFetch = -1, quiet = False,
                format = 'ESRI Shapefile', layers = None, DSCO = None, LCO = None, transform = False,
                addMissingFields = False, outputSRSDef = None, sourceSRSDef = None,
                nullifyOutputSRS = False, exactFieldNameMatch = True, newLayerName = None,
                WHERE = None, geomField = None, selFields = None, SQLStatement = None,
                dialect = None, gType = -2, geomConversion = _ogr.GEOMTYPE_DEFAULT, geomOp = _ogr.GEOMOP_NONE,
                geomOpParam = 0, fieldTypesToString = None, mapFieldType = None, unsetFieldWidth = False,
                displayProgress = False, wrapDateline = False, dateLineOffset = 10, clipSrc = None, clipSrcDS = None,
                clipSrcSQL = None, clipSrcLayer = None, clipSrcWhere = None, clipDst = None,
                clipDstDS = None, clipDstSQL = None, clipDstLayer = None, clipDstWhere = None,
                splitListFields = False, maxSplitListSubFields = -1, explodeCollections = False,
                zField = None, fieldMap = None, coordDim = -1, destOpenOptions = None,
                forceNullable = False, unsetDefault = False, unsetFid = False, preserveFID = False,
                copyMD = True, metadataOptions = None, spatSRSDef = None, transformOrder = 0,
                spatialFilter = None)

Utilities
---------

Utilities are modified to call the respective function.

Documentation
-------------

All new methods/functions are documented.

Test Suite
----------

gdal.Info method is tested in test_gdalinfo_lib.py. gdal.Translate
method is tested in test_gdal_translate_lib.py. gdal.Warp method is
tested in test_gdalwarp_lib.py. ogr.Translate method is tested in
test_ogr2ogr_lib.py.

Compatibility Issues
--------------------

None expected. Command line utilities will keep the same interface. It
will be checked by ensuring their tests in autotest/utilities still
pass.

Open question
-------------

What name should be given for librarified ogr2ogr? OGR2OGR() or
OGRTranslate() ?

The order of arguments in GDALTranslate(), GDALWarp() and OGR2OGR() is
currently dest then source(s).

::

   GDALDatasetH CPL_DLL GDALTranslate(const char *pszDest, GDALDatasetH hSrcDataset, GDALTranslateOptions *psOptions, int *pbUsageError)

   GDALDatasetH CPL_DLL GDALWarp( const char *pszDest, GDALDatasetH hDstDS, int nSrcCount,
                          GDALDatasetH *pahSrcDS, GDALWarpAppOptions *psOptions, int *pbUsageError )

   GDALDatasetH CPL_DLL OGR2OGR( const char *pszDest, GDALDatasetH hDstDS, GDALDatasetH hSrcDS,
                                 OGR2OGROptions *psOptions, int *pbUsageError )

It is similar to GDALCreateCopy(const char\* pszDestFilename,
GDALDatasetH hSrcDS, ....), so at least there's a form of consistency at
the API level. Any comments?

Related ticket
--------------

Implementation
--------------

Implementation will be done by Faza Mahamood.

The proposed implementation lies in the "gdalinfo" branch of the
`https://github.com/fazam/gdal/tree/gdalinfo <https://github.com/fazam/gdal/tree/gdalinfo>`__.

The list of changes :
`https://github.com/fazam/gdal/compare/gdalinfo <https://github.com/fazam/gdal/compare/gdalinfo>`__

Voting history
--------------

