/******************************************************************************
 * $Id$
 *
 * Project:  R Format Driver
 * Purpose:  Read/write R stats package object format.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2009, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2009-2010, Even Rouault <even dot rouault at mines-paris dot org>
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
#include "../raw/rawdataset.h"

CPL_CVSID("$Id$");

CPL_C_START
void    GDALRegister_R(void);
CPL_C_END

GDALDataset *
RCreateCopy( const char * pszFilename, GDALDataset *poSrcDS, 
             int bStrict, char ** papszOptions, 
             GDALProgressFunc pfnProgress, void * pProgressData );

#define R_NILSXP        0
#define R_LISTSXP       2
#define R_CHARSXP       9
#define R_INTSXP        13
#define R_REALSXP       14
#define R_STRSXP        16

/************************************************************************/
/* ==================================================================== */
/*                               RDataset                               */
/* ==================================================================== */
/************************************************************************/

class RDataset : public GDALPamDataset
{
    friend class RRasterBand;
    VSILFILE       *fp;
    int         bASCII;
    CPLString   osLastStringRead;

    vsi_l_offset nStartOfData;

    double     *padfMatrixValues;

    const char *ASCIIFGets();
    int         ReadInteger();
    double      ReadFloat();
    const char *ReadString();
    int         ReadPair( CPLString &osItemName, int &nItemType );

  public:
                RDataset();
                ~RDataset();

    static GDALDataset  *Open( GDALOpenInfo * );
    static int          Identify( GDALOpenInfo * );
};

/************************************************************************/
/* ==================================================================== */
/*                            RRasterBand                               */
/* ==================================================================== */
/************************************************************************/

class RRasterBand : public GDALPamRasterBand
{
    friend class RDataset;

    const double *padfMatrixValues;

  public:

                RRasterBand( RDataset *, int, const double * );
                ~RRasterBand();

    virtual CPLErr          IReadBlock( int, int, void * );
};

/************************************************************************/
/*                            RRasterBand()                             */
/************************************************************************/

RRasterBand::RRasterBand( RDataset *poDS, int nBand, 
                          const double *padfMatrixValues )
{
    this->poDS = poDS;
    this->nBand = nBand;
    this->padfMatrixValues = padfMatrixValues;

    eDataType = GDT_Float64;

    nBlockXSize = poDS->nRasterXSize;
    nBlockYSize = 1;
}

/************************************************************************/
/*                            ~RRasterBand()                            */
/************************************************************************/

