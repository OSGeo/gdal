/******************************************************************************
 *
 * Project:  netCDF read/write Driver
 * Purpose:  GDAL bindings over netCDF library for GMT Grids.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2004, Frank Warmerdam
 * Copyright (c) 2007-2013, Even Rouault <even dot rouault at spatialys.com>
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

#include "cpl_port.h"

#include <cstddef>
#include <cstring>
#include <algorithm>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_multiproc.h"
#include "cpl_progress.h"
#include "gdal.h"
#include "gdal_frmts.h"
#include "gdal_pam.h"
#include "gdal_priv.h"
#include "netcdf.h"
#include "ogr_core.h"

CPL_CVSID("$Id$")

extern CPLMutex *hNCMutex; /* shared with netcdf. See netcdfdataset.cpp */

/************************************************************************/
/* ==================================================================== */
/*                           GMTDataset                                 */
/* ==================================================================== */
/************************************************************************/

class GMTRasterBand;

class GMTDataset final: public GDALPamDataset
{
    int         z_id;
    double      adfGeoTransform[6];

  public:
    int         cdfid;

    GMTDataset() :
        z_id(0),
        cdfid(0)
    {
        std::fill_n(adfGeoTransform, CPL_ARRAYSIZE(adfGeoTransform), 0);
    }
    ~GMTDataset();

    static GDALDataset *Open( GDALOpenInfo * );

    CPLErr      GetGeoTransform( double * padfTransform ) override;
};

/************************************************************************/
/* ==================================================================== */
/*                         GMTRasterBand                                */
/* ==================================================================== */
/************************************************************************/

class GMTRasterBand final: public GDALPamRasterBand
{
    nc_type nc_datatype;
    int         nZId;

  public:

    GMTRasterBand( GMTDataset *poDS, int nZId, int nBand );
    virtual ~GMTRasterBand() {}

    virtual CPLErr IReadBlock( int, int, void * ) override;
};

/************************************************************************/
/*                           GMTRasterBand()                            */
/************************************************************************/

GMTRasterBand::GMTRasterBand( GMTDataset *poDSIn, int nZIdIn, int nBandIn ) :
    nc_datatype(NC_NAT),
    nZId(nZIdIn)
{
    poDS = poDSIn;
    nBand = nBandIn;

    nBlockXSize = poDS->GetRasterXSize();
    nBlockYSize = 1;

/* -------------------------------------------------------------------- */
/*      Get the type of the "z" variable, our target raster array.      */
/* -------------------------------------------------------------------- */
    if( nc_inq_var( poDSIn->cdfid, nZId, nullptr, &nc_datatype, nullptr, nullptr,
                    nullptr ) != NC_NOERR )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Error in nc_var_inq() on 'z'." );
        return;
    }

    if( nc_datatype == NC_BYTE )
        eDataType = GDT_Byte;
    else if( nc_datatype == NC_SHORT )
        eDataType = GDT_Int16;
    else if( nc_datatype == NC_INT )
        eDataType = GDT_Int32;
    else if( nc_datatype == NC_FLOAT )
        eDataType = GDT_Float32;
    else if( nc_datatype == NC_DOUBLE )
        eDataType = GDT_Float64;
    else
    {
        if( nBand == 1 )
            CPLError( CE_Warning, CPLE_AppDefined,
                      "Unsupported GMT datatype (%d), treat as Float32.",
                      (int) nc_datatype );
        eDataType = GDT_Float32;
    }
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr GMTRasterBand::IReadBlock( CPL_UNUSED int nBlockXOff, int nBlockYOff,
                                  void * pImage )
{
    CPLMutexHolderD(&hNCMutex);

    size_t start[2] = {static_cast<size_t>(nBlockYOff * nBlockXSize), 0};
    size_t edge[2] = {static_cast<size_t>(nBlockXSize), 0};

    int nErr = NC_NOERR;
    int cdfid = ((GMTDataset *) poDS)->cdfid;
    if( eDataType == GDT_Byte )
        nErr = nc_get_vara_uchar( cdfid, nZId, start, edge,
                                  (unsigned char *) pImage );
    else if( eDataType == GDT_Int16 )
        nErr = nc_get_vara_short( cdfid, nZId, start, edge,
                                  (short int *) pImage );
    else if( eDataType == GDT_Int32 )
    {
#if SIZEOF_UNSIGNED_LONG == 4
            nErr = nc_get_vara_long( cdfid, nZId, start, edge,
                                     (long *) pImage );
#else
            nErr = nc_get_vara_int( cdfid, nZId, start, edge,
                                    (int *) pImage );
#endif
    }
    else if( eDataType == GDT_Float32 )
        nErr = nc_get_vara_float( cdfid, nZId, start, edge,
                                  (float *) pImage );
    else if( eDataType == GDT_Float64 )
        nErr = nc_get_vara_double( cdfid, nZId, start, edge,
                                   (double *) pImage );

    if( nErr != NC_NOERR )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "GMT scanline fetch failed: %s",
                  nc_strerror( nErr ) );
        return CE_Failure;
    }

    return CE_None;
}

