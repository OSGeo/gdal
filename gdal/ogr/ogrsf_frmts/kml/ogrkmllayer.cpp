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
#include "cpl_string.h"
#include "ogr_p.h"

/* Function utility to dump OGRGeometry to KML text. */
char *OGR_G_ExportToKML( OGRGeometryH hGeometry, const char* pszAltitudeMode );

/************************************************************************/
/*                           OGRKMLLayer()                              */
/************************************************************************/

OGRKMLLayer::OGRKMLLayer( const char * pszName,
                          OGRSpatialReference *poSRSIn, int bWriterIn,
                          OGRwkbGeometryType eReqType,
                          OGRKMLDataSource *poDSIn )
{    	
    poCT_ = NULL;

    /* KML should be created as WGS84. */
    if( poSRSIn != NULL )
    {
        poSRS_ = new OGRSpatialReference(NULL);   
        poSRS_->SetWellKnownGeogCS( "WGS84" );
        if (!poSRS_->IsSame(poSRSIn))
        {
            poCT_ = OGRCreateCoordinateTransformation( poSRSIn, poSRS_ );
            if( poCT_ == NULL && poDSIn->IsFirstCTError() )
            {
                /* If we can't create a transformation, issue a warning - but continue the transformation*/
                char *pszWKT = NULL;

                poSRSIn->exportToPrettyWkt( &pszWKT, FALSE );

                CPLError( CE_Warning, CPLE_AppDefined,
                        "Failed to create coordinate transformation between the\n"
                        "input coordinate system and WGS84.  This may be because they\n"
                        "are not transformable, or because projection services\n"
                        "(PROJ.4 DLL/.so) could not be loaded.\n" 
                        "KML geometries may not render correctly.\n"
                        "This message will not be issued any more. \n"
                        "\nSource:\n%s\n", 
                        pszWKT );

                CPLFree( pszWKT );
                poDSIn->IssuedFirstCTError(); 
            }
        }
    }
    else
    {
        poSRS_ = NULL;
    }

    iNextKMLId_ = 0;
    nTotalKMLCount_ = -1;
    nLastAsked = -1;
    nLastCount = -1;

    poDS_ = poDSIn;
    
    poFeatureDefn_ = new OGRFeatureDefn( pszName );
    poFeatureDefn_->Reference();
    poFeatureDefn_->SetGeomType( eReqType );

    OGRFieldDefn oFieldName( "Name", OFTString );
    poFeatureDefn_->AddFieldDefn( &oFieldName );
    
    OGRFieldDefn oFieldDesc( "Description", OFTString );
    poFeatureDefn_->AddFieldDefn( &oFieldDesc );

    bWriter_ = bWriterIn;
    nWroteFeatureCount_ = 0;

    pszName_ = CPLStrdup(pszName);
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
	
    if( NULL != poCT_ )
        delete poCT_;
	
    CPLFree( pszName_ );
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
    nLastAsked = -1;
    nLastCount = -1;
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
    KML *poKMLFile = poDS_->GetKMLFile();
    poKMLFile->selectLayer(nLayerNumber_);

    while(TRUE)
    {
        Feature *poFeatureKML = NULL;
        poFeatureKML = poKMLFile->getFeature(iNextKMLId_++, nLastAsked, nLastCount);
    
        if(poFeatureKML == NULL)
            return NULL;
    
        CPLAssert( poFeatureKML != NULL );
    
        OGRFeature *poFeature = new OGRFeature( poFeatureDefn_ );
        
        if(poFeatureKML->poGeom)
        {
            poFeature->SetGeometryDirectly(poFeatureKML->poGeom);
            poFeatureKML->poGeom = NULL;
        }
    
        // Add fields
        poFeature->SetField( poFeatureDefn_->GetFieldIndex("Name"), poFeatureKML->sName.c_str() );
        poFeature->SetField( poFeatureDefn_->GetFieldIndex("Description"), poFeatureKML->sDescription.c_str() );
        poFeature->SetFID( iNextKMLId_ - 1 );
    
        // Clean up
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

int OGRKMLLayer::GetFeatureCount( int bForce )
{
    int nCount = 0;

#ifdef HAVE_EXPAT
    if (m_poFilterGeom == NULL && m_poAttrQuery == NULL)
    {
        KML *poKMLFile = poDS_->GetKMLFile();
        if( NULL != poKMLFile )
        {
            poKMLFile->selectLayer(nLayerNumber_);
            nCount = poKMLFile->getNumFeatures();
        }
    }
    else
        return OGRLayer::GetFeatureCount(bForce);
#endif

    return nCount;
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

    // If we haven't writen any features yet, output the layer's schema
    if (0 == nWroteFeatureCount_)
    {
        VSIFPrintf( fp, "<Schema name=\"%s\" id=\"%s\">\n", pszName_, pszName_ );
        OGRFeatureDefn *featureDefinition = GetLayerDefn();
        for (int j=0; j < featureDefinition->GetFieldCount(); j++)
        {
            OGRFieldDefn *fieldDefinition = featureDefinition->GetFieldDefn(j);			
            const char* pszKMLType = NULL;
            const char* pszKMLEltName = NULL;
            // Match the OGR type to the GDAL type
            switch (fieldDefinition->GetType())
            {
              case OFTInteger:
                pszKMLType = "int";
                pszKMLEltName = "SimpleField";
                break;
              case OFTIntegerList:
                pszKMLType = "int";
                pszKMLEltName = "SimpleArrayField";
                break;
              case OFTReal:
                pszKMLType = "float";
                pszKMLEltName = "SimpleField";
                break;
              case OFTRealList:
                pszKMLType = "float";
                pszKMLEltName = "SimpleArrayField";
                break;
              case OFTString:
                pszKMLType = "string";
                pszKMLEltName = "SimpleField";
                break;
              case OFTStringList:
                pszKMLType = "string";
                pszKMLEltName = "SimpleArrayField";
                break;
              case OFTBinary:
                pszKMLType = "bool";
                pszKMLEltName = "SimpleField";
                break;
                //TODO: KML doesn't handle these data types yet...
              case OFTDate:                
              case OFTTime:                
              case OFTDateTime:
                pszKMLType = "string";
                pszKMLEltName = "SimpleField";                
                break;

              default:
                pszKMLType = "string";
                pszKMLEltName = "SimpleField";
                break;
            }
            VSIFPrintf( fp, "\t<%s name=\"%s\" type=\"%s\"></%s>\n", 
                        pszKMLEltName, fieldDefinition->GetNameRef() ,pszKMLType, pszKMLEltName );
        }
        VSIFPrintf( fp, "</Schema>\n" );
    }

    VSIFPrintf( fp, "  <Placemark>\n" );

    if( poFeature->GetFID() == OGRNullFID )
        poFeature->SetFID( iNextKMLId_++ );

    // Find and write the name element
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

                char *pszEscaped = OGRGetXML_UTF8_EscapedString( pszRaw );

                VSIFPrintf( fp, "\t<name>%s</name>\n", pszEscaped);
                CPLFree( pszEscaped );
            }
        }
    }

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

                char *pszEscaped = OGRGetXML_UTF8_EscapedString( pszRaw );

                VSIFPrintf( fp, "\t<description>%s</description>\n", pszEscaped);
                CPLFree( pszEscaped );
            }
        }
    }

    OGRwkbGeometryType eGeomType = wkbFlatten(poFeatureDefn_->GetGeomType());
    if ( wkbPolygon == eGeomType
         || wkbMultiPolygon == eGeomType
         || wkbLineString == eGeomType
         || wkbMultiLineString == eGeomType )
    {
        //If we're dealing with a polygon, add a line style that will stand out a bit
        VSIFPrintf( fp, "  <Style><LineStyle><color>ff0000ff</color></LineStyle>");
        VSIFPrintf( fp, "  <PolyStyle><fill>0</fill></PolyStyle></Style>\n" );
    }

    int bHasFoundOtherField = FALSE;

    // Write all fields as SchemaData
    for( int iField = 0; iField < poFeatureDefn_->GetFieldCount(); iField++ )
    {
        OGRFieldDefn *poField = poFeatureDefn_->GetFieldDefn( iField );

        if( poFeature->IsFieldSet( iField ))
        {
            if (!bHasFoundOtherField)
            {                
                VSIFPrintf( fp, "\t<ExtendedData><SchemaData schemaUrl=\"#%s\">\n", pszName_ );
                bHasFoundOtherField = TRUE;
            }
            const char *pszRaw = poFeature->GetFieldAsString( iField );

            while( *pszRaw == ' ' )
                pszRaw++;

            char *pszEscaped = OGRGetXML_UTF8_EscapedString( pszRaw );

            VSIFPrintf( fp, "\t\t<SimpleData name=\"%s\">%s</SimpleData>\n", 
                        poField->GetNameRef(), pszEscaped);

            CPLFree( pszEscaped );
        }
    }

    if (bHasFoundOtherField)
    {
        VSIFPrintf( fp, "\t</SchemaData></ExtendedData>\n" );
    }

    // Write out Geometry - for now it isn't indented properly.
    if( poFeature->GetGeometryRef() != NULL )
    {
        char* pszGeometry = NULL;
        OGREnvelope sGeomBounds;
        OGRGeometry* poWGS84Geom;	

        if (NULL != poCT_)
        {
            poWGS84Geom = poFeature->GetGeometryRef()->clone();
            poWGS84Geom->transform( poCT_ );
        }
        else
        {
            poWGS84Geom = poFeature->GetGeometryRef();
        }
	
        // TODO - porting
        // pszGeometry = poFeature->GetGeometryRef()->exportToKML();
        pszGeometry = 
            OGR_G_ExportToKML( (OGRGeometryH)poWGS84Geom,
                               poDS_->GetAltitudeMode());
        
        VSIFPrintf( fp, "      %s\n", pszGeometry );
        CPLFree( pszGeometry );

        poWGS84Geom->getEnvelope( &sGeomBounds );
        poDS_->GrowExtents( &sGeomBounds );

        if (NULL != poCT_)
        {
            delete poWGS84Geom;
        }
    }
    
    VSIFPrintf( fp, "  </Placemark>\n" );
    nWroteFeatureCount_++;
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

