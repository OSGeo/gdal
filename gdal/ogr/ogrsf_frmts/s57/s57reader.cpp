/******************************************************************************
 *
 * Project:  S-57 Translator
 * Purpose:  Implements S57Reader class.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999, 2001, Frank Warmerdam
 * Copyright (c) 2009-2013, Even Rouault <even dot rouault at mines-paris dot org>
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

#include "cpl_conv.h"
#include "cpl_string.h"
#include "ogr_api.h"
#include "s57.h"

#include <cmath>

#include <algorithm>
#include <string>

CPL_CVSID("$Id$")

/**
* Recode the given string from a source encoding to UTF-8 encoding.  The source
* encoding is established by inspecting the AALL and NALL fields of the S57
* DSSI record. If first time, the DSSI is read to setup appropriate
* variables. Main scope of this function is to have the strings of all
* attributes encoded/recoded to the same codepage in the final Shapefiles .DBF.
*
* @param[in] SourceString source string to be recoded to UTF-8.
*     LookAtAALL-NALL: flag indicating if the string becomes from an
*     international attribute (e.g.  INFORM, OBJNAM) or national attribute (e.g
*     NINFOM, NOBJNM). The type of encoding is contained in two different
*     fields of the S57 DSSI record: AALL for the international attributes,
*     NAAL for the national ones, so depending on the type of encoding,
*     different fields must be checked to fetch in which way the source string
*     is encoded.
*
*     0: the type of endoding is for international attributes
*     1: the type of endoding is for national attributes
*
* @param[in] LookAtAALL_NALL to be documented
*
* @return the output string recoded to UTF-8 or left unchanged if no valid
*     recoding applicable. The recodinf relies on GDAL functions appropriately
*     called, which allocate themselves the necessary memory to hold the
*     recoded string.
* NOTE: Aall variable is currently not used.
*******************************************************************************/
char *S57Reader::RecodeByDSSI(const char *SourceString, bool LookAtAALL_NALL)
{
    if(needAallNallSetup==true)
    {
        OGRFeature *dsidFeature=ReadDSID();
        if( dsidFeature == NULL )
            return CPLStrdup(SourceString);
        Aall=dsidFeature->GetFieldAsInteger("DSSI_AALL");
        Nall=dsidFeature->GetFieldAsInteger("DSSI_NALL");
        CPLDebug("S57", "DSSI_AALL = %d, DSSI_NALL = %d", Aall, Nall);
        needAallNallSetup=false;
        delete dsidFeature;
    }

    char *RecodedString = NULL;
    if(!LookAtAALL_NALL)
    {
        // In case of international attributes, only ISO8859-1 code page is
        // used (standard ascii). The result is identical to the source string
        // if it contains 0..127 ascii code (LL0), can slightly differ if it
        // contains diacritics 0..255 ascii codes (LL1).
        RecodedString = CPLRecode(SourceString,CPL_ENC_ISO8859_1,CPL_ENC_UTF8);
    }
    else
    {
        if(Nall==2) //national string encoded in UCS-2
        {
            GByte *pabyStr = reinterpret_cast<GByte *>(
                const_cast<char *>( SourceString ) );

            /* Count the number of characters */
            int i=0;
            while( ! ((pabyStr[2 * i] == DDF_UNIT_TERMINATOR && pabyStr[2 * i + 1] == 0) ||
                      (pabyStr[2 * i] == 0 && pabyStr[2 * i + 1] == 0)) )
                i++;

            wchar_t *wideString
                = static_cast<wchar_t*>( CPLMalloc((i+1) * sizeof(wchar_t)) );
            i = 0;
            bool bLittleEndian = true;

            /* Skip BOM */
            if( pabyStr[0] == 0xFF && pabyStr[1] == 0xFE )
                i++;
            else if( pabyStr[0] == 0xFE && pabyStr[1] == 0xFF )
            {
                bLittleEndian = false;
                i++;
            }

            int j=0;
            while( ! ((pabyStr[2 * i] == DDF_UNIT_TERMINATOR && pabyStr[2 * i + 1] == 0) ||
                      (pabyStr[2 * i] == 0 && pabyStr[2 * i + 1] == 0)) )
            {
                if( bLittleEndian )
                    wideString[j++] = pabyStr[i * 2] | (pabyStr[i * 2 + 1] << 8);
                else
                    wideString[j++] = pabyStr[i * 2 + 1] | (pabyStr[i * 2] << 8);
                i++;
            }
            wideString[j] = 0;
            RecodedString = CPLRecodeFromWChar(wideString,CPL_ENC_UCS2,CPL_ENC_UTF8);
            CPLFree(wideString);
        }
        else
        {
            // National string encoded as ISO8859-1.
            // See comment for above on LL0/LL1).
            RecodedString = CPLRecode(SourceString,CPL_ENC_ISO8859_1,CPL_ENC_UTF8);
        }
    }

    if( RecodedString == NULL )
        RecodedString = CPLStrdup(SourceString);

    return RecodedString;
}

/************************************************************************/
/*                             S57Reader()                              */
/************************************************************************/

S57Reader::S57Reader( const char * pszFilename ) :
    poRegistrar(NULL),
    poClassContentExplorer(NULL),
    nFDefnCount(0),
    papoFDefnList(NULL),
    pszModuleName(CPLStrdup( pszFilename )),
    pszDSNM(NULL),
    poModule(NULL),
    nCOMF(1000000),
    nSOMF(10),
    bFileIngested(false),
    nNextVIIndex(0),
    nNextVCIndex(0),
    nNextVEIndex(0),
    nNextVFIndex(0),
    nNextFEIndex(0),
    nNextDSIDIndex(0),
    poDSIDRecord(NULL),
    poDSPMRecord(NULL),
    papszOptions(NULL),
    nOptionFlags(S57M_UPDATES),
    iPointOffset(0),
    poMultiPoint(NULL),
    Aall(0),  // See RecodeByDSSI() function.
    Nall(0),  // See RecodeByDSSI() function.
    needAallNallSetup(true),  // See RecodeByDSSI() function.
    bMissingWarningIssued(false),
    bAttrWarningIssued(false)
{
    szUPDNUpdate[0] = '\0';
}

/************************************************************************/
/*                             ~S57Reader()                             */
/************************************************************************/

S57Reader::~S57Reader()

{
    Close();

    CPLFree( pszModuleName );
    CSLDestroy( papszOptions );

    CPLFree( papoFDefnList );
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

int S57Reader::Open( int bTestOpen )

{
    if( poModule != NULL )
    {
        Rewind();
        return TRUE;
    }

    poModule = new DDFModule();
    if( !poModule->Open( pszModuleName ) )
    {
        // notdef: test bTestOpen.
        delete poModule;
        poModule = NULL;
        return FALSE;
    }

    // note that the following won't work for catalogs.
    if( poModule->FindFieldDefn("DSID") == NULL )
    {
        if( !bTestOpen )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "%s is an ISO8211 file, but not an S-57 data file.\n",
                      pszModuleName );
        }
        delete poModule;
        poModule = NULL;
        return FALSE;
    }

    // Make sure the FSPT field is marked as repeating.
    DDFFieldDefn *poFSPT = poModule->FindFieldDefn( "FSPT" );
    if( poFSPT != NULL && !poFSPT->IsRepeating() )
    {
        CPLDebug( "S57", "Forcing FSPT field to be repeating." );
        poFSPT->SetRepeatingFlag( TRUE );
    }

    nNextFEIndex = 0;
    nNextVIIndex = 0;
    nNextVCIndex = 0;
    nNextVEIndex = 0;
    nNextVFIndex = 0;
    nNextDSIDIndex = 0;

    return TRUE;
}

/************************************************************************/
/*                               Close()                                */
/************************************************************************/

void S57Reader::Close()

{
    if( poModule != NULL )
    {
        oVI_Index.Clear();
        oVC_Index.Clear();
        oVE_Index.Clear();
        oVF_Index.Clear();
        oFE_Index.Clear();

        if( poDSIDRecord != NULL )
        {
            delete poDSIDRecord;
            poDSIDRecord = NULL;
        }
        if( poDSPMRecord != NULL )
        {
            delete poDSPMRecord;
            poDSPMRecord = NULL;
        }

        ClearPendingMultiPoint();

        delete poModule;
        poModule = NULL;

        bFileIngested = false;

        CPLFree( pszDSNM );
        pszDSNM = NULL;
    }
}

/************************************************************************/
/*                       ClearPendingMultiPoint()                       */
/************************************************************************/

void S57Reader::ClearPendingMultiPoint()

{
    if( poMultiPoint != NULL )
    {
        delete poMultiPoint;
        poMultiPoint = NULL;
    }
}

/************************************************************************/
/*                       NextPendingMultiPoint()                        */
/************************************************************************/

OGRFeature *S57Reader::NextPendingMultiPoint()

{
    CPLAssert( poMultiPoint != NULL );
    CPLAssert( wkbFlatten(poMultiPoint->GetGeometryRef()->getGeometryType())
                                                        == wkbMultiPoint );

    OGRFeatureDefn *poDefn = poMultiPoint->GetDefnRef();
    OGRFeature  *poPoint = new OGRFeature( poDefn );
    OGRMultiPoint *poMPGeom
        = static_cast<OGRMultiPoint *>( poMultiPoint->GetGeometryRef() );

    poPoint->SetFID( poMultiPoint->GetFID() );

    for( int i = 0; i < poDefn->GetFieldCount(); i++ )
    {
        poPoint->SetField( i, poMultiPoint->GetRawFieldRef(i) );
    }

    OGRPoint *poSrcPoint
        = static_cast<OGRPoint *>( poMPGeom->getGeometryRef( iPointOffset++ ) );
    CPLAssert( poSrcPoint != NULL );
    poPoint->SetGeometry( poSrcPoint );

    if( (nOptionFlags & S57M_ADD_SOUNDG_DEPTH) )
        poPoint->SetField( "DEPTH", poSrcPoint->getZ() );

    if( iPointOffset >= poMPGeom->getNumGeometries() )
        ClearPendingMultiPoint();

    return poPoint;
}

/************************************************************************/
/*                             SetOptions()                             */
/************************************************************************/

bool S57Reader::SetOptions( char ** papszOptionsIn )

{
    CSLDestroy( papszOptions );
    papszOptions = CSLDuplicate( papszOptionsIn );

    const char *pszOptionValue
        = CSLFetchNameValue( papszOptions, S57O_SPLIT_MULTIPOINT );
    if( pszOptionValue != NULL && CPLTestBool(pszOptionValue) )
        nOptionFlags |= S57M_SPLIT_MULTIPOINT;
    else
        nOptionFlags &= ~S57M_SPLIT_MULTIPOINT;

    pszOptionValue = CSLFetchNameValue( papszOptions, S57O_ADD_SOUNDG_DEPTH );
    if( pszOptionValue != NULL && CPLTestBool(pszOptionValue) )
        nOptionFlags |= S57M_ADD_SOUNDG_DEPTH;
    else
        nOptionFlags &= ~S57M_ADD_SOUNDG_DEPTH;

    if( (nOptionFlags & S57M_ADD_SOUNDG_DEPTH) &&
        !(nOptionFlags & S57M_SPLIT_MULTIPOINT) )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Inconsistent options : ADD_SOUNDG_DEPTH should only be "
                 "enabled if SPLIT_MULTIPOINT is also enabled");
        return false;
    }

    pszOptionValue = CSLFetchNameValue( papszOptions, S57O_LNAM_REFS );
    if( pszOptionValue != NULL && CPLTestBool(pszOptionValue) )
        nOptionFlags |= S57M_LNAM_REFS;
    else
        nOptionFlags &= ~S57M_LNAM_REFS;

    pszOptionValue = CSLFetchNameValue( papszOptions, S57O_UPDATES );
    if( pszOptionValue == NULL )
        /* no change */;
    else if( pszOptionValue != NULL && !EQUAL(pszOptionValue,"APPLY") )
        nOptionFlags &= ~S57M_UPDATES;
    else
        nOptionFlags |= S57M_UPDATES;

    pszOptionValue = CSLFetchNameValue(papszOptions,
                                       S57O_PRESERVE_EMPTY_NUMBERS);
    if( pszOptionValue != NULL && CPLTestBool(pszOptionValue) )
        nOptionFlags |= S57M_PRESERVE_EMPTY_NUMBERS;
    else
        nOptionFlags &= ~S57M_PRESERVE_EMPTY_NUMBERS;

    pszOptionValue = CSLFetchNameValue( papszOptions, S57O_RETURN_PRIMITIVES );
    if( pszOptionValue != NULL && CPLTestBool(pszOptionValue) )
        nOptionFlags |= S57M_RETURN_PRIMITIVES;
    else
        nOptionFlags &= ~S57M_RETURN_PRIMITIVES;

    pszOptionValue = CSLFetchNameValue( papszOptions, S57O_RETURN_LINKAGES );
    if( pszOptionValue != NULL && CPLTestBool(pszOptionValue) )
        nOptionFlags |= S57M_RETURN_LINKAGES;
    else
        nOptionFlags &= ~S57M_RETURN_LINKAGES;

    pszOptionValue = CSLFetchNameValue( papszOptions, S57O_RETURN_DSID );
    if( pszOptionValue == NULL || CPLTestBool(pszOptionValue) )
        nOptionFlags |= S57M_RETURN_DSID;
    else
        nOptionFlags &= ~S57M_RETURN_DSID;

    pszOptionValue = CSLFetchNameValue( papszOptions, S57O_RECODE_BY_DSSI );
    if( pszOptionValue != NULL && CPLTestBool(pszOptionValue) )
        nOptionFlags |= S57M_RECODE_BY_DSSI;
    else
        nOptionFlags &= ~S57M_RECODE_BY_DSSI;

    return true;
}

/************************************************************************/
/*                           SetClassBased()                            */
/************************************************************************/

void S57Reader::SetClassBased( S57ClassRegistrar * poReg,
                               S57ClassContentExplorer* poClassContentExplorerIn )

{
    poRegistrar = poReg;
    poClassContentExplorer = poClassContentExplorerIn;
}

/************************************************************************/
/*                               Rewind()                               */
/************************************************************************/

void S57Reader::Rewind()

{
    ClearPendingMultiPoint();
    nNextFEIndex = 0;
    nNextVIIndex = 0;
    nNextVCIndex = 0;
    nNextVEIndex = 0;
    nNextVFIndex = 0;
    nNextDSIDIndex = 0;
}

/************************************************************************/
/*                               Ingest()                               */
/*                                                                      */
/*      Read all the records into memory, adding to the appropriate     */
/*      indexes.                                                        */
/************************************************************************/

bool S57Reader::Ingest()

