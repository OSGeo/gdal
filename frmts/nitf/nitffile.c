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
 ******************************************************************************
 *
 * $Log$
 * Revision 1.5  2002/12/17 20:03:08  warmerda
 * added rudimentary NITF 1.1 support
 *
 * Revision 1.4  2002/12/17 05:26:26  warmerda
 * implement basic write support
 *
 * Revision 1.3  2002/12/03 18:07:59  warmerda
 * load VQLUTs
 *
 * Revision 1.2  2002/12/03 04:43:54  warmerda
 * lots of work
 *
 * Revision 1.1  2002/12/02 06:09:29  warmerda
 * New
 *
 */

#include "nitflib.h"
#include "cpl_vsi.h"
#include "cpl_conv.h"
#include "cpl_string.h"

CPL_CVSID("$Id$");

static int 
NITFCollectSegmentInfo( NITFFile *psFile, int nOffset, char *pszType,
                        int nHeaderLenSize, int nDataLenSize, 
                        int *pnNextData );
static void NITFLoadLocationTable( NITFFile *psFile );
static int NITFLoadVQTables( NITFFile *psFile );

/************************************************************************/
/*                              NITFOpen()                              */
/************************************************************************/

NITFFile *NITFOpen( const char *pszFilename, int bUpdatable )

{
    FILE	*fp;
    char        *pachHeader;
    NITFFile    *psFile;
    int         nHeaderLen, nOffset, nNextData;
    char        szTemp[128];

/* -------------------------------------------------------------------- */
/*      Open the file.                                                  */
/* -------------------------------------------------------------------- */
    if( bUpdatable )
        fp = VSIFOpen( pszFilename, "r+b" );
    else
        fp = VSIFOpen( pszFilename, "rb" );

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
    VSIFRead( szTemp, 1, 9, fp );

    if( !EQUALN(szTemp,"NITF",4) && !EQUALN(szTemp,"NSIF",4) )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "The file %s is not an NITF file.", 
                  pszFilename );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Get header length.                                              */
/* -------------------------------------------------------------------- */
    if( EQUALN(szTemp,"NITF01.",7) )
    {
        VSIFSeek( fp, 394, SEEK_SET );
        VSIFRead( szTemp, 1, 6, fp );
        szTemp[6] = '\0';
    }
    else
    {
        VSIFSeek( fp, 354, SEEK_SET );
        VSIFRead( szTemp, 1, 6, fp );
        szTemp[6] = '\0';
    }

    nHeaderLen = atoi(szTemp);

/* -------------------------------------------------------------------- */
/*      Read the whole file header.                                     */
/* -------------------------------------------------------------------- */
    pachHeader = (char *) CPLMalloc(nHeaderLen);
    VSIFSeek( fp, 0, SEEK_SET );
    VSIFRead( pachHeader, 1, nHeaderLen, fp );

/* -------------------------------------------------------------------- */
/*      Create and initialize info structure about file.                */
/* -------------------------------------------------------------------- */
    psFile = (NITFFile *) CPLCalloc(sizeof(NITFFile),1);
    psFile->fp = fp;
    psFile->pachHeader = pachHeader;

/* -------------------------------------------------------------------- */
/*      Get version.                                                    */
/* -------------------------------------------------------------------- */
    NITFGetField( psFile->szVersion, pachHeader, 0, 9 );

/* -------------------------------------------------------------------- */
/*      Collect segment info for the types we care about.               */
/* -------------------------------------------------------------------- */
    nNextData = nHeaderLen;

    if( EQUALN(psFile->szVersion,"NITF01.",7) )
        nOffset = 400;
    else
        nOffset = 360;
    nOffset = NITFCollectSegmentInfo( psFile, nOffset,"IM",6, 10, &nNextData );

    nOffset = NITFCollectSegmentInfo( psFile, nOffset, "GR", 4, 6, &nNextData);

    nOffset += 3; /* NUMX reserved field */

    nOffset = NITFCollectSegmentInfo( psFile, nOffset, "TX", 4, 5, &nNextData);

    nOffset = NITFCollectSegmentInfo( psFile, nOffset, "DE", 4, 9, &nNextData);

    nOffset = NITFCollectSegmentInfo( psFile, nOffset, "RE", 4, 7, &nNextData);

