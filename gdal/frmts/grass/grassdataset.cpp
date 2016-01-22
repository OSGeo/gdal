/******************************************************************************
 * $Id$
 *
 * Project:  GRASS Driver
 * Purpose:  Implement GRASS raster read/write support
 * Author:   Frank Warmerdam, warmerda@home.com
 *
 ******************************************************************************
 * Copyright (c) 2000, Frank Warmerdam
 * Copyright (c) 2007-2009, Even Rouault <even dot rouault at mines-paris dot org>
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

#include <libgrass.h>

#include "gdal_priv.h"
#include "cpl_string.h"
#include "ogr_spatialref.h"

CPL_CVSID("$Id$");

CPL_C_START
void	GDALRegister_GRASS(void);
CPL_C_END

/************************************************************************/
/*                         Grass2CPLErrorHook()                         */
/************************************************************************/

int Grass2CPLErrorHook( char * pszMessage, int bFatal )

{
    if( !bFatal )
        CPLDebug( "libgrass", "%s", pszMessage );
    else
        CPLError( CE_Fatal, CPLE_AppDefined, "libgrass: %s", pszMessage );

    return 0;
}

/************************************************************************/
/* ==================================================================== */
/*				GRASSDataset				*/
/* ==================================================================== */
/************************************************************************/

class GRASSRasterBand;

class GRASSDataset : public GDALDataset
{
    friend class GRASSRasterBand;

    char	*pszProjection;

    double	adfGeoTransform[6];

  public:
                 GRASSDataset();
                 ~GRASSDataset();

    virtual const char *GetProjectionRef(void);
    virtual CPLErr GetGeoTransform( double * );

    static GDALDataset *Open( GDALOpenInfo * );
};

/************************************************************************/
/* ==================================================================== */
/*                            GRASSRasterBand                           */
/* ==================================================================== */
/************************************************************************/

class GRASSRasterBand : public GDALRasterBand
{
    friend class GRASSDataset;

    int		hCell;
    int         nGRSType;

    GDALColorTable *poCT;

    int		bHaveMinMax;
    double	dfCellMin;
    double	dfCellMax;

    double	dfNoData;

  public:

                   GRASSRasterBand( GRASSDataset *, int, 
                                    const char *, const char * );
    virtual        ~GRASSRasterBand();

    virtual CPLErr IReadBlock( int, int, void * );
    virtual GDALColorInterp GetColorInterpretation();
    virtual GDALColorTable *GetColorTable();
    virtual double GetMinimum( int *pbSuccess = NULL );
    virtual double GetMaximum( int *pbSuccess = NULL );
    virtual double GetNoDataValue( int *pbSuccess = NULL );
};


/************************************************************************/
/*                          GRASSRasterBand()                           */
/************************************************************************/

GRASSRasterBand::GRASSRasterBand( GRASSDataset *poDS, int nBand,
                                  const char * pszMapset,
                                  const char * pszCellName )

