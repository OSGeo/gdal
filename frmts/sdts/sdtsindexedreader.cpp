/******************************************************************************
 *
 * Project:  SDTS Translator
 * Purpose:  Implmementation of SDTSIndexedReader class.  This base class for
 *           various reader classes provides indexed caching of features for
 *           quick fetching when assembling composite features for other
 *           readers.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Frank Warmerdam
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

#include "sdts_al.h"

CPL_CVSID("$Id$")

/************************************************************************/
/*                         SDTSIndexedReader()                          */
/************************************************************************/

SDTSIndexedReader::SDTSIndexedReader() :
    nIndexSize(-1),
    papoFeatures(nullptr),
    iCurrentFeature(0)
{}

/************************************************************************/
/*                         ~SDTSIndexedReader()                         */
/************************************************************************/

SDTSIndexedReader::~SDTSIndexedReader()

{
    ClearIndex();
}

/************************************************************************/
/*                             IsIndexed()                              */
/************************************************************************/

/**
  Returns TRUE if the module is indexed, otherwise it returns FALSE.

  If the module is indexed all the feature have already been read into
  memory, and searches based on the record number can be performed
  efficiently.
  */

int SDTSIndexedReader::IsIndexed() const

{
    return nIndexSize >= 0;
}

/************************************************************************/
/*                             ClearIndex()                             */
/************************************************************************/

/**
  Free all features in the index (if filled).

  After this the reader is considered to not be indexed, and IsIndexed()
  will return FALSE until the index is forcibly filled again.
  */

void SDTSIndexedReader::ClearIndex()

{
    for( int i = 0; i < nIndexSize; i++ )
    {
        if( papoFeatures[i] != nullptr )
            delete papoFeatures[i];
    }

    CPLFree( papoFeatures );

    papoFeatures = nullptr;
    nIndexSize = 0;
}

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

/**
  Fetch the next available feature from this reader.

  The returned SDTSFeature * is to an internal indexed object if the
  IsIndexed() method returns TRUE, otherwise the returned feature becomes the
  responsibility of the caller to destroy with delete.

  Note that the Rewind() method can be used to start over at the beginning of
  the modules feature list.

  @return next feature, or NULL if no more are left.  Please review above
  ownership/delete semantics.

  */

SDTSFeature *SDTSIndexedReader::GetNextFeature()

{
    if( nIndexSize < 0 )
        return GetNextRawFeature();

    while( iCurrentFeature < nIndexSize )
    {
        if( papoFeatures[iCurrentFeature] != nullptr )
            return papoFeatures[iCurrentFeature++];
        else
            iCurrentFeature++;
    }

    return nullptr;
}

/************************************************************************/
/*                        GetIndexedFeatureRef()                        */
/************************************************************************/

/**
 Fetch a feature based on its record number.

 This method will forcibly fill the feature cache, reading all the
 features in the file into memory, if they haven't already been loaded.
 The ClearIndex() method can be used to flush this cache when no longer
 needed.

 @param iRecordId the record to fetch, normally based on the nRecord
 field of an SDTSModId.

 @return a pointer to an internal feature (not to be deleted) or NULL
 if there is no matching feature.
*/

SDTSFeature *SDTSIndexedReader::GetIndexedFeatureRef( int iRecordId )

{
    if( nIndexSize < 0 )
        FillIndex();

    if( iRecordId < 0 || iRecordId >= nIndexSize )
        return nullptr;

    return papoFeatures[iRecordId];
}

/************************************************************************/
/*                             FillIndex()                              */
/************************************************************************/

/**
 Read all features into a memory indexed cached.

 The ClearIndex() method can be used to free all indexed features.
 FillIndex() does nothing, if an index has already been built.
*/

void SDTSIndexedReader::FillIndex()

{
    if( nIndexSize >= 0 )
        return;

    Rewind();
    nIndexSize = 0;

    SDTSFeature *poFeature = nullptr;
    while( (poFeature = GetNextRawFeature()) != nullptr )
    {
        const int iRecordId = poFeature->oModId.nRecord;

        if( iRecordId < 0 || iRecordId >= 1000000 )
        {
            delete poFeature;
            continue;
        }
        if( iRecordId < nIndexSize && papoFeatures[iRecordId] != nullptr )
        {
            delete poFeature;
            continue;
        }

        if( iRecordId >= nIndexSize )
        {
            const int nNewSize = static_cast<int>(iRecordId * 1.25 + 100);

            papoFeatures = reinterpret_cast<SDTSFeature **>(
                CPLRealloc( papoFeatures, sizeof(void*) * nNewSize ) );

            for( int i = nIndexSize; i < nNewSize; i++ )
                papoFeatures[i] = nullptr;

            nIndexSize = nNewSize;
        }

        papoFeatures[iRecordId] = poFeature;
    }
}

/************************************************************************/
/*                        ScanModuleReferences()                        */
/************************************************************************/

/**
  Scan an entire SDTS module for record references with the given field
  name.

  The fields are required to have a MODN subfield from which the
  module is extracted.

  This method is normally used to find all the attribute modules referred
  to by a point, line or polygon module to build a unified schema.

  This method will have the side effect of rewinding unindexed readers
  because the scanning operation requires reading all records in the module
  from disk.

  @param pszFName the field name to search for.  By default "ATID" is
  used.

  @return a NULL terminated list of module names.  Free with CSLDestroy().
*/

char ** SDTSIndexedReader::ScanModuleReferences( const char * pszFName )

{
    return SDTSScanModuleReferences( &oDDFModule, pszFName );
}

/************************************************************************/
/*                               Rewind()                               */
/************************************************************************/

/**
  Rewind so that the next feature returned by GetNextFeature() will be the
  first in the module.

*/

void SDTSIndexedReader::Rewind()

{
    if( nIndexSize >= 0 )
        iCurrentFeature = 0;
    else
        oDDFModule.Rewind();
}
