/******************************************************************************
 * $Id$
 *
 * Project:  Arc/Info Binary Grid Driver
 * Purpose:  Implements GDAL interface to ArcView GRIDIO Library.
 * Author:   Frank Warmerdam, warmerda@home.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Frank Warmerdam
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
 *****************************************************************************
 *
 * $Log$
 * Revision 1.11  2000/03/27 13:38:57  warmerda
 * Fixed SetBand() call in Create().
 *
 * Revision 1.10  2000/03/23 23:03:08  warmerda
 * Fixed Create() method to use SetBand().
 *
 * Revision 1.9  2000/03/22 23:52:59  pgs
 * use SetBand dork
 *
 * Revision 1.8  2000/01/31 16:39:37  warmerda
 * added pfnDelete support, and removed delete hack from Create method
 *
 * Revision 1.7  2000/01/31 04:13:10  warmerda
 * Added logic in Create() to blow away old grid of same name if present.
 *
 * Revision 1.6  2000/01/31 04:08:23  warmerda
 * Fixed georeference writing.
 *
 * Revision 1.5  2000/01/31 03:55:49  warmerda
 * Added support for writing out georeferencing to newly created datsets.
 *
 * Revision 1.4  2000/01/13 19:10:53  warmerda
 * Call AccessWindowSet() _before_ creating a new layer.
 *
 * Revision 1.3  2000/01/13 17:34:18  warmerda
 * Fixed transposition of nXSize and nYSize.
 *
 * Revision 1.2  2000/01/10 17:36:07  warmerda
 * avoid error message on first CPLGetSymbol()
 *
 * Revision 1.1  2000/01/07 18:57:36  warmerda
 * New
 *
 */

#include "gdal_priv.h"

#define	READONLY	1
#define	READWRITE	2
#define WRITEONLY       3
#define	ROWIO		1
#define	CELLINT		1		/* 32 bit signed integers */
#define	CELLFLOAT	2		/* 32 bit floating point numbers*/

CPL_C_START
void	GDALRegister_AIGrid2(void);

static int      nGridIOSetupCalled = FALSE;
static int      (*pfnGridIOSetup)(void) = NULL;
static int      (*pfnGridIOExit)(void) = NULL;
static int      (*pfnCellLayerOpen)(char *, int, int, int *, double *) = NULL;
static int      (*pfnDescribeGridDbl)(char *, double *, int *, double *,
                                      double *, int *, int *, int * ) = NULL;
static int      (*pfnAccessWindowSet)(double *, double, double * ) = NULL;
static int      (*pfnGetWindowRowFloat)(int, int, float *) = NULL;
static int      (*pfnPutWindowRow)(int, int, float *) = NULL;
static int      (*pfnCellLayerClose)(int) = NULL;
static int      (*pfnCellLayerCreate)(char *, int, int, int, double, 
                                      double*) = NULL;
static int      (*pfnGridDelete)(char *) = NULL;
CPL_C_END

/************************************************************************/
/*                        LoadGridIOFunctions()                         */
/************************************************************************/

static int LoadGridIOFunctions()

