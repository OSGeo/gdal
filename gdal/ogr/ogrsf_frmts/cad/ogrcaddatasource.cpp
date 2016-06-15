#include "ogr_cad.h"
#include "cpl_conv.h"

OGRCADDataSource::OGRCADDataSource()
{
    papoLayers = nullptr;
    nLayers    = 0;
}

OGRCADDataSource::~OGRCADDataSource()
{
    for( size_t i = 0; i < nLayers; i++ )
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

    spoCADFile = std::unique_ptr<CADFile>(
                                          OpenCADFile( pszFilename, CADFile::OpenOptions::READ_ALL ) );

    if ( GetLastErrorCode() == CADErrorCodes::UNSUPPORTED_VERSION )
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "libopencad %s does not support this version of CAD file.\n"
                  "Supported formats are:\n%s", GetVersionString(), GetCADFormats() );
        return( FALSE );
    }

    nLayers = spoCADFile->getLayersCount();
    papoLayers = ( OGRCADLayer** ) CPLMalloc( sizeof( void* ) );

    for ( size_t iIndex = 0; iIndex < nLayers; ++iIndex )
    {
        papoLayers[iIndex] = new OGRCADLayer( spoCADFile->getLayer( iIndex ) );
    }
    
    return( TRUE );
}

OGRLayer *OGRCADDataSource::GetLayer( int iLayer )
{
    if ( iLayer < 0 || iLayer >= nLayers )
        return( nullptr );
    else
        return papoLayers[iLayer];
}