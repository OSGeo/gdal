/******************************************************************************
 *
 * Project:  Interlis 2 Reader
 * Purpose:  Implementation of ILI2Reader class.
 * Author:   Markus Schnider, Sourcepole AG
 *
 ******************************************************************************
 * Copyright (c) 2004, Pirmin Kalberer, Sourcepole AG
 * Copyright (c) 2008-2012, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "ili2readerp.h"
#include "ogr_ili2.h"
#include "cpl_conv.h"
#include "cpl_string.h"

#include "ili2reader.h"

using namespace std;

//
// constants
//
static const char *const ILI2_TID = "TID";
static const XMLCh xmlch_ILI2_TID[] = {'T', 'I', 'D', '\0'};
static const XMLCh ILI2_REF[] = {'R', 'E', 'F', '\0'};

constexpr int ILI2_STRING_TYPE = 0;
constexpr int ILI2_COORD_TYPE = 1;
constexpr int ILI2_ARC_TYPE = 2;
constexpr int ILI2_POLYLINE_TYPE = 4;
constexpr int ILI2_BOUNDARY_TYPE = 8;
constexpr int ILI2_AREA_TYPE = 16;  // also SURFACE
constexpr int ILI2_GEOMCOLL_TYPE = 32;

static const char *const ILI2_COORD = "COORD";
static const char *const ILI2_ARC = "ARC";
static const char *const ILI2_POLYLINE = "POLYLINE";
static const char *const ILI2_BOUNDARY = "BOUNDARY";
static const char *const ILI2_AREA = "AREA";
static const char *const ILI2_SURFACE = "SURFACE";

namespace gdal
{
namespace ili2
{
//
// helper functions
//
int cmpStr(const string &s1, const string &s2)
{
    string::const_iterator p1 = s1.begin();
    string::const_iterator p2 = s2.begin();

    while (p1 != s1.end() && p2 != s2.end())
    {
        if (CPLToupper(static_cast<unsigned char>(*p1)) !=
            CPLToupper(static_cast<unsigned char>(*p2)))
            return (CPLToupper(static_cast<unsigned char>(*p1)) <
                    CPLToupper(static_cast<unsigned char>(*p2)))
                       ? -1
                       : 1;
        ++p1;
        ++p2;
    }

    return (s2.size() == s1.size()) ? 0 : (s1.size() < s2.size()) ? -1 : 1;
}

string ltrim(const string &tmpstr)
{
    size_t i = 0;
    while (i < tmpstr.length() && (tmpstr[i] == ' ' || tmpstr[i] == '\t' ||
                                   tmpstr[i] == '\r' || tmpstr[i] == '\n'))
        ++i;
    return i > 0 ? tmpstr.substr(i, tmpstr.length() - i) : tmpstr;
}

string rtrim(const string &tmpstr)
{
    if (tmpstr.empty())
        return tmpstr;
    size_t i = tmpstr.length() - 1U;
    while (tmpstr[i] == ' ' || tmpstr[i] == '\t' || tmpstr[i] == '\r' ||
           tmpstr[i] == '\n')
        --i;
    return i < tmpstr.length() - 1 ? tmpstr.substr(0, i + 1) : tmpstr;
}

string trim(const string &tmpstr)
{
    auto ret = ltrim(tmpstr);
    ret = rtrim(ret);
    return ret;
}
}  // namespace ili2
}  // namespace gdal

using namespace gdal::ili2;

static int getGeometryTypeOfElem(DOMElement *elem)
{
    int type = ILI2_STRING_TYPE;
    if (elem == nullptr)
        return type;
    char *pszTagName = XMLString::transcode(elem->getTagName());

    if (elem->getNodeType() == DOMNode::ELEMENT_NODE)
    {
        if (cmpStr(ILI2_COORD, pszTagName) == 0)
        {
            type = ILI2_COORD_TYPE;
        }
        else if (cmpStr(ILI2_ARC, pszTagName) == 0)
        {
            type = ILI2_ARC_TYPE;
        }
        else if (cmpStr(ILI2_POLYLINE, pszTagName) == 0)
        {
            type = ILI2_POLYLINE_TYPE;
        }
        else if (cmpStr(ILI2_BOUNDARY, pszTagName) == 0)
        {
            type = ILI2_BOUNDARY_TYPE;
        }
        else if (cmpStr(ILI2_AREA, pszTagName) == 0)
        {
            type = ILI2_AREA_TYPE;
        }
        else if (cmpStr(ILI2_SURFACE, pszTagName) == 0)
        {
            type = ILI2_AREA_TYPE;
        }
    }
    XMLString::release(&pszTagName);
    return type;
}

static char *getObjValue(DOMElement *elem)
{
    DOMNode *child = elem->getFirstChild();
    if ((child != nullptr) && (child->getNodeType() == DOMNode::TEXT_NODE))
    {
        return CPLStrdup(transcode(child->getNodeValue()));
    }

    return nullptr;
}

static char *getREFValue(DOMElement *elem)
{
    CPLString osREFValue(transcode(elem->getAttribute(ILI2_REF)));
    return CPLStrdup(osREFValue);
}

static OGRPoint *getPoint(DOMElement *elem)
{
    // elem -> COORD (or ARC)
    DOMElement *coordElem = dynamic_cast<DOMElement *>(elem->getFirstChild());
    if (coordElem == nullptr)
        return nullptr;
    OGRPoint *pt = new OGRPoint();

    while (coordElem != nullptr)
    {
        char *pszTagName = XMLString::transcode(coordElem->getTagName());
        char *pszObjValue = getObjValue(coordElem);
        if (pszObjValue)
        {
            if (cmpStr("C1", pszTagName) == 0)
                pt->setX(CPLAtof(pszObjValue));
            else if (cmpStr("C2", pszTagName) == 0)
                pt->setY(CPLAtof(pszObjValue));
            else if (cmpStr("C3", pszTagName) == 0)
                pt->setZ(CPLAtof(pszObjValue));
        }
        CPLFree(pszObjValue);
        XMLString::release(&pszTagName);
        coordElem = dynamic_cast<DOMElement *>(coordElem->getNextSibling());
    }
    pt->flattenTo2D();
    return pt;
}

OGRCircularString *ILI2Reader::getArc(DOMElement *elem)
{
    // previous point -> start point
    auto elemPrev = dynamic_cast<DOMElement *>(elem->getPreviousSibling());
    if (elemPrev == nullptr)
        return nullptr;
    OGRPoint *ptStart = getPoint(elemPrev);  // COORD or ARC
    if (ptStart == nullptr)
        return nullptr;

    // elem -> ARC
    OGRCircularString *arc = new OGRCircularString();
    // end point
    OGRPoint *ptEnd = new OGRPoint();
    // point on the arc
    OGRPoint *ptOnArc = new OGRPoint();
    // double radius = 0; // radius

    DOMElement *arcElem = dynamic_cast<DOMElement *>(elem->getFirstChild());
    while (arcElem != nullptr)
    {
        char *pszTagName = XMLString::transcode(arcElem->getTagName());
        char *pszObjValue = getObjValue(arcElem);
        if (pszObjValue)
        {
            if (cmpStr("C1", pszTagName) == 0)
                ptEnd->setX(CPLAtof(pszObjValue));
            else if (cmpStr("C2", pszTagName) == 0)
                ptEnd->setY(CPLAtof(pszObjValue));
            else if (cmpStr("C3", pszTagName) == 0)
                ptEnd->setZ(CPLAtof(pszObjValue));
            else if (cmpStr("A1", pszTagName) == 0)
                ptOnArc->setX(CPLAtof(pszObjValue));
            else if (cmpStr("A2", pszTagName) == 0)
                ptOnArc->setY(CPLAtof(pszObjValue));
            else if (cmpStr("A3", pszTagName) == 0)
                ptOnArc->setZ(CPLAtof(pszObjValue));
            else if (cmpStr("R", pszTagName) == 0)
            {
                // radius = CPLAtof(pszObjValue);
            }
        }
        CPLFree(pszObjValue);
        XMLString::release(&pszTagName);
        arcElem = dynamic_cast<DOMElement *>(arcElem->getNextSibling());
    }
    arc->addPoint(ptStart);
    arc->addPoint(ptOnArc);
    arc->addPoint(ptEnd);
    delete ptStart;
    delete ptOnArc;
    delete ptEnd;
    return arc;
}

static OGRCompoundCurve *getPolyline(DOMElement *elem)
{
    // elem -> POLYLINE
    OGRCompoundCurve *ogrCurve = new OGRCompoundCurve();
    OGRLineString *ls = new OGRLineString();

    DOMElement *lineElem = dynamic_cast<DOMElement *>(elem->getFirstChild());
    while (lineElem != nullptr)
    {
        char *pszTagName = XMLString::transcode(lineElem->getTagName());
        if (cmpStr(ILI2_COORD, pszTagName) == 0)
        {
            OGRPoint *poPoint = getPoint(lineElem);
            if (poPoint)
            {
                ls->addPoint(poPoint);
                delete poPoint;
            }
        }
        else if (cmpStr(ILI2_ARC, pszTagName) == 0)
        {
            // Finish line and start arc
            if (ls->getNumPoints() > 1)
            {
                ogrCurve->addCurveDirectly(ls);
                ls = new OGRLineString();
            }
            else
            {
                ls->empty();
            }
            OGRCircularString *arc = new OGRCircularString();
            // end point
            OGRPoint *ptEnd = new OGRPoint();
            // point on the arc
            OGRPoint *ptOnArc = new OGRPoint();
            // radius
            // double radius = 0;

            DOMElement *arcElem =
                dynamic_cast<DOMElement *>(lineElem->getFirstChild());
            while (arcElem != nullptr)
            {
                char *pszTagName2 = XMLString::transcode(arcElem->getTagName());
                char *pszObjValue = getObjValue(arcElem);
                if (pszObjValue)
                {
                    if (cmpStr("C1", pszTagName2) == 0)
                        ptEnd->setX(CPLAtof(pszObjValue));
                    else if (cmpStr("C2", pszTagName2) == 0)
                        ptEnd->setY(CPLAtof(pszObjValue));
                    else if (cmpStr("C3", pszTagName2) == 0)
                        ptEnd->setZ(CPLAtof(pszObjValue));
                    else if (cmpStr("A1", pszTagName2) == 0)
                        ptOnArc->setX(CPLAtof(pszObjValue));
                    else if (cmpStr("A2", pszTagName2) == 0)
                        ptOnArc->setY(CPLAtof(pszObjValue));
                    else if (cmpStr("A3", pszTagName2) == 0)
                        ptOnArc->setZ(CPLAtof(pszObjValue));
                    else if (cmpStr("R", pszTagName2) == 0)
                    {
                        // radius = CPLAtof(pszObjValue);
                    }
                }
                CPLFree(pszObjValue);
                XMLString::release(&pszTagName2);

                arcElem = dynamic_cast<DOMElement *>(arcElem->getNextSibling());
            }

            auto elemPrev =
                dynamic_cast<DOMElement *>(lineElem->getPreviousSibling());
            if (elemPrev)
            {
                OGRPoint *ptStart = getPoint(elemPrev);  // COORD or ARC
                if (ptStart)
                    arc->addPoint(ptStart);
                delete ptStart;
            }
            arc->addPoint(ptOnArc);
            arc->addPoint(ptEnd);
            ogrCurve->addCurveDirectly(arc);

            // Add arc endpoint as next start point, if COORD sequence follows.
            DOMElement *nextElem =
                dynamic_cast<DOMElement *>(lineElem->getNextSibling());
            if (nextElem)
            {
                char *nextTagName =
                    XMLString::transcode(nextElem->getTagName());
                if (cmpStr(ILI2_COORD, nextTagName) == 0)
                {
                    ls->addPoint(ptEnd);
                }
                XMLString::release(&nextTagName);
            }

            delete ptEnd;
            delete ptOnArc;
        } /* else { // TODO: StructureValue in Polyline not yet supported
        } */
        XMLString::release(&pszTagName);

        lineElem = dynamic_cast<DOMElement *>(lineElem->getNextSibling());
    }

    if (ls->getNumPoints() > 1)
    {
        ogrCurve->addCurveDirectly(ls);
    }
    else
    {
        delete ls;
    }
    return ogrCurve;
}

