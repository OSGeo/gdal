/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements interface to MapInfo .ID files used as attribute
 *           indexes.  
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2003, Frank Warmerdam
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

#include "ogr_attrind.h"
#include "mitab/mitab_priv.h"
#include "cpl_minixml.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                            OGRMIAttrIndex                            */
/*                                                                      */
/*      MapInfo .ID implementation of access to one fields              */
/*      indexing.                                                       */
/************************************************************************/

class OGRMILayerAttrIndex;

class OGRMIAttrIndex : public OGRAttrIndex
{
public:
    int         iIndex;
    TABINDFile  *poINDFile;
    OGRMILayerAttrIndex *poLIndex;
    OGRFieldDefn *poFldDefn;

    int         iField;

                OGRMIAttrIndex( OGRMILayerAttrIndex *, int iIndex, int iField);
               ~OGRMIAttrIndex();

    GByte      *BuildKey( OGRField *psKey );
    long        GetFirstMatch( OGRField *psKey );
    long       *GetAllMatches( OGRField *psKey );
    long       *GetAllMatches( OGRField *psKey, long* panFIDList, int* nFIDCount, int* nLength );

    OGRErr      AddEntry( OGRField *psKey, long nFID );
    OGRErr      RemoveEntry( OGRField *psKey, long nFID );

    OGRErr      Clear();
};

/************************************************************************/
/* ==================================================================== */
/*                         OGRMILayerAttrIndex                          */
/*                                                                      */
/*      MapInfo .ID specific implementation of a layer attribute        */
/*      index.                                                          */
/* ==================================================================== */
/************************************************************************/

class OGRMILayerAttrIndex : public OGRLayerAttrIndex
{
public:
    TABINDFile  *poINDFile;

    int         nIndexCount;
    OGRMIAttrIndex **papoIndexList;

    char        *pszMetadataFilename;
    char        *pszMIINDFilename;

    int         bINDAsReadOnly;
    int         bUnlinkINDFile;

                OGRMILayerAttrIndex();
    virtual     ~OGRMILayerAttrIndex();

    /* base class virtual methods */
    OGRErr      Initialize( const char *pszIndexPath, OGRLayer * );
    OGRErr      CreateIndex( int iField );
    OGRErr      DropIndex( int iField );
    OGRErr      IndexAllFeatures( int iField = -1 );

    OGRErr      AddToIndex( OGRFeature *poFeature, int iField = -1 );
    OGRErr      RemoveFromIndex( OGRFeature *poFeature );

    OGRAttrIndex *GetFieldIndex( int iField );

    /* custom to OGRMILayerAttrIndex */
    OGRErr      SaveConfigToXML();
    OGRErr      LoadConfigFromXML();
    OGRErr      LoadConfigFromXML(const char* pszRawXML);
    void        AddAttrInd( int iField, int iINDIndex );

    OGRLayer   *GetLayer() { return poLayer; }
};

/************************************************************************/
/*                        OGRMILayerAttrIndex()                         */
/************************************************************************/

OGRMILayerAttrIndex::OGRMILayerAttrIndex()

{
    poINDFile = NULL;
    nIndexCount = 0;
    papoIndexList = NULL;
    bUnlinkINDFile = FALSE;
    bINDAsReadOnly = TRUE;
    pszMIINDFilename = NULL;
    pszMetadataFilename = NULL;
}

/************************************************************************/
/*                        ~OGRMILayerAttrIndex()                        */
/************************************************************************/

OGRMILayerAttrIndex::~OGRMILayerAttrIndex()

{
    if( poINDFile != NULL )
    {
        poINDFile->Close();
        delete poINDFile;
        poINDFile = NULL;
    }

    if (bUnlinkINDFile)
        VSIUnlink( pszMIINDFilename );

    for( int i = 0; i < nIndexCount; i++ )
        delete papoIndexList[i];
    CPLFree( papoIndexList );

    CPLFree( pszMIINDFilename );
    CPLFree( pszMetadataFilename );
}

