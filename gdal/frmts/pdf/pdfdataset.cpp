/******************************************************************************
 * $Id$
 *
 * Project:  PDF driver
 * Purpose:  GDALDataset driver for PDF dataset.
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

/* hack for PDF driver and poppler >= 0.15.0 that defines incompatible "typedef bool GBool" */
/* in include/poppler/goo/gtypes.h with the one defined in cpl_port.h */
#define CPL_GBOOL_DEFINED

#include "cpl_vsi_virtual.h"
#include "cpl_string.h"
#include "gdal_pam.h"
#include "ogr_spatialref.h"
#include "ogr_geometry.h"
#include "pdfio.h"

/* begin of poppler xpdf includes */
#include <poppler/Object.h>

#define private public /* Ugly! Page::pageObj is private but we need it... */
#include <poppler/Page.h>
#undef private

#include <poppler/Dict.h>

#define private public /* Ugly! Catalog::optContent is private but we need it... */
#include <poppler/Catalog.h>
#undef private

#define private public  /* Ugly! PDFDoc::str is private but we need it... */
#include <poppler/PDFDoc.h>
#undef private

#include <poppler/splash/SplashBitmap.h>
#include <poppler/splash/Splash.h>
#include <poppler/SplashOutputDev.h>
#include <poppler/GlobalParams.h>
#include <poppler/ErrorCodes.h>
/* end of poppler xpdf includes */

/* g++ -fPIC -g -Wall frmts/pdf/pdfdataset.cpp -shared -o gdal_PDF.so -Iport -Igcore -Iogr -L. -lgdal -lpoppler -I/usr/include/poppler */

CPL_CVSID("$Id$");

CPL_C_START
void    GDALRegister_PDF(void);
CPL_C_END


/************************************************************************/
/*                          ObjectAutoFree                              */
/************************************************************************/

class ObjectAutoFree : public Object
{
public:
    ObjectAutoFree() {}
    ~ObjectAutoFree() { free(); }
};


/************************************************************************/
/*                         Dump routines                                */
/************************************************************************/

static void DumpObject(FILE* f, Object& o, int nDepth = 0, int nDepthLimit = -1);
static void DumpDict(FILE* f, Dict* poDict, int nDepth = 0, int nDepthLimit = -1);
static void DumpArray(FILE* f, Array* poArray, int nDepth = 0, int nDepthLimit = -1);
static void DumpObjectSimplified(FILE* f, Object& o);

static void DumpArray(FILE* f, Array* poArray, int nDepth, int nDepthLimit)
{
    if (nDepthLimit >= 0 && nDepth > nDepthLimit)
        return;

    int nLength = poArray->getLength();
    int i;
    CPLString osIndent;
    for(i=0;i<nDepth;i++)
        osIndent += " ";
    for(i=0;i<nLength;i++)
    {
        fprintf(f, "%sItem[%d]:", osIndent.c_str(), i);
        ObjectAutoFree oVal;
        if (poArray->get(i, &oVal) != NULL)
        {
            if (oVal.getType() == objString ||
                oVal.getType() == objInt ||
                oVal.getType() == objReal ||
                oVal.getType() == objName)
            {
                fprintf(f, " ");
                DumpObjectSimplified(f, oVal);
                fprintf(f, "\n");
            }
            else
            {
                fprintf(f, "\n");
                DumpObject(f, oVal, nDepth+1, nDepthLimit);
            }
        }
    }
}

static void DumpObjectSimplified(FILE* f, Object& o)
{
    switch(o.getType())
    {
        case objString:
            fprintf(f, "%s (string)", o.getString()->getCString());
            break;

        case objInt:
            fprintf(f, "%d (int)", o.getInt());
            break;

        case objReal:
            fprintf(f, "%f (real)", o.getReal());
            break;

        case objName:
            fprintf(f, "%s (name)", o.getName());
            break;

        default:
            break;
    }
}

static void DumpObject(FILE* f, Object& o, int nDepth, int nDepthLimit)
{
    if (nDepthLimit >= 0 && nDepth > nDepthLimit)
        return;

    int i;
    CPLString osIndent;
    for(i=0;i<nDepth;i++)
        osIndent += " ";
    fprintf(f, "%sType = %s\n", osIndent.c_str(), o.getTypeName());
    switch(o.getType())
    {
        case objArray:
            DumpArray(f, o.getArray(), nDepth+1, nDepthLimit);
            break;

        case objDict:
            DumpDict(f, o.getDict(), nDepth+1, nDepthLimit);
            break;

        case objStream:
        {
            Dict* poDict = o.getStream()->getDict();
            if (poDict)
                DumpDict(f, poDict, nDepth+1, nDepthLimit);
            break;
        }

        case objString:
        case objInt:
        case objReal:
        case objName:
            fprintf(f, "%s", osIndent.c_str());
            DumpObjectSimplified(f, o);
            fprintf(f, "\n");
            break;

        default:
            break;
    }
}

static void DumpDict(FILE* f, Dict* poDict, int nDepth, int nDepthLimit)
{
    if (nDepthLimit >= 0 && nDepth > nDepthLimit)
        return;

    int nLength = poDict->getLength();
    int i;
    CPLString osIndent;
    for(i=0;i<nDepth;i++)
        osIndent += " ";
    for(i=0;i<nLength;i++)
    {
        char* pszKey = poDict->getKey(i);
        fprintf(f, "%sItem[%d] : %s", osIndent.c_str(), i, pszKey);
        if (strcmp(pszKey, "Parent") == 0)
        {
            fprintf(f, "\n");
            continue;
        }
        ObjectAutoFree oVal;
        if (poDict->getVal(i, &oVal) != NULL)
        {
            if (oVal.getType() == objString ||
                oVal.getType() == objInt ||
                oVal.getType() == objReal ||
                oVal.getType() == objName)
            {
                fprintf(f, " = ");
                DumpObjectSimplified(f, oVal);
                fprintf(f, "\n");
            }
            else
            {
                fprintf(f, "\n");
                DumpObject(f, oVal, nDepth+1, nDepthLimit);
            }
        }
    }
}


/************************************************************************/
/* ==================================================================== */
/*                              PDFDataset                              */
/* ==================================================================== */
/************************************************************************/

class PDFRasterBand;

class PDFDataset : public GDALPamDataset
{
    friend class PDFRasterBand;
    char        *pszWKT;
    double       dfDPI;
    double       adfCTM[6];
    double       adfGeoTransform[6];
    int          bGeoTransformValid;
    PDFDoc*      poDoc;
    int          iPage;

    double       dfMaxArea;
    int          ParseLGIDictObject(Object& oLGIDict);
    int          ParseLGIDictDictFirstPass(Dict* poLGIDict, int* pbIsLargestArea = NULL);
    int          ParseLGIDictDictSecondPass(Dict* poLGIDict);
    int          ParseProjDict(Dict* poProjDict);
    int          ParseVP(Object& oVP, double dfMediaBoxWidth, double dfMediaBoxHeight);

    int          bTried;
    GByte       *pabyData;

    OGRPolygon*  poNeatLine;

  public:
                 PDFDataset();
    virtual     ~PDFDataset();

    virtual const char* GetProjectionRef();
    virtual CPLErr GetGeoTransform( double * );

    static GDALDataset *Open( GDALOpenInfo * );
    static int          Identify( GDALOpenInfo * );
};

/************************************************************************/
/* ==================================================================== */
/*                         PDFRasterBand                                */
/* ==================================================================== */
/************************************************************************/

class PDFRasterBand : public GDALPamRasterBand
{
    friend class PDFDataset;

  public:

                PDFRasterBand( PDFDataset *, int );

    virtual CPLErr IReadBlock( int, int, void * );
    virtual GDALColorInterp GetColorInterpretation();
};


/************************************************************************/
/*                         PDFRasterBand()                             */
/************************************************************************/

PDFRasterBand::PDFRasterBand( PDFDataset *poDS, int nBand )

{
    this->poDS = poDS;
    this->nBand = nBand;

    eDataType = GDT_Byte;

    nBlockXSize = poDS->GetRasterXSize();
    nBlockYSize = 1;
}

/************************************************************************/
/*                        GetColorInterpretation()                      */
/************************************************************************/

