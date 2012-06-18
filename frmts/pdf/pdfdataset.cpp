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

#include "pdfdataset.h"
#include "cpl_vsi_virtual.h"
#include "cpl_string.h"
#include "gdal_pam.h"
#include "ogr_spatialref.h"
#include "ogr_geometry.h"

#ifdef HAVE_POPPLER
#include "pdfio.h"
#include <goo/GooList.h>
#endif

#include "pdfobject.h"
#include "pdfcreatecopy.h"

#include <set>
#include <map>

/* g++ -fPIC -g -Wall frmts/pdf/pdfdataset.cpp -shared -o gdal_PDF.so -Iport -Igcore -Iogr -L. -lgdal -lpoppler -I/usr/include/poppler */

CPL_CVSID("$Id$");

CPL_C_START
void    GDALRegister_PDF(void);
CPL_C_END

#if defined(HAVE_POPPLER) || defined(HAVE_PODOFO)

static double Get(GDALPDFObject* poObj, int nIndice = -1);

#ifdef HAVE_POPPLER

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
                            GBool inlineImg, double *baseMatrix)
        {
            if (bEnableBitmap)
                SplashOutputDev::setSoftMaskFromImageMask(state, ref, str,
                                               width, height, invert,
                                               inlineImg, baseMatrix);
            else
                str->close();
        }

        virtual void unsetSoftMaskFromImageMask(GfxState *state, double *baseMatrix)
        {
            if (bEnableBitmap)
                SplashOutputDev::unsetSoftMaskFromImageMask(state, baseMatrix);
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

class GDALPDFDumper
{
    private:
        FILE* f;
        int   nDepthLimit;
        std::set< int > aoSetObjectExplored;
        int   bDumpParent;

        void DumpSimplified(GDALPDFObject* poObj);

    public:
        GDALPDFDumper(FILE* fIn, int nDepthLimitIn = -1) : f(fIn), nDepthLimit(nDepthLimitIn)
        {
            bDumpParent = CSLTestBoolean(CPLGetConfigOption("PDF_DUMP_PARENT", "FALSE"));
        }

        void Dump(GDALPDFObject* poObj, int nDepth = 0);
        void Dump(GDALPDFDictionary* poDict, int nDepth = 0);
        void Dump(GDALPDFArray* poArray, int nDepth = 0);
};

void GDALPDFDumper::Dump(GDALPDFArray* poArray, int nDepth)
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
                poObj->GetType() == PDFObjectType_Null ||
                poObj->GetType() == PDFObjectType_Bool ||
                poObj->GetType() == PDFObjectType_Int ||
                poObj->GetType() == PDFObjectType_Real ||
                poObj->GetType() == PDFObjectType_Name)
            {
                fprintf(f, " ");
                DumpSimplified(poObj);
                fprintf(f, "\n");
            }
            else
            {
                fprintf(f, "\n");
                Dump( poObj, nDepth+1);
            }
        }
    }
}

void GDALPDFDumper::DumpSimplified(GDALPDFObject* poObj)
{
    switch(poObj->GetType())
    {
        case PDFObjectType_String:
            fprintf(f, "%s (string)", poObj->GetString().c_str());
            break;

        case PDFObjectType_Null:
            fprintf(f, "null");
            break;

        case PDFObjectType_Bool:
            fprintf(f, "%s (bool)", poObj->GetBool() ? "true" : "false");
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
            fprintf(f, "unknown !");
            break;
    }
}

void GDALPDFDumper::Dump(GDALPDFObject* poObj, int nDepth)
{
    if (nDepthLimit >= 0 && nDepth > nDepthLimit)
        return;

    int i;
    CPLString osIndent;
    for(i=0;i<nDepth;i++)
        osIndent += " ";
    fprintf(f, "%sType = %s",
            osIndent.c_str(), poObj->GetTypeName());
    int nRefNum = poObj->GetRefNum();
    if (nRefNum != 0)
        fprintf(f, ", Num = %d, Gen = %d",
                nRefNum, poObj->GetRefGen());
    fprintf(f, "\n");

    if (nRefNum != 0)
    {
        if (aoSetObjectExplored.find(nRefNum) != aoSetObjectExplored.end())
            return;
        aoSetObjectExplored.insert(nRefNum);
    }

    switch(poObj->GetType())
    {
        case PDFObjectType_Array:
            Dump(poObj->GetArray(), nDepth+1);
            break;

        case PDFObjectType_Dictionary:
            Dump(poObj->GetDictionary(), nDepth+1);
            break;

        case PDFObjectType_String:
        case PDFObjectType_Null:
        case PDFObjectType_Bool:
        case PDFObjectType_Int:
        case PDFObjectType_Real:
        case PDFObjectType_Name:
            fprintf(f, "%s", osIndent.c_str());
            DumpSimplified(poObj);
            fprintf(f, "\n");
            break;

        default:
            fprintf(f, "%s", osIndent.c_str());
            fprintf(f, "unknown !\n");
            break;
    }

    GDALPDFStream* poStream = poObj->GetStream();
    if (poStream != NULL)
    {
        fprintf(f, "%sHas stream (%d bytes)\n", osIndent.c_str(), poStream->GetLength());
    }
}

