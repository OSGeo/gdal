#include "geotiff.h"
#include "xtiffio.h"
#include "cpl_conv.h"
#include "geo_normalize.h"

int main( int nArgc, char ** papszArgv )

{
    int		i;

    for( i = 1; i < nArgc; i++ )
    {
        TIFF	*hTIFF;
        GTIF	*hGTIF;
        GTIFDefn sDefn;

        hTIFF = XTIFFOpen( papszArgv[i], "r" );
        if( hTIFF == NULL )
        {
            printf( "Couldn't open `%s'\n", papszArgv[i] );
            continue;
        }

        hGTIF = GTIFNew( hTIFF );

        if( GTIFGetDefn( hGTIF, &sDefn ) )
        {
            printf( "\n%s\n", papszArgv[i] );
            GTIFPrintDefn( &sDefn, stdout );

            printf( "PROJ.4 String = `%s'\n",
                    GTIFGetProj4Defn( &sDefn ) );
        }

        
        
        GTIFFree( hGTIF );
        XTIFFClose( hTIFF );
    }
}
