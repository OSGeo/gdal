/******************************************************************************
 *
 * Project:  ZMap driver
 * Purpose:  GDALDataset driver for ZMap dataset.
 * Author:   Even Rouault, <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2011-2012, Even Rouault <even dot rouault at spatialys.com>
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

#include "cpl_string.h"
#include "cpl_vsi_virtual.h"
#include "gdal_frmts.h"
#include "gdal_pam.h"

#include <cmath>

CPL_CVSID("$Id$")

/************************************************************************/
/* ==================================================================== */
/*                              ZMapDataset                             */
/* ==================================================================== */
/************************************************************************/

class ZMapRasterBand;

class ZMapDataset final: public GDALPamDataset
{
    friend class ZMapRasterBand;

    VSILFILE   *fp;
    int         nValuesPerLine;
    int         nFieldSize;
    int         nDecimalCount;
    int         nColNum;
    double      dfNoDataValue;
    vsi_l_offset nDataStartOff;
    double      adfGeoTransform[6];

  public:
                 ZMapDataset();
    virtual     ~ZMapDataset();

    virtual CPLErr GetGeoTransform( double * ) override;

    static GDALDataset *Open( GDALOpenInfo * );
    static int          Identify( GDALOpenInfo * );
    static GDALDataset *CreateCopy( const char * pszFilename, GDALDataset *poSrcDS,
                                    int bStrict, char ** papszOptions,
                                    GDALProgressFunc pfnProgress, void * pProgressData );
};

/************************************************************************/
/* ==================================================================== */
/*                            ZMapRasterBand                            */
/* ==================================================================== */
/************************************************************************/

class ZMapRasterBand final: public GDALPamRasterBand
{
    friend class ZMapDataset;

  public:
    explicit ZMapRasterBand( ZMapDataset * );

    virtual CPLErr IReadBlock( int, int, void * ) override;
    virtual double GetNoDataValue( int *pbSuccess = nullptr ) override;
};

/************************************************************************/
/*                           ZMapRasterBand()                           */
/************************************************************************/

ZMapRasterBand::ZMapRasterBand( ZMapDataset *poDSIn )