/************************************************************************/
/* ==================================================================== */
/*                              GMTDataset                              */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                            ~GMTDataset()                             */
/************************************************************************/

GMTDataset::~GMTDataset()

{
    CPLMutexHolderD(&hNCMutex);
    FlushCache(true);
    nc_close (cdfid);
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr GMTDataset::GetGeoTransform( double * padfTransform )

{
    memcpy( padfTransform, adfGeoTransform, sizeof(double) * 6 );
    return CE_None;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *GMTDataset::Open( GDALOpenInfo * poOpenInfo )

{
/* -------------------------------------------------------------------- */
/*      Does this file have the GMT magic number?                    */
/* -------------------------------------------------------------------- */
    if( poOpenInfo->fpL == nullptr || poOpenInfo->nHeaderBytes < 50 )
        return nullptr;

    if( poOpenInfo->pabyHeader[0] != 'C'
        || poOpenInfo->pabyHeader[1] != 'D'
        || poOpenInfo->pabyHeader[2] != 'F'
        || poOpenInfo->pabyHeader[3] != 1 )
        return nullptr;

#ifdef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
    // We don't necessarily want to catch bugs in libnetcdf ...
    if( CPLGetConfigOption("DISABLE_OPEN_REAL_NETCDF_FILES", nullptr) )
    {
        return nullptr;
    }
#endif

    CPLMutexHolderD(&hNCMutex);

/* -------------------------------------------------------------------- */
/*      Try opening the dataset.                                        */
/* -------------------------------------------------------------------- */
    int cdfid = 0;
    if( nc_open( poOpenInfo->pszFilename, NC_NOWRITE, &cdfid ) != NC_NOERR )
        return nullptr;

    int nm_id = 0;
    int z_id = 0;
    if( nc_inq_varid( cdfid, "dimension", &nm_id ) != NC_NOERR
        || nc_inq_varid( cdfid, "z", &z_id ) != NC_NOERR )
    {
#if DEBUG_VERBOSE
        CPLError( CE_Warning, CPLE_AppDefined,
                  "%s is a GMT file, but not in GMT configuration.",
                  poOpenInfo->pszFilename );
#endif
        nc_close( cdfid );
        return nullptr;
    }

    int dim_count = 0;
    if( nc_inq_ndims( cdfid, &dim_count ) != NC_NOERR || dim_count < 2 )
    {
        nc_close( cdfid );
        return nullptr;
    }

/* -------------------------------------------------------------------- */
/*      Confirm the requested access is supported.                      */
/* -------------------------------------------------------------------- */
    if( poOpenInfo->eAccess == GA_Update )
    {
        nc_close( cdfid );
        CPLError( CE_Failure, CPLE_NotSupported,
                  "The GMT driver does not support update access to existing"
                  " datasets.\n" );
        return nullptr;
    }

/* -------------------------------------------------------------------- */
/*      Get dimensions.  If we can't find this, then this is a          */
/*      GMT file, but not a normal grid product.                     */
/* -------------------------------------------------------------------- */
    int nm[2] = { 0, 0 };
    size_t start[2] = { 0, 0 };
    size_t edge[2] = { 2, 0 };

    nc_get_vara_int(cdfid, nm_id, start, edge, nm);
    if( !GDALCheckDatasetDimensions(nm[0], nm[1]) )
    {
        nc_close( cdfid );
        return nullptr;
    }

/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */
    // Release mutex otherwise we'll deadlock with GDALDataset own mutex
    CPLReleaseMutex(hNCMutex);

    GMTDataset *poDS = new GMTDataset();
    CPLAcquireMutex(hNCMutex, 1000.0);

    poDS->cdfid = cdfid;
    poDS->z_id = z_id;

    poDS->nRasterXSize = nm[0];
    poDS->nRasterYSize = nm[1];

/* -------------------------------------------------------------------- */
/*      Fetch "z" attributes scale_factor, add_offset, and              */
/*      node_offset.                                                    */
/* -------------------------------------------------------------------- */
    double scale_factor = 1.0;
    nc_get_att_double( cdfid, z_id, "scale_factor", &scale_factor );

    double add_offset = 0.0;
    nc_get_att_double( cdfid, z_id, "add_offset", &add_offset );

    int node_offset = 1;
    nc_get_att_int( cdfid, z_id, "node_offset", &node_offset );

/* -------------------------------------------------------------------- */
/*      Get x/y range information.                                      */
/* -------------------------------------------------------------------- */
    int x_range_id = 0;
    int y_range_id = 0;

    if( nc_inq_varid (cdfid, "x_range", &x_range_id) == NC_NOERR
        && nc_inq_varid (cdfid, "y_range", &y_range_id) == NC_NOERR )
    {
        double x_range[2];
        nc_get_vara_double( cdfid, x_range_id, start, edge, x_range );

        double y_range[2];
        nc_get_vara_double( cdfid, y_range_id, start, edge, y_range );

        // Pixel is area
        if( node_offset == 1 )
        {
            poDS->adfGeoTransform[0] = x_range[0];
            poDS->adfGeoTransform[1] =
                (x_range[1] - x_range[0]) / poDS->nRasterXSize;
            poDS->adfGeoTransform[2] = 0.0;
            poDS->adfGeoTransform[3] = y_range[1];
            poDS->adfGeoTransform[4] = 0.0;
            poDS->adfGeoTransform[5] =
                (y_range[0] - y_range[1]) / poDS->nRasterYSize;
        }

        // Pixel is point - offset by half pixel.
        else /* node_offset == 0 */
        {
            poDS->adfGeoTransform[1] =
                (x_range[1] - x_range[0]) / (poDS->nRasterXSize-1);
            poDS->adfGeoTransform[0] =
                x_range[0] - poDS->adfGeoTransform[1]*0.5;
            poDS->adfGeoTransform[2] = 0.0;
            poDS->adfGeoTransform[4] = 0.0;
            poDS->adfGeoTransform[5] =
                (y_range[0] - y_range[1]) / (poDS->nRasterYSize-1);
            poDS->adfGeoTransform[3] =
                y_range[1] - poDS->adfGeoTransform[5]*0.5;
        }
    }
    else
    {
        poDS->adfGeoTransform[0] = 0.0;
        poDS->adfGeoTransform[1] = 1.0;
        poDS->adfGeoTransform[2] = 0.0;
        poDS->adfGeoTransform[3] = 0.0;
        poDS->adfGeoTransform[4] = 0.0;
        poDS->adfGeoTransform[5] = 1.0;
    }

/* -------------------------------------------------------------------- */
/*      Create band information objects.                                */
/* -------------------------------------------------------------------- */
    poDS->nBands = 1;
    poDS->SetBand( 1, new GMTRasterBand( poDS, z_id, 1 ));

    if( scale_factor != 1.0 || add_offset != 0.0 )
    {
        poDS->GetRasterBand(1)->SetOffset( add_offset );
        poDS->GetRasterBand(1)->SetScale( scale_factor );
    }

/* -------------------------------------------------------------------- */
/*      Initialize any PAM information.                                 */
/* -------------------------------------------------------------------- */
    poDS->SetDescription( poOpenInfo->pszFilename );

    // Release mutex otherwise we'll deadlock with GDALDataset own mutex.
    CPLReleaseMutex(hNCMutex);
    poDS->TryLoadXML();

/* -------------------------------------------------------------------- */
/*      Check for external overviews.                                   */
/* -------------------------------------------------------------------- */
    poDS->oOvManager.Initialize(
        poDS, poOpenInfo->pszFilename, poOpenInfo->GetSiblingFiles() );
    CPLAcquireMutex(hNCMutex, 1000.0);

    return poDS;
}

/************************************************************************/
/*                           GMTCreateCopy()                            */
/*                                                                      */
/*      This code mostly cribbed from GMT's "gmt_cdf.c" module.         */
/************************************************************************/

static GDALDataset *
GMTCreateCopy( const char * pszFilename, GDALDataset *poSrcDS,
               int bStrict, CPL_UNUSED char ** papszOptions,
               CPL_UNUSED GDALProgressFunc pfnProgress,
               CPL_UNUSED void * pProgressData )
{
/* -------------------------------------------------------------------- */
/*      Figure out general characteristics.                             */
/* -------------------------------------------------------------------- */

    CPLMutexHolderD(&hNCMutex);

    if( poSrcDS->GetRasterCount() != 1 )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Currently GMT export only supports 1 band datasets." );
        return nullptr;
    }

    GDALRasterBand *poBand = poSrcDS->GetRasterBand(1);

    const int nXSize = poSrcDS->GetRasterXSize();
    const int nYSize = poSrcDS->GetRasterYSize();

    nc_type nc_datatype;

    if( poBand->GetRasterDataType() == GDT_Int16 )
        nc_datatype = NC_SHORT;
    else if( poBand->GetRasterDataType() == GDT_Int32 )
        nc_datatype = NC_INT;
    else if( poBand->GetRasterDataType() == GDT_Float32 )
        nc_datatype = NC_FLOAT;
    else if( poBand->GetRasterDataType() == GDT_Float64 )
        nc_datatype = NC_DOUBLE;
    else if( bStrict )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Band data type %s not supported in GMT, giving up.",
                  GDALGetDataTypeName( poBand->GetRasterDataType() ) );
        return nullptr;
    }
    else if( poBand->GetRasterDataType() == GDT_Byte )
        nc_datatype = NC_SHORT;
    else if( poBand->GetRasterDataType() == GDT_UInt16 )
        nc_datatype = NC_INT;
    else if( poBand->GetRasterDataType() == GDT_UInt32 )
        nc_datatype = NC_INT;
    else
        nc_datatype = NC_FLOAT;

