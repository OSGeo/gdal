/******************************************************************************
 *
 * Project:  Erdas Imagine (.img) Translator
 * Purpose:  Implementation of the HFAEntry class for reading and relating
 *           one node in the HFA object tree structure.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Intergraph Corporation
 * Copyright (c) 2008-2011, Even Rouault <even dot rouault at mines-paris dot org>
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
 * hfaentry.cpp
 *
 * Implementation of the HFAEntry class.
 *
 */

#include "cpl_port.h"
#include "hfa_p.h"

#include <cerrno>
#include <climits>
#include <cstddef>
#include <cstdio>
#include <cstring>
#if HAVE_FCNTL_H
#  include <fcntl.h>
#endif
#include <vector>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_string.h"
#include "cpl_vsi.h"

CPL_CVSID("$Id$")

/************************************************************************/
/*                              HFAEntry()                              */
/************************************************************************/

HFAEntry::HFAEntry() :
    bDirty(false),
    nFilePos(0),
    psHFA(NULL),
    poParent(NULL),
    poPrev(NULL),
    nNextPos(0),
    poNext(NULL),
    nChildPos(0),
    poChild(NULL),
    poType(NULL),
    nDataPos(0),
    nDataSize(0),
    pabyData(NULL),
    bIsMIFObject(false)
{
    szName[0] = '\0';
    szType[0] = '\0';
}

/************************************************************************/
/*                              HFAEntry()                              */
/*                                                                      */
/*      Construct an HFAEntry from the source file.                     */
/************************************************************************/

HFAEntry* HFAEntry::New( HFAInfo_t *psHFAIn, GUInt32 nPos,
                         HFAEntry * poParentIn, HFAEntry *poPrevIn )

{
    HFAEntry *poEntry = new HFAEntry;
    poEntry->psHFA = psHFAIn;

    poEntry->nFilePos = nPos;
    poEntry->poParent = poParentIn;
    poEntry->poPrev = poPrevIn;

    // Read the entry information from the file.
    GInt32 anEntryNums[6] = {};

    if( VSIFSeekL( poEntry->psHFA->fp, poEntry->nFilePos, SEEK_SET ) == -1 ||
        VSIFReadL( anEntryNums, sizeof(GInt32) * 6, 1, poEntry->psHFA->fp ) < 1 )
    {
        CPLError(CE_Failure, CPLE_FileIO,
                 "VSIFReadL(%p,6*4) @ %u failed in HFAEntry().\n%s",
                 poEntry->psHFA->fp, poEntry->nFilePos, VSIStrerror(errno));
        delete poEntry;
        return NULL;
    }

    for( int i = 0; i < 6; i++ )
        HFAStandard(4, anEntryNums + i);

    poEntry->nNextPos = anEntryNums[0];
    poEntry->nChildPos = anEntryNums[3];
    poEntry->nDataPos = anEntryNums[4];
    poEntry->nDataSize = anEntryNums[5];

    // Read the name, and type.
    if( VSIFReadL(poEntry->szName, 64, 1, poEntry->psHFA->fp) < 1 ||
        VSIFReadL(poEntry->szType, 32, 1, poEntry->psHFA->fp) < 1 )
    {
        poEntry->szName[sizeof(poEntry->szName) - 1] = '\0';
        poEntry->szType[sizeof(poEntry->szType) - 1] = '\0';
        CPLError(CE_Failure, CPLE_FileIO, "VSIFReadL() failed in HFAEntry().");
        delete poEntry;
        return NULL;
    }
    poEntry->szName[sizeof(poEntry->szName) - 1] = '\0';
    poEntry->szType[sizeof(poEntry->szType) - 1] = '\0';
    return poEntry;
}

/************************************************************************/
/*                              HFAEntry()                              */
/*                                                                      */
/*      Construct an HFAEntry in memory, with the intention that it     */
/*      would be written to disk later.                                 */
/************************************************************************/