{
    poDS = poDSIn;
    nBand = 1;

    eDataType = GDT_Float64;

    nBlockXSize = 1;
    nBlockYSize = poDSIn->GetRasterYSize();
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr ZMapRasterBand::IReadBlock( int nBlockXOff,
                                   CPL_UNUSED int nBlockYOff,
                                   void * pImage )
{
    ZMapDataset *poGDS = reinterpret_cast<ZMapDataset *>( poDS );

    if (poGDS->fp == nullptr)
        return CE_Failure;

    if (nBlockXOff < poGDS->nColNum + 1)
    {
        VSIFSeekL(poGDS->fp, poGDS->nDataStartOff, SEEK_SET);
        poGDS->nColNum = -1;
    }

    if (nBlockXOff > poGDS->nColNum + 1)
    {
        for( int i = poGDS->nColNum + 1; i < nBlockXOff; i++ )
        {
            if (IReadBlock(i,0,pImage) != CE_None)
                return CE_Failure;
        }
    }

    int i = 0;
    const double dfExp = std::pow(10.0, poGDS->nDecimalCount);
    while(i<nRasterYSize)
    {
        char* pszLine = const_cast<char *>( CPLReadLineL(poGDS->fp) );
        if (pszLine == nullptr)
            return CE_Failure;
        int nExpected = nRasterYSize - i;
        if (nExpected > poGDS->nValuesPerLine)
            nExpected = poGDS->nValuesPerLine;
        if( static_cast<int>( strlen(pszLine) )
            != nExpected * poGDS->nFieldSize )
            return CE_Failure;

        for( int j = 0; j < nExpected; j++ )
        {
            char* pszValue = pszLine + j * poGDS->nFieldSize;
            const char chSaved = pszValue[poGDS->nFieldSize];
            pszValue[poGDS->nFieldSize] = 0;
            if (strchr(pszValue, '.') != nullptr)
                reinterpret_cast<double *>( pImage )[i+j] = CPLAtofM(pszValue);
            else
                reinterpret_cast<double *>( pImage )[i+j]
                    = atoi(pszValue) * dfExp;
            pszValue[poGDS->nFieldSize] = chSaved;
        }

        i += nExpected;
    }

    poGDS->nColNum ++;

    return CE_None;
}

/************************************************************************/
/*                          GetNoDataValue()                            */
/************************************************************************/

double ZMapRasterBand::GetNoDataValue( int *pbSuccess )
{
    ZMapDataset *poGDS = reinterpret_cast<ZMapDataset *>( poDS );

    if (pbSuccess)
        *pbSuccess = TRUE;

    return poGDS->dfNoDataValue;
}

/************************************************************************/
/*                            ~ZMapDataset()                            */
/************************************************************************/

ZMapDataset::ZMapDataset() :
    fp(nullptr),
    nValuesPerLine(0),
    nFieldSize(0),
    nDecimalCount(0),
    nColNum(-1),
    dfNoDataValue(0.0),
    nDataStartOff(0)
{
    adfGeoTransform[0] = 0;
    adfGeoTransform[1] = 1;
    adfGeoTransform[2] = 0;
    adfGeoTransform[3] = 0;
    adfGeoTransform[4] = 0;
    adfGeoTransform[5] = 1;
}

/************************************************************************/
/*                            ~ZMapDataset()                            */
/************************************************************************/

ZMapDataset::~ZMapDataset()

{
    FlushCache(true);
    if (fp)
        VSIFCloseL(fp);
}

/************************************************************************/
/*                             Identify()                               */
/************************************************************************/

int ZMapDataset::Identify( GDALOpenInfo * poOpenInfo )
{
    if (poOpenInfo->nHeaderBytes == 0)
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Check that it looks roughly as a ZMap dataset                   */
/* -------------------------------------------------------------------- */
    const char* pszData
        = reinterpret_cast<const char *>( poOpenInfo->pabyHeader );

    /* Skip comments line at the beginning */
    int i = 0;
    if (pszData[i] == '!')
    {
        i++;
        for(;i<poOpenInfo->nHeaderBytes;i++)
        {
            char ch = pszData[i];
            if (ch == 13 || ch == 10)
            {
                i++;
                if (ch == 13 && pszData[i] == 10)
                    i++;
                if (pszData[i] != '!')
                    break;
            }
        }
    }

    if (pszData[i] != '@')
        return FALSE;
    i++;

    char** papszTokens = CSLTokenizeString2( pszData+i, ",", 0 );
    if (CSLCount(papszTokens) < 3)
    {
        CSLDestroy(papszTokens);
        return FALSE;
    }

    const char* pszToken = papszTokens[1];
    while (*pszToken == ' ')
        pszToken ++;

    if (!STARTS_WITH(pszToken, "GRID"))
    {
        CSLDestroy(papszTokens);
        return FALSE;
    }

    CSLDestroy(papszTokens);
    return TRUE;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *ZMapDataset::Open( GDALOpenInfo * poOpenInfo )

{
    if (!Identify(poOpenInfo) || poOpenInfo->fpL == nullptr )
        return nullptr;

/* -------------------------------------------------------------------- */
/*      Confirm the requested access is supported.                      */
/* -------------------------------------------------------------------- */
    if( poOpenInfo->eAccess == GA_Update )
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "The ZMAP driver does not support update access to existing"
                  " datasets." );
        return nullptr;
    }
/* -------------------------------------------------------------------- */
/*      Find dataset characteristics                                    */
/* -------------------------------------------------------------------- */

    const char* pszLine;

    while((pszLine = CPLReadLine2L(poOpenInfo->fpL, 100, nullptr)) != nullptr)
    {
        if (*pszLine == '!')
        {
            continue;
        }
        else
            break;
    }
    // cppcheck-suppress knownConditionTrueFalse
    if (pszLine == nullptr)
    {
        VSIFCloseL(poOpenInfo->fpL);
        poOpenInfo->fpL = nullptr;
        return nullptr;
    }

    /* Parse first header line */
    char** papszTokens = CSLTokenizeString2( pszLine, ",", 0 );
    if (CSLCount(papszTokens) != 3)
    {
        CSLDestroy(papszTokens);
        VSIFCloseL(poOpenInfo->fpL);
        poOpenInfo->fpL = nullptr;
        return nullptr;
    }

    const int nValuesPerLine = atoi(papszTokens[2]);
    if (nValuesPerLine <= 0)
    {
        CSLDestroy(papszTokens);
        VSIFCloseL(poOpenInfo->fpL);
        poOpenInfo->fpL = nullptr;
        return nullptr;
    }

    CSLDestroy(papszTokens);
    papszTokens = nullptr;

    /* Parse second header line */
    pszLine = CPLReadLine2L(poOpenInfo->fpL, 100, nullptr);
    if (pszLine == nullptr)
    {
        VSIFCloseL(poOpenInfo->fpL);
        poOpenInfo->fpL = nullptr;
        return nullptr;
    }
    papszTokens = CSLTokenizeString2( pszLine, ",", 0 );
    if (CSLCount(papszTokens) != 5)
    {
        CSLDestroy(papszTokens);
        VSIFCloseL(poOpenInfo->fpL);
        poOpenInfo->fpL = nullptr;
        return nullptr;
    }

    const int nFieldSize = atoi(papszTokens[0]);
    const double dfNoDataValue = CPLAtofM(papszTokens[1]);
    const int nDecimalCount = atoi(papszTokens[3]);
    const int nColumnNumber = atoi(papszTokens[4]);

    CSLDestroy(papszTokens);
    papszTokens = nullptr;

    if (nFieldSize <= 0 || nFieldSize >= 40 ||
        nDecimalCount <= 0 || nDecimalCount >= nFieldSize ||
        nColumnNumber != 1)
    {
        CPLDebug("ZMap", "nFieldSize=%d, nDecimalCount=%d, nColumnNumber=%d",
                 nFieldSize, nDecimalCount, nColumnNumber);
        VSIFCloseL(poOpenInfo->fpL);
        poOpenInfo->fpL = nullptr;
        return nullptr;
    }

    /* Parse third header line */
    pszLine = CPLReadLine2L(poOpenInfo->fpL, 100, nullptr);
    if (pszLine == nullptr)
    {
        VSIFCloseL(poOpenInfo->fpL);
        poOpenInfo->fpL = nullptr;
        return nullptr;
    }
    papszTokens = CSLTokenizeString2( pszLine, ",", 0 );
    if (CSLCount(papszTokens) != 6)
    {
        CSLDestroy(papszTokens);
        VSIFCloseL(poOpenInfo->fpL);
        poOpenInfo->fpL = nullptr;
        return nullptr;
    }

    const int nRows = atoi(papszTokens[0]);
    const int nCols = atoi(papszTokens[1]);
    const double dfMinX = CPLAtofM(papszTokens[2]);
    const double dfMaxX = CPLAtofM(papszTokens[3]);
    const double dfMinY = CPLAtofM(papszTokens[4]);
    const double dfMaxY = CPLAtofM(papszTokens[5]);

    CSLDestroy(papszTokens);
    papszTokens = nullptr;

    if (!GDALCheckDatasetDimensions(nCols, nRows) ||
        nCols == 1 || nRows == 1)
    {
        VSIFCloseL(poOpenInfo->fpL);
        poOpenInfo->fpL = nullptr;
        return nullptr;
    }

    /* Ignore fourth header line */
    pszLine = CPLReadLine2L(poOpenInfo->fpL, 100, nullptr);
    if (pszLine == nullptr)
    {
        VSIFCloseL(poOpenInfo->fpL);
        poOpenInfo->fpL = nullptr;
        return nullptr;
    }

    /* Check fifth header line */
    pszLine = CPLReadLine2L(poOpenInfo->fpL, 100, nullptr);
    if (pszLine == nullptr || pszLine[0] != '@')
    {
        VSIFCloseL(poOpenInfo->fpL);
        poOpenInfo->fpL = nullptr;
        return nullptr;
    }

/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */
    ZMapDataset *poDS = new ZMapDataset();
    poDS->fp = poOpenInfo->fpL;
    poOpenInfo->fpL = nullptr;
    poDS->nDataStartOff = VSIFTellL(poDS->fp);
    poDS->nValuesPerLine = nValuesPerLine;
    poDS->nFieldSize = nFieldSize;
    poDS->nDecimalCount = nDecimalCount;
    poDS->nRasterXSize = nCols;
    poDS->nRasterYSize = nRows;
    poDS->dfNoDataValue = dfNoDataValue;

    if (CPLTestBool(CPLGetConfigOption("ZMAP_PIXEL_IS_POINT", "FALSE")))
    {
        const double dfStepX = (dfMaxX - dfMinX) / (nCols - 1);
        const double dfStepY = (dfMaxY - dfMinY) / (nRows - 1);

        poDS->adfGeoTransform[0] = dfMinX - dfStepX / 2;
        poDS->adfGeoTransform[1] = dfStepX;
        poDS->adfGeoTransform[3] = dfMaxY + dfStepY / 2;
        poDS->adfGeoTransform[5] = -dfStepY;
    }
    else
    {
        const double dfStepX = (dfMaxX - dfMinX) / nCols ;
        const double dfStepY = (dfMaxY - dfMinY) / nRows;

        poDS->adfGeoTransform[0] = dfMinX;
        poDS->adfGeoTransform[1] = dfStepX;
        poDS->adfGeoTransform[3] = dfMaxY;
        poDS->adfGeoTransform[5] = -dfStepY;
    }

/* -------------------------------------------------------------------- */
/*      Create band information objects.                                */
/* -------------------------------------------------------------------- */
    poDS->nBands = 1;
    poDS->SetBand( 1, new ZMapRasterBand( poDS ) );

/* -------------------------------------------------------------------- */
/*      Initialize any PAM information.                                 */
/* -------------------------------------------------------------------- */
    poDS->SetDescription( poOpenInfo->pszFilename );
    poDS->TryLoadXML();

/* -------------------------------------------------------------------- */
/*      Support overviews.                                              */
/* -------------------------------------------------------------------- */
    poDS->oOvManager.Initialize( poDS, poOpenInfo->pszFilename );
    return poDS;
}

