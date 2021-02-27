/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  OGR Driver for DGNv8
 * Author:   Even Rouault <even.rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2017, Even Rouault <even.rouault at spatialys.com>
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

#include "ogr_dgnv8.h"
#include "cpl_conv.h"
#include "ogr_featurestyle.h"
#include "ogr_api.h"

#include <math.h>
#include <algorithm>

/* -------------------------------------------------------------------- */
/*      Line Styles                                                     */
/* -------------------------------------------------------------------- */
#define DGNS_SOLID              0
#define DGNS_DOTTED             1
#define DGNS_MEDIUM_DASH        2
#define DGNS_LONG_DASH          3
#define DGNS_DOT_DASH           4
#define DGNS_SHORT_DASH         5
#define DGNS_DASH_DOUBLE_DOT    6
#define DGNS_LONG_DASH_SHORT_DASH 7

constexpr double DEG_TO_RAD = M_PI / 180.0;
constexpr double RAD_TO_DEG = 180.0 / M_PI;
constexpr double CONTIGUITY_TOLERANCE = 1e10; // Arbitrary high value

CPL_CVSID("$Id$")

static CPLString ToUTF8(const OdString& str)
{
    return OGRDGNV8DataSource::ToUTF8(str);
}

/************************************************************************/
/*                       EscapeDoubleQuote()                            */
/************************************************************************/

static CPLString EscapeDoubleQuote(const char* pszStr)
{
    if( strchr( pszStr, '"') != nullptr )
    {
        CPLString osEscaped;

        for( size_t iC = 0; pszStr[iC] != '\0'; iC++ )
        {
            if( pszStr[iC] == '"' )
                osEscaped += "\\\"";
            else
                osEscaped += pszStr[iC];
        }
        return osEscaped;
    }
    else
    {
        return pszStr;
    }
}

/************************************************************************/
/*                          OGRDGNV8Layer()                             */
/************************************************************************/

OGRDGNV8Layer::OGRDGNV8Layer( OGRDGNV8DataSource* poDS,
                              OdDgModelPtr pModel ) :
    m_poDS(poDS),
    m_poFeatureDefn(nullptr),
    m_pModel(pModel),
    m_pIterator(static_cast<OdDgElementIterator*>(nullptr)),
    m_nIdxInPendingFeatures(0)
{
    const char* pszName;
    if( pModel->getName().isEmpty() )
        pszName = CPLSPrintf("Model #%d", pModel->getEntryId());
    else
        pszName = CPLSPrintf("%s", ToUTF8(pModel->getName()).c_str());
    CPLDebug("DGNV8", "%s is %dd", pszName,
             pModel->getModelIs3dFlag() ? 3 : 2);

/* -------------------------------------------------------------------- */
/*      Create the feature definition.                                  */
/* -------------------------------------------------------------------- */
    m_poFeatureDefn = new OGRFeatureDefn( pszName );
    SetDescription( m_poFeatureDefn->GetName() );
    m_poFeatureDefn->Reference();

    OGRFieldDefn oField( "", OFTInteger );

/* -------------------------------------------------------------------- */
/*      Element type                                                    */
/* -------------------------------------------------------------------- */
    oField.SetName( "Type" );
    oField.SetType( OFTInteger );
    oField.SetWidth( 2 );
    oField.SetPrecision( 0 );
    m_poFeatureDefn->AddFieldDefn( &oField );

/* -------------------------------------------------------------------- */
/*      Level number.                                                   */
/* -------------------------------------------------------------------- */
    oField.SetName( "Level" );
    oField.SetType( OFTInteger );
    oField.SetWidth( 2 );
    oField.SetPrecision( 0 );
    m_poFeatureDefn->AddFieldDefn( &oField );

/* -------------------------------------------------------------------- */
/*      graphic group                                                   */
/* -------------------------------------------------------------------- */
    oField.SetName( "GraphicGroup" );
    oField.SetType( OFTInteger );
    oField.SetWidth( 4 );
    oField.SetPrecision( 0 );
    m_poFeatureDefn->AddFieldDefn( &oField );

/* -------------------------------------------------------------------- */
/*      ColorIndex                                                      */
/* -------------------------------------------------------------------- */
    oField.SetName( "ColorIndex" );
    oField.SetType( OFTInteger );
    oField.SetWidth( 3 );
    oField.SetPrecision( 0 );
    m_poFeatureDefn->AddFieldDefn( &oField );

/* -------------------------------------------------------------------- */
/*      Weight                                                          */
/* -------------------------------------------------------------------- */
    oField.SetName( "Weight" );
    oField.SetType( OFTInteger );
    oField.SetWidth( 2 );
    oField.SetPrecision( 0 );
    m_poFeatureDefn->AddFieldDefn( &oField );

/* -------------------------------------------------------------------- */
/*      Style                                                           */
/* -------------------------------------------------------------------- */
    oField.SetName( "Style" );
    oField.SetType( OFTInteger );
    oField.SetWidth( 1 );
    oField.SetPrecision( 0 );
    m_poFeatureDefn->AddFieldDefn( &oField );

/* -------------------------------------------------------------------- */
/*      Text                                                            */
/* -------------------------------------------------------------------- */
    oField.SetName( "Text" );
    oField.SetType( OFTString );
    oField.SetWidth( 0 );
    oField.SetPrecision( 0 );
    m_poFeatureDefn->AddFieldDefn( &oField );
    
/* -------------------------------------------------------------------- */
/*      ULink                                                           */
/* -------------------------------------------------------------------- */
    oField.SetName( "ULink" );
    oField.SetType( OFTString );
    oField.SetSubType( OFSTJSON );
    oField.SetWidth( 0 );
    oField.SetPrecision( 0 );
    m_poFeatureDefn->AddFieldDefn( &oField );

    OGRDGNV8Layer::ResetReading();
}

/************************************************************************/
/*                          ~OGRDGNV8Layer()                            */
/************************************************************************/

OGRDGNV8Layer::~OGRDGNV8Layer()

{
    CleanPendingFeatures();
    m_poFeatureDefn->Release();
}
/************************************************************************/
/*                       CleanPendingFeatures()                         */
/************************************************************************/

void OGRDGNV8Layer::CleanPendingFeatures()

{
    for( size_t i = m_nIdxInPendingFeatures;
                i < m_aoPendingFeatures.size(); i++ )
    {
        delete m_aoPendingFeatures[i].first;
    }
    m_aoPendingFeatures.clear();
    m_nIdxInPendingFeatures = 0;
}
    
/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGRDGNV8Layer::ResetReading()

{
    if( m_pModel.get() )
    {
        m_pIterator = m_pModel->createGraphicsElementsIterator();
    }
    CleanPendingFeatures();
}

/************************************************************************/
/*                         CollectSubElements()                         */
/************************************************************************/

std::vector<tPairFeatureHoleFlag> OGRDGNV8Layer::CollectSubElements(
                            OdDgElementIteratorPtr iterator, int level )
{
    std::vector<tPairFeatureHoleFlag> oVector;
#ifdef DEBUG_VERBOSE
    CPLString osIndent;
    for(int i=0;i<level;i++)
        osIndent += "  ";
    int counter = 0;
#endif
    for( ; !iterator->done(); iterator->step())
    {
#ifdef DEBUG_VERBOSE
        fprintf(stdout, "%sSub element %d:\n", osIndent.c_str(), counter );
        counter ++;
#endif
        OdRxObjectPtr object = iterator->item().openObject( OdDg::kForRead );
        OdDgGraphicsElementPtr element = OdDgGraphicsElement::cast( object );
        if (element.isNull())
            continue;

        std::vector<tPairFeatureHoleFlag>  oSubVector =
            ProcessElement(element, level + 1);
        oVector.insert(oVector.end(), oSubVector.begin(), oSubVector.end());
    }
    return oVector;
}

/************************************************************************/
/*                        GetAnchorPosition()                           */
/************************************************************************/

static int GetAnchorPosition( OdDg::TextJustification value )
{
    switch( value )
    {
        case OdDg::kLeftTop           : return 7;
        case OdDg::kLeftCenter        : return 4;
        case OdDg::kLeftBottom        : return 10;
        case OdDg::kLeftMarginTop     : return 7;
        case OdDg::kLeftMarginCenter  : return 4;
        case OdDg::kLeftMarginBottom  : return 10;
        case OdDg::kCenterTop         : return 8;
        case OdDg::kCenterCenter      : return 5;
        case OdDg::kCenterBottom      : return 11;
        case OdDg::kRightMarginTop    : return 9;
        case OdDg::kRightMarginCenter : return 6;
        case OdDg::kRightMarginBottom : return 12;
        case OdDg::kRightTop          : return 9;
        case OdDg::kRightCenter       : return 6;
        case OdDg::kRightBottom       : return 12;
        case OdDg::kLeftDescender     : return 1;
        case OdDg::kCenterDescender   : return 2;
        case OdDg::kRightDescender    : return 3;
        default                       : return 0;
    }
}

/************************************************************************/
/*                        GetAnchorPosition()                           */
/************************************************************************/

static OdDg::TextJustification GetAnchorPositionFromOGR( int value )
{
    switch( value )
    {
        case 1:  return OdDg::kLeftDescender;
        case 2:  return OdDg::kCenterDescender;
        case 3:  return OdDg::kRightDescender;
        case 4:  return OdDg::kLeftCenter;
        case 5:  return OdDg::kCenterCenter;
        case 6:  return OdDg::kRightCenter;
        case 7:  return OdDg::kLeftTop;
        case 8:  return OdDg::kCenterTop;
        case 9:  return OdDg::kRightTop;
        case 10: return OdDg::kLeftBottom;
        case 11: return OdDg::kCenterBottom;
        case 12: return OdDg::kRightBottom;
        default: return OdDg::kLeftTop;
    }
}

/************************************************************************/
/*                           AlmostEqual()                              */
/************************************************************************/

static bool AlmostEqual(double dfA, double dfB)
{
    if( fabs(dfB) > 1e-7 )
        return fabs((dfA - dfB) / dfB) < 1e-6;
    else
        return fabs(dfA) <= 1e-7;
}

/************************************************************************/
/*                         ProcessTextTraits                           */
/************************************************************************/

template<class TextPtr> struct ProcessTextTraits
{
};

