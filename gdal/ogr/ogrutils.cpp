/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Utility functions for OGR classes, including some related to
 *           parsing well known text format vectors.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Frank Warmerdam
 * Copyright (c) 2008-2014, Even Rouault <even dot rouault at mines-paris dot org>
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

#include <cctype>
#include <cstdlib>

#include "cpl_conv.h"
#include "cpl_vsi.h"

#include "ogr_geometry.h"
#include "ogr_p.h"

# include "ogrsf_frmts.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                        OGRFormatDouble()                             */
/************************************************************************/

void OGRFormatDouble( char *pszBuffer, int nBufferLen, double dfVal,
                      char chDecimalSep, int nPrecision,
                      char chConversionSpecifier )
{
    // So to have identical cross platform representation.
    if( CPLIsInf(dfVal) )
    {
        if( dfVal > 0 )
            CPLsnprintf(pszBuffer, nBufferLen, "%s", "inf");
        else
            CPLsnprintf(pszBuffer, nBufferLen, "%s", "-inf");
        return;
    }
    if( CPLIsNan(dfVal) )
    {
        CPLsnprintf(pszBuffer, nBufferLen, "%s", "nan");
        return;
    }

    char szFormat[16] = {};
    snprintf(szFormat, sizeof(szFormat),
             "%%.%d%c", nPrecision, chConversionSpecifier);

    int ret = CPLsnprintf(pszBuffer, nBufferLen, szFormat, dfVal);
    // Windows CRT does not conform with C99 and returns -1 when buffer is
    // truncated.
    if( ret >= nBufferLen || ret == -1 )
    {
        CPLsnprintf(pszBuffer, nBufferLen, "%s", "too_big");
        return;
    }

    if( chConversionSpecifier == 'g' && strchr(pszBuffer, 'e') )
        return;

    int nTruncations = 0;
    while( nPrecision > 0 )
    {
        int i = 0;
        int nCountBeforeDot = 0;
        int iDotPos = -1;
        while( pszBuffer[i] != '\0' )
        {
            if( pszBuffer[i] == '.' && chDecimalSep != '\0' )
            {
                iDotPos = i;
                pszBuffer[i] = chDecimalSep;
            }
            else if( iDotPos < 0 && pszBuffer[i] != '-' )
                ++nCountBeforeDot;
            ++i;
        }
        if( iDotPos < 0 )
            break;

    /* -------------------------------------------------------------------- */
    /*      Trim trailing 00000x's as they are likely roundoff error.       */
    /* -------------------------------------------------------------------- */
        if( i > 10 )
        {
            if(  // && pszBuffer[i-1] == '1' &&
                pszBuffer[i-2] == '0'
                && pszBuffer[i-3] == '0'
                && pszBuffer[i-4] == '0'
                && pszBuffer[i-5] == '0'
                && pszBuffer[i-6] == '0' )
            {
                pszBuffer[--i] = '\0';
            }
            else if( i - 8 > iDotPos &&  // pszBuffer[i-1] == '1'
                     // && pszBuffer[i-2] == '0' &&
                     (nCountBeforeDot >= 4 || pszBuffer[i-3] == '0')
                     && (nCountBeforeDot >= 5 || pszBuffer[i-4] == '0')
                     && (nCountBeforeDot >= 6 || pszBuffer[i-5] == '0')
                     && (nCountBeforeDot >= 7 || pszBuffer[i-6] == '0')
                     && (nCountBeforeDot >= 8 || pszBuffer[i-7] == '0')
                     && pszBuffer[i-8] == '0'
                     && pszBuffer[i-9] == '0')
            {
                i -= 8;
                pszBuffer[i] = '\0';
            }
        }

    /* -------------------------------------------------------------------- */
    /*      Trim trailing zeros.                                            */
    /* -------------------------------------------------------------------- */
        while( i > 2 && pszBuffer[i-1] == '0' && pszBuffer[i-2] != '.' )
        {
            pszBuffer[--i] = '\0';
        }

    /* -------------------------------------------------------------------- */
    /*      Detect trailing 99999X's as they are likely roundoff error.     */
    /* -------------------------------------------------------------------- */
        if( i > 10 &&
            nPrecision + nTruncations >= 15)
        {
            if(  //pszBuffer[i-1] == '9' &&
                pszBuffer[i-2] == '9'
                && pszBuffer[i-3] == '9'
                && pszBuffer[i-4] == '9'
                && pszBuffer[i-5] == '9'
                && pszBuffer[i-6] == '9' )
            {
                --nPrecision;
                ++nTruncations;
                snprintf(szFormat, sizeof(szFormat),
                         "%%.%d%c", nPrecision, chConversionSpecifier);
                CPLsnprintf(pszBuffer, nBufferLen, szFormat, dfVal);
                if( chConversionSpecifier == 'g' && strchr(pszBuffer, 'e') )
                    return;
                continue;
            }
            else if( i - 9 > iDotPos &&
                     // pszBuffer[i-1] == '9' &&
                     //pszBuffer[i-2] == '9' &&
                    (nCountBeforeDot >= 4 || pszBuffer[i-3] == '9')
                     && (nCountBeforeDot >= 5 || pszBuffer[i-4] == '9')
                     && (nCountBeforeDot >= 6 || pszBuffer[i-5] == '9')
                     && (nCountBeforeDot >= 7 || pszBuffer[i-6] == '9')
                     && (nCountBeforeDot >= 8 || pszBuffer[i-7] == '9')
                     && pszBuffer[i-8] == '9'
                     && pszBuffer[i-9] == '9')
            {
                --nPrecision;
                ++nTruncations;
                snprintf(szFormat, sizeof(szFormat),
                         "%%.%d%c", nPrecision, chConversionSpecifier);
                CPLsnprintf(pszBuffer, nBufferLen, szFormat, dfVal);
                if( chConversionSpecifier == 'g' && strchr(pszBuffer, 'e') )
                    return;
                continue;
            }
        }

        break;
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
    const size_t bufSize = 75;
    // Assumed max length of the target buffer.
    const size_t maxTargetSize = 75;
    const char chDecimalSep = '.';
    static int nPrecision = -1;
    if( nPrecision < 0 )
        nPrecision = atoi(CPLGetConfigOption("OGR_WKT_PRECISION", "15"));

    char szX[bufSize] = {};
    char szY[bufSize] = {};
    char szZ[bufSize] = {};

    szZ[0] = '\0';

    size_t nLenX = 0;
    size_t nLenY = 0;

    if( CPL_IS_DOUBLE_A_INT(x) && CPL_IS_DOUBLE_A_INT(y) )
    {
        snprintf( szX, bufSize, "%d", static_cast<int>(x) );
        snprintf( szY, bufSize, "%d", static_cast<int>(y) );
    }
    else
    {
        OGRFormatDouble( szX, bufSize, x, chDecimalSep, nPrecision,
                         fabs(x) < 1 ? 'f' : 'g' );
        if( CPLIsFinite(x) && strchr(szX, '.') == NULL &&
            strchr(szX, 'e') == NULL && strlen(szX) < bufSize - 2 )
        {
            strcat(szX, ".0");
        }
        OGRFormatDouble( szY, bufSize, y, chDecimalSep, nPrecision,
                         fabs(y) < 1 ? 'f' : 'g' );
        if( CPLIsFinite(y) && strchr(szY, '.') == NULL &&
            strchr(szY, 'e') == NULL && strlen(szY) < bufSize - 2 )
        {
            strcat(szY, ".0");
        }
    }

    nLenX = strlen(szX);
    nLenY = strlen(szY);

    if( nDimension == 3 )
    {
        if( CPL_IS_DOUBLE_A_INT(z) )
        {
            snprintf( szZ, bufSize, "%d", static_cast<int>(z) );
        }
        else
        {
            OGRFormatDouble( szZ, bufSize, z, chDecimalSep, nPrecision, 'g' );
        }
    }

    if( nLenX + 1 + nLenY + ((nDimension == 3) ? (1 + strlen(szZ)) : 0) >=
        maxTargetSize )
    {
#ifdef DEBUG
        CPLDebug( "OGR",
                  "Yow!  Got this big result in OGRMakeWktCoordinate(): "
                  "%s %s %s",
                  szX, szY, szZ );
#endif
        if( nDimension == 3 )
            strcpy( pszTarget, "0 0 0");
        else
            strcpy( pszTarget, "0 0");
    }
    else
    {
        memcpy( pszTarget, szX, nLenX );
        pszTarget[nLenX] = ' ';
        memcpy( pszTarget + nLenX + 1, szY, nLenY );
        if( nDimension == 3 )
        {
            pszTarget[nLenX + 1 + nLenY] = ' ';
            strcpy( pszTarget + nLenX + 1 + nLenY + 1, szZ );
        }
        else
        {
            pszTarget[nLenX + 1 + nLenY] = '\0';
        }
    }
}

/************************************************************************/
/*                        OGRMakeWktCoordinateM()                       */
/*                                                                      */
/*      Format a well known text coordinate, trying to keep the         */
/*      ASCII representation compact, but accurate.  These rules        */
/*      will have to tighten up in the future.                          */
/*                                                                      */
/*      Currently a new point should require no more than 64            */
/*      characters barring the X or Y value being extremely large.      */
/************************************************************************/

void OGRMakeWktCoordinateM( char *pszTarget,
                            double x, double y, double z, double m,
                            OGRBoolean hasZ, OGRBoolean hasM )

{
    const size_t bufSize = 75;
    // Assumed max length of the target buffer.
    const size_t maxTargetSize = 75;
    const char chDecimalSep = '.';
    static int nPrecision = -1;
    if( nPrecision < 0 )
        nPrecision = atoi(CPLGetConfigOption("OGR_WKT_PRECISION", "15"));

    char szX[bufSize] = {};
    char szY[bufSize] = {};
    char szZ[bufSize] = {};
    char szM[bufSize] = {};

    size_t nLen = 0;
    size_t nLenX = 0;
    size_t nLenY = 0;

    if( CPL_IS_DOUBLE_A_INT(x) && CPL_IS_DOUBLE_A_INT(y) )
    {
        snprintf( szX, bufSize, "%d", static_cast<int>(x) );
        snprintf( szY, bufSize, "%d", static_cast<int>(y) );
    }
    else
    {
        OGRFormatDouble( szX, bufSize, x, chDecimalSep, nPrecision,
                         fabs(x) < 1 ? 'f' : 'g' );
        if( CPLIsFinite(x) && strchr(szX, '.') == NULL &&
            strchr(szX, 'e') == NULL && strlen(szX) < bufSize - 2 )
        {
            strcat(szX, ".0");
        }
        OGRFormatDouble( szY, bufSize, y, chDecimalSep, nPrecision,
                         fabs(y) < 1 ? 'f' : 'g' );
        if( CPLIsFinite(y) && strchr(szY, '.') == NULL &&
            strchr(szY, 'e') == NULL && strlen(szY) < bufSize - 2 )
        {
            strcat(szY, ".0");
        }
    }

    nLenX = strlen(szX);
    nLenY = strlen(szY);
    nLen = nLenX + nLenY + 1;

    if( hasZ )
    {
        if( CPL_IS_DOUBLE_A_INT(z) )
        {
            snprintf( szZ, bufSize, "%d", static_cast<int>(z) );
        }
        else
        {
            OGRFormatDouble( szZ, bufSize, z, chDecimalSep, nPrecision, 'g' );
        }
        nLen += strlen(szZ) + 1;
    }

    if( hasM )
    {
        if( CPL_IS_DOUBLE_A_INT(m) )
        {
            snprintf( szM, bufSize, "%d", static_cast<int>(m) );
        }
        else
        {
            OGRFormatDouble( szM, bufSize, m, chDecimalSep, nPrecision, 'g' );
        }
        nLen += strlen(szM) + 1;
    }

    if( nLen >= maxTargetSize )
    {
#ifdef DEBUG
        CPLDebug( "OGR",
                  "Yow!  Got this big result in OGRMakeWktCoordinate(): "
                  "%s %s %s %s",
                  szX, szY, szZ, szM );
#endif
        if( hasZ && hasM )
            strcpy( pszTarget, "0 0 0 0");
        else if( hasZ || hasM )
            strcpy( pszTarget, "0 0 0");
        else
            strcpy( pszTarget, "0 0");
    }
    else
    {
        char *target = pszTarget;
        strcpy( target, szX );
        target += nLenX;
        *target = ' ';
        ++target;
        strcpy( target, szY );
        target += nLenY;
        if( hasZ )
        {
            *target = ' ';
            ++target;
            strcpy( target, szZ );
            target += strlen(szZ);
        }
        if( hasM )
        {
            *target = ' ';
            ++target;
            strcpy( target, szM );
            target += strlen(szM);
        }
        *target = '\0';
    }
}

/************************************************************************/
/*                          OGRWktReadToken()                           */
/*                                                                      */
/*      Read one token or delimiter and put into token buffer.  Pre     */
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
        ++pszInput;

/* -------------------------------------------------------------------- */
/*      If this is a delimiter, read just one character.                */
/* -------------------------------------------------------------------- */
    if( *pszInput == '(' || *pszInput == ')' || *pszInput == ',' )
    {
        pszToken[0] = *pszInput;
        pszToken[1] = '\0';

        ++pszInput;
    }

/* -------------------------------------------------------------------- */
/*      Or if it alpha numeric read till we reach non-alpha numeric     */
/*      text.                                                           */
/* -------------------------------------------------------------------- */
    else
    {
        int iChar = 0;

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
        ++pszInput;

    return pszInput;
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
    const char *pszOrigInput = pszInput;
    *pnPointsRead = 0;

    if( pszInput == NULL )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Eat any leading white space.                                    */
/* -------------------------------------------------------------------- */
    while( *pszInput == ' ' || *pszInput == '\t' )
        ++pszInput;

/* -------------------------------------------------------------------- */
/*      If this isn't an opening bracket then we have a problem.        */
/* -------------------------------------------------------------------- */
    if( *pszInput != '(' )
    {
        CPLDebug( "OGR",
                  "Expected '(', but got %s in OGRWktReadPoints().",
                  pszInput );

        return pszInput;
    }

    ++pszInput;

/* ==================================================================== */
/*      This loop reads a single point.  It will continue till we       */
/*      run out of well formed points, or a closing bracket is          */
/*      encountered.                                                    */
/* ==================================================================== */
    char szDelim[OGR_WKT_TOKEN_MAX] = {};

    do {
/* -------------------------------------------------------------------- */
/*      Read the X and Y values, verify they are numeric.               */
/* -------------------------------------------------------------------- */
        char szTokenX[OGR_WKT_TOKEN_MAX] = {};
        char szTokenY[OGR_WKT_TOKEN_MAX] = {};

        pszInput = OGRWktReadToken( pszInput, szTokenX );
        pszInput = OGRWktReadToken( pszInput, szTokenY );

        if( (!isdigit(szTokenX[0]) && szTokenX[0] != '-' && szTokenX[0] != '.' )
            || (!isdigit(szTokenY[0]) && szTokenY[0] != '-' &&
                szTokenY[0] != '.') )
            return NULL;

/* -------------------------------------------------------------------- */
/*      Do we need to grow the point list to hold this point?           */
/* -------------------------------------------------------------------- */
        if( *pnPointsRead == *pnMaxPoints )
        {
            *pnMaxPoints = *pnMaxPoints * 2 + 10;
            *ppaoPoints = static_cast<OGRRawPoint *>(
                CPLRealloc(*ppaoPoints, sizeof(OGRRawPoint) * *pnMaxPoints) );

            if( *ppadfZ != NULL )
            {
                *ppadfZ = static_cast<double *>(
                    CPLRealloc(*ppadfZ, sizeof(double) * *pnMaxPoints) );
            }
        }

/* -------------------------------------------------------------------- */
/*      Add point to list.                                              */
/* -------------------------------------------------------------------- */
        (*ppaoPoints)[*pnPointsRead].x = CPLAtof(szTokenX);
        (*ppaoPoints)[*pnPointsRead].y = CPLAtof(szTokenY);

/* -------------------------------------------------------------------- */
/*      Do we have a Z coordinate?                                      */
/* -------------------------------------------------------------------- */
        pszInput = OGRWktReadToken( pszInput, szDelim );

        if( isdigit(szDelim[0]) || szDelim[0] == '-' || szDelim[0] == '.' )
        {
            if( *ppadfZ == NULL )
            {
                *ppadfZ = static_cast<double *>(
                    CPLCalloc(sizeof(double), *pnMaxPoints) );
            }

            (*ppadfZ)[*pnPointsRead] = CPLAtof(szDelim);

            pszInput = OGRWktReadToken( pszInput, szDelim );
        }
        else if( *ppadfZ != NULL )
        {
            (*ppadfZ)[*pnPointsRead] = 0.0;
        }

        ++(*pnPointsRead);

/* -------------------------------------------------------------------- */
/*      Do we have a M coordinate?                                      */
/*      If we do, just skip it.                                         */
/* -------------------------------------------------------------------- */
        if( isdigit(szDelim[0]) || szDelim[0] == '-' || szDelim[0] == '.' )
        {
            pszInput = OGRWktReadToken( pszInput, szDelim );
        }

/* -------------------------------------------------------------------- */
/*      Read next delimiter ... it should be a comma if there are       */
/*      more points.                                                    */
/* -------------------------------------------------------------------- */
        if( szDelim[0] != ')' && szDelim[0] != ',' )
        {
            CPLDebug( "OGR",
                      "Corrupt input in OGRWktReadPoints().  "
                      "Got `%s' when expecting `,' or `)', near `%s' in %s.",
                      szDelim, pszInput, pszOrigInput );
            return NULL;
        }
    } while( szDelim[0] == ',' );

    return pszInput;
}

/************************************************************************/
/*                          OGRWktReadPointsM()                         */
/*                                                                      */
/*      Read a point string.  The point list must be contained in       */
/*      brackets and each point pair separated by a comma.              */
/************************************************************************/

const char * OGRWktReadPointsM( const char * pszInput,
                                OGRRawPoint ** ppaoPoints,
                                double **ppadfZ, double **ppadfM,
                                int * flags,
                                int * pnMaxPoints,
                                int * pnPointsRead )

{
    const char *pszOrigInput = pszInput;
    const bool bNoFlags =
        !(*flags & OGRGeometry::OGR_G_3D) &&
        !(*flags & OGRGeometry::OGR_G_MEASURED);
    *pnPointsRead = 0;

    if( pszInput == NULL )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Eat any leading white space.                                    */
/* -------------------------------------------------------------------- */
    while( *pszInput == ' ' || *pszInput == '\t' )
        ++pszInput;

/* -------------------------------------------------------------------- */
/*      If this isn't an opening bracket then we have a problem.        */
/* -------------------------------------------------------------------- */
    if( *pszInput != '(' )
    {
        CPLDebug( "OGR",
                  "Expected '(', but got %s in OGRWktReadPointsM().",
                  pszInput );

        return pszInput;
    }

    ++pszInput;

/* ==================================================================== */
/*      This loop reads a single point.  It will continue till we       */
/*      run out of well formed points, or a closing bracket is          */
/*      encountered.                                                    */
/* ==================================================================== */
    char szDelim[OGR_WKT_TOKEN_MAX] = {};

    do {
/* -------------------------------------------------------------------- */
/*      Read the X and Y values, verify they are numeric.               */
/* -------------------------------------------------------------------- */
        char szTokenX[OGR_WKT_TOKEN_MAX] = {};
        char szTokenY[OGR_WKT_TOKEN_MAX] = {};

        pszInput = OGRWktReadToken( pszInput, szTokenX );
        pszInput = OGRWktReadToken( pszInput, szTokenY );

        if( (!isdigit(szTokenX[0]) && szTokenX[0] != '-' && szTokenX[0] != '.' )
            || (!isdigit(szTokenY[0]) && szTokenY[0] != '-' &&
                szTokenY[0] != '.') )
            return NULL;

/* -------------------------------------------------------------------- */
/*      Do we need to grow the point list to hold this point?           */
/* -------------------------------------------------------------------- */
        if( *pnPointsRead == *pnMaxPoints )
        {
            *pnMaxPoints = *pnMaxPoints * 2 + 10;
            *ppaoPoints = static_cast<OGRRawPoint *>(
                CPLRealloc(*ppaoPoints, sizeof(OGRRawPoint) * *pnMaxPoints) );

            if( *ppadfZ != NULL )
            {
                *ppadfZ = static_cast<double *>(
                    CPLRealloc(*ppadfZ, sizeof(double) * *pnMaxPoints) );
            }

            if( *ppadfM != NULL )
            {
                *ppadfM = static_cast<double *>(
                    CPLRealloc(*ppadfM, sizeof(double) * *pnMaxPoints) );
            }
        }

/* -------------------------------------------------------------------- */
/*      Add point to list.                                              */
/* -------------------------------------------------------------------- */
        (*ppaoPoints)[*pnPointsRead].x = CPLAtof(szTokenX);
        (*ppaoPoints)[*pnPointsRead].y = CPLAtof(szTokenY);

/* -------------------------------------------------------------------- */
/*      Read the next token.                                            */
/* -------------------------------------------------------------------- */
        pszInput = OGRWktReadToken( pszInput, szDelim );

/* -------------------------------------------------------------------- */
/*      If there are unexpectedly more coordinates, they are Z.         */
/* -------------------------------------------------------------------- */

        if( !(*flags & OGRGeometry::OGR_G_3D) &&
            !(*flags & OGRGeometry::OGR_G_MEASURED) &&
            (isdigit(szDelim[0]) || szDelim[0] == '-' || szDelim[0] == '.' ))
        {
            *flags |= OGRGeometry::OGR_G_3D;
        }

/* -------------------------------------------------------------------- */
/*      Get Z if flag says so.                                          */
/*      Zero out possible remains from earlier strings.                 */
/* -------------------------------------------------------------------- */

        if( *flags & OGRGeometry::OGR_G_3D )
        {
            if( *ppadfZ == NULL )
            {
                *ppadfZ = static_cast<double *>(
                    CPLCalloc(sizeof(double), *pnMaxPoints) );
            }
            if( isdigit(szDelim[0]) || szDelim[0] == '-' || szDelim[0] == '.' )
            {
                (*ppadfZ)[*pnPointsRead] = CPLAtof(szDelim);
                pszInput = OGRWktReadToken( pszInput, szDelim );
            }
            else
            {
                (*ppadfZ)[*pnPointsRead] = 0.0;
            }
        }
        else if( *ppadfZ != NULL )
        {
            (*ppadfZ)[*pnPointsRead] = 0.0;
        }

/* -------------------------------------------------------------------- */
/*      If there are unexpectedly even more coordinates,                */
/*      they are discarded unless there were no flags originally.       */
/*      This is for backwards compatibility. Should this be an error?   */
/* -------------------------------------------------------------------- */

        if( !(*flags & OGRGeometry::OGR_G_MEASURED) &&
            (isdigit(szDelim[0]) || szDelim[0] == '-' || szDelim[0] == '.' ) )
        {
            if( bNoFlags )
            {
                *flags |= OGRGeometry::OGR_G_MEASURED;
            }
            else
            {
                pszInput = OGRWktReadToken( pszInput, szDelim );
            }
        }

/* -------------------------------------------------------------------- */
/*      Get M if flag says so.                                          */
/*      Zero out possible remains from earlier strings.                 */
/* -------------------------------------------------------------------- */

        if( *flags & OGRGeometry::OGR_G_MEASURED )
        {
            if( *ppadfM == NULL )
            {
                *ppadfM = static_cast<double *>(
                    CPLCalloc(sizeof(double), *pnMaxPoints) );
            }
            if( isdigit(szDelim[0]) || szDelim[0] == '-' || szDelim[0] == '.' )
            {
                (*ppadfM)[*pnPointsRead] = CPLAtof(szDelim);
                pszInput = OGRWktReadToken( pszInput, szDelim );
            }
            else
            {
                (*ppadfM)[*pnPointsRead] = 0.0;
            }
        }
        else if( *ppadfM != NULL )
        {
            (*ppadfM)[*pnPointsRead] = 0.0;
        }

/* -------------------------------------------------------------------- */
/*      If there are still more coordinates and we do not have Z        */
/*      then we have a case of flags == M and four coordinates.         */
/*      This is allowed in BNF.                                         */
/* -------------------------------------------------------------------- */

        if( !(*flags & OGRGeometry::OGR_G_3D) &&
            (isdigit(szDelim[0]) || szDelim[0] == '-' || szDelim[0] == '.') )
        {
            *flags |= OGRGeometry::OGR_G_3D;
            if( *ppadfZ == NULL )
            {
                *ppadfZ = static_cast<double *>(
                    CPLCalloc(sizeof(double), *pnMaxPoints) );
            }
            (*ppadfZ)[*pnPointsRead] = (*ppadfM)[*pnPointsRead];
            (*ppadfM)[*pnPointsRead] = CPLAtof(szDelim);
            pszInput = OGRWktReadToken( pszInput, szDelim );
        }

/* -------------------------------------------------------------------- */
/*      Increase points index.                                          */
/* -------------------------------------------------------------------- */
        ++(*pnPointsRead);

/* -------------------------------------------------------------------- */
/*      The next delimiter should be a comma or an ending bracket.      */
/* -------------------------------------------------------------------- */
        if( szDelim[0] != ')' && szDelim[0] != ',' )
        {
            CPLDebug( "OGR",
                      "Corrupt input in OGRWktReadPointsM()  "
                      "Got `%s' when expecting `,' or `)', near `%s' in %s.",
                      szDelim, pszInput, pszOrigInput );
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

void *OGRCalloc( size_t count, size_t size )

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
 * \fn OGRGeneralCmdLineProcessor(int, char***, int)
 * General utility option processing.
 *
 * This function is intended to provide a variety of generic commandline
 * options for all OGR commandline utilities.  It takes care of the following
 * commandline options:
 *
 *  --version: report version of GDAL in use.
 *  --license: report GDAL license info.
 *  --format [format]: report details of one format driver.
 *  --formats: report all format drivers configured.
 *  --optfile filename: expand an option file into the argument list.
 *  --config key value: set system configuration option.
 *  --debug [on/off/value]: set debug level.
 *  --pause: Pause for user input (allows time to attach debugger)
 *  --locale [locale]: Install a locale using setlocale() (debugging)
 *  --help-general: report detailed help on general options.
 *
 * The argument array is replaced "in place" and should be freed with
 * CSLDestroy() when no longer needed.  The typical usage looks something
 * like the following.  Note that the formats should be registered so that
 * the --formats option will work properly.
 *
 *  int main( int argc, char ** argv )
 *  {
 *    OGRRegisterAll();
 *
 *    argc = OGRGeneralCmdLineProcessor( argc, &argv, 0 );
 *    if( argc < 1 )
 *        exit( -argc );
 *
 * @param nArgc number of values in the argument list.
 * @param ppapszArgv pointer to the argument list array (will be updated in
 * place).
 * @param nOptions unused.
 *
 * @return updated nArgc argument count.  Return of 0 requests terminate
 * without error, return of -1 requests exit with error code.
 */

/**/
/**/

int OGRGeneralCmdLineProcessor( int nArgc, char ***ppapszArgv,
                                int /* nOptions */ )

{
    return GDALGeneralCmdLineProcessor( nArgc, ppapszArgv, GDAL_OF_VECTOR );
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
 *   YYYY-MM-DD HH:MM:SS[.sss]+nn
 *   or YYYY-MM-DDTHH:MM:SS[.sss]Z (ISO 8601 format)
 *
 * The seconds may also have a decimal portion (which is ignored).  And
 * just dates (YYYY-MM-DD) or just times (HH:MM:SS[.sss]) are also supported.
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

int OGRParseDate( const char *pszInput,
                  OGRField *psField,
                  CPL_UNUSED int nOptions )
{
    psField->Date.Year = 0;
    psField->Date.Month = 0;
    psField->Date.Day = 0;
    psField->Date.Hour = 0;
    psField->Date.Minute = 0;
    psField->Date.Second = 0;
    psField->Date.TZFlag = 0;
    psField->Date.Reserved = 0;

/* -------------------------------------------------------------------- */
/*      Do we have a date?                                              */
/* -------------------------------------------------------------------- */
    while( *pszInput == ' ' )
        ++pszInput;

    bool bGotSomething = false;
    if( strstr(pszInput,"-") != NULL || strstr(pszInput,"/") != NULL )
    {
        if( !(*pszInput == '-' || *pszInput == '+' ||
              (*pszInput >= '0' && *pszInput <= '9')) )
            return FALSE;
        int nYear = atoi(pszInput);
        if( nYear != static_cast<GInt16>(nYear) )
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Years < -32768 or > 32767 are not supported");
            return FALSE;
        }
        psField->Date.Year = static_cast<GInt16>(nYear);
        if( (pszInput[1] == '-' || pszInput[1] == '/' ) ||
            (pszInput[1] != '\0' &&
             (pszInput[2] == '-' || pszInput[2] == '/' )) )
        {
            if( psField->Date.Year < 100 && psField->Date.Year >= 30 )
                psField->Date.Year += 1900;
            else if( psField->Date.Year < 30 && psField->Date.Year >= 0 )
                psField->Date.Year += 2000;
        }

        if( *pszInput == '-' )
            ++pszInput;
        while( *pszInput >= '0' && *pszInput <= '9' )
            ++pszInput;
        if( *pszInput != '-' && *pszInput != '/' )
            return FALSE;
        else
            ++pszInput;

        psField->Date.Month = static_cast<GByte>(atoi(pszInput));
        if( psField->Date.Month <= 0 || psField->Date.Month > 12 )
            return FALSE;

        while( *pszInput >= '0' && *pszInput <= '9' )
            ++pszInput;
        if( *pszInput != '-' && *pszInput != '/' )
            return FALSE;
        else
            ++pszInput;

        psField->Date.Day = static_cast<GByte>(atoi(pszInput));
        if( psField->Date.Day <= 0 || psField->Date.Day > 31 )
            return FALSE;

        while( *pszInput >= '0' && *pszInput <= '9' )
            ++pszInput;

        bGotSomething = true;

        // If ISO 8601 format.
        if( *pszInput == 'T' )
            ++pszInput;
    }

/* -------------------------------------------------------------------- */
/*      Do we have a time?                                              */
/* -------------------------------------------------------------------- */
    while( *pszInput == ' ' )
        ++pszInput;

    if( strstr(pszInput, ":") != NULL )
    {
        psField->Date.Hour = static_cast<GByte>(atoi(pszInput));
        if( psField->Date.Hour > 23 )
            return FALSE;

        while( *pszInput >= '0' && *pszInput <= '9' )
            ++pszInput;
        if( *pszInput != ':' )
            return FALSE;
        else
            ++pszInput;

        psField->Date.Minute = static_cast<GByte>(atoi(pszInput));
        if( psField->Date.Minute > 59 )
            return FALSE;

        while( *pszInput >= '0' && *pszInput <= '9' )
            ++pszInput;
        if( *pszInput == ':' )
        {
            ++pszInput;

            psField->Date.Second = static_cast<float>(CPLAtof(pszInput));
            if( psField->Date.Second > 61 )
                return FALSE;

            while( (*pszInput >= '0' && *pszInput <= '9')
                || *pszInput == '.' )
            {
                ++pszInput;
            }

            // If ISO 8601 format.
            if( *pszInput == 'Z' )
            {
                psField->Date.TZFlag = 100;
            }
        }

        bGotSomething = true;
    }

    // No date or time!
    if( !bGotSomething )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Do we have a timezone?                                          */
/* -------------------------------------------------------------------- */
    while( *pszInput == ' ' )
        ++pszInput;

    if( *pszInput == '-' || *pszInput == '+' )
    {
        // +HH integral offset
        if( strlen(pszInput) <= 3 )
        {
            psField->Date.TZFlag = static_cast<GByte>(100 + atoi(pszInput) * 4);
        }
        else if( pszInput[3] == ':'  // +HH:MM offset
                 && atoi(pszInput + 4) % 15 == 0 )
        {
            psField->Date.TZFlag = (GByte)(100
                + atoi(pszInput + 1) * 4
                + (atoi(pszInput + 4) / 15));

            if( pszInput[0] == '-' )
                psField->Date.TZFlag = -1 * (psField->Date.TZFlag - 100) + 100;
        }
        else if( isdigit(pszInput[3]) && isdigit(pszInput[4])  // +HHMM offset
                 && atoi(pszInput + 3) % 15 == 0 )
        {
            psField->Date.TZFlag = (GByte)(100
                + static_cast<GByte>(CPLScanLong(pszInput + 1, 2)) * 4
                + (atoi(pszInput + 3) / 15));

            if( pszInput[0] == '-' )
                psField->Date.TZFlag = -1 * (psField->Date.TZFlag - 100) + 100;
        }
        else if( isdigit(pszInput[3]) && pszInput[4] == '\0'  // +HMM offset
                 && atoi(pszInput + 2) % 15 == 0 )
        {
            psField->Date.TZFlag = (GByte)(100
                + static_cast<GByte>(CPLScanLong(pszInput + 1, 1)) * 4
                + (atoi(pszInput + 2) / 15));

            if( pszInput[0] == '-' )
                psField->Date.TZFlag = -1 * (psField->Date.TZFlag - 100) + 100;
        }
        // otherwise ignore any timezone info.
    }

    return TRUE;
}

/************************************************************************/
/*                           OGRParseXMLDateTime()                      */
/************************************************************************/

int OGRParseXMLDateTime( const char* pszXMLDateTime,
                         OGRField* psField)
{
    int year = 0;
    int month = 0;
    int day = 0;
    int hour = 0;
    int minute = 0;
    int TZHour = 0;
    int TZMinute = 0;
    float second = 0;
    char c = '\0';
    int TZ = 0;
    bool bRet = false;

    // Date is expressed as a UTC date.
    if( sscanf(pszXMLDateTime, "%04d-%02d-%02dT%02d:%02d:%f%c",
               &year, &month, &day, &hour, &minute, &second, &c) == 7 &&
        c == 'Z' )
    {
        TZ = 100;
        bRet = true;
    }
    // Date is expressed as a UTC date, with a timezone.
    else if( sscanf(pszXMLDateTime, "%04d-%02d-%02dT%02d:%02d:%f%c%02d:%02d",
                    &year, &month, &day, &hour, &minute, &second, &c,
                    &TZHour, &TZMinute) == 9 &&
             (c == '+' || c == '-') )
    {
        TZ = 100 + ((c == '+') ? 1 : -1) * ((TZHour * 60 + TZMinute) / 15);
        bRet = true;
    }
    // Date is expressed into an unknown timezone.
    else if( sscanf(pszXMLDateTime, "%04d-%02d-%02dT%02d:%02d:%f",
                    &year, &month, &day, &hour, &minute, &second) == 6 )
    {
        TZ = 0;
        bRet = true;
    }
    // Date is expressed as a UTC date with only year:month:day.
    else if( sscanf(pszXMLDateTime, "%04d-%02d-%02d", &year, &month, &day) ==
             3 )
    {
        TZ = 0;
        bRet = true;
    }

    if( !bRet )
      return FALSE;

    psField->Date.Year = static_cast<GInt16>(year);
    psField->Date.Month = static_cast<GByte>(month);
    psField->Date.Day = static_cast<GByte>(day);
    psField->Date.Hour = static_cast<GByte>(hour);
    psField->Date.Minute = static_cast<GByte>(minute);
    psField->Date.Second = second;
    psField->Date.TZFlag = static_cast<GByte>(TZ);
    psField->Date.Reserved = 0;

    return TRUE;
}

/************************************************************************/
/*                      OGRParseRFC822DateTime()                        */
/************************************************************************/

static const char* const aszMonthStr[] = {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun",
    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };

int OGRParseRFC822DateTime( const char* pszRFC822DateTime, OGRField* psField )
{
    // Following
    // http://asg.web.cmu.edu/rfc/rfc822.html#sec-5 :
    // [Fri,] 28 Dec 2007 05:24[:17] GMT
    char** papszTokens =
        CSLTokenizeStringComplex( pszRFC822DateTime, " ,:", TRUE, FALSE );
    char** papszVal = papszTokens;
    bool bRet = false;
    int nTokens = CSLCount(papszTokens);
    if( nTokens < 6 )
    {
        CSLDestroy(papszTokens);
        return false;
    }

    if( !((*papszVal)[0] >= '0' && (*papszVal)[0] <= '9') )
    {
        // Ignore day of week.
        ++papszVal;
    }

    const int day = atoi(*papszVal);
    ++papszVal;

    int month = 0;

    for( int i = 0; i < 12; ++i )
    {
        if( EQUAL(*papszVal, aszMonthStr[i]) )
            month = i + 1;
    }
    ++papszVal;

    int year = atoi(*papszVal);
    ++papszVal;
    if( year < 100 && year >= 30 )
        year += 1900;
    else if( year < 30 && year >= 0 )
        year += 2000;

    const int hour = atoi(*papszVal);
    ++papszVal;

    const int minute = atoi(*papszVal);
    ++papszVal;

    int second = 0;
    if( *papszVal != NULL && (*papszVal)[0] >= '0' && (*papszVal)[0] <= '9' )
    {
        second = atoi(*papszVal);
        ++papszVal;
    }

    if( month != 0 )
    {
        bRet = true;
        int TZ = 0;

        if( *papszVal == NULL )
        {
        }
        else if( strlen(*papszVal) == 5 &&
                 ((*papszVal)[0] == '+' || (*papszVal)[0] == '-') )
        {
            char szBuf[3] = { (*papszVal)[1], (*papszVal)[2], 0 };
            const int TZHour = atoi(szBuf);
            szBuf[0] = (*papszVal)[3];
            szBuf[1] = (*papszVal)[4];
            szBuf[2] = 0;
            const int TZMinute = atoi(szBuf);
            TZ = 100 + (((*papszVal)[0] == '+') ? 1 : -1) *
                        ((TZHour * 60 + TZMinute) / 15);
        }
        else
        {
            const char* aszTZStr[] = {
                "GMT", "UT", "Z", "EST", "EDT", "CST", "CDT", "MST", "MDT",
                "PST", "PDT"
            };
            int anTZVal[] = { 0, 0, 0, -5, -4, -6, -5, -7, -6, -8, -7 };
            for( int i = 0; i < 11; ++i )
            {
                if( EQUAL(*papszVal, aszTZStr[i]) )
                {
                    TZ = 100 + anTZVal[i] * 4;
                    break;
                }
            }
        }

        psField->Date.Year = static_cast<GInt16>(year);
        psField->Date.Month = static_cast<GByte>(month);
        psField->Date.Day = static_cast<GByte>(day);
        psField->Date.Hour = static_cast<GByte>(hour);
        psField->Date.Minute = static_cast<GByte>(minute);
        psField->Date.Second = static_cast<float>(second);
        psField->Date.TZFlag = static_cast<GByte>(TZ);
        psField->Date.Reserved = 0;
    }

    CSLDestroy(papszTokens);
    return bRet;
}

/**
  * Returns the day of the week in Gregorian calendar
  *
  * @param day : day of the month, between 1 and 31
  * @param month : month of the year, between 1 (Jan) and 12 (Dec)
  * @param year : year

  * @return day of the week : 0 for Monday, ... 6 for Sunday
  */

int OGRGetDayOfWeek( int day, int month, int year )
{
    // Reference: Zeller's congruence.
    const int q = day;
    int m = month;
    if( month >=3 )
    {
        // m = month;
    }
    else
    {
        m = month + 12;
        year--;
    }
    const int K = year % 100;
    const int J = year / 100;
    const int h = ( q + (((m+1)*26)/10) + K + K/4 + J/4 + 5 * J) % 7;
    return ( h + 5 ) % 7;
}

/************************************************************************/
/*                         OGRGetRFC822DateTime()                       */
/************************************************************************/

char* OGRGetRFC822DateTime( const OGRField* psField )
{
    char* pszTZ = NULL;
    const char* aszDayOfWeek[] =
        { "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun" };

    int dayofweek = OGRGetDayOfWeek(psField->Date.Day, psField->Date.Month,
                                    psField->Date.Year);

    int month = psField->Date.Month;
    if( month < 1 || month > 12 )
        month = 1;

    int TZFlag = psField->Date.TZFlag;
    if( TZFlag == 0 || TZFlag == 100 )
    {
        pszTZ = CPLStrdup("GMT");
    }
    else
    {
        int TZOffset = std::abs(TZFlag - 100) * 15;
        int TZHour = TZOffset / 60;
        int TZMinute = TZOffset - TZHour * 60;
        pszTZ = CPLStrdup(CPLSPrintf("%c%02d%02d", TZFlag > 100 ? '+' : '-',
                                        TZHour, TZMinute));
    }
    char* pszRet = CPLStrdup(CPLSPrintf(
        "%s, %02d %s %04d %02d:%02d:%02d %s",
        aszDayOfWeek[dayofweek], psField->Date.Day, aszMonthStr[month - 1],
        psField->Date.Year, psField->Date.Hour,
        psField->Date.Minute, static_cast<int>(psField->Date.Second), pszTZ));
    CPLFree(pszTZ);
    return pszRet;
}

/************************************************************************/
/*                            OGRGetXMLDateTime()                       */
/************************************************************************/

char* OGRGetXMLDateTime(const OGRField* psField)
{
    const int year = psField->Date.Year;
    const int month = psField->Date.Month;
    const int day = psField->Date.Day;
    const int hour = psField->Date.Hour;
    const int minute = psField->Date.Minute;
    const float second = psField->Date.Second;
    const int TZFlag = psField->Date.TZFlag;

    char* pszRet = NULL;

    if( TZFlag == 0 || TZFlag == 100 )
    {
        if( OGR_GET_MS(second) )
            pszRet = CPLStrdup(CPLSPrintf(
                "%04d-%02d-%02dT%02d:%02d:%06.3fZ",
                year, month, day, hour, minute, second));
        else
            pszRet = CPLStrdup(CPLSPrintf(
                "%04d-%02d-%02dT%02d:%02d:%02dZ",
                year, month, day, hour, minute, static_cast<int>(second)));
    }
    else
    {
        const int TZOffset = std::abs(TZFlag - 100) * 15;
        const int TZHour = TZOffset / 60;
        const int TZMinute = TZOffset - TZHour * 60;
        if( OGR_GET_MS(second) )
            pszRet = CPLStrdup(CPLSPrintf(
                "%04d-%02d-%02dT%02d:%02d:%06.3f%c%02d:%02d",
                year, month, day, hour, minute, second,
                (TZFlag > 100) ? '+' : '-', TZHour, TZMinute));
        else
            pszRet = CPLStrdup(
                CPLSPrintf("%04d-%02d-%02dT%02d:%02d:%02d%c%02d:%02d",
                           year, month, day, hour, minute,
                           static_cast<int>(second),
                           TZFlag > 100 ? '+' : '-', TZHour, TZMinute));
    }
    return pszRet;
}

/************************************************************************/
/*                 OGRGetXML_UTF8_EscapedString()                       */
/************************************************************************/

char* OGRGetXML_UTF8_EscapedString(const char* pszString)
{
    char *pszEscaped = NULL;
    if( !CPLIsUTF8(pszString, -1) &&
         CPLTestBool(CPLGetConfigOption("OGR_FORCE_ASCII", "YES")) )
    {
        static bool bFirstTime = true;
        if( bFirstTime )
        {
            bFirstTime = false;
            CPLError(CE_Warning, CPLE_AppDefined,
                     "%s is not a valid UTF-8 string. Forcing it to ASCII.  "
                     "If you still want the original string and change the XML "
                     "file encoding afterwards, you can define "
                     "OGR_FORCE_ASCII=NO as configuration option.  "
                     "This warning won't be issued anymore", pszString);
        }
        else
        {
            CPLDebug("OGR",
                     "%s is not a valid UTF-8 string. Forcing it to ASCII",
                     pszString);
        }
        char* pszTemp = CPLForceToASCII(pszString, -1, '?');
        pszEscaped = CPLEscapeString( pszTemp, -1, CPLES_XML );
        CPLFree(pszTemp);
    }
    else
        pszEscaped = CPLEscapeString( pszString, -1, CPLES_XML );
    return pszEscaped;
}

/************************************************************************/
/*                        OGRCompareDate()                              */
/************************************************************************/

int OGRCompareDate( OGRField *psFirstTuple,
                    OGRField *psSecondTuple )
{
    // TODO: We ignore TZFlag.

    if( psFirstTuple->Date.Year < psSecondTuple->Date.Year )
        return -1;
    else if( psFirstTuple->Date.Year > psSecondTuple->Date.Year )
        return 1;

    if( psFirstTuple->Date.Month < psSecondTuple->Date.Month )
        return -1;
    else if( psFirstTuple->Date.Month > psSecondTuple->Date.Month )
        return 1;

    if( psFirstTuple->Date.Day < psSecondTuple->Date.Day )
        return -1;
    else if( psFirstTuple->Date.Day > psSecondTuple->Date.Day )
        return 1;

    if( psFirstTuple->Date.Hour < psSecondTuple->Date.Hour )
        return -1;
    else if( psFirstTuple->Date.Hour > psSecondTuple->Date.Hour )
        return 1;

    if( psFirstTuple->Date.Minute < psSecondTuple->Date.Minute )
        return -1;
    else if( psFirstTuple->Date.Minute > psSecondTuple->Date.Minute )
        return 1;

    if( psFirstTuple->Date.Second < psSecondTuple->Date.Second )
        return -1;
    else if( psFirstTuple->Date.Second > psSecondTuple->Date.Second )
        return 1;

    return 0;
}

/************************************************************************/
/*                        OGRFastAtof()                                 */
/************************************************************************/

// On Windows, CPLAtof() is very slow if the number is followed by other long
// content.  Just extract the number into a short string before calling
// CPLAtof() on it.
static
double OGRCallAtofOnShortString(const char* pszStr)
{
    const char* p = pszStr;
    while( *p == ' ' || *p == '\t' )
        ++p;

    char szTemp[128] = {};
    int nCounter = 0;
    while( *p == '+' ||
           *p == '-' ||
           (*p >= '0' && *p <= '9') ||
           *p == '.' ||
           (*p == 'e' || *p == 'E' || *p == 'd' || *p == 'D') )
    {
        szTemp[nCounter++] = *(p++);
        if( nCounter == 127 )
            return CPLAtof(pszStr);
    }
    szTemp[nCounter] = '\0';
    return CPLAtof(szTemp);
}

/** Same contract as CPLAtof, except than it doesn't always call the
 *  system CPLAtof() that may be slow on some platforms. For simple but
 *  common strings, it'll use a faster implementation (up to 20x faster
 *  than CPLAtof() on MS runtime libraries) that has no guaranty to return
 *  exactly the same floating point number.
 */

double OGRFastAtof(const char* pszStr)
{
    double dfVal = 0;
    double dfSign = 1.0;
    const char* p = pszStr;

    static const double adfTenPower[] =
    {
        1e0, 1e1, 1e2, 1e3, 1e4, 1e5, 1e6, 1e7, 1e8, 1e9, 1e10,
        1e11, 1e12, 1e13, 1e14, 1e15, 1e16, 1e17, 1e18, 1e19, 1e20,
        1e21, 1e22, 1e23, 1e24, 1e25, 1e26, 1e27, 1e28, 1e29, 1e30, 1e31
    };

    while( *p == ' ' || *p == '\t' )
        ++p;

    if( *p == '+' )
        ++p;
    else if( *p == '-' )
    {
        dfSign = -1.0;
        ++p;
    }

    while( true )
    {
        if( *p >= '0' && *p <= '9' )
        {
            dfVal = dfVal * 10.0 + (*p - '0');
            ++p;
        }
        else if( *p == '.' )
        {
            ++p;
            break;
        }
        else if( *p == 'e' || *p == 'E' || *p == 'd' || *p == 'D' )
            return OGRCallAtofOnShortString(pszStr);
        else
            return dfSign * dfVal;
    }

    unsigned int countFractionnal = 0;
    while( true )
    {
        if( *p >= '0' && *p <= '9' )
        {
            dfVal = dfVal * 10.0 + (*p - '0');
            ++countFractionnal;
            ++p;
        }
        else if( *p == 'e' || *p == 'E' || *p == 'd' || *p == 'D' )
            return OGRCallAtofOnShortString(pszStr);
        else
        {
            if( countFractionnal < sizeof(adfTenPower) /
                sizeof(adfTenPower[0]) )
                return dfSign * (dfVal / adfTenPower[countFractionnal]);
            else
                return OGRCallAtofOnShortString(pszStr);
        }
    }
}

/**
 * Check that panPermutation is a permutation of [0, nSize-1].
 * @param panPermutation an array of nSize elements.
 * @param nSize size of the array.
 * @return OGRERR_NONE if panPermutation is a permutation of [0, nSize - 1].
 * @since OGR 1.9.0
 */
OGRErr OGRCheckPermutation( int* panPermutation, int nSize )
{
    OGRErr eErr = OGRERR_NONE;
    int* panCheck = static_cast<int *>(CPLCalloc(nSize, sizeof(int)));
    for( int i = 0; i < nSize; ++i )
    {
        if( panPermutation[i] < 0 || panPermutation[i] >= nSize )
        {
            CPLError(CE_Failure, CPLE_IllegalArg,
                     "Bad value for element %d", i);
            eErr = OGRERR_FAILURE;
            break;
        }
        if( panCheck[panPermutation[i]] != 0 )
        {
            CPLError(CE_Failure, CPLE_IllegalArg,
                     "Array is not a permutation of [0,%d]",
                     nSize - 1);
            eErr = OGRERR_FAILURE;
            break;
        }
        panCheck[panPermutation[i]] = 1;
    }
    CPLFree(panCheck);
    return eErr;
}

OGRErr OGRReadWKBGeometryType( unsigned char * pabyData,
                               OGRwkbVariant eWkbVariant,
                               OGRwkbGeometryType *peGeometryType )
{
    if( !peGeometryType )
        return OGRERR_FAILURE;

/* -------------------------------------------------------------------- */
/*      Get the byte order byte.                                        */
/* -------------------------------------------------------------------- */
    int nByteOrder = DB2_V72_FIX_BYTE_ORDER(*pabyData);
    if( !( nByteOrder == wkbXDR || nByteOrder == wkbNDR ) )
        return OGRERR_CORRUPT_DATA;
    OGRwkbByteOrder eByteOrder = (OGRwkbByteOrder) nByteOrder;

/* -------------------------------------------------------------------- */
/*      Get the geometry type.                                          */
/* -------------------------------------------------------------------- */
    bool bIs3D = false;
    bool bIsMeasured = false;
    int iRawType = 0;

    memcpy(&iRawType, pabyData + 1, 4);
    if( OGR_SWAP(eByteOrder))
    {
        CPL_SWAP32PTR(&iRawType);
    }

    // Test for M bit in PostGIS WKB, see ogrgeometry.cpp:4956.
    if( 0x40000000 & iRawType )
    {
        iRawType &= ~0x40000000;
        bIsMeasured = true;
    }
    // Old-style OGC z-bit is flipped? Tests also Z bit in PostGIS WKB.
    if( wkb25DBitInternalUse & iRawType )
    {
        // Clean off top 3 bytes.
        iRawType &= 0x000000FF;
        bIs3D = true;
    }

    // ISO SQL/MM Part3 draft -> Deprecated.
    // See http://jtc1sc32.org/doc/N1101-1150/32N1107-WD13249-3--spatial.pdf
    if( iRawType == 1000001 )
        iRawType = wkbCircularString;
    else if( iRawType == 1000002 )
        iRawType = wkbCompoundCurve;
    else if( iRawType == 1000003 )
        iRawType = wkbCurvePolygon;
    else if( iRawType == 1000004 )
        iRawType = wkbMultiCurve;
    else if( iRawType == 1000005 )
        iRawType = wkbMultiSurface;
    else if( iRawType == 2000001 )
        iRawType = wkbPointZM;
    else if( iRawType == 2000002 )
        iRawType = wkbLineStringZM;
    else if( iRawType == 2000003 )
        iRawType = wkbCircularStringZM;
    else if( iRawType == 2000004 )
        iRawType = wkbCompoundCurveZM;
    else if( iRawType == 2000005 )
        iRawType = wkbPolygonZM;
    else if( iRawType == 2000006 )
        iRawType = wkbCurvePolygonZM;
    else if( iRawType == 2000007 )
        iRawType = wkbMultiPointZM;
    else if( iRawType == 2000008 )
        iRawType = wkbMultiCurveZM;
    else if( iRawType == 2000009 )
        iRawType = wkbMultiLineStringZM;
    else if( iRawType == 2000010 )
        iRawType = wkbMultiSurfaceZM;
    else if( iRawType == 2000011 )
        iRawType = wkbMultiPolygonZM;
    else if( iRawType == 2000012 )
        iRawType = wkbGeometryCollectionZM;
    else if( iRawType == 3000001 )
        iRawType = wkbPoint25D;
    else if( iRawType == 3000002 )
        iRawType = wkbLineString25D;
    else if( iRawType == 3000003 )
        iRawType = wkbCircularStringZ;
    else if( iRawType == 3000004 )
        iRawType = wkbCompoundCurveZ;
    else if( iRawType == 3000005 )
        iRawType = wkbPolygon25D;
    else if( iRawType == 3000006 )
        iRawType = wkbCurvePolygonZ;
    else if( iRawType == 3000007 )
        iRawType = wkbMultiPoint25D;
    else if( iRawType == 3000008 )
        iRawType = wkbMultiCurveZ;
    else if( iRawType == 3000009 )
        iRawType = wkbMultiLineString25D;
    else if( iRawType == 3000010 )
        iRawType = wkbMultiSurfaceZ;
    else if( iRawType == 3000011 )
        iRawType = wkbMultiPolygon25D;
    else if( iRawType == 3000012 )
        iRawType = wkbGeometryCollection25D;
    else if( iRawType == 4000001 )
        iRawType = wkbPointM;
    else if( iRawType == 4000002 )
        iRawType = wkbLineStringM;
    else if( iRawType == 4000003 )
        iRawType = wkbCircularStringM;
    else if( iRawType == 4000004 )
        iRawType = wkbCompoundCurveM;
    else if( iRawType == 4000005 )
        iRawType = wkbPolygonM;
    else if( iRawType == 4000006 )
        iRawType = wkbCurvePolygonM;
    else if( iRawType == 4000007 )
        iRawType = wkbMultiPointM;
    else if( iRawType == 4000008 )
        iRawType = wkbMultiCurveM;
    else if( iRawType == 4000009 )
        iRawType = wkbMultiLineStringM;
    else if( iRawType == 4000010 )
        iRawType = wkbMultiSurfaceM;
    else if( iRawType == 4000011 )
        iRawType = wkbMultiPolygonM;
    else if( iRawType == 4000012 )
        iRawType = wkbGeometryCollectionM;

    // Sometimes the Z flag is in the 2nd byte?
    if( iRawType & (wkb25DBitInternalUse >> 16) )
    {
        // Clean off top 3 bytes.
        iRawType &= 0x000000FF;
        bIs3D = true;
    }

    if( eWkbVariant == wkbVariantPostGIS1 )
    {
        if( iRawType == POSTGIS15_CURVEPOLYGON )
            iRawType = wkbCurvePolygon;
        else if( iRawType == POSTGIS15_MULTICURVE )
            iRawType = wkbMultiCurve;
        else if( iRawType == POSTGIS15_MULTISURFACE )
            iRawType = wkbMultiSurface;
    }

    if( bIs3D )
    {
        iRawType += 1000;
    }
    if( bIsMeasured )
    {
        iRawType += 2000;
    }

    // ISO SQL/MM style types are between 1-16, 1001-1016, 2001-2016, and
    // 3001-3016.
    if( !((iRawType > 0 && iRawType <= 16) ||
           (iRawType > 1000 && iRawType <= 1016) ||
           (iRawType > 2000 && iRawType <= 2016) ||
           (iRawType > 3000 && iRawType <= 3016)) )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Unsupported WKB type %d", iRawType);
        return OGRERR_UNSUPPORTED_GEOMETRY_TYPE;
    }

    // Convert to OGRwkbGeometryType value.
    if( iRawType >= 1001 && iRawType <= 1007 )
    {
        iRawType -= 1000;
        iRawType |= wkb25DBitInternalUse;
    }

    *peGeometryType = static_cast<OGRwkbGeometryType>(iRawType);

    return OGRERR_NONE;
}
