/******************************************************************************
 * $Id$
 *
 * Project:  NITF Read/Write Library
 * Purpose:  Module responsible for opening NITF file, populating NITFFile
 *           structure, and instantiating segment specific access objects.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 **********************************************************************
 * Copyright (c) 2002, Frank Warmerdam
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

#include "nitflib.h"
#include "cpl_vsi.h"
#include "cpl_conv.h"
#include "cpl_string.h"

CPL_CVSID("$Id$");

static int NITFWriteBLOCKA( VSILFILE* fp, vsi_l_offset nOffsetUDIDL,
                            int *pnOffset,
                            char **papszOptions );
static int NITFWriteTREsFromOptions(
    VSILFILE* fp,
    vsi_l_offset nOffsetUDIDL,
    int *pnOffset,
    char **papszOptions,
    const char* pszTREPrefix);

static int 
NITFCollectSegmentInfo( NITFFile *psFile, int nFileHeaderLenSize, int nOffset,
                        const char szType[3],
                        int nHeaderLenSize, int nDataLenSize, 
                        GUIntBig *pnNextData );

/************************************************************************/
/*                              NITFOpen()                              */
/************************************************************************/

NITFFile *NITFOpen( const char *pszFilename, int bUpdatable )

{
    VSILFILE	*fp;
    char        *pachHeader;
    NITFFile    *psFile;
    int         nHeaderLen, nOffset, nHeaderLenOffset;
    GUIntBig    nNextData;
    char        szTemp[128], achFSDWNG[6];
    GIntBig     currentPos;
    int         bTriedStreamingFileHeader = FALSE;

/* -------------------------------------------------------------------- */
/*      Open the file.                                                  */
/* -------------------------------------------------------------------- */
    if( bUpdatable )
        fp = VSIFOpenL( pszFilename, "r+b" );
    else
        fp = VSIFOpenL( pszFilename, "rb" );

    if( fp == NULL )
    {
        CPLError( CE_Failure, CPLE_OpenFailed, 
                  "Failed to open file %s.", 
                  pszFilename );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Check file type.                                                */
/* -------------------------------------------------------------------- */
    VSIFReadL( szTemp, 1, 9, fp );

    if( !EQUALN(szTemp,"NITF",4) && !EQUALN(szTemp,"NSIF",4) )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "The file %s is not an NITF file.", 
                  pszFilename );
        VSIFCloseL(fp);
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Read the FSDWNG field.                                          */
/* -------------------------------------------------------------------- */
    if( VSIFSeekL( fp, 280, SEEK_SET ) != 0 
        || VSIFReadL( achFSDWNG, 1, 6, fp ) != 6 )
    {
        CPLError( CE_Failure, CPLE_NotSupported, 
                  "Unable to read FSDWNG field from NITF file.  File is either corrupt\n"
                  "or empty." );
        VSIFCloseL(fp);
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Get header length.                                              */
/* -------------------------------------------------------------------- */
    if( EQUALN(szTemp,"NITF01.",7) || EQUALN(achFSDWNG,"999998",6) )
        nHeaderLenOffset = 394;
    else
        nHeaderLenOffset = 354;

    if( VSIFSeekL( fp, nHeaderLenOffset, SEEK_SET ) != 0 
        || VSIFReadL( szTemp, 1, 6, fp ) != 6 )
    {
        CPLError( CE_Failure, CPLE_NotSupported, 
                  "Unable to read header length from NITF file.  File is either corrupt\n"
                  "or empty." );
        VSIFCloseL(fp);
        return NULL;
    }

    szTemp[6] = '\0';
    nHeaderLen = atoi(szTemp);

    VSIFSeekL( fp, nHeaderLen, SEEK_SET );
    currentPos = VSIFTellL( fp ) ;
    if( nHeaderLen < nHeaderLenOffset || nHeaderLen > currentPos )
    {
        CPLError( CE_Failure, CPLE_NotSupported, 
                  "NITF Header Length (%d) seems to be corrupt.",
                  nHeaderLen );
        VSIFCloseL(fp);
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Read the whole file header.                                     */
/* -------------------------------------------------------------------- */
    pachHeader = (char *) VSIMalloc(nHeaderLen);
    if (pachHeader == NULL)
    {
        CPLError( CE_Failure, CPLE_OutOfMemory, 
                  "Cannot allocate memory for NITF header");
        VSIFCloseL(fp);
        return NULL;
    }
    VSIFSeekL( fp, 0, SEEK_SET );
    if ((int)VSIFReadL( pachHeader, 1, nHeaderLen, fp ) != nHeaderLen)
    {
        CPLError( CE_Failure, CPLE_OutOfMemory, 
                  "Cannot read %d bytes for NITF header", (nHeaderLen));
        VSIFCloseL(fp);
        CPLFree(pachHeader);
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Create and initialize info structure about file.                */
/* -------------------------------------------------------------------- */
    psFile = (NITFFile *) CPLCalloc(sizeof(NITFFile),1);
    psFile->fp = fp;
    psFile->pachHeader = pachHeader;

retry_read_header:
/* -------------------------------------------------------------------- */
/*      Get version.                                                    */
/* -------------------------------------------------------------------- */
    NITFGetField( psFile->szVersion, pachHeader, 0, 9 );

/* -------------------------------------------------------------------- */
/*      Collect a variety of information as metadata.                   */
/* -------------------------------------------------------------------- */
#define GetMD( target, hdr, start, length, name )              \
    NITFExtractMetadata( &(target->papszMetadata), hdr,       \
                         start, length,                        \
                         "NITF_" #name );
       
    if( EQUAL(psFile->szVersion,"NITF02.10") 
        || EQUAL(psFile->szVersion,"NSIF01.00") )
    {
        char szWork[100];

        GetMD( psFile, pachHeader,   0,   9, FHDR   );
        GetMD( psFile, pachHeader,   9,   2, CLEVEL );
        GetMD( psFile, pachHeader,  11,   4, STYPE  );
        GetMD( psFile, pachHeader,  15,  10, OSTAID );
        GetMD( psFile, pachHeader,  25,  14, FDT    );
        GetMD( psFile, pachHeader,  39,  80, FTITLE );
        GetMD( psFile, pachHeader, 119,   1, FSCLAS );
        GetMD( psFile, pachHeader, 120,   2, FSCLSY );
        GetMD( psFile, pachHeader, 122,  11, FSCODE );
        GetMD( psFile, pachHeader, 133,   2, FSCTLH );
        GetMD( psFile, pachHeader, 135,  20, FSREL  );
        GetMD( psFile, pachHeader, 155,   2, FSDCTP );
        GetMD( psFile, pachHeader, 157,   8, FSDCDT );
        GetMD( psFile, pachHeader, 165,   4, FSDCXM );
        GetMD( psFile, pachHeader, 169,   1, FSDG   );
        GetMD( psFile, pachHeader, 170,   8, FSDGDT );
        GetMD( psFile, pachHeader, 178,  43, FSCLTX );
        GetMD( psFile, pachHeader, 221,   1, FSCATP );
        GetMD( psFile, pachHeader, 222,  40, FSCAUT );
        GetMD( psFile, pachHeader, 262,   1, FSCRSN );
        GetMD( psFile, pachHeader, 263,   8, FSSRDT );
        GetMD( psFile, pachHeader, 271,  15, FSCTLN );
        GetMD( psFile, pachHeader, 286,   5, FSCOP  );
        GetMD( psFile, pachHeader, 291,   5, FSCPYS );
        GetMD( psFile, pachHeader, 296,   1, ENCRYP );
        sprintf( szWork, "%3d,%3d,%3d", 
                 ((GByte *)pachHeader)[297], 
                 ((GByte *)pachHeader)[298], 
                 ((GByte *)pachHeader)[299] );
        GetMD( psFile, szWork, 0, 11, FBKGC );
        GetMD( psFile, pachHeader, 300,  24, ONAME  );
        GetMD( psFile, pachHeader, 324,  18, OPHONE );
        NITFGetField(szTemp, pachHeader, 342, 12);
    }
    else if( EQUAL(psFile->szVersion,"NITF02.00") )
    {
        int nCOff = 0;

        GetMD( psFile, pachHeader,   0,   9, FHDR   );
        GetMD( psFile, pachHeader,   9,   2, CLEVEL );
        GetMD( psFile, pachHeader,  11,   4, STYPE  );
        GetMD( psFile, pachHeader,  15,  10, OSTAID );
        GetMD( psFile, pachHeader,  25,  14, FDT    );
        GetMD( psFile, pachHeader,  39,  80, FTITLE );
        GetMD( psFile, pachHeader, 119,   1, FSCLAS );
        GetMD( psFile, pachHeader, 120,  40, FSCODE );
        GetMD( psFile, pachHeader, 160,  40, FSCTLH );
        GetMD( psFile, pachHeader, 200,  40, FSREL  );
        GetMD( psFile, pachHeader, 240,  20, FSCAUT );
        GetMD( psFile, pachHeader, 260,  20, FSCTLN );
        GetMD( psFile, pachHeader, 280,   6, FSDWNG );
        if( EQUALN(pachHeader+280,"999998",6) )
        {
            GetMD( psFile, pachHeader, 286,  40, FSDEVT );
            nCOff += 40;
        }
        GetMD( psFile, pachHeader, 286+nCOff,   5, FSCOP  );
        GetMD( psFile, pachHeader, 291+nCOff,   5, FSCPYS );
        GetMD( psFile, pachHeader, 296+nCOff,   1, ENCRYP );
        GetMD( psFile, pachHeader, 297+nCOff,  27, ONAME  );
        GetMD( psFile, pachHeader, 324+nCOff,  18, OPHONE );
        NITFGetField(szTemp, pachHeader, 342+nCOff, 12);
    }

    if (!bTriedStreamingFileHeader &&
         EQUAL(szTemp, "999999999999"))
    {
        GUIntBig nFileSize;
        GByte abyDELIM2_L2[12];
        GByte abyL1_DELIM1[11];

        bTriedStreamingFileHeader = TRUE;
        CPLDebug("NITF", "Total file unknown. Trying to get a STREAMING_FILE_HEADER");

        VSIFSeekL( fp, 0, SEEK_END );
        nFileSize = VSIFTellL(fp);

        VSIFSeekL( fp, nFileSize - 11, SEEK_SET );
        abyDELIM2_L2[11] = '\0';

        if (VSIFReadL( abyDELIM2_L2, 1, 11, fp ) == 11 &&
            abyDELIM2_L2[0] == 0x0E && abyDELIM2_L2[1] == 0xCA &&
            abyDELIM2_L2[2] == 0x14 && abyDELIM2_L2[3] == 0xBF)
        {
            int SFHL2 = atoi((const char*)(abyDELIM2_L2 + 4));
            if (SFHL2 > 0 && nFileSize > 11 + SFHL2 + 11 )
            {
                VSIFSeekL( fp, nFileSize - 11 - SFHL2 - 11 , SEEK_SET );

                if ( VSIFReadL( abyL1_DELIM1, 1, 11, fp ) == 11 &&
                     abyL1_DELIM1[7] == 0x0A && abyL1_DELIM1[8] == 0x6E &&
                     abyL1_DELIM1[9] == 0x1D && abyL1_DELIM1[10] == 0x97 &&
                     memcmp(abyL1_DELIM1, abyDELIM2_L2 + 4, 7) == 0 )
                {
                    if (SFHL2 == nHeaderLen)
                    {
                        CSLDestroy(psFile->papszMetadata);
                        psFile->papszMetadata = NULL;

                        if ( (int)VSIFReadL( pachHeader, 1, SFHL2, fp ) != SFHL2 )
                        {
                            VSIFCloseL(fp);
                            CPLFree(pachHeader);
                            CPLFree(psFile);
                            return NULL;
                        }

                        goto retry_read_header;
                    }
                }
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Collect segment info for the types we care about.               */
/* -------------------------------------------------------------------- */
    nNextData = nHeaderLen;

    nOffset = nHeaderLenOffset + 6;

    nOffset = NITFCollectSegmentInfo( psFile, nHeaderLen, nOffset,"IM",6, 10, &nNextData );

    if (nOffset != -1)
        nOffset = NITFCollectSegmentInfo( psFile, nHeaderLen, nOffset, "GR", 4, 6, &nNextData);

    /* LA Called NUMX in NITF 2.1 */
    if (nOffset != -1)
        nOffset = NITFCollectSegmentInfo( psFile, nHeaderLen, nOffset, "LA", 4, 3, &nNextData);

    if (nOffset != -1)
        nOffset = NITFCollectSegmentInfo( psFile, nHeaderLen, nOffset, "TX", 4, 5, &nNextData);

    if (nOffset != -1)
        nOffset = NITFCollectSegmentInfo( psFile, nHeaderLen, nOffset, "DE", 4, 9, &nNextData);

    if (nOffset != -1)
        nOffset = NITFCollectSegmentInfo( psFile, nHeaderLen, nOffset, "RE", 4, 7, &nNextData);
    else
    {
        NITFClose(psFile);
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Is there User Define Header Data? (TREs)                        */
/* -------------------------------------------------------------------- */
    if (nHeaderLen < nOffset + 5)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "NITF header too small");
        NITFClose(psFile);
        return NULL;
    }

    psFile->nTREBytes = 
        atoi(NITFGetField( szTemp, pachHeader, nOffset, 5 ));
    if (psFile->nTREBytes < 0)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Invalid TRE size : %d", psFile->nTREBytes);
        NITFClose(psFile);
        return NULL;
    }
    nOffset += 5;

    if( psFile->nTREBytes == 3 )
    {
        nOffset += 3; /* UDHOFL */
        psFile->nTREBytes = 0;
    }
    else if( psFile->nTREBytes > 3 )
    {
        nOffset += 3; /* UDHOFL */
        psFile->nTREBytes -= 3;

        if (nHeaderLen < nOffset + psFile->nTREBytes)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "NITF header too small");
            NITFClose(psFile);
            return NULL;
        }

        psFile->pachTRE = (char *) VSIMalloc(psFile->nTREBytes);
        if (psFile->pachTRE == NULL)
        {
            CPLError(CE_Failure, CPLE_OutOfMemory,
                     "Cannot allocate %d bytes", psFile->nTREBytes);
            NITFClose(psFile);
            return NULL;
        }
        memcpy( psFile->pachTRE, pachHeader + nOffset, 
                psFile->nTREBytes );
    }

/* -------------------------------------------------------------------- */
/*      Is there Extended Header Data?  (More TREs)                     */
/* -------------------------------------------------------------------- */
    if( nHeaderLen > nOffset + 8 )
    {
        int nXHDL = 
            atoi(NITFGetField( szTemp, pachHeader, nOffset, 5 ));
        if (nXHDL < 0)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                    "Invalid XHDL value : %d", nXHDL);
            NITFClose(psFile);
            return NULL;
        }

        nOffset += 5; /* XHDL */

        if( nXHDL > 3 )
        {
            char* pachNewTRE;

            nOffset += 3; /* XHDLOFL */
            nXHDL -= 3;

            if (nHeaderLen < nOffset + nXHDL)
            {
                CPLError(CE_Failure, CPLE_AppDefined, "NITF header too small");
                NITFClose(psFile);
                return NULL;
            }

            pachNewTRE = (char *) 
                VSIRealloc( psFile->pachTRE, 
                            psFile->nTREBytes + nXHDL );
            if (pachNewTRE == NULL)
            {
                CPLError(CE_Failure, CPLE_OutOfMemory,
                     "Cannot allocate %d bytes", psFile->nTREBytes + nXHDL);
                NITFClose(psFile);
                return NULL;
            }
            psFile->pachTRE = pachNewTRE;
            memcpy( psFile->pachTRE, pachHeader + nOffset, nXHDL );
            psFile->nTREBytes += nXHDL;
        }
    }

    return psFile;
}

/************************************************************************/
/*                             NITFClose()                              */
/************************************************************************/

void NITFClose( NITFFile *psFile )

{
    int  iSegment;

    for( iSegment = 0; iSegment < psFile->nSegmentCount; iSegment++ )
    {
        NITFSegmentInfo *psSegInfo = psFile->pasSegmentInfo + iSegment;

        if( psSegInfo->hAccess == NULL )
            continue;

        if( EQUAL(psSegInfo->szSegmentType,"IM"))
            NITFImageDeaccess( (NITFImage *) psSegInfo->hAccess );
        else if( EQUAL(psSegInfo->szSegmentType,"DE"))
            NITFDESDeaccess( (NITFDES *) psSegInfo->hAccess );
        else
        {
            CPLAssert( FALSE );
        }
    }

    CPLFree( psFile->pasSegmentInfo );
    if( psFile->fp != NULL )
        VSIFCloseL( psFile->fp );
    CPLFree( psFile->pachHeader );
    CSLDestroy( psFile->papszMetadata );
    CPLFree( psFile->pachTRE );

    if (psFile->psNITFSpecNode)
        CPLDestroyXMLNode(psFile->psNITFSpecNode);

    CPLFree( psFile );
}

static void NITFGotoOffset(VSILFILE* fp, GUIntBig nLocation)
{
    GUIntBig nCurrentLocation = VSIFTellL(fp);
    if (nLocation > nCurrentLocation)
    {
        GUIntBig nFileSize;
        int iFill;
        char cSpace = ' ';

        VSIFSeekL(fp, 0, SEEK_END);
        nFileSize = VSIFTellL(fp);
        if (nLocation > nFileSize)
        {
            for(iFill = 0; iFill < nLocation - nFileSize; iFill++)
                VSIFWriteL(&cSpace, 1, 1, fp);
        }
        else
            VSIFSeekL(fp, nLocation, SEEK_SET);
    }
    else if (nLocation < nCurrentLocation)
    {
        VSIFSeekL(fp, nLocation, SEEK_SET);
    }

}

/************************************************************************/
/*                             NITFCreate()                             */
/*                                                                      */
/*      Create a new uncompressed NITF file.                            */
/************************************************************************/

int NITFCreate( const char *pszFilename, 
                      int nPixels, int nLines, int nBands, 
                      int nBitsPerSample, const char *pszPVType,
                      char **papszOptions )

{
    VSILFILE	*fp;
    GUIntBig    nCur = 0;
    int         nOffset = 0, iBand, nIHSize, nNPPBH, nNPPBV;
    GIntBig     nImageSize;
    int         nNBPR, nNBPC;
    const char *pszIREP;
    const char *pszIC = CSLFetchNameValue(papszOptions,"IC");
    int nCLevel;
    const char *pszNUMT;
    int nHL, nNUMT = 0;
    vsi_l_offset nOffsetUDIDL;
    const char *pszVersion;
    int iIM, nIM = 1;
    const char *pszNUMI;
    int iGS, nGS = 0; // number of graphic segment
    const char *pszNUMS; // graphic segment option string

    if (nBands <= 0 || nBands > 99999)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Invalid band number : %d", nBands);
        return FALSE;
    }

    if( pszIC == NULL )
        pszIC = "NC";

/* -------------------------------------------------------------------- */
/*      Fetch some parameter overrides.                                 */
/* -------------------------------------------------------------------- */
    pszIREP = CSLFetchNameValue( papszOptions, "IREP" );
    if( pszIREP == NULL )
        pszIREP = "MONO";

    pszNUMT = CSLFetchNameValue( papszOptions, "NUMT" );
    if( pszNUMT != NULL )
    {
        nNUMT = atoi(pszNUMT);
        if (nNUMT < 0 || nNUMT > 999)
        {
            CPLError( CE_Failure, CPLE_AppDefined, 
                    "Invalid NUMT value : %s", pszNUMT);
            return FALSE;
        }
    }

    pszNUMI = CSLFetchNameValue( papszOptions, "NUMI" );
    if (pszNUMI != NULL)
    {
        nIM = atoi(pszNUMI);
        if (nIM < 1 || nIM > 999)
        {
            CPLError( CE_Failure, CPLE_AppDefined, 
                    "Invalid NUMI value : %s", pszNUMI);
            return FALSE;
        }
        if (nIM != 1 && !EQUAL(pszIC, "NC"))
        {
            CPLError( CE_Failure, CPLE_AppDefined, 
                    "Unable to create file with multiple images and compression at the same time");
            return FALSE;
        }
    }
    
    // Reads and validates graphics segment number option
    pszNUMS = CSLFetchNameValue(papszOptions, "NUMS");
    if (pszNUMS != NULL)
    {
        nGS = atoi(pszNUMS);
        if (nGS < 0 || nGS > 999)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Invalid NUMS value : %s",
                            pszNUMS);
            return FALSE;
        }
    }