{
    struct Cell_head	sCellInfo;

    this->poDS = poDS;
    this->nBand = nBand;

    G_get_cellhd( (char *) pszCellName, (char *) pszMapset, &sCellInfo );
    nGRSType = G_raster_map_type( (char *) pszCellName, (char *) pszMapset );

/* -------------------------------------------------------------------- */
/*      Get min/max values.                                             */
/* -------------------------------------------------------------------- */
    struct FPRange sRange;

    if( G_read_fp_range( (char *) pszCellName, (char *) pszMapset, 
                         &sRange ) == -1 )
    {
        bHaveMinMax = FALSE;
    }
    else
    {
        bHaveMinMax = TRUE;
        G_get_fp_range_min_max( &sRange, &dfCellMin, &dfCellMax );
    }

/* -------------------------------------------------------------------- */
/*      Setup band type, and preferred nodata value.                    */
/* -------------------------------------------------------------------- */
    dfNoData = 0.0;
    if( nGRSType == CELL_TYPE && sCellInfo.format == 0 )
    {
        if( bHaveMinMax && dfCellMin < 1.0 && dfCellMax > 254.0 )
        {
            this->eDataType = GDT_UInt16;
            dfNoData = 256.0;
        }
        else
        {
            this->eDataType = GDT_Byte;
            if( dfCellMax < 255.0 )
                dfNoData = 255.0;
            else
                dfNoData = 0.0;
        }
    }
    else if( nGRSType == CELL_TYPE && sCellInfo.format == 1 )
    {
        this->eDataType = GDT_UInt16;
        dfNoData = 65535.0;
    }
    else if( nGRSType == CELL_TYPE )
    {
        this->eDataType = GDT_UInt32;
        dfNoData = 65535.0;
    }
    else if( nGRSType == FCELL_TYPE )
    {
        this->eDataType = GDT_Float32;
        dfNoData = -12345.0;
    }
    else if( nGRSType == DCELL_TYPE )
    {
        this->eDataType = GDT_Float64;
        dfNoData = -12345.0;
    }
    
    nBlockXSize = poDS->nRasterXSize;;
    nBlockYSize = 1;

    hCell = G_open_cell_old((char *) pszCellName, (char *) pszMapset);

/* -------------------------------------------------------------------- */
/*      Do we have a color table?                                       */
/* -------------------------------------------------------------------- */
    struct Colors sGrassColors;

    poCT = NULL;
    if( G_read_colors( (char *) pszCellName, (char *) pszMapset, 
                       &sGrassColors ) == 1 )
    {
        poCT = new GDALColorTable();
        for( int iColor = 0; iColor < 256; iColor++ )
        {
            int	nRed, nGreen, nBlue;
            GDALColorEntry    sColor;

            if( G_get_color( iColor, &nRed, &nGreen, &nBlue, &sGrassColors ) )
            {
                sColor.c1 = nRed;
                sColor.c2 = nGreen;
                sColor.c3 = nBlue;
                sColor.c4 = 255;

                poCT->SetColorEntry( iColor, &sColor );
            }
            else
            {
                sColor.c1 = 0;
                sColor.c2 = 0;
                sColor.c3 = 0;
                sColor.c4 = 0;

                poCT->SetColorEntry( iColor, &sColor );
            }
        }

        G_free_colors( &sGrassColors );
    }
}

/************************************************************************/
/*                          ~GRASSRasterBand()                          */
/************************************************************************/

GRASSRasterBand::~GRASSRasterBand()

{
    if( poCT != NULL )
        delete poCT;

    if( hCell >= 0 )
        G_close_cell( hCell );
}


/************************************************************************/
/*                             IReadBlock()                             */
/*                                                                      */
/*      We only do "null" testing for floating point values.  We        */
/*      assume integer values are having the null raster entries set    */
/*      to zero which is the "nodata" value for integer layers.         */
/************************************************************************/

CPLErr GRASSRasterBand::IReadBlock( int nBlockXOff, int nBlockYOff,
                                  void * pImage )

{
    char *pachNullBuf;
    
    pachNullBuf = (char *) CPLMalloc(nBlockXSize);
    G_get_null_value_row( hCell, pachNullBuf, nBlockYOff );
            
    if( eDataType == GDT_Float32 || eDataType == GDT_Float64 
        || eDataType == GDT_UInt32 )
    {
        G_get_raster_row( hCell, pImage, nBlockYOff, nGRSType  );

        for( int i = 0; i < nBlockXSize; i++ )
        {
            if( pachNullBuf[i] != 0 )
            {
                if( eDataType == GDT_UInt32 )
                    ((GUInt32 *) pImage)[i] = (GUInt32) dfNoData;
                else if( eDataType == GDT_Float32 )
                    ((float *) pImage)[i] = dfNoData;
                else 
                    ((double *) pImage)[i] = dfNoData;
            }
        }
        
    }
    else
    {
        GUInt32 *panRow = (GUInt32 *) CPLMalloc(4 * nBlockXSize);
        
        G_get_raster_row( hCell, panRow, nBlockYOff, nGRSType  );

        for( int i = 0; i < nBlockXSize; i++ )
        {
            if( pachNullBuf[i] != 0 )
                panRow[i] = (GUInt32) dfNoData;
        }

        GDALCopyWords( panRow, GDT_UInt32, 4, 
                       pImage, eDataType, GDALGetDataTypeSize(eDataType)/8,
                       nBlockXSize );

        CPLFree( panRow );
    }

    CPLFree( pachNullBuf );

    return CE_None;
}