template<> struct ProcessTextTraits<OdDgText2dPtr>
{
    static double getRotation(OdDgText2dPtr text)
    {
        return text->getRotation();
    }

    static void setGeom(OGRFeature* poFeature, OdDgText2dPtr text)
    {
        OdGePoint2d point = text->getOrigin();
        poFeature->SetGeometryDirectly(
                new OGRPoint(point.x, point.y) );
    }
};

template<> struct ProcessTextTraits<OdDgText3dPtr>
{
    static double getRotation(OdDgText3dPtr /*text*/)
    {
        return 0.0;
    }

    static void setGeom(OGRFeature* poFeature, OdDgText3dPtr text)
    {
        OdGePoint3d point = text->getOrigin();
        poFeature->SetGeometryDirectly(
                new OGRPoint(point.x, point.y, point.z) );
    }
};

/************************************************************************/
/*                           ProcessText()                              */
/************************************************************************/

template<class TextPtr >
static void ProcessText(OGRFeature* poFeature,
                        const CPLString& osColor,
                        TextPtr text)
{
        const OdString oText = text->getText();
        poFeature->SetField( "Text", ToUTF8(oText).c_str());

        CPLString osStyle;
        osStyle.Printf("LABEL(t:\"%s\"",
                        EscapeDoubleQuote(ToUTF8(oText)).c_str());
        osStyle += osColor;
        osStyle += CPLSPrintf(",s:%fg", text->getHeightMultiplier());

        // Gets Font name
        OdDgFontTablePtr pFontTable = text->database()->getFontTable();
        OdDgFontTableRecordPtr pFont =
            pFontTable->getFont(text->getFontEntryId());
        if (!pFont.isNull())
        {
            osStyle += CPLSPrintf(",f:\"%s\"",
                EscapeDoubleQuote(ToUTF8(pFont->getName())).c_str() );
        }
        else
        {
            osStyle += CPLSPrintf(",f:MstnFont%u",
                text->getFontEntryId() );
        }

        const int nAnchor = GetAnchorPosition(text->getJustification());
        if( nAnchor > 0 )
            osStyle += CPLSPrintf(",p:%d", nAnchor);
        
        // Add the angle, if not horizontal
        const double dfRotation =
                        ProcessTextTraits<TextPtr>::getRotation(text);
        if( dfRotation != 0.0 )
          osStyle += CPLSPrintf(",a:%d",
                   static_cast<int>(floor(dfRotation * RAD_TO_DEG +0.5)) );

        osStyle += ")";
        poFeature->SetStyleString(osStyle);
        ProcessTextTraits<TextPtr>::setGeom( poFeature, text );
}

/************************************************************************/
/*                            ConsiderBrush()                           */
/************************************************************************/

static CPLString ConsiderBrush(OdDgGraphicsElementPtr element,
                                       const CPLString& osStyle)
{
    CPLString osNewStyle(osStyle);
    OdRxObjectPtrArray linkages;
    element->getLinkages(OdDgAttributeLinkage::kFillStyle, linkages);
    if( linkages.length() >= 1 )
    {
        OdDgFillColorLinkagePtr fillColor =
            OdDgFillColorLinkage::cast( linkages[0] );
        if( !fillColor.isNull() )
        {
            const OdUInt32 uFillColorIdx = fillColor->getColorIndex();
            if( OdDgColorTable::isCorrectIndex(
                                    element->database(), uFillColorIdx  ) )
            {
                ODCOLORREF color = OdDgColorTable::lookupRGB(
                    element->database(), uFillColorIdx  );
                const char* pszBrush = CPLSPrintf(
                    "BRUSH(fc:#%02x%02x%02x,id:\"ogr-brush-0\")",
                    ODGETRED(color),
                    ODGETGREEN(color),
                    ODGETBLUE(color));
                const OdUInt32 uColorIndex = element->getColorIndex();
                if( uFillColorIdx != uColorIndex )
                {
                    osNewStyle = CPLString(pszBrush) + ";" + osNewStyle;
                }
                else
                {
                    osNewStyle = pszBrush;
                }
            }
        }
    }
    return osNewStyle;
}

/************************************************************************/
/*                         ProcessCurveTraits                           */
/************************************************************************/

template<class CurveElementPtr> struct ProcessCurveTraits
{
};

template<> struct ProcessCurveTraits<OdDgCurveElement2dPtr>
{
    typedef OdDgCurve2d         CurveType;
    typedef OdDgArc2d           ArcType;
    typedef OdDgBSplineCurve2d  BSplineType;
    typedef OdDgEllipse2d       EllipseType;
    typedef OdGePoint2d         PointType;

    static void setPoint( OGRSimpleCurve* poSC, int i, OdGePoint2d point)
    {
        poSC->setPoint(i, point.x, point.y);
    }
};

template<> struct ProcessCurveTraits<OdDgCurveElement3dPtr>
{
    typedef OdDgCurve3d         CurveType;
    typedef OdDgArc3d           ArcType;
    typedef OdDgBSplineCurve3d  BSplineType;
    typedef OdDgEllipse3d       EllipseType;
    typedef OdGePoint3d         PointType;

    static void setPoint( OGRSimpleCurve* poSC, int i, OdGePoint3d point)
    {
        poSC->setPoint(i, point.x, point.y, point.z);
    }
};

/************************************************************************/
/*                            ProcessCurve()                            */
/************************************************************************/

template<class CurveElementPtr>
static void ProcessCurve(OGRFeature* poFeature, const CPLString& osPen,
                         CurveElementPtr curveElement)
{
    typedef typename ProcessCurveTraits<CurveElementPtr>::CurveType CurveType;
    typedef typename ProcessCurveTraits<CurveElementPtr>::ArcType ArcType;
    typedef typename ProcessCurveTraits<CurveElementPtr>::BSplineType
                                                                 BSplineType;
    typedef typename ProcessCurveTraits<CurveElementPtr>::EllipseType
                                                                 EllipseType;
    typedef typename ProcessCurveTraits<CurveElementPtr>::PointType PointType;

    OdSmartPtr<CurveType> curve = CurveType::cast( curveElement );
    OdSmartPtr<ArcType> arc = ArcType::cast( curveElement );
    OdSmartPtr<BSplineType> bspline = BSplineType::cast( curveElement );
    OdSmartPtr<EllipseType> ellipse = EllipseType::cast( curveElement );

    bool bIsCircular = false;
    if( !ellipse.isNull() )
    {
        bIsCircular = AlmostEqual(ellipse->getPrimaryAxis(),
                                  ellipse->getSecondaryAxis());
    }
    else if( !arc.isNull() )
    {
        bIsCircular = AlmostEqual(arc->getPrimaryAxis(),
                                  arc->getSecondaryAxis());
    }

    double dfStartParam, dfEndParam;
    OdResult eRes = curveElement->getStartParam(dfStartParam);
    CPL_IGNORE_RET_VAL(eRes);
    CPLAssert(eRes == eOk );
    eRes = curveElement->getEndParam(dfEndParam);
    CPL_IGNORE_RET_VAL(eRes);
    CPLAssert(eRes == eOk );

    CPLString osStyle(osPen);
    bool bIsFilled = false;
    if( !ellipse.isNull() )
    {
        osStyle = ConsiderBrush(curveElement, osPen);
        bIsFilled = osStyle.find("BRUSH") == 0;
    }

    OGRSimpleCurve* poSC;
    int nPoints;
    if( !bspline.isNull() )
    {
        // 10 is somewhat arbitrary
        nPoints = 10 * bspline->numControlPoints();
        poSC = new OGRLineString();
    }
    else if( !curve.isNull() )
    {
        // 5 is what is used in DGN driver
        nPoints = 5 * curve->getVerticesCount();
        poSC = new OGRLineString();
    }
    else if( bIsCircular )
    {
        poSC = new OGRCircularString();
        if( !ellipse.isNull() )
        {
            nPoints = 5;
        }
        else
        {
            nPoints = 3;
        }
    }
    else
    {
        if( bIsFilled )
            poSC = new OGRLinearRing();
        else
            poSC = new OGRLineString();
        const double dfArcStepSize =
            CPLAtofM(CPLGetConfigOption("OGR_ARC_STEPSIZE", "4"));
        if( !ellipse.isNull() )
        {
            nPoints = std::max(2,
                static_cast<int>(360 / dfArcStepSize));
        }
        else
        {
            nPoints = std::max(2,
                static_cast<int>(arc->getSweepAngle() *
                                 RAD_TO_DEG / dfArcStepSize) );
        }
    }

    poSC->setNumPoints(nPoints);
    for( int i = 0; i < nPoints; i++ )
    {
        PointType point;
        double dfParam = dfStartParam + i *
                (dfEndParam - dfStartParam) / (nPoints - 1);
        eRes = curveElement->getPointAtParam(dfParam, point);
        CPL_IGNORE_RET_VAL(eRes);
        CPLAssert(eRes == eOk );
        ProcessCurveTraits<CurveElementPtr>::setPoint(poSC, i, point);
    }

    if( bIsFilled )
    {
        if( bIsCircular )
        {
            OGRCurvePolygon* poCP = new OGRCurvePolygon();
            poCP->addRingDirectly(poSC);
            poFeature->SetGeometryDirectly( poCP );
        }
        else
        {
            OGRPolygon* poPoly = new OGRPolygon();
            poPoly->addRingDirectly(poSC);
            poFeature->SetGeometryDirectly( poPoly );
        }
    }
    else
    {
        poFeature->SetGeometryDirectly( poSC );
    }
    poFeature->SetStyleString(osStyle);
}

/************************************************************************/
/*                           IsContiguous()                             */
/************************************************************************/

