/******************************************************************************
 * $Id$
 *
 * Project:  CEOS Translator
 * Purpose:  Implementation of non-GDAL dependent CEOS support.
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
 * Revision 1.5  2006/04/04 00:34:40  fwarmerdam
 * updated contact info
 *
 * Revision 1.4  2003/07/08 15:34:04  warmerda
 * avoid warnings
 *
 * Revision 1.3  2001/10/29 17:48:34  warmerda
 * Change sequence number check to a warning since the sequence numbers are
 * apparently screwy in some otherwise interesting ESA/CEOS Landsat 7 files.
 *
 * Revision 1.2  2001/07/18 04:51:56  warmerda
 * added CPL_CVSID
 *
 * Revision 1.1  1999/05/05 17:32:38  warmerda
 * New
 *
 */

#include "ceosopen.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                            CEOSScanInt()                             */
/*                                                                      */
/*      Read up to nMaxChars from the passed string, and interpret      */
/*      as an integer.                                                  */
/************************************************************************/

static long CEOSScanInt( const char * pszString, int nMaxChars )

{
    char	szWorking[33];
    int		i;

    if( nMaxChars > 32 || nMaxChars == 0 )
        nMaxChars = 32;

    for( i = 0; i < nMaxChars && pszString[i] != '\0'; i++ )
        szWorking[i] = pszString[i];

    szWorking[i] = '\0';

    return( atoi(szWorking) );
}

/************************************************************************/
/*                           CEOSReadRecord()                           */
/*                                                                      */
/*      Read a single CEOS record at the current point in the file.     */
/*      Return NULL after reporting an error if it fails, otherwise     */
/*      return the record.                                              */
/************************************************************************/

CEOSRecord * CEOSReadRecord( FILE * fp )

{
    GByte	abyHeader[12];
    CEOSRecord  *psRecord;

/* -------------------------------------------------------------------- */
/*      Read the standard CEOS header.                                  */
/* -------------------------------------------------------------------- */
    if( VSIFEof( fp ) )
        return NULL;

    if( VSIFRead( abyHeader, 1, 12, fp ) != 12 )
    {
        CPLError( CE_Failure, CPLE_FileIO,
                  "Ran out of data reading CEOS record." );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Extract this information.                                       */
/* -------------------------------------------------------------------- */
    psRecord = (CEOSRecord *) CPLMalloc(sizeof(CEOSRecord));
    psRecord->nRecordNum = abyHeader[0] * 256 * 256 * 256
                         + abyHeader[1] * 256 * 256
                         + abyHeader[2] * 256
                         + abyHeader[3];

    psRecord->nRecordType = abyHeader[4] * 256 * 256 * 256
                          + abyHeader[5] * 256 * 256
                          + abyHeader[6] * 256
                          + abyHeader[7];

    psRecord->nLength = abyHeader[8]  * 256 * 256 * 256
                      + abyHeader[9]  * 256 * 256
                      + abyHeader[10] * 256
                      + abyHeader[11];

/* -------------------------------------------------------------------- */
/*      Does it look reasonable?  We assume there can't be too many     */
/*      records and that the length must be between 12 and 200000.      */
/* -------------------------------------------------------------------- */
    if( psRecord->nRecordNum < 0 || psRecord->nRecordNum > 200000
        || psRecord->nLength < 12 || psRecord->nLength > 200000 )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "CEOS record leader appears to be corrupt.\n"
                  "Record Number = %d, Record Length = %d\n",
                  psRecord->nRecordNum, psRecord->nLength );
        CPLFree( psRecord );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Read the remainder of the record into a buffer.  Ensure that    */
/*      the first 12 bytes gets moved into this buffer as well.         */
/* -------------------------------------------------------------------- */
    psRecord->pachData = (char *) VSIMalloc(psRecord->nLength );
    if( psRecord->pachData == NULL )
    {
        CPLError( CE_Failure, CPLE_OutOfMemory,
                  "Out of memory allocated %d bytes for CEOS record data.\n"
                  "Are you sure you aren't leaking CEOSRecords?\n",
                  psRecord->nLength );
        return NULL;
    }

    memcpy( psRecord->pachData, abyHeader, 12 );

    if( (int)VSIFRead( psRecord->pachData + 12, 1, psRecord->nLength-12, fp )
        != psRecord->nLength - 12 )
    {
        CPLError( CE_Failure, CPLE_FileIO,
                  "Short read on CEOS record data.\n" );
        CPLFree( psRecord );
        return NULL;
    }

    return psRecord;
}

/************************************************************************/
/*                         CEOSDestroyRecord()                          */
/*                                                                      */
/*      Free a record.                                                  */
/************************************************************************/

void CEOSDestroyRecord( CEOSRecord * psRecord )

{
    CPLFree( psRecord->pachData );
    CPLFree( psRecord );
}

/************************************************************************/
/*                              CEOSOpen()                              */
/************************************************************************/

/**
 * Open a CEOS transfer.
 *
 * @param Filename The name of the CEOS imagery file (ie. imag_01.dat).
 * @param Access An fopen() style access string.  Should be either "rb" for
 * read-only access, or "r+b" for read, and update access.
 *
 * @return A CEOSImage pointer as a handle to the image.  The CEOSImage also
 * has various information about the image available.  A NULL is returned
 * if an error occurs.
 */

CEOSImage * CEOSOpen( const char * pszFilename, const char * pszAccess )

