/******************************************************************************
 * $Id$
 *
 * Project:  GRASS Driver
 * Purpose:  Implement GRASS raster read/write support
 *           This version is for GRASS 5.7+ and uses GRASS libraries
 *           directly instead of using libgrass. 
 * Author:   Frank Warmerdam <warmerdam@pobox.com>
 *           Radim Blazek <blazek@itc.it>
 *
 ******************************************************************************
 * Copyright (c) 2000 Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2007-2010, Even Rouault <even dot rouault at mines-paris dot org>
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

#include <stdlib.h>

extern "C" {
#ifdef __cplusplus
#define class _class
#endif
#include <grass/imagery.h>
#ifdef __cplusplus
#undef class
#endif
    
#include <grass/version.h>
#include <grass/gprojects.h>
#include <grass/gis.h>

#if GRASS_VERSION_MAJOR  >= 7
char *GPJ_grass_to_wkt(const struct Key_Value *,
		       const struct Key_Value *,
		       int, int);
#else
char *GPJ_grass_to_wkt(struct Key_Value *,
		       struct Key_Value *,
		       int, int);
#endif
}

#include "gdal_priv.h"
#include "cpl_string.h"
#include "ogr_spatialref.h"

#define GRASS_MAX_COLORS 100000  // what is the right value

CPL_CVSID("$Id$");

CPL_C_START
void	GDALRegister_GRASS(void);
CPL_C_END

#if GRASS_VERSION_MAJOR  >= 7
#define G_get_cellhd             Rast_get_cellhd
#define G_raster_map_type        Rast_map_type
#define G_read_fp_range          Rast_read_fp_range
#define G_get_fp_range_min_max   Rast_get_fp_range_min_max
#define G_set_c_null_value       Rast_set_c_null_value
#define G_set_f_null_value       Rast_set_f_null_value
#define G_set_d_null_value       Rast_set_d_null_value
#define G_open_cell_old          Rast_open_old
#define G_copy                   memcpy
#define G_read_colors            Rast_read_colors
#define G_get_color_range        Rast_get_c_color_range
#define G_colors_count           Rast_colors_count
#define G_get_f_color_rule       Rast_get_fp_color_rule
#define G_free_colors            Rast_free_colors
#define G_close_cell             Rast_close
#define G_allocate_c_raster_buf  Rast_allocate_c_buf
#define G_get_c_raster_row       Rast_get_c_row
#define G_is_c_null_value        Rast_is_c_null_value
#define G_get_f_raster_row       Rast_get_f_row
#define G_get_d_raster_row       Rast_get_d_row
#define G_allocate_f_raster_buf  Rast_allocate_f_buf
#define G_allocate_d_raster_buf  Rast_allocate_d_buf
#define G__setenv                G_setenv_nogisrc
#endif

/************************************************************************/
/*                         Grass2CPLErrorHook()                         */
/************************************************************************/

int Grass2CPLErrorHook( char * pszMessage, int bFatal )

{
    if( !bFatal )
        //CPLDebug( "GRASS", "%s", pszMessage );
        CPLError( CE_Warning, CPLE_AppDefined, "GRASS warning: %s", pszMessage );
    else
        CPLError( CE_Warning, CPLE_AppDefined, "GRASS fatal error: %s", pszMessage );

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

    char	*pszGisdbase;  
    char	*pszLocation;  /* LOCATION_NAME */
    char	*pszElement;   /* cellhd or group */

    struct Cell_head sCellInfo; /* raster region */ 
    
    char	*pszProjection;

    double	adfGeoTransform[6];

  public:
                 GRASSDataset();
                 ~GRASSDataset();

    virtual const char *GetProjectionRef(void);
    virtual CPLErr GetGeoTransform( double * );

    static GDALDataset *Open( GDALOpenInfo * );

  private:
    static bool SplitPath ( char *, char **, char **, char **, char **, char ** );
};

/************************************************************************/
/* ==================================================================== */
/*                            GRASSRasterBand                           */
/* ==================================================================== */
/************************************************************************/

class GRASSRasterBand : public GDALRasterBand
{
    friend class GRASSDataset;

    char        *pszCellName;
    char        *pszMapset;
    int		hCell;
    int         nGRSType; // GRASS raster type: CELL_TYPE, FCELL_TYPE, DCELL_TYPE
    bool        nativeNulls; // use GRASS native NULL values

