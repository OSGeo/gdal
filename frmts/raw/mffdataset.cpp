/******************************************************************************
 * $Id$
 *
 * Project:  GView
 * Purpose:  Implementation of Atlantis MFF Support
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
 * Revision 1.10  2000/07/26 21:05:21  warmerda
 * added support for reading tiled MFF files
 *
 * Revision 1.9  2000/07/12 19:21:34  warmerda
 * fixed cleanup of GCPs
 *
 * Revision 1.8  2000/06/05 17:24:06  warmerda
 * added real complex support
 *
 * Revision 1.7  2000/05/18 22:06:03  warmerda
 * added gcp and metadata support
 *
 * Revision 1.6  2000/05/15 14:18:27  warmerda
 * added COMPLEX_INTERPRETATION metadata
 *
 * Revision 1.5  2000/04/21 21:59:26  warmerda
 * added overview support
 *
 * Revision 1.4  2000/03/24 20:00:46  warmerda
 * Don't require IMAGE_FILE_FORMAT.
 *
 * Revision 1.3  2000/03/13 14:34:42  warmerda
 * avoid const problem on write
 *
 * Revision 1.2  2000/03/06 21:51:32  warmerda
 * fixed docs
 *
 * Revision 1.1  2000/03/06 19:23:20  warmerda
 * New
 *
 */

#include "rawdataset.h"
#include "cpl_string.h"
#include <ctype.h>

static GDALDriver	*poMFFDriver = NULL;

CPL_C_START
void	GDALRegister_MFF(void);
CPL_C_END

/************************************************************************/
/* ==================================================================== */
/*				MFFDataset				*/
/* ==================================================================== */
/************************************************************************/

class MFFDataset : public RawDataset
{
    FILE	*fpImage;	// image data file.
    
    int         nGCPCount;
    GDAL_GCP    *pasGCPList;

    void        ScanForGCPs();

  public:
    		MFFDataset();
    	        ~MFFDataset();
    
    char	**papszHdrLines;
    
    FILE        **pafpBandFiles;
    
    virtual int    GetGCPCount();
    virtual const char *GetGCPProjection();
    virtual const GDAL_GCP *GetGCPs();

    static GDALDataset *Open( GDALOpenInfo * );
    static GDALDataset *Create( const char * pszFilename,
                                int nXSize, int nYSize, int nBands,
                                GDALDataType eType, char ** papszParmList );
};

/************************************************************************/
/* ==================================================================== */
/*                            MFFTiledBand                              */
/* ==================================================================== */
/************************************************************************/

class MFFTiledBand : public GDALRasterBand
{
    friend	MFFDataset;

    FILE        *fpRaw;
    int         bNative;

  public:

                   MFFTiledBand( MFFDataset *, int, FILE *, int, int, 
                                 GDALDataType, int );
                   ~MFFTiledBand();

    virtual CPLErr IReadBlock( int, int, void * );
};


/************************************************************************/
/*                            MFFTiledBand()                            */
/************************************************************************/

MFFTiledBand::MFFTiledBand( MFFDataset *poDS, int nBand, FILE *fp, 
                            int nTileXSize, int nTileYSize, 
                            GDALDataType eDataType, int bNative )

{
    this->poDS = poDS;
    this->nBand = nBand;

    this->eDataType = eDataType; 

    this->bNative = bNative;

    this->nBlockXSize = nTileXSize;
    this->nBlockYSize = nTileYSize;

    this->fpRaw = fp;
}

/************************************************************************/
/*                           ~MFFTiledBand()                            */
/************************************************************************/

MFFTiledBand::~MFFTiledBand()

{
    VSIFClose( fpRaw );
}


/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr MFFTiledBand::IReadBlock( int nBlockXOff, int nBlockYOff,
                                 void * pImage )

