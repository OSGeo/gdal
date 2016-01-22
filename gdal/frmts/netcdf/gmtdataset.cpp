/******************************************************************************
 * $Id$
 *
 * Project:  netCDF read/write Driver
 * Purpose:  GDAL bindings over netCDF library for GMT Grids.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2004, Frank Warmerdam
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

#include "gdal_pam.h"
#include "gdal_frmts.h"
#include "netcdf.h"
#include "cpl_multiproc.h"

CPL_CVSID("$Id$");

extern void *hNCMutex; /* shared with netcdf. See netcdfdataset.cpp */

/************************************************************************/
/* ==================================================================== */
/*			     GMTDataset				        */
/* ==================================================================== */
/************************************************************************/

class GMTRasterBand;

class GMTDataset : public GDALPamDataset
{
    int         z_id;
    double      adfGeoTransform[6];

  public:
    int         cdfid;

		~GMTDataset();
    
    static GDALDataset *Open( GDALOpenInfo * );

    CPLErr 	GetGeoTransform( double * padfTransform );
};

/************************************************************************/
/* ==================================================================== */
/*                         GMTRasterBand                                */
/* ==================================================================== */
/************************************************************************/

class GMTRasterBand : public GDALPamRasterBand
{
    nc_type nc_datatype;
    int         nZId;

  public:

    		GMTRasterBand( GMTDataset *poDS, int nZId, int nBand );
    
    virtual CPLErr IReadBlock( int, int, void * );
};


/************************************************************************/
/*                           GMTRasterBand()                            */
/************************************************************************/

GMTRasterBand::GMTRasterBand( GMTDataset *poDS, int nZId, int nBand )