RRasterBand::~RRasterBand()
{
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr RRasterBand::IReadBlock( CPL_UNUSED int nBlockXOff,
                                int nBlockYOff,
                                void * pImage )
{
    memcpy( pImage, padfMatrixValues + nBlockYOff * nBlockXSize,
            nBlockXSize * 8 );
    return CE_None;
}

/************************************************************************/
/* ==================================================================== */
/*                              RDataset()                              */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                              RDataset()                              */
/************************************************************************/

RDataset::RDataset()
{
    fp = NULL;
    padfMatrixValues = NULL;
}

/************************************************************************/
/*                             ~RDataset()                              */
/************************************************************************/

RDataset::~RDataset()
{
    FlushCache();
    CPLFree(padfMatrixValues);
    
    if( fp )
        VSIFCloseL( fp );
}

/************************************************************************/
/*                             ASCIIFGets()                             */
/*                                                                      */
/*      Fetch one line from an ASCII source into osLastStringRead.      */
/************************************************************************/

const char *RDataset::ASCIIFGets()

{
    char chNextChar;

    osLastStringRead.resize(0);

    do 
    {
        chNextChar = '\n';
        VSIFReadL( &chNextChar, 1, 1, fp );
        if( chNextChar != '\n' )
            osLastStringRead += chNextChar;
    } while( chNextChar != '\n' && chNextChar != '\0' );

    return osLastStringRead;
}

/************************************************************************/
/*                            ReadInteger()                             */
/************************************************************************/

int RDataset::ReadInteger()

{
    if( bASCII )
    {
        return atoi(ASCIIFGets());
    }
    else
    {
        GInt32  nValue;

        if( VSIFReadL( &nValue, 4, 1, fp ) != 1 )
            return -1;
        CPL_MSBPTR32( &nValue );

        return nValue;
    }
}

/************************************************************************/
/*                             ReadFloat()                              */
/************************************************************************/

double RDataset::ReadFloat()

{
    if( bASCII )
    {
        return atof(ASCIIFGets());
    }
    else
    {
        double  dfValue;

        if( VSIFReadL( &dfValue, 8, 1, fp ) != 1 )
            return -1;
        CPL_MSBPTR64( &dfValue );

        return dfValue;
    }
}

/************************************************************************/
/*                             ReadString()                             */
/************************************************************************/

const char *RDataset::ReadString()

{
    if( ReadInteger() % 256 != R_CHARSXP )
    {
        osLastStringRead = "";
        return "";
    }

    size_t nLen = ReadInteger();
        
    char *pachWrkBuf = (char *) VSIMalloc(nLen);
    if (pachWrkBuf == NULL)
    {
        osLastStringRead = "";
        return "";
    }
    if( VSIFReadL( pachWrkBuf, 1, nLen, fp ) != nLen )
    {
        osLastStringRead = "";
        CPLFree( pachWrkBuf );
        return "";
    }
    
    if( bASCII )
    {
        /* suck up newline and any extra junk */
        ASCIIFGets();
    }
    
    osLastStringRead.assign( pachWrkBuf, nLen );
    CPLFree( pachWrkBuf );

    return osLastStringRead;
}

/************************************************************************/
/*                              ReadPair()                              */
/************************************************************************/

int RDataset::ReadPair( CPLString &osObjName, int &nObjCode )

{
    nObjCode = ReadInteger();
    if( nObjCode == 254 )
        return TRUE;

    if( (nObjCode % 256) != R_LISTSXP )
    {
        CPLError( CE_Failure, CPLE_OpenFailed, 
                  "Did not find expected object pair object." );
        return FALSE;
    }

    int nPairCount = ReadInteger();
    if( nPairCount != 1 )
    {
        CPLError( CE_Failure, CPLE_OpenFailed, 
                  "Did not find expected pair count of 1." );
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Read the object name.                                           */
/* -------------------------------------------------------------------- */
    const char *pszName = ReadString();
    if( pszName == NULL || pszName[0] == '\0' )
        return FALSE;

    osObjName = pszName;

/* -------------------------------------------------------------------- */
/*      Confirm that we have a numeric matrix object.                   */
/* -------------------------------------------------------------------- */
    nObjCode = ReadInteger();

    return TRUE;
}

/************************************************************************/
/*                              Identify()                              */
/************************************************************************/

int RDataset::Identify( GDALOpenInfo *poOpenInfo )
{
    if( poOpenInfo->nHeaderBytes < 50 )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      If the extension is .rda and the file type is gzip              */
/*      compressed we assume it is a gziped R binary file.              */
/* -------------------------------------------------------------------- */
    if( memcmp(poOpenInfo->pabyHeader,"\037\213\b",3) == 0 
        && EQUAL(CPLGetExtension(poOpenInfo->pszFilename),"rda") )
        return TRUE;

/* -------------------------------------------------------------------- */
/*      Is this an ASCII or XDR binary R file?                          */
/* -------------------------------------------------------------------- */
    if( !EQUALN((const char *)poOpenInfo->pabyHeader,"RDA2\nA\n",7) 
        && !EQUALN((const char *)poOpenInfo->pabyHeader,"RDX2\nX\n",7) )
        return FALSE;

    return TRUE;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *RDataset::Open( GDALOpenInfo * poOpenInfo )
{
    if( !Identify( poOpenInfo ) )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Confirm the requested access is supported.                      */
/* -------------------------------------------------------------------- */
    if( poOpenInfo->eAccess == GA_Update )
    {
        CPLError( CE_Failure, CPLE_NotSupported, 
                  "The R driver does not support update access to existing"
                  " datasets.\n" );
        return NULL;
    }
    
/* -------------------------------------------------------------------- */
/*      Do we need to route the file through the decompression          */
/*      machinery?                                                      */
/* -------------------------------------------------------------------- */
    CPLString osAdjustedFilename;

    if( memcmp(poOpenInfo->pabyHeader,"\037\213\b",3) == 0 )
        osAdjustedFilename = "/vsigzip/";

    osAdjustedFilename += poOpenInfo->pszFilename;

/* -------------------------------------------------------------------- */
/*      Establish this as a dataset and open the file using VSI*L.      */
/* -------------------------------------------------------------------- */
    RDataset *poDS = new RDataset();

    poDS->fp = VSIFOpenL( osAdjustedFilename, "r" );
    if( poDS->fp == NULL )
    {
        delete poDS;
        return NULL;
    }

    poDS->bASCII = EQUALN((const char *)poOpenInfo->pabyHeader,"RDA2\nA\n",7);

/* -------------------------------------------------------------------- */
/*      Confirm this is a version 2 file.                               */
/* -------------------------------------------------------------------- */
    VSIFSeekL( poDS->fp, 7, SEEK_SET );
    if( poDS->ReadInteger() != R_LISTSXP )
    {
        delete poDS;
        CPLError( CE_Failure, CPLE_OpenFailed, 
                  "It appears %s is not a version 2 R object file after all!",
                  poOpenInfo->pszFilename );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Skip the version values.                                        */
/* -------------------------------------------------------------------- */
    poDS->ReadInteger();
    poDS->ReadInteger();

/* -------------------------------------------------------------------- */
/*      Confirm we have a numeric vector object in a pairlist.          */
/* -------------------------------------------------------------------- */
    CPLString osObjName;
    int nObjCode;

    if( !poDS->ReadPair( osObjName, nObjCode ) )
    {
        delete poDS;
        return NULL;
    }

    if( nObjCode % 256 != R_REALSXP )
    {
        delete poDS;
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "Failed to find expected numeric vector object." );
        return NULL;
    }

    poDS->SetMetadataItem( "R_OBJECT_NAME", osObjName );

/* -------------------------------------------------------------------- */
/*      Read the count.                                                 */
/* -------------------------------------------------------------------- */
    int nValueCount = poDS->ReadInteger();

    poDS->nStartOfData = VSIFTellL( poDS->fp );

/* -------------------------------------------------------------------- */
/*      Read/Skip ahead to attributes.                                  */
/* -------------------------------------------------------------------- */
    if( poDS->bASCII )
    {
        poDS->padfMatrixValues = (double*) VSIMalloc2( nValueCount, sizeof(double) );
        if (poDS->padfMatrixValues == NULL)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Cannot allocate %d doubles", nValueCount);
            delete poDS;
            return NULL;
        }
        for( int iValue = 0; iValue < nValueCount; iValue++ )
            poDS->padfMatrixValues[iValue] = poDS->ReadFloat();
    }
    else
    {
        VSIFSeekL( poDS->fp, 8 * nValueCount, SEEK_CUR );
    }

/* -------------------------------------------------------------------- */
/*      Read pairs till we run out, trying to find a few items that     */
/*      have special meaning to us.                                     */
/* -------------------------------------------------------------------- */
    poDS->nRasterXSize = poDS->nRasterYSize = 0;
    int nBandCount = 0;

    while( poDS->ReadPair( osObjName, nObjCode ) && nObjCode != 254 )
    {
        if( osObjName == "dim" && nObjCode % 256 == R_INTSXP )
        {
            int nCount = poDS->ReadInteger();
            if( nCount == 2 )
            {
                poDS->nRasterXSize = poDS->ReadInteger();
                poDS->nRasterYSize = poDS->ReadInteger();
                nBandCount = 1;
            }
            else if( nCount == 3 )
            {
                poDS->nRasterXSize = poDS->ReadInteger();
                poDS->nRasterYSize = poDS->ReadInteger();
                nBandCount = poDS->ReadInteger();
            }
            else
            {
                CPLError( CE_Failure, CPLE_AppDefined,
                          "R 'dim' dimension wrong." );
                delete poDS;
                return NULL;
            }
        }
        else if( nObjCode % 256 == R_REALSXP )
        {
            int nCount = poDS->ReadInteger();
            while( nCount-- > 0 && !VSIFEofL(poDS->fp) )
                poDS->ReadFloat();
        }
        else if( nObjCode % 256 == R_INTSXP )
        {
            int nCount = poDS->ReadInteger();
            while( nCount-- > 0 && !VSIFEofL(poDS->fp) )
                poDS->ReadInteger();
        }
        else if( nObjCode % 256 == R_STRSXP )			
        {
            int nCount = poDS->ReadInteger();
            while( nCount-- > 0 && !VSIFEofL(poDS->fp) )
                poDS->ReadString();
        }
        else if( nObjCode % 256 == R_CHARSXP )			
        {
            poDS->ReadString();
        }
    }

    if( poDS->nRasterXSize == 0 )
    {
        delete poDS;
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Failed to find dim dimension information for R dataset." );
        return NULL;
    }

    if (!GDALCheckDatasetDimensions(poDS->nRasterXSize, poDS->nRasterYSize) ||
        !GDALCheckBandCount(nBandCount, TRUE))
    {
        delete poDS;
        return NULL;
    }

    if( nValueCount 
        < ((GIntBig) nBandCount) * poDS->nRasterXSize * poDS->nRasterYSize )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Not enough pixel data." );
        delete poDS;
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Create the raster band object(s).                               */
/* -------------------------------------------------------------------- */
    for( int iBand = 0; iBand < nBandCount; iBand++ )
    {
        GDALRasterBand *poBand;

        if( poDS->bASCII )
            poBand = new RRasterBand( poDS, iBand+1, 
                                      poDS->padfMatrixValues + iBand * poDS->nRasterXSize * poDS->nRasterYSize );
        else
            poBand = new RawRasterBand( poDS, iBand+1, poDS->fp,
                                        poDS->nStartOfData 
                                        + poDS->nRasterXSize*poDS->nRasterYSize*8*iBand, 
                                        8, poDS->nRasterXSize * 8,
                                        GDT_Float64, !CPL_IS_LSB,
                                        TRUE, FALSE );
                                      
        poDS->SetBand( iBand+1, poBand );
    }

/* -------------------------------------------------------------------- */
/*      Initialize any PAM information.                                 */
/* -------------------------------------------------------------------- */
    poDS->SetDescription( poOpenInfo->pszFilename );
    poDS->TryLoadXML();

/* -------------------------------------------------------------------- */
/*      Check for overviews.                                            */
/* -------------------------------------------------------------------- */
    poDS->oOvManager.Initialize( poDS, poOpenInfo->pszFilename );

    return( poDS );
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
        poDriver->SetMetadataItem( GDAL_DCAP_RASTER, "YES" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME,
                                   "R Object Data Store" );
        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC,
                                   "frmt_r.html" );
        poDriver->SetMetadataItem( GDAL_DMD_EXTENSION, "rda" );
        poDriver->SetMetadataItem( GDAL_DMD_CREATIONDATATYPES, "Float32" );
        poDriver->SetMetadataItem( GDAL_DMD_CREATIONOPTIONLIST,
"<CreationOptionList>"
"   <Option name='ASCII' type='boolean' description='For ASCII output, default NO'/>"
"   <Option name='COMPRESS' type='boolean' description='Produced Compressed output, default YES'/>"
"</CreationOptionList>" );

        poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );

        poDriver->pfnOpen = RDataset::Open;
        poDriver->pfnIdentify = RDataset::Identify;
        poDriver->pfnCreateCopy = RCreateCopy;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}
