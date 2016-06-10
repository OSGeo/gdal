#include "ogr_cad.h"

/************************************************************************/
/*                           OGRCADDriverIdentify()                     */
/************************************************************************/

static int OGRCADDriverIdentify( GDALOpenInfo *poOpenInfo )
{
    if( poOpenInfo->fpL == NULL || poOpenInfo->nHeaderBytes == 0 )
        return FALSE;
    return EQUAL( CPLGetExtension(poOpenInfo->pszFilename), "dwg" );
}

/************************************************************************/
/*                           OGRCADDriverOpen()                         */
/************************************************************************/

static GDALDataset *OGRCADDriverOpen( GDALOpenInfo* poOpenInfo )
{
    if ( !OGRCADDriverIdentify ( poOpenInfo ) )
        return( NULL );
    
    OGRCADDataSource *poDS = new OGRCADDataSource();
    if( !poDS->Open( poOpenInfo->pszFilename, poOpenInfo->eAccess == GA_Update ) )
    {
        delete poDS;
        return( NULL );
    }
    else
        return( poDS );
}

/************************************************************************/
/*                           RegisterOGRCAD()                           */
/************************************************************************/

void RegisterOGRCAD()
{
    GDALDriver  *poDriver;
    
    if ( GDALGetDriverByName( "CAD" ) == NULL )
    {
        poDriver = new GDALDriver();
        poDriver->SetDescription( "CAD" );
        poDriver->SetMetadataItem( GDAL_DCAP_VECTOR, "YES" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME,
                                  "AutoCAD Driver" );
        poDriver->SetMetadataItem( GDAL_DMD_EXTENSION, "dwg" );
        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC,
                                  "drv_spf.html" );
        poDriver->pfnOpen = OGRCADDriverOpen;
        poDriver->pfnIdentify = OGRCADDriverIdentify;
//        poDriver->pfnCreate = OGRCADDriverCreate;
        poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );
        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}