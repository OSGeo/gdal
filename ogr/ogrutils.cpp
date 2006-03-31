/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Utility functions for OGR classes, including some related to
 *           parsing well known text format vectors.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
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
 * Revision 1.22  2006/03/31 17:44:20  fwarmerdam
 * header updates
 *
 * Revision 1.21  2006/02/15 04:25:37  fwarmerdam
 * added date support
 *
 * Revision 1.20  2006/01/07 17:15:38  dron
 * #include "ogrsf_frmts.h" only when compiling with the OGR formats.
 *
 * Revision 1.19  2006/01/06 17:23:26  dron
 * Use OGR_ENABLED macro to separate OGR formats related parts of the code.
 *
 * Revision 1.18  2005/10/16 01:59:06  cfis
 * Added declaration for OGRGeneralCmdLineProcessor to ogr_p.h, and included it
 * into ogr2ogr.  Also changed call to CPL_DLL from CPL_STDCALL
 *
 * Revision 1.17  2005/10/16 01:51:43  cfis
 * Added #include for ogrsf_frmts.f to support the --formats command line parameter
 * in OGRGeneralCmdLineProcessor.
 *
 * Revision 1.16  2005/10/16 01:49:02  cfis
 * Added support for general command line processing similar to what GDAL utilities have.
 *
 * Revision 1.15  2005/09/15 00:52:28  fwarmerdam
 * fixed debug message
 *
 * Revision 1.14  2005/07/29 04:13:17  fwarmerdam
 * preserve 'full' precision in OGRMAkeWktCoordinate
 *
 * Revision 1.13  2005/07/22 19:32:43  fwarmerdam
 * Preserve more precision in WKT encoding of coordinates.
 *
 * Revision 1.12  2005/07/20 01:43:51  fwarmerdam
 * upgraded OGR geometry dimension handling
 *
 * Revision 1.11  2003/01/06 17:57:18  warmerda
 * Added some extra validation in OGRMakeWktCoordinate()
 *
 * Revision 1.10  2002/12/09 18:55:07  warmerda
 * moved DMS stuff to gdal/port
 *
 * Revision 1.9  2002/12/09 16:10:39  warmerda
 * added DMS translation
 *
 * Revision 1.8  2002/08/07 02:46:10  warmerda
 * improved comments
 *
 * Revision 1.7  2001/11/01 17:01:28  warmerda
 * pass output buffer into OGRMakeWktCoordinate
 *
 * Revision 1.6  2001/07/18 05:03:05  warmerda
 * added CPL_CVSID
 *
 * Revision 1.5  2001/05/29 02:24:00  warmerda
 * fixed negative support for Z coordinate
 *
 * Revision 1.4  2001/05/24 18:05:36  warmerda
 * fixed support for negative coordinte parsing
 *
 * Revision 1.3  1999/11/18 19:02:19  warmerda
 * expanded tabs
 *
 * Revision 1.2  1999/09/13 02:27:33  warmerda
 * incorporated limited 2.5d support
 *
 * Revision 1.1  1999/05/20 14:35:00  warmerda
 * New
 *
 */

#include <ctype.h>

#include "ogr_geometry.h"
#include "ogr_p.h"

#ifdef OGR_ENABLED
# include "ogrsf_frmts.h"
#endif /* OGR_ENABLED */

CPL_CVSID("$Id$");

/************************************************************************/
/*                         OGRTrimExtraZeros()                          */
/************************************************************************/

static void OGRTrimExtraZeros( char *pszTarget )

{
    int i = 0;

    while( pszTarget[i] != '\0' ) 
        i++;

/* -------------------------------------------------------------------- */
/*      Trim trailing 000001's as they are likely roundoff error.       */
/* -------------------------------------------------------------------- */
    if( i > 10
        && pszTarget[i-1] == '1' 
        && pszTarget[i-2] == '0' 
        && pszTarget[i-3] == '0' 
        && pszTarget[i-4] == '0' 
        && pszTarget[i-5] == '0' 
        && pszTarget[i-6] == '0' )
    {
        pszTarget[--i] = '\0';
    }

/* -------------------------------------------------------------------- */
/*      Trim trailing zeros.                                            */
/* -------------------------------------------------------------------- */
    while( i > 2 && pszTarget[i-1] == '0' && pszTarget[i-2] != '.' )
    {
        pszTarget[--i] = '\0';
    }
}

