/******************************************************************************
 * $Id$
 *
 * Project:  DTED Translator
 * Purpose:  Implementation of DTED/CDED access functions.
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
 ****************************************************************************/

#include "dted_api.h"

#ifndef AVOID_CPL
CPL_CVSID("$Id$");
#endif

static int bWarnedTwoComplement = FALSE;


/************************************************************************/
/*                            DTEDGetField()                            */
/*                                                                      */
/*      Extract a field as a zero terminated string.  Address is        */
/*      deliberately 1 based so the getfield arguments will be the      */
/*      same as the numbers in the file format specification.           */
/************************************************************************/

static
char *DTEDGetField( char szResult[81], const char *pachRecord, int nStart, int nSize )

{
    CPLAssert( nSize < 81 );
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
    int bSwapLatLong = FALSE;
    char szResult[81];

/* -------------------------------------------------------------------- */
/*      Open the physical file.                                         */
/* -------------------------------------------------------------------- */
    if( EQUAL(pszAccess,"r") || EQUAL(pszAccess,"rb") )
        pszAccess = "rb";
    else
        pszAccess = "r+b";
    
    fp = VSIFOpenL( pszFilename, pszAccess );

    if( fp == NULL )
    {
        if( !bTestOpen )
        {
#ifndef AVOID_CPL
            CPLError( CE_Failure, CPLE_OpenFailed,
#else
            fprintf( stderr, 
#endif
                      "Failed to open file %s.",
                      pszFilename );
        }

        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Read, trying to find the UHL record.  Skip VOL or HDR           */
/*      records if they are encountered.                                */
/* -------------------------------------------------------------------- */
    do
    {
        if( VSIFReadL( achRecord, 1, DTED_UHL_SIZE, fp ) != DTED_UHL_SIZE )
        {
            if( !bTestOpen )
            {
#ifndef AVOID_CPL
               CPLError( CE_Failure, CPLE_OpenFailed,
#else
               fprintf( stderr, 
#endif
                          "Unable to read header, %s is not DTED.",
                          pszFilename );
            }
            VSIFCloseL( fp );
            return NULL;
        }

    } while( EQUALN(achRecord,"VOL",3) || EQUALN(achRecord,"HDR",3) );

    if( !EQUALN(achRecord,"UHL",3) )
    {
        if( !bTestOpen )
        {
#ifndef AVOID_CPL
            CPLError( CE_Failure, CPLE_OpenFailed,
#else
            fprintf( stderr, 
#endif
                      "No UHL record.  %s is not a DTED file.",
                      pszFilename );
        }
        VSIFCloseL( fp );
        return NULL;
    }
    
/* -------------------------------------------------------------------- */
/*      Create and initialize the DTEDInfo structure.                   */
/* -------------------------------------------------------------------- */
    psDInfo = (DTEDInfo *) CPLCalloc(1,sizeof(DTEDInfo));

    psDInfo->fp = fp;

    psDInfo->bUpdate = EQUAL(pszAccess,"r+b");
    
    psDInfo->nXSize = atoi(DTEDGetField(szResult,achRecord,48,4));
    psDInfo->nYSize = atoi(DTEDGetField(szResult,achRecord,52,4));

    psDInfo->nUHLOffset = (int)VSIFTellL( fp ) - DTED_UHL_SIZE;
    psDInfo->pachUHLRecord = (char *) CPLMalloc(DTED_UHL_SIZE);
    memcpy( psDInfo->pachUHLRecord, achRecord, DTED_UHL_SIZE );

    psDInfo->nDSIOffset = (int)VSIFTellL( fp );
    psDInfo->pachDSIRecord = (char *) CPLMalloc(DTED_DSI_SIZE);
    VSIFReadL( psDInfo->pachDSIRecord, 1, DTED_DSI_SIZE, fp );
    
    psDInfo->nACCOffset = (int)VSIFTellL( fp );
    psDInfo->pachACCRecord = (char *) CPLMalloc(DTED_ACC_SIZE);
    VSIFReadL( psDInfo->pachACCRecord, 1, DTED_ACC_SIZE, fp );

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
        
        VSIFCloseL( fp );
        return NULL;
    }

    psDInfo->nDataOffset = (int)VSIFTellL( fp );

/* -------------------------------------------------------------------- */
/*      Parse out position information.  Note that we are extracting    */
/*      the top left corner of the top left pixel area, not the         */
/*      center of the area.                                             */
/* -------------------------------------------------------------------- */
    psDInfo->dfPixelSizeX =
        atoi(DTEDGetField(szResult,achRecord,21,4)) / 36000.0;

    psDInfo->dfPixelSizeY =
        atoi(DTEDGetField(szResult,achRecord,25,4)) / 36000.0;

    /* create a scope so I don't need to declare these up top */
    deg = atoi(stripLeadingZeros(DTEDGetField(szResult,achRecord,5,3)));
    min = atoi(stripLeadingZeros(DTEDGetField(szResult,achRecord,8,2)));
    sec = atoi(stripLeadingZeros(DTEDGetField(szResult,achRecord,10,2)));

    /* NOTE : The first version of MIL-D-89020 was buggy.
       The latitude and longitude of the LL cornder of the UHF record was inverted.
       This was fixed in MIL-D-89020 Amendement 1, but some products may be affected.
       We detect this situation by looking at N/S in the longitude field and
       E/W in the latitude one
    */

    dfLLOriginX = deg + min / 60.0 + sec / 3600.0;
    if( achRecord[11] == 'W' )
        dfLLOriginX *= -1;
    else if ( achRecord[11] == 'N' )
        bSwapLatLong = TRUE;
    else if ( achRecord[11] == 'S' )
    {
        dfLLOriginX *= -1;
        bSwapLatLong = TRUE;
    }

    deg = atoi(stripLeadingZeros(DTEDGetField(szResult,achRecord,13,3)));
    min = atoi(stripLeadingZeros(DTEDGetField(szResult,achRecord,16,2)));
    sec = atoi(stripLeadingZeros(DTEDGetField(szResult,achRecord,18,2)));

    dfLLOriginY = deg + min / 60.0 + sec / 3600.0;
    if( achRecord[19] == 'S' || (bSwapLatLong && achRecord[19] == 'W'))
        dfLLOriginY *= -1;

    if (bSwapLatLong)
    {
        double dfTmp = dfLLOriginX;
        dfLLOriginX = dfLLOriginY;
        dfLLOriginY = dfTmp;
    }

    psDInfo->dfULCornerX = dfLLOriginX - 0.5 * psDInfo->dfPixelSizeX;
    psDInfo->dfULCornerY = dfLLOriginY - 0.5 * psDInfo->dfPixelSizeY
        + psDInfo->nYSize * psDInfo->dfPixelSizeY;

    return psDInfo;
}



/************************************************************************/
/*                            DTEDReadPoint()                           */
/*                                                                      */
/*      Read one single sample. The coordinates are given from the      */
/*      top-left corner of the file (contrary to the internal           */
/*      organisation or a DTED file)                                    */
/************************************************************************/

int DTEDReadPoint( DTEDInfo * psDInfo, int nXOff, int nYOff, GInt16* panVal)
{
    int nOffset;
    GByte pabyData[2];

    if (nYOff < 0 || nXOff < 0 || nYOff >= psDInfo->nYSize || nXOff >= psDInfo->nXSize)
    {
#ifndef AVOID_CPL
        CPLError( CE_Failure, CPLE_AppDefined,
#else
        fprintf( stderr, 
#endif
                  "Invalid raster coordinates (%d,%d) in DTED file.\n", nXOff, nYOff);
        return FALSE;
    }

    nOffset = psDInfo->nDataOffset + nXOff * (12+psDInfo->nYSize*2) + 8 + 2 * (psDInfo->nYSize-1-nYOff);
    if( VSIFSeekL( psDInfo->fp, nOffset, SEEK_SET ) != 0
        || VSIFReadL( pabyData, 2, 1, psDInfo->fp ) != 1)
    {
#ifndef AVOID_CPL
        CPLError( CE_Failure, CPLE_FileIO,
#else
        fprintf( stderr, 
#endif
                  "Failed to seek to, or read (%d,%d) at offset %d\n"
                  "in DTED file.\n",
                  nXOff, nYOff, nOffset );
        return FALSE;
    }

    *panVal = ((pabyData[0] & 0x7f) << 8) | pabyData[1];

    if( pabyData[0] & 0x80 )
    {
        *panVal *= -1;

        /*
        ** It seems that some files are improperly generated in twos
        ** complement form for negatives.  For these, redo the job
        ** in twos complement.  eg. w_069_s50.dt0
        */
        if(( *panVal < -16000 ) && (*panVal != DTED_NODATA_VALUE))
        {
            *panVal = (pabyData[0] << 8) | pabyData[1];

            if( !bWarnedTwoComplement )
            {
                bWarnedTwoComplement = TRUE;
#ifndef AVOID_CPL
                CPLError( CE_Warning, CPLE_AppDefined,
#else
                fprintf( stderr,
#endif
                            "The DTED driver found values less than -16000, and has adjusted\n"
                            "them assuming they are improperly two-complemented.  No more warnings\n"
                            "will be issued in this session about this operation." );
            }
        }
    }

    return TRUE;
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
    return DTEDReadProfileEx( psDInfo, nColumnOffset, panData, FALSE);
}

int DTEDReadProfileEx( DTEDInfo * psDInfo, int nColumnOffset,
                       GInt16 * panData, int bVerifyChecksum )
{
    int         nOffset;
    int         i;
    GByte       *pabyRecord;

/* -------------------------------------------------------------------- */
/*      Read data record from disk.                                     */
/* -------------------------------------------------------------------- */
    pabyRecord = (GByte *) CPLMalloc(12 + psDInfo->nYSize*2);
    
    nOffset = psDInfo->nDataOffset + nColumnOffset * (12+psDInfo->nYSize*2);

    if( VSIFSeekL( psDInfo->fp, nOffset, SEEK_SET ) != 0
        || VSIFReadL( pabyRecord, (12+psDInfo->nYSize*2), 1, psDInfo->fp ) != 1)
    {
#ifndef AVOID_CPL
        CPLError( CE_Failure, CPLE_FileIO,
#else
        fprintf( stderr, 
#endif
                  "Failed to seek to, or read profile %d at offset %d\n"
                  "in DTED file.\n",
                  nColumnOffset, nOffset );
        CPLFree( pabyRecord );
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Translate data values from "signed magnitude" to standard       */
/*      binary.                                                         */
/* -------------------------------------------------------------------- */
    for( i = 0; i < psDInfo->nYSize; i++ )
    {
        panData[i] = ((pabyRecord[8+i*2] & 0x7f) << 8) | pabyRecord[8+i*2+1];

        if( pabyRecord[8+i*2] & 0x80 )
        {
            panData[i] *= -1;

            /*
            ** It seems that some files are improperly generated in twos
            ** complement form for negatives.  For these, redo the job
            ** in twos complement.  eg. w_069_s50.dt0
            */
            if(( panData[i] < -16000 ) && (panData[i] != DTED_NODATA_VALUE))
            {
                panData[i] = (pabyRecord[8+i*2] << 8) | pabyRecord[8+i*2+1];

                if( !bWarnedTwoComplement )
                {
                    bWarnedTwoComplement = TRUE;
#ifndef AVOID_CPL
                    CPLError( CE_Warning, CPLE_AppDefined,
#else
                    fprintf( stderr,
#endif
                              "The DTED driver found values less than -16000, and has adjusted\n"
                              "them assuming they are improperly two-complemented.  No more warnings\n"
                              "will be issued in this session about this operation." );
                }
            }
        }
    }

    if (bVerifyChecksum)
    {
        unsigned int nCheckSum = 0;
        unsigned int fileCheckSum;

        /* -------------------------------------------------------------------- */
        /*      Verify the checksum.                                            */
        /* -------------------------------------------------------------------- */

        for( i = 0; i < psDInfo->nYSize*2 + 8; i++ )
            nCheckSum += pabyRecord[i];

        fileCheckSum = (pabyRecord[8+psDInfo->nYSize*2+0] << 24) |
                        (pabyRecord[8+psDInfo->nYSize*2+1] << 16) |
                        (pabyRecord[8+psDInfo->nYSize*2+2] << 8) |
                        pabyRecord[8+psDInfo->nYSize*2+3];

        if ((GIntBig)fileCheckSum > (GIntBig)(0xff * (8+psDInfo->nYSize*2)))
        {
            static int bWarned = FALSE;
            if (! bWarned)
            {
                bWarned = TRUE;
#ifndef AVOID_CPL
                CPLError( CE_Warning, CPLE_AppDefined,
#else
                fprintf( stderr,
#endif
                            "The DTED driver has read from the file a checksum "
                            "with an impossible value (0x%X) at column %d.\n"
                            "Check with your file producer.\n"
                            "No more warnings will be issued in this session about this operation.",
                            fileCheckSum, nColumnOffset);
            }
        }
        else if (fileCheckSum != nCheckSum)
        {
#ifndef AVOID_CPL
            CPLError( CE_Warning, CPLE_AppDefined,
#else
            fprintf( stderr,
#endif
                      "The DTED driver has found a computed and read checksum "
                      "that do not match at column %d.\n",
                      nColumnOffset);
            CPLFree( pabyRecord );
            return FALSE;
        }
    }

    CPLFree( pabyRecord );

    return TRUE;
}

/************************************************************************/
/*                          DTEDWriteProfile()                          */
/************************************************************************/

int DTEDWriteProfile( DTEDInfo * psDInfo, int nColumnOffset,
                     GInt16 * panData )

{
    int         nOffset;
    int         i, nCheckSum = 0;
    GByte       *pabyRecord;

/* -------------------------------------------------------------------- */
/*      Format the data record.                                         */
/* -------------------------------------------------------------------- */
    pabyRecord = (GByte *) CPLMalloc(12 + psDInfo->nYSize*2);
    
    for( i = 0; i < psDInfo->nYSize; i++ )
    {
        int     nABSVal = ABS(panData[psDInfo->nYSize-i-1]);
        pabyRecord[8+i*2] = (GByte) ((nABSVal >> 8) & 0x7f);
        pabyRecord[8+i*2+1] = (GByte) (nABSVal & 0xff);

        if( panData[psDInfo->nYSize-i-1] < 0 )
            pabyRecord[8+i*2] |= 0x80;
    }

    pabyRecord[0] = 0xaa;
    pabyRecord[1] = 0;
    pabyRecord[2] = (GByte) (nColumnOffset / 256);
    pabyRecord[3] = (GByte) (nColumnOffset % 256);
    pabyRecord[4] = (GByte) (nColumnOffset / 256);
    pabyRecord[5] = (GByte) (nColumnOffset % 256);
    pabyRecord[6] = 0;
    pabyRecord[7] = 0;

/* -------------------------------------------------------------------- */
/*      Compute the checksum.                                           */
/* -------------------------------------------------------------------- */
    for( i = 0; i < psDInfo->nYSize*2 + 8; i++ )
        nCheckSum += pabyRecord[i];

    pabyRecord[8+psDInfo->nYSize*2+0] = (GByte) ((nCheckSum >> 24) & 0xff);
    pabyRecord[8+psDInfo->nYSize*2+1] = (GByte) ((nCheckSum >> 16) & 0xff);
    pabyRecord[8+psDInfo->nYSize*2+2] = (GByte) ((nCheckSum >> 8) & 0xff);
    pabyRecord[8+psDInfo->nYSize*2+3] = (GByte) (nCheckSum & 0xff);

/* -------------------------------------------------------------------- */
/*      Write the record.                                               */
/* -------------------------------------------------------------------- */
    nOffset = psDInfo->nDataOffset + nColumnOffset * (12+psDInfo->nYSize*2);

    if( VSIFSeekL( psDInfo->fp, nOffset, SEEK_SET ) != 0
        || VSIFWriteL( pabyRecord,(12+psDInfo->nYSize*2),1,psDInfo->fp ) != 1)
    {
        CPLFree( pabyRecord );
#ifndef AVOID_CPL
        CPLError( CE_Failure, CPLE_FileIO,
#else
        fprintf( stderr, 
#endif
                  "Failed to seek to, or write profile %d at offset %d\n"
                  "in DTED file.\n",
                  nColumnOffset, nOffset );
        CPLFree( pabyRecord );
        return FALSE;
    }

    CPLFree( pabyRecord );

    return TRUE;
    
}

/************************************************************************/
/*                      DTEDGetMetadataLocation()                       */
/************************************************************************/

static void DTEDGetMetadataLocation( DTEDInfo *psDInfo, 
                                     DTEDMetaDataCode eCode, 
                                     char **ppszLocation, int *pnLength )
{
    switch( eCode )
    {
      case DTEDMD_ORIGINLONG:
        *ppszLocation = psDInfo->pachUHLRecord + 4;
        *pnLength = 8;
        break;

      case DTEDMD_ORIGINLAT:
        *ppszLocation = psDInfo->pachUHLRecord + 12;
        *pnLength = 8;
        break;

      case DTEDMD_VERTACCURACY_UHL:
        *ppszLocation = psDInfo->pachUHLRecord + 28;
        *pnLength = 4;
        break;

      case DTEDMD_SECURITYCODE_UHL:
        *ppszLocation = psDInfo->pachUHLRecord + 32;
        *pnLength = 3;
        break;

      case DTEDMD_UNIQUEREF_UHL:
        *ppszLocation = psDInfo->pachUHLRecord + 35;
        *pnLength = 12;
        break;

      case DTEDMD_DATA_EDITION:
        *ppszLocation = psDInfo->pachDSIRecord + 87;
        *pnLength = 2;
        break;

      case DTEDMD_MATCHMERGE_VERSION:
        *ppszLocation = psDInfo->pachDSIRecord + 89;
        *pnLength = 1;
        break;

      case DTEDMD_MAINT_DATE:
        *ppszLocation = psDInfo->pachDSIRecord + 90;
        *pnLength = 4;
        break;

      case DTEDMD_MATCHMERGE_DATE:
        *ppszLocation = psDInfo->pachDSIRecord + 94;
        *pnLength = 4;
        break;

      case DTEDMD_MAINT_DESCRIPTION:
        *ppszLocation = psDInfo->pachDSIRecord + 98;
        *pnLength = 4;
        break;

      case DTEDMD_PRODUCER:
        *ppszLocation = psDInfo->pachDSIRecord + 102;
        *pnLength = 8;
        break;

      case DTEDMD_VERTDATUM:
        *ppszLocation = psDInfo->pachDSIRecord + 141;
        *pnLength = 3;
        break;

      case DTEDMD_HORIZDATUM: 
        *ppszLocation = psDInfo->pachDSIRecord + 144; 
        *pnLength = 5; 
        break; 

      case DTEDMD_DIGITIZING_SYS:
        *ppszLocation = psDInfo->pachDSIRecord + 149;
        *pnLength = 10;
        break;

      case DTEDMD_COMPILATION_DATE:
        *ppszLocation = psDInfo->pachDSIRecord + 159;
        *pnLength = 4;
        break;

      case DTEDMD_HORIZACCURACY:
        *ppszLocation = psDInfo->pachACCRecord + 3;
        *pnLength = 4;
        break;

      case DTEDMD_REL_HORIZACCURACY:
        *ppszLocation = psDInfo->pachACCRecord + 11;
        *pnLength = 4;
        break;

      case DTEDMD_REL_VERTACCURACY:
        *ppszLocation = psDInfo->pachACCRecord + 15;
        *pnLength = 4;
        break;

      case DTEDMD_VERTACCURACY_ACC:
        *ppszLocation = psDInfo->pachACCRecord + 7;
        *pnLength = 4;
        break;

      case DTEDMD_SECURITYCODE_DSI:
        *ppszLocation = psDInfo->pachDSIRecord + 3;
        *pnLength = 1;
        break;

      case DTEDMD_UNIQUEREF_DSI:
        *ppszLocation = psDInfo->pachDSIRecord + 64;
        *pnLength = 15;
        break;

      case DTEDMD_NIMA_DESIGNATOR: 
        *ppszLocation = psDInfo->pachDSIRecord + 59;
        *pnLength = 5;
        break;

      default:
        *ppszLocation = NULL;
        *pnLength = 0;
    }
}

/************************************************************************/
/*                          DTEDGetMetadata()                           */
/************************************************************************/

char *DTEDGetMetadata( DTEDInfo *psDInfo, DTEDMetaDataCode eCode )

{
    int nFieldLen;
    char *pszFieldSrc;
    char *pszResult;

    DTEDGetMetadataLocation( psDInfo, eCode, &pszFieldSrc, &nFieldLen );
    if( pszFieldSrc == NULL )
        return VSIStrdup( "" );

    pszResult = (char *) malloc(nFieldLen+1);
    strncpy( pszResult, pszFieldSrc, nFieldLen );
    pszResult[nFieldLen] = '\0';

    return pszResult;
}

/************************************************************************/
/*                          DTEDSetMetadata()                           */
/************************************************************************/

int DTEDSetMetadata( DTEDInfo *psDInfo, DTEDMetaDataCode eCode, 
                     const char *pszNewValue )

{
    int nFieldLen;
    char *pszFieldSrc;

    if( !psDInfo->bUpdate )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Get the location in the headers to update.                      */
/* -------------------------------------------------------------------- */
    DTEDGetMetadataLocation( psDInfo, eCode, &pszFieldSrc, &nFieldLen );
    if( pszFieldSrc == NULL )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Update it, padding with spaces.                                 */
/* -------------------------------------------------------------------- */
    memset( pszFieldSrc, ' ', nFieldLen );
    strncpy( pszFieldSrc, pszNewValue, 
             MIN((size_t)nFieldLen,strlen(pszNewValue)) );

/* -------------------------------------------------------------------- */
/*      Write all headers back to disk.                                 */
/* -------------------------------------------------------------------- */
    VSIFSeekL( psDInfo->fp, psDInfo->nUHLOffset, SEEK_SET );
    VSIFWriteL( psDInfo->pachUHLRecord, 1, DTED_UHL_SIZE, psDInfo->fp );

    VSIFSeekL( psDInfo->fp, psDInfo->nDSIOffset, SEEK_SET );
    VSIFWriteL( psDInfo->pachDSIRecord, 1, DTED_DSI_SIZE, psDInfo->fp );

    VSIFSeekL( psDInfo->fp, psDInfo->nACCOffset, SEEK_SET );
    VSIFWriteL( psDInfo->pachACCRecord, 1, DTED_ACC_SIZE, psDInfo->fp );

    return TRUE;
}

/************************************************************************/
/*                             DTEDClose()                              */
/************************************************************************/

void DTEDClose( DTEDInfo * psDInfo )

{
    VSIFCloseL( psDInfo->fp );

    CPLFree( psDInfo->pachUHLRecord );
    CPLFree( psDInfo->pachDSIRecord );
    CPLFree( psDInfo->pachACCRecord );
    
    CPLFree( psDInfo );
}
