/******************************************************************************
 * $Id$
 *
 * Project:  SDTS Translator
 * Purpose:  Implmementation of SDTSIndexedReader class.  This base class for
 *           various reader classes provides indexed caching of features for
 *           quick fetching when assembling composite features for other
 *           readers.
 * Author:   Frank Warmerdam, warmerda@home.com
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
 ******************************************************************************
 *
 * $Log$
 * Revision 1.1  1999/09/02 03:39:40  warmerda
 * New
 *
 */

#include "sdts_al.h"

/************************************************************************/
/*                         SDTSIndexedReader()                          */
/************************************************************************/

SDTSIndexedReader::SDTSIndexedReader()

{
    nIndexSize = 0;
    papoFeatures = NULL;
    iCurrentFeature = 0;
}

/************************************************************************/
/*                         ~SDTSIndexedReader()                         */
/************************************************************************/

SDTSIndexedReader::~SDTSIndexedReader()

{
    for( int i = 0; i < nIndexSize; i++ )
    {
        if( papoFeatures[i] != NULL )
            delete papoFeatures[i];
    }
    
    CPLFree( papoFeatures );
}

/************************************************************************/
/*                           GetNextFeature()                           */
/*                                                                      */
/*      Read from index if file is in memory, otherwise pass off to     */
/*      the derived class to read a raw feature from the module.        */
/************************************************************************/

SDTSFeature *SDTSIndexedReader::GetNextFeature()

{
    if( nIndexSize == 0 )
        return GetNextRawFeature();
    else
    {
        while( iCurrentFeature < nIndexSize )
        {
            if( papoFeatures[iCurrentFeature] != NULL )
                return papoFeatures[iCurrentFeature++];
            else
                iCurrentFeature++;
        }

        return NULL;
    }
}

/************************************************************************/
/*                        GetIndexedFeatureRef()                        */
/************************************************************************/

SDTSFeature *SDTSIndexedReader::GetIndexedFeatureRef( int iRecordId )

{
    if( nIndexSize == 0 )
        FillIndex();

    if( iRecordId < 0 || iRecordId >= nIndexSize )
        return NULL;
    else
        return papoFeatures[iRecordId];
}

/************************************************************************/
/*                             FillIndex()                              */
/************************************************************************/

void SDTSIndexedReader::FillIndex()

{
    SDTSFeature		*poFeature;

    if( nIndexSize != 0 )
        return;

    Rewind();
    
    while( (poFeature = GetNextRawFeature()) != NULL )
    {
        int	iRecordId = poFeature->oModId.nRecord;

        CPLAssert( iRecordId < 1000000 );
        if( iRecordId >= 1000000 )
        {
            delete poFeature;
            continue;
        }

        if( iRecordId >= nIndexSize )
        {
            int		nNewSize = (int) (nIndexSize * 1.25 + 100);

            papoFeatures = (SDTSFeature **)
                CPLRealloc( papoFeatures, sizeof(void*) * nNewSize);

            for( int i = nIndexSize; i < nNewSize; i++ )
                papoFeatures[i] = NULL;

            nIndexSize = nNewSize;
        }

        CPLAssert( papoFeatures[iRecordId] == NULL );
        papoFeatures[iRecordId] = poFeature;
    }
}

/************************************************************************/
/*                        ScanModuleReferences()                        */
/************************************************************************/

char ** SDTSIndexedReader::ScanModuleReferences( const char * pszFName )

{
    return SDTSScanModuleReferences( &oDDFModule, pszFName );
}

/************************************************************************/
/*                               Rewind()                               */
/************************************************************************/

void SDTSIndexedReader::Rewind()

{
    if( nIndexSize != 0 )
        iCurrentFeature = 0;
    else
        oDDFModule.Rewind();
}