/************************************************************************/
/*                             Initialize()                             */
/************************************************************************/

OGRErr OGRMILayerAttrIndex::Initialize( const char *pszIndexPathIn, 
                                        OGRLayer *poLayerIn )

{
    if( poLayerIn == poLayer )
        return OGRERR_NONE;

/* -------------------------------------------------------------------- */
/*      Capture input information and form static pathnames.            */
/* -------------------------------------------------------------------- */
    poLayer = poLayerIn;

    pszIndexPath = CPLStrdup( pszIndexPathIn );

    /* try to process the XML string directly */
    if (EQUALN(pszIndexPathIn, "<OGRMILayerAttrIndex>", 21))
        return LoadConfigFromXML(pszIndexPathIn);
    
    pszMetadataFilename = CPLStrdup(
        CPLResetExtension( pszIndexPathIn, "idm" ) );
    
    pszMIINDFilename = CPLStrdup(CPLResetExtension( pszIndexPathIn, "ind" ));

/* -------------------------------------------------------------------- */
/*      If a metadata file already exists, load it.                     */
/* -------------------------------------------------------------------- */
    OGRErr eErr;
    VSIStatBuf sStat;

    if( VSIStat( pszMetadataFilename, &sStat ) == 0 )
    {
        eErr = LoadConfigFromXML();
        if( eErr != OGRERR_NONE )
            return eErr;
    }

    return OGRERR_NONE;
}

/************************************************************************/
/*                         LoadConfigFromXML()                          */
/************************************************************************/

OGRErr OGRMILayerAttrIndex::LoadConfigFromXML(const char* pszRawXML)

{
/* -------------------------------------------------------------------- */
/*      Parse the XML.                                                  */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psRoot = CPLParseXMLString( pszRawXML );

    if( psRoot == NULL )
        return OGRERR_FAILURE;

/* -------------------------------------------------------------------- */
/*      Open the index file.                                            */
/* -------------------------------------------------------------------- */
    poINDFile = new TABINDFile();
    
    if (pszMIINDFilename == NULL)
        pszMIINDFilename = CPLStrdup(CPLGetXMLValue(psRoot,"MIIDFilename",""));
    
    if( pszMIINDFilename == NULL )
        return OGRERR_FAILURE;

    /* NOTE: Replaced r+ with r according to explanation in Ticket #1620.
     * This change has to be observed if it doesn't cause any
     * problems in future. (mloskot)
     */
    if( poINDFile->Open( pszMIINDFilename, "r" ) != 0 )
    {
        CPLDestroyXMLNode( psRoot );
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "Failed to open index file %s.", 
                  pszMIINDFilename );
        return OGRERR_FAILURE;
    }
/* -------------------------------------------------------------------- */
/*      Process each attrindex.                                         */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psAttrIndex;

    for( psAttrIndex = psRoot->psChild; 
         psAttrIndex != NULL; 
         psAttrIndex = psAttrIndex->psNext )
    {
        int iField, iIndexIndex;

        if( psAttrIndex->eType != CXT_Element 
            || !EQUAL(psAttrIndex->pszValue,"OGRMIAttrIndex") )
            continue;

        iField = atoi(CPLGetXMLValue(psAttrIndex,"FieldIndex","-1"));
        iIndexIndex = atoi(CPLGetXMLValue(psAttrIndex,"IndexIndex","-1"));

        if( iField == -1 || iIndexIndex == -1 )
        {
            CPLError( CE_Warning, CPLE_AppDefined, 
                      "Skipping corrupt OGRMIAttrIndex entry." );
            continue;
        }

        AddAttrInd( iField, iIndexIndex );
    }

    CPLDestroyXMLNode( psRoot );

    CPLDebug( "OGR", "Restored %d field indexes for layer %s from %s on %s.",
              nIndexCount, poLayer->GetLayerDefn()->GetName(), 
              pszMetadataFilename, pszMIINDFilename );

    return OGRERR_NONE;
}

