/******************************************************************************
 * $Id$
 *
 * Project:  KML Driver
 * Purpose:  Implementation of OGRKMLLayer class.
 * Author:   Christopher Condit, condit@sdsc.edu
 *           Jens Oberender, j.obi@troja.net
 *
 ******************************************************************************
 * Copyright (c) 2006, Christopher Condit
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
#include "ogr_kml.h"
#include "ogr_api.h"
#include "cpl_conv.h"
#include "cpl_port.h"
#include "cpl_string.h"

/* Function utility to dump OGRGeoemtry to KML text. */
char *OGR_G_ExportToKML( OGRGeometryH hGeometry, const char* pszAltitudeMode );

/************************************************************************/
/*                           OGRKMLLayer()                              */
/************************************************************************/

OGRKMLLayer::OGRKMLLayer( const char * pszName,
                          OGRSpatialReference *poSRSIn, int bWriterIn,
                          OGRwkbGeometryType eReqType,
                          OGRKMLDataSource *poDSIn )
{
    poSRS_ = NULL;
    if( poSRSIn != NULL )
        poSRS_ = poSRSIn->Clone();        
    
    iNextKMLId_ = 0;
    nTotalKMLCount_ = -1;
    
    poDS_ = poDSIn;
    
    poFeatureDefn_ = new OGRFeatureDefn( pszName );
    poFeatureDefn_->Reference();
    poFeatureDefn_->SetGeomType( eReqType );

    OGRFieldDefn oFieldName( "Name", OFTString );
    poFeatureDefn_->AddFieldDefn( &oFieldName );
    
    OGRFieldDefn oFieldDesc( "Description", OFTString );
    poFeatureDefn_->AddFieldDefn( &oFieldDesc );

    bWriter_ = bWriterIn;
}

/************************************************************************/
/*                           ~OGRKMLLayer()                             */
/************************************************************************/

OGRKMLLayer::~OGRKMLLayer()
{
    if( NULL != poFeatureDefn_ )
        poFeatureDefn_->Release();

    if( NULL != poSRS_ )
        poSRS_->Release();
}

/************************************************************************/
/*                            GetLayerDefn()                            */
/************************************************************************/