static
bool IsContiguous( const std::vector<tPairFeatureHoleFlag>& oVectorSubElts,
                   bool& bHasCurves,
                   bool& bIsClosed )
{
    bHasCurves = false;
    bIsClosed = false;
    OGRPoint oFirstPoint, oLastPoint;
    bool bLastPointValid = false;
    bool bIsContiguous = true;
    for( size_t i = 0; i < oVectorSubElts.size(); i++ )
    {
        OGRGeometry* poGeom = oVectorSubElts[i].first->GetGeometryRef();
        if( poGeom != nullptr )
        {
            OGRwkbGeometryType eType =
                                wkbFlatten(poGeom->getGeometryType());
            if( eType == wkbCircularString )
            {
                bHasCurves = true;
            }
            if( bIsContiguous &&
                (eType == wkbCircularString ||
                 eType == wkbLineString) )
            {
                OGRCurve* poCurve = poGeom->toCurve();
                if( poCurve->getNumPoints() >= 2 )
                {
                    OGRPoint oStartPoint;
                    poCurve->StartPoint( &oStartPoint );

                    if( bLastPointValid )
                    {
                        if( !AlmostEqual(oStartPoint.getX(),
                                         oLastPoint.getX()) ||
                            !AlmostEqual(oStartPoint.getY(),
                                         oLastPoint.getY()) ||
                            !AlmostEqual(oStartPoint.getZ(),
                                         oLastPoint.getZ()) )
                        {
                             bIsContiguous = false;
                             break;
                        }
                    }
                    else
                    {
                        oFirstPoint = oStartPoint;
                    }
                    bLastPointValid = true;
                    poCurve->EndPoint( &oLastPoint );
                }
                else
                {
                    bIsContiguous = false;
                    break;
                }
            }
            else
            {
                bIsContiguous = false;
                break;
            }
        }
    }
    if( bIsContiguous )
    {
        bIsClosed = bLastPointValid &&
                AlmostEqual(oFirstPoint.getX(), oLastPoint.getX()) &&
                AlmostEqual(oFirstPoint.getY(), oLastPoint.getY()) &&
                AlmostEqual(oFirstPoint.getZ(), oLastPoint.getZ());
    }
    return bIsContiguous;
}

/************************************************************************/
/*                           ProcessElement()                           */
/************************************************************************/