/* -------------------------------------------------------------------- */
/*      Is there a TRE to suck up?                                      */
/* -------------------------------------------------------------------- */
    psFile->nTREBytes = 
        atoi(NITFGetField( szTemp, pachHeader, nOffset, 5 ));
    nOffset += 5;

    nOffset += 3; /* UDHOFL */

    if( psFile->nTREBytes != 0 )
    {
        psFile->pachTRE = pachHeader + nOffset;
        psFile->nTREBytes -= 3;
    }

/* -------------------------------------------------------------------- */
/*      Are the VQ tables to load up?                                   */
/* -------------------------------------------------------------------- */
    NITFLoadLocationTable( psFile );
    NITFLoadVQTables( psFile );

    return psFile;
}

/************************************************************************/
/*                             NITFClose()                              */
/************************************************************************/

void NITFClose( NITFFile *psFile )

{
    int  iSegment, i;

    for( iSegment = 0; iSegment < psFile->nSegmentCount; iSegment++ )
    {
        NITFSegmentInfo *psSegInfo = psFile->pasSegmentInfo + iSegment;

        if( psSegInfo->hAccess == NULL )
            continue;

        if( EQUAL(psSegInfo->szSegmentType,"IM"))
            NITFImageDeaccess( (NITFImage *) psSegInfo->hAccess );
        else
        {
            CPLAssert( FALSE );
        }
    }

    CPLFree( psFile->pasSegmentInfo );
    CPLFree( psFile->pasLocations );
    for( i = 0; i < 4; i++ )
        CPLFree( psFile->apanVQLUT[i] );

    if( psFile->fp != NULL )
        VSIFClose( psFile->fp );
    CPLFree( psFile->pachHeader );
    CPLFree( psFile );
}

/************************************************************************/
/*                             NITFCreate()                             */
/*                                                                      */
/*      Create a new uncompressed NITF file.                            */
/************************************************************************/

NITFFile *NITFCreate( const char *pszFilename, 
                      int nPixels, int nLines, int nBands, 
                      int nBitsPerSample, const char *pszPVType,
                      char **papszOptions )