OGRFeatureDefn* OGRKMLLayer::GetLayerDefn()
{
    return poFeatureDefn_;
}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGRKMLLayer::ResetReading()
{
    iNextKMLId_ = 0;    
}

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature *OGRKMLLayer::GetNextFeature()
{
#ifndef HAVE_EXPAT
    return NULL;
#else
    /* -------------------------------------------------------------------- */
    /*      Loop till we find a feature matching our criteria.              */
    /* -------------------------------------------------------------------- */
    while(TRUE)
    {
        unsigned short nCount = 0;
        unsigned short nCount2 = 0;
        KML *poKMLFile = poDS_->GetKMLFile();
        poKMLFile->selectLayer(nLayerNumber_);
    
        Feature *poFeatureKML = NULL;
        poFeatureKML = poKMLFile->getFeature(iNextKMLId_++);
    
        if(poFeatureKML == NULL)
            return NULL;
    
        CPLAssert( poFeatureKML != NULL );
    
        OGRFeature *poFeature = new OGRFeature( poFeatureDefn_ );
        
        // Handle a Point
        if(poFeatureKML->eType == Point)
        {
            poFeature->SetGeometryDirectly(
                    new OGRPoint( poFeatureKML->pvpsCoordinates->at(0)->dfLongitude, poFeatureKML->pvpsCoordinates->at(0)->dfLatitude, poFeatureKML->pvpsCoordinates->at(0)->dfAltitude)
                    );
        }
        // Handle a LineString
        else if(poFeatureKML->eType == LineString)
        {
            OGRLineString *poLS = new OGRLineString();
            for(nCount = 0; nCount < poFeatureKML->pvpsCoordinates->size(); nCount++)
            {
                poLS->addPoint(poFeatureKML->pvpsCoordinates->at(nCount)->dfLongitude, poFeatureKML->pvpsCoordinates->at(nCount)->dfLatitude, poFeatureKML->pvpsCoordinates->at(nCount)->dfAltitude);
            }
            poFeature->SetGeometryDirectly(poLS);
        }
        // Handle a Polygon
        else if(poFeatureKML->eType == Polygon)
        {
            OGRPolygon *poPG = new OGRPolygon();
            OGRLinearRing *poLR = new OGRLinearRing();
            for(nCount = 0; nCount < poFeatureKML->pvpsCoordinates->size(); nCount++)
            {
                poLR->addPoint(poFeatureKML->pvpsCoordinates->at(nCount)->dfLongitude, poFeatureKML->pvpsCoordinates->at(nCount)->dfLatitude, poFeatureKML->pvpsCoordinates->at(nCount)->dfAltitude);
            }
            poPG->addRingDirectly(poLR);
            for(nCount = 0; nCount < poFeatureKML->pvpsCoordinatesExtra->size(); nCount++)
            {
                poLR = new OGRLinearRing();
                for(nCount2 = 0; nCount2 < poFeatureKML->pvpsCoordinatesExtra->at(nCount)->size(); nCount2++)
                {
                    poLR->addPoint(poFeatureKML->pvpsCoordinatesExtra->at(nCount)->at(nCount2)->dfLongitude, 
                        poFeatureKML->pvpsCoordinatesExtra->at(nCount)->at(nCount2)->dfLatitude, 
                        poFeatureKML->pvpsCoordinatesExtra->at(nCount)->at(nCount2)->dfAltitude);
                }
                poPG->addRingDirectly(poLR);
            }
            poFeature->SetGeometryDirectly(poPG);
        }
    
        // Add fields
        poFeature->SetField( poFeatureDefn_->GetFieldIndex("Name"), poFeatureKML->sName.c_str() );
        poFeature->SetField( poFeatureDefn_->GetFieldIndex("Description"), poFeatureKML->sDescription.c_str() );
        poFeature->SetFID( iNextKMLId_ - 1 );
    
        // Clean up
        for(nCount = 0; nCount < poFeatureKML->pvpsCoordinates->size(); nCount++)
        {
            delete poFeatureKML->pvpsCoordinates->at(nCount);
        }
    
        if(poFeatureKML->pvpsCoordinatesExtra != NULL)
        {
            for(nCount = 0; nCount < poFeatureKML->pvpsCoordinatesExtra->size(); nCount++)
            {
                for(nCount2 = 0; nCount2 < poFeatureKML->pvpsCoordinatesExtra->at(nCount)->size(); nCount2++)
                    delete poFeatureKML->pvpsCoordinatesExtra->at(nCount)->at(nCount2);
                delete poFeatureKML->pvpsCoordinatesExtra->at(nCount);
            }
            delete poFeatureKML->pvpsCoordinatesExtra;
        }
    
        delete poFeatureKML->pvpsCoordinates;
        delete poFeatureKML;
    
        if( poFeature->GetGeometryRef() != NULL && poSRS_ != NULL)
        {
            poFeature->GetGeometryRef()->assignSpatialReference( poSRS_ );
        }
    
        /* Check spatial/attribute filters */
        if ((m_poFilterGeom == NULL || FilterGeometry( poFeature->GetGeometryRef() ) ) &&
            (m_poAttrQuery == NULL || m_poAttrQuery->Evaluate( poFeature )) )
        {
        // Return the feature
            return poFeature;
        }
        else
        {
            delete poFeature;
        }
    }

#endif /* HAVE_EXPAT */
}

/************************************************************************/
/*                          GetFeatureCount()                           */
/************************************************************************/

