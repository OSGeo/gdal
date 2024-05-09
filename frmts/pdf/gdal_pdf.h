/******************************************************************************
 * $Id$
 *
 * Project:  PDF Translator
 * Purpose:  Definition of classes for OGR .pdf driver.
 * Author:   Even Rouault, even dot rouault at spatialys.com
 *
 ******************************************************************************
 *
 * Support for open-source PDFium library
 *
 * Copyright (C) 2015 Klokan Technologies GmbH (http://www.klokantech.com/)
 * Author: Martin Mikita <martin.mikita@klokantech.com>, xmikit00 @ FIT VUT Brno
 *
 ******************************************************************************
 * Copyright (c) 2010-2014, Even Rouault <even dot rouault at spatialys.com>
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

#ifndef GDAL_PDF_H_INCLUDED
#define GDAL_PDF_H_INCLUDED

/* hack for PDF driver and poppler >= 0.15.0 that defines incompatible "typedef
 * bool GBool" */
/* in include/poppler/goo/gtypes.h with the one defined in cpl_port.h */
#define CPL_GBOOL_DEFINED
#define OGR_FEATURESTYLE_INCLUDE
#include "cpl_port.h"

#include <array>
#include <map>
#include <set>
#include <stack>
#include <utility>
#include <bitset>  // For detecting usage of PDF library
#include <algorithm>

#include "pdfsdk_headers.h"

#include "pdfdrivercore.h"

#include "cpl_vsi_virtual.h"

#include "gdal_pam.h"
#include "ogrsf_frmts.h"

#include "ogr_mem.h"
#include "pdfobject.h"

#define PDFLIB_POPPLER 0
#define PDFLIB_PODOFO 1
#define PDFLIB_PDFIUM 2
#define PDFLIB_COUNT 3

/************************************************************************/
/*                             OGRPDFLayer                              */
/************************************************************************/

#ifdef HAVE_PDF_READ_SUPPORT

class PDFDataset;

class OGRPDFLayer final : public OGRMemLayer
{
    PDFDataset *poDS;
    int bGeomTypeSet;
    int bGeomTypeMixed;

  public:
    OGRPDFLayer(PDFDataset *poDS, const char *pszName,
                OGRSpatialReference *poSRS, OGRwkbGeometryType eGeomType);

    void Fill(GDALPDFArray *poArray);

    virtual int TestCapability(const char *) override;

    GDALDataset *GetDataset() override;
};

#endif

/************************************************************************/
/*                          OGRPDFWritableLayer                         */
/************************************************************************/

class PDFWritableVectorDataset;

class OGRPDFWritableLayer final : public OGRMemLayer
{
    PDFWritableVectorDataset *poDS;

  public:
    OGRPDFWritableLayer(PDFWritableVectorDataset *poDS, const char *pszName,
                        OGRSpatialReference *poSRS,
                        OGRwkbGeometryType eGeomType);

    virtual int TestCapability(const char *) override;
    virtual OGRErr ICreateFeature(OGRFeature *poFeature) override;

    GDALDataset *GetDataset() override;
};

/************************************************************************/
/*                            GDALPDFTileDesc                           */
/************************************************************************/

typedef struct
{
    GDALPDFObject *poImage;
    double adfCM[6];
    double dfWidth;
    double dfHeight;
    int nBands;
} GDALPDFTileDesc;

#ifdef HAVE_PDFIUM
/**
 * Structures for Document and Document's Page for PDFium library,
 *  which does not support multi-threading.
 * Structures keeps objects for PDFium library and exclusive mutex locks
 *  for one-per-time access of PDFium library methods with multi-threading GDAL
 * Structures also keeps only one object per each opened PDF document
 *  - this saves time for opening and memory for opened objects
 * Document is closed after closing all pages object.
 */

/************************************************************************/
/*                           TPdfiumPageStruct                          */
/************************************************************************/