/* -------------------------------------------------------------------- */
/*      Compute raw image size, blocking factors and so forth.          */
/* -------------------------------------------------------------------- */
    nNPPBH = nPixels;
    nNPPBV = nLines;

    if( CSLFetchNameValue( papszOptions, "BLOCKSIZE" ) != NULL )
        nNPPBH = nNPPBV = atoi(CSLFetchNameValue( papszOptions, "BLOCKSIZE" ));

    if( CSLFetchNameValue( papszOptions, "BLOCKXSIZE" ) != NULL )
        nNPPBH = atoi(CSLFetchNameValue( papszOptions, "BLOCKXSIZE" ));

    if( CSLFetchNameValue( papszOptions, "BLOCKYSIZE" ) != NULL )
        nNPPBV = atoi(CSLFetchNameValue( papszOptions, "BLOCKYSIZE" ));
    
    if( CSLFetchNameValue( papszOptions, "NPPBH" ) != NULL )
        nNPPBH = atoi(CSLFetchNameValue( papszOptions, "NPPBH" ));
    
    if( CSLFetchNameValue( papszOptions, "NPPBV" ) != NULL )
        nNPPBV = atoi(CSLFetchNameValue( papszOptions, "NPPBV" ));
        
        
    if (EQUAL(pszIC, "NC") &&
        (nPixels > 8192 || nLines > 8192) && 
        nNPPBH == nPixels && nNPPBV == nLines)
    {
        /* See MIL-STD-2500-C, paragraph 5.4.2.2-d (#3263) */
        nNBPR = 1;
        nNBPC = 1;
        nNPPBH = 0;
        nNPPBV = 0;
        
        nImageSize = 
            ((nBitsPerSample)/8) 
            * ((GIntBig) nPixels *nLines)
            * nBands;
    }
    else if (EQUAL(pszIC, "NC") &&
             nPixels > 8192 && nNPPBH == nPixels)
    {
        /* See MIL-STD-2500-C, paragraph 5.4.2.2-d */
        nNBPR = 1;
        nNPPBH = 0;
        nNBPC = (nLines + nNPPBV - 1) / nNPPBV;

        if ( nNBPC > 9999 )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Unable to create file %s,\n"
                      "Too many blocks : %d x %d",
                     pszFilename, nNBPR, nNBPC);
            return FALSE;
        }

        nImageSize =
            ((nBitsPerSample)/8)
            * ((GIntBig) nPixels * (nNBPC * nNPPBV))
            * nBands;
    }
    else if (EQUAL(pszIC, "NC") &&
             nLines > 8192 && nNPPBV == nLines)
    {
        /* See MIL-STD-2500-C, paragraph 5.4.2.2-d */
        nNBPC = 1;
        nNPPBV = 0;
        nNBPR = (nPixels + nNPPBH - 1) / nNPPBH;

        if ( nNBPR > 9999 )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Unable to create file %s,\n"
                      "Too many blocks : %d x %d",
                     pszFilename, nNBPR, nNBPC);
            return FALSE;
        }

        nImageSize =
            ((nBitsPerSample)/8)
            * ((GIntBig) nLines * (nNBPR * nNPPBH))
            * nBands;
    }
    else
    {
        if( nNPPBH <= 0 || nNPPBV <= 0 ||
            nNPPBH > 9999 || nNPPBV > 9999  )
            nNPPBH = nNPPBV = 256;

        nNBPR = (nPixels + nNPPBH - 1) / nNPPBH;
        nNBPC = (nLines + nNPPBV - 1) / nNPPBV;
        if ( nNBPR > 9999 || nNBPC > 9999 )
        {
            CPLError( CE_Failure, CPLE_AppDefined, 
                      "Unable to create file %s,\n"
                      "Too many blocks : %d x %d",
                     pszFilename, nNBPR, nNBPC);
            return FALSE;
        }

        nImageSize = 
            ((nBitsPerSample)/8) 
            * ((GIntBig) nNBPR * nNBPC)
            * nNPPBH * nNPPBV * nBands;
    }

    if (EQUAL(pszIC, "NC"))
    {
        if ((double)nImageSize >= 1e10 - 1)
        {
            CPLError( CE_Failure, CPLE_AppDefined, 
                    "Unable to create file %s,\n"
                    "Too big image size : " CPL_FRMT_GUIB,
                    pszFilename, nImageSize );
            return FALSE;
        }
        if ((double)(nImageSize * nIM) >= 1e12 - 1)
        {
            CPLError( CE_Failure, CPLE_AppDefined, 
                    "Unable to create file %s,\n"
                    "Too big file size : " CPL_FRMT_GUIB,
                    pszFilename, nImageSize * nIM );
            return FALSE;
        }
    }

/* -------------------------------------------------------------------- */
/*      Open new file.                                                  */
/* -------------------------------------------------------------------- */
    fp = VSIFOpenL( pszFilename, "wb+" );
    if( fp == NULL )
    {
        CPLError( CE_Failure, CPLE_OpenFailed, 
                  "Unable to create file %s,\n"
                  "check path and permissions.",
                  pszFilename );
        return FALSE;
    }


/* -------------------------------------------------------------------- */
/*      Work out the version we are producing.  For now we really       */
/*      only support creating NITF02.10 or the nato analog              */
/*      NSIF01.00.                                                      */
/* -------------------------------------------------------------------- */
    pszVersion = CSLFetchNameValue( papszOptions, "FHDR" );
    if( pszVersion == NULL )
        pszVersion = "NITF02.10";
    else if( !EQUAL(pszVersion,"NITF02.10") 
             && !EQUAL(pszVersion,"NSIF01.00") )
    {
        CPLError( CE_Warning, CPLE_AppDefined, 
                  "FHDR=%s not supported, switching to NITF02.10.",
                  pszVersion );
        pszVersion = "NITF02.10";
    }

