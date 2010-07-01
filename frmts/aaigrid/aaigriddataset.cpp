/******************************************************************************
 * $Id$
 *
 * Project:  GDAL
 * Purpose:  Implements Arc/Info ASCII Grid Format.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2001, Frank Warmerdam (warmerdam@pobox.com)
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
#include <ctype.h>
#include <limits.h>
#include "cpl_string.h"
#include "ogr_spatialref.h"

CPL_CVSID("$Id$");

CPL_C_START
void    GDALRegister_AAIGrid(void);
CPL_C_END

static CPLString OSR_GDS( char **papszNV, const char * pszField, 
                           const char *pszDefaultValue );

/************************************************************************/
/* ==================================================================== */
/*                              AAIGDataset                             */
/* ==================================================================== */
/************************************************************************/

class AAIGRasterBand;

class CPL_DLL AAIGDataset : public GDALPamDataset
{
    friend class AAIGRasterBand;
    
    FILE        *fp;

    double      adfGeoTransform[6];
    char        **papszPrj;
    CPLString   osPrjFilename;
    char        *pszProjection;

    int         bNoDataSet;
    double      dfNoDataValue;

    unsigned char achReadBuf[256];
    GUIntBig    nBufferOffset;
    int         nOffsetInBuffer;

    char        Getc();
    GUIntBig    Tell();
    int         Seek( GUIntBig nOffset );

  public:
                AAIGDataset();
                ~AAIGDataset();

    virtual char **GetFileList(void);

    static GDALDataset *Open( GDALOpenInfo * );
    static CPLErr       Delete( const char *pszFilename );
    static CPLErr       Remove( const char *pszFilename, int bRepError );

    virtual CPLErr GetGeoTransform( double * );
    virtual const char *GetProjectionRef(void);
};

/************************************************************************/
/* ==================================================================== */
/*                            AAIGRasterBand                             */
/* ==================================================================== */
/************************************************************************/

class AAIGRasterBand : public GDALPamRasterBand
{
    friend class AAIGDataset;

    GUIntBig      *panLineOffset;

  public:

                   AAIGRasterBand( AAIGDataset *, int, GDALDataType );
    virtual       ~AAIGRasterBand();

    virtual double GetNoDataValue( int * );
    virtual CPLErr SetNoDataValue( double );
    virtual CPLErr IReadBlock( int, int, void * );
};

/************************************************************************/
/*                           AAIGRasterBand()                            */
/************************************************************************/

AAIGRasterBand::AAIGRasterBand( AAIGDataset *poDS, int nDataStart, 
                                GDALDataType eTypeIn )

{
    this->poDS = poDS;

    nBand = 1;
    eDataType = eTypeIn;

    nBlockXSize = poDS->nRasterXSize;
    nBlockYSize = 1;

    panLineOffset = 
        (GUIntBig *) VSICalloc( poDS->nRasterYSize, sizeof(GUIntBig) );
    if (panLineOffset == NULL)
    {
        CPLError(CE_Failure, CPLE_OutOfMemory,
                 "AAIGRasterBand::AAIGRasterBand : Out of memory (nRasterYSize = %d)",
                 poDS->nRasterYSize);
        return;
    }
    panLineOffset[0] = nDataStart;
}

/************************************************************************/
/*                          ~AAIGRasterBand()                           */
/************************************************************************/

AAIGRasterBand::~AAIGRasterBand()

