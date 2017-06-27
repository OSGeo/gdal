/******************************************************************************
 *
 * Project:  X-Plane awy.dat file reader
 * Purpose:  Implements OGRXPlaneAwyReader class
 * Author:   Even Rouault, even dot rouault at mines dash paris dot org
 *
 ******************************************************************************
 * Copyright (c) 2008-2011, Even Rouault <even dot rouault at mines-paris dot org>
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

#include "ogr_xplane_awy_reader.h"

CPL_CVSID("$Id$")

/************************************************************************/
/*                   OGRXPlaneCreateAwyFileReader                       */
/************************************************************************/

OGRXPlaneReader* OGRXPlaneCreateAwyFileReader( OGRXPlaneDataSource* poDataSource )
{
    OGRXPlaneReader* poReader = new OGRXPlaneAwyReader(poDataSource);
    return poReader;
}

/************************************************************************/
/*                         OGRXPlaneAwyReader()                         */
/************************************************************************/
OGRXPlaneAwyReader::OGRXPlaneAwyReader() :
    poAirwaySegmentLayer(NULL),
    poAirwayIntersectionLayer(NULL)
{}

/************************************************************************/
/*                          OGRXPlaneAwyReader()                        */
/************************************************************************/

OGRXPlaneAwyReader::OGRXPlaneAwyReader( OGRXPlaneDataSource* poDataSource ) :
    poAirwaySegmentLayer(new OGRXPlaneAirwaySegmentLayer()),
    poAirwayIntersectionLayer(new OGRXPlaneAirwayIntersectionLayer())
{
    poDataSource->RegisterLayer(poAirwaySegmentLayer);
    poDataSource->RegisterLayer(poAirwayIntersectionLayer);
}

/************************************************************************/
/*                        CloneForLayer()                               */
/************************************************************************/

OGRXPlaneReader* OGRXPlaneAwyReader::CloneForLayer(OGRXPlaneLayer* poLayer)
{
    OGRXPlaneAwyReader* poReader = new OGRXPlaneAwyReader();

    poReader->poInterestLayer = poLayer;

    SET_IF_INTEREST_LAYER(poAirwaySegmentLayer);
    SET_IF_INTEREST_LAYER(poAirwayIntersectionLayer);

    if (pszFilename)
    {
        poReader->pszFilename = CPLStrdup(pszFilename);
        poReader->fp = VSIFOpenL( pszFilename, "rt" );
    }

    return poReader;
}

/************************************************************************/
/*                       IsRecognizedVersion()                          */
/************************************************************************/

int OGRXPlaneAwyReader::IsRecognizedVersion( const char* pszVersionString)
{
    return STARTS_WITH_CI(pszVersionString, "640 Version");
}

/************************************************************************/
/*                                Read()                                */
/************************************************************************/

void OGRXPlaneAwyReader::Read()
{
    const char* pszLine = NULL;
    while((pszLine = CPLReadLineL(fp)) != NULL)
    {
        papszTokens = CSLTokenizeString(pszLine);
        nTokens = CSLCount(papszTokens);

        nLineNumber ++;

        if (nTokens == 1 && strcmp(papszTokens[0], "99") == 0)
        {
            CSLDestroy(papszTokens);
            papszTokens = NULL;
            bEOF = true;
            return;
        }
        else if( nTokens == 0 || !assertMinCol(10) )
        {
            CSLDestroy(papszTokens);
            papszTokens = NULL;
            continue;
        }

        ParseRecord();

        CSLDestroy(papszTokens);
        papszTokens = NULL;

        if( poInterestLayer && !poInterestLayer->IsEmpty() )
            return;
    }

    papszTokens = NULL;
    bEOF = true;
}

/************************************************************************/
/*                            ParseRecord()                             */
/************************************************************************/

