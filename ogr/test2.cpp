#include "ogr_feature.h"
#include "../frmts/shapelib/shapefil.h"
#include <assert.h>

#include "dbmalloc.h"

OGRGeometry *SHPReadOGRObject( SHPHandle hSHP, int iShape );
OGRFeatureDefn *SHPReadOGRFeatureDefn( SHPHandle hSHP, DBFHandle hDBF );
OGRFeature *SHPReadOGRFeature( SHPHandle hSHP, DBFHandle hDBF,
                               OGRFeatureDefn * poDefn, int iShape );

int main( int argc, char ** argv )

{
    SHPHandle	hSHP;
    DBFHandle   hDBF;
    int		nShapeCount, i;
    OGRFeatureDefn *poDefn;
    
    assert( argc == 2 );

    hSHP = SHPOpen( argv[1], "rb" );
    hDBF = DBFOpen( argv[1], "rb" );

    assert( hSHP != NULL && hDBF != NULL );

    SHPGetInfo( hSHP, &nShapeCount, NULL, NULL, NULL );

    poDefn = SHPReadOGRFeatureDefn( hSHP, hDBF );

    for( i = 0; i < nShapeCount; i++ )
    {
        OGRFeature	*poFeature;

        poFeature = SHPReadOGRFeature( hSHP, hDBF, poDefn, i );

        if( poFeature != NULL )
            poFeature->DumpReadable( stdout );

        delete poFeature;
    }

    delete poDefn;

    DBFClose( hDBF );
    SHPClose( hSHP );

    malloc_dump(2);
}
