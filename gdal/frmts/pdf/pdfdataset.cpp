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

#ifdef USE_POPPLER
#include "pdfio.h"
#endif

#include "pdfobject.h"
#include "pdfcreatecopy.h"

/* g++ -fPIC -g -Wall frmts/pdf/pdfdataset.cpp -shared -o gdal_PDF.so -Iport -Igcore -Iogr -L. -lgdal -lpoppler -I/usr/include/poppler */

CPL_CVSID("$Id$");

CPL_C_START
void    GDALRegister_PDF(void);
CPL_C_END

#ifdef USE_POPPLER

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
/*                         GDALPDFOutputDev                             */
/************************************************************************/

class GDALPDFOutputDev : public SplashOutputDev
{
    private:
        int bEnableVector;
        int bEnableText;
        int bEnableBitmap;

        void skipBytes(Stream *str,
                       int width, int height,
                       int nComps, int nBits)
        {
            int nVals = width * nComps;
            int nLineSize = (nVals * nBits + 7) >> 3;
            int nBytes = nLineSize * height;
            for (int i = 0; i < nBytes; i++)
            {
                if( str->getChar() == EOF)
                    break;
            }
        }

    public:
        GDALPDFOutputDev(SplashColorMode colorModeA, int bitmapRowPadA,
                         GBool reverseVideoA, SplashColorPtr paperColorA,
                         GBool bitmapTopDownA = gTrue,
                         GBool allowAntialiasA = gTrue) :
                SplashOutputDev(colorModeA, bitmapRowPadA,
                                reverseVideoA, paperColorA,
                                bitmapTopDownA, allowAntialiasA),
                bEnableVector(TRUE),
                bEnableText(TRUE),
                bEnableBitmap(TRUE) {}

        void SetEnableVector(int bFlag) { bEnableVector = bFlag; }
        void SetEnableText(int bFlag) { bEnableText = bFlag; }
        void SetEnableBitmap(int bFlag) { bEnableBitmap = bFlag; }

        virtual void startPage(int pageNum, GfxState *state)
        {
            SplashOutputDev::startPage(pageNum, state);
            SplashBitmap* poBitmap = getBitmap();
            memset(poBitmap->getDataPtr(), 255, poBitmap->getRowSize() * poBitmap->getHeight());
        }

        virtual void stroke(GfxState * state)
        {
            if (bEnableVector)
                SplashOutputDev::stroke(state);
        }

        virtual void fill(GfxState * state)
        {
            if (bEnableVector)
                SplashOutputDev::fill(state);
        }

        virtual void eoFill(GfxState * state)
        {
            if (bEnableVector)
                SplashOutputDev::eoFill(state);
        }

        virtual void drawChar(GfxState *state, double x, double y,
                              double dx, double dy,
                              double originX, double originY,
                              CharCode code, int nBytes, Unicode *u, int uLen)
        {
            if (bEnableText)
                SplashOutputDev::drawChar(state, x, y, dx, dy,
                                          originX, originY,
                                          code, nBytes, u, uLen);
        }

        virtual void beginTextObject(GfxState *state)
        {
            if (bEnableText)
                SplashOutputDev::beginTextObject(state);
        }

        virtual GBool deviceHasTextClip(GfxState *state)
        {
            if (bEnableText)
                return SplashOutputDev::deviceHasTextClip(state);
            return gFalse;
        }

        virtual void endTextObject(GfxState *state)
        {
            if (bEnableText)
                SplashOutputDev::endTextObject(state);
        }

        virtual void drawImageMask(GfxState *state, Object *ref, Stream *str,
                                   int width, int height, GBool invert,
                                   GBool interpolate, GBool inlineImg)
        {
            if (bEnableBitmap)
                SplashOutputDev::drawImageMask(state, ref, str,
                                               width, height, invert,
                                               interpolate, inlineImg);
            else
            {
                str->reset();
                if (inlineImg)
                {
                    skipBytes(str, width, height, 1, 1);
                }
                str->close();
            }
        }

#ifdef POPPLER_0_20_OR_LATER
        virtual void setSoftMaskFromImageMask(GfxState *state,
                            Object *ref, Stream *str,
                            int width, int height, GBool invert,
                            GBool inlineImg)
        {
            if (bEnableBitmap)
                SplashOutputDev::setSoftMaskFromImageMask(state, ref, str,
                                               width, height, invert,
                                               inlineImg);
            else
                str->close();
        }

        virtual void unsetSoftMaskFromImageMask(GfxState *state)
        {
            if (bEnableBitmap)
                SplashOutputDev::unsetSoftMaskFromImageMask(state);
        }
#endif

        virtual void drawImage(GfxState *state, Object *ref, Stream *str,
                               int width, int height, GfxImageColorMap *colorMap,
                               GBool interpolate, int *maskColors, GBool inlineImg)
        {
            if (bEnableBitmap)
                SplashOutputDev::drawImage(state, ref, str,
                                           width, height, colorMap,
                                           interpolate, maskColors, inlineImg);
            else
            {
                str->reset();
                if (inlineImg)
                {
                    skipBytes(str, width, height,
                              colorMap->getNumPixelComps(),
                              colorMap->getBits());
                }
                str->close();
            }
        }

        virtual void drawMaskedImage(GfxState *state, Object *ref, Stream *str,
                                     int width, int height,
                                     GfxImageColorMap *colorMap,
                                     GBool interpolate,
                                     Stream *maskStr, int maskWidth, int maskHeight,
                                     GBool maskInvert, GBool maskInterpolate)
        {
            if (bEnableBitmap)
                SplashOutputDev::drawMaskedImage(state, ref, str,
                                                 width, height, colorMap,
                                                 interpolate,
                                                 maskStr, maskWidth, maskHeight,
                                                 maskInvert, maskInterpolate);
            else
                str->close();
        }

        virtual void drawSoftMaskedImage(GfxState *state, Object *ref, Stream *str,
                                         int width, int height,
                                         GfxImageColorMap *colorMap,
                                         GBool interpolate,
                                         Stream *maskStr,
                                         int maskWidth, int maskHeight,
                                         GfxImageColorMap *maskColorMap,
                                         GBool maskInterpolate)
        {
            if (bEnableBitmap)
                SplashOutputDev::drawSoftMaskedImage(state, ref, str,
                                                     width, height, colorMap,
                                                     interpolate,
                                                     maskStr, maskWidth, maskHeight,
                                                     maskColorMap, maskInterpolate);
            else
                str->close();
        }
};

#endif

/************************************************************************/
/*                         Dump routines                                */
/************************************************************************/

static void DumpObject(FILE* f, GDALPDFObject* poObj, int nDepth = 0, int nDepthLimit = -1);
static void DumpDict(FILE* f, GDALPDFDictionary* poDict, int nDepth = 0, int nDepthLimit = -1);
static void DumpArray(FILE* f, GDALPDFArray* poArray, int nDepth = 0, int nDepthLimit = -1);
static void DumpObjectSimplified(FILE* f, GDALPDFObject* poObj);

static void DumpArray(FILE* f, GDALPDFArray* poArray, int nDepth, int nDepthLimit)
{
    if (nDepthLimit >= 0 && nDepth > nDepthLimit)
        return;

    int nLength = poArray->GetLength();
    int i;
    CPLString osIndent;
    for(i=0;i<nDepth;i++)
        osIndent += " ";
    for(i=0;i<nLength;i++)
    {
        fprintf(f, "%sItem[%d]:", osIndent.c_str(), i);
        GDALPDFObject* poObj = NULL;
        if ((poObj = poArray->Get(i)) != NULL)
        {
            if (poObj->GetType() == PDFObjectType_String ||
                poObj->GetType() == PDFObjectType_Int ||
                poObj->GetType() == PDFObjectType_Real ||
                poObj->GetType() == PDFObjectType_Name)
            {
                fprintf(f, " ");
                DumpObjectSimplified(f, poObj);
                fprintf(f, "\n");
            }
            else
            {
                fprintf(f, "\n");
                DumpObject(f, poObj, nDepth+1, nDepthLimit);
            }
        }
    }
}

static void DumpObjectSimplified(FILE* f, GDALPDFObject* poObj)
{
    switch(poObj->GetType())
    {
        case PDFObjectType_String:
            fprintf(f, "%s (string)", poObj->GetString().c_str());
            break;

        case PDFObjectType_Int:
            fprintf(f, "%d (int)", poObj->GetInt());
            break;

        case PDFObjectType_Real:
            fprintf(f, "%f (real)", poObj->GetReal());
            break;

        case PDFObjectType_Name:
            fprintf(f, "%s (name)", poObj->GetName().c_str());
            break;

        default:
            break;
    }
}

static void DumpObject(FILE* f, GDALPDFObject* poObj, int nDepth, int nDepthLimit)
{
    if (nDepthLimit >= 0 && nDepth > nDepthLimit)
        return;

    int i;
    CPLString osIndent;
    for(i=0;i<nDepth;i++)
        osIndent += " ";
    fprintf(f, "%sType = %s\n", osIndent.c_str(), poObj->GetTypeName());
    switch(poObj->GetType())
    {
        case PDFObjectType_Array:
            DumpArray(f, poObj->GetArray(), nDepth+1, nDepthLimit);
            break;

        case PDFObjectType_Dictionary:
            DumpDict(f, poObj->GetDictionary(), nDepth+1, nDepthLimit);
            break;

        case PDFObjectType_String:
        case PDFObjectType_Int:
        case PDFObjectType_Real:
        case PDFObjectType_Name:
            fprintf(f, "%s", osIndent.c_str());
            DumpObjectSimplified(f, poObj);
            fprintf(f, "\n");
            break;

        default:
            break;
    }
}