/************************************************************************/
/*                        OGRMakeWktCoordinate()                        */
/*                                                                      */
/*      Format a well known text coordinate, trying to keep the         */
/*      ASCII representation compact, but accurate.  These rules        */
/*      will have to tighten up in the future.                          */
/*                                                                      */
/*      Currently a new point should require no more than 64            */
/*      characters barring the X or Y value being extremely large.      */
/************************************************************************/

void OGRMakeWktCoordinate( char *pszTarget, double x, double y, double z, 
                           int nDimension )

{
    char  szX[40], szY[40], szZ[40];

    szZ[0] = '\0';

    if( x == (int) x && y == (int) y && z == (int) z )
    {
        sprintf( szX, "%d", (int) x );
        sprintf( szY, " %d", (int) y );
    }
    else
    {
        sprintf( szX, "%.15f", x );
        OGRTrimExtraZeros( szX );
        sprintf( szY, " %.15f", y );
        OGRTrimExtraZeros( szY );
    }

    if( nDimension == 3 )
    {
        if( z == (int) z )
            sprintf( szZ, " %d", (int) z );
        else
        {
            sprintf( szZ, " %.15f", z );
            OGRTrimExtraZeros( szZ );
        }
    }
            
    if( strlen(szX) + strlen(szY) + strlen(szZ) > 75 )
    {
        strcpy( szX, "0" );
        strcpy( szY, "0" );
        if( nDimension == 3 )
            strcpy( szZ, "0" );

#ifdef DEBUG
        CPLDebug( "OGR", 
                  "Yow!  Got this big result in OGRMakeWktCoordinate()\n"
                  "%s %s %s", 
                  szX, szY, szZ );
#endif
    }

    strcpy( pszTarget, szX );
    strcat( pszTarget, szY );
    strcat( pszTarget, szZ );
}

/************************************************************************/
/*                          OGRWktReadToken()                           */
/*                                                                      */
/*      Read one token or delimeter and put into token buffer.  Pre     */
/*      and post white space is swallowed.                              */
/************************************************************************/

const char *OGRWktReadToken( const char * pszInput, char * pszToken )

{
    if( pszInput == NULL )
        return NULL;
    
/* -------------------------------------------------------------------- */
/*      Swallow pre-white space.                                        */
/* -------------------------------------------------------------------- */
    while( *pszInput == ' ' || *pszInput == '\t' )
        pszInput++;

/* -------------------------------------------------------------------- */
/*      If this is a delimeter, read just one character.                */
/* -------------------------------------------------------------------- */
    if( *pszInput == '(' || *pszInput == ')' || *pszInput == ',' )
    {
        pszToken[0] = *pszInput;
        pszToken[1] = '\0';
        
        pszInput++;
    }

/* -------------------------------------------------------------------- */
/*      Or if it alpha numeric read till we reach non-alpha numeric     */
/*      text.                                                           */
/* -------------------------------------------------------------------- */
    else
    {
        int             iChar = 0;
        
        while( iChar < OGR_WKT_TOKEN_MAX-1
               && ((*pszInput >= 'a' && *pszInput <= 'z')
                   || (*pszInput >= 'A' && *pszInput <= 'Z')
                   || (*pszInput >= '0' && *pszInput <= '9')
                   || *pszInput == '.' 
                   || *pszInput == '+' 
                   || *pszInput == '-') )
        {
            pszToken[iChar++] = *(pszInput++);
        }

        pszToken[iChar++] = '\0';
    }

/* -------------------------------------------------------------------- */
/*      Eat any trailing white space.                                   */
/* -------------------------------------------------------------------- */
    while( *pszInput == ' ' || *pszInput == '\t' )
        pszInput++;

    return( pszInput );
}

