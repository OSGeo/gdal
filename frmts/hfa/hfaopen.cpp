/******************************************************************************
 * $Id$
 *
 * Project:  Erdas Imagine (.img) Translator
 * Purpose:  Supporting functions for HFA (.img) ... main (C callable) API
 *           that is not dependent on GDAL (just CPL).
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Intergraph Corporation
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
 * hfaopen.cpp
 *
 * Supporting routines for reading Erdas Imagine (.imf) Heirarchical
 * File Architecture files.  This is intended to be a library independent
 * of the GDAL core, but dependent on the Common Portability Library.
 *
 * $Log$
 * Revision 1.34  2004/08/19 16:54:07  warmerda
 * added back in support for writing to GDAL_MetaData table
 *
 * Revision 1.33  2004/07/16 20:40:32  warmerda
 * Added a series of patches from Andreas Wimmer which:
 *  o Add lots of improved support for metadata.
 *  o Use USE_SPILL only, instead of SPILL_FILE extra creation option.
 *  o Added ability to control block sizes.
 *
 * Revision 1.32  2004/06/09 17:54:01  dron
 * Create spill files when the image becomes larger 2GB (instead of former 4GB).
 * New option SPILL_FILE=YES to force spill file generation.
 *
 * Revision 1.31  2004/02/25 18:54:03  warmerda
 * Improve the check to see if we are over 4GB to avoid ULONG_MAX
 * which may be more than 4GB on machines with large longs and to
 * account for extra auxilary data we need space for.
 *
 * Revision 1.30  2004/02/04 16:51:24  warmerda
 * We now need to hardcode the size of the #Bin_Function# since we don't know
 * the size of the BaseData in the HFASetMetadata() call.
 *
 * Revision 1.29  2003/05/21 15:35:05  warmerda
 * cleanup type conversion warnings
 *
 * Revision 1.28  2003/05/13 19:32:10  warmerda
 * support for reading and writing opacity provided by Diana Esch-Mosher
 *
 * Revision 1.27  2003/04/22 19:40:36  warmerda
 * fixed email address
 *
 * Revision 1.26  2003/04/04 16:10:24  dron
 * Use ULONG_MAX to determine maximum file size. Depends on <limits.h> now.
 *
 * Revision 1.25  2003/03/25 11:13:02  dron
 * Handle path properly in HFADelete().
 *
 * Revision 1.24  2003/03/18 21:05:31  dron
 * Added HFARemove() and HFADelete() functions.
 *
 * Revision 1.23  2003/03/13 14:38:00  warmerda
 * added USE_SPILL creation option to force use of spill file.
 *
 * Revision 1.22  2003/03/06 18:03:14  dron
 * Fixed problem with unknown field in .ige file.
 *
 * Revision 1.21  2003/03/03 15:07:55  dron
 * Improvements in multiband spill file creation.
 *
 * Revision 1.20  2003/02/27 11:13:46  dron
 * Fixed 64-bit integer constant representation for MSVC compiler.
 *
 * Revision 1.19  2003/02/23 15:50:20  dron
 * Spill file creation now really works for single band datasets.
 *
 * Revision 1.18  2003/02/22 07:23:02  dron
 * Fixes i spill file writing.
 *
 * Revision 1.17  2003/02/21 15:40:58  dron
 * Added support for writing large (>4 GB) Erdas Imagine files.
 *
 * Revision 1.16  2002/05/21 15:09:12  warmerda
 * read/write support for GDAL_MetaData table now supported
 *
 * Revision 1.15  2002/04/12 20:19:49  warmerda
 * improved debug info
 *
 * Revision 1.14  2001/07/18 04:51:57  warmerda
 * added CPL_CVSID
 *
 * Revision 1.13  2001/01/08 14:17:27  warmerda
 * Use nDictMax for CPLMalloc().
 *
 * Revision 1.12  2001/01/03 16:20:10  warmerda
 * Converted to large file API
 *
 * Revision 1.11  2000/10/31 18:02:32  warmerda
 * Added external and unnamed overview support
 *
 * Revision 1.10  2000/10/20 04:18:15  warmerda
 * added overviews, stateplane, and u4
 *
 * Revision 1.9  2000/10/13 20:59:05  warmerda
 * fixed writing of RasterDMS dictionary types
 *
 * Revision 1.8  2000/10/12 20:23:58  warmerda
 * Fixed dictinary writing.
 *
 * Revision 1.7  2000/10/12 20:04:59  warmerda
 * split up long dictionary string
 *
 * Revision 1.6  2000/10/12 19:30:32  warmerda
 * substantially improved write support
 *
 * Revision 1.5  2000/09/29 21:42:38  warmerda
 * preliminary write support implemented
 *
 * Revision 1.4  1999/01/28 16:25:19  warmerda
 * Added implementation of HFAStandard().
 *
 * Revision 1.3  1999/01/22 17:38:47  warmerda
 * lots of additions
 *
 * Revision 1.2  1999/01/04 22:52:47  warmerda
 * field access working
 *
 * Revision 1.1  1999/01/04 05:28:13  warmerda
 * New
 *
 */

#include "hfa_p.h"
#include "cpl_conv.h"
#include <limits.h>

CPL_CVSID("$Id$");


static char *apszAuxMetadataItems[] = {

// node/entry            field_name                  metadata_key       type

 "Statistics",           "dminimum",              "STATISTICS_MINIMUM", "Esta_Statistics",
 "Statistics",           "dmaximum",              "STATISTICS_MAXIMUM", "Esta_Statistics",
 "Statistics",           "dmean",                 "STATISTICS_MEAN",    "Esta_Statistics",
 "Statistics",           "dmedian",               "STATISTICS_MEDIAN",  "Esta_Statistics",
 "Statistics",           "dmode",                 "STATISTICS_MODE",    "Esta_Statistics",
 "Statistics",           "dstddev",               "STATISTICS_STDDEV",  "Esta_Statistics",
 "HistogramParameters",  "lBinFunction.numBins",  "STATISTICS_HISTONUMBINS",    "Eimg_StatisticsParameters830",
 "HistogramParameters",  "dBinFunction.minLimit", "STATISTICS_HISTOMIN",        "Eimg_StatisticsParameters830",
 "HistogramParameters",  "dBinFunction.maxLimit", "STATISTICS_HISTOMAX",        "Eimg_StatisticsParameters830",
 NULL
};


char ** GetHFAAuxMetaDataList()
{
    return apszAuxMetadataItems;
}


/************************************************************************/
/*                          HFAGetDictionary()                          */
/************************************************************************/

static char * HFAGetDictionary( HFAHandle hHFA )

{
    int		nDictMax = 100;
    char	*pszDictionary = (char *) CPLMalloc(nDictMax);
    int		nDictSize = 0;

    VSIFSeekL( hHFA->fp, hHFA->nDictionaryPos, SEEK_SET );

    while( TRUE )
    {
        if( nDictSize >= nDictMax-1 )
        {
            nDictMax = nDictSize * 2 + 100;
            pszDictionary = (char *) CPLRealloc(pszDictionary, nDictMax );
        }

        if( VSIFReadL( pszDictionary + nDictSize, 1, 1, hHFA->fp ) < 1
            || pszDictionary[nDictSize] == '\0'
            || (nDictSize > 2 && pszDictionary[nDictSize-2] == ','
                && pszDictionary[nDictSize-1] == '.') )
            break;

        nDictSize++;
    }

    pszDictionary[nDictSize] = '\0';


    return( pszDictionary );
}

/************************************************************************/
/*                              HFAOpen()                               */
/************************************************************************/

HFAHandle HFAOpen( const char * pszFilename, const char * pszAccess )

{
    FILE	*fp;
    char	szHeader[16];
    HFAInfo_t	*psInfo;
    GUInt32	nHeaderPos;

/* -------------------------------------------------------------------- */
/*      Open the file.                                                  */
/* -------------------------------------------------------------------- */
    if( EQUAL(pszAccess,"r") || EQUAL(pszAccess,"rb" ) )
        fp = VSIFOpenL( pszFilename, "rb" );
    else
        fp = VSIFOpenL( pszFilename, "r+b" );

    /* should this be changed to use some sort of CPLFOpen() which will
       set the error? */
    if( fp == NULL )
    {
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "File open of %s failed.",
                  pszFilename );

        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Read and verify the header.                                     */
/* -------------------------------------------------------------------- */
    if( VSIFReadL( szHeader, 16, 1, fp ) < 1 )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Attempt to read 16 byte header failed for\n%s.",
                  pszFilename );

        return NULL;
    }

    if( !EQUALN(szHeader,"EHFA_HEADER_TAG",15) )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "File %s is not an Imagine HFA file ... header wrong.",
                  pszFilename );

        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Create the HFAInfo_t                                            */
/* -------------------------------------------------------------------- */
    psInfo = (HFAInfo_t *) CPLCalloc(sizeof(HFAInfo_t),1);

    psInfo->pszFilename = CPLStrdup(CPLGetFilename(pszFilename));
    psInfo->pszPath = CPLStrdup(CPLGetPath(pszFilename));
    psInfo->fp = fp;
    if( EQUAL(pszAccess,"r") || EQUAL(pszAccess,"rb" ) )
	psInfo->eAccess = HFA_ReadOnly;
    else
	psInfo->eAccess = HFA_Update;
    psInfo->bTreeDirty = FALSE;

/* -------------------------------------------------------------------- */
/*	Where is the header?						*/
/* -------------------------------------------------------------------- */
    VSIFReadL( &nHeaderPos, sizeof(GInt32), 1, fp );
    HFAStandard( 4, &nHeaderPos );

/* -------------------------------------------------------------------- */
/*      Read the header.                                                */
/* -------------------------------------------------------------------- */
    VSIFSeekL( fp, nHeaderPos, SEEK_SET );

    VSIFReadL( &(psInfo->nVersion), sizeof(GInt32), 1, fp );
    HFAStandard( 4, &(psInfo->nVersion) );

    VSIFReadL( szHeader, 4, 1, fp ); /* skip freeList */

    VSIFReadL( &(psInfo->nRootPos), sizeof(GInt32), 1, fp );
    HFAStandard( 4, &(psInfo->nRootPos) );

    VSIFReadL( &(psInfo->nEntryHeaderLength), sizeof(GInt16), 1, fp );
    HFAStandard( 2, &(psInfo->nEntryHeaderLength) );

    VSIFReadL( &(psInfo->nDictionaryPos), sizeof(GInt32), 1, fp );
    HFAStandard( 4, &(psInfo->nDictionaryPos) );

