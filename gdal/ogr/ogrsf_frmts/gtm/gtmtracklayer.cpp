/******************************************************************************
 * $Id$
 *
 * Project:  GTM Driver
 * Purpose:  Implementation of GTMTrackLayer class.
 * Author:   Leonardo de Paula Rosa Piga; http://lampiao.lsc.ic.unicamp.br/~piga
 *
 *
 ******************************************************************************
 * Copyright (c) 2009, Leonardo de Paula Rosa Piga
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


GTMTrackLayer::GTMTrackLayer( const char* pszName,
                              OGRSpatialReference* poSRSIn,
                              int bWriterIn,
                              OGRGTMDataSource* poDSIn )
{
    poCT = NULL;
  
    /* We are implementing just WGS84, although GTM supports other datum
       formats. */
    if( poSRSIn != NULL )
    {
        poSRS = new OGRSpatialReference(NULL);   
        poSRS->SetWellKnownGeogCS( "WGS84" );
        if (!poSRS->IsSame(poSRSIn))
        {
            poCT = OGRCreateCoordinateTransformation( poSRSIn, poSRS );
            if( poCT == NULL && poDSIn->isFirstCTError() )
            {
                /* If we can't create a transformation, issue a warning - but
                   continue the transformation*/
                char *pszWKT = NULL;

                poSRSIn->exportToPrettyWkt( &pszWKT, FALSE );

                CPLError( CE_Warning, CPLE_AppDefined,
                          "Failed to create coordinate transformation between the\n"
                          "input coordinate system and WGS84.  This may be because they\n"
                          "are not transformable, or because projection services\n"
                          "(PROJ.4 DLL/.so) could not be loaded.\n" 
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
        poSRS = NULL;
    }

    poDS = poDSIn;

    nNextFID = 0;
    nTotalFCount = poDS->getNTracks();

    poFeatureDefn = new OGRFeatureDefn( pszName );
    poFeatureDefn->Reference();
    poFeatureDefn->SetGeomType ( wkbLineString );
    poFeatureDefn->GetGeomFieldDefn(0)->SetSpatialRef(poSRS);

    /* We implement just name, type, and color for tracks, if others
       needed feel free to append more parameters and implement the
       code */
    OGRFieldDefn oFieldName( "name", OFTString );
    poFeatureDefn->AddFieldDefn( &oFieldName );

    OGRFieldDefn oFieldTrackType( "type", OFTInteger );
    poFeatureDefn->AddFieldDefn( &oFieldTrackType );

    OGRFieldDefn oFieldColor( "color", OFTInteger );
    poFeatureDefn->AddFieldDefn( &oFieldColor );
  
    this->pszName = CPLStrdup(pszName);
}

GTMTrackLayer::~GTMTrackLayer()
{
    /* poDS, poSRS, poCT, pszName, and poFeatureDefn are released on
       parent class*/
}


/************************************************************************/
/*                      WriteFeatureAttributes()                        */
/************************************************************************/
void GTMTrackLayer::WriteFeatureAttributes( OGRFeature *poFeature )
{
    void* pBuffer = NULL;
    char* psztrackname = NULL;
    int type = 1;
    unsigned int color = 0;
    for (int i = 0; i < poFeatureDefn->GetFieldCount(); ++i)
    {
        OGRFieldDefn *poFieldDefn = poFeatureDefn->GetFieldDefn( i );
        if( poFeature->IsFieldSet( i ) )
        {
            const char* pszName = poFieldDefn->GetNameRef();
            /* track name */
            if (strncmp(pszName, "name", 4) == 0)
            {
                psztrackname = CPLStrdup( poFeature->GetFieldAsString( i ) );
            }
            /* track type */
            else if (strncmp(pszName, "type", 4) == 0)
            {
                type = poFeature->GetFieldAsInteger( i );
                // Check if it is a valid type
                if (type < 1 || type > 30)
                    type = 1;
            }
            /* track color */
            else if (strncmp(pszName, "color", 5) == 0)
            {
                color = (unsigned int) poFeature->GetFieldAsInteger( i );
                if (color > 0xFFFFFF)
                    color = 0xFFFFFFF;
            }
        }
    }

    if (psztrackname == NULL)
        psztrackname = CPLStrdup( "" );

    int trackNameLength = 0;
    if (psztrackname != NULL)
        trackNameLength = strlen(psztrackname);

    int bufferSize = 14 + trackNameLength;
    pBuffer = CPLMalloc(bufferSize);
    void* pBufferAux = pBuffer;
    /* Write track string name size to buffer */
    appendUShort(pBufferAux, (unsigned short) trackNameLength);
    pBufferAux = (char*)pBufferAux + 2;

    /* Write track name */
    strncpy((char*)pBufferAux, psztrackname, trackNameLength);
    pBufferAux = (char*)pBufferAux + trackNameLength;

    /* Write track type */
    appendUChar(pBufferAux, (unsigned char) type);
    pBufferAux = (char*)pBufferAux + 1;
  
    /* Write track color */
    appendInt(pBufferAux, color);
    pBufferAux = (char*)pBufferAux + 4;
  
    /* Write track scale */
    appendFloat(pBufferAux, 0);
    pBufferAux = (char*)pBufferAux + 4;

    /* Write track label */
    appendUChar(pBufferAux, 0);
    pBufferAux = (char*)pBufferAux + 1;

    /* Write track layer */
    appendUShort(pBufferAux, 0);

    VSIFWriteL(pBuffer, bufferSize, 1, poDS->getTmpTracksFP());
    poDS->incNumTracks();

    if (psztrackname != NULL)
        CPLFree(psztrackname);
    CPLFree(pBuffer);
}

/************************************************************************/
/*                          WriteTrackpoint()                           */
/************************************************************************/
inline void GTMTrackLayer::WriteTrackpoint( double lat, double lon, float altitude, bool start )
{
    void* pBuffer = CPLMalloc(25);
    void* pBufferAux = pBuffer;
    //latitude
    appendDouble(pBufferAux, lat);
    pBufferAux = (char*)pBufferAux + 8; 
    //longitude
    appendDouble(pBufferAux, lon);
    pBufferAux = (char*)pBufferAux + 8; 
    //date
    appendInt(pBufferAux, 0);
    pBufferAux = (char*)pBufferAux + 4; 
    //start
    appendUChar(pBufferAux, start);
    pBufferAux = (char*)pBufferAux + 1; 
    //altitude
    appendFloat(pBufferAux, altitude);
    VSIFWriteL(pBuffer, 25, 1, poDS->getTmpTrackpointsFP());
    poDS->incNumTrackpoints();
    CPLFree(pBuffer);
}


/************************************************************************/
/*                           CreateFeature()                            */
/************************************************************************/
OGRErr GTMTrackLayer::CreateFeature (OGRFeature *poFeature)
{
    VSILFILE* fpTmpTrackpoints = poDS->getTmpTrackpointsFP();
    if (fpTmpTrackpoints == NULL)
        return CE_Failure;

    VSILFILE* fpTmpTracks = poDS->getTmpTracksFP();
    if (fpTmpTracks == NULL)
        return CE_Failure;

    OGRGeometry *poGeom = poFeature->GetGeometryRef();
    if ( poGeom == NULL )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Features without geometry not supported by GTM writer in track layer." );
        return OGRERR_FAILURE;
    }
   
    if (NULL != poCT)
    {
        poGeom = poGeom->clone();
        poGeom->transform( poCT );
    }
    
    switch( poGeom->getGeometryType() )
    {
    case wkbLineString:
    case wkbLineString25D:
    {
        WriteFeatureAttributes(poFeature);
        OGRLineString* line = (OGRLineString*)poGeom;
        for(int i = 0; i < line->getNumPoints(); ++i)
        {
            double lat = line->getY(i);
            double lon = line->getX(i);
            float altitude = 0;
            CheckAndFixCoordinatesValidity(lat, lon);
            poDS->checkBounds((float)lat, (float)lon);
            if (line->getGeometryType() == wkbLineString25D)
	      altitude = (float)line->getZ(i);
            WriteTrackpoint( lat, lon, altitude, i==0 );
        }
        break;
    }

    case wkbMultiLineString:
    case wkbMultiLineString25D:
    {
        int nGeometries = ((OGRGeometryCollection*)poGeom)->getNumGeometries ();
        for(int j = 0; j < nGeometries; ++j)
        {
            WriteFeatureAttributes(poFeature);
            OGRLineString* line = (OGRLineString*) ( ((OGRGeometryCollection*)poGeom)->getGeometryRef(j) );
            int n = (line) ? line->getNumPoints() : 0;
            for(int i = 0; i < n; ++i)
            {
                double lat = line->getY(i);
                double lon = line->getX(i);
                float altitude = 0;
                CheckAndFixCoordinatesValidity(lat, lon);
                if (line->getGeometryType() == wkbLineString25D)
		  altitude = (float) line->getZ(i);
                WriteTrackpoint( lat, lon, altitude, i==0 );
            }
        }
        break;
    }
    
    default:
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "Geometry type of `%s' not supported for 'track' element.\n",
                  OGRGeometryTypeToName(poGeom->getGeometryType()) );
        if (NULL != poCT)
            delete poGeom;
        return OGRERR_FAILURE;
    }
    }
    
    if (NULL != poCT)
        delete poGeom;

    return OGRERR_NONE;
}