void GDALPDFDumper::Dump(GDALPDFDictionary* poDict, int nDepth)
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
        GDALPDFObject* poObj = oIter->second;
        if (strcmp(pszKey, "Parent") == 0 && !bDumpParent)
        {
            if (poObj->GetRefNum())
                fprintf(f, ", Num = %d, Gen = %d",
                        poObj->GetRefNum(), poObj->GetRefGen());
            fprintf(f, "\n");
            continue;
        }
        if (poObj != NULL)
        {
            if (poObj->GetType() == PDFObjectType_String ||
                poObj->GetType() == PDFObjectType_Null ||
                poObj->GetType() == PDFObjectType_Bool ||
                poObj->GetType() == PDFObjectType_Int ||
                poObj->GetType() == PDFObjectType_Real ||
                poObj->GetType() == PDFObjectType_Name)
            {
                fprintf(f, " = ");
                DumpSimplified(poObj);
                fprintf(f, "\n");
            }
            else
            {
                fprintf(f, "\n");
                Dump(poObj, nDepth+1);
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
class PDFImageRasterBand;

class PDFDataset : public GDALPamDataset
{
    friend class PDFRasterBand;
    friend class PDFImageRasterBand;

    CPLString    osFilename;
    CPLString    osUserPwd;
    char        *pszWKT;
    double       dfDPI;
    int          bHasCTM;
    double       adfCTM[6];
    double       adfGeoTransform[6];
    int          bGeoTransformValid;
    int          nGCPCount;
    GDAL_GCP    *pasGCPList;
    int          bProjDirty;
    int          bNeatLineDirty;

    GDALMultiDomainMetadata oMDMD;
    int          bInfoDirty;
    int          bXMPDirty;

    int          bUsePoppler;
#ifdef HAVE_POPPLER
    PDFDoc*      poDocPoppler;
#endif
#ifdef HAVE_PODOFO
    PoDoFo::PdfMemDocument* poDocPodofo;
#endif
    GDALPDFObject* poPageObj;

    int          iPage;

    GDALPDFObject *poImageObj;

    double       dfMaxArea;
    int          ParseLGIDictObject(GDALPDFObject* poLGIDict);
    int          ParseLGIDictDictFirstPass(GDALPDFDictionary* poLGIDict, int* pbIsLargestArea = NULL);
    int          ParseLGIDictDictSecondPass(GDALPDFDictionary* poLGIDict);
    int          ParseProjDict(GDALPDFDictionary* poProjDict);
    int          ParseVP(GDALPDFObject* poVP, double dfMediaBoxWidth, double dfMediaBoxHeight);
    int          ParseMeasure(GDALPDFObject* poMeasure,
                              double dfMediaBoxWidth, double dfMediaBoxHeight,
                              double dfULX, double dfULY, double dfLRX, double dfLRY);

    int          bTried;
    GByte       *pabyData;

    OGRPolygon*  poNeatLine;

    void         GuessDPI(GDALPDFDictionary* poPageDict, int* pnBands);
    void         FindXMP(GDALPDFObject* poObj);
    void         ParseInfo(GDALPDFObject* poObj);

#ifdef HAVE_POPPLER
    ObjectAutoFree* poCatalogObjectPoppler;
#endif
    GDALPDFObject* poCatalogObject;
    GDALPDFObject* GetCatalog();

#ifdef HAVE_POPPLER
    void         AddLayer(const char* pszLayerName, OptionalContentGroup* ocg);
    void         ExploreLayers(GDALPDFArray* poArray, CPLString osTopLayer = "");
    void         FindLayers();
    void         TurnLayersOnOff();
    CPLStringList osLayerList;
    std::map<CPLString, OptionalContentGroup*> oLayerOCGMap;
#endif

    CPLStringList osLayerWithRefList;
    void          FindLayersGeneric(GDALPDFDictionary* poPageDict);

    int          bUseOCG;

  public:
                 PDFDataset();
    virtual     ~PDFDataset();

    virtual const char* GetProjectionRef();
    virtual CPLErr GetGeoTransform( double * );

    virtual CPLErr      SetProjection(const char* pszWKTIn);
    virtual CPLErr      SetGeoTransform(double* padfGeoTransform);

    virtual char      **GetMetadata( const char * pszDomain = "" );
    virtual CPLErr      SetMetadata( char ** papszMetadata,
                                     const char * pszDomain = "" );
    virtual const char *GetMetadataItem( const char * pszName,
                                         const char * pszDomain = "" );
    virtual CPLErr      SetMetadataItem( const char * pszName,
                                         const char * pszValue,
                                         const char * pszDomain = "" );

    virtual int    GetGCPCount();
    virtual const char *GetGCPProjection();
    virtual const GDAL_GCP *GetGCPs();
    virtual CPLErr SetGCPs( int nGCPCount, const GDAL_GCP *pasGCPList,
                            const char *pszGCPProjection );

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
    PDFDataset *poGDS = (PDFDataset *) poDS;
    if (poGDS->nBands == 1)
        return GCI_GrayIndex;
    else
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
        poGDS->pabyData = (GByte*)VSIMalloc3(MAX(3, poGDS->nBands), nRasterXSize, nRasterYSize);
        if (poGDS->pabyData == NULL)
            return CE_Failure;

        const char* pszRenderingOptions = CPLGetConfigOption("GDAL_PDF_RENDERING_OPTIONS", NULL);

#ifdef HAVE_POPPLER
      if(poGDS->bUsePoppler)
      {
        SplashColor sColor;
        sColor[0] = 255;
        sColor[1] = 255;
        sColor[2] = 255;
        GDALPDFOutputDev *poSplashOut;
        poSplashOut = new GDALPDFOutputDev((poGDS->nBands < 4) ? splashModeRGB8 : splashModeXBGR8,
                                           4, gFalse,
                                           (poGDS->nBands < 4) ? sColor : NULL);

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

        PDFDoc* poDoc = poGDS->poDocPoppler;
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
        if (!poGDS->bUseOCG)
            poCatalog->optContent = NULL;
#endif
        poDoc->displayPageSlice(poSplashOut,
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
                if (poGDS->nBands < 4)
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
      }
#endif // HAVE_POPPLER

#ifdef HAVE_PODOFO
      if (!poGDS->bUsePoppler)
      {
        CPLAssert(poGDS->nBands != 4);

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
      }
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
/* ==================================================================== */
/*                        PDFImageRasterBand                            */
/* ==================================================================== */
/************************************************************************/

class PDFImageRasterBand : public PDFRasterBand
{
    friend class PDFDataset;

  public:

                PDFImageRasterBand( PDFDataset *, int );

    virtual CPLErr IReadBlock( int, int, void * );
};


/************************************************************************/
/*                        PDFImageRasterBand()                          */
/************************************************************************/

PDFImageRasterBand::PDFImageRasterBand( PDFDataset *poDS, int nBand ) : PDFRasterBand(poDS, nBand)

{
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr PDFImageRasterBand::IReadBlock( int nBlockXOff, int nBlockYOff,
                                  void * pImage )

{
    PDFDataset *poGDS = (PDFDataset *) poDS;
    CPLAssert(poGDS->poImageObj != NULL);

    if (poGDS->bTried == FALSE)
    {
        int nBands = (poGDS->nBands == 1) ? 1 : 3;
        poGDS->bTried = TRUE;
        if (nBands == 3)
        {
            poGDS->pabyData = (GByte*)VSIMalloc3(nBands, nRasterXSize, nRasterYSize);
            if (poGDS->pabyData == NULL)
                return CE_Failure;
        }

        GDALPDFStream* poStream = poGDS->poImageObj->GetStream();
        GByte* pabyStream = NULL;

        if (poStream == NULL ||
            poStream->GetLength() != nBands * nRasterXSize * nRasterYSize ||
            (pabyStream = (GByte*) poStream->GetBytes()) == NULL)
        {
            VSIFree(poGDS->pabyData);
            poGDS->pabyData = NULL;
            return CE_Failure;
        }

        if (nBands == 3)
        {
            /* pixel interleaved to band interleaved */
            for(int i = 0; i < nRasterXSize * nRasterYSize; i++)
            {
                poGDS->pabyData[0 * nRasterXSize * nRasterYSize + i] = pabyStream[3 * i + 0];
                poGDS->pabyData[1 * nRasterXSize * nRasterYSize + i] = pabyStream[3 * i + 1];
                poGDS->pabyData[2 * nRasterXSize * nRasterYSize + i] = pabyStream[3 * i + 2];
            }
            VSIFree(pabyStream);
        }
        else
            poGDS->pabyData = pabyStream;
    }

    if (poGDS->pabyData == NULL)
        return CE_Failure;

    if (nBand == 4)
        memset(pImage, 255, nRasterXSize);
    else
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
    bUsePoppler = FALSE;
#ifdef HAVE_POPPLER
    poDocPoppler = NULL;
#endif
#ifdef HAVE_PODOFO
    poDocPodofo = NULL;
#endif
    poImageObj = NULL;
    pszWKT = NULL;
    dfMaxArea = 0;
    adfGeoTransform[0] = 0;
    adfGeoTransform[1] = 1;
    adfGeoTransform[2] = 0;
    adfGeoTransform[3] = 0;
    adfGeoTransform[4] = 0;
    adfGeoTransform[5] = 1;
    bHasCTM = FALSE;
    bGeoTransformValid = FALSE;
    nGCPCount = 0;
    pasGCPList = NULL;
    bProjDirty = FALSE;
    bNeatLineDirty = FALSE;
    bInfoDirty = FALSE;
    bXMPDirty = FALSE;
    bTried = FALSE;
    pabyData = NULL;
    iPage = -1;
    poNeatLine = NULL;
    bUseOCG = FALSE;
    poCatalogObject = NULL;
#ifdef HAVE_POPPLER
    poCatalogObjectPoppler = NULL;
#endif
}

/************************************************************************/
/*                           PDFFreeDoc()                               */
/************************************************************************/

#ifdef HAVE_POPPLER
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
/*                            GetCatalog()                              */
/************************************************************************/

GDALPDFObject* PDFDataset::GetCatalog()
{
    if (poCatalogObject)
        return poCatalogObject;

#ifdef HAVE_POPPLER
    if (bUsePoppler)
    {
        poCatalogObjectPoppler = new ObjectAutoFree;
        poDocPoppler->getXRef()->getCatalog(poCatalogObjectPoppler);
        if (!poCatalogObjectPoppler->isNull())
            poCatalogObject = new GDALPDFObjectPoppler(poCatalogObjectPoppler, FALSE);
    }
#endif

#ifdef HAVE_PODOFO
    if (!bUsePoppler)
    {
        int nCatalogNum = 0, nCatalogGen = 0;
        VSILFILE* fp = VSIFOpenL(osFilename.c_str(), "rb");
        if (fp != NULL)
        {
            GDALPDFWriter oWriter(fp, TRUE);
            if (oWriter.ParseTrailerAndXRef())
            {
                nCatalogNum = oWriter.GetCatalogNum();
                nCatalogGen = oWriter.GetCatalogGen();
            }
            oWriter.Close();
        }

        PoDoFo::PdfObject* poCatalogPodofo =
            poDocPodofo->GetObjects().GetObject(PoDoFo::PdfReference(nCatalogNum, nCatalogGen));
        if (poCatalogPodofo)
            poCatalogObject = new GDALPDFObjectPodofo(poCatalogPodofo, poDocPodofo->GetObjects());
    }
#endif

    return poCatalogObject;
}

/************************************************************************/
/*                            ~PDFDataset()                            */
/************************************************************************/

PDFDataset::~PDFDataset()
{
    CPLFree(pabyData);
    pabyData = NULL;

    delete poNeatLine;
    poNeatLine = NULL;

    /* Collect data necessary to update */
    int nNum = poPageObj->GetRefNum();
    int nGen = poPageObj->GetRefGen();
    GDALPDFDictionaryRW* poPageDictCopy = NULL;
    GDALPDFDictionaryRW* poCatalogDictCopy = NULL;
    if (eAccess == GA_Update &&
        (bProjDirty || bNeatLineDirty || bInfoDirty || bXMPDirty) &&
        nNum != 0 &&
        poPageObj != NULL &&
        poPageObj->GetType() == PDFObjectType_Dictionary)
    {
        poPageDictCopy = poPageObj->GetDictionary()->Clone();

        if (bXMPDirty)
        {
            /* We need the catalog because it points to the XMP Metadata object */
            GetCatalog();
            if (poCatalogObject && poCatalogObject->GetType() == PDFObjectType_Dictionary)
                poCatalogDictCopy = poCatalogObject->GetDictionary()->Clone();
        }
    }

    /* Close document (and file descriptor) to be able to open it */
    /* in read-write mode afterwards */
    delete poPageObj;
    poPageObj = NULL;
    delete poCatalogObject;
    poCatalogObject = NULL;
#ifdef HAVE_POPPLER
    delete poCatalogObjectPoppler;
    PDFFreeDoc(poDocPoppler);
    poDocPoppler = NULL;
#endif
#ifdef HAVE_PODOFO
    delete poDocPodofo;
    poDocPodofo = NULL;
#endif

    /* Now do the update */
    if (poPageDictCopy)
    {
        VSILFILE* fp = VSIFOpenL(osFilename, "rb+");
        if (fp != NULL)
        {
            GDALPDFWriter oWriter(fp, TRUE);
            if (oWriter.ParseTrailerAndXRef())
            {
                if ((bProjDirty || bNeatLineDirty) && poPageDictCopy != NULL)
                    oWriter.UpdateProj(this, dfDPI,
                                        poPageDictCopy, nNum, nGen);

                if (bInfoDirty)
                    oWriter.UpdateInfo(this);

                if (bXMPDirty && poCatalogDictCopy != NULL)
                    oWriter.UpdateXMP(this, poCatalogDictCopy);
            }
            oWriter.Close();
        }
        else
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Cannot open %s in update mode", osFilename.c_str());
        }
    }
    delete poPageDictCopy;
    poPageDictCopy = NULL;
    delete poCatalogDictCopy;
    poCatalogDictCopy = NULL;

    if( nGCPCount > 0 )
    {
        GDALDeinitGCPs( nGCPCount, pasGCPList );
        CPLFree( pasGCPList );
        pasGCPList = NULL;
        nGCPCount = 0;
    }
    CPLFree(pszWKT);
    pszWKT = NULL;
}

/************************************************************************/
/*                             Identify()                               */
/************************************************************************/

int PDFDataset::Identify( GDALOpenInfo * poOpenInfo )
{
    if (strncmp(poOpenInfo->pszFilename, "PDF:", 4) == 0)
        return TRUE;
    if (strncmp(poOpenInfo->pszFilename, "PDF_IMAGE:", 10) == 0)
        return TRUE;

    if (poOpenInfo->nHeaderBytes < 128)
        return FALSE;

    return strncmp((const char*)poOpenInfo->pabyHeader, "%PDF", 4) == 0;
}

/************************************************************************/
/*                    PDFDatasetErrorFunction()                         */
/************************************************************************/

#ifdef HAVE_POPPLER
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
/*                GDALPDFParseStreamContentOnlyDrawForm()               */
/************************************************************************/

static
CPLString GDALPDFParseStreamContentOnlyDrawForm(const char* pszContent)
{
    CPLString osToken;
    char ch;
    int nCurIdx = 0;
    CPLString osCurrentForm;

    while((ch = *pszContent) != '\0')
    {
        if (ch == '%')
        {
            /* Skip comments until end-of-line */
            while((ch = *pszContent) != '\0')
            {
                if (ch == '\r' || ch == '\n')
                    break;
                pszContent ++;
            }
            if (ch == 0)
                break;
        }
        else if (ch == ' ' || ch == '\r' || ch == '\n')
        {
            if (osToken.size())
            {
                if (nCurIdx == 0 && osToken[0] == '/')
                {
                    osCurrentForm = osToken.substr(1);
                    nCurIdx ++;
                }
                else if (nCurIdx == 1 && osToken == "Do")
                {
                    nCurIdx ++;
                }
                else
                {
                    return "";
                }
            }
            osToken = "";
        }
        else
            osToken += ch;
        pszContent ++;
    }

    return osCurrentForm;
}

/************************************************************************/
/*                    GDALPDFParseStreamContent()                       */
/************************************************************************/

typedef enum
{
    STATE_INIT,
    STATE_AFTER_q,
    STATE_AFTER_cm,
    STATE_AFTER_Do
} PDFStreamState;

/* This parser is reduced to understanding sequences that draw rasters, such as :
   q
   scaleX 0 0 scaleY translateX translateY cm
   /ImXXX Do
   Q

   All other sequences will abort the parsing.

   Returns TRUE if the stream only contains images.
*/

static
int GDALPDFParseStreamContent(const char* pszContent,
                              GDALPDFDictionary* poXObjectDict,
                              double* pdfDPI,
                              int* pbDPISet,
                              int* pnBands)
{
    CPLString osToken;
    char ch;
    PDFStreamState nState = STATE_INIT;
    int nCurIdx = 0;
    double adfVals[6];
    CPLString osCurrentImage;

    double dfDPI = 72.0;
    *pbDPISet = FALSE;

    while((ch = *pszContent) != '\0')
    {
        if (ch == '%')
        {
            /* Skip comments until end-of-line */
            while((ch = *pszContent) != '\0')
            {
                if (ch == '\r' || ch == '\n')
                    break;
                pszContent ++;
            }
            if (ch == 0)
                break;
        }
        else if (ch == ' ' || ch == '\r' || ch == '\n')
        {
            if (osToken.size())
            {
                if (nState == STATE_INIT)
                {
                    if (osToken == "q")
                    {
                        nState = STATE_AFTER_q;
                        nCurIdx = 0;
                    }
                    else
                        return FALSE;
                }
                else if (nState == STATE_AFTER_q)
                {
                    if (nCurIdx < 6)
                    {
                        adfVals[nCurIdx ++] = CPLAtof(osToken);
                    }
                    else if (osToken == "cm")
                    {
                        nState = STATE_AFTER_cm;
                        nCurIdx = 0;
                    }
                    else
                        return FALSE;
                }
                else if (nState == STATE_AFTER_cm)
                {
                    if (nCurIdx == 0 && osToken[0] == '/')
                    {
                        osCurrentImage = osToken.substr(1);
                    }
                    else if (osToken == "Do")
                    {
                        nState = STATE_AFTER_Do;
                    }
                    else
                        return FALSE;
                }
                else if (nState == STATE_AFTER_Do)
                {
                    if (osToken == "Q")
                    {
                        GDALPDFObject* poImage = poXObjectDict->Get(osCurrentImage);
                        if (poImage != NULL && poImage->GetType() == PDFObjectType_Dictionary)
                        {
                            GDALPDFDictionary* poImageDict = poImage->GetDictionary();
                            GDALPDFObject* poWidth = poImageDict->Get("Width");
                            GDALPDFObject* poHeight = poImageDict->Get("Height");
                            GDALPDFObject* poColorSpace = poImageDict->Get("ColorSpace");
                            if (poColorSpace && poColorSpace->GetType() == PDFObjectType_Name)
                            {
                                if (poColorSpace->GetName() == "DeviceRGB" && *pnBands < 3)
                                    *pnBands = 3;
                                else if (poColorSpace->GetName() == "DeviceGray")
                                    *pnBands = 1;
                            }

                            if (poWidth && poHeight && adfVals[1] == 0.0 && adfVals[2] == 0.0)
                            {
                                double dfWidth = Get(poWidth);
                                double dfHeight = Get(poHeight);
                                double dfScaleX = adfVals[0];
                                double dfScaleY = adfVals[3];
                                double dfDPI_X = ROUND_TO_INT_IF_CLOSE(dfWidth / dfScaleX * 72, 1e-3);
                                double dfDPI_Y = ROUND_TO_INT_IF_CLOSE(dfHeight / dfScaleY * 72, 1e-3);
                                //CPLDebug("PDF", "Image %s, width = %.16g, height = %.16g, scaleX = %.16g, scaleY = %.16g --> DPI_X = %.16g, DPI_Y = %.16g",
                                //                osCurrentImage.c_str(), dfWidth, dfHeight, dfScaleX, dfScaleY, dfDPI_X, dfDPI_Y);
                                if (dfDPI_X > dfDPI) dfDPI = dfDPI_X;
                                if (dfDPI_Y > dfDPI) dfDPI = dfDPI_Y;
                                *pbDPISet = TRUE;
                                *pdfDPI = dfDPI;
                            }
                        }
                        nState = STATE_INIT;
                    }
                    else
                        return FALSE;
                }
            }
            osToken = "";
        }
        else
            osToken += ch;
        pszContent ++;
    }

    return TRUE;
}

/************************************************************************/
/*                              GuessDPI()                              */
/************************************************************************/

void PDFDataset::GuessDPI(GDALPDFDictionary* poPageDict, int* pnBands)
{
    const char* pszDPI = CPLGetConfigOption("GDAL_PDF_DPI", NULL);
    if (pszDPI != NULL)
    {
        dfDPI = atof(pszDPI);
    }
    else
    {
        GDALPDFObject* poUserUnit = NULL;
        if ( (poUserUnit = poPageDict->Get("UserUnit")) != NULL &&
              (poUserUnit->GetType() == PDFObjectType_Int ||
               poUserUnit->GetType() == PDFObjectType_Real) )
        {
            dfDPI = ROUND_TO_INT_IF_CLOSE(Get(poUserUnit) * 72.0);
            CPLDebug("PDF", "Found UserUnit in Page --> DPI = %.16g", dfDPI);
            SetMetadataItem("DPI", CPLSPrintf("%.16g", dfDPI));
        }
        else
        {
            dfDPI = 150.0;

            /* Try to get a better value from the images that are drawn */
            /* Very simplistic logic. Will only work for raster only PDF */

            GDALPDFObject* poContents = poPageDict->Get("Contents");
            if (poContents != NULL && poContents->GetType() == PDFObjectType_Array)
            {
                GDALPDFArray* poContentsArray = poContents->GetArray();
                if (poContentsArray->GetLength() == 1)
                {
                    poContents = poContentsArray->Get(0);
                }
            }

            GDALPDFObject* poResources = poPageDict->Get("Resources");
            GDALPDFObject* poXObject = NULL;
            if (poResources != NULL &&
                poResources->GetType() == PDFObjectType_Dictionary)
                poXObject = poResources->GetDictionary()->Get("XObject");

            if (poContents != NULL &&
                poContents->GetType() == PDFObjectType_Dictionary &&
                poXObject != NULL &&
                poXObject->GetType() == PDFObjectType_Dictionary)
            {
                GDALPDFDictionary* poXObjectDict = poXObject->GetDictionary();
                GDALPDFStream* poPageStream = poContents->GetStream();
                if (poPageStream != NULL)
                {
                    char* pszContent = NULL;
                    int nLength = poPageStream->GetLength();
                    if( nLength < 100000 )
                    {
                        pszContent = poPageStream->GetBytes();

                        CPLString osForm = GDALPDFParseStreamContentOnlyDrawForm(pszContent);
                        if (osForm.size())
                        {
                            CPLFree(pszContent);
                            pszContent = NULL;

                            GDALPDFObject* poObjForm = poXObjectDict->Get(osForm);
                            if (poObjForm != NULL &&
                                poObjForm->GetType() == PDFObjectType_Dictionary &&
                                (poPageStream = poObjForm->GetStream()) != NULL)
                            {
                                GDALPDFDictionary* poObjFormDict = poObjForm->GetDictionary();
                                GDALPDFObject* poSubtype;
                                if ((poSubtype = poObjFormDict->Get("Subtype")) != NULL &&
                                    poSubtype->GetType() == PDFObjectType_Name &&
                                    poSubtype->GetName() == "Form")
                                {
                                    int nLength = poPageStream->GetLength();
                                    if( nLength < 100000 )
                                    {
                                        pszContent = poPageStream->GetBytes();
                                    }
                                }
                            }
                        }
                    }

                    if (pszContent != NULL)
                    {
                        int bDPISet = FALSE;
                        GDALPDFParseStreamContent(pszContent,
                                                  poXObjectDict,
                                                  &(dfDPI),
                                                  &bDPISet,
                                                  pnBands);
                        CPLFree(pszContent);
                        if (bDPISet)
                        {
                            CPLDebug("PDF", "DPI guessed from contents stream = %.16g", dfDPI);
                            SetMetadataItem("DPI", CPLSPrintf("%.16g", dfDPI));
                        }
                    }
                }
            }
        }
    }

    if (dfDPI < 1 || dfDPI > 7200)
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "Invalid value for GDAL_PDF_DPI. Using default value instead");
        dfDPI = 150;
    }
}

/************************************************************************/
/*                              FindXMP()                               */
/************************************************************************/

void PDFDataset::FindXMP(GDALPDFObject* poObj)
{
    if (poObj->GetType() != PDFObjectType_Dictionary)
        return;

    GDALPDFDictionary* poDict = poObj->GetDictionary();
    GDALPDFObject* poType = poDict->Get("Type");
    GDALPDFObject* poSubtype = poDict->Get("Subtype");
    if (poType == NULL ||
        poType->GetType() != PDFObjectType_Name ||
        poType->GetName() != "Metadata" ||
        poSubtype == NULL ||
        poSubtype->GetType() != PDFObjectType_Name ||
        poSubtype->GetName() != "XML")
    {
        return;
    }

    GDALPDFStream* poStream = poObj->GetStream();
    if (poStream == NULL)
        return;

    char* pszContent = poStream->GetBytes();
    int nLength = (int)poStream->GetLength();
    if (pszContent != NULL && nLength > 15 &&
        strncmp(pszContent, "<?xpacket begin=", strlen("<?xpacket begin=")) == 0)
    {
        char *apszMDList[2];
        apszMDList[0] = pszContent;
        apszMDList[1] = NULL;
        SetMetadata(apszMDList, "xml:XMP");
    }
    CPLFree(pszContent);
}

/************************************************************************/
/*                             ParseInfo()                              */
/************************************************************************/

void PDFDataset::ParseInfo(GDALPDFObject* poInfoObj)
{
    if (poInfoObj->GetType() != PDFObjectType_Dictionary)
        return;

    GDALPDFDictionary* poInfoObjDict = poInfoObj->GetDictionary();
    GDALPDFObject* poItem = NULL;
    int bOneMDISet = FALSE;
    if ((poItem = poInfoObjDict->Get("Author")) != NULL &&
            poItem->GetType() == PDFObjectType_String)
    {
        SetMetadataItem("AUTHOR", poItem->GetString().c_str());
        bOneMDISet = TRUE;
    }
    if ((poItem = poInfoObjDict->Get("Creator")) != NULL &&
            poItem->GetType() == PDFObjectType_String)
    {
        SetMetadataItem("CREATOR", poItem->GetString().c_str());
        bOneMDISet = TRUE;
    }
    if ((poItem = poInfoObjDict->Get("Keywords")) != NULL &&
            poItem->GetType() == PDFObjectType_String)
    {
        SetMetadataItem("KEYWORDS", poItem->GetString().c_str());
        bOneMDISet = TRUE;
    }
    if ((poItem = poInfoObjDict->Get("Subject")) != NULL &&
            poItem->GetType() == PDFObjectType_String)
    {
        SetMetadataItem("SUBJECT", poItem->GetString().c_str());
        bOneMDISet = TRUE;
    }
    if ((poItem = poInfoObjDict->Get("Title")) != NULL &&
            poItem->GetType() == PDFObjectType_String)
    {
        SetMetadataItem("TITLE", poItem->GetString().c_str());
        bOneMDISet = TRUE;
    }
    if ((poItem = poInfoObjDict->Get("Producer")) != NULL &&
            poItem->GetType() == PDFObjectType_String)
    {
        if (bOneMDISet || poItem->GetString() != "PoDoFo - http://podofo.sf.net")
        {
            SetMetadataItem("PRODUCER", poItem->GetString().c_str());
            bOneMDISet = TRUE;
        }
    }
    if ((poItem = poInfoObjDict->Get("CreationDate")) != NULL &&
        poItem->GetType() == PDFObjectType_String)
    {
        if (bOneMDISet)
            SetMetadataItem("CREATION_DATE", poItem->GetString().c_str());
    }
}

/************************************************************************/
/*                           PDFSanitizeLayerName()                     */
/************************************************************************/

static
CPLString PDFSanitizeLayerName(const char* pszName)
{
    CPLString osName;
    for(int i=0; pszName[i] != '\0'; i++)
    {
        if (pszName[i] == ' ' || pszName[i] == '.' || pszName[i] == ',')
            osName += "_";
        else
            osName += pszName[i];
    }
    return osName;
}

#ifdef HAVE_POPPLER

/************************************************************************/
/*                               AddLayer()                             */
/************************************************************************/

void PDFDataset::AddLayer(const char* pszLayerName, OptionalContentGroup* ocg)
{
    int nNewIndex = osLayerList.size() /*/ 2*/;

    if (nNewIndex == 100)
    {
        CPLStringList osNewLayerList;
        for(int i=0;i<100;i++)
        {
            osNewLayerList.AddNameValue(CPLSPrintf("LAYER_%03d_NAME", i),
                                        osLayerList[/*2 * */ i] + strlen("LAYER_00_NAME="));
            /*osNewLayerList.AddNameValue(CPLSPrintf("LAYER_%03d_INIT_STATE", i),
                                        osLayerList[i * 2 + 1] + strlen("LAYER_00_INIT_STATE="));*/
        }
        osLayerList = osNewLayerList;
    }

    char szFormatName[64];
    /* char szFormatInitState[64]; */
    sprintf(szFormatName, "LAYER_%%0%dd_NAME",  nNewIndex >= 100 ? 3 : 2);
    /* sprintf(szFormatInitState, "LAYER_%%0%dd_INIT_STATE", nNewIndex >= 100 ? 3 : 2); */

    osLayerList.AddNameValue(CPLSPrintf(szFormatName, nNewIndex),
                             pszLayerName);
    /*osLayerList.AddNameValue(CPLSPrintf(szFormatInitState, nNewIndex),
                             (ocg == NULL || ocg->getState() == OptionalContentGroup::On) ? "ON" : "OFF");*/
    oLayerOCGMap[pszLayerName] = ocg;

    //if (ocg != NULL && ocg->getState() == OptionalContentGroup::Off)
    //    bUseOCG = TRUE;
}

/************************************************************************/
/*                             ExploreLayers()                          */
/************************************************************************/

void PDFDataset::ExploreLayers(GDALPDFArray* poArray, CPLString osTopLayer)
{
    int nLength = poArray->GetLength();
    CPLString osCurLayer;
    for(int i=0;i<nLength;i++)
    {
        GDALPDFObject* poObj = poArray->Get(i);
        if (i == 0 && poObj->GetType() == PDFObjectType_String)
        {
            CPLString osName = PDFSanitizeLayerName(poObj->GetString().c_str());
            if (osTopLayer.size())
                osTopLayer = osTopLayer + "." + osName;
            else
                osTopLayer = osName;
            AddLayer(osTopLayer.c_str(), NULL);
        }
        else if (poObj->GetType() == PDFObjectType_Array)
        {
            ExploreLayers(poObj->GetArray(), osCurLayer);
            osCurLayer = "";
        }
        else if (poObj->GetType() == PDFObjectType_Dictionary)
        {
            GDALPDFDictionary* poDict = poObj->GetDictionary();
            GDALPDFObject* poName = poDict->Get("Name");
            if (poName != NULL && poName->GetType() == PDFObjectType_String)
            {
                CPLString osName = PDFSanitizeLayerName(poName->GetString().c_str());
                if (osTopLayer.size())
                    osCurLayer = osTopLayer + "." + osName;
                else
                    osCurLayer = osName;
                //CPLDebug("PDF", "Layer %s", osCurLayer.c_str());

                OCGs* optContentConfig = poDocPoppler->getOptContentConfig();
                struct Ref r;
                r.num = poObj->GetRefNum();
                r.gen = poObj->GetRefGen();
                OptionalContentGroup* ocg = optContentConfig->findOcgByRef(r);
                if (ocg)
                {
                    AddLayer(osCurLayer.c_str(), ocg);
                    osLayerWithRefList.AddString(
                        CPLSPrintf("%s %d %d", osCurLayer.c_str(), r.num, r.gen));
                }
            }
        }
    }
}

/************************************************************************/
/*                              FindLayers()                            */
/************************************************************************/

void PDFDataset::FindLayers()
{
    OCGs* optContentConfig = poDocPoppler->getOptContentConfig();
    if (optContentConfig == NULL || !optContentConfig->isOk() )
        return;

    Array* array = optContentConfig->getOrderArray();
    if (array)
    {
        GDALPDFArray* poArray = GDALPDFCreateArray(array);
        ExploreLayers(poArray);
        delete poArray;
    }
    else
    {
        GooList* ocgList = optContentConfig->getOCGs();
        for(int i=0;i<ocgList->getLength();i++)
        {
            OptionalContentGroup* ocg = (OptionalContentGroup*) ocgList->get(i);
            AddLayer((const char*)ocg->getName()->getCString(), ocg);
        }
    }

    oMDMD.SetMetadata(osLayerList.List(), "LAYERS");
}

/************************************************************************/
/*                            TurnLayersOnOff()                         */
/************************************************************************/

void PDFDataset::TurnLayersOnOff()
{
    OCGs* optContentConfig = poDocPoppler->getOptContentConfig();
    if (optContentConfig == NULL || !optContentConfig->isOk() )
        return;

    // Which layers to turn ON ?
    const char* pszLayers = CPLGetConfigOption("GDAL_PDF_LAYERS", NULL);
    if (pszLayers)
    {
        int i;
        int bAll = EQUAL(pszLayers, "ALL");
        GooList* ocgList = optContentConfig->getOCGs();
        for(i=0;i<ocgList->getLength();i++)
        {
            OptionalContentGroup* ocg = (OptionalContentGroup*) ocgList->get(i);
            ocg->setState( (bAll) ? OptionalContentGroup::On : OptionalContentGroup::Off );
        }

        char** papszLayers = CSLTokenizeString2(pszLayers, ",", 0);
        for(i=0;!bAll && papszLayers[i] != NULL;i++)
        {
            std::map<CPLString, OptionalContentGroup*>::iterator oIter =
                oLayerOCGMap.find(papszLayers[i]);
            if (oIter != oLayerOCGMap.end())
            {
                if (oIter->second)
                {
                    //CPLDebug("PDF", "Turn '%s' on", papszLayers[i]);
                    oIter->second->setState(OptionalContentGroup::On);
                }

                // Turn child layers on, unless there's one of them explicitely listed
                // in the list.
                int nLen = strlen(papszLayers[i]);
                int bFoundChildLayer = FALSE;
                oIter = oLayerOCGMap.begin();
                for( ; oIter != oLayerOCGMap.end() && !bFoundChildLayer; oIter ++)
                {
                    if ((int)oIter->first.size() > nLen &&
                        strncmp(oIter->first.c_str(), papszLayers[i], nLen) == 0 &&
                        oIter->first[nLen] == '.')
                    {
                        for(int j=0;papszLayers[j] != NULL;j++)
                        {
                            if (strcmp(papszLayers[j], oIter->first.c_str()) == 0)
                                bFoundChildLayer = TRUE;
                        }
                    }
                }

                if( !bFoundChildLayer )
                {
                    oIter = oLayerOCGMap.begin();
                    for( ; oIter != oLayerOCGMap.end() && !bFoundChildLayer; oIter ++)
                    {
                        if ((int)oIter->first.size() > nLen &&
                            strncmp(oIter->first.c_str(), papszLayers[i], nLen) == 0 &&
                            oIter->first[nLen] == '.')
                        {
                            if (oIter->second)
                            {
                                //CPLDebug("PDF", "Turn '%s' on too", oIter->first.c_str());
                                oIter->second->setState(OptionalContentGroup::On);
                            }
                        }
                    }
                }

                // Turn parent layers on too
                char* pszLastDot;
                while( (pszLastDot = strrchr(papszLayers[i], '.')) != NULL)
                {
                    *pszLastDot = '\0';
                    oIter = oLayerOCGMap.find(papszLayers[i]);
                    if (oIter != oLayerOCGMap.end())
                    {
                        if (oIter->second)
                        {
                            //CPLDebug("PDF", "Turn '%s' on too", papszLayers[i]);
                            oIter->second->setState(OptionalContentGroup::On);
                        }
                    }
                }
            }
            else
            {
                CPLError(CE_Warning, CPLE_AppDefined, "Unknown layer '%s'",
                            papszLayers[i]);
            }
        }
        CSLDestroy(papszLayers);

        bUseOCG = TRUE;
    }

    // Which layers to turn OFF ?
    const char* pszLayersOFF = CPLGetConfigOption("GDAL_PDF_LAYERS_OFF", NULL);
    if (pszLayersOFF)
    {
        char** papszLayersOFF = CSLTokenizeString2(pszLayersOFF, ",", 0);
        for(int i=0;papszLayersOFF[i] != NULL;i++)
        {
            std::map<CPLString, OptionalContentGroup*>::iterator oIter =
                oLayerOCGMap.find(papszLayersOFF[i]);
            if (oIter != oLayerOCGMap.end())
            {
                if (oIter->second)
                {
                    //CPLDebug("PDF", "Turn '%s' off", papszLayersOFF[i]);
                    oIter->second->setState(OptionalContentGroup::Off);
                }

                // Turn child layers off too
                int nLen = strlen(papszLayersOFF[i]);
                oIter = oLayerOCGMap.begin();
                for( ; oIter != oLayerOCGMap.end(); oIter ++)
                {
                    if ((int)oIter->first.size() > nLen &&
                        strncmp(oIter->first.c_str(), papszLayersOFF[i], nLen) == 0 &&
                        oIter->first[nLen] == '.')
                    {
                        if (oIter->second)
                        {
                            //CPLDebug("PDF", "Turn '%s' off too", oIter->first.c_str());
                            oIter->second->setState(OptionalContentGroup::Off);
                        }
                    }
                }
            }
            else
            {
                CPLError(CE_Warning, CPLE_AppDefined, "Unknown layer '%s'",
                            papszLayersOFF[i]);
            }
        }
        CSLDestroy(papszLayersOFF);

        bUseOCG = TRUE;
    }
}

#endif

/************************************************************************/
/*                         FindLayersGeneric()                          */
/************************************************************************/

void PDFDataset::FindLayersGeneric(GDALPDFDictionary* poPageDict)
{
    GDALPDFObject* poResources = poPageDict->Get("Resources");
    if (poResources != NULL &&
        poResources->GetType() == PDFObjectType_Dictionary)
    {
        GDALPDFObject* poProperties =
            poResources->GetDictionary()->Get("Properties");
        if (poProperties != NULL &&
            poProperties->GetType() == PDFObjectType_Dictionary)
        {
            std::map<CPLString, GDALPDFObject*>& oMap =
                                    poProperties->GetDictionary()->GetValues();
            std::map<CPLString, GDALPDFObject*>::iterator oIter = oMap.begin();
            std::map<CPLString, GDALPDFObject*>::iterator oEnd = oMap.end();

            for(; oIter != oEnd; ++oIter)
            {
                GDALPDFObject* poObj = oIter->second;
                if( poObj->GetRefNum() != 0 && poObj->GetType() == PDFObjectType_Dictionary )
                {
                    GDALPDFObject* poType = poObj->GetDictionary()->Get("Type");
                    GDALPDFObject* poName = poObj->GetDictionary()->Get("Name");
                    if( poType != NULL &&
                        poType->GetType() == PDFObjectType_Name &&
                        poType->GetName() == "OCG" &&
                        poName != NULL &&
                        poName->GetType() == PDFObjectType_String )
                    {
                        osLayerWithRefList.AddString(
                            CPLSPrintf("%s %d %d",
                                       PDFSanitizeLayerName(poName->GetString()).c_str(),
                                       poObj->GetRefNum(),
                                       poObj->GetRefGen()));
                    }
                }
            }
        }
    }
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
    int bOpenSubdatasetImage = strncmp(poOpenInfo->pszFilename, "PDF_IMAGE:", 10) == 0;
    int iPage = -1;
    int nImageNum = -1;
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
    else if (bOpenSubdatasetImage)
    {
        iPage = atoi(pszFilename + 10);
        if (iPage <= 0)
            return NULL;
        const char* pszNext = strchr(pszFilename + 10, ':');
        if (pszNext == NULL)
            return NULL;
        nImageNum = atoi(pszNext + 1);
        if (nImageNum <= 0)
            return NULL;
        pszFilename = strchr(pszNext + 1, ':');
        if (pszFilename == NULL)
            return NULL;
        pszFilename ++;
    }
    else
        iPage = 1;

    int bUsePoppler;
#if defined(HAVE_POPPLER) && !defined(HAVE_PODOFO)
    bUsePoppler = TRUE;
#elif !defined(HAVE_POPPLER) && defined(HAVE_PODOFO)
    bUsePoppler = FALSE;
#elif defined(HAVE_POPPLER) && defined(HAVE_PODOFO)
    const char* pszPDFLib = CPLGetConfigOption("GDAL_PDF_LIB", "POPPLER");
    if (EQUAL(pszPDFLib, "POPPLER"))
        bUsePoppler = TRUE;
    else if (EQUAL(pszPDFLib, "PODOFO"))
        bUsePoppler = FALSE;
    else
    {
        CPLDebug("PDF", "Invalid value for GDAL_PDF_LIB config option");
        bUsePoppler = TRUE;
    }
#else
    return NULL;
#endif

    GDALPDFObject* poPageObj = NULL;
#ifdef HAVE_POPPLER
    PDFDoc* poDocPoppler = NULL;
    ObjectAutoFree oObj;
    Page* poPagePoppler = NULL;
    Catalog* poCatalogPoppler = NULL;
#endif
#ifdef HAVE_PODOFO
    PoDoFo::PdfMemDocument* poDocPodofo = NULL;
    PoDoFo::PdfPage* poPagePodofo = NULL;
#endif
    int nPages = 0;

#ifdef HAVE_POPPLER
  if(bUsePoppler)
  {
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

    while(TRUE)
    {
        VSILFILE* fp = VSIFOpenL(pszFilename, "rb");
        if (fp == NULL)
            return NULL;

        fp = (VSILFILE*)VSICreateBufferedReaderHandle((VSIVirtualHandle*)fp);

        if (pszUserPwd)
            poUserPwd = new GooString(pszUserPwd);

        oObj.initNull();
        poDocPoppler = new PDFDoc(new VSIPDFFileStream(fp, pszFilename, 0, gFalse, 0, &oObj), NULL, poUserPwd);
        delete poUserPwd;

        if ( !poDocPoppler->isOk() || poDocPoppler->getNumPages() == 0 )
        {
            if (poDocPoppler->getErrorCode() == errEncrypted)
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
                    PDFFreeDoc(poDocPoppler);
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

            PDFFreeDoc(poDocPoppler);

            return NULL;
        }
        else
            break;
    }

    poCatalogPoppler = poDocPoppler->getCatalog();
    if ( poCatalogPoppler == NULL || !poCatalogPoppler->isOk() )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Invalid PDF : invalid catalog");
        PDFFreeDoc(poDocPoppler);
        return NULL;
    }

#ifdef dump_catalog
    {
        ObjectAutoFree oCatalog;
        poDocPoppler->getXRef()->getCatalog(&oCatalog);
        GDALPDFObjectPoppler oCatalogGDAL(&oCatalog, FALSE);
        GDALPDFDumper oDumper(stderr);
        oDumper.Dump(&oCatalogGDAL);
    }
#endif

    nPages = poDocPoppler->getNumPages();
    if (iPage < 1 || iPage > nPages)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Invalid page number (%d/%d)",
                 iPage, nPages);
        PDFFreeDoc(poDocPoppler);
        return NULL;
    }

    poPagePoppler = poCatalogPoppler->getPage(iPage);
    if ( poPagePoppler == NULL || !poPagePoppler->isOk() )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Invalid PDF : invalid page");
        PDFFreeDoc(poDocPoppler);
        return NULL;
    }

    /* Here's the dirty part: this is a private member */
    /* so we had to #define private public to get it ! */
    Object& oPageObj = poPagePoppler->pageObj;
    if ( !oPageObj.isDict() )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Invalid PDF : !oPageObj.isDict()");
        PDFFreeDoc(poDocPoppler);
        return NULL;
    }

    poPageObj = new GDALPDFObjectPoppler(&oPageObj, FALSE);
    Ref* poPageRef = poCatalogPoppler->getPageRef(iPage);
    if (poPageRef != NULL)
    {
        ((GDALPDFObjectPoppler*)poPageObj)->SetRefNumAndGen(poPageRef->num, poPageRef->gen);
    }
  }