static void DumpDict(FILE* f, GDALPDFDictionary* poDict, int nDepth, int nDepthLimit)
{
    if (nDepthLimit >= 0 && nDepth > nDepthLimit)
        return;

    std::map<CPLString, GDALPDFObject*>& oMap = poDict->GetValues();
    std::map<CPLString, GDALPDFObject*>::iterator oIter = oMap.begin();
    std::map<CPLString, GDALPDFObject*>::iterator oEnd = oMap.end();
    int i;
    CPLString osIndent;
    for(i=0;i<nDepth;i++)
        osIndent += " ";
    for(i=0;oIter != oEnd;++oIter, i++)
    {
        const char* pszKey = oIter->first.c_str();
        fprintf(f, "%sItem[%d] : %s", osIndent.c_str(), i, pszKey);
        if (strcmp(pszKey, "Parent") == 0)
        {
            fprintf(f, "\n");
            continue;
        }
        GDALPDFObject* poObj = oIter->second;
        if (poObj != NULL)
        {
            if (poObj->GetType() == PDFObjectType_String ||
                poObj->GetType() == PDFObjectType_Int ||
                poObj->GetType() == PDFObjectType_Real ||
                poObj->GetType() == PDFObjectType_Name)
            {
                fprintf(f, " = ");
                DumpObjectSimplified(f, poObj);
                fprintf(f, "\n");
            }
            else
            {
                fprintf(f, "\n");
                DumpObject(f, poObj, nDepth+1, nDepthLimit);
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
    CPLString    osFilename;
    CPLString    osUserPwd;
    char        *pszWKT;
    double       dfDPI;
    double       adfCTM[6];
    double       adfGeoTransform[6];
    int          bGeoTransformValid;
    int          nGCPCount;
    GDAL_GCP    *pasGCPList;

#ifdef USE_POPPLER
    PDFDoc*      poDoc;
#endif
    int          iPage;

    double       dfMaxArea;
    int          ParseLGIDictObject(GDALPDFObject* poLGIDict);
    int          ParseLGIDictDictFirstPass(GDALPDFDictionary* poLGIDict, int* pbIsLargestArea = NULL);
    int          ParseLGIDictDictSecondPass(GDALPDFDictionary* poLGIDict);
    int          ParseProjDict(GDALPDFDictionary* poProjDict);
    int          ParseVP(GDALPDFObject* poVP, double dfMediaBoxWidth, double dfMediaBoxHeight);

    int          bTried;
    GByte       *pabyData;

    OGRPolygon*  poNeatLine;

  public:
                 PDFDataset();
    virtual     ~PDFDataset();

    virtual const char* GetProjectionRef();
    virtual CPLErr GetGeoTransform( double * );

    virtual int    GetGCPCount();
    virtual const char *GetGCPProjection();
    virtual const GDAL_GCP *GetGCPs();

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
        poGDS->pabyData = (GByte*)VSIMalloc3(poGDS->nBands, nRasterXSize, nRasterYSize);
        if (poGDS->pabyData == NULL)
            return CE_Failure;

        const char* pszRenderingOptions = CPLGetConfigOption("GDAL_PDF_RENDERING_OPTIONS", NULL);

#ifdef USE_POPPLER

        SplashColor sColor;
        sColor[0] = 255;
        sColor[1] = 255;
        sColor[2] = 255;
        GDALPDFOutputDev *poSplashOut;
        poSplashOut = new GDALPDFOutputDev((poGDS->nBands == 3) ? splashModeRGB8 : splashModeXBGR8,
                                           4, gFalse,
                                           (poGDS->nBands == 3) ? sColor : NULL);

        if (pszRenderingOptions != NULL)
        {
            poSplashOut->SetEnableVector(FALSE);
            poSplashOut->SetEnableText(FALSE);
            poSplashOut->SetEnableBitmap(FALSE);

            char** papszTokens = CSLTokenizeString2( pszRenderingOptions, " ,", 0 );
            for(int i=0;papszTokens[i] != NULL;i++)
            {
                if (EQUAL(papszTokens[i], "VECTOR"))
                    poSplashOut->SetEnableVector(TRUE);
                else if (EQUAL(papszTokens[i], "TEXT"))
                    poSplashOut->SetEnableText(TRUE);
                else if (EQUAL(papszTokens[i], "BITMAP"))
                    poSplashOut->SetEnableBitmap(TRUE);
                else
                {
                    CPLError(CE_Warning, CPLE_NotSupported,
                             "Value %s is not a valid value for GDAL_PDF_RENDERING_OPTIONS",
                             papszTokens[i]);
                }
            }
            CSLDestroy(papszTokens);
        }

        PDFDoc* poDoc = poGDS->poDoc;
#ifdef POPPLER_0_20_OR_LATER
        poSplashOut->startDoc(poDoc);
#else
        poSplashOut->startDoc(poDoc->getXRef());
#endif
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

        GByte* pabyDataR = poGDS->pabyData;
        GByte* pabyDataG = poGDS->pabyData + nRasterXSize * nRasterYSize;
        GByte* pabyDataB = poGDS->pabyData + 2 * nRasterXSize * nRasterYSize;
        GByte* pabyDataA = poGDS->pabyData + 3 * nRasterXSize * nRasterYSize;
        GByte* pabySrc   = poBitmap->getDataPtr();
        GByte* pabyAlphaSrc  = (GByte*)poBitmap->getAlphaPtr();
        int i, j;
        for(j=0;j<nRasterYSize;j++)
        {
            for(i=0;i<nRasterXSize;i++)
            {
                if (poGDS->nBands == 3)
                {
                    pabyDataR[i] = pabySrc[i * 3 + 0];
                    pabyDataG[i] = pabySrc[i * 3 + 1];
                    pabyDataB[i] = pabySrc[i * 3 + 2];
                }
                else
                {
                    pabyDataR[i] = pabySrc[i * 4 + 2];
                    pabyDataG[i] = pabySrc[i * 4 + 1];
                    pabyDataB[i] = pabySrc[i * 4 + 0];
                    pabyDataA[i] = pabyAlphaSrc[i];
                }
            }
            pabyDataR += nRasterXSize;
            pabyDataG += nRasterXSize;
            pabyDataB += nRasterXSize;
            pabyDataA += nRasterXSize;
            pabyAlphaSrc += poBitmap->getAlphaRowSize();
            pabySrc += poBitmap->getRowSize();
        }
        delete poSplashOut;

#else
        CPLAssert(poGDS->nBands == 3);

        if (pszRenderingOptions != NULL)
        {
            CPLError(CE_Warning, CPLE_NotSupported,
                     "GDAL_PDF_RENDERING_OPTIONS only supported "
                     "when PDF driver is compiled against Poppler.");
        }

        memset(poGDS->pabyData, 0, ((size_t)3) * nRasterXSize * nRasterYSize);

        CPLString osTmpFilenamePrefix = CPLGenerateTempFilename("pdf");
        CPLString osTmpFilename(CPLSPrintf("%s-%d.ppm",
                                           osTmpFilenamePrefix.c_str(),
                                           poGDS->iPage));

        CPLString osCmd = CPLSPrintf("pdftoppm -r %f -f %d -l %d \"%s\" \"%s\"",
                   poGDS->dfDPI, poGDS->iPage, poGDS->iPage,
                   poGDS->osFilename.c_str(), osTmpFilenamePrefix.c_str());
        if (poGDS->osUserPwd.size() != 0)
        {
            osCmd += " -upw \"";
            osCmd += poGDS->osUserPwd;
            osCmd += "\"";
        }

        CPLDebug("PDF", "Running '%s'", osCmd.c_str());
        int nRet = system(osCmd.c_str());
        if (nRet == 0)
        {
            GDALDataset* poDS = (GDALDataset*) GDALOpen(osTmpFilename, GA_ReadOnly);
            if (poDS)
            {
                if (poDS->GetRasterCount() == 3)
                {
                    poDS->RasterIO(GF_Read, 0, 0,
                                   poDS->GetRasterXSize(),
                                   poDS->GetRasterYSize(),
                                   poGDS->pabyData, nRasterXSize, nRasterYSize,
                                   GDT_Byte, 3, NULL, 0, 0, 0);
                }
                delete poDS;
            }
        }
        else
        {
            CPLDebug("PDF", "Ret code = %d", nRet);
        }
        VSIUnlink(osTmpFilename);
#endif
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
#ifdef USE_POPPLER
    poDoc = NULL;
#endif
    pszWKT = NULL;
    dfMaxArea = 0;
    adfGeoTransform[0] = 0;
    adfGeoTransform[1] = 1;
    adfGeoTransform[2] = 0;
    adfGeoTransform[3] = 0;
    adfGeoTransform[4] = 0;
    adfGeoTransform[5] = 1;
    bGeoTransformValid = FALSE;
    nGCPCount = 0;
    pasGCPList = NULL;
    bTried = FALSE;
    pabyData = NULL;
    iPage = -1;
    poNeatLine = NULL;
}

/************************************************************************/
/*                           PDFFreeDoc()                               */
/************************************************************************/

#ifdef USE_POPPLER
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
#endif

/************************************************************************/
/*                            ~PDFDataset()                            */
/************************************************************************/

PDFDataset::~PDFDataset()
{
    CPLFree(pszWKT);
    CPLFree(pabyData);

    delete poNeatLine;

    if( nGCPCount > 0 )
    {
        GDALDeinitGCPs( nGCPCount, pasGCPList );
        CPLFree( pasGCPList );
        pasGCPList = NULL;
        nGCPCount = 0;
    }

#ifdef USE_POPPLER
    PDFFreeDoc(poDoc);
#endif
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

#ifdef USE_POPPLER
#ifdef POPPLER_0_20_OR_LATER
static void PDFDatasetErrorFunction(void* userData, ErrorCategory eErrCatagory, int nPos, char *pszMsg)
{
    CPLString osError;

    if (nPos >= 0)
        osError.Printf("Pos = %d, ", nPos);
    osError += pszMsg;

    if (strcmp(osError.c_str(), "Incorrect password") == 0)
        return;

    CPLError(CE_Failure, CPLE_AppDefined, "%s", osError.c_str());
}
#else
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
#endif
#endif

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

#ifdef USE_POPPLER
    GooString* poUserPwd = NULL;

    /* Set custom error handler for poppler errors */
#ifdef POPPLER_0_20_OR_LATER
    setErrorCallback(PDFDatasetErrorFunction, NULL);
#else
    setErrorFunction(PDFDatasetErrorFunction);
#endif

    /* poppler global variable */
    if (globalParams == NULL)
        globalParams = new GlobalParams();

    globalParams->setPrintCommands(CSLTestBoolean(
        CPLGetConfigOption("GDAL_PDF_PRINT_COMMANDS", "FALSE")));

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

    GDALPDFObjectPoppler oPageObjPoppler(&oPageObj, FALSE);
    GDALPDFObject* poPageObj = &oPageObjPoppler;
#else
    PoDoFo::PdfError::EnableDebug( false );
    PoDoFo::PdfError::EnableLogging( false );

    PoDoFo::PdfMemDocument* poDoc = new PoDoFo::PdfMemDocument();
    try
    {
        poDoc->Load(pszFilename);
    }
    catch(PoDoFo::PdfError& oError)
    {
        if (oError.GetError() == PoDoFo::ePdfError_InvalidPassword)
        {
            if (pszUserPwd)
            {
                if (EQUAL(pszUserPwd, "ASK_INTERACTIVE"))
                {
                    printf( "Enter password (will be echo'ed in the console): " );
                    fgets( szPassword, sizeof(szPassword), stdin );
                    szPassword[sizeof(szPassword)-1] = 0;
                    char* sz10 = strchr(szPassword, '\n');
                    if (sz10)
                        *sz10 = 0;
                    pszUserPwd = szPassword;
                }

                try
                {
                    poDoc->SetPassword(pszUserPwd);
                }
                catch(PoDoFo::PdfError& oError)
                {
                    if (oError.GetError() == PoDoFo::ePdfError_InvalidPassword)
                    {
                        CPLError(CE_Failure, CPLE_AppDefined, "Invalid password");
                    }
                    else
                    {
                        CPLError(CE_Failure, CPLE_AppDefined, "Invalid PDF : %s", oError.what());
                    }
                    delete poDoc;
                    return NULL;
                }
                catch(...)
                {
                    CPLError(CE_Failure, CPLE_AppDefined, "Invalid PDF");
                    delete poDoc;
                    return NULL;
                }
            }
            else
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                            "A password is needed. You can specify it through the PDF_USER_PWD "
                            "configuration option (that can be set to ASK_INTERACTIVE)");
                delete poDoc;
                return NULL;
            }
        }
        else
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Invalid PDF : %s", oError.what());
            delete poDoc;
            return NULL;
        }
    }
    catch(...)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Invalid PDF");
        delete poDoc;
        return NULL;
    }

    int nPages = poDoc->GetPageCount();
    if (iPage < 1 || iPage > nPages)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Invalid page number (%d/%d)",
                 iPage, nPages);
        delete poDoc;
        return NULL;
    }

    PoDoFo::PdfPage* poPage = poDoc->GetPage(iPage - 1);
    if ( poPage == NULL )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Invalid PDF : invalid page");
        delete poDoc;
        return NULL;
    }

    PoDoFo::PdfObject* pObj = poPage->GetObject();
    GDALPDFObjectPodofo oPageObjPodofo(pObj, poDoc->GetObjects());
    GDALPDFObject* poPageObj = &oPageObjPodofo;