    struct Colors sGrassColors;
    GDALColorTable *poCT;

    struct Cell_head sOpenWindow; /* the region when the raster was opened */ 

    int		bHaveMinMax;
    double	dfCellMin;
    double	dfCellMax;

    double	dfNoData;

    bool        valid;

  public:

                   GRASSRasterBand( GRASSDataset *, int, 
                                    const char *, const char * );
    virtual        ~GRASSRasterBand();

    virtual CPLErr IReadBlock( int, int, void * );
    virtual CPLErr IRasterIO ( GDALRWFlag, int, int, int, int, void *, int, int, GDALDataType,
                               GSpacing nPixelSpace,
                               GSpacing nLineSpace,
                               GDALRasterIOExtraArg* psExtraArg);
    virtual GDALColorInterp GetColorInterpretation();
    virtual GDALColorTable *GetColorTable();
    virtual double GetMinimum( int *pbSuccess = NULL );
    virtual double GetMaximum( int *pbSuccess = NULL );
    virtual double GetNoDataValue( int *pbSuccess = NULL );

  private:
    CPLErr ResetReading( struct Cell_head * );
    
};


/************************************************************************/
/*                          GRASSRasterBand()                           */
/************************************************************************/

GRASSRasterBand::GRASSRasterBand( GRASSDataset *poDS, int nBand,
                                  const char * pszMapset,
                                  const char * pszCellName )