static OGRCompoundCurve *getBoundary(DOMElement *elem)
{

    DOMElement *lineElem = dynamic_cast<DOMElement *>(elem->getFirstChild());
    if (lineElem != nullptr)
    {
        char *pszTagName = XMLString::transcode(lineElem->getTagName());
        if (cmpStr(ILI2_POLYLINE, pszTagName) == 0)
        {
            XMLString::release(&pszTagName);
            return getPolyline(lineElem);
        }
        XMLString::release(&pszTagName);
    }

    return new OGRCompoundCurve();
}

static OGRCurvePolygon *getPolygon(DOMElement *elem)
{
    OGRCurvePolygon *pg = new OGRCurvePolygon();

    DOMElement *boundaryElem =
        dynamic_cast<DOMElement *>(elem->getFirstChild());  // outer boundary
    while (boundaryElem != nullptr)
    {
        char *pszTagName = XMLString::transcode(boundaryElem->getTagName());
        if (cmpStr(ILI2_BOUNDARY, pszTagName) == 0)
        {
            OGRCompoundCurve *poCC = getBoundary(boundaryElem);
            if (pg->addRingDirectly(poCC) != OGRERR_NONE)
            {
                delete poCC;
            }
        }
        XMLString::release(&pszTagName);
        boundaryElem = dynamic_cast<DOMElement *>(
            boundaryElem->getNextSibling());  // inner boundaries
    }

    return pg;
}

