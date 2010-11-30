/******************************************************************************
 * $Id$
 *
 * Project:  NITF Read/Write Library
 * Purpose:  Simple test mainline to dump info about NITF file. 
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
#include "cpl_string.h"
#include "cpl_multiproc.h"
#include "cpl_vsi.h"

#ifdef OGR_ENABLED
#include "ogr_api.h"
#endif

CPL_CVSID("$Id$");

static void DumpRPC( NITFImage *psImage, NITFRPC00BInfo *psRPC );
static void DumpMetadata( const char *, const char *, char ** );

#ifdef WIN32

/* Below a few internal functions from nitffile.c and nitfimage.c */
/* To be used by nitfdump.c, they should be CPL_DLL exported, but */
/* they are mostly internal beasts so we copy&paste them here... */

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


static GUInt16 NITFReadMSBGUInt16(VSILFILE* fp, int* pbSuccess)
{
    GUInt16 nVal;
    if (VSIFReadL(&nVal, 1, sizeof(nVal), fp) != sizeof(nVal))
    {
        *pbSuccess = FALSE;
        return 0;
    }
    CPL_MSBPTR16( &nVal );
    return nVal;
}

static GUInt32 NITFReadMSBGUInt32(VSILFILE* fp, int* pbSuccess)
{
    GUInt32 nVal;
    if (VSIFReadL(&nVal, 1, sizeof(nVal), fp) != sizeof(nVal))
    {
        *pbSuccess = FALSE;
        return 0;
    }
    CPL_MSBPTR32( &nVal );
    return nVal;
}

/************************************************************************/
/*                     NITFReadRPFLocationTable()                       */
/************************************************************************/

NITFLocation* NITFReadRPFLocationTable(VSILFILE* fp, int* pnLocCount)
{
    GUInt16 nLocSectionLength;
    GUInt32 nLocSectionOffset;
    GUInt16 iLoc;
    GUInt16 nLocCount;
    GUInt16 nLocRecordLength;
    GUInt32 nLocComponentAggregateLength;
    NITFLocation* pasLocations = NULL;
    int bSuccess;
    GUIntBig nCurOffset;

    if (fp == NULL || pnLocCount == NULL)
        return NULL;

    *pnLocCount = 0;

    nCurOffset = VSIFTellL(fp);

    bSuccess = TRUE;
    nLocSectionLength = NITFReadMSBGUInt16(fp, &bSuccess);
    nLocSectionOffset = NITFReadMSBGUInt32(fp, &bSuccess);
    if (nLocSectionOffset != 14)
    {
        CPLDebug("NITF", "Unusual location section offset : %d", nLocSectionOffset);
    }

    nLocCount = NITFReadMSBGUInt16(fp, &bSuccess);

    if (!bSuccess || nLocCount == 0)
    {
        return NULL;
    }

    nLocRecordLength = NITFReadMSBGUInt16(fp, &bSuccess);
    if (nLocRecordLength != 10)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Did not get expected record length : %d", nLocRecordLength);
        return NULL;
    }

    nLocComponentAggregateLength = NITFReadMSBGUInt32(fp, &bSuccess);

    VSIFSeekL(fp, nCurOffset + nLocSectionOffset, SEEK_SET);

    pasLocations = (NITFLocation *)  VSICalloc(sizeof(NITFLocation), nLocCount);
    if (pasLocations == NULL)
    {
        CPLError(CE_Failure, CPLE_OutOfMemory,
                 "Cannot allocate memory for location table");
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Process the locations.                                          */
/* -------------------------------------------------------------------- */
    for( iLoc = 0; iLoc < nLocCount; iLoc++ )
    {
        pasLocations[iLoc].nLocId = NITFReadMSBGUInt16(fp, &bSuccess);
        pasLocations[iLoc].nLocSize = NITFReadMSBGUInt32(fp, &bSuccess);
        pasLocations[iLoc].nLocOffset = NITFReadMSBGUInt32(fp, &bSuccess);
    }

    if (!bSuccess)
    {
        CPLFree(pasLocations);
        return NULL;
    }

    *pnLocCount = nLocCount;
    return pasLocations;
}

#endif

typedef struct
{
    const char* pszLocName;
    int         nLocID;
} LocationNameId;