HFAEntry::HFAEntry( HFAInfo_t * psHFAIn,
                    const char * pszNodeName,
                    const char * pszTypeName,
                    HFAEntry * poParentIn ) :
    nFilePos(0),
    psHFA(psHFAIn),
    poParent(poParentIn),
    poPrev(NULL),
    nNextPos(0),
    poNext(NULL),
    nChildPos(0),
    poChild(NULL),
    poType(NULL),
    nDataPos(0),
    nDataSize(0),
    pabyData(NULL),
    bIsMIFObject(false)
{
    // Initialize Entry.
    SetName(pszNodeName);
    memset(szType, 0, sizeof(szType));
    snprintf(szType, sizeof(szType), "%s", pszTypeName);

    // Update the previous or parent node to refer to this one.
    if( poParent == NULL )
    {
        // Do nothing.
    }
    else if( poParent->poChild == NULL )
    {
        poParent->poChild = this;
        poParent->MarkDirty();
    }
    else
    {
        poPrev = poParent->poChild;
        while( poPrev->poNext != NULL )
            poPrev = poPrev->poNext;

        poPrev->poNext = this;
        poPrev->MarkDirty();
    }

    MarkDirty();
}

/************************************************************************/
/*                              New()                                   */
/*                                                                      */
/*      Construct an HFAEntry in memory, with the intention that it     */
/*      would be written to disk later.                                 */
/************************************************************************/

HFAEntry* HFAEntry::New( HFAInfo_t *psHFAIn,
                         const char *pszNodeName,
                         const char *pszTypeName,
                         HFAEntry *poParentIn )
{
    CPLAssert(poParentIn != NULL);
    return new HFAEntry(psHFAIn, pszNodeName, pszTypeName, poParentIn);
}

/************************************************************************/
/*                      BuildEntryFromMIFObject()                       */
/*                                                                      */
/*      Create a pseudo-HFAEntry wrapping a MIFObject.                  */
/************************************************************************/

HFAEntry *HFAEntry::BuildEntryFromMIFObject( HFAEntry *poContainer,
                                             const char *pszMIFObjectPath )
{
    CPLString osFieldName;

    osFieldName.Printf("%s.%s", pszMIFObjectPath, "MIFDictionary");
    const char *pszField = poContainer->GetStringField(osFieldName.c_str());
    if( pszField == NULL )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot find %s entry",
                 osFieldName.c_str());
        return NULL;
    }
    CPLString osDictionary = pszField;

    osFieldName.Printf("%s.%s", pszMIFObjectPath, "type.string");
    pszField = poContainer->GetStringField(osFieldName.c_str());
    if( pszField == NULL )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot find %s entry",
                 osFieldName.c_str());
        return NULL;
    }
    CPLString osType = pszField;

    osFieldName.Printf("%s.%s", pszMIFObjectPath, "MIFObject");
    int nRemainingDataSize = 0;
    pszField = poContainer->GetStringField(osFieldName.c_str(),
                                           NULL, &nRemainingDataSize);
    if( pszField == NULL )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot find %s entry",
                 osFieldName.c_str());
        return NULL;
    }

    GInt32 nMIFObjectSize = 0;
    // We rudely look before the field data to get at the pointer/size info.
    memcpy(&nMIFObjectSize, pszField - 8, 4);
    HFAStandard(4, &nMIFObjectSize);
    if( nMIFObjectSize <= 0 )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Invalid MIF object size (%d)",
                 nMIFObjectSize);
        return NULL;
    }

    // Check that we won't copy more bytes than available in the buffer.
    if( nMIFObjectSize > nRemainingDataSize )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Invalid MIF object size (%d > %d)",
                 nMIFObjectSize, nRemainingDataSize);
        return NULL;
    }

    GByte *l_pabyData = static_cast<GByte *>(VSIMalloc(nMIFObjectSize));
    if( l_pabyData == NULL )
        return NULL;

    memcpy(l_pabyData, pszField, nMIFObjectSize);

    return new HFAEntry(osDictionary, osType, nMIFObjectSize, l_pabyData);
}

/************************************************************************/
/*                              HFAEntry()                              */
/*                                                                      */
/*      Create a pseudo-HFAEntry wrapping a MIFObject.                  */
/************************************************************************/

