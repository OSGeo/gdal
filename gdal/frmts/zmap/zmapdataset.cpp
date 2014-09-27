/******************************************************************************
 * $Id$
 *
 * Project:  ZMap driver
 * Purpose:  GDALDataset driver for ZMap dataset.
 * Author:   Even Rouault, <even dot rouault at mines dash paris dot org>
 *
 ******************************************************************************
 * Copyright (c) 2011-2012, Even Rouault <even dot rouault at mines-paris dot org>
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

#include "cpl_vsi_virtual.h"
#include "cpl_string.h"
#include "gdal_pam.h"

CPL_CVSID("$Id$");

CPL_C_START
void    GDALRegister_ZMap(void);
CPL_C_END

/************************************************************************/
/* ==================================================================== */
/*                              ZMapDataset                             */
/* ==================================================================== */
/************************************************************************/

class ZMapRasterBand;

class ZMapDataset : public GDALPamDataset
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

    virtual CPLErr GetGeoTransform( double * );

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

class ZMapRasterBand : public GDALPamRasterBand
{
    friend class ZMapDataset;

  public:

                ZMapRasterBand( ZMapDataset * );

    virtual CPLErr IReadBlock( int, int, void * );

    virtual double GetNoDataValue( int *pbSuccess = NULL );
};


/************************************************************************/
/*                           ZMapRasterBand()                           */
/************************************************************************/

ZMapRasterBand::ZMapRasterBand( ZMapDataset *poDS )

