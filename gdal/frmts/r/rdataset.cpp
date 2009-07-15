/******************************************************************************
 * $Id: bmpdataset.cpp 17322 2009-06-30 01:11:20Z warmerdam $
 *
 * Project:  R Format Driver
 * Purpose:  Read/write R stats package object format.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2009, Frank Warmerdam <warmerdam@pobox.com>
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
#include "cpl_string.h"

CPL_CVSID("$Id: bmpdataset.cpp 17322 2009-06-30 01:11:20Z warmerdam $");

CPL_C_START
void    GDALRegister_R(void);
CPL_C_END

/************************************************************************/
/* ==================================================================== */
/*                               RDataset                               */
/* ==================================================================== */
/************************************************************************/

class RDataset : public GDALPamDataset
{
    friend class RRasterBand;


  public:
                RDataset();
                ~RDataset();

    static GDALDataset  *Open( GDALOpenInfo * );
};

/************************************************************************/
/* ==================================================================== */
/*                            RRasterBand                               */
/* ==================================================================== */
/************************************************************************/

class RRasterBand : public GDALPamRasterBand
{
    friend class RDataset;

  public:

                RRasterBand( RDataset *, int );
                ~RRasterBand();

    virtual CPLErr          IReadBlock( int, int, void * );
};

/************************************************************************/
/*                           RRasterBand()                            */
/************************************************************************/

RRasterBand::RRasterBand( RDataset *poDS, int nBand )
{
    this->poDS = poDS;
    this->nBand = nBand;
    eDataType = GDT_Byte;
}

/************************************************************************/
/*                           ~RRasterBand()                           */
/************************************************************************/