static const LocationNameId asLocationTable[] =
{
    { "HeaderComponent", 128 },
    { "LocationComponent", 129 },
    { "CoverageSectionSubheader", 130 },
    { "CompressionSectionSubsection", 131 },
    { "CompressionLookupSubsection", 132 },
    { "CompressionParameterSubsection", 133 },
    { "ColorGrayscaleSectionSubheader", 134 },
    { "ColormapSubsection", 135 },
    { "ImageDescriptionSubheader", 136 },
    { "ImageDisplayParametersSubheader", 137 },
    { "MaskSubsection", 138 },
    { "ColorConverterSubsection", 139 },
    { "SpatialDataSubsection", 140 },
    { "AttributeSectionSubheader", 141 },
    { "AttributeSubsection", 142 },
    { "ExplicitArealCoverageTable", 143 },
    { "RelatedImagesSectionSubheader", 144 },
    { "RelatedImagesSubsection", 145 },
    { "ReplaceUpdateSectionSubheader", 146 },
    { "ReplaceUpdateTable", 147 },
    { "BoundaryRectangleSectionSubheader", 148 },
    { "BoundaryRectangleTable", 149 },
    { "FrameFileIndexSectionSubHeader", 150 },
    { "FrameFileIndexSubsection", 151 },
    { "ColorTableIndexSectionSubheader", 152 },
    { "ColorTableIndexRecord", 153 }
};

const char* GetLocationNameFromId(int nID)
{
    unsigned int i;
    for(i=0;i<sizeof(asLocationTable) / sizeof(asLocationTable[0]);i++)
    {
        if (asLocationTable[i].nLocID == nID)
            return asLocationTable[i].pszLocName;
    }
    return "(unknown)";
}

/************************************************************************/
/*                                main()                                */
/************************************************************************/

int main( int nArgc, char ** papszArgv )

