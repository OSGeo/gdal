/******************************************************************************
 *
 * Project:  GTM Driver
 * Purpose:  Implementation of gtmwaypoint class.
 * Author:   Leonardo de Paula Rosa Piga; http://lampiao.lsc.ic.unicamp.br/~piga
 *
 *
 ******************************************************************************
 * Copyright (c) 2009, Leonardo de Paula Rosa Piga
 * Copyright (c) 2009-2013, Even Rouault <even dot rouault at spatialys.com>
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
#include "ogr_gtm.h"
#include "cpl_time.h"

CPL_CVSID("$Id$")

GTMWaypointLayer::GTMWaypointLayer( const char* pszNameIn,
                                    OGRSpatialReference* poSRSIn,
                                    CPL_UNUSED int bWriterIn,
                                    OGRGTMDataSource* poDSIn )
{
    poCT = nullptr;

    /* We are implementing just WGS84, although GTM supports other datum
       formats. */
    if( poSRSIn != nullptr )
    {
        poSRS = new OGRSpatialReference(nullptr);
        poSRS->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
        poSRS->SetWellKnownGeogCS( "WGS84" );
        if (!poSRS->IsSame(poSRSIn))
        {
            poCT = OGRCreateCoordinateTransformation( poSRSIn, poSRS );
            if( poCT == nullptr && poDSIn->isFirstCTError() )
            {
                /* If we can't create a transformation, issue a warning - but
                   continue the transformation*/
                char *pszWKT = nullptr;

                poSRSIn->exportToPrettyWkt( &pszWKT, FALSE );

                CPLError( CE_Warning, CPLE_AppDefined,
                          "Failed to create coordinate transformation between the\n"
                          "input coordinate system and WGS84.  This may be because they\n"
                          "are not transformable.\n"
                          "This message will not be issued any more. \n"
                          "\nSource:\n%s\n",
                          pszWKT );

                CPLFree( pszWKT );
                poDSIn->issuedFirstCTError();
            }
        }
    }
    else
    {
        poSRS = nullptr;
    }

    poDS = poDSIn;

    nNextFID = 0;
    nTotalFCount = poDS->getNWpts();

    pszName = CPLStrdup(pszNameIn);

    poFeatureDefn = new OGRFeatureDefn( pszName );
    SetDescription( poFeatureDefn->GetName() );
    poFeatureDefn->Reference();
    poFeatureDefn->SetGeomType ( wkbPoint );
    poFeatureDefn->GetGeomFieldDefn(0)->SetSpatialRef(poSRS);

    /* We implement just name, comment, and icon, if others needed feel
       free to append more parameters */
    OGRFieldDefn oFieldName( "name", OFTString );
    poFeatureDefn->AddFieldDefn( &oFieldName );

    OGRFieldDefn oFieldComment( "comment", OFTString );
    poFeatureDefn->AddFieldDefn( &oFieldComment );

    OGRFieldDefn oFieldIcon( "icon", OFTInteger );
    poFeatureDefn->AddFieldDefn( &oFieldIcon );

    OGRFieldDefn oFieldTime( "time", OFTDateTime );
    poFeatureDefn->AddFieldDefn( &oFieldTime );
}

GTMWaypointLayer::~GTMWaypointLayer() {}

