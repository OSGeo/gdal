/******************************************************************************
 * $Id$
 *
 * Project:  Erdas Imagine (.img) Translator
 * Purpose:  Supporting functions for HFA (.img) ... main (C callable) API
 *           that is not dependent on GDAL (just CPL).
 * Author:   Frank Warmerdam, warmerda@home.com
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

/************************************************************************/
/*                          HFAGetDictionary()                          */
/************************************************************************/

static char * HFAGetDictionary( HFAHandle hHFA )

{
    char	*pszDictionary = (char *) CPLMalloc(100); 
    int		nDictMax = 100;
    int		nDictSize = 0;
    
    VSIFSeek( hHFA->fp, hHFA->nDictionaryPos, SEEK_SET );

    while( TRUE )
    {
        if( nDictSize >= nDictMax-1 )
        {
            nDictMax = nDictSize * 2 + 100;
            pszDictionary = (char *) CPLRealloc(pszDictionary, nDictMax );
        }

        if( VSIFRead( pszDictionary + nDictSize, 1, 1, hHFA->fp ) < 1
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
        fp = VSIFOpen( pszFilename, "rb" );
    else
        fp = VSIFOpen( pszFilename, "r+b" );

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
    if( VSIFRead( szHeader, 16, 1, fp ) < 1 )
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

    psInfo->fp = fp;
    psInfo->bTreeDirty = FALSE;

/* -------------------------------------------------------------------- */
/*	Where is the header?						*/
/* -------------------------------------------------------------------- */
    VSIFRead( &nHeaderPos, sizeof(GInt32), 1, fp );
    HFAStandard( 4, &nHeaderPos );

/* -------------------------------------------------------------------- */
/*      Read the header.                                                */
/* -------------------------------------------------------------------- */
    VSIFSeek( fp, nHeaderPos, SEEK_SET );

    VSIFRead( &(psInfo->nVersion), sizeof(GInt32), 1, fp );
    HFAStandard( 4, &(psInfo->nVersion) );
    
    VSIFRead( szHeader, 4, 1, fp ); /* skip freeList */

    VSIFRead( &(psInfo->nRootPos), sizeof(GInt32), 1, fp );
    HFAStandard( 4, &(psInfo->nRootPos) );
    
    VSIFRead( &(psInfo->nEntryHeaderLength), sizeof(GInt16), 1, fp );
    HFAStandard( 2, &(psInfo->nEntryHeaderLength) );

    VSIFRead( &(psInfo->nDictionaryPos), sizeof(GInt32), 1, fp );
    HFAStandard( 4, &(psInfo->nDictionaryPos) );

/* -------------------------------------------------------------------- */
/*      Collect file size.                                              */
/* -------------------------------------------------------------------- */
    VSIFSeek( fp, 0, SEEK_END );
    psInfo->nEndOfFile = VSIFTell( fp );

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
/*      Initialize the band information.                                */
/* -------------------------------------------------------------------- */
    HFAParseBandInfo( psInfo );

    return psInfo;
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
        if( EQUAL(poNode->GetType(),"Eimg_Layer") )
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
    
    delete hHFA->poRoot;

    VSIFClose( hHFA->fp );

    if( hHFA->poDictionary != NULL )
        delete hHFA->poDictionary;
    
    CPLFree( hHFA->pszDictionary );

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
                  double **ppadfRed, double **ppadfGreen, double **ppadfBlue )

{
    if( nBand < 1 || nBand > hHFA->nBands )
        return CE_Failure;

    return( hHFA->papoBand[nBand-1]->GetPCT( pnColors, ppadfRed,
                                             ppadfGreen, ppadfBlue ) );
}

/************************************************************************/
/*                             HFAGetPCT()                              */
/*                                                                      */
/*      Read the PCT from a band, if it has one.                        */
/************************************************************************/

CPLErr HFASetPCT( HFAHandle hHFA, int nBand, int nColors,
                  double *padfRed, double *padfGreen, double *padfBlue )

{
    if( nBand < 1 || nBand > hHFA->nBands )
        return CE_Failure;

    return( hHFA->papoBand[nBand-1]->SetPCT( nColors, padfRed,
                                             padfGreen, padfBlue ) );
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

    fprintf( fp, "%s%s(%s) %d @ %d\n", szSpaces,
             poEntry->GetName(), poEntry->GetType(),
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
"1:e13:u1,u2,u4,u8,s8,u16,s16,u32,s32,f32,f64,c64,c128,pixelType,1:lblockWidth,1:lblockHeight,}Eimg_Layer,{1:lwidth,1:lheight,1:e3:thematic,athematic,fft of real-valued data,layerType,1:e13:u1,u2,u4,u8,s8,u16,s16,u32,s32,f32,f64,c64,c128,pixelType,1:lblockWidth,1:lblockHeight,}Eimg_Layer_SubSample,{1:e2:raster,vector,type,1:LdictionaryPtr,}Ehfa_Layer,{1:sfileCode,1:Loffset,1:lsize,1:e2:false,true,logvalid,",
"1:e2:no compression,ESRI GRID compression,compressionType,}Edms_VirtualBlockInfo,{1:lmin,1:lmax,}Edms_FreeIDList,{1:lnumvirtualblocks,1:lnumobjectsperblock,1:lnextobjectnum,1:e2:no compression,RLC compression,compressionType,0:poEdms_VirtualBlockInfo,blockinfo,0:poEdms_FreeIDList,freelist,1:tmodTime,}Edms_State,{0:pcstring,}Emif_String,{1:oEmif_String,algorithm,0:poEmif_String,nameList,}Eimg_RRDNamesList,{1:oEmif_String,projection,1:oEmif_String,units,}Eimg_MapInformation,",
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
    fp = VSIFOpen( pszFilename, "w+b" );
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
    psInfo->nXSize = 0;
    psInfo->nYSize = 0;
    psInfo->nBands = 0;
    psInfo->papoBand = NULL;
    psInfo->pMapInfo = NULL;
    psInfo->pDatum = NULL;
    psInfo->pProParameters = NULL;
    psInfo->bTreeDirty = FALSE;

/* -------------------------------------------------------------------- */
/*      Write out the Ehfa_HeaderTag                                    */
/* -------------------------------------------------------------------- */
    GInt32	nHeaderPos;

    VSIFWrite( (void *) "EHFA_HEADER_TAG", 1, 16, fp );

    nHeaderPos = 20;
    HFAStandard( 4, &nHeaderPos );
    VSIFWrite( &nHeaderPos, 4, 1, fp );

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

    VSIFWrite( &nVersion, 4, 1, fp );
    VSIFWrite( &nFreeList, 4, 1, fp );
    VSIFWrite( &nRootEntry, 4, 1, fp );
    VSIFWrite( &nEntryHeaderLength, 2, 1, fp );
    VSIFWrite( &nDictionaryPtr, 4, 1, fp );

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

    VSIFWrite( (void *) psInfo->pszDictionary, 1, 
               strlen(psInfo->pszDictionary)+1, fp );

    psInfo->poDictionary = new HFADictionary( psInfo->pszDictionary );

    psInfo->nEndOfFile = VSIFTell( fp );

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
    /* should check if this will wrap over 4GB limit */

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
        VSIFSeek( hHFA->fp, 20 + 8, SEEK_SET );
        VSIFWrite( &nRootPos, 4, 1, hHFA->fp );
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

/* -------------------------------------------------------------------- */
/*      Create the low level structure.                                 */
/* -------------------------------------------------------------------- */
    psInfo = HFACreateLL( pszFilename );
    if( psInfo == NULL )
        return NULL;

/* ==================================================================== */
/*      Create each band (layer)                                        */
/* ==================================================================== */
    
    for( int iBand = 0; iBand < nBands; iBand++ )
    {
        HFAEntry	*poEimg_Layer;
        char		szName[128];

        sprintf( szName, "Layer_%d", iBand+1 );

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

/* -------------------------------------------------------------------- */
/*      Work out some details about the tiling scheme.                  */
/* -------------------------------------------------------------------- */
        int	nBlocksPerRow, nBlocksPerColumn, nBlocks, nBytesPerBlock;
        
        nBlocksPerRow = (nXSize + nBlockSize - 1) / nBlockSize;
        nBlocksPerColumn = (nYSize + nBlockSize - 1) / nBlockSize;
        nBlocks = nBlocksPerRow * nBlocksPerColumn;

        nBytesPerBlock = (nBlockSize * nBlockSize 
            * HFAGetDataTypeBits(nDataType) + 7) / 8;

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
            int	    nOffset = 22 + 14*iBlock;

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
        nLDict = HFAAllocateSpace( psInfo, strlen(szLDict)+1);

        poEhfa_Layer->SetStringField( "type", "raster" );
        poEhfa_Layer->SetIntField( "dictionaryPtr", nLDict );

        VSIFSeek( psInfo->fp, nLDict, SEEK_SET );
        VSIFWrite( (void *) szLDict, strlen(szLDict)+1, 1, psInfo->fp );
    }

/* -------------------------------------------------------------------- */
/*      Initialize the band information.                                */
/* -------------------------------------------------------------------- */
    HFAParseBandInfo( psInfo );

    return psInfo;
}