OGRGeometry *ILI2Reader::getGeometry(DOMElement *elem, int type)
{
    OGRGeometryCollection *gm = new OGRGeometryCollection();

    DOMElement *childElem = elem;
    while (childElem != nullptr)
    {
        char *pszTagName = XMLString::transcode(childElem->getTagName());
        switch (type)
        {
            case ILI2_COORD_TYPE:
                if (cmpStr(ILI2_COORD, pszTagName) == 0)
                {
                    delete gm;
                    XMLString::release(&pszTagName);
                    return getPoint(childElem);
                }
                break;
            case ILI2_ARC_TYPE:
                // is it possible here? It have to be a ARC or COORD before
                // (getPreviousSibling)
                if (cmpStr(ILI2_ARC, pszTagName) == 0)
                {
                    delete gm;
                    XMLString::release(&pszTagName);
                    return getArc(childElem);
                }
                break;
            case ILI2_POLYLINE_TYPE:
                if (cmpStr(ILI2_POLYLINE, pszTagName) == 0)
                {
                    delete gm;
                    XMLString::release(&pszTagName);
                    return getPolyline(childElem);
                }
                break;
            case ILI2_BOUNDARY_TYPE:
                if (cmpStr(ILI2_BOUNDARY, pszTagName) == 0)
                {
                    delete gm;
                    XMLString::release(&pszTagName);
                    return getPolyline(childElem);
                }
                break;
            case ILI2_AREA_TYPE:
                if ((cmpStr(ILI2_AREA, pszTagName) == 0) ||
                    (cmpStr(ILI2_SURFACE, pszTagName) == 0))
                {
                    delete gm;
                    XMLString::release(&pszTagName);
                    return getPolygon(childElem);
                }
                break;
            default:
                if (type >= ILI2_GEOMCOLL_TYPE)
                {
                    int subType = getGeometryTypeOfElem(childElem);  //????
                    OGRGeometry *poSubGeom = getGeometry(childElem, subType);
                    if (poSubGeom)
                        gm->addGeometryDirectly(poSubGeom);
                }
                break;
        }
        XMLString::release(&pszTagName);

        // GEOMCOLL
        childElem = dynamic_cast<DOMElement *>(childElem->getNextSibling());
    }

    return gm;
}