/* -------------------------------------------------------------------- */
/*      Prepare the file header.                                        */
/* -------------------------------------------------------------------- */

#define PLACE(location,name,text)  { \
    const char* _text = text; \
    NITFGotoOffset(fp, location); \
    VSIFWriteL(_text, 1, strlen(_text), fp); }

#define OVR(width,location,name,text) { 				\
    const char* _text = text; \
    const char *pszParmValue; 						\
    pszParmValue = CSLFetchNameValue( papszOptions, #name ); 		\
    if( pszParmValue == NULL )						\
        pszParmValue = _text;						\
    NITFGotoOffset(fp, location); \
    VSIFWriteL(pszParmValue, 1, MIN(width,strlen(pszParmValue)), fp); }

#define WRITE_BYTE(location, val) { \
    char cVal = val; \
    NITFGotoOffset(fp, location); \
    VSIFWriteL(&cVal, 1, 1, fp); }

    VSIFSeekL(fp, 0, SEEK_SET);

    PLACE (  0, FDHR_FVER,    pszVersion                      );
    OVR( 2,  9, CLEVEL,       "03"                            );  /* Patched at the end */
    PLACE ( 11, STYPE        ,"BF01"                          );
    OVR(10, 15, OSTAID       ,"GDAL"                          );
    OVR(14, 25, FDT          ,"20021216151629"                );
    OVR(80, 39, FTITLE       ,""                              );
    OVR( 1,119, FSCLAS       ,"U"                             );
    OVR( 2,120, FSCLSY       ,""                              );
    OVR(11,122, FSCODE       ,""                              );
    OVR( 2,133, FSCTLH       ,""                              );
    OVR(20,135, FSREL        ,""                              );
    OVR( 2,155, FSDCTP       ,""                              );
    OVR( 8,157, FSDCDT       ,""                              );
    OVR( 4,165, FSDCXM       ,""                              );
    OVR( 1,169, FSDG         ,""                              );
    OVR( 8,170, FSDGDT       ,""                              );
    OVR(43,178, FSCLTX       ,""                              );
    OVR( 1,221, FSCATP       ,""                              );
    OVR(40,222, FSCAUT       ,""                              );
    OVR( 1,262, FSCRSN       ,""                              );
    OVR( 8,263, FSSRDT       ,""                              );
    OVR(15,271, FSCTLN       ,""                              );
    OVR( 5,286, FSCOP        ,"00000"                         );
    OVR( 5,291, FSCPYS       ,"00000"                         );
    PLACE (296, ENCRYP       ,"0"                             );
    WRITE_BYTE(297, 0x00); /* FBKGC */
    WRITE_BYTE(298, 0x00);
    WRITE_BYTE(299, 0x00);
    OVR(24,300, ONAME        ,""                              );
    OVR(18,324, OPHONE       ,""                              );
    PLACE (342, FL           ,"????????????"                  );
    PLACE (354, HL           ,"??????"                        );
    PLACE (360, NUMI         ,CPLSPrintf("%03d", nIM)         );

    nHL = 363;
    for(iIM=0;iIM<nIM;iIM++)
    {
        PLACE (nHL,     LISHi    ,"??????"                        );
        PLACE (nHL + 6, LIi      ,CPLSPrintf("%010" CPL_FRMT_GB_WITHOUT_PREFIX "d", nImageSize)  );
        nHL += 6 + 10;
    }

    // Creates Header entries for graphic segment
    //    NUMS: number of segment
    // For each segment:
    // 	  LSSH[i]: subheader length (4 byte), set to be 258, the size for
    //				minimal amount of information.
    //    LS[i] data length (6 byte)
    PLACE (nHL,     NUMS         ,CPLSPrintf("%03d",nGS)        );
    nHL += 3; // Move three characters
    for (iGS = 0; iGS < nGS; iGS++)
    {
        PLACE (nHL, LSSHi ,CPLSPrintf("0000") );
        PLACE (nHL + 4, LSi ,CPLSPrintf("000000") );
        nHL += 4 + 6;
    }

    PLACE (nHL, NUMX         ,"000"                           );
    PLACE (nHL + 3, NUMT         ,CPLSPrintf("%03d",nNUMT)        );

    PLACE (nHL + 6, LTSHnLTn     ,""                              );

    nHL += 6 + (4+5) * nNUMT;

    PLACE (nHL, NUMDES       ,"000"                           );
    nHL += 3;
    PLACE (nHL, NUMRES       ,"000"                           );
    nHL += 3;
    PLACE (nHL, UDHDL        ,"00000"                         );
    nHL += 5;
    PLACE (nHL, XHDL         ,"00000"                         );
    nHL += 5;

    if( CSLFetchNameValue(papszOptions,"FILE_TRE") != NULL )
    {
        NITFWriteTREsFromOptions(
            fp,
            nHL - 10,
            &nHL,
            papszOptions, "FILE_TRE=" );
    }

    if (nHL > 999999)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Too big file header length : %d", nHL);
        VSIFCloseL( fp );
        return FALSE;
    }

    // update header length
    PLACE (354, HL           ,CPLSPrintf("%06d",nHL)          );

    nCur = nHL;

/* -------------------------------------------------------------------- */
/*      Prepare the image header.                                       */
/* -------------------------------------------------------------------- */
  for(iIM=0;iIM<nIM;iIM++)
  {
    char** papszIREPBANDTokens = NULL;
    char** papszISUBCATTokens = NULL;

    if( CSLFetchNameValue(papszOptions,"IREPBAND") != NULL )
    {
        papszIREPBANDTokens = CSLTokenizeStringComplex(
            CSLFetchNameValue(papszOptions,"IREPBAND"), ",", 0, 0 );
        if( papszIREPBANDTokens != NULL && CSLCount( papszIREPBANDTokens ) != nBands)
        {
            CSLDestroy(  papszIREPBANDTokens );
            papszIREPBANDTokens = NULL;
        }
    }
    if( CSLFetchNameValue(papszOptions,"ISUBCAT") != NULL )
    {
        papszISUBCATTokens = CSLTokenizeStringComplex(
            CSLFetchNameValue(papszOptions,"ISUBCAT"), ",", 0, 0 );
        if( papszISUBCATTokens != NULL && CSLCount( papszISUBCATTokens ) != nBands)
        {
            CSLDestroy( papszISUBCATTokens );
            papszISUBCATTokens = NULL;
        }
    }

    VSIFSeekL(fp, nCur, SEEK_SET);

    PLACE (nCur+  0, IM           , "IM"                           );
    OVR(10,nCur+  2, IID1         , "Missing"                      );
    OVR(14,nCur+ 12, IDATIM       , "20021216151629"               );
    OVR(17,nCur+ 26, TGTID        , ""                             );
    OVR(80,nCur+ 43, IID2         , ""                             );
    OVR( 1,nCur+123, ISCLAS       , "U"                            );
    OVR( 2,nCur+124, ISCLSY       , ""                             );
    OVR(11,nCur+126, ISCODE       , ""                             );
    OVR( 2,nCur+137, ISCTLH       , ""                             );
    OVR(20,nCur+139, ISREL        , ""                             );
    OVR( 2,nCur+159, ISDCTP       , ""                             );
    OVR( 8,nCur+161, ISDCDT       , ""                             );
    OVR( 4,nCur+169, ISDCXM       , ""                             );
    OVR( 1,nCur+173, ISDG         , ""                             );
    OVR( 8,nCur+174, ISDGDT       , ""                             );
    OVR(43,nCur+182, ISCLTX       , ""                             );
    OVR( 1,nCur+225, ISCATP       , ""                             );
    OVR(40,nCur+226, ISCAUT       , ""                             );
    OVR( 1,nCur+266, ISCRSN       , ""                             );
    OVR( 8,nCur+267, ISSRDT       , ""                             );
    OVR(15,nCur+275, ISCTLN       , ""                             );
    PLACE (nCur+290, ENCRYP       , "0"                            );
    OVR(42,nCur+291, ISORCE       , "Unknown"                      );
    PLACE (nCur+333, NROWS        , CPLSPrintf("%08d", nLines)     );
    PLACE (nCur+341, NCOLS        , CPLSPrintf("%08d", nPixels)    );
    PLACE (nCur+349, PVTYPE       , pszPVType                      );
    PLACE (nCur+352, IREP         , pszIREP                        );
    OVR( 8,nCur+360, ICAT         , "VIS"                          );
    OVR( 2,nCur+368, ABPP         , CPLSPrintf("%02d",nBitsPerSample) );
    OVR( 1,nCur+370, PJUST        , "R"                            );
    OVR( 1,nCur+371, ICORDS       , " "                            );

    nOffset = 372;

    {
        const char *pszParmValue;
        pszParmValue = CSLFetchNameValue( papszOptions, "ICORDS" );
        if( pszParmValue == NULL )
            pszParmValue = " ";
        if( *pszParmValue != ' ' )
        {
            OVR(60,nCur+nOffset, IGEOLO, ""                            );
            nOffset += 60;
        }
    }

    {
        const char* pszICOM = CSLFetchNameValue( papszOptions, "ICOM");
        if (pszICOM != NULL)
        {
            int nLenICOM = strlen(pszICOM);
            int nICOM = (79 + nLenICOM) / 80;
            if (nICOM > 9)
            {
                CPLError(CE_Warning, CPLE_NotSupported, "ICOM will be truncated");
                nICOM = 9;
            }
            PLACE (nCur+nOffset, NICOM    , CPLSPrintf("%01d",nICOM) );
            VSIFWriteL(pszICOM, 1, MIN(nICOM * 80, nLenICOM), fp);
            nOffset += nICOM * 80;
        }
        else
        {
            PLACE (nCur+nOffset, NICOM    , "0"                            );
        }
    }

    OVR( 2,nCur+nOffset+1, IC     , "NC"                           );

    if( pszIC[0] != 'N' )
    {
        OVR( 4,nCur+nOffset+3, COMRAT , "    "                     );
        nOffset += 4;
    }

    if (nBands <= 9)
    {
        PLACE (nCur+nOffset+3, NBANDS , CPLSPrintf("%d",nBands)        );
    }
    else
    {
        PLACE (nCur+nOffset+3, NBANDS , "0"        );
        PLACE (nCur+nOffset+4, XBANDS , CPLSPrintf("%05d",nBands)        );
        nOffset += 5;
    }

    nOffset += 4;

/* -------------------------------------------------------------------- */
/*      Per band info                                                   */
/* -------------------------------------------------------------------- */
    for( iBand = 0; iBand < nBands; iBand++ )
    {
        const char *pszIREPBAND = "M";

        if( papszIREPBANDTokens != NULL )
        {
            if (strlen(papszIREPBANDTokens[iBand]) > 2)
            {
                papszIREPBANDTokens[iBand][2] = '\0';
                CPLError(CE_Warning, CPLE_NotSupported,
                         "Truncating IREPBAND[%d] to '%s'",
                         iBand + 1, papszIREPBANDTokens[iBand]);
            }
            pszIREPBAND = papszIREPBANDTokens[iBand];
        }
        else if( EQUAL(pszIREP,"RGB/LUT") )
            pszIREPBAND = "LU";
        else if( EQUAL(pszIREP,"RGB") )
        {
            if( iBand == 0 )
                pszIREPBAND = "R";
            else if( iBand == 1 )
                pszIREPBAND = "G";
            else if( iBand == 2 )
                pszIREPBAND = "B";
        }
        else if( EQUALN(pszIREP,"YCbCr",5) )
        {
            if( iBand == 0 )
                pszIREPBAND = "Y";
            else if( iBand == 1 )
                pszIREPBAND = "Cb";
            else if( iBand == 2 )
                pszIREPBAND = "Cr";
        }

        PLACE(nCur+nOffset+ 0, IREPBANDn, pszIREPBAND                 );

        if( papszISUBCATTokens != NULL )
        {
            if (strlen(papszISUBCATTokens[iBand]) > 6)
            {
                papszISUBCATTokens[iBand][6] = '\0';
                CPLError(CE_Warning, CPLE_NotSupported,
                         "Truncating ISUBCAT[%d] to '%s'",
                         iBand + 1, papszISUBCATTokens[iBand]);
            }
            PLACE(nCur+nOffset+ 2, ISUBCATn, papszISUBCATTokens[iBand] );
        }
//      else
//          PLACE(nCur+nOffset+ 2, ISUBCATn, ""                           );

        PLACE(nCur+nOffset+ 8, IFCn  , "N"                            );
//      PLACE(nCur+nOffset+ 9, IMFLTn, ""                             );

        if( !EQUAL(pszIREP,"RGB/LUT") )
        {
            PLACE(nCur+nOffset+12, NLUTSn, "0"                        );
            nOffset += 13;
        }
        else
        {
            int iC, nCount=256;

            if( CSLFetchNameValue(papszOptions,"LUT_SIZE") != NULL )
                nCount = atoi(CSLFetchNameValue(papszOptions,"LUT_SIZE"));

            if (!(nCount >= 0 && nCount <= 99999))
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "Invalid LUT value : %d. Defaulting to 256", nCount);
                nCount = 256;
            }
            PLACE(nCur+nOffset+12, NLUTSn, "3"                        );
            PLACE(nCur+nOffset+13, NELUTn, CPLSPrintf("%05d",nCount)  );

            for( iC = 0; iC < nCount; iC++ )
            {
                WRITE_BYTE(nCur+nOffset+18+iC+       0, (char) iC);
                WRITE_BYTE(nCur+nOffset+18+iC+nCount*1, (char) iC);
                WRITE_BYTE(nCur+nOffset+18+iC+nCount*2, (char) iC);
            }
            nOffset += 18 + nCount*3;
        }
    }

    CSLDestroy(papszIREPBANDTokens);
    CSLDestroy(papszISUBCATTokens);

