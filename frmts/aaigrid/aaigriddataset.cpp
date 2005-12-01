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
 *****************************************************************************
 *
 * $Log$
 * Revision 1.33  2005/12/01 04:50:12  fwarmerdam
 * Fixed dependency on poOpenInfo->fp.
 *
 * Revision 1.32  2005/12/01 04:26:15  fwarmerdam
 * Use large file API.
 *
 * Revision 1.31  2005/08/23 16:32:23  fwarmerdam
 * Fixed to support tabs for white space too.
 *
 * Revision 1.30  2005/05/05 14:01:36  fwarmerdam
 * PAM Enable
 *
 * Revision 1.29  2005/04/15 18:34:35  fwarmerdam
 * Added AREA_OR_POINT metadata.
 *
 * Revision 1.28  2005/02/08 18:15:34  fwarmerdam
 * Removed tail recursion in IReadBlock() to avoid stack overflows.
 *
 * Revision 1.27  2005/02/08 17:06:27  fwarmerdam
 * Fixed up Delete() method to avoid error if there is no .prj.
 *
 * Revision 1.26  2003/12/02 18:01:09  warmerda
 * Rewrote line reading function to avoid calls to CPLReadLine() since
 * some files have all the data for the whole file in one long line.
 *
 * Revision 1.25  2003/12/02 17:06:11  warmerda
 * Write out integers as integers.
 *
 * Revision 1.24  2003/07/08 21:12:07  warmerda
 * avoid warnings
 *
 * Revision 1.23  2003/05/27 17:34:22  warmerda
 * fixed problem with scanlines split over multiple text lines
 *
 * Revision 1.22  2003/05/02 16:06:36  dron
 * Memory leak fixed.
 *
 * Revision 1.21  2003/04/02 19:05:03  dron
 * Fixes for large file support in Windows environment.
 *
 * Revision 1.20  2003/03/27 19:52:58  dron
 * Implemented Delete() method and large file support.
 *
 * Revision 1.19  2003/02/06 04:55:35  warmerda
 * use default georef info if bounds missing
 *
 * Revision 1.18  2002/11/23 18:54:17  warmerda
 * added CREATIONDATATYPES metadata for drivers
 *
 * Revision 1.17  2002/10/02 13:10:16  warmerda
 * Fixed bug in setting of Y offset derived from yllcenter,  was off 1 pixel.
 * As per GRASS RT bug https://intevation.de/rt/webrt?serial_num=1332.
 *
 * Revision 1.16  2002/09/04 06:50:36  warmerda
 * avoid static driver pointers
 *
 * Revision 1.15  2002/07/09 21:06:54  warmerda
 * fixed free of SrcDS pszProjection in CreateCopy
 *
 * Revision 1.14  2002/06/15 00:07:23  aubin
 * mods to enable 64bit file i/o
 *
 * Revision 1.13  2002/06/14 12:28:41  dron
 * No data handling at writing.
 *
 * Revision 1.12  2002/06/13 13:48:28  dron
 * Added writing of .prj files for Arc/Info 8
 *
 * Revision 1.11  2002/06/12 21:12:24  warmerda
 * update to metadata based driver info
 *
 * Revision 1.10  2002/06/11 13:13:02  dron
 * Write support implemented with CreateCopy() function
 *
 * Revision 1.9  2002/06/04 17:37:23  dron
 * Header records may follow in any order now.
 *
 * Revision 1.8  2001/11/20 14:52:26  warmerda
 * Fix from Markus for center of pixel positioning.
 *
 * Revision 1.7  2001/11/11 23:50:59  warmerda
 * added required class keyword to friend declarations
 *
 * Revision 1.6  2001/11/06 14:34:22  warmerda
 * Fixed bug in YLLCENTER handling.  Added case for alternate line ordering.
 *
 * Revision 1.5  2001/07/18 04:51:56  warmerda
 * added CPL_CVSID
 *
 * Revision 1.4  2001/03/24 20:58:30  warmerda
 * Fixed typo.
 *
 * Revision 1.3  2001/03/13 19:39:03  warmerda
 * Added help link
 *
 * Revision 1.2  2001/03/12 15:35:05  warmerda
 * Fixed prj file naming.
 *
 * Revision 1.1  2001/03/12 15:15:30  warmerda
 * New
 *
 */

#include "gdal_pam.h"
#include <ctype.h>
#include "cpl_string.h"
#include "ogr_spatialref.h"

CPL_CVSID("$Id$");