RRasterBand::~RRasterBand()
{
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr RRasterBand::IReadBlock( int nBlockXOff, int nBlockYOff,
                                  void * pImage )
{
    return CE_Failure;
}

/************************************************************************/
/*                           RDataset()                               */
/************************************************************************/

RDataset::RDataset()
{
}

/************************************************************************/
/*                            ~RDataset()                             */
/************************************************************************/

RDataset::~RDataset()
{
    FlushCache();
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *RDataset::Open( GDALOpenInfo * poOpenInfo )
{
    return NULL;

/* -------------------------------------------------------------------- */
/*      Initialize any PAM information.                                 */
/* -------------------------------------------------------------------- */
    RDataset *poDS = new RDataset();

    poDS->SetDescription( poOpenInfo->pszFilename );
    poDS->TryLoadXML();

/* -------------------------------------------------------------------- */
/*      Check for overviews.                                            */
/* -------------------------------------------------------------------- */
    poDS->oOvManager.Initialize( poDS, poOpenInfo->pszFilename );

    return( poDS );
}

/************************************************************************/
/* ==================================================================== */
/*                        Writer Implementation                         */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                           RWriteInteger()                            */
/************************************************************************/

static void RWriteInteger( FILE *fp, int bASCII, int nValue )

{
    if( bASCII )
    {
        char szOutput[50];
        sprintf( szOutput, "%d\n", nValue );
        VSIFWriteL( szOutput, 1, strlen(szOutput), fp );
    }
    else
    {
        CPL_MSBPTR32( &nValue );
        VSIFWriteL( &nValue, 4, 1, fp );
    }
}

/************************************************************************/
/*                            RWriteString()                            */
/************************************************************************/

static void RWriteString( FILE *fp, int bASCII, const char *pszValue )

{
    RWriteInteger( fp, bASCII, 4105 );
    RWriteInteger( fp, bASCII, (int) strlen(pszValue) );
    
    if( bASCII )
    {
        VSIFWriteL( pszValue, 1, strlen(pszValue), fp );
        VSIFWriteL( "\n", 1, 1, fp );
    }
    else
    {
        VSIFWriteL( pszValue, 1, (int) strlen(pszValue), fp );
    }
}

/************************************************************************/
/*                            RCreateCopy()                             */
/************************************************************************/

static GDALDataset *
RCreateCopy( const char * pszFilename, GDALDataset *poSrcDS, 
             int bStrict, char ** papszOptions, 
             GDALProgressFunc pfnProgress, void * pProgressData )

{
    int  nBands = poSrcDS->GetRasterCount();
    int  nXSize = poSrcDS->GetRasterXSize();
    int  nYSize = poSrcDS->GetRasterYSize();
    int  bASCII = CSLFetchBoolean( papszOptions, "ASCII", FALSE );

/* -------------------------------------------------------------------- */
/*      Some some rudimentary checks                                    */
/* -------------------------------------------------------------------- */

/* -------------------------------------------------------------------- */
/*      Create the file.                                                */
/* -------------------------------------------------------------------- */
    FILE	*fp;

    fp = VSIFOpenL( pszFilename, "wb" );
    if( fp == NULL )
    {
        CPLError( CE_Failure, CPLE_OpenFailed, 
                  "Unable to create file %s.\n", 
                  pszFilename );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Write header with version, etc.                                 */
/* -------------------------------------------------------------------- */
    if( bASCII )
    {
        const char *pszHeader = "RDA2\nA\n";
        VSIFWriteL( pszHeader, 1, strlen(pszHeader), fp );
    }
    else
    {
        const char *pszHeader = "RDX2\nX\n";
        VSIFWriteL( pszHeader, 1, strlen(pszHeader), fp );
    }

    RWriteInteger( fp, bASCII, 2 );
    RWriteInteger( fp, bASCII, 133377 );
    RWriteInteger( fp, bASCII, 131840 );

/* -------------------------------------------------------------------- */
/*      Establish the primary pairlist with one component object.       */
/* -------------------------------------------------------------------- */
    RWriteInteger( fp, bASCII, 1026 );
    RWriteInteger( fp, bASCII, 1 );  

/* -------------------------------------------------------------------- */
/*      Write the object name.  Eventually we should derive this        */
/*      from the filename, possible with override by a creation         */
/*      option.                                                         */
/* -------------------------------------------------------------------- */
    RWriteString( fp, bASCII, "gg" );

/* -------------------------------------------------------------------- */
/*      For now we write the raster as a numeric array with             */
/*      attributes (526).                                               */
/* -------------------------------------------------------------------- */
    RWriteInteger( fp, bASCII, 526 );
    RWriteInteger( fp, bASCII, nXSize * nYSize * nBands );

/* -------------------------------------------------------------------- */
/*      Write the raster data.                                          */
/* -------------------------------------------------------------------- */
    double 	*padfScanline;
    CPLErr      eErr = CE_None;
    int         iLine;

    padfScanline = (double *) CPLMalloc( nXSize * sizeof(double) );

    for( int iBand = 0; iBand < nBands; iBand++ )
    {
        GDALRasterBand * poBand = poSrcDS->GetRasterBand( iBand+1 );

        for( iLine = 0; iLine < nYSize && eErr == CE_None; iLine++ )
        {
            int iValue;

            eErr = poBand->RasterIO( GF_Read, 0, iLine, nXSize, 1, 
                                     padfScanline, nXSize, 1, GDT_Float64,
                                     sizeof(double), 0 );

            if( bASCII )
            {
                for( iValue = 0; iValue < nXSize; iValue++ )
                {
                    char szValue[128];
                    sprintf(szValue,"%.16g\n", padfScanline[iValue] );
                    VSIFWriteL( szValue, 1, strlen(szValue), fp );
                }
            }
            else
            {
                for( iValue = 0; iValue < nXSize; iValue++ )
                    CPL_MSBPTR64( padfScanline + iValue );

                VSIFWriteL( padfScanline, 8, nXSize, fp );
            }
            
            if( eErr == CE_None
                && !pfnProgress( (iLine+1) / (double) nYSize,
                                 NULL, pProgressData ) )
            {
                eErr = CE_Failure;
                CPLError( CE_Failure, CPLE_UserInterrupt,
                          "User terminated CreateCopy()" );
            }
        }
    }

    CPLFree( padfScanline );

/* -------------------------------------------------------------------- */
/*      Write out the dims attribute.                                   */
/* -------------------------------------------------------------------- */
    RWriteInteger( fp, bASCII, 1026 );
    RWriteInteger( fp, bASCII, 1 );  
    
    RWriteString( fp, bASCII, "dim" );

    RWriteInteger( fp, bASCII, 13 );
    RWriteInteger( fp, bASCII, 3 );
    RWriteInteger( fp, bASCII, nXSize );
    RWriteInteger( fp, bASCII, nYSize );
    RWriteInteger( fp, bASCII, nBands );

    RWriteInteger( fp, bASCII, 254 );

/* -------------------------------------------------------------------- */
/*      Terminate overall pairlist.                                     */
/* -------------------------------------------------------------------- */
    RWriteInteger( fp, bASCII, 254 );

/* -------------------------------------------------------------------- */
/*      Cleanup.                                                        */
/* -------------------------------------------------------------------- */
    VSIFCloseL( fp );

    if( eErr != CE_None )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Re-open dataset, and copy any auxilary pam information.         */
/* -------------------------------------------------------------------- */
    RDataset *poDS = (RDataset *) GDALOpen( pszFilename, GA_ReadOnly );

    if( poDS )
        poDS->CloneInfo( poSrcDS, GCIF_PAM_DEFAULT );

    return poDS;
}

/************************************************************************/
/*                        GDALRegister_R()                              */
/************************************************************************/

void GDALRegister_R()

{
    GDALDriver  *poDriver;

    if( GDALGetDriverByName( "R" ) == NULL )
    {
        poDriver = new GDALDriver();

        poDriver->SetDescription( "R" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME,
                                   "R Object Data Store" );
        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC,
                                   "frmt_r.html" );
        poDriver->SetMetadataItem( GDAL_DMD_EXTENSION, "rda" );
        poDriver->SetMetadataItem( GDAL_DMD_CREATIONDATATYPES, "Int32 Float32" );
        poDriver->SetMetadataItem( GDAL_DMD_CREATIONOPTIONLIST,
"<CreationOptionList>"
"</CreationOptionList>" );

        poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );

        poDriver->pfnOpen = RDataset::Open;
        poDriver->pfnCreateCopy = RCreateCopy;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}

