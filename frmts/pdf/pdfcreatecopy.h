/******************************************************************************
 * $Id$
 *
 * Project:  PDF driver
 * Purpose:  GDALDataset driver for PDF dataset.
 * Author:   Even Rouault, <even dot rouault at spatialys dot com>
 *
 ******************************************************************************
 * Copyright (c) 2012-2019, Even Rouault <even dot rouault at spatialys dot com>
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

#ifndef PDFCREATECOPY_H_INCLUDED
#define PDFCREATECOPY_H_INCLUDED

#include "pdfobject.h"
#include "gdal_priv.h"
#include <vector>
#include <map>

#include "ogr_api.h"
#include "ogr_spatialref.h"

/* Cf PDF reference v1.7, Appendix C, page 993 */
#define MAXIMUM_SIZE_IN_UNITS   14400

#define APPLY_GT_X(gt, x, y) ((gt)[0] + (x) * (gt)[1] + (y) * (gt)[2])
#define APPLY_GT_Y(gt, x, y) ((gt)[3] + (x) * (gt)[4] + (y) * (gt)[5])

typedef enum
{
    COMPRESS_NONE,
    COMPRESS_DEFLATE,
    COMPRESS_JPEG,
    COMPRESS_JPEG2000,
    COMPRESS_DEFAULT
} PDFCompressMethod;

struct PDFMargins
{
    int nLeft = 0;
    int nRight = 0;
    int nTop = 0;
    int nBottom = 0;
};

class GDALFakePDFDataset final: public GDALDataset
{
    public:
        GDALFakePDFDataset() = default;
};

/************************************************************************/
/*                          GDALPDFWriter                               */
/************************************************************************/

class GDALXRefEntry
{
    public:
        vsi_l_offset    nOffset = 0;
        int             nGen = 0;
        int             bFree = FALSE;

        GDALXRefEntry() = default;
        explicit GDALXRefEntry(vsi_l_offset nOffsetIn, int nGenIn = 0) : nOffset(nOffsetIn), nGen(nGenIn) {}
        GDALXRefEntry(const GDALXRefEntry& oOther) : nOffset(oOther.nOffset), nGen(oOther.nGen), bFree(oOther.bFree) {}
        GDALXRefEntry& operator= (const GDALXRefEntry& oOther) { nOffset = oOther.nOffset; nGen = oOther.nGen; bFree = oOther.bFree; return *this; }
};

class GDALPDFImageDesc
{
    public:
        GDALPDFObjectNum  nImageId{};
        double       dfXOff = 0;
        double       dfYOff = 0;
        double       dfXSize = 0;
        double       dfYSize = 0;
};

class GDALPDFLayerDesc
{
    public:
        GDALPDFObjectNum  nOCGId{};
        GDALPDFObjectNum  nOCGTextId{};
        GDALPDFObjectNum  nFeatureLayerId{};
        CPLString    osLayerName{};
        int          bWriteOGRAttributes{false};
        std::vector<GDALPDFObjectNum> aIds{};
        std::vector<GDALPDFObjectNum> aIdsText{};
        std::vector<GDALPDFObjectNum> aUserPropertiesIds{};
        std::vector<CPLString> aFeatureNames{};
        std::vector<CPLString> aosIncludedFields{};
};

class GDALPDFRasterDesc
{
    public:
        GDALPDFObjectNum nOCGRasterId{};
        std::vector<GDALPDFImageDesc> asImageDesc{};
};

class GDALPDFPageContext
{
    public:
        GDALDataset* poClippingDS = nullptr;
        PDFCompressMethod eStreamCompressMethod = COMPRESS_NONE;
        double       dfDPI{0};
        PDFMargins   sMargins{};
        GDALPDFObjectNum  nPageId{};
        GDALPDFObjectNum  nContentId{};
        GDALPDFObjectNum  nResourcesId{};
        std::vector<GDALPDFLayerDesc> asVectorDesc{};
        std::vector<GDALPDFRasterDesc> asRasterDesc{};
        GDALPDFObjectNum  nAnnotsId{};
        std::vector<GDALPDFObjectNum> anAnnotationsId{};
};

