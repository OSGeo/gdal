#include "ogr_cad.h"
#include "cpl_conv.h"

OGRCADDataSource::OGRCADDataSource()
{
    papoLayers = NULL;
    nLayers    = 0;
}

OGRCADDataSource::~OGRCADDataSource()
{
    for( int i = 0; i < nLayers; i++ )
        delete papoLayers[i];
    CPLFree( papoLayers );
}


int OGRCADDataSource::Open( const char *pszFilename, int bUpdate )
{
    if ( bUpdate )
    {
        CPLError( CE_Failure, CPLE_OpenFailed,
                 "Update access not supported by CAD Driver." );
        return( FALSE );
    }

    poCADFile = OpenCADFile( pszFilename, CADFile::OpenOptions::READ_ALL );

    if ( GetLastErrorCode() == CADErrorCodes::UNSUPPORTED_VERSION )
    {
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "libopencad v%s does not support this version of CAD file.", GetVersionString() );
        return( FALSE );
    }

    nLayers = 1;
    papoLayers = ( OGRCADLayer ** ) CPLMalloc(sizeof(void*));
    
    papoLayers[0] = new OGRCADLayer( pszFilename );
    
    return( TRUE );
}

OGRLayer *OGRCADDataSource::GetLayer( int iLayer )
{
    if ( iLayer < 0 || iLayer >= nLayers )
        return( NULL );
    else
        return papoLayers[iLayer];
}