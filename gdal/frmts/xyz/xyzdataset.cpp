/******************************************************************************
 * $Id$
 *
 * Project:  XYZ driver
 * Purpose:  GDALDataset driver for XYZ dataset.
 * Author:   Even Rouault, <even dot rouault at mines dash paris dot org>
 *
 ******************************************************************************
 * Copyright (c) 2010-2013, Even Rouault <even dot rouault at mines-paris dot org>
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
void    GDALRegister_XYZ(void);
CPL_C_END

/************************************************************************/
/* ==================================================================== */
/*                              XYZDataset                              */
/* ==================================================================== */
/************************************************************************/

class XYZRasterBand;

class XYZDataset : public GDALPamDataset
{
    friend class XYZRasterBand;
    
    VSILFILE   *fp;
    int         bHasHeaderLine;
    int         nCommentLineCount;
    char        chDecimalSep;
    int         nXIndex;
    int         nYIndex;
    int         nZIndex;
    int         nMinTokens;
    int         nLineNum;     /* any line */
    int         nDataLineNum; /* line with values (header line and empty lines ignored) */
    double      adfGeoTransform[6];
    int         bSameNumberOfValuesPerLine;
    double      dfMinZ;
    double      dfMaxZ;

    static int          IdentifyEx( GDALOpenInfo *, int&, int& nCommentLineCount );

  public:
                 XYZDataset();
    virtual     ~XYZDataset();
    
    virtual CPLErr GetGeoTransform( double * );
    
    static GDALDataset *Open( GDALOpenInfo * );
    static int          Identify( GDALOpenInfo * );
    static GDALDataset *CreateCopy( const char * pszFilename, GDALDataset *poSrcDS, 
                                    int bStrict, char ** papszOptions, 
                                    GDALProgressFunc pfnProgress, void * pProgressData );
};

/************************************************************************/
/* ==================================================================== */
/*                            XYZRasterBand                             */
/* ==================================================================== */
/************************************************************************/

class XYZRasterBand : public GDALPamRasterBand
{
    friend class XYZDataset;

    int          nLastYOff;

  public:

                XYZRasterBand( XYZDataset *, int, GDALDataType );

    virtual CPLErr IReadBlock( int, int, void * );
    virtual double GetMinimum( int *pbSuccess = NULL );
    virtual double GetMaximum( int *pbSuccess = NULL );
    virtual double GetNoDataValue( int *pbSuccess = NULL );
};


/************************************************************************/
/*                           XYZRasterBand()                            */
/************************************************************************/

XYZRasterBand::XYZRasterBand( XYZDataset *poDS, int nBand, GDALDataType eDT )

