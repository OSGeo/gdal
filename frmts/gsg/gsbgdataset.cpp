/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  Implements the Golden Software Binary Grid Format.
 * Author:   Kevin Locke, kwl7@cornell.edu
 *           (Based largely on aaigriddataset.cpp by Frank Warmerdam)
 *
 ******************************************************************************
 * Copyright (c) 2006, Kevin Locke <kwl7@cornell.edu>
 * Copyright (c) 2008-2012, Even Rouault <even dot rouault at spatialys.com>
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

#include <assert.h>
#include <float.h>
#include <limits.h>
#include <limits>

#include "gdal_frmts.h"
#include "gdal_pam.h"

CPL_CVSID("$Id$")

/************************************************************************/
/* ==================================================================== */
/*                              GSBGDataset                             */
/* ==================================================================== */
/************************************************************************/

class GSBGRasterBand;

class GSBGDataset final: public GDALPamDataset
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
                 GSBGDataset() : fp(nullptr) {}
                ~GSBGDataset();

    static int          Identify( GDALOpenInfo * );
    static GDALDataset *Open( GDALOpenInfo * );
    static GDALDataset *Create( const char * pszFilename,
                                int nXSize, int nYSize, int nBands,
                                GDALDataType eType,
                                char **papszParamList );
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

class GSBGRasterBand final: public GDALPamRasterBand
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

    double GetNoDataValue( int *pbSuccess = nullptr ) override;
    double GetMinimum( int *pbSuccess = nullptr ) override;
    double GetMaximum( int *pbSuccess = nullptr ) override;
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
    pafRowMinZ(nullptr),
    pafRowMaxZ(nullptr),
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
    if( pafRowMinZ != nullptr )
        CPLFree( pafRowMinZ );
    if( pafRowMaxZ != nullptr )
        CPLFree( pafRowMaxZ );
}

/************************************************************************/
/*                          ScanForMinMaxZ()                            */
/************************************************************************/

CPLErr GSBGRasterBand::ScanForMinMaxZ()