int ILI2Reader::ReadModel(OGRILI2DataSource *poDS, ImdReader *poImdReader,
                          const char *modelFilename)
{
    poImdReader->ReadModel(modelFilename);
    for (FeatureDefnInfos::const_iterator it =
             poImdReader->featureDefnInfos.begin();
         it != poImdReader->featureDefnInfos.end(); ++it)
    {
        m_listLayer.push_back(std::make_unique<OGRILI2Layer>(
            it->GetTableDefnRef(), it->poGeomFieldInfos, poDS));
    }
    return 0;
}

// Detect field name of value element
static char *fieldName(DOMElement *elem)
{
    DOMNode *node = elem;
    if (getGeometryTypeOfElem(elem))
    {
        int depth = 0;  // Depth of value elem node
        for (node = elem; node; node = node->getParentNode())
            ++depth;
        // Field name is on level 4
        node = elem;
        for (int d = 0; d < depth - 4; ++d)
            node = node->getParentNode();
    }
    if (node == nullptr)
    {
        CPLError(CE_Failure, CPLE_AssertionFailed, "node == NULL");
        return CPLStrdup("***bug***");
    }
    return CPLStrdup(transcode(node->getNodeName()));
}

void ILI2Reader::setFieldDefn(OGRFeatureDefn *featureDef, DOMElement *elem)
{
    int type = 0;
    // recursively search children
    for (DOMNode *childNode = elem->getFirstChild();
         type == 0 && childNode &&
         childNode->getNodeType() == DOMNode::ELEMENT_NODE;
         childNode = childNode->getNextSibling())
    {
        DOMElement *childElem = dynamic_cast<DOMElement *>(childNode);
        CPLAssert(childElem);
        type = getGeometryTypeOfElem(childElem);
        if (type == 0)
        {
            if (childElem->getFirstChild() &&
                childElem->getFirstChild()->getNodeType() ==
                    DOMNode::ELEMENT_NODE)
            {
                setFieldDefn(featureDef, childElem);
            }
            else
            {
                char *fName = fieldName(childElem);
                if (featureDef->GetFieldIndex(fName) == -1)
                {
                    CPLDebug("OGR_ILI", "AddFieldDefn: %s", fName);
                    OGRFieldDefn oFieldDefn(fName, OFTString);
                    featureDef->AddFieldDefn(&oFieldDefn);
                }
                CPLFree(fName);
            }
        }
    }
}