/* -------------------------------------------------------------------- */
/*      Collect file size.                                              */
/* -------------------------------------------------------------------- */
    VSIFSeekL( fp, 0, SEEK_END );
    psInfo->nEndOfFile = (GUInt32) VSIFTellL( fp );

/* -------------------------------------------------------------------- */
/*      Instantiate the root entry.                                     */
/* -------------------------------------------------------------------- */
    psInfo->poRoot = new HFAEntry( psInfo, psInfo->nRootPos, NULL, NULL );

/* -------------------------------------------------------------------- */
/*      Read the dictionary                                             */
/* -------------------------------------------------------------------- */
    psInfo->pszDictionary = HFAGetDictionary( psInfo );
    psInfo->poDictionary = new HFADictionary( psInfo->pszDictionary );

/* -------------------------------------------------------------------- */
/*      Collect band definitions.                                       */
/* -------------------------------------------------------------------- */
    HFAParseBandInfo( psInfo );

    return psInfo;
}

/************************************************************************/
/*                          HFAGetDependent()                           */
/************************************************************************/

HFAInfo_t *HFAGetDependent( HFAInfo_t *psBase, const char *pszFilename )

{
    if( EQUAL(pszFilename,psBase->pszFilename) )
        return psBase;

    if( psBase->psDependent != NULL )
    {
        if( EQUAL(pszFilename,psBase->psDependent->pszFilename) )
            return psBase->psDependent;
        else
            return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Try to open the dependent file.                                 */
/* -------------------------------------------------------------------- */
    char	*pszDependent;
    FILE	*fp;

    pszDependent = CPLStrdup(
        CPLFormFilename( psBase->pszPath, pszFilename, NULL ) );

    fp = VSIFOpenL( pszDependent, "rb" );
    if( fp != NULL )
    {
        VSIFCloseL( fp );
        psBase->psDependent = HFAOpen( pszDependent, "rb" );
    }

    CPLFree( pszDependent );

    return psBase->psDependent;
}


/************************************************************************/
/*                          HFAParseBandInfo()                          */
/*                                                                      */
/*      This is used by HFAOpen() and HFACreate() to initialize the     */
/*      band structures.                                                */
/************************************************************************/

CPLErr HFAParseBandInfo( HFAInfo_t *psInfo )

{
    HFAEntry	*poNode;

/* -------------------------------------------------------------------- */
/*      Find the first band node.                                       */
/* -------------------------------------------------------------------- */
    psInfo->nBands = 0;
    poNode = psInfo->poRoot->GetChild();
    while( poNode != NULL )
    {
        if( EQUAL(poNode->GetType(),"Eimg_Layer")
            && poNode->GetIntField("width") > 0
            && poNode->GetIntField("height") > 0 )
        {
            if( psInfo->nBands == 0 )
            {
                psInfo->nXSize = poNode->GetIntField("width");
                psInfo->nYSize = poNode->GetIntField("height");
            }
            else if( poNode->GetIntField("width") != psInfo->nXSize
                     || poNode->GetIntField("height") != psInfo->nYSize )
            {
                CPLAssert( FALSE );
                return CE_Failure;
            }

            psInfo->papoBand = (HFABand **)
                CPLRealloc(psInfo->papoBand,
                           sizeof(HFABand *) * (psInfo->nBands+1));
            psInfo->papoBand[psInfo->nBands] = new HFABand( psInfo, poNode );
            psInfo->nBands++;
        }

        poNode = poNode->GetNext();
    }

    return CE_None;
}

/************************************************************************/
/*                              HFAClose()                              */
/************************************************************************/

void HFAClose( HFAHandle hHFA )

{
    int		i;

    if( hHFA->bTreeDirty )
        HFAFlush( hHFA );

    if( hHFA->psDependent != NULL )
        HFAClose( hHFA->psDependent );

    delete hHFA->poRoot;

    VSIFCloseL( hHFA->fp );

    if( hHFA->poDictionary != NULL )
        delete hHFA->poDictionary;

    CPLFree( hHFA->pszDictionary );
    CPLFree( hHFA->pszFilename );
    CPLFree( hHFA->pszPath );

    for( i = 0; i < hHFA->nBands; i++ )
    {
        delete hHFA->papoBand[i];
    }

    CPLFree( hHFA->papoBand );

    if( hHFA->pProParameters != NULL )
    {
        Eprj_ProParameters *psProParms = (Eprj_ProParameters *)
            hHFA->pProParameters;

        CPLFree( psProParms->proExeName );
        CPLFree( psProParms->proName );
        CPLFree( psProParms->proSpheroid.sphereName );

        CPLFree( psProParms );
    }

    if( hHFA->pDatum != NULL )
    {
        CPLFree( ((Eprj_Datum *) hHFA->pDatum)->datumname );
        CPLFree( ((Eprj_Datum *) hHFA->pDatum)->gridname );
        CPLFree( hHFA->pDatum );
    }

    if( hHFA->pMapInfo != NULL )
    {
        CPLFree( ((Eprj_MapInfo *) hHFA->pMapInfo)->proName );
        CPLFree( ((Eprj_MapInfo *) hHFA->pMapInfo)->units );
        CPLFree( hHFA->pMapInfo );
    }

    CPLFree( hHFA );
}

/************************************************************************/
/*                              HFARemove()                             */
/*  Used from HFADelete() function.                                     */
/************************************************************************/

CPLErr HFARemove( const char *pszFilename )

{
    VSIStatBuf      sStat;

    if( VSIStat( pszFilename, &sStat ) == 0 && VSI_ISREG( sStat.st_mode ) )
    {
        if( VSIUnlink( pszFilename ) == 0 )
            return CE_None;
        else
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Attempt to unlink %s failed.\n", pszFilename );
            return CE_Failure;
        }
    }
    else
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Unable to delete %s, not a file.\n", pszFilename );
        return CE_Failure;
    }
}

/************************************************************************/
/*                              HFADelete()                             */
/************************************************************************/

CPLErr HFADelete( const char *pszFilename )

{
    char	szName[128];
    HFAInfo_t   *psInfo = HFAOpen( pszFilename, "rb" );
    HFAEntry	*poDMS;


    sprintf( szName, "Layer_%d", 1 );
    poDMS = psInfo->poRoot->GetNamedChild( szName )->
        GetNamedChild( "ExternalRasterDMS" );

    if ( poDMS )
    {
        const char *pszRawFilename =
            poDMS->GetStringField( "fileName.string" );
        
        HFARemove( CPLFormFilename( psInfo->pszPath, pszRawFilename, NULL ) );
    }

    HFAClose( psInfo );
    return HFARemove( pszFilename );
}

/************************************************************************/
/*                          HFAGetRasterInfo()                          */
/************************************************************************/

CPLErr HFAGetRasterInfo( HFAHandle hHFA, int * pnXSize, int * pnYSize,
                         int * pnBands )

{
    if( pnXSize != NULL )
        *pnXSize = hHFA->nXSize;
    if( pnYSize != NULL )
        *pnYSize = hHFA->nYSize;
    if( pnBands != NULL )
        *pnBands = hHFA->nBands;

    return CE_None;
}

/************************************************************************/
/*                           HFAGetBandInfo()                           */
/************************************************************************/

CPLErr HFAGetBandInfo( HFAHandle hHFA, int nBand, int * pnDataType,
                       int * pnBlockXSize, int * pnBlockYSize,
                       int * pnOverviews )

{
    if( nBand < 0 || nBand > hHFA->nBands )
    {
        CPLAssert( FALSE );
        return CE_Failure;
    }

    if( pnDataType != NULL )
        *pnDataType = hHFA->papoBand[nBand-1]->nDataType;

    if( pnBlockXSize != NULL )
        *pnBlockXSize = hHFA->papoBand[nBand-1]->nBlockXSize;

    if( pnBlockYSize != NULL )
        *pnBlockYSize = hHFA->papoBand[nBand-1]->nBlockYSize;

    if( pnOverviews != NULL )
        *pnOverviews = hHFA->papoBand[nBand-1]->nOverviews;

    return( CE_None );
}

/************************************************************************/
/*                         HFAGetOverviewInfo()                         */
/************************************************************************/

CPLErr HFAGetOverviewInfo( HFAHandle hHFA, int nBand, int iOverview,
                           int * pnXSize, int * pnYSize,
                           int * pnBlockXSize, int * pnBlockYSize )

{
    HFABand	*poBand;

    if( nBand < 0 || nBand > hHFA->nBands )
    {
        CPLAssert( FALSE );
        return CE_Failure;
    }

    poBand = hHFA->papoBand[nBand-1];

    if( iOverview < 0 || iOverview >= poBand->nOverviews )
    {
        CPLAssert( FALSE );
        return CE_Failure;
    }
    poBand = poBand->papoOverviews[iOverview];

    if( pnXSize != NULL )
        *pnXSize = poBand->nWidth;

    if( pnYSize != NULL )
        *pnYSize = poBand->nHeight;

    if( pnBlockXSize != NULL )
        *pnBlockXSize = poBand->nBlockXSize;

    if( pnBlockYSize != NULL )
        *pnBlockYSize = poBand->nBlockYSize;

    return( CE_None );
}

/************************************************************************/
/*                         HFAGetRasterBlock()                          */
/************************************************************************/

CPLErr HFAGetRasterBlock( HFAHandle hHFA, int nBand,
                          int nXBlock, int nYBlock, void * pData )

{
    if( nBand < 1 || nBand > hHFA->nBands )
        return CE_Failure;

    return( hHFA->papoBand[nBand-1]->GetRasterBlock(nXBlock,nYBlock,pData) );
}

/************************************************************************/
/*                     HFAGetOverviewRasterBlock()                      */
/************************************************************************/

CPLErr HFAGetOverviewRasterBlock( HFAHandle hHFA, int nBand, int iOverview,
                                  int nXBlock, int nYBlock, void * pData )