HFAEntry::HFAEntry( const char *pszDictionary,
                    const char *pszTypeName,
                    int nDataSizeIn,
                    GByte *pabyDataIn ) :
    bDirty(false),
    nFilePos(0),
    poParent(NULL),
    poPrev(NULL),
    nNextPos(0),
    poNext(NULL),
    nChildPos(0),
    poChild(NULL),
    nDataPos(0),
    nDataSize(0),
    bIsMIFObject(true)
{
    // Initialize Entry
    memset(szName, 0, sizeof(szName));

    // Create a dummy HFAInfo_t.
    psHFA = static_cast<HFAInfo_t *>(CPLCalloc(sizeof(HFAInfo_t), 1));

    psHFA->eAccess = HFA_ReadOnly;
    psHFA->bTreeDirty = false;
    psHFA->poRoot = this;

    psHFA->poDictionary = new HFADictionary(pszDictionary);

    // Work out the type for this MIFObject.
    memset(szType, 0, sizeof(szType));
    snprintf(szType, sizeof(szType), "%s", pszTypeName);

    poType = psHFA->poDictionary->FindType(szType);

    nDataSize = nDataSizeIn;
    pabyData = pabyDataIn;
}

/************************************************************************/
/*                             ~HFAEntry()                              */
/*                                                                      */
/*      Ensure that children are cleaned up when this node is           */
/*      cleaned up.                                                     */
/************************************************************************/

HFAEntry::~HFAEntry()

{
    CPLFree(pabyData);

    if( poNext != NULL )
        delete poNext;

    if( poChild != NULL )
        delete poChild;

    if( bIsMIFObject )
    {
        delete psHFA->poDictionary;
        CPLFree(psHFA);
    }
}

/************************************************************************/
/*                          RemoveAndDestroy()                          */
/*                                                                      */
/*      Removes this entry, and its children from the current           */
/*      tree.  The parent and/or siblings are appropriately updated     */
/*      so that they will be flushed back to disk without the           */
/*      reference to this node.                                         */
/************************************************************************/

CPLErr HFAEntry::RemoveAndDestroy()

{
    if( poPrev != NULL )
    {
        poPrev->poNext = poNext;
        if( poNext != NULL )
            poPrev->nNextPos = poNext->nFilePos;
        else
            poPrev->nNextPos = 0;
        poPrev->MarkDirty();
    }
    if( poParent != NULL && poParent->poChild == this )
    {
        poParent->poChild = poNext;
        if( poNext )
            poParent->nChildPos = poNext->nFilePos;
        else
            poParent->nChildPos = 0;
        poParent->MarkDirty();
    }

    if( poNext != NULL )
    {
        poNext->poPrev = poPrev;
    }

    poNext = NULL;
    poPrev = NULL;
    poParent = NULL;

    delete this;

    return CE_None;
}

/************************************************************************/
/*                              SetName()                               */
/*                                                                      */
/*    Changes the name assigned to this node                            */
/************************************************************************/

void HFAEntry::SetName( const char *pszNodeName )
{
    memset(szName, 0, sizeof(szName));
    snprintf(szName, sizeof(szName), "%s", pszNodeName);

    MarkDirty();
}

/************************************************************************/
/*                              GetChild()                              */
/************************************************************************/

HFAEntry *HFAEntry::GetChild()

{
    // Do we need to create the child node?
    if( poChild == NULL && nChildPos != 0 )
    {
        poChild = HFAEntry::New(psHFA, nChildPos, this, NULL);
        if( poChild == NULL )
            nChildPos = 0;
    }

    return poChild;
}

/************************************************************************/
/*                              GetNext()                               */
/************************************************************************/

HFAEntry *HFAEntry::GetNext()

{
    // Do we need to create the next node?
    if( poNext == NULL && nNextPos != 0 )
    {
        // Check if we have a loop on the next node in this sibling chain.
        HFAEntry *poPast;

        for( poPast = this;
             poPast != NULL && poPast->nFilePos != nNextPos;
             poPast = poPast->poPrev ) {}

        if( poPast != NULL )
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "Corrupt (looping) entry in %s, "
                     "ignoring some entries after %s.",
                     psHFA->pszFilename,
                     szName);
            nNextPos = 0;
            return NULL;
        }

        poNext = HFAEntry::New(psHFA, nNextPos, poParent, this);
        if( poNext == NULL )
            nNextPos = 0;
    }

    return poNext;
}

/************************************************************************/
/*                              LoadData()                              */
/*                                                                      */
/*      Load the data for this entry, and build up the field            */
/*      information for it.                                             */
/************************************************************************/

void HFAEntry::LoadData()

