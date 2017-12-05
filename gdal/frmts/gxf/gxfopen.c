/******************************************************************************
 * $Id$
 *
 * Project:  GXF Reader
 * Purpose:  Majority of Geosoft GXF reading code.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1998, Global Geomatics
 * Copyright (c) 1998, Frank Warmerdam
 * Copyright (c) 2008-2012, Even Rouault <even dot rouault at mines-paris dot org>
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

#include "cpl_port.h"

#include <ctype.h>
#include "gxfopen.h"

CPL_CVSID("$Id$")


/* this is also defined in gdal.h which we avoid in this separable component */
#define CPLE_WrongFormat	200

#define MAX_LINE_COUNT_PER_HEADER       1000
#define MAX_HEADER_COUNT                1000

/************************************************************************/
/*                         GXFReadHeaderValue()                         */
/*                                                                      */
/*      Read one entry from the file header, and return it and its      */
/*      value in clean form.                                            */
/************************************************************************/

static char **GXFReadHeaderValue( VSILFILE * fp, char * pszHTitle )

{
    const char	*pszLine;
    char	**papszReturn = NULL;
    int		i;
    int     nLineCount = 0, nReturnLineCount = 0;
    int     bContinuedLine = FALSE;

/* -------------------------------------------------------------------- */
/*      Try to read a line.  If we fail or if this isn't a proper       */
/*      header value then return the failure.                           */
/* -------------------------------------------------------------------- */
    pszLine = CPLReadLineL( fp );
    if( pszLine == NULL )
    {
        strcpy( pszHTitle, "#EOF" );
        return( NULL );
    }

/* -------------------------------------------------------------------- */
/*      Extract the title.  It should be terminated by some sort of     */
/*      white space.                                                    */
/* -------------------------------------------------------------------- */
    for( i = 0; i < 70 && !isspace((unsigned char)pszLine[i]) && pszLine[i] != '\0'; i++ ) {}

    strncpy( pszHTitle, pszLine, i );
    pszHTitle[i] = '\0';

/* -------------------------------------------------------------------- */
/*      If this is #GRID, then return ... we are at the end of the      */
/*      header.                                                         */
/* -------------------------------------------------------------------- */
    if( EQUAL(pszHTitle,"#GRID") )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Skip white space.                                               */
/* -------------------------------------------------------------------- */
    while( isspace((unsigned char)pszLine[i]) )
        i++;

/* -------------------------------------------------------------------- */
/*    If we have reached the end of the line, try to read another line. */
/* -------------------------------------------------------------------- */
    if( pszLine[i] == '\0' )
    {
        pszLine = CPLReadLineL( fp );
        if( pszLine == NULL )
        {
            strcpy( pszHTitle, "#EOF" );
            return( NULL );
        }
    }

/* -------------------------------------------------------------------- */
/*      Keeping adding the value stuff as new lines till we reach a     */
/*      `#' mark at the beginning of a new line.                        */
/* -------------------------------------------------------------------- */
    do {
        vsi_l_offset    nCurPos;
        char            chNextChar = 0;
        char		*pszTrimmedLine;
        size_t      nLen = strlen(pszLine);

        /* Lines are supposed to be limited to 80 characters */
        if( nLen > 1024 )
        {
            CSLDestroy(papszReturn);
            return NULL;
        }

        pszTrimmedLine = CPLStrdup( pszLine );

        for( i = ((int)nLen)-1; i >= 0 && pszLine[i] == ' '; i-- )
            pszTrimmedLine[i] = '\0';

        if( bContinuedLine )
        {
            char* pszTmp = (char*) VSIMalloc(strlen(papszReturn[nReturnLineCount-1]) + strlen(pszTrimmedLine) + 1);
            if( pszTmp == NULL )
            {
                CSLDestroy(papszReturn);
                CPLFree(pszTrimmedLine);
                return NULL;
            }
            strcpy(pszTmp, papszReturn[nReturnLineCount-1]);
            if( pszTrimmedLine[0] == '\0' )
                pszTmp[strlen(papszReturn[nReturnLineCount-1]) - 1] = 0;
            else
                strcpy(pszTmp + (strlen(papszReturn[nReturnLineCount-1]) - 1), pszTrimmedLine);
            CPLFree(papszReturn[nReturnLineCount-1]);
            papszReturn[nReturnLineCount-1] = pszTmp;
        }
        else
        {
            papszReturn = CSLAddString( papszReturn, pszTrimmedLine );
            nReturnLineCount ++;
        }

        /* Is it a continued line ? */
        bContinuedLine = ( i >= 0 && pszTrimmedLine[i] == '\\' );

        CPLFree( pszTrimmedLine );

        nCurPos = VSIFTellL(fp);
        if( VSIFReadL(&chNextChar, 1, 1, fp) != 1 )
        {
            CSLDestroy(papszReturn);
            return NULL;
        }
        VSIFSeekL(fp, nCurPos, SEEK_SET);

        if( chNextChar == '#' )
            pszLine = NULL;
        else
        {
            pszLine = CPLReadLineL( fp );
            nLineCount ++;
        }
    } while( pszLine != NULL && nLineCount < MAX_LINE_COUNT_PER_HEADER );

    return( papszReturn );
}