void OGRXPlaneAwyReader::ParseRecord()
{
    double dfLat1 = 0.0;
    double dfLon1 = 0.0;
    double dfLat2 = 0.0;
    double dfLon2 = 0.0;

    const char* pszFirstPointName = papszTokens[0];
    RET_IF_FAIL(readLatLon(&dfLat1, &dfLon1, 1));
    const char* pszSecondPointName = papszTokens[3];
    RET_IF_FAIL(readLatLon(&dfLat2, &dfLon2, 4));
    const bool bIsHigh = atoi(papszTokens[6]) == 2;
    const int nBaseFL = atoi(papszTokens[7]);
    const int nTopFL = atoi(papszTokens[8]);
    const char* pszAirwaySegmentName = papszTokens[9];

    if (poAirwayIntersectionLayer)
    {
        poAirwayIntersectionLayer->AddFeature(pszFirstPointName,
                                              dfLat1, dfLon1);
        poAirwayIntersectionLayer->AddFeature(pszSecondPointName,
                                              dfLat2, dfLon2);
    }

    if (poAirwaySegmentLayer)
    {
/*
        poAirwaySegmentLayer->AddFeature(pszAirwaySegmentName,
                                         pszFirstPointName,
                                         pszSecondPointName,
                                         dfLat1, dfLon1, dfLat2, dfLon2,
                                         bIsHigh, nBaseFL, nTopFL);
*/
        if (strchr(pszAirwaySegmentName, '-'))
        {
            char** papszSegmentNames
                = CSLTokenizeString2( pszAirwaySegmentName, "-",
                                      CSLT_HONOURSTRINGS );
            int i = 0;
            while(papszSegmentNames[i])
            {
                poAirwaySegmentLayer->AddFeature(papszSegmentNames[i],
                                                 pszFirstPointName,
                                                 pszSecondPointName,
                                                 dfLat1, dfLon1, dfLat2, dfLon2,
                                                 bIsHigh, nBaseFL, nTopFL);
                i++;
            }
            CSLDestroy(papszSegmentNames);
        }
        else
        {
            poAirwaySegmentLayer->AddFeature(pszAirwaySegmentName,
                                            pszFirstPointName,
                                            pszSecondPointName,
                                            dfLat1, dfLon1, dfLat2, dfLon2,
                                            bIsHigh, nBaseFL, nTopFL);
        }
    }
}

/************************************************************************/
/*                       OGRXPlaneAirwaySegmentLayer()                  */
/************************************************************************/

OGRXPlaneAirwaySegmentLayer::OGRXPlaneAirwaySegmentLayer() :
    OGRXPlaneLayer("AirwaySegment")
{
    poFeatureDefn->SetGeomType( wkbLineString );

    OGRFieldDefn oFieldSegmentName("segment_name", OFTString );
    poFeatureDefn->AddFieldDefn( &oFieldSegmentName );

    OGRFieldDefn oFieldPoint1Name("point1_name", OFTString );
    poFeatureDefn->AddFieldDefn( &oFieldPoint1Name );

    OGRFieldDefn oFieldPoint2Name("point2_name", OFTString );
    poFeatureDefn->AddFieldDefn( &oFieldPoint2Name );

    OGRFieldDefn oFieldIsHigh("is_high", OFTInteger );
    oFieldIsHigh.SetWidth( 1 );
    poFeatureDefn->AddFieldDefn( &oFieldIsHigh );

    OGRFieldDefn oFieldBase("base_FL", OFTInteger );
    oFieldBase.SetWidth( 3 );
    poFeatureDefn->AddFieldDefn( &oFieldBase );

    OGRFieldDefn oFieldTop("top_FL", OFTInteger );
    oFieldTop.SetWidth( 3 );
    poFeatureDefn->AddFieldDefn( &oFieldTop );
}

/************************************************************************/
/*                           AddFeature()                               */
/************************************************************************/

OGRFeature*
     OGRXPlaneAirwaySegmentLayer::AddFeature(const char* pszAirwaySegmentName,
                                             const char* pszFirstPointName,
                                             const char* pszSecondPointName,
                                             double dfLat1,
                                             double dfLon1,
                                             double dfLat2,
                                             double dfLon2,
                                             int    bIsHigh,
                                             int    nBaseFL,
                                             int    nTopFL)
{
    OGRFeature* poFeature = new OGRFeature(poFeatureDefn);
    if (fabs(dfLon1 - dfLon2) < 270)
    {
        OGRLineString* lineString = new OGRLineString();
        lineString->addPoint(dfLon1, dfLat1);
        lineString->addPoint(dfLon2, dfLat2);
        poFeature->SetGeometryDirectly( lineString );
    }
    else
    {
        /* Crossing antemeridian */
        OGRMultiLineString* multiLineString = new OGRMultiLineString();
        OGRLineString* lineString1 = new OGRLineString();
        OGRLineString* lineString2 = new OGRLineString();
        lineString1->addPoint(dfLon1, dfLat1);
        if (dfLon1 < dfLon2)
        {
            const double dfLatInt =
                dfLat1 +
                (dfLat2 - dfLat1) * (-180 - dfLon1) / ((dfLon2 - 360) - dfLon1);
            lineString1->addPoint(-180, dfLatInt);
            lineString2->addPoint(180, dfLatInt);
        }
        else
        {
            const double dfLatInt =
                dfLat1 +
                (dfLat2 - dfLat1) * (180 - dfLon1) / ((dfLon2 + 360) - dfLon1);
            lineString1->addPoint(180, dfLatInt);
            lineString2->addPoint(-180, dfLatInt);
        }
        lineString2->addPoint(dfLon2, dfLat2);
        multiLineString->addGeometryDirectly( lineString1 );
        multiLineString->addGeometryDirectly( lineString2 );
        poFeature->SetGeometryDirectly( multiLineString );
    }

    int nCount = 0;
    poFeature->SetField( nCount++, pszAirwaySegmentName );
    poFeature->SetField( nCount++, pszFirstPointName );
    poFeature->SetField( nCount++, pszSecondPointName );
    poFeature->SetField( nCount++, bIsHigh );
    poFeature->SetField( nCount++, nBaseFL );
    poFeature->SetField( nCount++, nTopFL );

    RegisterFeature(poFeature);

    return poFeature;
}

