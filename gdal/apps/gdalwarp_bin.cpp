/******************************************************************************
 *
 * Project:  High Performance Image Reprojector
 * Purpose:  Test program for high performance warper API.
 * Author:   Frank Warmerdam <warmerdam@pobox.com>
 *
 ******************************************************************************
 * Copyright (c) 2002, i3 - information integration and imaging
 *                          Fort Collin, CO
 * Copyright (c) 2007-2015, Even Rouault <even dot rouault at mines-paris dot org>
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

#include "cpl_string.h"
#include "cpl_error.h"
#include "commonutils.h"
#include "gdal_utils_priv.h"

CPL_CVSID("$Id$")

/******************************************************************************/
/*! \page gdalwarp gdalwarp

image reprojection and warping utility

\section gdalwarp_synopsis SYNOPSIS

\htmlonly
Usage:
\endhtmlonly

\verbatim
gdalwarp [--help-general] [--formats]
    [-s_srs srs_def] [-t_srs srs_def] [-to "NAME=VALUE"]* [-novshiftgrid]
    [-order n | -tps | -rpc | -geoloc] [-et err_threshold]
    [-refine_gcps tolerance [minimum_gcps]]
    [-te xmin ymin xmax ymax] [-te_srs srs_def]
    [-tr xres yres] [-tap] [-ts width height]
    [-ovr level|AUTO|AUTO-n|NONE] [-wo "NAME=VALUE"] [-ot Byte/Int16/...] [-wt Byte/Int16]
    [-srcnodata "value [value...]"] [-dstnodata "value [value...]"]
    [-srcalpha|-nosrcalpha] [-dstalpha]
    [-r resampling_method] [-wm memory_in_mb] [-multi] [-q]
    [-cutline datasource] [-cl layer] [-cwhere expression]
    [-csql statement] [-cblend dist_in_pixels] [-crop_to_cutline]
    [-of format] [-co "NAME=VALUE"]* [-overwrite]
    [-nomd] [-cvmd meta_conflict_value] [-setci] [-oo NAME=VALUE]*
    [-doo NAME=VALUE]*
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
(i.e. EPSG:4296), PROJ.4 declarations (as above), or the name of a .prj file
containing well known text. Starting with GDAL 2.2, if the SRS has an explicit
vertical datum that points to a PROJ.4 geoidgrids, and the input dataset is a
single band dataset, a vertical correction will be applied to the values of the
dataset.</dd>
<dt> <b>-t_srs</b> <em>srs_def</em>:</dt><dd> target spatial reference set.
The coordinate systems that can be passed are anything supported by the
OGRSpatialReference.SetFromUserInput() call, which includes EPSG PCS and GCSes
(i.e. EPSG:4296), PROJ.4 declarations (as above), or the name of a .prj file
containing well known text. Starting with GDAL 2.2, if the SRS has an explicit
vertical datum that points to a PROJ.4 geoidgrids, and the input dataset is a
single band dataset, a vertical correction will be applied to the values of the
dataset.</dd>
<dt> <b>-to</b> <em>NAME=VALUE</em>:</dt><dd> set a transformer option suitable
to pass to GDALCreateGenImgProjTransformer2(). </dd>
<dt> <b>-novshiftgrid</b></dt><dd> (GDAL &gt;= 2.2) Disable the use of vertical
datum shift grids when one of the source or target SRS has an explicit vertical
datum, and the input dataset is a single band dataset.</dd>
<dt> <b>-order</b> <em>n</em>:</dt><dd> order of polynomial used for warping
(1 to 3). The default is to select a polynomial order based on the number of
GCPs.</dd>
<dt> <b>-tps</b>:</dt><dd>Force use of thin plate spline transformer based on
available GCPs.</dd>
<dt> <b>-rpc</b>:</dt> <dd>Force use of RPCs.</dd>
<dt> <b>-geoloc</b>:</dt><dd>Force use of Geolocation Arrays.</dd>
<dt> <b>-et</b> <em>err_threshold</em>:</dt><dd> error threshold for
transformation approximation (in pixel units - defaults to 0.125, unless, starting
with GDAL 2.1, the RPC_DEM warping option is specified, in which case, an exact
transformer, i.e. err_threshold=0, will be used).</dd>
<dt> <b>-refine_gcps</b> <em>tolerance minimum_gcps</em>:</dt><dd>  (GDAL >= 1.9.0) refines the GCPs by automatically eliminating outliers.
Outliers will be eliminated until minimum_gcps are left or when no outliers can be detected.
The tolerance is passed to adjust when a GCP will be eliminated.
Not that GCP refinement only works with polynomial interpolation.
The tolerance is in pixel units if no projection is available, otherwise it is in SRS units.
If minimum_gcps is not provided, the minimum GCPs according to the polynomial model is used.</dd>
<dt> <b>-te</b> <em>xmin ymin xmax ymax</em>:</dt><dd> set georeferenced
extents of output file to be created (in target SRS by default, or in the SRS
specified with -te_srs)
</dd>
<dt> <b>-te_srs</b> <i>srs_def</i>:</dt><dd> (GDAL >= 2.0) Specifies the SRS in
which to interpret the coordinates given with -te. The <i>srs_def</i> may
be any of the usual GDAL/OGR forms, complete WKT, PROJ.4, EPSG:n or a file
containing the WKT.
This must not be confused with -t_srs which is the target SRS of the output
dataset. -te_srs is a convenience e.g. when knowing the output coordinates in a
geodetic long/lat SRS, but still wanting a result in a projected coordinate system.
</dd>
<dt> <b>-tr</b> <em>xres yres</em>:</dt><dd> set output file resolution (in
target georeferenced units)</dd>
<dt> <b>-tap</b>:</dt><dd> (GDAL >= 1.8.0) (target aligned pixels) align
the coordinates of the extent of the output file to the values of the -tr,
such that the aligned extent includes the minimum extent.</dd>
<dt> <b>-ts</b> <em>width height</em>:</dt><dd> set output file size in
pixels and lines. If width or height is set to 0, the other dimension will be
guessed from the computed resolution. Note that -ts cannot be used with -tr</dd>
<dt> <b>-ovr</b> <em>level|AUTO|AUTO-n|NONE></em>:</dt><dd>(GDAL >= 2.0) To
specify which overview level of source files must be used. The default choice,
AUTO, will select the overview level whose resolution is the closest to the
target resolution. Specify an integer value (0-based, i.e. 0=1st overview level)
to select a particular level. Specify AUTO-n where n is an integer greater or
equal to 1, to select an overview level below the AUTO one. Or specify NONE to
force the base resolution to be used (can be useful if overviews have been
generated with a low quality resampling method, and the warping is done using a
higher quality resampling method).</dd>
<dt> <b>-wo</b> <em>"NAME=VALUE"</em>:</dt><dd> Set a warp option.  The
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
<dt><b>max</b></dt>: <dd>maximum resampling, selects the maximum value from all non-NODATA contributing pixels. (GDAL >= 2.0.0)</dd>
<dt><b>min</b></dt>: <dd>minimum resampling, selects the minimum value from all non-NODATA contributing pixels. (GDAL >= 2.0.0)</dd>
<dt><b>med</b></dt>: <dd>median resampling, selects the median value of all non-NODATA contributing pixels. (GDAL >= 2.0.0)</dd>
<dt><b>q1</b></dt>: <dd>first quartile resampling, selects the first quartile value of all non-NODATA contributing pixels. (GDAL >= 2.0.0)</dd>
<dt><b>q3</b></dt>: <dd>third quartile resampling, selects the third quartile value of all non-NODATA contributing pixels. (GDAL >= 2.0.0)</dd>
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
file. Use a value of <tt>None</tt> to ensure that nodata is not defined (GDAL>=1.11).
If this argument is not used then nodata values will be copied from the source dataset (GDAL>=1.11).</dd>
<dt> <b>-srcalpha</b>:</dt><dd> Force the last band of a source image to be
considered as a source alpha band. </dd>
<dt> <b>-nosrcalpha</b>:</dt><dd> Prevent the alpha band of a source image to be
considered as such (it will be warped as a regular band) (GDAL>=2.2). </dd>
<dt> <b>-dstalpha</b>:</dt><dd> Create an output alpha band to identify
nodata (unset/transparent) pixels. </dd>
<dt> <b>-wm</b> <em>memory_in_mb</em>:</dt><dd> Set the amount of memory (in
megabytes) that the warp API is allowed to use for caching.</dd>
<dt> <b>-multi</b>:</dt><dd> Use multithreaded warping implementation.
Two threads will be used to process chunks of image and perform
input/output operation simultaneously. Note that computation is not
multithreaded itself. To do that, you can use the -wo NUM_THREADS=val/ALL_CPUS
option, which can be combined with -multi</dd>
<dt> <b>-q</b>:</dt><dd> Be quiet.</dd>
<dt> <b>-of</b> <em>format</em>:</dt><dd> Select the output format. The default is GeoTIFF (GTiff). Use the short format name. </dd>
<dt> <b>-co</b> <em>"NAME=VALUE"</em>:</dt><dd> passes a creation option to
the output format driver. Multiple <b>-co</b> options may be listed. See <a class="el"
href="formats_list.html" title="GDAL Raster Formats">format specific
documentation for legal creation options for each format</a>
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
<dt> <b>-doo</b> <em>NAME=VALUE</em>:</dt><dd>(starting with GDAL 2.1) Output dataset open option (format specific)</dd>

<dt> <em>srcfile</em>:</dt><dd> The source file name(s). </dd>
<dt> <em>dstfile</em>:</dt><dd> The destination file name. </dd>
</dl>

Mosaicing into an existing output file is supported if the output file
already exists. The spatial extent of the existing file will not
be modified to accommodate new data, so you may have to remove it in that case, or
use the -overwrite option.

Polygon cutlines may be used as a mask to restrict the area of the
destination file that may be updated, including blending.  If the OGR
layer containing the cutline features has no explicit SRS, the cutline
features must be in the SRS of the destination file. When writing to a
not yet existing target dataset, its extent will be the one of the
original raster unless -te or -crop_to_cutline are specified.

When doing vertical shift adjustments, the transformer option -to ERROR_ON_MISSING_VERT_SHIFT=YES
can be used to error out as soon as a vertical shift value is missing (instead of 
0 being used).

<p>
\section gdalwarp_examples EXAMPLES

- For instance, an eight bit spot scene stored in GeoTIFF with
control points mapping the corners to lat/long could be warped to a UTM
projection with a command like this:<p>

\verbatim
gdalwarp -t_srs '+proj=utm +zone=11 +datum=WGS84' -overwrite raw_spot.tif utm11.tif
\endverbatim

- For instance, the second channel of an ASTER image stored in HDF with
control points mapping the corners to lat/long could be warped to a UTM
projection with a command like this:<p>

\verbatim
gdalwarp -overwrite HDF4_SDS:ASTER_L1B:"pg-PR1B0000-2002031402_100_001":2 pg-PR1B0000-2002031402_100_001_2.tif
\endverbatim

- (GDAL &gt;= 2.2) To apply a cutline on a un-georeferenced image and clip from pixel (220,60) to pixel (1160,690):<p>

\verbatim
gdalwarp -overwrite -to SRC_METHOD=NO_GEOTRANSFORM -to DST_METHOD=NO_GEOTRANSFORM -te 220 60 1160 690 -cutline cutline.csv in.png out.tif
\endverbatim

where cutline.csv content is like:
\verbatim
id,WKT
1,"POLYGON((....))"
\endverbatim

- (GDAL &gt;= 2.2) To transform a DEM from geoid elevations (using EGM96) to WGS84 ellipsoidal heights:<p>

\verbatim
gdalwarp -overwrite in_dem.tif out_dem.tif -s_srs EPSG:4326+5773 -t_srs EPSG:4979
\endverbatim

<p>
\section gdalwarp_seealso SEE ALSO

\if man
http://trac.osgeo.org/gdal/wiki/UserDocs/GdalWarp :
\else
<a href="http://trac.osgeo.org/gdal/wiki/UserDocs/GdalWarp">
\endif
Wiki page discussing options and behaviours of gdalwarp
\if man
\else
</a>
\endif

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

  OGRCleanupAll();

  exit( nCode );
}

/************************************************************************/
/*                               Usage()                                */
/************************************************************************/