void ILI2Reader::SetFieldValues(OGRFeature *feature, DOMElement *elem)
{
    int type = 0;
    // recursively search children
    for (DOMNode *childNode = elem->getFirstChild();
         type == 0 && childNode &&
         childNode->getNodeType() == DOMNode::ELEMENT_NODE;
         childNode = childNode->getNextSibling())
    {
        DOMElement *childElem = dynamic_cast<DOMElement *>(childNode);
        CPLAssert(childElem);
        type = getGeometryTypeOfElem(childElem);
        if (type == 0)
        {
            if (childElem->getFirstChild() &&
                childElem->getFirstChild()->getNodeType() ==
                    DOMNode::ELEMENT_NODE)
            {
                SetFieldValues(feature, childElem);
            }
            else
            {
                char *fName = fieldName(childElem);
                int fIndex = feature->GetFieldIndex(fName);
                if (fIndex != -1)
                {
                    char *objVal = getObjValue(childElem);
                    if (objVal == nullptr)
                        objVal = getREFValue(childElem);  // only to try
                    feature->SetField(fIndex, objVal);
                    CPLFree(objVal);
                }
                else
                {
                    CPLDebug("OGR_ILI", "Attribute '%s' not found", fName);
                    m_missAttrs.push_back(fName);
                }
                CPLFree(fName);
            }
        }
        else
        {
            char *fName = fieldName(childElem);
            int fIndex = feature->GetGeomFieldIndex(fName);
            OGRGeometry *geom = getGeometry(childElem, type);
            if (geom)
            {
                if (fIndex == -1)
                {  // Unknown model
                    feature->SetGeometryDirectly(geom);
                }
                else
                {
                    OGRwkbGeometryType geomType =
                        feature->GetGeomFieldDefnRef(fIndex)->GetType();
                    if (geomType == wkbMultiLineString ||
                        geomType == wkbPolygon)
                    {
                        feature->SetGeomFieldDirectly(
                            fIndex, geom->getLinearGeometry());
                        delete geom;
                    }
                    else
                    {
                        feature->SetGeomFieldDirectly(fIndex, geom);
                    }
                }
            }
            CPLFree(fName);
        }
    }
}

