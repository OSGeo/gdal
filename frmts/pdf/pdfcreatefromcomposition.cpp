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

#include "gdal_pdf.h"
#include "pdfcreatecopy.h"

#include <cmath>
#include <cstdlib>

#include "pdfcreatefromcomposition.h"
#include "cpl_conv.h"
#include "cpl_minixml.h"
#include "cpl_vsi_virtual.h"
#include "ogr_geometry.h"

/************************************************************************/
/*                         GDALPDFComposerWriter()                      */
/************************************************************************/

GDALPDFComposerWriter::GDALPDFComposerWriter(VSILFILE* fp):
    GDALPDFBaseWriter(fp)
{
    StartNewDoc();
}

/************************************************************************/
/*                        ~GDALPDFComposerWriter()                      */
/************************************************************************/

GDALPDFComposerWriter::~GDALPDFComposerWriter()
{
    Close();
}

/************************************************************************/
/*                                  Close()                             */
/************************************************************************/

void GDALPDFComposerWriter::Close()
{
    if (m_fp)
    {
        CPLAssert(!m_bInWriteObj);
        if (m_nPageResourceId.toBool())
        {
            WritePages();
            WriteXRefTableAndTrailer(false, 0);
        }
    }
    GDALPDFBaseWriter::Close();
}

/************************************************************************/
/*                          CreateOCGOrder()                            */
/************************************************************************/

GDALPDFArrayRW* GDALPDFComposerWriter::CreateOCGOrder(const TreeOfOCG* parent)
{
    auto poArrayOrder = new GDALPDFArrayRW();
    for( const auto& child: parent->m_children )
    {
        poArrayOrder->Add(child->m_nNum, 0);
        if( !child->m_children.empty() )
        {
            poArrayOrder->Add(CreateOCGOrder(child.get()));
        }
    }
    return poArrayOrder;
}

/************************************************************************/
/*                          CollectOffOCG()                             */
/************************************************************************/

void GDALPDFComposerWriter::CollectOffOCG(std::vector<GDALPDFObjectNum>& ar,
                                          const TreeOfOCG* parent)
{
    if( !parent->m_bInitiallyVisible )
        ar.push_back(parent->m_nNum);
    for( const auto& child: parent->m_children )
    {
        CollectOffOCG(ar, child.get());
    }
}

/************************************************************************/
/*                              WritePages()                            */
/************************************************************************/

void GDALPDFComposerWriter::WritePages()
{
    StartObj(m_nPageResourceId);
    {
        GDALPDFDictionaryRW oDict;
        GDALPDFArrayRW* poKids = new GDALPDFArrayRW();
        oDict.Add("Type", GDALPDFObjectRW::CreateName("Pages"))
             .Add("Count", (int)m_asPageId.size())
             .Add("Kids", poKids);

        for(size_t i=0;i<m_asPageId.size();i++)
            poKids->Add(m_asPageId[i], 0);

        VSIFPrintfL(m_fp, "%s\n", oDict.Serialize().c_str());
    }
    EndObj();

    if (m_nStructTreeRootId.toBool())
    {
        auto nParentTreeId = AllocNewObject();
        StartObj(nParentTreeId);
        VSIFPrintfL(m_fp, "<< /Nums [ ");
        for( size_t i = 0; i < m_anParentElements.size(); i++ )
        {
            VSIFPrintfL(m_fp, "%d %d 0 R ",
                        static_cast<int>(i),
                        m_anParentElements[i].toInt());
        }
        VSIFPrintfL(m_fp, " ] >> \n");
        EndObj();

        StartObj(m_nStructTreeRootId);
        VSIFPrintfL(m_fp,
                    "<< "
                    "/Type /StructTreeRoot "
                    "/ParentTree %d 0 R "
                    "/K [ ", nParentTreeId.toInt());
        for( const auto& num: m_anFeatureLayerId )
        {
            VSIFPrintfL(m_fp, "%d 0 R ", num.toInt());
        }
        VSIFPrintfL(m_fp,"] >>\n");
        EndObj();
    }

    StartObj(m_nCatalogId);
    {
        GDALPDFDictionaryRW oDict;
        oDict.Add("Type", GDALPDFObjectRW::CreateName("Catalog"))
             .Add("Pages", m_nPageResourceId, 0);
        if (m_nOutlinesId.toBool())
            oDict.Add("Outlines", m_nOutlinesId, 0);
        if (m_nXMPId.toBool())
            oDict.Add("Metadata", m_nXMPId, 0);
        if (!m_asOCGs.empty() )
        {
            GDALPDFDictionaryRW* poDictOCProperties = new GDALPDFDictionaryRW();
            oDict.Add("OCProperties", poDictOCProperties);

            GDALPDFDictionaryRW* poDictD = new GDALPDFDictionaryRW();
            poDictOCProperties->Add("D", poDictD);

            if( m_bDisplayLayersOnlyOnVisiblePages )
            {
                poDictD->Add("ListMode",
                             GDALPDFObjectRW::CreateName("VisiblePages"));
            }

            /* Build "Order" array of D dict */
            GDALPDFArrayRW* poArrayOrder = CreateOCGOrder(&m_oTreeOfOGC);
            poDictD->Add("Order", poArrayOrder);

            /* Build "OFF" array of D dict */
            std::vector<GDALPDFObjectNum> offOCGs;
            CollectOffOCG(offOCGs, &m_oTreeOfOGC);
            if( !offOCGs.empty() )
            {
                GDALPDFArrayRW* poArrayOFF = new GDALPDFArrayRW();
                for( const auto& num: offOCGs )
                {
                    poArrayOFF->Add(num, 0);
                }

                poDictD->Add("OFF", poArrayOFF);
            }

            /* Build "RBGroups" array of D dict */
            if( !m_oMapExclusiveOCGIdToOCGs.empty() )
            {
                GDALPDFArrayRW* poArrayRBGroups = new GDALPDFArrayRW();
                for( const auto& group: m_oMapExclusiveOCGIdToOCGs )
                {
                    GDALPDFArrayRW* poGroup = new GDALPDFArrayRW();
                    for( const auto& num: group.second )
                    {
                        poGroup->Add(num, 0);
                    }
                    poArrayRBGroups->Add(poGroup);
                }

                poDictD->Add("RBGroups", poArrayRBGroups);
            }


            GDALPDFArrayRW* poArrayOGCs = new GDALPDFArrayRW();
            for(const auto& ocg: m_asOCGs )
                poArrayOGCs->Add(ocg.nId, 0);
            poDictOCProperties->Add("OCGs", poArrayOGCs);
        }

        if (m_nStructTreeRootId.toBool())
        {
            GDALPDFDictionaryRW* poDictMarkInfo = new GDALPDFDictionaryRW();
            oDict.Add("MarkInfo", poDictMarkInfo);
            poDictMarkInfo->Add("UserProperties", GDALPDFObjectRW::CreateBool(TRUE));

            oDict.Add("StructTreeRoot", m_nStructTreeRootId, 0);
        }

        if (m_nNamesId.toBool())
            oDict.Add("Names", m_nNamesId, 0);

        VSIFPrintfL(m_fp, "%s\n", oDict.Serialize().c_str());
    }
    EndObj();
}

/************************************************************************/
/*                          CreateLayerTree()                           */
/************************************************************************/

bool GDALPDFComposerWriter::CreateLayerTree(const CPLXMLNode* psNode,
                                            const GDALPDFObjectNum& nParentId,
                                            TreeOfOCG* parent)
{
    for(const auto* psIter = psNode->psChild; psIter; psIter = psIter->psNext)
    {
        if( psIter->eType == CXT_Element &&
            strcmp(psIter->pszValue, "Layer") == 0 )
        {
            const char* pszId = CPLGetXMLValue(psIter, "id", nullptr);
            if( !pszId )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Missing id attribute in Layer");
                return false;
            }
            const char* pszName = CPLGetXMLValue(psIter, "name", nullptr);
            if( !pszName )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Missing name attribute in Layer");
                return false;
            }
            if( m_oMapLayerIdToOCG.find(pszId) != m_oMapLayerIdToOCG.end() )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Layer.id = %s is not unique", pszId);
                return false;
            }

            const bool bInitiallyVisible = CPLTestBool(
                CPLGetXMLValue(psIter, "initiallyVisible", "true"));

            const char* pszMutuallyExclusiveGroupId = CPLGetXMLValue(psIter,
                                            "mutuallyExclusiveGroupId", nullptr);

            auto nThisObjId = WriteOCG( pszName, nParentId );
            m_oMapLayerIdToOCG[pszId] = nThisObjId;

            auto newTreeOfOCG = cpl::make_unique<TreeOfOCG>();
            newTreeOfOCG->m_nNum = nThisObjId;
            newTreeOfOCG->m_bInitiallyVisible = bInitiallyVisible;
            parent->m_children.emplace_back(std::move(newTreeOfOCG));

            if( pszMutuallyExclusiveGroupId )
            {
                m_oMapExclusiveOCGIdToOCGs[pszMutuallyExclusiveGroupId].
                    push_back(nThisObjId);
            }

            if( !CreateLayerTree(psIter, nThisObjId,
                                 parent->m_children.back().get()) )
            {
                return false;
            }
        }
    }
    return true;
}


/************************************************************************/
/*                             ParseActions()                           */
/************************************************************************/

bool GDALPDFComposerWriter::ParseActions(const CPLXMLNode* psNode,
                                std::vector<std::unique_ptr<Action>>& actions)
{
    std::set<GDALPDFObjectNum> anONLayers{};
    std::set<GDALPDFObjectNum> anOFFLayers{};
    for(const auto* psIter = psNode->psChild; psIter; psIter = psIter->psNext)
    {
        if( psIter->eType == CXT_Element &&
            strcmp(psIter->pszValue, "GotoPageAction") == 0 )
        {
            auto poAction = cpl::make_unique<GotoPageAction>();
            const char* pszPageId = CPLGetXMLValue(psIter, "pageId", nullptr);
            if( !pszPageId )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Missing pageId attribute in GotoPageAction");
                return false;
            }

            auto oIter = m_oMapPageIdToObjectNum.find(pszPageId);
            if( oIter == m_oMapPageIdToObjectNum.end() )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                        "GotoPageAction.pageId = %s not pointing to a Page.id",
                            pszPageId);
                return false;
            }
            poAction->m_nPageDestId = oIter->second;
            poAction->m_dfX1 = CPLAtof(CPLGetXMLValue(psIter, "x1", "0"));
            poAction->m_dfX2 = CPLAtof(CPLGetXMLValue(psIter, "y1", "0"));
            poAction->m_dfY1 = CPLAtof(CPLGetXMLValue(psIter, "x2", "0"));
            poAction->m_dfY2 = CPLAtof(CPLGetXMLValue(psIter, "y2", "0"));
            actions.push_back(std::move(poAction));
        }
        else if( psIter->eType == CXT_Element &&
                 strcmp(psIter->pszValue, "SetAllLayersStateAction") == 0 )
        {
            if( CPLTestBool(CPLGetXMLValue(psIter, "visible", "true")) )
            {
                for( const auto& ocg: m_asOCGs )
                {
                    anOFFLayers.erase(ocg.nId);
                    anONLayers.insert(ocg.nId);
                }
            }
            else
            {
                for( const auto& ocg: m_asOCGs )
                {
                    anONLayers.erase(ocg.nId);
                    anOFFLayers.insert(ocg.nId);
                }
            }
        }
        else if( psIter->eType == CXT_Element &&
                 strcmp(psIter->pszValue, "SetLayerStateAction") == 0 )
        {
            const char* pszLayerId = CPLGetXMLValue(psIter, "layerId", nullptr);
            if( !pszLayerId )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                        "Missing layerId");
                return false;
            }
            auto oIter = m_oMapLayerIdToOCG.find(pszLayerId);
            if( oIter == m_oMapLayerIdToOCG.end() )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                        "Referencing layer of unknown id: %s", pszLayerId);
                return false;
            }
            const auto& ocg = oIter->second;

            if( CPLTestBool(CPLGetXMLValue(psIter, "visible", "true")) )
            {
                anOFFLayers.erase(ocg);
                anONLayers.insert(ocg);
            }
            else
            {
                anONLayers.erase(ocg);
                anOFFLayers.insert(ocg);
            }
        }
        else if( psIter->eType == CXT_Element &&
                 strcmp(psIter->pszValue, "JavascriptAction") == 0 )
        {
            auto poAction = cpl::make_unique<JavascriptAction>();
            poAction->m_osScript = CPLGetXMLValue(psIter, nullptr, "");
            actions.push_back(std::move(poAction));
        }
    }

    if( !anONLayers.empty() || !anOFFLayers.empty() )
    {
        auto poAction = cpl::make_unique<SetLayerStateAction>();
        poAction->m_anONLayers = std::move(anONLayers);
        poAction->m_anOFFLayers = std::move(anOFFLayers);
        actions.push_back(std::move(poAction));
    }

    return true;
}