#endif

    GDALPDFDictionary* poPageDict = poPageObj->GetDictionary();
    if ( poPageDict == NULL )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Invalid PDF : poPageDict == NULL");
#ifdef USE_POPPLER
        PDFFreeDoc(poDoc);
#else
        delete poDoc;
#endif
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
        DumpObject(f, poPageObj, 0 ,20);
        if (f != stderr)
            fclose(f);
    }

    PDFDataset* poDS = new PDFDataset();
    poDS->osFilename = pszFilename;

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

#ifdef USE_POPPLER
    poDS->poDoc = poDoc;
#endif
    poDS->osUserPwd = pszUserPwd ? pszUserPwd : "";
    poDS->iPage = iPage;
    poDS->dfDPI = atof(CPLGetConfigOption("GDAL_PDF_DPI", "150"));
    if (poDS->dfDPI < 1 || poDS->dfDPI > 7200)
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "Invalid value for GDAL_PDF_DPI. Using default value instead");
        poDS->dfDPI = 150;
    }

#ifdef USE_POPPLER
    PDFRectangle* psMediaBox = poPage->getMediaBox();
    double dfX1 = psMediaBox->x1;
    double dfY1 = psMediaBox->y1;
    double dfX2 = psMediaBox->x2;
    double dfY2 = psMediaBox->y2;
#else
    PoDoFo::PdfRect oMediaBox = poPage->GetMediaBox();
    double dfX1 = oMediaBox.GetLeft();
    double dfY1 = oMediaBox.GetBottom();
    double dfX2 = dfX1 + oMediaBox.GetWidth();
    double dfY2 = dfY1 + oMediaBox.GetHeight();
#endif

    double dfPixelPerPt = poDS->dfDPI / 72;
    poDS->nRasterXSize = (int) ((dfX2 - dfX1) * dfPixelPerPt);
    poDS->nRasterYSize = (int) ((dfY2 - dfY1) * dfPixelPerPt);

#ifdef USE_POPPLER
    double dfRotation = poDoc->getPageRotate(iPage);
#else
    double dfRotation = poPage->GetRotation();