OGRFeature* GTMTrackLayer::GetNextFeature()
{
    if (bError)
        return NULL;
        
    while (poDS->hasNextTrack())
    {
        Track* poTrack = poDS->fetchNextTrack();
        if (poTrack == NULL)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Could not read track. File probably corrupted");
            bError = TRUE;
            return NULL;
        }
        OGRFeature* poFeature = new OGRFeature( poFeatureDefn );
        OGRLineString* lineString = new OGRLineString ();
    
        for (int i = 0; i < poTrack->getNumPoints(); ++i)
        {
            const TrackPoint* psTrackPoint = poTrack->getPoint(i);
            lineString->addPoint(psTrackPoint->x, 
                                 psTrackPoint->y);
        }
        if (poSRS)
            lineString->assignSpatialReference(poSRS);
        poFeature->SetField( NAME, poTrack->getName());
        poFeature->SetField( TYPE, poTrack->getType());
        poFeature->SetField( COLOR, poTrack->getColor());
        poFeature->SetFID( nNextFID++ );
        delete poTrack;

        poFeature->SetGeometryDirectly(lineString);
        if( (m_poFilterGeom == NULL
             || FilterGeometry( poFeature->GetGeometryRef() ) )
            && (m_poAttrQuery == NULL
                || m_poAttrQuery->Evaluate( poFeature )) )
            return poFeature;

        delete poFeature;
    }
    return NULL;
}

int GTMTrackLayer::GetFeatureCount(int bForce)
{
    if (m_poFilterGeom == NULL && m_poAttrQuery == NULL)
        return poDS->getNTracks();
        
    return OGRLayer::GetFeatureCount(bForce);
}


void GTMTrackLayer::ResetReading()
{
    nNextFID = 0;
    poDS->rewindTrack();
}