/* -------------------------------------------------------------------- */
/*      Remainder of image header info.                                 */
/* -------------------------------------------------------------------- */
    PLACE(nCur+nOffset+  0, ISYNC , "0"                            );

    /* RGB JPEG compressed NITF requires IMODE=P (see #3345) */
    if (nBands >= 3 && (EQUAL(pszIC, "C3") || EQUAL(pszIC, "M3")))
    {
        PLACE(nCur+nOffset+  1, IMODE , "P"                            );
    }
    else
    {
        PLACE(nCur+nOffset+  1, IMODE , "B"                            );
    }
    PLACE(nCur+nOffset+  2, NBPR  , CPLSPrintf("%04d",nNBPR)       );
    PLACE(nCur+nOffset+  6, NBPC  , CPLSPrintf("%04d",nNBPC)       );
    PLACE(nCur+nOffset+ 10, NPPBH , CPLSPrintf("%04d",nNPPBH)      );
    PLACE(nCur+nOffset+ 14, NPPBV , CPLSPrintf("%04d",nNPPBV)      );
    PLACE(nCur+nOffset+ 18, NBPP  , CPLSPrintf("%02d",nBitsPerSample) );
    PLACE(nCur+nOffset+ 20, IDLVL , "001"                          );
    PLACE(nCur+nOffset+ 23, IALVL , "000"                          );
    PLACE(nCur+nOffset+ 26, ILOC  , "0000000000"                   );
    PLACE(nCur+nOffset+ 36, IMAG  , "1.0 "                         );
    PLACE(nCur+nOffset+ 40, UDIDL , "00000"                        );
    PLACE(nCur+nOffset+ 45, IXSHDL, "00000"                        );

    nOffsetUDIDL = nCur + nOffset + 40;
    nOffset += 50;

/* -------------------------------------------------------------------- */
/*      Add BLOCKA TRE if requested.                                    */
/* -------------------------------------------------------------------- */
    if( CSLFetchNameValue(papszOptions,"BLOCKA_BLOCK_COUNT") != NULL )
    {
        NITFWriteBLOCKA( fp,
                         nOffsetUDIDL, 
                         &nOffset, 
                         papszOptions );
    }

    if( CSLFetchNameValue(papszOptions,"TRE") != NULL )
    {
        NITFWriteTREsFromOptions(
            fp,
            nOffsetUDIDL, 
            &nOffset, 
            papszOptions, "TRE=" );
    }

/* -------------------------------------------------------------------- */
/*      Update the image header length in the file header.              */
/* -------------------------------------------------------------------- */
    nIHSize = nOffset;

    if (nIHSize > 999999)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Too big image header length : %d", nIHSize);
        VSIFCloseL( fp );
        return FALSE;
    }

    PLACE( 363 + iIM * 16, LISH1, CPLSPrintf("%06d",nIHSize)      );

    nCur += nIHSize + nImageSize;
  }

/* -------------------------------------------------------------------- */
/*      Compute and update CLEVEL ("complexity" level).                 */
/*      See: http://164.214.2.51/ntb/baseline/docs/2500b/2500b_not2.pdf */
/*            page 96u                                                  */
/* -------------------------------------------------------------------- */
    nCLevel = 3;
    if (nBands > 9 || nIM > 20 || nPixels > 2048 || nLines > 2048 ||
        nNPPBH > 2048 || nNPPBV > 2048 || nCur > 52428799 )
    {
        nCLevel = 5;
    }
    if (nPixels > 8192 || nLines > 8192 ||
        nNPPBH > 8192 || nNPPBV > 8192 || nCur > 1073741833)
    {
        nCLevel = 6;
    }
    if (nBands > 256 || nPixels > 65536 || nLines > 65536 ||
        nCur > 2147483647)
    {
        nCLevel = 7;
    }
    OVR( 2,  9, CLEVEL,       CPLSPrintf("%02d", nCLevel)     );

/* -------------------------------------------------------------------- */
/*      Update total file length                                        */
/* -------------------------------------------------------------------- */

    /* According to the spec, CLEVEL 7 supports up to 10,737,418,330 bytes */
    /* but we can support technically much more */
    if (EQUAL(pszIC, "NC") && GUINTBIG_TO_DOUBLE(nCur) >= 1e12 - 1)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Too big file : " CPL_FRMT_GUIB, nCur);
        VSIFCloseL( fp );
        return FALSE;
    }

    PLACE( 342, FL,
          CPLSPrintf( "%012" CPL_FRMT_GB_WITHOUT_PREFIX "d", nCur) );

/* -------------------------------------------------------------------- */
/*      Grow file to full required size by writing one byte at the end. */
/* -------------------------------------------------------------------- */
    if( EQUAL(pszIC,"NC") )
    {
        char cNul = 0;
        VSIFSeekL( fp, nCur-1, SEEK_SET );
        VSIFWriteL( &cNul, 1, 1, fp );
    }

    VSIFCloseL( fp );

    return TRUE;
}

/************************************************************************/
/*                            NITFWriteTRE()                            */
/************************************************************************/

static int NITFWriteTRE( VSILFILE* fp,
                         vsi_l_offset nOffsetUDIDL, 
                         int  *pnOffset,
                         const char *pszTREName, char *pabyTREData, int nTREDataSize )

{
    char szTemp[12];
    int  nOldOffset;

/* -------------------------------------------------------------------- */
/*      Update IXSHDL.                                                  */
/* -------------------------------------------------------------------- */
    VSIFSeekL(fp, nOffsetUDIDL + 5, SEEK_SET);
    VSIFReadL(szTemp, 1, 5, fp);
    szTemp[5] = 0;
    nOldOffset = atoi(szTemp);

    if( nOldOffset == 0 )
    {
        nOldOffset = 3;
        PLACE(nOffsetUDIDL+10, IXSOFL, "000" );
        *pnOffset += 3;
    }

    if (nOldOffset + 11 + nTREDataSize > 99999 || nTREDataSize > 99999)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Too big TRE to be written");
        return FALSE;
    }

    sprintf( szTemp, "%05d", nOldOffset + 11 + nTREDataSize );
    PLACE( nOffsetUDIDL + 5, IXSHDL, szTemp );

/* -------------------------------------------------------------------- */
/*      Create TRE prefix.                                              */
/* -------------------------------------------------------------------- */
    sprintf( szTemp, "%-6s%05d", 
             pszTREName, nTREDataSize );
    VSIFSeekL(fp, nOffsetUDIDL + 10 + nOldOffset, SEEK_SET);
    VSIFWriteL(szTemp, 11, 1, fp);
    VSIFWriteL(pabyTREData, nTREDataSize, 1, fp);

/* -------------------------------------------------------------------- */
/*      Increment values.                                               */
/* -------------------------------------------------------------------- */
    *pnOffset += nTREDataSize + 11;

    return TRUE;
}

/************************************************************************/
/*                   NITFWriteTREsFromOptions()                         */
/************************************************************************/

static int NITFWriteTREsFromOptions(
    VSILFILE* fp,
    vsi_l_offset nOffsetUDIDL,
    int *pnOffset,
    char **papszOptions, const char* pszTREPrefix )    

{
    int bIgnoreBLOCKA = 
        CSLFetchNameValue(papszOptions,"BLOCKA_BLOCK_COUNT") != NULL;
    int iOption;
    int nTREPrefixLen = strlen(pszTREPrefix);

    if( papszOptions == NULL )
        return TRUE;

    for( iOption = 0; papszOptions[iOption] != NULL; iOption++ )
    {
        const char *pszEscapedContents;
        char *pszUnescapedContents;
        char *pszTREName;
        int  nContentLength;
        const char* pszSpace;

        if( !EQUALN(papszOptions[iOption], pszTREPrefix, nTREPrefixLen) )
            continue;

        if( EQUALN(papszOptions[iOption]+nTREPrefixLen,"BLOCKA=",7)
            && bIgnoreBLOCKA )
            continue;
        
        /* We do no longer use CPLParseNameValue() as it removes leading spaces */
        /* from the value (see #3088) */
        pszSpace = strchr(papszOptions[iOption]+nTREPrefixLen, '=');
        if (pszSpace == NULL)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Could not parse creation options %s", papszOptions[iOption]+nTREPrefixLen);
            return FALSE;
        }
        
        pszTREName = CPLStrdup(papszOptions[iOption]+nTREPrefixLen);
        pszTREName[MIN(6, pszSpace - (papszOptions[iOption]+nTREPrefixLen))] = '\0';
        pszEscapedContents = pszSpace + 1;

        pszUnescapedContents = 
            CPLUnescapeString( pszEscapedContents, &nContentLength,
                               CPLES_BackslashQuotable );

        if( !NITFWriteTRE( fp,
                           nOffsetUDIDL,
                           pnOffset,
                           pszTREName, pszUnescapedContents, 
                           nContentLength ) )
        {
            CPLFree( pszTREName );
            CPLFree( pszUnescapedContents );
            return FALSE;
        }
        
        CPLFree( pszTREName );
        CPLFree( pszUnescapedContents );

    }

    return TRUE;
}

/************************************************************************/
/*                          NITFWriteBLOCKA()                           */
/************************************************************************/

static int NITFWriteBLOCKA( VSILFILE* fp, vsi_l_offset nOffsetUDIDL,
                            int *pnOffset,
                            char **papszOptions )

{
    static const char *apszFields[] = { 
        "BLOCK_INSTANCE", "0", "2",
        "N_GRAY",         "2", "5",
        "L_LINES",        "7", "5",
        "LAYOVER_ANGLE",  "12", "3",
        "SHADOW_ANGLE",   "15", "3",
        "BLANKS",         "18", "16",
        "FRLC_LOC",       "34", "21",
        "LRLC_LOC",       "55", "21",
        "LRFC_LOC",       "76", "21",
        "FRFC_LOC",       "97", "21",
        NULL,             NULL, NULL };
    int nBlockCount = 
        atoi(CSLFetchNameValue( papszOptions, "BLOCKA_BLOCK_COUNT" ));
    int iBlock;

/* ==================================================================== */
/*      Loop over all the blocks we have metadata for.                  */
/* ==================================================================== */
    for( iBlock = 1; iBlock <= nBlockCount; iBlock++ )
    {
        char szBLOCKA[123];
        int iField;

/* -------------------------------------------------------------------- */
/*      Write all fields.                                               */
/* -------------------------------------------------------------------- */
        for( iField = 0; apszFields[iField*3] != NULL; iField++ )
        {
            char szFullFieldName[64];
            int  iStart = atoi(apszFields[iField*3+1]);
            int  iSize = atoi(apszFields[iField*3+2]);
            const char *pszValue;

            sprintf( szFullFieldName, "BLOCKA_%s_%02d", 
                     apszFields[iField*3 + 0], iBlock );

            pszValue = CSLFetchNameValue( papszOptions, szFullFieldName );
            if( pszValue == NULL )
                pszValue = "";

            if (strlen(pszValue) > (size_t)iSize)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Too much data for %s. Got %d bytes, max allowed is %d",
                         szFullFieldName, (int)strlen(pszValue), iSize);
                return FALSE;
            }

            /* Right align value and left pad with spaces */
            memset( szBLOCKA + iStart, ' ', iSize );
            memcpy( szBLOCKA + iStart + MAX((size_t)0,iSize-strlen(pszValue)),
                    pszValue, strlen(pszValue) );
        }

        // required field - semantics unknown. 
        memcpy( szBLOCKA + 118, "010.0", 5);

        if( !NITFWriteTRE( fp,
                           nOffsetUDIDL,
                           pnOffset,
                           "BLOCKA", szBLOCKA, 123 ) )
            return FALSE;
    }
    
    return TRUE;
}
                      
/************************************************************************/
/*                       NITFCollectSegmentInfo()                       */
/*                                                                      */
/*      Collect the information about a set of segments of a            */
/*      particular type from the NITF file header, and add them to      */
/*      the segment list in the NITFFile object.                        */
/************************************************************************/

static int 
NITFCollectSegmentInfo( NITFFile *psFile, int nFileHeaderLen, int nOffset, const char szType[3],
                        int nHeaderLenSize, int nDataLenSize, GUIntBig *pnNextData )