#endif

#ifdef HAVE_PODOFO
  if (!bUsePoppler)
  {
    PoDoFo::PdfError::EnableDebug( false );
    PoDoFo::PdfError::EnableLogging( false );

    poDocPodofo = new PoDoFo::PdfMemDocument();
    try
    {
        poDocPodofo->Load(pszFilename);
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
                    poDocPodofo->SetPassword(pszUserPwd);
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
                    delete poDocPodofo;
                    return NULL;
                }
                catch(...)
                {
                    CPLError(CE_Failure, CPLE_AppDefined, "Invalid PDF");
                    delete poDocPodofo;
                    return NULL;
                }
            }
            else
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                            "A password is needed. You can specify it through the PDF_USER_PWD "
                            "configuration option (that can be set to ASK_INTERACTIVE)");
                delete poDocPodofo;
                return NULL;
            }
        }
        else
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Invalid PDF : %s", oError.what());
            delete poDocPodofo;
            return NULL;
        }
    }
    catch(...)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Invalid PDF");
        delete poDocPodofo;
        return NULL;
    }

    nPages = poDocPodofo->GetPageCount();
    if (iPage < 1 || iPage > nPages)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Invalid page number (%d/%d)",
                 iPage, nPages);
        delete poDocPodofo;
        return NULL;
    }

    poPagePodofo = poDocPodofo->GetPage(iPage - 1);
    if ( poPagePodofo == NULL )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Invalid PDF : invalid page");
        delete poDocPodofo;
        return NULL;
    }

    PoDoFo::PdfObject* pObj = poPagePodofo->GetObject();
    poPageObj = new GDALPDFObjectPodofo(pObj, poDocPodofo->GetObjects());
  }