{
    if( pabyData != NULL || nDataSize == 0 )
        return;
    if( nDataSize > INT_MAX - 1 )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Invalid value for nDataSize = %u", nDataSize);
        return;
    }

    // Allocate buffer, and read data.
    pabyData = static_cast<GByte *>(VSI_MALLOC_VERBOSE(nDataSize + 1));
    if( pabyData == NULL )
    {
        return;
    }

    if( VSIFSeekL(psHFA->fp, nDataPos, SEEK_SET) < 0 )
    {
        CPLError(CE_Failure, CPLE_FileIO,
                 "VSIFSeekL() failed in HFAEntry::LoadData().");
        return;
    }

    if( VSIFReadL(pabyData, nDataSize, 1, psHFA->fp) < 1 )
    {
        CPLError(CE_Failure, CPLE_FileIO,
                 "VSIFReadL() failed in HFAEntry::LoadData().");
        return;
    }

    // Make sure the buffer is always null terminated to avoid
    // issues when extracting strings from a corrupted file.
    pabyData[nDataSize] = '\0';

    // Get the type corresponding to this entry.
    poType = psHFA->poDictionary->FindType(szType);
    if( poType == NULL )
        return;
}

/************************************************************************/
/*                           GetTypeObject()                            */
/************************************************************************/

HFAType *HFAEntry::GetTypeObject()

{
    if( poType == NULL )
        poType = psHFA->poDictionary->FindType(szType);

    return poType;
}

/************************************************************************/
/*                              MakeData()                              */
/*                                                                      */
/*      Create a data block on the this HFAEntry in memory.  By         */
/*      default it will create the data the correct size for fixed      */
/*      sized types, or do nothing for variable length types.           */
/*      However, the caller can supply a desired size for variable      */
/*      sized fields.                                                   */
/************************************************************************/

GByte *HFAEntry::MakeData( int nSize )

{
    if( poType == NULL )
    {
        poType = psHFA->poDictionary->FindType(szType);
        if( poType == NULL )
            return NULL;
    }

    if( nSize == 0 && poType->nBytes > 0 )
        nSize = poType->nBytes;

    // nDataSize is a GUInt32.
    if( static_cast<int>(nDataSize) < nSize && nSize > 0 )
    {
        pabyData = static_cast<GByte *>(CPLRealloc(pabyData, nSize));
        memset(pabyData + nDataSize, 0, nSize - nDataSize);
        nDataSize = nSize;

        MarkDirty();

        // If the data already had a file position, we now need to
        // clear that, forcing it to be rewritten at the end of the
        // file.  Referencing nodes will need to be marked dirty so
        // they are rewritten.
        if( nFilePos != 0 )
        {
            nFilePos = 0;
            nDataPos = 0;
            if( poPrev != NULL ) poPrev->MarkDirty();
            if( poNext != NULL ) poNext->MarkDirty();
            if( poChild != NULL ) poChild->MarkDirty();
            if( poParent != NULL ) poParent->MarkDirty();
        }
    }
    else
    {
        LoadData();  // Make sure the data is loaded before we return pointer.
    }

    return pabyData;
}

/************************************************************************/
/*                          DumpFieldValues()                           */
/************************************************************************/

void HFAEntry::DumpFieldValues( FILE *fp, const char *pszPrefix )

{
    if( pszPrefix == NULL )
        pszPrefix = "";

    LoadData();

    if( pabyData == NULL || poType == NULL )
        return;

    poType->DumpInstValue(fp, pabyData, nDataPos, nDataSize, pszPrefix);
}

/************************************************************************/
/*                            FindChildren()                            */
/*                                                                      */
/*      Find all the children of the current node that match the        */
/*      name and type provided.  Either may be NULL if it is not a      */
/*      factor.  The pszName should be just the node name, not a        */
/*      path.                                                           */
/************************************************************************/

std::vector<HFAEntry*> HFAEntry::FindChildren( const char *pszName,
                                               const char *pszType,
                                               int nRecLevel,
                                               int *pbErrorDetected )