{
    FILE	*fp;
    CEOSRecord  *psRecord;
    CEOSImage   *psImage;
    int		nSeqNum, i;

/* -------------------------------------------------------------------- */
/*      Try to open the imagery file.                                   */
/* -------------------------------------------------------------------- */
    fp = VSIFOpen( pszFilename, pszAccess );

    if( fp == NULL )
    {
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "Failed to open CEOS file `%s' with access `%s'.\n",
                  pszFilename, pszAccess );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Try to read the header record.                                  */
/* -------------------------------------------------------------------- */
    psRecord = CEOSReadRecord( fp );
    if( psRecord == NULL )
        return NULL;

    if( psRecord->nRecordType != CRT_IMAGE_FDR )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Got a %X type record, instead of the expected\n"
                  "file descriptor record on file %s.\n",
                  psRecord->nRecordType, pszFilename );

        CEOSDestroyRecord( psRecord );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      The sequence number should be 2 indicating this is the          */
/*      imagery file.                                                   */
/* -------------------------------------------------------------------- */
    nSeqNum = CEOSScanInt( psRecord->pachData + 44, 4 );
    if( nSeqNum != 2 )
    {
        CPLError( CE_Warning, CPLE_AppDefined,
                  "Got a %d file sequence number, instead of the expected\n"
                  "2 indicating imagery on file %s.\n"
                  "Continuing to access anyways.\n", 
                  nSeqNum, pszFilename );
    }
    
/* -------------------------------------------------------------------- */
/*      Create a CEOSImage structure, and initialize it.                */
/* -------------------------------------------------------------------- */
    psImage = (CEOSImage *) CPLMalloc(sizeof(CEOSImage));
    psImage->fpImage = fp;

    psImage->nPixels = psImage->nLines = psImage->nBands = 0;
    
/* -------------------------------------------------------------------- */
/*      Extract various information.                                    */
/* -------------------------------------------------------------------- */
    psImage->nImageRecCount = CEOSScanInt( psRecord->pachData+180, 6 );
    psImage->nImageRecLength = CEOSScanInt( psRecord->pachData+186, 6 );
    psImage->nBitsPerPixel = CEOSScanInt( psRecord->pachData+216, 4 );
    psImage->nBands = CEOSScanInt( psRecord->pachData+232, 4 );
    psImage->nLines = CEOSScanInt( psRecord->pachData+236, 8 );
    psImage->nPixels = CEOSScanInt( psRecord->pachData+248, 8 );

    psImage->nPrefixBytes = CEOSScanInt( psRecord->pachData+276, 4 );
    psImage->nSuffixBytes = CEOSScanInt( psRecord->pachData+288, 4 );

/* -------------------------------------------------------------------- */
/*      Try to establish the layout of the imagery data.                */
/* -------------------------------------------------------------------- */
    psImage->nLineOffset = psImage->nBands * psImage->nImageRecLength;
    
    psImage->panDataStart = (int *) CPLMalloc(sizeof(int) * psImage->nBands);

    for( i = 0; i < psImage->nBands; i++ )
    {
        psImage->panDataStart[i] =
            psRecord->nLength + i * psImage->nImageRecLength
	            + 12 + psImage->nPrefixBytes;
    }

    return psImage;
}

/************************************************************************/
/*                          CEOSReadScanline()                          */
/************************************************************************/

/**
 * Read a scanline of image.
 *
 * @param psCEOS The CEOS dataset handle returned by CEOSOpen().
 * @param nBand The band number (ie. 1, 2, 3).
 * @param nScanline The scanline requested, one based.
 * @param pData The data buffer to read into.  Must be at least nPixels *
 * nBitesPerPixel bits long.
 *
 * @return CPLErr Returns error indicator or CE_None if the read succeeds.
 */

CPLErr CEOSReadScanline( CEOSImage * psCEOS, int nBand, int nScanline,
                         void * pData )

{
    int		nOffset, nBytes;
    
    /*
     * As a short cut, I currently just seek to the data, and read it
     * raw, rather than trying to read ceos records properly.
     */

    nOffset = psCEOS->panDataStart[nBand-1]
        	+ (nScanline-1) * psCEOS->nLineOffset;

    if( VSIFSeek( psCEOS->fpImage, nOffset, SEEK_SET ) != 0 )
    {
        CPLError( CE_Failure, CPLE_FileIO,
                  "Seek to %d for scanline %d failed.\n",
                  nOffset, nScanline );
        return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      Read the data.                                                  */
/* -------------------------------------------------------------------- */
    nBytes = psCEOS->nPixels * psCEOS->nBitsPerPixel / 8;
    if( (int) VSIFRead( pData, 1, nBytes, psCEOS->fpImage ) != nBytes )
    {
        CPLError( CE_Failure, CPLE_FileIO,
                  "Read of %d bytes for scanline %d failed.\n",
                  nBytes, nScanline );
        return CE_Failure;
    }

    return CE_None;
}

/************************************************************************/
/*                             CEOSClose()                              */
/************************************************************************/

/**
 * Close a CEOS transfer.  Any open files are closed, and memory deallocated.
 *
 * @param psCEOS The CEOSImage handle from CEOSOpen to be closed.
 */

void CEOSClose( CEOSImage * psCEOS )

{
    VSIFClose( psCEOS->fpImage );
    CPLFree( psCEOS );
}

