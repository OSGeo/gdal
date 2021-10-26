/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements interface to MapInfo .ID files used as attribute
 *           indexes.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2003, Frank Warmerdam
 * Copyright (c) 2008-2010, Even Rouault <even dot rouault at spatialys.com>
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

CPL_CVSID("$Id$")

/************************************************************************/
/*                            OGRMIAttrIndex                            */
/*                                                                      */
/*      MapInfo .ID implementation of access to one fields              */
/*      indexing.                                                       */
/************************************************************************/

class OGRMILayerAttrIndex;

class OGRMIAttrIndex : public OGRAttrIndex
{
    CPL_DISALLOW_COPY_ASSIGN(OGRMIAttrIndex)

public:
    int         iIndex;
    TABINDFile  *poINDFile;
    OGRMILayerAttrIndex *poLIndex;
    OGRFieldDefn *poFldDefn;

    int         iField;

                OGRMIAttrIndex( OGRMILayerAttrIndex *, int iIndex, int iField);
               ~OGRMIAttrIndex();

    GByte      *BuildKey( OGRField *psKey );
    GIntBig     GetFirstMatch( OGRField *psKey ) override;
    GIntBig    *GetAllMatches( OGRField *psKey ) override;
    GIntBig    *GetAllMatches( OGRField *psKey, GIntBig* panFIDList, int* nFIDCount, int* nLength ) override;

    OGRErr      AddEntry( OGRField *psKey, GIntBig nFID ) override;
    OGRErr      RemoveEntry( OGRField *psKey, GIntBig nFID ) override;

    OGRErr      Clear() override;
};

/************************************************************************/
/* ==================================================================== */
/*                         OGRMILayerAttrIndex                          */
/*                                                                      */
/*      MapInfo .ID specific implementation of a layer attribute        */
/*      index.                                                          */
/* ==================================================================== */
/************************************************************************/

class OGRMILayerAttrIndex final: public OGRLayerAttrIndex
{
    CPL_DISALLOW_COPY_ASSIGN(OGRMILayerAttrIndex)

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
    OGRErr      Initialize( const char *pszIndexPath, OGRLayer * ) override;
    OGRErr      CreateIndex( int iField ) override;
    OGRErr      DropIndex( int iField ) override;
    OGRErr      IndexAllFeatures( int iField = -1 ) override;

    OGRErr      AddToIndex( OGRFeature *poFeature, int iField = -1 ) override;
    OGRErr      RemoveFromIndex( OGRFeature *poFeature ) override;