{
    CPLFree( panLineOffset );
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr AAIGRasterBand::IReadBlock( int nBlockXOff, int nBlockYOff,
                                  void * pImage )

{
    AAIGDataset *poODS = (AAIGDataset *) poDS;
    int         iPixel;

    if( nBlockYOff < 0 || nBlockYOff > poODS->nRasterYSize - 1 
        || nBlockXOff != 0 || panLineOffset == NULL)
        return CE_Failure;

    if( panLineOffset[nBlockYOff] == 0 )
    {
        int iPrevLine;

        for( iPrevLine = 1; iPrevLine <= nBlockYOff; iPrevLine++ )
            if( panLineOffset[iPrevLine] == 0 )
                IReadBlock( nBlockXOff, iPrevLine-1, NULL );
    }

    if( panLineOffset[nBlockYOff] == 0 )
        return CE_Failure;

    
    if( poODS->Seek( panLineOffset[nBlockYOff] ) != 0 )
    {
        CPLError( CE_Failure, CPLE_FileIO,
                  "Can't seek to offset %lu in input file to read data.",
                  (long unsigned int)panLineOffset[nBlockYOff] );
        return CE_Failure;
    }

    for( iPixel = 0; iPixel < poODS->nRasterXSize; )
    {
        char szToken[500];
        char chNext;
        int  iTokenChar = 0;

        /* suck up any pre-white space. */
        do {
            chNext = poODS->Getc();
        } while( isspace( (unsigned char)chNext ) );

        while( chNext != '\0' && !isspace((unsigned char)chNext)  )
        {
            if( iTokenChar == sizeof(szToken)-2 )
            {
                CPLError( CE_Failure, CPLE_FileIO, 
                          "Token too long at scanline %d.", 
                          nBlockYOff );
                return CE_Failure;
            }

            szToken[iTokenChar++] = chNext;
            chNext = poODS->Getc();
        }

        if( chNext == '\0' &&
            (iPixel != poODS->nRasterXSize - 1 ||
            nBlockYOff != poODS->nRasterYSize - 1) )
        {
            CPLError( CE_Failure, CPLE_FileIO, 
                      "File short, can't read line %d.",
                      nBlockYOff );
            return CE_Failure;
        }

        szToken[iTokenChar] = '\0';

        if( pImage != NULL )
        {
            if( eDataType == GDT_Float32 )
                ((float *) pImage)[iPixel] = (float) CPLAtofM(szToken);
            else
                ((GInt32 *) pImage)[iPixel] = (GInt32) atoi(szToken);
        }
        
        iPixel++;
    }
    
    if( nBlockYOff < poODS->nRasterYSize - 1 )
        panLineOffset[nBlockYOff + 1] = poODS->Tell();

    return CE_None;
}

/************************************************************************/
/*                           GetNoDataValue()                           */
/************************************************************************/

double AAIGRasterBand::GetNoDataValue( int * pbSuccess )

{
    AAIGDataset *poODS = (AAIGDataset *) poDS;

    if( pbSuccess )
        *pbSuccess = poODS->bNoDataSet;

    return poODS->dfNoDataValue;
}

/************************************************************************/
/*                           SetNoDataValue()                           */
/************************************************************************/

CPLErr AAIGRasterBand::SetNoDataValue( double dfNoData )

{
    AAIGDataset *poODS = (AAIGDataset *) poDS;

    poODS->bNoDataSet = TRUE;
    poODS->dfNoDataValue = dfNoData;

    return CE_None;
}

/************************************************************************/
/* ==================================================================== */
/*                            AAIGDataset                               */
/* ==================================================================== */
/************************************************************************/


/************************************************************************/
/*                            AAIGDataset()                            */
/************************************************************************/

AAIGDataset::AAIGDataset()

{
    papszPrj = NULL;
    pszProjection = CPLStrdup("");
    fp = NULL;
    bNoDataSet = FALSE;
    dfNoDataValue = -9999.0;

    adfGeoTransform[0] = 0.0;
    adfGeoTransform[1] = 1.0;
    adfGeoTransform[2] = 0.0;
    adfGeoTransform[3] = 0.0;
    adfGeoTransform[4] = 0.0;
    adfGeoTransform[5] = 1.0;

    nOffsetInBuffer = 256;
    nBufferOffset = 0;
}

/************************************************************************/
/*                           ~AAIGDataset()                            */
/************************************************************************/

AAIGDataset::~AAIGDataset()

{
    FlushCache();

    if( fp != NULL )
        VSIFCloseL( fp );

    CPLFree( pszProjection );
    CSLDestroy( papszPrj );
}

/************************************************************************/
/*                                Tell()                                */
/************************************************************************/

GUIntBig AAIGDataset::Tell()

{
    return nBufferOffset + nOffsetInBuffer;
}

/************************************************************************/
/*                                Seek()                                */
/************************************************************************/

int AAIGDataset::Seek( GUIntBig nNewOffset )

{
    nOffsetInBuffer = sizeof(achReadBuf);
    return VSIFSeekL( fp, nNewOffset, SEEK_SET );
}

/************************************************************************/
/*                                Getc()                                */
/*                                                                      */
/*      Read a single character from the input file (efficiently we     */
/*      hope).                                                          */
/************************************************************************/

char AAIGDataset::Getc()

{
    if( nOffsetInBuffer < (int) sizeof(achReadBuf) )
        return achReadBuf[nOffsetInBuffer++];

    nBufferOffset = VSIFTellL( fp );
    int nRead = VSIFReadL( achReadBuf, 1, sizeof(achReadBuf), fp );
    unsigned int i;
    for(i=nRead;i<sizeof(achReadBuf);i++)
        achReadBuf[i] = '\0';

    nOffsetInBuffer = 0;

    return achReadBuf[nOffsetInBuffer++];
}

/************************************************************************/
/*                            GetFileList()                             */
/************************************************************************/

char **AAIGDataset::GetFileList()

{
    char **papszFileList = GDALPamDataset::GetFileList();

    if( papszPrj != NULL )
        papszFileList = CSLAddString( papszFileList, osPrjFilename );

    return papszFileList;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *AAIGDataset::Open( GDALOpenInfo * poOpenInfo )

{
    int i = 0;
    int j = 0;
    char **papszTokens = NULL;

    /* Default data type */
    GDALDataType eDataType = GDT_Int32;

/* -------------------------------------------------------------------- */
/*      Does this look like an AI grid file?                            */
/* -------------------------------------------------------------------- */
    if( poOpenInfo->nHeaderBytes < 100
        || !( EQUALN((const char *) poOpenInfo->pabyHeader,"ncols",5) ||
              EQUALN((const char *) poOpenInfo->pabyHeader,"nrows",5) ||
              EQUALN((const char *) poOpenInfo->pabyHeader,"xllcorner",9)||
              EQUALN((const char *) poOpenInfo->pabyHeader,"yllcorner",9)||
              EQUALN((const char *) poOpenInfo->pabyHeader,"xllcenter",9)||
              EQUALN((const char *) poOpenInfo->pabyHeader,"yllcenter",9)||
              EQUALN((const char *) poOpenInfo->pabyHeader,"dx",2)||
              EQUALN((const char *) poOpenInfo->pabyHeader,"dy",2)||
              EQUALN((const char *) poOpenInfo->pabyHeader,"cellsize",8)) )
        return NULL;

    papszTokens =  
        CSLTokenizeString2( (const char *) poOpenInfo->pabyHeader,
                                  " \n\r\t", 0 );
    int nTokens = CSLCount(papszTokens);

/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */
    AAIGDataset         *poDS;

    poDS = new AAIGDataset();

/* -------------------------------------------------------------------- */
/*      Parse the header.                                               */
/* -------------------------------------------------------------------- */
    double dfCellDX = 0;
    double dfCellDY = 0;

    if ( (i = CSLFindString( papszTokens, "ncols" )) < 0 ||
         i + 1 >= nTokens)
    {
        CSLDestroy( papszTokens );
        delete poDS;
        return NULL;
    }
    poDS->nRasterXSize = atoi(papszTokens[i + 1]);
    if ( (i = CSLFindString( papszTokens, "nrows" )) < 0 ||
         i + 1 >= nTokens)
    {
        CSLDestroy( papszTokens );
        delete poDS;
        return NULL;
    }
    poDS->nRasterYSize = atoi(papszTokens[i + 1]);

    if (!GDALCheckDatasetDimensions(poDS->nRasterXSize, poDS->nRasterYSize))
    {
        delete poDS;
        return NULL;
    }

    if ( (i = CSLFindString( papszTokens, "cellsize" )) < 0 )
    {
        int iDX, iDY;
        if( (iDX = CSLFindString(papszTokens,"dx")) < 0 
            || (iDY = CSLFindString(papszTokens,"dy")) < 0
            || iDX+1 >= nTokens
            || iDY+1 >= nTokens)
        {
            CSLDestroy( papszTokens );
            delete poDS;
            return NULL;
        }

        dfCellDX = CPLAtofM( papszTokens[iDX+1] );
        dfCellDY = CPLAtofM( papszTokens[iDY+1] );
    }    
    else
    {
        if (i + 1 >= nTokens)
        {
            CSLDestroy( papszTokens );
            delete poDS;
            return NULL;
        }
        dfCellDX = dfCellDY = CPLAtofM( papszTokens[i + 1] );
    }

    if ((i = CSLFindString( papszTokens, "xllcorner" )) >= 0 &&
        (j = CSLFindString( papszTokens, "yllcorner" )) >= 0 &&
        i + 1 < nTokens && j + 1 < nTokens)
    {
        poDS->adfGeoTransform[0] = CPLAtofM( papszTokens[i + 1] );
        poDS->adfGeoTransform[1] = dfCellDX;
        poDS->adfGeoTransform[2] = 0.0;
        poDS->adfGeoTransform[3] = CPLAtofM( papszTokens[j + 1] )
            + poDS->nRasterYSize * dfCellDY;
        poDS->adfGeoTransform[4] = 0.0;
        poDS->adfGeoTransform[5] = - dfCellDY;
    }
    else if ((i = CSLFindString( papszTokens, "xllcenter" )) >= 0 &&
             (j = CSLFindString( papszTokens, "yllcenter" )) >= 0  &&
             i + 1 < nTokens && j + 1 < nTokens)
    {
        poDS->SetMetadataItem( GDALMD_AREA_OR_POINT, GDALMD_AOP_POINT );

        poDS->adfGeoTransform[0] = CPLAtofM(papszTokens[i + 1]) - 0.5 * dfCellDX;
        poDS->adfGeoTransform[1] = dfCellDX;
        poDS->adfGeoTransform[2] = 0.0;
        poDS->adfGeoTransform[3] = CPLAtofM( papszTokens[j + 1] )
            - 0.5 * dfCellDY
            + poDS->nRasterYSize * dfCellDY;
        poDS->adfGeoTransform[4] = 0.0;
        poDS->adfGeoTransform[5] = - dfCellDY;
    }
    else
    {
        poDS->adfGeoTransform[0] = 0.0;
        poDS->adfGeoTransform[1] = dfCellDX;
        poDS->adfGeoTransform[2] = 0.0;
        poDS->adfGeoTransform[3] = 0.0;
        poDS->adfGeoTransform[4] = 0.0;
        poDS->adfGeoTransform[5] = - dfCellDY;
    }

    if( (i = CSLFindString( papszTokens, "NODATA_value" )) >= 0 &&
        i + 1 < nTokens)
    {
        const char* pszNoData = papszTokens[i + 1];

        poDS->bNoDataSet = TRUE;
        poDS->dfNoDataValue = CPLAtofM(pszNoData);
        if( strchr( pszNoData, '.' ) != NULL ||
            strchr( pszNoData, ',' ) != NULL ||
            INT_MIN > poDS->dfNoDataValue || poDS->dfNoDataValue > INT_MAX )
        {
            eDataType = GDT_Float32;
            poDS->dfNoDataValue = (double) (float) poDS->dfNoDataValue;
        }
    }
    
    CSLDestroy( papszTokens );

/* -------------------------------------------------------------------- */
/*      Open file with large file API.                                  */
/* -------------------------------------------------------------------- */

    poDS->fp = VSIFOpenL( poOpenInfo->pszFilename, "r" );
    if( poDS->fp == NULL )
    {
        CPLError( CE_Failure, CPLE_OpenFailed, 
                  "VSIFOpenL(%s) failed unexpectedly.", 
                  poOpenInfo->pszFilename );
        delete poDS;
        return NULL;
    } 

/* -------------------------------------------------------------------- */
/*      Find the start of real data.                                    */
/* -------------------------------------------------------------------- */
    int         nStartOfData;

    for( i = 2; TRUE ; i++ )
    {
        if( poOpenInfo->pabyHeader[i] == '\0' )
        {
            CPLError( CE_Failure, CPLE_AppDefined, 
                      "Couldn't find data values in ASCII Grid file.\n" );
            delete poDS;
            return NULL;
        }
        
        if( poOpenInfo->pabyHeader[i-1] == '\n' 
            || poOpenInfo->pabyHeader[i-2] == '\n' 
            || poOpenInfo->pabyHeader[i-1] == '\r' 
            || poOpenInfo->pabyHeader[i-2] == '\r' )
        {
            if( !isalpha(poOpenInfo->pabyHeader[i]) 
                && poOpenInfo->pabyHeader[i] != '\n' 
                && poOpenInfo->pabyHeader[i] != '\r')
            {
                nStartOfData = i;

				/* Beginning of real data found. */
                break;
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Recognize the type of data.										*/
/* -------------------------------------------------------------------- */
    CPLAssert( NULL != poDS->fp );

    if( eDataType != GDT_Float32)
    {
        /* Allocate 100K chunk + 1 extra byte for NULL character. */
        const size_t nChunkSize = 1024 * 100;
        GByte* pabyChunk = (GByte *) CPLCalloc( nChunkSize + 1, sizeof(GByte) );
        pabyChunk[nChunkSize] = '\0';

        VSIFSeekL( poDS->fp, nStartOfData, SEEK_SET );

        /* Scan for dot in subsequent chunks of data. */
        while( !VSIFEofL( poDS->fp) )
        {
            VSIFReadL( pabyChunk, sizeof(GByte), nChunkSize, poDS->fp );
            CPLAssert( pabyChunk[nChunkSize] == '\0' );

            if( strchr( (const char *)pabyChunk, '.' ) != NULL ||
                strchr( (const char *)pabyChunk, ',' ) != NULL )
            {
                eDataType = GDT_Float32;
                break;
            }
        }

        /* Deallocate chunk. */
        VSIFree( pabyChunk );
    }

/* -------------------------------------------------------------------- */
/*      Create band information objects.                                */
/* -------------------------------------------------------------------- */
    AAIGRasterBand* band = new AAIGRasterBand( poDS, nStartOfData, eDataType );
    poDS->SetBand( 1, band );
    if (band->panLineOffset == NULL)
    {
        delete poDS;
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Try to read projection file.                                    */
/* -------------------------------------------------------------------- */
    char        *pszDirname, *pszBasename;
    VSIStatBufL   sStatBuf;

    pszDirname = CPLStrdup(CPLGetPath(poOpenInfo->pszFilename));
    pszBasename = CPLStrdup(CPLGetBasename(poOpenInfo->pszFilename));

    poDS->osPrjFilename = CPLFormFilename( pszDirname, pszBasename, "prj" );
    int nRet = VSIStatL( poDS->osPrjFilename, &sStatBuf );

#ifndef WIN32
    if( nRet != 0 )
    {
        poDS->osPrjFilename = CPLFormFilename( pszDirname, pszBasename, "PRJ" );
        nRet = VSIStatL( poDS->osPrjFilename, &sStatBuf );
    }
#endif

    if( nRet == 0 )
    {
        OGRSpatialReference     oSRS;

        poDS->papszPrj = CSLLoad( poDS->osPrjFilename );

        CPLDebug( "AAIGrid", "Loaded SRS from %s", 
                  poDS->osPrjFilename.c_str() );

        if( oSRS.importFromESRI( poDS->papszPrj ) == OGRERR_NONE )
        {
            // If geographic values are in seconds, we must transform. 
            // Is there a code for minutes too? 
            if( oSRS.IsGeographic() 
                && EQUAL(OSR_GDS( poDS->papszPrj, "Units", ""), "DS") )
            {
                poDS->adfGeoTransform[0] /= 3600.0;
                poDS->adfGeoTransform[1] /= 3600.0;
                poDS->adfGeoTransform[2] /= 3600.0;
                poDS->adfGeoTransform[3] /= 3600.0;
                poDS->adfGeoTransform[4] /= 3600.0;
                poDS->adfGeoTransform[5] /= 3600.0;
            }

            CPLFree( poDS->pszProjection );
            oSRS.exportToWkt( &(poDS->pszProjection) );
        }
    }

    CPLFree( pszDirname );
    CPLFree( pszBasename );

/* -------------------------------------------------------------------- */
/*      Initialize any PAM information.                                 */
/* -------------------------------------------------------------------- */
    poDS->SetDescription( poOpenInfo->pszFilename );
    poDS->TryLoadXML();

    return( poDS );
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr AAIGDataset::GetGeoTransform( double * padfTransform )

{
    memcpy( padfTransform, adfGeoTransform, sizeof(double) * 6 );
    return( CE_None );
}

/************************************************************************/
/*                          GetProjectionRef()                          */
/************************************************************************/

const char *AAIGDataset::GetProjectionRef()

{
    return pszProjection;
}

/************************************************************************/
/*                        AAIGCreateCopy()                              */
/************************************************************************/

static GDALDataset *
AAIGCreateCopy( const char * pszFilename, GDALDataset *poSrcDS, 
                int bStrict, char ** papszOptions, 
                GDALProgressFunc pfnProgress, void * pProgressData )

{
    int  nBands = poSrcDS->GetRasterCount();
    int  nXSize = poSrcDS->GetRasterXSize();
    int  nYSize = poSrcDS->GetRasterYSize();

/* -------------------------------------------------------------------- */
/*      Some rudimentary checks                                         */
/* -------------------------------------------------------------------- */
    if( nBands != 1 )
    {
        CPLError( CE_Failure, CPLE_NotSupported, 
                  "AAIG driver doesn't support %d bands.  Must be 1 band.\n",
                  nBands );

        return NULL;
    }

    if( !pfnProgress( 0.0, NULL, pProgressData ) )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Create the dataset.                                             */
/* -------------------------------------------------------------------- */
    FILE        *fpImage;

    fpImage = VSIFOpenL( pszFilename, "wt" );
    if( fpImage == NULL )
    {
        CPLError( CE_Failure, CPLE_OpenFailed, 
                  "Unable to create file %s.\n", 
                  pszFilename );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Write ASCII Grid file header                                    */
/* -------------------------------------------------------------------- */
    double      adfGeoTransform[6];
    char        szHeader[2000];
    const char *pszForceCellsize = 
        CSLFetchNameValue( papszOptions, "FORCE_CELLSIZE" );

    poSrcDS->GetGeoTransform( adfGeoTransform );

    if( ABS(adfGeoTransform[1]+adfGeoTransform[5]) < 0.0000001 
        || ABS(adfGeoTransform[1]-adfGeoTransform[5]) < 0.0000001 
        || (pszForceCellsize && CSLTestBoolean(pszForceCellsize)) )
        sprintf( szHeader, 
                 "ncols        %d\n" 
                 "nrows        %d\n"
                 "xllcorner    %.12f\n"
                 "yllcorner    %.12f\n"
                 "cellsize     %.12f\n", 
                 nXSize, nYSize, 
                 adfGeoTransform[0], 
                 adfGeoTransform[3] - nYSize * adfGeoTransform[1],
                 adfGeoTransform[1] );
    else
    {
        if( pszForceCellsize == NULL )
            CPLError( CE_Warning, CPLE_AppDefined, 
                      "Producing a Golden Surfer style file with DX and DY instead\n"
                      "of CELLSIZE since the input pixels are non-square.  Use the\n"
                      "FORCE_CELLSIZE=TRUE creation option to force use of DX for\n"
                      "even though this will be distorted.  Most ASCII Grid readers\n"
                      "(ArcGIS included) do not support the DX and DY parameters.\n" );
        sprintf( szHeader, 
                 "ncols        %d\n" 
                 "nrows        %d\n"
                 "xllcorner    %.12f\n"
                 "yllcorner    %.12f\n"
                 "dx           %.12f\n"
                 "dy           %.12f\n", 
                 nXSize, nYSize, 
                 adfGeoTransform[0], 
                 adfGeoTransform[3] + nYSize * adfGeoTransform[5],
                 adfGeoTransform[1],
                 fabs(adfGeoTransform[5]) );
    }

/* -------------------------------------------------------------------- */
/*      Handle nodata (optionally).                                     */
/* -------------------------------------------------------------------- */
    GDALRasterBand * poBand = poSrcDS->GetRasterBand( 1 );
    double dfNoData;
    int bSuccess;

    // Write `nodata' value to header if it is exists in source dataset
    dfNoData = poBand->GetNoDataValue( &bSuccess );
    if ( bSuccess )
        sprintf( szHeader+strlen(szHeader), "NODATA_value %6.20g\n", 
                 dfNoData );
    
    VSIFWriteL( szHeader, 1, strlen(szHeader), fpImage );

/* -------------------------------------------------------------------- */
/*     Builds the format string used for printing float values.         */
/* -------------------------------------------------------------------- */
    char szFormatFloat[32];
    strcpy(szFormatFloat, " %6.20g");
    const char *pszDecimalPrecision = 
        CSLFetchNameValue( papszOptions, "DECIMAL_PRECISION" );
    if (pszDecimalPrecision)
    {
        int nDecimal = atoi(pszDecimalPrecision);
        if (nDecimal >= 0)
            sprintf(szFormatFloat, " %%.%df", nDecimal);
    }

/* -------------------------------------------------------------------- */
/*      Loop over image, copying image data.                            */
/* -------------------------------------------------------------------- */
    int         *panScanline = NULL;
    double      *padfScanline = NULL;
    int         bReadAsInt;
    int         iLine, iPixel;
    CPLErr      eErr = CE_None;
    
    bReadAsInt = ( poBand->GetRasterDataType() == GDT_Byte 
                || poBand->GetRasterDataType() == GDT_Int16
                || poBand->GetRasterDataType() == GDT_UInt16
                || poBand->GetRasterDataType() == GDT_Int32 );

    // Write scanlines to output file
    if (bReadAsInt)
        panScanline = (int *) CPLMalloc( nXSize *
                                GDALGetDataTypeSize(GDT_Int32) / 8 );
    else
        padfScanline = (double *) CPLMalloc( nXSize *
                                    GDALGetDataTypeSize(GDT_Float64) / 8 );

    for( iLine = 0; eErr == CE_None && iLine < nYSize; iLine++ )
    {
        eErr = poBand->RasterIO( GF_Read, 0, iLine, nXSize, 1, 
                                 (bReadAsInt) ? (void*)panScanline : (void*)padfScanline,
                                 nXSize, 1, (bReadAsInt) ? GDT_Int32 : GDT_Float64,
                                 0, 0 );

        if( bReadAsInt )
        {
            for ( iPixel = 0; iPixel < nXSize; iPixel++ )
            {
                sprintf( szHeader, " %d", panScanline[iPixel] );
                if( VSIFWriteL( szHeader, strlen(szHeader), 1, fpImage ) != 1 )
                {
                    eErr = CE_Failure;
                    CPLError( CE_Failure, CPLE_AppDefined, 
                              "Write failed, disk full?\n" );
                    break;
                }
            }
        }
        else
        {
            for ( iPixel = 0; iPixel < nXSize; iPixel++ )
            {
                sprintf( szHeader, szFormatFloat, padfScanline[iPixel] );
                if( VSIFWriteL( szHeader, strlen(szHeader), 1, fpImage ) != 1 )
                {
                    eErr = CE_Failure;
                    CPLError( CE_Failure, CPLE_AppDefined, 
                              "Write failed, disk full?\n" );
                    break;
                }
            }
        }
        VSIFWriteL( (void *) "\n", 1, 1, fpImage );

        if( eErr == CE_None &&
            !pfnProgress((iLine + 1) / ((double) nYSize), NULL, pProgressData) )
        {
            eErr = CE_Failure;
            CPLError( CE_Failure, CPLE_UserInterrupt, 
                      "User terminated CreateCopy()" );
        }
    }

    CPLFree( panScanline );
    CPLFree( padfScanline );
    VSIFCloseL( fpImage );

/* -------------------------------------------------------------------- */
/*      Try to write projection file.                                   */
/* -------------------------------------------------------------------- */
    const char  *pszOriginalProjection;

    pszOriginalProjection = (char *)poSrcDS->GetProjectionRef();
    if( !EQUAL( pszOriginalProjection, "" ) )
    {
        char                    *pszDirname, *pszBasename;
        char                    *pszPrjFilename;
        char                    *pszESRIProjection = NULL;
        FILE                    *fp;
        OGRSpatialReference     oSRS;

        pszDirname = CPLStrdup( CPLGetPath(pszFilename) );
        pszBasename = CPLStrdup( CPLGetBasename(pszFilename) );

        pszPrjFilename = CPLStrdup( CPLFormFilename( pszDirname, pszBasename, "prj" ) );
        fp = VSIFOpenL( pszPrjFilename, "wt" );
        if (fp != NULL)
        {
            oSRS.importFromWkt( (char **) &pszOriginalProjection );
            oSRS.morphToESRI();
            oSRS.exportToWkt( &pszESRIProjection );
            VSIFWriteL( pszESRIProjection, 1, strlen(pszESRIProjection), fp );

            VSIFCloseL( fp );
            CPLFree( pszESRIProjection );
        }
        else
        {
            CPLError( CE_Failure, CPLE_FileIO, 
                      "Unable to create file %s.\n", pszPrjFilename );
        }
        CPLFree( pszDirname );
        CPLFree( pszBasename );
        CPLFree( pszPrjFilename );
    }
    
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
/*                              OSR_GDS()                               */
/************************************************************************/

static CPLString OSR_GDS( char **papszNV, const char * pszField, 
                           const char *pszDefaultValue )

{
    int         iLine;

    if( papszNV == NULL || papszNV[0] == NULL )
        return pszDefaultValue;

    for( iLine = 0; 
         papszNV[iLine] != NULL && 
             !EQUALN(papszNV[iLine],pszField,strlen(pszField));
         iLine++ ) {}

    if( papszNV[iLine] == NULL )
        return pszDefaultValue;
    else
    {
        CPLString osResult;
        char    **papszTokens;
        
        papszTokens = CSLTokenizeString(papszNV[iLine]);

        if( CSLCount(papszTokens) > 1 )
            osResult = papszTokens[1];
        else
            osResult = pszDefaultValue;
        
        CSLDestroy( papszTokens );
        return osResult;
    }
}

/************************************************************************/
/*                        GDALRegister_AAIGrid()                        */
/************************************************************************/

void GDALRegister_AAIGrid()

{
    GDALDriver  *poDriver;

    if( GDALGetDriverByName( "AAIGrid" ) == NULL )
    {
        poDriver = new GDALDriver();
        
        poDriver->SetDescription( "AAIGrid" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, 
                                   "Arc/Info ASCII Grid" );
        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, 
                                   "frmt_various.html#AAIGrid" );
        poDriver->SetMetadataItem( GDAL_DMD_EXTENSION, "asc" );
        poDriver->SetMetadataItem( GDAL_DMD_CREATIONDATATYPES, 
                                   "Byte UInt16 Int16 Int32 Float32" );

        poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );
        poDriver->SetMetadataItem( GDAL_DMD_CREATIONOPTIONLIST, 
"<CreationOptionList>\n"
"   <Option name='FORCE_CELLSIZE' type='boolean' description='Force use of CELLSIZE, default is FALSE.'/>\n"
"   <Option name='DECIMAL_PRECISION' type='int' description='Number of decimal when writing floating-point numbers.'/>\n"
"</CreationOptionList>\n" );

        poDriver->pfnOpen = AAIGDataset::Open;
        poDriver->pfnCreateCopy = AAIGCreateCopy;
        
        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}

