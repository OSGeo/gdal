#include "ogr_kml.h"
#include "cpl_conv.h"
#include "cpl_error.h"

/************************************************************************/
/*                          ~OGRGMLDriver()                           */
/************************************************************************/
OGRKMLDriver::~OGRKMLDriver()
{
}

/************************************************************************/
/*                              GetName()                               */
/************************************************************************/
const char *OGRKMLDriver::GetName()
{
    return "KML";
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/
OGRDataSource *OGRKMLDriver::Open( const char * pszFilename,
                                   int bUpdate )
{
    CPLAssert( NULL != pszFilename );
    CPLDebug( "KML", "Attempt to open: %s", pszFilename );
    
    OGRKMLDataSource    *poDS = NULL;

    if( bUpdate )
        return NULL;

    poDS = new OGRKMLDataSource();

    if( !poDS->Open( pszFilename, TRUE )
        || poDS->GetLayerCount() == 0 )
    {
        delete poDS;
        return NULL;
    }
    
    return poDS;
}

/************************************************************************/
/*                          CreateDataSource()                          */
/************************************************************************/
OGRDataSource *OGRKMLDriver::CreateDataSource( const char * pszName,
                                               char **papszOptions )
{
    CPLAssert( NULL != pszName );
    CPLDebug( "KML", "Attempt to create: %s", pszName );
    
    OGRKMLDataSource *poDS = new OGRKMLDataSource();

    if( !poDS->Create( pszName, papszOptions ) )
    {
        delete poDS;
        return NULL;
    }
    else
        return poDS;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/
int OGRKMLDriver::TestCapability( const char * pszCap )
{
    if( EQUAL(pszCap,ODrCCreateDataSource) )
        return TRUE;
    else
        return FALSE;
}

/************************************************************************/
/*                           RegisterOGRGML()                           */
/************************************************************************/
void RegisterOGRKML()
{
    OGRSFDriverRegistrar::GetRegistrar()->RegisterDriver( new OGRKMLDriver );
}

