/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Generate a test .dgn file
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

#include "createdgnv8testfile_headers.h"

const double DEG_TO_RAD = 3.141592653589793 / 180.0;

/************************************************************************/
/*                         OGRDGNV8Services                             */
/*                                                                      */
/*      Services implementation for OGR.  Eventually we should          */
/*      override the OdExDgnSystemServices IO to use VSI*L.             */
/************************************************************************/

class OGRDGNV8Services : public OdExDgnSystemServices,
                         public OdExDgnHostAppServices
{
protected:
  ODRX_USING_HEAP_OPERATORS(OdExDgnSystemServices);
};

static OdStaticRxObject<OGRDGNV8Services> oServices;

int main()
{
    odrxInitialize(&oServices);
    oServices.disableProgressMeterOutput( true );
    ::odrxDynamicLinker()->loadModule(L"TG_Db", false);

    OdDgDatabasePtr pDb = oServices.createDatabase();

    OdDgModelPtr pModel = pDb->getActiveModelId().openObject(OdDg::kForWrite);
    pModel->setWorkingUnit( OdDgModel::kWuMasterUnit );
    pModel->setName( "my_model" );
    pModel->setDescription( "my_description" );

    // Add font
    OdDgFontTablePtr pFontTable = pModel->database()->getFontTable(OdDg::kForWrite);
    {
        OdDgFontTableRecordPtr pFont = OdDgFontTableRecord::createObject();
        pFont->setName("Arial");
        pFont->setType(kFontTypeTrueType);

        pFontTable->add(pFont);
    }

    // Read as point by OGR
    {
        OdDgLine3dPtr line = OdDgLine3d::createObject();
        pModel->addElement(line);
        OdGePoint3d point;  
        point.x = 0;
        point.y = 1;
        point.z = 2;
        line->setStartPoint(point);
        line->setEndPoint(point);

        line->setLevelEntryId(1);
        line->setGraphicsGroupEntryId(2);
        line->setColorIndex(3);
        line->setLineStyleEntryId(4);
        line->setLineWeight(5);
    }
    
    // Read as point by OGR
    {
        OdDgLine2dPtr line = OdDgLine2d::createObject();
        pModel->addElement(line);
        OdGePoint2d point;
        point.x = 0;
        point.y = 1;
        line->setStartPoint(point);
        line->setEndPoint(point);
    }

    {
        OdDgLine3dPtr line = OdDgLine3d::createObject();
        pModel->addElement(line);
        OdGePoint3d point;
        point.x = 0;
        point.y = 1;
        point.z = 2;
        line->setStartPoint(point);
        point.x = 3;
        point.y = 4;
        point.z = 5;
        line->setEndPoint(point);
    }

    {
        OdDgLine2dPtr line = OdDgLine2d::createObject();
        pModel->addElement(line);
        OdGePoint2d point;
        point.x = 0;
        point.y = 1;
        line->setStartPoint(point);
        point.x = 2;
        point.y = 3;
        line->setEndPoint(point);
    }

    {
        OdDgText2dPtr text = OdDgText2d::createObject();
        pModel->addElement(text);
        OdGePoint2d point;
        point.x = 0;
        point.y = 1;
        text->setOrigin(point);
        text->setText(L"myT\u00e9.xt");
        text->setRotation(-45 * DEG_TO_RAD);
        text->setHeightMultiplier( 1.0 );
        text->setLengthMultiplier( 1.0 );
        OdUInt32 nIdx = OdDgColorTable::getColorIndexByRGB(
            text->database(), ODRGB( 255, 200, 150 ) );
        text->setColorIndex(nIdx);

        OdDgElementId idFont = pFontTable->getAt( "Arial" );
        if( !idFont.isNull() )
        {
            OdDgFontTableRecordPtr pFont = idFont.openObject(OdDg::kForRead);
            OdUInt32 uFontEntryId = pFont->getNumber();
            text->setFontEntryId(uFontEntryId);
        }
    }

    {
        OdDgText3dPtr text = OdDgText3d::createObject();
        pModel->addElement(text);
        OdGePoint3d point;
        point.x = 0;
        point.y = 1;
        point.z = 2;
        text->setOrigin(point);
        text->setText("x");
        text->setHeightMultiplier( 1.0 );
        text->setLengthMultiplier( 1.0 );
    }

    {
        OdDgTextNode2dPtr textNode = OdDgTextNode2d::createObject();
        pModel->addElement(textNode);

        OdDgText2dPtr text = OdDgText2d::createObject();
        textNode->add(text);
        OdGePoint2d point;
        point.x = 0;
        point.y = 1;
        text->setOrigin(point);
        text->setText("z");
        text->setHeightMultiplier( 1.0 );
        text->setLengthMultiplier( 1.0 );
    }

    {
        OdDgTextNode3dPtr textNode = OdDgTextNode3d::createObject();
        pModel->addElement(textNode);

        OdDgText3dPtr text = OdDgText3d::createObject();
        textNode->add(text);
        OdGePoint3d point;
        point.x = 0;
        point.y = 1;
        point.z = 2;
        text->setOrigin(point);
        text->setText("z");
        text->setHeightMultiplier( 1.0 );
        text->setLengthMultiplier( 1.0 );
    }

    {
        OdDgLineString3dPtr line = OdDgLineString3d::createObject();
        pModel->addElement(line);
        OdGePoint3d point;
        point.x = 0;
        point.y = 1;
        point.z = 2;
        line->addVertex(point);
        point.x = 3;
        point.y = 4;
        point.z = 5;
        line->addVertex(point);
        point.x = 6;
        point.y = 7;
        point.z = 8;
        line->addVertex(point);
    }

    {
        OdDgLineString2dPtr line = OdDgLineString2d::createObject();
        pModel->addElement(line);
        OdGePoint2d point;
        point.x = 0;
        point.y = 1;
        line->addVertex(point);
        point.x = 3;
        point.y = 4;
        line->addVertex(point);
        point.x = 6;
        point.y = 7;
        line->addVertex(point);
    }

    {
        OdDgPointString2dPtr line = OdDgPointString2d::createObject();
        pModel->addElement(line);
        OdGePoint2d point;
        point.x = 0;
        point.y = 1;
        line->addVertex(point, OdGeMatrix2d::rotation(0));
        point.x = 3;
        point.y = 4;
        line->addVertex(point, OdGeMatrix2d::rotation(0));
    }

    {
        OdDgPointString3dPtr line = OdDgPointString3d::createObject();
        pModel->addElement(line);
        OdGePoint3d point;
        point.x = 0;
        point.y = 1;
        point.z = 2;
        line->addVertex(point, OdGeQuaternion( 1., 0., 0., 0. ));
        point.x = 3;
        point.y = 4;
        line->addVertex(point, OdGeQuaternion( 1., 0., 0., 0. ));
    }

    {
        OdDgMultilinePtr multiline = OdDgMultiline::createObject();
        pModel->addElement(multiline);
        OdDgMultilinePoint multilinePoint;
        OdGePoint3d point;
        point.x = 0;
        point.y = 1;
        point.z = 2;
        multilinePoint.setPoint(point);
        multiline->addPoint( multilinePoint );
    }

    // True ellipse
    {
        OdDgEllipse2dPtr ellipse = OdDgEllipse2d::createObject();
        pModel->addElement(ellipse);
        ellipse->setPrimaryAxis(1);
        ellipse->setSecondaryAxis(2);
        OdGePoint2d point;
        point.x = 0;
        point.y = 1;
        ellipse->setOrigin(point);
    }

    // Circle
    {
        OdDgEllipse2dPtr ellipse = OdDgEllipse2d::createObject();
        pModel->addElement(ellipse);
        ellipse->setPrimaryAxis(1);
        ellipse->setSecondaryAxis(1);
        ellipse->setRotationAngle(45 * DEG_TO_RAD);
        OdGePoint2d point;
        point.x = 0;
        point.y = 1;
        ellipse->setOrigin(point);
    }

    // Circle
    {
        OdDgEllipse3dPtr ellipse = OdDgEllipse3d::createObject();
        pModel->addElement(ellipse);
        ellipse->setPrimaryAxis(1);
        ellipse->setSecondaryAxis(1);
        OdGePoint3d point;
        point.x = 0;
        point.y = 1;
        point.z = 2;
        ellipse->setOrigin(point);
    }

    // True arc
    {
        OdDgArc2dPtr arc = OdDgArc2d::createObject();
        pModel->addElement(arc);
        arc->setPrimaryAxis(1);
        arc->setSecondaryAxis(2);
        OdGePoint2d point;
        point.x = 0;
        point.y = 1;
        arc->setOrigin(point);
        arc->setStartAngle(10 * DEG_TO_RAD);
        arc->setSweepAngle(180 * DEG_TO_RAD);
    }

    // Circle arc
    {
        OdDgArc2dPtr arc = OdDgArc2d::createObject();
        pModel->addElement(arc);
        arc->setPrimaryAxis(1);
        arc->setSecondaryAxis(1);
        arc->setRotationAngle(45 * DEG_TO_RAD);
        OdGePoint2d point;
        point.x = 0;
        point.y = 1;
        arc->setOrigin(point);
        arc->setStartAngle(10 * DEG_TO_RAD);
        arc->setSweepAngle(180 * DEG_TO_RAD);
    }

    // Circle arc
    {
        OdDgArc3dPtr arc = OdDgArc3d::createObject();
        pModel->addElement(arc);
        arc->setPrimaryAxis(1);
        arc->setSecondaryAxis(1);
        OdGePoint3d point;
        point.x = 0;
        point.y = 1;
        point.z = 2;
        arc->setOrigin(point);
        arc->setStartAngle(10 * DEG_TO_RAD);
        arc->setSweepAngle(180 * DEG_TO_RAD);
    }

    {
        OdDgCurve2dPtr curve = OdDgCurve2d::createObject();
        pModel->addElement(curve);
        OdGePoint2d point;
        point.x = 0;
        point.y = 0;
        curve->addVertex(point);
        point.x = 0;
        point.y = 1;
        curve->addVertex(point);
        point.x = 1;
        point.y = 1;
        curve->addVertex(point);
        point.x = 1;
        point.y = 0;
        curve->addVertex(point);

        point.x = 0;
        point.y = 0;
        curve->addVertex(point);
        point.x = 0;
        point.y = 1;
        curve->addVertex(point);
        point.x = 1;
        point.y = 1;
        curve->addVertex(point);
        point.x = 1;
        point.y = 0;
        curve->addVertex(point);
    }

    {
        OdDgCurve3dPtr curve = OdDgCurve3d::createObject();
        pModel->addElement(curve);
        OdGePoint3d point;
        point.x = 0;
        point.y = 0;
        point.z = 1;
        curve->addVertex(point);
        point.x = 0;
        point.y = 1;
        curve->addVertex(point);
        point.x = 1;
        point.y = 1;
        curve->addVertex(point);
        point.x = 1;
        point.y = 0;
        curve->addVertex(point);

        point.x = 0;
        point.y = 0;
        curve->addVertex(point);
        point.x = 0;
        point.y = 1;
        curve->addVertex(point);
        point.x = 1;
        point.y = 1;
        curve->addVertex(point);
        point.x = 1;
        point.y = 0;
        curve->addVertex(point);
    }

    {
        OdDgBSplineCurve3dPtr curve = OdDgBSplineCurve3d::createObject();
        pModel->addElement( curve );

        OdGePoint3dArray arrCtrlPts;
        OdGePoint3d center;
        double major = 1.0;
        double minor = 0.5;
        arrCtrlPts.push_back( center + OdGeVector3d( -1. * major,  1. * minor, 0. ) );
        arrCtrlPts.push_back( center + OdGeVector3d( -1. * major,  2. * minor, 0. ) );
        arrCtrlPts.push_back( center + OdGeVector3d( -2. * major,  2. * minor, 0. ) );
        arrCtrlPts.push_back( center + OdGeVector3d( -2. * major,  1. * minor, 0. ) );
        arrCtrlPts.push_back( center + OdGeVector3d( -1. * major,  1. * minor, 0. ) );
        arrCtrlPts.push_back( center + OdGeVector3d(  1. * major,  1. * minor, 0. ) );
        arrCtrlPts.push_back( center + OdGeVector3d(  2. * major,  1. * minor, 0. ) );
        arrCtrlPts.push_back( center + OdGeVector3d(  2. * major,  2. * minor, 0. ) );
        arrCtrlPts.push_back( center + OdGeVector3d(  1. * major,  2. * minor, 0. ) );
        arrCtrlPts.push_back( center + OdGeVector3d(  1. * major,  1. * minor, 0. ) );
        arrCtrlPts.push_back( center + OdGeVector3d(  1. * major, -1. * minor, 0. ) );
        arrCtrlPts.push_back( center + OdGeVector3d(  1. * major, -2. * minor, 0. ) );
        arrCtrlPts.push_back( center + OdGeVector3d(  2. * major, -2. * minor, 0. ) );
        arrCtrlPts.push_back( center + OdGeVector3d(  2. * major, -1. * minor, 0. ) );
        arrCtrlPts.push_back( center + OdGeVector3d(  1. * major, -1. * minor, 0. ) );
        arrCtrlPts.push_back( center + OdGeVector3d( -1. * major, -1. * minor, 0. ) );
        arrCtrlPts.push_back( center + OdGeVector3d( -2. * major, -1. * minor, 0. ) );
        arrCtrlPts.push_back( center + OdGeVector3d( -2. * major, -2. * minor, 0. ) );
        arrCtrlPts.push_back( center + OdGeVector3d( -1. * major, -2. * minor, 0. ) );
        arrCtrlPts.push_back( center + OdGeVector3d( -1. * major, -1. * minor, 0. ) );
        arrCtrlPts.push_back( center + OdGeVector3d( -1. * major,  1. * minor, 0. ) );

        OdGeKnotVector vrKnots;
        OdGeDoubleArray arrWeights;

        curve->setNurbsData( 4, false, true, arrCtrlPts, vrKnots, arrWeights );
    }

    {
        OdDgBSplineCurve2dPtr curve = OdDgBSplineCurve2d::createObject();
        pModel->addElement( curve );

        OdGePoint2dArray arrCtrlPts;
        OdGePoint2d center;
        double major = 1.0;
        double minor = 0.5;
        arrCtrlPts.push_back( center + OdGeVector2d( -1. * major,  1. * minor ) );
        arrCtrlPts.push_back( center + OdGeVector2d( -1. * major,  2. * minor ) );
        arrCtrlPts.push_back( center + OdGeVector2d( -2. * major,  2. * minor ) );
        arrCtrlPts.push_back( center + OdGeVector2d( -2. * major,  1. * minor ) );
        arrCtrlPts.push_back( center + OdGeVector2d( -1. * major,  1. * minor ) );
        arrCtrlPts.push_back( center + OdGeVector2d(  1. * major,  1. * minor ) );
        arrCtrlPts.push_back( center + OdGeVector2d(  2. * major,  1. * minor ) );
        arrCtrlPts.push_back( center + OdGeVector2d(  2. * major,  2. * minor ) );
        arrCtrlPts.push_back( center + OdGeVector2d(  1. * major,  2. * minor ) );
        arrCtrlPts.push_back( center + OdGeVector2d(  1. * major,  1. * minor ) );
        arrCtrlPts.push_back( center + OdGeVector2d(  1. * major, -1. * minor ) );
        arrCtrlPts.push_back( center + OdGeVector2d(  1. * major, -2. * minor ) );
        arrCtrlPts.push_back( center + OdGeVector2d(  2. * major, -2. * minor ) );
        arrCtrlPts.push_back( center + OdGeVector2d(  2. * major, -1. * minor ) );
        arrCtrlPts.push_back( center + OdGeVector2d(  1. * major, -1. * minor ) );
        arrCtrlPts.push_back( center + OdGeVector2d( -1. * major, -1. * minor ) );
        arrCtrlPts.push_back( center + OdGeVector2d( -2. * major, -1. * minor ) );
        arrCtrlPts.push_back( center + OdGeVector2d( -2. * major, -2. * minor ) );
        arrCtrlPts.push_back( center + OdGeVector2d( -1. * major, -2. * minor ) );
        arrCtrlPts.push_back( center + OdGeVector2d( -1. * major, -1. * minor ) );
        arrCtrlPts.push_back( center + OdGeVector2d( -1. * major,  1. * minor ) );

        OdGeKnotVector vrKnots;
        OdGeDoubleArray arrWeights;

        curve->setNurbsData( 4, false, true, arrCtrlPts, vrKnots, arrWeights );
    }

    // ComplexString
    {
        OdDgComplexStringPtr complex = OdDgComplexString::createObject();
        pModel->addElement(complex);

        OdDgLine2dPtr line = OdDgLine2d::createObject();
        complex->add(line);
        OdGePoint2d point;
        point.x = 0;
        point.y = 1;
        line->setStartPoint(point);
        point.x = 2;
        point.y = 3;
        line->setEndPoint(point);

        line = OdDgLine2d::createObject();
        complex->add(line);
        point.x = 2;
        point.y = 3;
        line->setStartPoint(point);
        point.x = 4;
        point.y = 5;
        line->setEndPoint(point);
    }

    // ComplexString
    {
        OdDgComplexStringPtr complex = OdDgComplexString::createObject();
        pModel->addElement(complex);

        OdDgLine3dPtr line = OdDgLine3d::createObject();
        complex->add(line);
        OdGePoint3d point;
        point.x = 0;
        point.y = 1;
        point.z = 2;
        line->setStartPoint(point);
        point.x = 2;
        point.y = 3;
        line->setEndPoint(point);

        line = OdDgLine3d::createObject();
        complex->add(line);
        point.x = 2;
        point.y = 3;
        line->setStartPoint(point);
        point.x = 4;
        point.y = 5;
        line->setEndPoint(point);
    }

    // ComplexString
    {
        OdDgComplexStringPtr complex = OdDgComplexString::createObject();
        pModel->addElement(complex);

        OdDgLine2dPtr line = OdDgLine2d::createObject();
        complex->add(line);
        OdGePoint2d point;
        point.x = 0;
        point.y = 1;
        line->setStartPoint(point);
        point.x = 2;
        point.y = 1;
        line->setEndPoint(point);

        OdDgArc2dPtr arc = OdDgArc2d::createObject();
        complex->add(arc);
        arc->setPrimaryAxis(1);
        arc->setSecondaryAxis(1);
        point.x = 3;
        point.y = 1;
        arc->setOrigin(point);
        arc->setStartAngle(180 * DEG_TO_RAD);
        arc->setSweepAngle(180 * DEG_TO_RAD);
    }

    // Polygon 2D
    {
        OdDgShape2dPtr shape = OdDgShape2d::createObject();
        pModel->addElement(shape);
        OdGePoint2d point;
        point.x = 0;
        point.y = 0;
        shape->addVertex(point);
        point.x = 0;
        point.y = 1;
        shape->addVertex(point);
        point.x = 1;
        point.y = 1;
        shape->addVertex(point);
        point.x = 0;
        point.y = 0;
        shape->addVertex(point);

        OdDgFillColorLinkagePtr fillColor =
                OdDgFillColorLinkage::createObject();
        OdUInt32 nIdx = OdDgColorTable::getColorIndexByRGB(
            shape->database(), ODRGB( 200, 255, 150 ) );
        fillColor->setColorIndex( nIdx );
        shape->addLinkage( fillColor->getPrimaryId(),
                           fillColor.get() );
    }

    // Polygon 3D
    {
        OdDgShape3dPtr shape = OdDgShape3d::createObject();
        pModel->addElement(shape);
        OdGePoint3d point;
        point.x = 0;
        point.y = 0;
        point.z = 1;
        shape->addVertex(point);
        point.x = 0;
        point.y = 1;
        shape->addVertex(point);
        point.x = 1;
        point.y = 1;
        shape->addVertex(point);
        point.x = 0;
        point.y = 0;
        shape->addVertex(point);
    }

    // Polygon 2D with hole
    {
        OdDgCellHeader2dPtr pCell = OdDgCellHeader2d::createObject(); 
        pModel->addElement(pCell);

        OdDgShape2dPtr shape = OdDgShape2d::createObject();
        pCell->add(shape);
        OdGePoint2d point;
        point.x = 0;
        point.y = 0;
        shape->addVertex(point);
        point.x = 0;
        point.y = 1;
        shape->addVertex(point);
        point.x = 1;
        point.y = 1;
        shape->addVertex(point);
        point.x = 0;
        point.y = 0;
        shape->addVertex(point);

        shape = OdDgShape2d::createObject();
        shape->setHbitFlag(true);
        pCell->add(shape);
        point.x = 0.1;
        point.y = 0.1;
        shape->addVertex(point);
        point.x = 0.1;
        point.y = 0.9;
        shape->addVertex(point);
        point.x = 0.9;
        point.y = 0.9;
        shape->addVertex(point);
        point.x = 0.1;
        point.y = 0.1;
        shape->addVertex(point);
    }

    // Polygon 3D with hole
    {
        OdDgCellHeader3dPtr pCell = OdDgCellHeader3d::createObject(); 
        pModel->addElement(pCell);

        OdDgShape3dPtr shape = OdDgShape3d::createObject();
        pCell->add(shape);
        OdGePoint3d point;
        point.x = 0;
        point.y = 0;
        point.z = 1;
        shape->addVertex(point);
        point.x = 0;
        point.y = 1;
        shape->addVertex(point);
        point.x = 1;
        point.y = 1;
        shape->addVertex(point);
        point.x = 0;
        point.y = 0;
        shape->addVertex(point);

        shape = OdDgShape3d::createObject();
        shape->setHbitFlag(true);
        pCell->add(shape);
        point.x = 0.1;
        point.y = 0.1;
        shape->addVertex(point);
        point.x = 0.1;
        point.y = 0.9;
        shape->addVertex(point);
        point.x = 0.9;
        point.y = 0.9;
        shape->addVertex(point);
        point.x = 0.1;
        point.y = 0.1;
        shape->addVertex(point);
    }

    // ComplexShape
    {
        OdDgComplexShapePtr complex = OdDgComplexShape::createObject();
        pModel->addElement(complex);

        OdDgLine2dPtr line = OdDgLine2d::createObject();
        complex->add(line);
        OdGePoint2d point;
        point.x = 0;
        point.y = 0;
        line->setStartPoint(point);
        point.x = 0;
        point.y = 1;
        line->setEndPoint(point);

        line = OdDgLine2d::createObject();
        complex->add(line);
        point.x = 1;
        point.y = 1;
        line->setStartPoint(point);
        point.x = 1;
        point.y = 0;
        line->setEndPoint(point);

        line = OdDgLine2d::createObject();
        complex->add(line);
        point.x = 1;
        point.y = 0;
        line->setStartPoint(point);
        point.x = 0;
        point.y = 0;
        line->setEndPoint(point);
    }

    // ComplexShape
    {
        OdDgComplexShapePtr complex = OdDgComplexShape::createObject();
        pModel->addElement(complex);

        OdDgLine2dPtr line = OdDgLine2d::createObject();
        complex->add(line);
        OdGePoint2d point;
        point.x = 0;
        point.y = 0;
        line->setStartPoint(point);
        point.x = 0;
        point.y = 1;
        line->setEndPoint(point);

        OdDgArc2dPtr arc = OdDgArc2d::createObject();
        complex->add(arc);
        arc->setPrimaryAxis(0.5);
        arc->setSecondaryAxis(0.5);
        point.x = 0.5;
        point.y = 1;
        arc->setOrigin(point);
        arc->setStartAngle(180 * DEG_TO_RAD);
        arc->setSweepAngle(-180 * DEG_TO_RAD);

        line = OdDgLine2d::createObject();
        complex->add(line);
        point.x = 1;
        point.y = 1;
        line->setStartPoint(point);
        point.x = 0;
        point.y = 0;
        line->setEndPoint(point);
    }

    // ComplexShape (out of order rings, we handle that, not sure this is legal though)
    {
        OdDgComplexShapePtr complex = OdDgComplexShape::createObject();
        pModel->addElement(complex);

        OdDgLine2dPtr line = OdDgLine2d::createObject();
        complex->add(line);
        OdGePoint2d point;
        point.x = 0;
        point.y = 0;
        line->setStartPoint(point);
        point.x = 0;
        point.y = 1;
        line->setEndPoint(point);

        line = OdDgLine2d::createObject();
        complex->add(line);
        point.x = 1;
        point.y = 0;
        line->setStartPoint(point);
        point.x = 0;
        point.y = 0;
        line->setEndPoint(point);

        line = OdDgLine2d::createObject();
        complex->add(line);
        point.x = 1;
        point.y = 1;
        line->setStartPoint(point);
        point.x = 1;
        point.y = 0;
        line->setEndPoint(point);
    }

    //create a definition and reference
    {
        OdDgSharedCellDefinitionPtr definition;
        OdDgSharedCellDefinitionTablePtr table = pDb->getSharedCellDefinitionTable(OdDg::kForWrite);

        definition = OdDgSharedCellDefinition::createObject();
        definition->setName( "Named definition" );
        table->add( definition );

        OdDgEllipse3dPtr ellipse;
        ellipse = OdDgEllipse3d::createObject();
        ellipse->setPrimaryAxis( 1. );
        ellipse->setSecondaryAxis( 1. );
        definition->add( ellipse );

        OdDgSharedCellReferencePtr reference;
  
        reference = OdDgSharedCellReference::createObject();
        reference->setDefinitionName( "Named definition" );
        OdGePoint3d point;
        point.x = 0;
        point.y = 1;
        point.z = 2;
        reference->setOrigin( point );
        pModel->addElement( reference );
    }

    // Unhandled element.
    {
        OdDgTagElementPtr tag = OdDgTagElement::createObject();
        pModel->addElement( tag );
    }

    pModel->fitToView();
    pDb->writeFile( "test_dgnv8.dgn" );
    pDb = nullptr;

    ::odrxUninitialize();

    return 0;
}