#endif
    if ( dfRotation == 90 ||
         dfRotation == 270 )
    {
/* FIXME: the non poppler case should be implemented. This needs to rotate */
/* the output of pdftoppm */
#ifdef USE_POPPLER
        /* Wondering how it would work with a georeferenced image */
        /* Has only been tested with ungeoreferenced image */
        int nTmp = poDS->nRasterXSize;
        poDS->nRasterXSize = poDS->nRasterYSize;
        poDS->nRasterYSize = nTmp;
#endif
    }

    GDALPDFObject* poLGIDict = NULL;
    GDALPDFObject* poVP = NULL;
    int bIsOGCBP = FALSE;
    if ( (poLGIDict = poPageDict->Get("LGIDict")) != NULL )
    {
        /* Cf 08-139r3_GeoPDF_Encoding_Best_Practice_Version_2.2.pdf */
        CPLDebug("PDF", "OGC Encoding Best Practice style detected");
        if (poDS->ParseLGIDictObject(poLGIDict))
        {
            poDS->adfGeoTransform[0] = poDS->adfCTM[4] + poDS->adfCTM[0] * dfX1 + poDS->adfCTM[2] * dfY2;
            poDS->adfGeoTransform[1] = poDS->adfCTM[0] / dfPixelPerPt;
            poDS->adfGeoTransform[2] = poDS->adfCTM[1] / dfPixelPerPt;
            poDS->adfGeoTransform[3] = poDS->adfCTM[5] + poDS->adfCTM[1] * dfX1 + poDS->adfCTM[3] * dfY2;
            poDS->adfGeoTransform[4] = - poDS->adfCTM[2] / dfPixelPerPt;
            poDS->adfGeoTransform[5] = - poDS->adfCTM[3] / dfPixelPerPt;
            poDS->bGeoTransformValid = TRUE;
            bIsOGCBP = TRUE;

            int i;
            for(i=0;i<poDS->nGCPCount;i++)
            {
                poDS->pasGCPList[i].dfGCPPixel = poDS->pasGCPList[i].dfGCPPixel * dfPixelPerPt;
                poDS->pasGCPList[i].dfGCPLine = poDS->nRasterYSize - poDS->pasGCPList[i].dfGCPLine * dfPixelPerPt;
            }
        }
    }
    else if ( (poVP = poPageDict->Get("VP")) != NULL )
    {
        /* Cf adobe_supplement_iso32000.pdf */
        CPLDebug("PDF", "Adobe ISO32000 style Geospatial PDF perhaps ?");
        if (dfX1 != 0 || dfY1 != 0)
        {
            CPLDebug("PDF", "non null dfX1 or dfY1 values. untested case...");
        }
        poDS->ParseVP(poVP, dfX2 - dfX1, dfY2 - dfY1);
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


#ifdef USE_POPPLER
    int nXRefSize = poDoc->getXRef()->getNumObjects();
    for(int i=0;i<nXRefSize;i++)
    {
        Object o;
        poDoc->getXRef()->fetch(i, 0, &o);
        if (o.isStream())
        {
            Dict* dict = o.getStream()->getDict();
            if (dict)
            {
                Object oSubType, oType;
                dict->lookup((char*)"Subtype", &oSubType);
                dict->lookup((char*)"Type", &oType);
                if (oType.getType() == objName &&
                    strcmp(oType.getName(), "Metadata") == 0 &&
                    oSubType.getType() == objName &&
                    strcmp(oSubType.getName(), "XML") == 0)
                {
                    int nBufferAlloc = 4096;
                    char* pszXMP = (char*) CPLMalloc(nBufferAlloc);
                    Stream* poStream = o.getStream();
                    poStream->reset();
                    int j;
                    for(j=0; ; j++)
                    {
                        const int ch = poStream->getChar();
                        if (ch == EOF)
                            break;
                        if (j == nBufferAlloc - 2)
                        {
                            nBufferAlloc *= 2;
                            pszXMP = (char*) CPLRealloc(pszXMP, nBufferAlloc);
                        }
                        pszXMP[j] = (char)ch;
                    }
                    pszXMP[j] = '\0';
                    if (strncmp(pszXMP, "<?xpacket begin=", strlen("<?xpacket begin=")) == 0)
                    {
                        char *apszMDList[2];
                        apszMDList[0] = pszXMP;
                        apszMDList[1] = NULL;
                        poDS->SetMetadata(apszMDList, "xml:XMP");
                    }
                    CPLFree(pszXMP);
                }
                oSubType.free();
                oType.free();
            }
        }
        o.free();
    }
#else
    PoDoFo::TIVecObjects it = poDoc->GetObjects().begin();
    for( ; it != poDoc->GetObjects().end(); ++it )
    {
        if( (*it)->HasStream() && (*it)->GetDataType() == PoDoFo::ePdfDataType_Dictionary)
        {
            PoDoFo::PdfDictionary& dict = (*it)->GetDictionary();
            const PoDoFo::PdfObject* poType = dict.GetKey(PoDoFo::PdfName("Type"));
            const PoDoFo::PdfObject* poSubType = dict.GetKey(PoDoFo::PdfName("Subtype"));
            if (poType == NULL ||
                poType->GetDataType() != PoDoFo::ePdfDataType_Name ||
                poType->GetName().GetName().compare("Metadata") != 0 ||
                poSubType == NULL || 
                poSubType->GetDataType() != PoDoFo::ePdfDataType_Name ||
                poSubType->GetName().GetName().compare("XML") != 0)
                continue;

            try
            {
                PoDoFo::PdfMemStream* pStream = dynamic_cast<PoDoFo::PdfMemStream*>((*it)->GetStream());
                pStream->Uncompress();

                const char* pszContent = pStream->Get();
                int nLength = (int)pStream->GetLength();
                if (pszContent != NULL && nLength > 15 &&
                    strncmp(pszContent, "<?xpacket begin=", strlen("<?xpacket begin=")) == 0)
                {
                    char *apszMDList[2];
                    apszMDList[0] = (char*) CPLMalloc(nLength + 1);
                    memcpy(apszMDList[0], pszContent, nLength);
                    apszMDList[0][nLength] = 0;
                    apszMDList[1] = NULL;
                    poDS->SetMetadata(apszMDList, "xml:XMP");
                    CPLFree(apszMDList[0]);

                    break;
                }
            }
            catch( const PoDoFo::PdfError & e )
            {
                e.PrintErrorMsg();
            }
        }
    }
#endif


#ifndef USE_POPPLER
    delete poDoc;
#endif

    int nBands = atoi(CPLGetConfigOption("GDAL_PDF_BANDS", "3"));
    if (nBands != 3 && nBands != 4)
    {
        CPLError(CE_Warning, CPLE_NotSupported,
                 "Invalid value for GDAL_PDF_BANDS. Using 3 as a fallback");
        nBands = 3;
    }
#ifndef USE_POPPLER
    if (nBands == 4)
    {
        CPLError(CE_Warning, CPLE_NotSupported,
                 "GDAL_PDF_BANDS=4 only supported when PDF driver is compiled against Poppler. "
                 "Using 3 as a fallback");
        nBands = 3;
    }
#endif

    int iBand;
    for(iBand = 1; iBand <= nBands; iBand ++)
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

int PDFDataset::ParseLGIDictObject(GDALPDFObject* poLGIDict)
{
    int i;
    int bOK = FALSE;
    if (poLGIDict->GetType() == PDFObjectType_Array)
    {
        GDALPDFArray* poArray = poLGIDict->GetArray();
        int nArrayLength = poArray->GetLength();
        int iMax = -1;
        GDALPDFObject* poArrayElt;
        for (i=0; i<nArrayLength; i++)
        {
            if ( (poArrayElt = poArray->Get(i)) == NULL ||
                 poArrayElt->GetType() != PDFObjectType_Dictionary )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "LGIDict[%d] is not a dictionary", i);
                return FALSE;
            }

            int bIsLargestArea = FALSE;
            if (ParseLGIDictDictFirstPass(poArrayElt->GetDictionary(), &bIsLargestArea))
            {
                if (bIsLargestArea || iMax < 0)
                    iMax = i;
            }
        }

        if (iMax < 0)
            return FALSE;

        poArrayElt = poArray->Get(iMax);
        bOK = ParseLGIDictDictSecondPass(poArrayElt->GetDictionary());
    }
    else if (poLGIDict->GetType() == PDFObjectType_Dictionary)
    {
        bOK = ParseLGIDictDictFirstPass(poLGIDict->GetDictionary()) &&
              ParseLGIDictDictSecondPass(poLGIDict->GetDictionary());
    }
    else
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "LGIDict is of type %s", poLGIDict->GetTypeName());
    }

    return bOK;
}

/************************************************************************/
/*                            Get()                                */
/************************************************************************/

static double Get(GDALPDFObject* poObj, int nIndice = -1)
{
    if (poObj->GetType() == PDFObjectType_Array && nIndice >= 0)
    {
        poObj = poObj->GetArray()->Get(nIndice);
        if (poObj == NULL)
            return 0;
        return Get(poObj);
    }
    else if (poObj->GetType() == PDFObjectType_Int)
        return poObj->GetInt();
    else if (poObj->GetType() == PDFObjectType_Real)
        return poObj->GetReal();
    else if (poObj->GetType() == PDFObjectType_String)
    {
        const char* pszStr = poObj->GetString().c_str();
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
                 poObj->GetTypeName());
        return 0;
    }
}

/************************************************************************/
/*                            Get()                                */
/************************************************************************/

static double Get(GDALPDFDictionary* poDict, const char* pszName)
{
    GDALPDFObject* poObj;
    if ( (poObj = poDict->Get(pszName)) != NULL )
        return Get(poObj);
    CPLError(CE_Failure, CPLE_AppDefined,
             "Cannot find parameter %s", pszName);
    return 0;
}

/************************************************************************/
/*                   ParseLGIDictDictFirstPass()                        */
/************************************************************************/

int PDFDataset::ParseLGIDictDictFirstPass(GDALPDFDictionary* poLGIDict,
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
    GDALPDFObject* poType;
    if ((poType = poLGIDict->Get("Type")) == NULL)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot find Type of LGIDict object");
        return FALSE;
    }

    if ( poType->GetType() != PDFObjectType_Name )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Invalid type for Type of LGIDict object");
        return FALSE;
    }

    if ( strcmp(poType->GetName().c_str(), "LGIDict") != 0 )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Invalid value for Type of LGIDict object : %s",
                 poType->GetName().c_str());
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Extract Version attribute                                       */
/* -------------------------------------------------------------------- */
    GDALPDFObject* poVersion;
    if ((poVersion = poLGIDict->Get("Version")) == NULL)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot find Version of LGIDict object");
        return FALSE;
    }

    if ( poVersion->GetType() == PDFObjectType_String )
    {
        /* OGC best practice is 2.1 */
        CPLDebug("PDF", "LGIDict Version : %s",
                 poVersion->GetString().c_str());
    }
    else if (poVersion->GetType() == PDFObjectType_Int)
    {
        /* Old TerraGo is 2 */
        CPLDebug("PDF", "LGIDict Version : %d",
                 poVersion->GetInt());
    }