{
    long    nOffset;
    int     nTilesPerRow;
    int     nWordSize, nBlockSize;

    nTilesPerRow = (nRasterXSize + nBlockXSize - 1) / nBlockXSize;
    nWordSize = GDALGetDataTypeSize( eDataType ) / 8;
    nBlockSize = nWordSize * nBlockXSize * nBlockYSize;

    nOffset = nBlockSize * (nBlockXOff + nBlockYOff*nTilesPerRow);

    if( VSIFSeek( fpRaw, nOffset, SEEK_SET ) == -1 
        || VSIFRead( pImage, 1, nBlockSize, fpRaw ) < 1 )
    {
        CPLError( CE_Failure, CPLE_FileIO, 
                  "Read of tile %d/%d failed with fseek or fread error.", 
                  nBlockXOff, nBlockYOff );
        return CE_Failure;
    }
    
    if( !bNative && nWordSize > 1 )
    {
        if( GDALDataTypeIsComplex( eDataType ) )
        {
            GDALSwapWords( pImage, nWordSize/2, nBlockXSize*nBlockYSize, 
                           nWordSize );
            GDALSwapWords( ((GByte *) pImage)+nWordSize/2, 
                           nWordSize/2, nBlockXSize*nBlockYSize, nWordSize );
        }
        else
            GDALSwapWords( pImage, nWordSize,
                           nBlockXSize * nBlockYSize, nWordSize );
    }
    
    return CE_None;
}

/************************************************************************/
/* ==================================================================== */
/*				MFFDataset				*/
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                            MFFDataset()                             */
/************************************************************************/

MFFDataset::MFFDataset()
{
    papszHdrLines = NULL;
    pafpBandFiles = NULL;
    nGCPCount = 0;
    pasGCPList = NULL;
}

/************************************************************************/
/*                            ~MFFDataset()                            */
/************************************************************************/

MFFDataset::~MFFDataset()

{
    FlushCache();
    CSLDestroy( papszHdrLines );
    if( pafpBandFiles != NULL )
    {
        for( int i = 0; i < GetRasterCount(); i++ )
        {
            if( pafpBandFiles[i] != NULL )
                VSIFClose( pafpBandFiles[i] );
        }
        CPLFree( pafpBandFiles );
    }

    if( nGCPCount > 0 )
    {
        GDALDeinitGCPs( nGCPCount, pasGCPList );
        CPLFree( pasGCPList );
    }
}

/************************************************************************/
/*                            GetGCPCount()                             */
/************************************************************************/

int MFFDataset::GetGCPCount()

{
    return nGCPCount;
}

/************************************************************************/
/*                          GetGCPProjection()                          */
/************************************************************************/

const char *MFFDataset::GetGCPProjection()

{
    if( nGCPCount > 0 )
        return "GEOGCS[\"WGS 84\",DATUM[\"WGS_1984\",SPHEROID[\"WGS 84\",6378137,298.257223563,AUTHORITY[\"EPSG\",7030]],TOWGS84[0,0,0,0,0,0,0],AUTHORITY[\"EPSG\",6326]],PRIMEM[\"Greenwich\",0,AUTHORITY[\"EPSG\",8901]],UNIT[\"DMSH\",0.0174532925199433,AUTHORITY[\"EPSG\",9108]],AXIS[\"Lat\",NORTH],AXIS[\"Long\",EAST],AUTHORITY[\"EPSG\",4326]]";
    else
        return "";
}

/************************************************************************/
/*                               GetGCP()                               */
/************************************************************************/

const GDAL_GCP *MFFDataset::GetGCPs()

{
    return pasGCPList;
}

/************************************************************************/
/*                            ScanForGCPs()                             */
/************************************************************************/

void MFFDataset::ScanForGCPs()