OGRErr OGRMILayerAttrIndex::LoadConfigFromXML()
{
    FILE *fp;
    int  nXMLSize;
    char *pszRawXML;

    CPLAssert( poINDFile == NULL );

/* -------------------------------------------------------------------- */
/*      Read the XML file.                                              */
/* -------------------------------------------------------------------- */
    fp = VSIFOpen( pszMetadataFilename, "rb" );
    if( fp == NULL )
        return OGRERR_NONE;

    VSIFSeek( fp, 0, SEEK_END );
    nXMLSize = VSIFTell( fp );
    VSIFSeek( fp, 0, SEEK_SET );

    pszRawXML = (char *) CPLMalloc(nXMLSize+1);
    pszRawXML[nXMLSize] = '\0';
    VSIFRead( pszRawXML, nXMLSize, 1, fp );

    VSIFClose( fp );

    OGRErr eErr = LoadConfigFromXML(pszRawXML);
    CPLFree(pszRawXML);

    return eErr;
}

/************************************************************************/
/*                          SaveConfigToXML()                           */
/************************************************************************/

OGRErr OGRMILayerAttrIndex::SaveConfigToXML()

{
    if( nIndexCount == 0 )
        return OGRERR_NONE;

/* -------------------------------------------------------------------- */
/*      Create the XML tree corresponding to this layer.                */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psRoot;

    psRoot = CPLCreateXMLNode( NULL, CXT_Element, "OGRMILayerAttrIndex" );

    CPLCreateXMLElementAndValue( psRoot, "MIIDFilename", 
                                 CPLGetFilename( pszMIINDFilename ) );
    
    for( int i = 0; i < nIndexCount; i++ )
    {
        OGRMIAttrIndex *poAI = papoIndexList[i];
        CPLXMLNode *psIndex;

        psIndex = CPLCreateXMLNode( psRoot, CXT_Element, "OGRMIAttrIndex" );

        CPLCreateXMLElementAndValue( psIndex, "FieldIndex", 
                                     CPLSPrintf( "%d", poAI->iField ) );
                                     
        CPLCreateXMLElementAndValue( psIndex, "FieldName", 
                                     poLayer->GetLayerDefn()->GetFieldDefn(poAI->iField)->GetNameRef() );

                                     
        CPLCreateXMLElementAndValue( psIndex, "IndexIndex", 
                                     CPLSPrintf( "%d", poAI->iIndex ) );
    }

/* -------------------------------------------------------------------- */
/*      Save it.                                                        */
/* -------------------------------------------------------------------- */
    char *pszRawXML = CPLSerializeXMLTree( psRoot );
    FILE *fp;

    CPLDestroyXMLNode( psRoot );

    fp = VSIFOpen( pszMetadataFilename, "wb" );
    if( fp == NULL )
    {
        CPLError( CE_Failure, CPLE_OpenFailed, 
                  "Failed to pen `%s' for write.", 
                  pszMetadataFilename );
        CPLFree( pszRawXML );
        return OGRERR_FAILURE;
    }

    VSIFWrite( pszRawXML, 1, strlen(pszRawXML), fp );
    VSIFClose( fp );

    CPLFree( pszRawXML );
    
    return OGRERR_NONE;
}

/************************************************************************/
/*                          IndexAllFeatures()                          */
/************************************************************************/

OGRErr OGRMILayerAttrIndex::IndexAllFeatures( int iField )

{
    OGRFeature *poFeature;

    poLayer->ResetReading();
    
    while( (poFeature = poLayer->GetNextFeature()) != NULL )
    {
        OGRErr eErr = AddToIndex( poFeature, iField );
        
        delete poFeature;

        if( eErr != CE_None )
            return eErr;
    }

    poLayer->ResetReading();

    return OGRERR_NONE;
}

/************************************************************************/
/*                            CreateIndex()                             */
/*                                                                      */
/*      Create an index corresponding to the indicated field, but do    */
/*      not populate it.  Use IndexAllFeatures() for that.              */
/************************************************************************/