{
    FILE	*fp;
    char        *pachIMHDR;
    char        achHeader[5000];
    int         nOffset = 0, iBand, nImageSize, nIHSize;
    const char *pszIREP;

    pszIREP = CSLFetchNameValue( papszOptions, "IREP" );
    if( pszIREP == NULL )
        pszIREP = "MONO";

/* -------------------------------------------------------------------- */
/*      Open new file.                                                  */
/* -------------------------------------------------------------------- */
    fp = VSIFOpen( pszFilename, "wb+" );
    if( fp == NULL )
    {
        CPLError( CE_Failure, CPLE_OpenFailed, 
                  "Unable to create file %s,\n"
                  "check path and permissions.",
                  pszFilename );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Compute raw image size.                                         */
/* -------------------------------------------------------------------- */
    nImageSize = ((nBitsPerSample)/8) * nPixels * nLines * nBands;

/* -------------------------------------------------------------------- */
/*      Prepare the file header.                                        */
/* -------------------------------------------------------------------- */
    memset( achHeader, ' ', sizeof(achHeader) );

#define PLACE(location,name,text)  strncpy(location,text,strlen(text))

    PLACE(achHeader+   0, FDHR_FVER,    "NITF02.10"                     );
    PLACE(achHeader+   9, CLEVEL,       "05"                            );
    PLACE(achHeader+  11, STYPE        ,"BF01"                          );
    PLACE(achHeader+  15, OSTAID       ,"GDAL"                          );
    PLACE(achHeader+  25, FDT          ,"20021216151629"                );
    PLACE(achHeader+  39, FTITLE       ,""                              );
    PLACE(achHeader+ 119, FSCLAS       ,"U"                             );
    PLACE(achHeader+ 120, FSCLSY       ,""                              );
    PLACE(achHeader+ 122, FSCODE       ,""                              );
    PLACE(achHeader+ 133, FSCTLH       ,""                              );
    PLACE(achHeader+ 135, FSREL        ,""                              );
    PLACE(achHeader+ 155, FSDCTP       ,""                              );
    PLACE(achHeader+ 157, FSDCDT       ,""                              );
    PLACE(achHeader+ 165, FSDCXM       ,""                              );
    PLACE(achHeader+ 169, FSDG         ,""                              );
    PLACE(achHeader+ 170, FSDGDT       ,""                              );
    PLACE(achHeader+ 178, FSCLTX       ,""                              );
    PLACE(achHeader+ 221, FSCATP       ,""                              );
    PLACE(achHeader+ 222, FSCAUT       ,""                              );
    PLACE(achHeader+ 262, FSCRSN       ,""                              );
    PLACE(achHeader+ 263, FSSRDT       ,""                              );
    PLACE(achHeader+ 271, FSCTLN       ,""                              );
    PLACE(achHeader+ 286, FSCOP        ,"00000"                         );
    PLACE(achHeader+ 291, FSCPYS       ,"00000"                         );
    PLACE(achHeader+ 296, ENCRYP       ,"0"                             );
    achHeader[297] = achHeader[298] = achHeader[299] = 0x00; /* FBKGC */
    PLACE(achHeader+ 300, ONAME        ,""                              );
    PLACE(achHeader+ 324, OPHONE       ,""                              );
    PLACE(achHeader+ 342, FL           ,"????????????"                  );
    PLACE(achHeader+ 354, HL           ,"000404"                        );
    PLACE(achHeader+ 360, NUMI         ,"1"                             );
    PLACE(achHeader+ 363, LISH1        ,"??????"                        );
    PLACE(achHeader+ 369, LI1          ,CPLSPrintf("%010d",nImageSize)  );
    PLACE(achHeader+ 379, NUMS         ,"000"                           );
    PLACE(achHeader+ 382, NUMX         ,"000"                           );
    PLACE(achHeader+ 385, NUMT         ,"000"                           );
    PLACE(achHeader+ 388, NUMDES       ,"000"                           );
    PLACE(achHeader+ 391, NUMRES       ,"000"                           );
    PLACE(achHeader+ 394, UDHDL        ,"00000"                         );
    PLACE(achHeader+ 399, XHDL         ,"00000"                         );
    
/* -------------------------------------------------------------------- */
/*      Prepare the image header.                                       */
/* -------------------------------------------------------------------- */
    pachIMHDR = achHeader + 404;

    PLACE(pachIMHDR+   0, IM           , "IM"                           );
    PLACE(pachIMHDR+   2, HD1          , "Missing"                      );
    PLACE(pachIMHDR+  12, IDATIM       , "20021216151629"               );
    PLACE(pachIMHDR+  26, TGTID        , ""                             );
    PLACE(pachIMHDR+  43, HD2          , ""                             );
    PLACE(pachIMHDR+ 123, ISCLAS       , "U"                            );
    PLACE(pachIMHDR+ 124, ISCLSY       , ""                             );
    PLACE(pachIMHDR+ 126, ISCODE       , ""                             );
    PLACE(pachIMHDR+ 137, ISCTLH       , ""                             );
    PLACE(pachIMHDR+ 139, ISREL        , ""                             );
    PLACE(pachIMHDR+ 159, ISDCTP       , ""                             );
    PLACE(pachIMHDR+ 161, ISDCDT       , ""                             );
    PLACE(pachIMHDR+ 169, ISDCXM       , ""                             );
    PLACE(pachIMHDR+ 173, ISDG         , ""                             );
    PLACE(pachIMHDR+ 174, ISDGDT       , ""                             );
    PLACE(pachIMHDR+ 182, ISCLTX       , ""                             );
    PLACE(pachIMHDR+ 225, ISCATP       , ""                             );
    PLACE(pachIMHDR+ 226, ISCAUT       , ""                             );
    PLACE(pachIMHDR+ 266, ISCRSN       , ""                             );
    PLACE(pachIMHDR+ 267, ISSRDT       , ""                             );
    PLACE(pachIMHDR+ 275, ISCTLN       , ""                             );
    PLACE(pachIMHDR+ 290, ENCRYP       , "0"                            );
    PLACE(pachIMHDR+ 291, ISORCE       , "Unknown"                      );
    PLACE(pachIMHDR+ 333, NROWS        , CPLSPrintf("%08d", nLines)     );
    PLACE(pachIMHDR+ 341, NCOLS        , CPLSPrintf("%08d", nPixels)    );
    PLACE(pachIMHDR+ 349, PVTYPE       , pszPVType                      );
    PLACE(pachIMHDR+ 352, IREP         , pszIREP                        );
    PLACE(pachIMHDR+ 360, ICAT         , "VIS"                          );
    PLACE(pachIMHDR+ 368, ABPP         , CPLSPrintf("%02d",nBitsPerSample) );
    PLACE(pachIMHDR+ 370, PJUST        , "R"                            );
    PLACE(pachIMHDR+ 371, ICORDS       , " "                            );
    PLACE(pachIMHDR+ 372, NICOM        , "0"                            );
    PLACE(pachIMHDR+ 373, IC           , "NC"                           );
    PLACE(pachIMHDR+ 375, NBANDS       , CPLSPrintf("%d",nBands)        );

    nOffset = 376;

/* -------------------------------------------------------------------- */
/*      Per band info                                                   */
/* -------------------------------------------------------------------- */
    for( iBand = 0; iBand < nBands; iBand++ )
    {
        const char *pszIREPBAND = "M";

        if( EQUAL(pszIREP,"RGB/LUT") )
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

        PLACE(pachIMHDR+nOffset+ 0, IREPBANDn, pszIREPBAND                 );
        PLACE(pachIMHDR+nOffset+ 2, ISUBCATn, ""                           );
        PLACE(pachIMHDR+nOffset+ 8, IFCn  , "N"                            );
        PLACE(pachIMHDR+nOffset+ 9, IMFLTn, ""                             );

        if( !EQUAL(pszIREP,"RGB/LUT") )
        {
            PLACE(pachIMHDR+nOffset+12, NLUTSn, "0"                        );
            nOffset += 13;
        }
        else
        {
            int iC;

            PLACE(pachIMHDR+nOffset+12, NLUTSn, "3"                        );
            PLACE(pachIMHDR+nOffset+13, NELUTn, "256"                      );

            for( iC = 0; iC < 256; iC++ )
            {
                pachIMHDR[nOffset+18+iC+  0] = iC;
                pachIMHDR[nOffset+18+iC+256] = iC;
                pachIMHDR[nOffset+18+iC+512] = iC;
            }
            nOffset += 18 + 256*3;
        }
    }

/* -------------------------------------------------------------------- */
/*      Remainder of image header info.                                 */
/* -------------------------------------------------------------------- */
    PLACE(pachIMHDR+nOffset+  0, ISYNC , "0"                            );
    PLACE(pachIMHDR+nOffset+  1, IMODE , "B"                            );
    PLACE(pachIMHDR+nOffset+  2, NBPR  , "0001"                         );
    PLACE(pachIMHDR+nOffset+  6, NBPC  , "0001"                         );
    PLACE(pachIMHDR+nOffset+ 10, NPPBH , CPLSPrintf("%04d",nPixels)     );
    PLACE(pachIMHDR+nOffset+ 14, NPPBV , CPLSPrintf("%04d",nLines)      );
    PLACE(pachIMHDR+nOffset+ 18, NBPP  , CPLSPrintf("%02d",nBitsPerSample) );
    PLACE(pachIMHDR+nOffset+ 22, IDLVL , "001"                          );
    PLACE(pachIMHDR+nOffset+ 25, IALVL , "000"                          );
    PLACE(pachIMHDR+nOffset+ 28, ILOC  , "0000000000"                   );
    PLACE(pachIMHDR+nOffset+ 38, IMAG  , "1.0 "                         );
    PLACE(pachIMHDR+nOffset+ 42, UDIDL , "00000"                        );
    PLACE(pachIMHDR+nOffset+ 47, IXSHDL, "00000"                        );

    nOffset += 53;

    nIHSize = nOffset;

/* -------------------------------------------------------------------- */
/*      Update the image header length in the file header and the       */
/*      total file size.                                                */
/* -------------------------------------------------------------------- */
    PLACE(achHeader+ 363, LISH1, CPLSPrintf("%06d",nIHSize)      );
    PLACE(achHeader+ 342, FL,
          CPLSPrintf( "%012d", 404 + nIHSize + nImageSize ) );

/* -------------------------------------------------------------------- */
/*      Write header info to file.                                      */
/* -------------------------------------------------------------------- */
    VSIFWrite( achHeader, 1, nIHSize+404, fp );

/* -------------------------------------------------------------------- */
/*      Grow file to full required size by writing one byte at the end. */
/* -------------------------------------------------------------------- */
    VSIFSeek( fp, nImageSize-1, SEEK_CUR );

    achHeader[0] = '\0';
    VSIFWrite( achHeader, 1, 1, fp );

    VSIFClose( fp );

    return NULL;
}

/************************************************************************/
/*                       NITFCollectSegmentInfo()                       */
/*                                                                      */
/*      Collect the information about a set of segments of a            */
/*      particular type from the NITF file header, and add them to      */
/*      the segment list in the NITFFile object.                        */
/************************************************************************/

static int 
NITFCollectSegmentInfo( NITFFile *psFile, int nOffset, char *pszType,
                        int nHeaderLenSize, int nDataLenSize, int *pnNextData )

{
    char szTemp[12];
    char *pachSegDef;
    int  nCount, nSegDefSize, iSegment;

/* -------------------------------------------------------------------- */
/*      Get the segment count, and grow the segmentinfo array           */
/*      accordingly.                                                    */
/* -------------------------------------------------------------------- */
    VSIFSeek( psFile->fp, nOffset, SEEK_SET );
    VSIFRead( szTemp, 1, 3, psFile->fp );
    szTemp[3] = '\0';

    nCount = atoi(szTemp);

    if( nCount == 0 )
        return nOffset + 3;

    if( psFile->pasSegmentInfo == NULL )
        psFile->pasSegmentInfo = (NITFSegmentInfo *)
            CPLMalloc( sizeof(NITFSegmentInfo) * nCount );
    else
        psFile->pasSegmentInfo = (NITFSegmentInfo *)
            CPLRealloc( psFile->pasSegmentInfo, 
                        sizeof(NITFSegmentInfo)
                        * (psFile->nSegmentCount+nCount) );

/* -------------------------------------------------------------------- */
/*      Read the detailed information about the segments.               */
/* -------------------------------------------------------------------- */
    nSegDefSize = nCount * (nHeaderLenSize + nDataLenSize);
    pachSegDef = (char *) CPLMalloc(nCount * (nHeaderLenSize + nDataLenSize));
    
    VSIFRead( pachSegDef, 1, nSegDefSize, psFile->fp );

/* -------------------------------------------------------------------- */
/*      Collect detailed about segment.                                 */
/* -------------------------------------------------------------------- */
    for( iSegment = 0; iSegment < nCount; iSegment++ )
    {
        NITFSegmentInfo *psInfo = psFile->pasSegmentInfo+psFile->nSegmentCount;
        
        psInfo->hAccess = NULL;
        strcpy( psInfo->szSegmentType, pszType );
        
        psInfo->nSegmentHeaderSize = 
            atoi(NITFGetField(szTemp,pachSegDef, 
                              iSegment * (nHeaderLenSize+nDataLenSize), 
                              nHeaderLenSize));
        psInfo->nSegmentSize = 
            atoi(NITFGetField(szTemp,pachSegDef, 
                              iSegment * (nHeaderLenSize+nDataLenSize) 
                              + nHeaderLenSize,
                              nDataLenSize));

        psInfo->nSegmentHeaderStart = *pnNextData;
        psInfo->nSegmentStart = *pnNextData + psInfo->nSegmentHeaderSize;

        *pnNextData += (psInfo->nSegmentHeaderSize+psInfo->nSegmentSize);
        psFile->nSegmentCount++;
    }

    CPLFree( pachSegDef );

    return nOffset + nSegDefSize + 3;
}

/************************************************************************/
/*                       NITFLoadLocationTable()                        */
/************************************************************************/

static void NITFLoadLocationTable( NITFFile *psFile )

{
    GUInt32  nLocTableOffset;
    GUInt16  nLocCount;
    int      iLoc;

/* -------------------------------------------------------------------- */
/*      Get the location table position from within the RPFHDR TRE      */
/*      structure.                                                      */
/* -------------------------------------------------------------------- */
    if( psFile->pachTRE == NULL || !EQUALN(psFile->pachTRE,"RPFHDR",6) )
        return;

    memcpy( &nLocTableOffset, psFile->pachTRE + 55, 4 );
    nLocTableOffset = CPL_MSBWORD32( nLocTableOffset );
    
    if( nLocTableOffset == 0 )
        return;

/* -------------------------------------------------------------------- */
/*      Read the count of entries in the location table.                */
/* -------------------------------------------------------------------- */
    VSIFSeek( psFile->fp, nLocTableOffset + 6, SEEK_SET );
    VSIFRead( &nLocCount, 1, 2, psFile->fp );
    nLocCount = CPL_MSBWORD16( nLocCount );
    psFile->nLocCount = nLocCount;

    psFile->pasLocations = (NITFLocation *) 
        CPLCalloc(sizeof(NITFLocation), nLocCount);
    
/* -------------------------------------------------------------------- */
/*      Process the locations.                                          */
/* -------------------------------------------------------------------- */
    VSIFSeek( psFile->fp, 6, SEEK_CUR );
    for( iLoc = 0; iLoc < nLocCount; iLoc++ )
    {
        unsigned char abyEntry[10];
        
        VSIFRead( abyEntry, 1, 10, psFile->fp );
        
        psFile->pasLocations[iLoc].nLocId = abyEntry[0] * 256 + abyEntry[1];

        CPL_MSBPTR32( abyEntry + 2 );
        memcpy( &(psFile->pasLocations[iLoc].nLocSize), abyEntry + 2, 4 );

        CPL_MSBPTR32( abyEntry + 6 );
        memcpy( &(psFile->pasLocations[iLoc].nLocOffset), abyEntry + 6, 4 );
    }
}

/************************************************************************/
/*                          NITFLoadVQTables()                          */
/************************************************************************/

static int NITFLoadVQTables( NITFFile *psFile )

{
    int     i, nVQOffset=0, nVQSize=0;

/* -------------------------------------------------------------------- */
/*      Do we already have the VQ tables?                               */
/* -------------------------------------------------------------------- */
    if( psFile->apanVQLUT[0] != NULL )
        return TRUE;

/* -------------------------------------------------------------------- */
/*      Do we have the location information?                            */
/* -------------------------------------------------------------------- */
    for( i = 0; i < psFile->nLocCount; i++ )
    {
        if( psFile->pasLocations[i].nLocId == 132 )
        {
            nVQOffset = psFile->pasLocations[i].nLocOffset;
            nVQSize = psFile->pasLocations[i].nLocSize;
        }
    }

    if( nVQOffset == 0 )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Load the tables.                                                */
/* -------------------------------------------------------------------- */
    for( i = 0; i < 4; i++ )
    {
        GUInt32 nVQVector;

        psFile->apanVQLUT[i] = (GUInt32 *) CPLCalloc(4096,sizeof(GUInt32));

        VSIFSeek( psFile->fp, nVQOffset + 6 + i*14 + 10, SEEK_SET );
        VSIFRead( &nVQVector, 1, 4, psFile->fp );
        nVQVector = CPL_MSBWORD32( nVQVector );
        
        VSIFSeek( psFile->fp, nVQOffset + nVQVector, SEEK_SET );
        VSIFRead( psFile->apanVQLUT[i], 4, 4096, psFile->fp );
    }

    return TRUE;
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