{
    if( nBand < 1 || nBand > hHFA->nBands )
        return CE_Failure;

    if( iOverview < 0 || iOverview >= hHFA->papoBand[nBand-1]->nOverviews )
        return CE_Failure;

    return( hHFA->papoBand[nBand-1]->papoOverviews[iOverview]->
            GetRasterBlock(nXBlock,nYBlock,pData) );
}

/************************************************************************/
/*                         HFASetRasterBlock()                          */
/************************************************************************/

CPLErr HFASetRasterBlock( HFAHandle hHFA, int nBand,
                          int nXBlock, int nYBlock, void * pData )

{
    if( nBand < 1 || nBand > hHFA->nBands )
        return CE_Failure;

    return( hHFA->papoBand[nBand-1]->SetRasterBlock(nXBlock,nYBlock,pData) );
}

/************************************************************************/
/*                         HFAGetDataTypeBits()                         */
/************************************************************************/

int HFAGetDataTypeBits( int nDataType )

{
    switch( nDataType )
    {
      case EPT_u1:
        return 1;

      case EPT_u2:
        return 2;

      case EPT_u4:
        return 4;

      case EPT_u8:
      case EPT_s8:
        return 8;

      case EPT_u16:
      case EPT_s16:
        return 16;

      case EPT_u32:
      case EPT_s32:
      case EPT_f32:
        return 32;

      case EPT_f64:
      case EPT_c64:
        return 64;

      case EPT_c128:
        return 128;
    }

    return 0;
}

/************************************************************************/
/*                           HFAGetMapInfo()                            */
/************************************************************************/

const Eprj_MapInfo *HFAGetMapInfo( HFAHandle hHFA )

{
    HFAEntry	*poMIEntry;
    Eprj_MapInfo *psMapInfo;

    if( hHFA->nBands < 1 )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Do we already have it?                                          */
/* -------------------------------------------------------------------- */
    if( hHFA->pMapInfo != NULL )
        return( (Eprj_MapInfo *) hHFA->pMapInfo );

/* -------------------------------------------------------------------- */
/*      Get the HFA node.                                               */
/* -------------------------------------------------------------------- */
    poMIEntry = hHFA->papoBand[0]->poNode->GetNamedChild( "Map_Info" );
    if( poMIEntry == NULL )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Allocate the structure.                                         */
/* -------------------------------------------------------------------- */
    psMapInfo = (Eprj_MapInfo *) CPLCalloc(sizeof(Eprj_MapInfo),1);

/* -------------------------------------------------------------------- */
/*      Fetch the fields.                                               */
/* -------------------------------------------------------------------- */
    psMapInfo->proName = CPLStrdup(poMIEntry->GetStringField("proName"));

    psMapInfo->upperLeftCenter.x =
        poMIEntry->GetDoubleField("upperLeftCenter.x");
    psMapInfo->upperLeftCenter.y =
        poMIEntry->GetDoubleField("upperLeftCenter.y");

    psMapInfo->lowerRightCenter.x =
        poMIEntry->GetDoubleField("lowerRightCenter.x");
    psMapInfo->lowerRightCenter.y =
        poMIEntry->GetDoubleField("lowerRightCenter.y");

   psMapInfo->pixelSize.width =
        poMIEntry->GetDoubleField("pixelSize.width");
   psMapInfo->pixelSize.height =
        poMIEntry->GetDoubleField("pixelSize.height");

   psMapInfo->units = CPLStrdup(poMIEntry->GetStringField("units"));

   hHFA->pMapInfo = (void *) psMapInfo;

   return psMapInfo;
}

/************************************************************************/
/*                           HFASetMapInfo()                            */
/************************************************************************/

CPLErr HFASetMapInfo( HFAHandle hHFA, const Eprj_MapInfo *poMapInfo )

{
/* -------------------------------------------------------------------- */
/*      Loop over bands, setting information on each one.               */
/* -------------------------------------------------------------------- */
    for( int iBand = 0; iBand < hHFA->nBands; iBand++ )
    {
        HFAEntry	*poMIEntry;

/* -------------------------------------------------------------------- */
/*      Create a new Map_Info if there isn't one present already.       */
/* -------------------------------------------------------------------- */
        poMIEntry = hHFA->papoBand[iBand]->poNode->GetNamedChild( "Map_Info" );
        if( poMIEntry == NULL )
        {
            poMIEntry = new HFAEntry( hHFA, "Map_Info", "Eprj_MapInfo",
                                      hHFA->papoBand[iBand]->poNode );
        }

        poMIEntry->MarkDirty();

/* -------------------------------------------------------------------- */
/*      Ensure we have enough space for all the data.                   */
/* -------------------------------------------------------------------- */
        int	nSize;
        GByte   *pabyData;

        nSize = 48 + 40
            + strlen(poMapInfo->proName) + 1
            + strlen(poMapInfo->units) + 1;

        pabyData = poMIEntry->MakeData( nSize );
        poMIEntry->SetPosition();

/* -------------------------------------------------------------------- */
/*      Write the various fields.                                       */
/* -------------------------------------------------------------------- */
        poMIEntry->SetStringField( "proName", poMapInfo->proName );

        poMIEntry->SetDoubleField( "upperLeftCenter.x",
                                   poMapInfo->upperLeftCenter.x );
        poMIEntry->SetDoubleField( "upperLeftCenter.y",
                                   poMapInfo->upperLeftCenter.y );

        poMIEntry->SetDoubleField( "lowerRightCenter.x",
                                   poMapInfo->lowerRightCenter.x );
        poMIEntry->SetDoubleField( "lowerRightCenter.y",
                                   poMapInfo->lowerRightCenter.y );

        poMIEntry->SetDoubleField( "pixelSize.width",
                                   poMapInfo->pixelSize.width );
        poMIEntry->SetDoubleField( "pixelSize.height",
                                   poMapInfo->pixelSize.height );

        poMIEntry->SetStringField( "units", poMapInfo->units );
    }

    return CE_None;
}

/************************************************************************/
/*                        HFAGetProParameters()                         */
/************************************************************************/

const Eprj_ProParameters *HFAGetProParameters( HFAHandle hHFA )

{
    HFAEntry	*poMIEntry;
    Eprj_ProParameters *psProParms;
    int		i;

    if( hHFA->nBands < 1 )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Do we already have it?                                          */
/* -------------------------------------------------------------------- */
    if( hHFA->pProParameters != NULL )
        return( (Eprj_ProParameters *) hHFA->pProParameters );

/* -------------------------------------------------------------------- */
/*      Get the HFA node.                                               */
/* -------------------------------------------------------------------- */
    poMIEntry = hHFA->papoBand[0]->poNode->GetNamedChild( "Projection" );
    if( poMIEntry == NULL )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Allocate the structure.                                         */
/* -------------------------------------------------------------------- */
    psProParms = (Eprj_ProParameters *)CPLCalloc(sizeof(Eprj_ProParameters),1);

/* -------------------------------------------------------------------- */
/*      Fetch the fields.                                               */
/* -------------------------------------------------------------------- */
    psProParms->proType = (Eprj_ProType) poMIEntry->GetIntField("proType");
    psProParms->proNumber = poMIEntry->GetIntField("proNumber");
    psProParms->proExeName =CPLStrdup(poMIEntry->GetStringField("proExeName"));
    psProParms->proName = CPLStrdup(poMIEntry->GetStringField("proName"));
    psProParms->proZone = poMIEntry->GetIntField("proZone");

    for( i = 0; i < 15; i++ )
    {
        char	szFieldName[30];

        sprintf( szFieldName, "proParams[%d]", i );
        psProParms->proParams[i] = poMIEntry->GetDoubleField(szFieldName);
    }

    psProParms->proSpheroid.sphereName =
        CPLStrdup(poMIEntry->GetStringField("proSpheroid.sphereName"));
    psProParms->proSpheroid.a = poMIEntry->GetDoubleField("proSpheroid.a");
    psProParms->proSpheroid.b = poMIEntry->GetDoubleField("proSpheroid.b");
    psProParms->proSpheroid.eSquared =
        poMIEntry->GetDoubleField("proSpheroid.eSquared");
    psProParms->proSpheroid.radius =
        poMIEntry->GetDoubleField("proSpheroid.radius");

    hHFA->pProParameters = (void *) psProParms;

    return psProParms;
}

/************************************************************************/
/*                        HFASetProParameters()                         */
/************************************************************************/

CPLErr HFASetProParameters( HFAHandle hHFA, const Eprj_ProParameters *poPro )

{
/* -------------------------------------------------------------------- */
/*      Loop over bands, setting information on each one.               */
/* -------------------------------------------------------------------- */
    for( int iBand = 0; iBand < hHFA->nBands; iBand++ )
    {
        HFAEntry	*poMIEntry;

/* -------------------------------------------------------------------- */
/*      Create a new Projection if there isn't one present already.     */
/* -------------------------------------------------------------------- */
        poMIEntry = hHFA->papoBand[iBand]->poNode->GetNamedChild("Projection");
        if( poMIEntry == NULL )
        {
            poMIEntry = new HFAEntry( hHFA, "Projection","Eprj_ProParameters",
                                      hHFA->papoBand[iBand]->poNode );
        }

        poMIEntry->MarkDirty();

/* -------------------------------------------------------------------- */
/*      Ensure we have enough space for all the data.                   */
/* -------------------------------------------------------------------- */
        int	nSize;
        GByte   *pabyData;

        nSize = 34 + 15 * 8
            + 8 + strlen(poPro->proName) + 1
            + 32 + 8 + strlen(poPro->proSpheroid.sphereName) + 1;

        if( poPro->proExeName != NULL )
            nSize += strlen(poPro->proExeName) + 1;

        pabyData = poMIEntry->MakeData( nSize );
        poMIEntry->SetPosition();

/* -------------------------------------------------------------------- */
/*      Write the various fields.                                       */
/* -------------------------------------------------------------------- */
        poMIEntry->SetIntField( "proType", poPro->proType );

        poMIEntry->SetIntField( "proNumber", poPro->proNumber );

        poMIEntry->SetStringField( "proExeName", poPro->proExeName );
        poMIEntry->SetStringField( "proName", poPro->proName );
        poMIEntry->SetIntField( "proZone", poPro->proZone );
        poMIEntry->SetDoubleField( "proParams[0]", poPro->proParams[0] );
        poMIEntry->SetDoubleField( "proParams[1]", poPro->proParams[1] );
        poMIEntry->SetDoubleField( "proParams[2]", poPro->proParams[2] );
        poMIEntry->SetDoubleField( "proParams[3]", poPro->proParams[3] );
        poMIEntry->SetDoubleField( "proParams[4]", poPro->proParams[4] );
        poMIEntry->SetDoubleField( "proParams[5]", poPro->proParams[5] );
        poMIEntry->SetDoubleField( "proParams[6]", poPro->proParams[6] );
        poMIEntry->SetDoubleField( "proParams[7]", poPro->proParams[7] );
        poMIEntry->SetDoubleField( "proParams[8]", poPro->proParams[8] );
        poMIEntry->SetDoubleField( "proParams[9]", poPro->proParams[9] );
        poMIEntry->SetDoubleField( "proParams[10]", poPro->proParams[10] );
        poMIEntry->SetDoubleField( "proParams[11]", poPro->proParams[11] );
        poMIEntry->SetDoubleField( "proParams[12]", poPro->proParams[12] );
        poMIEntry->SetDoubleField( "proParams[13]", poPro->proParams[13] );
        poMIEntry->SetDoubleField( "proParams[14]", poPro->proParams[14] );
        poMIEntry->SetStringField( "proSpheroid.sphereName",
                                   poPro->proSpheroid.sphereName );
        poMIEntry->SetDoubleField( "proSpheroid.a",
                                   poPro->proSpheroid.a );
        poMIEntry->SetDoubleField( "proSpheroid.b",
                                   poPro->proSpheroid.b );
        poMIEntry->SetDoubleField( "proSpheroid.eSquared",
                                   poPro->proSpheroid.eSquared );
        poMIEntry->SetDoubleField( "proSpheroid.radius",
                                   poPro->proSpheroid.radius );
    }

    return CE_None;
}