GDALColorInterp PDFRasterBand::GetColorInterpretation()
{
    return (GDALColorInterp)(GCI_RedBand + (nBand - 1));
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr PDFRasterBand::IReadBlock( int nBlockXOff, int nBlockYOff,
                                  void * pImage )

{
    PDFDataset *poGDS = (PDFDataset *) poDS;

    if (poGDS->bTried == FALSE)
    {
        poGDS->bTried = TRUE;
        poGDS->pabyData = (GByte*)VSIMalloc3(3, nRasterXSize, nRasterYSize);
        if (poGDS->pabyData == NULL)
            return CE_Failure;

        SplashColor sColor;
        sColor[0] = 255;
        sColor[1] = 255;
        sColor[2] = 255;
        SplashOutputDev *poSplashOut;
        poSplashOut = new SplashOutputDev(splashModeRGB8, 4, gFalse, sColor);
        PDFDoc* poDoc = poGDS->poDoc;
        poSplashOut->startDoc(poDoc->getXRef());
        double dfDPI = poGDS->dfDPI;

        /* EVIL: we modify a private member... */
        /* poppler (at least 0.12 and 0.14 versions) don't render correctly */
        /* some PDFs and display an error message 'Could not find a OCG with Ref' */
        /* in those cases. This processing of optional content is an addition of */
        /* poppler in comparison to original xpdf, which hasn't the issue. All in */
        /* all, nullifying optContent removes the error message and improves the rendering */
#ifdef POPPLER_HAS_OPTCONTENT
        Catalog* poCatalog = poDoc->getCatalog();
        OCGs* poOldOCGs = poCatalog->optContent;
        poCatalog->optContent = NULL;
#endif

        poGDS->poDoc->displayPageSlice(poSplashOut,
                                       poGDS->iPage,
                                       dfDPI, dfDPI,
                                       0,
                                       TRUE, gFalse, gFalse,
                                       0, 0,
                                       nRasterXSize,
                                       nRasterYSize);

        /* Restore back */
#ifdef POPPLER_HAS_OPTCONTENT
        poCatalog->optContent = poOldOCGs;
#endif

        SplashBitmap* poBitmap = poSplashOut->getBitmap();
        if (poBitmap->getWidth() != nRasterXSize || poBitmap->getHeight() != nRasterYSize)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Bitmap decoded size (%dx%d) doesn't match raster size (%dx%d)" ,
                     poBitmap->getWidth(), poBitmap->getHeight(),
                     nRasterXSize, nRasterYSize);
            VSIFree(poGDS->pabyData);
            poGDS->pabyData = NULL;
            delete poSplashOut;
            return CE_Failure;
        }

        int nRowSize = poBitmap->getRowSize();
        GByte* pabyBitmap = poBitmap->getDataPtr();
        int i, j;
        GByte* pabyDataR = poGDS->pabyData;
        GByte* pabyDataG = poGDS->pabyData + nRasterXSize * nRasterYSize;
        GByte* pabyDataB = poGDS->pabyData + 2 * nRasterXSize * nRasterYSize;
        for(j=0;j<nRasterYSize;j++)
        {
            for(i=0;i<nRasterXSize;i++)
            {
                pabyDataR[j * nRasterXSize + i] = pabyBitmap[j * nRowSize + i * 3];
                pabyDataG[j * nRasterXSize + i] = pabyBitmap[j * nRowSize + i * 3 + 1];
                pabyDataB[j * nRasterXSize + i] = pabyBitmap[j * nRowSize + i * 3 + 2];
            }
        }
        delete poSplashOut;
    }
    if (poGDS->pabyData == NULL)
        return CE_Failure;

    memcpy(pImage,
           poGDS->pabyData + (nBand - 1) * nRasterXSize * nRasterYSize + nBlockYOff * nRasterXSize,
           nRasterXSize);

    return CE_None;
}

/************************************************************************/
/*                            ~PDFDataset()                            */
/************************************************************************/

PDFDataset::PDFDataset()
{
    poDoc = NULL;
    pszWKT = NULL;
    dfMaxArea = 0;
    adfGeoTransform[0] = 0;
    adfGeoTransform[1] = 1;
    adfGeoTransform[2] = 0;
    adfGeoTransform[3] = 0;
    adfGeoTransform[4] = 0;
    adfGeoTransform[5] = 1;
    bGeoTransformValid = FALSE;
    bTried = FALSE;
    pabyData = NULL;
    iPage = -1;
    poNeatLine = NULL;
}

/************************************************************************/
/*                           PDFFreeDoc()                               */
/************************************************************************/

static void PDFFreeDoc(PDFDoc* poDoc)
{
    if (poDoc)
    {
        /* hack to avoid potential cross heap issues on Win32 */
        /* str is the VSIPDFFileStream object passed in the constructor of PDFDoc */
        delete poDoc->str;
        poDoc->str = NULL;

        delete poDoc;
    }
}

/************************************************************************/
/*                            ~PDFDataset()                            */
/************************************************************************/

PDFDataset::~PDFDataset()
{
    CPLFree(pszWKT);
    CPLFree(pabyData);

    delete poNeatLine;

    PDFFreeDoc(poDoc);
}

/************************************************************************/
/*                             Identify()                               */
/************************************************************************/

int PDFDataset::Identify( GDALOpenInfo * poOpenInfo )
{
    if (strncmp(poOpenInfo->pszFilename, "PDF:", 4) == 0)
        return TRUE;

    if (poOpenInfo->nHeaderBytes < 128)
        return FALSE;

    return strncmp((const char*)poOpenInfo->pabyHeader, "%PDF", 4) == 0;
}

/************************************************************************/
/*                    PDFDatasetErrorFunction()                         */
/************************************************************************/