/************************************************************************/
/*                       CreateOutlineFirstPass()                       */
/************************************************************************/

bool GDALPDFComposerWriter::CreateOutlineFirstPass(const CPLXMLNode* psNode,
                                          OutlineItem* poParentItem)
{
    for(const auto* psIter = psNode->psChild; psIter; psIter = psIter->psNext)
    {
        if( psIter->eType == CXT_Element &&
            strcmp(psIter->pszValue, "OutlineItem") == 0 )
        {
            auto newItem = cpl::make_unique<OutlineItem>();
            const char* pszName = CPLGetXMLValue(psIter, "name", nullptr);
            if( !pszName )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Missing name attribute in OutlineItem");
                return false;
            }
            newItem->m_osName = pszName;
            newItem->m_bOpen =
                CPLTestBool(CPLGetXMLValue(psIter, "open", "true"));
            if( CPLTestBool(CPLGetXMLValue(psIter, "italic", "false")) )
                newItem->m_nFlags |= 1 << 0;
            if( CPLTestBool(CPLGetXMLValue(psIter, "bold", "false")) )
                newItem->m_nFlags |= 1 << 1;

            const auto poActions = CPLGetXMLNode(psIter, "Actions");
            if( poActions )
            {
                if( !ParseActions(poActions, newItem->m_aoActions) )
                    return false;
            }

            newItem->m_nObjId = AllocNewObject();
            if( !CreateOutlineFirstPass(psIter, newItem.get()) )
            {
                return false;
            }
            poParentItem->m_nKidsRecCount += 1 + newItem->m_nKidsRecCount;
            poParentItem->m_aoKids.push_back(std::move(newItem));
        }
    }
    return true;
}

/************************************************************************/
/*                            SerializeActions()                        */
/************************************************************************/

GDALPDFDictionaryRW* GDALPDFComposerWriter::SerializeActions(
                        GDALPDFDictionaryRW* poDictForDest,
                        const std::vector<std::unique_ptr<Action>>& actions)
{
    GDALPDFDictionaryRW* poRetAction = nullptr;
    GDALPDFDictionaryRW* poLastActionDict = nullptr;
    for( const auto& poAction: actions )
    {
        GDALPDFDictionaryRW* poActionDict = nullptr;
        auto poGotoPageAction = dynamic_cast<GotoPageAction*>(poAction.get());
        if( poGotoPageAction )
        {
            GDALPDFArrayRW* poDest = new GDALPDFArrayRW;
            poDest->Add(poGotoPageAction->m_nPageDestId, 0);
            if( poGotoPageAction->m_dfX1 == 0.0 &&
                poGotoPageAction->m_dfX2 == 0.0 &&
                poGotoPageAction->m_dfY1 == 0.0 &&
                poGotoPageAction->m_dfY2 == 0.0 )
            {
                poDest->Add(GDALPDFObjectRW::CreateName("XYZ"))
                        .Add(GDALPDFObjectRW::CreateNull())
                        .Add(GDALPDFObjectRW::CreateNull())
                        .Add(GDALPDFObjectRW::CreateNull());
            }
            else
            {
                poDest->Add(GDALPDFObjectRW::CreateName("FitR"))
                        .Add(poGotoPageAction->m_dfX1)
                        .Add(poGotoPageAction->m_dfY1)
                        .Add(poGotoPageAction->m_dfX2)
                        .Add(poGotoPageAction->m_dfY2);
            }
            if( poDictForDest && actions.size() == 1 )
            {
                poDictForDest->Add("Dest", poDest);
            }
            else
            {
                poActionDict = new GDALPDFDictionaryRW();
                poActionDict->Add("Type", GDALPDFObjectRW::CreateName("Action"));
                poActionDict->Add("S", GDALPDFObjectRW::CreateName("GoTo"));
                poActionDict->Add("D", poDest);
            }
        }

        auto setLayerStateAction = dynamic_cast<SetLayerStateAction*>(poAction.get());
        if( poActionDict == nullptr && setLayerStateAction )
        {
            poActionDict = new GDALPDFDictionaryRW();
            poActionDict->Add("Type", GDALPDFObjectRW::CreateName("Action"));
            poActionDict->Add("S", GDALPDFObjectRW::CreateName("SetOCGState"));
            auto poStateArray = new GDALPDFArrayRW();
            if( !setLayerStateAction->m_anOFFLayers.empty() )
            {
                poStateArray->Add(GDALPDFObjectRW::CreateName("OFF"));
                for( const auto& ocg: setLayerStateAction->m_anOFFLayers )
                    poStateArray->Add(ocg, 0);
            }
            if( !setLayerStateAction->m_anONLayers.empty() )
            {
                poStateArray->Add(GDALPDFObjectRW::CreateName("ON"));
                for( const auto& ocg: setLayerStateAction->m_anONLayers )
                    poStateArray->Add(ocg, 0);
            }
            poActionDict->Add("State", poStateArray);
        }

        auto javascriptAction = dynamic_cast<JavascriptAction*>(poAction.get());
        if( poActionDict == nullptr && javascriptAction )
        {
            poActionDict = new GDALPDFDictionaryRW();
            poActionDict->Add("Type", GDALPDFObjectRW::CreateName("Action"));
            poActionDict->Add("S", GDALPDFObjectRW::CreateName("JavaScript"));
            poActionDict->Add("JS", javascriptAction->m_osScript);
        }

        if( poActionDict )
        {
            if( poLastActionDict == nullptr )
            {
                poRetAction = poActionDict;
            }
            else
            {
                poLastActionDict->Add("Next", poActionDict);
            }
            poLastActionDict = poActionDict;
        }
    }
    return poRetAction;
}

/************************************************************************/
/*                        SerializeOutlineKids()                        */
/************************************************************************/

bool GDALPDFComposerWriter::SerializeOutlineKids(const OutlineItem* poParentItem)
{
    for( size_t i = 0; i < poParentItem->m_aoKids.size(); i++ )
    {
        const auto& poItem = poParentItem->m_aoKids[i];
        StartObj(poItem->m_nObjId);
        GDALPDFDictionaryRW oDict;
        oDict.Add("Title", poItem->m_osName);

        auto poActionDict = SerializeActions(&oDict, poItem->m_aoActions);
        if( poActionDict )
        {
            oDict.Add("A", poActionDict);
        }

        if( i > 0 )
        {
            oDict.Add("Prev", poParentItem->m_aoKids[i-1]->m_nObjId, 0);
        }
        if( i + 1 < poParentItem->m_aoKids.size() )
        {
            oDict.Add("Next", poParentItem->m_aoKids[i+1]->m_nObjId, 0);
        }
        if( poItem->m_nFlags )
            oDict.Add("F", poItem->m_nFlags);
        oDict.Add("Parent", poParentItem->m_nObjId, 0);
        if( !poItem->m_aoKids.empty() )
        {
            oDict.Add("First", poItem->m_aoKids.front()->m_nObjId, 0);
            oDict.Add("Last", poItem->m_aoKids.back()->m_nObjId, 0);
            oDict.Add("Count", poItem->m_bOpen ?
                poItem->m_nKidsRecCount : -poItem->m_nKidsRecCount);
        }
        VSIFPrintfL(m_fp, "%s\n", oDict.Serialize().c_str());
        EndObj();
        SerializeOutlineKids(poItem.get());
    }
    return true;
}

/************************************************************************/
/*                           CreateOutline()                            */
/************************************************************************/

bool GDALPDFComposerWriter::CreateOutline(const CPLXMLNode* psNode)
{
    OutlineItem oRootOutlineItem;
    if( !CreateOutlineFirstPass(psNode, &oRootOutlineItem) )
        return false;
    if( oRootOutlineItem.m_aoKids.empty() )
        return true;

    m_nOutlinesId = AllocNewObject();
    StartObj(m_nOutlinesId);
    GDALPDFDictionaryRW oDict;
    oDict.Add("Type", GDALPDFObjectRW::CreateName("Outlines"))
         .Add("First", oRootOutlineItem.m_aoKids.front()->m_nObjId, 0)
         .Add("Last", oRootOutlineItem.m_aoKids.back()->m_nObjId, 0)
         .Add("Count", oRootOutlineItem.m_nKidsRecCount);
    VSIFPrintfL(m_fp, "%s\n", oDict.Serialize().c_str());
    EndObj();
    oRootOutlineItem.m_nObjId = m_nOutlinesId;
    return SerializeOutlineKids(&oRootOutlineItem);
}

/************************************************************************/
/*                        GenerateGeoreferencing()                      */
/************************************************************************/