{
    char szTemp[12];
    int  nCount, nSegDefSize, iSegment;

/* -------------------------------------------------------------------- */
/*      Get the segment count, and grow the segmentinfo array           */
/*      accordingly.                                                    */
/* -------------------------------------------------------------------- */
    if ( nFileHeaderLen < nOffset + 3 )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Not enough bytes to read segment count");
        return -1;
    }

    NITFGetField( szTemp, psFile->pachHeader, nOffset, 3 );
    nCount = atoi(szTemp);

    if( nCount <= 0 )
        return nOffset + 3;

    nSegDefSize = nCount * (nHeaderLenSize + nDataLenSize);
    if ( nFileHeaderLen < nOffset + 3 + nSegDefSize)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Not enough bytes to read segment info");
        return -1;
    }

    if( psFile->pasSegmentInfo == NULL )
        psFile->pasSegmentInfo = (NITFSegmentInfo *)
            CPLMalloc( sizeof(NITFSegmentInfo) * nCount );
    else
        psFile->pasSegmentInfo = (NITFSegmentInfo *)
            CPLRealloc( psFile->pasSegmentInfo, 
                        sizeof(NITFSegmentInfo)
                        * (psFile->nSegmentCount+nCount) );

/* -------------------------------------------------------------------- */
/*      Collect detailed about segment.                                 */
/* -------------------------------------------------------------------- */
    for( iSegment = 0; iSegment < nCount; iSegment++ )
    {
        NITFSegmentInfo *psInfo = psFile->pasSegmentInfo+psFile->nSegmentCount;
        
        psInfo->nDLVL = -1;
        psInfo->nALVL = -1;
        psInfo->nLOC_R = -1;
        psInfo->nLOC_C = -1;
        psInfo->nCCS_R = -1;
        psInfo->nCCS_C = -1;

        psInfo->hAccess = NULL;
        strcpy( psInfo->szSegmentType, szType );
        
        psInfo->nSegmentHeaderSize = 
            atoi(NITFGetField(szTemp, psFile->pachHeader, 
                              nOffset + 3 + iSegment * (nHeaderLenSize+nDataLenSize), 
                              nHeaderLenSize));
        if (strchr(szTemp, '-') != NULL) /* Avoid negative values being mapped to huge unsigned values */
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Invalid segment header size : %s", szTemp);
            return -1;
        }

        if (strcmp(szType, "DE") == 0 && psInfo->nSegmentHeaderSize == 207)
        {
            /* DMAAC A.TOC files have a wrong header size. It says 207 but it is 209 really */
            psInfo->nSegmentHeaderSize = 209;
        }

        psInfo->nSegmentSize = 
            CPLScanUIntBig(NITFGetField(szTemp,psFile->pachHeader, 
                              nOffset + 3 + iSegment * (nHeaderLenSize+nDataLenSize) 
                              + nHeaderLenSize,
                              nDataLenSize), nDataLenSize);
        if (strchr(szTemp, '-') != NULL) /* Avoid negative values being mapped to huge unsigned values */
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Invalid segment size : %s", szTemp);
            return -1;
        }

        psInfo->nSegmentHeaderStart = *pnNextData;
        psInfo->nSegmentStart = *pnNextData + psInfo->nSegmentHeaderSize;

        *pnNextData += (psInfo->nSegmentHeaderSize+psInfo->nSegmentSize);
        psFile->nSegmentCount++;
    }

    return nOffset + nSegDefSize + 3;
}

/************************************************************************/
/*                            NITFGetField()                            */
/*                                                                      */
/*      Copy a field from a passed in header buffer into a temporary    */
/*      buffer and zero terminate it.                                   */
/************************************************************************/

char *NITFGetField( char *pszTarget, const char *pszSource, 
                    int nStart, int nLength )

{
    memcpy( pszTarget, pszSource + nStart, nLength );
    pszTarget[nLength] = '\0';

    return pszTarget;
}

/************************************************************************/
/*                            NITFFindTRE()                             */
/************************************************************************/

const char *NITFFindTRE( const char *pszTREData, int nTREBytes,
                         const char *pszTag, int *pnFoundTRESize )

{
    char szTemp[100];

    while( nTREBytes >= 11 )
    {
        int nThisTRESize = atoi(NITFGetField(szTemp, pszTREData, 6, 5 ));
        if (nThisTRESize < 0)
        {
            NITFGetField(szTemp, pszTREData, 0, 6 );
            CPLError(CE_Failure, CPLE_AppDefined, "Invalid size (%d) for TRE %s",
                     nThisTRESize, szTemp);
            return NULL;
        }
        if (nTREBytes - 11 < nThisTRESize)
        {
            NITFGetField(szTemp, pszTREData, 0, 6 );
            if (EQUALN(szTemp, "RPFIMG",6))
            {
                /* See #3848 */
                CPLDebug("NITF", "Adjusting RPFIMG TRE size from %d to %d, which is the remaining size", nThisTRESize, nTREBytes - 11);
                nThisTRESize = nTREBytes - 11;
            }
            else
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                        "Cannot read %s TRE. Not enough bytes : remaining %d, expected %d",
                        szTemp, nTREBytes - 11, nThisTRESize);
                return NULL;
            }
        }

        if( EQUALN(pszTREData,pszTag,6) )
        {
            if( pnFoundTRESize != NULL )
                *pnFoundTRESize = nThisTRESize;

            return pszTREData + 11;
        }

        nTREBytes -= (nThisTRESize + 11);
        pszTREData += (nThisTRESize + 11);
    }

    return NULL;
}

/************************************************************************/
/*                     NITFFindTREByIndex()                             */
/************************************************************************/

const char *NITFFindTREByIndex( const char *pszTREData, int nTREBytes,
                                const char *pszTag, int nTreIndex,
                                int *pnFoundTRESize )

{
    char szTemp[100];

    while( nTREBytes >= 11 )
    {
        int nThisTRESize = atoi(NITFGetField(szTemp, pszTREData, 6, 5 ));
        if (nThisTRESize < 0)
        {
            NITFGetField(szTemp, pszTREData, 0, 6 );
            CPLError(CE_Failure, CPLE_AppDefined, "Invalid size (%d) for TRE %s",
                     nThisTRESize, szTemp);
            return NULL;
        }
        if (nTREBytes - 11 < nThisTRESize)
        {
            NITFGetField(szTemp, pszTREData, 0, 6 );
            if (EQUALN(szTemp, "RPFIMG",6))
            {
                /* See #3848 */
                CPLDebug("NITF", "Adjusting RPFIMG TRE size from %d to %d, which is the remaining size", nThisTRESize, nTREBytes - 11);
                nThisTRESize = nTREBytes - 11;
            }
            else
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                        "Cannot read %s TRE. Not enough bytes : remaining %d, expected %d",
                        szTemp, nTREBytes - 11, nThisTRESize);
                return NULL;
            }
        }

        if( EQUALN(pszTREData,pszTag,6) )
        {
            if ( nTreIndex <= 0)
            {
                if( pnFoundTRESize != NULL )
                    *pnFoundTRESize = nThisTRESize;

                return pszTREData + 11;
            }

            /* Found a prevoius one - skip it ... */
            nTreIndex--;
        }

        nTREBytes -= (nThisTRESize + 11);
        pszTREData += (nThisTRESize + 11);
    }

    return NULL;
}

/************************************************************************/
/*                        NITFExtractMetadata()                         */
/************************************************************************/

void NITFExtractMetadata( char ***ppapszMetadata, const char *pachHeader,
                          int nStart, int nLength, const char *pszName )

{
    char szWork[400];
    char* pszWork;

    if (nLength >= sizeof(szWork) - 1)
        pszWork = (char*)CPLMalloc(nLength + 1);
    else
        pszWork = szWork;

    /* trim white space */
    while( nLength > 0 && pachHeader[nStart + nLength - 1] == ' ' )
        nLength--;

    memcpy( pszWork, pachHeader + nStart, nLength );
    pszWork[nLength] = '\0';

    *ppapszMetadata = CSLSetNameValue( *ppapszMetadata, pszName, pszWork );

    if (szWork != pszWork)
        CPLFree(pszWork);
}
                          
/************************************************************************/
/*        NITF_WGS84_Geocentric_Latitude_To_Geodetic_Latitude()         */
/*                                                                      */
/*      The input is a geocentric latitude in degrees.  The output      */
/*      is a geodetic latitude in degrees.                              */
/************************************************************************/

/*
 * "The angle L' is called "geocentric latitude" and is defined as the
 * angle between the equatorial plane and the radius from the geocenter.
 * 
 * The angle L is called "geodetic latitude" and is defined as the angle
 * between the equatorial plane and the normal to the surface of the
 * ellipsoid.  The word "latitude" usually means geodetic latitude.  This
 * is the basis for most of the maps and charts we use.  The normal to the
 * surface is the direction that a plumb bob would hang were it not for
 * local anomalies in the earth's gravitational field."
 */

double NITF_WGS84_Geocentric_Latitude_To_Geodetic_Latitude( double dfLat )

{
    /* WGS84 Ellipsoid */
    double a = 6378137.0;
    double b = 6356752.3142;
    double dfPI = 3.14159265358979323;

    /* convert to radians */
    dfLat = dfLat * dfPI / 180.0;

    /* convert to geodetic */
    dfLat = atan( ((a*a)/(b*b)) * tan(dfLat) );

    /* convert back to degrees */
    dfLat = dfLat * 180.0 / dfPI;

    return dfLat;
}


/************************************************************************/
/*                        NITFGetSeriesInfo()                           */
/************************************************************************/

static const NITFSeries nitfSeries[] =
{
    { "GN", "GNC", "1:5M", "Global Navigation Chart", "CADRG"},
    { "JN", "JNC", "1:2M", "Jet Navigation Chart", "CADRG"},
    { "OH", "VHRC", "1:1M", "VFR Helicopter Route Chart", "CADRG"},
    { "ON", "ONC", "1:1M", "Operational Navigation Chart", "CADRG"},
    { "OW", "WAC", "1:1M", "High Flying Chart - Host Nation", "CADRG"},
    { "TP", "TPC", "1:500K", "Tactical Pilotage Chart", "CADRG"},
    { "LF", "LFC-FR (Day)", "1:500K", "Low Flying Chart (Day) - Host Nation", "CADRG"},
    { "L1", "LFC-1", "1:500K", "Low Flying Chart (TBD #1)", "CADRG"},
    { "L2", "LFC-2", "1:500K", "Low Flying Chart (TBD #2)", "CADRG"},
    { "L3", "LFC-3", "1:500K", "Low Flying Chart (TBD #3)", "CADRG"},
    { "L4", "LFC-4", "1:500K", "Low Flying Chart (TBD #4)", "CADRG"},
    { "L5", "LFC-5", "1:500K", "Low Flying Chart (TBD #5)", "CADRG"},
    { "LN", "LN (Night)", "1:500K", "Low Flying Chart (Night) - Host Nation", "CADRG"},
    { "JG", "JOG", "1:250K", "Joint Operation Graphic", "CADRG"},
    { "JA", "JOG-A", "1:250K", "Joint Operation Graphic - Air", "CADRG"},
    { "JR", "JOG-R", "1:250K", "Joint Operation Graphic - Radar", "CADRG"},
    { "JO", "OPG", "1:250K", "Operational Planning Graphic", "CADRG"},
    { "VT", "VTAC", "1:250K", "VFR Terminal Area Chart", "CADRG"},
    { "F1", "TFC-1", "1:250K", "Transit Flying Chart (TBD #1)", "CADRG"},
    { "F2", "TFC-2", "1:250K", "Transit Flying Chart (TBD #2)", "CADRG"},
    { "F3", "TFC-3", "1:250K", "Transit Flying Chart (TBD #3)", "CADRG"},
    { "F4", "TFC-4", "1:250K", "Transit Flying Chart (TBD #4)", "CADRG"},
    { "F5", "TFC-5", "1:250K", "Transit Flying Chart (TBD #5)", "CADRG"},
    { "TF", "TFC", "1:250K", "Transit Flying Chart (UK)", "CADRG"}, /* Not mentionned in 24111CN1.pdf paragraph 5.1.4 */
    { "AT", "ATC", "1:200K", "Series 200 Air Target Chart", "CADRG"},
    { "VH", "HRC", "1:125K", "Helicopter Route Chart", "CADRG"},
    { "TN", "TFC (Night)", "1:250K", "Transit Flying Charget (Night) - Host Nation", "CADRG"},
    { "TR", "TLM 200", "1:200K", "Topographic Line Map 1:200,000 scale", "CADRG"},
    { "TC", "TLM 100", "1:100K", "Topographic Line Map 1:100,000 scale", "CADRG"},
    { "RV", "Riverine", "1:50K", "Riverine Map 1:50,000 scale", "CADRG"},
    { "TL", "TLM 50", "1:50K", "Topographic Line Map 1:50,000 scale", "CADRG"},
    { "UL", "TLM 50 - Other", "1:50K", "Topographic Line Map (other 1:50,000 scale)", "CADRG"},
    { "TT", "TLM 25", "1:25K", "Topographic Line Map 1:25,000 scale", "CADRG"},
    { "TQ", "TLM 24", "1:24K", "Topographic Line Map 1:24,000 scale", "CADRG"},
    { "HA", "HA", "Various", "Harbor and Approach Charts", "CADRG"},
    { "CO", "CO", "Various", "Coastal Charts", "CADRG"},
    { "OA", "OPAREA", "Various", "Naval Range Operation Area Chart", "CADRG"},
    { "CG", "CG", "Various", "City Graphics", "CADRG"},
    { "C1", "CG", "1:10000", "City Graphics", "CADRG"},
    { "C2", "CG", "1:10560", "City Graphics", "CADRG"},
    { "C3", "CG", "1:11000", "City Graphics", "CADRG"},
    { "C4", "CG", "1:11800", "City Graphics", "CADRG"},
    { "C5", "CG", "1:12000", "City Graphics", "CADRG"},
    { "C6", "CG", "1:12500", "City Graphics", "CADRG"},
    { "C7", "CG", "1:12800", "City Graphics", "CADRG"},
    { "C8", "CG", "1:14000", "City Graphics", "CADRG"},
    { "C9", "CG", "1:14700", "City Graphics", "CADRG"},
    { "CA", "CG", "1:15000", "City Graphics", "CADRG"},
    { "CB", "CG", "1:15500", "City Graphics", "CADRG"},
    { "CC", "CG", "1:16000", "City Graphics", "CADRG"},
    { "CD", "CG", "1:16666", "City Graphics", "CADRG"},
    { "CE", "CG", "1:17000", "City Graphics", "CADRG"},
    { "CF", "CG", "1:17500", "City Graphics", "CADRG"},
    { "CH", "CG", "1:18000", "City Graphics", "CADRG"},
    { "CJ", "CG", "1:20000", "City Graphics", "CADRG"},
    { "CK", "CG", "1:21000", "City Graphics", "CADRG"},
    { "CL", "CG", "1:21120", "City Graphics", "CADRG"},
    { "CN", "CG", "1:22000", "City Graphics", "CADRG"},
    { "CP", "CG", "1:23000", "City Graphics", "CADRG"},
    { "CQ", "CG", "1:25000", "City Graphics", "CADRG"},
    { "CR", "CG", "1:26000", "City Graphics", "CADRG"},
    { "CS", "CG", "1:35000", "City Graphics", "CADRG"},
    { "CT", "CG", "1:36000", "City Graphics", "CADRG"},
    { "CM", "CM", "Various", "Combat Charts", "CADRG"},
    { "A1", "CM", "1:10K", "Combat Charts (1:10K)", "CADRG"},
    { "A2", "CM", "1:25K", "Combat Charts (1:25K)", "CADRG"},
    { "A3", "CM", "1:50K", "Combat Charts (1:50K)", "CADRG"},
    { "A4", "CM", "1:100K", "Combat Charts (1:100K)", "CADRG"},
    { "MI", "MIM", "1:50K", "Military Installation Maps", "CADRG"},
    { "M1", "MIM", "Various", "Military Installation Maps (TBD #1)", "CADRG"},
    { "M2", "MIM", "Various", "Military Installation Maps (TBD #2)", "CADRG"},
    { "VN", "VNC", "1:500K", "Visual Navigation Charts", "CADRG"},
    { "MM", "", "Various", "(Miscellaneous Maps & Charts)", "CADRG"},
    
    { "I1", "", "10m", "Imagery, 10 meter resolution", "CIB"},
    { "I2", "", "5m", "Imagery, 5 meter resolution", "CIB"},
    { "I3", "", "2m", "Imagery, 2 meter resolution", "CIB"},
    { "I4", "", "1m", "Imagery, 1 meter resolution", "CIB"},
    { "I5", "", ".5m", "Imagery, .5 (half) meter resolution", "CIB"},
    { "IV", "", "Various > 10m", "Imagery, greater than 10 meter resolution", "CIB"},
    
    { "D1", "", "100m", "Elevation Data from DTED level 1", "CDTED"},
    { "D2", "", "30m", "Elevation Data from DTED level 2", "CDTED"},
};

