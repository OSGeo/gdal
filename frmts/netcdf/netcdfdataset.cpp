/******************************************************************************
 * $Id$
 *
 * Project:  netCDF read/write Driver
 * Purpose:  GDAL bindings over netCDF library.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2004, Frank Warmerdam
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
 ******************************************************************************
 * 
 * $Log$
 * Revision 1.5  2004/10/16 14:57:31  fwarmerdam
 * Substantial rewrite by Radim to sometimes handle COARDS style datasets
 * but really only under some circumstances.   CreateCopy() removed since
 * no COARDS implementation is available.
 *
 * Revision 1.4  2004/10/14 14:51:31  fwarmerdam
 * Fixed last fix.
 *
 * Revision 1.3  2004/10/14 13:59:13  fwarmerdam
 * Added error for non-GMT netCDF files.
 *
 * Revision 1.2  2004/01/07 21:02:19  warmerda
 * fix up driver metadata
 *
 * Revision 1.1  2004/01/07 20:05:53  warmerda
 * New
 *
 */

#include "gdal_priv.h"
#include "gdal_frmts.h"
#include "netcdf.h"

CPL_CVSID("$Id$");

/************************************************************************/
/* ==================================================================== */
/*			     netCDFDataset				*/
/* ==================================================================== */
/************************************************************************/

class netCDFRasterBand;

class netCDFDataset : public GDALDataset
{
    double      adfGeoTransform[6];

  public:
    int         cdfid;

		~netCDFDataset();
    
    static GDALDataset *Open( GDALOpenInfo * );

    CPLErr 	GetGeoTransform( double * padfTransform );
};

/************************************************************************/
/* ==================================================================== */
/*                         netCDFRasterBand                             */
/* ==================================================================== */
/************************************************************************/

class netCDFRasterBand : public GDALRasterBand
{
    nc_type nc_datatype;
    int         nZId;
    int		nLevel;

  public:

    netCDFRasterBand( netCDFDataset *poDS, int nZId, int nLevel, int nBand );
    
    virtual CPLErr IReadBlock( int, int, void * );
};


/************************************************************************/
/*                          netCDFRasterBand()                          */
/************************************************************************/

netCDFRasterBand::netCDFRasterBand( netCDFDataset *poDS, int nZId, int nLevel, int nBand )

{
    this->poDS = poDS;
    this->nBand = nBand;
    this->nZId = nZId;
    this->nLevel = nLevel;
    
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
                      "Unsupported netCDF datatype (%d), treat as Float32.", 
                      (int) nc_datatype );
        eDataType = GDT_Float32;
    }
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr netCDFRasterBand::IReadBlock( int nBlockXOff, int nBlockYOff,
                                  void * pImage )

{
    int    nErr;
    int    cdfid = ((netCDFDataset *) poDS)->cdfid;
    size_t start[3], edge[3];

    int nd;
    nc_inq_varndims ( cdfid, nZId, &nd );

    start[nd-1] = 0;          // x
    start[nd-2] = nBlockYOff; // y
        
    edge[nd-1] = nBlockXSize; 
    edge[nd-2] = 1;

    if( nd == 3 )
    {
        start[nd-3] = nLevel;     // z
        edge[nd-3] = 1;
    }

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
                  "netCDF scanline fetch failed: %s", 
                  nc_strerror( nErr ) );
        return CE_Failure;
    }
    else
        return CE_None;
}

/************************************************************************/
/* ==================================================================== */
/*				netCDFDataset				*/
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                           ~netCDFDataset()                           */
/************************************************************************/

netCDFDataset::~netCDFDataset()

{
    nc_close (cdfid);
}


/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr netCDFDataset::GetGeoTransform( double * padfTransform )