/************************************************************************/
/*                       WriteRightJustified()                          */
/************************************************************************/

static void WriteRightJustified(VSILFILE* fp, const char *pszValue, int nWidth)
{
    int nLen = (int)strlen(pszValue);
    CPLAssert(nLen <= nWidth);
    for( int i=0; i < nWidth - nLen; i++ )
        VSIFWriteL(" ", 1, 1, fp);
    VSIFWriteL(pszValue, 1, nLen, fp);
}

static void WriteRightJustified(VSILFILE* fp, int nValue, int nWidth)
{
    CPLString osValue(CPLSPrintf("%d", nValue));
    WriteRightJustified(fp, osValue.c_str(), nWidth);
}

static void WriteRightJustified(VSILFILE* fp, double dfValue, int nWidth,
                                int nDecimals = -1)
{
    char szFormat[32];
    if (nDecimals >= 0)
        snprintf(szFormat, sizeof(szFormat), "%%.%df", nDecimals);
    else
        snprintf(szFormat, sizeof(szFormat), "%%g");
    char* pszValue = const_cast<char *>( CPLSPrintf(szFormat, dfValue) );
    char* pszE = strchr(pszValue, 'e');
    if (pszE)
        *pszE = 'E';

    if( static_cast<int>( strlen(pszValue) ) > nWidth)
    {
        snprintf(szFormat, sizeof(szFormat), "%%.%dg", nDecimals);
        pszValue = const_cast<char *>( CPLSPrintf(szFormat, dfValue) );
        pszE = strchr(pszValue, 'e');
        if (pszE)
            *pszE = 'E';
    }

    CPLString osValue(pszValue);
    WriteRightJustified(fp, osValue.c_str(), nWidth);
}