/* See 24111CN1.pdf paragraph 5.1.4 */
const NITFSeries* NITFGetSeriesInfo(const char* pszFilename)
{
    int i;
    char seriesCode[3] = {0,0,0};
    if (pszFilename == NULL) return NULL;
    for (i=strlen(pszFilename)-1;i>=0;i--)
    {
        if (pszFilename[i] == '.')
        {
            if (i < (int)strlen(pszFilename) - 3)
            {
                seriesCode[0] = pszFilename[i+1];
                seriesCode[1] = pszFilename[i+2];
                for(i=0;i<sizeof(nitfSeries) / sizeof(nitfSeries[0]); i++)
                {
                    if (EQUAL(seriesCode, nitfSeries[i].code))
                    {
                        return &nitfSeries[i];
                    }
                }
                return NULL;
            }
        }
    }
    return NULL;
}

/************************************************************************/
/*                       NITFCollectAttachments()                       */
/*                                                                      */
/*      Collect attachment, display level and location info into the    */
/*      segmentinfo structures.                                         */
/************************************************************************/

int NITFCollectAttachments( NITFFile *psFile )

{
    int iSegment;

/* ==================================================================== */
/*      Loop over all segments.                                         */
/* ==================================================================== */
    for( iSegment = 0; iSegment < psFile->nSegmentCount; iSegment++ )
    {
        NITFSegmentInfo *psSegInfo = psFile->pasSegmentInfo + iSegment;

/* -------------------------------------------------------------------- */
/*      For image segments, we use the normal image access stuff.       */
/* -------------------------------------------------------------------- */
        if( EQUAL(psSegInfo->szSegmentType,"IM") )
        {
            NITFImage *psImage = NITFImageAccess( psFile, iSegment );
            if (psImage == NULL)
                return FALSE;
                
            psSegInfo->nDLVL = psImage->nIDLVL;
            psSegInfo->nALVL = psImage->nIALVL;
            psSegInfo->nLOC_R = psImage->nILOCRow;
            psSegInfo->nLOC_C = psImage->nILOCColumn;
        }
/* -------------------------------------------------------------------- */
/*      For graphic file we need to process the header.                 */
/* -------------------------------------------------------------------- */
        else if( EQUAL(psSegInfo->szSegmentType,"SY")
                 || EQUAL(psSegInfo->szSegmentType,"GR") )
        {
            char achSubheader[298];
            int  nSTYPEOffset;
            char szTemp[100];

/* -------------------------------------------------------------------- */
/*      Load the graphic subheader.                                     */
/* -------------------------------------------------------------------- */
            if( VSIFSeekL( psFile->fp, psSegInfo->nSegmentHeaderStart, 
                           SEEK_SET ) != 0 
                || VSIFReadL( achSubheader, 1, sizeof(achSubheader), 
                              psFile->fp ) < 258 )
            {
                CPLError( CE_Warning, CPLE_FileIO, 
                          "Failed to read graphic subheader at " CPL_FRMT_GUIB ".", 
                          psSegInfo->nSegmentHeaderStart );
                continue;
            }

            // NITF 2.0. (also works for NITF 2.1)
            nSTYPEOffset = 200;
            if( EQUALN(achSubheader+193,"999998",6) )
                nSTYPEOffset += 40;

/* -------------------------------------------------------------------- */
/*      Report some standard info.                                      */
/* -------------------------------------------------------------------- */
            psSegInfo->nDLVL = atoi(NITFGetField(szTemp,achSubheader,
                                                 nSTYPEOffset + 14, 3));
            psSegInfo->nALVL = atoi(NITFGetField(szTemp,achSubheader,
                                                 nSTYPEOffset + 17, 3));
            psSegInfo->nLOC_R = atoi(NITFGetField(szTemp,achSubheader,
                                                  nSTYPEOffset + 20, 5));
            psSegInfo->nLOC_C = atoi(NITFGetField(szTemp,achSubheader,
                                                  nSTYPEOffset + 25, 5));
        }
    }
    
    return TRUE;
}

/************************************************************************/
/*                      NITFReconcileAttachments()                      */
/*                                                                      */
/*      Generate the CCS location information for all the segments      */
/*      if possible.                                                    */
/************************************************************************/

int NITFReconcileAttachments( NITFFile *psFile )

{
    int iSegment;
    int bSuccess = TRUE;
    int bMadeProgress = FALSE;

    for( iSegment = 0; iSegment < psFile->nSegmentCount; iSegment++ )
    {
        NITFSegmentInfo *psSegInfo = psFile->pasSegmentInfo + iSegment;
        int iOther;

        // already processed?
        if( psSegInfo->nCCS_R != -1 )
            continue;

        // unattached segments are straight forward.
        if( psSegInfo->nALVL < 1 )
        {
            psSegInfo->nCCS_R = psSegInfo->nLOC_R;
            psSegInfo->nCCS_C = psSegInfo->nLOC_C;
            if( psSegInfo->nCCS_R != -1 )
                bMadeProgress = TRUE;
            continue;
        }

        // Loc for segment to which we are attached.
        for( iOther = 0; iOther < psFile->nSegmentCount; iOther++ )
        {
            NITFSegmentInfo *psOtherSegInfo = psFile->pasSegmentInfo + iOther;
            
            if( psSegInfo->nALVL == psOtherSegInfo->nDLVL )
            {
                if( psOtherSegInfo->nCCS_R != -1 )
                {
                    psSegInfo->nCCS_R = psOtherSegInfo->nLOC_R + psSegInfo->nLOC_R;
                    psSegInfo->nCCS_C = psOtherSegInfo->nLOC_C + psSegInfo->nLOC_C;
                    if ( psSegInfo->nCCS_R != -1 )
                        bMadeProgress = TRUE;
                }
                else
                {
                    bSuccess = FALSE;
                }
                break;
            }
        }

        if( iOther == psFile->nSegmentCount )
            bSuccess = FALSE;
    }

/* -------------------------------------------------------------------- */
/*      If succeeded or made no progress then return our success        */
/*      flag.  Otherwise make another pass, hopefully filling in        */
/*      more values.                                                    */
/* -------------------------------------------------------------------- */
    if( bSuccess || !bMadeProgress )
        return bSuccess;
    else
        return NITFReconcileAttachments( psFile );
}

/************************************************************************/
/*                        NITFFindValFromEnd()                          */
/************************************************************************/

static const char* NITFFindValFromEnd(char** papszMD,
                                      int nMDSize,
                                      const char* pszVar,
                                      const char* pszDefault)
{
    int nVarLen = strlen(pszVar);
    int nIter = nMDSize-1;
    for(;nIter >= 0;nIter--)
    {
        if (strncmp(papszMD[nIter], pszVar, nVarLen) == 0 &&
            papszMD[nIter][nVarLen] == '=')
            return papszMD[nIter] + nVarLen + 1;
    }
    return NULL;
}

/************************************************************************/
/*                  NITFGenericMetadataReadTREInternal()                */
/************************************************************************/

