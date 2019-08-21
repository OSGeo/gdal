/******************************************************************************
 * $Id$
 *
 * Project:  PDF driver
 * Purpose:  GDALDataset driver for PDF dataset.
 * Author:   Even Rouault, <even dot rouault at spatialys dot com>
 *
 ******************************************************************************
 * Copyright (c) 2019, Even Rouault <even dot rouault at spatialys dot com>
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

#ifndef PDFCREATEFROMCOMPOSITION_H_INCLUDED
#define PDFCREATEFROMCOMPOSITION_H_INCLUDED

#include "gdal_pdf.h"
#include "pdfcreatecopy.h"
#include "cpl_minixml.h"
#include "ogrsf_frmts.h"
#include "ogr_geometry.h"

#include <map>
#include <memory>
#include <vector>

class GDALPDFComposerWriter final: public GDALPDFBaseWriter
{
        CPLString m_osJPEG2000Driver{};
        struct TreeOfOCG
        {
            GDALPDFObjectNum                        m_nNum{};
            bool                                    m_bInitiallyVisible{true};
            std::vector<std::unique_ptr<TreeOfOCG>> m_children{};
        };
        bool m_bDisplayLayersOnlyOnVisiblePages = false;
        TreeOfOCG m_oTreeOfOGC{};
        std::map<CPLString, std::vector<GDALPDFObjectNum>> m_oMapExclusiveOCGIdToOCGs{};

        std::map<CPLString, GDALPDFObjectNum> m_oMapLayerIdToOCG{};

        struct xyPair
        {
            double x = 0;
            double y = 0;
            xyPair(double xin = 0.0, double yin = 0.0): x(xin), y(yin) {}
        };

        struct Georeferencing
        {
            CPLString           m_osID{};
            OGRSpatialReference m_oSRS{};
            double              m_bboxX1{};
            double              m_bboxY1{};
            double              m_bboxX2{};
            double              m_bboxY2{};
            double              m_adfGT[6]{0,1,0,0,0,1};
        };

        std::vector<GDALPDFObjectNum> m_anParentElements;
        std::vector<GDALPDFObjectNum> m_anFeatureLayerId;
        std::map<CPLString, GDALPDFObjectNum> m_oMapPageIdToObjectNum;
        struct PageContext
        {
            double m_dfWidthInUserUnit = 0;
            double m_dfHeightInUserUnit = 0;
            CPLString m_osDrawingStream{};
            std::vector<GDALPDFObjectNum> m_anFeatureUserProperties;
            int m_nMCID = 0;
            PDFCompressMethod m_eStreamCompressMethod = COMPRESS_DEFLATE;
            std::map<CPLString, GDALPDFObjectNum> m_oXObjects{};
            std::map<CPLString, GDALPDFObjectNum> m_oProperties{};
            std::map<CPLString, GDALPDFObjectNum> m_oExtGState{};
            std::vector<GDALPDFObjectNum> m_anAnnotationsId{};
            std::map<CPLString, Georeferencing> m_oMapGeoreferencedId{};
        };

        bool CreateLayerTree(const CPLXMLNode* psNode,
                             const GDALPDFObjectNum& nParentId,
                             TreeOfOCG* parent);

        struct Action
        {
            virtual ~Action() = default;
        };

        struct GotoPageAction final: public Action
        {
            GDALPDFObjectNum m_nPageDestId{};
            double m_dfX1 = 0;
            double m_dfX2 = 0;
            double m_dfY1 = 0;
            double m_dfY2 = 0;
        };

        struct SetLayerStateAction final: public Action
        {
            std::set<GDALPDFObjectNum> m_anONLayers{};
            std::set<GDALPDFObjectNum> m_anOFFLayers{};
        };

        struct JavascriptAction final: public Action
        {
            CPLString m_osScript{};
        };

        bool ParseActions(const CPLXMLNode* psNode,
                            std::vector<std::unique_ptr<Action>>& actions);
        static GDALPDFDictionaryRW* SerializeActions(
                        GDALPDFDictionaryRW* poDictForDest,
                        const std::vector<std::unique_ptr<Action>>& actions);

        struct OutlineItem
        {
            GDALPDFObjectNum m_nObjId{};
            CPLString m_osName{};
            bool m_bOpen = true;
            int m_nFlags = 0;
            std::vector<std::unique_ptr<Action>> m_aoActions{};
            std::vector<std::unique_ptr<OutlineItem>> m_aoKids{};
            int m_nKidsRecCount = 0;
        };
        GDALPDFObjectNum m_nOutlinesId{};

        bool CreateOutlineFirstPass(const CPLXMLNode* psNode,
                                    OutlineItem* poParentItem);
        bool SerializeOutlineKids(const OutlineItem* poParentItem);
        bool CreateOutline(const CPLXMLNode* psNode);

        void WritePages();

        static GDALPDFArrayRW* CreateOCGOrder(const TreeOfOCG* parent);
        static void CollectOffOCG(std::vector<GDALPDFObjectNum>& ar,
                                          const TreeOfOCG* parent);
        bool GeneratePage(const CPLXMLNode* psPage);
        bool GenerateGeoreferencing(const CPLXMLNode* psGeoreferencing,
                                    double dfWidthInUserUnit,
                                    double dfHeightInUserUnit,
                                    GDALPDFObjectNum& nViewportId,
                                    GDALPDFObjectNum& nLGIDictId,
                                    Georeferencing& georeferencing);

        GDALPDFObjectNum GenerateISO32000_Georeferencing(
            OGRSpatialReferenceH hSRS,
            double bboxX1, double bboxY1, double bboxX2, double bboxY2,
            const std::vector<GDAL_GCP>& aGCPs,
            const std::vector<xyPair>& aBoundingPolygon);

        GDALPDFObjectNum GenerateOGC_BP_Georeferencing(
            OGRSpatialReferenceH hSRS,
            double bboxX1, double bboxY1, double bboxX2, double bboxY2,
            const std::vector<GDAL_GCP>& aGCPs,
            const std::vector<xyPair>& aBoundingPolygon);

        bool ExploreContent(const CPLXMLNode* psNode, PageContext& oPageContext);
        bool WriteRaster(const CPLXMLNode* psNode, PageContext& oPageContext);
        bool WriteVector(const CPLXMLNode* psNode, PageContext& oPageContext);
        bool WriteVectorLabel(const CPLXMLNode* psNode, PageContext& oPageContext);
        void StartBlending(const CPLXMLNode* psNode, PageContext& oPageContext,
                           double& dfOpacity);
        static void EndBlending(const CPLXMLNode* psNode, PageContext& oPageContext);

        static bool SetupVectorGeoreferencing(
            const char* pszGeoreferencingId,
            OGRLayer* poLayer,
            const PageContext& oPageContext,
            double& dfClippingMinX,
            double& dfClippingMinY,
            double& dfClippingMaxX,
            double& dfClippingMaxY,
            double adfMatrix[4],
            std::unique_ptr<OGRCoordinateTransformation>& poCT);

#ifdef HAVE_PDF_READ_SUPPORT
        bool WritePDF(const CPLXMLNode* psNode, PageContext& oPageContext);

        typedef std::map< std::pair<int, int>, GDALPDFObjectNum> RemapType;
        GDALPDFObjectNum EmitNewObject(GDALPDFObject* poObj,
                                       RemapType& oRemapObjectRefs);
        GDALPDFObjectNum SerializeAndRenumber(GDALPDFObject* poObj);
        bool SerializeAndRenumber(CPLString& osStr,
            GDALPDFObject* poObj,
            RemapType& oRemapObjectRefs);
        bool SerializeAndRenumberIgnoreRef(CPLString& osStr,
            GDALPDFObject* poObj,
            RemapType& oRemapObjectRefs);
#endif

    public:
        explicit GDALPDFComposerWriter(VSILFILE* fp);
        ~GDALPDFComposerWriter();

        bool Generate(const CPLXMLNode* psComposition);
        void Close();
};

GDALDataset* GDALPDFCreateFromCompositionFile(const char* pszPDFFilename,
                                              const char *pszXMLFilename);


#endif // PDFCREATEFROMCOMPOSITION_H_INCLUDED
