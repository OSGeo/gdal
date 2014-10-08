/******************************************************************************
 * $Id$
 *
 * Project:  High Performance Image Reprojector
 * Purpose:  Test program for high performance warper API.
 * Author:   Frank Warmerdam <warmerdam@pobox.com>
 *
 ******************************************************************************
 * Copyright (c) 2002, i3 - information integration and imaging 
 *                          Fort Collin, CO
 * Copyright (c) 2007-2013, Even Rouault <even dot rouault at mines-paris dot org>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 ****************************************************************************/

#include "gdalwarper.h"
#include "cpl_string.h"
#include "ogr_spatialref.h"
#include "ogr_api.h"
#include "commonutils.h"
#include <vector>

CPL_CVSID("$Id$");

static void
LoadCutline( const char *pszCutlineDSName, const char *pszCLayer, 
             const char *pszCWHERE, const char *pszCSQL, 
             void **phCutlineRet );
static void
TransformCutlineToSource( GDALDatasetH hSrcDS, void *hCutline,
                          char ***ppapszWarpOptions, char **papszTO );

static GDALDatasetH 
GDALWarpCreateOutput( char **papszSrcFiles, const char *pszFilename, 
                      const char *pszFormat, char **papszTO,
                      char ***ppapszCreateOptions, GDALDataType eDT,
                      void ** phTransformArg,
                      GDALDatasetH* phSrcDS,
                      int bSetColorInterpretation,
                      char** papszOpenOptions );

static void 
RemoveConflictingMetadata( GDALMajorObjectH hObj, char **papszMetadata,
                           const char *pszValueConflict );

static double	       dfMinX=0.0, dfMinY=0.0, dfMaxX=0.0, dfMaxY=0.0;
static double	       dfXRes=0.0, dfYRes=0.0;
static int             bTargetAlignedPixels = FALSE;
static int             nForcePixels=0, nForceLines=0, bQuiet = FALSE;
static int             bEnableDstAlpha = FALSE, bEnableSrcAlpha = FALSE;

static int             bVRT = FALSE;

/******************************************************************************/
/*! \page gdalwarp gdalwarp

image reprojection and warping utility

\section gdalwarp_synopsis SYNOPSIS

\htmlonly
Usage: 
\endhtmlonly

\verbatim
gdalwarp [--help-general] [--formats]
    [-s_srs srs_def] [-t_srs srs_def] [-to "NAME=VALUE"]
    [-order n | -tps | -rpc | -geoloc] [-et err_threshold]
    [-refine_gcps tolerance [minimum_gcps]]
    [-te xmin ymin xmax ymax] [-tr xres yres] [-tap] [-ts width height]
    [-wo "NAME=VALUE"] [-ot Byte/Int16/...] [-wt Byte/Int16]
    [-srcnodata "value [value...]"] [-dstnodata "value [value...]"] -dstalpha
    [-r resampling_method] [-wm memory_in_mb] [-multi] [-q]
    [-cutline datasource] [-cl layer] [-cwhere expression]
    [-csql statement] [-cblend dist_in_pixels] [-crop_to_cutline]
    [-of format] [-co "NAME=VALUE"]* [-overwrite]
    [-nomd] [-cvmd meta_conflict_value] [-setci] [-oo NAME=VALUE]*
    srcfile* dstfile
\endverbatim

\section gdalwarp_description DESCRIPTION

<p>
The gdalwarp utility is an image mosaicing, reprojection and warping
utility. The program can reproject to any supported projection,
and can also apply GCPs stored with the image if the image is "raw"
with control information.

<p>
<dl>
<dt> <b>-s_srs</b> <em>srs def</em>:</dt><dd> source spatial reference set.
The coordinate systems that can be passed are anything supported by the
OGRSpatialReference.SetFromUserInput() call, which includes EPSG PCS and GCSes
(ie. EPSG:4296), PROJ.4 declarations (as above), or the name of a .prf file
containing well known text.</dd>
<dt> <b>-t_srs</b> <em>srs_def</em>:</dt><dd> target spatial reference set.
The coordinate systems that can be passed are anything supported by the
OGRSpatialReference.SetFromUserInput() call, which includes EPSG PCS and GCSes
(ie. EPSG:4296), PROJ.4 declarations (as above), or the name of a .prf file
containing well known text.</dd>
<dt> <b>-to</b> <em>NAME=VALUE</em>:</dt><dd> set a transformer option suitable
to pass to GDALCreateGenImgProjTransformer2(). </dd>
<dt> <b>-order</b> <em>n</em>:</dt><dd> order of polynomial used for warping
(1 to 3). The default is to select a polynomial order based on the number of
GCPs.</dd>
<dt> <b>-tps</b>:</dt><dd>Force use of thin plate spline transformer based on
available GCPs.</dd>
<dt> <b>-rpc</b>:</dt> <dd>Force use of RPCs.</dd>
<dt> <b>-geoloc</b>:</dt><dd>Force use of Geolocation Arrays.</dd>
<dt> <b>-et</b> <em>err_threshold</em>:</dt><dd> error threshold for
transformation approximation (in pixel units - defaults to 0.125).</dd>
<dt> <b>-refine_gcps</b> <em>tolerance minimum_gcps</em>:</dt><dd>  (GDAL >= 1.9.0) refines the GCPs by automatically eliminating outliers.
Outliers will be eliminated until minimum_gcps are left or when no outliers can be detected.
The tolerance is passed to adjust when a GCP will be eliminated.
Not that GCP refinement only works with polynomial interpolation.
The tolerance is in pixel units if no projection is available, otherwise it is in SRS units.
If minimum_gcps is not provided, the minimum GCPs according to the polynomial model is used.</dd>
<dt> <b>-te</b> <em>xmin ymin xmax ymax</em>:</dt><dd> set georeferenced
extents of output file to be created (in target SRS).</dd>
<dt> <b>-tr</b> <em>xres yres</em>:</dt><dd> set output file resolution (in
target georeferenced units)</dd>
<dt> <b>-tap</b>:</dt><dd> (GDAL >= 1.8.0) (target aligned pixels) align
the coordinates of the extent of the output file to the values of the -tr,
such that the aligned extent includes the minimum extent.</dd>
<dt> <b>-ts</b> <em>width height</em>:</dt><dd> set output file size in
pixels and lines. If width or height is set to 0, the other dimension will be
guessed from the computed resolution. Note that -ts cannot be used with -tr</dd>
<dt> <b>-wo</b> <em>"NAME=VALUE"</em>:</dt><dd> Set a warp options.  The 
GDALWarpOptions::papszWarpOptions docs show all options.  Multiple
 <b>-wo</b> options may be listed.</dd>
<dt> <b>-ot</b> <em>type</em>:</dt><dd> For the output bands to be of the
indicated data type.</dd>
<dt> <b>-wt</b> <em>type</em>:</dt><dd> Working pixel data type. The data type
of pixels in the source image and destination image buffers.</dd>
<dt> <b>-r</b> <em>resampling_method</em>:</dt><dd> Resampling method to use. Available methods are:
<dl>
<dt><b>near</b></dt>: <dd>nearest neighbour resampling (default, fastest
algorithm, worst interpolation quality).</dd>
<dt><b>bilinear</b></dt>: <dd>bilinear resampling.</dd>
<dt><b>cubic</b></dt>: <dd>cubic resampling.</dd>
<dt><b>cubicspline</b></dt>: <dd>cubic spline resampling.</dd>
<dt><b>lanczos</b></dt>: <dd>Lanczos windowed sinc resampling.</dd>
<dt><b>average</b></dt>: <dd>average resampling, computes the average of all non-NODATA contributing pixels. (GDAL >= 1.10.0)</dd>
<dt><b>mode</b></dt>: <dd>mode resampling, selects the value which appears most often of all the sampled points. (GDAL >= 1.10.0)</dd>
</dl>
<dt> <b>-srcnodata</b> <em>value [value...]</em>:</dt><dd> Set nodata masking
values for input bands (different values can be supplied for each band).  If 
more than one value is supplied all values should be quoted to keep them 
together as a single operating system argument.  Masked values will not be 
used in interpolation.  Use a value of <tt>None</tt> to ignore intrinsic nodata settings on the source dataset.</dd>
<dt> <b>-dstnodata</b> <em>value [value...]</em>:</dt><dd> Set nodata values
for output bands (different values can be supplied for each band).  If more
than one value is supplied all values should be quoted to keep them together
as a single operating system argument.  New files will be initialized to this
value and if possible the nodata value will be recorded in the output
file. Use a value of <tt>None</tt> to ensure that nodata is not defined (GDAL>=2.0).
If this argument is not used then nodata values will be copied from the source dataset (GDAL>=2.0).</dd>
<dt> <b>-dstalpha</b>:</dt><dd> Create an output alpha band to identify 
nodata (unset/transparent) pixels. </dd>
<dt> <b>-wm</b> <em>memory_in_mb</em>:</dt><dd> Set the amount of memory (in
megabytes) that the warp API is allowed to use for caching.</dd>
<dt> <b>-multi</b>:</dt><dd> Use multithreaded warping implementation.
Multiple threads will be used to process chunks of image and perform
input/output operation simultaneously.</dd>
<dt> <b>-q</b>:</dt><dd> Be quiet.</dd>
<dt> <b>-of</b> <em>format</em>:</dt><dd> Select the output format. The default is GeoTIFF (GTiff). Use the short format name. </dd>
<dt> <b>-co</b> <em>"NAME=VALUE"</em>:</dt><dd> passes a creation option to
the output format driver. Multiple <b>-co</b> options may be listed. See
format specific documentation for legal creation options for each format.
</dd>

<dt> <b>-cutline</b> <em>datasource</em>:</dt><dd>Enable use of a blend cutline from the name OGR support datasource.</dd>
<dt> <b>-cl</b> <em>layername</em>:</dt><dd>Select the named layer from the 
cutline datasource.</dd>
<dt> <b>-cwhere</b> <em>expression</em>:</dt><dd>Restrict desired cutline features based on attribute query.</dd>
<dt> <b>-csql</b> <em>query</em>:</dt><dd>Select cutline features using an SQL query instead of from a layer with -cl.</dd>
<dt> <b>-cblend</b> <em>distance</em>:</dt><dd>Set a blend distance to use to blend over cutlines (in pixels).</dd>
<dt> <b>-crop_to_cutline</b>:</dt><dd>(GDAL >= 1.8.0) Crop the extent of the target dataset to the extent of the cutline.</dd>
<dt> <b>-overwrite</b>:</dt><dd>(GDAL >= 1.8.0) Overwrite the target dataset if it already exists.</dd>
<dt> <b>-nomd</b>:</dt><dd>(GDAL >= 1.10.0) Do not copy metadata. Without this option, dataset and band metadata 
(as well as some band information) will be copied from the first source dataset. 
Items that differ between source datasets will be set to * (see -cvmd option).</dd>
<dt> <b>-cvmd</b> <em>meta_conflict_value</em>:</dt><dd>(GDAL >= 1.10.0) 
Value to set metadata items that conflict between source datasets (default is "*"). Use "" to remove conflicting items. </dd>
<dt> <b>-setci</b>:</dt><dd>(GDAL >= 1.10.0) 
Set the color interpretation of the bands of the target dataset from the source dataset.</dd>
<dt> <b>-oo</b> <em>NAME=VALUE</em>:</dt><dd>(starting with GDAL 2.0) Dataset open option (format specific)</dd>

<dt> <em>srcfile</em>:</dt><dd> The source file name(s). </dd>
<dt> <em>dstfile</em>:</dt><dd> The destination file name. </dd>
</dl>

Mosaicing into an existing output file is supported if the output file 
already exists. The spatial extent of the existing file will not
be modified to accomodate new data, so you may have to remove it in that case, or
use the -overwrite option.

Polygon cutlines may be used as a mask to restrict the area of the destination file
that may be updated, including blending.  If the OGR layer containing the cutline
features has no explicit SRS, the cutline features must be in the SRS of the
destination file. When outputing to a not yet existing target dataset,
its extent will be the one of the original raster unless -te or -crop_to_cutline are
specified.

<p>
\section gdalwarp_example EXAMPLE

For instance, an eight bit spot scene stored in GeoTIFF with
control points mapping the corners to lat/long could be warped to a UTM
projection with a command like this:<p>

\verbatim
gdalwarp -t_srs '+proj=utm +zone=11 +datum=WGS84' raw_spot.tif utm11.tif
\endverbatim

For instance, the second channel of an ASTER image stored in HDF with
control points mapping the corners to lat/long could be warped to a UTM
projection with a command like this:<p>

\verbatim
gdalwarp HDF4_SDS:ASTER_L1B:"pg-PR1B0000-2002031402_100_001":2 pg-PR1B0000-2002031402_100_001_2.tif
\endverbatim

\if man
\section gdalwarp_author AUTHORS
Frank Warmerdam <warmerdam@pobox.com>, Silke Reimer <silke@intevation.de>
\endif
*/