/************************************************************************/
/*                             CreateCopy()                             */
/************************************************************************/

GDALDataset* ZMapDataset::CreateCopy( const char * pszFilename,
                                      GDALDataset *poSrcDS,
                                      int bStrict,
                                      CPL_UNUSED char ** papszOptions,
                                      GDALProgressFunc pfnProgress,
                                      void * pProgressData )
{
/* -------------------------------------------------------------------- */
/*      Some some rudimentary checks                                    */
/* -------------------------------------------------------------------- */
    int nBands = poSrcDS->GetRasterCount();
    if (nBands == 0)
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "ZMap driver does not support source dataset with zero band.\n");
        return nullptr;
    }

    if (nBands != 1)
    {
        CPLError( (bStrict) ? CE_Failure : CE_Warning, CPLE_NotSupported,
                  "ZMap driver only uses the first band of the dataset.\n");
        if (bStrict)
            return nullptr;
    }

    if( pfnProgress && !pfnProgress( 0.0, nullptr, pProgressData ) )
        return nullptr;

/* -------------------------------------------------------------------- */
/*      Get source dataset info                                         */
/* -------------------------------------------------------------------- */

    const int nXSize = poSrcDS->GetRasterXSize();
    const int nYSize = poSrcDS->GetRasterYSize();
    if (nXSize == 1 || nYSize == 1)
    {
        return nullptr;
    }

    double adfGeoTransform[6];
    poSrcDS->GetGeoTransform(adfGeoTransform);
    if (adfGeoTransform[2] != 0 || adfGeoTransform[4] != 0)
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "ZMap driver does not support CreateCopy() from skewed or "
                  "rotated dataset.\n");
        return nullptr;
    }

