/******************************************************************************
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

#include "cpl_string.h"
#include "cpl_vsi_virtual.h"
#include "gdal_frmts.h"
#include "gdal_pam.h"

#include <algorithm>
#include <vector>

CPL_CVSID("$Id$")

static const double RELATIVE_ERROR = 1e-3;

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
    GIntBig     nLineNum;     /* any line */
    GIntBig     nDataLineNum; /* line with values (header line and empty lines ignored) */
    double      adfGeoTransform[6];
    int         bSameNumberOfValuesPerLine;
    double      dfMinZ;
    double      dfMaxZ;
    bool        bEOF;

    static int          IdentifyEx( GDALOpenInfo *, int&, int& nCommentLineCount );

  public:
                 XYZDataset();
    virtual     ~XYZDataset();

    virtual CPLErr GetGeoTransform( double * ) override;

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

    virtual CPLErr IReadBlock( int, int, void * ) override;
    virtual double GetMinimum( int *pbSuccess = NULL ) override;
    virtual double GetMaximum( int *pbSuccess = NULL ) override;
    virtual double GetNoDataValue( int *pbSuccess = NULL ) override;
};

/************************************************************************/
/*                           XYZRasterBand()                            */
/************************************************************************/

XYZRasterBand::XYZRasterBand( XYZDataset *poDSIn, int nBandIn, GDALDataType eDT ) :
    nLastYOff(-1)
{
    poDS = poDSIn;
    nBand = nBandIn;

    eDataType = eDT;

    nBlockXSize = poDSIn->GetRasterXSize();
    nBlockYSize = 1;
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr XYZRasterBand::IReadBlock( CPL_UNUSED int nBlockXOff,
                                  int nBlockYOff,
                                  void * pImage )
{
    XYZDataset *poGDS = reinterpret_cast<XYZDataset *>( poDS );

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

    // Only valid if bSameNumberOfValuesPerLine.
    const GIntBig nLineInFile = static_cast<GIntBig>(nBlockYOff) * nBlockXSize;
    if ( (poGDS->bSameNumberOfValuesPerLine && poGDS->nDataLineNum > nLineInFile) ||
         (!poGDS->bSameNumberOfValuesPerLine && (nLastYOff == -1 || nBlockYOff == 0)) )
    {
        poGDS->nDataLineNum = 0;
        poGDS->nLineNum = 0;
        poGDS->bEOF = false;
        VSIFSeekL(poGDS->fp, 0, SEEK_SET);

        for(int i=0;i<poGDS->nCommentLineCount;i++)
        {
            if( CPLReadLine2L(poGDS->fp, 100, NULL) == NULL )
            {
                poGDS->bEOF = true;
                return CE_Failure;
            }
            poGDS->nLineNum ++;
        }

        if (poGDS->bHasHeaderLine)
        {
            const char* pszLine = CPLReadLine2L(poGDS->fp, 100, NULL);
            if (pszLine == NULL)
            {
                poGDS->bEOF = true;
                return CE_Failure;
            }
            poGDS->nLineNum ++;
        }
    }

    if( !poGDS->bSameNumberOfValuesPerLine )
    {
        if( nBlockYOff < nLastYOff )
        {
            nLastYOff = -1;
            for( int iY = 0; iY < nBlockYOff; iY++ )
            {
                if( IReadBlock(0, iY, NULL) != CE_None )
                    return CE_Failure;
            }
        }
        else
        {
            if( poGDS->bEOF )
            {
                return CE_Failure;
            }
            for( int iY = nLastYOff + 1; iY < nBlockYOff; iY++ )
            {
                if( IReadBlock(0, iY, NULL) != CE_None )
                    return CE_Failure;
            }
        }
    }
    else
    {
        if( poGDS->bEOF )
        {
            return CE_Failure;
        }
        while(poGDS->nDataLineNum < nLineInFile)
        {
            const char* pszLine = CPLReadLine2L(poGDS->fp, 100, NULL);
            if (pszLine == NULL)
            {
                poGDS->bEOF = true;
                return CE_Failure;
            }
            poGDS->nLineNum ++;

            const char* pszPtr = pszLine;
            char ch;
            int nCol = 0;
            bool bLastWasSep = true;
            while((ch = *pszPtr) != '\0')
            {
                if (ch == ' ')
                {
                    if (!bLastWasSep)
                        nCol ++;
                    bLastWasSep = true;
                }
                else if ((ch == ',' && poGDS->chDecimalSep != ',') || ch == '\t' || ch == ';')
                {
                    nCol ++;
                    bLastWasSep = true;
                }
                else
                {
                    bLastWasSep = false;
                }
                pszPtr ++;
            }

            /* Skip empty line */
            if (nCol == 0 && bLastWasSep)
                continue;

            poGDS->nDataLineNum ++;
        }
    }

    const double dfExpectedY
        = poGDS->adfGeoTransform[3] +
        (0.5 + nBlockYOff) * poGDS->adfGeoTransform[5];

    int idx = -1;
    while(true)
    {
        int nCol;
        bool bLastWasSep;
        do
        {
            const vsi_l_offset nOffsetBefore = VSIFTellL(poGDS->fp);
            const char* pszLine = CPLReadLine2L(poGDS->fp, 100, NULL);
            if (pszLine == NULL)
            {
                poGDS->bEOF = true;
                if( poGDS->bSameNumberOfValuesPerLine )
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Cannot read line " CPL_FRMT_GIB, poGDS->nLineNum + 1);
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
            bLastWasSep = true;
            double dfX = 0.0;
            double dfY = 0.0;
            double dfZ = 0.0;
            int nUsefulColsFound = 0;
            while((ch = *pszPtr) != '\0')
            {
                if (ch == ' ')
                {
                    if (!bLastWasSep)
                        nCol ++;
                    bLastWasSep = true;
                }
                else if ( ( ch == ',' && poGDS->chDecimalSep != ',' )
                          || ch == '\t' || ch == ';' )
                {
                    nCol ++;
                    bLastWasSep = true;
                }
                else
                {
                    if (bLastWasSep)
                    {
                        if (nCol == poGDS->nXIndex)
                        {
                            nUsefulColsFound ++;
                            if( !poGDS->bSameNumberOfValuesPerLine )
                                dfX = CPLAtofDelim(pszPtr, poGDS->chDecimalSep);
                        }
                        else if (nCol == poGDS->nYIndex)
                        {
                            nUsefulColsFound ++;
                            if( !poGDS->bSameNumberOfValuesPerLine )
                                dfY = CPLAtofDelim(pszPtr, poGDS->chDecimalSep);
                        }
                        else if( nCol == poGDS->nZIndex)
                        {
                            nUsefulColsFound ++;
                            dfZ = CPLAtofDelim(pszPtr, poGDS->chDecimalSep);
                        }
                    }
                    bLastWasSep = false;
                }
                pszPtr ++;
            }
            nCol ++;

            if( nUsefulColsFound == 3 )
            {
                if( poGDS->bSameNumberOfValuesPerLine )
                {
                    idx ++;
                }
                else
                {
                    if( fabs( (dfY - dfExpectedY) / poGDS->adfGeoTransform[5] ) > RELATIVE_ERROR )
                    {
                        if( idx < 0 )
                        {
                            CPLError( CE_Failure, CPLE_AppDefined,
                                      "At line " CPL_FRMT_GIB", found %f instead of %f "
                                      "for nBlockYOff = %d",
                                      poGDS->nLineNum, dfY, dfExpectedY,
                                      nBlockYOff);
                            return CE_Failure;
                        }
                        VSIFSeekL(poGDS->fp, nOffsetBefore, SEEK_SET);
                        nLastYOff = nBlockYOff;
                        poGDS->nLineNum --;
                        return CE_None;
                    }

                    idx = static_cast<int>(
                        ( dfX - 0.5 * poGDS->adfGeoTransform[1]
                          - poGDS->adfGeoTransform[0] )
                        / poGDS->adfGeoTransform[1] + 0.5 );
                }
                CPLAssert(idx >= 0 && idx < nRasterXSize);

                if( pImage )
                {
                    if (eDataType == GDT_Float32)
                    {
                        reinterpret_cast<float *>( pImage )[idx]
                            = static_cast<float>(dfZ);
                    }
                    else if (eDataType == GDT_Int32)
                    {
                        reinterpret_cast<GInt32 *>( pImage )[idx]
                            = static_cast<GInt32>( dfZ );
                    }
                    else if (eDataType == GDT_Int16)
                    {
                        reinterpret_cast<GInt16 *>( pImage )[idx]
                            = static_cast<GInt16>( dfZ );
                    }
                    else
                    {
                        reinterpret_cast<GByte *>( pImage )[idx]
                            = static_cast<GByte>( dfZ );
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

    if( poGDS->bSameNumberOfValuesPerLine ) {
        if( poGDS->nDataLineNum != static_cast<GIntBig>(nBlockYOff + 1) * nBlockXSize )
        {
            CPLError(CE_Failure, CPLE_AssertionFailed,
                     "The file has not the same number of values per "
                     "line as initialy thought. It must be somehow corrupted");
            return CE_Failure;
        }
    }

    nLastYOff = nBlockYOff;

    return CE_None;
}

/************************************************************************/
/*                            GetMinimum()                              */
/************************************************************************/

double XYZRasterBand::GetMinimum(int *pbSuccess)
{
    XYZDataset *poGDS = reinterpret_cast<XYZDataset *>( poDS );
    if( pbSuccess )
        *pbSuccess = TRUE;
    return poGDS->dfMinZ;
}

/************************************************************************/
/*                            GetMaximum()                              */
/************************************************************************/

double XYZRasterBand::GetMaximum(int *pbSuccess)
{
    XYZDataset *poGDS = reinterpret_cast<XYZDataset *>( poDS );
    if( pbSuccess )
        *pbSuccess = TRUE;
    return poGDS->dfMaxZ;
}

/************************************************************************/
/*                          GetNoDataValue()                            */
/************************************************************************/

double XYZRasterBand::GetNoDataValue(int *pbSuccess)
{
    XYZDataset *poGDS = reinterpret_cast<XYZDataset *>( poDS );
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

XYZDataset::XYZDataset() :
    fp(NULL),
    bHasHeaderLine(FALSE),
    nCommentLineCount(0),
    chDecimalSep('.'),
    nXIndex(-1),
    nYIndex(-1),
    nZIndex(-1),
    nMinTokens(0),
    nLineNum(0),
    nDataLineNum(GINTBIG_MAX),
    bSameNumberOfValuesPerLine(TRUE),
    dfMinZ(0),
    dfMaxZ(0),
    bEOF(false)
{
    adfGeoTransform[0] = 0;
    adfGeoTransform[1] = 1;
    adfGeoTransform[2] = 0;
    adfGeoTransform[3] = 0;
    adfGeoTransform[4] = 0;
    adfGeoTransform[5] = 1;
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
    bHasHeaderLine = FALSE;
    nCommentLineCount = 0;

    CPLString osFilename(poOpenInfo->pszFilename);

    GDALOpenInfo* poOpenInfoToDelete = NULL;
    /*  GZipped .xyz files are common, so automagically open them */
    /*  if the /vsigzip/ has not been explicitly passed */
    if (strlen(poOpenInfo->pszFilename) > 6 &&
        EQUAL(poOpenInfo->pszFilename + strlen(poOpenInfo->pszFilename) - 6, "xyz.gz") &&
        !STARTS_WITH_CI(poOpenInfo->pszFilename, "/vsigzip/"))
    {
        osFilename = "/vsigzip/";
        osFilename += poOpenInfo->pszFilename;
        poOpenInfo = poOpenInfoToDelete =
                new GDALOpenInfo(osFilename.c_str(), GA_ReadOnly,
                                 poOpenInfo->GetSiblingFiles());
    }

    if (poOpenInfo->nHeaderBytes == 0)
    {
        delete poOpenInfoToDelete;
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Chech that it looks roughly as a XYZ dataset                    */
/* -------------------------------------------------------------------- */
    const char* pszData
        = reinterpret_cast<const char *>( poOpenInfo->pabyHeader );

    if( poOpenInfo->nHeaderBytes >= 4 && STARTS_WITH(pszData, "DSAA") )
    {
        // Do not match GSAG datasets
        delete poOpenInfoToDelete;
        return FALSE;
    }

    /* Skip comments line at the beginning such as in */
    /* http://pubs.usgs.gov/of/2003/ofr-03-230/DATA/NSLCU.XYZ */
    int i = 0;
    if (pszData[i] == '/')
    {
        nCommentLineCount ++;

        i++;
        for( ; i < poOpenInfo->nHeaderBytes; i++)
        {
            const char ch = pszData[i];
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

    for( ; i < poOpenInfo->nHeaderBytes; i++ )
    {
        const char ch = pszData[i];
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
    bool bHasFoundNewLine = false;
    bool bPrevWasSep = true;
    int nCols = 0;
    int nMaxCols = 0;
    for(;i<poOpenInfo->nHeaderBytes;i++)
    {
        char ch = pszData[i];
        if (ch == 13 || ch == 10)
        {
            bHasFoundNewLine = true;
            if (!bPrevWasSep)
            {
                nCols ++;
                if (nCols > nMaxCols)
                    nMaxCols = nCols;
            }
            bPrevWasSep = true;
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
            bPrevWasSep = true;
        }
        else if ((ch >= '0' && ch <= '9') || ch == '.' || ch == '+' ||
                 ch == '-' || ch == 'e' || ch == 'E')
        {
            bPrevWasSep = false;
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
    int         bHasHeaderLine;
    int         nCommentLineCount = 0;

    if (!IdentifyEx(poOpenInfo, bHasHeaderLine, nCommentLineCount))
        return NULL;

    CPLString osFilename(poOpenInfo->pszFilename);

    /*  GZipped .xyz files are common, so automagically open them */
    /*  if the /vsigzip/ has not been explicitly passed */
    if (strlen(poOpenInfo->pszFilename) > 6 &&
        EQUAL(poOpenInfo->pszFilename + strlen(poOpenInfo->pszFilename) - 6, "xyz.gz") &&
        !STARTS_WITH_CI(poOpenInfo->pszFilename, "/vsigzip/"))
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
    if (!STARTS_WITH_CI(poOpenInfo->pszFilename, "/vsigzip/"))
        fp = reinterpret_cast<VSILFILE *>(
            VSICreateBufferedReaderHandle(
                reinterpret_cast<VSIVirtualHandle *>( fp ) ) );

    int nXIndex = -1;
    int nYIndex = -1;
    int nZIndex = -1;
    int nMinTokens = 0;

    for( int i = 0; i < nCommentLineCount; i++ )
    {
        if( CPLReadLine2L(fp, 100, NULL) == NULL )
        {
            VSIFCloseL(fp);
            return NULL;
        }
    }

/* -------------------------------------------------------------------- */
/*      Parse header line                                               */
/* -------------------------------------------------------------------- */
    if (bHasHeaderLine)
    {
        const char* pszLine = CPLReadLine2L(fp, 100, NULL);
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
        for( int i = 0; i < nTokens; i++ )
        {
            if (EQUAL(papszTokens[i], "x") ||
                STARTS_WITH_CI(papszTokens[i], "lon") ||
                STARTS_WITH_CI(papszTokens[i], "east"))
                nXIndex = i;
            else if (EQUAL(papszTokens[i], "y") ||
                     STARTS_WITH_CI(papszTokens[i], "lat") ||
                     STARTS_WITH_CI(papszTokens[i], "north"))
                nYIndex = i;
            else if (EQUAL(papszTokens[i], "z") ||
                     STARTS_WITH_CI(papszTokens[i], "alt") ||
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
        nMinTokens = 1 + std::max(std::max(nXIndex, nYIndex), nZIndex);
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

    GIntBig nLineNum = 0;
    GIntBig nDataLineNum = 0;
    double dfX = 0.0;
    double dfY = 0.0;
    double dfZ = 0.0;
    double dfMinX = 0.0;
    double dfMinY = 0.0;
    double dfMaxX = 0.0;
    double dfMaxY = 0.0;
    double dfMinZ = 0.0;
    double dfMaxZ = 0.0;
    double dfLastX = 0.0;
    double dfLastY = 0.0;
    std::vector<double> adfStepX;
    std::vector<double> adfStepY;
    GDALDataType eDT = GDT_Byte;
    bool bSameNumberOfValuesPerLine = true;
    char chDecimalSep = '\0';
    int bStepYSign = 0;

    const char* pszLine;
    GIntBig nCountStepX = 0;
    GIntBig nCountStepY = 0;
    while((pszLine = CPLReadLine2L(fp, 100, NULL)) != NULL)
    {
        nLineNum ++;

        const char* pszPtr = pszLine;
        char ch;
        int nCol = 0;
        bool bLastWasSep = true;
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
                    bLastWasSep = false;
                }
                else if( ch == ' ' )
                {
                    if (!bLastWasSep)
                        nCountFieldSep ++;
                    bLastWasSep = true;
                }
                else if( ch == '\t' || ch == ';' )
                {
                    nCountFieldSep ++;
                    bLastWasSep = true;
                }
                else
                    bLastWasSep = false;
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
            bLastWasSep = true;
        }

        char chLocalDecimalSep = chDecimalSep ? chDecimalSep : '.';
        int nUsefulColsFound = 0;
        while((ch = *pszPtr) != '\0')
        {
            if (ch == ' ')
            {
                if (!bLastWasSep)
                    nCol ++;
                bLastWasSep = true;
            }
            else if ((ch == ',' && chLocalDecimalSep != ',') || ch == '\t' || ch == ';')
            {
                nCol ++;
                bLastWasSep = true;
            }
            else
            {
                if (bLastWasSep)
                {
                    if (nCol == nXIndex)
                    {
                        nUsefulColsFound ++;
                        dfX = CPLAtofDelim(pszPtr, chLocalDecimalSep);
                    }
                    else if (nCol == nYIndex)
                    {
                        nUsefulColsFound ++;
                        dfY = CPLAtofDelim(pszPtr, chLocalDecimalSep);
                    }
                    else if (nCol == nZIndex)
                    {
                        nUsefulColsFound ++;
                        dfZ = CPLAtofDelim(pszPtr, chLocalDecimalSep);
                        if( nDataLineNum == 0 )
                        {
                            dfMinZ = dfZ;
                            dfMaxZ = dfZ;
                        }
                        else if( dfZ < dfMinZ )
                        {
                            dfMinZ = dfZ;
                        }
                        else if( dfZ > dfMaxZ )
                        {
                            dfMaxZ = dfZ;
                        }

                        if( dfZ < INT_MIN || dfZ > INT_MAX )
                        {
                            eDT = GDT_Float32;
                        }
                        else
                        {
                            int nZ = static_cast<int>( dfZ );
                            if( static_cast<double>( nZ ) != dfZ )
                            {
                                eDT = GDT_Float32;
                            }
                            else if ((eDT == GDT_Byte || eDT == GDT_Int16)
                                     && (nZ < 0 || nZ > 255))
                            {
                                if (nZ < -32768 || nZ > 32767)
                                    eDT = GDT_Int32;
                                else
                                    eDT = GDT_Int16;
                            }
                        }
                    }
                }
                bLastWasSep = false;
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
                     "At line " CPL_FRMT_GIB ", found %d tokens. Expected %d at least",
                      nLineNum, nCol, nMinTokens);
            VSIFCloseL(fp);
            return NULL;
        }
        if( nUsefulColsFound != 3 )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "At line " CPL_FRMT_GIB ", did not find X, Y and/or Z values",
                      nLineNum);
            VSIFCloseL(fp);
            return NULL;
        }

        if (nDataLineNum == 1)
        {
            dfMinX = dfX;
            dfMaxX = dfX;
            dfMinY = dfY;
            dfMaxY = dfY;
        }
        else
        {
            double dfStepY = dfY - dfLastY;
            if( dfStepY == 0.0 )
            {
                const double dfStepX = dfX - dfLastX;
                if( dfStepX <= 0 )
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                         "Ungridded dataset: At line " CPL_FRMT_GIB ", X spacing was %f. Expected >0 value",
                         nLineNum, dfStepX);
                    VSIFCloseL(fp);
                    return NULL;
                }
                if( std::find(adfStepX.begin(), adfStepX.end(), dfStepX) == adfStepX.end() )
                {
                    bool bAddNewValue = true;
                    std::vector<double>::iterator oIter = adfStepX.begin();
                    std::vector<double> adfStepXNew;
                    while( oIter != adfStepX.end() )
                    {
                        if( fabs(( dfStepX - *oIter ) / dfStepX ) < RELATIVE_ERROR )
                        {
                            double dfNewVal = *oIter;
                            if( nCountStepX > 0 )
                            {
                                // Update mean step
                                /* n * mean(n) = (n-1) * mean(n-1) + val(n)
                                mean(n) = mean(n-1) + (val(n) - mean(n-1)) / n */
                                nCountStepX ++;
                                dfNewVal += ( dfStepX - *oIter ) / nCountStepX;
                            }

                            adfStepXNew.push_back( dfNewVal );
                            bAddNewValue = false;
                            break;
                        }
                        else if( dfStepX < *oIter &&
                                 fabs(*oIter - static_cast<int>(*oIter / dfStepX + 0.5) * dfStepX) / dfStepX < RELATIVE_ERROR )
                        {
                            nCountStepX = -1; // disable update of mean
                            ++ oIter;
                        }
                        else if( dfStepX > *oIter &&
                                 fabs(dfStepX - static_cast<int>(dfStepX / *oIter + 0.5) * (*oIter)) / dfStepX < RELATIVE_ERROR )
                        {
                            nCountStepX = -1; // disable update of mean
                            bAddNewValue = false;
                            adfStepXNew.push_back( *oIter );
                            break;
                        }
                        else
                        {
                            adfStepXNew.push_back( *oIter );
                            ++ oIter;
                        }
                    }
                    adfStepX = adfStepXNew;
                    if( bAddNewValue )
                    {
                        CPLDebug("XYZ", "New stepX=%.15f", dfStepX);
                        adfStepX.push_back(dfStepX);
                        if( adfStepX.size() == 1 && nCountStepX == 0)
                        {
                            nCountStepX ++;
                        }
                        else if( adfStepX.size() == 2 )
                        {
                            nCountStepX = -1; // disable update of mean
                        }
                        else if( adfStepX.size() == 10 )
                        {
                            CPLError(CE_Failure, CPLE_AppDefined,
                                "Ungridded dataset: too many stepX values");
                            VSIFCloseL(fp);
                            return NULL;
                        }
                    }
                }
            }
            else
            {
                int bNewStepYSign = (dfStepY < 0.0) ? -1 : 1;
                if( bStepYSign == 0 )
                    bStepYSign = bNewStepYSign;
                else if( bStepYSign != bNewStepYSign )
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                         "Ungridded dataset: At line " CPL_FRMT_GIB ", change of Y direction",
                         nLineNum);
                    VSIFCloseL(fp);
                    return NULL;
                }
                if( bNewStepYSign < 0 ) dfStepY = -dfStepY;
                nCountStepY ++;
                if( adfStepY.empty() )
                {
                    adfStepY.push_back(dfStepY);
                }
                else if( fabs( (adfStepY[0] - dfStepY) / dfStepY ) > RELATIVE_ERROR )
                {
                    CPLDebug("XYZ", "New stepY=%.15f prev stepY=%.15f", dfStepY, adfStepY[0]);
                    CPLError(CE_Failure, CPLE_AppDefined,
                        "Ungridded dataset: At line " CPL_FRMT_GIB ", too many stepY values", nLineNum);
                    VSIFCloseL(fp);
                    return NULL;
                }
                else
                {
                    // Update mean step
                    adfStepY[0] += ( dfStepY - adfStepY[0] ) / nCountStepY;
                }
            }

            if (dfX < dfMinX) dfMinX = dfX;
            if (dfX > dfMaxX) dfMaxX = dfX;
            if (dfY < dfMinY) dfMinY = dfY;
            if (dfY > dfMaxY) dfMaxY = dfY;
        }

        dfLastX = dfX;
        dfLastY = dfY;
    }

    if (adfStepX.size() != 1)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Couldn't determine X spacing");
        VSIFCloseL(fp);
        return NULL;
    }

    if (adfStepY.size() != 1)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Couldn't determine Y spacing");
        VSIFCloseL(fp);
        return NULL;
    }

    const double dfXSize = 1 + ((dfMaxX - dfMinX) / adfStepX[0] + 0.5);
    const double dfYSize = 1 + ((dfMaxY - dfMinY) / adfStepY[0] + 0.5);
    // Test written such as to detect NaN values
    if( !(dfXSize > 0 && dfXSize < INT_MAX) ||
        !(dfYSize > 0 && dfYSize < INT_MAX ) )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Invalid dimensions");
        VSIFCloseL(fp);
        return NULL;
    }
    const int nXSize = static_cast<int>(dfXSize);
    const int nYSize = static_cast<int>(dfYSize);
    const double dfStepX = (dfMaxX - dfMinX) / (nXSize - 1);
    const double dfStepY = (dfMaxY - dfMinY) / (nYSize - 1)* bStepYSign;

#ifdef DEBUG_VERBOSE
    CPLDebug("XYZ", "minx=%f maxx=%f stepx=%f", dfMinX, dfMaxX, dfStepX);
    CPLDebug("XYZ", "miny=%f maxy=%f stepy=%f", dfMinY, dfMaxY, dfStepY);
#endif

    if (nDataLineNum != static_cast<GIntBig>(nXSize) * nYSize)
    {
        bSameNumberOfValuesPerLine = false;
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
    XYZDataset *poDS = new XYZDataset();
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
#ifdef DEBUG_VERBOSE
    CPLDebug( "XYZ", "bSameNumberOfValuesPerLine = %d",
              bSameNumberOfValuesPerLine );
#endif

    if (!GDALCheckDatasetDimensions(poDS->nRasterXSize, poDS->nRasterYSize))
    {
        delete poDS;
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Create band information objects.                                */
/* -------------------------------------------------------------------- */
    poDS->nBands = 1;
    for( int i = 0; i < poDS->nBands; i++ )
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
    return poDS;
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

    const GDALDataType eSrcDT = poSrcDS->GetRasterBand(1)->GetRasterDataType();
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
#ifdef DEBUG_VERBOSE
    else
        CPLDebug("XYZ", "Using raw column separator: '%s' ",
                 pszColSep);
#endif

    const char* pszAddHeaderLine =
            CSLFetchNameValue(papszOptions, "ADD_HEADER_LINE");
    if (pszAddHeaderLine != NULL && CPLTestBool(pszAddHeaderLine))
    {
        VSIFPrintfL(fp, "X%sY%sZ\n", pszColSep, pszColSep);
    }

/* -------------------------------------------------------------------- */
/*      Copy imagery                                                    */
/* -------------------------------------------------------------------- */
    void* pLineBuffer
        = reinterpret_cast<void *>( CPLMalloc( nXSize * sizeof(int) ) );
    CPLErr eErr = CE_None;
    for( int j=0; j < nYSize && eErr == CE_None; j++ )
    {
        eErr = poSrcDS->GetRasterBand(1)->RasterIO(
                                            GF_Read, 0, j, nXSize, 1,
                                            pLineBuffer, nXSize, 1,
                                            eReqDT, 0, 0, NULL);
        if (eErr != CE_None)
            break;
        const double dfY = adfGeoTransform[3] + (j + 0.5) * adfGeoTransform[5];
        CPLString osBuf;
        for( int i = 0; i < nXSize; i++ )
        {
            const double dfX
                = adfGeoTransform[0] + (i + 0.5) * adfGeoTransform[1];
            char szBuf[256];
            if (eReqDT == GDT_Int32)
                CPLsnprintf(szBuf, sizeof(szBuf), "%.18g%c%.18g%c%d\n",
                            dfX, pszColSep[0], dfY, pszColSep[0],
                            reinterpret_cast<int *>( pLineBuffer )[i] );
            else
                CPLsnprintf(szBuf, sizeof(szBuf), "%.18g%c%.18g%c%.18g\n",
                            dfX, pszColSep[0], dfY, pszColSep[0],
                            reinterpret_cast<float *>( pLineBuffer )[i]);
            osBuf += szBuf;
            if( (i & 1023) == 0 || i == nXSize - 1 )
            {
                if ( VSIFWriteL( osBuf,
                                 static_cast<int>( osBuf.size() ), 1, fp )
                     != 1 )
                {
                    eErr = CE_Failure;
                    CPLError( CE_Failure, CPLE_AppDefined,
                              "Write failed, disk full?\n" );
                    break;
                }
                osBuf = "";
            }
        }
        if ( pfnProgress
             && !pfnProgress( (j+1) * 1.0 / nYSize, NULL, pProgressData ) )
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
    /* If writing to stdout, we can't reopen it --> silence warning */
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

    return CE_None;
}

/************************************************************************/
/*                         GDALRegister_XYZ()                           */
/************************************************************************/

void GDALRegister_XYZ()

{
    if( GDALGetDriverByName( "XYZ" ) != NULL )
      return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription( "XYZ" );
    poDriver->SetMetadataItem( GDAL_DCAP_RASTER, "YES" );
    poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, "ASCII Gridded XYZ" );
    poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, "frmt_xyz.html" );
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