{
    this->poDS = poDS;
    this->nBand = nBand;

    eDataType = eDT;

    nBlockXSize = poDS->GetRasterXSize();
    nBlockYSize = 1;

    nLastYOff = -1;
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr XYZRasterBand::IReadBlock( int nBlockXOff, int nBlockYOff,
                                  void * pImage )

{
    XYZDataset *poGDS = (XYZDataset *) poDS;
    
    if (poGDS->fp == NULL)
        return CE_Failure;

    if( pImage )
    {
        int bSuccess = FALSE;
        double dfNoDataValue = GetNoDataValue(&bSuccess);
        if( !bSuccess )
            dfNoDataValue = 0.0;
        GDALCopyWords(&dfNoDataValue, GDT_Float64, 0,
                      pImage, eDataType, GDALGetDataTypeSize(eDataType) / 8,
                      nRasterXSize);
    }

    int nLineInFile = nBlockYOff * nBlockXSize; // only valid if bSameNumberOfValuesPerLine
    if ( (poGDS->bSameNumberOfValuesPerLine && poGDS->nDataLineNum > nLineInFile) ||
         (!poGDS->bSameNumberOfValuesPerLine && (nLastYOff == -1 || nBlockYOff == 0)) )
    {
        poGDS->nDataLineNum = 0;
        poGDS->nLineNum = 0;
        VSIFSeekL(poGDS->fp, 0, SEEK_SET);

        for(int i=0;i<poGDS->nCommentLineCount;i++)
        {
            CPLReadLine2L(poGDS->fp, 100, NULL);
            poGDS->nLineNum ++;
        }

        if (poGDS->bHasHeaderLine)
        {
            const char* pszLine = CPLReadLine2L(poGDS->fp, 100, 0);
            if (pszLine == NULL)
                return CE_Failure;
            poGDS->nLineNum ++;
        }
    }

    if( !poGDS->bSameNumberOfValuesPerLine && nBlockYOff != nLastYOff + 1 )
    {
        int iY;
        if( nBlockYOff < nLastYOff )
        {
            nLastYOff = -1;
            for(iY = 0; iY < nBlockYOff; iY++)
            {
                if( IReadBlock(0, iY, NULL) != CE_None )
                    return CE_Failure;
            }
        }
        else
        {
            for(iY = nLastYOff + 1; iY < nBlockYOff; iY++)
            {
                if( IReadBlock(0, iY, NULL) != CE_None )
                    return CE_Failure;
            }
        }
    }
    else if( poGDS->bSameNumberOfValuesPerLine )
    {
        while(poGDS->nDataLineNum < nLineInFile)
        {
            const char* pszLine = CPLReadLine2L(poGDS->fp, 100, 0);
            if (pszLine == NULL)
                return CE_Failure;
            poGDS->nLineNum ++;

            const char* pszPtr = pszLine;
            char ch;
            int nCol = 0;
            int bLastWasSep = TRUE;
            while((ch = *pszPtr) != '\0')
            {
                if (ch == ' ')
                {
                    if (!bLastWasSep)
                        nCol ++;
                    bLastWasSep = TRUE;
                }
                else if ((ch == ',' && poGDS->chDecimalSep != ',') || ch == '\t' || ch == ';')
                {
                    nCol ++;
                    bLastWasSep = TRUE;
                }
                else
                {
                    bLastWasSep = FALSE;
                }
                pszPtr ++;
            }

            /* Skip empty line */
            if (nCol == 0 && bLastWasSep)
                continue;

            poGDS->nDataLineNum ++;
        }
    }

    double dfExpectedY = poGDS->adfGeoTransform[3] + (0.5 + nBlockYOff) * poGDS->adfGeoTransform[5];
    int idx = -1;
    while(TRUE)
    {
        int nCol;
        int bLastWasSep;
        do
        {
            vsi_l_offset nOffsetBefore = VSIFTellL(poGDS->fp);
            const char* pszLine = CPLReadLine2L(poGDS->fp, 100, 0);
            if (pszLine == NULL)
            {
                if( poGDS->bSameNumberOfValuesPerLine )
                {
                    CPLError(CE_Failure, CPLE_AppDefined, "Cannot read line %d", poGDS->nLineNum + 1);
                    return CE_Failure;
                }
                else
                {
                    nLastYOff = nBlockYOff;
                    return CE_None;
                }
            }
            poGDS->nLineNum ++;

            const char* pszPtr = pszLine;
            char ch;
            nCol = 0;
            bLastWasSep = TRUE;
            double dfX = 0.0, dfY = 0.0, dfZ = 0.0;
            int bUsefulColsFound = 0;
            while((ch = *pszPtr) != '\0')
            {
                if (ch == ' ')
                {
                    if (!bLastWasSep)
                        nCol ++;
                    bLastWasSep = TRUE;
                }
                else if ((ch == ',' && poGDS->chDecimalSep != ',') || ch == '\t' || ch == ';')
                {
                    nCol ++;
                    bLastWasSep = TRUE;
                }
                else
                {
                    if (bLastWasSep)
                    {
                        if (nCol == poGDS->nXIndex)
                        {
                            bUsefulColsFound ++;
                            if( !poGDS->bSameNumberOfValuesPerLine )
                                dfX = CPLAtofDelim(pszPtr, poGDS->chDecimalSep);
                        }
                        else if (nCol == poGDS->nYIndex)
                        {
                            bUsefulColsFound ++;
                            if( !poGDS->bSameNumberOfValuesPerLine )
                                dfY = CPLAtofDelim(pszPtr, poGDS->chDecimalSep);
                        }
                        else if( nCol == poGDS->nZIndex)
                        {
                            bUsefulColsFound ++;
                            dfZ = CPLAtofDelim(pszPtr, poGDS->chDecimalSep);
                        }
                    }
                    bLastWasSep = FALSE;
                }
                pszPtr ++;
            }
            nCol ++;

            if( bUsefulColsFound == 3 )
            {
                if( poGDS->bSameNumberOfValuesPerLine )
                {
                    idx ++;
                }
                else
                {
                    if( fabs(dfY - dfExpectedY) > 1e-8 )
                    {
                        if( idx < 0 )
                        {
                            CPLError(CE_Failure, CPLE_AppDefined, "At line %d, found %f instead of %f for nBlockYOff = %d",
                                    poGDS->nLineNum, dfY, dfExpectedY, nBlockYOff);
                            return CE_Failure;
                        }
                        VSIFSeekL(poGDS->fp, nOffsetBefore, SEEK_SET);
                        nLastYOff = nBlockYOff;
                        poGDS->nLineNum --;
                        return CE_None;
                    }

                    idx = (int)((dfX - 0.5 * poGDS->adfGeoTransform[1] - poGDS->adfGeoTransform[0]) / poGDS->adfGeoTransform[1] + 0.5);
                }
                CPLAssert(idx >= 0 && idx < nRasterXSize);

                if( pImage )
                {
                    if (eDataType == GDT_Float32)
                    {
                        ((float*)pImage)[idx] = (float)dfZ;
                    }
                    else if (eDataType == GDT_Int32)
                    {
                        ((GInt32*)pImage)[idx] = (GInt32)dfZ;
                    }
                    else if (eDataType == GDT_Int16)
                    {
                        ((GInt16*)pImage)[idx] = (GInt16)dfZ;
                    }
                    else
                    {
                        ((GByte*)pImage)[idx] = (GByte)dfZ;
                    }
                }
            }
            /* Skip empty line */
        }
        while (nCol == 1 && bLastWasSep);

        poGDS->nDataLineNum ++;
        if (nCol < poGDS->nMinTokens)
            return CE_Failure;

        if( idx + 1 == nRasterXSize )
            break;
    }
    
    if( poGDS->bSameNumberOfValuesPerLine )
        CPLAssert(poGDS->nDataLineNum == (nBlockYOff + 1) * nBlockXSize);

    nLastYOff = nBlockYOff;

    return CE_None;
}

/************************************************************************/
/*                            GetMinimum()                              */
/************************************************************************/

double XYZRasterBand::GetMinimum(int *pbSuccess)
{
    XYZDataset *poGDS = (XYZDataset *) poDS;
    if( pbSuccess ) *pbSuccess = TRUE;
    return poGDS->dfMinZ;
}

/************************************************************************/
/*                            GetMaximum()                              */
/************************************************************************/

double XYZRasterBand::GetMaximum(int *pbSuccess)
{
    XYZDataset *poGDS = (XYZDataset *) poDS;
    if( pbSuccess ) *pbSuccess = TRUE;
    return poGDS->dfMaxZ;
}

/************************************************************************/
/*                          GetNoDataValue()                            */
/************************************************************************/

double XYZRasterBand::GetNoDataValue(int *pbSuccess)
{
    XYZDataset *poGDS = (XYZDataset *) poDS;
    if( !poGDS->bSameNumberOfValuesPerLine && 
        poGDS->dfMinZ > -32768 && eDataType != GDT_Byte )
    {
        if( pbSuccess ) *pbSuccess = TRUE;
        return (poGDS->dfMinZ > 0) ? 0 : -32768;
    }
    else if ( !poGDS->bSameNumberOfValuesPerLine && 
              poGDS->dfMinZ > 0 && eDataType == GDT_Byte )
    {
        if( pbSuccess ) *pbSuccess = TRUE;
        return 0;
    }
    return GDALPamRasterBand::GetNoDataValue(pbSuccess);
}

/************************************************************************/
/*                            ~XYZDataset()                            */
/************************************************************************/

XYZDataset::XYZDataset()
{
    fp = NULL;
    nDataLineNum = INT_MAX;
    nLineNum = 0;
    nCommentLineCount = 0;
    chDecimalSep = 0;
    bHasHeaderLine = FALSE;
    nXIndex = -1;
    nYIndex = -1;
    nZIndex = -1;
    nMinTokens = 0;
    adfGeoTransform[0] = 0;
    adfGeoTransform[1] = 1;
    adfGeoTransform[2] = 0;
    adfGeoTransform[3] = 0;
    adfGeoTransform[4] = 0;
    adfGeoTransform[5] = 1;
    bSameNumberOfValuesPerLine = TRUE;
    dfMinZ = 0;
    dfMaxZ = 0;
}

/************************************************************************/
/*                            ~XYZDataset()                            */
/************************************************************************/

XYZDataset::~XYZDataset()

{
    FlushCache();
    if (fp)
        VSIFCloseL(fp);
}

/************************************************************************/
/*                             Identify()                               */
/************************************************************************/

int XYZDataset::Identify( GDALOpenInfo * poOpenInfo )
{
    int bHasHeaderLine, nCommentLineCount;
    return IdentifyEx(poOpenInfo, bHasHeaderLine, nCommentLineCount);
}

/************************************************************************/
/*                            IdentifyEx()                              */
/************************************************************************/


int XYZDataset::IdentifyEx( GDALOpenInfo * poOpenInfo,
                            int& bHasHeaderLine,
                            int& nCommentLineCount)

{
    int         i;

    bHasHeaderLine = FALSE;
    nCommentLineCount = 0;

    CPLString osFilename(poOpenInfo->pszFilename);

    GDALOpenInfo* poOpenInfoToDelete = NULL;
    /*  GZipped .xyz files are common, so automagically open them */
    /*  if the /vsigzip/ has not been explicitely passed */
    if (strlen(poOpenInfo->pszFilename) > 6 &&
        EQUAL(poOpenInfo->pszFilename + strlen(poOpenInfo->pszFilename) - 6, "xyz.gz") &&
        !EQUALN(poOpenInfo->pszFilename, "/vsigzip/", 9))
    {
        osFilename = "/vsigzip/";
        osFilename += poOpenInfo->pszFilename;
        poOpenInfo = poOpenInfoToDelete =
                new GDALOpenInfo(osFilename.c_str(), GA_ReadOnly,
                                 poOpenInfo->papszSiblingFiles);
    }

    if (poOpenInfo->nHeaderBytes == 0)
    {
        delete poOpenInfoToDelete;
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Chech that it looks roughly as a XYZ dataset                    */
/* -------------------------------------------------------------------- */
    const char* pszData = (const char*)poOpenInfo->pabyHeader;

    /* Skip comments line at the beginning such as in */
    /* http://pubs.usgs.gov/of/2003/ofr-03-230/DATA/NSLCU.XYZ */
    i=0;
    if (pszData[i] == '/')
    {
        nCommentLineCount ++;

        i++;
        for(;i<poOpenInfo->nHeaderBytes;i++)
        {
            char ch = pszData[i];
            if (ch == 13 || ch == 10)
            {
                if (ch == 13 && pszData[i+1] == 10)
                    i++;
                if (pszData[i+1] == '/')
                {
                    nCommentLineCount ++;
                    i++;
                }
                else
                    break;
            }
        }
    }

    for(;i<poOpenInfo->nHeaderBytes;i++)
    {
        char ch = pszData[i];
        if (ch == 13 || ch == 10)
        {
            break;
        }
        else if (ch == ' ' || ch == ',' || ch == '\t' || ch == ';')
            ;
        else if ((ch >= '0' && ch <= '9') || ch == '.' || ch == '+' ||
                 ch == '-' || ch == 'e' || ch == 'E')
            ;
        else if (ch == '"' || (ch >= 'a' && ch <= 'z') ||
                              (ch >= 'A' && ch <= 'Z'))
            bHasHeaderLine = TRUE;
        else
        {
            delete poOpenInfoToDelete;
            return FALSE;
        }
    }
    int bHasFoundNewLine = FALSE;
    int bPrevWasSep = TRUE;
    int nCols = 0;
    int nMaxCols = 0;
    for(;i<poOpenInfo->nHeaderBytes;i++)
    {
        char ch = pszData[i];
        if (ch == 13 || ch == 10)
        {
            bHasFoundNewLine = TRUE;
            if (!bPrevWasSep)
            {
                nCols ++;
                if (nCols > nMaxCols)
                    nMaxCols = nCols;
            }
            bPrevWasSep = TRUE;
            nCols = 0;
        }
        else if (ch == ' ' || ch == ',' || ch == '\t' || ch == ';')
        {
            if (!bPrevWasSep)
            {
                nCols ++;
                if (nCols > nMaxCols)
                    nMaxCols = nCols;
            }
            bPrevWasSep = TRUE;
        }
        else if ((ch >= '0' && ch <= '9') || ch == '.' || ch == '+' ||
                 ch == '-' || ch == 'e' || ch == 'E')
        {
            bPrevWasSep = FALSE;
        }
        else
        {
            delete poOpenInfoToDelete;
            return FALSE;
        }
    }

    delete poOpenInfoToDelete;
    return bHasFoundNewLine && nMaxCols >= 3;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *XYZDataset::Open( GDALOpenInfo * poOpenInfo )

{
    int         i;
    int         bHasHeaderLine;
    int         nCommentLineCount = 0;

    if (!IdentifyEx(poOpenInfo, bHasHeaderLine, nCommentLineCount))
        return NULL;

    CPLString osFilename(poOpenInfo->pszFilename);

    /*  GZipped .xyz files are common, so automagically open them */
    /*  if the /vsigzip/ has not been explicitely passed */
    if (strlen(poOpenInfo->pszFilename) > 6 &&
        EQUAL(poOpenInfo->pszFilename + strlen(poOpenInfo->pszFilename) - 6, "xyz.gz") &&
        !EQUALN(poOpenInfo->pszFilename, "/vsigzip/", 9))
    {
        osFilename = "/vsigzip/";
        osFilename += poOpenInfo->pszFilename;
    }

/* -------------------------------------------------------------------- */
/*      Find dataset characteristics                                    */
/* -------------------------------------------------------------------- */
    VSILFILE* fp = VSIFOpenL(osFilename.c_str(), "rb");
    if (fp == NULL)
        return NULL;

    /* For better performance of CPLReadLine2L() we create a buffered reader */
    /* (except for /vsigzip/ since it has one internally) */
    if (!EQUALN(poOpenInfo->pszFilename, "/vsigzip/", 9))
        fp = (VSILFILE*) VSICreateBufferedReaderHandle((VSIVirtualHandle*)fp);
    
    const char* pszLine;
    int nXIndex = -1, nYIndex = -1, nZIndex = -1;
    int nMinTokens = 0;

    for(i=0;i<nCommentLineCount;i++)
        CPLReadLine2L(fp, 100, NULL);

/* -------------------------------------------------------------------- */
/*      Parse header line                                               */
/* -------------------------------------------------------------------- */
    if (bHasHeaderLine)
    {
        pszLine = CPLReadLine2L(fp, 100, NULL);
        if (pszLine == NULL)
        {
            VSIFCloseL(fp);
            return NULL;
        }
        char** papszTokens = CSLTokenizeString2( pszLine, " ,\t;",
                                                 CSLT_HONOURSTRINGS );
        int nTokens = CSLCount(papszTokens);
        if (nTokens < 3)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "At line %d, found %d tokens. Expected 3 at least",
                      1, nTokens);
            CSLDestroy(papszTokens);
            VSIFCloseL(fp);
            return NULL;
        }
        int i;
        for(i=0;i<nTokens;i++)
        {
            if (EQUAL(papszTokens[i], "x") ||
                EQUALN(papszTokens[i], "lon", 3) ||
                EQUALN(papszTokens[i], "east", 4))
                nXIndex = i;
            else if (EQUAL(papszTokens[i], "y") ||
                     EQUALN(papszTokens[i], "lat", 3) ||
                     EQUALN(papszTokens[i], "north", 5))
                nYIndex = i;
            else if (EQUAL(papszTokens[i], "z") ||
                     EQUALN(papszTokens[i], "alt", 3) ||
                     EQUAL(papszTokens[i], "height"))
                nZIndex = i;
        }
        CSLDestroy(papszTokens);
        papszTokens = NULL;
        if (nXIndex < 0 || nYIndex < 0 || nZIndex < 0)
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "Could not find one of the X, Y or Z column names in header line. Defaulting to the first 3 columns");
            nXIndex = 0;
            nYIndex = 1;
            nZIndex = 2;
        }
        nMinTokens = 1 + MAX(MAX(nXIndex, nYIndex), nZIndex);
    }
    else
    {
        nXIndex = 0;
        nYIndex = 1;
        nZIndex = 2;
        nMinTokens = 3;
    }
    