//
// ILI2Reader
//
IILI2Reader::~IILI2Reader()
{
}

ILI2Reader::ILI2Reader()
    : m_pszFilename(nullptr), m_poILI2Handler(nullptr), m_poSAXReader(nullptr),
      m_bReadStarted(FALSE), m_bXercesInitialized(false)
{
    SetupParser();
}

ILI2Reader::~ILI2Reader()
{
    CPLFree(m_pszFilename);

    CleanupParser();

    if (m_bXercesInitialized)
        OGRDeinitializeXerces();
}

void ILI2Reader::SetSourceFile(const char *pszFilename)
{
    CPLFree(m_pszFilename);
    m_pszFilename = CPLStrdup(pszFilename);
}

int ILI2Reader::SetupParser()
{

    if (!m_bXercesInitialized)
    {
        if (!OGRInitializeXerces())
            return FALSE;
        m_bXercesInitialized = true;
    }

    // Cleanup any old parser.
    if (m_poSAXReader != nullptr)
        CleanupParser();

    // Create and initialize parser.
    m_poSAXReader = XMLReaderFactory::createXMLReader();

    m_poILI2Handler = new ILI2Handler(this);

    m_poSAXReader->setContentHandler(m_poILI2Handler);
    m_poSAXReader->setErrorHandler(m_poILI2Handler);
    m_poSAXReader->setLexicalHandler(m_poILI2Handler);
    m_poSAXReader->setEntityResolver(m_poILI2Handler);
    m_poSAXReader->setDTDHandler(m_poILI2Handler);
    m_poSAXReader->setFeature(XMLUni::fgXercesDisableDefaultEntityResolution,
                              true);

    /* No Validation
    #if (OGR_ILI2_VALIDATION)
        m_poSAXReader->setFeature(
            XMLString::transcode("http://xml.org/sax/features/validation"),
    true); m_poSAXReader->setFeature(
            XMLString::transcode("http://xml.org/sax/features/namespaces"),
    true);

        m_poSAXReader->setFeature( XMLUni::fgSAX2CoreNameSpaces, true );
        m_poSAXReader->setFeature( XMLUni::fgXercesSchema, true );

    //    m_poSAXReader->setDoSchema(true);
    //    m_poSAXReader->setValidationSchemaFullChecking(true);
    #else
    */
    XMLCh *tmpCh =
        XMLString::transcode("http://xml.org/sax/features/validation");
    m_poSAXReader->setFeature(tmpCh, false);
    XMLString::release(&tmpCh);
    tmpCh = XMLString::transcode("http://xml.org/sax/features/namespaces");
    m_poSAXReader->setFeature(tmpCh, false);
    XMLString::release(&tmpCh);
    // #endif

    m_bReadStarted = FALSE;

    return TRUE;
}

void ILI2Reader::CleanupParser()
{
    if (m_poSAXReader == nullptr)
        return;

    delete m_poSAXReader;
    m_poSAXReader = nullptr;

    delete m_poILI2Handler;
    m_poILI2Handler = nullptr;

    m_bReadStarted = FALSE;
}