bool GDALPDFComposerWriter::GenerateGeoreferencing(const CPLXMLNode* psGeoreferencing,
                                                   double dfWidthInUserUnit,
                                                   double dfHeightInUserUnit,
                                                   GDALPDFObjectNum& nViewportId,
                                                   GDALPDFObjectNum& nLGIDictId,
                                                   Georeferencing& georeferencing)
{
    double bboxX1 = 0;
    double bboxY1 = 0;
    double bboxX2 = dfWidthInUserUnit;
    double bboxY2 = dfHeightInUserUnit;
    const auto psBoundingBox = CPLGetXMLNode(psGeoreferencing, "BoundingBox");
    if( psBoundingBox )
    {
        bboxX1 = CPLAtof(
            CPLGetXMLValue(psBoundingBox, "x1", CPLSPrintf("%.18g", bboxX1)));
        bboxY1 = CPLAtof(
            CPLGetXMLValue(psBoundingBox, "y1", CPLSPrintf("%.18g", bboxY1)));
        bboxX2 = CPLAtof(
            CPLGetXMLValue(psBoundingBox, "x2", CPLSPrintf("%.18g", bboxX2)));
        bboxY2 = CPLAtof(
            CPLGetXMLValue(psBoundingBox, "y2", CPLSPrintf("%.18g", bboxY2)));
        if( bboxX2 <= bboxX1 || bboxY2 <= bboxY1 )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Invalid BoundingBox");
            return false;
        }
    }

    std::vector<GDAL_GCP> aGCPs;
    for(const auto* psIter = psGeoreferencing->psChild;
        psIter; psIter = psIter->psNext)
    {
        if( psIter->eType == CXT_Element &&
            strcmp(psIter->pszValue, "ControlPoint") == 0 )
        {
            const char* pszx = CPLGetXMLValue(psIter, "x", nullptr);
            const char* pszy = CPLGetXMLValue(psIter, "y", nullptr);
            const char* pszX = CPLGetXMLValue(psIter, "GeoX", nullptr);
            const char* pszY = CPLGetXMLValue(psIter, "GeoY", nullptr);
            if( !pszx || !pszy || !pszX || !pszY )
            {
                CPLError(CE_Failure, CPLE_NotSupported,
                         "At least one of x, y, GeoX or GeoY attribute "
                         "missing on ControlPoint");
                return false;
            }
            GDAL_GCP gcp;
            gcp.pszId = nullptr;
            gcp.pszInfo = nullptr;
            gcp.dfGCPPixel = CPLAtof(pszx);
            gcp.dfGCPLine = CPLAtof(pszy);
            gcp.dfGCPX = CPLAtof(pszX);
            gcp.dfGCPY = CPLAtof(pszY);
            gcp.dfGCPZ = 0;
            aGCPs.emplace_back(std::move(gcp));
        }
    }
    if( aGCPs.size() < 4 )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "At least 4 ControlPoint are required");
        return false;
    }

    const char* pszBoundingPolygon =
        CPLGetXMLValue(psGeoreferencing, "BoundingPolygon", nullptr);
    std::vector<xyPair> aBoundingPolygon;
    if( pszBoundingPolygon )
    {
        OGRGeometry* poGeom = nullptr;
        OGRGeometryFactory::createFromWkt(pszBoundingPolygon, nullptr, &poGeom);
        if( poGeom && poGeom->getGeometryType() == wkbPolygon )
        {
            auto poPoly = poGeom->toPolygon();
            auto poRing = poPoly->getExteriorRing();
            if( poRing )
            {
                if( psBoundingBox == nullptr )
                {
                    OGREnvelope sEnvelope;
                    poRing->getEnvelope(&sEnvelope);
                    bboxX1 = sEnvelope.MinX;
                    bboxY1 = sEnvelope.MinY;
                    bboxX2 = sEnvelope.MaxX;
                    bboxY2 = sEnvelope.MaxY;
                }
                for( int i = 0; i < poRing->getNumPoints(); i++ )
                {
                    aBoundingPolygon.emplace_back(
                        xyPair(poRing->getX(i), poRing->getY(i)));
                }
            }
        }
        delete poGeom;
    }

    const auto pszSRS = CPLGetXMLValue(psGeoreferencing, "SRS", nullptr);
    if( !pszSRS )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Missing SRS");
        return false;
    }
    auto poSRS = cpl::make_unique<OGRSpatialReference>();
    if( poSRS->SetFromUserInput(pszSRS) != OGRERR_NONE )
    {
        return false;
    }
    poSRS->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);

    if( CPLTestBool(CPLGetXMLValue(psGeoreferencing, "ISO32000ExtensionFormat", "true")) )
    {
        nViewportId = GenerateISO32000_Georeferencing(
            OGRSpatialReference::ToHandle(poSRS.get()),
            bboxX1, bboxY1, bboxX2, bboxY2, aGCPs, aBoundingPolygon);
        if( !nViewportId.toBool() )
        {
            return false;
        }
    }

    if( CPLTestBool(CPLGetXMLValue(psGeoreferencing, "OGCBestPracticeFormat", "false")) )
    {
        nLGIDictId = GenerateOGC_BP_Georeferencing(
            OGRSpatialReference::ToHandle(poSRS.get()),
            bboxX1, bboxY1, bboxX2, bboxY2, aGCPs, aBoundingPolygon);
        if( !nLGIDictId.toBool() )
        {
            return false;
        }
    }

    const char* pszId = CPLGetXMLValue(psGeoreferencing, "id", nullptr);
    if( pszId )
    {
        if (!GDALGCPsToGeoTransform( static_cast<int>(aGCPs.size()),
                                     aGCPs.data(),
                                     georeferencing.m_adfGT, TRUE))
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Could not compute geotransform with approximate match.");
            return false;
        }
        if( std::fabs(georeferencing.m_adfGT[2]) < 1e-5 *
                    std::fabs(georeferencing.m_adfGT[1]) &&
            std::fabs(georeferencing.m_adfGT[4]) < 1e-5 *
                    std::fabs(georeferencing.m_adfGT[5]) )
        {
            georeferencing.m_adfGT[2] = 0;
            georeferencing.m_adfGT[4] = 0;
        }
        if( georeferencing.m_adfGT[2] != 0 ||
            georeferencing.m_adfGT[4] != 0 ||
            georeferencing.m_adfGT[5] < 0 )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Geotransform should define a north-up non rotated area.");
            return false;
        }
        georeferencing.m_osID = pszId;
        georeferencing.m_oSRS = *(poSRS.get());
        georeferencing.m_bboxX1 = bboxX1;
        georeferencing.m_bboxY1 = bboxY1;
        georeferencing.m_bboxX2 = bboxX2;
        georeferencing.m_bboxY2 = bboxY2;
    }

    return true;
}

/************************************************************************/
/*                      GenerateISO32000_Georeferencing()               */
/************************************************************************/

GDALPDFObjectNum GDALPDFComposerWriter::GenerateISO32000_Georeferencing(
    OGRSpatialReferenceH hSRS,
    double bboxX1, double bboxY1, double bboxX2, double bboxY2,
    const std::vector<GDAL_GCP>& aGCPs,
    const std::vector<xyPair>& aBoundingPolygon)
{
    OGRSpatialReferenceH hSRSGeog = OSRCloneGeogCS(hSRS);
    if( hSRSGeog == nullptr )
    {
        return GDALPDFObjectNum();
    }
    OSRSetAxisMappingStrategy(hSRSGeog, OAMS_TRADITIONAL_GIS_ORDER);
    OGRCoordinateTransformationH hCT = OCTNewCoordinateTransformation( hSRS, hSRSGeog);
    if( hCT == nullptr )
    {
        OSRDestroySpatialReference(hSRSGeog);
        return GDALPDFObjectNum();
    }

    std::vector<GDAL_GCP> aGCPReprojected;
    bool bSuccess = true;
    for( const auto& gcp: aGCPs )
    {
        double X = gcp.dfGCPX;
        double Y = gcp.dfGCPY;
        bSuccess &= OCTTransform( hCT, 1,&X, &Y, nullptr ) == 1;
        GDAL_GCP newGCP;
        newGCP.pszId = nullptr;
        newGCP.pszInfo = nullptr;
        newGCP.dfGCPPixel = gcp.dfGCPPixel;
        newGCP.dfGCPLine = gcp.dfGCPLine;
        newGCP.dfGCPX = X;
        newGCP.dfGCPY = Y;
        newGCP.dfGCPZ = 0;
        aGCPReprojected.emplace_back(std::move(newGCP));
    }
    if( !bSuccess )
    {
        OSRDestroySpatialReference(hSRSGeog);
        OCTDestroyCoordinateTransformation(hCT);

        return GDALPDFObjectNum();
    }

    const char * pszAuthorityCode = OSRGetAuthorityCode( hSRS, nullptr );
    const char * pszAuthorityName = OSRGetAuthorityName( hSRS, nullptr );
    int nEPSGCode = 0;
    if( pszAuthorityName != nullptr && EQUAL(pszAuthorityName, "EPSG") &&
        pszAuthorityCode != nullptr )
        nEPSGCode = atoi(pszAuthorityCode);

    int bIsGeographic = OSRIsGeographic(hSRS);

    char* pszESRIWKT = nullptr;
    const char* apszOptions[] = { "FORMAT=WKT1_ESRI", nullptr };
    OSRExportToWktEx(hSRS, &pszESRIWKT, apszOptions);

    OSRDestroySpatialReference(hSRSGeog);
    OCTDestroyCoordinateTransformation(hCT);

    auto nViewportId = AllocNewObject();
    auto nMeasureId = AllocNewObject();
    auto nGCSId = AllocNewObject();

    StartObj(nViewportId);
    GDALPDFDictionaryRW oViewPortDict;
    oViewPortDict.Add("Type", GDALPDFObjectRW::CreateName("Viewport"))
                .Add("Name", "Layer")
                .Add("BBox", &((new GDALPDFArrayRW())
                                ->Add(bboxX1).Add(bboxY1)
                                 .Add(bboxX2).Add(bboxY2)))
                .Add("Measure", nMeasureId, 0);
    VSIFPrintfL(m_fp, "%s\n", oViewPortDict.Serialize().c_str());
    EndObj();

    GDALPDFArrayRW* poGPTS = new GDALPDFArrayRW();
    GDALPDFArrayRW* poLPTS = new GDALPDFArrayRW();

    const int nPrecision =
        atoi(CPLGetConfigOption("PDF_COORD_DOUBLE_PRECISION", "16"));
    for( const auto& gcp: aGCPReprojected )
    {
        poGPTS->AddWithPrecision(gcp.dfGCPY, nPrecision).
                AddWithPrecision(gcp.dfGCPX, nPrecision); // Lat, long order
        poLPTS->AddWithPrecision((gcp.dfGCPPixel - bboxX1) / (bboxX2 - bboxX1), nPrecision).
                AddWithPrecision((gcp.dfGCPLine - bboxY1) / (bboxY2 - bboxY1), nPrecision);
    }

    StartObj(nMeasureId);
    GDALPDFDictionaryRW oMeasureDict;
    oMeasureDict .Add("Type", GDALPDFObjectRW::CreateName("Measure"))
                 .Add("Subtype", GDALPDFObjectRW::CreateName("GEO"))
                 .Add("GPTS", poGPTS)
                 .Add("LPTS", poLPTS)
                 .Add("GCS", nGCSId, 0);
    if( !aBoundingPolygon.empty() )
    {
        GDALPDFArrayRW* poBounds = new GDALPDFArrayRW();
        for( const auto& xy: aBoundingPolygon )
        {
             poBounds->Add((xy.x - bboxX1) / (bboxX2 - bboxX1)).
                       Add((xy.y - bboxY1) / (bboxY2 - bboxY1));
        }
        oMeasureDict.Add("Bounds", poBounds);
    }
    VSIFPrintfL(m_fp, "%s\n", oMeasureDict.Serialize().c_str());
    EndObj();

    StartObj(nGCSId);
    GDALPDFDictionaryRW oGCSDict;
    oGCSDict.Add("Type", GDALPDFObjectRW::CreateName(bIsGeographic ? "GEOGCS" : "PROJCS"))
            .Add("WKT", pszESRIWKT);
    if (nEPSGCode)
        oGCSDict.Add("EPSG", nEPSGCode);
    VSIFPrintfL(m_fp, "%s\n", oGCSDict.Serialize().c_str());
    EndObj();

    CPLFree(pszESRIWKT);

    return nViewportId;
}

/************************************************************************/
/*                      GenerateOGC_BP_Georeferencing()                 */
/************************************************************************/