class GDALPDFOCGDesc
{
    public:
        GDALPDFObjectNum  nId{};
        GDALPDFObjectNum  nParentId{};
        CPLString    osLayerName{};
};

class GDALPDFBaseWriter
{
protected:
    VSILFILE* m_fp = nullptr;
    bool m_bInWriteObj = false;
    std::vector<GDALXRefEntry> m_asXRefEntries{};
    GDALPDFObjectNum m_nPageResourceId{};
    GDALPDFObjectNum m_nCatalogId{};
    int         m_nCatalogGen = 0;
    GDALPDFObjectNum m_nInfoId{};
    int         m_nInfoGen = 0;
    GDALPDFObjectNum m_nXMPId{};
    int         m_nXMPGen = 0;
    GDALPDFObjectNum m_nStructTreeRootId{};
    GDALPDFObjectNum m_nNamesId{};

    GDALPDFObjectNum m_nContentLengthId{};
    VSILFILE* m_fpBack = nullptr;
    VSILFILE* m_fpGZip = nullptr;
    vsi_l_offset m_nStreamStart = 0;

    std::vector<GDALPDFObjectNum> m_asPageId{};
    std::vector<GDALPDFOCGDesc> m_asOCGs{};
    std::map<CPLString,GDALPDFImageDesc> m_oMapSymbolFilenameToDesc{};

public:

    struct ObjectStyle
    {
        unsigned int nPenR = 0;
        unsigned int nPenG = 0;
        unsigned int nPenB = 0;
        unsigned int nPenA = 255;
        unsigned int nBrushR = 127;
        unsigned int nBrushG = 127;
        unsigned int nBrushB = 127;
        unsigned int nBrushA = 127;
        unsigned int nTextR = 0;
        unsigned int nTextG = 0;
        unsigned int nTextB = 0;
        unsigned int nTextA = 255;
        int bSymbolColorDefined = FALSE;
        unsigned int nSymbolR = 0;
        unsigned int nSymbolG = 0;
        unsigned int nSymbolB = 0;
        unsigned int nSymbolA = 255;
        bool bHasPenBrushOrSymbol = false;
        CPLString osTextFont{};
        bool bTextBold = false;
        bool bTextItalic = false;
        double dfTextSize = 12.0;
        double dfTextAngle = 0.0;
        double dfTextStretch = 1.0;
        double dfTextDx = 0.0;
        double dfTextDy = 0.0;
        int nTextAnchor = 1;
        double dfPenWidth = 1.0;
        double dfSymbolSize = 5.0;
        CPLString osDashArray{};
        CPLString osLabelText{};
        CPLString osSymbolId{};
        GDALPDFObjectNum nImageSymbolId{};
        int nImageWidth = 0;
        int nImageHeight = 0;
    };

protected:
    explicit GDALPDFBaseWriter(VSILFILE* fp);
    ~GDALPDFBaseWriter();

    GDALPDFObjectNum AllocNewObject();

    void    StartObj(const GDALPDFObjectNum& nObjectId, int nGen = 0);
    void    EndObj();

    void    StartObjWithStream(const GDALPDFObjectNum& nObjectId,
                                           GDALPDFDictionaryRW& oDict,
                                           bool bDeflate);
    void    EndObjWithStream();

    void    StartNewDoc();
    void    Close();

    void    WriteXRefTableAndTrailer(bool bUpdate,
                                     vsi_l_offset nLastStartXRef);