/* -------------------------------------------------------------------- */
/*      Extract Neatline attribute                                      */
/* -------------------------------------------------------------------- */
    GDALPDFObject* poNeatline;
    if ((poNeatline = poLGIDict->Get("Neatline")) != NULL &&
        poNeatline->GetType() == PDFObjectType_Array)
    {
        int nLength = poNeatline->GetArray()->GetLength();
        if ( (nLength % 2) != 0 || nLength < 4 )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Invalid length for Neatline");
            return FALSE;
        }

        double dfMinX = 0, dfMinY = 0, dfMaxX = 0, dfMaxY = 0;
        for(i=0;i<nLength;i+=2)
        {
            double dfX = Get(poNeatline, i);
            double dfY = Get(poNeatline, i + 1);
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
        if (nLength == 4)
        {
            /* 2 points only ? They are the bounding box */
            double dfX1 = Get(poNeatline, 0);
            double dfY1 = Get(poNeatline, 1);
            double dfX2 = Get(poNeatline, 2);
            double dfY2 = Get(poNeatline, 3);
            poRing->addPoint(dfX1, dfY1);
            poRing->addPoint(dfX2, dfY1);
            poRing->addPoint(dfX2, dfY2);
            poRing->addPoint(dfX1, dfY2);
        }
        else
        {
            for(i=0;i<nLength;i+=2)
            {
                double dfX = Get(poNeatline, i);
                double dfY = Get(poNeatline, i + 1);
                poRing->addPoint(dfX, dfY);
            }
        }
        poNeatLine->addRingDirectly(poRing);
    }

    return TRUE;
}

/************************************************************************/
/*                  ParseLGIDictDictSecondPass()                        */
/************************************************************************/

