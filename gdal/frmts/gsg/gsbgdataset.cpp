/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  Implements the Golden Software Binary Grid Format.
 * Author:   Kevin Locke, kwl7@cornell.edu
 *           (Based largely on aaigriddataset.cpp by Frank Warmerdam)
 *
 ******************************************************************************
 * Copyright (c) 2006, Kevin Locke <kwl7@cornell.edu>
 * Copyright (c) 2008-2012, Even Rouault <even dot rouault at mines-paris dot org>
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

#include "cpl_conv.h"

#include <float.h>
#include <limits.h>
#include <assert.h>

#include "gdal_frmts.h"
#include "gdal_pam.h"

CPL_CVSID("$Id$")

#ifndef DBL_MAX
# ifdef __DBL_MAX__
#  define DBL_MAX __DBL_MAX__
# else
#  define DBL_MAX 1.7976931348623157E+308
# endif /* __DBL_MAX__ */
#endif /* DBL_MAX */

#ifndef FLT_MAX
# ifdef __FLT_MAX__
#  define FLT_MAX __FLT_MAX__
# else
#  define FLT_MAX 3.40282347E+38F
# endif /* __FLT_MAX__ */
#endif /* FLT_MAX */

#ifndef INT_MAX
# define INT_MAX 2147483647
#endif /* INT_MAX */

#ifndef SHRT_MAX
# define SHRT_MAX 32767
#endif /* SHRT_MAX */

/************************************************************************/
/* ==================================================================== */
/*                              GSBGDataset                             */
/* ==================================================================== */
/************************************************************************/

class GSBGRasterBand;

class GSBGDataset : public GDALPamDataset
{
    friend class GSBGRasterBand;

    static const float fNODATA_VALUE;
    static const size_t nHEADER_SIZE;

    static CPLErr WriteHeader( VSILFILE *fp, GInt16 nXSize, GInt16 nYSize,
                               double dfMinX, double dfMaxX,
                               double dfMinY, double dfMaxY,
                               double dfMinZ, double dfMaxZ );

    VSILFILE    *fp;

  public:
                 GSBGDataset() : fp(NULL) {}
                ~GSBGDataset();

    static int          Identify( GDALOpenInfo * );
    static GDALDataset *Open( GDALOpenInfo * );
    static GDALDataset *Create( const char * pszFilename,
                                int nXSize, int nYSize, int nBands,
                                GDALDataType eType,
                                char **papszParmList );
    static GDALDataset *CreateCopy( const char *pszFilename,
                                    GDALDataset *poSrcDS,
                                    int bStrict, char **papszOptions,
                                    GDALProgressFunc pfnProgress,
                                    void *pProgressData );

    CPLErr GetGeoTransform( double *padfGeoTransform ) override;
    CPLErr SetGeoTransform( double *padfGeoTransform ) override;
};

/* NOTE:  This is not mentioned in the spec, but Surfer 8 uses this value */
/* 0x7effffee (Little Endian: eeffff7e) */
const float GSBGDataset::fNODATA_VALUE = 1.701410009187828e+38f;

const size_t GSBGDataset::nHEADER_SIZE = 56;

/************************************************************************/
/* ==================================================================== */
/*                            GSBGRasterBand                            */
/* ==================================================================== */
/************************************************************************/

class GSBGRasterBand : public GDALPamRasterBand
{
    friend class GSBGDataset;

    double dfMinX;
    double dfMaxX;
    double dfMinY;
    double dfMaxY;
    double dfMinZ;
    double dfMaxZ;

    float *pafRowMinZ;
    float *pafRowMaxZ;
    int nMinZRow;
    int nMaxZRow;

    CPLErr ScanForMinMaxZ();

  public:

                GSBGRasterBand( GSBGDataset *, int );
                ~GSBGRasterBand();

    CPLErr IReadBlock( int, int, void * ) override;
    CPLErr IWriteBlock( int, int, void * ) override;

    double GetNoDataValue( int *pbSuccess = NULL ) override;
    double GetMinimum( int *pbSuccess = NULL ) override;
    double GetMaximum( int *pbSuccess = NULL ) override;
};

/************************************************************************/
/*                           GSBGRasterBand()                           */
/************************************************************************/

GSBGRasterBand::GSBGRasterBand( GSBGDataset *poDSIn, int nBandIn ) :
    dfMinX(0.0),
    dfMaxX(0.0),
    dfMinY(0.0),
    dfMaxY(0.0),
    dfMinZ(0.0),
    dfMaxZ(0.0),
    pafRowMinZ(NULL),
    pafRowMaxZ(NULL),
    nMinZRow(-1),
    nMaxZRow(-1)
{
    this->poDS = poDSIn;
    this->nBand = nBandIn;

    eDataType = GDT_Float32;

    nBlockXSize = poDS->GetRasterXSize();
    nBlockYSize = 1;
}

