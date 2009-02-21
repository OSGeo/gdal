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

static int NITFWriteBLOCKA( char *pachUDIDL, char *pachTRE, 
                            int *pnOffset, int nBytesAvailable,
                            char **papszOptions );
static int NITFWriteTREsFromOptions(
    char *pachUDIDL, char *pachTRE,
    int *pnOffset, int nBytesAvailable,
    char **papszOptions );

static int 
NITFCollectSegmentInfo( NITFFile *psFile, int nOffset, char *pszType,
                        int nHeaderLenSize, int nDataLenSize, 
                        int *pnNextData );

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
    GIntBig     currentPos;

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
    pachHeader = (char *) CPLMalloc(nHeaderLen);
    VSIFSeekL( fp, 0, SEEK_SET );
    VSIFReadL( pachHeader, 1, nHeaderLen, fp );

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
    }

/* -------------------------------------------------------------------- */
/*      Collect segment info for the types we care about.               */
/* -------------------------------------------------------------------- */
    nNextData = nHeaderLen;

    nOffset = nHeaderLenOffset + 6;

    nOffset = NITFCollectSegmentInfo( psFile, nOffset,"IM",6, 10, &nNextData );

    nOffset = NITFCollectSegmentInfo( psFile, nOffset, "GR", 4, 6, &nNextData);

    /* LA Called NUMX in NITF 2.1 */
    nOffset = NITFCollectSegmentInfo( psFile, nOffset, "LA", 4, 3, &nNextData);

    nOffset = NITFCollectSegmentInfo( psFile, nOffset, "TX", 4, 5, &nNextData);

    nOffset = NITFCollectSegmentInfo( psFile, nOffset, "DE", 4, 9, &nNextData);

    nOffset = NITFCollectSegmentInfo( psFile, nOffset, "RE", 4, 7, &nNextData);