    GDALPDFObjectNum     WriteSRS_ISO32000(GDALDataset* poSrcDS,
                                double dfUserUnit,
                                const char* pszNEATLINE,
                                PDFMargins* psMargins,
                                int bWriteViewport);
    GDALPDFObjectNum     WriteSRS_OGC_BP(GDALDataset* poSrcDS,
                            double dfUserUnit,
                            const char* pszNEATLINE,
                            PDFMargins* psMargins);
    static GDALPDFDictionaryRW* GDALPDFBuildOGC_BP_Projection(const OGRSpatialReference* poSRS);

    GDALPDFObjectNum WriteOCG(const char* pszLayerName, const GDALPDFObjectNum& nParentId = GDALPDFObjectNum());

    GDALPDFObjectNum     WriteBlock( GDALDataset* poSrcDS,
                        int nXOff, int nYOff, int nReqXSize, int nReqYSize,
                        const GDALPDFObjectNum& nColorTableIdIn,
                        PDFCompressMethod eCompressMethod,
                        int nPredictor,
                        int nJPEGQuality,
                        const char* pszJPEG2000_DRIVER,
                        GDALProgressFunc pfnProgress,
                        void * pProgressData );
    GDALPDFObjectNum     WriteMask(GDALDataset* poSrcDS,
                      int nXOff, int nYOff, int nReqXSize, int nReqYSize,
                      PDFCompressMethod eCompressMethod);

    GDALPDFObjectNum     WriteColorTable(GDALDataset* poSrcDS);

    void GetObjectStyle(const char* pszStyleString,
                        OGRFeatureH hFeat, const double adfMatrix[4],
                        std::map<CPLString,GDALPDFImageDesc> oMapSymbolFilenameToDesc,
                        ObjectStyle& os);
    static CPLString GenerateDrawingStream(OGRGeometryH hGeom,
                                    const double adfMatrix[4],
                                    ObjectStyle& os,
                                    double dfRadius);
    GDALPDFObjectNum WriteAttributes(
        OGRFeatureH hFeat,
        const std::vector<CPLString>& aosIncludedFields,
        const char* pszOGRDisplayField,
        int nMCID,
        const GDALPDFObjectNum& oParent,
        const GDALPDFObjectNum& oPage,
        CPLString& osOutFeatureName);

    GDALPDFObjectNum WriteLabel(OGRGeometryH hGeom,
                                    const double adfMatrix[4],
                                    ObjectStyle& os,
                                    PDFCompressMethod eStreamCompressMethod,
                                    double bboxXMin,
                                    double bboxYMin,
                                    double bboxXMax,
                                    double bboxYMax);

    GDALPDFObjectNum WriteLink(OGRFeatureH hFeat,
                              const char* pszOGRLinkField,
                              const double adfMatrix[4],
                              int bboxXMin,
                              int bboxYMin,
                              int bboxXMax,
                              int bboxYMax);

    static void ComputeIntBBox(OGRGeometryH hGeom,
                           const OGREnvelope& sEnvelope,
                           const double adfMatrix[4],
                           const ObjectStyle& os,
                           double dfRadius,
                           int& bboxXMin,
                           int& bboxYMin,
                           int& bboxXMax,
                           int& bboxYMax);

    GDALPDFObjectNum  WriteJavascript(const char* pszJavascript, bool bDeflate);

public:
    GDALPDFObjectNum  SetInfo(GDALDataset* poSrcDS,
                char** papszOptions);
    GDALPDFObjectNum  SetInfo(const char* pszAUTHOR,
                 const char* pszPRODUCER,
                 const char* pszCREATOR,
                 const char* pszCREATION_DATE,
                 const char* pszSUBJECT,
                 const char* pszTITLE,
                 const char* pszKEYWORDS);
    GDALPDFObjectNum  SetXMP(GDALDataset* poSrcDS,
                const char* pszXMP);
};

class GDALPDFUpdateWriter final: public GDALPDFBaseWriter
{
        bool m_bUpdateNeeded = false;
        vsi_l_offset m_nLastStartXRef = 0;
        int m_nLastXRefSize = 0;

public:
        explicit GDALPDFUpdateWriter( VSILFILE* fpIn );
       ~GDALPDFUpdateWriter();