{
    if( poModule == NULL || bFileIngested )
        return true;

/* -------------------------------------------------------------------- */
/*      Read all the records in the module, and place them in           */
/*      appropriate indexes.                                            */
/* -------------------------------------------------------------------- */
    CPLErrorReset();
    DDFRecord *poRecord = NULL;
    while( (poRecord = poModule->ReadRecord()) != NULL )
    {
        DDFField *poKeyField = poRecord->GetField(1);
        if (poKeyField == NULL)
            return false;
        DDFFieldDefn* poKeyFieldDefn = poKeyField->GetFieldDefn();
        if( poKeyFieldDefn == NULL )
            continue;
        const char* pszName = poKeyFieldDefn->GetName();
        if( pszName == NULL )
            continue;

        if( EQUAL(pszName,"VRID") )
        {
            const int nRCNM = poRecord->GetIntSubfield( "VRID",0, "RCNM",0 );
            const int nRCID = poRecord->GetIntSubfield( "VRID",0, "RCID",0 );

            switch( nRCNM )
            {
              case RCNM_VI:
                oVI_Index.AddRecord( nRCID, poRecord->Clone() );
                break;

              case RCNM_VC:
                oVC_Index.AddRecord( nRCID, poRecord->Clone() );
                break;

              case RCNM_VE:
                oVE_Index.AddRecord( nRCID, poRecord->Clone() );
                break;

              case RCNM_VF:
                oVF_Index.AddRecord( nRCID, poRecord->Clone() );
                break;

              default:
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Unhandled value for RCNM ; %d", nRCNM);
                break;
            }
        }

        else if( EQUAL(pszName,"FRID") )
        {
            int         nRCID = poRecord->GetIntSubfield( "FRID",0, "RCID",0);

            oFE_Index.AddRecord( nRCID, poRecord->Clone() );
        }

        else if( EQUAL(pszName,"DSID") )
        {
            CPLFree( pszDSNM );
            pszDSNM =
                CPLStrdup(poRecord->GetStringSubfield( "DSID", 0, "DSNM", 0 ));

            if( nOptionFlags & S57M_RETURN_DSID )
            {
                if( poDSIDRecord != NULL )
                    delete poDSIDRecord;

                poDSIDRecord = poRecord->Clone();
            }
        }

        else if( EQUAL(pszName,"DSPM") )
        {
            nCOMF = std::max(1, poRecord->GetIntSubfield( "DSPM",0, "COMF",0));
            nSOMF = std::max(1, poRecord->GetIntSubfield( "DSPM",0, "SOMF",0));

            if( nOptionFlags & S57M_RETURN_DSID )
            {
                if( poDSPMRecord != NULL )
                    delete poDSPMRecord;

                poDSPMRecord = poRecord->Clone();
            }
        }

        else
        {
            CPLDebug( "S57",
                      "Skipping %s record in S57Reader::Ingest().",
                      pszName );
        }
    }

    if( CPLGetLastErrorType() == CE_Failure )
        return false;

    bFileIngested = true;

/* -------------------------------------------------------------------- */
/*      If update support is enabled, read and apply them.              */
/* -------------------------------------------------------------------- */
    if( nOptionFlags & S57M_UPDATES )
        return FindAndApplyUpdates();

    return true;
}

/************************************************************************/
/*                           SetNextFEIndex()                           */
/************************************************************************/

void S57Reader::SetNextFEIndex( int nNewIndex, int nRCNM )

{
    if( nRCNM == RCNM_VI )
        nNextVIIndex = nNewIndex;
    else if( nRCNM == RCNM_VC )
        nNextVCIndex = nNewIndex;
    else if( nRCNM == RCNM_VE )
        nNextVEIndex = nNewIndex;
    else if( nRCNM == RCNM_VF )
        nNextVFIndex = nNewIndex;
    else if( nRCNM == RCNM_DSID )
        nNextDSIDIndex = nNewIndex;
    else
    {
        if( nNextFEIndex != nNewIndex )
            ClearPendingMultiPoint();

        nNextFEIndex = nNewIndex;
    }
}

/************************************************************************/
/*                           GetNextFEIndex()                           */
/************************************************************************/

int S57Reader::GetNextFEIndex( int nRCNM )

{
    if( nRCNM == RCNM_VI )
        return nNextVIIndex;
    if( nRCNM == RCNM_VC )
        return nNextVCIndex;
    if( nRCNM == RCNM_VE )
        return nNextVEIndex;
    if( nRCNM == RCNM_VF )
        return nNextVFIndex;
    if( nRCNM == RCNM_DSID )
        return nNextDSIDIndex;

    return nNextFEIndex;
}

/************************************************************************/
/*                          ReadNextFeature()                           */
/************************************************************************/

OGRFeature * S57Reader::ReadNextFeature( OGRFeatureDefn * poTarget )