#endif

    GDALPDFDictionary* poPageDict = poPageObj->GetDictionary();
    if ( poPageDict == NULL )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Invalid PDF : poPageDict == NULL");
#ifdef HAVE_POPPLER
        PDFFreeDoc(poDocPoppler);
#endif
#ifdef HAVE_PODOFO
        delete poDocPodofo;
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

        GDALPDFDumper oDumper(f);
        oDumper.Dump(poPageObj);
        if (f != stderr)
            fclose(f);
    }

    PDFDataset* poDS = new PDFDataset();
    poDS->bUsePoppler = bUsePoppler;
    poDS->osFilename = pszFilename;
    poDS->eAccess = poOpenInfo->eAccess;

    if ( nPages > 1 && !bOpenSubdataset )
    {
        int i;
        CPLStringList aosList;
        for(i=0;i<nPages;i++)
        {
            char szKey[32];
            sprintf( szKey, "SUBDATASET_%d_NAME", i+1 );
            aosList.AddNameValue(szKey, CPLSPrintf("PDF:%d:%s", i+1, poOpenInfo->pszFilename));
            sprintf( szKey, "SUBDATASET_%d_DESC", i+1 );
            aosList.AddNameValue(szKey, CPLSPrintf("Page %d of %s", i+1, poOpenInfo->pszFilename));
        }
        poDS->SetMetadata( aosList.List(), "SUBDATASETS" );
    }