/************************************************************************/
/*                               GDALExit()                             */
/*  This function exits and cleans up GDAL and OGR resources            */
/*  Perhaps it should be added to C api and used in all apps?           */
/************************************************************************/

static int GDALExit( int nCode )
{
  const char  *pszDebug = CPLGetConfigOption("CPL_DEBUG",NULL);
  if( pszDebug && (EQUAL(pszDebug,"ON") || EQUAL(pszDebug,"") ) )
  {  
    GDALDumpOpenDatasets( stderr );
    CPLDumpSharedList( NULL );
  }

  GDALDestroyDriverManager();

#ifdef OGR_ENABLED
  OGRCleanupAll();
#endif

  exit( nCode );
}

/************************************************************************/
/*                               Usage()                                */
/************************************************************************/

static void Usage(const char* pszErrorMsg = NULL)

{
    printf( 
        "Usage: gdalwarp [--help-general] [--formats]\n"
        "    [-s_srs srs_def] [-t_srs srs_def] [-to \"NAME=VALUE\"]\n"
        "    [-order n | -tps | -rpc | -geoloc] [-et err_threshold]\n"
        "    [-refine_gcps tolerance [minimum_gcps]]\n"
        "    [-te xmin ymin xmax ymax] [-tr xres yres] [-tap] [-ts width height]\n"
        "    [-wo \"NAME=VALUE\"] [-ot Byte/Int16/...] [-wt Byte/Int16]\n"
        "    [-srcnodata \"value [value...]\"] [-dstnodata \"value [value...]\"] -dstalpha\n" 
        "    [-r resampling_method] [-wm memory_in_mb] [-multi] [-q]\n"
        "    [-cutline datasource] [-cl layer] [-cwhere expression]\n"
        "    [-csql statement] [-cblend dist_in_pixels] [-crop_to_cutline]\n"
        "    [-of format] [-co \"NAME=VALUE\"]* [-overwrite]\n"
        "    [-nomd] [-cvmd meta_conflict_value] [-setci] [-oo NAME=VALUE]*\n"
        "    srcfile* dstfile\n"
        "\n"
        "Available resampling methods:\n"
        "    near (default), bilinear, cubic, cubicspline, lanczos, average, mode.\n" );

    if( pszErrorMsg != NULL )
        fprintf(stderr, "\nFAILURE: %s\n", pszErrorMsg);

    GDALExit( 1 );
}

/************************************************************************/
/*                             SanitizeSRS                              */
/************************************************************************/

char *SanitizeSRS( const char *pszUserInput )

{
    OGRSpatialReferenceH hSRS;
    char *pszResult = NULL;

    CPLErrorReset();
    
    hSRS = OSRNewSpatialReference( NULL );
    if( OSRSetFromUserInput( hSRS, pszUserInput ) == OGRERR_NONE )
        OSRExportToWkt( hSRS, &pszResult );
    else
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Translating source or target SRS failed:\n%s",
                  pszUserInput );
        GDALExit( 1 );
    }
    
    OSRDestroySpatialReference( hSRS );

    return pszResult;
}

/************************************************************************/
/*                                main()                                */
/************************************************************************/

#define CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(nExtraArg) \
    do { if (i + nExtraArg >= argc) \
        Usage(CPLSPrintf("%s option requires %d argument(s)", argv[i], nExtraArg)); } while(0)

int main( int argc, char ** argv )