/************************************************************************/
/*                      WriteFeatureAttributes()                        */
/************************************************************************/
void GTMWaypointLayer::WriteFeatureAttributes( OGRFeature *poFeature, float altitude )
{
    char psNameField[] = "          "; // 10 spaces
    char* pszcomment = nullptr;
    int icon = 48;
    int date = 0;
    for (int i = 0; i < poFeatureDefn->GetFieldCount(); ++i)
    {
        OGRFieldDefn *poFieldDefn = poFeatureDefn->GetFieldDefn( i );
        if( poFeature->IsFieldSetAndNotNull( i ) )
        {
            const char* l_pszName = poFieldDefn->GetNameRef();
            /* Waypoint name */
            if (STARTS_WITH(l_pszName, "name"))
            {
                strncpy (psNameField, poFeature->GetFieldAsString( i ), 10);
                CPLStrlcat (psNameField, "          ", sizeof(psNameField));
            }
            /* Waypoint comment */
            else if (STARTS_WITH(l_pszName, "comment"))
            {
                CPLFree(pszcomment);
                pszcomment = CPLStrdup( poFeature->GetFieldAsString( i ) );
            }
            /* Waypoint icon */
            else if (STARTS_WITH(l_pszName, "icon"))
            {
                icon = poFeature->GetFieldAsInteger( i );
                // Check if it is a valid icon
                if (icon < 1 || icon > 220)
                    icon = 48;
            }
            /* Waypoint date */
            else if (EQUAL(l_pszName, "time"))
            {
                struct tm brokendowndate;
                int year, month, day, hour, min, sec, TZFlag;
                if (poFeature->GetFieldAsDateTime( i, &year, &month, &day, &hour, &min, &sec, &TZFlag))
                {
                    brokendowndate.tm_year = year - 1900;
                    brokendowndate.tm_mon = month - 1;
                    brokendowndate.tm_mday = day;
                    brokendowndate.tm_hour = hour;
                    brokendowndate.tm_min = min;
                    brokendowndate.tm_sec = sec;
                    GIntBig unixTime = CPLYMDHMSToUnixTime(&brokendowndate);
                    if (TZFlag != 0 && TZFlag != 1)
                        unixTime -= (TZFlag - 100) * 15 * 60;
                    if (unixTime <= GTM_EPOCH || (unixTime - GTM_EPOCH) != (int)(unixTime - GTM_EPOCH))
                    {
                        CPLError(CE_Warning, CPLE_AppDefined,
                                  "%04d/%02d/%02d %02d:%02d:%02d is not a valid datetime for GTM",
                                  year, month, day, hour, min, sec);
                    }
                    else
                    {
                        date = (int)(unixTime - GTM_EPOCH);
                    }
                }
            }
        }
    }

    if (pszcomment == nullptr)
        pszcomment = CPLStrdup( "" );

    const size_t commentLength = strlen(pszcomment);

    const size_t bufferSize = 27 + commentLength;
    void* pBuffer = CPLMalloc(bufferSize);
    void* pBufferAux = pBuffer;
    /* Write waypoint name to buffer */
    memcpy((char*)pBufferAux, psNameField, 10);

    /* Write waypoint string comment size to buffer */
    pBufferAux = (char*)pBuffer+10;
    appendUShort(pBufferAux, (unsigned short) commentLength);

    /* Write waypoint string comment to buffer */
    memcpy((char*)pBuffer+12, pszcomment, commentLength);

    /* Write icon to buffer */
    pBufferAux = (char*)pBuffer+12+commentLength;
    appendUShort(pBufferAux, (unsigned short) icon);

    /* Write dslp to buffer */
    pBufferAux = (char*)pBufferAux + 2;
    appendUChar(pBufferAux, 3);

    /* Date */
    pBufferAux = (char*)pBufferAux + 1;
    appendInt(pBufferAux, date);

    /* wrot */
    pBufferAux = (char*)pBufferAux + 4;
    appendUShort(pBufferAux, 0);

    /* walt */
    pBufferAux = (char*)pBufferAux + 2;
    appendFloat(pBufferAux, altitude);

    /* wlayer */
    pBufferAux = (char*)pBufferAux + 4;
    appendUShort(pBufferAux, 0);

    VSIFWriteL(pBuffer, bufferSize, 1, poDS->getOutputFP());
    poDS->incNumWaypoints();

    CPLFree(pszcomment);
    CPLFree(pBuffer);
}

