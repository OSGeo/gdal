#include "ogr_kml.h"
#include "cpl_conv.h"
#include "cpl_string.h"
#include "cpl_error.h"

/************************************************************************/
/*                         OGRKMLDataSource()                         */
/************************************************************************/
OGRKMLDataSource::OGRKMLDataSource()
{
    pszName = NULL;
    papoLayers = NULL;
    nLayers = 0;
    
    fpOutput = NULL;

    papszCreateOptions = NULL;
}

/************************************************************************/
/*                        ~OGRKMLDataSource()                         */
/************************************************************************/
OGRKMLDataSource::~OGRKMLDataSource()
{
    if( fpOutput != NULL )
    {
        VSIFPrintf( fpOutput, "%s", 
                    "</Folder></kml>\n" );
        
        if( fpOutput != stdout )
            VSIFClose( fpOutput );
    }

    CSLDestroy( papszCreateOptions );
    CPLFree( pszName );

    for( int i = 0; i < nLayers; i++ )
    {
        delete papoLayers[i];
    }
    
    CPLFree( papoLayers );
    
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/
int OGRKMLDataSource::Open( const char * pszNewName, int bTestOpen )
{
    FILE        *fp;
    char        szHeader[1000];	
	
    CPLAssert( NULL != pszNewName );

/* -------------------------------------------------------------------- */
/*      Open the source file.                                           */
/* -------------------------------------------------------------------- */
    fp = VSIFOpen( pszNewName, "r" );
    if( fp == NULL )
    {
        if( !bTestOpen )
            CPLError( CE_Failure, CPLE_OpenFailed, 
                      "Failed to open KML file `%s'.", 
                      pszNewName );

        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      If we aren't sure it is KML, load a header chunk and check      */
/*      for signs it is KML                                             */
/* -------------------------------------------------------------------- */
    if( bTestOpen )
    {
        VSIFRead( szHeader, 1, sizeof(szHeader), fp );
        szHeader[sizeof(szHeader)-1] = '\0';
			
        if( szHeader[0] != '<' 
            || strstr(szHeader, "http://earth.google.com/kml/2.0") == NULL )
        {			
            VSIFClose( fp );
            return FALSE;
        }
    }
    
	VSIFClose( fp );
	
	CPLError( CE_Failure, CPLE_AppDefined, 
              "Reading KML files is not currently supported\n");

    return TRUE;	
}

/************************************************************************/
/*                         TranslateKMLSchema()                         */
/************************************************************************/
OGRKMLLayer *OGRKMLDataSource::TranslateKMLSchema( KMLFeatureClass *poClass )
{
    CPLAssert( NULL != poClass );

    OGRKMLLayer *poLayer;
    poLayer = new OGRKMLLayer( poClass->GetName(), NULL, FALSE, 
                               wkbUnknown, this );

    return poLayer;
}

/************************************************************************/
/*                               Create()                               */
/************************************************************************/
int OGRKMLDataSource::Create( const char *pszFilename, 
                              char **papszOptions )
{
    CPLAssert( NULL != pszFilename );

    if( fpOutput != NULL )
    {
        CPLAssert( FALSE );
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Create the output file.                                         */
/* -------------------------------------------------------------------- */
    pszName = CPLStrdup( pszFilename );

    if( EQUAL(pszFilename,"stdout") )
        fpOutput = stdout;
    else
        fpOutput = VSIFOpen( pszFilename, "wt+" );

    if( fpOutput == NULL )
    {
        CPLError( CE_Failure, CPLE_OpenFailed, 
                  "Failed to create KML file %s.", 
                  pszFilename );
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Write out "standard" header.                                    */
/* -------------------------------------------------------------------- */
    VSIFPrintf( fpOutput, "%s", 
                "<?xml version=\"1.0\" encoding=\"utf-8\" ?>\n" );	

    nSchemaInsertLocation = VSIFTell( fpOutput );

    VSIFPrintf( fpOutput, "%s", 
                "<kml xmlns=\"http://earth.google.com/kml/2.0\">\n<Folder>" );

    return TRUE;
}

/************************************************************************/
/*                            CreateLayer()                             */
/************************************************************************/

OGRLayer *
OGRKMLDataSource::CreateLayer( const char * pszLayerName,
                               OGRSpatialReference *poSRS,
                               OGRwkbGeometryType eType,
                               char ** papszOptions )
{
    CPLAssert( NULL != pszLayerName);

/* -------------------------------------------------------------------- */
/*      Verify we are in update mode.                                   */
/* -------------------------------------------------------------------- */
    if( fpOutput == NULL )
    {
        CPLError( CE_Failure, CPLE_NoWriteAccess,
                  "Data source %s opened for read access.\n"
                  "New layer %s cannot be created.\n",
                  pszName, pszLayerName );

        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Ensure name is safe as an element name.                         */
/* -------------------------------------------------------------------- */
    char *pszCleanLayerName = CPLStrdup( pszLayerName );

    CPLCleanXMLElementName( pszCleanLayerName );
    if( strcmp(pszCleanLayerName, pszLayerName) != 0 )
    {
        CPLError( CE_Warning, CPLE_AppDefined, 
                  "Layer name '%s' adjusted to '%s' for XML validity.",
                  pszLayerName, pszCleanLayerName );
    }

/* -------------------------------------------------------------------- */
/*      Create the layer object.                                        */
/* -------------------------------------------------------------------- */
    OGRKMLLayer *poLayer;

    poLayer = new OGRKMLLayer( pszCleanLayerName, poSRS, TRUE, eType, this );

    CPLFree( pszCleanLayerName );

/* -------------------------------------------------------------------- */
/*      Add layer to data source layer list.                            */
/* -------------------------------------------------------------------- */
    papoLayers = (OGRKMLLayer **)
        CPLRealloc( papoLayers,  sizeof(OGRKMLLayer *) * (nLayers+1) );
    
    papoLayers[nLayers++] = poLayer;

    return poLayer;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/
int OGRKMLDataSource::TestCapability( const char * pszCap )

{
    if( EQUAL(pszCap,ODsCCreateLayer) )
        return TRUE;
    else
        return FALSE;
}

/************************************************************************/
/*                              GetLayer()                              */
/************************************************************************/
OGRLayer *OGRKMLDataSource::GetLayer( int iLayer )
{
    if( iLayer < 0 || iLayer >= nLayers )
        return NULL;
    else
        return papoLayers[iLayer];
}

/************************************************************************/
/*                            GrowExtents()                             */
/************************************************************************/
void OGRKMLDataSource::GrowExtents( OGREnvelope *psGeomBounds )
{
    CPLAssert( NULL != psGeomBounds );

    sBoundingRect.Merge( *psGeomBounds );
}

/************************************************************************/
/*                            InsertHeader()                            */
/*                                                                      */
/*      This method is used to update boundedby info for a              */
/*      dataset, and insert schema descriptions depending on            */
/*      selection options in effect.                                    */
/************************************************************************/
void OGRKMLDataSource::InsertHeader()
{    
        return;
}