#ifdef HAVE_POPPLER
    poDS->poDocPoppler = poDocPoppler;
#endif
#ifdef HAVE_PODOFO
    poDS->poDocPodofo = poDocPodofo;
#endif
    poDS->poPageObj = poPageObj;
    poDS->osUserPwd = pszUserPwd ? pszUserPwd : "";
    poDS->iPage = iPage;

    int nBandsGuessed = 0;
    if (nImageNum < 0)
    {
        poDS->GuessDPI(poPageDict, &nBandsGuessed);
        nBandsGuessed = 0;
    }

    double dfX1 = 0.0, dfY1 = 0.0, dfX2 = 0.0, dfY2 = 0.0;

#ifdef HAVE_POPPLER
    if (bUsePoppler)
    {
        PDFRectangle* psMediaBox = poPagePoppler->getMediaBox();
        dfX1 = psMediaBox->x1;
        dfY1 = psMediaBox->y1;
        dfX2 = psMediaBox->x2;
        dfY2 = psMediaBox->y2;
    }
#endif

#ifdef HAVE_PODOFO
    if (!bUsePoppler)
    {
        PoDoFo::PdfRect oMediaBox = poPagePodofo->GetMediaBox();
        dfX1 = oMediaBox.GetLeft();
        dfY1 = oMediaBox.GetBottom();
        dfX2 = dfX1 + oMediaBox.GetWidth();
        dfY2 = dfY1 + oMediaBox.GetHeight();
    }