static void PDFDatasetErrorFunction(int nPos, char *pszMsg, va_list args)
{
    CPLString osError;

    if (nPos >= 0)
        osError.Printf("Pos = %d, ", nPos);
    osError += CPLString().vPrintf(pszMsg, args);

    if (strcmp(osError.c_str(), "Incorrect password") == 0)
        return;

    CPLError(CE_Failure, CPLE_AppDefined, "%s", osError.c_str());
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *PDFDataset::Open( GDALOpenInfo * poOpenInfo )

{
    if (!Identify(poOpenInfo))
        return NULL;

    const char* pszUserPwd = CPLGetConfigOption("PDF_USER_PWD", NULL);

    int bOpenSubdataset = strncmp(poOpenInfo->pszFilename, "PDF:", 4) == 0;
    int iPage = -1;
    const char* pszFilename = poOpenInfo->pszFilename;
    char szPassword[81];

    if (bOpenSubdataset)
    {
        iPage = atoi(pszFilename + 4);
        if (iPage <= 0)
            return NULL;
         pszFilename = strchr(pszFilename + 4, ':');
        if (pszFilename == NULL)
            return NULL;
        pszFilename ++;
    }
    else
        iPage = 1;

    GooString* poUserPwd = NULL;

    /* Set custom error handler for poppler errors */
    setErrorFunction(PDFDatasetErrorFunction);

    /* poppler global variable */
    if (globalParams == NULL)
        globalParams = new GlobalParams();

    PDFDoc* poDoc = NULL;
    ObjectAutoFree oObj;
    while(TRUE)
    {
        VSILFILE* fp = VSIFOpenL(pszFilename, "rb");
        if (fp == NULL)
            return NULL;

        fp = (VSILFILE*)VSICreateBufferedReaderHandle((VSIVirtualHandle*)fp);

        if (pszUserPwd)
            poUserPwd = new GooString(pszUserPwd);

        oObj.initNull();
        poDoc = new PDFDoc(new VSIPDFFileStream(fp, pszFilename, 0, gFalse, 0, &oObj), NULL, poUserPwd);
        delete poUserPwd;

        if ( !poDoc->isOk() || poDoc->getNumPages() == 0 )
        {
            if (poDoc->getErrorCode() == errEncrypted)
            {
                if (pszUserPwd && EQUAL(pszUserPwd, "ASK_INTERACTIVE"))
                {
                    printf( "Enter password (will be echo'ed in the console): " );
                    fgets( szPassword, sizeof(szPassword), stdin );
                    szPassword[sizeof(szPassword)-1] = 0;
                    char* sz10 = strchr(szPassword, '\n');
                    if (sz10)
                        *sz10 = 0;
                    pszUserPwd = szPassword;
                    PDFFreeDoc(poDoc);
                    continue;
                }
                else if (pszUserPwd == NULL)
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "A password is needed. You can specify it through the PDF_USER_PWD "
                             "configuration option (that can be set to ASK_INTERACTIVE)");
                }
                else
                {
                    CPLError(CE_Failure, CPLE_AppDefined, "Invalid password");
                }
            }
            else
            {
                CPLError(CE_Failure, CPLE_AppDefined, "Invalid PDF");
            }

            PDFFreeDoc(poDoc);

            return NULL;
        }
        else
            break;
    }

    Catalog* poCatalog = poDoc->getCatalog();
    if ( poCatalog == NULL || !poCatalog->isOk() )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Invalid PDF : invalid catalog");
        PDFFreeDoc(poDoc);
        return NULL;
    }

    int nPages = poDoc->getNumPages();
    if (iPage < 1 || iPage > nPages)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Invalid page number (%d/%d)",
                 iPage, nPages);
        PDFFreeDoc(poDoc);
        return NULL;
    }

    Page* poPage = poCatalog->getPage(iPage);
    if ( poPage == NULL || !poPage->isOk() )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Invalid PDF : invalid page");
        PDFFreeDoc(poDoc);
        return NULL;
    }

    /* Here's the dirty part: this is a private member */
    /* so we had to #define private public to get it ! */
    Object& oPageObj = poPage->pageObj;
    if ( !oPageObj.isDict() )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Invalid PDF : !oPageObj.isDict()");
        PDFFreeDoc(poDoc);
        return NULL;
    }

    Dict* poPageDict = oPageObj.getDict();
    if ( poPageDict == NULL )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Invalid PDF : poPageDict == NULL");
        PDFFreeDoc(poDoc);
        return NULL;
    }

    const char* pszDumpObject = CPLGetConfigOption("PDF_DUMP_OBJECT", NULL);
    if (pszDumpObject != NULL)
    {
        FILE* f;
        if (strcmp(pszDumpObject, "stderr") == 0)
            f = stderr;
        else if (EQUAL(pszDumpObject, "YES"))
            f = fopen(CPLSPrintf("dump_%s.txt", CPLGetFilename(pszFilename)), "wt");
        else
            f = fopen(pszDumpObject, "wt");
        if (f == NULL)
            f = stderr;
        DumpObject(f, oPageObj, 0 ,20);
        if (f != stderr)
            fclose(f);
    }

    PDFDataset* poDS = new PDFDataset();

    if ( nPages > 1 && !bOpenSubdataset )
    {
        int i;
        char** papszSubDatasets = NULL;
        for(i=0;i<nPages;i++)
        {
            char szKey[32];
            sprintf( szKey, "SUBDATASET_%d_NAME", i+1 );
            papszSubDatasets =
                CSLSetNameValue( papszSubDatasets, szKey,
                                 CPLSPrintf("PDF:%d:%s", i+1, poOpenInfo->pszFilename));
            sprintf( szKey, "SUBDATASET_%d_DESC", i+1 );
            papszSubDatasets =
                CSLSetNameValue( papszSubDatasets, szKey,
                                 CPLSPrintf("Page %d of %s", i+1, poOpenInfo->pszFilename));
        }
        poDS->SetMetadata( papszSubDatasets, "SUBDATASETS" );
        CSLDestroy(papszSubDatasets);
    }

    poDS->poDoc = poDoc;
    poDS->iPage = iPage;
    poDS->dfDPI = atof(CPLGetConfigOption("GDAL_PDF_DPI", "150"));
    if (poDS->dfDPI < 1 || poDS->dfDPI > 7200)
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "Invalid value for GDAL_PDF_DPI. Using default value instead");
        poDS->dfDPI = 150;
    }

    PDFRectangle* psMediaBox = poPage->getMediaBox();
    double dfX1 = psMediaBox->x1;
    double dfY1 = psMediaBox->y1;
    double dfX2 = psMediaBox->x2;
    double dfY2 = psMediaBox->y2;

    double dfPixelPerPt = poDS->dfDPI / 72;
    poDS->nRasterXSize = (dfX2 - dfX1) * dfPixelPerPt;
    poDS->nRasterYSize = (dfY2 - dfY1) * dfPixelPerPt;

    if ( poDoc->getPageRotate(iPage) == 90 ||
         poDoc->getPageRotate(iPage) == 270 )
    {
        /* Wondering how it would work with a georeferenced image */
        /* Has only been tested with ungeoreferenced image */
        int nTmp = poDS->nRasterXSize;
        poDS->nRasterXSize = poDS->nRasterYSize;
        poDS->nRasterYSize = nTmp;
    }

    ObjectAutoFree oLGIDict;
    ObjectAutoFree oVP;
    int bIsOGCBP = FALSE;
    if ( poPageDict->lookup((char*)"LGIDict",&oLGIDict) != NULL && !oLGIDict.isNull())
    {
        /* Cf 08-139r2_GeoPDF_Encoding_Best_Practice_Version_2.2.pdf */
        CPLDebug("PDF", "OGC Encoding Best Practice style detected");
        if (poDS->ParseLGIDictObject(oLGIDict))
        {
            poDS->adfGeoTransform[0] = poDS->adfCTM[4] + poDS->adfCTM[0] * dfX1 + poDS->adfCTM[2] * dfY2;
            poDS->adfGeoTransform[1] = poDS->adfCTM[0] / dfPixelPerPt;
            poDS->adfGeoTransform[2] = poDS->adfCTM[1] / dfPixelPerPt;
            poDS->adfGeoTransform[3] = poDS->adfCTM[5] + poDS->adfCTM[1] * dfX1 + poDS->adfCTM[3] * dfY2;
            poDS->adfGeoTransform[4] = - poDS->adfCTM[2] / dfPixelPerPt;
            poDS->adfGeoTransform[5] = - poDS->adfCTM[3] / dfPixelPerPt;
            poDS->bGeoTransformValid = TRUE;
            bIsOGCBP = TRUE;
        }
    }
    else if ( poPageDict->lookup((char*)"VP",&oVP) != NULL && !oVP.isNull())
    {
        /* Cf adobe_supplement_iso32000.pdf */
        CPLDebug("PDF", "Adobe ISO32000 style Geospatial PDF perhaps ?");
        if (dfX1 != 0 || dfY1 != 0)
        {
            CPLDebug("PDF", "non null dfX1 or dfY1 values. untested case...");
        }
        poDS->ParseVP(oVP, dfX2 - dfX1, dfY2 - dfY1);
    }
    else
    {
        /* Not a geospatial PDF doc */
    }

    if (poDS->poNeatLine)
    {
        char* pszNeatLineWkt = NULL;
        OGRLinearRing* poRing = poDS->poNeatLine->getExteriorRing();
        /* Adobe style is already in target SRS units */
        if (bIsOGCBP)
        {
            int nPoints = poRing->getNumPoints();
            int i;

            for(i=0;i<nPoints;i++)
            {
                double x = poRing->getX(i) * dfPixelPerPt;
                double y = poDS->nRasterYSize - poRing->getY(i) * dfPixelPerPt;
                double X = poDS->adfGeoTransform[0] + x * poDS->adfGeoTransform[1] +
                                                    y * poDS->adfGeoTransform[2];
                double Y = poDS->adfGeoTransform[3] + x * poDS->adfGeoTransform[4] +
                                                    y * poDS->adfGeoTransform[5];
                poRing->setPoint(i, X, Y);
            }
        }
        poRing->closeRings();

        poDS->poNeatLine->exportToWkt(&pszNeatLineWkt);
        poDS->SetMetadataItem("NEATLINE", pszNeatLineWkt);
        CPLFree(pszNeatLineWkt);
    }

    int iBand;
    for(iBand = 1; iBand <= 3; iBand ++)
        poDS->SetBand(iBand, new PDFRasterBand(poDS, iBand));

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
/*                       ParseLGIDictObject()                           */
/************************************************************************/