    OGRAttrIndex *GetFieldIndex( int iField ) override;

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

OGRMILayerAttrIndex::OGRMILayerAttrIndex() :
    poINDFile(nullptr),
    nIndexCount(0),
    papoIndexList(nullptr),
    pszMetadataFilename(nullptr),
    pszMIINDFilename(nullptr),
    bINDAsReadOnly(TRUE),
    bUnlinkINDFile(FALSE)
{}

/************************************************************************/
/*                        ~OGRMILayerAttrIndex()                        */
/************************************************************************/

OGRMILayerAttrIndex::~OGRMILayerAttrIndex()

{
    if( poINDFile != nullptr )
    {
        poINDFile->Close();
        delete poINDFile;
        poINDFile = nullptr;
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
    if (STARTS_WITH_CI(pszIndexPathIn, "<OGRMILayerAttrIndex>"))
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

    if( psRoot == nullptr )
        return OGRERR_FAILURE;

/* -------------------------------------------------------------------- */
/*      Open the index file.                                            */
/* -------------------------------------------------------------------- */
    poINDFile = new TABINDFile();

    if (pszMIINDFilename == nullptr)
        pszMIINDFilename = CPLStrdup(CPLGetXMLValue(psRoot,"MIIDFilename",""));

    if( pszMIINDFilename == nullptr )
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
    for( CPLXMLNode *psAttrIndex = psRoot->psChild;
         psAttrIndex != nullptr;
         psAttrIndex = psAttrIndex->psNext )
    {
        if( psAttrIndex->eType != CXT_Element
            || !EQUAL(psAttrIndex->pszValue,"OGRMIAttrIndex") )
            continue;

        int iField = atoi(CPLGetXMLValue(psAttrIndex,"FieldIndex","-1"));
        int iIndexIndex = atoi(CPLGetXMLValue(psAttrIndex,"IndexIndex","-1"));

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
              pszMetadataFilename ? pszMetadataFilename : "--unknown--",
              pszMIINDFilename );

    return OGRERR_NONE;
}

OGRErr OGRMILayerAttrIndex::LoadConfigFromXML()
{
    CPLAssert( poINDFile == nullptr );

/* -------------------------------------------------------------------- */
/*      Read the XML file.                                              */
/* -------------------------------------------------------------------- */
    VSILFILE *fp = VSIFOpenL( pszMetadataFilename, "rb" );
    if( fp == nullptr )
        return OGRERR_FAILURE;

    if( VSIFSeekL( fp, 0, SEEK_END ) != 0 )
    {
        VSIFCloseL(fp);
        return OGRERR_FAILURE;
    }
    const vsi_l_offset nXMLSize = VSIFTellL( fp );
    if( nXMLSize > 10 * 1024 * 1024 ||
        VSIFSeekL( fp, 0, SEEK_SET ) != 0 )
    {
        VSIFCloseL(fp);
        return OGRERR_FAILURE;
    }

    char *pszRawXML = static_cast<char *>(CPLMalloc(static_cast<size_t>(nXMLSize)+1));
    pszRawXML[nXMLSize] = '\0';
    if( VSIFReadL( pszRawXML, static_cast<size_t>(nXMLSize), 1, fp ) != 1 )
    {
        VSIFCloseL(fp);
        return OGRERR_FAILURE;
    }

    VSIFCloseL( fp );

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
    CPLXMLNode *psRoot =
        CPLCreateXMLNode( nullptr, CXT_Element, "OGRMILayerAttrIndex" );

    CPLCreateXMLElementAndValue( psRoot, "MIIDFilename",
                                 CPLGetFilename( pszMIINDFilename ) );

    for( int i = 0; i < nIndexCount; i++ )
    {
        OGRMIAttrIndex *poAI = papoIndexList[i];
        CPLXMLNode *psIndex =
            CPLCreateXMLNode( psRoot, CXT_Element, "OGRMIAttrIndex" );

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

    CPLDestroyXMLNode( psRoot );

    FILE *fp = VSIFOpen( pszMetadataFilename, "wb" );
    if( fp == nullptr )
    {
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "Failed to pen `%s' for write.",
                  pszMetadataFilename );
        CPLFree( pszRawXML );
        return OGRERR_FAILURE;
    }

    OGRErr eErr = (VSIFWrite( pszRawXML, strlen(pszRawXML), 1, fp ) == 1) ? OGRERR_NONE : OGRERR_FAILURE;
    VSIFClose( fp );

    CPLFree( pszRawXML );

    return eErr;
}

/************************************************************************/
/*                          IndexAllFeatures()                          */
/************************************************************************/

OGRErr OGRMILayerAttrIndex::IndexAllFeatures( int iField )

{
    poLayer->ResetReading();

    OGRFeature *poFeature = nullptr;
    while( (poFeature = poLayer->GetNextFeature()) != nullptr )
    {
        const OGRErr eErr = AddToIndex( poFeature, iField );

        delete poFeature;

        if( eErr != OGRERR_NONE )
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
    if( poINDFile == nullptr )
    {
        poINDFile = new TABINDFile();
        if( poINDFile->Open( pszMIINDFilename, "w+" ) != 0 )
        {
            delete poINDFile;
            poINDFile = nullptr;

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
                poINDFile = nullptr;
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
    OGRFieldDefn *poFldDefn=poLayer->GetLayerDefn()->GetFieldDefn(iField);

    for( int i = 0; i < nIndexCount; i++ )
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
    const int iINDIndex = poINDFile->CreateIndex( eTABFT, nFieldWidth );

    // CreateIndex() reports its own errors.
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
    OGRFieldDefn *poFldDefn=poLayer->GetLayerDefn()->GetFieldDefn(iField);

    int i = 0;
    for( ; i < nIndexCount; i++ )
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
    papoIndexList = static_cast<OGRMIAttrIndex **>(
        CPLRealloc(papoIndexList, sizeof(void*) * nIndexCount));

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

    return nullptr;
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

        if( !poFeature->IsFieldSetAndNotNull( iField ) )
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
                                int iIndexIn, int iFieldIn ) :
    iIndex(iIndexIn),
    poINDFile(poLayerIndex->poINDFile),
    poLIndex(poLayerIndex),
    poFldDefn(poLayerIndex->GetLayer()->GetLayerDefn()->GetFieldDefn(iFieldIn)),
    iField(iFieldIn)
{}

/************************************************************************/
/*                          ~OGRMIAttrIndex()                           */
/************************************************************************/

OGRMIAttrIndex::~OGRMIAttrIndex()
{
}

/************************************************************************/
/*                              AddEntry()                              */
/************************************************************************/

OGRErr OGRMIAttrIndex::AddEntry( OGRField *psKey, GIntBig nFID )

{
    if( psKey == nullptr )
        return OGRERR_FAILURE;

    if( nFID >= INT_MAX )
        return OGRERR_FAILURE;

    GByte *pabyKey = BuildKey( psKey );

    if( pabyKey == nullptr )
        return OGRERR_FAILURE;

    if( poINDFile->AddEntry( iIndex, pabyKey, static_cast<int>(nFID)+1 ) != 0 )
        return OGRERR_FAILURE;
    else
        return OGRERR_NONE;
}

/************************************************************************/
/*                            RemoveEntry()                             */
/************************************************************************/

OGRErr OGRMIAttrIndex::RemoveEntry( OGRField * /*psKey*/, GIntBig /*nFID*/ )

{
    return OGRERR_UNSUPPORTED_OPERATION;
}

/************************************************************************/
/*                              BuildKey()                              */
/************************************************************************/

GByte *OGRMIAttrIndex::BuildKey( OGRField *psKey )

{
    GByte* ret = nullptr;
    switch( poFldDefn->GetType() )
    {
      case OFTInteger:
        ret = poINDFile->BuildKey( iIndex, psKey->Integer );
        break;

      case OFTInteger64:
      {
        if( !CPL_INT64_FITS_ON_INT32(psKey->Integer64) )
        {
            CPLError(CE_Warning, CPLE_NotSupported,
                     "64bit integer value passed to OGRMIAttrIndex::BuildKey()");
        }
        ret = poINDFile->BuildKey( iIndex, static_cast<int>(psKey->Integer64) );
        break;
      }

      case OFTReal:
        ret = poINDFile->BuildKey( iIndex, psKey->Real );
        break;

      case OFTString:
        ret = poINDFile->BuildKey( iIndex, psKey->String );
        break;

      default:
        CPLAssert( false );
        break;
    }
    return ret;
}

/************************************************************************/
/*                           GetFirstMatch()                            */
/************************************************************************/

GIntBig OGRMIAttrIndex::GetFirstMatch( OGRField *psKey )

{
    GByte *pabyKey = BuildKey( psKey );
    const GIntBig nFID = poINDFile->FindFirst( iIndex, pabyKey );
    if( nFID < 1 )
        return OGRNullFID;
    else
        return nFID - 1;
}

/************************************************************************/
/*                           GetAllMatches()                            */
/************************************************************************/

GIntBig *OGRMIAttrIndex::GetAllMatches( OGRField *psKey, GIntBig* panFIDList, int* nFIDCount, int* nLength )
{
    GByte *pabyKey = BuildKey( psKey );

    if (panFIDList == nullptr)
    {
        panFIDList = static_cast<GIntBig *>(CPLMalloc(sizeof(GIntBig) * 2));
        *nFIDCount = 0;
        *nLength = 2;
    }

    GIntBig nFID = poINDFile->FindFirst( iIndex, pabyKey );
    while( nFID > 0 )
    {
        if( *nFIDCount >= *nLength-1 )
        {
            *nLength = (*nLength) * 2 + 10;
            panFIDList = static_cast<GIntBig *>(CPLRealloc(panFIDList, sizeof(GIntBig)* (*nLength)));
        }
        panFIDList[(*nFIDCount)++] = nFID - 1;

        nFID = poINDFile->FindNext( iIndex, pabyKey );
    }

    panFIDList[*nFIDCount] = OGRNullFID;

    return panFIDList;
}

GIntBig *OGRMIAttrIndex::GetAllMatches( OGRField *psKey )
{
    int nFIDCount, nLength;
    return GetAllMatches( psKey, nullptr, &nFIDCount, &nLength );
}

/************************************************************************/
/*                               Clear()                                */
/************************************************************************/

OGRErr OGRMIAttrIndex::Clear()

{
    return OGRERR_UNSUPPORTED_OPERATION;
}