/************************************************************************/
/*                 EqualAirwayIntersectionFeature                       */
/************************************************************************/

static int EqualAirwayIntersectionFeatureFunc(
    const void* _feature1, const void* _feature2 )
{
    OGRFeature* feature1 = (OGRFeature*)_feature1;
    OGRFeature* feature2 = (OGRFeature*)_feature2;
    if (strcmp(feature1->GetFieldAsString(0),
               feature2->GetFieldAsString(0)) == 0)
    {
        OGRPoint* point1 = (OGRPoint*) feature1->GetGeometryRef();
        OGRPoint* point2 = (OGRPoint*) feature2->GetGeometryRef();
        return
            point1->getX() == point2->getX() &&
            point1->getY() == point2->getY();
    }
    return false;
}

/************************************************************************/
/*                      OGRXPlaneAirwayHashDouble()                     */
/************************************************************************/

static unsigned long OGRXPlaneAirwayHashDouble(const double& dfVal)
{
    /* To make a long story short, we must copy the double into */
    /* an array in order to respect C strict-aliasing rule */
    /* We can't directly cast into an unsigned int* */
    /* See #2521 for the longer version */
    unsigned int anValue[2];
    memcpy(anValue, &dfVal, sizeof(double));
    return anValue[0] ^ anValue[1];
}

/************************************************************************/
/*               HashAirwayIntersectionFeatureFunc                      */
/************************************************************************/

static unsigned long HashAirwayIntersectionFeatureFunc(const void* _feature)
{
    OGRFeature* feature = reinterpret_cast<OGRFeature *>(
        const_cast<void *>(_feature));
    OGRPoint* point = reinterpret_cast<OGRPoint *>( feature->GetGeometryRef() );
    unsigned long hash = CPLHashSetHashStr(
        reinterpret_cast<unsigned char*>(const_cast<char *>(
            feature->GetFieldAsString(0))));
    const double x = point->getX();
    const double y = point->getY();
    return hash ^ OGRXPlaneAirwayHashDouble(x) ^ OGRXPlaneAirwayHashDouble(y);
}

/************************************************************************/
/*               FreeAirwayIntersectionFeatureFunc                      */
/************************************************************************/

static void FreeAirwayIntersectionFeatureFunc(void* _feature)
{
    delete reinterpret_cast<OGRFeature*>(_feature);
}

/************************************************************************/
/*                 OGRXPlaneAirwayIntersectionLayer()                   */
/************************************************************************/

OGRXPlaneAirwayIntersectionLayer::OGRXPlaneAirwayIntersectionLayer() :
    OGRXPlaneLayer("AirwayIntersection"),
    poSet(CPLHashSetNew(HashAirwayIntersectionFeatureFunc,
                        EqualAirwayIntersectionFeatureFunc,
                        FreeAirwayIntersectionFeatureFunc))
{
    poFeatureDefn->SetGeomType( wkbPoint );

    OGRFieldDefn oFieldName("name", OFTString );
    poFeatureDefn->AddFieldDefn( &oFieldName );
}

/************************************************************************/
/*                ~OGRXPlaneAirwayIntersectionLayer()                   */
/************************************************************************/

OGRXPlaneAirwayIntersectionLayer::~OGRXPlaneAirwayIntersectionLayer()
{
    CPLHashSetDestroy(poSet);
}

/************************************************************************/
/*                           AddFeature()                               */
/************************************************************************/

OGRFeature*
     OGRXPlaneAirwayIntersectionLayer::AddFeature(const char* pszIntersectionName,
                                                  double dfLat,
                                                  double dfLon)
{
    OGRFeature* poFeature = new OGRFeature(poFeatureDefn);
    poFeature->SetGeometryDirectly( new OGRPoint( dfLon, dfLat ) );
    poFeature->SetField( 0, pszIntersectionName );

    if (CPLHashSetLookup(poSet, poFeature) == NULL)
    {
        CPLHashSetInsert(poSet, poFeature->Clone());
        RegisterFeature(poFeature);

        return poFeature;
    }
    else
    {
        delete poFeature;
        return NULL;
    }
}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGRXPlaneAirwayIntersectionLayer::ResetReading()
{
    if (poReader)
    {
        CPLHashSetDestroy(poSet);
        poSet = CPLHashSetNew(HashAirwayIntersectionFeatureFunc,
                              EqualAirwayIntersectionFeatureFunc,
                              FreeAirwayIntersectionFeatureFunc);
    }

    OGRXPlaneLayer::ResetReading();
}