{
    float *pafRowVals = (float *)VSI_MALLOC2_VERBOSE( nRasterXSize, 4 );

    if( pafRowVals == nullptr )
    {
        return CE_Failure;
    }

    double dfNewMinZ = std::numeric_limits<double>::max();
    double dfNewMaxZ = std::numeric_limits<double>::lowest();
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

        pafRowMinZ[iRow] = std::numeric_limits<float>::max();
        pafRowMaxZ[iRow] = std::numeric_limits<float>::lowest();
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

    GSBGDataset *poGDS = cpl::down_cast<GSBGDataset *>(poDS);

    if( pafRowMinZ == nullptr || pafRowMaxZ == nullptr
        || nMinZRow < 0 || nMaxZRow < 0 )
    {
        pafRowMinZ = (float *)VSI_MALLOC2_VERBOSE( nRasterYSize,sizeof(float) );
        if( pafRowMinZ == nullptr )
        {
            return CE_Failure;
        }

        pafRowMaxZ = (float *)VSI_MALLOC2_VERBOSE( nRasterYSize,sizeof(float) );
        if( pafRowMaxZ == nullptr )
        {
            VSIFree( pafRowMinZ );
            pafRowMinZ = nullptr;
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
    pafRowMinZ[nBlockYOff] = std::numeric_limits<float>::max();
    pafRowMaxZ[nBlockYOff] = std::numeric_limits<float>::lowest();
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
        double dfNewMinZ = std::numeric_limits<double>::max();
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
        double dfNewMaxZ = std::numeric_limits<double>::lowest();
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
    FlushCache(true);
    if( fp != nullptr )
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
    if( !Identify(poOpenInfo) || poOpenInfo->fpL == nullptr )
    {
        return nullptr;
    }

/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */
    auto poDS = cpl::make_unique<GSBGDataset>();

    poDS->eAccess = poOpenInfo->eAccess;
    poDS->fp = poOpenInfo->fpL;
    poOpenInfo->fpL = nullptr;

/* -------------------------------------------------------------------- */
/*      Read the header.                                                */
/* -------------------------------------------------------------------- */
    if( VSIFSeekL( poDS->fp, 4, SEEK_SET ) != 0 )
    {
        CPLError( CE_Failure, CPLE_FileIO,
                  "Unable to seek to start of grid file header.\n" );
        return nullptr;
    }

    /* Parse number of X axis grid rows */
    GInt16 nTemp;
    if( VSIFReadL( (void *)&nTemp, 2, 1, poDS->fp ) != 1 )
    {
        CPLError( CE_Failure, CPLE_FileIO, "Unable to read raster X size.\n" );
        return nullptr;
    }
    poDS->nRasterXSize = CPL_LSBWORD16( nTemp );

    if( VSIFReadL( (void *)&nTemp, 2, 1, poDS->fp ) != 1 )
    {
        CPLError( CE_Failure, CPLE_FileIO, "Unable to read raster Y size.\n" );
        return nullptr;
    }
    poDS->nRasterYSize = CPL_LSBWORD16( nTemp );

    if (!GDALCheckDatasetDimensions(poDS->nRasterXSize, poDS->nRasterYSize))
    {
        return nullptr;
    }

/* -------------------------------------------------------------------- */
/*      Create band information objects.                                */
/* -------------------------------------------------------------------- */
    GSBGRasterBand *poBand = new GSBGRasterBand( poDS.get(), 1 );
    poDS->SetBand( 1, poBand );

    double dfTemp;
    if( VSIFReadL( (void *)&dfTemp, 8, 1, poDS->fp ) != 1 )
    {
        CPLError( CE_Failure, CPLE_FileIO,
                  "Unable to read minimum X value.\n" );
        return nullptr;
    }
    CPL_LSBPTR64( &dfTemp );
    poBand->dfMinX = dfTemp;

    if( VSIFReadL( (void *)&dfTemp, 8, 1, poDS->fp ) != 1 )
    {
        CPLError( CE_Failure, CPLE_FileIO,
                  "Unable to read maximum X value.\n" );
        return nullptr;
    }
    CPL_LSBPTR64( &dfTemp );
    poBand->dfMaxX = dfTemp;

    if( VSIFReadL( (void *)&dfTemp, 8, 1, poDS->fp ) != 1 )
    {
        CPLError( CE_Failure, CPLE_FileIO,
                  "Unable to read minimum Y value.\n" );
        return nullptr;
    }
    CPL_LSBPTR64( &dfTemp );
    poBand->dfMinY = dfTemp;

    if( VSIFReadL( (void *)&dfTemp, 8, 1, poDS->fp ) != 1 )
    {
        CPLError( CE_Failure, CPLE_FileIO,
                  "Unable to read maximum Y value.\n" );
        return nullptr;
    }
    CPL_LSBPTR64( &dfTemp );
    poBand->dfMaxY = dfTemp;

    if( VSIFReadL( (void *)&dfTemp, 8, 1, poDS->fp ) != 1 )
    {
        CPLError( CE_Failure, CPLE_FileIO,
                  "Unable to read minimum Z value.\n" );
        return nullptr;
    }
    CPL_LSBPTR64( &dfTemp );
    poBand->dfMinZ = dfTemp;

    if( VSIFReadL( (void *)&dfTemp, 8, 1, poDS->fp ) != 1 )
    {
        CPLError( CE_Failure, CPLE_FileIO,
                  "Unable to read maximum Z value.\n" );
        return nullptr;
    }
    CPL_LSBPTR64( &dfTemp );
    poBand->dfMaxZ = dfTemp;

/* -------------------------------------------------------------------- */
/*      Initialize any PAM information.                                 */
/* -------------------------------------------------------------------- */
    poDS->SetDescription( poOpenInfo->pszFilename );
    poDS->TryLoadXML();

/* -------------------------------------------------------------------- */
/*      Check for external overviews.                                   */
/* -------------------------------------------------------------------- */
    poDS->oOvManager.Initialize( poDS.get(), poOpenInfo->pszFilename, poOpenInfo->GetSiblingFiles() );

    return poDS.release();
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr GSBGDataset::GetGeoTransform( double *padfGeoTransform )
{
    if( padfGeoTransform == nullptr )
        return CE_Failure;

    GSBGRasterBand *poGRB = cpl::down_cast<GSBGRasterBand *>(GetRasterBand( 1 ));

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

    GSBGRasterBand *poGRB = cpl::down_cast<GSBGRasterBand *>(GetRasterBand( 1 ));

    if( padfGeoTransform == nullptr)
        return CE_Failure;

    /* non-zero transform 2 or 4 or negative 1 or 5 not supported natively */
    //CPLErr eErr = CE_None;
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

    CPLErr eErr = WriteHeader( fp,
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
                                  int /* nBands */,
                                  GDALDataType eType,
                                  CPL_UNUSED char **papszParamList )
{
    if( nXSize <= 0 || nYSize <= 0 )
    {
        CPLError( CE_Failure, CPLE_IllegalArg,
                  "Unable to create grid, both X and Y size must be "
                  "non-negative.\n" );

        return nullptr;
    }
    else if( nXSize > std::numeric_limits<short>::max() ||
             nYSize > std::numeric_limits<short>::max() )
    {
        CPLError(CE_Failure, CPLE_IllegalArg,
                 "Unable to create grid, Golden Software Binary Grid format "
                 "only supports sizes up to %dx%d.  %dx%d not supported.\n",
                 std::numeric_limits<short>::max(),
                 std::numeric_limits<short>::max(),
                 nXSize, nYSize);

        return nullptr;
    }

    if( eType != GDT_Byte && eType != GDT_Float32 && eType != GDT_UInt16
        && eType != GDT_Int16 )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Golden Software Binary Grid only supports Byte, Int16, "
                  "Uint16, and Float32 datatypes.  Unable to create with "
                  "type %s.\n", GDALGetDataTypeName( eType ) );

        return nullptr;
    }

    VSILFILE *fp = VSIFOpenL( pszFilename, "w+b" );

    if( fp == nullptr )
    {
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "Attempt to create file '%s' failed.\n",
                  pszFilename );
        return nullptr;
    }

    CPLErr eErr = WriteHeader( fp, (GInt16) nXSize, (GInt16) nYSize,
                               0.0, nXSize, 0.0, nYSize, 0.0, 0.0 );
    if( eErr != CE_None )
    {
        VSIFCloseL( fp );
        return nullptr;
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
                return nullptr;
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
    if( pfnProgress == nullptr )
        pfnProgress = GDALDummyProgress;

    int nBands = poSrcDS->GetRasterCount();
    if (nBands == 0)
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "GSBG driver does not support source dataset with zero band.\n");
        return nullptr;
    }
    else if (nBands > 1)
    {
        if( bStrict )
        {
            CPLError( CE_Failure, CPLE_NotSupported,
                      "Unable to create copy, Golden Software Binary Grid "
                      "format only supports one raster band.\n" );
            return nullptr;
        }
        else
            CPLError( CE_Warning, CPLE_NotSupported,
                      "Golden Software Binary Grid format only supports one "
                      "raster band, first band will be copied.\n" );
    }

    GDALRasterBand *poSrcBand = poSrcDS->GetRasterBand(1);
    if( poSrcBand->GetXSize() > std::numeric_limits<short>::max() ||
        poSrcBand->GetYSize() > std::numeric_limits<short>::max() )
    {
        CPLError(CE_Failure, CPLE_IllegalArg,
                 "Unable to create grid, Golden Software Binary Grid format "
                 "only supports sizes up to %dx%d.  %dx%d not supported.\n",
                 std::numeric_limits<short>::max(),
                 std::numeric_limits<short>::max(),
                 poSrcBand->GetXSize(), poSrcBand->GetYSize() );

        return nullptr;
    }

    if( !pfnProgress( 0.0, nullptr, pProgressData ) )
    {
        CPLError( CE_Failure, CPLE_UserInterrupt, "User terminated\n" );
        return nullptr;
    }

    VSILFILE    *fp = VSIFOpenL( pszFilename, "w+b" );

    if( fp == nullptr )
    {
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "Attempt to create file '%s' failed.\n",
                  pszFilename );
        return nullptr;
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
        return nullptr;
    }