int PDFDataset::ParseLGIDictDictSecondPass(GDALPDFDictionary* poLGIDict)
{
    int i;

/* -------------------------------------------------------------------- */
/*      Extract Description attribute                                   */
/* -------------------------------------------------------------------- */
    GDALPDFObject* poDescription;
    if ( (poDescription = poLGIDict->Get("Description")) != NULL &&
         poDescription->GetType() == PDFObjectType_String )
    {
        CPLDebug("PDF", "Description = %s", poDescription->GetString().c_str());
    }

/* -------------------------------------------------------------------- */
/*      Extract CTM attribute                                           */
/* -------------------------------------------------------------------- */
    GDALPDFObject* poCTM;
    int bHasCTM = FALSE;
    if ((poCTM = poLGIDict->Get("CTM")) != NULL &&
        poCTM->GetType() == PDFObjectType_Array)
    {
        int nLength = poCTM->GetArray()->GetLength();
        if ( nLength != 6 )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Invalid length for CTM");
            return FALSE;
        }

        bHasCTM = TRUE;
        for(i=0;i<nLength;i++)
        {
            adfCTM[i] = Get(poCTM, i);
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
    GDALPDFObject* poRegistration;
    if ((poRegistration = poLGIDict->Get("Registration")) != NULL &&
        poRegistration->GetType() == PDFObjectType_Array)
    {
        GDALPDFArray* poRegistrationArray = poRegistration->GetArray();
        int nLength = poRegistrationArray->GetLength();
        if (nLength > 4 || (!bHasCTM && nLength >= 2) )
        {
            nGCPCount = 0;
            pasGCPList = (GDAL_GCP *) CPLCalloc(sizeof(GDAL_GCP),nLength);

            for(i=0;i<nLength;i++)
            {
                GDALPDFObject* poGCP = poRegistrationArray->Get(i);
                if ( poGCP != NULL &&
                    poGCP->GetType() == PDFObjectType_Array &&
                    poGCP->GetArray()->GetLength() == 4 )
                {
                    double dfUserX = Get(poGCP, 0);
                    double dfUserY = Get(poGCP, 1);
                    double dfX = Get(poGCP, 2);
                    double dfY = Get(poGCP, 3);
                    CPLDebug("PDF", "GCP[%d].userX = %.16g", i, dfUserX);
                    CPLDebug("PDF", "GCP[%d].userY = %.16g", i, dfUserY);
                    CPLDebug("PDF", "GCP[%d].x = %.16g", i, dfX);
                    CPLDebug("PDF", "GCP[%d].y = %.16g", i, dfY);

                    char    szID[32];
                    sprintf( szID, "%d", nGCPCount+1 );
                    pasGCPList[nGCPCount].pszId = CPLStrdup( szID );
                    pasGCPList[nGCPCount].pszInfo = CPLStrdup("");
                    pasGCPList[nGCPCount].dfGCPPixel = dfUserX;
                    pasGCPList[nGCPCount].dfGCPLine = dfUserY;
                    pasGCPList[nGCPCount].dfGCPX = dfX;
                    pasGCPList[nGCPCount].dfGCPY = dfY;
                    nGCPCount ++;
                }
            }
        }
    }

    if (!bHasCTM && nGCPCount == 0)
    {
        CPLDebug("PDF", "Neither CTM nor Registration found");
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Extract Projection attribute                                    */
/* -------------------------------------------------------------------- */
    GDALPDFObject* poProjection;
    if ((poProjection = poLGIDict->Get("Projection")) == NULL ||
        poProjection->GetType() != PDFObjectType_Dictionary)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Could not find Projection");
        return FALSE;
    }

    return ParseProjDict(poProjection->GetDictionary());
}

/************************************************************************/
/*                         ParseProjDict()                               */
/************************************************************************/

int PDFDataset::ParseProjDict(GDALPDFDictionary* poProjDict)
{
    if (poProjDict == NULL)
        return FALSE;
    OGRSpatialReference oSRS;

/* -------------------------------------------------------------------- */
/*      Extract WKT attribute (GDAL extension)                          */
/* -------------------------------------------------------------------- */
    GDALPDFObject* poWKT;
    if ( (poWKT = poProjDict->Get("WKT")) != NULL &&
         poWKT->GetType() == PDFObjectType_String &&
         CSLTestBoolean( CPLGetConfigOption("GDAL_PDF_OGC_BP_READ_WKT", "TRUE") ) )
    {
        CPLDebug("PDF", "Found WKT attribute (GDAL extension). Using it");
        const char* pszWKTRead = poWKT->GetString().c_str();
        CPLFree(pszWKT);
        pszWKT = CPLStrdup(pszWKTRead);
        return TRUE;
    }

/* -------------------------------------------------------------------- */
/*      Extract Type attribute                                          */
/* -------------------------------------------------------------------- */
    GDALPDFObject* poType;
    if ((poType = poProjDict->Get("Type")) == NULL)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot find Type of Projection object");
        return FALSE;
    }

    if ( poType->GetType() != PDFObjectType_Name )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Invalid type for Type of Projection object");
        return FALSE;
    }

    if ( strcmp(poType->GetName().c_str(), "Projection") != 0 )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Invalid value for Type of Projection object : %s",
                 poType->GetName().c_str());
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Extract Datum attribute                                         */
/* -------------------------------------------------------------------- */
    int bIsWGS84 = FALSE;
    int bIsNAD83 = FALSE;
    int bIsNAD27 = FALSE;

    GDALPDFObject* poDatum;
    if ((poDatum = poProjDict->Get("Datum")) != NULL)
    {
        if (poDatum->GetType() == PDFObjectType_String)
        {
            const char* pszDatum = poDatum->GetString().c_str();
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
        else if (poDatum->GetType() == PDFObjectType_Dictionary)
        {
            GDALPDFDictionary* poDatumDict = poDatum->GetDictionary();

            GDALPDFObject* poDatumDescription = poDatumDict->Get("Description");
            const char* pszDatumDescription = "unknown";
            if (poDatumDescription != NULL &&
                poDatumDescription->GetType() == PDFObjectType_String)
                pszDatumDescription  = poDatumDescription->GetString().c_str();
            CPLDebug("PDF", "Datum.Description = %s", pszDatumDescription);
                
            GDALPDFObject* poEllipsoid = poDatumDict->Get("Ellipsoid");
            if (poEllipsoid == NULL ||
                !(poEllipsoid->GetType() == PDFObjectType_String ||
                  poEllipsoid->GetType() == PDFObjectType_Dictionary))
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "Cannot find Ellipsoid in Datum. Defaulting to WGS84...");
                oSRS.SetGeogCS( "unknown",
                                pszDatumDescription,
                                "unknown",
                                6378137,298.257223563);
            }
            else if (poEllipsoid->GetType() == PDFObjectType_String)
            {
                const char* pszEllipsoid = poEllipsoid->GetString().c_str();
                CPLDebug("PDF", "Datum.Ellipsoid = %s", pszEllipsoid);
                if( EQUAL(pszEllipsoid, "WE") )
                {
                    oSRS.SetGeogCS( "unknown",
                                    pszDatumDescription,
                                    "WGS 84",
                                    6378137,298.257223563);
                }
                else
                {
                    CPLError(CE_Warning, CPLE_AppDefined,
                            "Unhandled (yet) value for Ellipsoid : %s. Defaulting to WGS84...",
                            pszEllipsoid);
                    oSRS.SetGeogCS( "unknown",
                                    pszDatumDescription,
                                    pszEllipsoid,
                                    6378137,298.257223563);
                }
            }
            else// if (poEllipsoid->GetType() == PDFObjectType_Dictionary)
            {
                GDALPDFDictionary* poEllipsoidDict = poEllipsoid->GetDictionary();

                GDALPDFObject* poEllipsoidDescription = poEllipsoidDict->Get("Description");
                const char* pszEllipsoidDescription = "unknown";
                if (poEllipsoidDescription != NULL &&
                    poEllipsoidDescription->GetType() == PDFObjectType_String)
                    pszEllipsoidDescription = poEllipsoidDescription->GetString().c_str();
                CPLDebug("PDF", "Datum.Ellipsoid.Description = %s", pszEllipsoidDescription);
                    
                double dfSemiMajor = Get(poEllipsoidDict, "SemiMajorAxis");
                CPLDebug("PDF", "Datum.Ellipsoid.SemiMajorAxis = %.16g", dfSemiMajor);
                double dfInvFlattening = -1.0;
                
                if( poEllipsoidDict->Get("InvFlattening") )
                {
                    dfInvFlattening = Get(poEllipsoidDict, "InvFlattening");
                    CPLDebug("PDF", "Datum.Ellipsoid.InvFlattening = %.16g", dfInvFlattening);
                }
                else if( poEllipsoidDict->Get("SemiMinorAxis") )
                {
                    double dfSemiMinor = Get(poEllipsoidDict, "SemiMinorAxis");
                    CPLDebug("PDF", "Datum.Ellipsoid.SemiMinorAxis = %.16g", dfSemiMinor);
                    if( ABS(dfSemiMajor/dfSemiMinor) - 1.0 < 0.0000000000001 )
                        dfInvFlattening = 0.0;
                    else
                        dfInvFlattening = -1.0 / (dfSemiMinor/dfSemiMajor - 1.0);
                }
                
                if( dfSemiMajor != 0.0 && dfInvFlattening != -1.0 )
                {
                    oSRS.SetGeogCS( "unknown",
                                    pszDatumDescription,
                                    pszEllipsoidDescription,
                                    dfSemiMajor, dfInvFlattening);
                }
                else
                {
                    CPLError(CE_Warning, CPLE_AppDefined,
                             "Invalid Ellipsoid object. Defaulting to WGS84...");
                    oSRS.SetGeogCS( "unknown",
                                    pszDatumDescription,
                                    pszEllipsoidDescription,
                                    6378137,298.257223563);
                }
                
                
            }
            
            GDALPDFObject* poTOWGS84 = poDatumDict->Get("ToWGS84");
            if( poTOWGS84 != NULL && poTOWGS84->GetType() == PDFObjectType_Dictionary )
            {
                GDALPDFDictionary* poTOWGS84Dict = poTOWGS84->GetDictionary();
                double dx = Get(poTOWGS84Dict, "dx");
                double dy = Get(poTOWGS84Dict, "dy");
                double dz = Get(poTOWGS84Dict, "dz");
                if (poTOWGS84Dict->Get("rx") && poTOWGS84Dict->Get("ry") &&
                    poTOWGS84Dict->Get("rz") && poTOWGS84Dict->Get("sf"))
                {
                    double rx = Get(poTOWGS84Dict, "rx");
                    double ry = Get(poTOWGS84Dict, "ry");
                    double rz = Get(poTOWGS84Dict, "rz");
                    double sf = Get(poTOWGS84Dict, "sf");
                    oSRS.SetTOWGS84(dx, dy, dz, rx, ry, rz, sf);
                }
                else
                {
                    oSRS.SetTOWGS84(dx, dy, dz);
                }
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Extract Hemisphere attribute                                    */
/* -------------------------------------------------------------------- */
    CPLString osHemisphere;
    GDALPDFObject* poHemisphere;
    if ((poHemisphere = poProjDict->Get("Hemisphere")) != NULL &&
        poHemisphere->GetType() == PDFObjectType_String)
    {
        osHemisphere = poHemisphere->GetString();
    }

/* -------------------------------------------------------------------- */
/*      Extract ProjectionType attribute                                */
/* -------------------------------------------------------------------- */
    GDALPDFObject* poProjectionType;
    if ((poProjectionType = poProjDict->Get("ProjectionType")) == NULL ||
        poProjectionType->GetType() != PDFObjectType_String)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot find ProjectionType of Projection object");
        return FALSE;
    }
    CPLString osProjectionType(poProjectionType->GetString());
    CPLDebug("PDF", "Projection.ProjectionType = %s", osProjectionType.c_str());

    /* Unhandled: NONE, GEODETIC */

    if (EQUAL(osProjectionType, "GEOGRAPHIC"))
    {
        /* Nothing to do */
    }

    /* Unhandled: LOCAL CARTESIAN, MG (MGRS) */

    else if (EQUAL(osProjectionType, "UT")) /* UTM */
    {
        int nZone = (int)Get(poProjDict, "Zone");
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
        int nZone = (int)Get(poProjDict, "Zone");
        oSRS.SetStatePlane( nZone, bIsNAD83 );
    }

    else if (EQUAL(osProjectionType, "AC")) /* Albers Equal Area Conic */
    {
        double dfStdP1 = Get(poProjDict, "StandardParallelOne");
        double dfStdP2 = Get(poProjDict, "StandardParallelTwo");
        double dfCenterLat = Get(poProjDict, "OriginLatitude");
        double dfCenterLong = Get(poProjDict, "CentralMeridian");
        double dfFalseEasting = Get(poProjDict, "FalseEasting");
        double dfFalseNorthing = Get(poProjDict, "FalseNorthing");
        oSRS.SetACEA( dfStdP1, dfStdP2,
                     dfCenterLat, dfCenterLong,
                     dfFalseEasting, dfFalseNorthing );
    }
 
    else if (EQUAL(osProjectionType, "AL")) /* Azimuthal Equidistant */
    {
        double dfCenterLat = Get(poProjDict, "OriginLatitude");
        double dfCenterLong = Get(poProjDict, "CentralMeridian");
        double dfFalseEasting = Get(poProjDict, "FalseEasting");
        double dfFalseNorthing = Get(poProjDict, "FalseNorthing");
        oSRS.SetAE(  dfCenterLat, dfCenterLong,
                     dfFalseEasting, dfFalseNorthing );
    }

    else if (EQUAL(osProjectionType, "BF")) /* Bonne */
    {
        double dfStdP1 = Get(poProjDict, "OriginLatitude");
        double dfCentralMeridian = Get(poProjDict, "CentralMeridian");
        double dfFalseEasting = Get(poProjDict, "FalseEasting");
        double dfFalseNorthing = Get(poProjDict, "FalseNorthing");
        oSRS.SetBonne( dfStdP1, dfCentralMeridian,
                       dfFalseEasting, dfFalseNorthing );
    }

    else if (EQUAL(osProjectionType, "CS")) /* Cassini */
    {
        double dfCenterLat = Get(poProjDict, "OriginLatitude");
        double dfCenterLong = Get(poProjDict, "CentralMeridian");
        double dfFalseEasting = Get(poProjDict, "FalseEasting");
        double dfFalseNorthing = Get(poProjDict, "FalseNorthing");
        oSRS.SetCS(  dfCenterLat, dfCenterLong,
                     dfFalseEasting, dfFalseNorthing );
    }

    else if (EQUAL(osProjectionType, "LI")) /* Cylindrical Equal Area */
    {
        double dfStdP1 = Get(poProjDict, "OriginLatitude");
        double dfCentralMeridian = Get(poProjDict, "CentralMeridian");
        double dfFalseEasting = Get(poProjDict, "FalseEasting");
        double dfFalseNorthing = Get(poProjDict, "FalseNorthing");
        oSRS.SetCEA( dfStdP1, dfCentralMeridian,
                     dfFalseEasting, dfFalseNorthing );
    }

    else if (EQUAL(osProjectionType, "EF")) /* Eckert IV */
    {
        double dfCentralMeridian = Get(poProjDict, "CentralMeridian");
        double dfFalseEasting = Get(poProjDict, "FalseEasting");
        double dfFalseNorthing = Get(poProjDict, "FalseNorthing");
        oSRS.SetEckertIV( dfCentralMeridian,
                          dfFalseEasting, dfFalseNorthing );
    }

    else if (EQUAL(osProjectionType, "ED")) /* Eckert VI */
    {
        double dfCentralMeridian = Get(poProjDict, "CentralMeridian");
        double dfFalseEasting = Get(poProjDict, "FalseEasting");
        double dfFalseNorthing = Get(poProjDict, "FalseNorthing");
        oSRS.SetEckertVI( dfCentralMeridian,
                          dfFalseEasting, dfFalseNorthing );
    }

    else if (EQUAL(osProjectionType, "CP")) /* Equidistant Cylindrical */
    {
        double dfCenterLat = Get(poProjDict, "StandardParallel");
        double dfCenterLong = Get(poProjDict, "CentralMeridian");
        double dfFalseEasting = Get(poProjDict, "FalseEasting");
        double dfFalseNorthing = Get(poProjDict, "FalseNorthing");
        oSRS.SetEquirectangular( dfCenterLat, dfCenterLong,
                                 dfFalseEasting, dfFalseNorthing );
    }

    else if (EQUAL(osProjectionType, "GN")) /* Gnomonic */
    {
        double dfCenterLat = Get(poProjDict, "OriginLatitude");
        double dfCenterLong = Get(poProjDict, "CentralMeridian");
        double dfFalseEasting = Get(poProjDict, "FalseEasting");
        double dfFalseNorthing = Get(poProjDict, "FalseNorthing");
        oSRS.SetGnomonic(dfCenterLat, dfCenterLong,
                         dfFalseEasting, dfFalseNorthing );
    }

    else if (EQUAL(osProjectionType, "LE")) /* Lambert Conformal Conic */
    {
        double dfStdP1 = Get(poProjDict, "StandardParallelOne");
        double dfStdP2 = Get(poProjDict, "StandardParallelTwo");
        double dfCenterLat = Get(poProjDict, "OriginLatitude");
        double dfCenterLong = Get(poProjDict, "CentralMeridian");
        double dfFalseEasting = Get(poProjDict, "FalseEasting");
        double dfFalseNorthing = Get(poProjDict, "FalseNorthing");
        oSRS.SetLCC( dfStdP1, dfStdP2,
                     dfCenterLat, dfCenterLong,
                     dfFalseEasting, dfFalseNorthing );
    }

    else if (EQUAL(osProjectionType, "MC")) /* Mercator */
    {
        double dfCenterLat = Get(poProjDict, "OriginLatitude");
        double dfCenterLong = Get(poProjDict, "CentralMeridian");
        double dfScale = Get(poProjDict, "ScaleFactor");
        double dfFalseEasting = Get(poProjDict, "FalseEasting");
        double dfFalseNorthing = Get(poProjDict, "FalseNorthing");
        oSRS.SetMercator( dfCenterLat, dfCenterLong,
                          dfScale,
                          dfFalseEasting, dfFalseNorthing );
    }

    else if (EQUAL(osProjectionType, "MH")) /* Miller Cylindrical */
    {
        double dfCenterLat = 0 /* ? */;
        double dfCenterLong = Get(poProjDict, "CentralMeridian");
        double dfFalseEasting = Get(poProjDict, "FalseEasting");
        double dfFalseNorthing = Get(poProjDict, "FalseNorthing");
        oSRS.SetMC( dfCenterLat, dfCenterLong,
                    dfFalseEasting, dfFalseNorthing );
    }

    else if (EQUAL(osProjectionType, "MP")) /* Mollweide */
    {
        double dfCentralMeridian = Get(poProjDict, "CentralMeridian");
        double dfFalseEasting = Get(poProjDict, "FalseEasting");
        double dfFalseNorthing = Get(poProjDict, "FalseNorthing");
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
        double dfCenterLat = Get(poProjDict, "OriginLatitude");
        double dfLat1 = Get(poProjDict, "LatitudeOne");
        double dfLong1 = Get(poProjDict, "LongitudeOne");
        double dfLat2 = Get(poProjDict, "LatitudeTwo");
        double dfLong2 = Get(poProjDict, "LongitudeTwo");
        double dfScale = Get(poProjDict, "ScaleFactor");
        double dfFalseEasting = Get(poProjDict, "FalseEasting");
        double dfFalseNorthing = Get(poProjDict, "FalseNorthing");
        oSRS.SetHOM2PNO( dfCenterLat,
                         dfLat1, dfLong1,
                         dfLat2, dfLong2,
                         dfScale,
                         dfFalseEasting,
                         dfFalseNorthing );
    }

    else if (EQUAL(osProjectionType, "OD")) /* Orthographic */
    {
        double dfCenterLat = Get(poProjDict, "OriginLatitude");
        double dfCenterLong = Get(poProjDict, "CentralMeridian");
        double dfFalseEasting = Get(poProjDict, "FalseEasting");
        double dfFalseNorthing = Get(poProjDict, "FalseNorthing");
        oSRS.SetOrthographic( dfCenterLat, dfCenterLong,
                           dfFalseEasting, dfFalseNorthing );
    }

    else if (EQUAL(osProjectionType, "PG")) /* Polar Stereographic */
    {
        double dfCenterLat = Get(poProjDict, "LatitudeTrueScale");
        double dfCenterLong = Get(poProjDict, "LongitudeDownFromPole");
        double dfScale = 1.0;
        double dfFalseEasting = Get(poProjDict, "FalseEasting");
        double dfFalseNorthing = Get(poProjDict, "FalseNorthing");
        oSRS.SetPS( dfCenterLat, dfCenterLong,
                    dfScale,
                    dfFalseEasting, dfFalseNorthing);
    }

    else if (EQUAL(osProjectionType, "PH")) /* Polyconic */
    {
        double dfCenterLat = Get(poProjDict, "OriginLatitude");
        double dfCenterLong = Get(poProjDict, "CentralMeridian");
        double dfFalseEasting = Get(poProjDict, "FalseEasting");
        double dfFalseNorthing = Get(poProjDict, "FalseNorthing");
        oSRS.SetPolyconic( dfCenterLat, dfCenterLong,
                           dfFalseEasting, dfFalseNorthing );
    }

    else if (EQUAL(osProjectionType, "SA")) /* Sinusoidal */
    {
        double dfCenterLong = Get(poProjDict, "CentralMeridian");
        double dfFalseEasting = Get(poProjDict, "FalseEasting");
        double dfFalseNorthing = Get(poProjDict, "FalseNorthing");
        oSRS.SetSinusoidal( dfCenterLong,
                           dfFalseEasting, dfFalseNorthing );
    }

    else if (EQUAL(osProjectionType, "SD")) /* Stereographic */
    {
        double dfCenterLat = Get(poProjDict, "OriginLatitude");
        double dfCenterLong = Get(poProjDict, "CentralMeridian");
        double dfScale = 1.0;
        double dfFalseEasting = Get(poProjDict, "FalseEasting");
        double dfFalseNorthing = Get(poProjDict, "FalseNorthing");
        oSRS.SetStereographic( dfCenterLat, dfCenterLong,
                               dfScale,
                               dfFalseEasting, dfFalseNorthing);
    }

    else if (EQUAL(osProjectionType, "TC")) /* Transverse Mercator */
    {
        double dfCenterLat = Get(poProjDict, "OriginLatitude");
        double dfCenterLong = Get(poProjDict, "CentralMeridian");
        double dfScale = Get(poProjDict, "ScaleFactor");
        double dfFalseEasting = Get(poProjDict, "FalseEasting");
        double dfFalseNorthing = Get(poProjDict, "FalseNorthing");
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
        double dfCenterLong = Get(poProjDict, "CentralMeridian");
        double dfFalseEasting = Get(poProjDict, "FalseEasting");
        double dfFalseNorthing = Get(poProjDict, "FalseNorthing");
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
    CPLString osUnits;
    GDALPDFObject* poUnits;
    if ((poUnits = poProjDict->Get("Units")) != NULL &&
        poUnits->GetType() == PDFObjectType_String)
    {
        osUnits = poUnits->GetString();
        CPLDebug("PDF", "Projection.Units = %s", osUnits.c_str());

        if (EQUAL(osUnits, "M"))
            oSRS.SetLinearUnits( "Meter", 1.0 );
        else if (EQUAL(osUnits, "FT"))
            oSRS.SetLinearUnits( "foot", 0.3048 );
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

int PDFDataset::ParseVP(GDALPDFObject* poVP, double dfMediaBoxWidth, double dfMediaBoxHeight)
{
    int i;

    if (poVP->GetType() != PDFObjectType_Array)
        return FALSE;

    GDALPDFArray* poVPArray = poVP->GetArray();

    int nLength = poVPArray->GetLength();
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
        GDALPDFObject* poVPElt = poVPArray->Get(i);
        if (poVPElt == NULL || poVPElt->GetType() != PDFObjectType_Dictionary)
        {
            return FALSE;
        }

        GDALPDFDictionary* poVPEltDict = poVPElt->GetDictionary();

        GDALPDFObject* poBBox;
        if( (poBBox = poVPEltDict->Get("BBox")) == NULL ||
            poBBox->GetType() != PDFObjectType_Array )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                    "Cannot find Bbox object");
            return FALSE;
        }

        int nBboxLength = poBBox->GetArray()->GetLength();
        if (nBboxLength != 4)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                    "Invalid length for Bbox object");
            return FALSE;
        }

        double adfBBox[4];
        adfBBox[0] = Get(poBBox, 0);
        adfBBox[1] = Get(poBBox, 1);
        adfBBox[2] = Get(poBBox, 2);
        adfBBox[3] = Get(poBBox, 3);
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


    GDALPDFObject* poVPElt = poVPArray->Get(iLargest);
    if (poVPElt == NULL || poVPElt->GetType() != PDFObjectType_Dictionary)
    {
        return FALSE;
    }

    GDALPDFDictionary* poVPEltDict = poVPElt->GetDictionary();

    GDALPDFObject* poBBox;
    if( (poBBox = poVPEltDict->Get("BBox")) == NULL ||
        poBBox->GetType() != PDFObjectType_Array )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                "Cannot find Bbox object");
        return FALSE;
    }

    int nBboxLength = poBBox->GetArray()->GetLength();
    if (nBboxLength != 4)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                "Invalid length for Bbox object");
        return FALSE;
    }

    double dfULX = Get(poBBox, 0);
    double dfULY = dfMediaBoxHeight - Get(poBBox, 1);
    double dfLRX = Get(poBBox, 2);
    double dfLRY = dfMediaBoxHeight - Get(poBBox, 3);