std::vector<tPairFeatureHoleFlag> OGRDGNV8Layer::ProcessElement(
                                            OdDgGraphicsElementPtr element,
                                            int level)
{
    std::vector<tPairFeatureHoleFlag> oVector;
#ifdef DEBUG_VERBOSE
    CPLString osIndent;
    for(int i=0;i<level;i++)
        osIndent += "  ";
#endif

    bool bHoleFlag = false;
    OGRFeature  *poFeature = new OGRFeature( m_poFeatureDefn );

    OdRxClass *poClass = element->isA();
    const OdString osName = poClass->name();
    const char *pszEntityClassName = static_cast<const char*>(osName);
    poFeature->SetFID(
        static_cast<GIntBig>(
            static_cast<OdUInt64>(element->elementId().getHandle()) ) );
#ifdef DEBUG_VERBOSE
    fprintf(stdout, "%s%s\n", osIndent.c_str(), pszEntityClassName);
    fprintf(stdout, "%sID = %s\n", 
            osIndent.c_str(),
            static_cast<const char*>(element->elementId().getHandle().ascii()) );
    fprintf(stdout, "%sType = %d\n",
            osIndent.c_str(), element->getElementType() );
#endif

    poFeature->SetField("Type", element->getElementType() );
    const int nLevel = static_cast<int>(element->getLevelEntryId());
    poFeature->SetField("Level", nLevel );
    poFeature->SetField("GraphicGroup", 
                        static_cast<int>(element->getGraphicsGroupEntryId()) );
    const OdUInt32 uColorIndex = element->getColorIndex();
    CPLString osColor;
    if( uColorIndex != OdDg::kColorByLevel &&
        uColorIndex != OdDg::kColorByCell )
    {
        poFeature->SetField("ColorIndex",
                            static_cast<int>(uColorIndex));

        const ODCOLORREF color = element->getColor();
        osColor = CPLSPrintf(",c:#%02x%02x%02x",
                              ODGETRED(color),
                              ODGETGREEN(color),
                              ODGETBLUE(color));
    }
    const OdInt32 nLineStyle = element->getLineStyleEntryId();
    if( nLineStyle != OdDg::kLineStyleByLevel &&
        nLineStyle != OdDg::kLineStyleByCell )
    {
        poFeature->SetField("Style", nLineStyle);
    }

    const OdUInt32 uLineWeight = element->getLineWeight();
    int nLineWeight = 0;
    if( uLineWeight != OdDg::kLineWeightByLevel && 
        uLineWeight != OdDg::kLineWeightByCell )
    {
        nLineWeight = static_cast<int>(uLineWeight);
        poFeature->SetField("Weight", nLineWeight);
    }

    CPLJSONObject uLinkData;
    CPLJSONArray previousValues;

    OdRxObjectPtrArray linkages;
    element->getLinkages(linkages);
    if( linkages.size() > 0 )
    {
        for(unsigned i = 0; i < linkages.size(); ++i)
        {
            OdDgAttributeLinkagePtr pLinkage = linkages[i];
            OdUInt16 primaryId = pLinkage->getPrimaryId();
            CPLString primaryIdStr = CPLSPrintf("%d", primaryId );

            OdBinaryData pabyData;
            pLinkage->getData(pabyData);

            previousValues = uLinkData.GetArray( primaryIdStr.c_str() );
            if (!previousValues.IsValid() ) 
            {
                uLinkData.Add( primaryIdStr.c_str(), CPLJSONArray() );
                previousValues = uLinkData.GetArray( primaryIdStr.c_str() );
            } 
            CPLJSONObject theNewObject = CPLJSONObject();
            GByte* pData = pabyData.asArrayPtr();
            int nSize = pabyData.size();
            theNewObject.Add( "size", nSize );
            previousValues.Add( theNewObject );
            switch( primaryId ) {
                    case OdDgAttributeLinkage::kFRAMME    : // DB Linkage - FRAMME tag data signature
                    case OdDgAttributeLinkage::kBSI       : // DB Linkage - secondary id link (BSI radix 50)
                    case OdDgAttributeLinkage::kXBASE     : // DB Linkage - XBase (DBase)
                    case OdDgAttributeLinkage::kINFORMIX  : // DB Linkage - Informix
                    case OdDgAttributeLinkage::kINGRES    : // DB Linkage - INGRES
                    case OdDgAttributeLinkage::kSYBASE    : // DB Linkage - Sybase
                    case OdDgAttributeLinkage::kODBC      : // DB Linkage - ODBC
                    case OdDgAttributeLinkage::kOLEDB     : // DB Linkage - OLEDB
                    case OdDgAttributeLinkage::kORACLE    : // DB Linkage - Oracle
                    case OdDgAttributeLinkage::kRIS       : // DB Linkage - RIS
                    {
                        OdDgDBLinkagePtr dbLinkage = OdDgDBLinkage::cast( pLinkage );
                        if( !dbLinkage.isNull() )
                        {
                            std::string namedType;

                            switch( dbLinkage->getDBType() )
                            {
                            case OdDgDBLinkage::kBSI: 
                                namedType.assign("BSI");
                            break;
                            case OdDgDBLinkage::kFRAMME: 
                                namedType.assign("FRAMME"); 
                            break;
                            case OdDgDBLinkage::kInformix: 
                                namedType.assign("Informix");
                            break;
                            case OdDgDBLinkage::kIngres: 
                                namedType.assign("Ingres");
                            break;
                            case OdDgDBLinkage::kODBC: 
                                namedType.assign("ODBC");
                            break;
                            case OdDgDBLinkage::kOLEDB: 
                                namedType.assign("OLE DB");
                            break;
                            case OdDgDBLinkage::kOracle: 
                                namedType.assign("Oracle"); 
                            break;
                            case OdDgDBLinkage::kRIS: 
                                namedType.assign("RIS"); 
                            break;
                            case OdDgDBLinkage::kSybase: 
                                namedType.assign("Sybase");
                            break;
                            case OdDgDBLinkage::kXbase: 
                                namedType.assign("xBase");
                            break;
                            default: 
                                namedType.assign("Unknown"); 
                            break;
                            }

                            theNewObject.Add( "tableId", int( dbLinkage->getTableEntityId() ) );
                            theNewObject.Add( "MSLink", int( dbLinkage->getMSLink() ) );
                            theNewObject.Add( "type", namedType );
                        }
                    }
                    break;
                    case 0x1995: // 0x1995 (6549) IPCC/Portugal
                    {
                        theNewObject.Add( "domain", CPLSPrintf("0x%02x", pData[5] ) );
                        theNewObject.Add( "subdomain", CPLSPrintf("0x%02x", pData[4] ) );
                        theNewObject.Add( "family", CPLSPrintf("0x%02x", pData[7] ) );
                        theNewObject.Add( "object", CPLSPrintf("0x%02x", pData[6] ) );
                        theNewObject.Add( "key", CPLSPrintf("%02x%02x%02x%02x", pData[5], pData[4], pData[7], pData[6] ) );
                        theNewObject.Add( "type", "IPCC/Portugal" );
                    }
                    break;
                    case OdDgAttributeLinkage::kString: // 0x56d2 (22226):
                    {
                        OdDgStringLinkagePtr pStrLinkage = OdDgStringLinkage::cast( pLinkage );
                        if ( !pStrLinkage.isNull() )
                        {
                            theNewObject.Add( "string", ToUTF8(pStrLinkage->getString()).c_str() );
                            theNewObject.Add( "type", "string" );
                        }
                    }
                    break;
                    default:
                    {
                        CPLJSONArray rawWords;
                        for( int k=0; k < nSize-1; k+=2 )
                        {
                            rawWords.Add( CPLSPrintf("0x%02x%02x", pData[k+1], pData[k] ) );
                        }
                        theNewObject.Add( "raw", rawWords );
                        theNewObject.Add( "type", "unknown" );
                    }
                    break;
            }

        }

        poFeature->SetField( "ULink", uLinkData.ToString().c_str() );

    }

/* -------------------------------------------------------------------- */
/*      Generate corresponding PEN style.                               */
/* -------------------------------------------------------------------- */
    CPLString osPen;

    if( nLineStyle == DGNS_SOLID )
        osPen = "PEN(id:\"ogr-pen-0\"";
    else if( nLineStyle == DGNS_DOTTED )
        osPen = "PEN(id:\"ogr-pen-5\"";
    else if( nLineStyle == DGNS_MEDIUM_DASH )
        osPen = "PEN(id:\"ogr-pen-2\"";
    else if( nLineStyle == DGNS_LONG_DASH )
        osPen = "PEN(id:\"ogr-pen-4\"";
    else if( nLineStyle == DGNS_DOT_DASH )
        osPen = "PEN(id:\"ogr-pen-6\"";
    else if( nLineStyle == DGNS_SHORT_DASH )
        osPen = "PEN(id:\"ogr-pen-3\"";
    else if( nLineStyle == DGNS_DASH_DOUBLE_DOT )
        osPen = "PEN(id:\"ogr-pen-7\"";
    else if( nLineStyle == DGNS_LONG_DASH_SHORT_DASH )
        osPen = "PEN(p:\"10px 5px 4px 5px\"";
    else
        osPen = "PEN(id:\"ogr-pen-0\"";

    osPen += osColor;

    if( nLineWeight > 1 )
        osPen += CPLSPrintf(",w:%dpx", nLineWeight );

    osPen += ")";

    if( EQUAL(pszEntityClassName, "OdDgCellHeader2d") ||
        EQUAL(pszEntityClassName, "OdDgCellHeader3d") )
    {
        bool bDestroyFeature = true;
        OdDgElementIteratorPtr iterator;
        if( EQUAL(pszEntityClassName, "OdDgCellHeader2d") )
        {
            OdDgCellHeader2dPtr elementCell = OdDgCellHeader2d::cast( element );
            CPLAssert( !elementCell.isNull() );
            iterator = elementCell->createIterator();
        }
        else
        {
            OdDgCellHeader3dPtr elementCell = OdDgCellHeader3d::cast( element );
            CPLAssert( !elementCell.isNull() );
            iterator = elementCell->createIterator();
        }
        if( !iterator.isNull() )
        {
            oVector = CollectSubElements(iterator, level + 1 );
            int nCountMain = 0;
            bool bHasHole = false;
            bool bHasCurve = false;
            OGRGeometry* poExterior = nullptr;
            for( size_t i = 0; i < oVector.size(); i++ )
            {
                OGRFeature* poFeat = oVector[i].first;
                CPLAssert( poFeat );
                OGRGeometry* poGeom = poFeat->GetGeometryRef();
                if( poGeom == nullptr )
                {
                    nCountMain = 0;
                    break;
                }
                const OGRwkbGeometryType eType =
                    wkbFlatten(poGeom->getGeometryType());
                if( (eType == wkbPolygon || eType == wkbCurvePolygon) &&
                    poGeom->toCurvePolygon()->getNumInteriorRings() == 0 )
                {
                    if( eType == wkbCurvePolygon )
                        bHasCurve = true;
                    if( oVector[i].second )
                        bHasHole = true;
                    else
                    {
                        poExterior = poGeom;
                        nCountMain ++;
                    }
                }
                else
                {
                    nCountMain = 0;
                    break;
                }
            }
            if( nCountMain == 1 && bHasHole )
            {
                bDestroyFeature = false;
                OGRCurvePolygon* poCP;
                if( bHasCurve )
                    poCP = new OGRCurvePolygon();
                else
                    poCP = new OGRPolygon();
                poCP->addRing(poExterior->toCurvePolygon()->getExteriorRingCurve() );
                for( size_t i = 0; i < oVector.size(); i++ )
                {
                    OGRFeature* poFeat = oVector[i].first;
                    OGRGeometry* poGeom = poFeat->GetGeometryRef();
                    if( poGeom != poExterior )
                    {
                        poCP->addRing(poGeom->toCurvePolygon()->
                                            getExteriorRingCurve() );
                    }
                    delete poFeat;
                }
                oVector.clear();
                poFeature->SetGeometryDirectly(poCP);
                poFeature->SetStyleString( ConsiderBrush(element, osPen) );
            }
        }
        if( bDestroyFeature )
        {
            delete poFeature;
            poFeature = nullptr;
        }
    }
    else if( EQUAL(pszEntityClassName, "OdDgText2d") )
    {
        OdDgText2dPtr text = OdDgText2d::cast( element );
        CPLAssert( !text.isNull() );
        ProcessText(poFeature, osColor, text);
    }
    else if( EQUAL(pszEntityClassName, "OdDgText3d") )
    {
        OdDgText3dPtr text = OdDgText3d::cast( element );
        CPLAssert( !text.isNull() );
        ProcessText(poFeature, osColor, text);
    }
    else if( EQUAL(pszEntityClassName, "OdDgTextNode2d") ||
             EQUAL(pszEntityClassName, "OdDgTextNode3d") )
    {
        OdDgElementIteratorPtr iterator;
        if( EQUAL(pszEntityClassName, "OdDgTextNode2d") )
        {
            OdDgTextNode2dPtr textNode = OdDgTextNode2d::cast( element );
            CPLAssert( !textNode.isNull() );
            iterator = textNode->createIterator();
        }
        else
        {
            OdDgTextNode3dPtr textNode = OdDgTextNode3d::cast( element );
            CPLAssert( !textNode.isNull() );
            iterator = textNode->createIterator();
        }
        if( !iterator.isNull() )
        {
            oVector = CollectSubElements(iterator, level + 1 );
        }
        delete poFeature;
        poFeature = nullptr;
    }
    else if( EQUAL(pszEntityClassName, "OdDgLine2d") )
    {
        OdDgLine2dPtr line = OdDgLine2d::cast( element );
        CPLAssert( !line.isNull() );
        OdGePoint2d pointStart = line->getStartPoint( );
        OdGePoint2d pointEnd = line->getEndPoint( );
        if( pointStart == pointEnd )
        {
             poFeature->SetGeometryDirectly(
                new OGRPoint(pointStart.x, pointStart.y) );
        }
        else
        {
            OGRLineString* poLS = new OGRLineString();
            poLS->setNumPoints(2);
            poLS->setPoint(0, pointStart.x, pointStart.y);
            poLS->setPoint(1, pointEnd.x, pointEnd.y);
            poFeature->SetGeometryDirectly( poLS );
            poFeature->SetStyleString(osPen);
        }
    }
    else if( EQUAL(pszEntityClassName, "OdDgLine3d") )
    {
        OdDgLine3dPtr line = OdDgLine3d::cast( element );
        CPLAssert( !line.isNull() );
        OdGePoint3d pointStart = line->getStartPoint( );
        OdGePoint3d pointEnd = line->getEndPoint( );
        if( pointStart == pointEnd )
        {
             poFeature->SetGeometryDirectly(
                new OGRPoint(pointStart.x, pointStart.y, pointStart.z) );
        }
        else
        {
            OGRLineString* poLS = new OGRLineString();
            poLS->setNumPoints(2);
            poLS->setPoint(0, pointStart.x, pointStart.y, pointStart.z);
            poLS->setPoint(1, pointEnd.x, pointEnd.y, pointEnd.z);
            poFeature->SetGeometryDirectly( poLS );
            poFeature->SetStyleString(osPen);
        }
    }
    else if( EQUAL(pszEntityClassName, "OdDgLineString2d") )
    {
        OdDgLineString2dPtr line = OdDgLineString2d::cast( element );
        CPLAssert( !line.isNull() );
        const int nPoints = line->getVerticesCount();

        OGRLineString* poLS = new OGRLineString();
        poLS->setNumPoints(nPoints);
        for( int i = 0; i < nPoints; i++ )
        {
            OdGePoint2d point = line->getVertexAt( i );
            poLS->setPoint(i, point.x, point.y);
        }
        poFeature->SetGeometryDirectly( poLS );
        poFeature->SetStyleString(osPen);
    }
    else if( EQUAL(pszEntityClassName, "OdDgLineString3d") )
    {
        OdDgLineString3dPtr line = OdDgLineString3d::cast( element );
        CPLAssert( !line.isNull() );
        const int nPoints = line->getVerticesCount();

        OGRLineString* poLS = new OGRLineString();
        poLS->setNumPoints(nPoints);
        for( int i = 0; i < nPoints; i++ )
        {
            OdGePoint3d point = line->getVertexAt( i );
            poLS->setPoint(i, point.x, point.y, point.z);
        }
        poFeature->SetGeometryDirectly( poLS );
        poFeature->SetStyleString(osPen);
    }
    else if( EQUAL(pszEntityClassName, "OdDgPointString2d") )
    {
        OdDgPointString2dPtr string = OdDgPointString2d::cast( element );
        CPLAssert( !string.isNull() );
        const int nPoints = string->getVerticesCount();
        
        // Not sure this is the right way to model this
        // We lose the rotation per vertex.
        OGRMultiPoint* poMP = new OGRMultiPoint();
        for( int i = 0; i < nPoints; i++ )
        {
            OdGePoint2d point = string->getVertexAt( i );
            poMP->addGeometryDirectly(new OGRPoint(point.x, point.y));
        }
        poFeature->SetGeometryDirectly( poMP );
    }
    else if( EQUAL(pszEntityClassName, "OdDgPointString3d") )
    {
        OdDgPointString3dPtr string = OdDgPointString3d::cast( element );
        CPLAssert( !string.isNull() );
        const int nPoints = string->getVerticesCount();

        // Not sure this is the right way to model this
        // We lose the rotation per vertex.
        OGRMultiPoint* poMP = new OGRMultiPoint();
        for( int i = 0; i < nPoints; i++ )
        {
            OdGePoint3d point = string->getVertexAt( i );
            poMP->addGeometryDirectly(new OGRPoint(point.x, point.y, point.z));
        }
        poFeature->SetGeometryDirectly( poMP );
    }
    else if( EQUAL(pszEntityClassName, "OdDgMultiline") )
    {
        // This is a poor approximation since a multiline is a central line
        // with parallel lines.
        OdDgMultilinePtr line = OdDgMultiline::cast( element );
        CPLAssert( !line.isNull() );
        const int nPoints = static_cast<int>(line->getPointsCount());

        OGRLineString* poLS = new OGRLineString();
        poLS->setNumPoints(nPoints);
        for( int i = 0; i < nPoints; i++ )
        {
            OdDgMultilinePoint point;
            OdGePoint3d point3d;
            line->getPoint( i, point );
            point.getPoint( point3d );
            poLS->setPoint(i, point3d.x, point3d.y, point3d.z);
        }
        poFeature->SetGeometryDirectly( poLS );
        poFeature->SetStyleString(osPen);
    }
    else if( EQUAL(pszEntityClassName, "OdDgArc2d") ||
             EQUAL(pszEntityClassName, "OdDgCurve2d") ||
             EQUAL(pszEntityClassName, "OdDgBSplineCurve2d") ||
             EQUAL(pszEntityClassName, "OdDgEllipse2d") )
    {
        OdDgCurveElement2dPtr curveElement =
                            OdDgCurveElement2d::cast( element );
        CPLAssert( !curveElement.isNull() );

        ProcessCurve(poFeature, osPen, curveElement);
    }
    else if( EQUAL(pszEntityClassName, "OdDgArc3d") ||
             EQUAL(pszEntityClassName, "OdDgCurve3d") ||
             EQUAL(pszEntityClassName, "OdDgBSplineCurve3d") ||
             EQUAL(pszEntityClassName, "OdDgEllipse3d") )
    {
        OdDgCurveElement3dPtr curveElement =
                            OdDgCurveElement3d::cast( element );
        CPLAssert( !curveElement.isNull() );

        ProcessCurve(poFeature, osPen, curveElement);
    }
    else if( EQUAL(pszEntityClassName, "OdDgShape2d") )
    {
        OdDgShape2dPtr shape = OdDgShape2d::cast( element );
        CPLAssert( !shape.isNull() );
        bHoleFlag = shape->getHoleFlag();
        const int nPoints = shape->getVerticesCount();
       
        OGRLinearRing* poLS = new OGRLinearRing();
        poLS->setNumPoints(nPoints);
        for( int i = 0; i < nPoints; i++ )
        {
            OdGePoint2d point = shape->getVertexAt( i );
            poLS->setPoint(i, point.x, point.y);
        }
        OGRPolygon* poPoly = new OGRPolygon();
        poPoly->addRingDirectly(poLS);
        poFeature->SetGeometryDirectly( poPoly );
        poFeature->SetStyleString(ConsiderBrush(element, osPen));
    }
    else if( EQUAL(pszEntityClassName, "OdDgShape3d") )
    {
        OdDgShape3dPtr shape = OdDgShape3d::cast( element );
        CPLAssert( !shape.isNull() );
        bHoleFlag = shape->getHoleFlag();
        const int nPoints = shape->getVerticesCount();
       
        OGRLinearRing* poLS = new OGRLinearRing();
        poLS->setNumPoints(nPoints);
        for( int i = 0; i < nPoints; i++ )
        {
            OdGePoint3d point = shape->getVertexAt( i );
            poLS->setPoint(i, point.x, point.y, point.z);
        }
        OGRPolygon* poPoly = new OGRPolygon();
        poPoly->addRingDirectly(poLS);
        poFeature->SetGeometryDirectly( poPoly );
        poFeature->SetStyleString(ConsiderBrush(element, osPen));
    }
    else if( EQUAL(pszEntityClassName, "OdDgComplexString") )
    {
        OdDgComplexStringPtr complex = OdDgComplexString::cast( element );
        CPLAssert( !complex.isNull() );

        OdDgElementIteratorPtr iterator = complex->createIterator();
        if( !iterator.isNull() )
        {
            std::vector<tPairFeatureHoleFlag> oVectorSubElts =
                                CollectSubElements(iterator, level + 1 );

            // First pass to determine if we have non-linear pieces.
            bool bHasCurves = false;
            bool bIsClosed = false;
            bool bIsContiguous = IsContiguous(oVectorSubElts, bHasCurves, bIsClosed );

            if( bIsContiguous && bHasCurves )
            {
                OGRCompoundCurve* poCC = new OGRCompoundCurve();

                // Second pass to aggregate the geometries.
                for( size_t i = 0; i < oVectorSubElts.size(); i++ )
                {
                    OGRGeometry* poGeom =
                            oVectorSubElts[i].first->GetGeometryRef();
                    if( poGeom != nullptr )
                    {
                        OGRwkbGeometryType eType =
                                        wkbFlatten(poGeom->getGeometryType());
                        if( eType == wkbCircularString || eType == wkbLineString )
                        {
                            poCC->addCurve( poGeom->toCurve(),
                                            CONTIGUITY_TOLERANCE );
                        }
                    }
                    delete oVectorSubElts[i].first;
                }

                poFeature->SetGeometryDirectly( poCC );
            }
            else
            {
                OGRMultiCurve* poMC;
                if( bHasCurves )
                    poMC = new OGRMultiCurve();
                else
                    poMC = new OGRMultiLineString();

                // Second pass to aggregate the geometries.
                for( size_t i = 0; i < oVectorSubElts.size(); i++ )
                {
                    OGRGeometry* poGeom =
                                    oVectorSubElts[i].first->GetGeometryRef();
                    if( poGeom != nullptr )
                    {
                        OGRwkbGeometryType eType =
                                        wkbFlatten(poGeom->getGeometryType());
                        if( eType == wkbCircularString || eType == wkbLineString )
                        {
                            poMC->addGeometry( poGeom );
                        }
                    }
                    delete oVectorSubElts[i].first;
                }

                poFeature->SetGeometryDirectly( poMC );
            }
            poFeature->SetStyleString( osPen );
        }
    }
    else if( EQUAL(pszEntityClassName, "OdDgComplexShape") )
    {
        OdDgComplexCurvePtr complex = OdDgComplexCurve::cast( element );
        CPLAssert( !complex.isNull() );

        OdDgComplexShapePtr complexShape = OdDgComplexShape::cast( element );
        CPLAssert( !complexShape.isNull() );
        bHoleFlag = complexShape->getHoleFlag();

        OdDgElementIteratorPtr iterator = complex->createIterator();
        if( !iterator.isNull() )
        {
            std::vector<tPairFeatureHoleFlag> oVectorSubElts =
                                CollectSubElements(iterator, level + 1 );

            // First pass to determine if we have non-linear pieces.
            bool bHasCurves = false;
            bool bIsClosed = false;
            bool bIsContiguous = IsContiguous(oVectorSubElts, bHasCurves, bIsClosed );

            if( bIsContiguous && bIsClosed )
            {
                OGRCurvePolygon* poCP;
                OGRCompoundCurve* poCC = nullptr;
                OGRLinearRing* poLR = nullptr;

                if( bHasCurves )
                {
                    poCP = new OGRCurvePolygon();
                    poCC = new OGRCompoundCurve();
                }
                else
                {
                    poCP = new OGRPolygon();
                    poLR = new OGRLinearRing();
                }

                // Second pass to aggregate the geometries.
                for( size_t i = 0; i < oVectorSubElts.size(); i++ )
                {
                    OGRGeometry* poGeom =
                            oVectorSubElts[i].first->GetGeometryRef();
                    if( poGeom != nullptr )
                    {
                        OGRwkbGeometryType eType =
                                    wkbFlatten(poGeom->getGeometryType());
                        if( poCC != nullptr )
                        {
                            poCC->addCurve( poGeom->toCurve(),
                                            CONTIGUITY_TOLERANCE );
                        }
                        else if( eType == wkbLineString )
                        {
                            poLR->addSubLineString(poGeom->toLineString(),
                                poLR->getNumPoints() == 0 ? 0 : 1 );
                        }
                    }
                    delete oVectorSubElts[i].first;
                }

                poCP->addRingDirectly( ( bHasCurves ) ?
                    poCC->toCurve() :
                    poLR->toCurve() );

                poFeature->SetGeometryDirectly( poCP );
            }
            else
            {
                OGRGeometryCollection oGC;
                for( size_t i = 0; i < oVectorSubElts.size(); i++ )
                {
                    OGRGeometry* poGeom =
                                    oVectorSubElts[i].first->StealGeometry();
                    if( poGeom != nullptr )
                    {
                        OGRwkbGeometryType eType =
                                        wkbFlatten(poGeom->getGeometryType());
                        if( eType == wkbCircularString )
                        {
                            oGC.addGeometryDirectly(
                                OGRGeometryFactory::forceToLineString(poGeom) );
                        }
                        else if( eType == wkbLineString )
                        {
                            oGC.addGeometryDirectly( poGeom );
                        }
                        else
                        {
                            delete poGeom;
                        }
                    }
                    delete oVectorSubElts[i].first;
                }

                // Try to assemble into polygon geometry.
                OGRGeometry* poGeom = reinterpret_cast<OGRGeometry *>(
                    OGRBuildPolygonFromEdges(
                      reinterpret_cast<OGRGeometryH>( &oGC ),
                      TRUE, TRUE, CONTIGUITY_TOLERANCE, nullptr ) );
                poGeom->setCoordinateDimension( oGC.getCoordinateDimension() );
                poFeature->SetGeometryDirectly( poGeom );

            }
            poFeature->SetStyleString( ConsiderBrush( element, osPen) );
        }
    }
    else if( EQUAL(pszEntityClassName, "OdDgSharedCellReference") )
    {
        OdDgSharedCellReferencePtr ref =
                            OdDgSharedCellReference::cast( element );
        CPLAssert( !ref.isNull() );
        OdGePoint3d point = ref->getOrigin();
        poFeature->SetField( "Text",
                    ToUTF8(ref->getDefinitionName()).c_str() );
        poFeature->SetGeometryDirectly(
                new OGRPoint(point.x, point.y, point.z) );
    }
    else
    {
        if( m_aoSetIgnoredFeatureClasses.find(pszEntityClassName) ==
                                        m_aoSetIgnoredFeatureClasses.end() )
        {
            m_aoSetIgnoredFeatureClasses.insert(pszEntityClassName);
            CPLDebug("DGNV8", "Unhandled class %s for, at least, "
                     "feature " CPL_FRMT_GIB,
                     pszEntityClassName, poFeature->GetFID());
        }
    }

    if( poFeature != nullptr )
        oVector.push_back( tPairFeatureHoleFlag(poFeature, bHoleFlag) );

    return oVector;
}