{
    this->poDS = poDS;
    this->nBand = nBand;
    this->nZId = nZId;
    
    nBlockXSize = poDS->GetRasterXSize();
    nBlockYSize = 1;

/* -------------------------------------------------------------------- */
/*      Get the type of the "z" variable, our target raster array.      */
/* -------------------------------------------------------------------- */
    if( nc_inq_var( poDS->cdfid, nZId, NULL, &nc_datatype, NULL, NULL,
                    NULL ) != NC_NOERR )
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
    size_t start[2], edge[2];
    int    nErr = NC_NOERR;
    int    cdfid = ((GMTDataset *) poDS)->cdfid;
    
    CPLMutexHolderD(&hNCMutex);

    start[0] = nBlockYOff * nBlockXSize;
    edge[0] = nBlockXSize;

    if( eDataType == GDT_Byte )
        nErr = nc_get_vara_uchar( cdfid, nZId, start, edge, 
                                  (unsigned char *) pImage );
    else if( eDataType == GDT_Int16 )
        nErr = nc_get_vara_short( cdfid, nZId, start, edge, 
                                  (short int *) pImage );
    else if( eDataType == GDT_Int32 )
    {
        if( sizeof(long) == 4 )
            nErr = nc_get_vara_long( cdfid, nZId, start, edge, 
                                     (long *) pImage );
        else
            nErr = nc_get_vara_int( cdfid, nZId, start, edge, 
                                    (int *) pImage );
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
    else
        return CE_None;
}

/************************************************************************/
/* ==================================================================== */
/*				GMTDataset				*/
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                            ~GMTDataset()                             */
/************************************************************************/

GMTDataset::~GMTDataset()

{
    FlushCache();
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
    if( poOpenInfo->fp == NULL || poOpenInfo->nHeaderBytes < 50 )
        return NULL;

    if( poOpenInfo->pabyHeader[0] != 'C' 
        || poOpenInfo->pabyHeader[1] != 'D' 
        || poOpenInfo->pabyHeader[2] != 'F' 
        || poOpenInfo->pabyHeader[3] != 1 )
        return NULL;
    
    CPLMutexHolderD(&hNCMutex);

/* -------------------------------------------------------------------- */
/*      Try opening the dataset.                                        */
/* -------------------------------------------------------------------- */
    int cdfid, nm_id, dim_count, z_id;

    if( nc_open( poOpenInfo->pszFilename, NC_NOWRITE, &cdfid ) != NC_NOERR )
        return NULL;

    if( nc_inq_varid( cdfid, "dimension", &nm_id ) != NC_NOERR 
        || nc_inq_varid( cdfid, "z", &z_id ) != NC_NOERR )
    {
#ifdef notdef
        CPLError( CE_Warning, CPLE_AppDefined, 
                  "%s is a GMT file, but not in GMT configuration.",
                  poOpenInfo->pszFilename );
#endif
        nc_close( cdfid );
        return NULL;
    }

    if( nc_inq_ndims( cdfid, &dim_count ) != NC_NOERR || dim_count < 2 )
    {
        nc_close( cdfid );
        return NULL;
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
        return NULL;
    }
    
/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */
    GMTDataset 	*poDS;

    CPLReleaseMutex(hNCMutex);  // Release mutex otherwise we'll deadlock with GDALDataset own mutex
    poDS = new GMTDataset();
    CPLAcquireMutex(hNCMutex, 1000.0);

    poDS->cdfid = cdfid;
    poDS->z_id = z_id;
    
/* -------------------------------------------------------------------- */
/*      Get dimensions.  If we can't find this, then this is a          */
/*      GMT file, but not a normal grid product.                     */
/* -------------------------------------------------------------------- */
    size_t start[2], edge[2];
    int    nm[2];

    start[0] = 0;
    edge[0] = 2;

    nc_get_vara_int(cdfid, nm_id, start, edge, nm);
    
    poDS->nRasterXSize = nm[0];
    poDS->nRasterYSize = nm[1];

/* -------------------------------------------------------------------- */
/*      Fetch "z" attributes scale_factor, add_offset, and              */
/*      node_offset.                                                    */
/* -------------------------------------------------------------------- */
    double scale_factor=1.0, add_offset=0.0;
    int node_offset = 1;

    nc_get_att_double( cdfid, z_id, "scale_factor", &scale_factor );
    nc_get_att_double( cdfid, z_id, "add_offset", &add_offset );
    nc_get_att_int( cdfid, z_id, "node_offset", &node_offset );

/* -------------------------------------------------------------------- */
/*      Get x/y range information.                                      */
/* -------------------------------------------------------------------- */
    int x_range_id, y_range_id;

    if( nc_inq_varid (cdfid, "x_range", &x_range_id) == NC_NOERR 
        && nc_inq_varid (cdfid, "y_range", &y_range_id) == NC_NOERR )
    {
        double x_range[2], y_range[2];

        nc_get_vara_double( cdfid, x_range_id, start, edge, x_range );
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

    CPLReleaseMutex(hNCMutex); // Release mutex otherwise we'll deadlock with GDALDataset own mutex
    poDS->TryLoadXML();

/* -------------------------------------------------------------------- */
/*      Check for external overviews.                                   */
/* -------------------------------------------------------------------- */
    poDS->oOvManager.Initialize( poDS, poOpenInfo->pszFilename, poOpenInfo->papszSiblingFiles );
    CPLAcquireMutex(hNCMutex, 1000.0);

    return( poDS );
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
    nc_type nc_datatype;
    GDALRasterBand *poBand;
    int nXSize, nYSize;

    CPLMutexHolderD(&hNCMutex);

    if( poSrcDS->GetRasterCount() != 1 )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Currently GMT export only supports 1 band datasets." );
        return NULL;
    }

    poBand = poSrcDS->GetRasterBand(1);

    nXSize = poSrcDS->GetRasterXSize();
    nYSize = poSrcDS->GetRasterYSize();
    
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
        return NULL;
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
    double adfGeoTransform[6];
    double dfXMax, dfYMin;

    poSrcDS->GetGeoTransform( adfGeoTransform );
    
    if( adfGeoTransform[2] != 0.0 || adfGeoTransform[4] != 0.0 )
    {
        CPLError( bStrict ? CE_Failure : CE_Warning, CPLE_AppDefined, 
                  "Geotransform has rotational coefficients not supported in GMT." );
        if( bStrict )
            return NULL;
    }

    dfXMax = adfGeoTransform[0] + adfGeoTransform[1] * nXSize;
    dfYMin = adfGeoTransform[3] + adfGeoTransform[5] * nYSize;
    
/* -------------------------------------------------------------------- */
/*      Create base file.                                               */
/* -------------------------------------------------------------------- */
    int cdfid, err;

    err = nc_create (pszFilename, NC_CLOBBER,&cdfid);
    if( err != NC_NOERR )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "nc_create(%s): %s", 
                  pszFilename, nc_strerror( err ) );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Define the dimensions and so forth.                             */
/* -------------------------------------------------------------------- */
    int side_dim, xysize_dim, dims[1];
    int x_range_id, y_range_id, z_range_id, inc_id, nm_id, z_id;

    nc_def_dim(cdfid, "side", 2, &side_dim);
    nc_def_dim(cdfid, "xysize", (int) (nXSize * nYSize), &xysize_dim);

    dims[0]		= side_dim;
    nc_def_var (cdfid, "x_range", NC_DOUBLE, 1, dims, &x_range_id);
    nc_def_var (cdfid, "y_range", NC_DOUBLE, 1, dims, &y_range_id);
    nc_def_var (cdfid, "z_range", NC_DOUBLE, 1, dims, &z_range_id);
    nc_def_var (cdfid, "spacing", NC_DOUBLE, 1, dims, &inc_id);
    nc_def_var (cdfid, "dimension", NC_LONG, 1, dims, &nm_id);

    dims[0]		= xysize_dim;
    nc_def_var (cdfid, "z", nc_datatype, 1, dims, &z_id);

/* -------------------------------------------------------------------- */
/*      Assign attributes.                                              */
/* -------------------------------------------------------------------- */
    double default_scale = 1.0;
    double default_offset = 0.0;
    int default_node_offset = 1; // pixel is area

    nc_put_att_text (cdfid, x_range_id, "units", 7, "meters");
    nc_put_att_text (cdfid, y_range_id, "units", 7, "meters");
    nc_put_att_text (cdfid, z_range_id, "units", 7, "meters");

    nc_put_att_double (cdfid, z_id, "scale_factor", NC_DOUBLE, 1, 
                       &default_scale );
    nc_put_att_double (cdfid, z_id, "add_offset", NC_DOUBLE, 1, 
                       &default_offset );

    nc_put_att_int (cdfid, z_id, "node_offset", NC_LONG, 1, 
                    &default_node_offset );
    nc_put_att_text (cdfid, NC_GLOBAL, "title", 1, "");
    nc_put_att_text (cdfid, NC_GLOBAL, "source", 1, "");
	
    /* leave define mode */
    nc_enddef (cdfid);

/* -------------------------------------------------------------------- */
/*      Get raster min/max.                                             */
/* -------------------------------------------------------------------- */
    double adfMinMax[2];
    GDALComputeRasterMinMax( (GDALRasterBandH) poBand, FALSE, adfMinMax );
	
/* -------------------------------------------------------------------- */
/*      Set range variables.                                            */
/* -------------------------------------------------------------------- */
    size_t start[2], edge[2];
    double dummy[2];
    int nm[2];
	
    start[0] = 0;
    edge[0] = 2;
    dummy[0] = adfGeoTransform[0];
    dummy[1] = dfXMax;
    nc_put_vara_double(cdfid, x_range_id, start, edge, dummy);

    dummy[0] = dfYMin;
    dummy[1] = adfGeoTransform[3];
    nc_put_vara_double(cdfid, y_range_id, start, edge, dummy);

    dummy[0] = adfGeoTransform[1];
    dummy[1] = -adfGeoTransform[5];
    nc_put_vara_double(cdfid, inc_id, start, edge, dummy);

    nm[0] = nXSize;
    nm[1] = nYSize;
    nc_put_vara_int(cdfid, nm_id, start, edge, nm);

    nc_put_vara_double(cdfid, z_range_id, start, edge, adfMinMax);

/* -------------------------------------------------------------------- */
/*      Write out the image one scanline at a time.                     */
/* -------------------------------------------------------------------- */
    double *padfData;
    int  iLine;

    padfData = (double *) CPLMalloc( sizeof(double) * nXSize );

    edge[0] = nXSize;
    for( iLine = 0; iLine < nYSize; iLine++ )
    {
        start[0] = iLine * nXSize;
        poBand->RasterIO( GF_Read, 0, iLine, nXSize, 1, 
                          padfData, nXSize, 1, GDT_Float64, 0, 0 );
        err = nc_put_vara_double( cdfid, z_id, start, edge, padfData );
        if( err != NC_NOERR )
        {
            CPLError( CE_Failure, CPLE_AppDefined, 
                      "nc_put_vara_double(%s): %s", 
                      pszFilename, nc_strerror( err ) );
            nc_close (cdfid);
            return( NULL );
        }
    }
    
    CPLFree( padfData );

/* -------------------------------------------------------------------- */
/*      Close file, and reopen.                                         */
/* -------------------------------------------------------------------- */
    nc_close (cdfid);

/* -------------------------------------------------------------------- */
/*      Re-open dataset, and copy any auxilary pam information.         */
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
    GDALDriver	*poDriver;
    
    if (! GDAL_CHECK_VERSION("GMT driver"))
        return;

    if( GDALGetDriverByName( "GMT" ) == NULL )
    {
        poDriver = new GDALDriver();
        
        poDriver->SetDescription( "GMT" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, 
                                   "GMT NetCDF Grid Format" );
        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, 
                                   "frmt_various.html#GMT" );
        poDriver->SetMetadataItem( GDAL_DMD_EXTENSION, "nc" );

        poDriver->SetMetadataItem( GDAL_DMD_CREATIONDATATYPES, 
                                   "Int16 Int32 Float32 Float64" );

        poDriver->pfnOpen = GMTDataset::Open;
        poDriver->pfnCreateCopy = GMTCreateCopy;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}