/* -------------------------------------------------------------------- */
/*      Extract Measure attribute                                       */
/* -------------------------------------------------------------------- */
    GDALPDFObject* poMeasure;
    if( (poMeasure = poVPEltDict->Get("Measure")) == NULL ||
        poMeasure->GetType() != PDFObjectType_Dictionary )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot find Measure object");
        return FALSE;
    }

    GDALPDFDictionary* poMeasureDict = poMeasure->GetDictionary();

/* -------------------------------------------------------------------- */
/*      Extract Subtype attribute                                       */
/* -------------------------------------------------------------------- */
    GDALPDFObject* poSubtype;
    if( (poSubtype = poMeasureDict->Get("Subtype")) == NULL ||
        poSubtype->GetType() != PDFObjectType_Name )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot find Subtype object");
        return FALSE;
    }

    CPLDebug("PDF", "Subtype = %s", poSubtype->GetName().c_str());

/* -------------------------------------------------------------------- */
/*      Extract Bounds attribute                                       */
/* -------------------------------------------------------------------- */

    /* http://acrobatusers.com/sites/default/files/gallery_pictures/SEVERODVINSK.pdf */
    /* has lgit:LPTS, lgit:GPTS and lgit:Bounds that have more precision than */
    /* LPTS, GPTS and Bounds. Use those ones */

    GDALPDFObject* poBounds;
    if( (poBounds = poMeasureDict->Get("lgit:Bounds")) != NULL &&
        poBounds->GetType() == PDFObjectType_Array )
    {
        CPLDebug("PDF", "Using lgit:Bounds");
    }
    else if( (poBounds = poMeasureDict->Get("Bounds")) == NULL ||
              poBounds->GetType() != PDFObjectType_Array )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot find Bounds object");
        return FALSE;
    }

    int nBoundsLength = poBounds->GetArray()->GetLength();
    if (nBoundsLength != 8)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Invalid length for Bounds object");
        return FALSE;
    }

    double adfBounds[8];
    for(i=0;i<8;i++)
    {
        adfBounds[i] = Get(poBounds, i);
        CPLDebug("PDF", "Bounds[%d] = %f", i, adfBounds[i]);
    }