{
    GDALDatasetH	hDstDS;
    const char         *pszFormat = "GTiff";
    int bFormatExplicitelySet = FALSE;
    char              **papszSrcFiles = NULL;
    char               *pszDstFilename = NULL;
    int                 bCreateOutput = FALSE, i;
    void               *hTransformArg, *hGenImgProjArg=NULL, *hApproxArg=NULL;
    char               **papszWarpOptions = NULL;
    double             dfErrorThreshold = 0.125;
    double             dfWarpMemoryLimit = 0.0;
    GDALTransformerFunc pfnTransformer = NULL;
    char                **papszCreateOptions = NULL;
    GDALDataType        eOutputType = GDT_Unknown, eWorkingType = GDT_Unknown; 
    GDALResampleAlg     eResampleAlg = GRA_NearestNeighbour;
    const char          *pszSrcNodata = NULL;
    const char          *pszDstNodata = NULL;
    int                 bMulti = FALSE;
    char                **papszTO = NULL;
    char                *pszCutlineDSName = NULL;
    char                *pszCLayer = NULL, *pszCWHERE = NULL, *pszCSQL = NULL;
    void                *hCutline = NULL;
    int                  bHasGotErr = FALSE;
    int                  bCropToCutline = FALSE;
    int                  bOverwrite = FALSE;
    int                  bCopyMetadata = TRUE;
    int                  bCopyBandInfo = TRUE;
    const char           *pszMDConflictValue = "*";
    int                  bSetColorInterpretation = FALSE;
    char               **papszOpenOptions = NULL;

    /* Check that we are running against at least GDAL 1.6 */
    /* Note to developers : if we use newer API, please change the requirement */
    if (atoi(GDALVersionInfo("VERSION_NUM")) < 1600)
    {
        fprintf(stderr, "At least, GDAL >= 1.6.0 is required for this version of %s, "
                "which was compiled against GDAL %s\n", argv[0], GDAL_RELEASE_NAME);
        GDALExit(1);
    }

    EarlySetConfigOptions(argc, argv);

/* -------------------------------------------------------------------- */
/*      Register standard GDAL drivers, and process generic GDAL        */
/*      command options.                                                */
/* -------------------------------------------------------------------- */
    GDALAllRegister();
    argc = GDALGeneralCmdLineProcessor( argc, &argv, 0 );
    if( argc < 1 )
        GDALExit( -argc );

/* -------------------------------------------------------------------- */
/*      Parse arguments.                                                */
/* -------------------------------------------------------------------- */
    for( i = 1; i < argc; i++ )
    {
        if( EQUAL(argv[i],"-tps") || EQUAL(argv[i],"-rpc") || EQUAL(argv[i],"-geoloc")  )
        {
            const char* pszMethod = CSLFetchNameValue(papszTO, "METHOD");
            if (pszMethod)
                fprintf(stderr, "Warning: only one METHOD can be used. Method %s is already defined.\n",
                        pszMethod);
            const char* pszMAX_GCP_ORDER = CSLFetchNameValue(papszTO, "MAX_GCP_ORDER");
            if (pszMAX_GCP_ORDER)
                fprintf(stderr, "Warning: only one METHOD can be used. -order %s option was specified, so it is likely that GCP_POLYNOMIAL was implied.\n",
                        pszMAX_GCP_ORDER);
        }

        if( EQUAL(argv[i], "--utility_version") )
        {
            printf("%s was compiled against GDAL %s and is running against GDAL %s\n",
                   argv[0], GDAL_RELEASE_NAME, GDALVersionInfo("RELEASE_NAME"));
            return 0;
        }
        else if( EQUAL(argv[i],"--help") )
            Usage();
        else if( EQUAL(argv[i],"-co") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            papszCreateOptions = CSLAddString( papszCreateOptions, argv[++i] );
            bCreateOutput = TRUE;
        }   
        else if( EQUAL(argv[i],"-wo") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            papszWarpOptions = CSLAddString( papszWarpOptions, argv[++i] );
        }   
        else if( EQUAL(argv[i],"-multi") )
        {
            bMulti = TRUE;
        }   
        else if( EQUAL(argv[i],"-q") || EQUAL(argv[i],"-quiet"))
        {
            bQuiet = TRUE;
        }   
        else if( EQUAL(argv[i],"-dstalpha") )
        {
            bEnableDstAlpha = TRUE;
        }
        else if( EQUAL(argv[i],"-srcalpha") )
        {
            bEnableSrcAlpha = TRUE;
        }
        else if( EQUAL(argv[i],"-of") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            pszFormat = argv[++i];
            bFormatExplicitelySet = TRUE;
            bCreateOutput = TRUE;
            if( EQUAL(pszFormat,"VRT") )
                bVRT = TRUE;
        }
        else if( EQUAL(argv[i],"-t_srs") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            char *pszSRS = SanitizeSRS(argv[++i]);
            papszTO = CSLSetNameValue( papszTO, "DST_SRS", pszSRS );
            CPLFree( pszSRS );
        }
        else if( EQUAL(argv[i],"-s_srs") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            char *pszSRS = SanitizeSRS(argv[++i]);
            papszTO = CSLSetNameValue( papszTO, "SRC_SRS", pszSRS );
            CPLFree( pszSRS );
        }
        else if( EQUAL(argv[i],"-order") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            const char* pszMethod = CSLFetchNameValue(papszTO, "METHOD");
            if (pszMethod)
                fprintf(stderr, "Warning: only one METHOD can be used. Method %s is already defined\n",
                        pszMethod);
            papszTO = CSLSetNameValue( papszTO, "MAX_GCP_ORDER", argv[++i] );
        }
        else if( EQUAL(argv[i],"-refine_gcps") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            papszTO = CSLSetNameValue( papszTO, "REFINE_TOLERANCE", argv[++i] );
            if(atof(argv[i]) < 0)
            {
                Usage("The tolerance for -refine_gcps may not be negative.");
            }
            if (i < argc-1 && atoi(argv[i+1]) >= 0 && isdigit(argv[i+1][0]))
            {
                papszTO = CSLSetNameValue( papszTO, "REFINE_MINIMUM_GCPS", argv[++i] );
            }
            else
            {
                papszTO = CSLSetNameValue( papszTO, "REFINE_MINIMUM_GCPS", "-1" );
            }
        }
        else if( EQUAL(argv[i],"-tps") )
        {
            papszTO = CSLSetNameValue( papszTO, "METHOD", "GCP_TPS" );
        }
        else if( EQUAL(argv[i],"-rpc") )
        {
            papszTO = CSLSetNameValue( papszTO, "METHOD", "RPC" );
        }
        else if( EQUAL(argv[i],"-geoloc") )
        {
            papszTO = CSLSetNameValue( papszTO, "METHOD", "GEOLOC_ARRAY" );
        }
        else if( EQUAL(argv[i],"-to") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            papszTO = CSLAddString( papszTO, argv[++i] );
        }
        else if( EQUAL(argv[i],"-et") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            dfErrorThreshold = CPLAtofM(argv[++i]);
        }
        else if( EQUAL(argv[i],"-wm") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            if( CPLAtofM(argv[i+1]) < 10000 )
                dfWarpMemoryLimit = CPLAtofM(argv[i+1]) * 1024 * 1024;
            else
                dfWarpMemoryLimit = CPLAtofM(argv[i+1]);
            i++;
        }
        else if( EQUAL(argv[i],"-srcnodata") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            pszSrcNodata = argv[++i];
        }
        else if( EQUAL(argv[i],"-dstnodata") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            pszDstNodata = argv[++i];
        }
        else if( EQUAL(argv[i],"-tr") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(2);
            dfXRes = CPLAtofM(argv[++i]);
            dfYRes = fabs(CPLAtofM(argv[++i]));
            if( dfXRes == 0 || dfYRes == 0 )
            {
                Usage("Wrong value for -tr parameters.");
            }
            bCreateOutput = TRUE;
        }
        else if( EQUAL(argv[i],"-tap") )
        {
            bTargetAlignedPixels = TRUE;
        }
        else if( EQUAL(argv[i],"-ot") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            int	iType;
            
            for( iType = 1; iType < GDT_TypeCount; iType++ )
            {
                if( GDALGetDataTypeName((GDALDataType)iType) != NULL
                    && EQUAL(GDALGetDataTypeName((GDALDataType)iType),
                             argv[i+1]) )
                {
                    eOutputType = (GDALDataType) iType;
                }
            }

            if( eOutputType == GDT_Unknown )
            {
                Usage(CPLSPrintf( "Unknown output pixel type: %s.", argv[i+1] ));
            }
            i++;
            bCreateOutput = TRUE;
        }
        else if( EQUAL(argv[i],"-wt") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            int	iType;
            
            for( iType = 1; iType < GDT_TypeCount; iType++ )
            {
                if( GDALGetDataTypeName((GDALDataType)iType) != NULL
                    && EQUAL(GDALGetDataTypeName((GDALDataType)iType),
                             argv[i+1]) )
                {
                    eWorkingType = (GDALDataType) iType;
                }
            }

            if( eWorkingType == GDT_Unknown )
            {
                Usage(CPLSPrintf( "Unknown working pixel type: %s.", argv[i+1] ));
            }
            i++;
        }
        else if( EQUAL(argv[i],"-ts") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(2);
            nForcePixels = atoi(argv[++i]);
            nForceLines = atoi(argv[++i]);
            bCreateOutput = TRUE;
        }
        else if( EQUAL(argv[i],"-te") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(4);
            dfMinX = CPLAtofM(argv[++i]);
            dfMinY = CPLAtofM(argv[++i]);
            dfMaxX = CPLAtofM(argv[++i]);
            dfMaxY = CPLAtofM(argv[++i]);
            bCreateOutput = TRUE;
        }
        else if( EQUAL(argv[i],"-rn") )
            eResampleAlg = GRA_NearestNeighbour;

        else if( EQUAL(argv[i],"-rb") )
            eResampleAlg = GRA_Bilinear;

        else if( EQUAL(argv[i],"-rc") )
            eResampleAlg = GRA_Cubic;

        else if( EQUAL(argv[i],"-rcs") )
            eResampleAlg = GRA_CubicSpline;

        else if( EQUAL(argv[i],"-rl") )
            eResampleAlg = GRA_Lanczos;

        else if( EQUAL(argv[i],"-ra") )
            eResampleAlg = GRA_Average;

        else if( EQUAL(argv[i],"-rm") )
            eResampleAlg = GRA_Mode;

        else if( EQUAL(argv[i],"-r") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            if ( EQUAL(argv[++i], "near") )
                eResampleAlg = GRA_NearestNeighbour;
            else if ( EQUAL(argv[i], "bilinear") )
                eResampleAlg = GRA_Bilinear;
            else if ( EQUAL(argv[i], "cubic") )
                eResampleAlg = GRA_Cubic;
            else if ( EQUAL(argv[i], "cubicspline") )
                eResampleAlg = GRA_CubicSpline;
            else if ( EQUAL(argv[i], "lanczos") )
                eResampleAlg = GRA_Lanczos;
            else if ( EQUAL(argv[i], "average") )
                eResampleAlg = GRA_Average;
            else if ( EQUAL(argv[i], "mode") )
                eResampleAlg = GRA_Mode;
            else
            {
                Usage(CPLSPrintf( "Unknown resampling method: \"%s\".", argv[i] ));
            }
        }

        else if( EQUAL(argv[i],"-cutline") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            pszCutlineDSName = argv[++i];
        }
        else if( EQUAL(argv[i],"-cwhere") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            pszCWHERE = argv[++i];
        }
        else if( EQUAL(argv[i],"-cl") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            pszCLayer = argv[++i];
        }
        else if( EQUAL(argv[i],"-csql") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            pszCSQL = argv[++i];
        }
        else if( EQUAL(argv[i],"-cblend") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            papszWarpOptions = 
                CSLSetNameValue( papszWarpOptions, 
                                 "CUTLINE_BLEND_DIST", argv[++i] );
        }
        else if( EQUAL(argv[i],"-crop_to_cutline")  )
        {
            bCropToCutline = TRUE;
            bCreateOutput = TRUE;
        }
        else if( EQUAL(argv[i],"-overwrite") )
            bOverwrite = TRUE;
        else if( EQUAL(argv[i],"-nomd") )
        {
            bCopyMetadata = FALSE;
            bCopyBandInfo = FALSE;
        }   
        else if( EQUAL(argv[i],"-cvmd") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            pszMDConflictValue = argv[++i];
        }
        else if( EQUAL(argv[i],"-setci") )
            bSetColorInterpretation = TRUE;
        else if( EQUAL(argv[i], "-oo") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            papszOpenOptions = CSLAddString( papszOpenOptions,
                                                argv[++i] );
        }
        else if( argv[i][0] == '-' )
            Usage(CPLSPrintf("Unknown option name '%s'", argv[i]));

        else 
            papszSrcFiles = CSLAddString( papszSrcFiles, argv[i] );
    }
/* -------------------------------------------------------------------- */
/*      Check that incompatible options are not used                    */
/* -------------------------------------------------------------------- */

    if ((nForcePixels != 0 || nForceLines != 0) && 
        (dfXRes != 0 && dfYRes != 0))
    {
        Usage("-tr and -ts options cannot be used at the same time.");
    }
    
    if (bTargetAlignedPixels && dfXRes == 0 && dfYRes == 0)
    {
        Usage("-tap option cannot be used without using -tr.");
    }

/* -------------------------------------------------------------------- */
/*      The last filename in the file list is really our destination    */
/*      file.                                                           */
/* -------------------------------------------------------------------- */
    if( CSLCount(papszSrcFiles) > 1 )
    {
        pszDstFilename = papszSrcFiles[CSLCount(papszSrcFiles)-1];
        papszSrcFiles[CSLCount(papszSrcFiles)-1] = NULL;
    }

    if( pszDstFilename == NULL )
        Usage("No target filename specified.");
        
    if( bVRT && CSLCount(papszSrcFiles) > 1 )
    {
        fprintf(stderr, "Warning: gdalwarp -of VRT just takes into account "
                        "the first source dataset.\nIf all source datasets "
                        "are in the same projection, try making a mosaic of\n"
                        "them with gdalbuildvrt, and use the resulting "
                        "VRT file as the input of\ngdalwarp -of VRT.\n");
    }

/* -------------------------------------------------------------------- */
/*      Does the output dataset already exist?                          */
/* -------------------------------------------------------------------- */

    /* FIXME ? source filename=target filename and -overwrite is definitely */
    /* an error. But I can't imagine of a valid case (without -overwrite), */
    /* where it would make sense. In doubt, let's keep that dubious possibility... */
    if ( CSLCount(papszSrcFiles) == 1 &&
         strcmp(papszSrcFiles[0], pszDstFilename) == 0 && bOverwrite)
    {
        fprintf(stderr, "Source and destination datasets must be different.\n");
        GDALExit( 1 );
    }

    CPLPushErrorHandler( CPLQuietErrorHandler );
    hDstDS = GDALOpen( pszDstFilename, GA_Update );
    CPLPopErrorHandler();

    if( hDstDS != NULL && bOverwrite )
    {
        GDALClose(hDstDS);
        hDstDS = NULL;
    }

    if( hDstDS != NULL && bCreateOutput )
    {
        fprintf( stderr, 
                 "Output dataset %s exists,\n"
                 "but some commandline options were provided indicating a new dataset\n"
                 "should be created.  Please delete existing dataset and run again.\n",
                 pszDstFilename );
        GDALExit( 1 );
    }

    /* Avoid overwriting an existing destination file that cannot be opened in */
    /* update mode with a new GTiff file */
    if ( hDstDS == NULL && !bOverwrite )
    {
        CPLPushErrorHandler( CPLQuietErrorHandler );
        hDstDS = GDALOpen( pszDstFilename, GA_ReadOnly );
        CPLPopErrorHandler();
        
        if (hDstDS)
        {
            fprintf( stderr, 
                     "Output dataset %s exists, but cannot be opened in update mode\n",
                     pszDstFilename );
            GDALClose(hDstDS);
            GDALExit( 1 );
        }
    }

/* -------------------------------------------------------------------- */
/*      If we have a cutline datasource read it and attach it in the    */
/*      warp options.                                                   */
/* -------------------------------------------------------------------- */
    if( pszCutlineDSName != NULL )
    {
        LoadCutline( pszCutlineDSName, pszCLayer, pszCWHERE, pszCSQL,
                     &hCutline );
    }

#ifdef OGR_ENABLED
    if ( bCropToCutline && hCutline != NULL )
    {
        OGRGeometryH hCutlineGeom = OGR_G_Clone( (OGRGeometryH) hCutline );
        OGRSpatialReferenceH hCutlineSRS = OGR_G_GetSpatialReference( hCutlineGeom );
        const char *pszThisTargetSRS = CSLFetchNameValue( papszTO, "DST_SRS" );
        OGRCoordinateTransformationH hCT = NULL;
        if (hCutlineSRS == NULL)
        {
            /* We suppose it is in target coordinates */
        }
        else if (pszThisTargetSRS != NULL)
        {
            OGRSpatialReferenceH hTargetSRS = OSRNewSpatialReference(NULL);
            if( OSRImportFromWkt( hTargetSRS, (char **)&pszThisTargetSRS ) != CE_None )
            {
                fprintf(stderr, "Cannot compute bounding box of cutline.\n");
                GDALExit(1);
            }

            hCT = OCTNewCoordinateTransformation(hCutlineSRS, hTargetSRS);

            OSRDestroySpatialReference(hTargetSRS);
        }
        else if (pszThisTargetSRS == NULL)
        {
            if (papszSrcFiles[0] != NULL)
            {
                GDALDatasetH hSrcDS = GDALOpenEx( papszSrcFiles[0], GDAL_OF_RASTER, NULL,
                           (const char* const* )papszOpenOptions, NULL );
                if (hSrcDS == NULL)
                {
                    fprintf(stderr, "Cannot compute bounding box of cutline.\n");
                    GDALExit(1);
                }

                OGRSpatialReferenceH  hRasterSRS = NULL;
                const char *pszProjection = NULL;

                if( GDALGetProjectionRef( hSrcDS ) != NULL
                    && strlen(GDALGetProjectionRef( hSrcDS )) > 0 )
                    pszProjection = GDALGetProjectionRef( hSrcDS );
                else if( GDALGetGCPProjection( hSrcDS ) != NULL )
                    pszProjection = GDALGetGCPProjection( hSrcDS );

                if( pszProjection == NULL )
                {
                    fprintf(stderr, "Cannot compute bounding box of cutline.\n");
                    GDALExit(1);
                }

                hRasterSRS = OSRNewSpatialReference(NULL);
                if( OSRImportFromWkt( hRasterSRS, (char **)&pszProjection ) != CE_None )
                {
                    fprintf(stderr, "Cannot compute bounding box of cutline.\n");
                    GDALExit(1);
                }

                hCT = OCTNewCoordinateTransformation(hCutlineSRS, hRasterSRS);

                OSRDestroySpatialReference(hRasterSRS);

                GDALClose(hSrcDS);
            }
            else
            {
                fprintf(stderr, "Cannot compute bounding box of cutline.\n");
                GDALExit(1);
            }
        }

        if (hCT)
        {
            OGR_G_Transform( hCutlineGeom, hCT );

            OCTDestroyCoordinateTransformation(hCT);
        }

        OGREnvelope sEnvelope;
        OGR_G_GetEnvelope(hCutlineGeom, &sEnvelope);

        dfMinX = sEnvelope.MinX;
        dfMinY = sEnvelope.MinY;
        dfMaxX = sEnvelope.MaxX;
        dfMaxY = sEnvelope.MaxY;
        
        OGR_G_DestroyGeometry(hCutlineGeom);
    }
#endif
    
/* -------------------------------------------------------------------- */
/*      If not, we need to create it.                                   */
/* -------------------------------------------------------------------- */
    int   bInitDestSetForFirst = FALSE;

    void* hUniqueTransformArg = NULL;
    GDALDatasetH hUniqueSrcDS = NULL;

    const char* pszWarpThreads = CSLFetchNameValue(papszWarpOptions, "NUM_THREADS");
    if( pszWarpThreads != NULL )
        papszTO = CSLSetNameValue(papszTO, "NUM_THREADS", pszWarpThreads);

    if( hDstDS == NULL )
    {
        if (!bQuiet && !bFormatExplicitelySet)
            CheckExtensionConsistency(pszDstFilename, pszFormat);

        hDstDS = GDALWarpCreateOutput( papszSrcFiles, pszDstFilename,pszFormat,
                                       papszTO, &papszCreateOptions, 
                                       eOutputType, &hUniqueTransformArg,
                                       &hUniqueSrcDS, bSetColorInterpretation,
                                       papszOpenOptions);
        bCreateOutput = TRUE;

        if( CSLFetchNameValue( papszWarpOptions, "INIT_DEST" ) == NULL 
            && pszDstNodata == NULL )
        {
            papszWarpOptions = CSLSetNameValue(papszWarpOptions,
                                               "INIT_DEST", "0");
            bInitDestSetForFirst = TRUE;
        }
        else if( CSLFetchNameValue( papszWarpOptions, "INIT_DEST" ) == NULL )
        {
            papszWarpOptions = CSLSetNameValue(papszWarpOptions,
                                               "INIT_DEST", "NO_DATA" );
            bInitDestSetForFirst = TRUE;
        }

        CSLDestroy( papszCreateOptions );
        papszCreateOptions = NULL;
    }
 
    if( hDstDS == NULL )
        GDALExit( 1 );

/* -------------------------------------------------------------------- */
/*      Loop over all source files, processing each in turn.            */
/* -------------------------------------------------------------------- */
    int iSrc;

    for( iSrc = 0; papszSrcFiles[iSrc] != NULL; iSrc++ )
    {
        GDALDatasetH hSrcDS;
       
/* -------------------------------------------------------------------- */
/*      Open this file.                                                 */
/* -------------------------------------------------------------------- */
        if (hUniqueSrcDS)
            hSrcDS = hUniqueSrcDS;
        else
            hSrcDS = GDALOpenEx( papszSrcFiles[iSrc], GDAL_OF_RASTER, NULL,
                           (const char* const* )papszOpenOptions, NULL );
    
        if( hSrcDS == NULL )
            GDALExit( 2 );

/* -------------------------------------------------------------------- */
/*      Check that there's at least one raster band                     */
/* -------------------------------------------------------------------- */
        if ( GDALGetRasterCount(hSrcDS) == 0 )
        {     
            fprintf(stderr, "Input file %s has no raster bands.\n", papszSrcFiles[iSrc] );
            GDALExit( 1 );
        }

        if( !bQuiet )
            printf( "Processing input file %s.\n", papszSrcFiles[iSrc] );

/* -------------------------------------------------------------------- */
/*      Get the metadata of the first source DS and copy it to the      */
/*      destination DS. Copy Band-level metadata and other info, only   */
/*      if source and destination band count are equal. Any values that */
/*      conflict between source datasets are set to pszMDConflictValue. */
/* -------------------------------------------------------------------- */
        if ( bCopyMetadata )
        {
            char **papszMetadata = NULL;
            const char *pszSrcInfo = NULL;
            const char *pszDstInfo = NULL;
            GDALRasterBandH hSrcBand = NULL;
            GDALRasterBandH hDstBand = NULL;

            /* copy metadata from first dataset */
            if ( iSrc == 0 )
            {
                CPLDebug("WARP", "Copying metadata from first source to destination dataset");
                /* copy dataset-level metadata */
                papszMetadata = GDALGetMetadata( hSrcDS, NULL );
                if ( CSLCount(papszMetadata) > 0 ) {
                    if ( GDALSetMetadata( hDstDS, papszMetadata, NULL ) != CE_None )
                         fprintf( stderr, 
                                  "Warning: error copying metadata to destination dataset.\n" );
                }
                /* copy band-level metadata and other info */
                if ( GDALGetRasterCount( hSrcDS ) == GDALGetRasterCount( hDstDS ) )              
                {
                    for ( int iBand = 0; iBand < GDALGetRasterCount( hSrcDS ); iBand++ )
                    {
                        hSrcBand = GDALGetRasterBand( hSrcDS, iBand + 1 );
                        hDstBand = GDALGetRasterBand( hDstDS, iBand + 1 );
                        /* copy metadata, except stats (#5319) */
                        papszMetadata = GDALGetMetadata( hSrcBand, NULL);              
                        if ( CSLCount(papszMetadata) > 0 )
                        {
                            //GDALSetMetadata( hDstBand, papszMetadata, NULL );       
                            char** papszMetadataNew = NULL;
                            for( int i = 0; papszMetadata != NULL && papszMetadata[i] != NULL; i++ )
                            {
                                if (strncmp(papszMetadata[i], "STATISTICS_", 11) != 0)
                                    papszMetadataNew = CSLAddString(papszMetadataNew, papszMetadata[i]);
                            }
                            GDALSetMetadata( hDstBand, papszMetadataNew, NULL );
                            CSLDestroy(papszMetadataNew);
                        }
                        /* copy other info (Description, Unit Type) - what else? */
                        if ( bCopyBandInfo ) {
                            pszSrcInfo = GDALGetDescription( hSrcBand );
                            if(  pszSrcInfo != NULL && strlen(pszSrcInfo) > 0 )
                                GDALSetDescription( hDstBand, pszSrcInfo );  
                            pszSrcInfo = GDALGetRasterUnitType( hSrcBand );
                            if(  pszSrcInfo != NULL && strlen(pszSrcInfo) > 0 )
                                GDALSetRasterUnitType( hDstBand, pszSrcInfo );  
                        }
                    }
                }
            }
            /* remove metadata that conflicts between datasets */
            else 
            {
                CPLDebug("WARP", 
                         "Removing conflicting metadata from destination dataset (source #%d)", iSrc );
                /* remove conflicting dataset-level metadata */
                RemoveConflictingMetadata( hDstDS, GDALGetMetadata( hSrcDS, NULL ), pszMDConflictValue );
                
                /* remove conflicting copy band-level metadata and other info */
                if ( GDALGetRasterCount( hSrcDS ) == GDALGetRasterCount( hDstDS ) )              
                {
                    for ( int iBand = 0; iBand < GDALGetRasterCount( hSrcDS ); iBand++ )
                    {
                        hSrcBand = GDALGetRasterBand( hSrcDS, iBand + 1 );
                        hDstBand = GDALGetRasterBand( hDstDS, iBand + 1 );
                        /* remove conflicting metadata */
                        RemoveConflictingMetadata( hDstBand, GDALGetMetadata( hSrcBand, NULL ), pszMDConflictValue );
                        /* remove conflicting info */
                        if ( bCopyBandInfo ) {
                            pszSrcInfo = GDALGetDescription( hSrcBand );
                            pszDstInfo = GDALGetDescription( hDstBand );
                            if( ! ( pszSrcInfo != NULL && strlen(pszSrcInfo) > 0  &&
                                    pszDstInfo != NULL && strlen(pszDstInfo) > 0  &&
                                    EQUAL( pszSrcInfo, pszDstInfo ) ) )
                                GDALSetDescription( hDstBand, "" );  
                            pszSrcInfo = GDALGetRasterUnitType( hSrcBand );
                            pszDstInfo = GDALGetRasterUnitType( hDstBand );
                            if( ! ( pszSrcInfo != NULL && strlen(pszSrcInfo) > 0  &&
                                    pszDstInfo != NULL && strlen(pszDstInfo) > 0  &&
                                    EQUAL( pszSrcInfo, pszDstInfo ) ) )
                                GDALSetRasterUnitType( hDstBand, "" );  
                        }
                    }
                }
            }          
        }

/* -------------------------------------------------------------------- */
/*      Warns if the file has a color table and something more          */
/*      complicated than nearest neighbour resampling is asked          */
/* -------------------------------------------------------------------- */
 
        if ( eResampleAlg != GRA_NearestNeighbour &&
             eResampleAlg != GRA_Mode &&
             GDALGetRasterColorTable(GDALGetRasterBand(hSrcDS, 1)) != NULL)
        {
            if( !bQuiet )
                fprintf( stderr, "Warning: Input file %s has a color table, which will likely lead to "
                        "bad results when using a resampling method other than "
                        "nearest neighbour or mode. Converting the dataset prior to 24/32 bit "
                        "is advised.\n", papszSrcFiles[iSrc] );
        }

/* -------------------------------------------------------------------- */
/*      Do we have a source alpha band?                                 */
/* -------------------------------------------------------------------- */
        if( GDALGetRasterColorInterpretation( 
                GDALGetRasterBand(hSrcDS,GDALGetRasterCount(hSrcDS)) ) 
            == GCI_AlphaBand 
            && !bEnableSrcAlpha )
        {
            bEnableSrcAlpha = TRUE;
            if( !bQuiet )
                printf( "Using band %d of source image as alpha.\n", 
                        GDALGetRasterCount(hSrcDS) );
        }

/* -------------------------------------------------------------------- */
/*      Create a transformation object from the source to               */
/*      destination coordinate system.                                  */
/* -------------------------------------------------------------------- */
        if (hUniqueTransformArg)
            hTransformArg = hGenImgProjArg = hUniqueTransformArg;
        else
            hTransformArg = hGenImgProjArg =
                GDALCreateGenImgProjTransformer2( hSrcDS, hDstDS, papszTO );
        
        if( hTransformArg == NULL )
            GDALExit( 1 );
        
        pfnTransformer = GDALGenImgProjTransform;

/* -------------------------------------------------------------------- */
/*      Warp the transformer with a linear approximator unless the      */
/*      acceptable error is zero.                                       */
/* -------------------------------------------------------------------- */
        if( dfErrorThreshold != 0.0 )
        {
            hTransformArg = hApproxArg = 
                GDALCreateApproxTransformer( GDALGenImgProjTransform, 
                                             hGenImgProjArg, dfErrorThreshold);
            pfnTransformer = GDALApproxTransform;
        }

/* -------------------------------------------------------------------- */
/*      Clear temporary INIT_DEST settings after the first image.       */
/* -------------------------------------------------------------------- */
        if( bInitDestSetForFirst && iSrc == 1 )
            papszWarpOptions = CSLSetNameValue( papszWarpOptions, 
                                                "INIT_DEST", NULL );

/* -------------------------------------------------------------------- */
/*      Setup warp options.                                             */
/* -------------------------------------------------------------------- */
        GDALWarpOptions *psWO = GDALCreateWarpOptions();

        psWO->papszWarpOptions = CSLDuplicate(papszWarpOptions);
        psWO->eWorkingDataType = eWorkingType;
        psWO->eResampleAlg = eResampleAlg;

        psWO->hSrcDS = hSrcDS;
        psWO->hDstDS = hDstDS;

        psWO->pfnTransformer = pfnTransformer;
        psWO->pTransformerArg = hTransformArg;

        if( !bQuiet )
            psWO->pfnProgress = GDALTermProgress;

        if( dfWarpMemoryLimit != 0.0 )
            psWO->dfWarpMemoryLimit = dfWarpMemoryLimit;

/* -------------------------------------------------------------------- */
/*      Setup band mapping.                                             */
/* -------------------------------------------------------------------- */
        if( bEnableSrcAlpha )
            psWO->nBandCount = GDALGetRasterCount(hSrcDS) - 1;
        else
            psWO->nBandCount = GDALGetRasterCount(hSrcDS);

        psWO->panSrcBands = (int *) CPLMalloc(psWO->nBandCount*sizeof(int));
        psWO->panDstBands = (int *) CPLMalloc(psWO->nBandCount*sizeof(int));

        for( i = 0; i < psWO->nBandCount; i++ )
        {
            psWO->panSrcBands[i] = i+1;
            psWO->panDstBands[i] = i+1;
        }

/* -------------------------------------------------------------------- */
/*      Setup alpha bands used if any.                                  */
/* -------------------------------------------------------------------- */
        if( bEnableSrcAlpha )
            psWO->nSrcAlphaBand = GDALGetRasterCount(hSrcDS);

        if( !bEnableDstAlpha 
            && GDALGetRasterCount(hDstDS) == psWO->nBandCount+1 
            && GDALGetRasterColorInterpretation( 
                GDALGetRasterBand(hDstDS,GDALGetRasterCount(hDstDS))) 
            == GCI_AlphaBand )
        {
            if( !bQuiet )
                printf( "Using band %d of destination image as alpha.\n", 
                        GDALGetRasterCount(hDstDS) );
                
            bEnableDstAlpha = TRUE;
        }

        if( bEnableDstAlpha )
            psWO->nDstAlphaBand = GDALGetRasterCount(hDstDS);

/* -------------------------------------------------------------------- */
/*      Setup NODATA options.                                           */
/* -------------------------------------------------------------------- */
        if( pszSrcNodata != NULL && !EQUAL(pszSrcNodata,"none") )
        {
            char **papszTokens = CSLTokenizeString( pszSrcNodata );
            int  nTokenCount = CSLCount(papszTokens);

            psWO->padfSrcNoDataReal = (double *) 
                CPLMalloc(psWO->nBandCount*sizeof(double));
            psWO->padfSrcNoDataImag = (double *) 
                CPLMalloc(psWO->nBandCount*sizeof(double));

            for( i = 0; i < psWO->nBandCount; i++ )
            {
                if( i < nTokenCount )
                {
                    CPLStringToComplex( papszTokens[i], 
                                        psWO->padfSrcNoDataReal + i,
                                        psWO->padfSrcNoDataImag + i );
                }
                else
                {
                    psWO->padfSrcNoDataReal[i] = psWO->padfSrcNoDataReal[i-1];
                    psWO->padfSrcNoDataImag[i] = psWO->padfSrcNoDataImag[i-1];
                }
            }

            CSLDestroy( papszTokens );

            psWO->papszWarpOptions = CSLSetNameValue(psWO->papszWarpOptions,
                                               "UNIFIED_SRC_NODATA", "YES" );
        }

/* -------------------------------------------------------------------- */
/*      If -srcnodata was not specified, but the data has nodata        */
/*      values, use them.                                               */
/* -------------------------------------------------------------------- */
        if( pszSrcNodata == NULL )
        {
            int bHaveNodata = FALSE;
            double dfReal = 0.0;

            for( i = 0; !bHaveNodata && i < psWO->nBandCount; i++ )
            {
                GDALRasterBandH hBand = GDALGetRasterBand( hSrcDS, i+1 );
                dfReal = GDALGetRasterNoDataValue( hBand, &bHaveNodata );
            }

            if( bHaveNodata )
            {
                if( !bQuiet )
                {
                    if (CPLIsNan(dfReal))
                        printf( "Using internal nodata values (e.g. nan) for image %s.\n",
                                papszSrcFiles[iSrc] );
                    else
                        printf( "Using internal nodata values (e.g. %g) for image %s.\n",
                                dfReal, papszSrcFiles[iSrc] );
                }
                psWO->padfSrcNoDataReal = (double *) 
                    CPLMalloc(psWO->nBandCount*sizeof(double));
                psWO->padfSrcNoDataImag = (double *) 
                    CPLMalloc(psWO->nBandCount*sizeof(double));
                
                for( i = 0; i < psWO->nBandCount; i++ )
                {
                    GDALRasterBandH hBand = GDALGetRasterBand( hSrcDS, i+1 );

                    dfReal = GDALGetRasterNoDataValue( hBand, &bHaveNodata );

                    if( bHaveNodata )
                    {
                        psWO->padfSrcNoDataReal[i] = dfReal;
                        psWO->padfSrcNoDataImag[i] = 0.0;
                    }
                    else
                    {
                        psWO->padfSrcNoDataReal[i] = -123456.789;
                        psWO->padfSrcNoDataImag[i] = 0.0;
                    }
                }
            }
        }

/* -------------------------------------------------------------------- */
/*      If the output dataset was created, and we have a destination    */
/*      nodata value, go through marking the bands with the information.*/
/* -------------------------------------------------------------------- */
        if( pszDstNodata != NULL )
        {
            char **papszTokens = CSLTokenizeString( pszDstNodata );
            int  nTokenCount = CSLCount(papszTokens);
            int bDstNoDataNone = TRUE;

            psWO->padfDstNoDataReal = (double *) 
                CPLMalloc(psWO->nBandCount*sizeof(double));
            psWO->padfDstNoDataImag = (double *) 
                CPLMalloc(psWO->nBandCount*sizeof(double));

            for( i = 0; i < psWO->nBandCount; i++ )
            {
                if( i < nTokenCount )
                {
                    if ( papszTokens[i] != NULL && EQUAL(papszTokens[i],"none") )
                    {
                        CPLDebug( "WARP", "dstnodata of band %d not set", i );
                        bDstNoDataNone = TRUE;
                        continue;
                    }
                    else if ( papszTokens[i] == NULL ) // this shouldn't happen, but just in case
                    {
                        fprintf( stderr, "Error parsing dstnodata arg #%d\n", i );
                        bDstNoDataNone = TRUE;
                        continue;
                    }
                    CPLStringToComplex( papszTokens[i], 
                                        psWO->padfDstNoDataReal + i,
                                        psWO->padfDstNoDataImag + i );
                    bDstNoDataNone = FALSE;
                    CPLDebug( "WARP", "dstnodata of band %d set to %f", i, psWO->padfDstNoDataReal[i] );
                }
                else
                {
                    if ( ! bDstNoDataNone )
                    {                    
                        psWO->padfDstNoDataReal[i] = psWO->padfDstNoDataReal[i-1];
                        psWO->padfDstNoDataImag[i] = psWO->padfDstNoDataImag[i-1];
                        CPLDebug( "WARP", "dstnodata of band %d set from previous band", i );
                    }
                    else
                    {
                        CPLDebug( "WARP", "dstnodata value of band %d not set", i );
                        continue;
                    }
                }
                
                GDALRasterBandH hBand = GDALGetRasterBand( hDstDS, i+1 );
                int bClamped = FALSE, bRounded = FALSE;

#define CLAMP(val,type,minval,maxval) \
    do { if (val < minval) { bClamped = TRUE; val = minval; } \
    else if (val > maxval) { bClamped = TRUE; val = maxval; } \
    else if (val != (type)val) { bRounded = TRUE; val = (type)(val + 0.5); } } \
    while(0)

                switch(GDALGetRasterDataType(hBand))
                {
                    case GDT_Byte:
                        CLAMP(psWO->padfDstNoDataReal[i], GByte,
                              0.0, 255.0);
                        break;
                    case GDT_Int16:
                        CLAMP(psWO->padfDstNoDataReal[i], GInt16,
                              -32768.0, 32767.0);
                        break;
                    case GDT_UInt16:
                        CLAMP(psWO->padfDstNoDataReal[i], GUInt16,
                              0.0, 65535.0);
                        break;
                    case GDT_Int32:
                        CLAMP(psWO->padfDstNoDataReal[i], GInt32,
                              -2147483648.0, 2147483647.0);
                        break;
                    case GDT_UInt32:
                        CLAMP(psWO->padfDstNoDataReal[i], GUInt32,
                              0.0, 4294967295.0);
                        break;
                    default:
                        break;
                }
                    
                if (bClamped)
                {
                    printf( "for band %d, destination nodata value has been clamped "
                           "to %.0f, the original value being out of range.\n",
                           i + 1, psWO->padfDstNoDataReal[i]);
                }
                else if(bRounded)
                {
                    printf("for band %d, destination nodata value has been rounded "
                           "to %.0f, %s being an integer datatype.\n",
                           i + 1, psWO->padfDstNoDataReal[i],
                           GDALGetDataTypeName(GDALGetRasterDataType(hBand)));
                }

                if( bCreateOutput )
                {
                    GDALSetRasterNoDataValue( 
                        GDALGetRasterBand( hDstDS, psWO->panDstBands[i] ), 
                        psWO->padfDstNoDataReal[i] );
                }
            }

            CSLDestroy( papszTokens );
        }
        /* else try to fill dstNoData from source bands */
        else if ( psWO->padfSrcNoDataReal != NULL )
        {
            psWO->padfDstNoDataReal = (double *) 
                CPLMalloc(psWO->nBandCount*sizeof(double));
            psWO->padfDstNoDataImag = (double *) 
                CPLMalloc(psWO->nBandCount*sizeof(double));

            if( !bQuiet )
                printf( "Copying nodata values from source %s to destination %s.\n",
                        papszSrcFiles[iSrc], pszDstFilename );

            for( i = 0; i < psWO->nBandCount; i++ )
            {
                int bHaveNodata = FALSE;
                
                GDALRasterBandH hBand = GDALGetRasterBand( hSrcDS, i+1 );
                GDALGetRasterNoDataValue( hBand, &bHaveNodata );

                CPLDebug("WARP", "band=%d bHaveNodata=%d", i, bHaveNodata);
                if( bHaveNodata )
                {
                    psWO->padfDstNoDataReal[i] = psWO->padfSrcNoDataReal[i];
                    psWO->padfDstNoDataImag[i] = psWO->padfSrcNoDataImag[i];
                    CPLDebug("WARP", "srcNoData=%f dstNoData=%f", 
                             psWO->padfSrcNoDataReal[i], psWO->padfDstNoDataReal[i] );
                }

                if( bCreateOutput )
                {
                    CPLDebug("WARP", "calling GDALSetRasterNoDataValue() for band#%d", i );
                    GDALSetRasterNoDataValue( 
                        GDALGetRasterBand( hDstDS, psWO->panDstBands[i] ), 
                        psWO->padfDstNoDataReal[i] );
                }
            }

            if( bInitDestSetForFirst && iSrc == 0 )
            {
                /* As we didn't know at the beginning if there was source nodata */
                /* we have initialized INIT_DEST=0. Override this with NO_DATA now */
                psWO->papszWarpOptions = CSLSetNameValue(psWO->papszWarpOptions,
                                                   "INIT_DEST", "NO_DATA" );
            }
        }

/* -------------------------------------------------------------------- */
/*      If we have a cutline, transform it into the source              */
/*      pixel/line coordinate system and insert into warp options.      */
/* -------------------------------------------------------------------- */
        if( hCutline != NULL )
        {
            TransformCutlineToSource( hSrcDS, hCutline, 
                                      &(psWO->papszWarpOptions), 
                                      papszTO );
        }

/* -------------------------------------------------------------------- */
/*      If we are producing VRT output, then just initialize it with    */
/*      the warp options and write out now rather than proceeding       */
/*      with the operations.                                            */
/* -------------------------------------------------------------------- */
        if( bVRT )
        {
            if( GDALInitializeWarpedVRT( hDstDS, psWO ) != CE_None )
                GDALExit( 1 );

            GDALClose( hDstDS );
            GDALClose( hSrcDS );

            /* The warped VRT will clean itself the transformer used */
            /* So we have only to destroy the hGenImgProjArg if we */
            /* have wrapped it inside the hApproxArg */
            if (pfnTransformer == GDALApproxTransform)
            {
                if( hGenImgProjArg != NULL )
                    GDALDestroyGenImgProjTransformer( hGenImgProjArg );
            }

            GDALDestroyWarpOptions( psWO );

            CPLFree( pszDstFilename );
            CSLDestroy( argv );
            CSLDestroy( papszSrcFiles );
            CSLDestroy( papszWarpOptions );
            CSLDestroy( papszTO );
            CSLDestroy( papszOpenOptions );
    
            GDALDumpOpenDatasets( stderr );
        
            GDALDestroyDriverManager();
        
            return 0;
        }

/* -------------------------------------------------------------------- */
/*      Initialize and execute the warp.                                */
/* -------------------------------------------------------------------- */
        GDALWarpOperation oWO;

        if( oWO.Initialize( psWO ) == CE_None )
        {
            CPLErr eErr;
            if( bMulti )
                eErr = oWO.ChunkAndWarpMulti( 0, 0, 
                                       GDALGetRasterXSize( hDstDS ),
                                       GDALGetRasterYSize( hDstDS ) );
            else
                eErr = oWO.ChunkAndWarpImage( 0, 0, 
                                       GDALGetRasterXSize( hDstDS ),
                                       GDALGetRasterYSize( hDstDS ) );
            if (eErr != CE_None)
                bHasGotErr = TRUE;
        }

/* -------------------------------------------------------------------- */
/*      Cleanup                                                         */
/* -------------------------------------------------------------------- */
        if( hApproxArg != NULL )
            GDALDestroyApproxTransformer( hApproxArg );
        
        if( hGenImgProjArg != NULL )
            GDALDestroyGenImgProjTransformer( hGenImgProjArg );
        
        GDALDestroyWarpOptions( psWO );

        GDALClose( hSrcDS );
    }

/* -------------------------------------------------------------------- */
/*      Final Cleanup.                                                  */
/* -------------------------------------------------------------------- */
    CPLErrorReset();
    GDALFlushCache( hDstDS );
    if( CPLGetLastErrorType() == CE_Failure )
        bHasGotErr = TRUE;
    GDALClose( hDstDS );
    
    CPLFree( pszDstFilename );
    CSLDestroy( argv );
    CSLDestroy( papszSrcFiles );
    CSLDestroy( papszWarpOptions );
    CSLDestroy( papszTO );
    CSLDestroy( papszOpenOptions );

    GDALDumpOpenDatasets( stderr );

    GDALDestroyDriverManager();
    
#ifdef OGR_ENABLED
    if( hCutline != NULL )
        OGR_G_DestroyGeometry( (OGRGeometryH) hCutline );
    OGRCleanupAll();
#endif

    return (bHasGotErr) ? 1 : 0;
}

