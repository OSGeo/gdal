#include "ogr_kml.h"
#include "ogr_api.h"
#include "cpl_conv.h"
#include "cpl_port.h"
#include "cpl_string.h"

// Function utility to dump OGRGeoemtry to KML text
char *OGR_G_ExportToKML( OGRGeometryH hGeometry );

/************************************************************************/
/*                           OGRKMLLayer()                              */
/************************************************************************/
OGRKMLLayer::OGRKMLLayer( const char * pszName,
                          OGRSpatialReference *poSRSIn, int bWriterIn,
                          OGRwkbGeometryType eReqType,
                          OGRKMLDataSource *poDSIn )
{
    if( poSRSIn == NULL )
        poSRS = NULL;
    else
        poSRS = poSRSIn->Clone();
    
    iNextKMLId = 0;
    nTotalKMLCount = -1;
    
    poDS = poDSIn;
    
    poFeatureDefn = new OGRFeatureDefn( pszName );
    poFeatureDefn->Reference();
    poFeatureDefn->SetGeomType( eReqType );

    bWriter = bWriterIn;

    poFClass = NULL;
}

/************************************************************************/
/*                           ~OGRKMLLayer()                           */
/************************************************************************/
OGRKMLLayer::~OGRKMLLayer()
{
    if( poFeatureDefn )
        poFeatureDefn->Release();

    if( poSRS != NULL )
        poSRS->Release();
}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/
void OGRKMLLayer::ResetReading()
{
    iNextKMLId = 0;    
}

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/
OGRFeature *OGRKMLLayer::GetNextFeature()
{
	CPLError( CE_Failure, CPLE_AppDefined, 
              "OGRKMLLayer::GetExtent: KML driver is write-only!\n");

    return NULL;
}

/************************************************************************/
/*                          GetFeatureCount()                           */
/************************************************************************/
int OGRKMLLayer::GetFeatureCount( int bForce )
{
    if( poFClass == NULL )
        return 0;

    if( m_poFilterGeom != NULL || m_poAttrQuery != NULL )
        return OGRLayer::GetFeatureCount( bForce );
    else
        return poFClass->GetFeatureCount();
}

/************************************************************************/
/*                             GetExtent()                              */
/************************************************************************/
OGRErr OGRKMLLayer::GetExtent(OGREnvelope *psExtent, int bForce )
{
    CPLAssert( NULL != psExtent );

	CPLError( CE_Failure, CPLE_AppDefined, 
              "OGRKMLLayer::GetExtent: KML driver is write-only!\n");

    return OGRERR_NONE;	
}

/************************************************************************/
/*                           CreateFeature()                            */
/************************************************************************/
OGRErr OGRKMLLayer::CreateFeature( OGRFeature *poFeature )
{
    FILE        *fp = poDS->GetOutputFP();

    if( !bWriter )
        return OGRERR_FAILURE;

    VSIFPrintf( fp, "  <Placemark>\n" );

    if( poFeature->GetFID() == OGRNullFID )
        poFeature->SetFID( iNextKMLId++ );
    
    // Write out Geometry - for now it isn't indented properly.
    if( poFeature->GetGeometryRef() != NULL )
    {
        char    *pszGeometry;
        OGREnvelope sGeomBounds;

        // TODO - porting
        // pszGeometry = poFeature->GetGeometryRef()->exportToKML();
        pszGeometry = OGR_G_ExportToKML( static_cast<OGRGeometryH>( poFeature->GetGeometryRef() ) );
        
        VSIFPrintf( fp, "      %s\n", pszGeometry );
        CPLFree( pszGeometry );

        poFeature->GetGeometryRef()->getEnvelope( &sGeomBounds );
        poDS->GrowExtents( &sGeomBounds );
    }

	VSIFPrintf( fp, "    <description><![CDATA[\n" );
    // Write all "set" fields. 
    for( int iField = 0; iField < poFeatureDefn->GetFieldCount(); iField++ )
    {        
        OGRFieldDefn *poField = poFeatureDefn->GetFieldDefn( iField );

        if( poFeature->IsFieldSet( iField ) )
        {
            const char *pszRaw = poFeature->GetFieldAsString( iField );

            while( *pszRaw == ' ' )
                pszRaw++;

            char *pszEscaped = CPLEscapeString( pszRaw, -1, CPLES_XML );

            VSIFPrintf( fp, "      <b>%s:</b> <i>%s</i><br />\n", 
                        poField->GetNameRef(), pszEscaped);
            CPLFree( pszEscaped );
        }
    }
	VSIFPrintf( fp, "   ]]></description>" );
	
	if ( (wkbPolygon == poFeatureDefn->GetGeomType() ) ||
		 (wkbMultiPolygon == poFeatureDefn->GetGeomType() ) ||
		 (wkbLineString == poFeatureDefn->GetGeomType() ) ||
		 (wkbMultiLineString == poFeatureDefn->GetGeomType() ) )	
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
    if( EQUAL(pszCap,OLCSequentialWrite) )
        return bWriter;

    else if( EQUAL(pszCap,OLCCreateField) )
        return bWriter && iNextKMLId == 0;

    else if( EQUAL(pszCap,OLCFastGetExtent) )
    {
        double  dfXMin, dfXMax, dfYMin, dfYMax;

        if( poFClass == NULL )
            return FALSE;

        return poFClass->GetExtents( &dfXMin, &dfXMax, &dfYMin, &dfYMax );
    }

    else if( EQUAL(pszCap,OLCFastFeatureCount) )
    {
        if( poFClass == NULL 
            || m_poFilterGeom != NULL 
            || m_poAttrQuery != NULL )
            return FALSE;

        return poFClass->GetFeatureCount() != -1;
    }

    else 
        return FALSE;
}

/************************************************************************/
/*                            CreateField()                             */
/************************************************************************/
OGRErr OGRKMLLayer::CreateField( OGRFieldDefn *poField, int bApproxOK )
{
    if( !bWriter || iNextKMLId != 0 )
        return OGRERR_FAILURE;
		  
	OGRFieldDefn oCleanCopy( poField );
    poFeatureDefn->AddFieldDefn( &oCleanCopy );

    return OGRERR_NONE;
}

/************************************************************************/
/*                           GetSpatialRef()                            */
/************************************************************************/
OGRSpatialReference *OGRKMLLayer::GetSpatialRef()
{
    return poSRS;
}

