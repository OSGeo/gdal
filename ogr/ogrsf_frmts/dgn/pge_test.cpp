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
 * Revision 1.4  2003/05/21 03:42:01  warmerda
 * Expanded tabs
 *
 * Revision 1.3  2002/03/15 16:08:42  warmerda
 * update to use strings lists for read/write function args
 *
 * Revision 1.2  2002/03/15 15:07:06  warmerda
 * use dgn_pge.h
 *
 * Revision 1.1  2002/03/14 21:40:37  warmerda
 * New
 *
 */

#include "dgn_pge.h"
#include "cpl_string.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                                main()                                */
/************************************************************************/
int main( int nArgc, char **papszArgv )

{
    char        **papszTagSets = NULL;
    char        **papszTagNames = NULL;
    char        **papszTagValues = NULL;
        
    if( nArgc >= 3 && EQUAL(papszArgv[1],"-r") )
    {
        if( DGNReadTags( papszArgv[2], 0, 
                         &papszTagSets, &papszTagNames, &papszTagValues ) )
        {
            int nItems = CSLCount(papszTagSets);

            for( int i = 0; i < nItems; i++ )
                printf( "  %s:%s = %s\n", 
                        papszTagSets[i], 
                        papszTagNames[i],
                        papszTagValues[i] );
            
            CSLDestroy( papszTagSets );
            CSLDestroy( papszTagNames );
            CSLDestroy( papszTagValues );
        }
        else
            printf( "DGNReadTags() returned an error.\n" );
    }
    else if( nArgc >= 6 && EQUAL(papszArgv[1],"-w") )
    {
        for( int i = 3; i < nArgc; i += 3 )
        {
            papszTagSets = CSLAddString( papszTagSets, papszArgv[i] );
            papszTagNames = CSLAddString( papszTagNames, papszArgv[i+1] );
            papszTagValues = CSLAddString( papszTagValues, papszArgv[i+2] );
        }

        if( !DGNWriteTags( papszArgv[2], 0, 
                           papszTagSets, papszTagNames, papszTagValues ) )
            printf( "DGNWriteTags() failed.\n" );
        else
            printf( "DGNWriteTags() succeeded\n" );
    }
    else 
    {
        printf( "Usage: pge_test -r filename\n" );
        printf( "    or pge_test -w filename [tagset tag value]*\n" );
    }

    return 0;
}