/************************************************************************/
/*                    GetNextUnfilteredFeature()                        */
/************************************************************************/

OGRFeature *OGRDGNV8Layer::GetNextUnfilteredFeature()

{
    while( true )
    {
        if( m_nIdxInPendingFeatures < m_aoPendingFeatures.size() )
        {
            OGRFeature* poFeature =
                m_aoPendingFeatures[m_nIdxInPendingFeatures].first;
            m_aoPendingFeatures[m_nIdxInPendingFeatures].first = nullptr;
            m_nIdxInPendingFeatures ++;
            return poFeature;
        }

        if( m_pIterator.isNull() )
            return nullptr;

        while( true )
        {
            if( m_pIterator->done() )
                return nullptr;
            OdRxObjectPtr object = m_pIterator->item().openObject();
            m_pIterator->step();
            OdDgGraphicsElementPtr element =
                            OdDgGraphicsElement::cast( object );
            if (element.isNull())
                continue;
            
            m_aoPendingFeatures = ProcessElement(element);
            m_nIdxInPendingFeatures = 0;

            break;
        }
    }
}

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature *OGRDGNV8Layer::GetNextFeature()

{
    while( true )
    {
        OGRFeature* poFeature = GetNextUnfilteredFeature();
        if( poFeature == nullptr )
            break;
        if( poFeature->GetGeometryRef() == nullptr )
        {
            delete poFeature;
            continue;
        }

        if( (m_poAttrQuery == nullptr
             || m_poAttrQuery->Evaluate( poFeature ))
            && FilterGeometry( poFeature->GetGeometryRef() ) )
            return poFeature;

        delete poFeature;
    }
    return nullptr;
}

