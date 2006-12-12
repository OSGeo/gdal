/******************************************************************************
 * $Id$
 *
 * Project:  BSB Reader
 * Purpose:  Low level BSB Access API Implementation (non-GDAL).
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 * NOTE: This code is implemented on the basis of work by Mike Higgins.  The 
 * BSB format is subject to US patent 5,727,090; however, that patent 
 * apparently only covers *writing* BSB files, not reading them, so this code 
 * should not be affected.
 *
 ******************************************************************************
 * Copyright (c) 2001, Frank Warmerdam
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
 * Revision 1.15  2006/12/12 03:58:34  fwarmerdam
 * Ensure saved character gets properly initialized.
 *
 * Revision 1.14  2006/08/23 17:29:46  fwarmerdam
 * added virtual io api support
 *
 * Revision 1.13  2003/10/15 15:51:17  warmerda
 * Avoid C++ comments.
 *
 * Revision 1.12  2003/07/08 20:28:09  warmerda
 * avoid warings
 *
 * Revision 1.11  2003/02/19 03:50:59  warmerda
 * hacks to support two apparently corrupt BSB files supplied by optech
 *
 * Revision 1.10  2002/12/04 15:54:23  warmerda
 * improved error reporting, remap 0 pixel values on write
 *
 * Revision 1.9  2002/12/04 14:52:54  warmerda
 * Added binary flag writing.
 *
 * Revision 1.8  2002/11/04 04:26:45  warmerda
 * preliminary work on write support
 *
 * Revision 1.7  2002/07/19 22:05:15  warmerda
 * added support for NO1 (encrypted) files
 *
 * Revision 1.6  2002/07/19 20:57:32  warmerda
 * nos files are VER/1 but have ver2 style line markers
 *
 * Revision 1.5  2002/07/19 20:33:55  warmerda
 * Fixed for BSB 1.1 and 3.0 issues:
 *  o Allow for longer lines.
 *  o In BSB 1.1 the scanline marker is zero based.
 *
 * Revision 1.4  2002/03/19 20:58:24  warmerda
 * Changes from Olle Soderholm that should allow for NOS files as well.
 *
 * Revision 1.3  2001/12/08 21:58:32  warmerda
 * save header
 *
 * Revision 1.2  2001/12/08 15:58:45  warmerda
 * fixed VSIFSeek() arguments as per Chris Schaefer
 *
 * Revision 1.1  2001/12/08 04:35:16  warmerda
 * New
 *
 */

#include "bsb_read.h"
#include "cpl_conv.h"
#include "cpl_string.h"

CPL_CVSID("$Id$");

static const char *BSBReadHeaderLine( BSBInfo *psInfo, int bNO1 );