GDALPDFObjectNum GDALPDFComposerWriter::GenerateOGC_BP_Georeferencing(
    OGRSpatialReferenceH hSRS,
    double bboxX1, double bboxY1, double bboxX2, double bboxY2,
    const std::vector<GDAL_GCP>& aGCPs,
    const std::vector<xyPair>& aBoundingPolygon)
{
    const OGRSpatialReference* poSRS = OGRSpatialReference::FromHandle(hSRS);
    GDALPDFDictionaryRW* poProjectionDict = GDALPDFBuildOGC_BP_Projection(poSRS);
    if (poProjectionDict == nullptr)
    {
        OSRDestroySpatialReference(hSRS);
        return GDALPDFObjectNum();
    }

    GDALPDFArrayRW* poNeatLineArray = new GDALPDFArrayRW();
    if( !aBoundingPolygon.empty() )
    {
        for( const auto& xy: aBoundingPolygon )
        {
             poNeatLineArray->Add(xy.x).Add(xy.y);
        }
    }
    else
    {
        poNeatLineArray->Add(bboxX1).Add(bboxY1).
                         Add(bboxX2).Add(bboxY2);
    }

    GDALPDFArrayRW* poRegistration = new GDALPDFArrayRW();

    for( const auto& gcp: aGCPs )
    {
        GDALPDFArrayRW* poGCP = new GDALPDFArrayRW();
        poGCP->Add(gcp.dfGCPPixel, TRUE).Add(gcp.dfGCPLine, TRUE).
               Add(gcp.dfGCPX, TRUE).Add(gcp.dfGCPY, TRUE);
        poRegistration->Add(poGCP);
    }

    auto nLGIDictId = AllocNewObject();
    StartObj(nLGIDictId);
    GDALPDFDictionaryRW oLGIDict;
    oLGIDict.Add("Type", GDALPDFObjectRW::CreateName("LGIDict"))
            .Add("Version", "2.1")
            .Add("Neatline", poNeatLineArray);

    oLGIDict.Add("Registration", poRegistration);

    /* GDAL extension */
    if( CPLTestBool( CPLGetConfigOption("GDAL_PDF_OGC_BP_WRITE_WKT", "TRUE") ) )
    {
        char* pszWKT = nullptr;
        OSRExportToWkt(hSRS, &pszWKT);
        if( pszWKT )
            poProjectionDict->Add("WKT", pszWKT);
        CPLFree(pszWKT);
    }

    oLGIDict.Add("Projection", poProjectionDict);

    VSIFPrintfL(m_fp, "%s\n", oLGIDict.Serialize().c_str());
    EndObj();

    return nLGIDictId;
}

/************************************************************************/
/*                         GeneratePage()                               */
/************************************************************************/


bool GDALPDFComposerWriter::GeneratePage(const CPLXMLNode* psPage)
{
    double dfWidthInUserUnit = CPLAtof(CPLGetXMLValue(psPage, "Width", "-1"));
    double dfHeightInUserUnit = CPLAtof(CPLGetXMLValue(psPage, "Height", "-1"));
    if( dfWidthInUserUnit <= 0 || dfWidthInUserUnit >= MAXIMUM_SIZE_IN_UNITS ||
        dfHeightInUserUnit <= 0 || dfHeightInUserUnit >= MAXIMUM_SIZE_IN_UNITS )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Missing or invalid Width and/or Height");
        return false;
    }
    double dfUserUnit = CPLAtof(CPLGetXMLValue(psPage, "DPI",
                        CPLSPrintf("%f", DEFAULT_DPI))) * USER_UNIT_IN_INCH;

    std::vector<GDALPDFObjectNum> anViewportIds;
    std::vector<GDALPDFObjectNum> anLGIDictIds;

    PageContext oPageContext;
    for(const auto* psIter = psPage->psChild; psIter; psIter = psIter->psNext)
    {
        if( psIter->eType == CXT_Element &&
            strcmp(psIter->pszValue, "Georeferencing") == 0 )
        {
            GDALPDFObjectNum nViewportId;
            GDALPDFObjectNum nLGIDictId;
            Georeferencing georeferencing;
            if( !GenerateGeoreferencing(psIter,
                                        dfWidthInUserUnit, dfHeightInUserUnit,
                                        nViewportId, nLGIDictId,
                                        georeferencing) )
            {
                return false;
            }
            if( nViewportId.toBool() )
                anViewportIds.emplace_back(nViewportId);
            if( nLGIDictId.toBool() )
                anLGIDictIds.emplace_back(nLGIDictId);
            if( !georeferencing.m_osID.empty() )
            {
                oPageContext.m_oMapGeoreferencedId[georeferencing.m_osID] =
                    georeferencing;
            }
        }
    }

    auto nPageId = AllocNewObject();
    m_asPageId.push_back(nPageId);

    const char* pszId = CPLGetXMLValue(psPage, "id", nullptr);
    if( pszId )
    {
        if( m_oMapPageIdToObjectNum.find(pszId) != m_oMapPageIdToObjectNum.end() )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Duplicated page id %s", pszId);
            return false;
        }
        m_oMapPageIdToObjectNum[pszId] = nPageId;
    }

    const auto psContent = CPLGetXMLNode(psPage, "Content");
    if( !psContent )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Missing Content");
        return false;
    }

    const bool bDeflateStreamCompression = EQUAL(
        CPLGetXMLValue(psContent, "streamCompression", "DEFLATE"), "DEFLATE");

    oPageContext.m_dfWidthInUserUnit = dfWidthInUserUnit;
    oPageContext.m_dfHeightInUserUnit = dfHeightInUserUnit;
    oPageContext.m_eStreamCompressMethod =
        bDeflateStreamCompression ? COMPRESS_DEFLATE : COMPRESS_NONE;
    if( !ExploreContent(psContent, oPageContext) )
        return false;

    int nStructParentsIdx = -1;
    if( !oPageContext.m_anFeatureUserProperties.empty() )
    {
        nStructParentsIdx = static_cast<int>(m_anParentElements.size());
        auto nParentsElements = AllocNewObject();
        m_anParentElements.push_back(nParentsElements);
        {
            StartObj(nParentsElements);
            VSIFPrintfL(m_fp, "[ ");
            for( const auto& num: oPageContext.m_anFeatureUserProperties )
                VSIFPrintfL(m_fp, "%d 0 R ", num.toInt());
            VSIFPrintfL(m_fp, " ]\n");
            EndObj();
        }
    }

    GDALPDFObjectNum nAnnotsId;
    if( !oPageContext.m_anAnnotationsId.empty() )
    {
        /* -------------------------------------------------------------- */
        /*  Write annotation arrays.                                      */
        /* -------------------------------------------------------------- */
        nAnnotsId = AllocNewObject();
        StartObj(nAnnotsId);
        {
            GDALPDFArrayRW oArray;
            for(size_t i = 0; i < oPageContext.m_anAnnotationsId.size(); i++)
            {
                oArray.Add(oPageContext.m_anAnnotationsId[i], 0);
            }
            VSIFPrintfL(m_fp, "%s\n", oArray.Serialize().c_str());
        }
        EndObj();
    }

    auto nContentId = AllocNewObject();
    auto nResourcesId = AllocNewObject();

    StartObj(nPageId);
    GDALPDFDictionaryRW oDictPage;
    oDictPage.Add("Type", GDALPDFObjectRW::CreateName("Page"))
             .Add("Parent", m_nPageResourceId, 0)
             .Add("MediaBox", &((new GDALPDFArrayRW())
                               ->Add(0).Add(0).
                                 Add(dfWidthInUserUnit).
                                 Add(dfHeightInUserUnit)))
             .Add("UserUnit", dfUserUnit)
             .Add("Contents", nContentId, 0)
             .Add("Resources", nResourcesId, 0);

    if( nAnnotsId.toBool() )
        oDictPage.Add("Annots", nAnnotsId, 0);

    oDictPage.Add("Group",
                    &((new GDALPDFDictionaryRW())
                    ->Add("Type", GDALPDFObjectRW::CreateName("Group"))
                        .Add("S", GDALPDFObjectRW::CreateName("Transparency"))
                        .Add("CS", GDALPDFObjectRW::CreateName("DeviceRGB"))));
    if (!anViewportIds.empty())
    {
        auto poViewports = new GDALPDFArrayRW();
        for( const auto& id: anViewportIds )
            poViewports->Add(id, 0);
        oDictPage.Add("VP", poViewports);
    }

    if (anLGIDictIds.size() == 1 )
    {
        oDictPage.Add("LGIDict", anLGIDictIds[0], 0);
    }
    else if (!anLGIDictIds.empty())
    {
        auto poLGIDict = new GDALPDFArrayRW();
        for( const auto& id: anLGIDictIds )
            poLGIDict->Add(id, 0);
        oDictPage.Add("LGIDict", poLGIDict);
    }

    if( nStructParentsIdx >= 0 )
    {
        oDictPage.Add("StructParents", nStructParentsIdx);
    }

    VSIFPrintfL(m_fp, "%s\n", oDictPage.Serialize().c_str());
    EndObj();

    /* -------------------------------------------------------------- */
    /*  Write content dictionary                                      */
    /* -------------------------------------------------------------- */
    {
        GDALPDFDictionaryRW oDict;
        StartObjWithStream(nContentId, oDict, bDeflateStreamCompression);
        VSIFPrintfL(m_fp, "%s", oPageContext.m_osDrawingStream.c_str());
        EndObjWithStream();
    }

    /* -------------------------------------------------------------- */
    /*  Write page resource dictionary.                               */
    /* -------------------------------------------------------------- */
    StartObj(nResourcesId);
    {
        GDALPDFDictionaryRW oDict;
        if( !oPageContext.m_oXObjects.empty() )
        {
            GDALPDFDictionaryRW* poDict = new GDALPDFDictionaryRW();
            for( const auto&kv: oPageContext.m_oXObjects )
            {
                poDict->Add(kv.first, kv.second, 0);
            }
            oDict.Add("XObject", poDict);
        }

        if( !oPageContext.m_oProperties.empty() )
        {
            GDALPDFDictionaryRW* poDict = new GDALPDFDictionaryRW();
            for( const auto&kv: oPageContext.m_oProperties )
            {
                poDict->Add(kv.first, kv.second, 0);
            }
            oDict.Add("Properties", poDict);
        }

        if( !oPageContext.m_oExtGState.empty() )
        {
            GDALPDFDictionaryRW* poDict = new GDALPDFDictionaryRW();
            for( const auto&kv: oPageContext.m_oExtGState )
            {
                poDict->Add(kv.first, kv.second, 0);
            }
            oDict.Add("ExtGState", poDict);
        }

        VSIFPrintfL(m_fp, "%s\n", oDict.Serialize().c_str());
    }
    EndObj();

    return true;
}

/************************************************************************/
/*                          ExploreContent()                            */
/************************************************************************/

bool GDALPDFComposerWriter::ExploreContent(const CPLXMLNode* psNode,
                                           PageContext& oPageContext)
{
    for(const auto* psIter = psNode->psChild; psIter; psIter = psIter->psNext)
    {
        if( psIter->eType == CXT_Element &&
            strcmp(psIter->pszValue, "IfLayerOn") == 0 )
        {
            const char* pszLayerId = CPLGetXMLValue(psIter, "layerId", nullptr);
            if( !pszLayerId )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                        "Missing layerId");
                return false;
            }
            auto oIter = m_oMapLayerIdToOCG.find(pszLayerId);
            if( oIter == m_oMapLayerIdToOCG.end() )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                        "Referencing layer of unknown id: %s", pszLayerId);
                return false;
            }
            oPageContext.m_oProperties[
                CPLOPrintf("Lyr%d", oIter->second.toInt())] = oIter->second;
            oPageContext.m_osDrawingStream +=
                CPLOPrintf("/OC /Lyr%d BDC\n", oIter->second.toInt());
            if( !ExploreContent(psIter, oPageContext) )
                return false;
            oPageContext.m_osDrawingStream += "EMC\n";
        }

        else if( psIter->eType == CXT_Element &&
            strcmp(psIter->pszValue, "Raster") == 0 )
        {
            if( !WriteRaster(psIter, oPageContext) )
                return false;
        }

        else if( psIter->eType == CXT_Element &&
            strcmp(psIter->pszValue, "Vector") == 0 )
        {
            if( !WriteVector(psIter, oPageContext) )
                return false;
        }

        else if( psIter->eType == CXT_Element &&
            strcmp(psIter->pszValue, "VectorLabel") == 0 )
        {
            if( !WriteVectorLabel(psIter, oPageContext) )
                return false;
        }

        else if( psIter->eType == CXT_Element &&
            strcmp(psIter->pszValue, "PDF") == 0 )
        {
#ifdef HAVE_PDF_READ_SUPPORT
            if( !WritePDF(psIter, oPageContext) )
                return false;
#else
            CPLError(CE_Failure, CPLE_NotSupported,
                    "PDF node not supported due to missing PDF read support in this GDAL build");
            return false;
#endif
        }
    }
    return true;
}