/* -------------------------------------------------------------------- */
/*      Create target file                                              */
/* -------------------------------------------------------------------- */

    VSILFILE* fp = VSIFOpenL(pszFilename, "wb");
    if (fp == nullptr)
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Cannot create %s", pszFilename );
        return nullptr;
    }

    const int nFieldSize = 20;
    const int nValuesPerLine = 4;
    const int nDecimalCount = 7;

    int bHasNoDataValue = FALSE;
    double dfNoDataValue =
        poSrcDS->GetRasterBand(1)->GetNoDataValue(&bHasNoDataValue);
    if (!bHasNoDataValue)
        dfNoDataValue = 1.e30;

    VSIFPrintfL(fp, "!\n");
    VSIFPrintfL(fp, "! Created by GDAL.\n");
    VSIFPrintfL(fp, "!\n");
    VSIFPrintfL(fp, "@GRID FILE, GRID, %d\n", nValuesPerLine);

    WriteRightJustified(fp, nFieldSize, 10);
    VSIFPrintfL(fp, ",");
    WriteRightJustified(fp, dfNoDataValue, 10);
    VSIFPrintfL(fp, ",");
    WriteRightJustified(fp, "", 10);
    VSIFPrintfL(fp, ",");
    WriteRightJustified(fp, nDecimalCount, 10);
    VSIFPrintfL(fp, ",");
    WriteRightJustified(fp, 1, 10);
    VSIFPrintfL(fp, "\n");

    WriteRightJustified(fp, nYSize, 10);
    VSIFPrintfL(fp, ",");
    WriteRightJustified(fp, nXSize, 10);
    VSIFPrintfL(fp, ",");

    if (CPLTestBool(CPLGetConfigOption("ZMAP_PIXEL_IS_POINT", "FALSE")))
    {
        WriteRightJustified(fp, adfGeoTransform[0] + adfGeoTransform[1] / 2, 14, 7);
        VSIFPrintfL(fp, ",");
        WriteRightJustified(fp, adfGeoTransform[0] + adfGeoTransform[1] * nXSize -
                                adfGeoTransform[1] / 2, 14, 7);
        VSIFPrintfL(fp, ",");
        WriteRightJustified(fp, adfGeoTransform[3] + adfGeoTransform[5] * nYSize -
                                adfGeoTransform[5] / 2, 14, 7);
        VSIFPrintfL(fp, ",");
        WriteRightJustified(fp, adfGeoTransform[3] + adfGeoTransform[5] / 2, 14, 7);
    }
    else
    {
        WriteRightJustified(fp, adfGeoTransform[0], 14, 7);
        VSIFPrintfL(fp, ",");
        WriteRightJustified(fp, adfGeoTransform[0] + adfGeoTransform[1] * nXSize, 14, 7);
        VSIFPrintfL(fp, ",");
        WriteRightJustified(fp, adfGeoTransform[3] + adfGeoTransform[5] * nYSize, 14, 7);
        VSIFPrintfL(fp, ",");
        WriteRightJustified(fp, adfGeoTransform[3], 14, 7);
    }

    VSIFPrintfL(fp, "\n");

    VSIFPrintfL(fp, "0.0, 0.0, 0.0\n");
    VSIFPrintfL(fp, "@\n");

