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
 * Revision 1.14  2003/09/11 19:51:55  warmerda
 * avoid type casting warnings
 *
 * Revision 1.13  2003/06/23 18:31:39  warmerda
 * fixed field alignment for a few fields on write
 *
 * Revision 1.12  2003/06/06 17:10:14  warmerda
 * Improved header size test to support large headers, even those as large as
 * the whole file.
 *
 * Revision 1.11  2003/06/06 16:52:32  warmerda
 * changes based on better understanding of conditional FSDEVT field
 *
 * Revision 1.10  2003/06/06 15:07:53  warmerda
 * fixed security area sizing for NITF 2.0 images, its like NITF 1.1.
 *
 * Revision 1.9  2003/05/29 19:50:39  warmerda
 * added improved TRE handling
 *
 * Revision 1.8  2003/05/05 17:57:54  warmerda
 * added blocked writing support
 *
 * Revision 1.7  2002/12/18 20:16:20  warmerda
 * allow lots of fields to be overridden with passed in options
 *
 * Revision 1.6  2002/12/17 21:23:15  warmerda
 * implement LUT reading and writing
 *
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
    int         nHeaderLen, nOffset, nNextData, nHeaderLenOffset;
    char        szTemp[128], achFSDWNG[6];
    long        nFileLength;

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
/*      Read the FSDWNG field.                                          */
/* -------------------------------------------------------------------- */
    if( VSIFSeek( fp, 280, SEEK_SET ) != 0 
        || VSIFRead( achFSDWNG, 1, 6, fp ) != 6 )
    {
        CPLError( CE_Failure, CPLE_NotSupported, 
                  "Unable to read FSDWNG field from NITF file.  File is either corrupt\n"
                  "or empty." );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Get header length.                                              */
/* -------------------------------------------------------------------- */
    if( EQUALN(szTemp,"NITF01.",7) || EQUALN(achFSDWNG,"999998",6) )
        nHeaderLenOffset = 394;
    else
        nHeaderLenOffset = 354;

    if( VSIFSeek( fp, nHeaderLenOffset, SEEK_SET ) != 0 
        || VSIFRead( szTemp, 1, 6, fp ) != 6 )
    {
        CPLError( CE_Failure, CPLE_NotSupported, 
                  "Unable to read header length from NITF file.  File is either corrupt\n"
                  "or empty." );
        return NULL;
    }

    szTemp[6] = '\0';
    nHeaderLen = atoi(szTemp);

    VSIFSeek( fp, 0, SEEK_END );
    nFileLength = VSIFTell( fp ) ;
    if( nHeaderLen < nHeaderLenOffset || nHeaderLen > nFileLength )
    {
        CPLError( CE_Failure, CPLE_NotSupported, 
                  "NITF Header Length (%d) seems to be corrupt.",
                  nHeaderLen );
        return NULL;
    }

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

    nOffset = nHeaderLenOffset + 6;

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
    int         nOffset = 0, iBand, nImageSize, nIHSize, nNPPBH, nNPPBV;
    int         nNBPR, nNBPC;
    const char *pszIREP;

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
/*      Fetch some parameter overrides.                                 */
/* -------------------------------------------------------------------- */
    pszIREP = CSLFetchNameValue( papszOptions, "IREP" );
    if( pszIREP == NULL )
        pszIREP = "MONO";

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
    
    if( nNPPBH > 9999 || nNPPBV > 9999  )
        nNPPBH = nNPPBV = 256;

    nNBPR = (nPixels + nNPPBH - 1) / nNPPBH;
    nNBPC = (nLines + nNPPBV - 1) / nNPPBV;

    nImageSize = 
        ((nBitsPerSample)/8) * nNBPR * nNBPC * nNPPBH * nNPPBV * nBands;

/* -------------------------------------------------------------------- */
/*      Prepare the file header.                                        */
/* -------------------------------------------------------------------- */
    memset( achHeader, ' ', sizeof(achHeader) );

#define PLACE(location,name,text)  strncpy(location,text,strlen(text))
#define OVR(width,location,name,text) { 				\
    const char *pszParmValue; 						\
    pszParmValue = CSLFetchNameValue( papszOptions, #name ); 		\
    if( pszParmValue == NULL )						\
        pszParmValue = text;						\
    strncpy(location,pszParmValue,MIN(width,strlen(pszParmValue))); }   

    PLACE (achHeader+  0, FDHR_FVER,    "NITF02.10"                     );
    OVR( 2,achHeader+  9, CLEVEL,       "05"                            );
    PLACE (achHeader+ 11, STYPE        ,"BF01"                          );
    OVR(10,achHeader+ 15, OSTAID       ,"GDAL"                          );
    OVR(14,achHeader+ 25, FDT          ,"20021216151629"                );
    OVR(80,achHeader+ 39, FTITLE       ,""                              );
    OVR( 1,achHeader+119, FSCLAS       ,"U"                             );
    OVR( 2,achHeader+120, FSCLSY       ,""                              );
    OVR(11,achHeader+122, FSCODE       ,""                              );
    OVR( 2,achHeader+133, FSCTLH       ,""                              );
    OVR(20,achHeader+135, FSREL        ,""                              );
    OVR( 2,achHeader+155, FSDCTP       ,""                              );
    OVR( 8,achHeader+157, FSDCDT       ,""                              );
    OVR( 4,achHeader+165, FSDCXM       ,""                              );
    OVR( 1,achHeader+169, FSDG         ,""                              );
    OVR( 8,achHeader+170, FSDGDT       ,""                              );
    OVR(43,achHeader+178, FSCLTX       ,""                              );
    OVR( 1,achHeader+221, FSCATP       ,""                              );
    OVR(40,achHeader+222, FSCAUT       ,""                              );
    OVR( 1,achHeader+262, FSCRSN       ,""                              );
    OVR( 8,achHeader+263, FSSRDT       ,""                              );
    OVR(15,achHeader+271, FSCTLN       ,""                              );
    OVR( 5,achHeader+286, FSCOP        ,"00000"                         );
    OVR( 5,achHeader+291, FSCPYS       ,"00000"                         );
    PLACE (achHeader+296, ENCRYP       ,"0"                             );
    achHeader[297] = achHeader[298] = achHeader[299] = 0x00; /* FBKGC */
    OVR(24,achHeader+300, ONAME        ,""                              );
    OVR(18,achHeader+324, OPHONE       ,""                              );
    PLACE (achHeader+342, FL           ,"????????????"                  );
    PLACE (achHeader+354, HL           ,"000404"                        );
    PLACE (achHeader+360, NUMI         ,"001"                           );
    PLACE (achHeader+363, LISH1        ,"??????"                        );
    PLACE (achHeader+369, LI1          ,CPLSPrintf("%010d",nImageSize)  );
    PLACE (achHeader+379, NUMS         ,"000"                           );
    PLACE (achHeader+382, NUMX         ,"000"                           );
    PLACE (achHeader+385, NUMT         ,"000"                           );
    PLACE (achHeader+388, NUMDES       ,"000"                           );
    PLACE (achHeader+391, NUMRES       ,"000"                           );
    PLACE (achHeader+394, UDHDL        ,"00000"                         );
    PLACE (achHeader+399, XHDL         ,"00000"                         );
    
/* -------------------------------------------------------------------- */
/*      Prepare the image header.                                       */
/* -------------------------------------------------------------------- */
    pachIMHDR = achHeader + 404;

    PLACE (pachIMHDR+  0, IM           , "IM"                           );
    OVR(10,pachIMHDR+  2, HD1          , "Missing"                      );
    OVR(14,pachIMHDR+ 12, IDATIM       , "20021216151629"               );
    OVR(17,pachIMHDR+ 26, TGTID        , ""                             );
    OVR(80,pachIMHDR+ 43, IID2         , ""                             );
    OVR( 1,pachIMHDR+123, ISCLAS       , "U"                            );
    OVR( 2,pachIMHDR+124, ISCLSY       , ""                             );
    OVR(11,pachIMHDR+126, ISCODE       , ""                             );
    OVR( 2,pachIMHDR+137, ISCTLH       , ""                             );
    OVR(20,pachIMHDR+139, ISREL        , ""                             );
    OVR( 2,pachIMHDR+159, ISDCTP       , ""                             );
    OVR( 8,pachIMHDR+161, ISDCDT       , ""                             );
    OVR( 4,pachIMHDR+169, ISDCXM       , ""                             );
    OVR( 1,pachIMHDR+173, ISDG         , ""                             );
    OVR( 8,pachIMHDR+174, ISDGDT       , ""                             );
    OVR(43,pachIMHDR+182, ISCLTX       , ""                             );
    OVR( 1,pachIMHDR+225, ISCATP       , ""                             );
    OVR(40,pachIMHDR+226, ISCAUT       , ""                             );
    OVR( 1,pachIMHDR+266, ISCRSN       , ""                             );
    OVR( 8,pachIMHDR+267, ISSRDT       , ""                             );
    OVR(15,pachIMHDR+275, ISCTLN       , ""                             );
    PLACE (pachIMHDR+290, ENCRYP       , "0"                            );
    OVR(42,pachIMHDR+291, ISORCE       , "Unknown"                      );
    PLACE (pachIMHDR+333, NROWS        , CPLSPrintf("%08d", nLines)     );
    PLACE (pachIMHDR+341, NCOLS        , CPLSPrintf("%08d", nPixels)    );
    PLACE (pachIMHDR+349, PVTYPE       , pszPVType                      );
    PLACE (pachIMHDR+352, IREP         , pszIREP                        );
    OVR( 8,pachIMHDR+360, ICAT         , "VIS"                          );
    OVR( 2,pachIMHDR+368, ABPP         , CPLSPrintf("%02d",nBitsPerSample) );
    OVR( 1,pachIMHDR+370, PJUST        , "R"                            );
    OVR( 1,pachIMHDR+371, ICORDS       , " "                            );

    nOffset = 372;
    if( pachIMHDR[371] != ' ' )
    {
        OVR(60,pachIMHDR+nOffset, IGEOLO, ""                            );
        nOffset += 60;
    }

    PLACE (pachIMHDR+nOffset, NICOM    , "0"                            );
    PLACE (pachIMHDR+nOffset+1, IC     , "NC"                           );
    PLACE (pachIMHDR+nOffset+3, NBANDS , CPLSPrintf("%d",nBands)        );

    nOffset += 4;

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
            int iC, nCount=256;

            if( CSLFetchNameValue(papszOptions,"LUT_SIZE") != NULL )
                nCount = atoi(CSLFetchNameValue(papszOptions,"LUT_SIZE"));

            PLACE(pachIMHDR+nOffset+12, NLUTSn, "3"                        );
            PLACE(pachIMHDR+nOffset+13, NELUTn, CPLSPrintf("%05d",nCount)  );

            for( iC = 0; iC < nCount; iC++ )
            {
                pachIMHDR[nOffset+18+iC+       0] = (char) iC;
                pachIMHDR[nOffset+18+iC+nCount*1] = (char) iC;
                pachIMHDR[nOffset+18+iC+nCount*2] = (char) iC;
            }
            nOffset += 18 + nCount*3;
        }
    }