/************************************************************************/
/*                          StartBlending()                             */
/************************************************************************/

void GDALPDFComposerWriter::StartBlending(const CPLXMLNode* psNode,
                                          PageContext& oPageContext,
                                          double& dfOpacity)
{
    dfOpacity = 1;
    const auto psBlending = CPLGetXMLNode(psNode, "Blending");
    if( psBlending )
    {
        auto nExtGState = AllocNewObject();
        StartObj(nExtGState);
        {
            GDALPDFDictionaryRW gs;
            gs.Add("Type", GDALPDFObjectRW::CreateName("ExtGState"));
            dfOpacity = CPLAtof(CPLGetXMLValue(
                psBlending, "opacity", "1"));
            gs.Add("ca", dfOpacity);
            gs.Add("BM", GDALPDFObjectRW::CreateName(
                CPLGetXMLValue(psBlending, "function", "Normal")));
            VSIFPrintfL(m_fp, "%s\n", gs.Serialize().c_str());
        }
        EndObj();
        oPageContext.m_oExtGState[
            CPLOPrintf("GS%d", nExtGState.toInt())] = nExtGState;
        oPageContext.m_osDrawingStream += "q\n";
        oPageContext.m_osDrawingStream +=
            CPLOPrintf("/GS%d gs\n", nExtGState.toInt());
    }
}

/************************************************************************/
/*                          EndBlending()                             */
/************************************************************************/

void GDALPDFComposerWriter::EndBlending(const CPLXMLNode* psNode,
                                        PageContext& oPageContext)
{
    const auto psBlending = CPLGetXMLNode(psNode, "Blending");
    if( psBlending )
    {
        oPageContext.m_osDrawingStream += "Q\n";
    }
}

/************************************************************************/
/*                           WriteRaster()                              */
/************************************************************************/

bool GDALPDFComposerWriter::WriteRaster(const CPLXMLNode* psNode,
                                        PageContext& oPageContext)
{
    const char* pszDataset = CPLGetXMLValue(psNode, "dataset", nullptr);
    if( !pszDataset )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                "Missing dataset");
        return false;
    }
    double dfX1 = CPLAtof(CPLGetXMLValue(psNode, "x1", "0"));
    double dfY1 = CPLAtof(CPLGetXMLValue(psNode, "y1", "0"));
    double dfX2 = CPLAtof(CPLGetXMLValue(psNode, "x2",
        CPLSPrintf("%.18g", oPageContext.m_dfWidthInUserUnit)));
    double dfY2 = CPLAtof(CPLGetXMLValue(psNode, "y2",
        CPLSPrintf("%.18g", oPageContext.m_dfHeightInUserUnit)));
    if( dfX2 <= dfX1 || dfY2 <= dfY1 )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Invalid x1,y1,x2,y2");
        return false;
    }
    GDALDatasetUniquePtr poDS(GDALDataset::Open(
        pszDataset, GDAL_OF_RASTER | GDAL_OF_VERBOSE_ERROR,
        nullptr, nullptr, nullptr));
    if( !poDS )
        return false;
    const int  nWidth = poDS->GetRasterXSize();
    const int  nHeight = poDS->GetRasterYSize();
    const int  nBlockXSize = std::max(16,
        atoi(CPLGetXMLValue(psNode, "tileSize", "256")));
    const int  nBlockYSize = nBlockXSize;
    const char* pszCompressMethod = CPLGetXMLValue(
        psNode, "Compression.method", "DEFLATE");
    PDFCompressMethod eCompressMethod = COMPRESS_DEFLATE;
    if( EQUAL(pszCompressMethod, "JPEG") )
        eCompressMethod = COMPRESS_JPEG;
    else if( EQUAL(pszCompressMethod, "JPEG2000") )
        eCompressMethod = COMPRESS_JPEG2000;
    const int nPredictor = CPLTestBool(CPLGetXMLValue(
        psNode, "Compression.predictor", "false")) ? 2 : 0;
    const int nJPEGQuality = atoi(
        CPLGetXMLValue(psNode, "Compression.quality", "-1"));
    const char* pszJPEG2000_DRIVER = m_osJPEG2000Driver.empty() ?
        nullptr : m_osJPEG2000Driver.c_str();;

    const char* pszGeoreferencingId =
        CPLGetXMLValue(psNode, "georeferencingId", nullptr);
    double dfClippingMinX = 0;
    double dfClippingMinY = 0;
    double dfClippingMaxX = 0;
    double dfClippingMaxY = 0;
    bool bClip = false;
    double adfRasterGT[6] = {0,1,0,0,0,1};
    double adfInvGeoreferencingGT[6]; // from georeferenced to PDF coordinates
    if( pszGeoreferencingId )
    {
        auto iter = oPageContext.m_oMapGeoreferencedId.find(pszGeoreferencingId);
        if( iter == oPageContext.m_oMapGeoreferencedId.end() )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Cannot find georeferencing of id %s",
                     pszGeoreferencingId);
            return false;
        }
        auto& georeferencing = iter->second;
        dfX1 = georeferencing.m_bboxX1;
        dfY1 = georeferencing.m_bboxY1;
        dfX2 = georeferencing.m_bboxX2;
        dfY2 = georeferencing.m_bboxY2;

        bClip = true;
        dfClippingMinX = APPLY_GT_X(georeferencing.m_adfGT, dfX1, dfY1);
        dfClippingMinY = APPLY_GT_Y(georeferencing.m_adfGT, dfX1, dfY1);
        dfClippingMaxX = APPLY_GT_X(georeferencing.m_adfGT, dfX2, dfY2);
        dfClippingMaxY = APPLY_GT_Y(georeferencing.m_adfGT, dfX2, dfY2);

        if( poDS->GetGeoTransform(adfRasterGT) != CE_None ||
            adfRasterGT[2] != 0 || adfRasterGT[4] != 0 ||
            adfRasterGT[5] > 0 )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Raster has no geotransform or a rotated geotransform");
            return false;
        }

        auto poSRS = poDS->GetSpatialRef();
        if( !poSRS || !poSRS->IsSame(&georeferencing.m_oSRS) )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Raster has no projection, or different from the one "
                     "of the georeferencing area");
            return false;
        }

        CPL_IGNORE_RET_VAL(
            GDALInvGeoTransform(georeferencing.m_adfGT,
                                adfInvGeoreferencingGT));
    }
    const double dfRasterMinX = adfRasterGT[0];
    const double dfRasterMaxY = adfRasterGT[3];

    /* Does the source image has a color table ? */
    const auto nColorTableId = WriteColorTable(poDS.get());

    double dfIgnoredOpacity;
    StartBlending(psNode, oPageContext, dfIgnoredOpacity);

    CPLString osGroupStream;
    std::vector<GDALPDFObjectNum> anImageIds;

    const int nXBlocks = (nWidth + nBlockXSize - 1) / nBlockXSize;
    const int nYBlocks = (nHeight + nBlockYSize - 1) / nBlockYSize;
    int nBlockXOff, nBlockYOff;
    for(nBlockYOff = 0; nBlockYOff < nYBlocks; nBlockYOff ++)
    {
        for(nBlockXOff = 0; nBlockXOff < nXBlocks; nBlockXOff ++)
        {
            int nReqWidth =
                std::min(nBlockXSize, nWidth - nBlockXOff * nBlockXSize);
            int nReqHeight =
                std::min(nBlockYSize, nHeight - nBlockYOff * nBlockYSize);

            int nX = nBlockXOff * nBlockXSize;
            int nY = nBlockYOff * nBlockYSize;

            double dfXPDFOff = nX * (dfX2 - dfX1) / nWidth + dfX1;
            double dfYPDFOff = (nHeight - nY - nReqHeight) * (dfY2 - dfY1) / nHeight + dfY1;
            double dfXPDFSize = nReqWidth * (dfX2 - dfX1) / nWidth;
            double dfYPDFSize = nReqHeight * (dfY2 - dfY1) / nHeight;

            if( bClip )
            {
                /* Compute extent of block to write */
                double dfBlockMinX = adfRasterGT[0] + nX * adfRasterGT[1];
                double dfBlockMaxX = adfRasterGT[0] + (nX + nReqWidth) * adfRasterGT[1];
                double dfBlockMinY = adfRasterGT[3] + (nY + nReqHeight) * adfRasterGT[5];
                double dfBlockMaxY = adfRasterGT[3] + nY * adfRasterGT[5];

                // Clip the extent of the block with the extent of the main raster.
                const double dfIntersectMinX =
                    std::max(dfBlockMinX, dfClippingMinX);
                const double dfIntersectMinY =
                    std::max(dfBlockMinY, dfClippingMinY);
                const double dfIntersectMaxX =
                    std::min(dfBlockMaxX, dfClippingMaxX);
                const double dfIntersectMaxY =
                    std::min(dfBlockMaxY, dfClippingMaxY);

                bool bOK = false;
                if( dfIntersectMinX < dfIntersectMaxX &&
                    dfIntersectMinY < dfIntersectMaxY )
                {
                    /* Re-compute (x,y,width,height) subwindow of current raster from */
                    /* the extent of the clipped block */
                    nX = (int)((dfIntersectMinX - dfRasterMinX) / adfRasterGT[1] + 0.5);
                    nY = (int)((dfRasterMaxY - dfIntersectMaxY) / (-adfRasterGT[5]) + 0.5);
                    nReqWidth = (int)((dfIntersectMaxX - dfRasterMinX) / adfRasterGT[1] + 0.5) - nX;
                    nReqHeight = (int)((dfRasterMaxY - dfIntersectMinY) / (-adfRasterGT[5]) + 0.5) - nY;

                    if( nReqWidth > 0 && nReqHeight > 0)
                    {
                        dfBlockMinX = adfRasterGT[0] + nX * adfRasterGT[1];
                        dfBlockMaxX = adfRasterGT[0] + (nX + nReqWidth) * adfRasterGT[1];
                        dfBlockMinY = adfRasterGT[3] + (nY + nReqHeight) * adfRasterGT[5];
                        dfBlockMaxY = adfRasterGT[3] + nY * adfRasterGT[5];

                        double dfPDFX1 = APPLY_GT_X(adfInvGeoreferencingGT, dfBlockMinX, dfBlockMinY);
                        double dfPDFY1 = APPLY_GT_Y(adfInvGeoreferencingGT, dfBlockMinX, dfBlockMinY);
                        double dfPDFX2 = APPLY_GT_X(adfInvGeoreferencingGT, dfBlockMaxX, dfBlockMaxY);
                        double dfPDFY2 = APPLY_GT_Y(adfInvGeoreferencingGT, dfBlockMaxX, dfBlockMaxY);

                        dfXPDFOff = dfPDFX1;
                        dfYPDFOff = dfPDFY1;
                        dfXPDFSize = dfPDFX2 - dfPDFX1;
                        dfYPDFSize = dfPDFY2 - dfPDFY1;
                        bOK = true;
                    }
                }
                if( !bOK )
                {
                    continue;
                }
            }

            const auto nImageId = WriteBlock(poDS.get(),
                                    nX,
                                    nY,
                                    nReqWidth, nReqHeight,
                                    nColorTableId,
                                    eCompressMethod,
                                    nPredictor,
                                    nJPEGQuality,
                                    pszJPEG2000_DRIVER,
                                    nullptr,
                                    nullptr);

            if (!nImageId.toBool())
                return false;

            anImageIds.push_back(nImageId);
            osGroupStream += "q\n";
            GDALPDFObjectRW* poXSize = GDALPDFObjectRW::CreateReal(dfXPDFSize);
            GDALPDFObjectRW* poYSize = GDALPDFObjectRW::CreateReal(dfYPDFSize);
            GDALPDFObjectRW* poXOff = GDALPDFObjectRW::CreateReal(dfXPDFOff);
            GDALPDFObjectRW* poYOff = GDALPDFObjectRW::CreateReal(dfYPDFOff);
            osGroupStream += CPLOPrintf("%s 0 0 %s %s %s cm\n",
                    poXSize->Serialize().c_str(),
                    poYSize->Serialize().c_str(),
                    poXOff->Serialize().c_str(),
                    poYOff->Serialize().c_str());
            delete poXSize;
            delete poYSize;
            delete poXOff;
            delete poYOff;
            osGroupStream += CPLOPrintf("/Image%d Do\n", nImageId.toInt());
            osGroupStream += "Q\n";
        }
    }

    if( anImageIds.size() <= 1 ||
        CPLGetXMLNode(psNode, "Blending") == nullptr )
    {
        for( const auto& nImageId: anImageIds )
        {
            oPageContext.m_oXObjects[
                    CPLOPrintf("Image%d", nImageId.toInt())] = nImageId;
        }
        oPageContext.m_osDrawingStream += osGroupStream;
    }
    else
    {
        // In case several tiles are drawn with blending, use a transparency
        // group to avoid edge effects.

        auto nGroupId = AllocNewObject();
        GDALPDFDictionaryRW oDictGroup;
        GDALPDFDictionaryRW* poGroup = new GDALPDFDictionaryRW();
        poGroup->Add("Type", GDALPDFObjectRW::CreateName("Group"))
                .Add("S",GDALPDFObjectRW::CreateName("Transparency"));

        GDALPDFDictionaryRW* poXObjects = new GDALPDFDictionaryRW();
        for( const auto& nImageId: anImageIds )
        {
            poXObjects->Add(CPLOPrintf("Image%d", nImageId.toInt()), nImageId, 0);
        }
        GDALPDFDictionaryRW* poResources = new GDALPDFDictionaryRW();
        poResources->Add("XObject", poXObjects);

        oDictGroup.Add("Type", GDALPDFObjectRW::CreateName("XObject"))
            .Add("BBox", &((new GDALPDFArrayRW())
                            ->Add(0).Add(0)).
                              Add(oPageContext.m_dfWidthInUserUnit).
                              Add(oPageContext.m_dfHeightInUserUnit))
            .Add("Subtype", GDALPDFObjectRW::CreateName("Form"))
            .Add("Group", poGroup)
            .Add("Resources", poResources);


        StartObjWithStream(nGroupId, oDictGroup,
                            oPageContext.m_eStreamCompressMethod != COMPRESS_NONE);
        VSIFPrintfL(m_fp, "%s", osGroupStream.c_str());
        EndObjWithStream();

        oPageContext.m_oXObjects[
                    CPLOPrintf("Group%d", nGroupId.toInt())] = nGroupId;
        oPageContext.m_osDrawingStream +=
            CPLOPrintf("/Group%d Do\n", nGroupId.toInt());
    }

    EndBlending(psNode, oPageContext);

    return true;
}