/************************************************************************/
/*                        GetFeatureInternal()                          */
/************************************************************************/

OdDgGraphicsElementPtr OGRDGNV8Layer::GetFeatureInternal(GIntBig nFID,
                                                  OdDg::OpenMode openMode)
{
    if( nFID < 0 )
        return OdDgGraphicsElementPtr();
    const OdDbHandle handle( static_cast<OdUInt64>(nFID) );
    const OdDgElementId id = m_pModel->database()->getElementId(handle);
    OdRxObjectPtr object = id.openObject(openMode);
    OdDgGraphicsElementPtr element = OdDgGraphicsElement::cast( object );
    if (element.isNull() || element->ownerId() != m_pModel->elementId() )
        return OdDgGraphicsElementPtr();
    return element;
}

/************************************************************************/
/*                            GetFeature()                              */
/************************************************************************/

OGRFeature *OGRDGNV8Layer::GetFeature(GIntBig nFID)
{
    OdDgGraphicsElementPtr element = GetFeatureInternal(nFID, OdDg::kForRead);
    if( element.isNull() )
        return nullptr;
    std::vector<tPairFeatureHoleFlag> oVector = ProcessElement(element);
    // Only return a feature if and only if we have a single element
    if( oVector.empty() )
        return nullptr;
    if( oVector.size() > 1 )
    {
        for( size_t i = 0; i < oVector.size(); i++ )
            delete oVector[i].first;
        return nullptr;
    }
    return oVector[0].first;
}

/************************************************************************/
/*                          DeleteFeature()                             */
/************************************************************************/

OGRErr OGRDGNV8Layer::DeleteFeature(GIntBig nFID)
{
    if( !m_poDS->GetUpdate() )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Attempt to delete feature on read-only DGN file." );
        return OGRERR_FAILURE;
    }

    OdDgGraphicsElementPtr element = GetFeatureInternal(nFID,
                                                        OdDg::kForWrite);
    if( element.isNull() )
        return OGRERR_FAILURE;
    try
    {
        element->erase(true);
    }
    catch (const OdError& e)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Teigha DGN error occurred: %s",
                 ToUTF8(e.description()).c_str());
        return OGRERR_FAILURE;
    }
    catch (const std::exception &exc)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "std::exception occurred: %s", exc.what());
        return OGRERR_FAILURE;
    }
    catch (...)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Unknown exception occurred");
        return OGRERR_FAILURE;
    }
    m_poDS->SetModified();
    return OGRERR_NONE;
}


/************************************************************************/
/*                             GetExtent()                              */
/************************************************************************/

OGRErr OGRDGNV8Layer::GetExtent( OGREnvelope *psExtent, int bForce )
{
    OdDgModel::StorageUnitDescription description;
    m_pModel->getStorageUnit( description );
    OdDgElementIteratorPtr iterator =
                        m_pModel->createGraphicsElementsIterator();
    bool bValid = false;
    while( true )
    {
        if( iterator.isNull() || iterator->done() )
            break;
        OdRxObjectPtr object = iterator->item().openObject();
        iterator->step();
        OdDgGraphicsElementPtr element = OdDgGraphicsElement::cast( object );
        if (element.isNull())
            continue;
        OdDgGraphicsElementPEPtr pElementPE =
            OdDgGraphicsElementPEPtr(OdRxObjectPtr(element));
        if( pElementPE.isNull() )
            continue;
        OdGeExtents3d savedExtent;
        if( pElementPE->getRange( element, savedExtent ) == eOk )
        {
            OdGePoint3d min = savedExtent.minPoint();
            OdGePoint3d max = savedExtent.maxPoint();
            if( !bValid )
            {
                psExtent->MinX = min.x / description.m_uorPerStorageUnit;
                psExtent->MinY = min.y / description.m_uorPerStorageUnit;
                psExtent->MaxX = max.x / description.m_uorPerStorageUnit;
                psExtent->MaxY = max.y / description.m_uorPerStorageUnit;
                bValid = true;
            }
            else
            {
                psExtent->MinX = std::min(psExtent->MinX,
                    min.x / description.m_uorPerStorageUnit);
                psExtent->MinY = std::min(psExtent->MinY,
                    min.y / description.m_uorPerStorageUnit);
                psExtent->MaxX = std::max(psExtent->MaxX,
                    max.x / description.m_uorPerStorageUnit);
                psExtent->MaxY = std::max(psExtent->MaxY,
                    max.y / description.m_uorPerStorageUnit);
            }
        }
    }
    if( bValid )
        return OGRERR_NONE;
    return OGRLayer::GetExtent(psExtent, bForce);
}


/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRDGNV8Layer::TestCapability( const char * pszCap )

{
    if( EQUAL(pszCap,OLCRandomRead) )
        return TRUE;
    else if( EQUAL(pszCap,OLCStringsAsUTF8) )
        return TRUE;
    else if( EQUAL(pszCap,OLCSequentialWrite) ||
             EQUAL(pszCap,OLCDeleteFeature) )
        return m_poDS->GetUpdate();
    else if( EQUAL(pszCap,OLCCurveGeometries) )
        return TRUE;

    return FALSE;
}


/************************************************************************/
/*                           ICreateFeature()                            */
/*                                                                      */
/*      Create a new feature and write to file.                         */
/************************************************************************/

OGRErr OGRDGNV8Layer::ICreateFeature( OGRFeature *poFeature )

{
    if( !m_poDS->GetUpdate() )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Attempt to create feature on read-only DGN file." );
        return OGRERR_FAILURE;
    }

    if( poFeature->GetGeometryRef() == nullptr )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Features with empty, geometry collection geometries not "
                  "supported in DGN format." );
        return OGRERR_FAILURE;
    }

    try
    {
        OdDgGraphicsElementPtr element = CreateGraphicsElement(
                                poFeature, poFeature->GetGeometryRef() );
        if( element.isNull() )
            return OGRERR_FAILURE;
        m_pModel->addElement(element);
        poFeature->SetFID(
            static_cast<GIntBig>(
                static_cast<OdUInt64>(element->elementId().getHandle()) ) );
    }
    catch (const OdError& e)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Teigha DGN error occurred: %s",
                 ToUTF8(e.description()).c_str());
        return OGRERR_FAILURE;
    }
    catch (const std::exception &exc)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "std::exception occurred: %s", exc.what());
        return OGRERR_FAILURE;
    }
    catch (...)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Unknown exception occurred");
        return OGRERR_FAILURE;
    }

    m_poDS->SetModified();
    return OGRERR_NONE;
}

/************************************************************************/
/*                              GetTool()                               */
/************************************************************************/

static OGRStyleTool* GetTool( OGRFeature* poFeature, OGRSTClassId eClassId )
{

    OGRStyleMgr oMgr;
    oMgr.InitFromFeature( poFeature );
    for( int i=0; i<oMgr.GetPartCount(); i++)
    {
        OGRStyleTool* poTool = oMgr.GetPart( i );
        if( poTool == nullptr || poTool->GetType() != eClassId)
        {
            delete poTool;
        }
        else
        {
            return poTool;
        }
    }
    return nullptr;
}

/************************************************************************/
/*                           TranslateLabel()                           */
/************************************************************************/

OdDgGraphicsElementPtr OGRDGNV8Layer::TranslateLabel(
                                    OGRFeature *poFeature, OGRPoint *poPoint )

{
    const char *pszText = poFeature->GetFieldAsString( "Text" );

    OGRStyleLabel *poLabel = static_cast<OGRStyleLabel*>(
                                            GetTool(poFeature, OGRSTCLabel));

    OdDgText2dPtr text = OdDgText2d::createObject();
    OdGePoint2d point;
    point.x = poPoint->getX();
    point.y = poPoint->getY();
    text->setOrigin(point);

    double dfHeightMultiplier = 1.0;
    if( poLabel != nullptr )
    {
        GBool bDefault;

        if( poLabel->TextString(bDefault) != nullptr && !bDefault )
            pszText = poLabel->TextString(bDefault);

        const double dfRotation = poLabel->Angle(bDefault);
        text->setRotation(dfRotation * DEG_TO_RAD);

        poLabel->SetUnit(OGRSTUMM);
        double dfVal = poLabel->Size( bDefault );
        if( !bDefault  )
            dfHeightMultiplier = dfVal/1000.0;

        /* get font id */
        const char *pszFontName = poLabel->FontName( bDefault );
        if( !bDefault && pszFontName != nullptr )
        {
            OdDgFontTablePtr pFontTable =
                m_pModel->database()->getFontTable(OdDg::kForRead);
            OdDgElementId idFont = pFontTable->getAt( 
                            OGRDGNV8DataSource::FromUTF8(pszFontName) );
            if( !idFont.isNull() )
            {
                OdDgFontTableRecordPtr pFont =
                    idFont.openObject(OdDg::kForRead);
                OdUInt32 uFontEntryId = pFont->getNumber();
                text->setFontEntryId(uFontEntryId);
            }
        }

        int nAnchor = poLabel->Anchor(bDefault);
        if( !bDefault )
            text->setJustification( GetAnchorPositionFromOGR( nAnchor ) );
    }

    text->setHeightMultiplier( dfHeightMultiplier );
    text->setLengthMultiplier( text->getHeightMultiplier() ); // FIXME ??
    text->setText( OGRDGNV8DataSource::FromUTF8(pszText) );

    if( poLabel )
        delete poLabel;
    return text;
}