/************************************************************************/
/*                        GDALWarpCreateOutput()                        */
/*                                                                      */
/*      Create the output file based on various commandline options,    */
/*      and the input file.                                             */
/*      If there's just one source file, then *phTransformArg and       */
/*      *phSrcDS will be set, in order them to be reused by main        */
/*      function. This saves dataset re-opening, and above all transform*/
/*      recomputation, which can be expensive in the -tps case          */
/************************************************************************/

static GDALDatasetH 
GDALWarpCreateOutput( char **papszSrcFiles, const char *pszFilename, 
                      const char *pszFormat, char **papszTO, 
                      char ***ppapszCreateOptions, GDALDataType eDT,
                      void ** phTransformArg,
                      GDALDatasetH* phSrcDS,
                      int bSetColorInterpretation,
                      char** papszOpenOptions)


{
    GDALDriverH hDriver;
    GDALDatasetH hDstDS;
    void *hTransformArg;
    GDALColorTableH hCT = NULL;
    double dfWrkMinX=0, dfWrkMaxX=0, dfWrkMinY=0, dfWrkMaxY=0;
    double dfWrkResX=0, dfWrkResY=0;
    int nDstBandCount = 0;
    std::vector<GDALColorInterp> apeColorInterpretations;

    /* If (-ts and -te) or (-tr and -te) are specified, we don't need to compute the suggested output extent */
    int    bNeedsSuggestedWarpOutput = 
                  !( ((nForcePixels != 0 && nForceLines != 0) || (dfXRes != 0 && dfYRes != 0)) &&
                     !(dfMinX == 0.0 && dfMinY == 0.0 && dfMaxX == 0.0 && dfMaxY == 0.0) );

    *phTransformArg = NULL;
    *phSrcDS = NULL;

/* -------------------------------------------------------------------- */
/*      Find the output driver.                                         */
/* -------------------------------------------------------------------- */
    hDriver = GDALGetDriverByName( pszFormat );
    if( hDriver == NULL 
        || GDALGetMetadataItem( hDriver, GDAL_DCAP_CREATE, NULL ) == NULL )
    {
        int	iDr;
        
        printf( "Output driver `%s' not recognised or does not support\n", 
                pszFormat );
        printf( "direct output file creation.  The following format drivers are configured\n"
                "and support direct output:\n" );

        for( iDr = 0; iDr < GDALGetDriverCount(); iDr++ )
        {
            GDALDriverH hDriver = GDALGetDriver(iDr);

            if( GDALGetMetadataItem( hDriver, GDAL_DCAP_RASTER, NULL) != NULL &&
                GDALGetMetadataItem( hDriver, GDAL_DCAP_CREATE, NULL) != NULL )
            {
                printf( "  %s: %s\n",
                        GDALGetDriverShortName( hDriver  ),
                        GDALGetDriverLongName( hDriver ) );
            }
        }
        printf( "\n" );
        GDALExit( 1 );
    }

/* -------------------------------------------------------------------- */
/*      For virtual output files, we have to set a special subclass     */
/*      of dataset to create.                                           */
/* -------------------------------------------------------------------- */
    if( bVRT )
        *ppapszCreateOptions = 
            CSLSetNameValue( *ppapszCreateOptions, "SUBCLASS", 
                             "VRTWarpedDataset" );

/* -------------------------------------------------------------------- */
/*      Loop over all input files to collect extents.                   */
/* -------------------------------------------------------------------- */
    int     iSrc;
    char    *pszThisTargetSRS = (char*)CSLFetchNameValue( papszTO, "DST_SRS" );
    if( pszThisTargetSRS != NULL )
        pszThisTargetSRS = CPLStrdup( pszThisTargetSRS );

    for( iSrc = 0; papszSrcFiles[iSrc] != NULL; iSrc++ )
    {
        GDALDatasetH hSrcDS;
        const char *pszThisSourceSRS = CSLFetchNameValue(papszTO,"SRC_SRS");

        hSrcDS = GDALOpenEx( papszSrcFiles[iSrc], GDAL_OF_RASTER, NULL,
                                (const char* const* )papszOpenOptions, NULL );
        if( hSrcDS == NULL )
            GDALExit( 1 );

/* -------------------------------------------------------------------- */
/*      Check that there's at least one raster band                     */
/* -------------------------------------------------------------------- */
        if ( GDALGetRasterCount(hSrcDS) == 0 )
        {
            fprintf(stderr, "Input file %s has no raster bands.\n", papszSrcFiles[iSrc] );
            GDALExit( 1 );
        }

        if( eDT == GDT_Unknown )
            eDT = GDALGetRasterDataType(GDALGetRasterBand(hSrcDS,1));

/* -------------------------------------------------------------------- */
/*      If we are processing the first file, and it has a color         */
/*      table, then we will copy it to the destination file.            */
/* -------------------------------------------------------------------- */
        if( iSrc == 0 )
        {
            nDstBandCount = GDALGetRasterCount(hSrcDS);
            hCT = GDALGetRasterColorTable( GDALGetRasterBand(hSrcDS,1) );
            if( hCT != NULL )
            {
                hCT = GDALCloneColorTable( hCT );
                if( !bQuiet )
                    printf( "Copying color table from %s to new file.\n", 
                            papszSrcFiles[iSrc] );
            }

            for(int iBand = 0; iBand < nDstBandCount; iBand++)
            {
                apeColorInterpretations.push_back(
                    GDALGetRasterColorInterpretation(GDALGetRasterBand(hSrcDS,iBand+1)) );
            }
        }

/* -------------------------------------------------------------------- */
/*      Get the sourcesrs from the dataset, if not set already.         */
/* -------------------------------------------------------------------- */
        if( pszThisSourceSRS == NULL )
        {
            const char *pszMethod = CSLFetchNameValue( papszTO, "METHOD" );

            if( GDALGetProjectionRef( hSrcDS ) != NULL 
                && strlen(GDALGetProjectionRef( hSrcDS )) > 0
                && (pszMethod == NULL || EQUAL(pszMethod,"GEOTRANSFORM")) )
                pszThisSourceSRS = GDALGetProjectionRef( hSrcDS );
            
            else if( GDALGetGCPProjection( hSrcDS ) != NULL
                     && strlen(GDALGetGCPProjection(hSrcDS)) > 0 
                     && GDALGetGCPCount( hSrcDS ) > 1 
                     && (pszMethod == NULL || EQUALN(pszMethod,"GCP_",4)) )
                pszThisSourceSRS = GDALGetGCPProjection( hSrcDS );
            else if( pszMethod != NULL && EQUAL(pszMethod,"RPC") )
                pszThisSourceSRS = SRS_WKT_WGS84;
            else
                pszThisSourceSRS = "";
        }

        if( pszThisTargetSRS == NULL )
            pszThisTargetSRS = CPLStrdup( pszThisSourceSRS );
        
/* -------------------------------------------------------------------- */
/*      Create a transformation object from the source to               */
/*      destination coordinate system.                                  */
/* -------------------------------------------------------------------- */
        hTransformArg = 
            GDALCreateGenImgProjTransformer2( hSrcDS, NULL, papszTO );
        
        if( hTransformArg == NULL )
        {
            CPLFree( pszThisTargetSRS );
            GDALClose( hSrcDS );
            return NULL;
        }
        
        GDALTransformerInfo* psInfo = (GDALTransformerInfo*)hTransformArg;

/* -------------------------------------------------------------------- */
/*      Get approximate output definition.                              */
/* -------------------------------------------------------------------- */
        if( bNeedsSuggestedWarpOutput )
        {
            double adfThisGeoTransform[6];
            double adfExtent[4];
            int    nThisPixels, nThisLines;

            if ( GDALSuggestedWarpOutput2( hSrcDS, 
                                        psInfo->pfnTransform, hTransformArg, 
                                        adfThisGeoTransform, 
                                        &nThisPixels, &nThisLines, 
                                        adfExtent, 0 ) != CE_None )
            {
                CPLFree( pszThisTargetSRS );
                GDALClose( hSrcDS );
                return NULL;
            }
            
            if ( CPLGetConfigOption( "CHECK_WITH_INVERT_PROJ", NULL ) == NULL )
            {
                double MinX = adfExtent[0];
                double MaxX = adfExtent[2];
                double MaxY = adfExtent[3];
                double MinY = adfExtent[1];
                int bSuccess = TRUE;
                
                /* Check that the the edges of the target image are in the validity area */
                /* of the target projection */
    #define N_STEPS 20
                int i,j;
                for(i=0;i<=N_STEPS && bSuccess;i++)
                {
                    for(j=0;j<=N_STEPS && bSuccess;j++)
                    {
                        double dfRatioI = i * 1.0 / N_STEPS;
                        double dfRatioJ = j * 1.0 / N_STEPS;
                        double expected_x = (1 - dfRatioI) * MinX + dfRatioI * MaxX;
                        double expected_y = (1 - dfRatioJ) * MinY + dfRatioJ * MaxY;
                        double x = expected_x;
                        double y = expected_y;
                        double z = 0;
                        /* Target SRS coordinates to source image pixel coordinates */
                        if (!psInfo->pfnTransform(hTransformArg, TRUE, 1, &x, &y, &z, &bSuccess) || !bSuccess)
                            bSuccess = FALSE;
                        /* Source image pixel coordinates to target SRS coordinates */
                        if (!psInfo->pfnTransform(hTransformArg, FALSE, 1, &x, &y, &z, &bSuccess) || !bSuccess)
                            bSuccess = FALSE;
                        if (fabs(x - expected_x) > (MaxX - MinX) / nThisPixels ||
                            fabs(y - expected_y) > (MaxY - MinY) / nThisLines)
                            bSuccess = FALSE;
                    }
                }
                
                /* If not, retry with CHECK_WITH_INVERT_PROJ=TRUE that forces ogrct.cpp */
                /* to check the consistency of each requested projection result with the */
                /* invert projection */
                if (!bSuccess)
                {
                    CPLSetConfigOption( "CHECK_WITH_INVERT_PROJ", "TRUE" );
                    CPLDebug("WARP", "Recompute out extent with CHECK_WITH_INVERT_PROJ=TRUE");

                    if( GDALSuggestedWarpOutput2( hSrcDS, 
                                        psInfo->pfnTransform, hTransformArg, 
                                        adfThisGeoTransform, 
                                        &nThisPixels, &nThisLines, 
                                        adfExtent, 0 ) != CE_None )
                    {
                        CPLFree( pszThisTargetSRS );
                        GDALClose( hSrcDS );
                        return NULL;
                    }
                }
            }

    /* -------------------------------------------------------------------- */
    /*      Expand the working bounds to include this region, ensure the    */
    /*      working resolution is no more than this resolution.             */
    /* -------------------------------------------------------------------- */
            if( dfWrkMaxX == 0.0 && dfWrkMinX == 0.0 )
            {
                dfWrkMinX = adfExtent[0];
                dfWrkMaxX = adfExtent[2];
                dfWrkMaxY = adfExtent[3];
                dfWrkMinY = adfExtent[1];
                dfWrkResX = adfThisGeoTransform[1];
                dfWrkResY = ABS(adfThisGeoTransform[5]);
            }
            else
            {
                dfWrkMinX = MIN(dfWrkMinX,adfExtent[0]);
                dfWrkMaxX = MAX(dfWrkMaxX,adfExtent[2]);
                dfWrkMaxY = MAX(dfWrkMaxY,adfExtent[3]);
                dfWrkMinY = MIN(dfWrkMinY,adfExtent[1]);
                dfWrkResX = MIN(dfWrkResX,adfThisGeoTransform[1]);
                dfWrkResY = MIN(dfWrkResY,ABS(adfThisGeoTransform[5]));
            }
        }

        if (iSrc == 0 && papszSrcFiles[1] == NULL)
        {
            *phTransformArg = hTransformArg;
            *phSrcDS = hSrcDS;
        }
        else
        {
            GDALDestroyGenImgProjTransformer( hTransformArg );
            GDALClose( hSrcDS );
        }
    }

/* -------------------------------------------------------------------- */
/*      Did we have any usable sources?                                 */
/* -------------------------------------------------------------------- */
    if( nDstBandCount == 0 )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "No usable source images." );
        CPLFree( pszThisTargetSRS );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Turn the suggested region into a geotransform and suggested     */