{
    std::vector<HFAEntry *> apoChildren;

    if( *pbErrorDetected )
        return apoChildren;
    if( nRecLevel == 50 )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Bad entry structure: recursion detected !");
        *pbErrorDetected = TRUE;
        return apoChildren;
    }

    for( HFAEntry *poEntry = GetChild();
         poEntry != NULL;
         poEntry = poEntry->GetNext() )
    {
        std::vector<HFAEntry *> apoEntryChildren;

        if( (pszName == NULL || EQUAL(poEntry->GetName(), pszName)) &&
            (pszType == NULL || EQUAL(poEntry->GetType(), pszType)) )
            apoChildren.push_back(poEntry);

        apoEntryChildren = poEntry->FindChildren(
            pszName, pszType, nRecLevel + 1, pbErrorDetected);
        if( *pbErrorDetected )
            return apoChildren;

        for( size_t i = 0; i < apoEntryChildren.size(); i++ )
            apoChildren.push_back(apoEntryChildren[i]);
    }

    return apoChildren;
}

std::vector<HFAEntry*> HFAEntry::FindChildren( const char *pszName,
                                               const char *pszType)

{
    int bErrorDetected = FALSE;
    return FindChildren(pszName, pszType, 0, &bErrorDetected);
}

/************************************************************************/
/*                           GetNamedChild()                            */
/************************************************************************/

HFAEntry *HFAEntry::GetNamedChild( const char *pszName )

{
    // Establish how much of this name path is for the next child.
    // Up to the '.' or end of the string.
    int nNameLen = 0;
    for( ;
         pszName[nNameLen] != '.' &&
         pszName[nNameLen] != '\0' &&
         pszName[nNameLen] != ':';
         nNameLen++ ) {}

    // Scan children looking for this name.
    for( HFAEntry *poEntry = GetChild();
         poEntry != NULL;
         poEntry = poEntry->GetNext() )
    {
        if( EQUALN(poEntry->GetName(), pszName, nNameLen) &&
            static_cast<int>(strlen(poEntry->GetName())) == nNameLen )
        {
            if( pszName[nNameLen] == '.' )
            {
                HFAEntry *poResult;

                poResult = poEntry->GetNamedChild(pszName + nNameLen + 1);
                if( poResult != NULL )
                    return poResult;
            }
            else
                return poEntry;
        }
    }

    return NULL;
}

/************************************************************************/
/*                           GetFieldValue()                            */
/************************************************************************/

bool HFAEntry::GetFieldValue( const char *pszFieldPath,
                              char chReqType, void *pReqReturn,
                              int *pnRemainingDataSize )

{
    // Is there a node path in this string?
    if( strchr(pszFieldPath, ':') != NULL )
    {
        HFAEntry *poEntry = GetNamedChild(pszFieldPath);
        if( poEntry == NULL )
            return false;

        pszFieldPath = strchr(pszFieldPath, ':') + 1;
    }

    // Do we have the data and type for this node?
    LoadData();

    if( pabyData == NULL )
        return false;

    if( poType == NULL )
        return false;

    // Extract the instance information.
    return
        poType->ExtractInstValue(pszFieldPath,
                                 pabyData, nDataPos, nDataSize,
                                 chReqType, pReqReturn, pnRemainingDataSize);
}

/************************************************************************/
/*                           GetFieldCount()                            */
/************************************************************************/

int HFAEntry::GetFieldCount( const char *pszFieldPath, CPLErr * /* peErr */ )
{
    // Is there a node path in this string?
    if( strchr(pszFieldPath, ':') != NULL )
    {
        HFAEntry *poEntry = GetNamedChild(pszFieldPath);
        if( poEntry == NULL )
            return -1;

        pszFieldPath = strchr(pszFieldPath, ':') + 1;
    }

    // Do we have the data and type for this node?
    LoadData();

    if( pabyData == NULL )
        return -1;

    if( poType == NULL )
        return -1;

    // Extract the instance information.

    return poType->GetInstCount( pszFieldPath,
                                 pabyData, nDataPos, nDataSize );
}

/************************************************************************/
/*                            GetIntField()                             */
/************************************************************************/

GInt32 HFAEntry::GetIntField( const char *pszFieldPath, CPLErr *peErr )

{
    GInt32 nIntValue = 0;

    if( !GetFieldValue(pszFieldPath, 'i', &nIntValue, NULL) )
    {
        if( peErr != NULL )
            *peErr = CE_Failure;

        return 0;
    }

    if( peErr != NULL )
        *peErr = CE_None;

    return nIntValue;
}

/************************************************************************/
/*                           GetBigIntField()                           */
/*                                                                      */
/*      This is just a helper method that reads two ULONG array         */
/*      entries as a GIntBig.  The passed name should be the name of    */
/*      the array with no array index.  Array indexes 0 and 1 will      */
/*      be concatenated.                                                */
/************************************************************************/