{
    NITFFile	*psFile;
    int          iSegment, iFile;
    char         szTemp[100];
    int          bDisplayTRE = FALSE;
    int          bExtractSHP = FALSE, bExtractSHPInMem = FALSE;

    if( nArgc < 2 )
    {
        printf( "Usage: nitfdump [-tre] [-extractshp | -extractshpinmem] <nitf_filename>*\n" );
        exit( 1 );
    }
    
    for( iFile = 1; iFile < nArgc; iFile++ )
    {
        if ( EQUAL(papszArgv[iFile], "-tre") )
            bDisplayTRE = TRUE;
        else if ( EQUAL(papszArgv[iFile], "-extractshp") )
            bExtractSHP = TRUE;
        else if ( EQUAL(papszArgv[iFile], "-extractshpinmem") )
        {
            bExtractSHP = TRUE;
            bExtractSHPInMem = TRUE;
        }
    }

/* ==================================================================== */
/*      Loop over all files.                                            */
/* ==================================================================== */
    for( iFile = 1; iFile < nArgc; iFile++ )
    {
        int bHasFoundLocationTable = FALSE;

        if ( EQUAL(papszArgv[iFile], "-tre") )
            continue;
        if ( EQUAL(papszArgv[iFile], "-extractshp") )
            continue;
        if ( EQUAL(papszArgv[iFile], "-extractshpinmem") )
            continue;

/* -------------------------------------------------------------------- */
/*      Open the file.                                                  */
/* -------------------------------------------------------------------- */
        psFile = NITFOpen( papszArgv[iFile], FALSE );
        if( psFile == NULL )
            exit( 2 );

        printf( "Dump for %s\n", papszArgv[iFile] );

/* -------------------------------------------------------------------- */
/*      Dump first TRE list.                                            */
/* -------------------------------------------------------------------- */
        if( psFile->pachTRE != NULL )
        {
            int nTREBytes = psFile->nTREBytes;
            const char *pszTREData = psFile->pachTRE;


            printf( "File TREs:" );

            while( nTREBytes > 10 )
            {
                int nThisTRESize = atoi(NITFGetField(szTemp, pszTREData, 6, 5 ));
                if (nThisTRESize < 0 || nThisTRESize > nTREBytes - 11)
                {
                    NITFGetField(szTemp, pszTREData, 0, 6 );
                    printf(" Invalid size (%d) for TRE %s", nThisTRESize, szTemp);
                    break;
                }

                printf( " %6.6s(%d)", pszTREData, nThisTRESize );
                pszTREData += nThisTRESize + 11;
                nTREBytes -= (nThisTRESize + 11);
            }
            printf( "\n" );

            if (bDisplayTRE)
            {
                nTREBytes = psFile->nTREBytes;
                pszTREData = psFile->pachTRE;

                while( nTREBytes > 10 )
                {
                    char *pszEscaped;
                    int nThisTRESize = atoi(NITFGetField(szTemp, pszTREData, 6, 5 ));
                    if (nThisTRESize < 0 || nThisTRESize > nTREBytes - 11)
                    {
                        break;
                    }

                    pszEscaped = CPLEscapeString( pszTREData + 11, nThisTRESize,
                                                       CPLES_BackslashQuotable );
                    printf( "TRE '%6.6s' : %s\n", pszTREData, pszEscaped);
                    CPLFree(pszEscaped);

                    pszTREData += nThisTRESize + 11;
                    nTREBytes -= (nThisTRESize + 11);
                }
            }
        }

/* -------------------------------------------------------------------- */
/*      Dump Metadata                                                   */
/* -------------------------------------------------------------------- */
        DumpMetadata( "File Metadata:", "  ", psFile->papszMetadata );

/* -------------------------------------------------------------------- */
/*      Dump general info about segments.                               */
/* -------------------------------------------------------------------- */
        NITFCollectAttachments( psFile );
        NITFReconcileAttachments( psFile );

        for( iSegment = 0; iSegment < psFile->nSegmentCount; iSegment++ )
        {
            NITFSegmentInfo *psSegInfo = psFile->pasSegmentInfo + iSegment;

            printf( "Segment %d (Type=%s):\n", 
                    iSegment + 1, psSegInfo->szSegmentType );

            printf( "  HeaderStart=" CPL_FRMT_GUIB ", HeaderSize=%u, DataStart=" CPL_FRMT_GUIB ", DataSize=" CPL_FRMT_GUIB "\n",
                    psSegInfo->nSegmentHeaderStart,
                    psSegInfo->nSegmentHeaderSize, 
                    psSegInfo->nSegmentStart,
                    psSegInfo->nSegmentSize );
            printf( "  DLVL=%d, ALVL=%d, LOC=C%d,R%d, CCS=C%d,R%d\n",
                    psSegInfo->nDLVL, 
                    psSegInfo->nALVL, 
                    psSegInfo->nLOC_C, 
                    psSegInfo->nLOC_R,
                    psSegInfo->nCCS_C,
                    psSegInfo->nCCS_R );
            printf( "\n" );
        }

/* -------------------------------------------------------------------- */
/*      Report details of images.                                       */
/* -------------------------------------------------------------------- */
        for( iSegment = 0; iSegment < psFile->nSegmentCount; iSegment++ )
        {
            NITFSegmentInfo *psSegInfo = psFile->pasSegmentInfo + iSegment;
            NITFImage *psImage;
            NITFRPC00BInfo sRPCInfo;
            int iBand;
            char **papszMD;

            if( !EQUAL(psSegInfo->szSegmentType,"IM") )
                continue;
        
            psImage = NITFImageAccess( psFile, iSegment );
            if( psImage == NULL )
            {
                printf( "NITFAccessImage(%d) failed!\n", iSegment );
                continue;
            }

            printf( "Image Segment %d, %dPx%dLx%dB x %dbits:\n", 
                    iSegment + 1, psImage->nCols, psImage->nRows, 
                    psImage->nBands, psImage->nBitsPerSample );
            printf( "  PVTYPE=%s, IREP=%s, ICAT=%s, IMODE=%c, IC=%s, COMRAT=%s, ICORDS=%c\n", 
                    psImage->szPVType, psImage->szIREP, psImage->szICAT,
                    psImage->chIMODE, psImage->szIC, psImage->szCOMRAT,
                    psImage->chICORDS );
            if( psImage->chICORDS != ' ' )
            {
                printf( "  UL=(%.15g,%.15g), UR=(%.15g,%.15g) Center=%d\n  LL=(%.15g,%.15g), LR=(%.15g,%.15g)\n", 
                        psImage->dfULX, psImage->dfULY,
                        psImage->dfURX, psImage->dfURY,
                        psImage->bIsBoxCenterOfPixel,
                        psImage->dfLLX, psImage->dfLLY,
                        psImage->dfLRX, psImage->dfLRY );
            }

            printf( "  IDLVL=%d, IALVL=%d, ILOC R=%d,C=%d, IMAG=%s\n",
                    psImage->nIDLVL, psImage->nIALVL, 
                    psImage->nILOCRow, psImage->nILOCColumn, 
                    psImage->szIMAG );

            printf( "  %d x %d blocks of size %d x %d\n",
                    psImage->nBlocksPerRow, psImage->nBlocksPerColumn,
                    psImage->nBlockWidth, psImage->nBlockHeight );
        
            if( psImage->pachTRE != NULL )
            {
                int nTREBytes = psImage->nTREBytes;
                const char *pszTREData = psImage->pachTRE;
            
            
                printf( "  Image TREs:" );
            
                while( nTREBytes > 10 )
                {
                    int nThisTRESize = atoi(NITFGetField(szTemp, pszTREData, 6, 5 ));
                    if (nThisTRESize < 0 || nThisTRESize > nTREBytes - 11)
                    {
                        NITFGetField(szTemp, pszTREData, 0, 6 );
                        printf(" Invalid size (%d) for TRE %s", nThisTRESize, szTemp);
                        break;
                    }
                
                    printf( " %6.6s(%d)", pszTREData, nThisTRESize );
                    pszTREData += nThisTRESize + 11;
                    nTREBytes -= (nThisTRESize + 11);
                }
                printf( "\n" );

                if (bDisplayTRE)
                {
                    nTREBytes = psImage->nTREBytes;
                    pszTREData = psImage->pachTRE;
    
                    while( nTREBytes > 10 )
                    {
                        char *pszEscaped;
                        int nThisTRESize = atoi(NITFGetField(szTemp, pszTREData, 6, 5 ));
                        if (nThisTRESize < 0 || nThisTRESize > nTREBytes - 11)
                        {
                            break;
                        }
    
                        pszEscaped = CPLEscapeString( pszTREData + 11, nThisTRESize,
                                                        CPLES_BackslashQuotable );
                        printf( "  TRE '%6.6s' : %s\n", pszTREData, pszEscaped);
                        CPLFree(pszEscaped);
    
                        pszTREData += nThisTRESize + 11;
                        nTREBytes -= (nThisTRESize + 11);
                    }
                }
            }

            /* Report info from location table, if found.                  */
            if( psImage->nLocCount > 0 )
            {
                int i;
                bHasFoundLocationTable = TRUE;
                printf( "  Location Table\n" );
                for( i = 0; i < psImage->nLocCount; i++ )
                {
                    printf( "    LocName=%s, LocId=%d, Offset=%d, Size=%d\n", 
                            GetLocationNameFromId(psImage->pasLocations[i].nLocId),
                            psImage->pasLocations[i].nLocId,
                            psImage->pasLocations[i].nLocOffset,
                            psImage->pasLocations[i].nLocSize );
                }
                printf( "\n" );
            }

            if( strlen(psImage->pszComments) > 0 )
                printf( "  Comments:\n%s\n", psImage->pszComments );

            for( iBand = 0; iBand < psImage->nBands; iBand++ )
            {
                NITFBandInfo *psBandInfo = psImage->pasBandInfo + iBand;

                printf( "  Band %d: IREPBAND=%s, ISUBCAT=%s, %d LUT entries.\n",
                        iBand + 1, psBandInfo->szIREPBAND, psBandInfo->szISUBCAT,
                        psBandInfo->nSignificantLUTEntries );
            }

            if( NITFReadRPC00B( psImage, &sRPCInfo ) )
            {
                DumpRPC( psImage, &sRPCInfo );
            }

            papszMD = NITFReadUSE00A( psImage );
            if( papszMD != NULL )
            {
                DumpMetadata( "  USE00A TRE:", "    ", papszMD );
                CSLDestroy( papszMD );
            }

            papszMD = NITFReadBLOCKA( psImage );
            if( papszMD != NULL )
            {
                DumpMetadata( "  BLOCKA TRE:", "    ", papszMD );
                CSLDestroy( papszMD );
            }

            papszMD = NITFReadSTDIDC( psImage );
            if( papszMD != NULL )
            {
                DumpMetadata( "  STDIDC TRE:", "    ", papszMD );
                CSLDestroy( papszMD );
            }

            DumpMetadata( "  Image Metadata:", "    ", psImage->papszMetadata );
            printf("\n");
        }

/* ==================================================================== */
/*      Report details of graphic segments.                             */
/* ==================================================================== */
        for( iSegment = 0; iSegment < psFile->nSegmentCount; iSegment++ )
        {
            NITFSegmentInfo *psSegInfo = psFile->pasSegmentInfo + iSegment;
            char achSubheader[298];
            int  nSTYPEOffset;

            if( !EQUAL(psSegInfo->szSegmentType,"GR") 
                && !EQUAL(psSegInfo->szSegmentType,"SY") )
                continue;
        
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
            printf( "Graphic Segment %d, type=%2.2s, sfmt=%c, sid=%10.10s\n",
                    iSegment + 1, 
                    achSubheader + 0, 
                    achSubheader[nSTYPEOffset],
                    achSubheader + 2 );

            printf( "  sname=%20.20s\n", achSubheader + 12 );
            printf("\n");
        }

/* ==================================================================== */
/*      Report details of text segments.                                */
/* ==================================================================== */
        for( iSegment = 0; iSegment < psFile->nSegmentCount; iSegment++ )
        {
            char *pabyHeaderData;
            char *pabyTextData;

            NITFSegmentInfo *psSegment = psFile->pasSegmentInfo + iSegment;

            if( !EQUAL(psSegment->szSegmentType,"TX") )
                continue;

            printf( "Text Segment %d\n", iSegment + 1);

/* -------------------------------------------------------------------- */
/*      Load the text header                                            */
/* -------------------------------------------------------------------- */

            /* Allocate one extra byte for the NULL terminating character */
            pabyHeaderData = (char *) CPLCalloc(1,
                    (size_t) psSegment->nSegmentHeaderSize + 1);
            if (VSIFSeekL(psFile->fp, psSegment->nSegmentHeaderStart,
                        SEEK_SET) != 0 ||
                VSIFReadL(pabyHeaderData, 1, (size_t) psSegment->nSegmentHeaderSize,
                        psFile->fp) != psSegment->nSegmentHeaderSize)
            {
                CPLError( CE_Warning, CPLE_FileIO,
                        "Failed to read %d bytes of text header data at " CPL_FRMT_GUIB ".",
                        psSegment->nSegmentHeaderSize,
                        psSegment->nSegmentHeaderStart);
                CPLFree(pabyHeaderData);
                continue;
            }

            printf("  Header : %s\n", pabyHeaderData);
            CPLFree(pabyHeaderData);

/* -------------------------------------------------------------------- */
/*      Load the raw TEXT data itself.                                  */
/* -------------------------------------------------------------------- */


            /* Allocate one extra byte for the NULL terminating character */
            pabyTextData = (char *) CPLCalloc(1,(size_t)psSegment->nSegmentSize+1);
            if( VSIFSeekL( psFile->fp, psSegment->nSegmentStart, 
                        SEEK_SET ) != 0 
                || VSIFReadL( pabyTextData, 1, (size_t)psSegment->nSegmentSize, 
                            psFile->fp ) != psSegment->nSegmentSize )
            {
                CPLError( CE_Warning, CPLE_FileIO, 
                        "Failed to read " CPL_FRMT_GUIB " bytes of text data at " CPL_FRMT_GUIB ".", 
                        psSegment->nSegmentSize,
                        psSegment->nSegmentStart );
                CPLFree( pabyTextData );
                continue;
            }

            printf("  Data  : %s\n", pabyTextData);
            printf("\n");
            CPLFree( pabyTextData );

        }

/* -------------------------------------------------------------------- */
/*      Report details of DES.                                          */
/* -------------------------------------------------------------------- */
        for( iSegment = 0; iSegment < psFile->nSegmentCount; iSegment++ )
        {
            NITFSegmentInfo *psSegInfo = psFile->pasSegmentInfo + iSegment;
            NITFDES *psDES;
            int nOffset = 0;
            char szTREName[7];
            int nThisTRESize;
            int nRPFDESOffset = -1;

            if( !EQUAL(psSegInfo->szSegmentType,"DE") )
                continue;
        
            psDES = NITFDESAccess( psFile, iSegment );
            if( psDES == NULL )
            {
                printf( "NITFDESAccess(%d) failed!\n", iSegment );
                continue;
            }

            printf( "DE Segment %d:\n", iSegment + 1 );

            printf( "  Segment TREs:" );
            nOffset = 0;
            while (NITFDESGetTRE( psDES, nOffset, szTREName, NULL, &nThisTRESize))
            {
                printf( " %6.6s(%d)", szTREName, nThisTRESize );
                if (strcmp(szTREName, "RPFDES") == 0)
                    nRPFDESOffset = nOffset + 11;
                nOffset += 11 + nThisTRESize;
            }
            printf( "\n" );

            if (bDisplayTRE)
            {
                char* pabyTREData = NULL;
                nOffset = 0;
                while (NITFDESGetTRE( psDES, nOffset, szTREName, &pabyTREData, &nThisTRESize))
                {
                    char* pszEscaped = CPLEscapeString( pabyTREData, nThisTRESize,
                                                        CPLES_BackslashQuotable );
                    printf( "  TRE '%6.6s' : %s\n", szTREName, pszEscaped);
                    CPLFree(pszEscaped);

                    nOffset += 11 + nThisTRESize;

                    NITFDESFreeTREData(pabyTREData);
                }
            }

            /* Report info from location table, if found. */
            if( !bHasFoundLocationTable && nRPFDESOffset >= 0 )
            {
                int i;
                int nLocCount = 0;
                NITFLocation* pasLocations;

                VSIFSeekL(psFile->fp, psSegInfo->nSegmentStart + nRPFDESOffset, SEEK_SET);
                pasLocations = NITFReadRPFLocationTable(psFile->fp, &nLocCount);
                if (pasLocations)
                {
                    printf( "  Location Table\n" );
                    for( i = 0; i < nLocCount; i++ )
                    {
                        printf( "    LocName=%s, LocId=%d, Offset=%d, Size=%d\n", 
                                GetLocationNameFromId(pasLocations[i].nLocId),
                                pasLocations[i].nLocId,
                                pasLocations[i].nLocOffset,
                                pasLocations[i].nLocSize );
                    }

                    CPLFree(pasLocations);
                    printf( "\n" );
                }
            }

            DumpMetadata( "  DES Metadata:", "    ", psDES->papszMetadata );

            if ( bExtractSHP && CSLFetchNameValue(psDES->papszMetadata, "NITF_SHAPE_USE") != NULL )
            {
                char szFilename[32];
                char szRadix[32];
                if (bExtractSHPInMem)
                    sprintf(szRadix, "/vsimem/nitf_segment_%d", iSegment + 1);
                else
                    sprintf(szRadix, "nitf_segment_%d", iSegment + 1);

                if (NITFDESExtractShapefile(psDES, szRadix))
                {
#ifdef OGR_ENABLED
                    OGRDataSourceH hDS;
                    OGRRegisterAll();
                    sprintf(szFilename, "%s.SHP", szRadix);
                    hDS = OGROpen(szFilename, FALSE, NULL);
                    if (hDS)
                    {
                        int nGeom = 0;
                        OGRLayerH hLayer = OGR_DS_GetLayer(hDS, 0);
                        if (hLayer)
                        {
                            OGRFeatureH hFeat;
                            printf("\n");
                            while ( (hFeat = OGR_L_GetNextFeature(hLayer)) != NULL )
                            {
                                OGRGeometryH hGeom = OGR_F_GetGeometryRef(hFeat);
                                if (hGeom)
                                {
                                    char* pszWKT = NULL;
                                    OGR_G_ExportToWkt(hGeom, &pszWKT);
                                    if (pszWKT)
                                        printf("    Geometry %d : %s\n", nGeom ++, pszWKT);
                                    CPLFree(pszWKT);
                                }
                                OGR_F_Destroy(hFeat);
                            }
                        }
                        OGR_DS_Destroy(hDS);
                    }
#endif
                }

                if (bExtractSHPInMem)
                {
                    sprintf(szFilename, "%s.SHP", szRadix);
                    VSIUnlink(szFilename);
                    sprintf(szFilename, "%s.SHX", szRadix);
                    VSIUnlink(szFilename);
                    sprintf(szFilename, "%s.DBF", szRadix);
                    VSIUnlink(szFilename);
                }
            }
        }

/* -------------------------------------------------------------------- */
/*      Close.                                                          */
/* -------------------------------------------------------------------- */
        NITFClose( psFile );
    }
    
    CPLFinderClean();
    CPLCleanupTLS();
    VSICleanupFileManager();
#ifdef OGR_ENABLED
    OGRCleanupAll();
#endif

    exit( 0 );
}