{
    static int      bInitialized = FALSE;
    
    if( bInitialized )
        return pfnGridIOSetup != NULL;

    bInitialized = TRUE;

    CPLPushErrorHandler( CPLQuietErrorHandler );
    pfnGridIOSetup = (int (*)(void)) 
        CPLGetSymbol( "avgridio.dll", "GridIOSetup" );
    CPLPopErrorHandler();

    if( pfnGridIOSetup == NULL )
        return FALSE;

    pfnGridIOExit = (int (*)(void)) 
        CPLGetSymbol( "avgridio.dll", "GridIOExit" );

    pfnCellLayerOpen = (int (*)(char *, int, int, int*, double*))
        CPLGetSymbol( "avgridio.dll", "CellLayerOpen" );
    pfnCellLayerCreate = (int (*)(char *, int, int, int, double, double*))
        CPLGetSymbol( "avgridio.dll", "CellLayerCreate" );
    pfnDescribeGridDbl = 
        (int (*)(char*,double*,int*,double*,double*,int*,int*,int*))
        CPLGetSymbol( "avgridio.dll", "DescribeGridDbl" );
    pfnAccessWindowSet = (int (*)(double*,double,double*))
        CPLGetSymbol( "avgridio.dll", "AccessWindowSet" );
    pfnGetWindowRowFloat = (int (*)(int,int,float*))
        CPLGetSymbol( "avgridio.dll", "GetWindowRowFloat" );
    pfnPutWindowRow = (int (*)(int,int,float*))
        CPLGetSymbol( "avgridio.dll", "PutWindowRow" );
    pfnCellLayerClose = (int (*)(int))
        CPLGetSymbol( "avgridio.dll", "CellLayerClose" );
    pfnGridDelete = (int (*)(char*))
        CPLGetSymbol( "avgridio.dll", "GridDelete" );

    if( pfnCellLayerOpen == NULL || pfnDescribeGridDbl == NULL
        || pfnAccessWindowSet == NULL || pfnGetWindowRowFloat == NULL
        || pfnCellLayerClose == NULL || pfnGridDelete == NULL )
        pfnGridIOSetup = NULL;

    return pfnGridIOSetup != NULL;
}

/************************************************************************/
/* ==================================================================== */
/*				GIODataset				*/
/* ==================================================================== */
/************************************************************************/

class GIORasterBand;

class CPL_DLL GIODataset : public GDALDataset
{
    friend	GIORasterBand;

    char       *pszPath;

    int         bCreated;
    int         nGridChannel;
    int         nCellType;
    double      dfCellSize;
    double      adfGeoTransform[6];

    void        WriteGeoreference();

  public:
                GIODataset();
                ~GIODataset();

    static CPLErr       Delete( const char * pszGridName );
    static GDALDataset *Open( GDALOpenInfo * );
    static GDALDataset *Create( const char * pszFilename,
                                int nXSize, int nYSize, int nBands,
                                GDALDataType eType, char ** papszParmList );

    virtual CPLErr GetGeoTransform( double * );
    virtual CPLErr SetGeoTransform( double * );
};

/************************************************************************/
/* ==================================================================== */
/*                            GIORasterBand                             */
/* ==================================================================== */
/************************************************************************/

class GIORasterBand : public GDALRasterBand
{
    friend	GIODataset;

  public:

                   GIORasterBand( GIODataset *, int );
                  ~GIORasterBand();

    virtual CPLErr IReadBlock( int, int, void * );
    virtual CPLErr IWriteBlock( int, int, void * );

};

static GDALDriver	*poGIODriver = NULL;

/************************************************************************/
/*                           GIORasterBand()                            */
/************************************************************************/

GIORasterBand::GIORasterBand( GIODataset *poDS, int nBand )

{
    this->poDS = poDS;
    this->nBand = nBand;

    nBlockXSize = poDS->nRasterXSize;
    nBlockYSize = 1;

    eDataType = GDT_Float32;
}

/************************************************************************/
/*                           ~GIORasterBand()                           */
/************************************************************************/

GIORasterBand::~GIORasterBand()