static char** NITFGenericMetadataReadTREInternal(char **papszMD,
                                                 int* pnMDSize,
                                                 int* pnMDAlloc,
                                                 CPLXMLNode* psOutXMLNode,
                                                 const char* pszTREName,
                                                 const char *pachTRE,
                                                 int nTRESize,
                                                 CPLXMLNode* psTreNode,
                                                 int *pnTreOffset,
                                                 const char* pszMDPrefix,
                                                 int *pbError)
{
    CPLXMLNode* psIter;
    for(psIter = psTreNode->psChild;
        psIter != NULL && *pbError == FALSE;
        psIter = psIter->psNext)
    {
        if (psIter->eType == CXT_Element &&
            psIter->pszValue != NULL &&
            strcmp(psIter->pszValue, "field") == 0)
        {
            const char* pszName = CPLGetXMLValue(psIter, "name", NULL);
            const char* pszLongName = CPLGetXMLValue(psIter, "longname", NULL);
            const char* pszLength = CPLGetXMLValue(psIter, "length", NULL);
            int nLength = -1;
            if (pszLength != NULL)
                nLength = atoi(pszLength);
            else
            {
                const char* pszLengthVar = CPLGetXMLValue(psIter, "length_var", NULL);
                if (pszLengthVar != NULL)
                {
                    char** papszMDIter = papszMD;
                    while(papszMDIter != NULL && *papszMDIter != NULL)
                    {
                        if (strstr(*papszMDIter, pszLengthVar) != NULL)
                        {
                            const char* pszEqual = strchr(*papszMDIter, '=');
                            if (pszEqual != NULL)
                            {
                                nLength = atoi(pszEqual + 1);
                                break;
                            }
                        }
                        papszMDIter ++;
                    }
                }
            }
            if (pszName != NULL && nLength > 0)
            {
                char* pszMDItemName;
                char** papszTmp = NULL;

                if (*pnTreOffset + nLength > nTRESize)
                {
                    *pbError = TRUE;
                    CPLError( CE_Warning, CPLE_AppDefined,
                              "Not enough bytes when reading %s TRE "
                              "(at least %d needed, only %d available)",
                              pszTREName, *pnTreOffset + nLength, nTRESize );
                    break;
                }

                pszMDItemName = CPLStrdup(
                            CPLSPrintf("%s%s", pszMDPrefix, pszName));

                NITFExtractMetadata( &papszTmp, pachTRE, *pnTreOffset,
                                     nLength, pszMDItemName );
                if (*pnMDSize + 1 >= *pnMDAlloc)
                {
                    *pnMDAlloc = (*pnMDAlloc * 4 / 3) + 32;
                    papszMD = (char**)CPLRealloc(papszMD, *pnMDAlloc * sizeof(char**));
                }
                papszMD[*pnMDSize] = papszTmp[0];
                papszMD[(*pnMDSize) + 1] = NULL;
                (*pnMDSize) ++;
                papszTmp[0] = NULL;
                CPLFree(papszTmp);

                if (psOutXMLNode != NULL)
                {
                    const char* pszVal = strchr(papszMD[(*pnMDSize) - 1], '=') + 1;
                    CPLXMLNode* psFieldNode;
                    CPLXMLNode* psNameNode;
                    CPLXMLNode* psValueNode;

                    CPLAssert(pszVal != NULL);
                    psFieldNode =
                        CPLCreateXMLNode(psOutXMLNode, CXT_Element, "field");
                    psNameNode =
                        CPLCreateXMLNode(psFieldNode, CXT_Attribute, "name");
                    psValueNode =
                        CPLCreateXMLNode(psFieldNode, CXT_Attribute, "value");
                    CPLCreateXMLNode(psNameNode, CXT_Text,
                       (pszName[0] || pszLongName == NULL) ? pszName : pszLongName);
                    CPLCreateXMLNode(psValueNode, CXT_Text, pszVal);
                }

                CPLFree(pszMDItemName);

                *pnTreOffset += nLength;
            }
            else if (nLength > 0)
            {
                *pnTreOffset += nLength;
            }
            else
            {
                *pbError = TRUE;
                CPLError( CE_Warning, CPLE_AppDefined,
                          "Invalid item construct in %s TRE in XML ressource",
                          pszTREName );
                break;
            }
        }
        else if (psIter->eType == CXT_Element &&
                 psIter->pszValue != NULL &&
                 strcmp(psIter->pszValue, "loop") == 0)
        {
            const char* pszCounter = CPLGetXMLValue(psIter, "counter", NULL);
            const char* pszIterations = CPLGetXMLValue(psIter, "iterations", NULL);
            const char* pszFormula = CPLGetXMLValue(psIter, "formula", NULL);
            const char* pszMDSubPrefix = CPLGetXMLValue(psIter, "md_prefix", NULL);
            int nIterations = -1;

            if (pszCounter != NULL)
            {
                char* pszMDItemName = CPLStrdup(
                            CPLSPrintf("%s%s", pszMDPrefix, pszCounter));
                nIterations = atoi(NITFFindValFromEnd(papszMD, *pnMDSize, pszMDItemName, "-1"));
                CPLFree(pszMDItemName);
                if (nIterations < 0)
                {
                    CPLError( CE_Warning, CPLE_AppDefined,
                            "Invalid loop construct in %s TRE in XML ressource : "
                            "invalid 'counter' %s",
                            pszTREName, pszCounter );
                    *pbError = TRUE;
                    break;
                }
            }
            else if (pszIterations != NULL)
            {
                nIterations = atoi(pszIterations);
            }
            else if (pszFormula != NULL &&
                     strcmp(pszFormula, "(NPART+1)*(NPART)/2") == 0)
            {
                char* pszMDItemName = CPLStrdup(
                            CPLSPrintf("%s%s", pszMDPrefix, "NPART"));
                int NPART = atoi(NITFFindValFromEnd(papszMD, *pnMDSize, pszMDItemName, "-1"));
                CPLFree(pszMDItemName);
                if (NPART < 0)
                {
                    CPLError( CE_Warning, CPLE_AppDefined,
                            "Invalid loop construct in %s TRE in XML ressource : "
                            "invalid 'counter' %s",
                            pszTREName, "NPART" );
                    *pbError = TRUE;
                    break;
                }
                nIterations = NPART * (NPART + 1) / 2;
            }
            else if (pszFormula != NULL &&
                     strcmp(pszFormula, "(NUMOPG+1)*(NUMOPG)/2") == 0)
            {
                char* pszMDItemName = CPLStrdup(
                            CPLSPrintf("%s%s", pszMDPrefix, "NUMOPG"));
                int NUMOPG = atoi(NITFFindValFromEnd(papszMD, *pnMDSize, pszMDItemName, "-1"));
                CPLFree(pszMDItemName);
                if (NUMOPG < 0)
                {
                    CPLError( CE_Warning, CPLE_AppDefined,
                            "Invalid loop construct in %s TRE in XML ressource : "
                            "invalid 'counter' %s",
                            pszTREName, "NUMOPG" );
                    *pbError = TRUE;
                    break;
                }
                nIterations = NUMOPG * (NUMOPG + 1) / 2;
            }
            else if (pszFormula != NULL &&
                     strcmp(pszFormula, "NPAR*NPARO") == 0)
            {
                char* pszMDNPARName = CPLStrdup(
                            CPLSPrintf("%s%s", pszMDPrefix, "NPAR"));
                int NPAR = atoi(NITFFindValFromEnd(papszMD, *pnMDSize, pszMDNPARName, "-1"));
                char* pszMDNPAROName = CPLStrdup(
                            CPLSPrintf("%s%s", pszMDPrefix, "NPARO"));
                int NPARO= atoi(NITFFindValFromEnd(papszMD, *pnMDSize, pszMDNPAROName, "-1"));
                CPLFree(pszMDNPARName);
                CPLFree(pszMDNPAROName);
                if (NPAR < 0)
                {
                    CPLError( CE_Warning, CPLE_AppDefined,
                            "Invalid loop construct in %s TRE in XML ressource : "
                            "invalid 'counter' %s",
                            pszTREName, "NPAR" );
                    *pbError = TRUE;
                    break;
                }
                if (NPARO < 0)
                {
                    CPLError( CE_Warning, CPLE_AppDefined,
                            "Invalid loop construct in %s TRE in XML ressource : "
                            "invalid 'counter' %s",
                            pszTREName, "NPAR0" );
                    *pbError = TRUE;
                    break;
                }
                nIterations = NPAR*NPARO;
            }
            else if (pszFormula != NULL &&
                     strcmp(pszFormula, "NPLN-1") == 0)
            {
                char* pszMDItemName = CPLStrdup(
                            CPLSPrintf("%s%s", pszMDPrefix, "NPLN"));
                int NPLN = atoi(NITFFindValFromEnd(papszMD, *pnMDSize, pszMDItemName, "-1"));
                CPLFree(pszMDItemName);
                if (NPLN < 0)
                {
                    CPLError( CE_Warning, CPLE_AppDefined,
                            "Invalid loop construct in %s TRE in XML ressource : "
                            "invalid 'counter' %s",
                            pszTREName, "NPLN" );
                    *pbError = TRUE;
                    break;
                }
                nIterations = NPLN-1;
            }
            else if (pszFormula != NULL &&
                     strcmp(pszFormula, "NXPTS*NYPTS") == 0)
            {
                char* pszMDNPARName = CPLStrdup(
                            CPLSPrintf("%s%s", pszMDPrefix, "NXPTS"));
                int NXPTS = atoi(NITFFindValFromEnd(papszMD, *pnMDSize, pszMDNPARName, "-1"));
                char* pszMDNPAROName = CPLStrdup(
                            CPLSPrintf("%s%s", pszMDPrefix, "NYPTS"));
                int NYPTS= atoi(NITFFindValFromEnd(papszMD, *pnMDSize, pszMDNPAROName, "-1"));
                CPLFree(pszMDNPARName);
                CPLFree(pszMDNPAROName);
                if (NXPTS < 0)
                {
                    CPLError( CE_Warning, CPLE_AppDefined,
                            "Invalid loop construct in %s TRE in XML ressource : "
                            "invalid 'counter' %s",
                            pszTREName, "NXPTS" );
                    *pbError = TRUE;
                    break;
                }
                if (NYPTS < 0)
                {
                    CPLError( CE_Warning, CPLE_AppDefined,
                            "Invalid loop construct in %s TRE in XML ressource : "
                            "invalid 'counter' %s",
                            pszTREName, "NYPTS" );
                    *pbError = TRUE;
                    break;
                }
                nIterations = NXPTS*NYPTS;
            }
            else
            {
                CPLError( CE_Warning, CPLE_AppDefined,
                          "Invalid loop construct in %s TRE in XML ressource : "
                          "missing or invalid 'counter' or 'iterations' or 'formula'",
                          pszTREName );
                *pbError = TRUE;
                break;
            }

            if (nIterations > 0)
            {
                int iIter;
                const char* pszPercent;
                int bHasValidPercentD = FALSE;
                CPLXMLNode* psRepeatedNode = NULL;
                CPLXMLNode* psLastChild = NULL;

                /* Check that md_prefix has one and only %XXXXd pattern */
                if (pszMDSubPrefix != NULL &&
                    (pszPercent = strchr(pszMDSubPrefix, '%')) != NULL &&
                    strchr(pszPercent+1,'%') == NULL)
                {
                    const char* pszIter = pszPercent + 1;
                    while(*pszIter != '\0')
                    {
                        if (*pszIter >= '0' && *pszIter <= '9')
                            pszIter ++;
                        else if (*pszIter == 'd')
                        {
                            bHasValidPercentD = atoi(pszPercent + 1) <= 10;
                            break;
                        }
                        else
                            break;
                    }
                }

                if (psOutXMLNode != NULL)
                {
                    CPLXMLNode* psNumberNode;
                    CPLXMLNode* psNameNode;
                    const char* pszName = CPLGetXMLValue(psIter, "name", NULL);
                    psRepeatedNode = CPLCreateXMLNode(psOutXMLNode, CXT_Element, "repeated");
                    if (pszName)
                    {
                        psNameNode = CPLCreateXMLNode(psRepeatedNode, CXT_Attribute, "name");
                        CPLCreateXMLNode(psNameNode, CXT_Text, pszName);
                    }
                    psNumberNode = CPLCreateXMLNode(psRepeatedNode, CXT_Attribute, "number");
                    CPLCreateXMLNode(psNumberNode, CXT_Text, CPLSPrintf("%d", nIterations));

                    psLastChild = psRepeatedNode->psChild;
                    while(psLastChild->psNext != NULL)
                        psLastChild = psLastChild->psNext;
                }

                for(iIter = 0; iIter < nIterations && *pbError == FALSE; iIter++)
                {
                    char* pszMDNewPrefix = NULL;
                    CPLXMLNode* psGroupNode = NULL;
                    if (pszMDSubPrefix != NULL)
                    {
                        if (bHasValidPercentD)
                        {
                            char* szTmp = (char*)CPLMalloc(
                                            strlen(pszMDSubPrefix) + 10 + 1);
                            sprintf(szTmp, pszMDSubPrefix, iIter + 1);
                            pszMDNewPrefix = CPLStrdup(CPLSPrintf("%s%s",
                                                       pszMDPrefix, szTmp));
                            CPLFree(szTmp);
                        }
                        else
                            pszMDNewPrefix = CPLStrdup(CPLSPrintf("%s%s%04d_",
                                      pszMDPrefix, pszMDSubPrefix, iIter + 1));
                    }
                    else
                        pszMDNewPrefix = CPLStrdup(CPLSPrintf("%s%04d_",
                                                   pszMDPrefix, iIter + 1));

                    if (psRepeatedNode != NULL)
                    {
                        CPLXMLNode* psIndexNode;
                        psGroupNode = CPLCreateXMLNode(NULL, CXT_Element, "group");
                        CPLAssert(psLastChild->psNext == NULL);
                        psLastChild->psNext = psGroupNode;
                        psLastChild = psGroupNode;
                        psIndexNode = CPLCreateXMLNode(psGroupNode, CXT_Attribute, "index");
                        CPLCreateXMLNode(psIndexNode, CXT_Text, CPLSPrintf("%d", iIter));
                    }

                    papszMD = NITFGenericMetadataReadTREInternal(papszMD,
                                                                 pnMDSize,
                                                                 pnMDAlloc,
                                                                 psGroupNode,
                                                                 pszTREName,
                                                                 pachTRE,
                                                                 nTRESize,
                                                                 psIter,
                                                                 pnTreOffset,
                                                                 pszMDNewPrefix,
                                                                 pbError);
                    CPLFree(pszMDNewPrefix);
                }
            }
        }
        else if (psIter->eType == CXT_Element &&
                 psIter->pszValue != NULL &&
                 strcmp(psIter->pszValue, "if") == 0)
        {
            const char* pszCond = CPLGetXMLValue(psIter, "cond", NULL);
            const char* pszEqual = NULL;
            if (pszCond != NULL && strcmp(pszCond, "QSS!=U AND QOD!=Y") == 0)
            {
                char* pszQSSName = CPLStrdup(
                            CPLSPrintf("%s%s", pszMDPrefix, "QSS"));
                char* pszQODName = CPLStrdup(
                            CPLSPrintf("%s%s", pszMDPrefix, "QOD"));
                const char* pszQSSVal = NITFFindValFromEnd(papszMD, *pnMDSize, pszQSSName, NULL);
                const char* pszQODVal = NITFFindValFromEnd(papszMD, *pnMDSize, pszQODName, NULL);
                if (pszQSSVal == NULL)
                {
                    CPLDebug("NITF", "Cannot find if cond variable %s", "QSS");
                }
                else if (pszQODVal == NULL)
                {
                    CPLDebug("NITF", "Cannot find if cond variable %s", "QOD");
                }
                else if (strcmp(pszQSSVal, "U") != 0 && strcmp(pszQODVal, "Y") != 0)
                {
                    papszMD = NITFGenericMetadataReadTREInternal(papszMD,
                                                                 pnMDSize,
                                                                 pnMDAlloc,
                                                                 psOutXMLNode,
                                                                 pszTREName,
                                                                 pachTRE,
                                                                 nTRESize,
                                                                 psIter,
                                                                 pnTreOffset,
                                                                 pszMDPrefix,
                                                                 pbError);
                }
                CPLFree(pszQSSName);
                CPLFree(pszQODName);
            }
            else if (pszCond != NULL && (pszEqual = strchr(pszCond, '=')) != NULL)
            {
                char* pszCondVar = (char*)CPLMalloc(pszEqual - pszCond + 1);
                const char* pszCondExpectedVal = pszEqual + 1;
                char* pszMDItemName;
                const char* pszCondVal;
                int bTestEqual = TRUE;
                memcpy(pszCondVar, pszCond, pszEqual - pszCond);
                if (pszEqual - pszCond > 1 && pszCondVar[pszEqual - pszCond - 1] == '!')
                {
                    bTestEqual = FALSE;
                    pszCondVar[pszEqual - pszCond - 1] = '\0';
                }
                pszCondVar[pszEqual - pszCond] = '\0';
                pszMDItemName = CPLStrdup(
                            CPLSPrintf("%s%s", pszMDPrefix, pszCondVar));
                pszCondVal = NITFFindValFromEnd(papszMD, *pnMDSize, pszMDItemName, NULL);
                if (pszCondVal == NULL)
                {
                    CPLDebug("NITF", "Cannot find if cond variable %s",
                             pszMDItemName);
                }
                else if ((bTestEqual && strcmp(pszCondVal, pszCondExpectedVal) == 0) ||
                         (!bTestEqual && strcmp(pszCondVal, pszCondExpectedVal) != 0))
                {
                    papszMD = NITFGenericMetadataReadTREInternal(papszMD,
                                                                 pnMDSize,
                                                                 pnMDAlloc,
                                                                 psOutXMLNode,
                                                                 pszTREName,
                                                                 pachTRE,
                                                                 nTRESize,
                                                                 psIter,
                                                                 pnTreOffset,
                                                                 pszMDPrefix,
                                                                 pbError);
                }
                CPLFree(pszMDItemName);
                CPLFree(pszCondVar);
            }
            else
            {
                CPLError( CE_Warning, CPLE_AppDefined,
                          "Invalid if construct in %s TRE in XML ressource : "
                          "missing or invalid 'cond' attribute",
                          pszTREName );
                *pbError = TRUE;
                break;
            }
        }
        else if (psIter->eType == CXT_Element &&
                 psIter->pszValue != NULL &&
                 strcmp(psIter->pszValue, "if_remaining_bytes") == 0)
        {
            if (*pnTreOffset < nTRESize)
            {
                papszMD = NITFGenericMetadataReadTREInternal(papszMD,
                                                             pnMDSize,
                                                             pnMDAlloc,
                                                             psOutXMLNode,
                                                             pszTREName,
                                                             pachTRE,
                                                             nTRESize,
                                                             psIter,
                                                             pnTreOffset,
                                                             pszMDPrefix,
                                                             pbError);
        }
        }
        else
        {
            //CPLDebug("NITF", "Unknown element : %s", psIter->pszValue ? psIter->pszValue : "null");
        }
    }
    return papszMD;
}