GIntBig HFAEntry::GetBigIntField( const char *pszFieldPath, CPLErr *peErr )

{
    char szFullFieldPath[1024];

    snprintf(szFullFieldPath, sizeof(szFullFieldPath), "%s[0]", pszFieldPath);
    const GUInt32 nLower = GetIntField(szFullFieldPath, peErr);
    if( peErr != NULL && *peErr != CE_None )
        return 0;

    snprintf(szFullFieldPath, sizeof(szFullFieldPath), "%s[1]", pszFieldPath);
    const GUInt32 nUpper = GetIntField(szFullFieldPath, peErr);
    if( peErr != NULL && *peErr != CE_None )
        return 0;

    return nLower + (static_cast<GIntBig>(nUpper) << 32);
}

/************************************************************************/
/*                           GetDoubleField()                           */
/************************************************************************/

double HFAEntry::GetDoubleField( const char *pszFieldPath, CPLErr *peErr )

{
    double dfDoubleValue = 0;

    if( !GetFieldValue(pszFieldPath, 'd', &dfDoubleValue, NULL) )
    {
        if( peErr != NULL )
            *peErr = CE_Failure;

        return 0.0;
    }

    if( peErr != NULL )
        *peErr = CE_None;

    return dfDoubleValue;
}

/************************************************************************/
/*                           GetStringField()                           */
/************************************************************************/

const char *HFAEntry::GetStringField( const char *pszFieldPath, CPLErr *peErr,
                                      int *pnRemainingDataSize)

{
    char *pszResult = NULL;

    if( !GetFieldValue(pszFieldPath, 's', &pszResult, pnRemainingDataSize) )
    {
        if( peErr != NULL )
            *peErr = CE_Failure;

        return NULL;
    }

    if( peErr != NULL )
        *peErr = CE_None;

    return pszResult;
}

/************************************************************************/
/*                           SetFieldValue()                            */
/************************************************************************/

CPLErr HFAEntry::SetFieldValue( const char *pszFieldPath,
                                char chReqType, void *pValue )

{
    // Is there a node path in this string?
    if( strchr(pszFieldPath, ':') != NULL )
    {
        HFAEntry *poEntry = GetNamedChild(pszFieldPath);
        if( poEntry == NULL )
            return CE_Failure;

        pszFieldPath = strchr(pszFieldPath, ':') + 1;
    }

    // Do we have the data and type for this node?  Try loading
    // from a file, or instantiating a new node.
    LoadData();
    if( MakeData() == NULL || pabyData == NULL || poType == NULL )
    {
        return CE_Failure;
    }

    // Extract the instance information.
    MarkDirty();

    return poType->SetInstValue(pszFieldPath, pabyData, nDataPos, nDataSize,
                                chReqType, pValue);
}

/************************************************************************/
/*                           SetStringField()                           */
/************************************************************************/

CPLErr HFAEntry::SetStringField( const char *pszFieldPath,
                                 const char *pszValue )

{
    return SetFieldValue( pszFieldPath, 's', (void *) pszValue );
}

/************************************************************************/
/*                            SetIntField()                             */
/************************************************************************/

CPLErr HFAEntry::SetIntField( const char *pszFieldPath, int nValue )

{
    return SetFieldValue(pszFieldPath, 'i', &nValue);
}

/************************************************************************/
/*                           SetDoubleField()                           */
/************************************************************************/

CPLErr HFAEntry::SetDoubleField( const char *pszFieldPath,
                                 double dfValue )

{
    return SetFieldValue( pszFieldPath, 'd', &dfValue );
}

/************************************************************************/
/*                            SetPosition()                             */
/*                                                                      */
/*      Set the disk position for this entry, and recursively apply     */
/*      to any children of this node.  The parent will take care of     */
/*      our siblings.                                                   */
/************************************************************************/

void HFAEntry::SetPosition()

{
    // Establish the location of this entry, and its data.
    if( nFilePos == 0 )
    {
        nFilePos =
            HFAAllocateSpace(psHFA, psHFA->nEntryHeaderLength + nDataSize);

        if( nDataSize > 0 )
            nDataPos = nFilePos + psHFA->nEntryHeaderLength;
    }

    // Force all children to set their position.
    for( HFAEntry *poThisChild = poChild;
         poThisChild != NULL;
         poThisChild = poThisChild->poNext )
    {
        poThisChild->SetPosition();
    }
}