{
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr GIORasterBand::IReadBlock( int nBlockXOff, int nBlockYOff,
                                  void * pImage )

{
    GIODataset	*poODS = (GIODataset *) poDS;

    if( poODS->bCreated )
        memset( pImage, sizeof(float) * poODS->nRasterXSize, 0 );
    else
        pfnGetWindowRowFloat( poODS->nGridChannel, nBlockYOff, (float*)pImage);

    return CE_None;
}

/************************************************************************/
/*                             IWriteBlock()                            */
/************************************************************************/

CPLErr GIORasterBand::IWriteBlock( int nBlockXOff, int nBlockYOff,
                                   void * pImage )

{
    GIODataset	*poODS = (GIODataset *) poDS;
    
    pfnPutWindowRow( poODS->nGridChannel, nBlockYOff, (float *) pImage);
    
    return CE_None;
}

/************************************************************************/
/* ==================================================================== */
/*                            GIODataset                               */
/* ==================================================================== */
/************************************************************************/


/************************************************************************/
/*                            GIODataset()                            */
/************************************************************************/

GIODataset::GIODataset()

{
    nGridChannel = -1;
}

/************************************************************************/
/*                           ~GIODataset()                            */
/************************************************************************/

GIODataset::~GIODataset()

{
    FlushCache();
    if( nGridChannel != -1 )
        pfnCellLayerClose( nGridChannel );

    if( bCreated )
        WriteGeoreference();

    CPLFree( pszPath );
}

/************************************************************************/
/*                         WriteGeoreference()                          */
/************************************************************************/

void GIODataset::WriteGeoreference()

{
/* -------------------------------------------------------------------- */
/*      If this is a created dataset, we will write the                 */
/*      georeferencing out directly.  We do this by hand, since         */
/*      using the ESRI API we can only do this as part of the           */
/*      creation.  First we update the bounds file.                     */
/* -------------------------------------------------------------------- */
    FILE      *fp;
    double    adfFileBounds[4];

    fp = VSIFOpen( CPLFormFilename( pszPath, "dblbnd", "adf" ), "r+b" );
    if( fp == NULL )
    {
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "Unable to open dbfbnd.adf to update georeferencing." );
        return;
    }

    adfFileBounds[0] = adfGeoTransform[0];
    adfFileBounds[1] = adfGeoTransform[3]+adfGeoTransform[5]*GetRasterYSize();
    adfFileBounds[2] = adfGeoTransform[0]+adfGeoTransform[1]*GetRasterXSize();
    adfFileBounds[3] = adfGeoTransform[3];

#ifdef CPL_LSB
    CPL_SWAPDOUBLE( adfFileBounds+0 );
    CPL_SWAPDOUBLE( adfFileBounds+1 );
    CPL_SWAPDOUBLE( adfFileBounds+2 );
    CPL_SWAPDOUBLE( adfFileBounds+3 );
#endif
    
    VSIFWrite( adfFileBounds, 1, 32, fp );
    VSIFClose( fp );

/* -------------------------------------------------------------------- */
/*      Now update the cellsize in the header file.                     */
/* -------------------------------------------------------------------- */
    double      adfCellSizes[2];

    fp = VSIFOpen( CPLFormFilename( pszPath, "hdr", "adf" ), "r+b" );
    if( fp == NULL )
    {
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "Unable to open hdr.adf to update georeferencing." );
        return;
    }
    
    adfCellSizes[0] = adfGeoTransform[1];
    adfCellSizes[1] = ABS(adfGeoTransform[5]);
                          
#ifdef CPL_LSB
    CPL_SWAPDOUBLE( adfCellSizes+0 );
    CPL_SWAPDOUBLE( adfCellSizes+1 );
#endif

    VSIFSeek( fp, 256, SEEK_SET );
    VSIFWrite( adfCellSizes, 1, 16, fp );

    VSIFClose( fp );
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *GIODataset::Open( GDALOpenInfo * poOpenInfo )