OGRErr OGRMILayerAttrIndex::CreateIndex( int iField )

{
/* -------------------------------------------------------------------- */
/*      Do we have an open .ID file yet?  If not, create it now.        */
/* -------------------------------------------------------------------- */
    if( poINDFile == NULL )
    {
        poINDFile = new TABINDFile();
        if( poINDFile->Open( pszMIINDFilename, "w+" ) != 0 )
        {
            delete poINDFile;
            poINDFile = NULL;
            
            CPLError( CE_Failure, CPLE_OpenFailed, 
                      "Failed to create %s.",
                      pszMIINDFilename );
            return OGRERR_FAILURE;
        }
    }
    else if (bINDAsReadOnly)
    {
        poINDFile->Close();
        if( poINDFile->Open( pszMIINDFilename, "r+" ) != 0 )
        {
            CPLError( CE_Failure, CPLE_OpenFailed, 
                      "Failed to open %s as write-only.",
                      pszMIINDFilename );

            if( poINDFile->Open( pszMIINDFilename, "r" ) != 0 )
            {
                CPLError( CE_Failure, CPLE_OpenFailed, 
                      "Cannot re-open %s as read-only.",
                      pszMIINDFilename );
                delete poINDFile;
                poINDFile = NULL;
            }

            return OGRERR_FAILURE;
        }
        else
        {
            bINDAsReadOnly = FALSE;
        }
    }

/* -------------------------------------------------------------------- */
/*      Do we have this field indexed already?                          */
/* -------------------------------------------------------------------- */
    int i;
    OGRFieldDefn *poFldDefn=poLayer->GetLayerDefn()->GetFieldDefn(iField);

    for( i = 0; i < nIndexCount; i++ )
    {
        if( papoIndexList[i]->iField == iField )
        {
            CPLError( CE_Failure, CPLE_AppDefined, 
                      "It seems we already have an index for field %d/%s\n"
                      "of layer %s.", 
                      iField, poFldDefn->GetNameRef(),
                      poLayer->GetLayerDefn()->GetName() );
            return OGRERR_FAILURE;
        }
    }

/* -------------------------------------------------------------------- */
/*      What is the corresponding field type in TAB?  Note that we      */
/*      don't allow indexing of any of the list types.                  */
/* -------------------------------------------------------------------- */
    TABFieldType eTABFT;
    int           nFieldWidth = 0;

    switch( poFldDefn->GetType() )
    {
      case OFTInteger:
        eTABFT = TABFInteger;
        break;

      case OFTReal:
        eTABFT = TABFFloat;
        break;

      case OFTString:
        eTABFT = TABFChar;
        if( poFldDefn->GetWidth() > 0 )
            nFieldWidth = poFldDefn->GetWidth();
        else
            nFieldWidth = 64;
        break;

      default:
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Indexing not support for the field type of field %s.",
                  poFldDefn->GetNameRef() );
        return OGRERR_FAILURE;
    }

/* -------------------------------------------------------------------- */
/*      Create the index.                                               */
/* -------------------------------------------------------------------- */
    int iINDIndex;

    iINDIndex = poINDFile->CreateIndex( eTABFT, nFieldWidth );

    // CreateIndex() reports it's own errors.
    if( iINDIndex < 0 )
        return OGRERR_FAILURE;

    AddAttrInd( iField, iINDIndex );

    bUnlinkINDFile = FALSE;

/* -------------------------------------------------------------------- */
/*      Save the new configuration.                                     */
/* -------------------------------------------------------------------- */
    return SaveConfigToXML();
}

/************************************************************************/
/*                             DropIndex()                              */
/*                                                                      */
/*      For now we don't have any capability to remove index data       */
/*      from the MapInfo index file, so we just limit ourselves to      */
/*      ignoring it from now on.                                        */
/************************************************************************/

OGRErr OGRMILayerAttrIndex::DropIndex( int iField )