/* -------------------------------------------------------------------- */
/*      Establish bounds from geotransform.                             */
/* -------------------------------------------------------------------- */
    double adfGeoTransform[6] = {};

    poSrcDS->GetGeoTransform( adfGeoTransform );

    if( adfGeoTransform[2] != 0.0 || adfGeoTransform[4] != 0.0 )
    {
        CPLError( bStrict ? CE_Failure : CE_Warning, CPLE_AppDefined,
                  "Geotransform has rotational coefficients not supported in GMT." );
        if( bStrict )
            return nullptr;
    }

    const double dfXMax = adfGeoTransform[0] + adfGeoTransform[1] * nXSize;
    const double dfYMin = adfGeoTransform[3] + adfGeoTransform[5] * nYSize;

/* -------------------------------------------------------------------- */
/*      Create base file.                                               */
/* -------------------------------------------------------------------- */
    int cdfid = 0;

    int err = nc_create (pszFilename, NC_CLOBBER, &cdfid);
    if( err != NC_NOERR )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "nc_create(%s): %s",
                  pszFilename, nc_strerror( err ) );
        return nullptr;
    }

/* -------------------------------------------------------------------- */
/*      Define the dimensions and so forth.                             */
/* -------------------------------------------------------------------- */
    int side_dim = 0;
    int xysize_dim = 0;

    CPL_IGNORE_RET_VAL(nc_def_dim(cdfid, "side", 2, &side_dim));
    CPL_IGNORE_RET_VAL(nc_def_dim(cdfid, "xysize", (int) (nXSize * nYSize), &xysize_dim));

    int dims[1] = {side_dim};

    int x_range_id, y_range_id, z_range_id;
    CPL_IGNORE_RET_VAL(nc_def_var (cdfid, "x_range", NC_DOUBLE, 1, dims, &x_range_id));
    CPL_IGNORE_RET_VAL(nc_def_var (cdfid, "y_range", NC_DOUBLE, 1, dims, &y_range_id));
    CPL_IGNORE_RET_VAL(nc_def_var (cdfid, "z_range", NC_DOUBLE, 1, dims, &z_range_id));

    int inc_id, nm_id, z_id;
    CPL_IGNORE_RET_VAL(nc_def_var (cdfid, "spacing", NC_DOUBLE, 1, dims, &inc_id));
    CPL_IGNORE_RET_VAL(nc_def_var (cdfid, "dimension", NC_LONG, 1, dims, &nm_id));

    dims[0] = xysize_dim;
    CPL_IGNORE_RET_VAL(nc_def_var (cdfid, "z", nc_datatype, 1, dims, &z_id));