{
    if( !bFileIngested && !Ingest() )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Special case for "in progress" multipoints being split up.      */
/* -------------------------------------------------------------------- */
    if( poMultiPoint != NULL )
    {
        if( poTarget == NULL || poTarget == poMultiPoint->GetDefnRef() )
        {
            return NextPendingMultiPoint();
        }
        else
        {
            ClearPendingMultiPoint();
        }
    }

/* -------------------------------------------------------------------- */
/*      Next vector feature?                                            */
/* -------------------------------------------------------------------- */
    if( (nOptionFlags & S57M_RETURN_DSID)
        && nNextDSIDIndex == 0
        && (poTarget == NULL || EQUAL(poTarget->GetName(),"DSID")) )
    {
        return ReadDSID();
    }

/* -------------------------------------------------------------------- */
/*      Next vector feature?                                            */
/* -------------------------------------------------------------------- */
    if( nOptionFlags & S57M_RETURN_PRIMITIVES )
    {
        int nRCNM = 0;
        int *pnCounter = NULL;

        if( poTarget == NULL )
        {
            if( nNextVIIndex < oVI_Index.GetCount() )
            {
                nRCNM = RCNM_VI;
                pnCounter = &nNextVIIndex;
            }
            else if( nNextVCIndex < oVC_Index.GetCount() )
            {
                nRCNM = RCNM_VC;
                pnCounter = &nNextVCIndex;
            }
            else if( nNextVEIndex < oVE_Index.GetCount() )
            {
                nRCNM = RCNM_VE;
                pnCounter = &nNextVEIndex;
            }
            else if( nNextVFIndex < oVF_Index.GetCount() )
            {
                nRCNM = RCNM_VF;
                pnCounter = &nNextVFIndex;
            }
        }
        else
        {
            if( EQUAL(poTarget->GetName(),OGRN_VI) )
            {
                nRCNM = RCNM_VI;
                pnCounter = &nNextVIIndex;
            }
            else if( EQUAL(poTarget->GetName(),OGRN_VC) )
            {
                nRCNM = RCNM_VC;
                pnCounter = &nNextVCIndex;
            }
            else if( EQUAL(poTarget->GetName(),OGRN_VE) )
            {
                nRCNM = RCNM_VE;
                pnCounter = &nNextVEIndex;
            }
            else if( EQUAL(poTarget->GetName(),OGRN_VF) )
            {
                nRCNM = RCNM_VF;
                pnCounter = &nNextVFIndex;
            }
        }

        if( nRCNM != 0 )
        {
            OGRFeature *poFeature = ReadVector( *pnCounter, nRCNM );
            if( poFeature != NULL )
            {
                *pnCounter += 1;
                return poFeature;
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Next feature.                                                   */
/* -------------------------------------------------------------------- */
    while( nNextFEIndex < oFE_Index.GetCount() )
    {
        OGRFeatureDefn *poFeatureDefn
          = static_cast<OGRFeatureDefn *>( oFE_Index.GetClientInfoByIndex( nNextFEIndex ) );

        if( poFeatureDefn == NULL )
        {
            poFeatureDefn = FindFDefn( oFE_Index.GetByIndex( nNextFEIndex ) );
            oFE_Index.SetClientInfoByIndex( nNextFEIndex, poFeatureDefn );
        }

        if( poFeatureDefn != poTarget && poTarget != NULL )
        {
            nNextFEIndex++;
            continue;
        }

        OGRFeature *poFeature = ReadFeature( nNextFEIndex++, poTarget );
        if( poFeature != NULL )
        {
            if( (nOptionFlags & S57M_SPLIT_MULTIPOINT)
                && poFeature->GetGeometryRef() != NULL
                && wkbFlatten(poFeature->GetGeometryRef()->getGeometryType())
                                                        == wkbMultiPoint)
            {
                poMultiPoint = poFeature;
                iPointOffset = 0;
                return NextPendingMultiPoint();
            }

            return poFeature;
        }
    }

    return NULL;
}

/************************************************************************/
/*                            ReadFeature()                             */
/*                                                                      */
/*      Read the features who's id is provided.                         */
/************************************************************************/

OGRFeature *S57Reader::ReadFeature( int nFeatureId, OGRFeatureDefn *poTarget )

{
    if( nFeatureId < 0 || nFeatureId >= oFE_Index.GetCount() )
        return NULL;

    OGRFeature  *poFeature = NULL;

    if( (nOptionFlags & S57M_RETURN_DSID)
        && nFeatureId == 0
        && (poTarget == NULL || EQUAL(poTarget->GetName(),"DSID")) )
    {
        poFeature = ReadDSID();
    }
    else
    {
        poFeature = AssembleFeature( oFE_Index.GetByIndex(nFeatureId),
                                    poTarget );
    }
    if( poFeature != NULL )
        poFeature->SetFID( nFeatureId );

    return poFeature;
}

/************************************************************************/
/*                          AssembleFeature()                           */
/*                                                                      */
/*      Assemble an OGR feature based on a feature record.              */
/************************************************************************/

OGRFeature *S57Reader::AssembleFeature( DDFRecord * poRecord,
                                        OGRFeatureDefn * poTarget )

{
/* -------------------------------------------------------------------- */
/*      Find the feature definition to use.  Currently this is based    */
/*      on the primitive, but eventually this should be based on the    */
/*      object class (FRID.OBJL) in some cases, and the primitive in    */
/*      others.                                                         */
/* -------------------------------------------------------------------- */
    OGRFeatureDefn *poFDefn = FindFDefn( poRecord );
    if( poFDefn == NULL )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Does this match our target feature definition?  If not skip     */
/*      this feature.                                                   */
/* -------------------------------------------------------------------- */
    if( poTarget != NULL && poFDefn != poTarget )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Create the new feature object.                                  */
/* -------------------------------------------------------------------- */
    OGRFeature *poFeature = new OGRFeature( poFDefn );

/* -------------------------------------------------------------------- */
/*      Assign a few standard feature attributes.                        */
/* -------------------------------------------------------------------- */
    int nOBJL = poRecord->GetIntSubfield( "FRID", 0, "OBJL", 0 );
    poFeature->SetField( "OBJL", nOBJL );

    poFeature->SetField( "RCID",
                         poRecord->GetIntSubfield( "FRID", 0, "RCID", 0 ));
    poFeature->SetField( "PRIM",
                         poRecord->GetIntSubfield( "FRID", 0, "PRIM", 0 ));
    poFeature->SetField( "GRUP",
                         poRecord->GetIntSubfield( "FRID", 0, "GRUP", 0 ));
    poFeature->SetField( "RVER",
                         poRecord->GetIntSubfield( "FRID", 0, "RVER", 0 ));
    poFeature->SetField( "AGEN",
                         poRecord->GetIntSubfield( "FOID", 0, "AGEN", 0 ));
    poFeature->SetField( "FIDN",
                         poRecord->GetIntSubfield( "FOID", 0, "FIDN", 0 ));
    poFeature->SetField( "FIDS",
                         poRecord->GetIntSubfield( "FOID", 0, "FIDS", 0 ));

/* -------------------------------------------------------------------- */
/*      Generate long name, if requested.                               */
/* -------------------------------------------------------------------- */
    if( nOptionFlags & S57M_LNAM_REFS )
    {
        GenerateLNAMAndRefs( poRecord, poFeature );
    }

/* -------------------------------------------------------------------- */
/*      Generate primitive references if requested.                     */
/* -------------------------------------------------------------------- */
    if( nOptionFlags & S57M_RETURN_LINKAGES )
        GenerateFSPTAttributes( poRecord, poFeature );

/* -------------------------------------------------------------------- */
/*      Apply object class specific attributes, if supported.           */
/* -------------------------------------------------------------------- */
    if( poRegistrar != NULL )
        ApplyObjectClassAttributes( poRecord, poFeature );

/* -------------------------------------------------------------------- */
/*      Find and assign spatial component.                              */
/* -------------------------------------------------------------------- */
    const int nPRIM = poRecord->GetIntSubfield( "FRID", 0, "PRIM", 0 );

    if( nPRIM == PRIM_P )
    {
        if( nOBJL == 129 ) /* SOUNDG */
            AssembleSoundingGeometry( poRecord, poFeature );
        else
            AssemblePointGeometry( poRecord, poFeature );
    }
    else if( nPRIM == PRIM_L )
    {
        AssembleLineGeometry( poRecord, poFeature );
    }
    else if( nPRIM == PRIM_A )
    {
        AssembleAreaGeometry( poRecord, poFeature );
    }

    return poFeature;
}

/************************************************************************/
/*                     ApplyObjectClassAttributes()                     */
/************************************************************************/

void S57Reader::ApplyObjectClassAttributes( DDFRecord * poRecord,
                                            OGRFeature * poFeature )

{
/* -------------------------------------------------------------------- */
/*      ATTF Attributes                                                 */
/* -------------------------------------------------------------------- */
    DDFField    *poATTF = poRecord->FindField( "ATTF" );

    if( poATTF == NULL )
        return;

    int nAttrCount = poATTF->GetRepeatCount();
    for( int iAttr = 0; iAttr < nAttrCount; iAttr++ )
    {
        const int nAttrId
            = poRecord->GetIntSubfield( "ATTF", 0, "ATTL", iAttr );

        if( poRegistrar->GetAttrInfo(nAttrId) == NULL )
        {
            if( !bAttrWarningIssued )
            {
                bAttrWarningIssued = true;
                CPLError( CE_Warning, CPLE_AppDefined,
                          "Illegal feature attribute id (ATTF:ATTL[%d]) of %d\n"
                          "on feature FIDN=%d, FIDS=%d.\n"
                          "Skipping attribute. "
                          "No more warnings will be issued.",
                          iAttr, nAttrId,
                          poFeature->GetFieldAsInteger( "FIDN" ),
                          poFeature->GetFieldAsInteger( "FIDS" ) );
            }

            continue;
        }

        /* Fetch the attribute value */
        const char *pszValue =
            poRecord->GetStringSubfield("ATTF",0,"ATVL",iAttr);
        if( pszValue == NULL )
            return;

        //If needed, recode the string in UTF-8.
        char* pszValueToFree = NULL;
        if(nOptionFlags & S57M_RECODE_BY_DSSI)
            pszValue = pszValueToFree = RecodeByDSSI(pszValue,false);

        /* Apply to feature in an appropriate way */
        const char *pszAcronym = poRegistrar->GetAttrAcronym(nAttrId);
        const int iField = poFeature->GetDefnRef()->GetFieldIndex(pszAcronym);
        if( iField < 0 )
        {
            if( !bMissingWarningIssued )
            {
                bMissingWarningIssued = true;
                CPLError( CE_Warning, CPLE_AppDefined,
                          "Attributes %s ignored, not in expected schema.\n"
                          "No more warnings will be issued for this dataset.",
                          pszAcronym );
            }
            CPLFree(pszValueToFree);
            continue;
        }

        OGRFieldDefn *poFldDefn
            = poFeature->GetDefnRef()->GetFieldDefn( iField );
        if( poFldDefn->GetType() == OFTInteger
            || poFldDefn->GetType() == OFTReal )
        {
            if( strlen(pszValue) == 0 )
            {
                if( nOptionFlags & S57M_PRESERVE_EMPTY_NUMBERS )
                    poFeature->SetField( iField, EMPTY_NUMBER_MARKER );
                else
                {
                    /* leave as null if value was empty string */
                }
            }
            else
                poFeature->SetField( iField, pszValue );
        }
        else
            poFeature->SetField( iField, pszValue );

        CPLFree(pszValueToFree);
    }

/* -------------------------------------------------------------------- */
/*      NATF (national) attributes                                      */
/* -------------------------------------------------------------------- */
    DDFField    *poNATF = poRecord->FindField( "NATF" );

    if( poNATF == NULL )
        return;

    nAttrCount = poNATF->GetRepeatCount();
    for( int iAttr = 0; iAttr < nAttrCount; iAttr++ )
    {
        const int nAttrId = poRecord->GetIntSubfield("NATF",0,"ATTL",iAttr);
        const char *pszAcronym = poRegistrar->GetAttrAcronym(nAttrId);

        if( pszAcronym == NULL )
        {
            if( !bAttrWarningIssued )
            {
                bAttrWarningIssued = true;
                CPLError( CE_Warning, CPLE_AppDefined,
                          "Illegal feature attribute id (NATF:ATTL[%d]) of %d\n"
                          "on feature FIDN=%d, FIDS=%d.\n"
                          "Skipping attribute, no more warnings will be issued.",
                          iAttr, nAttrId,
                          poFeature->GetFieldAsInteger( "FIDN" ),
                          poFeature->GetFieldAsInteger( "FIDS" ) );
            }

            continue;
        }

        //If needed, recode the string in UTF-8.
        const char *pszValue = poRecord->GetStringSubfield("NATF",0,"ATVL",iAttr);
        if( pszValue != NULL )
        {
            if(nOptionFlags & S57M_RECODE_BY_DSSI)
            {
                char* pszValueRecoded = RecodeByDSSI(pszValue,true);
                poFeature->SetField(pszAcronym,pszValueRecoded);
                CPLFree(pszValueRecoded);
            }
            else
                poFeature->SetField(pszAcronym,pszValue);
        }
    }
}

/************************************************************************/
/*                        GenerateLNAMAndRefs()                         */
/************************************************************************/

void S57Reader::GenerateLNAMAndRefs( DDFRecord * poRecord,
                                     OGRFeature * poFeature )

{
/* -------------------------------------------------------------------- */
/*      Apply the LNAM to the object.                                   */
/* -------------------------------------------------------------------- */
    char szLNAM[32];
    snprintf( szLNAM, sizeof(szLNAM), "%04X%08X%04X",
             poFeature->GetFieldAsInteger( "AGEN" ),
             poFeature->GetFieldAsInteger( "FIDN" ),
             poFeature->GetFieldAsInteger( "FIDS" ) );
    poFeature->SetField( "LNAM", szLNAM );

/* -------------------------------------------------------------------- */
/*      Do we have references to other features.                        */
/* -------------------------------------------------------------------- */
    DDFField *poFFPT = poRecord->FindField( "FFPT" );

    if( poFFPT == NULL )
        return;

/* -------------------------------------------------------------------- */
/*      Apply references.                                               */
/* -------------------------------------------------------------------- */
    const int nRefCount = poFFPT->GetRepeatCount();

    DDFSubfieldDefn *poLNAM
        = poFFPT->GetFieldDefn()->FindSubfieldDefn( "LNAM" );
    DDFSubfieldDefn *poRIND
        = poFFPT->GetFieldDefn()->FindSubfieldDefn( "RIND" );
    if( poLNAM == NULL || poRIND == NULL )
    {
        return;
    }

    int *panRIND = static_cast<int *>( CPLMalloc(sizeof(int) * nRefCount) );
    char **papszRefs = NULL;

    for( int iRef = 0; iRef < nRefCount; iRef++ )
    {
        int nMaxBytes = 0;

        unsigned char *pabyData = reinterpret_cast<unsigned char *>(
            const_cast<char *>(
                poFFPT->GetSubfieldData( poLNAM, &nMaxBytes, iRef ) ) );
        if( pabyData == NULL || nMaxBytes < 8 )
        {
            CSLDestroy( papszRefs );
            CPLFree( panRIND );
            return;
        }

        snprintf( szLNAM, sizeof(szLNAM), "%02X%02X%02X%02X%02X%02X%02X%02X",
                 pabyData[1], pabyData[0], /* AGEN */
                 pabyData[5], pabyData[4], pabyData[3], pabyData[2], /* FIDN */
                 pabyData[7], pabyData[6] );

        papszRefs = CSLAddString( papszRefs, szLNAM );

        pabyData = reinterpret_cast<unsigned char *>(const_cast<char *>(
            poFFPT->GetSubfieldData( poRIND, &nMaxBytes, iRef ) ) );
        if( pabyData == NULL || nMaxBytes < 1 )
        {
            CSLDestroy( papszRefs );
            CPLFree( panRIND );
            return;
        }
        panRIND[iRef] = pabyData[0];
    }

    poFeature->SetField( "LNAM_REFS", papszRefs );
    CSLDestroy( papszRefs );

    poFeature->SetField( "FFPT_RIND", nRefCount, panRIND );
    CPLFree( panRIND );
}

/************************************************************************/
/*                       GenerateFSPTAttributes()                       */
/************************************************************************/

void S57Reader::GenerateFSPTAttributes( DDFRecord * poRecord,
                                        OGRFeature * poFeature )

{
/* -------------------------------------------------------------------- */
/*      Feature the spatial record containing the point.                */
/* -------------------------------------------------------------------- */
    DDFField *poFSPT = poRecord->FindField( "FSPT" );
    if( poFSPT == NULL )
        return;

    const int nCount = poFSPT->GetRepeatCount();

/* -------------------------------------------------------------------- */
/*      Allocate working lists of the attributes.                       */
/* -------------------------------------------------------------------- */
    int * const panORNT = static_cast<int *>( CPLMalloc( sizeof(int) * nCount ) );
    int * const panUSAG = static_cast<int *>( CPLMalloc( sizeof(int) * nCount ) );
    int * const panMASK = static_cast<int *>( CPLMalloc( sizeof(int) * nCount ) );
    int * const panRCNM = static_cast<int *>( CPLMalloc( sizeof(int) * nCount ) );
    int *panRCID = static_cast<int *>( CPLMalloc( sizeof(int) * nCount ) );

/* -------------------------------------------------------------------- */
/*      loop over all entries, decoding them.                           */
/* -------------------------------------------------------------------- */
    for( int i = 0; i < nCount; i++ )
    {
        panRCID[i] = ParseName( poFSPT, i, panRCNM + i );
        panORNT[i] = poRecord->GetIntSubfield( "FSPT", 0, "ORNT",i);
        panUSAG[i] = poRecord->GetIntSubfield( "FSPT", 0, "USAG",i);
        panMASK[i] = poRecord->GetIntSubfield( "FSPT", 0, "MASK",i);
    }

/* -------------------------------------------------------------------- */
/*      Assign to feature.                                              */
/* -------------------------------------------------------------------- */
    poFeature->SetField( "NAME_RCNM", nCount, panRCNM );
    poFeature->SetField( "NAME_RCID", nCount, panRCID );
    poFeature->SetField( "ORNT", nCount, panORNT );
    poFeature->SetField( "USAG", nCount, panUSAG );
    poFeature->SetField( "MASK", nCount, panMASK );

/* -------------------------------------------------------------------- */
/*      Cleanup.                                                        */
/* -------------------------------------------------------------------- */
    CPLFree( panRCNM );
    CPLFree( panRCID );
    CPLFree( panORNT );
    CPLFree( panUSAG );
    CPLFree( panMASK );
}

/************************************************************************/
/*                              ReadDSID()                              */
/************************************************************************/

OGRFeature *S57Reader::ReadDSID()

{
    if( poDSIDRecord == NULL && poDSPMRecord == NULL )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Find the feature definition to use.                             */
/* -------------------------------------------------------------------- */
    OGRFeatureDefn *poFDefn = NULL;

    for( int i = 0; i < nFDefnCount; i++ )
    {
        if( EQUAL(papoFDefnList[i]->GetName(),"DSID") )
        {
            poFDefn = papoFDefnList[i];
            break;
        }
    }

    if( poFDefn == NULL )
    {
        // CPLAssert( false );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Create feature.                                                 */
/* -------------------------------------------------------------------- */
    OGRFeature *poFeature = new OGRFeature( poFDefn );

/* -------------------------------------------------------------------- */
/*      Apply DSID values.                                              */
/* -------------------------------------------------------------------- */
    if( poDSIDRecord != NULL )
    {
        poFeature->SetField( "DSID_EXPP",
                     poDSIDRecord->GetIntSubfield( "DSID", 0, "EXPP", 0 ));
        poFeature->SetField( "DSID_INTU",
                     poDSIDRecord->GetIntSubfield( "DSID", 0, "INTU", 0 ));
        poFeature->SetField( "DSID_DSNM",
                     poDSIDRecord->GetStringSubfield( "DSID", 0, "DSNM", 0 ));
        poFeature->SetField( "DSID_EDTN",
                     poDSIDRecord->GetStringSubfield( "DSID", 0, "EDTN", 0 ));
        if( strlen(szUPDNUpdate) > 0 )
            poFeature->SetField( "DSID_UPDN", szUPDNUpdate );
        else
            poFeature->SetField( "DSID_UPDN",
                     poDSIDRecord->GetStringSubfield( "DSID", 0, "UPDN", 0 ));

        poFeature->SetField( "DSID_UADT",
                     poDSIDRecord->GetStringSubfield( "DSID", 0, "UADT", 0 ));
        poFeature->SetField( "DSID_ISDT",
                     poDSIDRecord->GetStringSubfield( "DSID", 0, "ISDT", 0 ));
        poFeature->SetField( "DSID_STED",
                     poDSIDRecord->GetFloatSubfield( "DSID", 0, "STED", 0 ));
        poFeature->SetField( "DSID_PRSP",
                     poDSIDRecord->GetIntSubfield( "DSID", 0, "PRSP", 0 ));
        poFeature->SetField( "DSID_PSDN",
                     poDSIDRecord->GetStringSubfield( "DSID", 0, "PSDN", 0 ));
        poFeature->SetField( "DSID_PRED",
                     poDSIDRecord->GetStringSubfield( "DSID", 0, "PRED", 0 ));
        poFeature->SetField( "DSID_PROF",
                     poDSIDRecord->GetIntSubfield( "DSID", 0, "PROF", 0 ));
        poFeature->SetField( "DSID_AGEN",
                     poDSIDRecord->GetIntSubfield( "DSID", 0, "AGEN", 0 ));
        poFeature->SetField( "DSID_COMT",
                     poDSIDRecord->GetStringSubfield( "DSID", 0, "COMT", 0 ));

/* -------------------------------------------------------------------- */
/*      Apply DSSI values.                                              */
/* -------------------------------------------------------------------- */
        poFeature->SetField( "DSSI_DSTR",
                     poDSIDRecord->GetIntSubfield( "DSSI", 0, "DSTR", 0 ));
        poFeature->SetField( "DSSI_AALL",
                     poDSIDRecord->GetIntSubfield( "DSSI", 0, "AALL", 0 ));
        poFeature->SetField( "DSSI_NALL",
                     poDSIDRecord->GetIntSubfield( "DSSI", 0, "NALL", 0 ));
        poFeature->SetField( "DSSI_NOMR",
                     poDSIDRecord->GetIntSubfield( "DSSI", 0, "NOMR", 0 ));
        poFeature->SetField( "DSSI_NOCR",
                     poDSIDRecord->GetIntSubfield( "DSSI", 0, "NOCR", 0 ));
        poFeature->SetField( "DSSI_NOGR",
                     poDSIDRecord->GetIntSubfield( "DSSI", 0, "NOGR", 0 ));
        poFeature->SetField( "DSSI_NOLR",
                     poDSIDRecord->GetIntSubfield( "DSSI", 0, "NOLR", 0 ));
        poFeature->SetField( "DSSI_NOIN",
                     poDSIDRecord->GetIntSubfield( "DSSI", 0, "NOIN", 0 ));
        poFeature->SetField( "DSSI_NOCN",
                     poDSIDRecord->GetIntSubfield( "DSSI", 0, "NOCN", 0 ));
        poFeature->SetField( "DSSI_NOED",
                     poDSIDRecord->GetIntSubfield( "DSSI", 0, "NOED", 0 ));
        poFeature->SetField( "DSSI_NOFA",
                     poDSIDRecord->GetIntSubfield( "DSSI", 0, "NOFA", 0 ));
    }

/* -------------------------------------------------------------------- */
/*      Apply DSPM record.                                              */
/* -------------------------------------------------------------------- */
    if( poDSPMRecord != NULL )
    {
        poFeature->SetField( "DSPM_HDAT",
                      poDSPMRecord->GetIntSubfield( "DSPM", 0, "HDAT", 0 ));
        poFeature->SetField( "DSPM_VDAT",
                      poDSPMRecord->GetIntSubfield( "DSPM", 0, "VDAT", 0 ));
        poFeature->SetField( "DSPM_SDAT",
                      poDSPMRecord->GetIntSubfield( "DSPM", 0, "SDAT", 0 ));
        poFeature->SetField( "DSPM_CSCL",
                      poDSPMRecord->GetIntSubfield( "DSPM", 0, "CSCL", 0 ));
        poFeature->SetField( "DSPM_DUNI",
                      poDSPMRecord->GetIntSubfield( "DSPM", 0, "DUNI", 0 ));
        poFeature->SetField( "DSPM_HUNI",
                      poDSPMRecord->GetIntSubfield( "DSPM", 0, "HUNI", 0 ));
        poFeature->SetField( "DSPM_PUNI",
                      poDSPMRecord->GetIntSubfield( "DSPM", 0, "PUNI", 0 ));
        poFeature->SetField( "DSPM_COUN",
                      poDSPMRecord->GetIntSubfield( "DSPM", 0, "COUN", 0 ));
        poFeature->SetField( "DSPM_COMF",
                      poDSPMRecord->GetIntSubfield( "DSPM", 0, "COMF", 0 ));
        poFeature->SetField( "DSPM_SOMF",
                      poDSPMRecord->GetIntSubfield( "DSPM", 0, "SOMF", 0 ));
        poFeature->SetField( "DSPM_COMT",
                      poDSPMRecord->GetStringSubfield( "DSPM", 0, "COMT", 0 ));
    }

    poFeature->SetFID( nNextDSIDIndex++ );

    return poFeature;
}

/************************************************************************/
/*                             ReadVector()                             */
/*                                                                      */
/*      Read a vector primitive objects based on the type (RCNM_)       */
/*      and index within the related index.                             */
/************************************************************************/

OGRFeature *S57Reader::ReadVector( int nFeatureId, int nRCNM )

{
    DDFRecordIndex *poIndex = NULL;
    const char *pszFDName = NULL;

/* -------------------------------------------------------------------- */
/*      What type of vector are we fetching.                            */
/* -------------------------------------------------------------------- */
    switch( nRCNM )
    {
      case RCNM_VI:
        poIndex = &oVI_Index;
        pszFDName = OGRN_VI;
        break;

      case RCNM_VC:
        poIndex = &oVC_Index;
        pszFDName = OGRN_VC;
        break;

      case RCNM_VE:
        poIndex = &oVE_Index;
        pszFDName = OGRN_VE;
        break;

      case RCNM_VF:
        poIndex = &oVF_Index;
        pszFDName = OGRN_VF;
        break;

      default:
        CPLAssert( false );
        return NULL;
    }

    if( nFeatureId < 0 || nFeatureId >= poIndex->GetCount() )
        return NULL;

    DDFRecord *poRecord = poIndex->GetByIndex( nFeatureId );

/* -------------------------------------------------------------------- */
/*      Find the feature definition to use.                             */
/* -------------------------------------------------------------------- */
    OGRFeatureDefn *poFDefn = NULL;

    for( int i = 0; i < nFDefnCount; i++ )
    {
        if( EQUAL(papoFDefnList[i]->GetName(),pszFDName) )
        {
            poFDefn = papoFDefnList[i];
            break;
        }
    }

    if( poFDefn == NULL )
    {
        // CPLAssert( false );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Create feature, and assign standard fields.                     */
/* -------------------------------------------------------------------- */
    OGRFeature *poFeature = new OGRFeature( poFDefn );

    poFeature->SetFID( nFeatureId );

    poFeature->SetField( "RCNM",
                         poRecord->GetIntSubfield( "VRID", 0, "RCNM",0) );
    poFeature->SetField( "RCID",
                         poRecord->GetIntSubfield( "VRID", 0, "RCID",0) );
    poFeature->SetField( "RVER",
                         poRecord->GetIntSubfield( "VRID", 0, "RVER",0) );
    poFeature->SetField( "RUIN",
                         poRecord->GetIntSubfield( "VRID", 0, "RUIN",0) );

/* -------------------------------------------------------------------- */
/*      Collect point geometries.                                       */
/* -------------------------------------------------------------------- */
    if( nRCNM == RCNM_VI || nRCNM == RCNM_VC )
    {
        double dfX = 0.0;
        double dfY = 0.0;

        if( poRecord->FindField( "SG2D" ) != NULL )
        {
            dfX = poRecord->GetIntSubfield("SG2D",0,"XCOO",0) / (double)nCOMF;
            dfY = poRecord->GetIntSubfield("SG2D",0,"YCOO",0) / (double)nCOMF;
            poFeature->SetGeometryDirectly( new OGRPoint( dfX, dfY ) );
        }

        else if( poRecord->FindField( "SG3D" ) != NULL ) /* presume sounding*/
        {
            double dfZ = 0.0;
            const int nVCount = poRecord->FindField("SG3D")->GetRepeatCount();
            if( nVCount == 1 )
            {
                dfX =poRecord->GetIntSubfield("SG3D",0,"XCOO",0)/(double)nCOMF;
                dfY =poRecord->GetIntSubfield("SG3D",0,"YCOO",0)/(double)nCOMF;
                dfZ =poRecord->GetIntSubfield("SG3D",0,"VE3D",0)/(double)nSOMF;
                poFeature->SetGeometryDirectly( new OGRPoint( dfX, dfY, dfZ ));
            }
            else
            {
                OGRMultiPoint *poMP = new OGRMultiPoint();

                for( int i = 0; i < nVCount; i++ )
                {
                    dfX = poRecord->GetIntSubfield("SG3D",0,"XCOO",i)
                        / static_cast<double>( nCOMF );
                    dfY = poRecord->GetIntSubfield("SG3D",0,"YCOO",i)
                        / static_cast<double>( nCOMF );
                    dfZ = poRecord->GetIntSubfield("SG3D",0,"VE3D",i)
                        / static_cast<double>( nSOMF );

                    poMP->addGeometryDirectly( new OGRPoint( dfX, dfY, dfZ ) );
                }

                poFeature->SetGeometryDirectly( poMP );
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Collect an edge geometry.                                       */
/* -------------------------------------------------------------------- */
    else if( nRCNM == RCNM_VE )
    {
        int nPoints = 0;
        OGRLineString *poLine = new OGRLineString();

        for( int iField = 0; iField < poRecord->GetFieldCount(); ++iField )
        {
            DDFField *poSG2D = poRecord->GetField( iField );

            if( EQUAL(poSG2D->GetFieldDefn()->GetName(), "SG2D") )
            {
                const int nVCount = poSG2D->GetRepeatCount();

                poLine->setNumPoints( nPoints + nVCount );

                for( int i = 0; i < nVCount; ++i )
                {
                    poLine->setPoint
                        (nPoints++,
                        poRecord->GetIntSubfield("SG2D",0,"XCOO",i)
                           / static_cast<double>( nCOMF ),
                        poRecord->GetIntSubfield("SG2D",0,"YCOO",i)
                           / static_cast<double>(nCOMF ) );
                }
            }
        }

        poFeature->SetGeometryDirectly( poLine );
    }

/* -------------------------------------------------------------------- */
/*      Special edge fields.                                            */
/*      Allow either 2 VRPT fields or one VRPT field with 2 rows        */
/* -------------------------------------------------------------------- */
    DDFField *poVRPT = NULL;

    if( nRCNM == RCNM_VE
        && (poVRPT = poRecord->FindField( "VRPT" )) != NULL )
    {
        poFeature->SetField( "NAME_RCNM_0", RCNM_VC );
        poFeature->SetField( "NAME_RCID_0", ParseName( poVRPT ) );
        poFeature->SetField( "ORNT_0",
                             poRecord->GetIntSubfield("VRPT",0,"ORNT",0) );
        poFeature->SetField( "USAG_0",
                             poRecord->GetIntSubfield("VRPT",0,"USAG",0) );
        poFeature->SetField( "TOPI_0",
                             poRecord->GetIntSubfield("VRPT",0,"TOPI",0) );
        poFeature->SetField( "MASK_0",
                             poRecord->GetIntSubfield("VRPT",0,"MASK",0) );

        int iField = 0;
        int iSubField = 1;

        if( poVRPT != NULL && poVRPT->GetRepeatCount() == 1 )
        {
            // Only one row, need a second VRPT field
            iField = 1;
            iSubField = 0;

            if( (poVRPT = poRecord->FindField( "VRPT", iField )) == NULL )
            {
                CPLError( CE_Warning, CPLE_AppDefined,
                          "Unable to fetch last edge node.\n"
                          "Feature OBJL=%s, RCID=%d may have corrupt or"
                          " missing geometry.",
                          poFeature->GetDefnRef()->GetName(),
                          poFeature->GetFieldAsInteger( "RCID" ) );

                return poFeature;
            }
        }

        poFeature->SetField( "NAME_RCID_1", ParseName( poVRPT, iSubField ) );
        poFeature->SetField( "NAME_RCNM_1", RCNM_VC );
        poFeature->SetField( "ORNT_1",
                             poRecord->GetIntSubfield("VRPT",iField,
                             "ORNT",iSubField) );
        poFeature->SetField( "USAG_1",
                             poRecord->GetIntSubfield("VRPT",iField,
                             "USAG",iSubField) );
        poFeature->SetField( "TOPI_1",
                             poRecord->GetIntSubfield("VRPT",iField,
                             "TOPI",iSubField) );
        poFeature->SetField( "MASK_1",
                             poRecord->GetIntSubfield("VRPT",iField,
                             "MASK",iSubField) );
    }

/* -------------------------------------------------------------------- */
/*      Geometric attributes                                            */
/*      Retrieve POSACC and QUAPOS attributes                           */
/* -------------------------------------------------------------------- */

    const int posaccField = poRegistrar->FindAttrByAcronym("POSACC");
    const int quaposField = poRegistrar->FindAttrByAcronym("QUAPOS");

    DDFField * poATTV = poRecord->FindField("ATTV");
    if( poATTV != NULL )
    {
        for( int j = 0; j < poATTV->GetRepeatCount(); j++ )
        {
            const int subField = poRecord->GetIntSubfield("ATTV",0,"ATTL",j);
            // POSACC field
            if (subField == posaccField) {
                poFeature->SetField( "POSACC",
                                    poRecord->GetFloatSubfield("ATTV",0,"ATVL",j) );
            }

            // QUAPOS field
            if (subField == quaposField) {
                poFeature->SetField( "QUAPOS",
                                    poRecord->GetIntSubfield("ATTV",0,"ATVL",j) );
            }
        }
    }

    return poFeature;
}

/************************************************************************/
/*                             FetchPoint()                             */
/*                                                                      */
/*      Fetch the location of a spatial point object.                   */
/************************************************************************/

bool S57Reader::FetchPoint( int nRCNM, int nRCID,
                            double *pdfX, double *pdfY, double *pdfZ )

{
    DDFRecord *poSRecord = NULL;

    if( nRCNM == RCNM_VI )
        poSRecord = oVI_Index.FindRecord( nRCID );
    else
        poSRecord = oVC_Index.FindRecord( nRCID );

    if( poSRecord == NULL )
        return false;

    double dfX = 0.0;
    double dfY = 0.0;
    double dfZ = 0.0;

    if( poSRecord->FindField( "SG2D" ) != NULL )
    {
        dfX = poSRecord->GetIntSubfield("SG2D",0,"XCOO",0)
            / static_cast<double>( nCOMF );
        dfY = poSRecord->GetIntSubfield("SG2D",0,"YCOO",0)
            / static_cast<double>( nCOMF );
    }
    else if( poSRecord->FindField( "SG3D" ) != NULL )
    {
        dfX = poSRecord->GetIntSubfield("SG3D",0,"XCOO",0)
            / static_cast<double>( nCOMF );
        dfY = poSRecord->GetIntSubfield("SG3D",0,"YCOO",0)
            / static_cast<double>( nCOMF );
        dfZ = poSRecord->GetIntSubfield("SG3D",0,"VE3D",0)
            / static_cast<double>( nSOMF );
    }
    else
        return false;

    if( pdfX != NULL )
        *pdfX = dfX;
    if( pdfY != NULL )
        *pdfY = dfY;
    if( pdfZ != NULL )
        *pdfZ = dfZ;

    return true;
}

/************************************************************************/
/*                  S57StrokeArcToOGRGeometry_Angles()                  */
/************************************************************************/

static OGRLineString *
S57StrokeArcToOGRGeometry_Angles( double dfCenterX, double dfCenterY,
                                  double dfRadius,
                                  double dfStartAngle, double dfEndAngle,
                                  int nVertexCount )

{
    OGRLineString * const poLine = new OGRLineString;

    nVertexCount = std::max(2, nVertexCount);
    const double dfSlice = (dfEndAngle-dfStartAngle)/(nVertexCount-1);

    poLine->setNumPoints( nVertexCount );

    for( int iPoint=0; iPoint < nVertexCount; iPoint++ )
    {
        const double dfAngle = (dfStartAngle + iPoint * dfSlice) * M_PI / 180.0;

        const double dfArcX = dfCenterX + cos(dfAngle) * dfRadius;
        const double dfArcY = dfCenterY + sin(dfAngle) * dfRadius;

        poLine->setPoint( iPoint, dfArcX, dfArcY );
    }

    return poLine;
}

/************************************************************************/
/*                  S57StrokeArcToOGRGeometry_Points()                  */
/************************************************************************/

static OGRLineString *
S57StrokeArcToOGRGeometry_Points( double dfStartX, double dfStartY,
                                  double dfCenterX, double dfCenterY,
                                  double dfEndX, double dfEndY,
                                  int nVertexCount )

{
    double dfStartAngle = 0.0;
    double dfEndAngle = 360.0;

    if( dfStartX == dfEndX && dfStartY == dfEndY )
    {
        // dfStartAngle = 0.0;
        // dfEndAngle = 360.0;
    }
    else
    {
        double dfDeltaX = dfStartX - dfCenterX;
        double dfDeltaY = dfStartY - dfCenterY;
        dfStartAngle = atan2(dfDeltaY, dfDeltaX) * 180.0 / M_PI;

        dfDeltaX = dfEndX - dfCenterX;
        dfDeltaY = dfEndY - dfCenterY;
        dfEndAngle = atan2(dfDeltaY, dfDeltaX) * 180.0 / M_PI;

#ifdef notdef
        if( dfStartAngle > dfAlongAngle && dfAlongAngle > dfEndAngle )
        {
            // TODO: Use std::swap.
            const double dfTempAngle = dfStartAngle;
            dfStartAngle = dfEndAngle;
            dfEndAngle = dfTempAngle;
        }
#endif

        while( dfStartAngle < dfEndAngle )
            dfStartAngle += 360.0;

//        while( dfAlongAngle < dfStartAngle )
//            dfAlongAngle += 360.0;

//        while( dfEndAngle < dfAlongAngle )
//            dfEndAngle += 360.0;

        if( dfEndAngle - dfStartAngle > 360.0 )
        {
            // TODO: Use std::swap.
            const double dfTempAngle = dfStartAngle;
            dfStartAngle = dfEndAngle;
            dfEndAngle = dfTempAngle;

            while( dfEndAngle < dfStartAngle )
                dfStartAngle -= 360.0;
        }
    }

    const double dfRadius =
        sqrt( (dfCenterX - dfStartX) * (dfCenterX - dfStartX)
              + (dfCenterY - dfStartY) * (dfCenterY - dfStartY) );

    return S57StrokeArcToOGRGeometry_Angles( dfCenterX, dfCenterY,
                                             dfRadius,
                                             dfStartAngle, dfEndAngle,
                                             nVertexCount );
}

/************************************************************************/
/*                             FetchLine()                              */
/************************************************************************/

bool S57Reader::FetchLine( DDFRecord *poSRecord,
                          int iStartVertex, int iDirection,
                          OGRLineString *poLine )

{
    int             nPoints = 0;
    DDFField        *poSG2D = NULL;
    DDFField        *poAR2D = NULL;
    DDFSubfieldDefn *poXCOO = NULL;
    DDFSubfieldDefn *poYCOO = NULL;
    bool bStandardFormat = true;

/* -------------------------------------------------------------------- */
/*      Points may be multiple rows in one SG2D/AR2D field or           */
/*      multiple SG2D/AR2D fields (or a combination of both)            */
/*      Iterate over all the SG2D/AR2D fields in the record             */
/* -------------------------------------------------------------------- */

    for( int iField = 0; iField < poSRecord->GetFieldCount(); ++iField )
    {
        poSG2D = poSRecord->GetField( iField );

        if( EQUAL(poSG2D->GetFieldDefn()->GetName(), "SG2D") )
        {
            poAR2D = NULL;
        }
        else if( EQUAL(poSG2D->GetFieldDefn()->GetName(), "AR2D") )
        {
            poAR2D = poSG2D;
        }
        else
        {
            /* Other types of fields are skipped */
            continue;
        }

/* -------------------------------------------------------------------- */
/*      Get some basic definitions.                                     */
/* -------------------------------------------------------------------- */

        poXCOO = poSG2D->GetFieldDefn()->FindSubfieldDefn("XCOO");
        poYCOO = poSG2D->GetFieldDefn()->FindSubfieldDefn("YCOO");

        if( poXCOO == NULL || poYCOO == NULL )
        {
            CPLDebug( "S57", "XCOO or YCOO are NULL" );
            return false;
        }

        const int nVCount = poSG2D->GetRepeatCount();

/* -------------------------------------------------------------------- */
/*      It is legitimate to have zero vertices for line segments        */
/*      that just have the start and end node (bug 840).                */
/*                                                                      */
/*      This is bogus! nVCount != 0, because poXCOO != 0 here           */
/*      In case of zero vertices, there will not be any SG2D fields     */
/* -------------------------------------------------------------------- */
        if( nVCount == 0 )
            continue;

/* -------------------------------------------------------------------- */
/*      Make sure out line is long enough to hold all the vertices      */
/*      we will apply.                                                  */
/* -------------------------------------------------------------------- */
        int nVBase = 0;

        if( iDirection < 0 )
            nVBase = iStartVertex + nPoints + nVCount;
        else
            nVBase = iStartVertex + nPoints;

        if( poLine->getNumPoints() < iStartVertex + nPoints + nVCount )
            poLine->setNumPoints( iStartVertex + nPoints + nVCount );

        nPoints += nVCount;
/* -------------------------------------------------------------------- */
/*      Are the SG2D and XCOO/YCOO definitions in the form we expect?   */
/* -------------------------------------------------------------------- */
        bStandardFormat =
            (poSG2D->GetFieldDefn()->GetSubfieldCount() == 2) &&
            EQUAL(poXCOO->GetFormat(),"b24") &&
            EQUAL(poYCOO->GetFormat(),"b24");

/* -------------------------------------------------------------------- */
/*      Collect the vertices:                                           */
/*                                                                      */
/*      This approach assumes that the data is LSB organized int32      */
/*      binary data as per the specification.  We avoid lots of         */
/*      extra calls to low level DDF methods as they are quite          */
/*      expensive.                                                      */
/* -------------------------------------------------------------------- */
        if( bStandardFormat )
        {
            int nBytesRemaining = 0;

            const char *pachData =
                poSG2D->GetSubfieldData( poYCOO, &nBytesRemaining, 0 );

            for( int i = 0; i < nVCount; i++ )
            {
                GInt32 nYCOO = 0;
                memcpy( &nYCOO, pachData, 4 );
                pachData += 4;

                GInt32 nXCOO = 0;
                memcpy( &nXCOO, pachData, 4 );
                pachData += 4;

#ifdef CPL_MSB
                CPL_SWAP32PTR( &nXCOO );
                CPL_SWAP32PTR( &nYCOO );
#endif
                const double dfX = nXCOO / static_cast<double>( nCOMF );
                const double dfY = nYCOO / static_cast<double>( nCOMF );

                poLine->setPoint( nVBase, dfX, dfY );

                nVBase += iDirection;
            }
        }

/* -------------------------------------------------------------------- */
/*      Collect the vertices:                                           */
/*                                                                      */
/*      The generic case where we use low level but expensive DDF       */
/*      methods to get the data.  This should work even if some         */
/*      things are changed about the SG2D fields such as making them    */
/*      floating point or a different byte order.                       */
/* -------------------------------------------------------------------- */
        else
        {
            for( int i = 0; i < nVCount; i++ )
            {
                int nBytesRemaining = 0;

                const char *pachData
                    = poSG2D->GetSubfieldData( poXCOO, &nBytesRemaining, i );

                const double dfX
                    = poXCOO->ExtractIntData( pachData, nBytesRemaining, NULL )
                    / static_cast<double>( nCOMF );

                pachData = poSG2D->GetSubfieldData(poYCOO,&nBytesRemaining,i);

                const double dfY
                    = poXCOO->ExtractIntData( pachData, nBytesRemaining, NULL )
                    / static_cast<double>( nCOMF );

                poLine->setPoint( nVBase, dfX, dfY );

                nVBase += iDirection;
            }
       }

/* -------------------------------------------------------------------- */
/*      If this is actually an arc, turn the start, end and center      */
/*      of rotation into a "stroked" arc linestring.                    */
/* -------------------------------------------------------------------- */
        if( poAR2D != NULL && poLine->getNumPoints() >= 3 )
        {
            int iLast = poLine->getNumPoints() - 1;

            OGRLineString *poArc = S57StrokeArcToOGRGeometry_Points(
                poLine->getX(iLast-0), poLine->getY(iLast-0),
                poLine->getX(iLast-1), poLine->getY(iLast-1),
                poLine->getX(iLast-2), poLine->getY(iLast-2),
                30 );

            if( poArc != NULL )
            {
                for( int i = 0; i < poArc->getNumPoints(); i++ )
                    poLine->setPoint( iLast-2+i, poArc->getX(i),
                                      poArc->getY(i) );

                delete poArc;
            }
        }
    }

    return true;
}

/************************************************************************/
/*                       AssemblePointGeometry()                        */
/************************************************************************/

void S57Reader::AssemblePointGeometry( DDFRecord * poFRecord,
                                       OGRFeature * poFeature )

{
/* -------------------------------------------------------------------- */
/*      Feature the spatial record containing the point.                */
/* -------------------------------------------------------------------- */
    DDFField *poFSPT = poFRecord->FindField( "FSPT" );
    if( poFSPT == NULL )
        return;

    if( poFSPT->GetRepeatCount() != 1 )
    {
#ifdef DEBUG
        fprintf( stderr, /*ok*/
                 "Point features with other than one spatial linkage.\n" );
        poFRecord->Dump( stderr );
#endif
        CPLDebug( "S57",
           "Point feature encountered with other than one spatial linkage." );
    }

    int nRCNM = 0;
    const int nRCID = ParseName( poFSPT, 0, &nRCNM );

    double dfX = 0.0;
    double dfY = 0.0;
    double dfZ = 0.0;

    if( nRCID == -1 || !FetchPoint( nRCNM, nRCID, &dfX, &dfY, &dfZ ) )
    {
        CPLError( CE_Warning, CPLE_AppDefined,
                  "Failed to fetch %d/%d point geometry for point feature.\n"
                  "Feature will have empty geometry.",
                  nRCNM, nRCID );
        return;
    }

    if( dfZ == 0.0 )
        poFeature->SetGeometryDirectly( new OGRPoint( dfX, dfY ) );
    else
        poFeature->SetGeometryDirectly( new OGRPoint( dfX, dfY, dfZ ) );
}

/************************************************************************/
/*                      AssembleSoundingGeometry()                      */
/************************************************************************/

void S57Reader::AssembleSoundingGeometry( DDFRecord * poFRecord,
                                          OGRFeature * poFeature )

{
/* -------------------------------------------------------------------- */
/*      Feature the spatial record containing the point.                */
/* -------------------------------------------------------------------- */
    DDFField *poFSPT = poFRecord->FindField( "FSPT" );
    if( poFSPT == NULL )
        return;

    if( poFSPT->GetRepeatCount() != 1 )
        return;

    int nRCNM = 0;
    const int nRCID = ParseName( poFSPT, 0, &nRCNM );

    DDFRecord *poSRecord = nRCNM == RCNM_VI
        ? oVI_Index.FindRecord( nRCID )
        : oVC_Index.FindRecord( nRCID );

    if( poSRecord == NULL )
        return;

/* -------------------------------------------------------------------- */
/*      Extract vertices.                                               */
/* -------------------------------------------------------------------- */
    OGRMultiPoint * const poMP = new OGRMultiPoint();

    DDFField *poField = poSRecord->FindField( "SG2D" );
    if( poField == NULL )
        poField = poSRecord->FindField( "SG3D" );
    if( poField == NULL )
    {
        delete poMP;
        return;
    }

    DDFSubfieldDefn *poXCOO
        = poField->GetFieldDefn()->FindSubfieldDefn( "XCOO" );
    DDFSubfieldDefn *poYCOO
        = poField->GetFieldDefn()->FindSubfieldDefn( "YCOO" );
    if( poXCOO == NULL || poYCOO == NULL )
    {
        CPLDebug( "S57", "XCOO or YCOO are NULL" );
        delete poMP;
        return;
    }
    DDFSubfieldDefn * const poVE3D
        = poField->GetFieldDefn()->FindSubfieldDefn( "VE3D" );

    const int nPointCount = poField->GetRepeatCount();

    const char *pachData = poField->GetData();
    int nBytesLeft = poField->GetDataSize();

    for( int i = 0; i < nPointCount; i++ )
    {
        int nBytesConsumed = 0;

        const double dfY = poYCOO->ExtractIntData( pachData, nBytesLeft,
                                                   &nBytesConsumed )
            / static_cast<double>( nCOMF );
        nBytesLeft -= nBytesConsumed;
        pachData += nBytesConsumed;

        const double dfX = poXCOO->ExtractIntData( pachData, nBytesLeft,
                                                   &nBytesConsumed )
            / static_cast<double>( nCOMF );
        nBytesLeft -= nBytesConsumed;
        pachData += nBytesConsumed;

        double dfZ = 0.0;
        if( poVE3D != NULL )
        {
            dfZ = poYCOO->ExtractIntData( pachData, nBytesLeft,
                                          &nBytesConsumed )
                / static_cast<double>( nSOMF );
            nBytesLeft -= nBytesConsumed;
            pachData += nBytesConsumed;
        }

        poMP->addGeometryDirectly( new OGRPoint( dfX, dfY, dfZ ) );
    }

    poFeature->SetGeometryDirectly( poMP );
}

/************************************************************************/
/*                            GetIntSubfield()                          */
/************************************************************************/

static int
GetIntSubfield( DDFField *poField,
                const char * pszSubfield,
                int iSubfieldIndex)
{
    DDFSubfieldDefn *poSFDefn =
        poField->GetFieldDefn()->FindSubfieldDefn( pszSubfield );

    if( poSFDefn == NULL )
        return 0;

/* -------------------------------------------------------------------- */
/*      Get a pointer to the data.                                      */
/* -------------------------------------------------------------------- */
    int nBytesRemaining = 0;

    const char *pachData = poField->GetSubfieldData( poSFDefn,
                                &nBytesRemaining,
                                iSubfieldIndex );

    return poSFDefn->ExtractIntData( pachData, nBytesRemaining, NULL );
}

/************************************************************************/
/*                        AssembleLineGeometry()                        */
/************************************************************************/

void S57Reader::AssembleLineGeometry( DDFRecord * poFRecord,
                                      OGRFeature * poFeature )

{
    OGRLineString *poLine = new OGRLineString();
    OGRMultiLineString *poMLS = new OGRMultiLineString();

/* -------------------------------------------------------------------- */
/*      Loop collecting edges.                                          */
/*      Iterate over the FSPT fields.                                   */
/* -------------------------------------------------------------------- */
    const int nFieldCount = poFRecord->GetFieldCount();

    for( int iField = 0; iField < nFieldCount; ++iField )
    {
        double dlastfX = 0.0;
        double dlastfY = 0.0;

        DDFField *poFSPT = poFRecord->GetField( iField );

        if( !EQUAL(poFSPT->GetFieldDefn()->GetName(), "FSPT") )
            continue;

/* -------------------------------------------------------------------- */
/*      Loop over the rows of each FSPT field                           */
/* -------------------------------------------------------------------- */
        const int nEdgeCount = poFSPT->GetRepeatCount();

        for( int iEdge = 0; iEdge < nEdgeCount; ++iEdge )
        {
            const bool bReverse
              = ( GetIntSubfield( poFSPT, "ORNT", iEdge ) == 2 );

/* -------------------------------------------------------------------- */
/*      Find the spatial record for this edge.                          */
/* -------------------------------------------------------------------- */
            const int nRCID = ParseName( poFSPT, iEdge );

            DDFRecord *poSRecord = oVE_Index.FindRecord( nRCID );
            if( poSRecord == NULL )
            {
                CPLError( CE_Warning, CPLE_AppDefined,
                          "Couldn't find spatial record %d.\n"
                          "Feature OBJL=%s, RCID=%d may have corrupt or"
                          "missing geometry.",
                          nRCID,
                          poFeature->GetDefnRef()->GetName(),
                          GetIntSubfield( poFSPT, "RCID", 0 ) );
                continue;
            }

/* -------------------------------------------------------------------- */
/*      Get the first and last nodes                                    */
/* -------------------------------------------------------------------- */
            DDFField *poVRPT = poSRecord->FindField( "VRPT" );
            if( poVRPT == NULL )
            {
                CPLError( CE_Warning, CPLE_AppDefined,
                          "Unable to fetch start node for RCID %d.\n"
                          "Feature OBJL=%s, RCID=%d may have corrupt or"
                          "missing geometry.",
                          nRCID,
                          poFeature->GetDefnRef()->GetName(),
                          GetIntSubfield( poFSPT, "RCID", 0 ) );
                continue;
            }

            // The "VRPT" field has only one row
            // Get the next row from a second "VRPT" field
            int nVC_RCID_firstnode = 0;
            int nVC_RCID_lastnode = 0;

            if( poVRPT != NULL && poVRPT->GetRepeatCount() == 1 )
            {
                nVC_RCID_firstnode = ParseName( poVRPT );
                poVRPT = poSRecord->FindField( "VRPT", 1 );

                if( poVRPT == NULL )
                {
                    CPLError( CE_Warning, CPLE_AppDefined,
                              "Unable to fetch end node for RCID %d.\n"
                              "Feature OBJL=%s, RCID=%d may have corrupt or"
                              "missing geometry.",
                              nRCID,
                              poFeature->GetDefnRef()->GetName(),
                              GetIntSubfield( poFSPT, "RCID", 0 ) );
                    continue;
                }

                nVC_RCID_lastnode = ParseName( poVRPT );

                if( bReverse )
                {
                    // TODO: std::swap.
                    const int tmp = nVC_RCID_lastnode;
                    nVC_RCID_lastnode = nVC_RCID_firstnode;
                    nVC_RCID_firstnode = tmp;
                }
            }
            else if( bReverse )
            {
                nVC_RCID_lastnode = ParseName( poVRPT );
                nVC_RCID_firstnode = ParseName( poVRPT, 1 );
            }
            else
            {
                nVC_RCID_firstnode = ParseName( poVRPT );
                nVC_RCID_lastnode = ParseName( poVRPT, 1 );
            }

            double dfX = 0.0;
            double dfY = 0.0;
            if( nVC_RCID_firstnode == -1 ||
                ! FetchPoint( RCNM_VC, nVC_RCID_firstnode, &dfX, &dfY ) )
            {
                CPLError( CE_Warning, CPLE_AppDefined,
                    "Unable to fetch start node RCID=%d.\n"
                    "Feature OBJL=%s, RCID=%d may have corrupt or"
                    " missing geometry.",
                    nVC_RCID_firstnode,
                    poFeature->GetDefnRef()->GetName(),
                    poFRecord->GetIntSubfield( "FRID", 0,
                                "RCID", 0 ) );

                continue;
            }

/* -------------------------------------------------------------------- */
/*      Does the first node match the trailing node on the existing     */
/*      line string?  If so, skip it, otherwise if the existing         */
/*      linestring is not empty we need to push it out and start a      */
/*      new one as it means things are not connected.                   */
/* -------------------------------------------------------------------- */
            if( poLine->getNumPoints() == 0 )
            {
                poLine->addPoint( dfX, dfY );
            }
            else if( std::abs(dlastfX - dfX) > 0.00000001 ||
                std::abs(dlastfY - dfY) > 0.00000001 )
            {
                // we need to start a new linestring.
                poMLS->addGeometryDirectly( poLine );
                poLine = new OGRLineString();
                poLine->addPoint( dfX, dfY );
            }
            else
            {
                /* omit point, already present */
            }

/* -------------------------------------------------------------------- */
/*      Collect the vertices.                                           */
/*      Iterate over all the SG2D fields in the Spatial record          */
/* -------------------------------------------------------------------- */
            for( int iSField = 0;
                 iSField < poSRecord->GetFieldCount();
                 ++iSField )
            {
                DDFField *poSG2D = poSRecord->GetField( iSField );

                if( EQUAL(poSG2D->GetFieldDefn()->GetName(), "SG2D") ||
                    EQUAL(poSG2D->GetFieldDefn()->GetName(), "AR2D") )
                {
                    DDFSubfieldDefn *poXCOO
                        = poSG2D->GetFieldDefn()->FindSubfieldDefn("XCOO");
                    DDFSubfieldDefn *poYCOO
                        = poSG2D->GetFieldDefn()->FindSubfieldDefn("YCOO");

                    if( poXCOO == NULL || poYCOO == NULL )
                    {
                        CPLDebug( "S57", "XCOO or YCOO are NULL" );
                        delete poLine;
                        delete poMLS;
                        return;
                    }

                    const int nVCount = poSG2D->GetRepeatCount();

                    int nStart = 0;
                    int nEnd = 0;
                    int nInc = 0;
                    if( bReverse )
                    {
                        nStart = nVCount-1;
                        nInc = -1;
                    }
                    else
                    {
                        nEnd = nVCount-1;
                        nInc = 1;
                    }

                    int nVBase = poLine->getNumPoints();
                    poLine->setNumPoints( nVBase + nVCount );

                    int nBytesRemaining = 0;

                    for( int i = nStart; i != nEnd+nInc; i += nInc )
                    {
                        const char *pachData
                          = poSG2D->GetSubfieldData(
                              poXCOO, &nBytesRemaining, i );

                        dfX = poXCOO->ExtractIntData(
                            pachData, nBytesRemaining, NULL )
                            / static_cast<double>( nCOMF );

                        pachData = poSG2D->GetSubfieldData(
                            poYCOO, &nBytesRemaining, i );

                        dfY = poXCOO->ExtractIntData(
                            pachData, nBytesRemaining, NULL )
                            / static_cast<double>( nCOMF );

                        poLine->setPoint( nVBase++, dfX, dfY );
                    }
                }
            }

            // remember the coordinates of the last point
            dlastfX = dfX;
            dlastfY = dfY;

/* -------------------------------------------------------------------- */
/*      Add the end node.                                               */
/* -------------------------------------------------------------------- */
            if( nVC_RCID_lastnode != -1 &&
                FetchPoint( RCNM_VC, nVC_RCID_lastnode, &dfX, &dfY ) )
            {
                poLine->addPoint( dfX, dfY );
                dlastfX = dfX;
                dlastfY = dfY;
            }
            else
            {
                CPLError( CE_Warning, CPLE_AppDefined,
                          "Unable to fetch end node RCID=%d.\n"
                          "Feature OBJL=%s, RCID=%d may have corrupt or"
                          " missing geometry.",
                          nVC_RCID_lastnode,
                          poFeature->GetDefnRef()->GetName(),
                          poFRecord->GetIntSubfield( "FRID", 0, "RCID", 0 ) );
                continue;
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Set either the line or multilinestring as the geometry.  We     */
/*      are careful to just produce a linestring if there are no        */
/*      disconnections.                                                 */
/* -------------------------------------------------------------------- */
    if( poMLS->getNumGeometries() > 0 )
    {
        poMLS->addGeometryDirectly( poLine );
        poFeature->SetGeometryDirectly( poMLS );
    }
    else if( poLine->getNumPoints() >= 2 )
    {
        poFeature->SetGeometryDirectly( poLine );
        delete poMLS;
    }
    else
    {
        delete poLine;
        delete poMLS;
    }
}

/************************************************************************/
/*                        AssembleAreaGeometry()                        */
/************************************************************************/

void S57Reader::AssembleAreaGeometry( DDFRecord * poFRecord,
                                      OGRFeature * poFeature )

{
    OGRGeometryCollection * const poLines = new OGRGeometryCollection();

/* -------------------------------------------------------------------- */
/*      Find the FSPT fields.                                           */
/* -------------------------------------------------------------------- */
    const int nFieldCount = poFRecord->GetFieldCount();

    for( int iFSPT = 0; iFSPT < nFieldCount; ++iFSPT )
    {
        DDFField *poFSPT = poFRecord->GetField(iFSPT);

        if ( !EQUAL(poFSPT->GetFieldDefn()->GetName(), "FSPT") )
            continue;

        const int nEdgeCount = poFSPT->GetRepeatCount();

/* ==================================================================== */
/*      Loop collecting edges.                                          */
/* ==================================================================== */
        for( int iEdge = 0; iEdge < nEdgeCount; iEdge++ )
        {
/* -------------------------------------------------------------------- */
/*      Find the spatial record for this edge.                          */
/* -------------------------------------------------------------------- */
            const int nRCID = ParseName( poFSPT, iEdge );

            DDFRecord *poSRecord = oVE_Index.FindRecord( nRCID );
            if( poSRecord == NULL )
            {
                CPLError( CE_Warning, CPLE_AppDefined,
                          "Couldn't find spatial record %d.\n"
                          "Feature OBJL=%s, RCID=%d may have corrupt or"
                          "missing geometry.",
                          nRCID,
                          poFeature->GetDefnRef()->GetName(),
                          GetIntSubfield( poFSPT, "RCID", 0 ) );
                continue;
            }

/* -------------------------------------------------------------------- */
/*      Create the line string.                                         */
/* -------------------------------------------------------------------- */
            OGRLineString *poLine = new OGRLineString();

/* -------------------------------------------------------------------- */
/*      Add the start node.                                             */
/* -------------------------------------------------------------------- */
            DDFField *poVRPT = poSRecord->FindField( "VRPT" );
            if( poVRPT != NULL )
            {
                int nVC_RCID = ParseName( poVRPT );
                double dfX = 0.0;
                double dfY = 0.0;

                if( nVC_RCID != -1
                    && FetchPoint( RCNM_VC, nVC_RCID, &dfX, &dfY ) )
                    poLine->addPoint( dfX, dfY );
            }

/* -------------------------------------------------------------------- */
/*      Collect the vertices.                                           */
/* -------------------------------------------------------------------- */
            if( !FetchLine( poSRecord, poLine->getNumPoints(), 1, poLine ) )
            {
                CPLDebug( "S57",
                          "FetchLine() failed in AssembleAreaGeometry()!" );
            }

/* -------------------------------------------------------------------- */
/*      Add the end node.                                               */
/* -------------------------------------------------------------------- */
            if( poVRPT != NULL && poVRPT->GetRepeatCount() > 1 )
            {
                const int nVC_RCID = ParseName( poVRPT, 1 );
                double dfX = 0.0;
                double dfY = 0.0;

                if( nVC_RCID != -1
                    && FetchPoint( RCNM_VC, nVC_RCID, &dfX, &dfY ) )
                    poLine->addPoint( dfX, dfY );
            }
            else if( (poVRPT = poSRecord->FindField( "VRPT", 1 )) != NULL )
            {
                const int nVC_RCID = ParseName( poVRPT );
                double dfX = 0.0;
                double dfY = 0.0;

                if( nVC_RCID != -1
                    && FetchPoint( RCNM_VC, nVC_RCID, &dfX, &dfY ) )
                    poLine->addPoint( dfX, dfY );
            }

            poLines->addGeometryDirectly( poLine );
        }
    }

/* -------------------------------------------------------------------- */
/*      Build lines into a polygon.                                     */
/* -------------------------------------------------------------------- */
    OGRErr eErr;

    OGRPolygon  *poPolygon = reinterpret_cast<OGRPolygon *>(
        OGRBuildPolygonFromEdges( reinterpret_cast<OGRGeometryH>( poLines ),
                                  TRUE, FALSE, 0.0, &eErr ) );
    if( eErr != OGRERR_NONE )
    {
        CPLError( CE_Warning, CPLE_AppDefined,
                  "Polygon assembly has failed for feature FIDN=%d,FIDS=%d.\n"
                  "Geometry may be missing or incomplete.",
                  poFeature->GetFieldAsInteger( "FIDN" ),
                  poFeature->GetFieldAsInteger( "FIDS" ) );
    }

    delete poLines;

    if( poPolygon != NULL )
        poFeature->SetGeometryDirectly( poPolygon );
}

/************************************************************************/
/*                             FindFDefn()                              */
/*                                                                      */
/*      Find the OGRFeatureDefn corresponding to the passed feature     */
/*      record.  It will search based on geometry class, or object      */
/*      class depending on the bClassBased setting.                     */
/************************************************************************/

OGRFeatureDefn * S57Reader::FindFDefn( DDFRecord * poRecord )

{
    if( poRegistrar != NULL )
    {
        const int nOBJL = poRecord->GetIntSubfield( "FRID", 0, "OBJL", 0 );

        if( nOBJL < static_cast<int>( apoFDefnByOBJL.size() )
            && apoFDefnByOBJL[nOBJL] != NULL )
            return apoFDefnByOBJL[nOBJL];

        if( !poClassContentExplorer->SelectClass( nOBJL ) )
        {
            for( int i = 0; i < nFDefnCount; i++ )
            {
                if( EQUAL(papoFDefnList[i]->GetName(),"Generic") )
                    return papoFDefnList[i];
            }
            return NULL;
        }

        for( int i = 0; i < nFDefnCount; i++ )
        {
            const char* pszAcronym = poClassContentExplorer->GetAcronym();
            if( pszAcronym != NULL &&
                EQUAL(papoFDefnList[i]->GetName(),
                      pszAcronym) )
                return papoFDefnList[i];
        }

        return NULL;
    }
    else
    {
        const int nPRIM = poRecord->GetIntSubfield( "FRID", 0, "PRIM", 0 );
        OGRwkbGeometryType eGType;

        if( nPRIM == PRIM_P )
            eGType = wkbPoint;
        else if( nPRIM == PRIM_L )
            eGType = wkbLineString;
        else if( nPRIM == PRIM_A )
            eGType = wkbPolygon;
        else
            eGType = wkbNone;

        for( int i = 0; i < nFDefnCount; i++ )
        {
            if( papoFDefnList[i]->GetGeomType() == eGType )
                return papoFDefnList[i];
        }
    }

    return NULL;
}

/************************************************************************/
/*                             ParseName()                              */
/*                                                                      */
/*      Pull the RCNM and RCID values from a NAME field.  The RCID      */
/*      is returned and the RCNM can be gotten via the pnRCNM argument. */
/*      Note: nIndex is the index of the requested 'NAME' instance      */
/************************************************************************/

int S57Reader::ParseName( DDFField * poField, int nIndex, int * pnRCNM )

{
    if( poField == NULL )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Missing field in ParseName()." );
        return -1;
    }

    DDFSubfieldDefn* poName
        = poField->GetFieldDefn()->FindSubfieldDefn( "NAME" );
    if( poName == NULL )
        return -1;

    int nMaxBytes = 0;
    unsigned char *pabyData = reinterpret_cast<unsigned char *>(
        const_cast<char *>(
            poField->GetSubfieldData( poName, &nMaxBytes, nIndex ) ) );
    if( pabyData == NULL || nMaxBytes < 5 )
        return -1;

    if( pnRCNM != NULL )
        *pnRCNM = pabyData[0];

    return CPL_LSBSINT32PTR(pabyData + 1);
}

/************************************************************************/
/*                           AddFeatureDefn()                           */
/************************************************************************/

void S57Reader::AddFeatureDefn( OGRFeatureDefn * poFDefn )

{
    nFDefnCount++;
    papoFDefnList = static_cast<OGRFeatureDefn **>(
        CPLRealloc(papoFDefnList, sizeof(OGRFeatureDefn*)*nFDefnCount ) );

    papoFDefnList[nFDefnCount-1] = poFDefn;

    if( poRegistrar != NULL )
    {
        if( poClassContentExplorer->SelectClass( poFDefn->GetName() ) )
        {
            const int nOBJL = poClassContentExplorer->GetOBJL();
            if( nOBJL >= 0 )
            {
                if( nOBJL >= (int) apoFDefnByOBJL.size() )
                    apoFDefnByOBJL.resize(nOBJL+1);
                apoFDefnByOBJL[nOBJL] = poFDefn;
            }
        }
    }
}

/************************************************************************/
/*                          CollectClassList()                          */
/*                                                                      */
/*      Establish the list of classes (unique OBJL values) that         */
/*      occur in this dataset.                                          */
/************************************************************************/

bool S57Reader::CollectClassList(std::vector<int> &anClassCount)

{
    if( !bFileIngested && !Ingest() )
        return false;

    bool bSuccess = true;

    for( int iFEIndex = 0; iFEIndex < oFE_Index.GetCount(); iFEIndex++ )
    {
        DDFRecord *poRecord = oFE_Index.GetByIndex( iFEIndex );
        const int nOBJL = poRecord->GetIntSubfield( "FRID", 0, "OBJL", 0 );

        if( nOBJL < 0 )
            bSuccess = false;
        else
        {
            if( nOBJL >= (int) anClassCount.size() )
                anClassCount.resize(nOBJL+1);
            anClassCount[nOBJL]++;
        }
    }

    return bSuccess;
}

/************************************************************************/
/*                         ApplyRecordUpdate()                          */
/*                                                                      */
/*      Update one target record based on an S-57 update record         */
/*      (RUIN=3).                                                       */
/************************************************************************/

bool S57Reader::ApplyRecordUpdate( DDFRecord *poTarget, DDFRecord *poUpdate )

{
    const char *pszKey = poUpdate->GetField(1)->GetFieldDefn()->GetName();

/* -------------------------------------------------------------------- */
/*      Validate versioning.                                            */
/* -------------------------------------------------------------------- */
    if( poTarget->GetIntSubfield( pszKey, 0, "RVER", 0 ) + 1
        != poUpdate->GetIntSubfield( pszKey, 0, "RVER", 0 )  )
    {
        CPLDebug( "S57",
                  "Mismatched RVER value on RCNM=%d,RCID=%d.\n",
                  poTarget->GetIntSubfield( pszKey, 0, "RCNM", 0 ),
                  poTarget->GetIntSubfield( pszKey, 0, "RCID", 0 ) );

        // CPLAssert( false );
        return false;
    }

/* -------------------------------------------------------------------- */
/*      Update the target version.                                      */
/* -------------------------------------------------------------------- */
    DDFField *poKey = poTarget->FindField( pszKey );

    if( poKey == NULL )
    {
        // CPLAssert( false );
        return false;
    }

    DDFSubfieldDefn *poRVER_SFD
        = poKey->GetFieldDefn()->FindSubfieldDefn( "RVER" );
    if( poRVER_SFD == NULL )
        return false;

    unsigned char *pnRVER
        = (unsigned char *) poKey->GetSubfieldData( poRVER_SFD, NULL, 0 );

    *pnRVER += 1;

/* -------------------------------------------------------------------- */
/*      Check for, and apply record record to spatial record pointer    */
/*      updates.                                                        */
/* -------------------------------------------------------------------- */
    if( poUpdate->FindField( "FSPC" ) != NULL )
    {
        const int nFSUI = poUpdate->GetIntSubfield( "FSPC", 0, "FSUI", 0 );
        DDFField *poSrcFSPT = poUpdate->FindField( "FSPT" );
        DDFField *poDstFSPT = poTarget->FindField( "FSPT" );

        if( (poSrcFSPT == NULL && nFSUI != 2) || poDstFSPT == NULL )
        {
            // CPLAssert( false );
            return false;
        }

        const int nFSIX = poUpdate->GetIntSubfield( "FSPC", 0, "FSIX", 0 );
        const int nNSPT = poUpdate->GetIntSubfield( "FSPC", 0, "NSPT", 0 );

        int nPtrSize = poDstFSPT->GetFieldDefn()->GetFixedWidth();

        if( nFSUI == 1 ) /* INSERT */
        {
            int nInsertionBytes = nPtrSize * nNSPT;

            if( poSrcFSPT->GetDataSize() < nInsertionBytes )
            {
                CPLDebug( "S57", "Not enough bytes in source FSPT field. "
                          "Has %d, requires %d",
                          poSrcFSPT->GetDataSize(), nInsertionBytes );
                return false;
            }

            char *pachInsertion
                = static_cast<char *>( CPLMalloc(nInsertionBytes + nPtrSize) );
            memcpy( pachInsertion, poSrcFSPT->GetData(), nInsertionBytes );

            /*
            ** If we are inserting before an instance that already
            ** exists, we must add it to the end of the data being
            ** inserted.
            */
            if( nFSIX <= poDstFSPT->GetRepeatCount() )
            {
                if( poDstFSPT->GetDataSize() < nPtrSize * nFSIX )
                {
                    CPLDebug( "S57", "Not enough bytes in dest FSPT field. "
                              "Has %d, requires %d",
                              poDstFSPT->GetDataSize(), nPtrSize * nFSIX );
                    CPLFree( pachInsertion );
                    return false;
                }

                memcpy( pachInsertion + nInsertionBytes,
                        poDstFSPT->GetData() + nPtrSize * (nFSIX-1),
                        nPtrSize );
                nInsertionBytes += nPtrSize;
            }

            poTarget->SetFieldRaw( poDstFSPT, nFSIX - 1,
                                   pachInsertion, nInsertionBytes );
            CPLFree( pachInsertion );
        }
        else if( nFSUI == 2 ) /* DELETE */
        {
            /* Wipe each deleted coordinate */
            for( int i = nNSPT-1; i >= 0; i-- )
            {
                poTarget->SetFieldRaw( poDstFSPT, i + nFSIX - 1, NULL, 0 );
            }
        }
        else if( nFSUI == 3 ) /* MODIFY */
        {
            /* copy over each ptr */
            if( poSrcFSPT->GetDataSize() < nNSPT * nPtrSize )
            {
                CPLDebug("S57", "Not enough bytes in source FSPT field. Has %d, requires %d",
                         poSrcFSPT->GetDataSize(), nNSPT * nPtrSize );
                return false;
            }

            for( int i = 0; i < nNSPT; i++ )
            {
                const char *pachRawData = poSrcFSPT->GetData() + nPtrSize * i;
                poTarget->SetFieldRaw( poDstFSPT, i + nFSIX - 1,
                                       pachRawData, nPtrSize );
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Check for, and apply vector record to vector record pointer     */
/*      updates.                                                        */
/* -------------------------------------------------------------------- */
    if( poUpdate->FindField( "VRPC" ) != NULL )
    {
        const int nVPUI = poUpdate->GetIntSubfield( "VRPC", 0, "VPUI", 0 );
        DDFField *poSrcVRPT = poUpdate->FindField( "VRPT" );
        DDFField *poDstVRPT = poTarget->FindField( "VRPT" );

        if( (poSrcVRPT == NULL && nVPUI != 2) || poDstVRPT == NULL )
        {
            // CPLAssert( false );
            return false;
        }

        const int nVPIX = poUpdate->GetIntSubfield( "VRPC", 0, "VPIX", 0 );
        const int nNVPT = poUpdate->GetIntSubfield( "VRPC", 0, "NVPT", 0 );

        const int nPtrSize = poDstVRPT->GetFieldDefn()->GetFixedWidth();

        if( nVPUI == 1 ) /* INSERT */
        {
            int nInsertionBytes = nPtrSize * nNVPT;

            if( poSrcVRPT->GetDataSize() < nInsertionBytes )
            {
                CPLDebug("S57", "Not enough bytes in source VRPT field. Has %d, requires %d",
                         poSrcVRPT->GetDataSize(), nInsertionBytes );
                return false;
            }

            char *pachInsertion
                = static_cast<char *>( CPLMalloc(nInsertionBytes + nPtrSize) );
            memcpy( pachInsertion, poSrcVRPT->GetData(), nInsertionBytes );

            /*
            ** If we are inserting before an instance that already
            ** exists, we must add it to the end of the data being
            ** inserted.
            */
            if( nVPIX <= poDstVRPT->GetRepeatCount() )
            {
                if( poDstVRPT->GetDataSize() < nPtrSize * nVPIX )
                {
                    CPLDebug("S57", "Not enough bytes in dest VRPT field. Has %d, requires %d",
                         poDstVRPT->GetDataSize(), nPtrSize * nVPIX );
                    CPLFree( pachInsertion );
                    return false;
                }

                memcpy( pachInsertion + nInsertionBytes,
                        poDstVRPT->GetData() + nPtrSize * (nVPIX-1),
                        nPtrSize );
                nInsertionBytes += nPtrSize;
            }

            poTarget->SetFieldRaw( poDstVRPT, nVPIX - 1,
                                   pachInsertion, nInsertionBytes );
            CPLFree( pachInsertion );
        }
        else if( nVPUI == 2 ) /* DELETE */
        {
            /* Wipe each deleted coordinate */
            for( int i = nNVPT-1; i >= 0; i-- )
            {
                poTarget->SetFieldRaw( poDstVRPT, i + nVPIX - 1, NULL, 0 );
            }
        }
        else if( nVPUI == 3 ) /* MODIFY */
        {
            if( poSrcVRPT->GetDataSize() < nNVPT * nPtrSize )
            {
                CPLDebug( "S57", "Not enough bytes in source VRPT field. "
                          "Has %d, requires %d",
                          poSrcVRPT->GetDataSize(), nNVPT * nPtrSize );
                return false;
            }

            /* copy over each ptr */
            for( int i = 0; i < nNVPT; i++ )
            {
                const char *pachRawData = poSrcVRPT->GetData() + nPtrSize * i;

                poTarget->SetFieldRaw( poDstVRPT, i + nVPIX - 1,
                                       pachRawData, nPtrSize );
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Check for, and apply record update to coordinates.              */
/* -------------------------------------------------------------------- */
    if( poUpdate->FindField( "SGCC" ) != NULL )
    {
        DDFField *poSrcSG2D = poUpdate->FindField( "SG2D" );
        DDFField *poDstSG2D = poTarget->FindField( "SG2D" );

        /* If we don't have SG2D, check for SG3D */
        if( poDstSG2D == NULL )
        {
            poDstSG2D = poTarget->FindField( "SG3D" );
            if (poDstSG2D != NULL)
            {
                poSrcSG2D = poUpdate->FindField("SG3D");
            }
        }

        const int nCCUI = poUpdate->GetIntSubfield( "SGCC", 0, "CCUI", 0 );

        if( (poSrcSG2D == NULL && nCCUI != 2)
            || (poDstSG2D == NULL && nCCUI != 1) )
        {
            // CPLAssert( false );
            return false;
        }

        if (poDstSG2D == NULL)
        {
            poTarget->AddField(poTarget->GetModule()->FindFieldDefn("SG2D"));
            poDstSG2D = poTarget->FindField("SG2D");
            if (poDstSG2D == NULL) {
                // CPLAssert( false );
                return false;
            }

            // Delete null default data that was created
            poTarget->SetFieldRaw( poDstSG2D, 0, NULL, 0 );
        }

        int nCoordSize = poDstSG2D->GetFieldDefn()->GetFixedWidth();
        const int nCCIX = poUpdate->GetIntSubfield( "SGCC", 0, "CCIX", 0 );
        const int nCCNC = poUpdate->GetIntSubfield( "SGCC", 0, "CCNC", 0 );

        if( nCCUI == 1 ) /* INSERT */
        {
            int nInsertionBytes = nCoordSize * nCCNC;

            if( poSrcSG2D->GetDataSize() < nInsertionBytes )
            {
                CPLDebug( "S57", "Not enough bytes in source SG2D field. "
                          "Has %d, requires %d",
                          poSrcSG2D->GetDataSize(), nInsertionBytes );
                return false;
            }

            char *pachInsertion
                = static_cast<char *>(
                    CPLMalloc(nInsertionBytes + nCoordSize) );
            memcpy( pachInsertion, poSrcSG2D->GetData(), nInsertionBytes );

            /*
            ** If we are inserting before an instance that already
            ** exists, we must add it to the end of the data being
            ** inserted.
            */
            if( nCCIX <= poDstSG2D->GetRepeatCount() )
            {
                if( poDstSG2D->GetDataSize() < nCoordSize * nCCIX )
                {
                    CPLDebug( "S57", "Not enough bytes in dest SG2D field. "
                              "Has %d, requires %d",
                              poDstSG2D->GetDataSize(), nCoordSize * nCCIX );
                    CPLFree( pachInsertion );
                    return false;
                }

                memcpy( pachInsertion + nInsertionBytes,
                        poDstSG2D->GetData() + nCoordSize * (nCCIX-1),
                        nCoordSize );
                nInsertionBytes += nCoordSize;
            }

            poTarget->SetFieldRaw( poDstSG2D, nCCIX - 1,
                                   pachInsertion, nInsertionBytes );
            CPLFree( pachInsertion );
        }
        else if( nCCUI == 2 ) /* DELETE */
        {
            /* Wipe each deleted coordinate */
            for( int i = nCCNC-1; i >= 0; i-- )
            {
                poTarget->SetFieldRaw( poDstSG2D, i + nCCIX - 1, NULL, 0 );
            }
        }
        else if( nCCUI == 3 ) /* MODIFY */
        {
            if( poSrcSG2D->GetDataSize() < nCCNC * nCoordSize )
            {
                CPLDebug( "S57", "Not enough bytes in source SG2D field. "
                          "Has %d, requires %d",
                          poSrcSG2D->GetDataSize(), nCCNC * nCoordSize );
                return false;
            }

            /* copy over each ptr */
            for( int i = 0; i < nCCNC; i++ )
            {
                const char *pachRawData = poSrcSG2D->GetData() + nCoordSize * i;

                poTarget->SetFieldRaw( poDstSG2D, i + nCCIX - 1,
                                       pachRawData, nCoordSize );
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Apply updates to Feature to Feature pointer fields.  Note       */
/*      INSERT and DELETE are untested.  UPDATE tested per bug #5028.   */
/* -------------------------------------------------------------------- */
    if( poUpdate->FindField( "FFPC" ) != NULL )
    {
        int     nFFUI = poUpdate->GetIntSubfield( "FFPC", 0, "FFUI", 0 );
        DDFField *poSrcFFPT = poUpdate->FindField( "FFPT" );
        DDFField *poDstFFPT = poTarget->FindField( "FFPT" );

        if( (poSrcFFPT == NULL && nFFUI != 2)
            || (poDstFFPT == NULL && nFFUI != 1) )
        {
            CPLDebug( "S57", "Missing source or target FFPT applying update.");
            // CPLAssert( false );
            return false;
        }

        // Create FFPT field on target record, if it does not yet exist.
        if (poDstFFPT == NULL)
        {
            // Untested!
            poTarget->AddField(poTarget->GetModule()->FindFieldDefn("FFPT"));
            poDstFFPT = poTarget->FindField("FFPT");
            if (poDstFFPT == NULL) {
                // CPLAssert( false );
                return false;
            }

            // Delete null default data that was created
            poTarget->SetFieldRaw( poDstFFPT, 0, NULL, 0 );
        }

        // FFPT includes COMT which is variable length which would
        // greatly complicate updates.  But in practice COMT is always
        // an empty string so we will take a chance and assume that so
        // we have a fixed record length.  We *could* actually verify that
        // but I have not done so for now.
        const int nFFPTSize = 10;
        const int nFFIX = poUpdate->GetIntSubfield( "FFPC", 0, "FFIX", 0 );
        const int nNFPT = poUpdate->GetIntSubfield( "FFPC", 0, "NFPT", 0 );

        if (nFFUI == 1 ) /* INSERT */
        {
            // Untested!
            CPLDebug( "S57", "Using untested FFPT INSERT code!");

            int nInsertionBytes = nFFPTSize * nNFPT;

            if( poSrcFFPT->GetDataSize() < nInsertionBytes )
            {
                CPLDebug( "S57", "Not enough bytes in source FFPT field. "
                          "Has %d, requires %d",
                          poSrcFFPT->GetDataSize(), nInsertionBytes );
                return false;
            }

            char *pachInsertion
                = static_cast<char *>( CPLMalloc(nInsertionBytes + nFFPTSize) );
            memcpy( pachInsertion, poSrcFFPT->GetData(), nInsertionBytes );

            /*
            ** If we are inserting before an instance that already
            ** exists, we must add it to the end of the data being
            ** inserted.
            */
            if( nFFIX <= poDstFFPT->GetRepeatCount() )
            {
                if( poDstFFPT->GetDataSize() < nFFPTSize * nFFIX )
                {
                    CPLDebug( "S57", "Not enough bytes in dest FFPT field. "
                              "Has %d, requires %d",
                              poDstFFPT->GetDataSize(), nFFPTSize * nFFIX );
                    CPLFree( pachInsertion );
                    return false;
                }

                memcpy( pachInsertion + nInsertionBytes,
                        poDstFFPT->GetData() + nFFPTSize * (nFFIX-1),
                        nFFPTSize );
                nInsertionBytes += nFFPTSize;
            }

            poTarget->SetFieldRaw( poDstFFPT, nFFIX - 1,
                                   pachInsertion, nInsertionBytes );
            CPLFree( pachInsertion );
        }
        else if( nFFUI == 2 ) /* DELETE */
        {
            // Untested!
            CPLDebug( "S57", "Using untested FFPT DELETE code!");

            /* Wipe each deleted record */
            for( int i = nNFPT-1; i >= 0; i-- )
            {
                poTarget->SetFieldRaw( poDstFFPT, i + nFFIX - 1, NULL, 0 );
            }
        }
        else if( nFFUI == 3 ) /* UPDATE */
        {
            if( poSrcFFPT->GetDataSize() < nNFPT * nFFPTSize )
            {
                CPLDebug( "S57", "Not enough bytes in source FFPT field. "
                          "Has %d, requires %d",
                          poSrcFFPT->GetDataSize(), nNFPT * nFFPTSize );
                return false;
            }

            /* copy over each ptr */
            for( int i = 0; i < nNFPT; i++ )
            {
                const char *pachRawData = poSrcFFPT->GetData() + nFFPTSize * i;

                poTarget->SetFieldRaw( poDstFFPT, i + nFFIX - 1,
                                       pachRawData, nFFPTSize );
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Check for and apply changes to attribute lists.                 */
/* -------------------------------------------------------------------- */
    if( poUpdate->FindField( "ATTF" ) != NULL )
    {
        DDFField *poDstATTF = poTarget->FindField( "ATTF" );

        if( poDstATTF == NULL )
        {
            CPLError( CE_Warning, CPLE_AppDefined,
                      "Unable to apply ATTF change to target record without "
                      "an ATTF field (see GDAL/OGR Bug #1648)" );
            return false;
        }

        DDFField *poSrcATTF = poUpdate->FindField( "ATTF" );
        const int nRepeatCount = poSrcATTF->GetRepeatCount();

        for( int iAtt = 0; iAtt < nRepeatCount; iAtt++ )
        {
            const int nATTL
                = poUpdate->GetIntSubfield( "ATTF", 0, "ATTL", iAtt );
            int iTAtt = poDstATTF->GetRepeatCount() - 1;  // Used after for.

            for( ; iTAtt >= 0; iTAtt-- )
            {
                if( poTarget->GetIntSubfield( "ATTF", 0, "ATTL", iTAtt )
                    == nATTL )
                    break;
            }
            if( iTAtt == -1 )
                iTAtt = poDstATTF->GetRepeatCount();

            int nDataBytes = 0;
            const char *pszRawData =
                poSrcATTF->GetInstanceData( iAtt, &nDataBytes );
            if( pszRawData[2] == 0x7f /* delete marker */ )
            {
                poTarget->SetFieldRaw( poDstATTF, iTAtt, NULL, 0 );
            }
            else
            {
                poTarget->SetFieldRaw( poDstATTF, iTAtt, pszRawData,
                                       nDataBytes );
            }
        }
    }

    return true;
}

/************************************************************************/
/*                            ApplyUpdates()                            */
/*                                                                      */
/*      Read records from an update file, and apply them to the         */
/*      currently loaded index of features.                             */
/************************************************************************/

bool S57Reader::ApplyUpdates( DDFModule *poUpdateModule )

{
/* -------------------------------------------------------------------- */
/*      Ensure base file is loaded.                                     */
/* -------------------------------------------------------------------- */
    if( !bFileIngested && !Ingest() )
        return false;

/* -------------------------------------------------------------------- */
/*      Read records, and apply as updates.                             */
/* -------------------------------------------------------------------- */
    CPLErrorReset();

    DDFRecord *poRecord = NULL;

    while( (poRecord = poUpdateModule->ReadRecord()) != NULL )
    {
        DDFField *poKeyField = poRecord->GetField(1);
        if( poKeyField == NULL )
            return false;

        const char *pszKey = poKeyField->GetFieldDefn()->GetName();

        if( EQUAL(pszKey,"VRID") || EQUAL(pszKey,"FRID"))
        {
            const int nRCNM = poRecord->GetIntSubfield( pszKey,0, "RCNM",0 );
            const int nRCID = poRecord->GetIntSubfield( pszKey,0, "RCID",0 );
            const int nRVER = poRecord->GetIntSubfield( pszKey,0, "RVER",0 );
            const int nRUIN = poRecord->GetIntSubfield( pszKey,0, "RUIN",0 );
            DDFRecordIndex *poIndex = NULL;

            if( EQUAL(poKeyField->GetFieldDefn()->GetName(),"VRID") )
            {
                switch( nRCNM )
                {
                  case RCNM_VI:
                    poIndex = &oVI_Index;
                    break;

                  case RCNM_VC:
                    poIndex = &oVC_Index;
                    break;

                  case RCNM_VE:
                    poIndex = &oVE_Index;
                    break;

                  case RCNM_VF:
                    poIndex = &oVF_Index;
                    break;

                  default:
                    // CPLAssert( false );
                    return false;
                    break;
                }
            }
            else
            {
                poIndex = &oFE_Index;
            }

            if( poIndex != NULL )
            {
                if( nRUIN == 1 )  /* insert */
                {
                    poIndex->AddRecord( nRCID, poRecord->CloneOn(poModule) );
                }
                else if( nRUIN == 2 ) /* delete */
                {
                    DDFRecord *poTarget = poIndex->FindRecord( nRCID );
                    if( poTarget == NULL )
                    {
                        CPLError( CE_Warning, CPLE_AppDefined,
                                  "Can't find RCNM=%d,RCID=%d for delete.\n",
                                  nRCNM, nRCID );
                    }
                    else if( poTarget->GetIntSubfield( pszKey, 0, "RVER", 0 )
                             != nRVER - 1 )
                    {
                        CPLError( CE_Warning, CPLE_AppDefined,
                                  "Mismatched RVER value on RCNM=%d,RCID=%d.\n",
                                  nRCNM, nRCID );
                    }
                    else
                    {
                        poIndex->RemoveRecord( nRCID );
                    }
                }

                else if( nRUIN == 3 ) /* modify in place */
                {
                    DDFRecord *poTarget = poIndex->FindRecord( nRCID );
                    if( poTarget == NULL )
                    {
                        CPLError( CE_Warning, CPLE_AppDefined,
                                  "Can't find RCNM=%d,RCID=%d for update.\n",
                                  nRCNM, nRCID );
                    }
                    else
                    {
                        if( !ApplyRecordUpdate( poTarget, poRecord ) )
                        {
                            CPLError( CE_Warning, CPLE_AppDefined,
                                      "An update to RCNM=%d,RCID=%d failed.\n",
                                      nRCNM, nRCID );
                        }
                    }
                }
            }
        }

        else if( EQUAL(pszKey,"DSID") )
        {
            if( poDSIDRecord != NULL )
            {
                const char* pszUPDN
                    = poRecord->GetStringSubfield( "DSID", 0, "UPDN", 0 );
                if( pszUPDN != NULL && strlen(pszUPDN) < sizeof(szUPDNUpdate) )
                    strcpy( szUPDNUpdate, pszUPDN );
            }
        }

        else
        {
            CPLDebug( "S57",
                      "Skipping %s record in S57Reader::ApplyUpdates().\n",
                      pszKey );
        }
    }

    return CPLGetLastErrorType() != CE_Failure;
}

/************************************************************************/
/*                        FindAndApplyUpdates()                         */
/*                                                                      */
/*      Find all update files that would appear to apply to this        */
/*      base file.                                                      */
/************************************************************************/

bool S57Reader::FindAndApplyUpdates( const char * pszPath )

{
    if( pszPath == NULL )
        pszPath = pszModuleName;

    if( !EQUAL(CPLGetExtension(pszPath),"000") )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Can't apply updates to a base file with a different\n"
                  "extension than .000.\n" );
        return false;
    }

    bool bSuccess = true;

    for( int iUpdate = 1; bSuccess; iUpdate++ )
    {
        //Creaing file extension
        CPLString extension;
        CPLString dirname;

        if( 1 <= iUpdate &&  iUpdate < 10 )
        {
            char buf[2];
            CPLsnprintf( buf, sizeof(buf), "%i", iUpdate );
            extension.append("00");
            extension.append(buf);
            dirname.append(buf);
        }
        else if( 10 <= iUpdate && iUpdate < 100 )
        {
            char buf[3];
            CPLsnprintf( buf, sizeof(buf), "%i", iUpdate );
            extension.append("0");
            extension.append(buf);
            dirname.append(buf);
        }
        else if( 100 <= iUpdate && iUpdate < 1000 )
        {
            char buf[4];
            CPLsnprintf( buf, sizeof(buf), "%i", iUpdate );
            extension.append(buf);
            dirname.append(buf);
        }

        DDFModule oUpdateModule;

        //trying current dir first
        char *pszUpdateFilename =
            CPLStrdup(CPLResetExtension(pszPath,extension.c_str()));

        VSILFILE *file = VSIFOpenL( pszUpdateFilename, "r" );
        if( file )
        {
            VSIFCloseL( file );
            bSuccess = CPL_TO_BOOL(
                oUpdateModule.Open( pszUpdateFilename, TRUE ) );
            if( bSuccess )
            {
                CPLDebug( "S57", "Applying feature updates from %s.",
                          pszUpdateFilename );
                if( !ApplyUpdates( &oUpdateModule ) )
                    return false;
            }
        }
        else // File is store on Primar generated CD.
        {
            char* pszBaseFileDir = CPLStrdup(CPLGetDirname(pszPath));
            char* pszFileDir = CPLStrdup(CPLGetDirname(pszBaseFileDir));

            CPLString remotefile(pszFileDir);
            remotefile.append( "/" );
            remotefile.append( dirname );
            remotefile.append( "/" );
            remotefile.append( CPLGetBasename(pszPath) );
            remotefile.append( "." );
            remotefile.append( extension );
            bSuccess = CPL_TO_BOOL(
                oUpdateModule.Open( remotefile.c_str(), TRUE ) );

            if( bSuccess )
                CPLDebug( "S57", "Applying feature updates from %s.",
                          remotefile.c_str() );
            CPLFree( pszBaseFileDir );
            CPLFree( pszFileDir );
            if( bSuccess )
            {
                if( !ApplyUpdates( &oUpdateModule ) )
                    return false;
            }
        }//end for if-else
        CPLFree( pszUpdateFilename );
    }

    return true;
}

/************************************************************************/
/*                             GetExtent()                              */
/*                                                                      */
/*      Scan all the cached records collecting spatial bounds as        */
/*      efficiently as possible for this transfer.                      */
/************************************************************************/

OGRErr S57Reader::GetExtent( OGREnvelope *psExtent, int bForce )

{
/* -------------------------------------------------------------------- */
/*      If we aren't forced to get the extent say no if we haven't      */
/*      already indexed the iso8211 records.                            */
/* -------------------------------------------------------------------- */
    if( !bForce && !bFileIngested )
        return OGRERR_FAILURE;

    if( !Ingest() )
        return OGRERR_FAILURE;

/* -------------------------------------------------------------------- */
/*      We will scan all the low level vector elements for extents      */
/*      coordinates.                                                    */
/* -------------------------------------------------------------------- */
    bool bGotExtents = false;
    int nXMin=0;
    int nXMax=0;
    int nYMin=0;
    int nYMax=0;

    const int INDEX_COUNT = 4;
    DDFRecordIndex *apoIndex[INDEX_COUNT];

    apoIndex[0] = &oVI_Index;
    apoIndex[1] = &oVC_Index;
    apoIndex[2] = &oVE_Index;
    apoIndex[3] = &oVF_Index;

    for( int iIndex = 0; iIndex < INDEX_COUNT; iIndex++ )
    {
        DDFRecordIndex  *poIndex = apoIndex[iIndex];

        for( int iVIndex = 0; iVIndex < poIndex->GetCount(); iVIndex++ )
        {
            DDFRecord *poRecord = poIndex->GetByIndex( iVIndex );
            DDFField *poSG3D = poRecord->FindField( "SG3D" );
            DDFField *poSG2D = poRecord->FindField( "SG2D" );

            if( poSG3D != NULL )
            {
                const int  nVCount = poSG3D->GetRepeatCount();
                const GByte *pabyData = (const GByte*)poSG3D->GetData();
                if( poSG3D->GetDataSize() <
                    3 * nVCount * static_cast<int>( sizeof(int) ) )
                    return OGRERR_FAILURE;

                for( int i = 0; i < nVCount; i++ )
                {
                    GInt32 nX = CPL_LSBSINT32PTR(pabyData + 4*(i*3+1));
                    GInt32 nY = CPL_LSBSINT32PTR(pabyData + 4*(i*3+0));

                    if( bGotExtents )
                    {
                        nXMin = std::min(nXMin, nX);
                        nXMax = std::max(nXMax, nX);
                        nYMin = std::min(nYMin, nY);
                        nYMax = std::max(nYMax, nY);
                    }
                    else
                    {
                        nXMin = nX;
                        nXMax = nX;
                        nYMin = nY;
                        nYMax = nY;
                        bGotExtents = true;
                    }
                }
            }
            else if( poSG2D != NULL )
            {
                const int nVCount = poSG2D->GetRepeatCount();

                if( poSG2D->GetDataSize() < 2 * nVCount * (int)sizeof(int) )
                    return OGRERR_FAILURE;

                const GByte *pabyData = (const GByte*)poSG2D->GetData();

                for( int i = 0; i < nVCount; i++ )
                {
                    const GInt32 nX = CPL_LSBSINT32PTR(pabyData + 4*(i*2+1));
                    const GInt32 nY = CPL_LSBSINT32PTR(pabyData + 4*(i*2+0));

                    if( bGotExtents )
                    {
                        nXMin = std::min(nXMin, nX);
                        nXMax = std::max(nXMax, nX);
                        nYMin = std::min(nYMin, nY);
                        nYMax = std::max(nYMax, nY);
                    }
                    else
                    {
                        nXMin = nX;
                        nXMax = nX;
                        nYMin = nY;
                        nYMax = nY;
                        bGotExtents = true;
                    }
                }
            }
        }
    }

    if( !bGotExtents )
    {
        return OGRERR_FAILURE;
    }
    else
    {
        psExtent->MinX = nXMin / static_cast<double>( nCOMF );
        psExtent->MaxX = nXMax / static_cast<double>( nCOMF );
        psExtent->MinY = nYMin / static_cast<double>( nCOMF );
        psExtent->MaxY = nYMax / static_cast<double>( nCOMF );

        return OGRERR_NONE;
    }
}