/* -------------------------------------------------------------------- */
/*      Extract GPTS attribute                                          */
/* -------------------------------------------------------------------- */
    GDALPDFObject* poGPTS;
    if( (poGPTS = poMeasureDict->Get("lgit:GPTS")) != NULL &&
        poGPTS->GetType() == PDFObjectType_Array )
    {
        CPLDebug("PDF", "Using lgit:GPTS");
    }
    else if( (poGPTS = poMeasureDict->Get("GPTS")) == NULL ||
              poGPTS->GetType() != PDFObjectType_Array )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot find GPTS object");
        return FALSE;
    }

    int nGPTSLength = poGPTS->GetArray()->GetLength();
    if (nGPTSLength != 8)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Invalid length for GPTS object");
        return FALSE;
    }

    double adfGPTS[8];
    for(i=0;i<8;i++)
    {
        adfGPTS[i] = Get(poGPTS, i);
        CPLDebug("PDF", "GPTS[%d] = %.18f", i, adfGPTS[i]);
    }

/* -------------------------------------------------------------------- */
/*      Extract LPTS attribute                                          */
/* -------------------------------------------------------------------- */
    GDALPDFObject* poLPTS;
    if( (poLPTS = poMeasureDict->Get("lgit:LPTS")) != NULL &&
        poLPTS->GetType() == PDFObjectType_Array )
    {
        CPLDebug("PDF", "Using lgit:LPTS");
    }
    else if( (poLPTS = poMeasureDict->Get("LPTS")) == NULL ||
              poLPTS->GetType() != PDFObjectType_Array )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot find LPTS object");
        return FALSE;
    }

    int nLPTSLength = poLPTS->GetArray()->GetLength();
    if (nLPTSLength != 8)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Invalid length for LPTS object");
        return FALSE;
    }

    double adfLPTS[8];
    for(i=0;i<8;i++)
    {
        adfLPTS[i] = Get(poLPTS, i);
        CPLDebug("PDF", "LPTS[%d] = %f", i, adfLPTS[i]);
    }

/* -------------------------------------------------------------------- */
/*      Extract GCS attribute                                           */
/* -------------------------------------------------------------------- */
    GDALPDFObject* poGCS;
    if( (poGCS = poMeasureDict->Get("GCS")) == NULL ||
        poGCS->GetType() != PDFObjectType_Dictionary )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot find GCS object");
        return FALSE;
    }

    GDALPDFDictionary* poGCSDict = poGCS->GetDictionary();

/* -------------------------------------------------------------------- */
/*      Extract GCS.Type attribute                                      */
/* -------------------------------------------------------------------- */
    GDALPDFObject* poGCSType;
    if( (poGCSType = poGCSDict->Get("Type")) == NULL ||
        poGCSType->GetType() != PDFObjectType_Name )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot find GCS.Type object");
        return FALSE;
    }

    CPLDebug("PDF", "GCS.Type = %s", poGCSType->GetName().c_str());

/* -------------------------------------------------------------------- */
/*      Extract EPSG attribute                                          */
/* -------------------------------------------------------------------- */
    GDALPDFObject* poEPSG;
    int nEPSGCode = 0;
    if( (poEPSG = poGCSDict->Get("EPSG")) != NULL &&
        poEPSG->GetType() == PDFObjectType_Int )
    {
        nEPSGCode = poEPSG->GetInt();
        CPLDebug("PDF", "GCS.EPSG = %d", nEPSGCode);
    }

/* -------------------------------------------------------------------- */
/*      Extract GCS.WKT attribute                                       */
/* -------------------------------------------------------------------- */
    GDALPDFObject* poGCSWKT;
    if( (poGCSWKT = poGCSDict->Get("WKT")) == NULL ||
        poGCSWKT->GetType() != PDFObjectType_String )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot find GCS.WKT object");
        return FALSE;
    }

    CPLDebug("PDF", "GCS.WKT = %s", poGCSWKT->GetString().c_str());

    OGRSpatialReference oSRS;
    int bSRSOK = FALSE;
    if (nEPSGCode != 0 &&
        oSRS.importFromEPSG(nEPSGCode) == OGRERR_NONE)
    {
        bSRSOK = TRUE;
        CPLFree(pszWKT);
        pszWKT = NULL;
        oSRS.exportToWkt(&pszWKT);
    }
    else
    {
        CPLFree(pszWKT);
        pszWKT = CPLStrdup(poGCSWKT->GetString().c_str());
    }

    if (!bSRSOK)
    {
        char* pszWktTemp = pszWKT;
        if (oSRS.importFromWkt(&pszWktTemp) != OGRERR_NONE)
        {
            CPLFree(pszWKT);
            pszWKT = NULL;
            return FALSE;
        }
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

/* -------------------------------------------------------------------- */
/*      Compute geotransform                                            */
/* -------------------------------------------------------------------- */
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
    GDALPDFObject* poPointData;
    if( (poPointData = poVPEltDict->Get("PtData")) != NULL &&
        poPointData->GetType() == PDFObjectType_Dictionary )
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
/*                            GetGCPCount()                             */
/************************************************************************/

int PDFDataset::GetGCPCount()
{
    return nGCPCount;
}

/************************************************************************/
/*                          GetGCPProjection()                          */
/************************************************************************/

const char * PDFDataset::GetGCPProjection()
{
    if (pszWKT != NULL && nGCPCount != 0)
        return pszWKT;
    return "";
}

/************************************************************************/
/*                              GetGCPs()                               */
/************************************************************************/

const GDAL_GCP * PDFDataset::GetGCPs()
{
    return pasGCPList;
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
        poDriver->SetMetadataItem( GDAL_DMD_CREATIONDATATYPES,
                                   "Byte" );
#ifdef USE_POPPLER
        poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );
#endif

        poDriver->SetMetadataItem( GDAL_DMD_CREATIONOPTIONLIST,
"<CreationOptionList>\n"
"   <Option name='COMPRESS' type='string-select' default='DEFLATE'>\n"
"     <Value>NONE</Value>\n"
"     <Value>DEFLATE</Value>\n"
"     <Value>JPEG</Value>\n"
"     <Value>JPEG2000</Value>\n"
"   </Option>\n"
"   <Option name='GEO_ENCODING' type='string-select' default='ISO32000'>\n"
"     <Value>NONE</Value>\n"
"     <Value>ISO32000</Value>\n"
"     <Value>OGC_BP</Value>\n"
"     <Value>BOTH</Value>\n"
"   </Option>\n"
"   <Option name='JPEG_QUALITY' type='int' description='JPEG quality 1-100' default='75'/>\n"
"   <Option name='JPEG2000_DRIVER' type='string'/>\n"
"   <Option name='TILED' type='boolean' description='Switch to tiled format'/>\n"
"   <Option name='BLOCKXSIZE' type='int' description='Block Width'/>\n"
"   <Option name='BLOCKYSIZE' type='int' description='Block Height'/>\n"
"</CreationOptionList>\n" );

        poDriver->pfnOpen = PDFDataset::Open;
        poDriver->pfnIdentify = PDFDataset::Identify;
        poDriver->pfnCreateCopy = GDALPDFCreateCopy;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}