/* -------------------------------------------------------------------- */
/*      Assign attributes.                                              */
/* -------------------------------------------------------------------- */
    CPL_IGNORE_RET_VAL(nc_put_att_text (cdfid, x_range_id, "units", 7, "meters"));
    CPL_IGNORE_RET_VAL(nc_put_att_text (cdfid, y_range_id, "units", 7, "meters"));
    CPL_IGNORE_RET_VAL(nc_put_att_text (cdfid, z_range_id, "units", 7, "meters"));

    double default_scale = 1.0;
    nc_put_att_double (cdfid, z_id, "scale_factor", NC_DOUBLE, 1,
                       &default_scale );
    double default_offset = 0.0;
    nc_put_att_double (cdfid, z_id, "add_offset", NC_DOUBLE, 1,
                       &default_offset );

    int default_node_offset = 1; // pixel is area
    CPL_IGNORE_RET_VAL(nc_put_att_int (cdfid, z_id, "node_offset", NC_LONG, 1,
                    &default_node_offset ));
    CPL_IGNORE_RET_VAL(nc_put_att_text (cdfid, NC_GLOBAL, "title", 1, ""));
    CPL_IGNORE_RET_VAL(nc_put_att_text (cdfid, NC_GLOBAL, "source", 1, ""));

    /* leave define mode */
    nc_enddef (cdfid);