CPL_C_START
void    GDALRegister_AAIGrid(void);
CPL_C_END

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
        (GUIntBig *) CPLCalloc( poDS->nRasterYSize, sizeof(GUIntBig) );
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
        || nBlockXOff != 0 )
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
                  "Can't seek to offset %ld in input file to read data.",
                  panLineOffset[nBlockYOff] );
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
        } while( isspace( chNext ) );

        while( !isspace(chNext)  )
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

        if( chNext == '\0' )
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
                ((float *) pImage)[iPixel] = (float) atof(szToken);
            else
                ((GInt16 *) pImage)[iPixel] = (GInt16) atoi(szToken);
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
    if( VSIFReadL( achReadBuf, 1, sizeof(achReadBuf), fp ) < 1 )
        return EOF;

    nOffsetInBuffer = 0;

    return achReadBuf[nOffsetInBuffer++];
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *AAIGDataset::Open( GDALOpenInfo * poOpenInfo )

{
    int         i, j;
    GDALDataType eDataType;
    char        **papszTokens;

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
              EQUALN((const char *) poOpenInfo->pabyHeader,"cellsize",8)) )
        return NULL;

    papszTokens =  
        CSLTokenizeString2( (const char *) poOpenInfo->pabyHeader,
                                  " \n\r\t", 0 );

/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */
    AAIGDataset         *poDS;

    poDS = new AAIGDataset();