/************************************************************************/
/*                       GetColorFromString()                           */
/************************************************************************/

int OGRDGNV8Layer::GetColorFromString(const char* pszColor)
{
    unsigned int nRed = 0;
    unsigned int nGreen = 0;
    unsigned int nBlue = 0;
    const int nCount =
        sscanf(pszColor, "#%2x%2x%2x", &nRed, &nGreen, &nBlue);
    if( nCount == 3 )
    {
        OdUInt32 nIdx = OdDgColorTable::getColorIndexByRGB(
            m_poDS->GetDb(), ODRGB( nRed, nGreen ,nBlue ) );
        return static_cast<int>(nIdx);
    }
    else
    {
        return -1;
    }
}

/************************************************************************/
/*                       AttachFillLinkage()                            */
/************************************************************************/

void OGRDGNV8Layer::AttachFillLinkage( OGRFeature* poFeature,
                                       OdDgGraphicsElementPtr element )
{
    const char* pszStyle = poFeature->GetStyleString();
    if( pszStyle != nullptr && strstr(pszStyle, "BRUSH") != nullptr )
    {
        OGRStyleBrush* poBrush = static_cast<OGRStyleBrush*>(
                                            GetTool(poFeature, OGRSTCBrush));
        if( poBrush != nullptr )
        {
            GBool bDefault;
            const char* pszColor = poBrush->ForeColor(bDefault);
            if( pszColor && !bDefault )
            {
                const int nIdx = GetColorFromString(pszColor);
                if( nIdx >= 0 )
                {
                    OdDgFillColorLinkagePtr fillColor =
                            OdDgFillColorLinkage::createObject();
                    fillColor->setColorIndex( nIdx );
                    element->addLinkage( fillColor->getPrimaryId(),
                                         fillColor.get() );
                }
            }
                
            delete poBrush;
        }
    }
}

/************************************************************************/
/*                       AttachCommonAttributes()                       */
/************************************************************************/

void OGRDGNV8Layer::AttachCommonAttributes( OGRFeature *poFeature,
                                            OdDgGraphicsElementPtr element )
{
    const int nLevel = poFeature->GetFieldAsInteger( "Level" );
    const int nGraphicGroup = poFeature->GetFieldAsInteger( "GraphicGroup" );
    const int nWeight = poFeature->GetFieldAsInteger( "Weight" );
    const int nStyle = poFeature->GetFieldAsInteger( "Style" );
    
    element->setLevelEntryId(nLevel);
    element->setGraphicsGroupEntryId(nGraphicGroup);
    
    const int nColorIndexField = poFeature->GetFieldIndex("ColorIndex");
    if( poFeature->IsFieldSetAndNotNull(nColorIndexField) )
    {
        const int nColor = poFeature->GetFieldAsInteger(nColorIndexField);
        element->setColorIndex(nColor);
    }
    else
    {
        const char* pszStyle = poFeature->GetStyleString();
        if( pszStyle != nullptr && strstr(pszStyle, "PEN") != nullptr )
        {
            OGRStylePen* poPen = static_cast<OGRStylePen*>(
                                            GetTool(poFeature, OGRSTCPen));
            if( poPen != nullptr )
            {
                GBool bDefault;
                const char* pszColor = poPen->Color(bDefault);
                if( pszColor && !bDefault )
                {
                    const int nIdx = GetColorFromString(pszColor);
                    if( nIdx >= 0 )
                    {
                        element->setColorIndex(nIdx);
                    }
                }
                delete poPen;
            }
        }
        else if( pszStyle != nullptr && strstr(pszStyle, "LABEL") != nullptr )
        {
            OGRStyleLabel *poLabel = static_cast<OGRStyleLabel*>(
                                            GetTool(poFeature, OGRSTCLabel));
            if( poLabel != nullptr )
            {
                GBool bDefault;
                const char* pszColor = poLabel->ForeColor(bDefault);
                if( pszColor && !bDefault )
                {
                    const int nIdx = GetColorFromString(pszColor);
                    if( nIdx >= 0 )
                    {
                        element->setColorIndex(nIdx);
                    }
                }

                delete poLabel;
            }
        }
    }

    element->setLineStyleEntryId(nStyle);
    element->setLineWeight(nWeight);
}

/************************************************************************/
/*                          AddToComplexCurve()                         */
/************************************************************************/

void OGRDGNV8Layer::AddToComplexCurve( OGRFeature* poFeature,
                                       OGRCircularString* poCS,
                                       OdDgComplexCurvePtr complexCurve )
{
    for( int i = 0; i + 2 < poCS->getNumPoints(); i+= 2 )
    {
        double R, cx, cy;
        double alpha0, alpha1, alpha2;
        if( OGRGeometryFactory::GetCurveParmeters(
                poCS->getX(i),
                poCS->getY(i),
                poCS->getX(i+1),
                poCS->getY(i+1),
                poCS->getX(i+2),
                poCS->getY(i+2),
                R, cx, cy, alpha0, alpha1, alpha2) )
        {
            OdDgArc2dPtr arc = OdDgArc2d::createObject();
            arc->setPrimaryAxis(R);
            arc->setSecondaryAxis(R);
            OdGePoint2d point;
            point.x = cx;
            point.y = cy;
            arc->setOrigin(point);
            arc->setStartAngle(alpha0); // already in radians
            arc->setSweepAngle(alpha2 - alpha0);
            AttachCommonAttributes(poFeature, arc);
            complexCurve->add(arc);
        }
    }
}

/************************************************************************/
/*                         AddToComplexCurve()                          */
/************************************************************************/

void OGRDGNV8Layer::AddToComplexCurve( OGRFeature* poFeature,
                                       OGRCompoundCurve* poCC,
                                       OdDgComplexCurvePtr complexCurve )
{
    for( int iCurve = 0; iCurve < poCC->getNumCurves(); ++iCurve )
    {
        OGRCurve* poCurve = poCC->getCurve(iCurve);
        OGRwkbGeometryType eType = wkbFlatten(poCurve->getGeometryType());
        if( eType == wkbLineString || OGR_GT_HasZ(eType) )
        {
            complexCurve->add( CreateGraphicsElement(poFeature, poCurve) );
        }
        else if( eType == wkbCircularString )
        {
            OGRCircularString* poCS = poCurve->toCircularString();
            AddToComplexCurve(poFeature, poCS, complexCurve);
        }
        else
        {
            CPLAssert(false);
        }
    }
}

/************************************************************************/
/*                        CreateShapeFromLS()                           */
/************************************************************************/

static
OdDgGraphicsElementPtr CreateShapeFromLS( OGRLineString* poLS,
                                          bool bHbit = false )
{
    if( OGR_GT_HasZ(poLS->getGeometryType()) )
    {
        OdDgShape3dPtr shape = OdDgShape3d::createObject();
        for( int i = 0; i < poLS->getNumPoints(); i++ )
        {
            OGRPoint ogrPoint;
            OdGePoint3d point;
            poLS->getPoint(i, &ogrPoint);
            point.x = ogrPoint.getX();
            point.y = ogrPoint.getY();
            point.z = ogrPoint.getZ();
            shape->addVertex(point);
        }
        shape->setHbitFlag(bHbit);
        return shape;
    }
    else
    {
        OdDgShape2dPtr shape = OdDgShape2d::createObject();
        for( int i = 0; i < poLS->getNumPoints(); i++ )
        {
            OGRPoint ogrPoint;
            OdGePoint2d point;
            poLS->getPoint(i, &ogrPoint);
            point.x = ogrPoint.getX();
            point.y = ogrPoint.getY();
            shape->addVertex(point);
        }
        shape->setHbitFlag(bHbit);
        return shape;
    }
}

/************************************************************************/
/*                           CreateShape()                              */
/************************************************************************/

OdDgGraphicsElementPtr OGRDGNV8Layer::CreateShape( OGRFeature* poFeature,
                                                   OGRCurve* poCurve,
                                                   bool bIsHole )
{
    OdDgGraphicsElementPtr element;
    OGRwkbGeometryType eType = wkbFlatten(poCurve->getGeometryType());
    if( eType == wkbLineString )
    {
        OGRLineString* poLS = poCurve->toLineString();
        element = CreateShapeFromLS(poLS, bIsHole);
    }
    else if( eType == wkbCircularString )
    {
        OdDgComplexShapePtr complexShape = OdDgComplexShape::createObject();
        complexShape->setHbitFlag(bIsHole);
        OGRCircularString* poCS = poCurve->toCircularString();
        AddToComplexCurve(poFeature, poCS, complexShape);
        element = complexShape;
    }
    else if( eType == wkbCompoundCurve )
    {
        OdDgComplexShapePtr complexShape = OdDgComplexShape::createObject();
        complexShape->setHbitFlag(bIsHole);
        OGRCompoundCurve* poCC = poCurve->toCompoundCurve();
        AddToComplexCurve( poFeature, poCC, complexShape );
        element = complexShape;
    }
    
    if( !bIsHole )
        AttachFillLinkage( poFeature, element );

    return element;
}

/************************************************************************/
/*                           IsFullCircle()                             */
/************************************************************************/