/* -------------------------------------------------------------------- */
/*      Parse data lines                                                */
/* -------------------------------------------------------------------- */

    int nXSize = 0, nYSize = 0;
    int nLineNum = 0;
    int nDataLineNum = 0;
    double dfX = 0, dfY = 0, dfZ = 0;
    double dfMinX = 0, dfMinY = 0, dfMaxX = 0, dfMaxY = 0;
    double dfMinZ = 0, dfMaxZ = 0;
    double dfLastX = 0, dfLastY = 0;
    double dfStepX = 0, dfStepY = 0;
    GDALDataType eDT = GDT_Byte;
    int bSameNumberOfValuesPerLine = TRUE;
    char chDecimalSep = '\0';
    while((pszLine = CPLReadLine2L(fp, 100, NULL)) != NULL)
    {
        nLineNum ++;

        const char* pszPtr = pszLine;
        char ch;
        int nCol = 0;
        int bLastWasSep = TRUE;
        if( chDecimalSep == '\0' )
        {
            int nCountComma = 0;
            int nCountFieldSep = 0;
            while((ch = *pszPtr) != '\0')
            {
                if( ch == '.' )
                {
                    chDecimalSep = '.';
                    break;
                }
                else if( ch == ',' )
                {
                    nCountComma ++;
                    bLastWasSep = FALSE;
                }
                else if( ch == ' ' )
                {
                    if (!bLastWasSep)
                        nCountFieldSep ++;
                    bLastWasSep = TRUE;
                }
                else if( ch == '\t' || ch == ';' )
                {
                    nCountFieldSep ++;
                    bLastWasSep = TRUE;
                }
                else
                    bLastWasSep = FALSE;
                pszPtr ++;
            }
            if( chDecimalSep == '\0' )
            {
                /* 1,2,3 */
                if( nCountComma >= 2 && nCountFieldSep == 0 )
                    chDecimalSep = '.';
                /* 23,5;33;45 */
                else if ( nCountComma > 0 && nCountFieldSep > 0 )
                    chDecimalSep = ',';
            }
            pszPtr = pszLine;
            bLastWasSep = TRUE;
        }

        char chLocalDecimalSep = chDecimalSep ? chDecimalSep : '.';
        while((ch = *pszPtr) != '\0')
        {
            if (ch == ' ')
            {
                if (!bLastWasSep)
                    nCol ++;
                bLastWasSep = TRUE;
            }
            else if ((ch == ',' && chLocalDecimalSep != ',') || ch == '\t' || ch == ';')
            {
                nCol ++;
                bLastWasSep = TRUE;
            }
            else
            {
                if (bLastWasSep)
                {
                    if (nCol == nXIndex)
                        dfX = CPLAtofDelim(pszPtr, chLocalDecimalSep);
                    else if (nCol == nYIndex)
                        dfY = CPLAtofDelim(pszPtr, chLocalDecimalSep);
                    else if (nCol == nZIndex && eDT != GDT_Float32)
                    {
                        dfZ = CPLAtofDelim(pszPtr, chLocalDecimalSep);
                        if( nDataLineNum == 0 )
                            dfMinZ = dfMaxZ = dfZ;
                        else if( dfZ < dfMinZ )
                            dfMinZ = dfZ;
                        else if( dfZ > dfMaxZ )
                            dfMaxZ = dfZ;
                        int nZ = (int)dfZ;
                        if ((double)nZ != dfZ)
                        {
                            eDT = GDT_Float32;
                        }
                        else if ((eDT == GDT_Byte || eDT == GDT_Int16) && (nZ < 0 || nZ > 255))
                        {
                            if (nZ < -32768 || nZ > 32767)
                                eDT = GDT_Int32;
                            else
                                eDT = GDT_Int16;
                        }
                    }
                }
                bLastWasSep = FALSE;
            }
            pszPtr ++;
        }
        /* skip empty lines */
        if (bLastWasSep && nCol == 0)
        {
            continue;
        }
        nDataLineNum ++;
        nCol ++;
        if (nCol < nMinTokens)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "At line %d, found %d tokens. Expected %d at least",
                      nLineNum, nCol, nMinTokens);
            VSIFCloseL(fp);
            return NULL;
        }

        if (nDataLineNum == 2)
        {
            dfStepX = dfX - dfLastX;
            if (dfStepX <= 0)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Ungridded dataset: At line %d, X spacing was %f. Expected >0 value",
                         nLineNum, dfStepX);
                VSIFCloseL(fp);
                return NULL;
            }
        }
        else if (nDataLineNum > 2)
        {
            //double dfNewStepX = dfX - dfLastX;
            double dfNewStepY = dfY - dfLastY;
            if (dfNewStepY != 0)
            {
                nYSize ++;
                if (dfStepY == 0)
                {
                    nXSize = nDataLineNum - 1;
                    double dfAdjustedStepX = (dfMaxX - dfMinX) / (nXSize - 1);
                    if (fabs(dfStepX - dfAdjustedStepX) > 1e-8)
                    {
                        CPLDebug("XYZ", "Adjusting stepx from %f to %f", dfStepX, dfAdjustedStepX);
                    }
                    dfStepX = dfAdjustedStepX;
                }
                if (fabs(dfX - dfMinX) > 1e-8)
                {
                    if( dfX < dfMinX && fmod(dfMinX - dfX, dfStepX) < 1e-8 )
                    {
                        bSameNumberOfValuesPerLine = FALSE;
                        //CPLDebug("XYZ", "Adjusting minx to %f", dfX);
                        dfMinX = dfX;
                        nXSize = 1 + (int)((dfMaxX - dfMinX) / dfStepX + 1e-5 );
                    }
                    else if( !(dfX > dfMinX && fmod(dfX - dfMinX, dfStepX) < 1e-8) )
                    {
                        CPLError(CE_Failure, CPLE_AppDefined,
                                "Ungridded dataset: At line %d, X is %f, where as %f was expected (1)",
                                nLineNum, dfX, dfMinX);
                        VSIFCloseL(fp);
                        return NULL;
                    }
                }
                if (fabs(dfLastX - dfMaxX) > 1e-8)
                {
                    if( dfLastX > dfMaxX && fmod(dfLastX - dfMaxX, dfStepX) < 1e-8 )
                    {
                        bSameNumberOfValuesPerLine = FALSE;
                        //CPLDebug("XYZ", "Adjusting maxx to %f", dfLastX);
                        dfMaxX = dfLastX;
                        nXSize = 1 + (int)((dfMaxX - dfMinX) / dfStepX + 1e-5 );
                    }
                    else if( !(dfLastX < dfMaxX && fmod(dfMaxX - dfLastX, dfStepX) < 1e-8) )
                    {
                        CPLError(CE_Failure, CPLE_AppDefined,
                                "Ungridded dataset: At line %d, X is %f, where as %f was expected (2)",
                                nLineNum - 1, dfLastX, dfMaxX);
                        VSIFCloseL(fp);
                        return NULL;
                    }
                }
                /*if (dfStepY != 0 && fabs(dfNewStepY - dfStepY) > 1e-8)
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Ungridded dataset: At line %d, Y spacing was %f, whereas it was %f before",
                             nLineNum, dfNewStepY, dfStepY);
                    VSIFCloseL(fp);
                    return NULL;
                }*/
                dfStepY = dfNewStepY;
            }
            //else if (dfNewStepX != 0)
            //{
                /*if (dfStepX != 0 && fabs(dfNewStepX - dfStepX) > 1e-8)
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "At line %d, X spacing was %f, whereas it was %f before",
                             nLineNum, dfNewStepX, dfStepX);
                    VSIFCloseL(fp);
                    return NULL;
                }*/
            //}
        }

        if (nDataLineNum == 1)
        {
            dfMinX = dfMaxX = dfX;
            dfMinY = dfMaxY = dfY;
        }
        else
        {
            if (dfStepY == 0 && dfX < dfMinX) dfMinX = dfX;
            if (dfStepY == 0 && dfX > dfMaxX) dfMaxX = dfX;
            if (dfY < dfMinY) dfMinY = dfY;
            if (dfY > dfMaxY) dfMaxY = dfY;
        }

        dfLastX = dfX;
        dfLastY = dfY;
    }
    nYSize ++;

    if (dfStepX == 0)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Couldn't determine X spacing");
        VSIFCloseL(fp);
        return NULL;
    }

    if (dfStepY == 0)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Couldn't determine Y spacing");
        VSIFCloseL(fp);
        return NULL;
    }

    double dfAdjustedStepY = ((dfStepY < 0) ? -1 : 1) * (dfMaxY - dfMinY) / (nYSize - 1);
    if (fabs(dfStepY - dfAdjustedStepY) > 1e-8)
    {
        CPLDebug("XYZ", "Adjusting stepy from %f to %f", dfStepY, dfAdjustedStepY);
    }
    dfStepY = dfAdjustedStepY;

    //CPLDebug("XYZ", "minx=%f maxx=%f stepx=%f", dfMinX, dfMaxX, dfStepX);
    //CPLDebug("XYZ", "miny=%f maxy=%f stepy=%f", dfMinY, dfMaxY, dfStepY);

    if (bSameNumberOfValuesPerLine && nDataLineNum != nXSize * nYSize)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Found %d lines. Expected %d",
                 nDataLineNum,nXSize * nYSize);
        VSIFCloseL(fp);
        return NULL;
    }
    
    if (poOpenInfo->eAccess == GA_Update)
    {
        CPLError( CE_Failure, CPLE_NotSupported, 
                  "The XYZ driver does not support update access to existing"
                  " datasets.\n" );
        VSIFCloseL(fp);
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */
    XYZDataset         *poDS;

    poDS = new XYZDataset();
    poDS->fp = fp;
    poDS->bHasHeaderLine = bHasHeaderLine;
    poDS->nCommentLineCount = nCommentLineCount;
    poDS->chDecimalSep = chDecimalSep ? chDecimalSep : '.';
    poDS->nXIndex = nXIndex;
    poDS->nYIndex = nYIndex;
    poDS->nZIndex = nZIndex;
    poDS->nMinTokens = nMinTokens;
    poDS->nRasterXSize = nXSize;
    poDS->nRasterYSize = nYSize;
    poDS->adfGeoTransform[0] = dfMinX - dfStepX / 2;
    poDS->adfGeoTransform[1] = dfStepX;
    poDS->adfGeoTransform[3] = (dfStepY < 0) ? dfMaxY - dfStepY / 2 :
                                               dfMinY - dfStepY / 2;
    poDS->adfGeoTransform[5] = dfStepY;
    poDS->bSameNumberOfValuesPerLine = bSameNumberOfValuesPerLine;
    poDS->dfMinZ = dfMinZ;
    poDS->dfMaxZ = dfMaxZ;
    //CPLDebug("XYZ", "bSameNumberOfValuesPerLine = %d", bSameNumberOfValuesPerLine);

    if (!GDALCheckDatasetDimensions(poDS->nRasterXSize, poDS->nRasterYSize))
    {
        delete poDS;
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Create band information objects.                                */
/* -------------------------------------------------------------------- */
    poDS->nBands = 1;
    for( i = 0; i < poDS->nBands; i++ )
        poDS->SetBand( i+1, new XYZRasterBand( poDS, i+1, eDT ) );

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
/*                             CreateCopy()                             */
/************************************************************************/

GDALDataset* XYZDataset::CreateCopy( const char * pszFilename,
                                     GDALDataset *poSrcDS, 
                                     int bStrict, char ** papszOptions, 
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
                  "XYZ driver does not support source dataset with zero band.\n");
        return NULL;
    }

    if (nBands != 1)
    {
        CPLError( (bStrict) ? CE_Failure : CE_Warning, CPLE_NotSupported, 
                  "XYZ driver only uses the first band of the dataset.\n");
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
    double adfGeoTransform[6];
    poSrcDS->GetGeoTransform(adfGeoTransform);
    if (adfGeoTransform[2] != 0 || adfGeoTransform[4] != 0)
    {
        CPLError( CE_Failure, CPLE_NotSupported, 
                  "XYZ driver does not support CreateCopy() from skewed or rotated dataset.\n");
        return NULL;
    }

    GDALDataType eSrcDT = poSrcDS->GetRasterBand(1)->GetRasterDataType();
    GDALDataType eReqDT;
    if (eSrcDT == GDT_Byte || eSrcDT == GDT_Int16 ||
        eSrcDT == GDT_UInt16 || eSrcDT == GDT_Int32)
        eReqDT = GDT_Int32;
    else
        eReqDT = GDT_Float32;

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

/* -------------------------------------------------------------------- */
/*      Read creation options                                           */
/* -------------------------------------------------------------------- */
    const char* pszColSep =
            CSLFetchNameValue(papszOptions, "COLUMN_SEPARATOR");
    if (pszColSep == NULL)
        pszColSep = " ";
    else if (EQUAL(pszColSep, "COMMA"))
        pszColSep = ",";
    else if (EQUAL(pszColSep, "SPACE"))
        pszColSep = " ";
    else if (EQUAL(pszColSep, "SEMICOLON"))
        pszColSep = ";";
    else if (EQUAL(pszColSep, "\\t") || EQUAL(pszColSep, "TAB"))
        pszColSep = "\t";

    const char* pszAddHeaderLine =
            CSLFetchNameValue(papszOptions, "ADD_HEADER_LINE");
    if (pszAddHeaderLine != NULL && CSLTestBoolean(pszAddHeaderLine))
    {
        VSIFPrintfL(fp, "X%sY%sZ\n", pszColSep, pszColSep);
    }

/* -------------------------------------------------------------------- */
/*      Copy imagery                                                    */
/* -------------------------------------------------------------------- */
    void* pLineBuffer = (void*) CPLMalloc(nXSize * sizeof(int));
    int i, j;
    CPLErr eErr = CE_None;
    for(j=0;j<nYSize && eErr == CE_None;j++)
    {
        eErr = poSrcDS->GetRasterBand(1)->RasterIO(
                                            GF_Read, 0, j, nXSize, 1,
                                            pLineBuffer, nXSize, 1,
                                            eReqDT, 0, 0);
        if (eErr != CE_None)
            break;
        double dfY = adfGeoTransform[3] + (j + 0.5) * adfGeoTransform[5];
        CPLString osBuf;
        for(i=0;i<nXSize;i++)
        {
            char szBuf[256];
            double dfX = adfGeoTransform[0] + (i + 0.5) * adfGeoTransform[1];
            if (eReqDT == GDT_Int32)
                sprintf(szBuf, "%.18g%c%.18g%c%d\n", dfX, pszColSep[0], dfY, pszColSep[0], ((int*)pLineBuffer)[i]);
            else
                sprintf(szBuf, "%.18g%c%.18g%c%.18g\n", dfX, pszColSep[0], dfY, pszColSep[0], ((float*)pLineBuffer)[i]);
            osBuf += szBuf;
            if( (i & 1023) == 0 || i == nXSize - 1 )
            {
                if ( VSIFWriteL( osBuf, (int)osBuf.size(), 1, fp ) != 1 )
                {
                    eErr = CE_Failure;
                    CPLError( CE_Failure, CPLE_AppDefined, 
                              "Write failed, disk full?\n" );
                    break;
                }
                osBuf = "";
            }
        }
        if (!pfnProgress( (j+1) * 1.0 / nYSize, NULL, pProgressData))
        {
            eErr = CE_Failure;
            break;
        }
    }
    CPLFree(pLineBuffer);
    VSIFCloseL(fp);
    
    if (eErr != CE_None)
        return NULL;

/* -------------------------------------------------------------------- */
/*      We don't want to call GDALOpen() since it will be expensive,    */
/*      so we "hand prepare" an XYZ dataset referencing our file.       */
/* -------------------------------------------------------------------- */
    XYZDataset* poXYZ_DS = new XYZDataset();
    poXYZ_DS->nRasterXSize = nXSize;
    poXYZ_DS->nRasterYSize = nYSize;
    poXYZ_DS->nBands = 1;
    poXYZ_DS->SetBand( 1, new XYZRasterBand( poXYZ_DS, 1, eReqDT ) );
    /* If outputing to stdout, we can't reopen it --> silence warning */
    CPLPushErrorHandler(CPLQuietErrorHandler);
    poXYZ_DS->fp = VSIFOpenL( pszFilename, "rb" );
    CPLPopErrorHandler();
    memcpy( &(poXYZ_DS->adfGeoTransform), adfGeoTransform, sizeof(double)*6 );
    poXYZ_DS->nXIndex = 0;
    poXYZ_DS->nYIndex = 1;
    poXYZ_DS->nZIndex = 2;
    if( pszAddHeaderLine )
    {
        poXYZ_DS->nDataLineNum = 1;
        poXYZ_DS->bHasHeaderLine = TRUE;
    }

    return poXYZ_DS;
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr XYZDataset::GetGeoTransform( double * padfTransform )

{
    memcpy(padfTransform, adfGeoTransform, 6 * sizeof(double));

    return( CE_None );
}

/************************************************************************/
/*                         GDALRegister_XYZ()                           */
/************************************************************************/

void GDALRegister_XYZ()

{
    GDALDriver  *poDriver;

    if( GDALGetDriverByName( "XYZ" ) == NULL )
    {
        poDriver = new GDALDriver();
        
        poDriver->SetDescription( "XYZ" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, 
                                   "ASCII Gridded XYZ" );
        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, 
                                   "frmt_xyz.html" );
        poDriver->SetMetadataItem( GDAL_DMD_EXTENSION, "xyz" );
        poDriver->SetMetadataItem( GDAL_DMD_CREATIONOPTIONLIST,
"<CreationOptionList>"
"   <Option name='COLUMN_SEPARATOR' type='string' default=' ' description='Separator between fields.'/>"
"   <Option name='ADD_HEADER_LINE' type='boolean' default='false' description='Add an header line with column names.'/>"
"</CreationOptionList>");

        poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );

        poDriver->pfnOpen = XYZDataset::Open;
        poDriver->pfnIdentify = XYZDataset::Identify;
        poDriver->pfnCreateCopy = XYZDataset::CreateCopy;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}