/*      number of pixels and lines.                                     */
/* -------------------------------------------------------------------- */
    double adfDstGeoTransform[6] = { 0, 0, 0, 0, 0, 0 };
    int nPixels = 0, nLines = 0;
    
    if( bNeedsSuggestedWarpOutput )
    {
        adfDstGeoTransform[0] = dfWrkMinX;
        adfDstGeoTransform[1] = dfWrkResX;
        adfDstGeoTransform[2] = 0.0;
        adfDstGeoTransform[3] = dfWrkMaxY;
        adfDstGeoTransform[4] = 0.0;
        adfDstGeoTransform[5] = -1 * dfWrkResY;

        nPixels = (int) ((dfWrkMaxX - dfWrkMinX) / dfWrkResX + 0.5);
        nLines = (int) ((dfWrkMaxY - dfWrkMinY) / dfWrkResY + 0.5);
    }

/* -------------------------------------------------------------------- */
/*      Did the user override some parameters?                          */
/* -------------------------------------------------------------------- */
    if( dfXRes != 0.0 && dfYRes != 0.0 )
    {
        if( dfMinX == 0.0 && dfMinY == 0.0 && dfMaxX == 0.0 && dfMaxY == 0.0 )
        {
            dfMinX = adfDstGeoTransform[0];
            dfMaxX = adfDstGeoTransform[0] + adfDstGeoTransform[1] * nPixels;
            dfMaxY = adfDstGeoTransform[3];
            dfMinY = adfDstGeoTransform[3] + adfDstGeoTransform[5] * nLines;
        }
        
        if ( bTargetAlignedPixels )
        {
            dfMinX = floor(dfMinX / dfXRes) * dfXRes;
            dfMaxX = ceil(dfMaxX / dfXRes) * dfXRes;
            dfMinY = floor(dfMinY / dfYRes) * dfYRes;
            dfMaxY = ceil(dfMaxY / dfYRes) * dfYRes;
        }

        nPixels = (int) ((dfMaxX - dfMinX + (dfXRes/2.0)) / dfXRes);
        nLines = (int) ((dfMaxY - dfMinY + (dfYRes/2.0)) / dfYRes);
        adfDstGeoTransform[0] = dfMinX;
        adfDstGeoTransform[3] = dfMaxY;
        adfDstGeoTransform[1] = dfXRes;
        adfDstGeoTransform[5] = -dfYRes;
    }

    else if( nForcePixels != 0 && nForceLines != 0 )
    {
        if( dfMinX == 0.0 && dfMinY == 0.0 && dfMaxX == 0.0 && dfMaxY == 0.0 )
        {
            dfMinX = dfWrkMinX;
            dfMaxX = dfWrkMaxX;
            dfMaxY = dfWrkMaxY;
            dfMinY = dfWrkMinY;
        }

        dfXRes = (dfMaxX - dfMinX) / nForcePixels;
        dfYRes = (dfMaxY - dfMinY) / nForceLines;

        adfDstGeoTransform[0] = dfMinX;
        adfDstGeoTransform[3] = dfMaxY;
        adfDstGeoTransform[1] = dfXRes;
        adfDstGeoTransform[5] = -dfYRes;

        nPixels = nForcePixels;
        nLines = nForceLines;
    }

    else if( nForcePixels != 0 )
    {
        if( dfMinX == 0.0 && dfMinY == 0.0 && dfMaxX == 0.0 && dfMaxY == 0.0 )
        {
            dfMinX = dfWrkMinX;
            dfMaxX = dfWrkMaxX;
            dfMaxY = dfWrkMaxY;
            dfMinY = dfWrkMinY;
        }

        dfXRes = (dfMaxX - dfMinX) / nForcePixels;
        dfYRes = dfXRes;

        adfDstGeoTransform[0] = dfMinX;
        adfDstGeoTransform[3] = dfMaxY;
        adfDstGeoTransform[1] = dfXRes;
        adfDstGeoTransform[5] = -dfYRes;

        nPixels = nForcePixels;
        nLines = (int) ((dfMaxY - dfMinY + (dfYRes/2.0)) / dfYRes);
    }

    else if( nForceLines != 0 )
    {
        if( dfMinX == 0.0 && dfMinY == 0.0 && dfMaxX == 0.0 && dfMaxY == 0.0 )
        {
            dfMinX = dfWrkMinX;
            dfMaxX = dfWrkMaxX;
            dfMaxY = dfWrkMaxY;
            dfMinY = dfWrkMinY;
        }

        dfYRes = (dfMaxY - dfMinY) / nForceLines;
        dfXRes = dfYRes;

        adfDstGeoTransform[0] = dfMinX;
        adfDstGeoTransform[3] = dfMaxY;
        adfDstGeoTransform[1] = dfXRes;
        adfDstGeoTransform[5] = -dfYRes;

        nPixels = (int) ((dfMaxX - dfMinX + (dfXRes/2.0)) / dfXRes);
        nLines = nForceLines;
    }

    else if( dfMinX != 0.0 || dfMinY != 0.0 || dfMaxX != 0.0 || dfMaxY != 0.0 )
    {
        dfXRes = adfDstGeoTransform[1];
        dfYRes = fabs(adfDstGeoTransform[5]);

        nPixels = (int) ((dfMaxX - dfMinX + (dfXRes/2.0)) / dfXRes);
        nLines = (int) ((dfMaxY - dfMinY + (dfYRes/2.0)) / dfYRes);

        dfXRes = (dfMaxX - dfMinX) / nPixels;
        dfYRes = (dfMaxY - dfMinY) / nLines;

        adfDstGeoTransform[0] = dfMinX;
        adfDstGeoTransform[3] = dfMaxY;
        adfDstGeoTransform[1] = dfXRes;
        adfDstGeoTransform[5] = -dfYRes;
    }