#endif

    double dfUserUnit = poDS->dfDPI / 72.0;
    poDS->nRasterXSize = (int) ((dfX2 - dfX1) * dfUserUnit);
    poDS->nRasterYSize = (int) ((dfY2 - dfY1) * dfUserUnit);

    double dfRotation = 0;
#ifdef HAVE_POPPLER
    if (bUsePoppler)
        dfRotation = poDocPoppler->getPageRotate(iPage);
#endif

#ifdef HAVE_PODOFO
    if (!bUsePoppler)
        dfRotation = poPagePodofo->GetRotation();
#endif
    if ( dfRotation == 90 ||
         dfRotation == 270 )
    {
/* FIXME: the non poppler case should be implemented. This needs to rotate */
/* the output of pdftoppm */
#ifdef HAVE_POPPLER
      if (bUsePoppler)
      {
        /* Wondering how it would work with a georeferenced image */
        /* Has only been tested with ungeoreferenced image */
        int nTmp = poDS->nRasterXSize;
        poDS->nRasterXSize = poDS->nRasterYSize;
        poDS->nRasterYSize = nTmp;
      }
#endif
    }

    GDALPDFObject* poLGIDict = NULL;
    GDALPDFObject* poVP = NULL;
    int bIsOGCBP = FALSE;
    if ( (poLGIDict = poPageDict->Get("LGIDict")) != NULL && nImageNum < 0 )
    {
        /* Cf 08-139r3_GeoPDF_Encoding_Best_Practice_Version_2.2.pdf */
        CPLDebug("PDF", "OGC Encoding Best Practice style detected");
        if (poDS->ParseLGIDictObject(poLGIDict))
        {
            if (poDS->bHasCTM)
            {
                poDS->adfGeoTransform[0] = poDS->adfCTM[4] + poDS->adfCTM[0] * dfX1 + poDS->adfCTM[2] * dfY2;
                poDS->adfGeoTransform[1] = poDS->adfCTM[0] / dfUserUnit;
                poDS->adfGeoTransform[2] = poDS->adfCTM[1] / dfUserUnit;
                poDS->adfGeoTransform[3] = poDS->adfCTM[5] + poDS->adfCTM[1] * dfX1 + poDS->adfCTM[3] * dfY2;
                poDS->adfGeoTransform[4] = - poDS->adfCTM[2] / dfUserUnit;
                poDS->adfGeoTransform[5] = - poDS->adfCTM[3] / dfUserUnit;
                poDS->bGeoTransformValid = TRUE;
            }

            bIsOGCBP = TRUE;

            int i;
            for(i=0;i<poDS->nGCPCount;i++)
            {
                poDS->pasGCPList[i].dfGCPPixel = poDS->pasGCPList[i].dfGCPPixel * dfUserUnit;
                poDS->pasGCPList[i].dfGCPLine = poDS->nRasterYSize - poDS->pasGCPList[i].dfGCPLine * dfUserUnit;
            }
        }
    }
    else if ( (poVP = poPageDict->Get("VP")) != NULL && nImageNum < 0 )
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
        GDALPDFObject* poResources = poPageDict->Get("Resources");
        GDALPDFObject* poXObject = NULL;
        if (poResources != NULL &&
            poResources->GetType() == PDFObjectType_Dictionary)
            poXObject = poResources->GetDictionary()->Get("XObject");

        if (poXObject != NULL &&
            poXObject->GetType() == PDFObjectType_Dictionary)
        {
            GDALPDFDictionary* poXObjectDict = poXObject->GetDictionary();
            std::map<CPLString, GDALPDFObject*>& oMap = poXObjectDict->GetValues();
            std::map<CPLString, GDALPDFObject*>::iterator oMapIter = oMap.begin();
            int nSubDataset = 0;
            while(oMapIter != oMap.end())
            {
                GDALPDFObject* poObj = oMapIter->second;
                if (poObj->GetType() == PDFObjectType_Dictionary)
                {
                    GDALPDFDictionary* poDict = poObj->GetDictionary();
                    GDALPDFObject* poSubtype = NULL;
                    GDALPDFObject* poMeasure = NULL;
                    GDALPDFObject* poWidth = NULL;
                    GDALPDFObject* poHeight = NULL;
                    int nW = 0;
                    int nH = 0;
                    if ((poSubtype = poDict->Get("Subtype")) != NULL &&
                        poSubtype->GetType() == PDFObjectType_Name &&
                        poSubtype->GetName() == "Image" &&
                        (poMeasure = poDict->Get("Measure")) != NULL &&
                        poMeasure->GetType() == PDFObjectType_Dictionary &&
                        (poWidth = poDict->Get("Width")) != NULL &&
                        poWidth->GetType() == PDFObjectType_Int &&
                        (nW = poWidth->GetInt()) > 0 &&
                        (poHeight = poDict->Get("Height")) != NULL &&
                        poHeight->GetType() == PDFObjectType_Int &&
                        (nH = poHeight->GetInt()) > 0 )
                    {
                        if (nImageNum < 0)
                            CPLDebug("PDF", "Measure found on Image object (%d)",
                                     poObj->GetRefNum());

                        GDALPDFObject* poColorSpace = poDict->Get("ColorSpace");
                        GDALPDFObject* poBitsPerComponent = poDict->Get("BitsPerComponent");
                        if (poObj->GetRefNum() != 0 &&
                            poObj->GetRefGen() == 0 &&
                            poColorSpace != NULL &&
                            poColorSpace->GetType() == PDFObjectType_Name &&
                            (poColorSpace->GetName() == "DeviceGray" ||
                             poColorSpace->GetName() == "DeviceRGB") &&
                            (poBitsPerComponent == NULL ||
                             (poBitsPerComponent->GetType() == PDFObjectType_Int &&
                              poBitsPerComponent->GetInt() == 8)))
                        {
                            if (nImageNum < 0)
                            {
                                nSubDataset ++;
                                poDS->SetMetadataItem(CPLSPrintf("SUBDATASET_%d_NAME",
                                                                 nSubDataset),
                                                      CPLSPrintf("PDF_IMAGE:%d:%d:%s",
                                                                 iPage, poObj->GetRefNum(), pszFilename),
                                                      "SUBDATASETS");
                                poDS->SetMetadataItem(CPLSPrintf("SUBDATASET_%d_DESC",
                                                                 nSubDataset),
                                                      CPLSPrintf("Georeferenced image of size %dx%d of page %d of %s",
                                                                 nW, nH, iPage, pszFilename),
                                                      "SUBDATASETS");
                            }
                            else if (poObj->GetRefNum() == nImageNum)
                            {
                                poDS->nRasterXSize = nW;
                                poDS->nRasterYSize = nH;
                                poDS->ParseMeasure(poMeasure, nW, nH, 0, nH, nW, 0);
                                poDS->poImageObj = poObj;
                                if (poColorSpace->GetName() == "DeviceGray")
                                    nBandsGuessed = 1;
                                break;
                            }
                        }
                    }
                }
                ++ oMapIter;
            }
        }

        if (nImageNum >= 0 && poDS->poImageObj == NULL)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Cannot find image %d", nImageNum);
            delete poDS;
            return NULL;
        }

        /* Not a geospatial PDF doc */
    }

    /* If pixel size or top left coordinates are very close to an int, round them to the int */
    double dfEps = ( fabs(poDS->adfGeoTransform[0]) > 1e5 &&
                     fabs(poDS->adfGeoTransform[3]) > 1e5 ) ? 1e-5 : 1e-8;
    poDS->adfGeoTransform[0] = ROUND_TO_INT_IF_CLOSE(poDS->adfGeoTransform[0], dfEps);
    poDS->adfGeoTransform[1] = ROUND_TO_INT_IF_CLOSE(poDS->adfGeoTransform[1]);
    poDS->adfGeoTransform[3] = ROUND_TO_INT_IF_CLOSE(poDS->adfGeoTransform[3], dfEps);
    poDS->adfGeoTransform[5] = ROUND_TO_INT_IF_CLOSE(poDS->adfGeoTransform[5]);

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
                double x = poRing->getX(i) * dfUserUnit;
                double y = poDS->nRasterYSize - poRing->getY(i) * dfUserUnit;
                double X = poDS->adfGeoTransform[0] + x * poDS->adfGeoTransform[1] +
                                                      y * poDS->adfGeoTransform[2];
                double Y = poDS->adfGeoTransform[3] + x * poDS->adfGeoTransform[4] +
                                                      y * poDS->adfGeoTransform[5];
                poRing->setPoint(i, X, Y);
            }
        }
        poRing->closeRings();

        poDS->poNeatLine->exportToWkt(&pszNeatLineWkt);
        if (nImageNum < 0)
            poDS->SetMetadataItem("NEATLINE", pszNeatLineWkt);
        CPLFree(pszNeatLineWkt);
    }