int PDFDataset::ParseLGIDictObject(Object& oLGIDict)
{
    int i;
    int bOK = FALSE;
    if (oLGIDict.isArray())
    {
        int nArrayLength = oLGIDict.arrayGetLength();
        int iMax = -1;
        for (i=0; i<nArrayLength; i++)
        {
            ObjectAutoFree oArrayElt;
            if ( oLGIDict.arrayGet(i,&oArrayElt) == NULL ||
                 !oArrayElt.isDict() )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "LGIDict[%d] is not a dictionary", i);
                return FALSE;
            }

            int bIsLargestArea = FALSE;
            ParseLGIDictDictFirstPass(oArrayElt.getDict(), &bIsLargestArea);
            if (bIsLargestArea)
                iMax = i;
        }

        if (iMax < 0)
            return FALSE;

        ObjectAutoFree oArrayElt;
        oLGIDict.arrayGet(iMax,&oArrayElt);
        bOK = ParseLGIDictDictSecondPass(oArrayElt.getDict());
    }
    else if (oLGIDict.isDict())
    {
        bOK = ParseLGIDictDictFirstPass(oLGIDict.getDict()) &&
              ParseLGIDictDictSecondPass(oLGIDict.getDict());
    }
    else
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "LGIDict is of type %s", oLGIDict.getTypeName());
    }

    return bOK;
}

/************************************************************************/
/*                            GetValue()                                */
/************************************************************************/

static double GetValue(Object& o, int nIndice = -1)
{
    if (o.isArray() && nIndice >= 0)
    {
        ObjectAutoFree oElt;
        if (o.arrayGet(nIndice, &oElt) == NULL)
            return 0;
        return GetValue(oElt);
    }
    else if (o.isInt())
        return o.getInt();
    else if (o.isReal())
        return o.getReal();
    else if (o.isString())
    {
        const char* pszStr = o.getString()->getCString();
        int nLen = strlen(pszStr);
        /* cf Military_Installations_2008.pdf that has values like "96 0 0.0W" */
        char chLast = pszStr[nLen-1];
        if (chLast == 'W' || chLast == 'E' || chLast == 'N' || chLast == 'S')
        {
            double dfDeg = atof(pszStr);
            double dfMin = 0, dfSec = 0;
            const char* pszNext = strchr(pszStr, ' ');
            if (pszNext)
                pszNext ++;
            if (pszNext)
                dfMin = atof(pszNext);
            if (pszNext)
                pszNext = strchr(pszNext, ' ');
            if (pszNext)
                pszNext ++;
            if (pszNext)
                dfSec = atof(pszNext);
            double dfVal = dfDeg + dfMin / 60 + dfSec / 3600;
            if (chLast == 'W' || chLast == 'S')
                return -dfVal;
            else
                return dfVal;
        }
        return atof(pszStr);
    }
    else
    {
        CPLError(CE_Warning, CPLE_AppDefined, "Unexpected type : %s",
                 o.getTypeName());
        return 0;
    }
}

/************************************************************************/
/*                            GetValue()                                */
/************************************************************************/

static double GetValue(Dict* poDict, const char* pszName)
{
    ObjectAutoFree o;
    if (poDict->lookup((char*)pszName, &o) != NULL && !o.isNull())
        return GetValue(o);
    CPLError(CE_Failure, CPLE_AppDefined,
             "Cannot find parameter %s", pszName);
    return 0;
}

/************************************************************************/
/*                   ParseLGIDictDictFirstPass()                        */
/************************************************************************/

int PDFDataset::ParseLGIDictDictFirstPass(Dict* poLGIDict,
                                             int* pbIsLargestArea)
{
    int i;

    if (pbIsLargestArea)
        *pbIsLargestArea = FALSE;

    if (poLGIDict == NULL)
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Extract Type attribute                                          */
/* -------------------------------------------------------------------- */
    ObjectAutoFree oType;
    if (poLGIDict->lookup((char*)"Type",&oType) == NULL &&
        !oType.isNull())
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot find Type of LGIDict object");
        return FALSE;
    }

    if ( !oType.isName() )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Invalid type for Type of LGIDict object");
        return FALSE;
    }

    if ( strcmp(oType.getName(), "LGIDict") != 0 )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Invalid value for Type of LGIDict object : %s",
                 oType.getName());
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Extract Version attribute                                       */
/* -------------------------------------------------------------------- */
    ObjectAutoFree oVersion;
    if (poLGIDict->lookup((char*)"Version",&oVersion) == NULL &&
        !oVersion.isNull())
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot find Version of LGIDict object");
        return FALSE;
    }

    if (oVersion.isString())
    {
        /* OGC best practice is 2.1 */
        CPLDebug("PDF", "LGIDict Version : %s",
                 oVersion.getString()->getCString());
    }
    else if (oVersion.isInt())
    {
        /* Old TerraGo is 2 */
        CPLDebug("PDF", "LGIDict Version : %d",
                 oVersion.getInt());
    }

/* -------------------------------------------------------------------- */
/*      Extract Neatline attribute                                      */
/* -------------------------------------------------------------------- */
    ObjectAutoFree oNeatline;
    if (poLGIDict->lookup((char*)"Neatline", &oNeatline) != NULL &&
        oNeatline.isArray() )
    {
        int nLength = oNeatline.arrayGetLength();
        if ( (nLength % 2) != 0 || nLength < 4 )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Invalid length for Neatline");
            return FALSE;
        }

        double dfMinX = 0, dfMinY = 0, dfMaxX = 0, dfMaxY = 0;
        for(i=0;i<nLength;i+=2)
        {
            double dfX = GetValue(oNeatline, i);
            double dfY = GetValue(oNeatline, i + 1);
            if (i == 0 || dfX < dfMinX) dfMinX = dfX;
            if (i == 0 || dfY < dfMinY) dfMinY = dfY;
            if (i == 0 || dfX > dfMaxX) dfMaxX = dfX;
            if (i == 0 || dfY > dfMaxY) dfMaxY = dfY;
        }
        double dfArea = (dfMaxX - dfMinX) * (dfMaxY - dfMinY);
        if (dfArea < dfMaxArea)
        {
            CPLDebug("PDF", "Not the largest neatline. Skipping it");
            return TRUE;
        }

        CPLDebug("PDF", "This is a the largest neatline for now");
        dfMaxArea = dfArea;
        if (pbIsLargestArea)
            *pbIsLargestArea = TRUE;

        delete poNeatLine;
        poNeatLine = new OGRPolygon();
        OGRLinearRing* poRing = new OGRLinearRing();
        for(i=0;i<nLength;i+=2)
        {
            double dfX = GetValue(oNeatline, i);
            double dfY = GetValue(oNeatline, i + 1);
            poRing->addPoint(dfX, dfY);
        }
        poNeatLine->addRingDirectly(poRing);
    }

    return TRUE;
}

/************************************************************************/
/*                  ParseLGIDictDictSecondPass()                        */
/************************************************************************/

int PDFDataset::ParseLGIDictDictSecondPass(Dict* poLGIDict)
{
    int i;

/* -------------------------------------------------------------------- */
/*      Extract CTM attribute                                           */
/* -------------------------------------------------------------------- */
    ObjectAutoFree oCTM;
    int bHasCTM = FALSE;
    if (poLGIDict->lookup((char*)"CTM", &oCTM) != NULL &&
        oCTM.isArray())
    {
        int nLength = oCTM.arrayGetLength();
        if ( nLength != 6 )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Invalid length for CTM");
            return FALSE;
        }

        bHasCTM = TRUE;
        for(i=0;i<nLength;i++)
        {
            adfCTM[i] = GetValue(oCTM, i);
            /* Nullify rotation terms that are significantly smaller than */
            /* scaling termes */
            if ((i == 1 || i == 2) && fabs(adfCTM[i]) < fabs(adfCTM[0]) * 1e-10)
                adfCTM[i] = 0;
            CPLDebug("PDF", "CTM[%d] = %.16g", i, adfCTM[i]);
        }
    }

/* -------------------------------------------------------------------- */
/*      Extract Registration attribute                                  */
/* -------------------------------------------------------------------- */
    if (!bHasCTM)
    {
        ObjectAutoFree oRegistration;
        if (poLGIDict->lookup((char*)"Registration", &oRegistration) != NULL &&
            !oRegistration.isNull())
        {
            /* TODO */
            CPLDebug("PDF", "Registration unhandled for now");
            return FALSE;
        }
        else
        {
            CPLDebug("PDF", "Neither CTM nor Registration found");
            return FALSE;
        }
    }

/* -------------------------------------------------------------------- */
/*      Extract Projection attribute                                    */
/* -------------------------------------------------------------------- */
    ObjectAutoFree oProjection;
    if (poLGIDict->lookup((char*)"Projection", &oProjection) == NULL ||
        !oProjection.isDict())
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Could not find Projection");
        return FALSE;
    }

    return ParseProjDict(oProjection.getDict());
}

