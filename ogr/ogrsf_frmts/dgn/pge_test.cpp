/******************************************************************************
 * $Id$
 *
 * Project:  DGN Tag Read/Write Bindings for Pacific Gas and Electric
 * Purpose:  Test mainline program.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2002, Pacific Gas and Electric Co, San Franciso, CA, USA.
 *
 * All rights reserved.  Not to be used, reproduced or disclosed without
 * permission.
 ******************************************************************************
 *
 * $Log$
 * Revision 1.2  2002/03/15 15:07:06  warmerda
 * use dgn_pge.h
 *
 * Revision 1.1  2002/03/14 21:40:37  warmerda
 * New
 *
 */

#include "dgn_pge.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                                main()                                */
/************************************************************************/
int main( int nArgc, char **papszArgv )

{
    if( nArgc >= 3 && EQUAL(papszArgv[1],"-r") )
    {
        printf( "%s\n", pgeDGNReadTags( papszArgv[2], 0 ) );
    }
    else if( nArgc >= 6 && EQUAL(papszArgv[1],"-w") )
    {
        char	szPassString[10000];

        sprintf( szPassString, "TAGLIST;" );

        for( int i = 3; i < nArgc; i += 3 )
        {
            sprintf( szPassString+strlen(szPassString), 
                     "\"%s\":\"%s\":\"%s\";", 
                     papszArgv[i],
                     papszArgv[i+1],
                     papszArgv[i+2] );
        }

        printf( "Passing:\n%s\n", szPassString );
        printf( "Result:%s\n", 
                pgeDGNWriteTags( papszArgv[2], 0, szPassString ) );
    }
    else 
    {
        printf( "Usage: pge_test -r filename\n" );
        printf( "    or pge_test -w filename [tagset tag value]*\n" );
    }

    return 0;
}