#ifdef HAVE_POPPLER
  if (bUsePoppler)
  {
    GooString* poMetadata = poCatalogPoppler->readMetadata();
    if (poMetadata)
    {
        char* pszContent = poMetadata->getCString();
        if (pszContent != NULL &&
            strncmp(pszContent, "<?xpacket begin=", strlen("<?xpacket begin=")) == 0)
        {
            char *apszMDList[2];
            apszMDList[0] = pszContent;
            apszMDList[1] = NULL;
            poDS->SetMetadata(apszMDList, "xml:XMP");
        }
        delete poMetadata;
    }

    /* Read Info object */
    Object oInfo;
    poDocPoppler->getDocInfo(&oInfo);
    GDALPDFObjectPoppler oInfoObjPoppler(&oInfo, FALSE);
    poDS->ParseInfo(&oInfoObjPoppler);
    oInfo.free();

    /* Find layers */
    poDS->FindLayers();

    /* Turn user specified layers on or off */
    poDS->TurnLayersOnOff();
  }
#endif

#ifdef HAVE_PODOFO
  if (!bUsePoppler)
  {
    PoDoFo::TIVecObjects it = poDocPodofo->GetObjects().begin();
    for( ; it != poDocPodofo->GetObjects().end(); ++it )
    {
        GDALPDFObjectPodofo oObjPodofo((*it), poDocPodofo->GetObjects());
        poDS->FindXMP(&oObjPodofo);
    }

    /* Find layers */
    poDS->FindLayersGeneric(poPageDict);

    /* Read Info object */
    PoDoFo::PdfInfo* poInfo = poDocPodofo->GetInfo();
    if (poInfo != NULL)
    {
        GDALPDFObjectPodofo oInfoObjPodofo(poInfo->GetObject(), poDocPodofo->GetObjects());
        poDS->ParseInfo(&oInfoObjPodofo);
    }
  }
#endif

    int nBands = 3;
    if( nBandsGuessed )
        nBands = nBandsGuessed;
    const char* pszPDFBands = CPLGetConfigOption("GDAL_PDF_BANDS", NULL);
    if( pszPDFBands )
    {
        nBands = atoi(pszPDFBands);
        if (nBands != 3 && nBands != 4)
        {
            CPLError(CE_Warning, CPLE_NotSupported,
                    "Invalid value for GDAL_PDF_BANDS. Using 3 as a fallback");
            nBands = 3;
        }
    }
#ifdef HAVE_PODOFO
    if (!bUsePoppler && nBands == 4)
    {
        CPLError(CE_Warning, CPLE_NotSupported,
                 "GDAL_PDF_BANDS=4 only supported when PDF driver is compiled against Poppler. "
                 "Using 3 as a fallback");
        nBands = 3;
    }
#endif

    int iBand;
    for(iBand = 1; iBand <= nBands; iBand ++)
    {
        if (poDS->poImageObj != NULL)
            poDS->SetBand(iBand, new PDFImageRasterBand(poDS, iBand));
        else
            poDS->SetBand(iBand, new PDFRasterBand(poDS, iBand));
    }

/* -------------------------------------------------------------------- */
/*      Initialize any PAM information.                                 */
/* -------------------------------------------------------------------- */
    poDS->SetDescription( poOpenInfo->pszFilename );
    poDS->TryLoadXML();

/* -------------------------------------------------------------------- */
/*      Support overviews.                                              */
/* -------------------------------------------------------------------- */
    poDS->oOvManager.Initialize( poDS, poOpenInfo->pszFilename );

    /* Clear dirty flag */
    poDS->bProjDirty = FALSE;
    poDS->bNeatLineDirty = FALSE;
    poDS->bInfoDirty = FALSE;
    poDS->bXMPDirty = FALSE;

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
/*                            Get()                                     */
/************************************************************************/

static double Get(GDALPDFObject* poObj, int nIndice)
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
    bHasCTM = FALSE;
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
#ifdef not_supported
        if (poProjDict->Get("StandardParallelOne") == NULL)
        {
#endif
        double dfCenterLat = Get(poProjDict, "OriginLatitude");
        double dfCenterLong = Get(poProjDict, "CentralMeridian");
        double dfScale = Get(poProjDict, "ScaleFactor");
        double dfFalseEasting = Get(poProjDict, "FalseEasting");
        double dfFalseNorthing = Get(poProjDict, "FalseNorthing");
        oSRS.SetMercator( dfCenterLat, dfCenterLong,
                          dfScale,
                          dfFalseEasting, dfFalseNorthing );
#ifdef not_supported
        }
        else
        {
            double dfStdP1 = Get(poProjDict, "StandardParallelOne");
            double dfCenterLat = poProjDict->Get("OriginLatitude") ? Get(poProjDict, "OriginLatitude") : 0;
            double dfCenterLong = Get(poProjDict, "CentralMeridian");
            double dfFalseEasting = Get(poProjDict, "FalseEasting");
            double dfFalseNorthing = Get(poProjDict, "FalseNorthing");
            oSRS.SetMercator2SP( dfStdP1,
                                 dfCenterLat, dfCenterLong,
                                 dfFalseEasting, dfFalseNorthing );
        }
#endif
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

    int bRet = ParseMeasure(poMeasure, dfMediaBoxWidth, dfMediaBoxHeight,
                            dfULX, dfULY, dfLRX, dfLRY);

/* -------------------------------------------------------------------- */
/*      Extract PointData attribute                                     */
/* -------------------------------------------------------------------- */
    GDALPDFObject* poPointData;
    if( (poPointData = poVPEltDict->Get("PtData")) != NULL &&
        poPointData->GetType() == PDFObjectType_Dictionary )
    {
        CPLDebug("PDF", "Found PointData");
    }

    return bRet;
}

/************************************************************************/
/*                           ParseMeasure()                             */
/************************************************************************/

int PDFDataset::ParseMeasure(GDALPDFObject* poMeasure,
                             double dfMediaBoxWidth, double dfMediaBoxHeight,
                             double dfULX, double dfULY, double dfLRX, double dfLRY)
{
    int i;
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
/*      Extract Bounds attribute (optional)                             */
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
        poBounds = NULL;
    }

    if (poBounds != NULL)
    {
        int nBoundsLength = poBounds->GetArray()->GetLength();
        if (nBoundsLength == 8)
        {
            double adfBounds[8];
            for(i=0;i<8;i++)
            {
                adfBounds[i] = Get(poBounds, i);
                CPLDebug("PDF", "Bounds[%d] = %f", i, adfBounds[i]);
            }

            // TODO we should use it to restrict the neatline but
            // I have yet to set a sample where bounds are not the four
            // corners of the unit square.
        }
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
    GDALPDFObject* poGCSWKT = poGCSDict->Get("WKT");
    if( poGCSWKT != NULL &&
        poGCSWKT->GetType() != PDFObjectType_String )
    {
        poGCSWKT = NULL;
    }

    if (poGCSWKT != NULL)
        CPLDebug("PDF", "GCS.WKT = %s", poGCSWKT->GetString().c_str());

    if (nEPSGCode <= 0 && poGCSWKT == NULL)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot find GCS.WKT or GCS.EPSG objects");
        return FALSE;
    }

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
        if (poGCSWKT == NULL)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                    "Cannot resolve EPSG object, and GCS.WKT not found");
            return FALSE;
        }

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

    /* Files found at http://carto.iict.ch/blog/publications-cartographiques-au-format-geospatial-pdf/ */
    /* are in a PROJCS. However the coordinates in GPTS array are not in (lat, long) as required by the */
    /* ISO 32000 supplement spec, but in (northing, easting). Adobe reader is able to understand that, */
    /* so let's also try to do it with a heuristics. */

    int bReproject = TRUE;
    if (oSRS.IsProjected() &&
        (fabs(adfGPTS[0]) > 91 || fabs(adfGPTS[2]) > 91 || fabs(adfGPTS[4]) > 91 || fabs(adfGPTS[6]) > 91 ||
         fabs(adfGPTS[1]) > 361 || fabs(adfGPTS[3]) > 361 || fabs(adfGPTS[5]) > 361 || fabs(adfGPTS[7]) > 361))
    {
        CPLDebug("PDF", "GPTS coordinates seems to be in (northing, easting), which is non-standard");
        bReproject = FALSE;
    }

    OGRCoordinateTransformation* poCT = NULL;
    if (bReproject)
    {
        poCT = OGRCreateCoordinateTransformation( poSRSGeog, &oSRS);
        if (poCT == NULL)
        {
            delete poSRSGeog;
            CPLFree(pszWKT);
            pszWKT = NULL;
            return FALSE;
        }
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
        if (bReproject)
        {
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
        }

        x = ROUND_TO_INT_IF_CLOSE(x);
        y = ROUND_TO_INT_IF_CLOSE(y);

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

    return TRUE;
}