/************************************************************************/
/*                         ParseProjDict()                               */
/************************************************************************/

int PDFDataset::ParseProjDict(Dict* poProjDict)
{
    if (poProjDict == NULL)
        return FALSE;
    OGRSpatialReference oSRS;

/* -------------------------------------------------------------------- */
/*      Extract Type attribute                                          */
/* -------------------------------------------------------------------- */
    ObjectAutoFree oType;
    if (poProjDict->lookup((char*)"Type",&oType) == NULL &&
        !oType.isNull())
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot find Type of Projection object");
        return FALSE;
    }

    if ( !oType.isName() )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Invalid type for Type of Projection object");
        return FALSE;
    }

    if ( strcmp(oType.getName(), "Projection") != 0 )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Invalid value for Type of Projection object : %s",
                 oType.getName());
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Extract Datum attribute                                         */
/* -------------------------------------------------------------------- */
    int bIsWGS84 = FALSE;
    int bIsNAD83 = FALSE;
    int bIsNAD27 = FALSE;

    ObjectAutoFree oDatum;
    if (poProjDict->lookup((char*)"Datum",&oDatum) != NULL &&
        !oDatum.isNull())
    {
        if (oDatum.isString())
        {
            char* pszDatum = oDatum.getString()->getCString();
            CPLDebug("PDF", "Datum = %s", pszDatum);
            if (EQUAL(pszDatum, "WE") || EQUAL(pszDatum, "WGE"))
            {
                bIsWGS84 = TRUE;
                oSRS.SetWellKnownGeogCS("WGS84");
            }
            else if (EQUAL(pszDatum, "NAR") || EQUALN(pszDatum, "NAR-", 4))
            {
                bIsNAD83 = TRUE;
                oSRS.SetWellKnownGeogCS("NAD83");
            }
            else if (EQUAL(pszDatum, "NAS") || EQUALN(pszDatum, "NAS-", 4))
            {
                bIsNAD27 = TRUE;
                oSRS.SetWellKnownGeogCS("NAD27");
            }
            else if (EQUAL(pszDatum, "HEN")) /* HERAT North, Afghanistan */
            {
                oSRS.SetGeogCS( "unknown" /*const char * pszGeogName*/,
                                "unknown" /*const char * pszDatumName */,
                                "International 1924",
                                6378388,297);
                oSRS.SetTOWGS84(-333,-222,114);
            }
            else if (EQUAL(pszDatum, "ING-A")) /* INDIAN 1960, Vietnam 16N */
            {
                oSRS.importFromEPSG(4131);
            }
            else if (EQUAL(pszDatum, "GDS")) /* Geocentric Datum of Australia */
            {
                oSRS.importFromEPSG(4283);
            }
            else
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                        "Unhandled (yet) value for Datum : %s. Defaulting to WGS84...",
                        pszDatum);
                oSRS.SetGeogCS( "unknown" /*const char * pszGeogName*/,
                                "unknown" /*const char * pszDatumName */,
                                "unknown",
                                6378137,298.257223563);
            }
        }
        else if (oDatum.isDict())
        {
            /* TODO */
            CPLError(CE_Warning, CPLE_AppDefined,
                     "Datum as dictionary unhandled yet. Defaulting to WGS84...");
            oSRS.SetGeogCS( "unknown" /*const char * pszGeogName*/,
                                "unknown" /*const char * pszDatumName */,
                                "unknown",
                                6378137,298.257223563);
        }
    }

/* -------------------------------------------------------------------- */
/*      Extract Hemisphere attribute                                    */
/* -------------------------------------------------------------------- */
    ObjectAutoFree oHemisphere;
    CPLString osHemisphere;
    if (poProjDict->lookup((char*)"Hemisphere",&oHemisphere) != NULL &&
        oHemisphere.isString())
    {
        osHemisphere = oHemisphere.getString()->getCString();
    }