/************************************************************************/
/*                      NITFGenericMetadataReadTRE()                    */
/************************************************************************/

static
char **NITFGenericMetadataReadTRE(char **papszMD,
                                  const char* pszTREName,
                                  const char *pachTRE,
                                  int nTRESize,
                                  CPLXMLNode* psTreNode)
{
    int nTreLength, nTreMinLength = -1, nTreMaxLength = -1;
    int bError = FALSE;
    int nTreOffset = 0;
    const char* pszMDPrefix;
    int nMDSize, nMDAlloc;

    nTreLength = atoi(CPLGetXMLValue(psTreNode, "length", "-1"));
    nTreMinLength = atoi(CPLGetXMLValue(psTreNode, "minlength", "-1"));
    nTreMaxLength = atoi(CPLGetXMLValue(psTreNode, "maxlength", "-1"));

    if( (nTreLength > 0 && nTRESize != nTreLength) ||
        (nTreMinLength > 0 && nTRESize < nTreMinLength) )
    {
        CPLError( CE_Warning, CPLE_AppDefined,
                  "%s TRE wrong size, ignoring.", pszTREName );
        return papszMD;
    }

    pszMDPrefix = CPLGetXMLValue(psTreNode, "md_prefix", "");

    nMDSize = nMDAlloc = CSLCount(papszMD);

    papszMD = NITFGenericMetadataReadTREInternal(papszMD,
                                                 &nMDSize,
                                                 &nMDAlloc,
                                                 NULL,
                                                 pszTREName,
                                                 pachTRE,
                                                 nTRESize,
                                                 psTreNode,
                                                 &nTreOffset,
                                                 pszMDPrefix,
                                                 &bError);

    if (bError == FALSE && nTreLength > 0 && nTreOffset != nTreLength)
    {
        CPLError( CE_Warning, CPLE_AppDefined,
                  "Inconsistant declaration of %s TRE",
                  pszTREName );
    }
    if (nTreOffset < nTRESize)
        CPLDebug("NITF", "%d remaining bytes at end of %s TRE",
                 nTRESize -nTreOffset, pszTREName);

    return papszMD;
}


/************************************************************************/
/*                           NITFLoadXMLSpec()                          */
/************************************************************************/

#define NITF_SPEC_FILE "nitf_spec.xml"

static CPLXMLNode* NITFLoadXMLSpec(NITFFile* psFile)
{

    if (psFile->psNITFSpecNode == NULL)
    {
        const char* pszXMLDescFilename = CPLFindFile("gdal", NITF_SPEC_FILE);
        if (pszXMLDescFilename == NULL)
        {
            CPLDebug("NITF", "Cannot find XML file : %s", NITF_SPEC_FILE);
            return NULL;
        }
        psFile->psNITFSpecNode = CPLParseXMLFile(pszXMLDescFilename);
        if (psFile->psNITFSpecNode == NULL)
        {
            CPLDebug("NITF", "Invalid XML file : %s", pszXMLDescFilename);
            return NULL;
        }
    }

    return psFile->psNITFSpecNode;
}

/************************************************************************/
/*                      NITFFindTREXMLDescFromName()                    */
/************************************************************************/

static CPLXMLNode* NITFFindTREXMLDescFromName(NITFFile* psFile,
                                              const char* pszTREName)
{
    CPLXMLNode* psTreeNode;
    CPLXMLNode* psTresNode;
    CPLXMLNode* psIter;

    psTreeNode = NITFLoadXMLSpec(psFile);
    if (psTreeNode == NULL)
        return NULL;

    psTresNode = CPLGetXMLNode(psTreeNode, "=tres");
    if (psTresNode == NULL)
    {
        CPLDebug("NITF", "Cannot find <tres> root element");
        return NULL;
    }

    for(psIter = psTresNode->psChild;psIter != NULL;psIter = psIter->psNext)
    {
        if (psIter->eType == CXT_Element &&
            psIter->pszValue != NULL &&
            strcmp(psIter->pszValue, "tre") == 0)
        {
            const char* pszName = CPLGetXMLValue(psIter, "name", NULL);
            if (pszName != NULL && strcmp(pszName, pszTREName) == 0)
            {
                return psIter;
            }
        }
    }

    return NULL;
}

/************************************************************************/
/*                         NITFCreateXMLTre()                           */
/************************************************************************/

CPLXMLNode* NITFCreateXMLTre(NITFFile* psFile,
                             const char* pszTREName,
                             const char *pachTRE,
                             int nTRESize)
{
    int nTreLength, nTreMinLength = -1, nTreMaxLength = -1;
    int bError = FALSE;
    int nTreOffset = 0;
    CPLXMLNode* psTreNode;
    CPLXMLNode* psOutXMLNode = NULL;
    int nMDSize = 0, nMDAlloc = 0;

    psTreNode = NITFFindTREXMLDescFromName(psFile, pszTREName);
    if (psTreNode == NULL)
    {
        if (!(EQUALN(pszTREName, "RPF", 3) || strcmp(pszTREName, "XXXXXX") == 0))
        {
            CPLDebug("NITF", "Cannot find definition of TRE %s in %s",
                    pszTREName, NITF_SPEC_FILE);
        }
        return NULL;
    }

    nTreLength = atoi(CPLGetXMLValue(psTreNode, "length", "-1"));
    nTreMinLength = atoi(CPLGetXMLValue(psTreNode, "minlength", "-1"));
    nTreMaxLength = atoi(CPLGetXMLValue(psTreNode, "maxlength", "-1"));

    if( (nTreLength > 0 && nTRESize != nTreLength) ||
        (nTreMinLength > 0 && nTRESize < nTreMinLength) )
    {
        CPLError( CE_Warning, CPLE_AppDefined,
                  "%s TRE wrong size, ignoring.", pszTREName );
        return NULL;
    }

    psOutXMLNode = CPLCreateXMLNode(NULL, CXT_Element, "tre");
    CPLCreateXMLNode(CPLCreateXMLNode(psOutXMLNode, CXT_Attribute, "name"),
                     CXT_Text, pszTREName);

    CSLDestroy(NITFGenericMetadataReadTREInternal(NULL,
                                                  &nMDSize,
                                                  &nMDAlloc,
                                                  psOutXMLNode,
                                                  pszTREName,
                                                  pachTRE,
                                                  nTRESize,
                                                  psTreNode,
                                                  &nTreOffset,
                                                  "",
                                                  &bError));

    if (bError == FALSE && nTreLength > 0 && nTreOffset != nTreLength)
    {
        CPLError( CE_Warning, CPLE_AppDefined,
                  "Inconsistant declaration of %s TRE",
                  pszTREName );
    }
    if (nTreOffset < nTRESize)
        CPLDebug("NITF", "%d remaining bytes at end of %s TRE",
                 nTRESize -nTreOffset, pszTREName);

    return psOutXMLNode;
}

/************************************************************************/
/*                        NITFGenericMetadataRead()                     */
/*                                                                      */
/* Add metadata from TREs of file and image objects in the papszMD list */
/* pszSpecificTRE can be NULL, in which case all TREs listed in         */
/* data/nitf_resources.xml that have md_prefix defined will be looked   */
/* for. If not NULL, only the specified one will be looked for.         */
/************************************************************************/

char **NITFGenericMetadataRead( char **papszMD,
                                NITFFile* psFile,
                                NITFImage *psImage,
                                const char* pszSpecificTREName)
{
    CPLXMLNode* psTreeNode = NULL;
    CPLXMLNode* psTresNode = NULL;
    CPLXMLNode* psIter = NULL;

    if (psFile == NULL && psImage == NULL)
        return papszMD;

    psTreeNode = NITFLoadXMLSpec(psFile ? psFile : psImage->psFile);
    if (psTreeNode == NULL)
        return papszMD;

    psTresNode = CPLGetXMLNode(psTreeNode, "=tres");
    if (psTresNode == NULL)
    {
        CPLDebug("NITF", "Cannot find <tres> root element");
        return papszMD;
    }

    for(psIter = psTresNode->psChild;psIter!=NULL;psIter = psIter->psNext)
    {
        if (psIter->eType == CXT_Element &&
            psIter->pszValue != NULL &&
            strcmp(psIter->pszValue, "tre") == 0)
        {
            const char* pszName = CPLGetXMLValue(psIter, "name", NULL);
            const char* pszMDPrefix = CPLGetXMLValue(psIter, "md_prefix", NULL);
            if (pszName != NULL && ((pszSpecificTREName == NULL && pszMDPrefix != NULL) ||
                                    (pszSpecificTREName != NULL && strcmp(pszName, pszSpecificTREName) == 0)))
            {
                if (psFile != NULL)
                {
                    const char *pachTRE = NULL;
                    int  nTRESize = 0;

                    pachTRE = NITFFindTRE( psFile->pachTRE, psFile->nTREBytes,
                                           pszName, &nTRESize);
                    if( pachTRE != NULL )
                        papszMD = NITFGenericMetadataReadTRE(
                                  papszMD, pszName, pachTRE, nTRESize, psIter);
                }
                if (psImage != NULL)
                {
                    const char *pachTRE = NULL;
                    int  nTRESize = 0;

                    pachTRE = NITFFindTRE( psImage->pachTRE, psImage->nTREBytes,
                                           pszName, &nTRESize);
                    if( pachTRE != NULL )
                       papszMD = NITFGenericMetadataReadTRE(
                                  papszMD, pszName, pachTRE, nTRESize, psIter);
                }
                if (pszSpecificTREName)
                    break;
            }
        }
    }

    return papszMD;
}