// Map of Pdfium pages in following structure
typedef struct
{
    int pageNum;
    CPDF_Page *page;
    CPLMutex *readMutex;
    int sharedNum;
} TPdfiumPageStruct;

typedef std::map<int, TPdfiumPageStruct *> TMapPdfiumPages;

/************************************************************************/
/*                         TPdfiumDocumentStruct                        */
/************************************************************************/

// Structure for Mutex on File
typedef struct
{
    char *filename;
    CPDF_Document *doc;
    TMapPdfiumPages pages;
    FPDF_FILEACCESS *psFileAccess;
} TPdfiumDocumentStruct;

#endif  // ~ HAVE_PDFIUM

/************************************************************************/
/* ==================================================================== */
/*                              PDFDataset                              */
/* ==================================================================== */
/************************************************************************/

class PDFRasterBand;
class PDFImageRasterBand;

#ifdef HAVE_POPPLER
class ObjectAutoFree;
#endif

#define MAX_TOKEN_SIZE 256
#define TOKEN_STACK_SIZE 8

#define GDAL_DEFAULT_DPI 150.0

#ifdef HAVE_PDF_READ_SUPPORT

class PDFDataset final : public GDALPamDataset
{
    friend class PDFRasterBand;
    friend class PDFImageRasterBand;

    VSIVirtualHandleUniquePtr m_fp{};
    bool m_bIsOvrDS = false;

    CPLString m_osFilename{};
    CPLString m_osUserPwd{};
    OGRSpatialReference m_oSRS{};
    double m_dfDPI = GDAL_DEFAULT_DPI;
    bool m_bHasCTM = false;
    std::array<double, 6> m_adfCTM = {{0, 0, 0, 0, 0, 0}};
    std::array<double, 6> m_adfGeoTransform = {{0, 1, 0, 0, 0, 1}};
    bool m_bGeoTransformValid = false;
    int m_nGCPCount = 0;
    GDAL_GCP *m_pasGCPList = nullptr;
    bool m_bProjDirty = false;
    bool m_bNeatLineDirty = false;

    GDALMultiDomainMetadata m_oMDMD_PDF{};
    bool m_bInfoDirty = false;
    bool m_bXMPDirty = false;

    std::bitset<PDFLIB_COUNT> m_bUseLib{};
#ifdef HAVE_POPPLER
    PDFDoc *m_poDocPoppler = nullptr;
#endif
#ifdef HAVE_PODOFO
    PoDoFo::PdfMemDocument *m_poDocPodofo = nullptr;
    bool m_bPdfToPpmFailed = false;
#endif
#ifdef HAVE_PDFIUM
    TPdfiumDocumentStruct *m_poDocPdfium = nullptr;
    TPdfiumPageStruct *m_poPagePdfium = nullptr;
#endif
    std::vector<std::unique_ptr<PDFDataset>> m_apoOvrDS{};
    std::vector<std::unique_ptr<PDFDataset>> m_apoOvrDSBackup{};
    GDALPDFObject *m_poPageObj = nullptr;

    int m_iPage = -1;

    GDALPDFObject *m_poImageObj = nullptr;

    double m_dfMaxArea = 0;
    int ParseLGIDictObject(GDALPDFObject *poLGIDict);
    int ParseLGIDictDictFirstPass(GDALPDFDictionary *poLGIDict,
                                  int *pbIsBestCandidate = nullptr);
    int ParseLGIDictDictSecondPass(GDALPDFDictionary *poLGIDict);
    int ParseProjDict(GDALPDFDictionary *poProjDict);
    int ParseVP(GDALPDFObject *poVP, double dfMediaBoxWidth,
                double dfMediaBoxHeight);
    int ParseMeasure(GDALPDFObject *poMeasure, double dfMediaBoxWidth,
                     double dfMediaBoxHeight, double dfULX, double dfULY,
                     double dfLRX, double dfLRY);