{
    struct Cell_head	sCellInfo;

    // Note: GISDBASE, LOCATION_NAME ans MAPSET was set in GRASSDataset::Open

    this->poDS = poDS;
    this->nBand = nBand;
    this->valid = false;

    this->pszCellName = G_store ( (char *) pszCellName );
    this->pszMapset = G_store ( (char *) pszMapset );

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
    // Negative values are also (?) stored as 4 bytes (format = 3) 
    //       => raster with format < 3 has only positive values

    // GRASS modules usually do not waste space and only the format necessary to keep 
    // full raster values range is used -> no checks if shorter type could be used
    
    if( nGRSType == CELL_TYPE ) {
	if ( sCellInfo.format == 0 ) {  // 1 byte / cell -> possible range 0,255
	    if ( bHaveMinMax && dfCellMin > 0 ) {
                this->eDataType = GDT_Byte;
		dfNoData = 0.0;
	    } else if ( bHaveMinMax && dfCellMax < 255 ) {
                this->eDataType = GDT_Byte;
		dfNoData = 255.0;
	    } else { // maximum is not known or full range is used
		this->eDataType = GDT_UInt16;
		dfNoData = 256.0;
	    }
	    nativeNulls = false;
	} else if ( sCellInfo.format == 1 ) {  // 2 bytes / cell -> possible range 0,65535
	    if ( bHaveMinMax && dfCellMin > 0 ) {
		this->eDataType = GDT_UInt16;
		dfNoData = 0.0;
	    } else if ( bHaveMinMax && dfCellMax < 65535 ) {
                this->eDataType = GDT_UInt16;
		dfNoData = 65535;
	    } else { // maximum is not known or full range is used
		CELL cval;
		this->eDataType = GDT_Int32; 
		G_set_c_null_value ( &cval, 1);
		dfNoData = (double) cval;
		nativeNulls = true;
	    }
	    nativeNulls = false;
	} else {  // 3-4 bytes 
	    CELL cval;
	    this->eDataType = GDT_Int32;
	    G_set_c_null_value ( &cval, 1);
	    dfNoData = (double) cval;
	    nativeNulls = true;
	}
    } 
    else if( nGRSType == FCELL_TYPE ) {
	FCELL fval;
        this->eDataType = GDT_Float32;
	G_set_f_null_value ( &fval, 1);
	dfNoData = (double) fval;
	nativeNulls = true;
    }
    else if( nGRSType == DCELL_TYPE )
    {
	DCELL dval;
        this->eDataType = GDT_Float64;
	G_set_d_null_value ( &dval, 1);
	dfNoData = (double) dval;
	nativeNulls = true;
    }

    nBlockXSize = poDS->nRasterXSize;;
    nBlockYSize = 1;

    G_set_window( &(((GRASSDataset *)poDS)->sCellInfo) );
    if ( (hCell = G_open_cell_old((char *) pszCellName, (char *) pszMapset)) < 0 ) {
	CPLError( CE_Warning, CPLE_AppDefined, "GRASS: Cannot open raster '%s'", pszCellName );
	return;
    }
    G_copy((void *) &sOpenWindow, (void *) &(((GRASSDataset *)poDS)->sCellInfo), sizeof(struct Cell_head));

/* -------------------------------------------------------------------- */
/*      Do we have a color table?                                       */
/* -------------------------------------------------------------------- */
    poCT = NULL;
    if( G_read_colors( (char *) pszCellName, (char *) pszMapset, &sGrassColors ) == 1 )
    {
	int maxcolor; 
	CELL min, max;

	G_get_color_range ( &min, &max, &sGrassColors);

        if ( bHaveMinMax ) {
	    if ( max < dfCellMax ) {
	       maxcolor = max;
            } else {
	       maxcolor = (int) ceil ( dfCellMax );
	    }
	    if ( maxcolor > GRASS_MAX_COLORS ) { 
		maxcolor = GRASS_MAX_COLORS;
                CPLDebug( "GRASS", "Too many values, color table cut to %d entries.", maxcolor );
	    }
	} else {
	    if ( max < GRASS_MAX_COLORS ) {
	       maxcolor = max;
            } else {
	       maxcolor = GRASS_MAX_COLORS;
               CPLDebug( "GRASS", "Too many values, color table set to %d entries.", maxcolor );
	    }
        }
	    
        poCT = new GDALColorTable();
        for( int iColor = 0; iColor <= maxcolor; iColor++ )
        {
            int	nRed, nGreen, nBlue;
            GDALColorEntry    sColor;

#if GRASS_VERSION_MAJOR  >= 7
            if( Rast_get_c_color( &iColor, &nRed, &nGreen, &nBlue, &sGrassColors ) )
#else
            if( G_get_color( iColor, &nRed, &nGreen, &nBlue, &sGrassColors ) )
#endif
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
	    
	/* Create metadata enries for color table rules */
	char key[200], value[200];
	int rcount = G_colors_count ( &sGrassColors );

	sprintf ( value, "%d", rcount );
	this->SetMetadataItem( "COLOR_TABLE_RULES_COUNT", value );

	/* Add the rules in reverse order */
	for ( int i = rcount-1; i >= 0; i-- ) {
	    DCELL val1, val2;
	    unsigned char r1, g1, b1, r2, g2, b2;

	     G_get_f_color_rule ( &val1, &r1, &g1, &b1, &val2, &r2, &g2, &b2, &sGrassColors, i );
		

	     sprintf ( key, "COLOR_TABLE_RULE_RGB_%d", rcount-i-1 );
	     sprintf ( value, "%e %e %d %d %d %d %d %d", val1, val2, r1, g1, b1, r2, g2, b2 );
	     this->SetMetadataItem( key, value );
	}
    } else {
	this->SetMetadataItem( "COLOR_TABLE_RULES_COUNT", "0" );
    }
    
    this->valid = true;
}

/************************************************************************/
/*                          ~GRASSRasterBand()                          */
/************************************************************************/

GRASSRasterBand::~GRASSRasterBand()
{
    if( poCT != NULL ) {
        G_free_colors( &sGrassColors );
        delete poCT;
    }

    if( hCell >= 0 )
        G_close_cell( hCell );
    
    if ( pszCellName )
        G_free ( pszCellName );

    if ( pszMapset )
        G_free ( pszMapset );
}

/************************************************************************/
/*                             ResetReading                             */
/*                                                                      */
/* Reset current window and reopen cell if the window has changed,      */
/* reset GRASS variables                                                */
/*                                                                      */
/* Returns CE_Failure if fails, otherwise CE_None                       */
/************************************************************************/
CPLErr GRASSRasterBand::ResetReading ( struct Cell_head *sNewWindow )
{

    /* Check if the window has changed */
    if ( sNewWindow->north  != sOpenWindow.north  || sNewWindow->south  != sOpenWindow.south ||
	 sNewWindow->east   != sOpenWindow.east   || sNewWindow->west   != sOpenWindow.west ||
	 sNewWindow->ew_res != sOpenWindow.ew_res || sNewWindow->ns_res != sOpenWindow.ns_res ||
	 sNewWindow->rows   != sOpenWindow.rows   || sNewWindow->cols   != sOpenWindow.cols )
    {
	if( hCell >= 0 ) {
            G_close_cell( hCell );
	    hCell = -1;
	}

	/* Set window */
	G_set_window( sNewWindow );

	/* Open raster */
	G__setenv( "GISDBASE", ((GRASSDataset *)poDS)->pszGisdbase );
	G__setenv( "LOCATION_NAME", ((GRASSDataset *)poDS)->pszLocation );
	G__setenv( "MAPSET", pszMapset); 
	G_reset_mapsets();
	G_add_mapset_to_search_path ( pszMapset );
	
	if ( (hCell = G_open_cell_old( pszCellName, pszMapset)) < 0 ) {
	    CPLError( CE_Warning, CPLE_AppDefined, "GRASS: Cannot open raster '%s'", pszCellName );
            this->valid = false;
	    return CE_Failure;
	}

	G_copy((void *) &sOpenWindow, (void *) sNewWindow, sizeof(struct Cell_head));
	
    }
    else
    {
        /* The windows are identical, check current window */
        struct Cell_head sCurrentWindow;

        G_get_window ( &sCurrentWindow );

        if ( sNewWindow->north  != sCurrentWindow.north  || sNewWindow->south  != sCurrentWindow.south ||
             sNewWindow->east   != sCurrentWindow.east   || sNewWindow->west   != sCurrentWindow.west ||
             sNewWindow->ew_res != sCurrentWindow.ew_res || sNewWindow->ns_res != sCurrentWindow.ns_res ||
             sNewWindow->rows   != sCurrentWindow.rows   || sNewWindow->cols   != sCurrentWindow.cols
             )
        {
            /* Reset window */
            G_set_window( sNewWindow );
        }
    }


    return CE_None;
}

/************************************************************************/
/*                             IReadBlock()                             */
/*                                                                      */
/************************************************************************/

CPLErr GRASSRasterBand::IReadBlock( int nBlockXOff, int nBlockYOff, void * pImage )

{
    if ( ! this->valid ) return CE_Failure;

    // Reset window because IRasterIO could be previosly called
    if ( ResetReading ( &(((GRASSDataset *)poDS)->sCellInfo) ) != CE_None ) {
       return CE_Failure;
    }       
    
    if ( eDataType == GDT_Byte || eDataType == GDT_UInt16 ) {
        CELL  *cbuf;

	cbuf = G_allocate_c_raster_buf();
	G_get_c_raster_row ( hCell, cbuf, nBlockYOff );	

	/* Reset NULLs */
	for ( int col = 0; col < nBlockXSize; col++ ) {
	    if ( G_is_c_null_value(&(cbuf[col])) )
		cbuf[col] = (CELL) dfNoData;
	}

	GDALCopyWords ( (void *) cbuf, GDT_Int32, sizeof(CELL), 
	                pImage, eDataType, GDALGetDataTypeSize(eDataType)/8,
			nBlockXSize );    

	G_free ( cbuf );

    } else if ( eDataType == GDT_Int32 ) {
	G_get_c_raster_row ( hCell, (CELL *) pImage, nBlockYOff );
    } else if ( eDataType == GDT_Float32 ) {
	G_get_f_raster_row ( hCell, (FCELL *) pImage, nBlockYOff );
    } else if ( eDataType == GDT_Float64 ) {
	G_get_d_raster_row ( hCell, (DCELL *) pImage, nBlockYOff );
    }
	
    return CE_None;
}

/************************************************************************/
/*                             IRasterIO()                              */
/*                                                                      */
/************************************************************************/

CPLErr GRASSRasterBand::IRasterIO ( GDALRWFlag eRWFlag,
	                           int nXOff, int nYOff, int nXSize, int nYSize,
				   void * pData, int nBufXSize, int nBufYSize,
				   GDALDataType eBufType,
				   GSpacing nPixelSpace,
                   GSpacing nLineSpace,
                   GDALRasterIOExtraArg* psExtraArg )
{
    /* GRASS library does that, we have only calculate and reset the region in map units
     * and if the region has changed, reopen the raster */
    
    /* Calculate the region */
    struct Cell_head sWindow;
    struct Cell_head *psDsWindow;
    
    if ( ! this->valid ) return CE_Failure;

    psDsWindow = &(((GRASSDataset *)poDS)->sCellInfo);
    
    sWindow.north = psDsWindow->north - nYOff * psDsWindow->ns_res; 
    sWindow.south = sWindow.north - nYSize * psDsWindow->ns_res; 
    sWindow.west = psDsWindow->west + nXOff * psDsWindow->ew_res; 
    sWindow.east = sWindow.west + nXSize * psDsWindow->ew_res; 
    sWindow.proj = psDsWindow->proj;
    sWindow.zone = psDsWindow->zone;

    sWindow.cols = nBufXSize;
    sWindow.rows = nBufYSize;
     
    /* Reset resolution */
    G_adjust_Cell_head ( &sWindow, 1, 1);

    if ( ResetReading ( &sWindow ) != CE_None )
    {
        return CE_Failure;
    }
    
    /* Read Data */
    CELL  *cbuf = NULL;
    FCELL *fbuf = NULL;
    DCELL *dbuf = NULL;
    bool  direct = false;

    /* Reset space if default (0) */
    if ( nPixelSpace == 0 )
	nPixelSpace = GDALGetDataTypeSize ( eBufType ) / 8;

    if ( nLineSpace == 0 )
	nLineSpace = nBufXSize * nPixelSpace;

    if ( nGRSType == CELL_TYPE && ( !nativeNulls || eBufType != GDT_Int32 || sizeof(CELL) != 4 ||
		                    nPixelSpace != sizeof(CELL) )  ) 
    {
	cbuf = G_allocate_c_raster_buf();
    } else if( nGRSType == FCELL_TYPE && ( eBufType != GDT_Float32 || nPixelSpace != sizeof(FCELL) ) ) {
	fbuf = G_allocate_f_raster_buf();
    } else if( nGRSType == DCELL_TYPE && ( eBufType != GDT_Float64 || nPixelSpace != sizeof(DCELL) ) ) {
	dbuf = G_allocate_d_raster_buf();
    } else {
	direct = true;
    }

    for ( int row = 0; row < nBufYSize; row++ ) {
        char *pnt = (char *)pData + row * nLineSpace;
	
	if ( nGRSType == CELL_TYPE ) {
	    if ( direct ) {
		G_get_c_raster_row ( hCell, (CELL *) pnt, row );
	    } else {
		G_get_c_raster_row ( hCell, cbuf, row );
		
		/* Reset NULLs */
		for ( int col = 0; col < nBufXSize; col++ ) {
		    if ( G_is_c_null_value(&(cbuf[col])) ) 
			cbuf[col] = (CELL) dfNoData;
		}

		GDALCopyWords ( (void *) cbuf, GDT_Int32, sizeof(CELL), 
			        (void *)  pnt,  eBufType, nPixelSpace,
				nBufXSize ); 
	    }
	} else if( nGRSType == FCELL_TYPE ) {
	    if ( direct ) {
		G_get_f_raster_row ( hCell, (FCELL *) pnt, row );
	    } else {
		G_get_f_raster_row ( hCell, fbuf, row );
		
		GDALCopyWords ( (void *) fbuf, GDT_Float32, sizeof(FCELL), 
			        (void *)  pnt,  eBufType, nPixelSpace,
				nBufXSize ); 
	    }
	} else if( nGRSType == DCELL_TYPE ) {
	    if ( direct ) {
		G_get_d_raster_row ( hCell, (DCELL *) pnt, row );
	    } else {
		G_get_d_raster_row ( hCell, dbuf, row );
		
		GDALCopyWords ( (void *) dbuf, GDT_Float64, sizeof(DCELL), 
			        (void *)  pnt,  eBufType, nPixelSpace,
				nBufXSize ); 
	    }
	}
    }

    if ( cbuf ) G_free ( cbuf );
    if ( fbuf ) G_free ( fbuf );
    if ( dbuf ) G_free ( dbuf );
    
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
    
    if ( pszGisdbase )
	G_free ( pszGisdbase );
    
    if ( pszLocation )
        G_free ( pszLocation );
    
    if ( pszElement )
	G_free ( pszElement );

    G_free( pszProjection );
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
/*                            SplitPath()                               */
/* Split full path to cell or group to:                                 */
/*     gisdbase, location, mapset, element, name                        */
/* New string are allocated and should be freed when no longer needed.  */
/*                                                                      */
/* Returns: true - OK                                                   */
/*          false - failed                                              */
/************************************************************************/
bool GRASSDataset::SplitPath( char *path, char **gisdbase, char **location, 
	                      char **mapset, char **element, char **name )
{
    char *p, *ptr[5], *tmp;
    int  i = 0;
    
    *gisdbase = *location = *mapset = *element = *name = NULL;
    
    if ( !path || strlen(path) == 0 ) 
	return false;

    tmp = G_store ( path );

    while ( (p = strrchr(tmp,'/')) != NULL  && i < 4 ) {
	*p = '\0';
	
	if ( strlen(p+1) == 0 ) /* repeated '/' */
	    continue;

	ptr[i++] = p+1;
    }

    /* Note: empty GISDBASE == 0 is not accepted (relative path) */
    if ( i != 4 ) {
        G_free ( tmp );
	return false;
    }

    *gisdbase = G_store ( tmp );
    *location = G_store ( ptr[3] );
    *mapset   = G_store ( ptr[2] );
    *element  = G_store ( ptr[1] );
    *name     = G_store ( ptr[0] );

    G_free ( tmp );
    return true;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

#if (GRASS_VERSION_MAJOR  >= 6 && GRASS_VERSION_MINOR  >= 3) || GRASS_VERSION_MAJOR  >= 7
typedef int (*GrassErrorHandler)(const char *, int);
#else
typedef int (*GrassErrorHandler)(char *, int);
#endif

GDALDataset *GRASSDataset::Open( GDALOpenInfo * poOpenInfo )

{
    char	*pszGisdb = NULL, *pszLoc = NULL;
    char	*pszMapset = NULL, *pszElem = NULL, *pszName = NULL;
    char        **papszCells = NULL;
    char        **papszMapsets = NULL;

/* -------------------------------------------------------------------- */
/*      Does this even look like a grass file path?                     */
/* -------------------------------------------------------------------- */
    if( strstr(poOpenInfo->pszFilename,"/cellhd/") == NULL
        && strstr(poOpenInfo->pszFilename,"/group/") == NULL )
        return NULL;

    /* Always init, if no rasters are opened G_no_gisinit resets the projection and 
     * rasters in different projection may be then opened */

    // Don't use GISRC file and read/write GRASS variables (from location G_VAR_GISRC) to memory only.
    G_set_gisrc_mode ( G_GISRC_MODE_MEMORY );

    // Init GRASS libraries (required)
    G_no_gisinit();  // Doesn't check write permissions for mapset compare to G_gisinit

    // Set error function
    G_set_error_routine ( (GrassErrorHandler) Grass2CPLErrorHook );
    

    // GISBASE is path to the directory where GRASS is installed,
    if ( !getenv( "GISBASE" ) ) {
        static char* gisbaseEnv = NULL;
        const char *gisbase = GRASS_GISBASE;
        CPLError( CE_Warning, CPLE_AppDefined, "GRASS warning: GISBASE "
                "enviroment variable was not set, using:\n%s", gisbase );
        char buf[2000];
        snprintf ( buf, sizeof(buf), "GISBASE=%s", gisbase );
        buf[sizeof(buf)-1] = '\0';

        CPLFree(gisbaseEnv);
        gisbaseEnv = CPLStrdup ( buf );
        putenv( gisbaseEnv );
    }

    if ( !SplitPath( poOpenInfo->pszFilename, &pszGisdb, &pszLoc, &pszMapset,
                     &pszElem, &pszName) ) {
	return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Check element name                                              */
/* -------------------------------------------------------------------- */
    if ( strcmp(pszElem,"cellhd") != 0 && strcmp(pszElem,"group") != 0 ) { 
	G_free(pszGisdb); 
        G_free(pszLoc); 
        G_free(pszMapset); 
        G_free(pszElem); 
        G_free(pszName);
	return NULL;
    }
    
/* -------------------------------------------------------------------- */
/*      Set GRASS variables                                             */
/* -------------------------------------------------------------------- */

    G__setenv( "GISDBASE", pszGisdb );
    G__setenv( "LOCATION_NAME", pszLoc );
    G__setenv( "MAPSET", pszMapset); // group is searched only in current mapset 
    G_reset_mapsets();
    G_add_mapset_to_search_path ( pszMapset );

/* -------------------------------------------------------------------- */
/*      Check if this is a valid grass cell.                            */
/* -------------------------------------------------------------------- */
    if ( strcmp(pszElem,"cellhd") == 0 ) {
	
        if ( G_find_file2("cell", pszName, pszMapset) == NULL ) {
	    G_free(pszGisdb); G_free(pszLoc); G_free(pszMapset); G_free(pszElem); G_free(pszName);
	    return NULL;
	}

	papszMapsets = CSLAddString( papszMapsets, pszMapset );
	papszCells = CSLAddString( papszCells, pszName );
    }
/* -------------------------------------------------------------------- */
/*      Check if this is a valid GRASS imagery group.                   */
/* -------------------------------------------------------------------- */
    else {
        struct Ref ref;

        I_init_group_ref( &ref );
        if ( I_get_group_ref( pszName, &ref ) == 0 ) {
	    G_free(pszGisdb); G_free(pszLoc); G_free(pszMapset); G_free(pszElem); G_free(pszName);
	    return NULL;
	}
        
        for( int iRef = 0; iRef < ref.nfiles; iRef++ ) 
	{
            papszCells = CSLAddString( papszCells, ref.file[iRef].name );
            papszMapsets = CSLAddString( papszMapsets, ref.file[iRef].mapset );
            G_add_mapset_to_search_path ( ref.file[iRef].mapset );
        }

        I_free_group_ref( &ref );
    }
    
    G_free( pszMapset );
    G_free( pszName );

/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */
    GRASSDataset 	*poDS;

    poDS = new GRASSDataset();

    /* notdef: should only allow read access to an existing cell, right? */
    poDS->eAccess = poOpenInfo->eAccess;

    poDS->pszGisdbase = pszGisdb;
    poDS->pszLocation = pszLoc;
    poDS->pszElement = pszElem;
    
/* -------------------------------------------------------------------- */
/*      Capture some information from the file that is of interest.     */
/* -------------------------------------------------------------------- */

#if GRASS_VERSION_MAJOR  >= 7
    Rast_get_cellhd( papszCells[0], papszMapsets[0], &(poDS->sCellInfo) );
#else
    if( G_get_cellhd( papszCells[0], papszMapsets[0], &(poDS->sCellInfo) ) != 0 ) {
        CPLError( CE_Warning, CPLE_AppDefined, "GRASS: Cannot open raster header");
        delete poDS;
        return NULL;
    }
#endif

    poDS->nRasterXSize = poDS->sCellInfo.cols;
    poDS->nRasterYSize = poDS->sCellInfo.rows;

    poDS->adfGeoTransform[0] = poDS->sCellInfo.west;
    poDS->adfGeoTransform[1] = poDS->sCellInfo.ew_res;
    poDS->adfGeoTransform[2] = 0.0;
    poDS->adfGeoTransform[3] = poDS->sCellInfo.north;
    poDS->adfGeoTransform[4] = 0.0;
    poDS->adfGeoTransform[5] = -1 * poDS->sCellInfo.ns_res;
    
/* -------------------------------------------------------------------- */
/*      Try to get a projection definition.                             */
/* -------------------------------------------------------------------- */
    struct Key_Value *projinfo, *projunits;

    projinfo = G_get_projinfo();
    projunits = G_get_projunits();
    poDS->pszProjection = GPJ_grass_to_wkt ( projinfo, projunits, 0, 0);
    if (projinfo) G_free_key_value(projinfo);
    if (projunits) G_free_key_value(projunits);

/* -------------------------------------------------------------------- */
/*      Create band information objects.                                */
/* -------------------------------------------------------------------- */
    for( int iBand = 0; papszCells[iBand] != NULL; iBand++ )
    {
	GRASSRasterBand *rb = new GRASSRasterBand( poDS, iBand+1, papszMapsets[iBand], 
                                                                  papszCells[iBand] );

	if ( !rb->valid ) {
	    CPLError( CE_Warning, CPLE_AppDefined, "GRASS: Cannot open raster band %d", iBand);
	    delete rb;
	    delete poDS;
	    return NULL;
	}

        poDS->SetBand( iBand+1, rb );
    }

    CSLDestroy(papszCells);
    CSLDestroy(papszMapsets);
    
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
    
    if (! GDAL_CHECK_VERSION("GDAL/GRASS57 driver"))
        return;

    if( GDALGetDriverByName( "GRASS" ) == NULL )
    {
        poDriver = new GDALDriver();
        
        poDriver->SetDescription( "GRASS" );
        poDriver->SetMetadataItem( GDAL_DCAP_RASTER, "YES" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, 
                                   "GRASS Database Rasters (5.7+)" );
        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, 
                                   "frmt_grass.html" );
        
        poDriver->pfnOpen = GRASSDataset::Open;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}

