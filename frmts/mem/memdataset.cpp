/******************************************************************************
 * $Id$
 *
 * Project:  Memory Array Translator
 * Purpose:  Complete implementation.
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
 * Revision 1.3  2001/07/18 04:51:57  warmerda
 * added CPL_CVSID
 *
 * Revision 1.2  2000/07/19 19:07:04  warmerda
 * break linkage between MEMDataset and MEMRasterBand
 *
 * Revision 1.1  2000/07/19 15:55:11  warmerda
 * New
 *
 */

#include "memdataset.h"
#include "cpl_string.h"

CPL_CVSID("$Id$");

static GDALDriver	*poMEMDriver = NULL;

/************************************************************************/
/*                           MEMRasterBand()                            */
/************************************************************************/

MEMRasterBand::MEMRasterBand( GDALDataset *poDS, int nBand,
                              GByte *pabyDataIn, GDALDataType eTypeIn, 
                              int nPixelOffsetIn, int nLineOffsetIn,
                              int bAssumeOwnership )

{
    this->poDS = poDS;
    this->nBand = nBand;

    this->eAccess = poDS->GetAccess();

    eDataType = eTypeIn;

    nBlockXSize = poDS->GetRasterXSize();
    nBlockYSize = 1;

    if( nPixelOffsetIn == 0 )
        nPixelOffsetIn = GDALGetDataTypeSize(eTypeIn) / 8;

    if( nLineOffsetIn == 0 )
        nLineOffsetIn = nPixelOffsetIn * nBlockXSize;

    nPixelOffset = nPixelOffsetIn;
    nLineOffset = nLineOffsetIn;
    bOwnData = bAssumeOwnership;

    pabyData = pabyDataIn;
}

/************************************************************************/
/*                           ~MEMRasterBand()                           */
/************************************************************************/

MEMRasterBand::~MEMRasterBand()

{
    if( bOwnData )
        VSIFree( pabyData );
}


/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr MEMRasterBand::IReadBlock( int nBlockXOff, int nBlockYOff,
                                   void * pImage )

{
    int     nWordSize = GDALGetDataTypeSize( eDataType );
    CPLAssert( nBlockXOff == 0 );

    if( nPixelOffset*8 == nWordSize )
    {
        memcpy( pImage, 
                pabyData+nLineOffset*nBlockYOff, 
                nPixelOffset * nBlockXSize );
    }
    else
    {
        GByte *pabyCur = pabyData + nLineOffset*nBlockYOff;

        for( int iPixel = 0; iPixel < nBlockXSize; iPixel++ )
        {
            memcpy( ((GByte *) pImage) + iPixel*nWordSize, 
                    pabyCur + iPixel*nPixelOffset, 
                    nWordSize );
        }
    }

    return CE_None;
}

/************************************************************************/
/*                            IWriteBlock()                             */
/************************************************************************/

CPLErr MEMRasterBand::IWriteBlock( int nBlockXOff, int nBlockYOff,
                                     void * pImage )

{
    int     nWordSize = GDALGetDataTypeSize( eDataType );
    CPLAssert( nBlockXOff == 0 );

    if( nPixelOffset*8 == nWordSize )
    {
        memcpy( pabyData+nLineOffset*nBlockYOff, 
                pImage, 
                nPixelOffset * nBlockXSize );
    }
    else
    {
        GByte *pabyCur = pabyData + nLineOffset*nBlockYOff;

        for( int iPixel = 0; iPixel < nBlockXSize; iPixel++ )
        {
            memcpy( pabyCur + iPixel*nPixelOffset, 
                    ((GByte *) pImage) + iPixel*nWordSize, 
                    nWordSize );
        }
    }

    return CE_None;
}

/************************************************************************/
/* ==================================================================== */
/*      MEMDataset                                                     */
/* ==================================================================== */
/************************************************************************/


/************************************************************************/
/*                            MEMDataset()                             */
/************************************************************************/

MEMDataset::MEMDataset()

{
}

/************************************************************************/
/*                            ~MEMDataset()                            */
/************************************************************************/

MEMDataset::~MEMDataset()