{
    int            nChannel, nReturn;
    
/* -------------------------------------------------------------------- */
/*      If the pass name ends in .adf assume a file within the          */
/*      coverage has been selected, and strip that off the coverage     */
/*      name.                                                           */
/* -------------------------------------------------------------------- */
    char            *pszCoverName;

    pszCoverName = CPLStrdup( poOpenInfo->pszFilename );
    if( EQUAL(pszCoverName+strlen(pszCoverName)-4, ".adf") )
    {
        int      i;

        for( i = strlen(pszCoverName)-1; i > 0; i-- )
        {
            if( pszCoverName[i] == '\\' || pszCoverName[i] == '/' )
            {
                pszCoverName[i] = '\0';
                break;
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Verify that the resulting name is directory path.               */
/* -------------------------------------------------------------------- */
    VSIStatBuf      sStat;

    if( VSIStat( pszCoverName, &sStat ) != 0 || !VSI_ISDIR(sStat.st_mode) )
    {
        CPLFree( pszCoverName );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Call GridIOSetup(), if not called already.                      */
/* -------------------------------------------------------------------- */
    if( !nGridIOSetupCalled )
    {
        if( pfnGridIOSetup() != 1 )
            return NULL;

        nGridIOSetupCalled = TRUE;
    }
    
/* -------------------------------------------------------------------- */
/*      Try to fetch description information for grid.                  */
/* -------------------------------------------------------------------- */
    int            nCellType, nClasses, nRecordLength;
    int            anGridSize[2];
    double         adfBox[4], adfStatistics[10], dfCellSize;

    anGridSize[0] = -1;
    anGridSize[1] = -1;
    nReturn = pfnDescribeGridDbl( pszCoverName, 
                                  &dfCellSize, anGridSize, adfBox, 
                                  adfStatistics, &nCellType, 
                                  &nClasses, &nRecordLength );

    if( nReturn < 1 && anGridSize[0] == -1 )
    {
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Open the file.                                                  */
/* -------------------------------------------------------------------- */
    nChannel = pfnCellLayerOpen( pszCoverName, 
                                 READONLY, ROWIO, &nCellType, &dfCellSize );

    if( nChannel < 0 )
        return NULL;
    
/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */
    GIODataset 	*poDS;

    poDS = new GIODataset();

    poDS->pszPath = pszCoverName;
    poDS->nGridChannel = nChannel;
    poDS->poDriver = poGIODriver;

/* -------------------------------------------------------------------- */
/*      Establish raster info.                                          */
/* -------------------------------------------------------------------- */
    poDS->nRasterXSize = anGridSize[1];
    poDS->nRasterYSize = anGridSize[0];
    poDS->nBands = 1;
    poDS->bCreated = FALSE;

    poDS->adfGeoTransform[0] = adfBox[0];
    poDS->adfGeoTransform[1] = dfCellSize;
    poDS->adfGeoTransform[2] = 0.0;
    poDS->adfGeoTransform[3] = adfBox[3];
    poDS->adfGeoTransform[4] = 0.0;
    poDS->adfGeoTransform[5] = -dfCellSize;

    poDS->nCellType = nCellType;

/* -------------------------------------------------------------------- */
/*      Set the access window.                                          */
/* -------------------------------------------------------------------- */
    double      adfAdjustedBox[4];

    pfnAccessWindowSet( adfBox, dfCellSize, adfAdjustedBox ); 

/* -------------------------------------------------------------------- */
/*      Create band information objects.                                */
/* -------------------------------------------------------------------- */
    poDS->papoBands = (GDALRasterBand **)VSICalloc(sizeof(GDALRasterBand *),
                                                   poDS->nBands);
	poDS->SetBand( 1, new GIORasterBand( poDS, 1 ));

    return( poDS );
}


/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr GIODataset::GetGeoTransform( double * padfTransform )

{
    memcpy( padfTransform, adfGeoTransform, sizeof(double)*6 );

    return( CE_None );
}

/************************************************************************/
/*                          SetGeoTransform()                           */
/************************************************************************/

CPLErr GIODataset::SetGeoTransform( double * padfTransform )

{
    if( padfTransform[2] != 0.0 || padfTransform[4] != 0.0 )
        return CE_Failure;

    memcpy( adfGeoTransform, padfTransform, sizeof(double)*6 );

    return( CE_None );
}

/************************************************************************/
/*                               Create()                               */
/************************************************************************/

GDALDataset *GIODataset::Create( const char * pszFilename,
                                 int nXSize, int nYSize, int nBands,
                                 GDALDataType eType,
                                 char ** /* papszParmList */ )

{
    double            adfBox[4];
    int               nChannel;
    
/* -------------------------------------------------------------------- */
/*      Do some rudimentary argument checking.                          */
/* -------------------------------------------------------------------- */
    if( nBands != 1 )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "AIGrid2 driver only supports one band datasets, not\n"
                  "%d bands as requested for %s.\n", 
                  nBands, pszFilename );

        return NULL;
    }

    if( eType != GDT_Float32 )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "AIGrid2 driver only supports Float32 datasets, not\n"
                  "%s as requested for %s.\n", 
                  GDALGetDataTypeName(eType), pszFilename );

        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Call GridIOSetup(), if not called already.                      */
/* -------------------------------------------------------------------- */
    if( !nGridIOSetupCalled )
    {
        if( pfnGridIOSetup() != 1 )
            return NULL;

        nGridIOSetupCalled = TRUE;
    }

/* -------------------------------------------------------------------- */
/*      Set the access window.                                          */
/* -------------------------------------------------------------------- */
    double      adfAdjustedBox[4];
    
    adfBox[0] = -0.5;
    adfBox[1] = -0.5;
    adfBox[2] = nXSize-0.5;
    adfBox[3] = nYSize-0.5;

    pfnAccessWindowSet( adfBox, 1.0, adfAdjustedBox ); 
    
/* -------------------------------------------------------------------- */
/*      Create the file.                                                */
/* -------------------------------------------------------------------- */
    nChannel = pfnCellLayerCreate( (char *) pszFilename, 
                                   WRITEONLY, ROWIO, CELLFLOAT,
                                   1.0, adfBox );

    if( nChannel < 0 )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "CellLayerCreate() failed, unable to create grid:\n%s",
                  pszFilename );
        return NULL;
    }
    
/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */
    GIODataset 	*poDS;

    poDS = new GIODataset();

    poDS->pszPath = CPLStrdup( pszFilename );
    poDS->nGridChannel = nChannel;
    poDS->poDriver = poGIODriver;
    poDS->bCreated = TRUE;

/* -------------------------------------------------------------------- */
/*      Establish raster info.                                          */
/* -------------------------------------------------------------------- */
    poDS->nRasterXSize = nXSize;
    poDS->nRasterYSize = nYSize;
    poDS->nBands = 1;

    poDS->adfGeoTransform[0] = adfBox[0];
    poDS->adfGeoTransform[1] = 1.0;
    poDS->adfGeoTransform[2] = 0.0;
    poDS->adfGeoTransform[3] = adfBox[3];
    poDS->adfGeoTransform[4] = 0.0;
    poDS->adfGeoTransform[5] = -1.0;

    poDS->nCellType = CELLFLOAT;

/* -------------------------------------------------------------------- */
/*      Create band information objects.                                */
/* -------------------------------------------------------------------- */
    poDS->SetBand( 1, new GIORasterBand( poDS, 1 ) );

    return poDS;
}