/************************************************************************/
/*                          OGRWktReadPoints()                          */
/*                                                                      */
/*      Read a point string.  The point list must be contained in       */
/*      brackets and each point pair separated by a comma.              */
/************************************************************************/

const char * OGRWktReadPoints( const char * pszInput,
                               OGRRawPoint ** ppaoPoints, double **ppadfZ,
                               int * pnMaxPoints,
                               int * pnPointsRead )

{
    *pnPointsRead = 0;

    if( pszInput == NULL )
        return NULL;
    
/* -------------------------------------------------------------------- */
/*      Eat any leading white space.                                    */
/* -------------------------------------------------------------------- */
    while( *pszInput == ' ' || *pszInput == '\t' )
        pszInput++;

/* -------------------------------------------------------------------- */
/*      If this isn't an opening bracket then we have a problem!        */
/* -------------------------------------------------------------------- */
    if( *pszInput != '(' )
    {
        CPLDebug( "OGR",
                  "Expected '(', but got %s in OGRWktReadPoints().\n",
                  pszInput );
                  
        return pszInput;
    }

    pszInput++;

/* ==================================================================== */
/*      This loop reads a single point.  It will continue till we       */
/*      run out of well formed points, or a closing bracket is          */
/*      encountered.                                                    */
/* ==================================================================== */
    char        szDelim[OGR_WKT_TOKEN_MAX];
    
    do {
/* -------------------------------------------------------------------- */
/*      Read the X and Y values, verify they are numeric.               */
/* -------------------------------------------------------------------- */
        char    szTokenX[OGR_WKT_TOKEN_MAX];
        char    szTokenY[OGR_WKT_TOKEN_MAX];

        pszInput = OGRWktReadToken( pszInput, szTokenX );
        pszInput = OGRWktReadToken( pszInput, szTokenY );

        if( (!isdigit(szTokenX[0]) && szTokenX[0] != '-')
            || (!isdigit(szTokenY[0]) && szTokenY[0] != '-') )
            return NULL;

/* -------------------------------------------------------------------- */
/*      Do we need to grow the point list to hold this point?           */
/* -------------------------------------------------------------------- */
        if( *pnPointsRead == *pnMaxPoints )
        {
            *pnMaxPoints = *pnMaxPoints * 2 + 10;
            *ppaoPoints = (OGRRawPoint *)
                CPLRealloc(*ppaoPoints, sizeof(OGRRawPoint) * *pnMaxPoints);

            if( *ppadfZ != NULL )
            {
                *ppadfZ = (double *)
                    CPLRealloc(*ppadfZ, sizeof(double) * *pnMaxPoints);
            }
        }

/* -------------------------------------------------------------------- */
/*      Add point to list.                                              */
/* -------------------------------------------------------------------- */
        (*ppaoPoints)[*pnPointsRead].x = atof(szTokenX);
        (*ppaoPoints)[*pnPointsRead].y = atof(szTokenY);

/* -------------------------------------------------------------------- */
/*      Do we have a Z coordinate?                                      */
/* -------------------------------------------------------------------- */
        pszInput = OGRWktReadToken( pszInput, szDelim );

        if( isdigit(szDelim[0]) || szDelim[0] == '-' )
        {
            if( *ppadfZ == NULL )
            {
                *ppadfZ = (double *) CPLCalloc(sizeof(double),*pnMaxPoints);
            }

            (*ppadfZ)[*pnPointsRead] = atof(szDelim);
            
            pszInput = OGRWktReadToken( pszInput, szDelim );
        }
        
        (*pnPointsRead)++;

/* -------------------------------------------------------------------- */
/*      Read next delimeter ... it should be a comma if there are       */
/*      more points.                                                    */
/* -------------------------------------------------------------------- */
        if( szDelim[0] != ')' && szDelim[0] != ',' )
        {
            CPLDebug( "OGR",
                      "Corrupt input in OGRWktReadPoints()\n"
                      "Got `%s' when expecting `,' or `)'.\n",
                      szDelim );
            return NULL;
        }
        
    } while( szDelim[0] == ',' );

    return pszInput;
}

