/******************************************************************************
 * $Id$
 *
 * Project:  DTED Translator
 * Purpose:  Implementation of DTED/CDED access functions.
 * Author:   Frank Warmerdam, warmerda@home.com
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
 * Revision 1.4  2000/07/07 14:20:57  warmerda
 * fixed AVOID_CPL support
 *
 * Revision 1.3  2000/01/24 13:58:25  warmerda
 * support rb access
 *
 * Revision 1.2  2000/01/12 19:25:57  warmerda
 * Dale Lutz provided fixes to avoid calling DTEDGetField() more than
 * onces within an expression, and to ensure values evaluated with
 * atoi() don't have leading zeros to avoid treatment as octal.
 *
 * Revision 1.1  1999/12/07 18:01:28  warmerda
 * New
 */

#include "dted_api.h"

/************************************************************************/
/*                            DTEDGetField()                            */
/*                                                                      */
/*      Extract a field as a zero terminated string.  Address is        */
/*      deliberately 1 based so the getfield arguments will be the      */
/*      same as the numbers in the file format specification.           */
/************************************************************************/

static
const char *DTEDGetField( const char *pachRecord, int nStart, int nSize )

{
    static char szResult[81];

    CPLAssert( nSize < sizeof(szResult) );
    memcpy( szResult, pachRecord + nStart - 1, nSize );
    szResult[nSize] = '\0';

    return szResult;
}

/************************************************************************/
/*                         StripLeadingZeros()                          */
/*                                                                      */
/*      Return a pointer to the first non-zero character in BUF.        */
/*      BUF must be null terminated.                                    */
/*      If buff is all zeros, then it will point to the last non-zero   */
/************************************************************************/

static const char* stripLeadingZeros(const char* buf)
{
    const char* ptr = buf;

    /* Go until we run out of characters  or hit something non-zero */

    while( *ptr == '0' && *(ptr+1) != '\0' )
    {
        ptr++;
    }

    return ptr;
}

/************************************************************************/
/*                              DTEDOpen()                              */
/************************************************************************/

DTEDInfo * DTEDOpen( const char * pszFilename,
                     const char * pszAccess,
                     int bTestOpen )