/************************************************************************/
/*                            HFAGetDatum()                             */
/************************************************************************/

const Eprj_Datum *HFAGetDatum( HFAHandle hHFA )

{
    HFAEntry	*poMIEntry;
    Eprj_Datum	*psDatum;
    int		i;

    if( hHFA->nBands < 1 )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Do we already have it?                                          */
/* -------------------------------------------------------------------- */
    if( hHFA->pDatum != NULL )
        return( (Eprj_Datum *) hHFA->pDatum );

/* -------------------------------------------------------------------- */
/*      Get the HFA node.                                               */
/* -------------------------------------------------------------------- */
    poMIEntry = hHFA->papoBand[0]->poNode->GetNamedChild( "Projection.Datum" );
    if( poMIEntry == NULL )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Allocate the structure.                                         */
/* -------------------------------------------------------------------- */
    psDatum = (Eprj_Datum *) CPLCalloc(sizeof(Eprj_Datum),1);

/* -------------------------------------------------------------------- */
/*      Fetch the fields.                                               */
/* -------------------------------------------------------------------- */
    psDatum->datumname = CPLStrdup(poMIEntry->GetStringField("datumname"));
    psDatum->type = (Eprj_DatumType) poMIEntry->GetIntField("type");

    for( i = 0; i < 7; i++ )
    {
        char	szFieldName[30];

        sprintf( szFieldName, "params[%d]", i );
        psDatum->params[i] = poMIEntry->GetDoubleField(szFieldName);
    }

    psDatum->gridname = CPLStrdup(poMIEntry->GetStringField("gridname"));

    hHFA->pDatum = (void *) psDatum;

    return psDatum;
}

/************************************************************************/
/*                            HFASetDatum()                             */
/************************************************************************/

CPLErr HFASetDatum( HFAHandle hHFA, const Eprj_Datum *poDatum )

{
/* -------------------------------------------------------------------- */
/*      Loop over bands, setting information on each one.               */
/* -------------------------------------------------------------------- */
    for( int iBand = 0; iBand < hHFA->nBands; iBand++ )
    {
        HFAEntry	*poDatumEntry=NULL, *poProParms;

/* -------------------------------------------------------------------- */
/*      Create a new Projection if there isn't one present already.     */
/* -------------------------------------------------------------------- */
        poProParms =
            hHFA->papoBand[iBand]->poNode->GetNamedChild("Projection");
        if( poProParms == NULL )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Can't add Eprj_Datum with no Eprj_ProjParameters." );
            return CE_Failure;
        }

        poDatumEntry = poProParms->GetNamedChild("Datum");
        if( poDatumEntry == NULL )
        {
            poDatumEntry = new HFAEntry( hHFA, "Datum","Eprj_Datum",
                                      poProParms );
        }

        poDatumEntry->MarkDirty();

/* -------------------------------------------------------------------- */
/*      Ensure we have enough space for all the data.                   */
/* -------------------------------------------------------------------- */
        int	nSize;
        GByte   *pabyData;

        nSize = 26 + strlen(poDatum->datumname) + 1 + 7*8;

        if( poDatum->gridname != NULL )
            nSize += strlen(poDatum->gridname) + 1;

        pabyData = poDatumEntry->MakeData( nSize );
        poDatumEntry->SetPosition();

/* -------------------------------------------------------------------- */
/*      Write the various fields.                                       */
/* -------------------------------------------------------------------- */
        poDatumEntry->SetStringField( "datumname", poDatum->datumname );
        poDatumEntry->SetIntField( "type", poDatum->type );

        poDatumEntry->SetDoubleField( "params[0]", poDatum->params[0] );
        poDatumEntry->SetDoubleField( "params[1]", poDatum->params[1] );
        poDatumEntry->SetDoubleField( "params[2]", poDatum->params[2] );
        poDatumEntry->SetDoubleField( "params[3]", poDatum->params[3] );
        poDatumEntry->SetDoubleField( "params[4]", poDatum->params[4] );
        poDatumEntry->SetDoubleField( "params[5]", poDatum->params[5] );
        poDatumEntry->SetDoubleField( "params[6]", poDatum->params[6] );

        poDatumEntry->SetStringField( "gridname", poDatum->gridname );
    }

    return CE_None;
}

/************************************************************************/
/*                             HFAGetPCT()                              */
/*                                                                      */
/*      Read the PCT from a band, if it has one.                        */
/************************************************************************/

CPLErr HFAGetPCT( HFAHandle hHFA, int nBand, int *pnColors,
                  double **ppadfRed, double **ppadfGreen, 
		  double **ppadfBlue , double **ppadfAlpha)

{
    if( nBand < 1 || nBand > hHFA->nBands )
        return CE_Failure;

    return( hHFA->papoBand[nBand-1]->GetPCT( pnColors, ppadfRed,
                                             ppadfGreen, ppadfBlue,
					     ppadfAlpha) );
}

/************************************************************************/
/*                             HFASetPCT()                              */
/*                                                                      */
/*      Set the PCT on a band.                                          */
/************************************************************************/

CPLErr HFASetPCT( HFAHandle hHFA, int nBand, int nColors,
                  double *padfRed, double *padfGreen, double *padfBlue, 
		  double *padfAlpha )

{
    if( nBand < 1 || nBand > hHFA->nBands )
        return CE_Failure;

    return( hHFA->papoBand[nBand-1]->SetPCT( nColors, padfRed,
                                             padfGreen, padfBlue, padfAlpha ) );
}

/************************************************************************/
/*                          HFAGetDataRange()                           */
/************************************************************************/

CPLErr	HFAGetDataRange( HFAHandle hHFA, int nBand,
                         double * pdfMin, double *pdfMax )

{
    HFAEntry	*poBinInfo;

    if( nBand < 1 || nBand > hHFA->nBands )
        return CE_Failure;

    poBinInfo = hHFA->papoBand[nBand-1]->poNode->GetNamedChild("Statistics" );

    if( poBinInfo == NULL )
        return( CE_Failure );

    *pdfMin = poBinInfo->GetDoubleField( "minimum" );
    *pdfMax = poBinInfo->GetDoubleField( "maximum" );

    if( *pdfMax > *pdfMin )
        return CE_None;
    else
        return CE_Failure;
}

/************************************************************************/
/*                            HFADumpNode()                             */
/************************************************************************/

static void	HFADumpNode( HFAEntry *poEntry, int nIndent, int bVerbose,
                             FILE * fp )

{
    static char	szSpaces[256];
    int		i;

    for( i = 0; i < nIndent*2; i++ )
        szSpaces[i] = ' ';
    szSpaces[nIndent*2] = '\0';

    fprintf( fp, "%s%s(%s) @ %d + %d @ %d\n", szSpaces,
             poEntry->GetName(), poEntry->GetType(),
             poEntry->GetFilePos(),
             poEntry->GetDataSize(), poEntry->GetDataPos() );

    if( bVerbose )
    {
        strcat( szSpaces, "+ " );
        poEntry->DumpFieldValues( fp, szSpaces );
        fprintf( fp, "\n" );
    }

    if( poEntry->GetChild() != NULL )
        HFADumpNode( poEntry->GetChild(), nIndent+1, bVerbose, fp );

    if( poEntry->GetNext() != NULL )
        HFADumpNode( poEntry->GetNext(), nIndent, bVerbose, fp );
}

/************************************************************************/
/*                            HFADumpTree()                             */
/*                                                                      */
/*      Dump the tree of information in a HFA file.                     */
/************************************************************************/

void HFADumpTree( HFAHandle hHFA, FILE * fpOut )

{
    HFADumpNode( hHFA->poRoot, 0, TRUE, fpOut );
}

/************************************************************************/
/*                         HFADumpDictionary()                          */
/*                                                                      */
/*      Dump the dictionary (in raw, and parsed form) to the named      */
/*      device.                                                         */
/************************************************************************/

void HFADumpDictionary( HFAHandle hHFA, FILE * fpOut )

{
    fprintf( fpOut, "%s\n", hHFA->pszDictionary );

    hHFA->poDictionary->Dump( fpOut );
}

/************************************************************************/
/*                            HFAStandard()                             */
/*                                                                      */
/*      Swap byte order on MSB systems.                                 */
/************************************************************************/

#ifdef CPL_MSB
void HFAStandard( int nBytes, void * pData )

{
    int		i;
    GByte	*pabyData = (GByte *) pData;

    for( i = nBytes/2-1; i >= 0; i-- )
    {
        GByte	byTemp;

        byTemp = pabyData[i];
        pabyData[i] = pabyData[nBytes-i-1];
        pabyData[nBytes-i-1] = byTemp;
    }
}
#endif