/************************************************************************/
/*                          GetProjectionRef()                          */
/************************************************************************/

const char* PDFDataset::GetProjectionRef()
{
    if (pszWKT && bGeoTransformValid)
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
/*                            SetProjection()                           */
/************************************************************************/

CPLErr PDFDataset::SetProjection(const char* pszWKTIn)
{
    CPLFree(pszWKT);
    pszWKT = pszWKTIn ? CPLStrdup(pszWKTIn) : CPLStrdup("");
    bProjDirty = TRUE;
    return CE_None;
}

/************************************************************************/
/*                          SetGeoTransform()                           */
/************************************************************************/

CPLErr PDFDataset::SetGeoTransform(double* padfGeoTransform)
{
    memcpy(adfGeoTransform, padfGeoTransform, 6 * sizeof(double));
    bGeoTransformValid = TRUE;
    bProjDirty = TRUE;

    /* Reset NEATLINE if not explicitely set by the user */
    if (!bNeatLineDirty)
        SetMetadataItem("NEATLINE", NULL);
    return CE_None;
}

/************************************************************************/
/*                           GetMetadata()                              */
/************************************************************************/

char      **PDFDataset::GetMetadata( const char * pszDomain )
{
    if( pszDomain != NULL && EQUAL(pszDomain, "LAYERS_WITH_REF") )
    {
        return osLayerWithRefList.List(); /* Used by OGR driver */
    }

    return oMDMD.GetMetadata(pszDomain);
}

/************************************************************************/
/*                            SetMetadata()                             */
/************************************************************************/

CPLErr      PDFDataset::SetMetadata( char ** papszMetadata,
                                     const char * pszDomain )
{
    if (pszDomain == NULL || EQUAL(pszDomain, ""))
    {
        if (CSLFindString(papszMetadata, "NEATLINE") != -1)
        {
            bProjDirty = TRUE;
            bNeatLineDirty = TRUE;
        }
        bInfoDirty = TRUE;
    }
    else if (EQUAL(pszDomain, "xml:XMP"))
        bXMPDirty = TRUE;
    return oMDMD.SetMetadata(papszMetadata, pszDomain);
}

/************************************************************************/
/*                          GetMetadataItem()                           */
/************************************************************************/

const char *PDFDataset::GetMetadataItem( const char * pszName,
                                         const char * pszDomain )
{
    if ( (pszDomain == NULL || EQUAL(pszDomain, "")) && EQUAL(pszName, "PDF_PAGE_OBJECT") )
    {
        return CPLSPrintf("%p", poPageObj);
    }
    if ( (pszDomain == NULL || EQUAL(pszDomain, "")) && EQUAL(pszName, "PDF_CATALOG_OBJECT") )
    {
        return CPLSPrintf("%p", GetCatalog());
    }

    return oMDMD.GetMetadataItem(pszName, pszDomain);
}

/************************************************************************/
/*                          SetMetadataItem()                           */
/************************************************************************/

CPLErr      PDFDataset::SetMetadataItem( const char * pszName,
                                         const char * pszValue,
                                         const char * pszDomain )
{
    if (pszDomain == NULL || EQUAL(pszDomain, ""))
    {
        if (EQUAL(pszName, "NEATLINE"))
        {
            bProjDirty = TRUE;
            bNeatLineDirty = TRUE;
        }
        else
        {
            if (pszValue == NULL)
                pszValue = "";
            bInfoDirty = TRUE;
        }
    }
    else if (EQUAL(pszDomain, "xml:XMP"))
        bXMPDirty = TRUE;
    return oMDMD.SetMetadataItem(pszName, pszValue, pszDomain);
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
/*                               SetGCPs()                              */
/************************************************************************/

CPLErr PDFDataset::SetGCPs( int nGCPCountIn, const GDAL_GCP *pasGCPListIn,
                            const char *pszGCPProjectionIn )
{
    const char* pszGEO_ENCODING =
        CPLGetConfigOption("GDAL_PDF_GEO_ENCODING", "ISO32000");
    if( nGCPCountIn != 4 && EQUAL(pszGEO_ENCODING, "ISO32000"))
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "PDF driver only supports writing 4 GCPs when "
                 "GDAL_PDF_GEO_ENCODING=ISO32000.");
        return CE_Failure;
    }
    
    /* Free previous GCPs */
    GDALDeinitGCPs( nGCPCount, pasGCPList );
    CPLFree( pasGCPList );
    
    /* Duplicate in GCPs */
    nGCPCount = nGCPCountIn;
    pasGCPList = GDALDuplicateGCPs(nGCPCount, pasGCPListIn);
    
    CPLFree(pszWKT);
    pszWKT = CPLStrdup(pszGCPProjectionIn);
    
    bProjDirty = TRUE;

    /* Reset NEATLINE if not explicitely set by the user */
    if (!bNeatLineDirty)
        SetMetadataItem("NEATLINE", NULL);

    return CE_None;
}

#endif // #if defined(HAVE_POPPLER) || defined(HAVE_PODOFO)


/************************************************************************/
/*                          GDALPDFOpen()                               */
/************************************************************************/

GDALDataset* GDALPDFOpen(const char* pszFilename, GDALAccess eAccess)
{
#if defined(HAVE_POPPLER) || defined(HAVE_PODOFO)
    GDALOpenInfo oOpenInfo(pszFilename, eAccess);
    return PDFDataset::Open(&oOpenInfo);
#else
    return NULL;
#endif
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
#ifdef HAVE_POPPLER
        poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );
        poDriver->SetMetadataItem( "HAVE_POPPLER", "YES" );
#endif
#ifdef HAVE_PODOFO
        poDriver->SetMetadataItem( "HAVE_PODOFO", "YES" );
#endif

        poDriver->SetMetadataItem( GDAL_DMD_CREATIONOPTIONLIST,
"<CreationOptionList>\n"
"   <Option name='COMPRESS' type='string-select' description='Compression method for raster data' default='DEFLATE'>\n"
"     <Value>NONE</Value>\n"
"     <Value>DEFLATE</Value>\n"
"     <Value>JPEG</Value>\n"
"     <Value>JPEG2000</Value>\n"
"   </Option>\n"
"   <Option name='STREAM_COMPRESS' type='string-select' description='Compression method for stream objects' default='DEFLATE'>\n"
"     <Value>NONE</Value>\n"
"     <Value>DEFLATE</Value>\n"
"   </Option>\n"
"   <Option name='GEO_ENCODING' type='string-select' description='Format of geo-encoding' default='ISO32000'>\n"
"     <Value>NONE</Value>\n"
"     <Value>ISO32000</Value>\n"
"     <Value>OGC_BP</Value>\n"
"     <Value>BOTH</Value>\n"
"   </Option>\n"
"   <Option name='NEATLINE' type='string' description='Neatline'/>\n"
"   <Option name='DPI' type='float' description='DPI' default='72'/>\n"
"   <Option name='PREDICTOR' type='int' description='Predictor Type (for DEFLATE compression)'/>\n"
"   <Option name='JPEG_QUALITY' type='int' description='JPEG quality 1-100' default='75'/>\n"
"   <Option name='JPEG2000_DRIVER' type='string'/>\n"
"   <Option name='TILED' type='boolean' description='Switch to tiled format' default='NO'/>\n"
"   <Option name='BLOCKXSIZE' type='int' description='Block Width'/>\n"
"   <Option name='BLOCKYSIZE' type='int' description='Block Height'/>\n"
"   <Option name='LAYER_NAME' type='string' description='Layer name for raster content'/>\n"
"   <Option name='EXTRA_STREAM' type='string' description='Extra data to insert into the page content stream'/>\n"
"   <Option name='EXTRA_IMAGES' type='string' description='List of image_file_name,x,y,scale (possibly repeated)'/>\n"
"   <Option name='EXTRA_LAYER_NAME' type='string' description='Layer name for extra content'/>\n"
"   <Option name='MARGIN' type='int' description='Margin around image in user units'/>\n"
"   <Option name='LEFT_MARGIN' type='int' description='Left margin in user units'/>\n"
"   <Option name='RIGHT_MARGIN' type='int' description='Right margin in user units'/>\n"
"   <Option name='TOP_MARGIN' type='int' description='Top margin in user units'/>\n"
"   <Option name='BOTTOM_MARGIN' type='int' description='Bottom margin in user units'/>\n"
"   <Option name='OGR_DATASOURCE' type='string' description='Name of OGR datasource to display on top of the raster layer'/>\n"
"   <Option name='OGR_DISPLAY_FIELD' type='string' description='Name of field to use as the display field in the feature tree'/>\n"
"   <Option name='OGR_DISPLAY_LAYER_NAMES' type='string' description='Comma separated list of OGR layer names to display in the feature tree'/>\n"
"   <Option name='OGR_WRITE_ATTRIBUTES' type='boolean' description='Whether to write attributes of OGR features' default='YES'/>\n"
"   <Option name='XMP' type='string' description='xml:XMP metadata'/>\n"
"   <Option name='WRITE_INFO' type='boolean' description='to control whether a Info block must be written' default='YES'/>\n"
"   <Option name='AUTHOR' type='string'/>\n"
"   <Option name='CREATOR' type='string'/>\n"
"   <Option name='CREATION_DATE' type='string'/>\n"
"   <Option name='KEYWORDS' type='string'/>\n"
"   <Option name='PRODUCER' type='string'/>\n"
"   <Option name='SUBJECT' type='string'/>\n"
"   <Option name='TITLE' type='string'/>\n"
"</CreationOptionList>\n" );

#if defined(HAVE_POPPLER) || defined(HAVE_PODOFO)
        poDriver->pfnOpen = PDFDataset::Open;
        poDriver->pfnIdentify = PDFDataset::Identify;
#endif

        poDriver->pfnCreateCopy = GDALPDFCreateCopy;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}