/* -------------------------------------------------------------------- */
/*      Extract ProjectionType attribute                                */
/* -------------------------------------------------------------------- */
    ObjectAutoFree oProjectionType;
    if (poProjDict->lookup((char*)"ProjectionType",&oProjectionType) == NULL ||
        !oProjectionType.isString())
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot find ProjectionType of Projection object");
        return FALSE;
    }
    CPLString osProjectionType(oProjectionType.getString()->getCString());
    CPLDebug("PDF", "Projection.ProjectionType = %s", osProjectionType.c_str());

    /* Unhandled: NONE, GEODETIC */

    if (EQUAL(osProjectionType, "GEOGRAPHIC"))
    {
        /* Nothing to do */
    }

    /* Unhandled: LOCAL CARTESIAN, MG (MGRS) */

    else if (EQUAL(osProjectionType, "UT")) /* UTM */
    {
        int nZone = (int)GetValue(poProjDict, "Zone");
        int bNorth = EQUAL(osHemisphere, "N");
        if (bIsWGS84)
            oSRS.importFromEPSG( ((bNorth) ? 32600 : 32700) + nZone );
        else
            oSRS.SetUTM( nZone, bNorth );
    }

    else if (EQUAL(osProjectionType, "UP")) /* Universal Polar Stereographic (UPS) */
    {
        int bNorth = EQUAL(osHemisphere, "N");
        if (bIsWGS84)
            oSRS.importFromEPSG( (bNorth) ? 32661 : 32761 );
        else
            oSRS.SetPS( (bNorth) ? 90 : -90, 0,
                        0.994, 200000, 200000 );
    }

    else if (EQUAL(osProjectionType, "SPCS")) /* State Plane */
    {
        int nZone = (int)GetValue(poProjDict, "Zone");
        oSRS.SetStatePlane( nZone, bIsNAD83 );
    }

    else if (EQUAL(osProjectionType, "AC")) /* Albers Equal Area Conic */
    {
        double dfStdP1 = GetValue(poProjDict, "StandardParallelOne");
        double dfStdP2 = GetValue(poProjDict, "StandardParallelTwo");
        double dfCenterLat = GetValue(poProjDict, "OriginLatitude");
        double dfCenterLong = GetValue(poProjDict, "CentralMeridian");
        double dfFalseEasting = GetValue(poProjDict, "FalseEasting");
        double dfFalseNorthing = GetValue(poProjDict, "FalseNorthing");
        oSRS.SetACEA( dfStdP1, dfStdP2,
                     dfCenterLat, dfCenterLong,
                     dfFalseEasting, dfFalseNorthing );
    }
 
    else if (EQUAL(osProjectionType, "AL")) /* Azimuthal Equidistant */
    {
        double dfCenterLat = GetValue(poProjDict, "OriginLatitude");
        double dfCenterLong = GetValue(poProjDict, "CentralMeridian");
        double dfFalseEasting = GetValue(poProjDict, "FalseEasting");
        double dfFalseNorthing = GetValue(poProjDict, "FalseNorthing");
        oSRS.SetAE(  dfCenterLat, dfCenterLong,
                     dfFalseEasting, dfFalseNorthing );
    }

    else if (EQUAL(osProjectionType, "BF")) /* Bonne */
    {
        double dfStdP1 = GetValue(poProjDict, "OriginLatitude");
        double dfCentralMeridian = GetValue(poProjDict, "CentralMeridian");
        double dfFalseEasting = GetValue(poProjDict, "FalseEasting");
        double dfFalseNorthing = GetValue(poProjDict, "FalseNorthing");
        oSRS.SetBonne( dfStdP1, dfCentralMeridian,
                       dfFalseEasting, dfFalseNorthing );
    }

    else if (EQUAL(osProjectionType, "CS")) /* Cassini */
    {
        double dfCenterLat = GetValue(poProjDict, "OriginLatitude");
        double dfCenterLong = GetValue(poProjDict, "CentralMeridian");
        double dfFalseEasting = GetValue(poProjDict, "FalseEasting");
        double dfFalseNorthing = GetValue(poProjDict, "FalseNorthing");
        oSRS.SetCS(  dfCenterLat, dfCenterLong,
                     dfFalseEasting, dfFalseNorthing );
    }

    else if (EQUAL(osProjectionType, "LI")) /* Cylindrical Equal Area */
    {
        double dfStdP1 = GetValue(poProjDict, "OriginLatitude");
        double dfCentralMeridian = GetValue(poProjDict, "CentralMeridian");
        double dfFalseEasting = GetValue(poProjDict, "FalseEasting");
        double dfFalseNorthing = GetValue(poProjDict, "FalseNorthing");
        oSRS.SetCEA( dfStdP1, dfCentralMeridian,
                     dfFalseEasting, dfFalseNorthing );
    }

    else if (EQUAL(osProjectionType, "EF")) /* Eckert IV */
    {
        double dfCentralMeridian = GetValue(poProjDict, "CentralMeridian");
        double dfFalseEasting = GetValue(poProjDict, "FalseEasting");
        double dfFalseNorthing = GetValue(poProjDict, "FalseNorthing");
        oSRS.SetEckertIV( dfCentralMeridian,
                          dfFalseEasting, dfFalseNorthing );
    }

    else if (EQUAL(osProjectionType, "ED")) /* Eckert VI */
    {
        double dfCentralMeridian = GetValue(poProjDict, "CentralMeridian");
        double dfFalseEasting = GetValue(poProjDict, "FalseEasting");
        double dfFalseNorthing = GetValue(poProjDict, "FalseNorthing");
        oSRS.SetEckertVI( dfCentralMeridian,
                          dfFalseEasting, dfFalseNorthing );
    }

    else if (EQUAL(osProjectionType, "CP")) /* Equidistant Cylindrical */
    {
        double dfCenterLat = GetValue(poProjDict, "StandardParallel");
        double dfCenterLong = GetValue(poProjDict, "CentralMeridian");
        double dfFalseEasting = GetValue(poProjDict, "FalseEasting");
        double dfFalseNorthing = GetValue(poProjDict, "FalseNorthing");
        oSRS.SetEquirectangular( dfCenterLat, dfCenterLong,
                                 dfFalseEasting, dfFalseNorthing );
    }

    else if (EQUAL(osProjectionType, "GN")) /* Gnomonic */
    {
        double dfCenterLat = GetValue(poProjDict, "OriginLatitude");
        double dfCenterLong = GetValue(poProjDict, "CentralMeridian");
        double dfFalseEasting = GetValue(poProjDict, "FalseEasting");
        double dfFalseNorthing = GetValue(poProjDict, "FalseNorthing");
        oSRS.SetGnomonic(dfCenterLat, dfCenterLong,
                         dfFalseEasting, dfFalseNorthing );
    }

    else if (EQUAL(osProjectionType, "LE")) /* Lambert Conformal Conic */
    {
        double dfStdP1 = GetValue(poProjDict, "StandardParallelOne");
        double dfStdP2 = GetValue(poProjDict, "StandardParallelTwo");
        double dfCenterLat = GetValue(poProjDict, "OriginLatitude");
        double dfCenterLong = GetValue(poProjDict, "CentralMeridian");
        double dfFalseEasting = GetValue(poProjDict, "FalseEasting");
        double dfFalseNorthing = GetValue(poProjDict, "FalseNorthing");
        oSRS.SetLCC( dfStdP1, dfStdP2,
                     dfCenterLat, dfCenterLong,
                     dfFalseEasting, dfFalseNorthing );
    }

    else if (EQUAL(osProjectionType, "MC")) /* Mercator */
    {
        double dfCenterLat = GetValue(poProjDict, "OriginLatitude");
        double dfCenterLong = GetValue(poProjDict, "CentralMeridian");
        double dfScale = GetValue(poProjDict, "ScaleFactor");
        double dfFalseEasting = GetValue(poProjDict, "FalseEasting");
        double dfFalseNorthing = GetValue(poProjDict, "FalseNorthing");
        oSRS.SetMercator( dfCenterLat, dfCenterLong,
                          dfScale,
                          dfFalseEasting, dfFalseNorthing );
    }

    else if (EQUAL(osProjectionType, "MH")) /* Miller Cylindrical */
    {
        double dfCenterLat = 0 /* ? */;
        double dfCenterLong = GetValue(poProjDict, "CentralMeridian");
        double dfFalseEasting = GetValue(poProjDict, "FalseEasting");
        double dfFalseNorthing = GetValue(poProjDict, "FalseNorthing");
        oSRS.SetMC( dfCenterLat, dfCenterLong,
                    dfFalseEasting, dfFalseNorthing );
    }

    else if (EQUAL(osProjectionType, "MP")) /* Mollweide */
    {
        double dfCentralMeridian = GetValue(poProjDict, "CentralMeridian");
        double dfFalseEasting = GetValue(poProjDict, "FalseEasting");
        double dfFalseNorthing = GetValue(poProjDict, "FalseNorthing");
        oSRS.SetMollweide( dfCentralMeridian,
                           dfFalseEasting, dfFalseNorthing );
    }

    /* Unhandled:  "NY" : Ney's (Modified Lambert Conformal Conic) */

    else if (EQUAL(osProjectionType, "NT")) /* New Zealand Map Grid */
    {
        /* No parameter specified in the PDF, so let's take the ones of EPSG:27200 */
        double dfCenterLat = -41;
        double dfCenterLong = 173;
        double dfFalseEasting = 2510000;
        double dfFalseNorthing = 6023150;
        oSRS.SetNZMG( dfCenterLat, dfCenterLong,
                      dfFalseEasting, dfFalseNorthing );
    }

    else if (EQUAL(osProjectionType, "OC")) /* Oblique Mercator */
    {
        double dfCenterLat = GetValue(poProjDict, "OriginLatitude");
        double dfLat1 = GetValue(poProjDict, "LatitudeOne");
        double dfLong1 = GetValue(poProjDict, "LongitudeOne");
        double dfLat2 = GetValue(poProjDict, "LatitudeTwo");
        double dfLong2 = GetValue(poProjDict, "LongitudeTwo");
        double dfScale = GetValue(poProjDict, "ScaleFactor");
        double dfFalseEasting = GetValue(poProjDict, "FalseEasting");
        double dfFalseNorthing = GetValue(poProjDict, "FalseNorthing");
        oSRS.SetHOM2PNO( dfCenterLat,
                         dfLat1, dfLong1,
                         dfLat2, dfLong2,
                         dfScale,
                         dfFalseEasting,
                         dfFalseNorthing );
    }

    else if (EQUAL(osProjectionType, "OD")) /* Orthographic */
    {
        double dfCenterLat = GetValue(poProjDict, "OriginLatitude");
        double dfCenterLong = GetValue(poProjDict, "CentralMeridian");
        double dfFalseEasting = GetValue(poProjDict, "FalseEasting");
        double dfFalseNorthing = GetValue(poProjDict, "FalseNorthing");
        oSRS.SetOrthographic( dfCenterLat, dfCenterLong,
                           dfFalseEasting, dfFalseNorthing );
    }

    else if (EQUAL(osProjectionType, "PG")) /* Polar Stereographic */
    {
        double dfCenterLat = GetValue(poProjDict, "LatitudeTrueScale");
        double dfCenterLong = GetValue(poProjDict, "LongitudeDownFromPole");
        double dfScale = 1.0;
        double dfFalseEasting = GetValue(poProjDict, "FalseEasting");
        double dfFalseNorthing = GetValue(poProjDict, "FalseNorthing");
        oSRS.SetPS( dfCenterLat, dfCenterLong,
                    dfScale,
                    dfFalseEasting, dfFalseNorthing);
    }

    else if (EQUAL(osProjectionType, "PH")) /* Polyconic */
    {
        double dfCenterLat = GetValue(poProjDict, "OriginLatitude");
        double dfCenterLong = GetValue(poProjDict, "CentralMeridian");
        double dfFalseEasting = GetValue(poProjDict, "FalseEasting");
        double dfFalseNorthing = GetValue(poProjDict, "FalseNorthing");
        oSRS.SetPolyconic( dfCenterLat, dfCenterLong,
                           dfFalseEasting, dfFalseNorthing );
    }

    else if (EQUAL(osProjectionType, "SA")) /* Sinusoidal */
    {
        double dfCenterLong = GetValue(poProjDict, "CentralMeridian");
        double dfFalseEasting = GetValue(poProjDict, "FalseEasting");
        double dfFalseNorthing = GetValue(poProjDict, "FalseNorthing");
        oSRS.SetSinusoidal( dfCenterLong,
                           dfFalseEasting, dfFalseNorthing );
    }

    else if (EQUAL(osProjectionType, "SD")) /* Stereographic */
    {
        double dfCenterLat = GetValue(poProjDict, "OriginLatitude");
        double dfCenterLong = GetValue(poProjDict, "CentralMeridian");
        double dfScale = 1.0;
        double dfFalseEasting = GetValue(poProjDict, "FalseEasting");
        double dfFalseNorthing = GetValue(poProjDict, "FalseNorthing");
        oSRS.SetStereographic( dfCenterLat, dfCenterLong,
                               dfScale,
                               dfFalseEasting, dfFalseNorthing);
    }

    else if (EQUAL(osProjectionType, "TC")) /* Transverse Mercator */
    {
        double dfCenterLat = GetValue(poProjDict, "OriginLatitude");
        double dfCenterLong = GetValue(poProjDict, "CentralMeridian");
        double dfScale = GetValue(poProjDict, "ScaleFactor");
        double dfFalseEasting = GetValue(poProjDict, "FalseEasting");
        double dfFalseNorthing = GetValue(poProjDict, "FalseNorthing");
        if (dfCenterLat == 0.0 && dfScale == 0.9996 && dfFalseEasting == 500000 &&
            (dfFalseNorthing == 0.0 || dfFalseNorthing == 10000000.0))
        {
            int nZone = (int) floor( (dfCenterLong + 180.0) / 6.0 ) + 1;
            int bNorth = dfFalseNorthing == 0;
            if (bIsWGS84)
                oSRS.importFromEPSG( ((bNorth) ? 32600 : 32700) + nZone );
            else if (bIsNAD83 && bNorth)
                oSRS.importFromEPSG( 26900 + nZone );
            else
                oSRS.SetUTM( nZone, bNorth );
        }
        else
        {
            oSRS.SetTM( dfCenterLat, dfCenterLong,
                        dfScale,
                        dfFalseEasting, dfFalseNorthing );
        }
    }

    /* Unhandled TX : Transverse Cylindrical Equal Area */

    else if (EQUAL(osProjectionType, "VA")) /* Van der Grinten */
    {
        double dfCenterLong = GetValue(poProjDict, "CentralMeridian");
        double dfFalseEasting = GetValue(poProjDict, "FalseEasting");
        double dfFalseNorthing = GetValue(poProjDict, "FalseNorthing");
        oSRS.SetVDG( dfCenterLong,
                     dfFalseEasting, dfFalseNorthing );
    }

    else
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Unhandled (yet) value for ProjectionType : %s",
                 osProjectionType.c_str());
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Extract Units attribute                                         */
/* -------------------------------------------------------------------- */
    ObjectAutoFree oUnits;
    CPLString osUnits;
    if (poProjDict->lookup((char*)"Units",&oUnits) != NULL &&
        oUnits.isString())
    {
        osUnits = oUnits.getString()->getCString();
        CPLDebug("PDF", "Projection.Units = %s", osUnits.c_str());

        if (EQUAL(osUnits, "FT"))
            oSRS.SetLinearUnits( "Foot", 0.3048 );
    }