    bool m_bTried = false;
    GByte *m_pabyCachedData = nullptr;
    int m_nLastBlockXOff = -1;
    int m_nLastBlockYOff = -1;
    bool m_bCacheBlocksForOtherBands = false;

    OGRPolygon *m_poNeatLine = nullptr;

    std::vector<GDALPDFTileDesc> m_asTiles{}; /* in the order of the PDF file */
    std::vector<int> m_aiTiles{};             /* in the order of blocks */
    int m_nBlockXSize = 0;
    int m_nBlockYSize = 0;
    int CheckTiledRaster();

    void GuessDPI(GDALPDFDictionary *poPageDict, int *pnBands);
    void FindXMP(GDALPDFObject *poObj);
    void ParseInfo(GDALPDFObject *poObj);

#ifdef HAVE_POPPLER
    std::unique_ptr<Object> m_poCatalogObjectPoppler{};
#endif
    GDALPDFObject *m_poCatalogObject = nullptr;
    GDALPDFObject *GetCatalog();
    GDALPDFArray *GetPagesKids();

#if defined(HAVE_POPPLER) || defined(HAVE_PDFIUM)
    void AddLayer(const std::string &osName, int iPage);
    void CreateLayerList();
    std::string
    BuildPostfixedLayerNameAndAddLayer(const std::string &osName,
                                       const std::pair<int, int> &oOCGRef,
                                       int iPageOfInterest, int nPageCount);
#endif

#if defined(HAVE_POPPLER)
    void ExploreLayersPoppler(GDALPDFArray *poArray, int iPageOfInterest,
                              int nPageCount, CPLString osTopLayer,
                              int nRecLevel, int &nVisited, bool &bStop);
    void FindLayersPoppler(int iPageOfInterest);
    void TurnLayersOnOffPoppler();
    std::vector<std::pair<CPLString, OptionalContentGroup *>>
        m_oLayerOCGListPoppler{};
#endif

#ifdef HAVE_PDFIUM
    void ExploreLayersPdfium(GDALPDFArray *poArray, int iPageOfInterest,
                             int nPageCount, int nRecLevel,
                             CPLString osTopLayer = "");
    void FindLayersPdfium(int iPageOfInterest);
    void PDFiumRenderPageBitmap(FPDF_BITMAP bitmap, FPDF_PAGE page, int start_x,
                                int start_y, int size_x, int size_y,
                                const char *pszRenderingOptions);
    void TurnLayersOnOffPdfium();

  public:
    typedef enum
    {
        VISIBILITY_DEFAULT,
        VISIBILITY_ON,
        VISIBILITY_OFF
    } VisibilityState;

    VisibilityState GetVisibilityStateForOGCPdfium(int nNum, int nGen);

  private:
    std::map<CPLString, std::pair<int, int>> m_oMapLayerNameToOCGNumGenPdfium{};
    std::map<std::pair<int, int>, VisibilityState>
        m_oMapOCGNumGenToVisibilityStatePdfium{};
#endif

    // Map OCGs identified by their (number, generation) to the list of pages
    // where they are referenced from.
    std::map<std::pair<int, int>, std::vector<int>> m_oMapOCGNumGenToPages{};

    struct LayerStruct
    {
        std::string osName{};
        int nInsertIdx = 0;
        int iPage = 0;
    };

    std::vector<LayerStruct> m_oLayerNameSet{};
    CPLStringList m_aosLayerNames{};

    struct LayerWithRef
    {
        CPLString osName{};
        GDALPDFObjectNum nOCGNum{};
        int nOCGGen = 0;

        LayerWithRef(const CPLString &osNameIn,
                     const GDALPDFObjectNum &nOCGNumIn, int nOCGGenIn)
            : osName(osNameIn), nOCGNum(nOCGNumIn), nOCGGen(nOCGGenIn)
        {
        }
    };

    std::vector<LayerWithRef> m_aoLayerWithRef{};

