#include "ogr_geometry.h"
#include "../frmts/shapelib/shapefil.h"
#include <assert.h>

OGRGeometry *SHPReadOGRObject( SHPHandle hSHP, int iShape );

int main( int argc, char ** argv )

{
    SHPHandle	hSHP;
    int		nShapeCount, i;
    
    assert( argc == 2 );

    hSHP = SHPOpen( argv[1], "rb" );

    assert( hSHP != NULL );

    SHPGetInfo( hSHP, &nShapeCount, NULL, NULL, NULL );

    for( i = 0; i < nShapeCount; i++ )
    {
        OGRGeometry	*poShape;

        poShape = SHPReadOGRObject( hSHP, i );

        if( poShape != NULL )
            poShape->dumpReadable( stdout );

        delete poShape;
    }

    SHPClose( hSHP );
}