/************************************************************************

Background:

To: Frank Warmerdam <warmerda@home.com>
From: Mike Higgins <higgins@monitor.net>
Subject: Re: GISTrans: Maptech / NDI BSB Chart Format
Mime-Version: 1.0
Content-Type: text/plain; charset="us-ascii"; format=flowed

         I did it! I just wrote a program that reads NOAA BSB chart files 
and converts them to BMP files! BMP files are not the final goal of my 
project, but it served as a proof-of-concept.  Next I will want to write 
routines to extract pieces of the file at full resolution for printing, and 
routines to filter pieces of the chart for display at lower resolution on 
the screen.  (One of the terrible things about most chart display programs 
is that they all sub-sample the charts instead of filtering it down). How 
did I figure out how to read the BSB files?

         If you recall, I have been trying to reverse engineer the file 
formats of those nautical charts. When I am between projects I often do a 
WEB search for the BSB file format to see if someone else has published a 
hack for them. Monday I hit a NOAA project status report that mentioned 
some guy named Marty Yellin who had recently completed writing a program to 
convert BSB files to other file formats! I searched for him and found him 
mentioned as a contact person for some NOAA program. I was composing a 
letter to him in my head, or considering calling the NOAA phone number and 
asking for his extension number, when I saw another NOAA status report 
indicating that he had retired in 1998. His name showed up in a few more 
reports, one of which said that he was the inventor of the BSB file format, 
that it was patented (#5,727,090), and that the patent had been licensed to 
Maptech (the evil company that will not allow anyone using their file 
format to convert them to non-proprietary formats). Patents are readily 
available on the WEB at the IBM patent server and this one is in the 
dtabase!  I printed up a copy of the patent and of course it describes very 
nicely (despite the usual typos and omissions of referenced items in the 
figures) how to write one of these BSB files!

         I was considering talking to a patent lawyer about the legality of 
using information in the patent to read files without getting a license,
when I noticed that the patent is only claiming programs that WRITE the 
file format. I have noticed this before in RF patents where they describe 
how to make a receiver and never bother to claim a transmitter. The logic 
is that the transmitter is no good to anybody unless they license receivers 
from the patent holder. But I think they did it backwards here! They should 
have claimed a program that can READ the described file format. Now I can 
read the files, build programs that read the files, and even sell them 
without violating the claims in the patent! As long as I never try to write 
one of the evil BSB files, I'm OK!!!

         If you ever need to read these BSB chart programs, drop me a 
note.  I would be happy to send you a copy of this conversion program.

... later email ...

         Well, here is my little proof of concept program. I hereby give 
you permission to distribute it freely, modify for you own use, etc.
I built it as a "WIN32 Console application" which means it runs in an MS 
DOS box under Microsoft Windows. But the only Windows specific stuff in it 
are the include files for the BMP file headers.  If you ripped out the BMP 
code it should compile under UNIX or anyplace else.
         I'd be overjoyed to have you announce it to GISTrans or anywhere 
else.  I'm philosophically opposed to the proprietary treatment of the  BSB 
file format and I want to break it open! Chart data for the People!

 ************************************************************************/

/************************************************************************/
/*                             BSBUngetc()                              */
/************************************************************************/

void BSBUngetc( BSBInfo *psInfo, int nCharacter )

{
    CPLAssert( psInfo->nSavedCharacter == -1000 );
    psInfo->nSavedCharacter = nCharacter;
}

/************************************************************************/
/*                              BSBGetc()                               */
/************************************************************************/

int BSBGetc( BSBInfo *psInfo, int bNO1 )

{
    int nByte;

    if( psInfo->nSavedCharacter != -1000 )
    {
        nByte = psInfo->nSavedCharacter;
        psInfo->nSavedCharacter = -1000;
        return nByte;
    }

    if( psInfo->nBufferOffset >= psInfo->nBufferSize )
    {
        psInfo->nBufferOffset = 0;
        psInfo->nBufferSize = 
            VSIFReadL( psInfo->pabyBuffer, 1, psInfo->nBufferAllocation,
                       psInfo->fp );
        if( psInfo->nBufferSize <= 0 )
            return 0;
    }

    nByte = psInfo->pabyBuffer[psInfo->nBufferOffset++];
    
    if( bNO1 )
    {
        nByte = nByte - 9;
        if( nByte < 0 )
            nByte = nByte + 256;
    }

    return nByte;
}


/************************************************************************/
/*                              BSBOpen()                               */
/*                                                                      */
/*      Read BSB header, and return information.                        */
/************************************************************************/

BSBInfo *BSBOpen( const char *pszFilename )