/* ==================================================================== */
/*      Default data dictionary.  Emitted verbatim into the imagine     */
/*      file.                                                           */
/* ==================================================================== */

static const char *aszDefaultDD[] = {
"{1:lversion,1:LfreeList,1:LrootEntryPtr,1:sentryHeaderLength,1:LdictionaryPtr,}Ehfa_File,{1:Lnext,1:Lprev,1:Lparent,1:Lchild,1:Ldata,1:ldataSize,64:cname,32:ctype,1:tmodTime,}Ehfa_Entry,{16:clabel,1:LheaderPtr,}Ehfa_HeaderTag,{1:LfreeList,1:lfreeSize,}Ehfa_FreeListNode,{1:lsize,1:Lptr,}Ehfa_Data,{1:lwidth,1:lheight,1:e3:thematic,athematic,fft of real-valued data,layerType,",
"1:e13:u1,u2,u4,u8,s8,u16,s16,u32,s32,f32,f64,c64,c128,pixelType,1:lblockWidth,1:lblockHeight,}Eimg_Layer,{1:lwidth,1:lheight,1:e3:thematic,athematic,fft of real-valued data,layerType,1:e13:u1,u2,u4,u8,s8,u16,s16,u32,s32,f32,f64,c64,c128,pixelType,1:lblockWidth,1:lblockHeight,}Eimg_Layer_SubSample,{1:e2:raster,vector,type,1:LdictionaryPtr,}Ehfa_Layer,{1:LspaceUsedForRasterData,}ImgFormatInfo831,{1:sfileCode,1:Loffset,1:lsize,1:e2:false,true,logvalid,",
"1:e2:no compression,ESRI GRID compression,compressionType,}Edms_VirtualBlockInfo,{1:lmin,1:lmax,}Edms_FreeIDList,{1:lnumvirtualblocks,1:lnumobjectsperblock,1:lnextobjectnum,1:e2:no compression,RLC compression,compressionType,0:poEdms_VirtualBlockInfo,blockinfo,0:poEdms_FreeIDList,freelist,1:tmodTime,}Edms_State,{0:pcstring,}Emif_String,{1:oEmif_String,fileName,2:LlayerStackValidFlagsOffset,2:LlayerStackDataOffset,1:LlayerStackCount,1:LlayerStackIndex,}ImgExternalRaster,{1:oEmif_String,algorithm,0:poEmif_String,nameList,}Eimg_RRDNamesList,{1:oEmif_String,projection,1:oEmif_String,units,}Eimg_MapInformation,",
"{1:oEmif_String,dependent,}Eimg_DependentFile,{1:oEmif_String,ImageLayerName,}Eimg_DependentLayerName,{1:lnumrows,1:lnumcolumns,1:e13:EGDA_TYPE_U1,EGDA_TYPE_U2,EGDA_TYPE_U4,EGDA_TYPE_U8,EGDA_TYPE_S8,EGDA_TYPE_U16,EGDA_TYPE_S16,EGDA_TYPE_U32,EGDA_TYPE_S32,EGDA_TYPE_F32,EGDA_TYPE_F64,EGDA_TYPE_C64,EGDA_TYPE_C128,datatype,1:e4:EGDA_SCALAR_OBJECT,EGDA_TABLE_OBJECT,EGDA_MATRIX_OBJECT,EGDA_RASTER_OBJECT,objecttype,}Egda_BaseData,{1:*bvalueBD,}Eimg_NonInitializedValue,{1:dx,1:dy,}Eprj_Coordinate,{1:dwidth,1:dheight,}Eprj_Size,{0:pcproName,1:*oEprj_Coordinate,upperLeftCenter,",
"1:*oEprj_Coordinate,lowerRightCenter,1:*oEprj_Size,pixelSize,0:pcunits,}Eprj_MapInfo,{0:pcdatumname,1:e3:EPRJ_DATUM_PARAMETRIC,EPRJ_DATUM_GRID,EPRJ_DATUM_REGRESSION,type,0:pdparams,0:pcgridname,}Eprj_Datum,{0:pcsphereName,1:da,1:db,1:deSquared,1:dradius,}Eprj_Spheroid,{1:e2:EPRJ_INTERNAL,EPRJ_EXTERNAL,proType,1:lproNumber,0:pcproExeName,0:pcproName,1:lproZone,0:pdproParams,1:*oEprj_Spheroid,proSpheroid,}Eprj_ProParameters,{1:dminimum,1:dmaximum,1:dmean,1:dmedian,1:dmode,1:dstddev,}Esta_Statistics,{1:lnumBins,1:e4:direct,linear,logarithmic,explicit,binFunctionType,1:dminLimit,1:dmaxLimit,1:*bbinLimits,}Edsc_BinFunction,{0:poEmif_String,LayerNames,1:*bExcludedValues,1:oEmif_String,AOIname,",
"1:lSkipFactorX,1:lSkipFactorY,1:*oEdsc_BinFunction,BinFunction,}Eimg_StatisticsParameters830,{1:lnumrows,}Edsc_Table,{1:lnumRows,1:LcolumnDataPtr,1:e4:integer,real,complex,string,dataType,1:lmaxNumChars,}Edsc_Column,{1:lposition,0:pcname,1:e2:EMSC_FALSE,EMSC_TRUE,editable,1:e3:LEFT,CENTER,RIGHT,alignment,0:pcformat,1:e3:DEFAULT,APPLY,AUTO-APPLY,formulamode,0:pcformula,1:dcolumnwidth,0:pcunits,1:e5:NO_COLOR,RED,GREEN,BLUE,COLOR,colorflag,0:pcgreenname,0:pcbluename,}Eded_ColumnAttributes_1,{1:lversion,1:lnumobjects,1:e2:EAOI_UNION,EAOI_INTERSECTION,operation,}Eaoi_AreaOfInterest,.",
NULL
};

/************************************************************************/
/*                            HFACreateLL()                             */
/*                                                                      */
/*      Low level creation of an Imagine file.  Writes out the          */
/*      Ehfa_HeaderTag, dictionary and Ehfa_File.                       */
/************************************************************************/

HFAHandle HFACreateLL( const char * pszFilename )

{
    FILE	*fp;
    HFAInfo_t   *psInfo;

/* -------------------------------------------------------------------- */
/*      Create the file in the file system.                             */
/* -------------------------------------------------------------------- */
    fp = VSIFOpenL( pszFilename, "w+b" );
    if( fp == NULL )
    {
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "Creation of file %s failed.",
                  pszFilename );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Create the HFAInfo_t                                            */
/* -------------------------------------------------------------------- */
    psInfo = (HFAInfo_t *) CPLCalloc(sizeof(HFAInfo_t),1);

    psInfo->fp = fp;
    psInfo->eAccess = HFA_Update;
    psInfo->nXSize = 0;
    psInfo->nYSize = 0;
    psInfo->nBands = 0;
    psInfo->papoBand = NULL;
    psInfo->pMapInfo = NULL;
    psInfo->pDatum = NULL;
    psInfo->pProParameters = NULL;
    psInfo->bTreeDirty = FALSE;
    psInfo->pszFilename = CPLStrdup(CPLGetFilename(pszFilename));
    psInfo->pszPath = CPLStrdup(CPLGetPath(pszFilename));

/* -------------------------------------------------------------------- */
/*      Write out the Ehfa_HeaderTag                                    */
/* -------------------------------------------------------------------- */
    GInt32	nHeaderPos;

    VSIFWriteL( (void *) "EHFA_HEADER_TAG", 1, 16, fp );

    nHeaderPos = 20;
    HFAStandard( 4, &nHeaderPos );
    VSIFWriteL( &nHeaderPos, 4, 1, fp );

/* -------------------------------------------------------------------- */
/*      Write the Ehfa_File node, locked in at offset 20.               */
/* -------------------------------------------------------------------- */
    GInt32	nVersion = 1, nFreeList = 0, nRootEntry = 0;
    GInt16      nEntryHeaderLength = 128;
    GInt32	nDictionaryPtr = 38;

    psInfo->nEntryHeaderLength = nEntryHeaderLength;
    psInfo->nRootPos = 0;
    psInfo->nDictionaryPos = nDictionaryPtr;
    psInfo->nVersion = nVersion;

    HFAStandard( 4, &nVersion );
    HFAStandard( 4, &nFreeList );
    HFAStandard( 4, &nRootEntry );
    HFAStandard( 2, &nEntryHeaderLength );
    HFAStandard( 4, &nDictionaryPtr );

    VSIFWriteL( &nVersion, 4, 1, fp );
    VSIFWriteL( &nFreeList, 4, 1, fp );
    VSIFWriteL( &nRootEntry, 4, 1, fp );
    VSIFWriteL( &nEntryHeaderLength, 2, 1, fp );
    VSIFWriteL( &nDictionaryPtr, 4, 1, fp );

/* -------------------------------------------------------------------- */
/*      Write the dictionary, locked in at location 38.  Note that      */
/*      we jump through a bunch of hoops to operate on the              */
/*      dictionary in chunks because some compiles (such as VC++)       */
/*      don't allow particularly large static strings.                  */
/* -------------------------------------------------------------------- */
    int      nDictLen = 0, iChunk;

    for( iChunk = 0; aszDefaultDD[iChunk] != NULL; iChunk++ )
        nDictLen += strlen(aszDefaultDD[iChunk]);

    psInfo->pszDictionary = (char *) CPLMalloc(nDictLen+1);
    psInfo->pszDictionary[0] = '\0';

    for( iChunk = 0; aszDefaultDD[iChunk] != NULL; iChunk++ )
        strcat( psInfo->pszDictionary, aszDefaultDD[iChunk] );

    VSIFWriteL( (void *) psInfo->pszDictionary, 1,
                strlen(psInfo->pszDictionary)+1, fp );

    psInfo->poDictionary = new HFADictionary( psInfo->pszDictionary );

    psInfo->nEndOfFile = (GUInt32) VSIFTellL( fp );

/* -------------------------------------------------------------------- */
/*      Create a root entry.                                            */
/* -------------------------------------------------------------------- */
    psInfo->poRoot = new HFAEntry( psInfo, "root", "root", NULL );

    return psInfo;
}

