#include "ogrsf_frmts.h"

int main( int argc, char ** argv )

{
    OGRDataSource *poDS;
    int         iLayer;

    RegisterOGRShape();

    poDS = OGRSFDriverRegistrar::Open( argv[1], FALSE );
    if( poDS == NULL )
        exit( 1 );

    printf( "Data Source: %s\n", poDS->GetName() );

    for( iLayer = 0; iLayer < poDS->GetLayerCount(); iLayer++ )
    {
        OGRLayer        *poLayer;
        OGRFeature      *poFeature;

        poLayer = poDS->GetLayer( iLayer );
        
        printf( "Layer Name: %s\n", poLayer->GetLayerDefn()->GetName() );
        printf( "Feature Count: %d\n", poLayer->GetFeatureCount() );

        while( (poFeature = poLayer->GetNextFeature()) != NULL )
        {
            poFeature->DumpReadable( stdout );
            delete poFeature;
        }
    }

    delete poDS;

    return 0;
}