{
/* -------------------------------------------------------------------- */
/*      Do we have this field indexed already?                          */
/* -------------------------------------------------------------------- */
    int i;
    OGRFieldDefn *poFldDefn=poLayer->GetLayerDefn()->GetFieldDefn(iField);

    for( i = 0; i < nIndexCount; i++ )
    {
        if( papoIndexList[i]->iField == iField )
            break;

    }

    if( i == nIndexCount )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "DROP INDEX on field (%s) that doesn't have an index.",
                  poFldDefn->GetNameRef() );
        return OGRERR_FAILURE;
    }

/* -------------------------------------------------------------------- */
/*      Remove from the list.                                           */
/* -------------------------------------------------------------------- */
    OGRMIAttrIndex *poAI = papoIndexList[i];

    memmove( papoIndexList + i, papoIndexList + i + 1, 
             sizeof(void*) * (nIndexCount - i - 1) );

    delete poAI;

    nIndexCount--;
             
/* -------------------------------------------------------------------- */
/*      Save the new configuration, or if there is nothing left try     */
/*      to clean up the index files.                                    */
/* -------------------------------------------------------------------- */

    if( nIndexCount > 0 )
        return SaveConfigToXML();
    else
    {
        bUnlinkINDFile = TRUE;
        VSIUnlink( pszMetadataFilename );

        return OGRERR_NONE;
    }
}

/************************************************************************/
/*                             AddAttrInd()                             */
/************************************************************************/

void OGRMILayerAttrIndex::AddAttrInd( int iField, int iINDIndex )

{
    OGRMIAttrIndex *poAttrInd = new OGRMIAttrIndex( this, iINDIndex, iField);

    nIndexCount++;
    papoIndexList = (OGRMIAttrIndex **) 
        CPLRealloc(papoIndexList, sizeof(void*) * nIndexCount);
    
    papoIndexList[nIndexCount-1] = poAttrInd;
}

/************************************************************************/
/*                         GetFieldAttrIndex()                          */
/************************************************************************/

OGRAttrIndex *OGRMILayerAttrIndex::GetFieldIndex( int iField )

{
    for( int i = 0; i < nIndexCount; i++ )
    {
        if( papoIndexList[i]->iField == iField )
            return papoIndexList[i];
    }

    return NULL;
}

/************************************************************************/
/*                             AddToIndex()                             */
/************************************************************************/

OGRErr OGRMILayerAttrIndex::AddToIndex( OGRFeature *poFeature,
                                        int iTargetField )

{
    OGRErr eErr = OGRERR_NONE;

    if( poFeature->GetFID() == OGRNullFID )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Attempt to index feature with no FID." );
        return OGRERR_FAILURE;
    }

    for( int i = 0; i < nIndexCount && eErr == OGRERR_NONE; i++ )
    {
        int iField = papoIndexList[i]->iField;

        if( iTargetField != -1 && iTargetField != iField )
            continue;

        if( !poFeature->IsFieldSet( iField ) )
            continue;

        eErr = 
            papoIndexList[i]->AddEntry( poFeature->GetRawFieldRef( iField ),
                                        poFeature->GetFID() );
    }

    return eErr;
}

/************************************************************************/
/*                          RemoveFromIndex()                           */
/************************************************************************/

OGRErr OGRMILayerAttrIndex::RemoveFromIndex( OGRFeature * /*poFeature*/ )

{
    return OGRERR_UNSUPPORTED_OPERATION;
}

/************************************************************************/
/*                     OGRCreateDefaultLayerIndex()                     */
/************************************************************************/

OGRLayerAttrIndex *OGRCreateDefaultLayerIndex()

{
    return new OGRMILayerAttrIndex();
}

/************************************************************************/
/* ==================================================================== */
/*                            OGRMIAttrIndex                            */
/* ==================================================================== */
/************************************************************************/

/* class declared at top of file */

