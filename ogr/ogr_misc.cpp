/******************************************************************************
 * $Id$
 *
 * Project:  OGR
 * Purpose:  Free standing functions for OGR.
 *
 ******************************************************************************
 * Copyright (c) 1999, Frank Warmerdam
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
 ******************************************************************************
 *
 * $Log$
 * Revision 1.1  2005/10/16 01:40:31  cfis
 * Implementation of  --config, --debug, and --formats command line parameters similar to what GDAL utilities have.
 *
 */

#include "ogr_misc.h"


/************************************************************************/
/*                    OGRGeneralCmdLineProcessor()                     */
/************************************************************************/

/**
 * General utility option processing.
 *
 * This function is intended to provide a variety of generic commandline 
 * options for all OGR commandline utilities.  It takes care of the following
 * commandline options:
 *  
 *  --formats: report all format drivers configured.
 *  --format [format]: report details of one format driver. 
 *  --optfile filename: expand an option file into the argument list. 
 *  --config key value: set system configuration option. 
 *  --debug [on/off/value]: set debug level.
 *  --help-general: report detailed help on general options. 
 *
 * The argument array is replaced "in place" and should be freed with 
 * CSLDestroy() when no longer needed.  The typical usage looks something
 * like the following.  Note that the formats should be registered so that
 * the --formats and --format options will work properly.
 *
 *  int main( int argc, char ** argv )
 *  { 
 *    OGRAllRegister();
 *
 *    argc = OGRGeneralCmdLineProcessor( argc, &argv, 0 );
 *    if( argc < 1 )
 *        exit( -argc );
 *
 * @param nArgc number of values in the argument list.
 * @param Pointer to the argument list array (will be updated in place). 
 *
 * @return updated nArgc argument count.  Return of 0 requests terminate 
 * without error, return of -1 requests exit with error code.
 */

int CPL_STDCALL 
OGRGeneralCmdLineProcessor( int nArgc, char ***ppapszArgv, int nOptions )

{
    char **papszReturn = NULL;
    int  iArg;
    char **papszArgv = *ppapszArgv;

    (void) nOptions;
    
/* -------------------------------------------------------------------- */
/*      Preserve the program name.                                      */
/* -------------------------------------------------------------------- */
    papszReturn = CSLAddString( papszReturn, papszArgv[0] );

/* ==================================================================== */
/*      Loop over all arguments.                                        */
/* ==================================================================== */
    for( iArg = 1; iArg < nArgc; iArg++ )
    {

/* -------------------------------------------------------------------- */
/*      --config                                                        */
/* -------------------------------------------------------------------- */
        if( EQUAL(papszArgv[iArg],"--config") )
        {
            if( iArg + 2 >= nArgc )
            {
                CPLError( CE_Failure, CPLE_AppDefined, 
                          "--config option given without a key and value argument." );
                CSLDestroy( papszReturn );
                return -1;
            }

            CPLSetConfigOption( papszArgv[iArg+1], papszArgv[iArg+2] );

            iArg += 2;
        }

/* -------------------------------------------------------------------- */
/*      --debug                                                         */
/* -------------------------------------------------------------------- */
        else if( EQUAL(papszArgv[iArg],"--debug") )
        {
            if( iArg + 1 >= nArgc )
            {
                CPLError( CE_Failure, CPLE_AppDefined, 
                          "--debug option given without debug level." );
                CSLDestroy( papszReturn );
                return -1;
            }

            CPLSetConfigOption( "CPL_DEBUG", papszArgv[iArg+1] );
            iArg += 1;
        }

/* -------------------------------------------------------------------- */
/*      --optfile                                                       */
/*                                                                      */
/*      Annoyingly the options inserted by --optfile will *not* be      */
/*      processed properly if they are general options.                 */
/* -------------------------------------------------------------------- */
        else if( EQUAL(papszArgv[iArg],"--optfile") )
        {
            const char *pszLine;
            FILE *fpOptFile;

            if( iArg + 1 >= nArgc )
            {
                CPLError( CE_Failure, CPLE_AppDefined, 
                          "--optfile option given without filename." );
                CSLDestroy( papszReturn );
                return -1;
            }

            fpOptFile = VSIFOpen( papszArgv[iArg+1], "rb" );

            if( fpOptFile == NULL )
            {
                CPLError( CE_Failure, CPLE_AppDefined, 
                          "Unable to open optfile '%s'.\n%s",
                          papszArgv[iArg+1], VSIStrerror( errno ) );
                CSLDestroy( papszReturn );
                return -1;
            }
            
            while( (pszLine = CPLReadLine( fpOptFile )) != NULL )
            {
                char **papszTokens;
                int i;

                if( pszLine[0] == '#' || strlen(pszLine) == 0 )
                    continue;

                papszTokens = CSLTokenizeString( pszLine );
                for( i = 0; papszTokens != NULL && papszTokens[i] != NULL; i++)
                    papszReturn = CSLAddString( papszReturn, papszTokens[i] );
                CSLDestroy( papszTokens );
            }

            VSIFClose( fpOptFile );
                
            iArg += 1;
        }

/* -------------------------------------------------------------------- */
/*      --formats                                                       */
/* -------------------------------------------------------------------- */
        else if( EQUAL(papszArgv[iArg], "--formats") )
        {
            int iDr;

            printf( "Supported Formats:\n" );

            OGRSFDriverRegistrar *poR = OGRSFDriverRegistrar::GetRegistrar();
        
            for( iDr = 0; iDr < poR->GetDriverCount(); iDr++ )
            {
                OGRSFDriver *poDriver = poR->GetDriver(iDr);
                printf( "  %s\n", poDriver->GetName());
            }

            CSLDestroy( papszReturn );
            return 0;
        }


/* -------------------------------------------------------------------- */
/*      --help-general                                                  */
/* -------------------------------------------------------------------- */
        else if( EQUAL(papszArgv[iArg],"--help-general") )
        {
            printf( "Generic GDAL utility command options:\n" );
            printf( "  --formats: report all configured format drivers.\n" );
            printf( "  --optfile filename: expand an option file into the argument list.\n" );
            printf( "  --config key value: set system configuration option.\n" );
            printf( "  --debug [on/off/value]: set debug level.\n" );
            printf( "  --help-general: report detailed help on general options.\n" );
            CSLDestroy( papszReturn );
            return 0;
        }

/* -------------------------------------------------------------------- */
/*      carry through unrecognised options.                             */
/* -------------------------------------------------------------------- */
        else
        {
            papszReturn = CSLAddString( papszReturn, papszArgv[iArg] );
        }
    }

    *ppapszArgv = papszReturn;

    return CSLCount( *ppapszArgv );
}

