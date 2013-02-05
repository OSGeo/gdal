/******************************************************************************
 * $Id$
 *
 * Project:  GDAL
 * Purpose:  Server application that is forked by libgdal
 * Author:   Even Rouault, <even dot rouault at mines-paris dot org>
 *
 ******************************************************************************
 * Copyright (c) 2013, Even Rouault, <even dot rouault at mines-paris dot org>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 ****************************************************************************/

#include <gdal.h>
#include "cpl_spawn.h"
#include "cpl_string.h"

CPL_C_START
int CPL_DLL GDALServerLoop(CPL_FILE_HANDLE fin, CPL_FILE_HANDLE fout);
CPL_C_END

CPL_CVSID("$Id$");

/************************************************************************/
/*                               Usage()                                */
/************************************************************************/

void Usage(const char* pszErrorMsg)

{
    printf( "Usage: gdalserver [--help-general] [--help] -run\n");
    printf( "\n" );
    printf( "This utility is not meant at being directly used by a user.\n");
    printf( "It is a helper utility for the client/server working of GDAL.\n");

    if( pszErrorMsg != NULL )
        fprintf(stderr, "\nFAILURE: %s\n", pszErrorMsg);

    exit( 1 );
}

/************************************************************************/
/*                                main()                                */
/************************************************************************/

int main(int argc, char* argv[])
{
    int i, nRet, bRun = FALSE;

    GDALAllRegister();

    argc = GDALGeneralCmdLineProcessor( argc, &argv, 0 );
    if( argc < 1 )
        exit( -argc );

    for( i = 1; i < argc; i++ )
    {
        if( EQUAL(argv[i], "--utility_version") )
        {
            printf("%s was compiled against GDAL %s and is running against GDAL %s\n",
                   argv[0], GDAL_RELEASE_NAME, GDALVersionInfo("RELEASE_NAME"));
            return 0;
        }
        else if( EQUAL(argv[i],"--help") )
            Usage(NULL);
        else if( EQUAL(argv[i],"-run") )
            bRun = TRUE;
        else if( argv[i][0] == '-' )
            Usage(CPLSPrintf("Unkown option name '%s'", argv[i]));
        else
            Usage("Too many command options.");
    }
    if( !bRun )
        Usage(NULL);

    CSLDestroy(argv);

#ifdef WIN32
#ifdef _MSC_VER
    __try 
#endif
    { 
        nRet = GDALServerLoop(GetStdHandle(STD_INPUT_HANDLE),
                              GetStdHandle(STD_OUTPUT_HANDLE));
    }
#ifdef _MSC_VER
    __except(1) 
    {
        fprintf(stderr, "gdalserver exited with a fatal error.\n");
        nRet = 1;
    }
#endif
#else
    nRet = GDALServerLoop(fileno(stdin),
                          fileno(stdout));
#endif

    return nRet;
}