{
    memcpy( padfTransform, adfGeoTransform, sizeof(double) * 6 );
    return CE_None;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *netCDFDataset::Open( GDALOpenInfo * poOpenInfo )

{
/* -------------------------------------------------------------------- */
/*      Does this file have the netCDF magic number?                    */
/* -------------------------------------------------------------------- */
    if( poOpenInfo->fp == NULL || poOpenInfo->nHeaderBytes < 50 )
        return NULL;

    if( poOpenInfo->pabyHeader[0] != 'C' 
        || poOpenInfo->pabyHeader[1] != 'D' 
        || poOpenInfo->pabyHeader[2] != 'F' 
        || poOpenInfo->pabyHeader[3] != 1 )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Try opening the dataset.                                        */
/* -------------------------------------------------------------------- */
    int cdfid, dim_count, var_count;

    if( nc_open( poOpenInfo->pszFilename, NC_NOWRITE, &cdfid ) != NC_NOERR )
        return NULL;

    if( nc_inq_ndims( cdfid, &dim_count ) != NC_NOERR || dim_count < 2 )
    {
        CPLError( CE_Warning, CPLE_AppDefined, 
                  "%s is a netCDF file, but not in GMT configuration.",
                  poOpenInfo->pszFilename );

        nc_close( cdfid );
        return NULL;
    }
    CPLDebug( "GDAL_netCDF", "dim_count = %d\n", dim_count );

/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */
    netCDFDataset 	*poDS;

    poDS = new netCDFDataset();

    poDS->cdfid = cdfid;
    
/* -------------------------------------------------------------------- */
/*      Get dimensions.  If we can't find this, then this is a          */
/*      netCDF file, but not a normal grid product.                     */
/* -------------------------------------------------------------------- */
    size_t xdim, ydim;
    
    nc_inq_dimlen ( cdfid, 0, &xdim );
    nc_inq_dimlen ( cdfid, 1, &ydim );
    
    poDS->nRasterXSize = xdim;
    poDS->nRasterYSize = ydim;

/* -------------------------------------------------------------------- */
/*      Get x/y range information.                                      */
/* -------------------------------------------------------------------- */
    size_t start[2], edge[2];
    int x_range_id, y_range_id;

    if( nc_inq_varid (cdfid, "x_range", &x_range_id) == NC_NOERR 
        && nc_inq_varid (cdfid, "y_range", &y_range_id) == NC_NOERR )
    {
        double x_range[2], y_range[2];

        nc_get_vara_double( cdfid, x_range_id, start, edge, x_range );
        nc_get_vara_double( cdfid, y_range_id, start, edge, y_range );

        poDS->adfGeoTransform[0] = x_range[0];
        poDS->adfGeoTransform[1] = 
            (x_range[1] - x_range[0]) / poDS->nRasterXSize;
        poDS->adfGeoTransform[2] = 0.0;
        poDS->adfGeoTransform[3] = y_range[1];
        poDS->adfGeoTransform[4] = 0.0;
        poDS->adfGeoTransform[5] = 
            (y_range[0] - y_range[1]) / poDS->nRasterYSize;
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
    if ( nc_inq_nvars ( cdfid, &var_count) != NC_NOERR )
	return NULL;
    
    CPLDebug( "GDAL_netCDF", "var_count = %d\n", var_count );

    // Add new band for each variable - 3. dimension level
    int i = 0;
    //printf ( "var_count = %d\n", var_count );
    for ( int var = 0; var < var_count; var++ ) {
	int nd;
	
	nc_inq_varndims ( cdfid, var, &nd );
	if ( nd < 2 ) 
	    continue;
    
	//printf ( "var = %d nd = %d\n", var, nd );

	// Number of leves in 3. dimension
	size_t lev_count = 1;
	if ( dim_count > 2 ) {
	    nc_inq_dimlen ( cdfid, 2, &lev_count );
	}
        //printf ( "lev_count = %d\n", lev_count );
		
	for ( int lev = 0; lev < (int) lev_count; lev++ ) {
	    //printf ( "lev = %d\n", lev );
            poDS->SetBand( i+1, new netCDFRasterBand( poDS, var, lev, i+1 ));
	    i++;
	}
    }
    poDS->nBands = i;
        
    return( poDS );
}
/************************************************************************/
/*                          GDALRegister_netCDF()                          */
/************************************************************************/

void GDALRegister_netCDF()

{
    GDALDriver	*poDriver;

    if( GDALGetDriverByName( "netCDF" ) == NULL )
    {
        poDriver = new GDALDriver();
        
        poDriver->SetDescription( "netCDF" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, 
                                   "network Common Data Format" );
        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, 
                                   "frmt_various.html#netCDF" );
        poDriver->SetMetadataItem( GDAL_DMD_EXTENSION, "nc" );

        poDriver->pfnOpen = netCDFDataset::Open;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}