       void Close();

       const GDALPDFObjectNum& GetCatalogNum() const { return m_nCatalogId; }
       int  GetCatalogGen() const { return m_nCatalogGen; }

       int  ParseTrailerAndXRef();
       void UpdateProj(GDALDataset* poSrcDS,
                       double dfDPI,
                       GDALPDFDictionaryRW* poPageDict,
                       const GDALPDFObjectNum& nPageId,
                       int nPageGen);
       void UpdateInfo(GDALDataset* poSrcDS);
       void UpdateXMP (GDALDataset* poSrcDS,
                       GDALPDFDictionaryRW* poCatalogDict);
};

class GDALPDFWriter final: public GDALPDFBaseWriter
{
    GDALPDFPageContext oPageContext{};

    CPLString    m_osOffLayers{};
    CPLString    m_osExclusiveLayers{};

    void    WritePages();

    public:
        explicit GDALPDFWriter( VSILFILE* fpIn );
       ~GDALPDFWriter();

       void Close();

       bool  StartPage(GDALDataset* poSrcDS,
                      double dfDPI,
                      bool bWriteUserUnit,
                      const char* pszGEO_ENCODING,
                      const char* pszNEATLINE,
                      PDFMargins* psMargins,
                      PDFCompressMethod eStreamCompressMethod,
                      int bHasOGRData);

       bool WriteImagery(GDALDataset* poDS,
                        const char* pszLayerName,
                        PDFCompressMethod eCompressMethod,
                        int nPredictor,
                        int nJPEGQuality,
                        const char* pszJPEG2000_DRIVER,
                        int nBlockXSize, int nBlockYSize,
                        GDALProgressFunc pfnProgress,
                        void * pProgressData);

       bool WriteClippedImagery(GDALDataset* poDS,
                               const char* pszLayerName,
                               PDFCompressMethod eCompressMethod,
                               int nPredictor,
                               int nJPEGQuality,
                               const char* pszJPEG2000_DRIVER,
                               int nBlockXSize, int nBlockYSize,
                               GDALProgressFunc pfnProgress,
                               void * pProgressData);
       bool WriteOGRDataSource(const char* pszOGRDataSource,
                              const char* pszOGRDisplayField,
                              const char* pszOGRDisplayLayerNames,
                              const char* pszOGRLinkField,
                              int bWriteOGRAttributes);

       GDALPDFLayerDesc StartOGRLayer(CPLString osLayerName,
                                      int bWriteOGRAttributes);
       void EndOGRLayer(GDALPDFLayerDesc& osVectorDesc);

       int WriteOGRLayer(OGRDataSourceH hDS,
                         int iLayer,
                         const char* pszOGRDisplayField,
                         const char* pszOGRLinkField,
                         CPLString osLayerName,
                         int bWriteOGRAttributes,
                         int& iObj);

       int WriteOGRFeature(GDALPDFLayerDesc& osVectorDesc,
                           OGRFeatureH hFeat,
                           OGRCoordinateTransformationH hCT,
                           const char* pszOGRDisplayField,
                           const char* pszOGRLinkField,
                           int bWriteOGRAttributes,
                           int& iObj);

       GDALPDFObjectNum  WriteJavascript(const char* pszJavascript);
       GDALPDFObjectNum  WriteJavascriptFile(const char* pszJavascriptFile);

       int  EndPage(const char* pszExtraImages,
                    const char* pszExtraStream,
                    const char* pszExtraLayerName,
                    const char* pszOffLayers,
                    const char* pszExclusiveLayers);
};

GDALDataset         *GDALPDFCreateCopy( const char *, GDALDataset *,
                                        int, char **,
                                        GDALProgressFunc pfnProgress,
                                        void * pProgressData );

#endif // PDFCREATECOPY_H_INCLUDED