/************************************************************************/
/*                             OGRMalloc()                              */
/*                                                                      */
/*      Cover for CPLMalloc()                                           */
/************************************************************************/

void *OGRMalloc( size_t size )

{
    return CPLMalloc( size );
}

/************************************************************************/
/*                             OGRCalloc()                              */
/*                                                                      */
/*      Cover for CPLCalloc()                                           */
/************************************************************************/

void * OGRCalloc( size_t count, size_t size )

{
    return CPLCalloc( count, size );
}

/************************************************************************/
/*                             OGRRealloc()                             */
/*                                                                      */
/*      Cover for CPLRealloc()                                          */
/************************************************************************/

void *OGRRealloc( void * pOld, size_t size )

{
    return CPLRealloc( pOld, size );
}

/************************************************************************/
/*                              OGRFree()                               */
/*                                                                      */
/*      Cover for CPLFree().                                            */
/************************************************************************/

void OGRFree( void * pMemory )

{
    CPLFree( pMemory );
}

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

int OGRGeneralCmdLineProcessor( int nArgc, char ***ppapszArgv, int nOptions )

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
#ifdef OGR_ENABLED
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
#endif /* OGR_ENABLED */

/* -------------------------------------------------------------------- */
/*      --help-general                                                  */
/* -------------------------------------------------------------------- */
        else if( EQUAL(papszArgv[iArg],"--help-general") )
        {
            printf( "Generic GDAL utility command options:\n" );
#ifdef OGR_ENABLED
            printf( "  --formats: report all configured format drivers.\n" );
#endif /* OGR_ENABLED */
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

/************************************************************************/
/*                            OGRParseDate()                            */
/*                                                                      */
/*      Parse a variety of text date formats into an OGRField.          */
/************************************************************************/

/**
 * Parse date string.
 *
 * This function attempts to parse a date string in a variety of formats
 * into the OGRField.Date format suitable for use with OGR.  Generally 
 * speaking this function is expecting values like:
 * 
 *   YYYY-MM-DD HH:MM:SS+nn
 *
 * The seconds may also have a decimal portion (which is ignored).  And
 * just dates (YYYY-MM-DD) or just times (HH:MM:SS) are also supported. 
 * The date may also be in YYYY/MM/DD format.  If the year is less than 100
 * and greater than 30 a "1900" century value will be set.  If it is less than
 * 30 and greater than -1 then a "2000" century value will be set.  In 
 * the future this function may be generalized, and additional control 
 * provided through nOptions, but an nOptions value of "0" should always do
 * a reasonable default form of processing.
 *
 * The value of psField will be indeterminate if the function fails (returns
 * FALSE).  
 *
 * @param pszInput the input date string.
 * @param psField the OGRField that will be updated with the parsed result.
 * @param nOptions parsing options, for now always 0. 
 *
 * @return TRUE if apparently successful or FALSE on failure.
 */

int OGRParseDate( const char *pszInput, OGRField *psField, int nOptions )

{
    int bGotSomething = FALSE;

    psField->Date.Year = 0;
    psField->Date.Month = 0;
    psField->Date.Day = 0;
    psField->Date.Hour = 0;
    psField->Date.Minute = 0;
    psField->Date.Second = 0;
    psField->Date.TZFlag = 0;
    
/* -------------------------------------------------------------------- */
/*      Do we have a date?                                              */
/* -------------------------------------------------------------------- */
    while( *pszInput == ' ' )
        pszInput++;
    
    if( strstr(pszInput,"-") != NULL || strstr(pszInput,"/") != NULL )
    {
        psField->Date.Year = atoi(pszInput);
        if( psField->Date.Year < 100 && psField->Date.Year >= 30 )
            psField->Date.Year += 1900;
        else if( psField->Date.Year < 30 && psField->Date.Year >= 0 )
            psField->Date.Year += 2000;

        while( *pszInput >= '0' && *pszInput <= '9' ) 
            pszInput++;
        if( *pszInput != '-' && *pszInput != '/' )
            return FALSE;
        else 
            pszInput++;

        psField->Date.Month = atoi(pszInput);
        if( psField->Date.Month > 12 )
            return FALSE;

        while( *pszInput >= '0' && *pszInput <= '9' ) 
            pszInput++;
        if( *pszInput != '-' && *pszInput != '/' )
            return FALSE;
        else 
            pszInput++;

        psField->Date.Day = atoi(pszInput);
        if( psField->Date.Day > 31 )
            return FALSE;

        while( *pszInput >= '0' && *pszInput <= '9' )
            pszInput++;

        bGotSomething = TRUE;
    }

/* -------------------------------------------------------------------- */
/*      Do we have a time?                                              */
/* -------------------------------------------------------------------- */
    while( *pszInput == ' ' )
        pszInput++;
    
    if( strstr(pszInput,":") != NULL )
    {
        psField->Date.Hour = atoi(pszInput);
        if( psField->Date.Hour > 23 )
            return FALSE;

        while( *pszInput >= '0' && *pszInput <= '9' ) 
            pszInput++;
        if( *pszInput != ':' )
            return FALSE;
        else 
            pszInput++;

        psField->Date.Minute = atoi(pszInput);
        if( psField->Date.Minute > 59 )
            return FALSE;

        while( *pszInput >= '0' && *pszInput <= '9' ) 
            pszInput++;
        if( *pszInput != ':' )
            return FALSE;
        else 
            pszInput++;

        psField->Date.Second = atoi(pszInput);
        if( psField->Date.Second > 59 )
            return FALSE;

        while( *pszInput >= '0' && *pszInput <= '9' 
               || *pszInput == '.' )
            pszInput++;

        bGotSomething = TRUE;
    }

    // No date or time!
    if( !bGotSomething )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Do we have a timezone?                                          */
/* -------------------------------------------------------------------- */
    while( *pszInput == ' ' )
        pszInput++;
    
    if( *pszInput == '-' || *pszInput == '+' )
    {
        // +HH integral offset
        if( strlen(pszInput) <= 3 )
            psField->Date.TZFlag = 100 + atoi(pszInput) * 4;

        else if( pszInput[3] == ':'  // +HH:MM offset
                 && atoi(pszInput+4) % 15 == 0 )
        {
            psField->Date.TZFlag = 100 
                + atoi(pszInput+1) * 4
                + (atoi(pszInput+4) / 15);

            if( pszInput[0] == '-' )
                psField->Date.TZFlag = -1 * (psField->Date.TZFlag - 100) + 100;
        }
        else if( isdigit(pszInput[3]) && isdigit(pszInput[4])  // +HHMM offset
                 && atoi(pszInput+3) % 15 == 0 )
        {
            psField->Date.TZFlag = 100 
                + CPLScanLong(pszInput+1,2) * 4
                + (atoi(pszInput+3) / 15);

            if( pszInput[0] == '-' )
                psField->Date.TZFlag = -1 * (psField->Date.TZFlag - 100) + 100;
        }
        else if( isdigit(pszInput[3]) && pszInput[4] == '\0'  // +HMM offset
                 && atoi(pszInput+2) % 15 == 0 )
        {
            psField->Date.TZFlag = 100 
                + CPLScanLong(pszInput+1,1) * 4
                + (atoi(pszInput+2) / 15);

            if( pszInput[0] == '-' )
                psField->Date.TZFlag = -1 * (psField->Date.TZFlag - 100) + 100;
        }
        // otherwise ignore any timezone info.
    }

    return TRUE;
}