/************************************************************************/
/*                     SetupVectorGeoreferencing()                      */
/************************************************************************/

bool GDALPDFComposerWriter::SetupVectorGeoreferencing(
        const char* pszGeoreferencingId,
        OGRLayer* poLayer,
        const PageContext& oPageContext,
        double& dfClippingMinX,
        double& dfClippingMinY,
        double& dfClippingMaxX,
        double& dfClippingMaxY,
        double adfMatrix[4],
        std::unique_ptr<OGRCoordinateTransformation>& poCT)
{
    CPLAssert( pszGeoreferencingId );

    auto iter = oPageContext.m_oMapGeoreferencedId.find(pszGeoreferencingId);
    if( iter == oPageContext.m_oMapGeoreferencedId.end() )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                    "Cannot find georeferencing of id %s",
                    pszGeoreferencingId);
        return false;
    }
    auto& georeferencing = iter->second;
    const double dfX1 = georeferencing.m_bboxX1;
    const double dfY1 = georeferencing.m_bboxY1;
    const double dfX2 = georeferencing.m_bboxX2;
    const double dfY2 = georeferencing.m_bboxY2;

    dfClippingMinX = APPLY_GT_X(georeferencing.m_adfGT, dfX1, dfY1);
    dfClippingMinY = APPLY_GT_Y(georeferencing.m_adfGT, dfX1, dfY1);
    dfClippingMaxX = APPLY_GT_X(georeferencing.m_adfGT, dfX2, dfY2);
    dfClippingMaxY = APPLY_GT_Y(georeferencing.m_adfGT, dfX2, dfY2);

    auto poSRS = poLayer->GetSpatialRef();
    if( !poSRS )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Layer has no SRS");
        return false;
    }
    if( !poSRS->IsSame(&georeferencing.m_oSRS) )
    {
        poCT.reset(
            OGRCreateCoordinateTransformation(poSRS, &georeferencing.m_oSRS));
    }

    if( !poCT )
    {
        poLayer->SetSpatialFilterRect(dfClippingMinX, dfClippingMinY,
                                        dfClippingMaxX, dfClippingMaxY);
    }

    double adfInvGeoreferencingGT[6]; // from georeferenced to PDF coordinates
    CPL_IGNORE_RET_VAL(
        GDALInvGeoTransform(const_cast<double*>(georeferencing.m_adfGT),
                            adfInvGeoreferencingGT));
    adfMatrix[0] = adfInvGeoreferencingGT[0];
    adfMatrix[1] = adfInvGeoreferencingGT[1];
    adfMatrix[2] = adfInvGeoreferencingGT[3];
    adfMatrix[3] = adfInvGeoreferencingGT[5];

    return true;
}

/************************************************************************/
/*                           WriteVector()                              */
/************************************************************************/