/* -------------------------------------------------------------------- */
/*      Do we want to generate an alpha band in the output file?        */
/* -------------------------------------------------------------------- */
    if( bEnableSrcAlpha )
        nDstBandCount--;

    if( bEnableDstAlpha )
        nDstBandCount++;

/* -------------------------------------------------------------------- */
/*      Create the output file.                                         */
/* -------------------------------------------------------------------- */
    if( !bQuiet )
        printf( "Creating output file that is %dP x %dL.\n", nPixels, nLines );

    hDstDS = GDALCreate( hDriver, pszFilename, nPixels, nLines, 
                         nDstBandCount, eDT, *ppapszCreateOptions );
    
    if( hDstDS == NULL )
    {
        CPLFree( pszThisTargetSRS );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Write out the projection definition.                            */
/* -------------------------------------------------------------------- */
    const char *pszDstMethod = CSLFetchNameValue(papszTO,"DST_METHOD");
    if( pszDstMethod == NULL || !EQUAL(pszDstMethod, "NO_GEOTRANSFORM") )
    {
        GDALSetProjection( hDstDS, pszThisTargetSRS );
        GDALSetGeoTransform( hDstDS, adfDstGeoTransform );
    }
    else
    {
        adfDstGeoTransform[0] = 0.0;
        adfDstGeoTransform[3] = 0.0;
        adfDstGeoTransform[5] = fabs(adfDstGeoTransform[5]);
    }

    if (*phTransformArg != NULL)
        GDALSetGenImgProjTransformerDstGeoTransform( *phTransformArg, adfDstGeoTransform);

/* -------------------------------------------------------------------- */
/*      Try to set color interpretation of source bands to target       */
/*      dataset.                                                        */
/*      FIXME? We should likely do that for other drivers than VRT      */
/*      but it might create spurious .aux.xml files (at least with HFA, */
/*      and netCDF)                                                     */
/* -------------------------------------------------------------------- */
    if( bVRT || bSetColorInterpretation )
    {
        int nBandsToCopy = (int)apeColorInterpretations.size();
        if ( bEnableSrcAlpha )
            nBandsToCopy --;
        for(int iBand = 0; iBand < nBandsToCopy; iBand++)
        {
            GDALSetRasterColorInterpretation(
                GDALGetRasterBand( hDstDS, iBand + 1 ),
                apeColorInterpretations[iBand] );
        }
    }
    
/* -------------------------------------------------------------------- */
/*      Try to set color interpretation of output file alpha band.      */
/* -------------------------------------------------------------------- */
    if( bEnableDstAlpha )
    {
        GDALSetRasterColorInterpretation( 
            GDALGetRasterBand( hDstDS, nDstBandCount ), 
            GCI_AlphaBand );
    }

/* -------------------------------------------------------------------- */
/*      Copy the color table, if required.                              */
/* -------------------------------------------------------------------- */
    if( hCT != NULL )
    {
        GDALSetRasterColorTable( GDALGetRasterBand(hDstDS,1), hCT );
        GDALDestroyColorTable( hCT );
    }

    CPLFree( pszThisTargetSRS );
    return hDstDS;
}