static void Usage(const char* pszErrorMsg = NULL)

{
    printf(
        "Usage: gdalwarp [--help-general] [--formats]\n"
        "    [-s_srs srs_def] [-t_srs srs_def] [-to \"NAME=VALUE\"]* [-novshiftgrid]\n"
        "    [-order n | -tps | -rpc | -geoloc] [-et err_threshold]\n"
        "    [-refine_gcps tolerance [minimum_gcps]]\n"
        "    [-te xmin ymin xmax ymax] [-tr xres yres] [-tap] [-ts width height]\n"
        "    [-ovr level|AUTO|AUTO-n|NONE] [-wo \"NAME=VALUE\"] [-ot Byte/Int16/...] [-wt Byte/Int16]\n"
        "    [-srcnodata \"value [value...]\"] [-dstnodata \"value [value...]\"] -dstalpha\n"
        "    [-r resampling_method] [-wm memory_in_mb] [-multi] [-q]\n"
        "    [-cutline datasource] [-cl layer] [-cwhere expression]\n"
        "    [-csql statement] [-cblend dist_in_pixels] [-crop_to_cutline]\n"
        "    [-of format] [-co \"NAME=VALUE\"]* [-overwrite]\n"
        "    [-nomd] [-cvmd meta_conflict_value] [-setci] [-oo NAME=VALUE]*\n"
        "    [-doo NAME=VALUE]*\n"
        "    srcfile* dstfile\n"
        "\n"
        "Available resampling methods:\n"
        "    near (default), bilinear, cubic, cubicspline, lanczos, average, mode,  max, min, med, Q1, Q3.\n" );

    if( pszErrorMsg != NULL )
        fprintf(stderr, "\nFAILURE: %s\n", pszErrorMsg);

    GDALExit( 1 );
}