/* -------------------------------------------------------------------- */
/*      Parse the header.                                               */
/* -------------------------------------------------------------------- */
    double dfCellSize;

    if ( (i = CSLFindString( papszTokens, "ncols" )) < 0 )
    {
        CSLDestroy( papszTokens );
        return NULL;
    }
    poDS->nRasterXSize = atoi(papszTokens[i + 1]);
    if ( (i = CSLFindString( papszTokens, "nrows" )) < 0 )
    {
        CSLDestroy( papszTokens );
        return NULL;
    }
    poDS->nRasterYSize = atoi(papszTokens[i + 1]);
    if ( (i = CSLFindString( papszTokens, "cellsize" )) < 0 )
    {
        CSLDestroy( papszTokens );
        return NULL;
    }    
    dfCellSize = atof( papszTokens[i + 1] );
    if ((i = CSLFindString( papszTokens, "xllcorner" )) >= 0 &&
        (j = CSLFindString( papszTokens, "yllcorner" )) >= 0 )
    {
        poDS->adfGeoTransform[0] = atof( papszTokens[i + 1] );
        poDS->adfGeoTransform[1] = dfCellSize;
        poDS->adfGeoTransform[2] = 0.0;
        poDS->adfGeoTransform[3] = atof( papszTokens[j + 1] )
            + poDS->nRasterYSize * dfCellSize;
        poDS->adfGeoTransform[4] = 0.0;
        poDS->adfGeoTransform[5] = - dfCellSize;
    }
    else if ((i = CSLFindString( papszTokens, "xllcenter" )) >= 0 &&
             (j = CSLFindString( papszTokens, "yllcenter" )) >= 0 )
    {
        poDS->SetMetadataItem( GDALMD_AREA_OR_POINT, GDALMD_AOP_POINT );

        poDS->adfGeoTransform[0] = atof(papszTokens[i + 1]) - 0.5 * dfCellSize;
        poDS->adfGeoTransform[1] = dfCellSize;
        poDS->adfGeoTransform[2] = 0.0;
        poDS->adfGeoTransform[3] = atof( papszTokens[j + 1] )
            - 0.5 * dfCellSize
            + poDS->nRasterYSize * dfCellSize ;
        poDS->adfGeoTransform[4] = 0.0;
        poDS->adfGeoTransform[5] = - dfCellSize;
    }
    else
    {
        poDS->adfGeoTransform[0] = 0.0;
        poDS->adfGeoTransform[1] = dfCellSize;
        poDS->adfGeoTransform[2] = 0.0;
        poDS->adfGeoTransform[3] = 0.0;
        poDS->adfGeoTransform[4] = 0.0;
        poDS->adfGeoTransform[5] = - dfCellSize;
    }

    if( (i = CSLFindString( papszTokens, "NODATA_value" )) >= 0 )
    {
        poDS->bNoDataSet = TRUE;
        poDS->dfNoDataValue = atof(papszTokens[i + 1]);
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
            return NULL;                        
        }

        if( poOpenInfo->pabyHeader[i-1] == '\n' 
            || poOpenInfo->pabyHeader[i-2] == '\n' )
        {
            if( !isalpha(poOpenInfo->pabyHeader[i]) )
            {
                nStartOfData = i;
                if( strstr((const char *)poOpenInfo->pabyHeader+i,".") != NULL)
                    eDataType = GDT_Float32;
                else
                    eDataType = GDT_Int16;

                break;
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Create band information objects.                                */
/* -------------------------------------------------------------------- */
    poDS->SetBand( 1, new AAIGRasterBand( poDS, nStartOfData, eDataType ) );

/* -------------------------------------------------------------------- */
/*      Try to read projection file.                                    */
/* -------------------------------------------------------------------- */
    char        *pszDirname, *pszBasename;
    const char  *pszPrjFilename;
    VSIStatBufL   sStatBuf;

    pszDirname = CPLStrdup(CPLGetPath(poOpenInfo->pszFilename));
    pszBasename = CPLStrdup(CPLGetBasename(poOpenInfo->pszFilename));

    pszPrjFilename = CPLFormFilename( pszDirname, pszBasename, "prj" );
    if( VSIStatL( pszPrjFilename, &sStatBuf ) == 0 )
    {
        OGRSpatialReference     oSRS;

        poDS->papszPrj = CSLLoad( pszPrjFilename );

        if( oSRS.importFromESRI( poDS->papszPrj ) == OGRERR_NONE )
        {
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

    poSrcDS->GetGeoTransform( adfGeoTransform );

    sprintf( szHeader, 
             "ncols        %d\n" 
             "nrows        %d\n"
             "xllcorner    %.12f\n"
             "yllcorner    %.12f\n"
             "cellsize     %.12f\n", 
             nXSize, nYSize, 
             adfGeoTransform[0], 
             adfGeoTransform[3]- nYSize * adfGeoTransform[1],
             adfGeoTransform[1] );

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
/*      Loop over image, copying image data.                            */
/* -------------------------------------------------------------------- */
    double      *padfScanline;
    int         iLine, iPixel;
    CPLErr      eErr = CE_None;
    
    // Write scanlines to output file
    padfScanline = (double *) CPLMalloc( nXSize *
                                GDALGetDataTypeSize(GDT_CFloat64) / 8 );
    for( iLine = 0; eErr == CE_None && iLine < nYSize; iLine++ )
    {
        eErr = poBand->RasterIO( GF_Read, 0, iLine, nXSize, 1, 
                              padfScanline, nXSize, 1, GDT_CFloat64,
                              sizeof(double), sizeof(double) * nXSize );

        if( poBand->GetRasterDataType() == GDT_Byte 
            || poBand->GetRasterDataType() == GDT_Int16
            || poBand->GetRasterDataType() == GDT_UInt16
            || poBand->GetRasterDataType() == GDT_Int32 )
        {
            for ( iPixel = 0; iPixel < nXSize; iPixel++ )
            {
                sprintf( szHeader, " %d", (int) padfScanline[iPixel] );
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
                sprintf( szHeader, " %6.20g", padfScanline[iPixel] );
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
        const char              *pszPrjFilename;
        char                    *pszESRIProjection = NULL;
        FILE                    *fp;
        OGRSpatialReference     oSRS;

        pszDirname = CPLStrdup( CPLGetPath(pszFilename) );
        pszBasename = CPLStrdup( CPLGetBasename(pszFilename) );

        pszPrjFilename = CPLFormFilename( pszDirname, pszBasename, "prj" );
        fp = VSIFOpenL( pszPrjFilename, "wt" );
        
        oSRS.importFromWkt( (char **) &pszOriginalProjection );
        oSRS.morphToESRI();
        oSRS.exportToWkt( &pszESRIProjection );
        VSIFWriteL( pszESRIProjection, 1, strlen(pszESRIProjection), fp );

        VSIFCloseL( fp );
        CPLFree( pszDirname );
        CPLFree( pszBasename );
        CPLFree( pszESRIProjection );
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
/*                               Remove()                               */
/*    Called from the Delete()                                          */
/************************************************************************/

CPLErr AAIGDataset::Remove( const char * pszFilename, int bRepError )

{
    VSIStatBufL      sStat;

    if( VSIStatL( pszFilename, &sStat ) == 0 && VSI_ISREG( sStat.st_mode ) )
    {
        if( VSIUnlink( pszFilename ) == 0 )
            return CE_None;
        else
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Attempt to unlink %s failed.\n", pszFilename );
            return CE_Failure;
        }
    }
    else if( bRepError )
    {
        
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Unable to delete %s, not a file.\n", pszFilename );
        return CE_Failure;
    }
    
    return CE_None;
}

/************************************************************************/
/*                               Delete()                               */
/************************************************************************/

CPLErr AAIGDataset::Delete( const char *pszFilename )

{
    Remove( CPLResetExtension( pszFilename, "prj" ), FALSE );
    return Remove( pszFilename, TRUE );
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
                                   "Byte UInt16 Int16 Float32" );

        poDriver->pfnOpen = AAIGDataset::Open;
        poDriver->pfnCreateCopy = AAIGCreateCopy;
        poDriver->pfnDelete = AAIGDataset::Delete;
        
        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}

