#include "gmlreader.h"


/************************************************************************/
/*                               Usage()                                */
/************************************************************************/
static void Usage()

{
    printf( "Usage: gmlview [gmlfile]\n" );
    exit( 1 );
}

int main( int nArgc, char **papszArgv )

{
    IGMLReader	*poReader;
    GMLFeature  *poFeature;

    if( nArgc < 2 )
        Usage();

    poReader = CreateGMLReader();
    poReader->SetSourceFile( papszArgv[1] );

    while( (poFeature = poReader->NextFeature()) != NULL )
    {
        poFeature->Dump( stdout );
        delete poFeature;
    }

    delete poReader;
}