/************************************************************************/
/*                              GXFOpen()                               */
/************************************************************************/

/**
 * Open a GXF file, and collect contents of the header.
 *
 * @param pszFilename the name of the file to open.
 *
 * @return a handle for use with other GXF functions to access the file.  This
 * will be NULL if the access fails.
 */

GXFHandle GXFOpen( const char * pszFilename )

{
    VSILFILE	*fp;
    GXFInfo_t	*psGXF;
    char	szTitle[71];
    char	**papszList;
    int     nHeaderCount = 0;

/* -------------------------------------------------------------------- */
/*      We open in binary to ensure that we can efficiently seek()      */
/*      to any location when reading scanlines randomly.  If we         */
/*      opened as text we might still be able to seek(), but I          */
/*      believe that on Windows, the C library has to read through      */
/*      all the data to find the right spot taking into account DOS     */
/*      CRs.                                                            */
/* -------------------------------------------------------------------- */
    fp = VSIFOpenL( pszFilename, "rb" );

    if( fp == NULL )
    {
        /* how to effectively communicate this error out? */
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "Unable to open file: %s\n", pszFilename );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Create the GXF Information object.                              */
/* -------------------------------------------------------------------- */
    psGXF = (GXFInfo_t *) VSICalloc( sizeof(GXFInfo_t), 1 );
    psGXF->fp = fp;
    psGXF->dfTransformScale = 1.0;
    psGXF->nSense = GXFS_LL_RIGHT;
    psGXF->dfXPixelSize = 1.0;
    psGXF->dfYPixelSize = 1.0;
    psGXF->dfSetDummyTo = -1e12;

    psGXF->dfUnitToMeter = 1.0;
    psGXF->pszTitle = VSIStrdup("");

/* -------------------------------------------------------------------- */
/*      Read the header, one line at a time.                            */
/* -------------------------------------------------------------------- */
    while( (papszList = GXFReadHeaderValue( fp, szTitle)) != NULL && nHeaderCount < MAX_HEADER_COUNT )
    {
        if( STARTS_WITH_CI(szTitle, "#TITL") )
        {
            CPLFree( psGXF->pszTitle );
            psGXF->pszTitle = CPLStrdup( papszList[0] );
        }
        else if( STARTS_WITH_CI(szTitle, "#POIN") )
        {
            psGXF->nRawXSize = atoi(papszList[0]);
        }
        else if( STARTS_WITH_CI(szTitle, "#ROWS") )
        {
            psGXF->nRawYSize = atoi(papszList[0]);
        }
        else if( STARTS_WITH_CI(szTitle, "#PTSE") )
        {
            psGXF->dfXPixelSize = CPLAtof(papszList[0]);
        }
        else if( STARTS_WITH_CI(szTitle, "#RWSE") )
        {
            psGXF->dfYPixelSize = CPLAtof(papszList[0]);
        }
        else if( STARTS_WITH_CI(szTitle, "#DUMM") )
        {
            memset( psGXF->szDummy, 0, sizeof(psGXF->szDummy));
            strncpy( psGXF->szDummy, papszList[0], sizeof(psGXF->szDummy) - 1);
            psGXF->dfSetDummyTo = CPLAtof(papszList[0]);
        }
        else if( STARTS_WITH_CI(szTitle, "#XORI") )
        {
            psGXF->dfXOrigin = CPLAtof(papszList[0]);
        }
        else if( STARTS_WITH_CI(szTitle, "#YORI") )
        {
            psGXF->dfYOrigin = CPLAtof(papszList[0]);
        }
        else if( STARTS_WITH_CI(szTitle, "#ZMIN") )
        {
            psGXF->dfZMinimum = CPLAtof(papszList[0]);
        }
        else if( STARTS_WITH_CI(szTitle, "#ZMAX") )
        {
            psGXF->dfZMaximum = CPLAtof(papszList[0]);
        }
        else if( STARTS_WITH_CI(szTitle, "#SENS") )
        {
            psGXF->nSense = atoi(papszList[0]);
        }
        else if( STARTS_WITH_CI(szTitle,"#MAP_PROJECTION") &&
                 psGXF->papszMapProjection == NULL )
        {
            psGXF->papszMapProjection = papszList;
            papszList = NULL;
        }
        else if( STARTS_WITH_CI(szTitle,"#MAP_D") &&
                 psGXF->papszMapDatumTransform == NULL  )
        {
            psGXF->papszMapDatumTransform = papszList;
            papszList = NULL;
        }
        else if( STARTS_WITH_CI(szTitle, "#UNIT") &&
                 psGXF->pszUnitName == NULL )
        {
            char	**papszFields;

            papszFields = CSLTokenizeStringComplex( papszList[0], ", ",
                                                    TRUE, TRUE );

            if( CSLCount(papszFields) > 1 )
            {
                psGXF->pszUnitName = VSIStrdup( papszFields[0] );
                psGXF->dfUnitToMeter = CPLAtof( papszFields[1] );
                if( psGXF->dfUnitToMeter == 0.0 )
                    psGXF->dfUnitToMeter = 1.0;
            }

            CSLDestroy( papszFields );
        }
        else if( STARTS_WITH_CI(szTitle, "#TRAN") &&
                 psGXF->pszTransformName == NULL )
        {
            char	**papszFields;

            papszFields = CSLTokenizeStringComplex( papszList[0], ", ",
                                                    TRUE, TRUE );

            if( CSLCount(papszFields) > 1 )
            {
                psGXF->dfTransformScale = CPLAtof(papszFields[0]);
                psGXF->dfTransformOffset = CPLAtof(papszFields[1]);
            }

            if( CSLCount(papszFields) > 2 )
                psGXF->pszTransformName = CPLStrdup( papszFields[2] );

            CSLDestroy( papszFields );
        }
        else if( STARTS_WITH_CI(szTitle,"#GTYPE") )
        {
            psGXF->nGType = atoi(papszList[0]);
            if( psGXF->nGType < 0 || psGXF->nGType > 20 )
            {
                CSLDestroy( papszList );
                GXFClose( psGXF );
                return NULL;
            }
        }

        CSLDestroy( papszList );
        nHeaderCount ++;
    }

    CSLDestroy( papszList );

/* -------------------------------------------------------------------- */
/*      Did we find the #GRID?                                          */
/* -------------------------------------------------------------------- */
    if( !STARTS_WITH_CI(szTitle, "#GRID") )
    {
        GXFClose( psGXF );
        CPLError( CE_Failure, CPLE_WrongFormat,
                  "Didn't parse through to #GRID successfully in.\n"
                  "file `%s'.\n",
                  pszFilename );

        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Allocate, and initialize the raw scanline offset array.         */
/* -------------------------------------------------------------------- */
    if( psGXF->nRawYSize <= 0 || psGXF->nRawYSize >= INT_MAX )
    {
        GXFClose( psGXF );
        return NULL;
    }

    /* Avoid excessive memory allocation */
    if( psGXF->nRawYSize >= 1000000 )
    {
        vsi_l_offset nCurOffset;
        vsi_l_offset nFileSize;
        nCurOffset = VSIFTellL( psGXF->fp );
        VSIFSeekL( psGXF->fp, 0, SEEK_END );
        nFileSize = VSIFTellL( psGXF->fp );
        VSIFSeekL( psGXF->fp, nCurOffset, SEEK_SET );
        if( (vsi_l_offset)psGXF->nRawYSize > nFileSize )
        {
            GXFClose( psGXF );
            return NULL;
        }
    }

    psGXF->panRawLineOffset = (vsi_l_offset *)
        VSICalloc( sizeof(vsi_l_offset), psGXF->nRawYSize+1 );
    if( psGXF->panRawLineOffset == NULL )
    {
        GXFClose( psGXF );
        return NULL;
    }

    psGXF->panRawLineOffset[0] = VSIFTellL( psGXF->fp );

/* -------------------------------------------------------------------- */
/*      Update the zmin/zmax values to take into account #TRANSFORM     */
/*      information.                                                    */
/* -------------------------------------------------------------------- */
    if( psGXF->dfZMinimum != 0.0 || psGXF->dfZMaximum != 0.0 )
    {
        psGXF->dfZMinimum = (psGXF->dfZMinimum * psGXF->dfTransformScale)
            			+ psGXF->dfTransformOffset;
        psGXF->dfZMaximum = (psGXF->dfZMaximum * psGXF->dfTransformScale)
            			+ psGXF->dfTransformOffset;
    }

    return( (GXFHandle) psGXF );
}

/************************************************************************/
/*                              GXFClose()                              */
/************************************************************************/

/**
 * Close GXF file opened with GXFOpen().
 *
 * @param hGXF handle to GXF file.
 */

void GXFClose( GXFHandle hGXF )

{
    GXFInfo_t	*psGXF = (GXFInfo_t *) hGXF;

    CPLFree( psGXF->panRawLineOffset );
    CPLFree( psGXF->pszUnitName );
    CSLDestroy( psGXF->papszMapDatumTransform );
    CSLDestroy( psGXF->papszMapProjection );
    CPLFree( psGXF->pszTitle );
    CPLFree( psGXF->pszTransformName );

    VSIFCloseL( psGXF->fp );

    CPLReadLineL( NULL );

    CPLFree( psGXF );
}

/************************************************************************/
/*                           GXFParseBase90()                           */
/*                                                                      */
/*      Parse a base 90 number ... exceptions (repeat, and dummy)       */
/*      values have to be recognised outside this function.             */
/************************************************************************/

static
double GXFParseBase90( GXFInfo_t * psGXF, const char * pszText,
                       int bScale )

{
    int		i = 0;
    unsigned int nValue = 0;

    while( i < psGXF->nGType )
    {
        nValue = nValue*90U + (unsigned)(pszText[i] - 37);
        i++;
    }

    if( bScale )
        return( (nValue * psGXF->dfTransformScale) + psGXF->dfTransformOffset);
    else
        return( nValue );
}


/************************************************************************/
/*                       GXFReadRawScanlineFrom()                       */
/************************************************************************/

static CPLErr GXFReadRawScanlineFrom( GXFInfo_t * psGXF, vsi_l_offset iOffset,
                                      vsi_l_offset * pnNewOffset, double * padfLineBuf )

{
    const char	*pszLine;
    int		nValuesRead = 0, nValuesSought = psGXF->nRawXSize;

    if( VSIFSeekL( psGXF->fp, iOffset, SEEK_SET ) != 0 )
        return CE_Failure;

    while( nValuesRead < nValuesSought )
    {
        pszLine = CPLReadLineL( psGXF->fp );
        if( pszLine == NULL )
            break;

/* -------------------------------------------------------------------- */
/*      Uncompressed case.                                              */
/* -------------------------------------------------------------------- */
        if( psGXF->nGType == 0 )
        {
            /* we could just tokenize the line, but that's pretty expensive.
               Instead I will parse on white space ``by hand''. */
            while( *pszLine != '\0' && nValuesRead < nValuesSought )
            {
                int		i;

                /* skip leading white space */
                for( ; isspace((unsigned char)*pszLine); pszLine++ ) {}

                /* Skip the data value (non white space) */
                for( i = 0; pszLine[i] != '\0' && !isspace((unsigned char)pszLine[i]); i++) {}

                if( strncmp(pszLine,psGXF->szDummy,i) == 0 )
                {
                    padfLineBuf[nValuesRead++] = psGXF->dfSetDummyTo;
                }
                else
                {
                    padfLineBuf[nValuesRead++] = CPLAtof(pszLine);
                }

                /* skip further whitespace */
                for( pszLine += i; isspace((unsigned char)*pszLine); pszLine++ ) {}
            }
        }

/* -------------------------------------------------------------------- */
/*      Compressed case.                                                */
/* -------------------------------------------------------------------- */
        else
        {
            size_t nLineLenOri = strlen(pszLine);
            int nLineLen = (int)nLineLenOri;

            while( *pszLine != '\0' && nValuesRead < nValuesSought )
            {
                if( nLineLen < psGXF->nGType )
                    return CE_Failure;

                if( pszLine[0] == '!' )
                {
                    padfLineBuf[nValuesRead++] = psGXF->dfSetDummyTo;
                }
                else if( pszLine[0] == '"' )
                {
                    int		nCount, i;
                    double	dfValue;

                    pszLine += psGXF->nGType;
                    nLineLen -= psGXF->nGType;
                    if( nLineLen < psGXF->nGType )
                    {
                        pszLine = CPLReadLineL( psGXF->fp );
                        if( pszLine == NULL )
                            return CE_Failure;
                        nLineLenOri = strlen(pszLine);
                        nLineLen = (int)nLineLenOri;
                        if( nLineLen < psGXF->nGType )
                            return CE_Failure;
                    }

                    nCount = (int) GXFParseBase90( psGXF, pszLine, FALSE);
                    pszLine += psGXF->nGType;
                    nLineLen -= psGXF->nGType;

                    if( nLineLen < psGXF->nGType )
                    {
                        pszLine = CPLReadLineL( psGXF->fp );
                        if( pszLine == NULL )
                            return CE_Failure;
                        nLineLenOri = strlen(pszLine);
                        nLineLen = (int)nLineLenOri;
                        if( nLineLen < psGXF->nGType )
                            return CE_Failure;
                    }

                    if( *pszLine == '!' )
                        dfValue = psGXF->dfSetDummyTo;
                    else
                        dfValue = GXFParseBase90( psGXF, pszLine, TRUE );

                    if( nValuesRead + nCount > nValuesSought )
                    {
                        CPLError(CE_Failure, CPLE_AppDefined, "Wrong count value");
                        return CE_Failure;
                    }

                    for( i=0; i < nCount && nValuesRead < nValuesSought; i++ )
                        padfLineBuf[nValuesRead++] = dfValue;
                }
                else
                {
                    padfLineBuf[nValuesRead++] =
                        GXFParseBase90( psGXF, pszLine, TRUE );
                }

                pszLine += psGXF->nGType;
                nLineLen -= psGXF->nGType;
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Return the new offset, if requested.                            */
/* -------------------------------------------------------------------- */
    if( pnNewOffset != NULL )
    {
        *pnNewOffset = VSIFTellL( psGXF->fp );
    }

    return CE_None;
}

/************************************************************************/
/*                           GXFGetScanline()                           */
/************************************************************************/

/**
 * Read a scanline of raster data from GXF file.
 *
 * This function operates similarly to GXFGetRawScanline(), but it
 * attempts to mirror data horizontally or vertically based on the #SENSE
 * flag to return data in a top to bottom, and left to right organization.
 * If the file is organized in columns (#SENSE is GXFS_UR_DOWN, GXFS_UL_DOWN,
 * GXFS_LR_UP, or GXFS_LL_UP) then this function will fail, returning
 * CE_Failure, and reporting a sense error.
 *
 * See GXFGetRawScanline() for other notes.
 *
 * @param hGXF the GXF file handle, as returned from GXFOpen().
 * @param iScanline the scanline to read, zero is the top scanline.
 * @param padfLineBuf a buffer of doubles into which the scanline pixel
 * values are read.  This must be at least as long as a scanline.
 *
 * @return CE_None if access succeeds or CE_Failure if something goes wrong.
 */

CPLErr GXFGetScanline( GXFHandle hGXF, int iScanline, double * padfLineBuf )

{
    GXFInfo_t	*psGXF = (GXFInfo_t *) hGXF;
    CPLErr	nErr;
    int		iRawScanline;

    if( psGXF->nSense == GXFS_LL_RIGHT
        || psGXF->nSense == GXFS_LR_LEFT )
    {
        iRawScanline = psGXF->nRawYSize - iScanline - 1;
    }

    else if( psGXF->nSense == GXFS_UL_RIGHT
             || psGXF->nSense == GXFS_UR_LEFT )
    {
        iRawScanline = iScanline;
    }
    else
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Unable to support vertically oriented images." );
        return( CE_Failure );
    }

    nErr = GXFGetRawScanline( hGXF, iRawScanline, padfLineBuf );

    if( nErr == CE_None
        && (psGXF->nSense == GXFS_LR_LEFT || psGXF->nSense == GXFS_UR_LEFT) )
    {
        int	i;
        double	dfTemp;

        for( i = psGXF->nRawXSize / 2 - 1; i >= 0; i-- )
        {
            dfTemp = padfLineBuf[i];
            padfLineBuf[i] = padfLineBuf[psGXF->nRawXSize-i-1];
            padfLineBuf[psGXF->nRawXSize-i-1] = dfTemp;
        }
    }

    return( nErr );
}

/************************************************************************/
/*                         GXFGetRawScanline()                          */
/************************************************************************/

/**
 * Read a scanline of raster data from GXF file.
 *
 * This function will read a row of data from the GXF file.  It is "Raw"
 * in the sense that it doesn't attempt to account for the #SENSE flag as
 * the GXFGetScanline() function does.  Unlike GXFGetScanline(), this function
 * supports column organized files.
 *
 * Any dummy pixels are assigned the dummy value indicated by GXFGetRawInfo().
 *
 * @param hGXF the GXF file handle, as returned from GXFOpen().
 * @param iScanline the scanline to read, zero is the first scanline in the
 * file.
 * @param padfLineBuf a buffer of doubles into which the scanline pixel
 * values are read.  This must be at least as long as a scanline.
 *
 * @return CE_None if access succeeds or CE_Failure if something goes wrong.
 */

CPLErr GXFGetRawScanline( GXFHandle hGXF, int iScanline, double * padfLineBuf )

{
    GXFInfo_t	*psGXF = (GXFInfo_t *) hGXF;
    CPLErr	eErr;

/* -------------------------------------------------------------------- */
/*      Validate scanline.                                              */
/* -------------------------------------------------------------------- */
    if( iScanline < 0 || iScanline >= psGXF->nRawYSize )
    {
        CPLError( CE_Failure, CPLE_IllegalArg,
                  "GXFGetRawScanline(): Scanline `%d' does not exist.\n",
                  iScanline );
        return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      If we don't have the requested scanline, fetch preceding        */
/*      scanlines to find the pointer to this scanline.                 */
/* -------------------------------------------------------------------- */
    if( psGXF->panRawLineOffset[iScanline] == 0 )
    {
        int		i;

        CPLAssert( iScanline > 0 );

        for( i = 0; i < iScanline; i++ )
        {
            if( psGXF->panRawLineOffset[i+1] == 0 )
            {
                eErr = GXFGetRawScanline( hGXF, i, padfLineBuf );
                if( eErr != CE_None )
                    return( eErr );
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Get this scanline, and update the offset for the next line.     */
/* -------------------------------------------------------------------- */
    eErr =
        GXFReadRawScanlineFrom( psGXF, psGXF->panRawLineOffset[iScanline],
                                psGXF->panRawLineOffset+iScanline+1,
                                padfLineBuf );

    return eErr;
}

/************************************************************************/
/*                         GXFScanForZMinMax()                          */
/*                                                                      */
/*      The header doesn't contain the ZMin/ZMax values, but the        */
/*      application has requested it ... scan the entire image for      */
/*      it.                                                             */
/************************************************************************/

static void GXFScanForZMinMax( GXFHandle hGXF )

{
    GXFInfo_t	*psGXF = (GXFInfo_t *) hGXF;
    int		iLine, iPixel;
    double	*padfScanline;

    padfScanline = (double *) VSICalloc(sizeof(double),psGXF->nRawXSize);
    if( padfScanline == NULL )
        return;

    psGXF->dfZMinimum = 1e50;
    psGXF->dfZMaximum = -1e50;

    for( iLine = 0; iLine < psGXF->nRawYSize; iLine++ )
    {
        if( GXFGetRawScanline( hGXF, iLine, padfScanline ) != CE_None )
            break;

        for( iPixel = 0; iPixel < psGXF->nRawXSize; iPixel++ )
        {
            if( padfScanline[iPixel] != psGXF->dfSetDummyTo )
            {
                psGXF->dfZMinimum =
                    MIN(psGXF->dfZMinimum,padfScanline[iPixel]);
                psGXF->dfZMaximum =
                    MAX(psGXF->dfZMaximum,padfScanline[iPixel]);
            }
        }
    }

    VSIFree( padfScanline );

/* -------------------------------------------------------------------- */
/*      Did we get any real data points?                                */
/* -------------------------------------------------------------------- */
    if( psGXF->dfZMinimum > psGXF->dfZMaximum )
    {
        psGXF->dfZMinimum = 0.0;
        psGXF->dfZMaximum = 0.0;
    }
}

/************************************************************************/
/*                             GXFGetRawInfo()                          */
/************************************************************************/

/**
 * Fetch header information about a GXF file.
 *
 * Note that the X and Y sizes are of the raw raster and don't take into
 * account the #SENSE flag.  If the file is column oriented (rows in the
 * files are actually columns in the raster) these values would need to be
 * transposed for the actual raster.
 *
 * The legal pnSense values are:
 * <ul>
 * <li> GXFS_LL_UP(-1): lower left origin, scanning up.
 * <li> GXFS_LL_RIGHT(1): lower left origin, scanning right.
 * <li> GXFS_UL_RIGHT(-2): upper left origin, scanning right.
 * <li> GXFS_UL_DOWN(2): upper left origin, scanning down.
 * <li> GXFS_UR_DOWN(-3): upper right origin, scanning down.
 * <li> GXFS_UR_LEFT(3): upper right origin, scanning left.
 * <li> GXFS_LR_LEFT(-4): lower right origin, scanning left.
 * <li> GXFS_LR_UP(4): lower right origin, scanning up.
 * </ul>
 *
 * Note that the GXFGetScanline() function attempts to provide a GXFS_UL_RIGHT
 * view onto files, but doesn't handle the *_DOWN and *_UP oriented files.
 *
 * The Z min and max values may not occur in the GXF header.  If they are
 * requested, and aren't available in the header the entire file is scanned
 * in order to establish them.  This can be expensive.
 *
 * If no #DUMMY value was specified in the file, a default of -1e12 is used.
 *
 * @param hGXF handle to GXF file returned by GXFOpen().
 * @param pnXSize int to be set with the width of the raw raster.  May be NULL.
 * @param pnYSize int to be set with the height of the raw raster. May be NULL.
 * @param pnSense int to set with #SENSE flag, may be NULL.
 * @param pdfZMin double to set with minimum raster value, may be NULL.
 * @param pdfZMax double to set with minimum raster value, may be NULL.
 * @param pdfDummy double to set with dummy (nodata / invalid data) pixel
 * value.
 */

CPLErr GXFGetRawInfo( GXFHandle hGXF, int *pnXSize, int *pnYSize,
                      int * pnSense, double * pdfZMin, double * pdfZMax,
                      double * pdfDummy )

{
    GXFInfo_t	*psGXF = (GXFInfo_t *) hGXF;

    if( pnXSize != NULL )
        *pnXSize = psGXF->nRawXSize;

    if( pnYSize != NULL )
        *pnYSize = psGXF->nRawYSize;

    if( pnSense != NULL )
        *pnSense = psGXF->nSense;

    if( (pdfZMin != NULL || pdfZMax != NULL)
        && psGXF->dfZMinimum == 0.0 && psGXF->dfZMaximum == 0.0 )
    {
        GXFScanForZMinMax( hGXF );
    }

    if( pdfZMin != NULL )
        *pdfZMin = psGXF->dfZMinimum;

    if( pdfZMax != NULL )
        *pdfZMax = psGXF->dfZMaximum;

    if( pdfDummy != NULL )
        *pdfDummy = psGXF->dfSetDummyTo;

    return( CE_None );
}

/************************************************************************/
/*                        GXFGetMapProjection()                         */
/************************************************************************/

/**
 * Return the lines related to the map projection.  It is up to
 * the caller to parse them and interpret.  The return result
 * will be NULL if no #MAP_PROJECTION line was found in the header.
 *
 * @param hGXF the GXF file handle.
 *
 * @return a NULL terminated array of string pointers containing the
 * projection, or NULL.  The strings remained owned by the GXF API, and
 * should not be modified or freed by the caller.
 */

char **GXFGetMapProjection( GXFHandle hGXF )

{
    return( ((GXFInfo_t *) hGXF)->papszMapProjection );
}

/************************************************************************/
/*                      GXFGetMapDatumTransform()                       */
/************************************************************************/

/**
 * Return the lines related to the datum transformation.  It is up to
 * the caller to parse them and interpret.  The return result
 * will be NULL if no #MAP_DATUM_TRANSFORM line was found in the header.
 *
 * @param hGXF the GXF file handle.
 *
 * @return a NULL terminated array of string pointers containing the
 * datum, or NULL.  The strings remained owned by the GXF API, and
 * should not be modified or freed by the caller.
 */

char **GXFGetMapDatumTransform( GXFHandle hGXF )

{
    return( ((GXFInfo_t *) hGXF)->papszMapDatumTransform );
}

/************************************************************************/
/*                         GXFGetRawPosition()                          */
/************************************************************************/

/**
 * Get the raw grid positioning information.
 *
 * Note that these coordinates refer to the raw grid, and are in the units
 * specified by the #UNITS field.  See GXFGetPosition() for a similar
 * function that takes into account the #SENSE values similarly to
 * GXFGetScanline().
 *
 * Note that the pixel values are considered to be point values in GXF,
 * and thus the origin is for the first point.  If you consider the pixels
 * to be areas, then the origin is for the center of the origin pixel, not
 * the outer corner.
 *
 * @param hGXF the GXF file handle.
 * @param pdfXOrigin X position of the origin in the base coordinate system.
 * @param pdfYOrigin Y position of the origin in the base coordinate system.
 * @param pdfXPixelSize X pixel size in base coordinates.
 * @param pdfYPixelSize Y pixel size in base coordinates.
 * @param pdfRotation rotation in degrees counter-clockwise from the
 * base coordinate system.
 *
 * @return Returns CE_None if successful, or CE_Failure if no posiitioning
 * information was found in the file.
 */


CPLErr GXFGetRawPosition( GXFHandle hGXF,
                          double * pdfXOrigin, double * pdfYOrigin,
                          double * pdfXPixelSize, double * pdfYPixelSize,
                          double * pdfRotation )

{
    GXFInfo_t	*psGXF = (GXFInfo_t *) hGXF;

    if( pdfXOrigin != NULL )
        *pdfXOrigin = psGXF->dfXOrigin;
    if( pdfYOrigin != NULL )
        *pdfYOrigin = psGXF->dfYOrigin;
    if( pdfXPixelSize != NULL )
        *pdfXPixelSize = psGXF->dfXPixelSize;
    if( pdfYPixelSize != NULL )
        *pdfYPixelSize = psGXF->dfYPixelSize;
    if( pdfRotation != NULL )
        *pdfRotation = psGXF->dfRotation;

    if( psGXF->dfXOrigin == 0.0 && psGXF->dfYOrigin == 0.0
        && psGXF->dfXPixelSize == 0.0 && psGXF->dfYPixelSize == 0.0 )
        return( CE_Failure );
    else
        return( CE_None );
}


/************************************************************************/
/*                           GXFGetPosition()                           */
/************************************************************************/

/**
 * Get the grid positioning information.
 *
 * Note that these coordinates refer to the grid positioning after taking
 * into account the #SENSE flag (as is done by the GXFGetScanline()) function.
 *
 * Note that the pixel values are considered to be point values in GXF,
 * and thus the origin is for the first point.  If you consider the pixels
 * to be areas, then the origin is for the center of the origin pixel, not
 * the outer corner.
 *
 * This function does not support vertically oriented images, nor does it
 * properly transform rotation for images with a SENSE other than
 * GXFS_UL_RIGHT.
 *
 * @param hGXF the GXF file handle.
 * @param pdfXOrigin X position of the origin in the base coordinate system.
 * @param pdfYOrigin Y position of the origin in the base coordinate system.
 * @param pdfXPixelSize X pixel size in base coordinates.
 * @param pdfYPixelSize Y pixel size in base coordinates.
 * @param pdfRotation rotation in degrees counter-clockwise from the
 * base coordinate system.
 *
 * @return Returns CE_None if successful, or CE_Failure if no posiitioning
 * information was found in the file.
 */


CPLErr GXFGetPosition( GXFHandle hGXF,
                       double * pdfXOrigin, double * pdfYOrigin,
                       double * pdfXPixelSize, double * pdfYPixelSize,
                       double * pdfRotation )

{
    GXFInfo_t	*psGXF = (GXFInfo_t *) hGXF;
    double	dfCXOrigin, dfCYOrigin, dfCXPixelSize, dfCYPixelSize;

    switch( psGXF->nSense )
    {
      case GXFS_UL_RIGHT:
        dfCXOrigin = psGXF->dfXOrigin;
        dfCYOrigin = psGXF->dfYOrigin;
        dfCXPixelSize = psGXF->dfXPixelSize;
        dfCYPixelSize = psGXF->dfYPixelSize;
        break;

      case GXFS_UR_LEFT:
        dfCXOrigin = psGXF->dfXOrigin
            	     - (psGXF->nRawXSize-1) * psGXF->dfXPixelSize;
        dfCYOrigin = psGXF->dfYOrigin;
        dfCXPixelSize = psGXF->dfXPixelSize;
        dfCYPixelSize = psGXF->dfYPixelSize;
        break;

      case GXFS_LL_RIGHT:
        dfCXOrigin = psGXF->dfXOrigin;
        dfCYOrigin = psGXF->dfYOrigin
                     + (psGXF->nRawYSize-1) * psGXF->dfYPixelSize;
        dfCXPixelSize = psGXF->dfXPixelSize;
        dfCYPixelSize = psGXF->dfYPixelSize;
        break;

      case GXFS_LR_LEFT:
        dfCXOrigin = psGXF->dfXOrigin
            	     - (psGXF->nRawXSize-1) * psGXF->dfXPixelSize;
        dfCYOrigin = psGXF->dfYOrigin
                     + (psGXF->nRawYSize-1) * psGXF->dfYPixelSize;
        dfCXPixelSize = psGXF->dfXPixelSize;
        dfCYPixelSize = psGXF->dfYPixelSize;
        break;

      default:
        CPLError( CE_Failure, CPLE_AppDefined,
           "GXFGetPosition() doesn't support vertically organized images." );
        return CE_Failure;
    }

    if( pdfXOrigin != NULL )
        *pdfXOrigin = dfCXOrigin;
    if( pdfYOrigin != NULL )
        *pdfYOrigin = dfCYOrigin;
    if( pdfXPixelSize != NULL )
        *pdfXPixelSize = dfCXPixelSize;
    if( pdfYPixelSize != NULL )
        *pdfYPixelSize = dfCYPixelSize;
    if( pdfRotation != NULL )
        *pdfRotation = psGXF->dfRotation;

    if( psGXF->dfXOrigin == 0.0 && psGXF->dfYOrigin == 0.0
        && psGXF->dfXPixelSize == 0.0 && psGXF->dfYPixelSize == 0.0 )
        return( CE_Failure );
    else
        return( CE_None );
}
