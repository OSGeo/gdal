#include "gmlreader.h"
#include "ogr_gml.h"


/************************************************************************/
/*                               Usage()                                */
/************************************************************************/
static void Usage()

{
    printf( "Usage: gmlview [-nodump] [-si schemafile] gmlfile [-so schemafile]\n" );
    exit( 1 );
}

/************************************************************************/
/*                              DumpFile()                              */
/************************************************************************/

static void DumpFile( IGMLReader *poReader, int bNoDump )

{
    GMLFeature  *poFeature;
    
    while( (poFeature = poReader->NextFeature()) != NULL )
    {
        OGRGeometry *poGeometry;

        if( !bNoDump )
            poFeature->Dump( stdout );

        if( poFeature->GetGeometry() != NULL )
        {
            poGeometry = OGRGeometryFactory::createFromGML( 
                poFeature->GetGeometry() );
            if( poGeometry != NULL )
            {
                if( !bNoDump )
                    poGeometry->dumpReadable( stdout );
                delete poGeometry;
            }
        }
        
        delete poFeature;
    }
}

int main( int nArgc, char **papszArgv )

{
    IGMLReader	*poReader;
    int         bNoDump = FALSE;

    if( nArgc < 2 )
        Usage();

    poReader = CreateGMLReader();

    for( int iArg = 1; iArg < nArgc; iArg++ )
    {
        if( EQUAL(papszArgv[iArg],"-si") && iArg < nArgc-1 )
        {
            poReader->LoadClasses( papszArgv[iArg+1] );
            iArg++;
        }
        else if( EQUAL(papszArgv[iArg],"-so") && iArg < nArgc-1 )
        {
            poReader->SaveClasses( papszArgv[iArg+1] );
            iArg++;
        }
        else if( EQUAL(papszArgv[iArg],"-nodump") )
            bNoDump = TRUE;
        else if( papszArgv[iArg][0] != '-' )
        {
            poReader->SetSourceFile( papszArgv[iArg] );
            DumpFile( poReader, bNoDump );
        }
        else
            Usage();
    }

    delete poReader;
}