/************************************************************************/
/*                       GDALWarpAppOptionsForBinaryNew()             */
/************************************************************************/

static GDALWarpAppOptionsForBinary *GDALWarpAppOptionsForBinaryNew(void)
{
    return (GDALWarpAppOptionsForBinary*) CPLCalloc(  1, sizeof(GDALWarpAppOptionsForBinary) );
}

/************************************************************************/
/*                       GDALWarpAppOptionsForBinaryFree()            */
/************************************************************************/

static void GDALWarpAppOptionsForBinaryFree( GDALWarpAppOptionsForBinary* psOptionsForBinary )
{
    if( psOptionsForBinary )
    {
        CSLDestroy(psOptionsForBinary->papszSrcFiles);
        CPLFree(psOptionsForBinary->pszDstFilename);
        CSLDestroy(psOptionsForBinary->papszOpenOptions);
        CSLDestroy(psOptionsForBinary->papszDestOpenOptions);
        CPLFree(psOptionsForBinary->pszFormat);
        CPLFree(psOptionsForBinary);
    }
}

/************************************************************************/
/*                                main()                                */
/************************************************************************/

int main( int argc, char ** argv )

{
    GDALDatasetH *pahSrcDS = NULL;
    int nSrcCount = 0;

    EarlySetConfigOptions(argc, argv);

/* -------------------------------------------------------------------- */
/*      Register standard GDAL drivers, and process generic GDAL        */
/*      command options.                                                */
/* -------------------------------------------------------------------- */
    GDALAllRegister();
    argc = GDALGeneralCmdLineProcessor( argc, &argv, 0 );
    if( argc < 1 )
        GDALExit( -argc );

    for( int i = 0; argv != NULL && argv[i] != NULL; i++ )
    {
        if( EQUAL(argv[i], "--utility_version") )
        {
            printf("%s was compiled against GDAL %s and is running against GDAL %s\n",
                   argv[0], GDAL_RELEASE_NAME, GDALVersionInfo("RELEASE_NAME"));
            CSLDestroy( argv );
            return 0;
        }
        else if( EQUAL(argv[i],"--help") )
        {
            Usage(NULL);
        }
    }

/* -------------------------------------------------------------------- */
/*      Set optimal setting for best performance with huge input VRT.   */
/*      The rationale for 450 is that typical Linux process allow       */
/*      only 1024 file descriptors per process and we need to keep some */
/*      spare for shared libraries, etc. so let's go down to 900.       */
/*      And some datasets may need 2 file descriptors, so divide by 2   */
/*      for security.                                                   */
/* -------------------------------------------------------------------- */
    if( CPLGetConfigOption("GDAL_MAX_DATASET_POOL_SIZE", NULL) == NULL )
    {
#if defined(__MACH__) && defined(__APPLE__)
        // On Mach, the default limit is 256 files per process
        // TODO We should eventually dynamically query the limit for all OS
        CPLSetConfigOption("GDAL_MAX_DATASET_POOL_SIZE", "100");
#else
        CPLSetConfigOption("GDAL_MAX_DATASET_POOL_SIZE", "450");
#endif
    }

    GDALWarpAppOptionsForBinary* psOptionsForBinary = GDALWarpAppOptionsForBinaryNew();
    /* coverity[tainted_data] */
    GDALWarpAppOptions *psOptions = GDALWarpAppOptionsNew(argv + 1, psOptionsForBinary);
    CSLDestroy( argv );

    if( psOptions == NULL )
    {
        Usage(NULL);
    }

    if( psOptionsForBinary->pszDstFilename == NULL )
    {
        Usage("No target filename specified.");
    }

    if ( CSLCount(psOptionsForBinary->papszSrcFiles) == 1 &&
         strcmp(psOptionsForBinary->papszSrcFiles[0], psOptionsForBinary->pszDstFilename) == 0 &&
         psOptionsForBinary->bOverwrite)
    {
        CPLError(CE_Failure, CPLE_IllegalArg, "Source and destination datasets must be different.\n");
        GDALExit(1);
    }

/* -------------------------------------------------------------------- */
/*      Open Source files.                                              */
/* -------------------------------------------------------------------- */
    for(int i = 0; psOptionsForBinary->papszSrcFiles[i] != NULL; i++)
    {
        nSrcCount++;
        pahSrcDS = (GDALDatasetH *) CPLRealloc(pahSrcDS, sizeof(GDALDatasetH) * nSrcCount);
        pahSrcDS[nSrcCount-1] = GDALOpenEx( psOptionsForBinary->papszSrcFiles[i], GDAL_OF_RASTER | GDAL_OF_VERBOSE_ERROR, NULL,
                                            (const char* const* )psOptionsForBinary->papszOpenOptions, NULL );

        if( pahSrcDS[nSrcCount-1] == NULL )
            GDALExit(2);
    }

/* -------------------------------------------------------------------- */
/*      Does the output dataset already exist?                          */
/* -------------------------------------------------------------------- */

    /* FIXME ? source filename=target filename and -overwrite is definitely */
    /* an error. But I can't imagine of a valid case (without -overwrite), */
    /* where it would make sense. In doubt, let's keep that dubious possibility... */

    int bOutStreaming = FALSE;
    if( strcmp(psOptionsForBinary->pszDstFilename, "/vsistdout/") == 0 )
    {
        psOptionsForBinary->bQuiet = TRUE;
        bOutStreaming = TRUE;
    }
#ifdef S_ISFIFO
    else
    {
        VSIStatBufL sStat;
        if( VSIStatExL(psOptionsForBinary->pszDstFilename, &sStat, VSI_STAT_EXISTS_FLAG | VSI_STAT_NATURE_FLAG) == 0 &&
            S_ISFIFO(sStat.st_mode) )
        {
            bOutStreaming = TRUE;
        }
    }
#endif

    GDALDatasetH hDstDS = NULL;
    if( bOutStreaming )
    {
        GDALWarpAppOptionsSetWarpOption(psOptions, "STREAMABLE_OUTPUT", "YES");
    }
    else
    {
        CPLPushErrorHandler( CPLQuietErrorHandler );
        hDstDS = GDALOpenEx( psOptionsForBinary->pszDstFilename, GDAL_OF_RASTER | GDAL_OF_VERBOSE_ERROR | GDAL_OF_UPDATE,
                             NULL, psOptionsForBinary->papszDestOpenOptions, NULL );
        CPLPopErrorHandler();
    }

    if( hDstDS != NULL && psOptionsForBinary->bOverwrite )
    {
        GDALClose(hDstDS);
        hDstDS = NULL;
    }

    if( hDstDS != NULL && psOptionsForBinary->bCreateOutput )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                 "Output dataset %s exists,\n"
                 "but some command line options were provided indicating a new dataset\n"
                 "should be created.  Please delete existing dataset and run again.\n",
                 psOptionsForBinary->pszDstFilename );
        GDALExit(1);
    }

    /* Avoid overwriting an existing destination file that cannot be opened in */
    /* update mode with a new GTiff file */
    if ( !bOutStreaming && hDstDS == NULL && !psOptionsForBinary->bOverwrite )
    {
        CPLPushErrorHandler( CPLQuietErrorHandler );
        hDstDS = GDALOpen( psOptionsForBinary->pszDstFilename, GA_ReadOnly );
        CPLPopErrorHandler();

        if (hDstDS)
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                     "Output dataset %s exists, but cannot be opened in update mode\n",
                     psOptionsForBinary->pszDstFilename );
            GDALClose(hDstDS);
            GDALExit(1);
        }
    }

    if( !(psOptionsForBinary->bQuiet) )
    {
        GDALWarpAppOptionsSetProgress(psOptions, GDALTermProgress, NULL);
    }

    if (hDstDS == NULL && !psOptionsForBinary->bQuiet && !psOptionsForBinary->bFormatExplicitlySet)
        CheckExtensionConsistency(psOptionsForBinary->pszDstFilename, psOptionsForBinary->pszFormat);

    int bUsageError = FALSE;
    GDALDatasetH hOutDS = GDALWarp(psOptionsForBinary->pszDstFilename, hDstDS,
                      nSrcCount, pahSrcDS, psOptions, &bUsageError);
    if( bUsageError )
        Usage();
    int nRetCode = (hOutDS) ? 0 : 1;

    GDALWarpAppOptionsFree(psOptions);
    GDALWarpAppOptionsForBinaryFree(psOptionsForBinary);

    // Close first hOutDS since it might reference sources (case of VRT)
    GDALClose( hOutDS ? hOutDS : hDstDS );
    for(int i = 0; i < nSrcCount; i++)
    {
        GDALClose(pahSrcDS[i]);
    }
    CPLFree(pahSrcDS);

    GDALDumpOpenDatasets( stderr );

    GDALDestroyDriverManager();

    OGRCleanupAll();

    return nRetCode;
}