/************************************************************************/
/*                           ICreateFeature()                            */
/************************************************************************/
OGRErr GTMWaypointLayer::ICreateFeature (OGRFeature *poFeature)
{
    VSILFILE* fp = poDS->getOutputFP();
    if (fp == nullptr)
        return OGRERR_FAILURE;

    OGRGeometry *poGeom = poFeature->GetGeometryRef();
    if ( poGeom == nullptr )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Features without geometry not supported by GTM writer in waypoints layer." );
        return OGRERR_FAILURE;
    }

    if (nullptr != poCT)
    {
        poGeom = poGeom->clone();
        poGeom->transform( poCT );
    }

    switch( poGeom->getGeometryType() )
    {
    case wkbPoint:
    case wkbPoint25D:
    {
        OGRPoint* point = poGeom->toPoint();
        double lat = point->getY();
        double lon = point->getX();
        CheckAndFixCoordinatesValidity(lat, lon);
        poDS->checkBounds((float)lat, (float)lon);
        writeDouble(fp, lat);
        writeDouble(fp, lon);
        float altitude = 0.0;
        if (poGeom->getGeometryType() == wkbPoint25D)
            altitude = (float) point->getZ();

        WriteFeatureAttributes(poFeature, altitude);
        break;
    }

    default:
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "Geometry type of `%s' not supported for 'waypoint' element.\n",
                  OGRGeometryTypeToName(poGeom->getGeometryType()) );
        return OGRERR_FAILURE;
    }
    }

    if (nullptr != poCT)
        delete poGeom;

    return OGRERR_NONE;
}

OGRFeature* GTMWaypointLayer::GetNextFeature()
{
    if( bError )
        return nullptr;

    while (poDS->hasNextWaypoint())
    {
        Waypoint* poWaypoint = poDS->fetchNextWaypoint();
        if (poWaypoint == nullptr)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Could not read waypoint. File probably corrupted");
            bError = true;
            return nullptr;
        }

        OGRFeature* poFeature = new OGRFeature( poFeatureDefn );
        double altitude = poWaypoint->getAltitude();
        if (altitude == 0.0)
            poFeature->SetGeometryDirectly(new OGRPoint
                                           (poWaypoint->getLongitude(),
                                            poWaypoint->getLatitude()));
        else
            poFeature->SetGeometryDirectly(new OGRPoint
                                           (poWaypoint->getLongitude(),
                                            poWaypoint->getLatitude(),
                                            altitude));

        if (poSRS)
            poFeature->GetGeometryRef()->assignSpatialReference(poSRS);
        poFeature->SetField( NAME, poWaypoint->getName());
        poFeature->SetField( COMMENT, poWaypoint->getComment());
        poFeature->SetField( ICON, poWaypoint->getIcon());

        GIntBig wptdate = poWaypoint->getDate();
        if (wptdate != 0)
        {
            struct tm brokendownTime;
            CPLUnixTimeToYMDHMS(wptdate, &brokendownTime);
            poFeature->SetField( DATE,
                                 brokendownTime.tm_year + 1900,
                                 brokendownTime.tm_mon + 1,
                                 brokendownTime.tm_mday,
                                 brokendownTime.tm_hour,
                                 brokendownTime.tm_min,
                                 static_cast<float>(brokendownTime.tm_sec));
        }

        poFeature->SetFID( nNextFID++ );
        delete poWaypoint;
        if( (m_poFilterGeom == nullptr
             || FilterGeometry( poFeature->GetGeometryRef() ) )
            && (m_poAttrQuery == nullptr
                || m_poAttrQuery->Evaluate( poFeature )) )
            return poFeature;

        delete poFeature;
    }
    return nullptr;
}

GIntBig GTMWaypointLayer::GetFeatureCount(int bForce)
{
    if (m_poFilterGeom == nullptr && m_poAttrQuery == nullptr)
        return poDS->getNWpts();

    return OGRLayer::GetFeatureCount(bForce);
}

void GTMWaypointLayer::ResetReading()
{
    nNextFID = 0;
    poDS->rewindWaypoint();
}