{
    int     nCorner;

    nGCPCount = 0;
    pasGCPList = (GDAL_GCP *) CPLCalloc(sizeof(GDAL_GCP),5);

    for( nCorner = 0; nCorner < 5; nCorner++ )
    {
        const char * pszBase;
        double       dfRasterX, dfRasterY;
        char         szLatName[40], szLongName[40];

        if( nCorner == 0 )
        {
            dfRasterX = 0;
            dfRasterY = 0;
            pszBase = "TOP_LEFT_CORNER";
        }
        else if( nCorner == 1 )
        {
            dfRasterX = GetRasterXSize();
            dfRasterY = 0;
            pszBase = "TOP_RIGHT_CORNER";
        }
        else if( nCorner == 2 )
        {
            dfRasterX = GetRasterXSize();
            dfRasterY = GetRasterYSize();
            pszBase = "BOTTOM_RIGHT_CORNER";
        }
        else if( nCorner == 3 )
        {
            dfRasterX = 0;
            dfRasterY = GetRasterYSize();
            pszBase = "BOTTOM_LEFT_CORNER";
        }
        else if( nCorner == 4 )
        {
            dfRasterX = GetRasterXSize()/2.0;
            dfRasterY = GetRasterYSize()/2.0;
            pszBase = "CENTRE";
        }

        sprintf( szLatName, "%s_LATITUDE", pszBase );
        sprintf( szLongName, "%s_LONGITUDE", pszBase );
        
        if( CSLFetchNameValue(papszHdrLines, szLatName) != NULL
            && CSLFetchNameValue(papszHdrLines, szLongName) != NULL )
        {
            GDALInitGCPs( 1, pasGCPList + nGCPCount );
            
            CPLFree( pasGCPList[nGCPCount].pszId );

            pasGCPList[nGCPCount].pszId = CPLStrdup( pszBase );
                
            pasGCPList[nGCPCount].dfGCPX = 
                atof(CSLFetchNameValue(papszHdrLines, szLongName));
            pasGCPList[nGCPCount].dfGCPY = 
                atof(CSLFetchNameValue(papszHdrLines, szLatName));
            pasGCPList[nGCPCount].dfGCPZ = 0.0;

            pasGCPList[nGCPCount].dfGCPPixel = dfRasterX;
            pasGCPList[nGCPCount].dfGCPLine = dfRasterY;

            nGCPCount++;
        }
    }
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *MFFDataset::Open( GDALOpenInfo * poOpenInfo )

{
    int		i, bNative = TRUE;
    char        **papszHdrLines;
    
/* -------------------------------------------------------------------- */
/*      We assume the user is pointing to the header file.              */
/* -------------------------------------------------------------------- */
    if( poOpenInfo->nHeaderBytes < 17 || poOpenInfo->fp == NULL )
        return NULL;

    if( !EQUAL(CPLGetExtension(poOpenInfo->pszFilename),"hdr") )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Load the .hdr file, and compress white space out around the     */
/*      equal sign.                                                     */
/* -------------------------------------------------------------------- */
    papszHdrLines = CSLLoad( poOpenInfo->pszFilename );
    if( papszHdrLines == NULL )
        return NULL;

    for( i = 0; papszHdrLines[i] != NULL; i++ )
    {
        int       bAfterEqual = FALSE;
        int       iSrc, iDst;
        char     *pszLine = papszHdrLines[i];

        for( iSrc=0, iDst=0; pszLine[iSrc] != '\0'; iSrc++ )
        {
            if( bAfterEqual || pszLine[iSrc] != ' ' )
            {
                pszLine[iDst++] = pszLine[iSrc];
            }

            if( iDst > 0 && pszLine[iDst-1] == '=' )
                bAfterEqual = FALSE;
        }
        pszLine[iDst] = '\0';
    }

/* -------------------------------------------------------------------- */
/*      Verify it is an MFF file.                                       */
/* -------------------------------------------------------------------- */
    if( CSLFetchNameValue( papszHdrLines, "IMAGE_FILE_FORMAT" ) != NULL
        && !EQUAL(CSLFetchNameValue(papszHdrLines,"IMAGE_FILE_FORMAT"),"MFF") )
    {
        CSLDestroy( papszHdrLines );
        return NULL;
    }

    if( (CSLFetchNameValue( papszHdrLines, "IMAGE_LINES" ) == NULL 
         || CSLFetchNameValue(papszHdrLines,"LINE_SAMPLES") == NULL)
        && (CSLFetchNameValue( papszHdrLines, "no_rows" ) == NULL 
            || CSLFetchNameValue(papszHdrLines,"no_columns") == NULL) )
    {
        CSLDestroy( papszHdrLines );
        return NULL;
    }
    
/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */
    MFFDataset 	*poDS;

    poDS = new MFFDataset();

    poDS->poDriver = poMFFDriver;
    poDS->papszHdrLines = papszHdrLines;
    
/* -------------------------------------------------------------------- */
/*      Set some dataset wide information.                              */
/* -------------------------------------------------------------------- */
    if( CSLFetchNameValue(papszHdrLines,"no_rows") != NULL
        && CSLFetchNameValue(papszHdrLines,"no_columns") != NULL )
    {
        poDS->RasterInitialize( 
            atoi(CSLFetchNameValue(papszHdrLines,"no_columns")),
            atoi(CSLFetchNameValue(papszHdrLines,"no_rows")) );
    }
    else
    {
        poDS->RasterInitialize( 
            atoi(CSLFetchNameValue(papszHdrLines,"LINE_SAMPLES")),
            atoi(CSLFetchNameValue(papszHdrLines,"IMAGE_LINES")) );
    }

    if( CSLFetchNameValue( papszHdrLines, "BYTE_ORDER" ) != NULL )
    {
#ifdef CPL_MSB
        bNative = EQUAL(CSLFetchNameValue(papszHdrLines,"BYTE_ORDER"),"MSB");
#else
        bNative = EQUAL(CSLFetchNameValue(papszHdrLines,"BYTE_ORDER"),"LSB");
#endif
    }

/* -------------------------------------------------------------------- */
/*      Get some information specific to APP tiled files.               */
/* -------------------------------------------------------------------- */
    int bTiled, nTileXSize=0, nTileYSize=0;
    const char *pszRefinedType = NULL;

    pszRefinedType = CSLFetchNameValue(papszHdrLines, "type" );

    bTiled = CSLFetchNameValue(papszHdrLines,"no_rows") != NULL;
    if( bTiled )
    {
        if( CSLFetchNameValue(papszHdrLines,"tile_size_rows") )
            nTileYSize = 
                atoi(CSLFetchNameValue(papszHdrLines,"tile_size_rows"));
        if( CSLFetchNameValue(papszHdrLines,"tile_size_columns") )
            nTileXSize = 
                atoi(CSLFetchNameValue(papszHdrLines,"tile_size_columns"));
    }

/* -------------------------------------------------------------------- */
/*      Read the directory to find matching band files.                 */
/* -------------------------------------------------------------------- */
    char       **papszDirFiles;
    char       *pszTargetBase, *pszTargetPath;
    int        nRawBand;

    pszTargetPath = CPLStrdup(CPLGetPath(poOpenInfo->pszFilename));
    pszTargetBase = CPLStrdup(CPLGetBasename( poOpenInfo->pszFilename ));
    papszDirFiles = CPLReadDir( CPLGetPath( poOpenInfo->pszFilename ) );
    if( papszDirFiles == NULL )
        return NULL;

    for( nRawBand = 0; TRUE; nRawBand++ )
    {
        const char  *pszExtension;
        int          nBand;
        GDALDataType eDataType;

        /* Find the next raw band file. */
        for( i = 0; papszDirFiles[i] != NULL; i++ )
        {
            if( !EQUAL(CPLGetBasename(papszDirFiles[i]),pszTargetBase) )
                continue;

            pszExtension = CPLGetExtension(papszDirFiles[i]);
            if( isdigit(pszExtension[1])
                && atoi(pszExtension+1) == nRawBand 
                && strchr("bBcCiIjJrRxXzZ",pszExtension[0]) != NULL )
                break;
        }

        if( papszDirFiles[i] == NULL  )
            break;

        /* open the file for required level of access */
        FILE     *fpRaw;
        const char *pszRawFilename = CPLFormFilename(pszTargetPath, 
                                                     papszDirFiles[i], NULL );

        if( poOpenInfo->eAccess == GA_Update )
            fpRaw = VSIFOpen( pszRawFilename, "rb+" );
        else
            fpRaw = VSIFOpen( pszRawFilename, "rb" );
        
        if( fpRaw == NULL )
        {
            CPLError( CE_Warning, CPLE_OpenFailed, 
                      "Unable to open %s ... skipping.\n", 
                      pszRawFilename );
            continue;
        }

        pszExtension = CPLGetExtension(papszDirFiles[i]);
        if( pszRefinedType != NULL )
        {
            if( EQUAL(pszRefinedType,"C*4") )
                eDataType = GDT_CFloat32;
            else if( EQUAL(pszRefinedType,"C*8") )
                eDataType = GDT_CFloat64;
            else if( EQUAL(pszRefinedType,"R*4") )
                eDataType = GDT_Float32;
            else if( EQUAL(pszRefinedType,"R*8") )
                eDataType = GDT_Float64;
            else if( EQUAL(pszRefinedType,"I*1") )
                eDataType = GDT_Byte;
            else if( EQUAL(pszRefinedType,"I*2") )
                eDataType = GDT_Int16;
            else if( EQUAL(pszRefinedType,"I*4") )
                eDataType = GDT_Int32;
            else if( EQUAL(pszRefinedType,"U*2") )
                eDataType = GDT_UInt16;
            else if( EQUAL(pszRefinedType,"U*4") )
                eDataType = GDT_UInt32;
            else if( EQUAL(pszRefinedType,"J*1") )
                continue; /* we don't support 1 byte complex */
            else if( EQUAL(pszRefinedType,"J*2") )
                eDataType = GDT_CInt16;
            else if( EQUAL(pszRefinedType,"K*4") )
                eDataType = GDT_CInt32;
            else
                continue;
        }
        else if( EQUALN(pszExtension,"b",1) )
        {
            eDataType = GDT_Byte;
        }
        else if( EQUALN(pszExtension,"i",1) )
        {
            eDataType = GDT_UInt16;
        }
        else if( EQUALN(pszExtension,"j",1) )
        {
            eDataType = GDT_CInt16;
        }
        else if( EQUALN(pszExtension,"r",1) )
        {
            eDataType = GDT_Float32;
        }
        else if( EQUALN(pszExtension,"x",1) )
        {
            eDataType = GDT_CFloat32;
        }
        else
            continue;

        nBand = poDS->GetRasterCount() + 1;

        int nPixelOffset = GDALGetDataTypeSize(eDataType)/8;
        GDALRasterBand *poBand = NULL;
        
        if( bTiled )
        {
            poBand = 
                new MFFTiledBand( poDS, nBand, fpRaw, nTileXSize, nTileYSize,
                                  eDataType, bNative );
        }
        else
        {
            poBand = 
                new RawRasterBand( poDS, nBand, fpRaw, 0, nPixelOffset,
                                   nPixelOffset * poDS->GetRasterXSize(),
                                   eDataType, bNative );
        }

        poDS->SetBand( nBand, poBand );
    }
    
/* -------------------------------------------------------------------- */
/*      Check for overviews.                                            */
/* -------------------------------------------------------------------- */
    poDS->oOvManager.Initialize( poDS, poOpenInfo->pszFilename );

/* -------------------------------------------------------------------- */
/*      Set all information from the .hdr that isn't well know to be    */
/*      metadata.                                                       */
/* -------------------------------------------------------------------- */
    for( i = 0; papszHdrLines[i] != NULL; i++ )
    {
        const char *pszValue;
        char       *pszName;

        pszValue = CPLParseNameValue(papszHdrLines[i], &pszName);
        if( pszName == NULL || pszValue == NULL )
            continue;

        if( !EQUAL(pszName,"END") 
            && !EQUAL(pszName,"FILE_TYPE") 
            && !EQUAL(pszName,"BYTE_ORDER") 
            && !EQUAL(pszName,"no_columns") 
            && !EQUAL(pszName,"no_rows") 
            && !EQUAL(pszName,"type") 
            && !EQUAL(pszName,"tile_size_rows") 
            && !EQUAL(pszName,"tile_size_columns") 
            && !EQUAL(pszName,"IMAGE_FILE_FORMAT") 
            && !EQUAL(pszName,"IMAGE_LINES") 
            && !EQUAL(pszName,"LINE_SAMPLES") )
        {
            poDS->SetMetadataItem( pszName, pszValue );
        }

        CPLFree( pszName );
    }

/* -------------------------------------------------------------------- */
/*      Any GCPs in header file?                                        */
/* -------------------------------------------------------------------- */
    poDS->ScanForGCPs();

/* -------------------------------------------------------------------- */
/*      Cleanup                                                         */
/* -------------------------------------------------------------------- */
    CPLFree(pszTargetPath);
    CPLFree(pszTargetBase);
    CSLDestroy(papszDirFiles);

    return( poDS );
}

/************************************************************************/
/*                               Create()                               */
/************************************************************************/

GDALDataset *MFFDataset::Create( const char * pszFilenameIn,
                                 int nXSize, int nYSize, int nBands,
                                 GDALDataType eType,
                                 char ** /* papszParmList */ )

{
/* -------------------------------------------------------------------- */
/*      Verify input options.                                           */
/* -------------------------------------------------------------------- */
    if( eType != GDT_Byte && eType != GDT_Float32 && eType != GDT_UInt16 
        && eType != GDT_CInt16 && eType != GDT_CFloat32 )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
              "Attempt to create MFF file with currently unsupported\n"
              "data type (%s).\n",
              GDALGetDataTypeName(eType) );

        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Establish the base filename (path+filename, less extension).    */
/* -------------------------------------------------------------------- */
    char	*pszBaseFilename;
    int         i;

    pszBaseFilename = (char *) CPLMalloc(strlen(pszFilenameIn)+5);
    strcpy( pszBaseFilename, pszFilenameIn );
    
    for( i = strlen(pszBaseFilename)-1; i > 0; i-- )
    {
        if( pszBaseFilename[i] == '.' )
        {
            pszBaseFilename[i] = '\0';
            break;
        }

        if( pszBaseFilename[i] == '/' || pszBaseFilename[i] == '\\' )
            break;
    }
    
/* -------------------------------------------------------------------- */
/*      Create the header file.                                         */
/* -------------------------------------------------------------------- */
    FILE       *fp;
    const char *pszFilename;

    pszFilename = CPLFormFilename( NULL, pszBaseFilename, "hdr" );

    fp = VSIFOpen( pszFilename, "wt" );
    if( fp == NULL )
    {
        CPLError( CE_Failure, CPLE_OpenFailed, 
                  "Couldn't create %s.\n", pszFilename );
        return NULL;
    }

    fprintf( fp, "IMAGE_FILE_FORMAT = MFF\n" );
    fprintf( fp, "FILE_TYPE = IMAGE\n" );
    fprintf( fp, "IMAGE_LINES = %d\n", nYSize );
    fprintf( fp, "LINE_SAMPLES = %d\n", nXSize );
#ifdef CPL_MSB     
    fprintf( fp, "BYTE_ORDER = MSB\n" );
#else
    fprintf( fp, "BYTE_ORDER = LSB\n" );
#endif
    fprintf( fp, "END\n" );
    VSIFClose( fp );
   
/* -------------------------------------------------------------------- */
/*      Create the data files, but don't bother writing any data to them.*/
/* -------------------------------------------------------------------- */
    for( int iBand = 0; iBand < nBands; iBand++ )
    {
        char       szExtension[4];

        if( eType == GDT_Byte )
            sprintf( szExtension, "b%02d", iBand );
        else if( eType == GDT_UInt16 )
            sprintf( szExtension, "i%02d", iBand );
        else if( eType == GDT_Float32 )
            sprintf( szExtension, "r%02d", iBand );
        else if( eType == GDT_CInt16 )
            sprintf( szExtension, "j%02d", iBand );
        else if( eType == GDT_CFloat32 )
            sprintf( szExtension, "x%02d", iBand );

        pszFilename = CPLFormFilename( NULL, pszBaseFilename, szExtension );
        fp = VSIFOpen( pszFilename, "wb" );
        if( fp == NULL )
        {
            CPLError( CE_Failure, CPLE_OpenFailed, 
                      "Couldn't create %s.\n", pszFilename );
            return NULL;
        }

        VSIFWrite( (void *) "", 1, 1, fp );
        VSIFClose( fp );
    }

/* -------------------------------------------------------------------- */
/*      Open the dataset normally.                                      */
/* -------------------------------------------------------------------- */
    GDALDataset *poDS;

    strcat( pszBaseFilename, ".hdr" );
    poDS = (GDALDataset *) GDALOpen( pszBaseFilename, GA_Update );
    CPLFree( pszBaseFilename );
    
    return poDS;
}

/************************************************************************/
/*                         GDALRegister_MFF()                          */
/************************************************************************/

void GDALRegister_MFF()

{
    GDALDriver	*poDriver;

    if( poMFFDriver == NULL )
    {
        poMFFDriver = poDriver = new GDALDriver();
        
        poDriver->pszShortName = "MFF";
        poDriver->pszLongName = "Atlantis MFF Raster";
        
        poDriver->pfnOpen = MFFDataset::Open;
        poDriver->pfnCreate = MFFDataset::Create;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}