static bool IsFullCircle( OGRCircularString* poCS,
                          double& cx, double& cy,
                          double& R )
{
    if( poCS->getNumPoints() == 3 && poCS->get_IsClosed() )
    {
        const double x0 = poCS->getX(0);
        const double y0 = poCS->getY(0);
        const double x1 = poCS->getX(1);
        const double y1 = poCS->getY(1);
        cx = (x0 + x1) / 2;
        cy = (y0 + y1) / 2;
        R = sqrt((x1 - cx) * (x1 - cx) + (y1 - cy) * (y1 - cy));
        return true;
    }
    // Full circle defined by 2 arcs?
    else if( poCS->getNumPoints() == 5 && poCS->get_IsClosed() )
    {
        double R_1 = 0.0;
        double cx_1 = 0.0;
        double cy_1 = 0.0;
        double alpha0_1 = 0.0;
        double alpha1_1 = 0.0;
        double alpha2_1 = 0.0;
        double R_2 = 0.0;
        double cx_2 = 0.0;
        double cy_2 = 0.0;
        double alpha0_2 = 0.0;
        double alpha1_2 = 0.0;
        double alpha2_2 = 0.0;
        if( OGRGeometryFactory::GetCurveParmeters(
                poCS->getX(0), poCS->getY(0),
                poCS->getX(1), poCS->getY(1),
                poCS->getX(2), poCS->getY(2),
                R_1, cx_1, cy_1, alpha0_1, alpha1_1, alpha2_1) &&
            OGRGeometryFactory::GetCurveParmeters(
                poCS->getX(2), poCS->getY(2),
                poCS->getX(3), poCS->getY(3),
                poCS->getX(4), poCS->getY(4),
                R_2, cx_2, cy_2, alpha0_2, alpha1_2, alpha2_2) &&
            AlmostEqual(R_1,R_2) &&
            AlmostEqual(cx_1,cx_2) &&
            AlmostEqual(cy_1,cy_2) &&
            (alpha2_1 - alpha0_1) * (alpha2_2 - alpha0_2) > 0 )
        {
            cx = cx_1;
            cy = cy_1;
            R = R_1;
            return true;
        }
    }
    return false;
}

/************************************************************************/
/*                       CreateGraphicsElement()                        */
/*                                                                      */
/*      Create an element or element group from a given geometry and    */
/*      the given feature.  This method recurses to handle              */
/*      collections as essentially independent features.                */
/************************************************************************/

OdDgGraphicsElementPtr OGRDGNV8Layer::CreateGraphicsElement(
                                             OGRFeature *poFeature,
                                             OGRGeometry *poGeom)

{
    const OGRwkbGeometryType eType = poGeom->getGeometryType();
    const OGRwkbGeometryType eFType = wkbFlatten(eType);

    const int nType = poFeature->GetFieldAsInteger( "Type" );

    OdDgGraphicsElementPtr element;

    if( eFType == wkbPoint )
    {
        OGRPoint *poPoint = poGeom->toPoint();
        const char *pszText = poFeature->GetFieldAsString("Text");
        const char *pszStyle = poFeature->GetStyleString();

        if( (pszText == nullptr || pszText[0] == 0)
            && (pszStyle == nullptr || strstr(pszStyle,"LABEL") == nullptr) )
        {
            if( OGR_GT_HasZ(eType) )
            {
                OdDgLine3dPtr line = OdDgLine3d::createObject();
                element = line;
                OdGePoint3d point;
                point.x = poPoint->getX();
                point.y = poPoint->getY();
                point.z = poPoint->getZ();
                line->setStartPoint(point);
                line->setEndPoint(point);
            }
            else
            {
                OdDgLine2dPtr line = OdDgLine2d::createObject();
                element = line;
                OdGePoint2d point;
                point.x = poPoint->getX();
                point.y = poPoint->getY();
                line->setStartPoint(point);
                line->setEndPoint(point);
            }
        }
        else
        {
            element = TranslateLabel( poFeature, poPoint );
        }
    }
    else if( eFType == wkbLineString )
    {
        OGRLineString* poLS = poGeom->toLineString();
        if( poLS->getNumPoints() == 2 &&
            (nType == 0 || nType == OdDgElement::kTypeLine) )
        {
            if( OGR_GT_HasZ(eType) )
            {
                OdDgLine3dPtr line = OdDgLine3d::createObject();
                element = line;
                OGRPoint ogrPoint;
                OdGePoint3d point;
                poLS->getPoint(0, &ogrPoint);
                point.x = ogrPoint.getX();
                point.y = ogrPoint.getY();
                point.z = ogrPoint.getZ();
                line->setStartPoint(point);
                poLS->getPoint(1, &ogrPoint);
                point.x = ogrPoint.getX();
                point.y = ogrPoint.getY();
                point.z = ogrPoint.getZ();
                line->setEndPoint(point);
            }
            else
            {
                OdDgLine2dPtr line = OdDgLine2d::createObject();
                element = line;
                OGRPoint ogrPoint;
                OdGePoint2d point;
                poLS->getPoint(0, &ogrPoint);
                point.x = ogrPoint.getX();
                point.y = ogrPoint.getY();
                line->setStartPoint(point);
                poLS->getPoint(1, &ogrPoint);
                point.x = ogrPoint.getX();
                point.y = ogrPoint.getY();
                line->setEndPoint(point);
            }
        }
        else
        {
            if( OGR_GT_HasZ(eType) )
            {
                OdDgLineString3dPtr line = OdDgLineString3d::createObject();
                element = line;
                for( int i = 0; i < poLS->getNumPoints(); i++ )
                {
                    OGRPoint ogrPoint;
                    OdGePoint3d point;
                    poLS->getPoint(i, &ogrPoint);
                    point.x = ogrPoint.getX();
                    point.y = ogrPoint.getY();
                    point.z = ogrPoint.getZ();
                    line->addVertex(point);
                }
            }
            else
            {
                OdDgLineString2dPtr line = OdDgLineString2d::createObject();
                element = line;
                for( int i = 0; i < poLS->getNumPoints(); i++ )
                {
                    OGRPoint ogrPoint;
                    OdGePoint2d point;
                    poLS->getPoint(i, &ogrPoint);
                    point.x = ogrPoint.getX();
                    point.y = ogrPoint.getY();
                    line->addVertex(point);
                }
            }
        }
    }
    else if( eFType == wkbCircularString )
    {
        OGRCircularString* poCS = poGeom->toCircularString();
        double R, cx, cy;
        if( IsFullCircle(poCS, cx, cy, R) && !OGR_GT_HasZ(eType) )
        {
            OdDgEllipse2dPtr ellipse = OdDgEllipse2d::createObject();
            element = ellipse;
            ellipse->setPrimaryAxis(R);
            ellipse->setSecondaryAxis(R);
            OdGePoint2d point;
            point.x = cx;
            point.y = cy;
            ellipse->setOrigin(point);
        }
        else if( poCS->getNumPoints() == 3 && !OGR_GT_HasZ(eType) )
        {
            double alpha0, alpha1, alpha2;
            if( OGRGeometryFactory::GetCurveParmeters(
                    poCS->getX(0),
                    poCS->getY(0),
                    poCS->getX(1),
                    poCS->getY(1),
                    poCS->getX(2),
                    poCS->getY(2),
                    R, cx, cy, alpha0, alpha1, alpha2) )
            {
                OdDgArc2dPtr arc = OdDgArc2d::createObject();
                element = arc;
                arc->setPrimaryAxis(R);
                arc->setSecondaryAxis(R);
                OdGePoint2d point;
                point.x = cx;
                point.y = cy;
                arc->setOrigin(point);
                arc->setStartAngle(alpha0); // already in radians
                arc->setSweepAngle(alpha2 - alpha0);
            }
        }
        else if( !OGR_GT_HasZ(eType) )
        {
            OdDgComplexCurvePtr complexCurve =
                            OdDgComplexString::createObject();
            element = complexCurve;
            AddToComplexCurve(poFeature, poCS, complexCurve);
        }

        if( element.isNull() )
        {
            OGRGeometry* poLS =
                OGRGeometryFactory::forceToLineString( poGeom->clone() );
            element = CreateGraphicsElement(poFeature, poLS);
            delete poLS;
            return element;
        }
    }
    else if( eFType == wkbCompoundCurve )
    {
        OGRCompoundCurve* poCC = poGeom->toCompoundCurve();
        OdDgComplexCurvePtr complexCurve = OdDgComplexString::createObject();
        element = complexCurve;
        AddToComplexCurve( poFeature, poCC, complexCurve );
    }
    else if( eFType == wkbCurvePolygon || eFType == wkbPolygon )
    {
        OGRCurvePolygon* poPoly = poGeom->toCurvePolygon();
        if( poPoly->getNumInteriorRings() == 0 &&
            poPoly->getExteriorRingCurve() == nullptr )
        {
            if( OGR_GT_HasZ(eType) )
            {
                element = OdDgShape3d::createObject();
            }
            else
            {
                element = OdDgShape2d::createObject();
            }
        }
        else if( poPoly->getNumInteriorRings() == 0 )
        {
            element = CreateShape(poFeature, poPoly->getExteriorRingCurve());
        }
        else
        {
            if( OGR_GT_HasZ(eType) )
            {
                OdDgCellHeader3dPtr pCell = OdDgCellHeader3d::createObject(); 
                element = pCell;
                for( int iRing = -1;
                         iRing < poPoly->getNumInteriorRings(); iRing++ )
                {
                    OGRCurve* poCurve = (iRing < 0 ) ?
                            poPoly->getExteriorRingCurve() :
                           poPoly->getInteriorRingCurve(iRing);

                    OdDgGraphicsElementPtr shape = CreateShape(
                        poFeature, poCurve, iRing >= 0);
                    AttachCommonAttributes(poFeature, shape);
                    pCell->add(shape);
                }
            }
            else
            {
                OdDgCellHeader2dPtr pCell = OdDgCellHeader2d::createObject(); 
                element = pCell;
                for( int iRing = -1;
                         iRing < poPoly->getNumInteriorRings(); iRing++ )
                {
                    OGRCurve* poCurve = (iRing < 0 ) ?
                            poPoly->getExteriorRingCurve() :
                           poPoly->getInteriorRingCurve(iRing);

                    OdDgGraphicsElementPtr shape = CreateShape(
                        poFeature, poCurve, iRing >= 0);
                    AttachCommonAttributes(poFeature, shape);
                    pCell->add(shape);
                }
            }
            AttachFillLinkage( poFeature, element );
        }
    }
    else if( OGR_GT_IsSubClassOf( eFType, wkbGeometryCollection ) )
    {
        OGRGeometryCollection* poGC = poGeom->toGeometryCollection();
        OdDgCellHeader2dPtr pCell = OdDgCellHeader2d::createObject(); 
        element = pCell;
        if( !pCell.isNull() )
        {
            for( auto&& poMember: poGC )
            {
                pCell->add(CreateGraphicsElement( poFeature, poMember ));
            }
        }
    }
    else
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Unsupported geometry type (%s) for DGN.",
                  OGRGeometryTypeToName( eType ) );
    }

    if( !element.isNull() )
        AttachCommonAttributes(poFeature, element);

    return element;
}