    CPLString FindLayerOCG(GDALPDFDictionary *poPageDict,
                           const char *pszLayerName);
    void FindLayersGeneric(GDALPDFDictionary *poPageDict);

    void MapOCGsToPages();

    bool m_bUseOCG = false;

    static const char *GetOption(char **papszOpenOptions,
                                 const char *pszOptionName,
                                 const char *pszDefaultVal);

    bool m_bHasLoadedLayers = false;
    std::vector<std::unique_ptr<OGRPDFLayer>> m_apoLayers{};

    double m_dfPageWidth = 0;
    double m_dfPageHeight = 0;
    void PDFCoordsToSRSCoords(double x, double y, double &X, double &Y);

    std::map<int, OGRGeometry *> m_oMapMCID{};
    void CleanupIntermediateResources();

    std::map<CPLString, int> m_oMapOperators{};
    void InitMapOperators();

    bool m_bSetStyle = false;

    bool ExploreTree(GDALPDFObject *poObj,
                     std::set<std::pair<int, int>> &aoSetAlreadyVisited,
                     int nRecLevel, bool bDryRun);
    void ExploreContents(GDALPDFObject *poObj, GDALPDFObject *poResources,
                         int nDepth, int &nVisited, bool &bStop);

    void ExploreContentsNonStructuredInternal(
        GDALPDFObject *poContents, GDALPDFObject *poResources,
        std::map<CPLString, OGRPDFLayer *> &oMapPropertyToLayer,
        OGRPDFLayer *poSingleLayer);
    void ExploreContentsNonStructured(GDALPDFObject *poObj,
                                      GDALPDFObject *poResources);

    int UnstackTokens(const char *pszToken, int nRequiredArgs,
                      char aszTokenStack[TOKEN_STACK_SIZE][MAX_TOKEN_SIZE],
                      int &nTokenStackSize, double *adfCoords);

    struct GraphicState
    {
        std::array<double, 6> adfCM = {1, 0, 0, 1, 0, 0};
        std::array<double, 3> adfStrokeColor = {0.0, 0.0, 0.0};
        std::array<double, 3> adfFillColor = {1.0, 1.0, 1.0};

        void PreMultiplyBy(double adfMatrix[6]);
        void ApplyMatrix(double adfCoords[2]) const;
    };

    OGRGeometry *
    ParseContent(const char *pszContent, GDALPDFObject *poResources,
                 int bInitBDCStack, int bMatchQ,
                 std::map<CPLString, OGRPDFLayer *> &oMapPropertyToLayer,
                 const GraphicState &graphicStateIn, OGRPDFLayer *poCurLayer);
    OGRGeometry *BuildGeometry(std::vector<double> &oCoords, int bHasFoundFill,
                               int bHasMultiPart);

    bool OpenVectorLayers(GDALPDFDictionary *poPageDict);

    void InitOverviews();

  public:
    PDFDataset(PDFDataset *poParentDS = nullptr, int nXSize = 0,
               int nYSize = 0);
    virtual ~PDFDataset();

    virtual CPLErr GetGeoTransform(double *) override;

    virtual CPLErr SetGeoTransform(double *padfGeoTransform) override;

    const OGRSpatialReference *GetSpatialRef() const override;
    CPLErr SetSpatialRef(const OGRSpatialReference *poSRS) override;

    virtual char **GetMetadataDomainList() override;
    virtual char **GetMetadata(const char *pszDomain = "") override;
    virtual CPLErr SetMetadata(char **papszMetadata,
                               const char *pszDomain = "") override;
    virtual const char *GetMetadataItem(const char *pszName,
                                        const char *pszDomain = "") override;
    virtual CPLErr SetMetadataItem(const char *pszName, const char *pszValue,
                                   const char *pszDomain = "") override;

    virtual CPLErr IRasterIO(GDALRWFlag, int, int, int, int, void *, int, int,
                             GDALDataType, int, int *, GSpacing nPixelSpace,
                             GSpacing nLineSpace, GSpacing nBandSpace,
                             GDALRasterIOExtraArg *psExtraArg) override;