{
    this->poDS = poDS;
    this->nBand = nBand;

    eDataType = GDT_Float64;

    nBlockXSize = 1;
    nBlockYSize = poDS->GetRasterYSize();
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr ZMapRasterBand::IReadBlock( int nBlockXOff,
                                   CPL_UNUSED int nBlockYOff,
                                   void * pImage )
{
    int i;
    ZMapDataset *poGDS = (ZMapDataset *) poDS;

    if (poGDS->fp == NULL)
        return CE_Failure;

    if (nBlockXOff < poGDS->nColNum + 1)
    {
        VSIFSeekL(poGDS->fp, poGDS->nDataStartOff, SEEK_SET);
        poGDS->nColNum = -1;
    }

    if (nBlockXOff > poGDS->nColNum + 1)
    {
        for(i=poGDS->nColNum + 1;i<nBlockXOff;i++)
        {
            if (IReadBlock(i,0,pImage) != CE_None)
                return CE_Failure;
        }
    }

    char* pszLine;
    i = 0;
    double dfExp = pow(10.0, poGDS->nDecimalCount);
    while(i<nRasterYSize)
    {
        pszLine = (char*)CPLReadLineL(poGDS->fp);
        if (pszLine == NULL)
            return CE_Failure;
        int nExpected = nRasterYSize - i;
        if (nExpected > poGDS->nValuesPerLine)
            nExpected = poGDS->nValuesPerLine;
        if ((int)strlen(pszLine) != nExpected * poGDS->nFieldSize)
            return CE_Failure;

        for(int j=0;j<nExpected;j++)
        {
            char* pszValue = pszLine + j * poGDS->nFieldSize;
            char chSaved = pszValue[poGDS->nFieldSize];
            pszValue[poGDS->nFieldSize] = 0;
            if (strchr(pszValue, '.') != NULL)
                ((double*)pImage)[i+j] = CPLAtofM(pszValue);
            else
                ((double*)pImage)[i+j] = atoi(pszValue) * dfExp;
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
    ZMapDataset *poGDS = (ZMapDataset *) poDS;

    if (pbSuccess)
        *pbSuccess = TRUE;

    return poGDS->dfNoDataValue;
}

/************************************************************************/
/*                            ~ZMapDataset()                            */
/************************************************************************/

ZMapDataset::ZMapDataset()
{
    fp = NULL;
    nDataStartOff = 0;
    nColNum = -1;
    nValuesPerLine = 0;
    nFieldSize = 0;
    nDecimalCount = 0;
    dfNoDataValue = 0.0;
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
    FlushCache();
    if (fp)
        VSIFCloseL(fp);
}

/************************************************************************/
/*                             Identify()                               */
/************************************************************************/

int ZMapDataset::Identify( GDALOpenInfo * poOpenInfo )
{
    int         i;

    if (poOpenInfo->nHeaderBytes == 0)
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Chech that it looks roughly as a ZMap dataset                   */
/* -------------------------------------------------------------------- */
    const char* pszData = (const char*)poOpenInfo->pabyHeader;

    /* Skip comments line at the beginning */
    i=0;
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

    if (strncmp(pszToken, "GRID", 4) != 0)
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
    if (!Identify(poOpenInfo))
        return NULL;

/* -------------------------------------------------------------------- */
/*      Find dataset characteristics                                    */
/* -------------------------------------------------------------------- */
    VSILFILE* fp = VSIFOpenL(poOpenInfo->pszFilename, "rb");
    if (fp == NULL)
        return NULL;

    const char* pszLine;

    while((pszLine = CPLReadLine2L(fp, 100, NULL)) != NULL)
    {
        if (*pszLine == '!')
        {
            continue;
        }
        else
            break;
    }
    if (pszLine == NULL)
    {
        VSIFCloseL(fp);
        return NULL;
    }

    /* Parse first header line */
    char** papszTokens = CSLTokenizeString2( pszLine, ",", 0 );
    if (CSLCount(papszTokens) != 3)
    {
        CSLDestroy(papszTokens);
        VSIFCloseL(fp);
        return NULL;
    }

    int nValuesPerLine = atoi(papszTokens[2]);
    if (nValuesPerLine <= 0)
    {
        CSLDestroy(papszTokens);
        VSIFCloseL(fp);
        return NULL;
    }

    CSLDestroy(papszTokens);
    papszTokens = NULL;

    /* Parse second header line */
    pszLine = CPLReadLine2L(fp, 100, NULL);
    if (pszLine == NULL)
    {
        VSIFCloseL(fp);
        return NULL;
    }
    papszTokens = CSLTokenizeString2( pszLine, ",", 0 );
    if (CSLCount(papszTokens) != 5)
    {
        CSLDestroy(papszTokens);
        VSIFCloseL(fp);
        return NULL;
    }

    int nFieldSize = atoi(papszTokens[0]);
    double dfNoDataValue = CPLAtofM(papszTokens[1]);
    int nDecimalCount = atoi(papszTokens[3]);
    int nColumnNumber = atoi(papszTokens[4]);

    CSLDestroy(papszTokens);
    papszTokens = NULL;

    if (nFieldSize <= 0 || nFieldSize >= 40 ||
        nDecimalCount <= 0 || nDecimalCount >= nFieldSize ||
        nColumnNumber != 1)
    {
        CPLDebug("ZMap", "nFieldSize=%d, nDecimalCount=%d, nColumnNumber=%d",
                 nFieldSize, nDecimalCount, nColumnNumber);
        VSIFCloseL(fp);
        return NULL;
    }

    /* Parse third header line */
    pszLine = CPLReadLine2L(fp, 100, NULL);
    if (pszLine == NULL)
    {
        VSIFCloseL(fp);
        return NULL;
    }
    papszTokens = CSLTokenizeString2( pszLine, ",", 0 );
    if (CSLCount(papszTokens) != 6)
    {
        CSLDestroy(papszTokens);
        VSIFCloseL(fp);
        return NULL;
    }

    int nRows = atoi(papszTokens[0]);
    int nCols = atoi(papszTokens[1]);
    double dfMinX = CPLAtofM(papszTokens[2]);
    double dfMaxX = CPLAtofM(papszTokens[3]);
    double dfMinY = CPLAtofM(papszTokens[4]);
    double dfMaxY = CPLAtofM(papszTokens[5]);

    CSLDestroy(papszTokens);
    papszTokens = NULL;

    if (!GDALCheckDatasetDimensions(nCols, nRows) ||
        nCols == 1 || nRows == 1)
    {
        VSIFCloseL(fp);
        return NULL;
    }
    
    /* Ignore fourth header line */
    pszLine = CPLReadLine2L(fp, 100, NULL);
    if (pszLine == NULL)
    {
        VSIFCloseL(fp);
        return NULL;
    }

    /* Check fifth header line */
    pszLine = CPLReadLine2L(fp, 100, NULL);
    if (pszLine == NULL || pszLine[0] != '@')
    {
        VSIFCloseL(fp);
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */
    ZMapDataset         *poDS;

    poDS = new ZMapDataset();
    poDS->fp = fp;
    poDS->nDataStartOff = VSIFTellL(fp);
    poDS->nValuesPerLine = nValuesPerLine;
    poDS->nFieldSize = nFieldSize;
    poDS->nDecimalCount = nDecimalCount;
    poDS->nRasterXSize = nCols;
    poDS->nRasterYSize = nRows;
    poDS->dfNoDataValue = dfNoDataValue;

    if (CSLTestBoolean(CPLGetConfigOption("ZMAP_PIXEL_IS_POINT", "FALSE")))
    {
        double dfStepX = (dfMaxX - dfMinX) / (nCols - 1);
        double dfStepY = (dfMaxY - dfMinY) / (nRows - 1);

        poDS->adfGeoTransform[0] = dfMinX - dfStepX / 2;
        poDS->adfGeoTransform[1] = dfStepX;
        poDS->adfGeoTransform[3] = dfMaxY + dfStepY / 2;
        poDS->adfGeoTransform[5] = -dfStepY;
    }
    else
    {
        double dfStepX = (dfMaxX - dfMinX) / nCols ;
        double dfStepY = (dfMaxY - dfMinY) / nRows;

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
    return( poDS );
}


/************************************************************************/
/*                       WriteRightJustified()                          */
/************************************************************************/

static void WriteRightJustified(VSILFILE* fp, const char *pszValue, int nWidth)
{
    int nLen = strlen(pszValue);
    CPLAssert(nLen <= nWidth);
    int i;
    for(i=0;i<nWidth -nLen;i++)
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
        sprintf(szFormat, "%%.%df", nDecimals);
    else
        sprintf(szFormat, "%%g");
    char* pszValue = (char*)CPLSPrintf(szFormat, dfValue);
    char* pszE = strchr(pszValue, 'e');
    if (pszE)
        *pszE = 'E';

    if ((int)strlen(pszValue) > nWidth)
    {
        sprintf(szFormat, "%%.%dg", nDecimals);
        pszValue = (char*)CPLSPrintf(szFormat, dfValue);
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
        return NULL;
    }

    if (nBands != 1)
    {
        CPLError( (bStrict) ? CE_Failure : CE_Warning, CPLE_NotSupported,
                  "ZMap driver only uses the first band of the dataset.\n");
        if (bStrict)
            return NULL;
    }

    if( pfnProgress && !pfnProgress( 0.0, NULL, pProgressData ) )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Get source dataset info                                         */
/* -------------------------------------------------------------------- */

    int nXSize = poSrcDS->GetRasterXSize();
    int nYSize = poSrcDS->GetRasterYSize();
    if (nXSize == 1 || nYSize == 1)
    {
        return NULL;
    }
    
    double adfGeoTransform[6];
    poSrcDS->GetGeoTransform(adfGeoTransform);
    if (adfGeoTransform[2] != 0 || adfGeoTransform[4] != 0)
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "ZMap driver does not support CreateCopy() from skewed or rotated dataset.\n");
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Create target file                                              */
/* -------------------------------------------------------------------- */

    VSILFILE* fp = VSIFOpenL(pszFilename, "wb");
    if (fp == NULL)
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Cannot create %s", pszFilename );
        return NULL;
    }

    int nFieldSize = 20;
    int nValuesPerLine = 4;
    int nDecimalCount = 7;

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

    if (CSLTestBoolean(CPLGetConfigOption("ZMAP_PIXEL_IS_POINT", "FALSE")))
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
    double* padfLineBuffer = (double*) CPLMalloc(nYSize * sizeof(double));
    int i, j;
    CPLErr eErr = CE_None;
    for(i=0;i<nXSize && eErr == CE_None;i++)
    {
        eErr = poSrcDS->GetRasterBand(1)->RasterIO(
                                            GF_Read, i, 0, 1, nYSize,
                                            padfLineBuffer, 1, nYSize,
                                            GDT_Float64, 0, 0);
        if (eErr != CE_None)
            break;
        int bEOLPrinted = FALSE;
        for(j=0;j<nYSize;j++)
        {
            WriteRightJustified(fp, padfLineBuffer[j], nFieldSize, nDecimalCount);
            if (((j + 1) % nValuesPerLine) == 0)
            {
                bEOLPrinted = TRUE;
                VSIFPrintfL(fp, "\n");
            }
            else
                bEOLPrinted = FALSE;
        }
        if (!bEOLPrinted)
            VSIFPrintfL(fp, "\n");

        if (!pfnProgress( (j+1) * 1.0 / nYSize, NULL, pProgressData))
        {
            eErr = CE_Failure;
            break;
        }
    }
    CPLFree(padfLineBuffer);
    VSIFCloseL(fp);

    if (eErr != CE_None)
        return NULL;

    return (GDALDataset*) GDALOpen(pszFilename, GA_ReadOnly);
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr ZMapDataset::GetGeoTransform( double * padfTransform )

{
    memcpy(padfTransform, adfGeoTransform, 6 * sizeof(double));

    return( CE_None );
}

/************************************************************************/
/*                         GDALRegister_ZMap()                          */
/************************************************************************/

void GDALRegister_ZMap()

{
    GDALDriver  *poDriver;

    if( GDALGetDriverByName( "ZMap" ) == NULL )
    {
        poDriver = new GDALDriver();

        poDriver->SetDescription( "ZMap" );
        poDriver->SetMetadataItem( GDAL_DCAP_RASTER, "YES" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME,
                                   "ZMap Plus Grid" );
        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC,
                                   "frmt_various.html#ZMap" );
        poDriver->SetMetadataItem( GDAL_DMD_EXTENSION, "dat" );

        poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );

        poDriver->pfnOpen = ZMapDataset::Open;
        poDriver->pfnIdentify = ZMapDataset::Identify;
        poDriver->pfnCreateCopy = ZMapDataset::CreateCopy;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}