/* -------------------------------------------------------------------- */
/*      Copy band data.                                                 */
/* -------------------------------------------------------------------- */
    float *pfData = (float *)VSI_MALLOC2_VERBOSE( nXSize, sizeof( float ) );
    if( pfData == nullptr )
    {
        VSIFCloseL( fp );
        return nullptr;
    }

    int     bSrcHasNDValue;
    float   fSrcNoDataValue = (float) poSrcBand->GetNoDataValue( &bSrcHasNDValue );
    double  dfMinZ = std::numeric_limits<double>::max();
    double  dfMaxZ = std::numeric_limits<double>::lowest();
    for( GInt16 iRow = nYSize - 1; iRow >= 0; iRow-- )
    {
        eErr = poSrcBand->RasterIO( GF_Read, 0, iRow,
                                    nXSize, 1, pfData,
                                    nXSize, 1, GDT_Float32, 0, 0, nullptr );

        if( eErr != CE_None )
        {
            VSIFCloseL( fp );
            VSIFree( pfData );
            return nullptr;
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
            return nullptr;
        }

        if( !pfnProgress( static_cast<double>(nYSize - iRow)/nYSize,
                          nullptr, pProgressData ) )
        {
            VSIFCloseL( fp );
            VSIFree( pfData );
            CPLError( CE_Failure, CPLE_UserInterrupt, "User terminated" );
            return nullptr;
        }
    }

    VSIFree( pfData );

    /* write out the min and max values */
    eErr = WriteHeader( fp, nXSize, nYSize,
                        dfMinX, dfMaxX, dfMinY, dfMaxY, dfMinZ, dfMaxZ );

    if( eErr != CE_None )
    {
        VSIFCloseL( fp );
        return nullptr;
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
    if( GDALGetDriverByName( "GSBG" ) != nullptr )
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription( "GSBG" );
    poDriver->SetMetadataItem( GDAL_DCAP_RASTER, "YES" );
    poDriver->SetMetadataItem( GDAL_DMD_LONGNAME,
                               "Golden Software Binary Grid (.grd)" );
    poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, "drivers/raster/gsbg.html" );
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