/* -------------------------------------------------------------------- */
/*      Is there User Define Header Data? (TREs)                        */
/* -------------------------------------------------------------------- */
    psFile->nTREBytes = 
        atoi(NITFGetField( szTemp, pachHeader, nOffset, 5 ));
    nOffset += 5;

    if( psFile->nTREBytes > 0 )
    {
        nOffset += 3; /* UDHOFL */
        psFile->nTREBytes -= 3;
    }

    if( psFile->nTREBytes != 0 )
    {
        psFile->pachTRE = (char *) CPLMalloc(psFile->nTREBytes);
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

        nOffset += 5; /* XHDL */

        if( nXHDL != 0 )
        {
            nOffset += 3; /* XHDLOFL */
            nXHDL -= 3;
        }

        if( nXHDL != 0 )
        {
            psFile->pachTRE = (char *) 
                CPLRealloc( psFile->pachTRE, 
                            psFile->nTREBytes + nXHDL );
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
    CPLFree( psFile );
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
    FILE	*fp;
    char        *pachIMHDR;
    char        achHeader[5000];
    int         nHeaderUsed = 0;
    int         nOffset = 0, iBand, nIHSize, nNPPBH, nNPPBV;
    GIntBig     nImageSize;
    int         nNBPR, nNBPC;
    const char *pszIREP;
    const char *pszIC = CSLFetchNameValue(papszOptions,"IC");
    const char *pszCLevel;
    const char *pszOpt;
    int nHL, nNUMT = 0;
    int nUDIDLOffset;
    const char *pszVersion;

    if (nBands <= 0 || nBands > 9)
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

    pszOpt = CSLFetchNameValue( papszOptions, "NUMT" );
    if( pszOpt != NULL )
        nNUMT = atoi(pszOpt);
    
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
/*      Compute CLEVEL ("complexity" level).                            */
/*      See: http://164.214.2.51/ntb/baseline/docs/2500b/2500b_not2.pdf */
/*            page 96u                                                  */
/*            TBD: Set CLEVEL based on file size too                    */
/* -------------------------------------------------------------------- */
    pszCLevel = "03";
    if (nPixels >  2048 || nLines >  2048 )  pszCLevel = "05";
    if (nPixels >  8192 || nLines >  8192 )  pszCLevel = "06";
    if (nPixels > 65536 || nLines > 65536 )  pszCLevel = "07";

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

    PLACE (achHeader+  0, FDHR_FVER,    pszVersion                      );
    OVR( 2,achHeader+  9, CLEVEL,       pszCLevel                       );
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
    PLACE (achHeader+354, HL           ,"??????"                        );
    PLACE (achHeader+360, NUMI         ,"001"                           );
    PLACE (achHeader+363, LISH1        ,"??????"                        );
    PLACE (achHeader+369, LI1          ,CPLSPrintf("%010ud",(GUInt32) nImageSize)  );
    PLACE (achHeader+379, NUMS         ,"000"                           );
    PLACE (achHeader+382, NUMX         ,"000"                           );
    PLACE (achHeader+385, NUMT         ,CPLSPrintf("%03d",nNUMT)        );

    PLACE (achHeader+388, LTSHnLTn     ,""                              );

    nHL = 388 + (4+5) * nNUMT;

    PLACE (achHeader+nHL, NUMDES       ,"000"                           );
    nHL += 3;
    PLACE (achHeader+nHL, NUMRES       ,"000"                           );
    nHL += 3;
    PLACE (achHeader+nHL, UDHDL        ,"00000"                         );
    nHL += 5;
    PLACE (achHeader+nHL, XHDL         ,"00000"                         );
    nHL += 5;

    // update header length
    PLACE (achHeader+354, HL           ,CPLSPrintf("%06d",nHL)          );
    
    nHeaderUsed = nHL;

/* -------------------------------------------------------------------- */
/*      Prepare the image header.                                       */
/* -------------------------------------------------------------------- */
    pachIMHDR = achHeader + nHeaderUsed;

    PLACE (pachIMHDR+  0, IM           , "IM"                           );
    OVR(10,pachIMHDR+  2, IID1         , "Missing"                      );
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
    OVR( 2,pachIMHDR+nOffset+1, IC     , "NC"                           );

    if( pszIC[0] != 'N' )
    {
        OVR( 4,pachIMHDR+nOffset+3, COMRAT , "    "                     );
        nOffset += 4;
    }

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
        else if( EQUALN(pszIREP,"YCbCr",5) )
        {
            if( iBand == 0 )
                pszIREPBAND = "Y";
            else if( iBand == 1 )
                pszIREPBAND = "Cb";
            else if( iBand == 2 )
                pszIREPBAND = "Cr";
        }

        PLACE(pachIMHDR+nOffset+ 0, IREPBANDn, pszIREPBAND                 );
//      PLACE(pachIMHDR+nOffset+ 2, ISUBCATn, ""                           );
        PLACE(pachIMHDR+nOffset+ 8, IFCn  , "N"                            );
//      PLACE(pachIMHDR+nOffset+ 9, IMFLTn, ""                             );

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

    nUDIDLOffset = nOffset + 40;
    nOffset += 50;

/* -------------------------------------------------------------------- */
/*      Add BLOCKA TRE if requested.                                    */
/* -------------------------------------------------------------------- */
    if( CSLFetchNameValue(papszOptions,"BLOCKA_BLOCK_COUNT") != NULL )
    {
        NITFWriteBLOCKA( pachIMHDR + nUDIDLOffset, 
                         pachIMHDR + nOffset, 
                         &nOffset, 
                         sizeof(achHeader) - (pachIMHDR+nOffset-achHeader),
                         papszOptions );
    }

    if( CSLFetchNameValue(papszOptions,"TRE") != NULL )
    {
        NITFWriteTREsFromOptions( 
            pachIMHDR + nUDIDLOffset, 
            pachIMHDR + nOffset, 
            &nOffset, 
            sizeof(achHeader) - (pachIMHDR+nOffset-achHeader),
            papszOptions );
    }

/* -------------------------------------------------------------------- */
/*      Update the image header length in the file header.              */
/* -------------------------------------------------------------------- */
    nIHSize = nOffset;

    PLACE(achHeader+ 363, LISH1, CPLSPrintf("%06d",nIHSize)      );

    nHeaderUsed += nIHSize;

/* -------------------------------------------------------------------- */
/*      Update total file length, and write header info to file.        */
/* -------------------------------------------------------------------- */
    PLACE(achHeader+ 342, FL,
          CPLSPrintf( "%012d", (int) (nHeaderUsed + nImageSize) ) );

    VSIFWriteL( achHeader, 1, nIHSize+nHL, fp );

/* -------------------------------------------------------------------- */
/*      Grow file to full required size by writing one byte at the end. */
/* -------------------------------------------------------------------- */
    {
        const char *pszIC = CSLFetchNameValue(papszOptions,"IC");
    
        if( pszIC != NULL && (EQUAL(pszIC,"C8") || EQUAL(pszIC,"C3")) )
            /* don't extend file */;
        else
        {
            VSIFSeekL( fp, nImageSize-1, SEEK_CUR );
            achHeader[0] = '\0';
            VSIFWriteL( achHeader, 1, 1, fp );
        }
    }

    VSIFCloseL( fp );

    return TRUE;
}

/************************************************************************/
/*                            NITFWriteTRE()                            */
/************************************************************************/

static int NITFWriteTRE( char *pachUDIDL, 
                         char *pachTREInHeader, 
                         int  *pnOffset, int nBytesAvailable,
                         char *pszTREName, char *pabyTREData, int nTREDataSize )

{
    char szTemp[200];
    int  nOldOffset;

/* -------------------------------------------------------------------- */
/*      Try to allocate space in the header for for this BLOCKA         */
/*      TRE.                                                            */
/* -------------------------------------------------------------------- */
    if( nBytesAvailable < nTREDataSize+14 )
    {
        CPLError( CE_Warning, CPLE_AppDefined,
                  "%s TRE not written due to lack of header space.",
                  pszTREName );
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Update IXSHDL.                                                  */
/* -------------------------------------------------------------------- */
    nOldOffset = atoi(NITFGetField( szTemp, pachUDIDL, 5, 5 ));

    if( nOldOffset == 0 )
    {
        nOldOffset = 3;
        PLACE(pachUDIDL+10, IXSOFL, "000" );
        *pnOffset += 3;
    }
    sprintf( szTemp, "%05d", nOldOffset + 11 + nTREDataSize );
    PLACE( pachUDIDL + 5, IXSHDL, szTemp );

/* -------------------------------------------------------------------- */
/*      Create TRE prefix.                                              */
/* -------------------------------------------------------------------- */
    sprintf( pachTREInHeader + nOldOffset, "%-6s%05d", 
             pszTREName, nTREDataSize );
    memcpy( pachTREInHeader + nOldOffset + 11, 
            pabyTREData, nTREDataSize );

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
    char *pachUDIDL, char *pachTRE,
    int *pnOffset, int nBytesAvailable,
    char **papszOptions )    

{
    int bIgnoreBLOCKA = 
        CSLFetchNameValue(papszOptions,"BLOCKA_BLOCK_COUNT") != NULL;
    int iOption;

    if( papszOptions == NULL )
        return TRUE;

    for( iOption = 0; papszOptions[iOption] != NULL; iOption++ )
    {
        const char *pszEscapedContents;
        char *pszUnescapedContents;
        char *pszTREName;
        int  nContentLength;

        if( !EQUALN(papszOptions[iOption],"TRE=",4) )
            continue;

        if( EQUALN(papszOptions[iOption]+4,"BLOCKA=",7)
            && bIgnoreBLOCKA )
            continue;

        pszEscapedContents = CPLParseNameValue( papszOptions[iOption]+4, 
                                                &pszTREName );
        if (pszEscapedContents == NULL)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Could not parse creation options %s", papszOptions[iOption]+4);
            return FALSE;
        }

        pszUnescapedContents = 
            CPLUnescapeString( pszEscapedContents, &nContentLength,
                               CPLES_BackslashQuotable );

        if( !NITFWriteTRE( pachUDIDL, pachTRE,
                           pnOffset, nBytesAvailable, 
                           pszTREName, pszUnescapedContents, 
                           nContentLength ) )
            return FALSE;
        
        CPLFree( pszTREName );
        CPLFree( pszUnescapedContents );
        nBytesAvailable -= (nContentLength + 14);
    }

    return TRUE;
}

/************************************************************************/
/*                          NITFWriteBLOCKA()                           */
/************************************************************************/

static int NITFWriteBLOCKA( char *pachUDIDL, char *pachTRE,
                            int *pnOffset, int nBytesAvailable,
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
        char szBLOCKA[200];
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

            memset( szBLOCKA + iStart, ' ', iSize );
            memcpy( szBLOCKA + iStart + MAX(0,iSize-strlen(pszValue)), 
                    pszValue, MIN(iSize,strlen(pszValue)) );
        }

        // required field - semantics unknown. 
        memcpy( szBLOCKA + 118, "010.0", 5);

        if( !NITFWriteTRE( pachUDIDL, pachTRE, 
                           pnOffset, nBytesAvailable, 
                           "BLOCKA", szBLOCKA, 123 ) )
            return FALSE;

        nBytesAvailable -= (123 + 14);
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
    VSIFSeekL( psFile->fp, nOffset, SEEK_SET );
    VSIFReadL( szTemp, 1, 3, psFile->fp );
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
    
    VSIFReadL( pachSegDef, 1, nSegDefSize, psFile->fp );

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

    /* trim white space */
    while( nLength > 0 && pachHeader[nStart + nLength - 1] == ' ' )
        nLength--;

    memcpy( szWork, pachHeader + nStart, nLength );
    szWork[nLength] = '\0';

    *ppapszMetadata = CSLSetNameValue( *ppapszMetadata, pszName, szWork );
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
    { "L1", "LFC-1", "1:500K", "Low Flying Chart (TED #1)", "CADRG"},
    { "L2", "LFC-2", "1:500K", "Low Flying Chart (TED #2)", "CADRG"},
    { "L3", "LFC-3", "1:500K", "Low Flying Chart (TED #3)", "CADRG"},
    { "L4", "LFC-4", "1:500K", "Low Flying Chart (TED #4)", "CADRG"},
    { "L5", "LFC-5", "1:500K", "Low Flying Chart (TED #5)", "CADRG"},
    { "LN", "LN (Night)", "1:500K", "Low Flying Chart (Night) - Host Nation", "CADRG"},
    { "JG", "JOG", "1:250K", "Joint Operation Graphic", "CADRG"},
    { "JA", "JOG-A", "1:250K", "Joint Operation Graphic - Air", "CADRG"},
    { "JR", "JOG-R", "1:250K", "Joint Operation Graphic - Radar", "CADRG"},
    { "JO", "OPG", "1:250K", "Operational Planning Graphic", "CADRG"},
    { "VT", "VTAC", "1:250K", "VFR Terminal Area Chart", "CADRG"},
    { "F1", "TFC-1", "1:250K", "Transit Flying Chart (TED #1)", "CADRG"},
    { "F2", "TFC-2", "1:250K", "Transit Flying Chart (TED #2)", "CADRG"},
    { "F3", "TFC-3", "1:250K", "Transit Flying Chart (TED #3)", "CADRG"},
    { "F4", "TFC-4", "1:250K", "Transit Flying Chart (TED #4)", "CADRG"},
    { "F5", "TFC-5", "1:250K", "Transit Flying Chart (TED #5)", "CADRG"},
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
    { "M1", "MIM", "Various", "Military Installation Maps (TED #1)", "CADRG"},
    { "M2", "MIM", "Various", "Military Installation Maps (TED #2)", "CADRG"},
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
            if (i < strlen(pszFilename) - 3)
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