/************************************************************************/
/*                           ~GSBGRasterBand()                          */
/************************************************************************/

GSBGRasterBand::~GSBGRasterBand( )

{
    if( pafRowMinZ != NULL )
        CPLFree( pafRowMinZ );
    if( pafRowMaxZ != NULL )
        CPLFree( pafRowMaxZ );
}

/************************************************************************/
/*                          ScanForMinMaxZ()                            */
/************************************************************************/

CPLErr GSBGRasterBand::ScanForMinMaxZ()

{
    float *pafRowVals = (float *)VSI_MALLOC2_VERBOSE( nRasterXSize, 4 );

    if( pafRowVals == NULL )
    {
        return CE_Failure;
    }

    double dfNewMinZ = DBL_MAX;
    double dfNewMaxZ = -DBL_MAX;
    int nNewMinZRow = 0;
    int nNewMaxZRow = 0;

    /* Since we have to scan, lets calc. statistics too */
    double dfSum = 0.0;
    double dfSum2 = 0.0;
    unsigned long nValuesRead = 0;
    for( int iRow=0; iRow<nRasterYSize; iRow++ )
    {
        CPLErr eErr = IReadBlock( 0, iRow, pafRowVals );
        if( eErr != CE_None )
        {
            VSIFree( pafRowVals );
            return CE_Failure;
        }

        pafRowMinZ[iRow] = FLT_MAX;
        pafRowMaxZ[iRow] = -FLT_MAX;
        for( int iCol=0; iCol<nRasterXSize; iCol++ )
        {
            if( pafRowVals[iCol] == GSBGDataset::fNODATA_VALUE )
                continue;

            if( pafRowVals[iCol] < pafRowMinZ[iRow] )
                pafRowMinZ[iRow] = pafRowVals[iCol];

            if( pafRowVals[iCol] > pafRowMinZ[iRow] )
                pafRowMaxZ[iRow] = pafRowVals[iCol];

            dfSum += pafRowVals[iCol];
            dfSum2 += pafRowVals[iCol] * pafRowVals[iCol];
            nValuesRead++;
        }

        if( pafRowMinZ[iRow] < dfNewMinZ )
        {
            dfNewMinZ = pafRowMinZ[iRow];
            nNewMinZRow = iRow;
        }

        if( pafRowMaxZ[iRow] > dfNewMaxZ )
        {
            dfNewMaxZ = pafRowMaxZ[iRow];
            nNewMaxZRow = iRow;
        }
    }

    VSIFree( pafRowVals );

    if( nValuesRead == 0 )
    {
        dfMinZ = 0.0;
        dfMaxZ = 0.0;
        nMinZRow = 0;
        nMaxZRow = 0;
        return CE_None;
    }

    dfMinZ = dfNewMinZ;
    dfMaxZ = dfNewMaxZ;
    nMinZRow = nNewMinZRow;
    nMaxZRow = nNewMaxZRow;

    double dfMean = dfSum / nValuesRead;
    double dfStdDev = sqrt((dfSum2 / nValuesRead) - (dfMean * dfMean));
    SetStatistics( dfMinZ, dfMaxZ, dfMean, dfStdDev );

    return CE_None;
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr GSBGRasterBand::IReadBlock( int nBlockXOff, int nBlockYOff,
                                   void * pImage )

{
    if( nBlockYOff < 0 || nBlockYOff > nRasterYSize - 1 || nBlockXOff != 0 )
        return CE_Failure;

    GSBGDataset *poGDS = reinterpret_cast<GSBGDataset *>(poDS);
    if( VSIFSeekL( poGDS->fp,
                   GSBGDataset::nHEADER_SIZE +
                        4 * static_cast<vsi_l_offset>(nRasterXSize) * (nRasterYSize - nBlockYOff - 1),
                   SEEK_SET ) != 0 )
    {
        CPLError( CE_Failure, CPLE_FileIO,
                  "Unable to seek to beginning of grid row.\n" );
        return CE_Failure;
    }

    if( VSIFReadL( pImage, sizeof(float), nBlockXSize,
                   poGDS->fp ) != static_cast<unsigned>(nBlockXSize) )
    {
        CPLError( CE_Failure, CPLE_FileIO,
                  "Unable to read block from grid file.\n" );
        return CE_Failure;
    }

#ifdef CPL_MSB
    float *pfImage = (float *)pImage;
    for( int iPixel=0; iPixel<nBlockXSize; iPixel++ ) {
        CPL_LSBPTR32( pfImage+iPixel );
    }
#endif

    return CE_None;
}

/************************************************************************/
/*                            IWriteBlock()                             */
/************************************************************************/

CPLErr GSBGRasterBand::IWriteBlock( int nBlockXOff, int nBlockYOff,
                                    void *pImage )

{
    if( eAccess == GA_ReadOnly )
    {
        CPLError( CE_Failure, CPLE_NoWriteAccess,
                  "Unable to write block, dataset opened read only.\n" );
        return CE_Failure;
    }

    if( nBlockYOff < 0 || nBlockYOff > nRasterYSize - 1 || nBlockXOff != 0 )
        return CE_Failure;

    GSBGDataset *poGDS = dynamic_cast<GSBGDataset *>(poDS);
    assert( poGDS != NULL );

    if( pafRowMinZ == NULL || pafRowMaxZ == NULL
        || nMinZRow < 0 || nMaxZRow < 0 )
    {
        pafRowMinZ = (float *)VSI_MALLOC2_VERBOSE( nRasterYSize,sizeof(float) );
        if( pafRowMinZ == NULL )
        {
            return CE_Failure;
        }

        pafRowMaxZ = (float *)VSI_MALLOC2_VERBOSE( nRasterYSize,sizeof(float) );
        if( pafRowMaxZ == NULL )
        {
            VSIFree( pafRowMinZ );
            pafRowMinZ = NULL;
            return CE_Failure;
        }

        CPLErr eErr = ScanForMinMaxZ();
        if( eErr != CE_None )
            return eErr;
    }

    if( VSIFSeekL( poGDS->fp,
                   GSBGDataset::nHEADER_SIZE +
                        4 * nRasterXSize * (nRasterYSize - nBlockYOff - 1),
                   SEEK_SET ) != 0 )
    {
        CPLError( CE_Failure, CPLE_FileIO,
                  "Unable to seek to beginning of grid row.\n" );
        return CE_Failure;
    }

    float *pfImage = (float *)pImage;
    pafRowMinZ[nBlockYOff] = FLT_MAX;
    pafRowMaxZ[nBlockYOff] = -FLT_MAX;
    for( int iPixel=0; iPixel<nBlockXSize; iPixel++ )
    {
        if( pfImage[iPixel] != GSBGDataset::fNODATA_VALUE )
        {
            if( pfImage[iPixel] < pafRowMinZ[nBlockYOff] )
                pafRowMinZ[nBlockYOff] = pfImage[iPixel];

            if( pfImage[iPixel] > pafRowMaxZ[nBlockYOff] )
                pafRowMaxZ[nBlockYOff] = pfImage[iPixel];
        }

        CPL_LSBPTR32( pfImage+iPixel );
    }

    if( VSIFWriteL( pImage, sizeof(float), nBlockXSize,
                    poGDS->fp ) != static_cast<unsigned>(nBlockXSize) )
    {
        CPLError( CE_Failure, CPLE_FileIO,
                  "Unable to write block to grid file.\n" );
        return CE_Failure;
    }

    /* Update min/max Z values as appropriate */
    bool bHeaderNeedsUpdate = false;
    if( nMinZRow == nBlockYOff && pafRowMinZ[nBlockYOff] > dfMinZ )
    {
        double dfNewMinZ = DBL_MAX;
        for( int iRow=0; iRow<nRasterYSize; iRow++ )
        {
            if( pafRowMinZ[iRow] < dfNewMinZ )
            {
                dfNewMinZ = pafRowMinZ[iRow];
                nMinZRow = iRow;
            }
        }

        if( dfNewMinZ != dfMinZ )
        {
            dfMinZ = dfNewMinZ;
            bHeaderNeedsUpdate = true;
        }
    }

    if( nMaxZRow == nBlockYOff && pafRowMaxZ[nBlockYOff] < dfMaxZ )
    {
        double dfNewMaxZ = -DBL_MAX;
        for( int iRow=0; iRow<nRasterYSize; iRow++ )
        {
            if( pafRowMaxZ[iRow] > dfNewMaxZ )
            {
                dfNewMaxZ = pafRowMaxZ[iRow];
                nMaxZRow = iRow;
            }
        }

        if( dfNewMaxZ != dfMaxZ )
        {
            dfMaxZ = dfNewMaxZ;
            bHeaderNeedsUpdate = true;
        }
    }

    if( pafRowMinZ[nBlockYOff] < dfMinZ || pafRowMaxZ[nBlockYOff] > dfMaxZ )
    {
        if( pafRowMinZ[nBlockYOff] < dfMinZ )
        {
            dfMinZ = pafRowMinZ[nBlockYOff];
            nMinZRow = nBlockYOff;
        }

        if( pafRowMaxZ[nBlockYOff] > dfMaxZ )
        {
            dfMaxZ = pafRowMaxZ[nBlockYOff];
            nMaxZRow = nBlockYOff;
        }

        bHeaderNeedsUpdate = true;
    }

    if( bHeaderNeedsUpdate && dfMaxZ > dfMinZ )
    {
        CPLErr eErr = poGDS->WriteHeader( poGDS->fp,
                                          (GInt16) nRasterXSize,
                                          (GInt16) nRasterYSize,
                                          dfMinX, dfMaxX,
                                          dfMinY, dfMaxY,
                                          dfMinZ, dfMaxZ );
        return eErr;
    }

    return CE_None;
}

/************************************************************************/
/*                           GetNoDataValue()                           */
/************************************************************************/

double GSBGRasterBand::GetNoDataValue( int * pbSuccess )
{
    if( pbSuccess )
        *pbSuccess = TRUE;

    return GSBGDataset::fNODATA_VALUE;
}

/************************************************************************/
/*                             GetMinimum()                             */
/************************************************************************/

double GSBGRasterBand::GetMinimum( int *pbSuccess )
{
    if( pbSuccess )
        *pbSuccess = TRUE;

    return dfMinZ;
}

/************************************************************************/
/*                             GetMaximum()                             */
/************************************************************************/

double GSBGRasterBand::GetMaximum( int *pbSuccess )
{
    if( pbSuccess )
        *pbSuccess = TRUE;

    return dfMaxZ;
}

/************************************************************************/
/* ==================================================================== */
/*                              GSBGDataset                             */
/* ==================================================================== */
/************************************************************************/

GSBGDataset::~GSBGDataset()

{
    FlushCache();
    if( fp != NULL )
        VSIFCloseL( fp );
}

/************************************************************************/
/*                              Identify()                              */
/************************************************************************/

int GSBGDataset::Identify( GDALOpenInfo * poOpenInfo )

{
    /* Check for signature */
    if( poOpenInfo->nHeaderBytes < 4
        || !STARTS_WITH_CI((const char *) poOpenInfo->pabyHeader, "DSBB") )
    {
        return FALSE;
    }

    return TRUE;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *GSBGDataset::Open( GDALOpenInfo * poOpenInfo )

{
    if( !Identify(poOpenInfo) )
    {
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */
    GSBGDataset *poDS = new GSBGDataset();

/* -------------------------------------------------------------------- */
/*      Open file with large file API.                                  */
/* -------------------------------------------------------------------- */
    poDS->eAccess = poOpenInfo->eAccess;
    if( poOpenInfo->eAccess == GA_ReadOnly )
        poDS->fp = VSIFOpenL( poOpenInfo->pszFilename, "rb" );
    else
        poDS->fp = VSIFOpenL( poOpenInfo->pszFilename, "r+b" );

    if( poDS->fp == NULL )
    {
        delete poDS;
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "VSIFOpenL(%s) failed unexpectedly.",
                  poOpenInfo->pszFilename );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Read the header.                                                */
/* -------------------------------------------------------------------- */
    if( VSIFSeekL( poDS->fp, 4, SEEK_SET ) != 0 )
    {
        delete poDS;
        CPLError( CE_Failure, CPLE_FileIO,
                  "Unable to seek to start of grid file header.\n" );
        return NULL;
    }

    /* Parse number of X axis grid rows */
    GInt16 nTemp;
    if( VSIFReadL( (void *)&nTemp, 2, 1, poDS->fp ) != 1 )
    {
        delete poDS;
        CPLError( CE_Failure, CPLE_FileIO, "Unable to read raster X size.\n" );
        return NULL;
    }
    poDS->nRasterXSize = CPL_LSBWORD16( nTemp );

    if( VSIFReadL( (void *)&nTemp, 2, 1, poDS->fp ) != 1 )
    {
        delete poDS;
        CPLError( CE_Failure, CPLE_FileIO, "Unable to read raster Y size.\n" );
        return NULL;
    }
    poDS->nRasterYSize = CPL_LSBWORD16( nTemp );

    if (!GDALCheckDatasetDimensions(poDS->nRasterXSize, poDS->nRasterYSize))
    {
        delete poDS;
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Create band information objects.                                */
/* -------------------------------------------------------------------- */
    GSBGRasterBand *poBand = new GSBGRasterBand( poDS, 1 );

    double dfTemp;
    if( VSIFReadL( (void *)&dfTemp, 8, 1, poDS->fp ) != 1 )
    {
        delete poDS;
        delete poBand;
        CPLError( CE_Failure, CPLE_FileIO,
                  "Unable to read minimum X value.\n" );
        return NULL;
    }
    CPL_LSBPTR64( &dfTemp );
    poBand->dfMinX = dfTemp;

    if( VSIFReadL( (void *)&dfTemp, 8, 1, poDS->fp ) != 1 )
    {
        delete poDS;
        delete poBand;
        CPLError( CE_Failure, CPLE_FileIO,
                  "Unable to read maximum X value.\n" );
        return NULL;
    }
    CPL_LSBPTR64( &dfTemp );
    poBand->dfMaxX = dfTemp;

    if( VSIFReadL( (void *)&dfTemp, 8, 1, poDS->fp ) != 1 )
    {
        delete poDS;
        delete poBand;
        CPLError( CE_Failure, CPLE_FileIO,
                  "Unable to read minimum Y value.\n" );
        return NULL;
    }
    CPL_LSBPTR64( &dfTemp );
    poBand->dfMinY = dfTemp;

    if( VSIFReadL( (void *)&dfTemp, 8, 1, poDS->fp ) != 1 )
    {
        delete poDS;
        delete poBand;
        CPLError( CE_Failure, CPLE_FileIO,
                  "Unable to read maximum Y value.\n" );
        return NULL;
    }
    CPL_LSBPTR64( &dfTemp );
    poBand->dfMaxY = dfTemp;

    if( VSIFReadL( (void *)&dfTemp, 8, 1, poDS->fp ) != 1 )
    {
        delete poDS;
        delete poBand;
        CPLError( CE_Failure, CPLE_FileIO,
                  "Unable to read minimum Z value.\n" );
        return NULL;
    }
    CPL_LSBPTR64( &dfTemp );
    poBand->dfMinZ = dfTemp;

    if( VSIFReadL( (void *)&dfTemp, 8, 1, poDS->fp ) != 1 )
    {
        delete poDS;
        delete poBand;
        CPLError( CE_Failure, CPLE_FileIO,
                  "Unable to read maximum Z value.\n" );
        return NULL;
    }
    CPL_LSBPTR64( &dfTemp );
    poBand->dfMaxZ = dfTemp;

    poDS->SetBand( 1, poBand );

/* -------------------------------------------------------------------- */
/*      Initialize any PAM information.                                 */
/* -------------------------------------------------------------------- */
    poDS->SetDescription( poOpenInfo->pszFilename );
    poDS->TryLoadXML();

/* -------------------------------------------------------------------- */
/*      Check for external overviews.                                   */
/* -------------------------------------------------------------------- */
    poDS->oOvManager.Initialize( poDS, poOpenInfo->pszFilename, poOpenInfo->GetSiblingFiles() );

    return poDS;
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr GSBGDataset::GetGeoTransform( double *padfGeoTransform )
{
    if( padfGeoTransform == NULL )
        return CE_Failure;

    GSBGRasterBand *poGRB = dynamic_cast<GSBGRasterBand *>(GetRasterBand( 1 ));

    if( poGRB == NULL )
    {
        padfGeoTransform[0] = 0;
        padfGeoTransform[1] = 1;
        padfGeoTransform[2] = 0;
        padfGeoTransform[3] = 0;
        padfGeoTransform[4] = 0;
        padfGeoTransform[5] = 1;
        return CE_Failure;
    }

    /* check if we have a PAM GeoTransform stored */
    CPLPushErrorHandler( CPLQuietErrorHandler );
    CPLErr eErr = GDALPamDataset::GetGeoTransform( padfGeoTransform );
    CPLPopErrorHandler();

    if( eErr == CE_None )
        return CE_None;

    if( nRasterXSize == 1 || nRasterYSize == 1 )
        return CE_Failure;

    /* calculate pixel size first */
    padfGeoTransform[1] = (poGRB->dfMaxX - poGRB->dfMinX)/(nRasterXSize - 1);
    padfGeoTransform[5] = (poGRB->dfMinY - poGRB->dfMaxY)/(nRasterYSize - 1);

    /* then calculate image origin */
    padfGeoTransform[0] = poGRB->dfMinX - padfGeoTransform[1] / 2;
    padfGeoTransform[3] = poGRB->dfMaxY - padfGeoTransform[5] / 2;

    /* tilt/rotation does not supported by the GS grids */
    padfGeoTransform[4] = 0.0;
    padfGeoTransform[2] = 0.0;

    return CE_None;
}

/************************************************************************/
/*                          SetGeoTransform()                           */
/************************************************************************/

CPLErr GSBGDataset::SetGeoTransform( double *padfGeoTransform )
{
    if( eAccess == GA_ReadOnly )
    {
        CPLError( CE_Failure, CPLE_NoWriteAccess,
                  "Unable to set GeoTransform, dataset opened read only.\n" );
        return CE_Failure;
    }

    GSBGRasterBand *poGRB = dynamic_cast<GSBGRasterBand *>(GetRasterBand( 1 ));

    if( poGRB == NULL || padfGeoTransform == NULL)
        return CE_Failure;

    /* non-zero transform 2 or 4 or negative 1 or 5 not supported natively */
    CPLErr eErr = CE_None;
    /*if( padfGeoTransform[2] != 0.0 || padfGeoTransform[4] != 0.0
        || padfGeoTransform[1] < 0.0 || padfGeoTransform[5] < 0.0 )
        eErr = GDALPamDataset::SetGeoTransform( padfGeoTransform );

    if( eErr != CE_None )
        return eErr;*/

    double dfMinX = padfGeoTransform[0] + padfGeoTransform[1] / 2;
    double dfMaxX =
        padfGeoTransform[1] * (nRasterXSize - 0.5) + padfGeoTransform[0];
    double dfMinY =
        padfGeoTransform[5] * (nRasterYSize - 0.5) + padfGeoTransform[3];
    double dfMaxY = padfGeoTransform[3] + padfGeoTransform[5] / 2;

    eErr = WriteHeader( fp,
                        (GInt16) poGRB->nRasterXSize,
                        (GInt16) poGRB->nRasterYSize,
                        dfMinX, dfMaxX, dfMinY, dfMaxY,
                        poGRB->dfMinZ, poGRB->dfMaxZ );

    if( eErr == CE_None )
    {
        poGRB->dfMinX = dfMinX;
        poGRB->dfMaxX = dfMaxX;
        poGRB->dfMinY = dfMinY;
        poGRB->dfMaxY = dfMaxY;
    }

    return eErr;
}

/************************************************************************/
/*                             WriteHeader()                            */
/************************************************************************/

CPLErr GSBGDataset::WriteHeader( VSILFILE *fp, GInt16 nXSize, GInt16 nYSize,
                                 double dfMinX, double dfMaxX,
                                 double dfMinY, double dfMaxY,
                                 double dfMinZ, double dfMaxZ )

{
    if( VSIFSeekL( fp, 0, SEEK_SET ) != 0 )
    {
        CPLError( CE_Failure, CPLE_FileIO,
                  "Unable to seek to start of grid file.\n" );
        return CE_Failure;
    }

    if( VSIFWriteL( (void *)"DSBB", 1, 4, fp ) != 4 )
    {
        CPLError( CE_Failure, CPLE_FileIO,
                  "Unable to write signature to grid file.\n" );
        return CE_Failure;
    }

    GInt16 nTemp = CPL_LSBWORD16(nXSize);
    if( VSIFWriteL( (void *)&nTemp, 2, 1, fp ) != 1 )
    {
        CPLError( CE_Failure, CPLE_FileIO,
                  "Unable to write raster X size to grid file.\n" );
        return CE_Failure;
    }

    nTemp = CPL_LSBWORD16(nYSize);
    if( VSIFWriteL( (void *)&nTemp, 2, 1, fp ) != 1 )
    {
        CPLError( CE_Failure, CPLE_FileIO,
                  "Unable to write raster Y size to grid file.\n" );
        return CE_Failure;
    }

    double dfTemp = dfMinX;
    CPL_LSBPTR64( &dfTemp );
    if( VSIFWriteL( (void *)&dfTemp, 8, 1, fp ) != 1 )
    {
        CPLError( CE_Failure, CPLE_FileIO,
                  "Unable to write minimum X value to grid file.\n" );
        return CE_Failure;
    }

    dfTemp = dfMaxX;
    CPL_LSBPTR64( &dfTemp );
    if( VSIFWriteL( (void *)&dfTemp, 8, 1, fp ) != 1 )
    {
        CPLError( CE_Failure, CPLE_FileIO,
                  "Unable to write maximum X value to grid file.\n" );
        return CE_Failure;
    }

    dfTemp = dfMinY;
    CPL_LSBPTR64( &dfTemp );
    if( VSIFWriteL( (void *)&dfTemp, 8, 1, fp ) != 1 )
    {
        CPLError( CE_Failure, CPLE_FileIO,
                  "Unable to write minimum Y value to grid file.\n" );
        return CE_Failure;
    }

    dfTemp = dfMaxY;
    CPL_LSBPTR64( &dfTemp );
    if( VSIFWriteL( (void *)&dfTemp, 8, 1, fp ) != 1 )
    {
        CPLError( CE_Failure, CPLE_FileIO,
                  "Unable to write maximum Y value to grid file.\n" );
        return CE_Failure;
    }

    dfTemp = dfMinZ;
    CPL_LSBPTR64( &dfTemp );
    if( VSIFWriteL( (void *)&dfTemp, 8, 1, fp ) != 1 )
    {
        CPLError( CE_Failure, CPLE_FileIO,
                  "Unable to write minimum Z value to grid file.\n" );
        return CE_Failure;
    }

    dfTemp = dfMaxZ;
    CPL_LSBPTR64( &dfTemp );
    if( VSIFWriteL( (void *)&dfTemp, 8, 1, fp ) != 1 )
    {
        CPLError( CE_Failure, CPLE_FileIO,
                  "Unable to write maximum Z value to grid file.\n" );
        return CE_Failure;
    }

    return CE_None;
}

/************************************************************************/
/*                               Create()                               */
/************************************************************************/

GDALDataset *GSBGDataset::Create( const char * pszFilename,
                                  int nXSize,
                                  int nYSize,
                                  CPL_UNUSED int nBands,
                                  GDALDataType eType,
                                  CPL_UNUSED char **papszParmList )
{
    if( nXSize <= 0 || nYSize <= 0 )
    {
        CPLError( CE_Failure, CPLE_IllegalArg,
                  "Unable to create grid, both X and Y size must be "
                  "non-negative.\n" );

        return NULL;
    }
    else if( nXSize > SHRT_MAX
             || nYSize > SHRT_MAX )
    {
        CPLError( CE_Failure, CPLE_IllegalArg,
                  "Unable to create grid, Golden Software Binary Grid format "
                  "only supports sizes up to %dx%d.  %dx%d not supported.\n",
                  SHRT_MAX, SHRT_MAX, nXSize, nYSize );

        return NULL;
    }

    if( eType != GDT_Byte && eType != GDT_Float32 && eType != GDT_UInt16
        && eType != GDT_Int16 )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Golden Software Binary Grid only supports Byte, Int16, "
                  "Uint16, and Float32 datatypes.  Unable to create with "
                  "type %s.\n", GDALGetDataTypeName( eType ) );

        return NULL;
    }

    VSILFILE *fp = VSIFOpenL( pszFilename, "w+b" );

    if( fp == NULL )
    {
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "Attempt to create file '%s' failed.\n",
                  pszFilename );
        return NULL;
    }

    CPLErr eErr = WriteHeader( fp, (GInt16) nXSize, (GInt16) nYSize,
                               0.0, nXSize, 0.0, nYSize, 0.0, 0.0 );
    if( eErr != CE_None )
    {
        VSIFCloseL( fp );
        return NULL;
    }

    float fVal = fNODATA_VALUE;
    CPL_LSBPTR32( &fVal );
    for( int iRow = 0; iRow < nYSize; iRow++ )
    {
        for( int iCol=0; iCol<nXSize; iCol++ )
        {
            if( VSIFWriteL( (void *)&fVal, 4, 1, fp ) != 1 )
            {
                VSIFCloseL( fp );
                CPLError( CE_Failure, CPLE_FileIO,
                          "Unable to write grid cell.  Disk full?\n" );
                return NULL;
            }
        }
    }

    VSIFCloseL( fp );

    return (GDALDataset *)GDALOpen( pszFilename, GA_Update );
}

/************************************************************************/
/*                             CreateCopy()                             */
/************************************************************************/

GDALDataset *GSBGDataset::CreateCopy( const char *pszFilename,
                                      GDALDataset *poSrcDS,
                                      int bStrict,
                                      CPL_UNUSED char **papszOptions,
                                      GDALProgressFunc pfnProgress,
                                      void *pProgressData )
{
    if( pfnProgress == NULL )
        pfnProgress = GDALDummyProgress;

    int nBands = poSrcDS->GetRasterCount();
    if (nBands == 0)
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "GSBG driver does not support source dataset with zero band.\n");
        return NULL;
    }
    else if (nBands > 1)
    {
        if( bStrict )
        {
            CPLError( CE_Failure, CPLE_NotSupported,
                      "Unable to create copy, Golden Software Binary Grid "
                      "format only supports one raster band.\n" );
            return NULL;
        }
        else
            CPLError( CE_Warning, CPLE_NotSupported,
                      "Golden Software Binary Grid format only supports one "
                      "raster band, first band will be copied.\n" );
    }

    GDALRasterBand *poSrcBand = poSrcDS->GetRasterBand( 1 );
    if( poSrcBand->GetXSize() > SHRT_MAX
        || poSrcBand->GetYSize() > SHRT_MAX )
    {
        CPLError( CE_Failure, CPLE_IllegalArg,
                  "Unable to create grid, Golden Software Binary Grid format "
                  "only supports sizes up to %dx%d.  %dx%d not supported.\n",
                  SHRT_MAX, SHRT_MAX,
                  poSrcBand->GetXSize(), poSrcBand->GetYSize() );

        return NULL;
    }

    if( !pfnProgress( 0.0, NULL, pProgressData ) )
    {
        CPLError( CE_Failure, CPLE_UserInterrupt, "User terminated\n" );
        return NULL;
    }

    VSILFILE    *fp = VSIFOpenL( pszFilename, "w+b" );

    if( fp == NULL )
    {
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "Attempt to create file '%s' failed.\n",
                  pszFilename );
        return NULL;
    }

    GInt16  nXSize = (GInt16) poSrcBand->GetXSize();
    GInt16  nYSize = (GInt16) poSrcBand->GetYSize();
    double  adfGeoTransform[6];

    poSrcDS->GetGeoTransform( adfGeoTransform );

    double dfMinX = adfGeoTransform[0] + adfGeoTransform[1] / 2;
    double dfMaxX = adfGeoTransform[1] * (nXSize - 0.5) + adfGeoTransform[0];
    double dfMinY = adfGeoTransform[5] * (nYSize - 0.5) + adfGeoTransform[3];
    double dfMaxY = adfGeoTransform[3] + adfGeoTransform[5] / 2;
    CPLErr eErr = WriteHeader( fp, nXSize, nYSize,
                               dfMinX, dfMaxX, dfMinY, dfMaxY, 0.0, 0.0 );

    if( eErr != CE_None )
    {
        VSIFCloseL( fp );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Copy band data.                                                 */
/* -------------------------------------------------------------------- */
    float *pfData = (float *)VSI_MALLOC2_VERBOSE( nXSize, sizeof( float ) );
    if( pfData == NULL )
    {
        VSIFCloseL( fp );
        return NULL;
    }

    int     bSrcHasNDValue;
    float   fSrcNoDataValue = (float) poSrcBand->GetNoDataValue( &bSrcHasNDValue );
    double  dfMinZ = DBL_MAX;
    double  dfMaxZ = -DBL_MAX;
    for( GInt16 iRow = nYSize - 1; iRow >= 0; iRow-- )
    {
        eErr = poSrcBand->RasterIO( GF_Read, 0, iRow,
                                    nXSize, 1, pfData,
                                    nXSize, 1, GDT_Float32, 0, 0, NULL );

        if( eErr != CE_None )
        {
            VSIFCloseL( fp );
            VSIFree( pfData );
            return NULL;
        }

        for( int iCol=0; iCol<nXSize; iCol++ )
        {
            if( bSrcHasNDValue && pfData[iCol] == fSrcNoDataValue )
            {
                pfData[iCol] = fNODATA_VALUE;
            }
            else
            {
                if( pfData[iCol] > dfMaxZ )
                    dfMaxZ = pfData[iCol];

                if( pfData[iCol] < dfMinZ )
                    dfMinZ = pfData[iCol];
            }

            CPL_LSBPTR32( pfData+iCol );
        }

        if( VSIFWriteL( (void *)pfData, 4, nXSize,
                        fp ) != static_cast<unsigned>(nXSize) )
        {
            VSIFCloseL( fp );
            VSIFree( pfData );
            CPLError( CE_Failure, CPLE_FileIO,
                      "Unable to write grid row. Disk full?\n" );
            return NULL;
        }

        if( !pfnProgress( static_cast<double>(nYSize - iRow)/nYSize,
                          NULL, pProgressData ) )
        {
            VSIFCloseL( fp );
            VSIFree( pfData );
            CPLError( CE_Failure, CPLE_UserInterrupt, "User terminated" );
            return NULL;
        }
    }

    VSIFree( pfData );

    /* write out the min and max values */
    eErr = WriteHeader( fp, nXSize, nYSize,
                        dfMinX, dfMaxX, dfMinY, dfMaxY, dfMinZ, dfMaxZ );

    if( eErr != CE_None )
    {
        VSIFCloseL( fp );
        return NULL;
    }

    VSIFCloseL( fp );

    GDALPamDataset *poDS = (GDALPamDataset *)GDALOpen( pszFilename,
                                                GA_Update );
    if (poDS)
    {
        poDS->CloneInfo( poSrcDS, GCIF_PAM_DEFAULT );
    }
    return poDS;
}

/************************************************************************/
/*                          GDALRegister_GSBG()                         */
/************************************************************************/

void GDALRegister_GSBG()

{
    if( GDALGetDriverByName( "GSBG" ) != NULL )
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription( "GSBG" );
    poDriver->SetMetadataItem( GDAL_DCAP_RASTER, "YES" );
    poDriver->SetMetadataItem( GDAL_DMD_LONGNAME,
                               "Golden Software Binary Grid (.grd)" );
    poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, "frmt_various.html#GSBG" );
    poDriver->SetMetadataItem( GDAL_DMD_EXTENSION, "grd" );
    poDriver->SetMetadataItem( GDAL_DMD_CREATIONDATATYPES,
                               "Byte Int16 UInt16 Float32" );
    poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );

    poDriver->pfnIdentify = GSBGDataset::Identify;
    poDriver->pfnOpen = GSBGDataset::Open;
    poDriver->pfnCreate = GSBGDataset::Create;
    poDriver->pfnCreateCopy = GSBGDataset::CreateCopy;

    GetGDALDriverManager()->RegisterDriver( poDriver );
}