/* -------------------------------------------------------------------- */
/*      Export SpatialRef                                               */
/* -------------------------------------------------------------------- */
    CPLFree(pszWKT);
    pszWKT = NULL;
    if (oSRS.exportToWkt(&pszWKT) != OGRERR_NONE)
    {
        CPLFree(pszWKT);
        pszWKT = NULL;
    }

    return TRUE;
}

/************************************************************************/
/*                              ParseVP()                               */
/************************************************************************/

int PDFDataset::ParseVP(Object& oVP, double dfMediaBoxWidth, double dfMediaBoxHeight)
{
    int i;

    if (!oVP.isArray())
        return FALSE;

    int nLength = oVP.arrayGetLength();
    CPLDebug("PDF", "VP length = %d", nLength);
    if (nLength < 1)
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Find the largest BBox                                           */
/* -------------------------------------------------------------------- */
    int iLargest = 0;
    double dfLargestArea = 0;

    for(i=0;i<nLength;i++)
    {
        ObjectAutoFree oVPElt;
        if ( !oVP.arrayGet(i,&oVPElt) || !oVPElt.isDict() )
        {
            return FALSE;
        }

        ObjectAutoFree oBBox;
        if( !oVPElt.dictLookup((char*)"BBox",&oBBox) ||
            !oBBox.isArray() )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                    "Cannot find Bbox object");
            return FALSE;
        }

        int nBboxLength = oBBox.arrayGetLength();
        if (nBboxLength != 4)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                    "Invalid length for Bbox object");
            return FALSE;
        }

        double adfBBox[4];
        adfBBox[0] = GetValue(oBBox, 0);
        adfBBox[1] = GetValue(oBBox, 1);
        adfBBox[2] = GetValue(oBBox, 2);
        adfBBox[3] = GetValue(oBBox, 3);
        double dfArea = fabs(adfBBox[2] - adfBBox[0]) * fabs(adfBBox[3] - adfBBox[1]);
        if (dfArea > dfLargestArea)
        {
            iLargest = i;
            dfLargestArea = dfArea;
        }
    }

    if (nLength > 1)
    {
        CPLDebug("PDF", "Largest BBox in VP array is element %d", iLargest);
    }


    ObjectAutoFree oVPElt;
    if ( !oVP.arrayGet(iLargest,&oVPElt) || !oVPElt.isDict() )
    {
        return FALSE;
    }

    ObjectAutoFree oBBox;
    if( !oVPElt.dictLookup((char*)"BBox",&oBBox) ||
        !oBBox.isArray() )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                "Cannot find Bbox object");
        return FALSE;
    }

    int nBboxLength = oBBox.arrayGetLength();
    if (nBboxLength != 4)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                "Invalid length for Bbox object");
        return FALSE;
    }

    double dfULX = GetValue(oBBox, 0);
    double dfULY = dfMediaBoxHeight - GetValue(oBBox, 1);
    double dfLRX = GetValue(oBBox, 2);
    double dfLRY = dfMediaBoxHeight - GetValue(oBBox, 3);

/* -------------------------------------------------------------------- */
/*      Extract Measure attribute                                       */
/* -------------------------------------------------------------------- */
    ObjectAutoFree oMeasure;
    if( !oVPElt.dictLookup((char*)"Measure",&oMeasure) ||
        !oMeasure.isDict() )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot find Measure object");
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Extract Subtype attribute                                       */
/* -------------------------------------------------------------------- */
    ObjectAutoFree oSubtype;
    if( !oMeasure.dictLookup((char*)"Subtype",&oSubtype) ||
        !oSubtype.isName() )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot find Subtype object");
        return FALSE;
    }

    CPLDebug("PDF", "Subtype = %s", oSubtype.getName());

/* -------------------------------------------------------------------- */
/*      Extract Bounds attribute                                       */
/* -------------------------------------------------------------------- */

    /* http://acrobatusers.com/sites/default/files/gallery_pictures/SEVERODVINSK.pdf */
    /* has lgit:LPTS, lgit:GPTS and lgit:Bounds that have more precision than */
    /* LPTS, GPTS and Bounds. Use those ones */

    ObjectAutoFree oBounds;
    if( oMeasure.dictLookup((char*)"lgit:Bounds",&oBounds) &&
        oBounds.isArray() )
    {
        CPLDebug("PDF", "Using lgit:Bounds");
    }
    else if( !oMeasure.dictLookup((char*)"Bounds",&oBounds) ||
        !oBounds.isArray() )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot find Bounds object");
        return FALSE;
    }

    int nBoundsLength = oBounds.arrayGetLength();
    if (nBoundsLength != 8)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Invalid length for Bounds object");
        return FALSE;
    }

    double adfBounds[8];
    for(i=0;i<8;i++)
    {
        adfBounds[i] = GetValue(oBounds, i);
        CPLDebug("PDF", "Bounds[%d] = %f", i, adfBounds[i]);
    }

/* -------------------------------------------------------------------- */
/*      Extract GPTS attribute                                          */
/* -------------------------------------------------------------------- */
    ObjectAutoFree oGPTS;
    if( oMeasure.dictLookup((char*)"lgit:GPTS",&oGPTS) &&
        oGPTS.isArray() )
    {
        CPLDebug("PDF", "Using lgit:GPTS");
    }
    else if( !oMeasure.dictLookup((char*)"GPTS",&oGPTS) ||
        !oGPTS.isArray() )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot find GPTS object");
        return FALSE;
    }

    int nGPTSLength = oGPTS.arrayGetLength();
    if (nGPTSLength != 8)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Invalid length for GPTS object");
        return FALSE;
    }

    double adfGPTS[8];
    for(i=0;i<8;i++)
    {
        adfGPTS[i] = GetValue(oGPTS, i);
        CPLDebug("PDF", "GPTS[%d] = %.18f", i, adfGPTS[i]);
    }