/************************************************************************/
/*                       GetColorInterpretation()                       */
/************************************************************************/

GDALColorInterp GRASSRasterBand::GetColorInterpretation()

{
    if( poCT != NULL )
        return GCI_PaletteIndex;
    else
        return GCI_GrayIndex;
}

/************************************************************************/
/*                           GetColorTable()                            */
/************************************************************************/

GDALColorTable *GRASSRasterBand::GetColorTable()

{
    return poCT;
}

/************************************************************************/
/*                             GetMinimum()                             */
/************************************************************************/

double GRASSRasterBand::GetMinimum( int *pbSuccess )

{
    if( pbSuccess )
        *pbSuccess = bHaveMinMax;

    if( bHaveMinMax )
        return dfCellMin;

    else if( eDataType == GDT_Float32 || eDataType == GDT_Float64 )
        return -4294967295.0;
    else
        return 0;
}

/************************************************************************/
/*                             GetMaximum()                             */
/************************************************************************/

double GRASSRasterBand::GetMaximum( int *pbSuccess )

{
    if( pbSuccess )
        *pbSuccess = bHaveMinMax;

    if( bHaveMinMax )
        return dfCellMax;

    else if( eDataType == GDT_Float32 || eDataType == GDT_Float64 )
        return 4294967295.0;
    else if( eDataType == GDT_UInt32 )
        return 4294967295.0;
    else if( eDataType == GDT_UInt16 )
        return 65535;
    else 
        return 255;
}

/************************************************************************/
/*                           GetNoDataValue()                           */
/************************************************************************/

double GRASSRasterBand::GetNoDataValue( int *pbSuccess )

{
    if( pbSuccess )
        *pbSuccess = TRUE;

    return dfNoData;
}

/************************************************************************/
/* ==================================================================== */
/*                             GRASSDataset                             */
/* ==================================================================== */
/************************************************************************/


/************************************************************************/
/*                            GRASSDataset()                            */
/************************************************************************/

GRASSDataset::GRASSDataset()

{
    pszProjection = NULL;

    adfGeoTransform[0] = 0.0;
    adfGeoTransform[1] = 1.0;
    adfGeoTransform[2] = 0.0;
    adfGeoTransform[3] = 0.0;
    adfGeoTransform[4] = 0.0;
    adfGeoTransform[5] = 1.0;
}

/************************************************************************/
/*                           ~GRASSDataset()                            */
/************************************************************************/

GRASSDataset::~GRASSDataset()

{
    CPLFree( pszProjection );
}

/************************************************************************/
/*                          GetProjectionRef()                          */
/************************************************************************/

