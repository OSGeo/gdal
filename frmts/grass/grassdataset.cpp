/******************************************************************************
 * $Id$
 *
 * Project:  GRASS Driver
 * Purpose:  Implement GRASS raster read/write support
 * Author:   Frank Warmerdam, warmerda@home.com
 *
 ******************************************************************************
 * Copyright (c) 2000, Frank Warmerdam
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
 * Revision 1.4  2000/09/22 03:20:06  warmerda
 * added projection support, and error handling
 *
 * Revision 1.3  2000/09/20 17:03:21  warmerda
 * Added colortable support.
 *
 * Revision 1.2  2000/09/14 21:07:33  warmerda
 * modified to use new G_check_cell() function
 *
 * Revision 1.1  2000/09/11 13:31:52  warmerda
 * New
 *
 */

#include <libgrass.h>

#include "gdal_priv.h"
#include "cpl_string.h"
#include "ogr_spatialref.h"

static GDALDriver	*poGRASSDriver = NULL;

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
    friend	GRASSRasterBand;

    char	*pszProjection;

  public:
                 GRASSDataset();
                 ~GRASSDataset();

    virtual const char *GetProjectionRef(void);

    static GDALDataset *Open( GDALOpenInfo * );
};

/************************************************************************/
/* ==================================================================== */
/*                            GRASSRasterBand                           */
/* ==================================================================== */
/************************************************************************/

class GRASSRasterBand : public GDALRasterBand
{
    friend	GRASSDataset;

    int		hCell;
    int         nGRSType;

    GDALColorTable *poCT;

  public:

                   GRASSRasterBand( GRASSDataset *, int, 
                                    const char *, const char * );
    virtual        ~GRASSRasterBand();

    virtual CPLErr IReadBlock( int, int, void * );
    virtual GDALColorInterp GetColorInterpretation();
    virtual GDALColorTable *GetColorTable();
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

    if( nGRSType == CELL_TYPE && sCellInfo.format == 0 )
        this->eDataType = GDT_Byte;
    else if( nGRSType == CELL_TYPE && sCellInfo.format == 1 )
        this->eDataType = GDT_UInt16;
    else if( nGRSType == CELL_TYPE )
        this->eDataType = GDT_UInt32;
    else if( nGRSType == FCELL_TYPE )
        this->eDataType = GDT_Float32;
    else if( nGRSType == DCELL_TYPE )
        this->eDataType = GDT_Float64;
    
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
/************************************************************************/

CPLErr GRASSRasterBand::IReadBlock( int nBlockXOff, int nBlockYOff,
                                  void * pImage )

{
    if( eDataType == GDT_Float32 || eDataType == GDT_Float64 
        || eDataType == GDT_UInt32 )
        G_get_raster_row( hCell, pImage, nBlockYOff, nGRSType  );
    else
    {
        GUInt32 *panRow = (GUInt32 *) CPLMalloc(4 * nBlockXSize);
        
        G_get_raster_row( hCell, panRow, nBlockYOff, nGRSType  );
        GDALCopyWords( panRow, GDT_UInt32, 4, 
                       pImage, eDataType, GDALGetDataTypeSize(eDataType)/8,
                       nBlockXSize );

        CPLFree( panRow );
    }

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
/*                                Open()                                */
/************************************************************************/

typedef int (*GrassErrorHandler)();

GDALDataset *GRASSDataset::Open( GDALOpenInfo * poOpenInfo )

{
    static int	bDoneGISInit = FALSE;
    char	*pszMapset = NULL, *pszCell = NULL;

    if( !bDoneGISInit )
    {
        G_set_error_routine( (GrassErrorHandler) Grass2CPLErrorHook );
        G_gisinit_2( "GDAL", NULL, NULL, NULL );
    }

/* -------------------------------------------------------------------- */
/*      Check if this is a valid grass cell.                            */
/* -------------------------------------------------------------------- */
    if( !G_check_cell( poOpenInfo->pszFilename, &pszMapset, &pszCell ) )
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
    
    if( G_get_cellhd( pszCell, pszMapset, &sCellInfo ) != 0 )
    {
        /* notdef: report failure. */
        return NULL;
    }

    poDS->nRasterXSize = sCellInfo.cols;
    poDS->nRasterYSize = sCellInfo.rows;

    G_set_window( &sCellInfo );

/* -------------------------------------------------------------------- */
/*      Try to get a projection definition.                             */
/* -------------------------------------------------------------------- */
    char	*pszProj4;

    pszProj4 = G_get_cell_as_proj4( pszCell, pszMapset );
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
    poDS->SetBand( 1, new GRASSRasterBand( poDS, 1, pszMapset, pszCell ) );

    G_free( pszMapset );
    G_free( pszCell );

    return poDS;
}

/************************************************************************/
/*                          GDALRegister_GRASS()                        */
/************************************************************************/

void GDALRegister_GRASS()

{
    GDALDriver	*poDriver;

    if( poGRASSDriver == NULL )
    {
        poGRASSDriver = poDriver = new GDALDriver();
        
        poDriver->pszShortName = "GRASS";
        poDriver->pszLongName = "GRASS Database Rasters";
#ifdef notdef
        poDriver->pszHelpTopic = "frmt_various.html#GRASS";
#endif
        
        poDriver->pfnOpen = GRASSDataset::Open;

#ifdef notdef
        poDriver->pfnCreateCopy = GRASSCreateCopy;
#endif

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}