{
    FlushCache();
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *MEMDataset::Open( GDALOpenInfo * poOpenInfo )

{
    char    **papszOptions;

/* -------------------------------------------------------------------- */
/*      Do we have the special filename signature for MEM format        */
/*      description strings?                                            */
/* -------------------------------------------------------------------- */
    if( !EQUALN(poOpenInfo->pszFilename,"MEM:::",6) 
        || poOpenInfo->fp != NULL )
        return NULL;

    papszOptions = CSLTokenizeStringComplex(poOpenInfo->pszFilename+6, ",",
                                            TRUE, FALSE );

/* -------------------------------------------------------------------- */
/*      Verify we have all required fields                              */
/* -------------------------------------------------------------------- */
    if( CSLFetchNameValue( papszOptions, "PIXELS" ) == NULL
        || CSLFetchNameValue( papszOptions, "LINES" ) == NULL
        || CSLFetchNameValue( papszOptions, "DATAPOINTER" ) == NULL )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
              "Missing required field (one of PIXELS, LINES or DATAPOINTER)\n"
              "Unable to access in-memory array." );

        CSLDestroy( papszOptions );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Create the new GTiffDataset object.                             */
/* -------------------------------------------------------------------- */
    MEMDataset *poDS;

    poDS = new MEMDataset();
    poDS->poDriver = poMEMDriver;

    poDS->nRasterXSize = atoi(CSLFetchNameValue(papszOptions,"PIXELS"));
    poDS->nRasterYSize = atoi(CSLFetchNameValue(papszOptions,"LINES"));

    poDS->eAccess = GA_Update;

/* -------------------------------------------------------------------- */
/*      Extract other information.                                      */
/* -------------------------------------------------------------------- */
    const char *pszOption;
    GDALDataType eType;
    int nBands, nPixelOffset, nLineOffset, nBandOffset;
    GByte *pabyData;

    pszOption = CSLFetchNameValue(papszOptions,"BANDS");
    if( pszOption == NULL )
        nBands = 1;
    else
        nBands = atoi(pszOption);

    pszOption = CSLFetchNameValue(papszOptions,"DATATYPE");
    if( pszOption == NULL )
        eType = GDT_Byte;
    else
        eType = (GDALDataType) atoi(pszOption);

    pszOption = CSLFetchNameValue(papszOptions,"PIXELOFFSET");
    if( pszOption == NULL )
        nPixelOffset = GDALGetDataTypeSize(eType) / 8;
    else
        nPixelOffset = atoi(pszOption);

    pszOption = CSLFetchNameValue(papszOptions,"LINEOFFSET");
    if( pszOption == NULL )
        nLineOffset = poDS->nRasterXSize * nPixelOffset;
    else
        nLineOffset = atoi(pszOption);

    pszOption = CSLFetchNameValue(papszOptions,"BANDOFFSET");
    if( pszOption == NULL )
        nBandOffset = nLineOffset * poDS->nRasterYSize;
    else
        nBandOffset = atoi(pszOption);

    pabyData = (GByte *) strtol(CSLFetchNameValue(papszOptions,"DATAPOINTER"),
                                NULL, 0 );

/* -------------------------------------------------------------------- */
/*      Create band information objects.                                */
/* -------------------------------------------------------------------- */
    for( int iBand = 0; iBand < nBands; iBand++ )
    {
        poDS->SetBand( iBand+1, 
                       new MEMRasterBand( poDS, iBand+1, 
                                          pabyData + iBand * nBandOffset,
                                          eType, nPixelOffset, nLineOffset, 
                                          FALSE ) );
    }

/* -------------------------------------------------------------------- */
/*      Try to return a regular handle on the file.                     */
/* -------------------------------------------------------------------- */
    return poDS;
}

/************************************************************************/
/*                               Create()                               */
/************************************************************************/

GDALDataset *MEMDataset::Create( const char * pszFilename,
                                 int nXSize, int nYSize, int nBands,
                                 GDALDataType eType,
                                 char ** /* notdef: papszParmList */ )

{
/* -------------------------------------------------------------------- */
/*      First allocate band data, verifying that we can get enough      */
/*      memory.                                                         */
/* -------------------------------------------------------------------- */
    GByte  	**papBandData;
    int   	iBand;
    int         nWordSize = GDALGetDataTypeSize(eType) / 8;

    papBandData = (GByte **) CPLCalloc(sizeof(void *),nBands);
    for( iBand = 0; iBand < nBands; iBand++ )
    {
        papBandData[iBand] = (GByte *) VSICalloc( nWordSize, nXSize * nYSize );
        if( papBandData[iBand] == NULL )
        {
            for( iBand = 0; iBand < nBands; iBand++ )
            {
                if( papBandData[iBand] )
                    VSIFree( papBandData[iBand] );
            }

            CPLError( CE_Failure, CPLE_OutOfMemory,
                      "Unable to create band arrays ... out of memory." );
            return NULL;
        }
    }

/* -------------------------------------------------------------------- */
/*      Create the new GTiffDataset object.                             */
/* -------------------------------------------------------------------- */
    MEMDataset *poDS;

    poDS = new MEMDataset();
    poDS->poDriver = poMEMDriver;

    poDS->nRasterXSize = nXSize;
    poDS->nRasterYSize = nYSize;
    poDS->eAccess = GA_Update;

/* -------------------------------------------------------------------- */
/*      Create band information objects.                                */
/* -------------------------------------------------------------------- */
    for( iBand = 0; iBand < nBands; iBand++ )
    {
        poDS->SetBand( iBand+1, 
                       new MEMRasterBand( poDS, iBand+1, papBandData[iBand],
                                          eType, 0, 0, TRUE ) );
    }

    CPLFree( papBandData );

/* -------------------------------------------------------------------- */
/*      Try to return a regular handle on the file.                     */
/* -------------------------------------------------------------------- */
    return poDS;
}

/************************************************************************/
/*                          GDALRegister_MEM()                          */
/************************************************************************/

void GDALRegister_MEM()

{
    GDALDriver	*poDriver;

    if( poMEMDriver == NULL )
    {
        poMEMDriver = poDriver = new GDALDriver();
        
        poDriver->pszShortName = "MEM";
        poDriver->pszLongName = "In Memory Raster";
        
        poDriver->pfnOpen = MEMDataset::Open;
        poDriver->pfnCreate = MEMDataset::Create;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}