/************************************************************************/
/*                          HFAAllocateSpace()                          */
/*                                                                      */
/*      Return an area in the file to the caller to write the           */
/*      requested number of bytes.  Currently this is always at the     */
/*      end of the file, but eventually we might actually keep track    */
/*      of free space.  The HFAInfo_t's concept of file size is         */
/*      updated, even if nothing ever gets written to this region.      */
/*                                                                      */
/*      Returns the offset to the requested space, or zero one          */
/*      failure.                                                        */
/************************************************************************/

GUInt32 HFAAllocateSpace( HFAInfo_t *psInfo, GUInt32 nBytes )

{
    /* should check if this will wrap over 2GB limit */

    psInfo->nEndOfFile += nBytes;
    return psInfo->nEndOfFile - nBytes;
}

/************************************************************************/
/*                              HFAFlush()                              */
/*                                                                      */
/*      Write out any dirty tree information to disk, putting the       */
/*      disk file in a consistent state.                                */
/************************************************************************/

CPLErr HFAFlush( HFAHandle hHFA )

{
    CPLErr	eErr;

    if( !hHFA->bTreeDirty )
        return CE_None;

    CPLAssert( hHFA->poRoot != NULL );

/* -------------------------------------------------------------------- */
/*      Flush HFAEntry tree to disk.                                    */
/* -------------------------------------------------------------------- */
    eErr = hHFA->poRoot->FlushToDisk();
    if( eErr != CE_None )
        return eErr;

    hHFA->bTreeDirty = FALSE;

/* -------------------------------------------------------------------- */
/*      do we need to update the Ehfa_File pointer to the root node?    */
/* -------------------------------------------------------------------- */
    if( hHFA->nRootPos != hHFA->poRoot->GetFilePos() )
    {
        GUInt32		nRootPos;

        nRootPos = hHFA->nRootPos = hHFA->poRoot->GetFilePos();
        HFAStandard( 4, &nRootPos );
        VSIFSeekL( hHFA->fp, 20 + 8, SEEK_SET );
        VSIFWriteL( &nRootPos, 4, 1, hHFA->fp );
    }

    return CE_None;
}

/************************************************************************/
/*                             HFACreate()                              */
/************************************************************************/

HFAHandle HFACreate( const char * pszFilename,
                     int nXSize, int nYSize, int nBands,
                     int nDataType, char ** papszOptions )