/************************************************************************/
/*                            FlushToDisk()                             */
/*                                                                      */
/*      Write this entry, and its data to disk if the entries           */
/*      information is dirty.  Also force children to do the same.      */
/************************************************************************/

CPLErr HFAEntry::FlushToDisk()

{
    // If we are the root node, call SetPosition() on the whole
    // tree to ensure that all entries have an allocated position.
    if( poParent == NULL )
        SetPosition();

    // Only write this node out if it is dirty.
    if( bDirty )
    {
        // Ensure we know where the relative entries are located.
        if( poNext != NULL )
            nNextPos = poNext->nFilePos;

        if( poChild != NULL )
            nChildPos = poChild->nFilePos;

        // Write the Ehfa_Entry fields.

        // VSIFFlushL(psHFA->fp);
        if( VSIFSeekL(psHFA->fp, nFilePos, SEEK_SET) != 0 )
        {
            CPLError(CE_Failure, CPLE_FileIO,
                     "Failed to seek to %d for writing, out of disk space?",
                     nFilePos);
            return CE_Failure;
        }

        GUInt32 nLong = nNextPos;
        HFAStandard(4, &nLong);
        bool bOK = VSIFWriteL(&nLong, 4, 1, psHFA->fp) > 0;

        if( poPrev != NULL )
            nLong = poPrev->nFilePos;
        else
            nLong = 0;
        HFAStandard(4, &nLong);
        bOK &= VSIFWriteL(&nLong, 4, 1, psHFA->fp) > 0;

        if( poParent != NULL )
            nLong = poParent->nFilePos;
        else
            nLong = 0;
        HFAStandard(4, &nLong);
        bOK &= VSIFWriteL(&nLong, 4, 1, psHFA->fp) > 0;

        nLong = nChildPos;
        HFAStandard(4, &nLong);
        bOK &= VSIFWriteL(&nLong, 4, 1, psHFA->fp) > 0;

        nLong = nDataPos;
        HFAStandard(4, &nLong);
        bOK &= VSIFWriteL(&nLong, 4, 1, psHFA->fp) > 0;

        nLong = nDataSize;
        HFAStandard(4, &nLong);
        bOK &= VSIFWriteL(&nLong, 4, 1, psHFA->fp) > 0;

        bOK &= VSIFWriteL(szName, 1, 64, psHFA->fp) > 0;
        bOK &= VSIFWriteL(szType, 1, 32, psHFA->fp) > 0;

        nLong = 0;  // Should we keep the time, or set it more reasonably?
        bOK &= VSIFWriteL(&nLong, 4, 1, psHFA->fp) > 0;
        if( !bOK )
        {
            CPLError( CE_Failure, CPLE_FileIO,
                      "Failed to write HFAEntry %s(%s), out of disk space?",
                      szName, szType );
            return CE_Failure;
        }

        // Write out the data.
        // VSIFFlushL(psHFA->fp);
        if( nDataSize > 0 && pabyData != NULL )
        {
            if( VSIFSeekL(psHFA->fp, nDataPos, SEEK_SET) != 0 ||
                VSIFWriteL(pabyData, nDataSize, 1, psHFA->fp) != 1 )
            {
                CPLError(CE_Failure, CPLE_FileIO,
                         "Failed to write %d bytes HFAEntry %s(%s) data, "
                         "out of disk space?",
                         nDataSize, szName, szType);
                return CE_Failure;
            }
        }

        // VSIFFlushL(psHFA->fp);
    }

    // Process all the children of this node.
    for( HFAEntry *poThisChild = poChild;
         poThisChild != NULL;
         poThisChild = poThisChild->poNext )
    {
        CPLErr eErr = poThisChild->FlushToDisk();
        if( eErr != CE_None )
            return eErr;
    }

    bDirty = false;

    return CE_None;
}

/************************************************************************/
/*                             MarkDirty()                              */
/*                                                                      */
/*      Mark this node as dirty (in need of writing to disk), and       */
/*      also mark the tree as a whole as being dirty.                   */
/************************************************************************/

void HFAEntry::MarkDirty()

{
    bDirty = true;
    psHFA->bTreeDirty = true;
}
