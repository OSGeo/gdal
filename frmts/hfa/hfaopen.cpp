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
    HFAEntry	*poNode;

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
/*      Instantiate the root entry.                                     */
/* -------------------------------------------------------------------- */
    psInfo->poRoot = new HFAEntry( psInfo, psInfo->nRootPos, NULL, NULL );

/* -------------------------------------------------------------------- */
/*      Read the dictionary                                             */
/* -------------------------------------------------------------------- */
    psInfo->pszDictionary = HFAGetDictionary( psInfo );
    psInfo->poDictionary = new HFADictionary( psInfo->pszDictionary );

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
                continue;
            }

            psInfo->papoBand = (HFABand **)
                CPLRealloc(psInfo->papoBand,
                           sizeof(HFABand *) * (psInfo->nBands+1));
            psInfo->papoBand[psInfo->nBands] = new HFABand( psInfo, poNode );
            psInfo->nBands++;
        }
        
        poNode = poNode->GetNext();
    }

    return psInfo;
}

/************************************************************************/
/*                              HFAClose()                              */
/************************************************************************/

void HFAClose( HFAHandle hHFA )

{
    int		i;
    
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
                       int * pnBlockXSize, int * pnBlockYSize )

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