{
    HFAHandle	psInfo;
    int		nBlockSize = 64;
    const char * pszValue = CSLFetchNameValue( papszOptions, "BLOCKSIZE" );
    if ( pszValue != NULL )
    {
        nBlockSize = atoi( pszValue );
        // check for sane values
        if ( ( nBlockSize < 32 ) || (nBlockSize > 2048) )
        {
            nBlockSize = 64;
        }
    }
    int		bCreateLargeRaster = CSLFetchBoolean(papszOptions,"USE_SPILL",
                                                     FALSE);
    char	*pszFullFilename = NULL, *pszRawFilename = NULL;

/* -------------------------------------------------------------------- */
/*      Create the low level structure.                                 */
/* -------------------------------------------------------------------- */
    psInfo = HFACreateLL( pszFilename );
    if( psInfo == NULL )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Work out some details about the tiling scheme.                  */
/* -------------------------------------------------------------------- */
    int	nBlocksPerRow, nBlocksPerColumn, nBlocks, nBytesPerBlock;
    int	nBytesPerRow, nBlockMapSize, iHeaderSize, iFlagsSize;

    nBlocksPerRow = (nXSize + nBlockSize - 1) / nBlockSize;
    nBlocksPerColumn = (nYSize + nBlockSize - 1) / nBlockSize;
    nBlocks = nBlocksPerRow * nBlocksPerColumn;
    nBytesPerBlock = (nBlockSize * nBlockSize
	* HFAGetDataTypeBits(nDataType) + 7) / 8;

    CPLDebug( "HFACreate", "Blocks per row %d, blocks per column %d, "
	      "total number of blocks %d, bytes per block %d.",
	      nBlocksPerRow, nBlocksPerColumn, nBlocks, nBytesPerBlock );

    nBytesPerRow = ( nBlocksPerRow + 7 ) / 8;
    nBlockMapSize = nBytesPerRow * nBlocksPerColumn;
    iHeaderSize = 49;
    iFlagsSize = nBlockMapSize + 20;

/* -------------------------------------------------------------------- */
/*      Check whether we should create external large file with         */
/*      image.  We create a spill file if the amount of imagery is      */
/*      close to 2GB.  We don't check the amount of auxilary            */
/*      information, so in theory if there were an awful lot of         */
/*      non-imagery data our approximate size could be smaller than     */
/*      the file will actually we be.  We leave room for 10MB of        */
/*      auxilary data.                                                  */
/*      We can also force spill file creation using option              */
/*      SPILL_FILE=YES.                                                 */
/* -------------------------------------------------------------------- */
    double dfApproxSize = (double)nBytesPerBlock * (double)nBlocks *
        (double)nBands + 10000000.0;

    if( dfApproxSize > 2147483648.0 )
        bCreateLargeRaster = TRUE;

    // erdas imagine always creates this entry no matter if an external
    // spill file is used or not
    HFAEntry *poImgFormat;
    poImgFormat = new HFAEntry( psInfo, "IMGFormatInfo",
                                "ImgFormatInfo831", psInfo->poRoot );
    poImgFormat->MakeData();
    if ( bCreateLargeRaster )
    {
        poImgFormat->SetIntField( "spaceUsedForRasterData", 0 );
    }
    else
    {
        poImgFormat->SetIntField( "spaceUsedForRasterData",
                                  nBytesPerBlock*nBlocks*nBands );
    }

/* ==================================================================== */
/*      Create each band (layer)                                        */
/* ==================================================================== */
    int		iBand;

    for( iBand = 0; iBand < nBands; iBand++ )
    {
	HFAEntry	*poEimg_Layer;
        char		szName[128];

        sprintf( szName, "Layer_%d", iBand + 1 );

/* -------------------------------------------------------------------- */
/*      Create the Eimg_Layer for the band.                             */
/* -------------------------------------------------------------------- */
        poEimg_Layer =
            new HFAEntry( psInfo, szName, "Eimg_Layer", psInfo->poRoot );

        poEimg_Layer->SetIntField( "width", nXSize );
        poEimg_Layer->SetIntField( "height", nYSize );
        poEimg_Layer->SetStringField( "layerType", "athematic" );
        poEimg_Layer->SetIntField( "pixelType", nDataType );
        poEimg_Layer->SetIntField( "blockWidth", nBlockSize );
        poEimg_Layer->SetIntField( "blockHeight", nBlockSize );

	if ( !bCreateLargeRaster )
	{
/* -------------------------------------------------------------------- */
/*      Create the RasterDMS (block list).  This is a complex type      */
/*      with pointers, and variable size.  We set the superstructure    */
/*      ourselves rather than trying to have the HFA type management    */
/*      system do it for us (since this would be hard to implement).    */
/* -------------------------------------------------------------------- */
	    int	nDmsSize;
	    HFAEntry *poEdms_State;
	    GByte	*pabyData;

	    poEdms_State =
		new HFAEntry( psInfo, "RasterDMS", "Edms_State", poEimg_Layer );

	    nDmsSize = 14 * nBlocks + 38;
	    pabyData = poEdms_State->MakeData( nDmsSize );

	    /* set some simple values */
	    poEdms_State->SetIntField( "numvirtualblocks", nBlocks );
	    poEdms_State->SetIntField( "numobjectsperblock",
				       nBlockSize*nBlockSize );
	    poEdms_State->SetIntField( "nextobjectnum",
				       nBlockSize*nBlockSize*nBlocks );
	    poEdms_State->SetStringField( "compressionType", "no compression" );

	    /* we need to hardcode file offset into the data, so locate it now */
	    poEdms_State->SetPosition();

	    /* Set block info headers */
	    GUInt32		nValue;

	    /* blockinfo count */
	    nValue = nBlocks;
	    HFAStandard( 4, &nValue );
	    memcpy( pabyData + 14, &nValue, 4 );

	    /* blockinfo position */
	    nValue = poEdms_State->GetDataPos() + 22;
	    HFAStandard( 4, &nValue );
	    memcpy( pabyData + 18, &nValue, 4 );

	    /* Set each blockinfo */
	    for( int iBlock = 0; iBlock < nBlocks; iBlock++ )
	    {
		GInt16  nValue16;
		int	    nOffset = 22 + 14 * iBlock;

		/* fileCode */
		nValue16 = 0;
		HFAStandard( 2, &nValue16 );
		memcpy( pabyData + nOffset, &nValue16, 2 );

		/* offset */
		nValue = HFAAllocateSpace( psInfo, nBytesPerBlock );
		HFAStandard( 4, &nValue );
		memcpy( pabyData + nOffset + 2, &nValue, 4 );

		/* size */
		nValue = nBytesPerBlock;
		HFAStandard( 4, &nValue );
		memcpy( pabyData + nOffset + 6, &nValue, 4 );

		/* logValid (true) */
		nValue16 = 1;
		HFAStandard( 2, &nValue16 );
		memcpy( pabyData + nOffset + 10, &nValue16, 2 );

		/* compressionType (no compression) */
		nValue16 = 0;
		HFAStandard( 2, &nValue16 );
		memcpy( pabyData + nOffset + 12, &nValue16, 2 );
	    }

/* -------------------------------------------------------------------- */
/*      Create the Ehfa_Layer.                                          */
/* -------------------------------------------------------------------- */
	    HFAEntry *poEhfa_Layer;
	    GUInt32  nLDict;
	    char     szLDict[128], chBandType;

	    if( nDataType == EPT_u1 )
		chBandType = '1';
	    else if( nDataType == EPT_u2 )
		chBandType = '2';
	    else if( nDataType == EPT_u4 )
		chBandType = '4';
	    else if( nDataType == EPT_u8 )
		chBandType = 'c';
	    else if( nDataType == EPT_s8 )
		chBandType = 'C';
	    else if( nDataType == EPT_u16 )
		chBandType = 's';
	    else if( nDataType == EPT_s16 )
		chBandType = 'S';
	    else if( nDataType == EPT_u32 )
                // for some reason erdas imagine expects an L for unsinged 32 bit ints
                // otherwise it gives strange "out of memory errors"
		chBandType = 'L';
	    else if( nDataType == EPT_s32 )
		chBandType = 'L';
	    else if( nDataType == EPT_f32 )
		chBandType = 'f';
	    else if( nDataType == EPT_f64 )
		chBandType = 'd';
	    else if( nDataType == EPT_c64 )
		chBandType = 'm';
	    else if( nDataType == EPT_c128 )
		chBandType = 'M';
	    else
	    {
		CPLAssert( FALSE );
		chBandType = 'c';
	    }

            // the first value in the entry below gives the number of pixels within a block
	    sprintf( szLDict, "{%d:%cdata,}RasterDMS,.", nBlockSize*nBlockSize, chBandType );

	    poEhfa_Layer = new HFAEntry( psInfo, "Ehfa_Layer", "Ehfa_Layer",
					 poEimg_Layer );
	    poEhfa_Layer->MakeData();
	    poEhfa_Layer->SetPosition();
	    nLDict = HFAAllocateSpace( psInfo, strlen(szLDict) + 1 );

	    poEhfa_Layer->SetStringField( "type", "raster" );
	    poEhfa_Layer->SetIntField( "dictionaryPtr", nLDict );

	    VSIFSeekL( psInfo->fp, nLDict, SEEK_SET );
	    VSIFWriteL( (void *) szLDict, strlen(szLDict) + 1, 1, psInfo->fp );
	}
	else
	{
/* -------------------------------------------------------------------- */
/*      Create ExternalRasterDMS object.                                */
/* -------------------------------------------------------------------- */
	    HFAEntry *poEdms_State;

	    pszFullFilename =
		CPLStrdup( CPLResetExtension( pszFilename, "ige" ) );
	    pszRawFilename =
		CPLStrdup( CPLGetFilename( pszFullFilename ) );
	    poEdms_State =
		new HFAEntry( psInfo, "ExternalRasterDMS",
			      "ImgExternalRaster", poEimg_Layer );
	    poEdms_State->MakeData( 8 + strlen( pszRawFilename ) + 1 + 6 * 4 );

	    poEdms_State->SetStringField( "fileName.string", pszRawFilename );

	    poEdms_State->SetIntField( "layerStackValidFlagsOffset[0]",
				       iHeaderSize );
	    poEdms_State->SetIntField( "layerStackValidFlagsOffset[1]", 0 );

	    poEdms_State->SetIntField( "layerStackDataOffset[0]",
				       iHeaderSize + nBands * iFlagsSize );
	    poEdms_State->SetIntField( "layerStackDataOffset[1]", 0 );
	    poEdms_State->SetIntField( "layerStackCount", nBands );
	    poEdms_State->SetIntField( "layerStackIndex", iBand );

/* -------------------------------------------------------------------- */
/*      Create the Ehfa_Layer.                                          */
/* -------------------------------------------------------------------- */
	    HFAEntry *poEhfa_Layer;
	    GUInt32  nLDict;
	    char     szLDict[128], chBandType;

	    if( nDataType == EPT_u1 )
		chBandType = '1';
	    else if( nDataType == EPT_u2 )
		chBandType = '2';
	    else if( nDataType == EPT_u4 )
		chBandType = '4';
	    else if( nDataType == EPT_u8 )
		chBandType = 'c';
	    else if( nDataType == EPT_s8 )
		chBandType = 'C';
	    else if( nDataType == EPT_u16 )
		chBandType = 's';
	    else if( nDataType == EPT_s16 )
		chBandType = 'S';
	    else if( nDataType == EPT_u32 )
		chBandType = 'I';
	    else if( nDataType == EPT_s32 )
		chBandType = 'L';
	    else if( nDataType == EPT_f32 )
		chBandType = 'f';
	    else if( nDataType == EPT_f64 )
		chBandType = 'd';
	    else if( nDataType == EPT_c64 )
		chBandType = 'm';
	    else if( nDataType == EPT_c128 )
		chBandType = 'M';
	    else
	    {
		CPLAssert( FALSE );
		chBandType = 'c';
	    }

	    sprintf( szLDict, "{4096:%cdata,}RasterDMS,.", chBandType );

	    poEhfa_Layer = new HFAEntry( psInfo, "Ehfa_Layer", "Ehfa_Layer",
					 poEimg_Layer );
	    poEhfa_Layer->MakeData();
	    poEhfa_Layer->SetPosition();
	    nLDict = HFAAllocateSpace( psInfo, strlen(szLDict) + 1 );

	    poEhfa_Layer->SetStringField( "type", "raster" );
	    poEhfa_Layer->SetIntField( "dictionaryPtr", nLDict );

	    VSIFSeekL( psInfo->fp, nLDict, SEEK_SET );
	    VSIFWriteL( (void *) szLDict, strlen(szLDict) + 1, 1, psInfo->fp );
	}
    }

    if ( bCreateLargeRaster )
    {
/* -------------------------------------------------------------------- */
/*      Create external file and write its header.                      */
/* -------------------------------------------------------------------- */
	GByte	    bUnknown;
	GInt32	    nValue32;
	FILE	    *fpExternal;
	char	    *pszMagick = "ERDAS_IMG_EXTERNAL_RASTER";

	fpExternal = VSIFOpenL( pszFullFilename, "wb" );
	if( fpExternal == NULL )
	{
	    CPLError( CE_Failure, CPLE_OpenFailed,
		      "Unable to create external data file: %s\n",
		      pszFullFilename );
	    CPLFree( pszRawFilename );
	    CPLFree( pszFullFilename );
	    return NULL;
	}

	VSIFWriteL( pszMagick, 1, 26, fpExternal );
	bUnknown = 1;
	VSIFWriteL( &bUnknown, 1, 1, fpExternal );
	nValue32 = nBands;
	HFAStandard( 4, &nValue32 );
	VSIFWriteL( &nValue32, 4, 1, fpExternal );
	nValue32 = nXSize;
	HFAStandard( 4, &nValue32 );
	VSIFWriteL( &nValue32, 4, 1, fpExternal );
	nValue32 = nYSize;
	HFAStandard( 4, &nValue32 );
	VSIFWriteL( &nValue32, 4, 1, fpExternal );
	nValue32 = nBlockSize;
	HFAStandard( 4, &nValue32 );
	VSIFWriteL( &nValue32, 4, 1, fpExternal );
	VSIFWriteL( &nValue32, 4, 1, fpExternal );
	bUnknown = 3;
	VSIFWriteL( &bUnknown, 1, 1, fpExternal );
	bUnknown = 0;
	VSIFWriteL( &bUnknown, 1, 1, fpExternal );

/* -------------------------------------------------------------------- */
/*      Write out ValidFlags section(s).                                */
/* -------------------------------------------------------------------- */
	unsigned char   *pabyBlockMap;

	pabyBlockMap = (unsigned char *) CPLMalloc( nBlockMapSize );
	memset( pabyBlockMap, 0xff, nBlockMapSize );
	for ( iBand = 0; iBand < nBands; iBand++ )
	{
	    int		    i, iRemainder;

	    nValue32 = 1;	// Unknown
	    HFAStandard( 4, &nValue32 );
	    VSIFWriteL( &nValue32, 4, 1, fpExternal );
	    nValue32 = 0;	// Unknown
	    VSIFWriteL( &nValue32, 4, 1, fpExternal );
	    nValue32 = nBlocksPerColumn;
	    HFAStandard( 4, &nValue32 );
	    VSIFWriteL( &nValue32, 4, 1, fpExternal );
	    nValue32 = nBlocksPerRow;
	    HFAStandard( 4, &nValue32 );
	    VSIFWriteL( &nValue32, 4, 1, fpExternal );
	    nValue32 = 0x30000;	// Unknown
	    VSIFWriteL( &nValue32, 4, 1, fpExternal );

	    iRemainder = nBlocksPerRow % 8;
	    CPLDebug( "HFACreate",
		      "Block map size %d, bytes per row %d, remainder %d.",
		      nBlockMapSize, nBytesPerRow, iRemainder );
	    if ( iRemainder )
	    {
		for ( i = nBytesPerRow - 1; i < nBlockMapSize; i+=nBytesPerRow )
		    pabyBlockMap[i] = (GByte) ((1<<iRemainder) - 1);
	    }

	    VSIFWriteL( pabyBlockMap, 1, nBlockMapSize, fpExternal );
	}
	VSIFCloseL( fpExternal );

	if ( pabyBlockMap )
	    CPLFree( pabyBlockMap );
	if ( pszRawFilename )
	    CPLFree( pszRawFilename );
	if ( pszFullFilename )
	    CPLFree( pszFullFilename );
    }

/* -------------------------------------------------------------------- */
/*      Initialize the band information.                                */
/* -------------------------------------------------------------------- */
    HFAParseBandInfo( psInfo );

    return psInfo;
}

/************************************************************************/
/*                           HFAGetMetadata()                           */
/*                                                                      */
/*      Read metadata structured in a table called GDAL_MetaData.       */
/************************************************************************/

char ** HFAGetMetadata( HFAHandle hHFA, int nBand )