{
    FILE        *fp;
    char        achRecord[DTED_UHL_SIZE];
    DTEDInfo    *psDInfo = NULL;
    double      dfLLOriginX, dfLLOriginY;
    int deg = 0;
    int min = 0;
    int sec = 0;

/* -------------------------------------------------------------------- */
/*      Open the physical file.                                         */
/* -------------------------------------------------------------------- */
    if( EQUAL(pszAccess,"r") || EQUAL(pszAccess,"rb") )
        pszAccess = "rb";
    else
        pszAccess = "r+b";
    
    fp = VSIFOpen( pszFilename, pszAccess );

    if( fp == NULL )
    {
#ifndef AVOID_CPL
        if( !bTestOpen )
        {
            CPLError( CE_Failure, CPLE_OpenFailed,
                      "Failed to open file %s.",
                      pszFilename );
        }
#endif

        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Read, trying to find the UHL record.  Skip VOL or HDR           */
/*      records if they are encountered.                                */
/* -------------------------------------------------------------------- */
    do
    {
        if( VSIFRead( achRecord, 1, DTED_UHL_SIZE, fp ) != DTED_UHL_SIZE )
        {
#ifndef AVOID_CPL
            if( !bTestOpen )
                CPLError( CE_Failure, CPLE_OpenFailed,
                          "Unable to read header, %s is not DTED.",
                          pszFilename );
#endif
            VSIFClose( fp );
            return NULL;
        }

    } while( EQUALN(achRecord,"VOL",3) || EQUALN(achRecord,"HDR",3) );

    if( !EQUALN(achRecord,"UHL",3) )
    {
#ifndef AVOID_CPL
        if( !bTestOpen )
            CPLError( CE_Failure, CPLE_OpenFailed,
                      "No UHL record.  %s is not a DTED file.",
                      pszFilename );
#endif
        
        VSIFClose( fp );
        return NULL;
    }
    
/* -------------------------------------------------------------------- */
/*      Create and initialize the DTEDInfo structure.                   */
/* -------------------------------------------------------------------- */
    psDInfo = (DTEDInfo *) CPLCalloc(1,sizeof(DTEDInfo));

    psDInfo->fp = fp;
    
    psDInfo->nXSize = atoi(DTEDGetField(achRecord,48,4));
    psDInfo->nYSize = atoi(DTEDGetField(achRecord,52,4));

    psDInfo->pachUHLRecord = (char *) CPLMalloc(DTED_UHL_SIZE);
    memcpy( psDInfo->pachUHLRecord, achRecord, DTED_UHL_SIZE );

    psDInfo->pachDSIRecord = (char *) CPLMalloc(DTED_DSI_SIZE);
    VSIFRead( psDInfo->pachDSIRecord, 1, DTED_DSI_SIZE, fp );
    
    psDInfo->pachACCRecord = (char *) CPLMalloc(DTED_ACC_SIZE);
    VSIFRead( psDInfo->pachACCRecord, 1, DTED_ACC_SIZE, fp );

    if( !EQUALN(psDInfo->pachDSIRecord,"DSI",3)
        || !EQUALN(psDInfo->pachACCRecord,"ACC",3) )
    {
#ifndef AVOID_CPL
        CPLError( CE_Failure, CPLE_OpenFailed,
#else
        fprintf( stderr, 
#endif
                 "DSI or ACC record missing.  DTED access to\n%s failed.",
                 pszFilename );
        
        VSIFClose( fp );
        return NULL;
    }

    psDInfo->nDataOffset = VSIFTell( fp );

/* -------------------------------------------------------------------- */
/*      Parse out position information.  Note that we are extracting    */
/*      the top left corner of the top left pixel area, not the         */
/*      center of the area.                                             */
/* -------------------------------------------------------------------- */
    psDInfo->dfPixelSizeX =
        atoi(DTEDGetField(achRecord,21,4)) / 36000.0;

    psDInfo->dfPixelSizeY =
        atoi(DTEDGetField(achRecord,25,4)) / 36000.0;

    /* create a scope so I don't need to declare these up top */
    deg = atoi(stripLeadingZeros(DTEDGetField(achRecord,5,3)));
    min = atoi(stripLeadingZeros(DTEDGetField(achRecord,8,2)));
    sec = atoi(stripLeadingZeros(DTEDGetField(achRecord,10,2)));

    dfLLOriginX = deg + min / 60.0 + sec / 3600.0;
    if( achRecord[11] == 'W' )
        dfLLOriginX *= -1;

    deg = atoi(stripLeadingZeros(DTEDGetField(achRecord,13,3)));
    min = atoi(stripLeadingZeros(DTEDGetField(achRecord,16,2)));
    sec = atoi(stripLeadingZeros(DTEDGetField(achRecord,18,2)));

    dfLLOriginY = deg + min / 60.0 + sec / 3600.0;
    if( achRecord[19] == 'S' )
        dfLLOriginY *= -1;

    psDInfo->dfULCornerX = dfLLOriginX - 0.5 * psDInfo->dfPixelSizeX;
    psDInfo->dfULCornerY = dfLLOriginY - 0.5 * psDInfo->dfPixelSizeY
        + psDInfo->nYSize * psDInfo->dfPixelSizeY;

    return psDInfo;
}

/************************************************************************/
/*                          DTEDReadProfile()                           */
/*                                                                      */
/*      Read one profile line.  These are organized in bottom to top    */
/*      order starting from the leftmost column (0).                    */
/************************************************************************/

int DTEDReadProfile( DTEDInfo * psDInfo, int nColumnOffset,
                     GInt16 * panData )

{
    int         nOffset;
    int         i;
    GByte       *pabyRecord;

/* -------------------------------------------------------------------- */
/*      Read data record from disk.                                     */
/* -------------------------------------------------------------------- */
    pabyRecord = (GByte *) CPLMalloc(12 + psDInfo->nYSize*2);
    
    nOffset = psDInfo->nDataOffset + nColumnOffset * (12+psDInfo->nYSize*2);

    if( VSIFSeek( psDInfo->fp, nOffset, SEEK_SET ) != 0
        || VSIFRead( pabyRecord, (12+psDInfo->nYSize*2), 1, psDInfo->fp ) != 1)
    {
#ifndef AVOID_CPL
        CPLError( CE_Failure, CPLE_FileIO,
#else
        fprintf( stderr, 
#endif
                  "Failed to seek to, or read profile %d at offset %d\n"
                  "in DTED file.\n",
                  nColumnOffset, nOffset );
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Translate data values from "signed magnitude" to standard       */
/*      binary.                                                         */
/* -------------------------------------------------------------------- */
    for( i = 0; i < psDInfo->nYSize; i++ )
    {
        panData[i] = (pabyRecord[8+i*2] & 0x7f) * 256 + pabyRecord[8+i*2+1];

        if( pabyRecord[8+i*2] & 0x80 )
            panData[i] *= -1;
    }

    CPLFree( pabyRecord );

    return TRUE;
}

/************************************************************************/
/*                             DTEDClose()                              */
/************************************************************************/

void DTEDClose( DTEDInfo * psDInfo )

{
    VSIFClose( psDInfo->fp );

    CPLFree( psDInfo->pachUHLRecord );
    CPLFree( psDInfo->pachDSIRecord );
    CPLFree( psDInfo->pachACCRecord );
    
    CPLFree( psDInfo );
}
