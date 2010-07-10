/******************************************************************************
 * $Id$
 *
 * Project:  XYZ driver
 * Purpose:  GDALDataset driver for XYZ dataset.
 * Author:   Even Rouault, <even dot rouault at mines dash paris dot org>
 *
 ******************************************************************************
 * Copyright (c) 2010, Even Rouault
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
    
    FILE       *fp;
    int         bHasHeaderLine;
    int         nXIndex;
    int         nYIndex;
    int         nZIndex;
    int         nMinTokens;
    int         nLineNum;     /* any line */
    int         nDataLineNum; /* line with values (header line and empty lines ignored) */
    double      adfGeoTransform[6];
    
    static int          IdentifyEx( GDALOpenInfo *, int& );

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

  public:

                XYZRasterBand( XYZDataset *, int, GDALDataType );

    virtual CPLErr IReadBlock( int, int, void * );
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
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr XYZRasterBand::IReadBlock( int nBlockXOff, int nBlockYOff,
                                  void * pImage )

{
    XYZDataset *poGDS = (XYZDataset *) poDS;

    int nLineInFile = nBlockYOff * nBlockXSize;
    if (poGDS->nDataLineNum > nLineInFile)
    {
        poGDS->nDataLineNum = 0;
        VSIFSeekL(poGDS->fp, 0, SEEK_SET);
        if (poGDS->bHasHeaderLine)
        {
            const char* pszLine = CPLReadLine2L(poGDS->fp, 100, 0);
            if (pszLine == NULL)
            {
                memset(pImage, 0, nBlockXSize * (GDALGetDataTypeSize(eDataType) / 8));
                return CE_Failure;
            }
            poGDS->nLineNum ++;
        }
    }

    while(poGDS->nDataLineNum < nLineInFile)
    {
        const char* pszLine = CPLReadLine2L(poGDS->fp, 100, 0);
        if (pszLine == NULL)
        {
            memset(pImage, 0, nBlockXSize * (GDALGetDataTypeSize(eDataType) / 8));
            return CE_Failure;
        }
        poGDS->nLineNum ++;

        const char* pszPtr = pszLine;
        char ch;
        int nCol = 0;
        int bLastWasSep = TRUE;
        while((ch = *pszPtr) != '\0')
        {
            if (ch == ' ' || ch == ',' || ch == '\t' || ch == ';')
            {
                if (!bLastWasSep)
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

    int i;
    for(i=0;i<nBlockXSize;i++)
    {
        int nCol;
        int bLastWasSep;
        do
        {
            const char* pszLine = CPLReadLine2L(poGDS->fp, 100, 0);
            if (pszLine == NULL)
            {
                memset(pImage, 0, nBlockXSize * (GDALGetDataTypeSize(eDataType) / 8));
                return CE_Failure;
            }
            poGDS->nLineNum ++;

            const char* pszPtr = pszLine;
            char ch;
            nCol = 0;
            bLastWasSep = TRUE;
            while((ch = *pszPtr) != '\0')
            {
                if (ch == ' ' || ch == ',' || ch == '\t' || ch == ';')
                {
                    if (!bLastWasSep)
                        nCol ++;
                    bLastWasSep = TRUE;
                }
                else
                {
                    if (bLastWasSep && nCol == poGDS->nZIndex)
                    {
                        double dfZ = CPLAtofM(pszPtr);
                        if (eDataType == GDT_Float32)
                        {
                            ((float*)pImage)[i] = (float)dfZ;
                        }
                        else if (eDataType == GDT_Int32)
                        {
                            ((GInt32*)pImage)[i] = (GInt32)dfZ;
                        }
                        else if (eDataType == GDT_Int16)
                        {
                            ((GInt16*)pImage)[i] = (GInt16)dfZ;
                        }
                        else
                        {
                            ((GByte*)pImage)[i] = (GByte)dfZ;
                        }
                    }
                    bLastWasSep = FALSE;
                }
                pszPtr ++;
            }

            /* Skip empty line */
        }
        while (nCol == 0 && bLastWasSep);

        poGDS->nDataLineNum ++;
        nCol ++;
        if (nCol < poGDS->nMinTokens)
        {
            memset(pImage, 0, nBlockXSize * (GDALGetDataTypeSize(eDataType) / 8));
            return CE_Failure;
        }
    }
    CPLAssert(poGDS->nDataLineNum == (nBlockYOff + 1) * nBlockXSize);

    return CE_None;
}

/************************************************************************/
/*                            ~XYZDataset()                            */
/************************************************************************/

XYZDataset::XYZDataset()
{
    fp = NULL;
    nDataLineNum = INT_MAX;
    nLineNum = 0;
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
    int bHasHeaderLine;
    return IdentifyEx(poOpenInfo, bHasHeaderLine);
}

/************************************************************************/
/*                            IdentifyEx()                              */
/************************************************************************/


int XYZDataset::IdentifyEx( GDALOpenInfo * poOpenInfo, int& bHasHeaderLine )

{
    int         i;

    bHasHeaderLine = FALSE;

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
    for(i=0;i<poOpenInfo->nHeaderBytes;i++)
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

    if (!IdentifyEx(poOpenInfo, bHasHeaderLine))
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
    FILE* fp = VSIFOpenL(osFilename.c_str(), "rb");
    if (fp == NULL)
        return NULL;

    /* For better performance of CPLReadLine2L() we create a buffered reader */
    /* (except for /vsigzip/ since it has one internally) */
    if (!EQUALN(poOpenInfo->pszFilename, "/vsigzip/", 9))
        fp = (FILE*) VSICreateBufferedReaderHandle((VSIVirtualHandle*)fp);
    
    const char* pszLine;
    int nXIndex = -1, nYIndex = -1, nZIndex = -1;
    int nMinTokens = 0;
    
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
    double dfFirstX = 0;
    double dfX = 0, dfY = 0, dfZ = 0;
    double dfMinX = 0, dfMinY = 0, dfMaxX = 0, dfMaxY = 0;
    double dfLastX = 0, dfLastY = 0;
    double dfStepX = 0, dfStepY = 0;
    GDALDataType eDT = GDT_Byte;
    while((pszLine = CPLReadLine2L(fp, 100, NULL)) != NULL)
    {
        nLineNum ++;

        const char* pszPtr = pszLine;
        char ch;
        int nCol = 0;
        int bLastWasSep = TRUE;
        while((ch = *pszPtr) != '\0')
        {
            if (ch == ' ' || ch == ',' || ch == '\t' || ch == ';')
            {
                if (!bLastWasSep)
                    nCol ++;
                bLastWasSep = TRUE;
            }
            else
            {
                if (bLastWasSep)
                {
                    if (nCol == nXIndex)
                        dfX = CPLAtofM(pszPtr);
                    else if (nCol == nYIndex)
                        dfY = CPLAtofM(pszPtr);
                    else if (nCol == nZIndex && eDT != GDT_Float32)
                    {
                        dfZ = CPLAtofM(pszPtr);
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

        if (nDataLineNum == 1)
        {
            dfFirstX = dfMinX = dfMaxX = dfX;
            dfMinY = dfMaxY = dfY;
        }
        else
        {
            if (dfX < dfMinX) dfMinX = dfX;
            if (dfX > dfMaxX) dfMaxX = dfX;
            if (dfY < dfMinY) dfMinY = dfY;
            if (dfY > dfMaxY) dfMaxY = dfY;
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
            double dfNewStepX = dfX - dfLastX;
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
                if (dfStepY != 0 && fabs(dfX - dfFirstX) > 1e-8)
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Ungridded dataset: At line %d, X is %f, where as %f was expected",
                             nLineNum, dfX, dfFirstX);
                    VSIFCloseL(fp);
                    return NULL;
                }
                if (dfStepY != 0 && fabs(dfLastX - dfMaxX) > 1e-8)
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Ungridded dataset: At line %d, X is %f, where as %f was expected",
                             nLineNum - 1, dfLastX, dfMaxX);
                    VSIFCloseL(fp);
                    return NULL;
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
            else if (dfNewStepX != 0)
            {
                /*if (dfStepX != 0 && fabs(dfNewStepX - dfStepX) > 1e-8)
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "At line %d, X spacing was %f, whereas it was %f before",
                             nLineNum, dfNewStepX, dfStepX);
                    VSIFCloseL(fp);
                    return NULL;
                }*/
            }
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

    if (nDataLineNum != nXSize * nYSize)
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

    FILE* fp = VSIFOpenL(pszFilename, "wb");
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
    for(j=0;j<nYSize;j++)
    {
        CPLErr eErr = poSrcDS->GetRasterBand(1)->RasterIO(
                                            GF_Read, 0, j, nXSize, 1,
                                            pLineBuffer, nXSize, 1,
                                            eReqDT, 0, 0);
        if (eErr != CE_None)
            break;
        double dfY = adfGeoTransform[3] + (j + 0.5) * adfGeoTransform[5];
        for(i=0;i<nXSize;i++)
        {
            double dfX = adfGeoTransform[0] + (i + 0.5) * adfGeoTransform[1];
            VSIFPrintfL(fp, "%.18g%s%.18g%s", dfX, pszColSep, dfY, pszColSep);
            if (eReqDT == GDT_Int32)
                VSIFPrintfL(fp, "%d\n", ((int*)pLineBuffer)[i]);
            else
                VSIFPrintfL(fp, "%.18g\n", ((float*)pLineBuffer)[i]);
        }
        if (!pfnProgress( (j+1) * 1.0 / nYSize, NULL, pProgressData))
            break;
    }
    CPLFree(pLineBuffer);
    VSIFCloseL(fp);

    return (GDALDataset*) GDALOpen(pszFilename, GA_ReadOnly);
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