/* -------------------------------------------------------------------- */
/*      Remainder of image header info.                                 */
/* -------------------------------------------------------------------- */
    PLACE(pachIMHDR+nOffset+  0, ISYNC , "0"                            );
    PLACE(pachIMHDR+nOffset+  1, IMODE , "B"                            );
    PLACE(pachIMHDR+nOffset+  2, NBPR  , CPLSPrintf("%04d",nNBPR)       );
    PLACE(pachIMHDR+nOffset+  6, NBPC  , CPLSPrintf("%04d",nNBPC)       );
    PLACE(pachIMHDR+nOffset+ 10, NPPBH , CPLSPrintf("%04d",nNPPBH)      );
    PLACE(pachIMHDR+nOffset+ 14, NPPBV , CPLSPrintf("%04d",nNPPBV)      );
    PLACE(pachIMHDR+nOffset+ 18, NBPP  , CPLSPrintf("%02d",nBitsPerSample) );
    PLACE(pachIMHDR+nOffset+ 20, IDLVL , "001"                          );
    PLACE(pachIMHDR+nOffset+ 23, IALVL , "000"                          );
    PLACE(pachIMHDR+nOffset+ 26, ILOC  , "0000000000"                   );
    PLACE(pachIMHDR+nOffset+ 36, IMAG  , "1.0 "                         );
    PLACE(pachIMHDR+nOffset+ 40, UDIDL , "00000"                        );
    PLACE(pachIMHDR+nOffset+ 45, IXSHDL, "00000"                        );

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
    const char *pszTRE;

/* -------------------------------------------------------------------- */
/*      Get the location table position from within the RPFHDR TRE      */
/*      structure.                                                      */
/* -------------------------------------------------------------------- */
    pszTRE= NITFFindTRE( psFile->pachTRE, psFile->nTREBytes, "RPFHDR", NULL );
    if( pszTRE == NULL )
        return;

    memcpy( &nLocTableOffset, pszTRE + 44, 4 );
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