bool GDALPDFComposerWriter::WriteVector(const CPLXMLNode* psNode,
                                        PageContext& oPageContext)
{
    const char* pszDataset = CPLGetXMLValue(psNode, "dataset", nullptr);
    if( !pszDataset )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                "Missing dataset");
        return false;
    }
    const char* pszLayer = CPLGetXMLValue(psNode, "layer", nullptr);
    if( !pszLayer )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                "Missing layer");
        return false;
    }

    GDALDatasetUniquePtr poDS(GDALDataset::Open(
        pszDataset, GDAL_OF_VECTOR | GDAL_OF_VERBOSE_ERROR,
        nullptr, nullptr, nullptr));
    if( !poDS )
        return false;
    OGRLayer* poLayer = poDS->GetLayerByName(pszLayer);
    if( !poLayer )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannt find layer %s", pszLayer);
        return false;
    }
    const bool bVisible =
        CPLTestBool(CPLGetXMLValue(psNode, "visible", "true"));

    const auto psLogicalStructure =
        CPLGetXMLNode(psNode, "LogicalStructure");
    const char* pszOGRDisplayField = nullptr;
    std::vector<CPLString> aosIncludedFields;
    const bool bLogicalStructure = psLogicalStructure != nullptr;
    if( psLogicalStructure )
    {
        pszOGRDisplayField = CPLGetXMLValue(psLogicalStructure,
                                            "fieldToDisplay", nullptr);
        if( CPLGetXMLNode(psLogicalStructure, "ExcludeAllFields") != nullptr ||
            CPLGetXMLNode(psLogicalStructure, "IncludeField") != nullptr )
        {
            for(const auto* psIter = psLogicalStructure->psChild;
                psIter; psIter = psIter->psNext)
            {
                if( psIter->eType == CXT_Element &&
                    strcmp(psIter->pszValue, "IncludeField") == 0 )
                {
                    aosIncludedFields.push_back(CPLGetXMLValue(psIter, nullptr, ""));
                }
            }
        }
        else
        {
            std::set<CPLString> oSetExcludedFields;
            for(const auto* psIter = psLogicalStructure->psChild;
                psIter; psIter = psIter->psNext)
            {
                if( psIter->eType == CXT_Element &&
                    strcmp(psIter->pszValue, "ExcludeField") == 0 )
                {
                    oSetExcludedFields.insert(CPLGetXMLValue(psIter, nullptr, ""));
                }
            }
            const auto poLayerDefn = poLayer->GetLayerDefn();
            for( int i = 0; i < poLayerDefn->GetFieldCount(); i++ )
            {
                const auto poFieldDefn = poLayerDefn->GetFieldDefn(i);
                const char* pszName = poFieldDefn->GetNameRef();
                if( oSetExcludedFields.find(pszName) == oSetExcludedFields.end() )
                {
                    aosIncludedFields.push_back(pszName);
                }
            }
        }
    }
    const char* pszStyleString = CPLGetXMLValue(psNode,
                                                    "ogrStyleString", nullptr);
    const char* pszOGRLinkField = CPLGetXMLValue(psNode,
                                                    "linkAttribute", nullptr);

    const char* pszGeoreferencingId =
        CPLGetXMLValue(psNode, "georeferencingId", nullptr);
    std::unique_ptr<OGRCoordinateTransformation> poCT;
    double dfClippingMinX = 0;
    double dfClippingMinY = 0;
    double dfClippingMaxX = 0;
    double dfClippingMaxY = 0;
    double adfMatrix[4] = { 0, 1, 0, 1 };
    if( pszGeoreferencingId &&
        !SetupVectorGeoreferencing(pszGeoreferencingId,
                                   poLayer, oPageContext,
                                   dfClippingMinX, dfClippingMinY,
                                   dfClippingMaxX, dfClippingMaxY,
                                   adfMatrix, poCT) )
    {
        return false;
    }

    double dfOpacityFactor = 1.0;
    if( !bVisible )
    {
        if( oPageContext.m_oExtGState.find("GSinvisible") ==
                oPageContext.m_oExtGState.end() )
        {
            auto nExtGState = AllocNewObject();
            StartObj(nExtGState);
            {
                GDALPDFDictionaryRW gs;
                gs.Add("Type", GDALPDFObjectRW::CreateName("ExtGState"));
                gs.Add("ca", 0);
                gs.Add("CA", 0);
                VSIFPrintfL(m_fp, "%s\n", gs.Serialize().c_str());
            }
            EndObj();
            oPageContext.m_oExtGState["GSinvisible"] = nExtGState;
        }
        oPageContext.m_osDrawingStream += "q\n";
        oPageContext.m_osDrawingStream += "/GSinvisible gs\n";
        oPageContext.m_osDrawingStream += "0 w\n";
        dfOpacityFactor = 0;
    }
    else
    {
        StartBlending(psNode, oPageContext, dfOpacityFactor);
    }

    if (!m_nStructTreeRootId.toBool())
        m_nStructTreeRootId = AllocNewObject();

    GDALPDFObjectNum nFeatureLayerId;
    if( bLogicalStructure )
    {
        nFeatureLayerId = AllocNewObject();
        m_anFeatureLayerId.push_back(nFeatureLayerId);
    }

    std::vector<GDALPDFObjectNum> anFeatureUserProperties;
    for( auto&& poFeature: poLayer )
    {
        auto hFeat = OGRFeature::ToHandle(poFeature.get());
        auto hGeom = OGR_F_GetGeometryRef(hFeat);
        if( !hGeom || OGR_G_IsEmpty(hGeom) )
            continue;
        if( poCT )
        {
            if( OGRGeometry::FromHandle(hGeom)->transform(poCT.get()) != OGRERR_NONE )
                continue;

            OGREnvelope sEnvelope;
            OGR_G_GetEnvelope(hGeom, &sEnvelope);
            if( sEnvelope.MinX > dfClippingMaxX ||
                sEnvelope.MaxX < dfClippingMinX ||
                sEnvelope.MinY > dfClippingMaxY ||
                sEnvelope.MaxY < dfClippingMinY )
            {
                continue;
            }
        }

        if( bLogicalStructure )
        {
            CPLString osOutFeatureName;
            anFeatureUserProperties.push_back(
                WriteAttributes(
                            hFeat,
                            aosIncludedFields,
                            pszOGRDisplayField,
                            oPageContext.m_nMCID,
                            nFeatureLayerId,
                            m_asPageId.back(),
                            osOutFeatureName));
        }

        ObjectStyle os;
        GetObjectStyle(pszStyleString, hFeat, adfMatrix,
                       m_oMapSymbolFilenameToDesc, os);
        os.nPenA = static_cast<int>(std::round(os.nPenA * dfOpacityFactor));
        os.nBrushA = static_cast<int>(std::round(os.nBrushA * dfOpacityFactor));

        const double dfRadius = os.dfSymbolSize;

        if( os.nImageSymbolId.toBool() )
        {
            oPageContext.m_oXObjects[
                CPLOPrintf("SymImage%d", os.nImageSymbolId.toInt())] = os.nImageSymbolId;
        }

        if( pszOGRLinkField )
        {
            OGREnvelope sEnvelope;
            OGR_G_GetEnvelope(hGeom, &sEnvelope);
            int bboxXMin, bboxYMin, bboxXMax, bboxYMax;
            ComputeIntBBox(hGeom, sEnvelope, adfMatrix, os, dfRadius,
                        bboxXMin, bboxYMin, bboxXMax, bboxYMax);

            auto nLinkId = WriteLink(hFeat, pszOGRLinkField, adfMatrix,
                    bboxXMin, bboxYMin, bboxXMax, bboxYMax);
            if( nLinkId.toBool() )
                oPageContext.m_anAnnotationsId.push_back(nLinkId);
        }

        if( bLogicalStructure )
        {
            oPageContext.m_osDrawingStream +=
                CPLOPrintf("/feature <</MCID %d>> BDC\n", oPageContext.m_nMCID);
        }

        if( bVisible || bLogicalStructure )
        {
            oPageContext.m_osDrawingStream += "q\n";
            if (bVisible && (os.nPenA != 255 || os.nBrushA != 255))
            {
                CPLString osGSName;
                osGSName.Printf("GS_CA_%d_ca_%d", os.nPenA, os.nBrushA);
                if( oPageContext.m_oExtGState.find(osGSName) ==
                    oPageContext.m_oExtGState.end() )
                {
                    auto nExtGState = AllocNewObject();
                    StartObj(nExtGState);
                    {
                        GDALPDFDictionaryRW gs;
                        gs.Add("Type", GDALPDFObjectRW::CreateName("ExtGState"));
                        if (os.nPenA != 255)
                            gs.Add("CA", (os.nPenA == 127 || os.nPenA == 128) ? 0.5 : os.nPenA / 255.0);
                        if (os.nBrushA != 255)
                            gs.Add("ca", (os.nBrushA == 127 || os.nBrushA == 128) ? 0.5 : os.nBrushA / 255.0 );
                        VSIFPrintfL(m_fp, "%s\n", gs.Serialize().c_str());
                    }
                    EndObj();
                    oPageContext.m_oExtGState[osGSName] = nExtGState;
                }
                oPageContext.m_osDrawingStream += "/" + osGSName +" gs\n";
            }

            oPageContext.m_osDrawingStream +=
                GenerateDrawingStream(hGeom, adfMatrix, os, dfRadius);

            oPageContext.m_osDrawingStream += "Q\n";
        }

        if( bLogicalStructure )
        {
            oPageContext.m_osDrawingStream += "EMC\n";
            oPageContext.m_nMCID ++;
        }
    }

    if( bLogicalStructure )
    {
        for( const auto& num: anFeatureUserProperties )
        {
            oPageContext.m_anFeatureUserProperties.push_back(num);
        }

        {
            StartObj(nFeatureLayerId);

            GDALPDFDictionaryRW oDict;
            GDALPDFDictionaryRW* poDictA = new GDALPDFDictionaryRW();
            oDict.Add("A", poDictA);
            poDictA->Add("O", GDALPDFObjectRW::CreateName("UserProperties"));
            GDALPDFArrayRW* poArrayK = new GDALPDFArrayRW();
            for( const auto& num: anFeatureUserProperties )
                poArrayK->Add(num, 0);
            oDict.Add("K", poArrayK);
            oDict.Add("P", m_nStructTreeRootId, 0);
            oDict.Add("S", GDALPDFObjectRW::CreateName("Layer"));

            const char* pszOGRDisplayName =
                CPLGetXMLValue(psLogicalStructure, "displayLayerName", poLayer->GetName());
            oDict.Add("T", pszOGRDisplayName);

            VSIFPrintfL(m_fp, "%s\n", oDict.Serialize().c_str());

            EndObj();
        }
    }

    if( !bVisible )
    {
        oPageContext.m_osDrawingStream += "Q\n";
    }
    else
    {
        EndBlending(psNode, oPageContext);
    }

    return true;
}

/************************************************************************/
/*                         WriteVectorLabel()                           */
/************************************************************************/

bool GDALPDFComposerWriter::WriteVectorLabel(const CPLXMLNode* psNode,
                                        PageContext& oPageContext)
{
    const char* pszDataset = CPLGetXMLValue(psNode, "dataset", nullptr);
    if( !pszDataset )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                "Missing dataset");
        return false;
    }
    const char* pszLayer = CPLGetXMLValue(psNode, "layer", nullptr);
    if( !pszLayer )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                "Missing layer");
        return false;
    }

    GDALDatasetUniquePtr poDS(GDALDataset::Open(
        pszDataset, GDAL_OF_VECTOR | GDAL_OF_VERBOSE_ERROR,
        nullptr, nullptr, nullptr));
    if( !poDS )
        return false;
    OGRLayer* poLayer = poDS->GetLayerByName(pszLayer);
    if( !poLayer )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannt find layer %s", pszLayer);
        return false;
    }

    const char* pszStyleString = CPLGetXMLValue(psNode,
                                                "ogrStyleString", nullptr);

    double dfOpacityFactor = 1.0;
    StartBlending(psNode, oPageContext, dfOpacityFactor);

    const char* pszGeoreferencingId =
        CPLGetXMLValue(psNode, "georeferencingId", nullptr);
    std::unique_ptr<OGRCoordinateTransformation> poCT;
    double dfClippingMinX = 0;
    double dfClippingMinY = 0;
    double dfClippingMaxX = 0;
    double dfClippingMaxY = 0;
    double adfMatrix[4] = { 0, 1, 0, 1 };
    if( pszGeoreferencingId &&
        !SetupVectorGeoreferencing(pszGeoreferencingId,
                                   poLayer, oPageContext,
                                   dfClippingMinX, dfClippingMinY,
                                   dfClippingMaxX, dfClippingMaxY,
                                   adfMatrix, poCT) )
    {
        return false;
    }

    for( auto&& poFeature: poLayer )
    {
        auto hFeat = OGRFeature::ToHandle(poFeature.get());
        auto hGeom = OGR_F_GetGeometryRef(hFeat);
        if( !hGeom || OGR_G_IsEmpty(hGeom) )
            continue;
        if( poCT )
        {
            if( OGRGeometry::FromHandle(hGeom)->transform(poCT.get()) != OGRERR_NONE )
                continue;

            OGREnvelope sEnvelope;
            OGR_G_GetEnvelope(hGeom, &sEnvelope);
            if( sEnvelope.MinX > dfClippingMaxX ||
                sEnvelope.MaxX < dfClippingMinX ||
                sEnvelope.MinY > dfClippingMaxY ||
                sEnvelope.MaxY < dfClippingMinY )
            {
                continue;
            }
        }

        ObjectStyle os;
        GetObjectStyle(pszStyleString, hFeat, adfMatrix,
                       m_oMapSymbolFilenameToDesc, os);
        os.nPenA = static_cast<int>(std::round(os.nPenA * dfOpacityFactor));
        os.nBrushA = static_cast<int>(std::round(os.nBrushA * dfOpacityFactor));

        if (!os.osLabelText.empty() &&
            wkbFlatten(OGR_G_GetGeometryType(hGeom)) == wkbPoint)
        {
            auto nObjectId = WriteLabel(hGeom, adfMatrix, os,
                                        oPageContext.m_eStreamCompressMethod,
                                        0,0,
                                        oPageContext.m_dfWidthInUserUnit,
                                        oPageContext.m_dfHeightInUserUnit);
            oPageContext.m_osDrawingStream +=
                CPLOPrintf("/Label%d Do\n", nObjectId.toInt());
            oPageContext.m_oXObjects[
                    CPLOPrintf("Label%d", nObjectId.toInt())] = nObjectId;
        }
    }

    EndBlending(psNode, oPageContext);

    return true;
}

#ifdef HAVE_PDF_READ_SUPPORT

/************************************************************************/
/*                            EmitNewObject()                           */
/************************************************************************/

GDALPDFObjectNum GDALPDFComposerWriter::EmitNewObject(GDALPDFObject* poObj,
                                                      RemapType& oRemapObjectRefs)
{
    auto nId = AllocNewObject();
    const auto nRefNum = poObj->GetRefNum();
    if( nRefNum.toBool() )
    {
        int nRefGen = poObj->GetRefGen();
        std::pair<int, int> oKey(nRefNum.toInt(), nRefGen);
        oRemapObjectRefs[oKey] = nId;
    }
    CPLString osStr;
    if( !SerializeAndRenumberIgnoreRef(osStr, poObj, oRemapObjectRefs) )
        return GDALPDFObjectNum();
    StartObj(nId);
    VSIFWriteL(osStr.data(), 1, osStr.size(), m_fp);
    VSIFPrintfL(m_fp, "\n");
    EndObj();
    return nId;
}

/************************************************************************/
/*                         SerializeAndRenumber()                       */
/************************************************************************/

bool GDALPDFComposerWriter::SerializeAndRenumber(CPLString& osStr,
                                                 GDALPDFObject* poObj,
                                                 RemapType& oRemapObjectRefs)
{
    auto nRefNum = poObj->GetRefNum();
    if( nRefNum.toBool() )
    {
        int nRefGen = poObj->GetRefGen();

        std::pair<int, int> oKey(nRefNum.toInt(), nRefGen);
        auto oIter = oRemapObjectRefs.find(oKey);
        if( oIter != oRemapObjectRefs.end() )
        {
            osStr.append(CPLSPrintf("%d 0 R", oIter->second.toInt()));
            return true;
        }
        else
        {
            auto nId = EmitNewObject(poObj, oRemapObjectRefs);
            osStr.append(CPLSPrintf("%d 0 R", nId.toInt()));
            return nId.toBool();
        }
    }
    else
    {
        return SerializeAndRenumberIgnoreRef(osStr, poObj, oRemapObjectRefs);
    }
}