/* -------------------------------------------------------------------- */
/*      Get raster min/max.                                             */
/* -------------------------------------------------------------------- */
    double adfMinMax[2] = { 0.0, 0.0 };
    GDALComputeRasterMinMax( (GDALRasterBandH) poBand, FALSE, adfMinMax );

/* -------------------------------------------------------------------- */
/*      Set range variables.                                            */
/* -------------------------------------------------------------------- */
    size_t start[2] = { 0, 0 };
    size_t edge[2] = { 2, 0 };
    double dummy[2] = { adfGeoTransform[0], dfXMax };
    nc_put_vara_double(cdfid, x_range_id, start, edge, dummy);

    dummy[0] = dfYMin;
    dummy[1] = adfGeoTransform[3];
    nc_put_vara_double(cdfid, y_range_id, start, edge, dummy);

    dummy[0] = adfGeoTransform[1];
    dummy[1] = -adfGeoTransform[5];
    nc_put_vara_double(cdfid, inc_id, start, edge, dummy);

    int nm[2] = {nXSize, nYSize};
    nc_put_vara_int(cdfid, nm_id, start, edge, nm);

    nc_put_vara_double(cdfid, z_range_id, start, edge, adfMinMax);

/* -------------------------------------------------------------------- */
/*      Write out the image one scanline at a time.                     */
/* -------------------------------------------------------------------- */
    double *padfData = (double *) CPLMalloc( sizeof(double) * nXSize );

    edge[0] = nXSize;
    for( int iLine = 0; iLine < nYSize; iLine++ )
    {
        start[0] = iLine * nXSize;
        if( poBand->RasterIO( GF_Read, 0, iLine, nXSize, 1,
                          padfData, nXSize, 1, GDT_Float64, 0, 0, nullptr ) != CE_None )
        {
            nc_close (cdfid);
            CPLFree( padfData );
            return nullptr;
        }
        err = nc_put_vara_double( cdfid, z_id, start, edge, padfData );
        if( err != NC_NOERR )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "nc_put_vara_double(%s): %s",
                      pszFilename, nc_strerror( err ) );
            nc_close (cdfid);
            CPLFree( padfData );
            return nullptr;
        }
    }

    CPLFree( padfData );

/* -------------------------------------------------------------------- */
/*      Close file, and reopen.                                         */
/* -------------------------------------------------------------------- */
    nc_close (cdfid);

/* -------------------------------------------------------------------- */
/*      Re-open dataset, and copy any auxiliary pam information.         */
/* -------------------------------------------------------------------- */
    GDALPamDataset *poDS = (GDALPamDataset *)
        GDALOpen( pszFilename, GA_ReadOnly );

    if( poDS )
        poDS->CloneInfo( poSrcDS, GCIF_PAM_DEFAULT );

    return poDS;
}

/************************************************************************/
/*                          GDALRegister_GMT()                          */
/************************************************************************/

void GDALRegister_GMT()

{
    if( !GDAL_CHECK_VERSION( "GMT driver" ) )
        return;

    if( GDALGetDriverByName( "GMT" ) != nullptr )
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription( "GMT" );
    poDriver->SetMetadataItem( GDAL_DCAP_RASTER, "YES" );
    poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, "GMT NetCDF Grid Format" );
    poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, "drivers/raster/gmt.html" );
    poDriver->SetMetadataItem( GDAL_DMD_EXTENSION, "nc" );
    poDriver->SetMetadataItem( GDAL_DMD_CREATIONDATATYPES,
                               "Int16 Int32 Float32 Float64" );

    poDriver->pfnOpen = GMTDataset::Open;
    poDriver->pfnCreateCopy = GMTCreateCopy;

    GetGDALDriverManager()->RegisterDriver( poDriver );
}