/************************************************************************/
/*                            DumpMetadata()                            */
/************************************************************************/

static void DumpMetadata( const char *pszTitle, const char *pszPrefix, 
                          char ** papszMD )
{
    int i;

    if( papszMD == NULL )
        return;

    printf( "%s\n", pszTitle );

    for( i = 0; papszMD[i] != NULL; i++ )
        printf( "%s%s\n", pszPrefix, papszMD[i] );
}

/************************************************************************/
/*                              DumpRPC()                               */
/************************************************************************/

static void DumpRPC( NITFImage *psImage, NITFRPC00BInfo *psRPC )

{
    int  i;

    printf( "  RPC00B:\n" );
    printf( "    SUCCESS=%d\n", psRPC->SUCCESS );
    printf( "    ERR_BIAS=%.16g\n", psRPC->ERR_BIAS );
    printf( "    ERR_RAND=%.16g\n", psRPC->ERR_RAND );

    printf( "    LINE_OFF=%.16g\n", psRPC->LINE_OFF );
    printf( "    SAMP_OFF=%.16g\n", psRPC->SAMP_OFF );
    printf( "    LAT_OFF =%.16g\n", psRPC->LAT_OFF );
    printf( "    LONG_OFF=%.16g\n", psRPC->LONG_OFF );
    printf( "    HEIGHT_OFF=%.16g\n", psRPC->HEIGHT_OFF );

    printf( "    LINE_SCALE=%.16g\n", psRPC->LINE_SCALE );
    printf( "    SAMP_SCALE=%.16g\n", psRPC->SAMP_SCALE );
    printf( "    LAT_SCALE =%.16g\n", psRPC->LAT_SCALE );
    printf( "    LONG_SCALE=%.16g\n", psRPC->LONG_SCALE );
    printf( "    HEIGHT_SCALE=%.16g\n", psRPC->HEIGHT_SCALE );

    printf( "    LINE_NUM_COEFF = " );
    for( i=0; i < 20; i++ )
    {
        printf( "%.12g ", psRPC->LINE_NUM_COEFF[i] );

        if( i == 19 )
            printf( "\n" );
        else if( (i%5) == 4  )
            printf( "\n                     " );
    }
    
    printf( "    LINE_DEN_COEFF = " );
    for( i=0; i < 20; i++ )
    {
        printf( "%.12g ", psRPC->LINE_DEN_COEFF[i] );

        if( i == 19 )
            printf( "\n" );
        else if( (i%5) == 4  )
            printf( "\n                     " );
    }
    
    printf( "    SAMP_NUM_COEFF = " );
    for( i=0; i < 20; i++ )
    {
        printf( "%.12g ", psRPC->SAMP_NUM_COEFF[i] );

        if( i == 19 )
            printf( "\n" );
        else if( (i%5) == 4  )
            printf( "\n                     " );
    }
    
    printf( "    SAMP_DEN_COEFF = " );
    for( i=0; i < 20; i++ )
    {
        printf( "%.12g ", psRPC->SAMP_DEN_COEFF[i] );

        if( i == 19 )
            printf( "\n" );
        else if( (i%5) == 4  )
            printf( "\n                     " );
    }

/* -------------------------------------------------------------------- */
/*      Dump some known locations.                                      */
/* -------------------------------------------------------------------- */
    {
        double adfLong[] = { psImage->dfULX, psImage->dfURX, 
                             psImage->dfLLX, psImage->dfLRX, 
                             (psImage->dfULX + psImage->dfLRX) / 2,
                             (psImage->dfULX + psImage->dfLRX) / 2 };
        double adfLat[] = { psImage->dfULY, psImage->dfURY, 
                            psImage->dfLLY, psImage->dfLRY, 
                            (psImage->dfULY + psImage->dfLRY) / 2,
                            (psImage->dfULY + psImage->dfLRY) / 2 };
        double adfHeight[] = { 0.0, 0.0, 0.0, 0.0, 0.0, 300.0 };
        double dfPixel, dfLine;
        
        for( i = 0; i < sizeof(adfLong) / sizeof(double); i++ )
        {
            NITFRPCGeoToImage( psRPC, adfLong[i], adfLat[i], adfHeight[i], 
                               &dfPixel, &dfLine );
            
            printf( "    RPC Transform (%.12g,%.12g,%g) -> (%g,%g)\n", 
                    adfLong[i], adfLat[i], adfHeight[i], dfPixel, dfLine );
        }
    }
}