/************************************************************************/
/*                      GeoTransform_Transformer()                      */
/*                                                                      */
/*      Convert points from georef coordinates to pixel/line based      */
/*      on a geotransform.                                              */
/************************************************************************/

class CutlineTransformer : public OGRCoordinateTransformation
{
public:

    void         *hSrcImageTransformer;

    virtual OGRSpatialReference *GetSourceCS() { return NULL; }
    virtual OGRSpatialReference *GetTargetCS() { return NULL; }

    virtual int Transform( int nCount, 
                           double *x, double *y, double *z = NULL ) {
        int nResult;

        int *pabSuccess = (int *) CPLCalloc(sizeof(int),nCount);
        nResult = TransformEx( nCount, x, y, z, pabSuccess );
        CPLFree( pabSuccess );

        return nResult;
    }

    virtual int TransformEx( int nCount, 
                             double *x, double *y, double *z = NULL,
                             int *pabSuccess = NULL ) {
        return GDALGenImgProjTransform( hSrcImageTransformer, TRUE, 
                                        nCount, x, y, z, pabSuccess );
    }
};


/************************************************************************/
/*                            LoadCutline()                             */
/*                                                                      */
/*      Load blend cutline from OGR datasource.                         */
/************************************************************************/

static void
LoadCutline( const char *pszCutlineDSName, const char *pszCLayer, 
             const char *pszCWHERE, const char *pszCSQL, 
             void **phCutlineRet )