/************************************************************************/
/*                               Delete()                               */
/************************************************************************/

CPLErr GIODataset::Delete( const char * pszGridName )

{
    VSIStatBuf      sStat;

    if( !nGridIOSetupCalled )
    {
        if( pfnGridIOSetup() != 1 )
            return CE_Failure;

        nGridIOSetupCalled = TRUE;
    }
    
    if( VSIStat( pszGridName, &sStat ) != 0 || !VSI_ISDIR( sStat.st_mode ) )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "%s is not a grid directory.\n", 
                  pszGridName );

        return CE_Failure;
    }
    
    if( pfnGridDelete != NULL )
        pfnGridDelete( (char *) pszGridName );
    
    return CE_None;
}


/************************************************************************/
/*                        GDALRegister_AIGrid2()                        */
/************************************************************************/

void GDALRegister_AIGrid2()

{
    GDALDriver	*poDriver;

    if( poGIODriver == NULL && LoadGridIOFunctions() )
    {
        
        poGIODriver = poDriver = new GDALDriver();
        
        poDriver->pszShortName = "AIGrid2";
        poDriver->pszLongName = "Arc/Info Binary Grid (avgridio.dll)";
        
        poDriver->pfnOpen = GIODataset::Open;
        poDriver->pfnCreate = GIODataset::Create;
        poDriver->pfnDelete = GIODataset::Delete;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}