/************************************************************************/
/*                           OGRMIAttrIndex()                           */
/************************************************************************/

OGRMIAttrIndex::OGRMIAttrIndex( OGRMILayerAttrIndex *poLayerIndex, 
                                int iIndexIn, int iFieldIn )

{
    iIndex = iIndexIn;
    iField = iFieldIn;
    poLIndex = poLayerIndex;
    poINDFile = poLayerIndex->poINDFile;

    poFldDefn = poLayerIndex->GetLayer()->GetLayerDefn()->GetFieldDefn(iField);
}

/************************************************************************/
/*                          ~OGRMIAttrIndex()                           */
/************************************************************************/

OGRMIAttrIndex::~OGRMIAttrIndex()
{
}

/************************************************************************/
/*                              AddEntry()                              */
/************************************************************************/

OGRErr OGRMIAttrIndex::AddEntry( OGRField *psKey, long nFID )

{
    GByte *pabyKey = BuildKey( psKey );

    if( psKey == NULL )
        return OGRERR_FAILURE;

    if( poINDFile->AddEntry( iIndex, pabyKey, nFID+1 ) != 0 )
        return OGRERR_FAILURE;
    else
        return OGRERR_NONE;
}

/************************************************************************/
/*                            RemoveEntry()                             */
/************************************************************************/

OGRErr OGRMIAttrIndex::RemoveEntry( OGRField * /*psKey*/, long /*nFID*/ )

{
    return OGRERR_UNSUPPORTED_OPERATION;
}

/************************************************************************/
/*                              BuildKey()                              */
/************************************************************************/

GByte *OGRMIAttrIndex::BuildKey( OGRField *psKey )

{
    switch( poFldDefn->GetType() )
    {
      case OFTInteger:
        return poINDFile->BuildKey( iIndex, psKey->Integer );
        break;

      case OFTReal:
        return poINDFile->BuildKey( iIndex, psKey->Real );
        break;

      case OFTString:
        return poINDFile->BuildKey( iIndex, psKey->String );
        break;

      default:
        CPLAssert( FALSE );

        return NULL;
    }
}

/************************************************************************/
/*                           GetFirstMatch()                            */
/************************************************************************/

long OGRMIAttrIndex::GetFirstMatch( OGRField *psKey )

{
    GByte *pabyKey = BuildKey( psKey );
    long nFID;

    nFID = poINDFile->FindFirst( iIndex, pabyKey );
    if( nFID < 1 )
        return OGRNullFID;
    else
        return nFID - 1;
}

/************************************************************************/
/*                           GetAllMatches()                            */
/************************************************************************/

long *OGRMIAttrIndex::GetAllMatches( OGRField *psKey, long* panFIDList, int* nFIDCount, int* nLength )
{
    GByte *pabyKey = BuildKey( psKey );
    long nFID;

    if (panFIDList == NULL)
    {
        panFIDList = (long *) CPLMalloc(sizeof(long) * 2);
        *nFIDCount = 0;
        *nLength = 2;
    }

    nFID = poINDFile->FindFirst( iIndex, pabyKey );
    while( nFID > 0 )
    {
        if( *nFIDCount >= *nLength-1 )
        {
            *nLength = (*nLength) * 2 + 10;
            panFIDList = (long *) CPLRealloc(panFIDList, sizeof(long)* (*nLength));
        }
        panFIDList[(*nFIDCount)++] = nFID - 1;
        
        nFID = poINDFile->FindNext( iIndex, pabyKey );
    }

    panFIDList[*nFIDCount] = OGRNullFID;
    
    return panFIDList;
}

long *OGRMIAttrIndex::GetAllMatches( OGRField *psKey )
{
    int nFIDCount, nLength;
    return GetAllMatches( psKey, NULL, &nFIDCount, &nLength );
}

/************************************************************************/
/*                               Clear()                                */
/************************************************************************/

OGRErr OGRMIAttrIndex::Clear()

{
    return OGRERR_UNSUPPORTED_OPERATION;
}