const char *GRASSDataset::GetProjectionRef() 
{
    if( pszProjection == NULL )
        return "";
    else
        return pszProjection;
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr GRASSDataset::GetGeoTransform( double * padfGeoTransform ) 
{
    memcpy( padfGeoTransform, adfGeoTransform, sizeof(double) * 6 );
    
    return CE_None;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

typedef int (*GrassErrorHandler)();

GDALDataset *GRASSDataset::Open( GDALOpenInfo * poOpenInfo )

{
    static int	bDoneGISInit = FALSE;
    char	*pszMapset = NULL, *pszCell = NULL;
    char        **papszCells = NULL;
    char        **papszMapsets = NULL;

    if( !bDoneGISInit )
    {
        G_set_error_routine( (GrassErrorHandler) Grass2CPLErrorHook );
        G_gisinit_2( "GDAL", NULL, NULL, NULL );
    }

/* -------------------------------------------------------------------- */
/*      Check if this is a valid grass cell.                            */
/* -------------------------------------------------------------------- */
    if( G_check_cell( poOpenInfo->pszFilename, &pszMapset, &pszCell ) )
    {
        papszCells = CSLAddString( papszCells, pszCell );
        papszMapsets = CSLAddString( papszMapsets, pszMapset );

        G_free( pszMapset );
        G_free( pszCell );
    }
    
/* -------------------------------------------------------------------- */
/*      Check if this is a valid GRASS imagery group.                   */
/* -------------------------------------------------------------------- */
    else if( I_check_group( poOpenInfo->pszFilename, &pszMapset, &pszCell ) )
    {
        struct Ref ref;

        I_init_group_ref( &ref );
        I_get_group_ref( pszCell, &ref );
        
        for( int iRef = 0; iRef < ref.nfiles; iRef++ )
        {
            papszCells = CSLAddString( papszCells, ref.file[iRef].name );
            papszMapsets = CSLAddString( papszMapsets, ref.file[iRef].mapset );
        }

        I_free_group_ref( &ref );

        G_free( pszMapset );
        G_free( pszCell );
    }

    else
        return NULL;

/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */
    GRASSDataset 	*poDS;

    poDS = new GRASSDataset();

    /* notdef: should only allow read access to an existing cell, right? */
    poDS->eAccess = poOpenInfo->eAccess;
    
/* -------------------------------------------------------------------- */
/*      Capture some information from the file that is of interest.     */
/* -------------------------------------------------------------------- */
    struct Cell_head	sCellInfo;
    
    if( G_get_cellhd( papszCells[0], papszMapsets[0], &sCellInfo ) != 0 )
    {
        /* notdef: report failure. */
        return NULL;
    }

    poDS->nRasterXSize = sCellInfo.cols;
    poDS->nRasterYSize = sCellInfo.rows;

    G_set_window( &sCellInfo );

    poDS->adfGeoTransform[0] = sCellInfo.west;
    poDS->adfGeoTransform[1] = sCellInfo.ew_res;
    poDS->adfGeoTransform[2] = 0.0;
    poDS->adfGeoTransform[3] = sCellInfo.north;
    poDS->adfGeoTransform[4] = 0.0;
    poDS->adfGeoTransform[5] = -1 * sCellInfo.ns_res;

/* -------------------------------------------------------------------- */
/*      Try to get a projection definition.                             */
/* -------------------------------------------------------------------- */
    char	*pszProj4;

    pszProj4 = G_get_cell_as_proj4( papszCells[0], papszMapsets[0] );
    if( pszProj4 != NULL )
    {
        OGRSpatialReference   oSRS;

        if( oSRS.importFromProj4( pszProj4 ) == OGRERR_NONE )
        {
            oSRS.exportToWkt( &(poDS->pszProjection) );
        }

        G_free( pszProj4 );
    }

/* -------------------------------------------------------------------- */
/*      Create band information objects.                                */
/* -------------------------------------------------------------------- */
    for( int iBand = 0; papszCells[iBand] != NULL; iBand++ )
    {
        poDS->SetBand( iBand+1, 
                       new GRASSRasterBand( poDS, iBand+1, 
                                            papszMapsets[iBand], 
                                            papszCells[iBand] ) );
    }
    
/* -------------------------------------------------------------------- */
/*      Confirm the requested access is supported.                      */
/* -------------------------------------------------------------------- */
    if( poOpenInfo->eAccess == GA_Update )
    {
        delete poDS;
        CPLError( CE_Failure, CPLE_NotSupported, 
                  "The GRASS driver does not support update access to existing"
                  " datasets.\n" );
        return NULL;
    }
    
    return poDS;
}

/************************************************************************/
/*                          GDALRegister_GRASS()                        */
/************************************************************************/

void GDALRegister_GRASS()

{
    GDALDriver	*poDriver;
    
    if (! GDAL_CHECK_VERSION("GDAL/GRASS driver"))
        return;

    if( GDALGetDriverByName( "GRASS" ) == NULL )
    {
        poDriver = new GDALDriver();
        
        poDriver->SetDescription( "GRASS" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, 
                                   "GRASS Database Rasters" );
        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, 
                                   "frmt_grass.html" );
        
        poDriver->pfnOpen = GRASSDataset::Open;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}