    virtual int GetGCPCount() override;
    const OGRSpatialReference *GetGCPSpatialRef() const override;
    virtual const GDAL_GCP *GetGCPs() override;
    CPLErr SetGCPs(int nGCPCountIn, const GDAL_GCP *pasGCPListIn,
                   const OGRSpatialReference *poSRS) override;

    CPLErr ReadPixels(int nReqXOff, int nReqYOff, int nReqXSize, int nReqYSize,
                      GSpacing nPixelSpace, GSpacing nLineSpace,
                      GSpacing nBandSpace, GByte *pabyData);

    virtual int GetLayerCount() override;
    virtual OGRLayer *GetLayer(int) override;

    virtual int TestCapability(const char *) override;

    OGRGeometry *GetGeometryFromMCID(int nMCID);

    GDALPDFObject *GetPageObj()
    {
        return m_poPageObj;
    }

    double GetPageWidth() const
    {
        return m_dfPageWidth;
    }

    double GetPageHeight() const
    {
        return m_dfPageHeight;
    }

    static PDFDataset *Open(GDALOpenInfo *);

    static GDALDataset *OpenWrapper(GDALOpenInfo *poOpenInfo)
    {
        return Open(poOpenInfo);
    }

    virtual CPLErr IBuildOverviews(const char *, int, const int *, int,
                                   const int *, GDALProgressFunc, void *,
                                   CSLConstList papszOptions) override;

#ifdef HAVE_PDFIUM
    static bool g_bPdfiumInit;
#endif
};

/************************************************************************/
/* ==================================================================== */
/*                         PDFRasterBand                                */
/* ==================================================================== */
/************************************************************************/

class PDFRasterBand CPL_NON_FINAL : public GDALPamRasterBand
{
    friend class PDFDataset;

    int nResolutionLevel;

    CPLErr IReadBlockFromTile(int, int, void *);

  public:
    PDFRasterBand(PDFDataset *, int, int);
    virtual ~PDFRasterBand();

    virtual GDALSuggestedBlockAccessPattern
    GetSuggestedBlockAccessPattern() const override;

    virtual int GetOverviewCount() override;
    virtual GDALRasterBand *GetOverview(int) override;

    virtual CPLErr IReadBlock(int, int, void *) override;
    virtual GDALColorInterp GetColorInterpretation() override;

    virtual CPLErr IRasterIO(GDALRWFlag, int, int, int, int, void *, int, int,
                             GDALDataType, GSpacing nPixelSpace,
                             GSpacing nLineSpace,
                             GDALRasterIOExtraArg *psExtraArg) override;
};

#endif /* HAVE_PDF_READ_SUPPORT */

/************************************************************************/
/*                          PDFWritableDataset                          */
/************************************************************************/

class PDFWritableVectorDataset final : public GDALDataset
{
    char **papszOptions;

    int nLayers;
    OGRLayer **papoLayers;

    int bModified;

  public:
    PDFWritableVectorDataset();
    virtual ~PDFWritableVectorDataset();

    virtual OGRLayer *ICreateLayer(const char *pszName,
                                   const OGRGeomFieldDefn *poGeomFieldDefn,
                                   CSLConstList papszOptions) override;

    virtual OGRErr SyncToDisk();

    virtual int GetLayerCount() override;
    virtual OGRLayer *GetLayer(int) override;

    virtual int TestCapability(const char *) override;

    static GDALDataset *Create(const char *pszName, int nXSize, int nYSize,
                               int nBands, GDALDataType eType,
                               char **papszOptions);

    void SetModified()
    {
        bModified = TRUE;
    }
};

GDALDataset *GDALPDFOpen(const char *pszFilename, GDALAccess eAccess);
CPLString PDFSanitizeLayerName(const char *pszName);

#endif /* ndef GDAL_PDF_H_INCLUDED */