{
    HFAEntry *poTable;

    if( nBand > 0 && nBand <= hHFA->nBands )
        poTable = hHFA->papoBand[nBand - 1]->poNode->GetChild();
    else if( nBand == 0 )
        poTable = hHFA->poRoot->GetChild();
    else
        return NULL;

    for( ; poTable != NULL && !EQUAL(poTable->GetName(),"GDAL_MetaData");
         poTable = poTable->GetNext() ) {}

    if( poTable == NULL || !EQUAL(poTable->GetType(),"Edsc_Table") )
        return NULL;

    if( poTable->GetIntField( "numRows" ) != 1 )
    {
        CPLDebug( "HFADataset", "GDAL_MetaData.numRows = %d, expected 1!",
                  poTable->GetIntField( "numRows" ) );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Loop over each column.  Each column will be one metadata        */
/*      entry, with the title being the key, and the row value being    */
/*      the value.  There is only ever one row in GDAL_MetaData         */
/*      tables.                                                         */
/* -------------------------------------------------------------------- */
    HFAEntry *poColumn;
    char    **papszMD = NULL;

    for( poColumn = poTable->GetChild();
         poColumn != NULL;
         poColumn = poColumn->GetNext() )
    {
        const char *pszValue;
        int        columnDataPtr;

        // Skip the #Bin_Function# entry.
        if( EQUALN(poColumn->GetName(),"#",1) )
            continue;

        pszValue = poColumn->GetStringField( "dataType" );
        if( pszValue == NULL || !EQUAL(pszValue,"string") )
            continue;

        columnDataPtr = poColumn->GetIntField( "columnDataPtr" );
        if( columnDataPtr == 0 )
            continue;

/* -------------------------------------------------------------------- */
/*      read up to 500 bytes from the indicated location.               */
/* -------------------------------------------------------------------- */
        char szMDValue[501];
        int  nMDBytes = sizeof(szMDValue)-1;

        if( VSIFSeekL( hHFA->fp, columnDataPtr, SEEK_SET ) != 0 )
            continue;

        nMDBytes = VSIFReadL( szMDValue, 1, nMDBytes, hHFA->fp );
        if( nMDBytes == 0 )
            continue;

        szMDValue[nMDBytes] = '\0';

        papszMD = CSLSetNameValue( papszMD, poColumn->GetName(), szMDValue );
    }

    return papszMD;
}

/************************************************************************/
/*                         HFASetGDALMetadata()                         */
/*                                                                      */
/*      This function is used to set metadata in a table called         */
/*      GDAL_MetaData.  It is called by HFASetMetadata() for all        */
/*      metadata items that aren't some specific supported              */
/*      information (like histogram or stats info).                     */
/************************************************************************/

static CPLErr 
HFASetGDALMetadata( HFAHandle hHFA, int nBand, char **papszMD )

{
    if( papszMD == NULL )
        return CE_None;

    HFAEntry  *poNode;

    if( nBand > 0 && nBand <= hHFA->nBands )
        poNode = hHFA->papoBand[nBand - 1]->poNode;
    else if( nBand == 0 )
        poNode = hHFA->poRoot;
    else
        return CE_Failure;

/* -------------------------------------------------------------------- */
/*      Create the Descriptor table.                                    */
/* -------------------------------------------------------------------- */
    HFAEntry	*poEdsc_Table;

    poEdsc_Table = new HFAEntry( hHFA, "GDAL_MetaData", "Edsc_Table",
                                 poNode );

    poEdsc_Table->SetIntField( "numrows", 1 );

/* -------------------------------------------------------------------- */
/*      Create the Binning function node.  I am not sure that we        */
/*      really need this though.                                        */
/* -------------------------------------------------------------------- */
    HFAEntry       *poEdsc_BinFunction;

    poEdsc_BinFunction =
        new HFAEntry( hHFA, "#Bin_Function#", "Edsc_BinFunction",
                      poEdsc_Table );

    // Because of the BaseData we have to hardcode the size. 
    poEdsc_BinFunction->MakeData( 30 );

    poEdsc_BinFunction->SetIntField( "numBins", 1 );
    poEdsc_BinFunction->SetStringField( "binFunction", "direct" );
    poEdsc_BinFunction->SetDoubleField( "minLimit", 0.0 );
    poEdsc_BinFunction->SetDoubleField( "maxLimit", 0.0 );

/* -------------------------------------------------------------------- */
/*      Process each metadata item as a separate column.		*/
/* -------------------------------------------------------------------- */
    for( int iColumn = 0; papszMD[iColumn] != NULL; iColumn++ )
    {
        HFAEntry        *poEdsc_Column;
        char            *pszKey = NULL;
        const char      *pszValue;

        pszValue = CPLParseNameValue( papszMD[iColumn], &pszKey );
        if( pszValue == NULL )
            continue;

/* -------------------------------------------------------------------- */
/*      Create the Edsc_Column.                                         */
/* -------------------------------------------------------------------- */
        poEdsc_Column = new HFAEntry( hHFA, pszKey, "Edsc_Column",
                                      poEdsc_Table );
        poEdsc_Column->SetIntField( "numRows", 1 );
        poEdsc_Column->SetStringField( "dataType", "string" );
        poEdsc_Column->SetIntField( "maxNumChars", strlen(pszValue)+1 );

/* -------------------------------------------------------------------- */
/*      Write the data out.                                             */
/* -------------------------------------------------------------------- */
        int      nOffset = HFAAllocateSpace( hHFA, strlen(pszValue)+1);

        poEdsc_Column->SetIntField( "columnDataPtr", nOffset );

        VSIFSeekL( hHFA->fp, nOffset, SEEK_SET );
        VSIFWriteL( (void *) pszValue, 1, strlen(pszValue)+1, hHFA->fp );

        CPLFree( pszKey );
    }

    return CE_Failure;
}

/************************************************************************/
/*                           HFASetMetadata()                           */
/************************************************************************/

CPLErr HFASetMetadata( HFAHandle hHFA, int nBand, char **papszMD )

{
    char **papszGDALMD = NULL;

    if( CSLCount(papszMD) == 0 )
        return CE_None;

    HFAEntry  *poNode;

    if( nBand > 0 && nBand <= hHFA->nBands )
        poNode = hHFA->papoBand[nBand - 1]->poNode;
    else if( nBand == 0 )
        poNode = hHFA->poRoot;
    else
        return CE_Failure;

    // check if the Metadata is an "known" entity which should be stored in a
    // better place
    char * pszBinValues = NULL;
    char ** pszAuxMetaData = GetHFAAuxMetaDataList();
    // check each metadata item
    for( int iColumn = 0; papszMD[iColumn] != NULL; iColumn++ )
    {
        char            *pszKey = NULL;
        const char      *pszValue;

        pszValue = CPLParseNameValue( papszMD[iColumn], &pszKey );
        if( pszValue == NULL )
            continue;

        // know look if its known
        int i;
        for( i = 0; pszAuxMetaData[i] != NULL; i += 4 )
        {
            if ( EQUALN( pszAuxMetaData[i + 2], pszKey, strlen(pszKey) ) )
                break;
        }
        if ( pszAuxMetaData[i] != NULL )
        {
            // found one, get the right entry
            HFAEntry *poEntry =	poNode->GetNamedChild( pszAuxMetaData[i] );
            if( poEntry == NULL )
            {
                // child does not yet exist --> create it
                poEntry = new HFAEntry( hHFA, pszAuxMetaData[i], pszAuxMetaData[i+3],
                                        poNode );
                if ( EQUALN( "HistogramParameters", pszAuxMetaData[i], 19 ) )
                {
                    // this is a bit nasty I need to set the string field for the object
                    // first because the SetStringField sets the count for the object
                    // BinFunction to the length of the string
                    poEntry->MakeData( 70 );
                    poEntry->SetStringField( "BinFunction.binFunctionType", "linear" );
                }
            }
            if ( poEntry == NULL )
                continue;

            const char *pszFieldName = pszAuxMetaData[i+1] + 1;
            switch( pszAuxMetaData[i+1][0] )
            {
              case 'd':
              {
                  double dfValue = atof( pszValue );
                  poEntry->SetDoubleField( pszFieldName, dfValue );
              }
              break;
              case 'i':
              case 'l':
              {
                  int nValue = atoi( pszValue );
                  poEntry->SetIntField( pszFieldName, nValue );
              }
              break;
              default:
                CPLAssert( FALSE );
            }
        }
        else if ( EQUALN( "STATISTICS_HISTOBINVALUES", pszKey, strlen(pszKey) ) )
        {
            pszBinValues = strdup( pszValue );
	}
        else
            papszGDALMD = CSLAddString( papszGDALMD, papszMD[iColumn] );
    }

    if ( pszBinValues != NULL )
    {
        HFAEntry * poEntry = poNode->GetNamedChild( "HistogramParameters" );
        if ( poEntry != NULL )
        {
            // if this node exists we have added Histogram data -- complete with some defaults
            poEntry->SetIntField( "SkipFactorX", 1 );
            poEntry->SetIntField( "SkipFactorY", 1 );

            int nNumBins = poEntry->GetIntField( "BinFunction.numBins" );
            double dMinLimit = poEntry->GetDoubleField( "BinFunction.minLimit" );
            double dMaxLimit = poEntry->GetDoubleField( "BinFunction.maxLimit" );
            // fill the descriptor table
            poEntry = new HFAEntry( hHFA, "Descriptor_Table", "Edsc_Table", poNode );
            poEntry->SetIntField( "numRows", nNumBins );
            // bin function
            HFAEntry * poBinFunc = new HFAEntry( hHFA, "#Bin_Function#", "Edsc_BinFunction",
                                                 poEntry );
            poBinFunc->MakeData( 30 );
            poBinFunc->SetIntField( "numBins", nNumBins );
            poBinFunc->SetDoubleField( "minLimit", dMinLimit );
            poBinFunc->SetDoubleField( "maxLimit", dMaxLimit );
            poBinFunc->SetStringField( "binFunctionType", "linear" ); // we use always a linear

            // we need a child named histogram
            HFAEntry * poHisto = new HFAEntry( hHFA, "Histogram", "Edsc_Column",
                                               poEntry );
            poHisto->SetIntField( "numRows", nNumBins );
            // allocate space for the bin values
            GUInt32 nOffset = HFAAllocateSpace( hHFA, nNumBins*4 );
            poHisto->SetIntField( "columnDataPtr", nOffset );
            poHisto->SetStringField( "dataType", "integer" );
            poHisto->SetIntField( "maxNumChars", 0 );
            // write out histogram data
            char * pszWork = pszBinValues;
            for ( int nBin = 0; nBin < nNumBins; ++nBin )
            {
                char * pszEnd = strchr( pszWork, '|' );
                if ( pszEnd != NULL )
                {
                    *pszEnd = 0;
                    VSIFSeekL( hHFA->fp, nOffset + 4*nBin, SEEK_SET );
                    int nValue = atoi( pszWork );
                    HFAStandard( 4, &nValue );

                    VSIFWriteL( (void *)&nValue, 1, 4, hHFA->fp );
                    pszWork = pszEnd + 1;
                }
            }
            free( pszBinValues );
        }
    }

    if( CSLCount( papszGDALMD) != 0 )
    {
        CPLErr eErr = HFASetGDALMetadata( hHFA, nBand, papszGDALMD );
        
        CSLDestroy( papszGDALMD );
        return eErr;
    }
    else
        return CE_Failure;
}