/* Disabled as it doesn't take into account spatial filters */
/*
int OGRKMLLayer::GetFeatureCount( int bForce )
{
    int nCount = 0;

#ifdef HAVE_EXPAT
    KML *poKMLFile = poDS_->GetKMLFile();
    if( NULL != poKMLFile )
    {
        poKMLFile->selectLayer(nLayerNumber_);
        nCount = poKMLFile->getNumFeatures();
    }
#endif

    return nCount;
}
*/

/************************************************************************/
/*                             GetExtent()                              */
/************************************************************************/

OGRErr OGRKMLLayer::GetExtent(OGREnvelope *psExtent, int bForce )
{
#ifdef HAVE_EXPAT
    CPLAssert( NULL != psExtent );

    KML *poKMLFile = poDS_->GetKMLFile();
    if( NULL != poKMLFile )
    {
        poKMLFile->selectLayer(nLayerNumber_);
        if( poKMLFile->getExtents( psExtent->MinX, psExtent->MaxX,
                                   psExtent->MinY, psExtent->MaxY ) )
        {
            return OGRERR_NONE;
        }
    }
#endif

    return OGRERR_FAILURE;
}

/************************************************************************/
/*                           CreateFeature()                            */
/************************************************************************/

OGRErr OGRKMLLayer::CreateFeature( OGRFeature* poFeature )
{
    CPLAssert( NULL != poFeature );
    CPLAssert( NULL != poDS_ );

    if( !bWriter_ )
        return OGRERR_FAILURE;

    FILE *fp = poDS_->GetOutputFP();
    CPLAssert( NULL != fp );

    VSIFPrintf( fp, "  <Placemark>\n" );

    if( poFeature->GetFID() == OGRNullFID )
        poFeature->SetFID( iNextKMLId_++ );

    // First find and write the name element
    if (NULL != poDS_->GetNameField())
    {
        for( int iField = 0; iField < poFeatureDefn_->GetFieldCount(); iField++ )
        {        
            OGRFieldDefn *poField = poFeatureDefn_->GetFieldDefn( iField );

            if( poFeature->IsFieldSet( iField )
                && EQUAL(poField->GetNameRef(), poDS_->GetNameField()) )
            {           
                const char *pszRaw = poFeature->GetFieldAsString( iField );
                while( *pszRaw == ' ' )
                    pszRaw++;

                char *pszEscaped = CPLEscapeString( pszRaw, -1, CPLES_XML );

                VSIFPrintf( fp, "      <name>%s</name>\n", pszEscaped);
                CPLFree( pszEscaped );   
            }    
        }
    }
        
    VSIFPrintf( fp, "      <description>");

    if (NULL != poDS_->GetDescriptionField())
    {
        for( int iField = 0; iField < poFeatureDefn_->GetFieldCount(); iField++ )
        {        
            OGRFieldDefn *poField = poFeatureDefn_->GetFieldDefn( iField );

            if( poFeature->IsFieldSet( iField )
                && EQUAL(poField->GetNameRef(), poDS_->GetDescriptionField()) )
            {           
                const char *pszRaw = poFeature->GetFieldAsString( iField );
                while( *pszRaw == ' ' )
                    pszRaw++;

                char *pszEscaped = CPLEscapeString( pszRaw, -1, CPLES_XML );

                VSIFPrintf( fp, "%s", pszEscaped);
                CPLFree( pszEscaped );   
            }    
        }
    }
    
    int bHasFoundOtherField = FALSE;

    // Write all "set" fields that aren't being used for the name element
    for( int iField = 0; iField < poFeatureDefn_->GetFieldCount(); iField++ )
    {        
        OGRFieldDefn *poField = poFeatureDefn_->GetFieldDefn( iField );

        if( poFeature->IsFieldSet( iField ) && 
            (NULL == poDS_->GetNameField() || !EQUAL(poField->GetNameRef(), poDS_->GetNameField())) &&
            (NULL == poDS_->GetDescriptionField() || !EQUAL(poField->GetNameRef(), poDS_->GetDescriptionField())))
        {
            if (!bHasFoundOtherField)
            {
                VSIFPrintf( fp, "\n<![CDATA[\n" );
                bHasFoundOtherField = TRUE;
            }
            const char *pszRaw = poFeature->GetFieldAsString( iField );

            while( *pszRaw == ' ' )
                pszRaw++;

            char *pszEscaped = CPLEscapeString( pszRaw, -1, CPLES_XML );

            VSIFPrintf( fp, "      <b>%s:</b> <i>%s</i><br />\n", 
                        poField->GetNameRef(), pszEscaped);
            CPLFree( pszEscaped );
        }
    }

    if (bHasFoundOtherField)
    {
        VSIFPrintf( fp, "]]>" );
    }

    VSIFPrintf( fp, "</description>\n" );
	
    // Write out Geometry - for now it isn't indented properly.
    if( poFeature->GetGeometryRef() != NULL )
    {
        char* pszGeometry = NULL;
        OGREnvelope sGeomBounds;

        // TODO - porting
        // pszGeometry = poFeature->GetGeometryRef()->exportToKML();
        pszGeometry = 
            OGR_G_ExportToKML( static_cast<OGRGeometryH>( poFeature->GetGeometryRef() ),
                               poDS_->GetAltitudeMode());
        
        VSIFPrintf( fp, "      %s\n", pszGeometry );
        CPLFree( pszGeometry );

        poFeature->GetGeometryRef()->getEnvelope( &sGeomBounds );
        poDS_->GrowExtents( &sGeomBounds );
    }
    
    if ( wkbPolygon == poFeatureDefn_->GetGeomType()
         || wkbMultiPolygon == poFeatureDefn_->GetGeomType()
         || wkbLineString == poFeatureDefn_->GetGeomType()
         || wkbMultiLineString == poFeatureDefn_->GetGeomType() )
    {
        //If we're dealing with a polygon, add a line style that will stand out a bit
        VSIFPrintf( fp, "  <Style><LineStyle><color>ff0000ff</color></LineStyle>");
        VSIFPrintf( fp, "  <PolyStyle><fill>0</fill></PolyStyle></Style>\n" );
    }

    VSIFPrintf( fp, "  </Placemark>\n" );

    return OGRERR_NONE;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRKMLLayer::TestCapability( const char * pszCap )
{
    if( EQUAL(pszCap, OLCSequentialWrite) )
    {
        return bWriter_;
    }
    else if( EQUAL(pszCap, OLCCreateField) )
    {
        return bWriter_ && iNextKMLId_ == 0;
    }
    else if( EQUAL(pszCap, OLCFastGetExtent) )
    {
//        double  dfXMin, dfXMax, dfYMin, dfYMax;
//        if( poFClass == NULL )
            return FALSE;

//        return poFClass->GetExtents( &dfXMin, &dfXMax, &dfYMin, &dfYMax );
    }
    else if( EQUAL(pszCap,OLCFastFeatureCount) )
    {
//        if( poFClass == NULL 
//            || m_poFilterGeom != NULL 
//            || m_poAttrQuery != NULL )
            return FALSE;

//        return poFClass->GetFeatureCount() != -1;
    }

    return FALSE;
}

/************************************************************************/
/*                            CreateField()                             */
/************************************************************************/

OGRErr OGRKMLLayer::CreateField( OGRFieldDefn *poField, int bApproxOK )
{
    if( !bWriter_ || iNextKMLId_ != 0 )
        return OGRERR_FAILURE;
		  
	OGRFieldDefn oCleanCopy( poField );
    poFeatureDefn_->AddFieldDefn( &oCleanCopy );

    return OGRERR_NONE;
}

/************************************************************************/
/*                           GetSpatialRef()                            */
/************************************************************************/

OGRSpatialReference *OGRKMLLayer::GetSpatialRef()
{
    return poSRS_;
}

/************************************************************************/
/*                           SetLayerNumber()                           */
/************************************************************************/

void OGRKMLLayer::SetLayerNumber( int nLayer )
{
    nLayerNumber_ = nLayer;
}