int ILI2Reader::SaveClasses(const char *pszFile = nullptr)
{

    // Add logic later to determine reasonable default schema file.
    if (pszFile == nullptr)
        return FALSE;

    VSILFILE *fp = VSIFOpenL(pszFile, "rb");
    if (fp == nullptr)
        return FALSE;

    InputSource *is = OGRCreateXercesInputSource(fp);

    // parse and create layers and features
    try
    {
        CPLDebug("OGR_ILI", "Parsing %s", pszFile);
        m_poSAXReader->parse(*is);
        VSIFCloseL(fp);
        OGRDestroyXercesInputSource(is);
    }
    catch (const DOMException &toCatch)
    {
        // Can happen with createElement() in ILI2Handler::startElement()
        CPLError(CE_Failure, CPLE_AppDefined, "DOMException: %s\n",
                 transcode(toCatch.getMessage()).c_str());
        VSIFCloseL(fp);
        OGRDestroyXercesInputSource(is);
        return FALSE;
    }
    catch (const SAXException &toCatch)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Parsing failed: %s\n",
                 transcode(toCatch.getMessage()).c_str());
        VSIFCloseL(fp);
        OGRDestroyXercesInputSource(is);
        return FALSE;
    }

    if (!m_missAttrs.empty())
    {
        m_missAttrs.sort();
        m_missAttrs.unique();
        string attrs = "";
        list<string>::const_iterator it;
        for (it = m_missAttrs.begin(); it != m_missAttrs.end(); ++it)
            attrs += *it + ", ";

        CPLError(CE_Warning, CPLE_NotSupported,
                 "Failed to add new definition to existing layers, attributes "
                 "not saved: %s",
                 attrs.c_str());
    }

    return TRUE;
}

std::vector<std::unique_ptr<OGRLayer>> &ILI2Reader::GetLayers()
{
    return m_listLayer;
}

int ILI2Reader::GetLayerCount()
{
    return static_cast<int>(m_listLayer.size());
}

OGRLayer *ILI2Reader::GetLayer(const char *pszName)
{
    for (auto it = m_listLayer.rbegin(); it != m_listLayer.rend(); ++it)
    {
        const OGRFeatureDefn *fDef = (*it)->GetLayerDefn();
        if (cmpStr(fDef->GetName(), pszName) == 0)
        {
            return it->get();
        }
    }
    return nullptr;
}

int ILI2Reader::AddFeature(DOMElement *elem)
{
    CPLString osName(transcode(elem->getTagName()));
    // CPLDebug( "OGR_ILI", "Reading layer: %s", osName.c_str() );

    // test if this layer exist
    OGRILI2Layer *curLayer = cpl::down_cast<OGRILI2Layer *>(GetLayer(osName));
    const bool needsNewLayer = (curLayer == nullptr);
    std::unique_ptr<OGRILI2Layer> newLayer;

    // add a layer
    if (needsNewLayer)
    {
        CPLDebug("OGR_ILI", "Adding layer: %s", osName.c_str());
        OGRFeatureDefn *poFeatureDefn = new OGRFeatureDefn(osName);
        poFeatureDefn->SetGeomType(wkbUnknown);
        GeomFieldInfos oGeomFieldInfos;
        newLayer = std::make_unique<OGRILI2Layer>(poFeatureDefn,
                                                  oGeomFieldInfos, nullptr);
        curLayer = newLayer.get();
    }

    // the feature and field definition
    OGRFeatureDefn *featureDef = curLayer->GetLayerDefn();
    if (needsNewLayer)
    {
        // add TID field
        OGRFieldDefn ofieldDefn(ILI2_TID, OFTString);
        featureDef->AddFieldDefn(&ofieldDefn);

        setFieldDefn(featureDef, elem);
    }

    // add the features
    OGRFeature *feature = new OGRFeature(featureDef);

    // assign TID
    int fIndex = feature->GetFieldIndex(ILI2_TID);
    if (fIndex != -1)
    {
        feature->SetField(
            fIndex, transcode(elem->getAttribute(xmlch_ILI2_TID)).c_str());
    }
    else
    {
        CPLDebug("OGR_ILI", "'%s' not found", ILI2_TID);
    }

    SetFieldValues(feature, elem);
    curLayer->AddFeature(feature);

    if (needsNewLayer)
        m_listLayer.push_back(std::move(newLayer));

    return 0;
}

IILI2Reader *CreateILI2Reader()
{
    return new ILI2Reader();
}

void DestroyILI2Reader(IILI2Reader *reader)
{
    if (reader)
        delete reader;
}