/************************************************************************/
/*                    SerializeAndRenumberIgnoreRef()                   */
/************************************************************************/

bool GDALPDFComposerWriter::SerializeAndRenumberIgnoreRef(CPLString& osStr,
                                                 GDALPDFObject* poObj,
                                                 RemapType& oRemapObjectRefs)
{
    switch(poObj->GetType())
    {
        case PDFObjectType_Array:
        {
            auto poArray = poObj->GetArray();
            int nLength = poArray->GetLength();
            osStr.append("[ ");
            for(int i=0;i<nLength;i++)
            {
                if( !SerializeAndRenumber(osStr, poArray->Get(i), oRemapObjectRefs) )
                    return false;
                osStr.append(" ");
            }
            osStr.append("]");
            break;
        }
        case PDFObjectType_Dictionary:
        {
            osStr.append("<< ");
            auto poDict = poObj->GetDictionary();
            auto& oMap = poDict->GetValues();
            for( const auto& oIter: oMap )
            {
                const char* pszKey = oIter.first.c_str();
                GDALPDFObject* poSubObj = oIter.second;
                osStr.append("/");
                osStr.append(pszKey);
                osStr.append(" ");
                if( !SerializeAndRenumber(osStr, poSubObj, oRemapObjectRefs) )
                    return false;
                osStr.append(" ");
            }
            osStr.append(">>");
            auto poStream = poObj->GetStream();
            if( poStream )
            {
                // CPLAssert( poObj->GetRefNum().toBool() ); // should be a top level object
                osStr.append("\nstream\n");
                auto pRawBytes = poStream->GetRawBytes();
                if( !pRawBytes )
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Cannot get stream content");
                    return false;
                }
                osStr.append(pRawBytes, poStream->GetRawLength());
                VSIFree(pRawBytes);
                osStr.append("\nendstream\n");
            }
            break;
        }
        case PDFObjectType_Unknown:
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Corrupted PDF");
            return false;
        }
        default:
        {
            poObj->Serialize(osStr, false);
            break;
        }
    }
    return true;
}

/************************************************************************/
/*                         SerializeAndRenumber()                       */
/************************************************************************/

GDALPDFObjectNum GDALPDFComposerWriter::SerializeAndRenumber(GDALPDFObject* poObj)
{
    RemapType oRemapObjectRefs;
    return EmitNewObject(poObj, oRemapObjectRefs);
}

/************************************************************************/
/*                             WritePDF()                               */
/************************************************************************/

bool GDALPDFComposerWriter::WritePDF(const CPLXMLNode* psNode,
                                        PageContext& oPageContext)
{
    const char* pszDataset = CPLGetXMLValue(psNode, "dataset", nullptr);
    if( !pszDataset )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                "Missing dataset");
        return false;
    }

    GDALOpenInfo oOpenInfo(pszDataset, GA_ReadOnly);
    std::unique_ptr<PDFDataset> poDS(PDFDataset::Open(&oOpenInfo));
    if( !poDS )
    {
        CPLError(CE_Failure, CPLE_OpenFailed,
                 "%s is not a valid PDF file", pszDataset);
        return false;
    }
    if( poDS->GetPageWidth() != oPageContext.m_dfWidthInUserUnit ||
        poDS->GetPageHeight() != oPageContext.m_dfHeightInUserUnit )
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "Dimensions of the inserted PDF page are %fx%f, which is "
                 "different from the output PDF page %fx%f",
                 poDS->GetPageWidth(),
                 poDS->GetPageHeight(),
                 oPageContext.m_dfWidthInUserUnit,
                 oPageContext.m_dfHeightInUserUnit);
    }
    auto poPageObj = poDS->GetPageObj();
    if( !poPageObj )
        return false;
    auto poPageDict = poPageObj->GetDictionary();
    if( !poPageDict )
        return false;
    auto poContents = poPageDict->Get("Contents");
    if (poContents != nullptr && poContents->GetType() == PDFObjectType_Array)
    {
        GDALPDFArray* poContentsArray = poContents->GetArray();
        if (poContentsArray->GetLength() == 1)
        {
            poContents = poContentsArray->Get(0);
        }
    }
    if (poContents == nullptr ||
            poContents->GetType() != PDFObjectType_Dictionary )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Missing Contents");
        return false;
    }

    auto poResources = poPageDict->Get("Resources");
    if( !poResources )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Missing Resources");
        return false;
    }

    // Serialize and renumber the Page Resources dictionary
    auto nClonedResources = SerializeAndRenumber(poResources);
    if( !nClonedResources.toBool() )
    {
        return false;
    }

    // Create a Transparency group using cloned Page Resources, and
    // the Page Contents stream
    auto nFormId = AllocNewObject();
    GDALPDFDictionaryRW oDictGroup;
    GDALPDFDictionaryRW* poGroup = new GDALPDFDictionaryRW();
    poGroup->Add("Type", GDALPDFObjectRW::CreateName("Group"))
            .Add("S",GDALPDFObjectRW::CreateName("Transparency"));

    oDictGroup.Add("Type", GDALPDFObjectRW::CreateName("XObject"))
        .Add("BBox", &((new GDALPDFArrayRW())
                        ->Add(0).Add(0)).
                            Add(oPageContext.m_dfWidthInUserUnit).
                            Add(oPageContext.m_dfHeightInUserUnit))
        .Add("Subtype", GDALPDFObjectRW::CreateName("Form"))
        .Add("Group", poGroup)
        .Add("Resources", nClonedResources, 0);

    auto poStream = poContents->GetStream();
    if( !poStream )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Missing Contents stream");
        return false;
    }
    auto pabyContents = poStream->GetBytes();
    if( !pabyContents )
    {
        return false;
    }
    const auto nContentsLength = poStream->GetLength();

    StartObjWithStream(nFormId, oDictGroup,
                       oPageContext.m_eStreamCompressMethod != COMPRESS_NONE);
    VSIFWriteL(pabyContents, 1, nContentsLength, m_fp);
    VSIFree(pabyContents);
    EndObjWithStream();

    // Paint the transparency group
    double dfIgnoredOpacity;
    StartBlending(psNode, oPageContext, dfIgnoredOpacity);

    oPageContext.m_osDrawingStream +=
                CPLOPrintf("/Form%d Do\n", nFormId.toInt());
    oPageContext.m_oXObjects[
            CPLOPrintf("Form%d", nFormId.toInt())] = nFormId;

    EndBlending(psNode, oPageContext);

    return true;
}

#endif // HAVE_PDF_READ_SUPPORT

/************************************************************************/
/*                              Generate()                              */
/************************************************************************/

bool GDALPDFComposerWriter::Generate(const CPLXMLNode* psComposition)
{
    m_osJPEG2000Driver = CPLGetXMLValue(psComposition, "JPEG2000Driver", "");

    auto psMetadata = CPLGetXMLNode(psComposition, "Metadata");
    if( psMetadata )
    {
        SetInfo(
            CPLGetXMLValue(psMetadata, "Author", nullptr),
            CPLGetXMLValue(psMetadata, "Producer", nullptr),
            CPLGetXMLValue(psMetadata, "Creator", nullptr),
            CPLGetXMLValue(psMetadata, "CreationDate", nullptr),
            CPLGetXMLValue(psMetadata, "Subject", nullptr),
            CPLGetXMLValue(psMetadata, "Title", nullptr),
            CPLGetXMLValue(psMetadata, "Keywords", nullptr));
        SetXMP(nullptr, CPLGetXMLValue(psMetadata, "XMP", nullptr));
    }

    const char* pszJavascript = CPLGetXMLValue(psComposition, "Javascript", nullptr);
    if( pszJavascript )
        WriteJavascript(pszJavascript, false);

    auto psLayerTree = CPLGetXMLNode(psComposition, "LayerTree");
    if( psLayerTree )
    {
        m_bDisplayLayersOnlyOnVisiblePages = CPLTestBool(
            CPLGetXMLValue(psLayerTree, "displayOnlyOnVisiblePages", "false"));
        if( !CreateLayerTree(psLayerTree, GDALPDFObjectNum(), &m_oTreeOfOGC) )
            return false;
    }

    bool bFoundPage = false;
    for(const auto* psIter = psComposition->psChild; psIter; psIter = psIter->psNext)
    {
        if( psIter->eType == CXT_Element &&
            strcmp(psIter->pszValue, "Page") == 0 )
        {
            if( !GeneratePage(psIter) )
                return false;
            bFoundPage = true;
        }
    }
    if( !bFoundPage )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "At least one page should be defined");
        return false;
    }

    auto psOutline = CPLGetXMLNode(psComposition, "Outline");
    if( psOutline )
    {
        if( !CreateOutline(psOutline) )
            return false;
    }

    return true;
}


/************************************************************************/
/*                          GDALPDFErrorHandler()                       */
/************************************************************************/

static void CPL_STDCALL GDALPDFErrorHandler(CPL_UNUSED CPLErr eErr,
                                           CPL_UNUSED CPLErrorNum nType,
                                           const char *pszMsg)
{
    std::vector<CPLString> *paosErrors =
        static_cast<std::vector<CPLString> *>(CPLGetErrorHandlerUserData());
    paosErrors->push_back(pszMsg);
}

/************************************************************************/
/*                      GDALPDFCreateFromCompositionFile()              */
/************************************************************************/

GDALDataset* GDALPDFCreateFromCompositionFile(const char* pszPDFFilename,
                                              const char *pszXMLFilename)
{
    CPLXMLTreeCloser oXML(
        (pszXMLFilename[0] == '<' &&
            strstr(pszXMLFilename, "<PDFComposition") != nullptr) ?
            CPLParseXMLString(pszXMLFilename) : CPLParseXMLFile(pszXMLFilename));
    if( !oXML.get() )
        return nullptr;
    auto psComposition = CPLGetXMLNode(oXML.get(), "=PDFComposition");
    if( !psComposition )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot find PDFComposition");
        return nullptr;
    }

    // XML Validation.
    if( CPLTestBool(CPLGetConfigOption("GDAL_XML_VALIDATION", "YES")) )
    {
        const char *pszXSD = CPLFindFile("gdal", "pdfcomposition.xsd");
        if( pszXSD != nullptr )
        {
            std::vector<CPLString> aosErrors;
            CPLPushErrorHandlerEx(GDALPDFErrorHandler, &aosErrors);
            const int bRet = CPLValidateXML(pszXMLFilename, pszXSD, nullptr);
            CPLPopErrorHandler();
            if( !bRet )
            {
                if( !aosErrors.empty() &&
                    strstr(aosErrors[0].c_str(), "missing libxml2 support") ==
                        nullptr )
                {
                    for( size_t i = 0; i < aosErrors.size(); i++ )
                    {
                        CPLError(CE_Warning, CPLE_AppDefined, "%s",
                                 aosErrors[i].c_str());
                    }
                }
            }
            CPLErrorReset();
        }
    }

/* -------------------------------------------------------------------- */
/*      Create file.                                                    */
/* -------------------------------------------------------------------- */
    VSILFILE* fp = VSIFOpenL(pszPDFFilename, "wb");
    if( fp == nullptr )
    {
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "Unable to create PDF file %s.\n",
                  pszPDFFilename );
        return nullptr;
    }

    GDALPDFComposerWriter oWriter(fp);
    if( !oWriter.Generate(psComposition) )
        return nullptr;

    return new GDALFakePDFDataset();
}