/* -------------------------------------------------------------------- */
/*      Extract LPTS attribute                                          */
/* -------------------------------------------------------------------- */
    ObjectAutoFree oLPTS;

    if( oMeasure.dictLookup((char*)"lgit:LPTS",&oLPTS) &&
        oLPTS.isArray() )
    {
        CPLDebug("PDF", "Using lgit:LPTS");
    }
    else if( !oMeasure.dictLookup((char*)"LPTS",&oLPTS) ||
        !oLPTS.isArray() )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot find LPTS object");
        return FALSE;
    }

    int nLPTSLength = oLPTS.arrayGetLength();
    if (nLPTSLength != 8)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Invalid length for LPTS object");
        return FALSE;
    }

    double adfLPTS[8];
    for(i=0;i<8;i++)
    {
        adfLPTS[i] = GetValue(oLPTS, i);
        CPLDebug("PDF", "LPTS[%d] = %f", i, adfLPTS[i]);
    }

/* -------------------------------------------------------------------- */
/*      Extract GCS attribute                                           */
/* -------------------------------------------------------------------- */
    ObjectAutoFree oGCS;
    if( !oMeasure.dictLookup((char*)"GCS",&oGCS) ||
        !oGCS.isDict() )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot find GCS object");
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Extract GCS.Type attribute                                      */
/* -------------------------------------------------------------------- */
    ObjectAutoFree oGCSType;
    if( !oGCS.dictLookup((char*)"Type",&oGCSType) ||
        !oGCSType.isName() )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot find GCS.Type object");
        return FALSE;
    }

    CPLDebug("PDF", "GCS.Type = %s", oGCSType.getName());

/* -------------------------------------------------------------------- */
/*      Extract GCS.WKT attribute                                      */
/* -------------------------------------------------------------------- */
    ObjectAutoFree oGCSWKT;
    if( !oGCS.dictLookup((char*)"WKT",&oGCSWKT) ||
        !oGCSWKT.isString() )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot find GCS.WKT object");
        return FALSE;
    }

    CPLDebug("PDF", "GCS.WKT = %s", oGCSWKT.getString()->getCString());
    CPLFree(pszWKT);
    pszWKT = CPLStrdup(oGCSWKT.getString()->getCString());

/* -------------------------------------------------------------------- */
/*      Compute geotransform                                            */
/* -------------------------------------------------------------------- */
    OGRSpatialReference oSRS;
    char* pszWktTemp = pszWKT;
    if (oSRS.importFromWkt(&pszWktTemp) != OGRERR_NONE)
    {
        CPLFree(pszWKT);
        pszWKT = NULL;
        return FALSE;
    }

    /* For http://www.avenza.com/sites/default/files/spatialpdf/US_County_Populations.pdf */
    /* or http://www.agmkt.state.ny.us/soilwater/aem/gis_mapping_tools/HUC12_Albany.pdf */
    const char* pszDatum = oSRS.GetAttrValue("Datum");
    if (pszDatum && strncmp(pszDatum, "D_", 2) == 0)
    {
        oSRS.morphFromESRI();

        CPLFree(pszWKT);
        pszWKT = NULL;
        if (oSRS.exportToWkt(&pszWKT) != OGRERR_NONE)
        {
            CPLFree(pszWKT);
            pszWKT = NULL;
        }
        else
        {
            CPLDebug("PDF", "WKT after morphFromESRI() = %s", pszWKT);
        }
    }

    OGRSpatialReference* poSRSGeog = oSRS.CloneGeogCS();

    OGRCoordinateTransformation* poCT = OGRCreateCoordinateTransformation( poSRSGeog, &oSRS);
    if (poCT == NULL)
    {
        delete poSRSGeog;
        CPLFree(pszWKT);
        pszWKT = NULL;
        return FALSE;
    }

    GDAL_GCP asGCPS[4];

    /* Create NEATLINE */
    poNeatLine = new OGRPolygon();
    OGRLinearRing* poRing = new OGRLinearRing();
    poNeatLine->addRingDirectly(poRing);

    for(i=0;i<4;i++)
    {
        /* We probably assume LPTS is 0 or 1 */
        asGCPS[i].dfGCPPixel = (dfULX * (1 - adfLPTS[2*i+0]) + dfLRX * adfLPTS[2*i+0]) / dfMediaBoxWidth * nRasterXSize;
        asGCPS[i].dfGCPLine  = (dfULY * (1 - adfLPTS[2*i+1]) + dfLRY * adfLPTS[2*i+1]) / dfMediaBoxHeight * nRasterYSize;

        double lat = adfGPTS[2*i], lon = adfGPTS[2*i+1];
        double x = lon, y = lat;
        if (!poCT->Transform(1, &x, &y, NULL))
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Cannot reproject (%f, %f)", lon, lat);
            delete poSRSGeog;
            delete poCT;
            CPLFree(pszWKT);
            pszWKT = NULL;
            return FALSE;
        }
        asGCPS[i].dfGCPX     = x;
        asGCPS[i].dfGCPY     = y;

        poRing->addPoint(x, y);
    }

    delete poSRSGeog;
    delete poCT;

    if (!GDALGCPsToGeoTransform( 4, asGCPS,
                               adfGeoTransform, FALSE))
    {
        CPLDebug("PDF", "Could not compute GT with exact match. Try with approximate");
        if (!GDALGCPsToGeoTransform( 4, asGCPS,
                               adfGeoTransform, TRUE))
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Could not compute GT with approximate match.");
            return FALSE;
        }
    }
    bGeoTransformValid = TRUE;

    /* If the non scaling terms of the geotransform are significantly smaller than */
    /* the pixel size, then nullify them as being just artifacts of reprojection and */
    /* GDALGCPsToGeoTransform() numerical imprecisions */
    double dfPixelSize = MIN(fabs(adfGeoTransform[1]), fabs(adfGeoTransform[5]));
    double dfRotationShearTerm = MAX(fabs(adfGeoTransform[2]), fabs(adfGeoTransform[4]));
    if (dfRotationShearTerm < 1e-5 * dfPixelSize)
    {
        double dfLRX = adfGeoTransform[0] + nRasterXSize * adfGeoTransform[1] + nRasterYSize * adfGeoTransform[2];
        double dfLRY = adfGeoTransform[3] + nRasterXSize * adfGeoTransform[4] + nRasterYSize * adfGeoTransform[5];
        adfGeoTransform[1] = (dfLRX - adfGeoTransform[0]) / nRasterXSize;
        adfGeoTransform[5] = (dfLRY - adfGeoTransform[3]) / nRasterYSize;
        adfGeoTransform[2] = adfGeoTransform[4] = 0;
    }

/* -------------------------------------------------------------------- */
/*      Extract PointData attribute                                     */
/* -------------------------------------------------------------------- */
    ObjectAutoFree oPointData;
    if( oVPElt.dictLookup((char*)"PtData",&oPointData) &&
        oPointData.isDict() )
    {
        CPLDebug("PDF", "Found PointData");
    }

    return TRUE;
}

/************************************************************************/
/*                          GetProjectionRef()                          */
/************************************************************************/

const char* PDFDataset::GetProjectionRef()
{
    if (pszWKT)
        return pszWKT;
    return "";
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr PDFDataset::GetGeoTransform( double * padfTransform )

{
    memcpy(padfTransform, adfGeoTransform, 6 * sizeof(double));

    return( (bGeoTransformValid) ? CE_None : CE_Failure );
}

/************************************************************************/
/*                         GDALRegister_PDF()                           */
/************************************************************************/

void GDALRegister_PDF()

{
    GDALDriver  *poDriver;

    if (! GDAL_CHECK_VERSION("PDF driver"))
        return;

    if( GDALGetDriverByName( "PDF" ) == NULL )
    {
        poDriver = new GDALDriver();

        poDriver->SetDescription( "PDF" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME,
                                   "Geospatial PDF" );
        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC,
                                   "frmt_pdf.html" );
        poDriver->SetMetadataItem( GDAL_DMD_EXTENSION, "pdf" );

        poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );

        poDriver->pfnOpen = PDFDataset::Open;
        poDriver->pfnIdentify = PDFDataset::Identify;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}