/* -------------------------------------------------------------------- */
/*      Copy imagery                                                    */
/* -------------------------------------------------------------------- */
    double* padfLineBuffer = reinterpret_cast<double *>(
        CPLMalloc(nYSize * sizeof(double) ) );

    CPLErr eErr = CE_None;
    for( int i=0; i < nXSize && eErr == CE_None; i++ )
    {
        eErr = poSrcDS->GetRasterBand(1)->RasterIO(
                                            GF_Read, i, 0, 1, nYSize,
                                            padfLineBuffer, 1, nYSize,
                                            GDT_Float64, 0, 0, nullptr);
        if (eErr != CE_None)
            break;
        bool bEOLPrinted = false;
        int j = 0;
        for( ; j < nYSize; j++ )
        {
            WriteRightJustified(fp, padfLineBuffer[j], nFieldSize, nDecimalCount);
            if (((j + 1) % nValuesPerLine) == 0)
            {
                bEOLPrinted = true;
                VSIFPrintfL(fp, "\n");
            }
            else
                bEOLPrinted = false;
        }
        if (!bEOLPrinted)
            VSIFPrintfL(fp, "\n");

        if (pfnProgress != nullptr &&
            !pfnProgress( (j+1) * 1.0 / nYSize, nullptr, pProgressData))
        {
            eErr = CE_Failure;
            break;
        }
    }
    CPLFree(padfLineBuffer);
    VSIFCloseL(fp);

    if (eErr != CE_None)
        return nullptr;

    return reinterpret_cast<GDALDataset*>(
        GDALOpen(pszFilename, GA_ReadOnly) );
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr ZMapDataset::GetGeoTransform( double * padfTransform )

{
    memcpy(padfTransform, adfGeoTransform, 6 * sizeof(double));

    return CE_None;
}

/************************************************************************/
/*                         GDALRegister_ZMap()                          */
/************************************************************************/

void GDALRegister_ZMap()

{
    if( GDALGetDriverByName( "ZMap" ) != nullptr )
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription( "ZMap" );
    poDriver->SetMetadataItem( GDAL_DCAP_RASTER, "YES" );
    poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, "ZMap Plus Grid" );
    poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, "drivers/raster/zmap.html" );
    poDriver->SetMetadataItem( GDAL_DMD_EXTENSION, "dat" );
    poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );

    poDriver->pfnOpen = ZMapDataset::Open;
    poDriver->pfnIdentify = ZMapDataset::Identify;
    poDriver->pfnCreateCopy = ZMapDataset::CreateCopy;

    GetGDALDriverManager()->RegisterDriver( poDriver );
}