{
    FILE	*fp;
    char	achTestBlock[1000];
    const char  *pszLine;
    int         i, bNO1 = FALSE;
    BSBInfo     *psInfo;
    int    nSkipped = 0;

/* -------------------------------------------------------------------- */
/*      Open the file.                                                  */
/* -------------------------------------------------------------------- */
    fp = VSIFOpenL( pszFilename, "rb" );
    if( fp == NULL )
    {
        CPLError( CE_Failure, CPLE_OpenFailed, 
                  "File %s not found.", pszFilename );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*	Read the first 1000 bytes, and verify that it contains the	*/
/*	"BSB/" keyword"							*/
/* -------------------------------------------------------------------- */
    if( VSIFReadL( achTestBlock, 1, sizeof(achTestBlock), fp ) 
        != sizeof(achTestBlock) )
    {
        VSIFCloseL( fp );
        CPLError( CE_Failure, CPLE_FileIO,
                  "Could not read first %d bytes for header!", 
                  sizeof(achTestBlock) );
        return NULL;
    }

    for( i = 0; i < sizeof(achTestBlock) - 4; i++ )
    {
        /* Test for "BSB/" */
        if( achTestBlock[i+0] == 'B' && achTestBlock[i+1] == 'S' 
            && achTestBlock[i+2] == 'B' && achTestBlock[i+3] == '/' )
            break;

        /* Test for "NOS/" */
        if( achTestBlock[i+0] == 'N' && achTestBlock[i+1] == 'O'
            && achTestBlock[i+2] == 'S' && achTestBlock[i+3] == '/' )
            break;

        /* Test for "NOS/" offset by 9 in ASCII for NO1 files */
        if( achTestBlock[i+0] == 'W' && achTestBlock[i+1] == 'X'
            && achTestBlock[i+2] == '\\' && achTestBlock[i+3] == '8' )
        {
            bNO1 = TRUE;
            break;
        }
    }

    if( i == sizeof(achTestBlock) - 4 )
    {
        VSIFCloseL( fp );
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "This does not appear to be a BSB file, no BSB/ header." );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Create info structure.                                          */
/* -------------------------------------------------------------------- */
    psInfo = (BSBInfo *) CPLCalloc(1,sizeof(BSBInfo));
    psInfo->fp = fp;
    psInfo->bNO1 = bNO1;

    psInfo->nBufferAllocation = 1024;
    psInfo->pabyBuffer = (GByte *) CPLMalloc(psInfo->nBufferAllocation);
    psInfo->nBufferSize = 0; 
    psInfo->nBufferOffset = 0;
    psInfo->nSavedCharacter = -1000;

/* -------------------------------------------------------------------- */
/*      Rewind, and read line by line.                                  */
/* -------------------------------------------------------------------- */
    VSIFSeekL( fp, 0, SEEK_SET );

    while( (pszLine = BSBReadHeaderLine(psInfo, bNO1)) != NULL )
    {
        char	**papszTokens = NULL;
        int      nCount = 0;

        if( pszLine[3] == '/' )
        {
            psInfo->papszHeader = CSLAddString( psInfo->papszHeader, pszLine );
            papszTokens = CSLTokenizeStringComplex( pszLine+4, ",=", 
                                                    FALSE,FALSE);
            nCount = CSLCount(papszTokens);
        }

        if( EQUALN(pszLine,"BSB/",4) )
        {
            int		nRAIndex;

            nRAIndex = CSLFindString(papszTokens, "RA" );
            if( nRAIndex < 0 || nRAIndex > nCount - 2 )
            {
                CSLDestroy( papszTokens );
                CPLError( CE_Failure, CPLE_AppDefined, 
                          "Failed to extract RA from BSB/ line." );
                BSBClose( psInfo );
                return NULL;
            }
            psInfo->nXSize = atoi(papszTokens[nRAIndex+1]);
            psInfo->nYSize = atoi(papszTokens[nRAIndex+2]);
        }
        else if( EQUALN(pszLine,"NOS/",4) )
        {
            int  nRAIndex;
            
            nRAIndex = CSLFindString(papszTokens, "RA" );
            if( nRAIndex < 0 || nRAIndex > nCount - 2 )
            {
                CSLDestroy( papszTokens );
                CPLError( CE_Failure, CPLE_AppDefined,
                          "Failed to extract RA from NOS/ line." );
                BSBClose( psInfo );
                return NULL;
            }
            psInfo->nXSize = atoi(papszTokens[nRAIndex+3]);
            psInfo->nYSize = atoi(papszTokens[nRAIndex+4]);
        }
        else if( EQUALN(pszLine,"RGB/",4) && nCount >= 4 )
        {
            int	iPCT = atoi(papszTokens[0]);
            if( iPCT > psInfo->nPCTSize-1 )
            {
                psInfo->pabyPCT = (unsigned char *) 
                    CPLRealloc(psInfo->pabyPCT,(iPCT+1) * 3);
                memset( psInfo->pabyPCT + psInfo->nPCTSize*3, 0, 
                        (iPCT+1-psInfo->nPCTSize) * 3);
                psInfo->nPCTSize = iPCT+1;
            }

            psInfo->pabyPCT[iPCT*3+0] = (unsigned char)atoi(papszTokens[1]);
            psInfo->pabyPCT[iPCT*3+1] = (unsigned char)atoi(papszTokens[2]);
            psInfo->pabyPCT[iPCT*3+2] = (unsigned char)atoi(papszTokens[3]);
        }
        else if( EQUALN(pszLine,"VER/",4) && nCount >= 1 )
        {
            psInfo->nVersion = (int) (100 * atof(papszTokens[0]) + 0.5);
        }

        CSLDestroy( papszTokens );
    }
    
/* -------------------------------------------------------------------- */
/*      Verify we found required keywords.                              */
/* -------------------------------------------------------------------- */
    if( psInfo->nXSize == 0 || psInfo->nPCTSize == 0 )
    {
        BSBClose( psInfo );
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Failed to find required RGB/ or BSB/ keyword in header." );
        
        return NULL;
    }

    if( psInfo->nVersion == 0 )
    {
        CPLError( CE_Warning, CPLE_AppDefined,
                  "VER (version) keyword not found, assuming 2.0." );
        psInfo->nVersion = 200;
    }

/* -------------------------------------------------------------------- */
/*      If all has gone well this far, we should be pointing at the     */
/*      sequence "0x1A 0x00".  Read past to get to start of data.       */
/*                                                                      */
/*      We actually do some funny stuff here to be able to read past    */
/*      some garbage to try and find the 0x1a 0x00 sequence since in    */
/*      at least some files (ie. optech/World.kap) we find a few        */
/*      bytes of extra junk in the way.                                 */
/* -------------------------------------------------------------------- */
/* from optech/World.kap 

   11624: 30333237 34353938 2C302E30 35373836 03274598,0.05786
   11640: 39303232 38332C31 332E3135 39363435 902283,13.159645
   11656: 35390D0A 1A0D0A1A 00040190 C0510002 59~~~~~~~~~~~Q~~
   11672: 90C05100 0390C051 000490C0 51000590 ~~Q~~~~Q~~~~Q~~~
 */

    {
        int    nChar = -1;

        while( nSkipped < 100 
              && (BSBGetc( psInfo, bNO1 ) != 0x1A 
                  || (nChar = BSBGetc( psInfo, bNO1 )) != 0x00) )
        {
            if( nChar == 0x1A )
            {
                BSBUngetc( psInfo, nChar );
                nChar = -1;
            }
            nSkipped++;
        }

        if( nSkipped == 100 )
        {
            BSBClose( psInfo );
            CPLError( CE_Failure, CPLE_AppDefined, 
                      "Failed to find compressed data segment of BSB file." );
            return NULL;
        }
    }

/* -------------------------------------------------------------------- */
/*      Read the number of bit size of color numbers.                   */
/* -------------------------------------------------------------------- */
    psInfo->nColorSize = BSBGetc( psInfo, bNO1 );

    /* The USGS files like 83116_1.KAP seem to use the ASCII number instead
       of the binary number for the colorsize value. */
    
    if( nSkipped > 0 
        && psInfo->nColorSize >= 0x31 && psInfo->nColorSize <= 0x38 )
        psInfo->nColorSize -= 0x30;

    CPLAssert( psInfo->nColorSize > 0 && psInfo->nColorSize < 9 );

/* -------------------------------------------------------------------- */
/*      Initialize line offset list.                                    */
/* -------------------------------------------------------------------- */
    psInfo->panLineOffset = (int *) 
        CPLMalloc(sizeof(int) * psInfo->nYSize);
    for( i = 0; i < psInfo->nYSize; i++ )
        psInfo->panLineOffset[i] = -1;

    psInfo->panLineOffset[0] = 
        VSIFTellL( fp ) - psInfo->nBufferSize + psInfo->nBufferOffset;

    return psInfo;
}

/************************************************************************/
/*                         BSBReadHeaderLine()                          */
/*                                                                      */
/*      Read one virtual line of text from the BSB header.  This        */
/*      will end if a 0x1A (EOF) is encountered, indicating the data    */
/*      is about to start.  It will also merge multiple physical        */
/*      lines where appropriate.  The buffer is internal, and only      */
/*      lasts till the next call.                                       */
/************************************************************************/

static const char *BSBReadHeaderLine( BSBInfo *psInfo, int bNO1 )

{
    static char	szLine[1000];
    char        chNext;
    int	        nLineLen = 0;

    while( !VSIFEofL(psInfo->fp) && nLineLen < sizeof(szLine)-1 )
    {
        chNext = (char) BSBGetc( psInfo, bNO1 );
        if( chNext == 0x1A )
        {
            BSBUngetc( psInfo, chNext );
            return NULL;
        }

        /* each CR/LF (or LF/CR) as if just "CR" */
        if( chNext == 10 || chNext == 13 )
        {
            char	chLF;

            chLF = (char) BSBGetc( psInfo, bNO1 );
            if( chLF != 10 && chLF != 13 )
                BSBUngetc( psInfo, chLF );
            chNext = '\n';
        }

        /* If we are at the end-of-line, check for blank at start
        ** of next line, to indicate need of continuation.
        */
        if( chNext == '\n' )
        {
            char chTest;

            chTest = (char) BSBGetc(psInfo, bNO1);
            /* Are we done? */
            if( chTest != ' ' )
            {
                BSBUngetc( psInfo, chTest );
                szLine[nLineLen] = '\0';
                return szLine;
            }

            /* eat pending spaces */
            while( chTest == ' ' )
                chTest = (char) BSBGetc(psInfo,bNO1);
            BSBUngetc( psInfo,chTest );

            /* insert comma in data stream */
            szLine[nLineLen++] = ',';
        }
        else
        {
            szLine[nLineLen++] = chNext;
        }
    }

    return NULL;
}

/************************************************************************/
/*                          BSBReadScanline()                           */
/************************************************************************/

int BSBReadScanline( BSBInfo *psInfo, int nScanline, 
                     unsigned char *pabyScanlineBuf )

{
    int		nLineMarker = 0, nValueShift, iPixel = 0;
    unsigned char byValueMask, byCountMask;
    FILE	*fp = psInfo->fp;
    int         byNext, i;

/* -------------------------------------------------------------------- */
/*      Do we know where the requested line is?  If not, read all       */
/*      the preceeding ones to "find" our line.                         */
/* -------------------------------------------------------------------- */
    if( nScanline < 0 || nScanline >= psInfo->nYSize )
    {
        CPLError( CE_Failure, CPLE_FileIO, 
                  "Scanline %d out of range.", 
                   nScanline );
        return FALSE;
    }

    if( psInfo->panLineOffset[nScanline] == -1 )
    {
        for( i = 0; i < nScanline; i++ )
        {
            if( psInfo->panLineOffset[i+1] == -1 )
            {
                if( !BSBReadScanline( psInfo, i, pabyScanlineBuf ) )
                    return FALSE;
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Seek to requested scanline.                                     */
/* -------------------------------------------------------------------- */
    psInfo->nBufferSize = 0;
    if( VSIFSeekL( fp, psInfo->panLineOffset[nScanline], SEEK_SET ) != 0 )
    {
        CPLError( CE_Failure, CPLE_FileIO, 
                  "Seek to offset %d for scanline %d failed.", 
                  psInfo->panLineOffset[nScanline], nScanline );
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Read the line number.  Pre 2.0 BSB seemed to expect the line    */
/*      numbers to be zero based, while 2.0 and later seemed to         */
/*      expect it to be one based, and for a 0 to be some sort of       */
/*      missing line marker.                                            */
/* -------------------------------------------------------------------- */
    do {
        byNext = BSBGetc( psInfo, psInfo->bNO1 );

        /* Special hack to skip over extra zeros in some files, such
        ** as optech/sample1.kap.
        */
        while( nScanline != 0 && nLineMarker == 0 && byNext == 0 )
            byNext = BSBGetc( psInfo, psInfo->bNO1 );

        nLineMarker = nLineMarker * 128 + (byNext & 0x7f);
    } while( (byNext & 0x80) != 0 );

    if( nLineMarker != nScanline 
        && nLineMarker != nScanline + 1 )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Got scanline id %d when looking for %d @ offset %ld.", 
                  nLineMarker, nScanline+1, VSIFTellL( fp ) );
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Setup masking values.                                           */
/* -------------------------------------------------------------------- */
    nValueShift = 7 - psInfo->nColorSize;
    byValueMask = (unsigned char)
        ((((1 << psInfo->nColorSize)) - 1) << nValueShift);
    byCountMask = (unsigned char)
        (1 << (7 - psInfo->nColorSize)) - 1;
    
/* -------------------------------------------------------------------- */
/*      Read and expand runs.                                           */
/* -------------------------------------------------------------------- */
    while( (byNext = BSBGetc(psInfo,psInfo->bNO1)) != 0 )
    {
        int	nPixValue;
        int     nRunCount, i;

        nPixValue = (byNext & byValueMask) >> nValueShift;

        nRunCount = byNext & byCountMask;

        while( (byNext & 0x80) != 0 )
        {
            byNext = BSBGetc( psInfo, psInfo->bNO1 );
            nRunCount = nRunCount * 128 + (byNext & 0x7f);
        }

        if( iPixel + nRunCount + 1 > psInfo->nXSize )
            nRunCount = psInfo->nXSize - iPixel - 1;

        for( i = 0; i < nRunCount+1; i++ )
            pabyScanlineBuf[iPixel++] = (unsigned char) nPixValue;
    }

/* -------------------------------------------------------------------- */
/*      For reasons that are unclear, some scanlines are exactly one    */
/*      pixel short (such as in the BSB 3.0 354704.KAP product from     */
/*      NDI/CHS) but are otherwise OK.  Just add a zero if this         */
/*      appear to have occured.                                         */
/* -------------------------------------------------------------------- */
    if( iPixel == psInfo->nXSize - 1 )
        pabyScanlineBuf[iPixel++] = 0;

/* -------------------------------------------------------------------- */
/*      Remember the start of the next line.                            */
/* -------------------------------------------------------------------- */
    if( iPixel == psInfo->nXSize && nScanline < psInfo->nYSize-1 )
        psInfo->panLineOffset[nScanline+1] = 
            VSIFTellL( fp ) - psInfo->nBufferSize + psInfo->nBufferOffset;

    if( iPixel != psInfo->nXSize )
    {
        CPLError( CE_Warning, CPLE_AppDefined, 
                  "Got %d pixels when looking for %d pixels.",
                  iPixel, psInfo->nXSize );
        return FALSE;
    }

    return TRUE;
}

/************************************************************************/
/*                              BSBClose()                              */
/************************************************************************/

void BSBClose( BSBInfo *psInfo )

{
    if( psInfo->fp != NULL )
        VSIFCloseL( psInfo->fp );

    CPLFree( psInfo->pabyBuffer );

    CSLDestroy( psInfo->papszHeader );
    CPLFree( psInfo->panLineOffset );
    CPLFree( psInfo->pabyPCT );
    CPLFree( psInfo );
}

/************************************************************************/
/*                             BSBCreate()                              */
/************************************************************************/

BSBInfo *BSBCreate( const char *pszFilename, int nCreationFlags, int nVersion, 
                    int nXSize, int nYSize )

{
    FILE	*fp;
    BSBInfo     *psInfo;

/* -------------------------------------------------------------------- */
/*      Open new KAP file.                                              */
/* -------------------------------------------------------------------- */
    fp = VSIFOpen( pszFilename, "wb" );
    if( fp == NULL )
    {
        CPLError( CE_Failure, CPLE_OpenFailed, 
                  "Failed to open output file %s.", 
                  pszFilename );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Write out BSB line.                                             */
/* -------------------------------------------------------------------- */
    VSIFPrintf( fp, 
                "!Copyright unknown\n" );
    VSIFPrintf( fp, 
                "VER/%.1f\n", nVersion / 100.0 );
    VSIFPrintf( fp, 
                "BSB/NA=UNKNOWN,NU=999502,RA=%d,%d,DU=254\n",
                nXSize, nYSize );
    VSIFPrintf( fp, 
                "KNP/SC=25000,GD=WGS84,PR=Mercator\n" );
    VSIFPrintf( fp, 
                "    PP=31.500000,PI=0.033333,SP=,SK=0.000000,TA=90.000000\n");
    VSIFPrintf( fp, 
                "     UN=Metres,SD=HHWLT,DX=2.500000,DY=2.500000\n");


/* -------------------------------------------------------------------- */
/*      Create info structure.                                          */
/* -------------------------------------------------------------------- */
    psInfo = (BSBInfo *) CPLCalloc(1,sizeof(BSBInfo));
    psInfo->fp = fp;
    psInfo->bNO1 = FALSE;
    psInfo->nVersion = nVersion;
    psInfo->nXSize = nXSize;
    psInfo->nYSize = nYSize;
    psInfo->bNewFile = TRUE;
    psInfo->nLastLineWritten = -1;

    return psInfo;
}

/************************************************************************/
/*                            BSBWritePCT()                             */
/************************************************************************/

int BSBWritePCT( BSBInfo *psInfo, int nPCTSize, unsigned char *pabyPCT )

{
    int        i;
    
/* -------------------------------------------------------------------- */
/*      Verify the PCT not too large.                                   */
/* -------------------------------------------------------------------- */
    if( nPCTSize > 128 )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Pseudo-color table too large (%d entries), at most 128\n"
                  " entries allowed in BSB format.", nPCTSize );
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Compute the number of bits required for the colors.             */
/* -------------------------------------------------------------------- */
    for( psInfo->nColorSize = 1; 
         (1 << psInfo->nColorSize) < nPCTSize; 
         psInfo->nColorSize++ ) {}

/* -------------------------------------------------------------------- */
/*      Write out the color table.  Note that color table entry zero    */
/*      is ignored.  Zero is not a legal value.                         */
/* -------------------------------------------------------------------- */
    for( i = 1; i < nPCTSize; i++ )
    {
        VSIFPrintf( psInfo->fp, 
                    "RGB/%d,%d,%d,%d\n", 
                    i, pabyPCT[i*3+0], pabyPCT[i*3+1], pabyPCT[i*3+2] );
    }

    return TRUE;
}

/************************************************************************/
/*                          BSBWriteScanline()                          */
/************************************************************************/

int BSBWriteScanline( BSBInfo *psInfo, unsigned char *pabyScanlineBuf )

{
    int   nValue, iX;

    if( psInfo->nLastLineWritten == psInfo->nYSize - 1 )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Attempt to write too many scanlines." );
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      If this is the first scanline writen out the EOF marker, and    */
/*      the introductory info in the image segment.                     */
/* -------------------------------------------------------------------- */
    if( psInfo->nLastLineWritten == -1 )
    {
        VSIFPutc( 0x1A, psInfo->fp );
        VSIFPutc( 0x00, psInfo->fp );
        VSIFPutc( psInfo->nColorSize, psInfo->fp );
    }

/* -------------------------------------------------------------------- */
/*      Write the line number.                                          */
/* -------------------------------------------------------------------- */
    nValue = ++psInfo->nLastLineWritten;

    if( psInfo->nVersion >= 200 )
        nValue++;

    if( nValue >= 128*128 )
        VSIFPutc( 0x80 | ((nValue & (0x7f<<14)) >> 14), psInfo->fp );
    if( nValue >= 128 )
        VSIFPutc( 0x80 | ((nValue & (0x7f<<7)) >> 7), psInfo->fp );
    VSIFPutc( nValue & 0x7f, psInfo->fp );

/* -------------------------------------------------------------------- */
/*      Write out each pixel as a separate byte.  We don't try to       */
/*      actually capture the runs since that radical and futuristic     */
/*      concept is patented!                                            */
/* -------------------------------------------------------------------- */
    for( iX = 0; iX < psInfo->nXSize; iX++ )
    {
        if( pabyScanlineBuf[iX] == 0 )
            VSIFPutc( 1 << (7-psInfo->nColorSize), 
                      psInfo->fp );
        else
            VSIFPutc( pabyScanlineBuf[iX] << (7-psInfo->nColorSize), 
                      psInfo->fp );
    }

    VSIFPutc( 0x00, psInfo->fp );

    return TRUE;
}