{
#ifndef OGR_ENABLED
    CPLError( CE_Failure, CPLE_AppDefined, 
              "Request to load a cutline failed, this build does not support OGR features.\n" );
    GDALExit( 1 );
#else // def OGR_ENABLED
    OGRRegisterAll();

/* -------------------------------------------------------------------- */
/*      Open source vector dataset.                                     */
/* -------------------------------------------------------------------- */
    OGRDataSourceH hSrcDS;

    hSrcDS = OGROpen( pszCutlineDSName, FALSE, NULL );
    if( hSrcDS == NULL )
        GDALExit( 1 );

/* -------------------------------------------------------------------- */
/*      Get the source layer                                            */
/* -------------------------------------------------------------------- */
    OGRLayerH hLayer = NULL;

    if( pszCSQL != NULL )
        hLayer = OGR_DS_ExecuteSQL( hSrcDS, pszCSQL, NULL, NULL ); 
    else if( pszCLayer != NULL )
        hLayer = OGR_DS_GetLayerByName( hSrcDS, pszCLayer );
    else
        hLayer = OGR_DS_GetLayer( hSrcDS, 0 );

    if( hLayer == NULL )
    {
        fprintf( stderr, "Failed to identify source layer from datasource.\n" );
        GDALExit( 1 );
    }

/* -------------------------------------------------------------------- */
/*      Apply WHERE clause if there is one.                             */
/* -------------------------------------------------------------------- */
    if( pszCWHERE != NULL )
        OGR_L_SetAttributeFilter( hLayer, pszCWHERE );

/* -------------------------------------------------------------------- */
/*      Collect the geometries from this layer, and build list of       */
/*      burn values.                                                    */
/* -------------------------------------------------------------------- */
    OGRFeatureH hFeat;
    OGRGeometryH hMultiPolygon = OGR_G_CreateGeometry( wkbMultiPolygon );

    OGR_L_ResetReading( hLayer );
    
    while( (hFeat = OGR_L_GetNextFeature( hLayer )) != NULL )
    {
        OGRGeometryH hGeom = OGR_F_GetGeometryRef(hFeat);

        if( hGeom == NULL )
        {
            fprintf( stderr, "ERROR: Cutline feature without a geometry.\n" );
            GDALExit( 1 );
        }
        
        OGRwkbGeometryType eType = wkbFlatten(OGR_G_GetGeometryType( hGeom ));

        if( eType == wkbPolygon )
            OGR_G_AddGeometry( hMultiPolygon, hGeom );
        else if( eType == wkbMultiPolygon )
        {
            int iGeom;

            for( iGeom = 0; iGeom < OGR_G_GetGeometryCount( hGeom ); iGeom++ )
            {
                OGR_G_AddGeometry( hMultiPolygon, 
                                   OGR_G_GetGeometryRef(hGeom,iGeom) );
            }
        }
        else
        {
            fprintf( stderr, "ERROR: Cutline not of polygon type.\n" );
            GDALExit( 1 );
        }

        OGR_F_Destroy( hFeat );
    }

    if( OGR_G_GetGeometryCount( hMultiPolygon ) == 0 )
    {
        fprintf( stderr, "ERROR: Did not get any cutline features.\n" );
        GDALExit( 1 );
    }

/* -------------------------------------------------------------------- */
/*      Ensure the coordinate system gets set on the geometry.          */
/* -------------------------------------------------------------------- */
    OGR_G_AssignSpatialReference(
        hMultiPolygon, OGR_L_GetSpatialRef(hLayer) );

    *phCutlineRet = (void *) hMultiPolygon;

/* -------------------------------------------------------------------- */
/*      Cleanup                                                         */
/* -------------------------------------------------------------------- */
    if( pszCSQL != NULL )
        OGR_DS_ReleaseResultSet( hSrcDS, hLayer );

    OGR_DS_Destroy( hSrcDS );
#endif
}

/************************************************************************/
/*                      TransformCutlineToSource()                      */
/*                                                                      */
/*      Transform cutline from its SRS to source pixel/line coordinates.*/
/************************************************************************/
static void
TransformCutlineToSource( GDALDatasetH hSrcDS, void *hCutline,
                          char ***ppapszWarpOptions, char **papszTO_In )

{
#ifdef OGR_ENABLED
    OGRGeometryH hMultiPolygon = OGR_G_Clone( (OGRGeometryH) hCutline );
    char **papszTO = CSLDuplicate( papszTO_In );

/* -------------------------------------------------------------------- */
/*      Checkout that SRS are the same.                                 */
/* -------------------------------------------------------------------- */
    OGRSpatialReferenceH  hRasterSRS = NULL;
    const char *pszProjection = NULL;

    if( GDALGetProjectionRef( hSrcDS ) != NULL 
        && strlen(GDALGetProjectionRef( hSrcDS )) > 0 )
        pszProjection = GDALGetProjectionRef( hSrcDS );
    else if( GDALGetGCPProjection( hSrcDS ) != NULL )
        pszProjection = GDALGetGCPProjection( hSrcDS );

    if( pszProjection == NULL || EQUAL( pszProjection, "" ) )
        pszProjection = CSLFetchNameValue( papszTO, "SRC_SRS" );

    if( pszProjection != NULL )
    {
        hRasterSRS = OSRNewSpatialReference(NULL);
        if( OSRImportFromWkt( hRasterSRS, (char **)&pszProjection ) != CE_None )
        {
            OSRDestroySpatialReference(hRasterSRS);
            hRasterSRS = NULL;
        }
    }

    OGRSpatialReferenceH hCutlineSRS = OGR_G_GetSpatialReference( hMultiPolygon );
    if( hRasterSRS != NULL && hCutlineSRS != NULL )
    {
        /* ok, we will reproject */
    }
    else if( hRasterSRS != NULL && hCutlineSRS == NULL )
    {
        fprintf(stderr,
                "Warning : the source raster dataset has a SRS, but the cutline features\n"
                "not.  We assume that the cutline coordinates are expressed in the destination SRS.\n"
                "If not, cutline results may be incorrect.\n");
    }
    else if( hRasterSRS == NULL && hCutlineSRS != NULL )
    {
        fprintf(stderr,
                "Warning : the input vector layer has a SRS, but the source raster dataset does not.\n"
                "Cutline results may be incorrect.\n");
    }

    if( hRasterSRS != NULL )
        OSRDestroySpatialReference(hRasterSRS);

/* -------------------------------------------------------------------- */
/*      Extract the cutline SRS WKT.                                    */
/* -------------------------------------------------------------------- */
    if( hCutlineSRS != NULL )
    {
        char *pszCutlineSRS_WKT = NULL;

        OSRExportToWkt( hCutlineSRS, &pszCutlineSRS_WKT );
        papszTO = CSLSetNameValue( papszTO, "DST_SRS", pszCutlineSRS_WKT );
        CPLFree( pszCutlineSRS_WKT );
    }

/* -------------------------------------------------------------------- */
/*      It may be unwise to let the mask geometry be re-wrapped by      */
/*      the CENTER_LONG machinery as this can easily screw up world     */
/*      spanning masks and invert the mask topology.                    */
/* -------------------------------------------------------------------- */
    papszTO = CSLSetNameValue( papszTO, "INSERT_CENTER_LONG", "FALSE" );

/* -------------------------------------------------------------------- */
/*      Transform the geometry to pixel/line coordinates.               */
/* -------------------------------------------------------------------- */
    CutlineTransformer oTransformer;

    /* The cutline transformer will *invert* the hSrcImageTransformer */
    /* so it will convert from the cutline SRS to the source pixel/line */
    /* coordinates */
    oTransformer.hSrcImageTransformer = 
        GDALCreateGenImgProjTransformer2( hSrcDS, NULL, papszTO );

    CSLDestroy( papszTO );

    if( oTransformer.hSrcImageTransformer == NULL )
        GDALExit( 1 );

    OGR_G_Transform( hMultiPolygon, 
                     (OGRCoordinateTransformationH) &oTransformer );

    GDALDestroyGenImgProjTransformer( oTransformer.hSrcImageTransformer );

/* -------------------------------------------------------------------- */
/*      Convert aggregate geometry into WKT.                            */
/* -------------------------------------------------------------------- */
    char *pszWKT = NULL;

    OGR_G_ExportToWkt( hMultiPolygon, &pszWKT );
    OGR_G_DestroyGeometry( hMultiPolygon );

    *ppapszWarpOptions = CSLSetNameValue( *ppapszWarpOptions, 
                                          "CUTLINE", pszWKT );
    CPLFree( pszWKT );
#endif
}

static void 
RemoveConflictingMetadata( GDALMajorObjectH hObj, char **papszMetadata, 
                           const char *pszValueConflict )
{
    if ( hObj == NULL ) return;

    char *pszKey = NULL; 
    const char *pszValueRef; 
    const char *pszValueComp; 
    char ** papszMetadataRef = CSLDuplicate( papszMetadata );
    int nCount = CSLCount( papszMetadataRef ); 

    for( int i = 0; i < nCount; i++ ) 
    {
        pszKey = NULL;
        pszValueRef = CPLParseNameValue( papszMetadataRef[i], &pszKey ); 
        if( pszKey != NULL )
        {
            pszValueComp = GDALGetMetadataItem( hObj, pszKey, NULL );
            if ( ( pszValueRef == NULL || pszValueComp == NULL ||
                ! EQUAL( pszValueRef, pszValueComp ) ) &&
                ( pszValueComp == NULL ||
                ! EQUAL( pszValueComp, pszValueConflict ) ) ) {
                GDALSetMetadataItem( hObj, pszKey, pszValueConflict, NULL ); 
            }
            CPLFree( pszKey );
        }
    } 

    CSLDestroy( papszMetadataRef );
}
