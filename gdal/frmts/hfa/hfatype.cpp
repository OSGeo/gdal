/******************************************************************************
 *
 * Project:  Erdas Imagine (.img) Translator
 * Purpose:  Implementation of the HFAType class, for managing one type
 *           defined in the HFA data dictionary.  Managed by HFADictionary.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Intergraph Corporation
 * Copyright (c) 2009-2011, Even Rouault <even dot rouault at mines-paris dot org>
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

#include "hfa_p.h"

CPL_CVSID("$Id$");

/************************************************************************/
/* ==================================================================== */
/*                             HFAType                                  */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                              HFAType()                               */
/************************************************************************/

HFAType::HFAType() :
    bInCompleteDefn(false),
    nBytes(0),
    nFields(0),
    papoFields(NULL),
    pszTypeName(NULL)
{}

/************************************************************************/
/*                              ~HFAType()                              */
/************************************************************************/

HFAType::~HFAType()

{
    for( int i = 0; i < nFields; i++ )
    {
        delete papoFields[i];
    }

    CPLFree( papoFields );

    CPLFree( pszTypeName );
}

/************************************************************************/
/*                             Initialize()                             */
/************************************************************************/

const char *HFAType::Initialize( const char * pszInput )

{
    if( *pszInput != '{' )
    {
        if( *pszInput != '\0' )
            CPLDebug( "HFAType", "Initialize(%60.60s) - unexpected input.",
                      pszInput );

        while( *pszInput != '{' && *pszInput != '\0' )
            pszInput++;

        if( *pszInput == '\0' )
            return NULL;
    }

    pszInput++;

/* -------------------------------------------------------------------- */
/*      Read the field definitions.                                     */
/* -------------------------------------------------------------------- */
    while( pszInput != NULL && *pszInput != '}' )
    {
        HFAField *poNewField = new HFAField();

        pszInput = poNewField->Initialize( pszInput );
        if( pszInput != NULL )
        {
            papoFields = static_cast<HFAField **>(
                CPLRealloc(papoFields, sizeof(void*) * (nFields + 1)));
            papoFields[nFields++] = poNewField;
        }
        else
        {
            delete poNewField;
        }
    }

    if( pszInput == NULL )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Collect the name.                                               */
/* -------------------------------------------------------------------- */
    pszInput++;  // Skip `}'
    int i = 0;  // Used after for.
    for( ; pszInput[i] != '\0' && pszInput[i] != ','; i++ ) {}
    if( pszInput[i] == '\0' )
        return NULL;

    pszTypeName = static_cast<char *>(CPLMalloc(i+1));
    strncpy( pszTypeName, pszInput, i );
    pszTypeName[i] = '\0';

    pszInput += i + 1;

    return pszInput;
}

/************************************************************************/
/*                            CompleteDefn()                            */
/************************************************************************/

void HFAType::CompleteDefn( HFADictionary * poDict )

{
/* -------------------------------------------------------------------- */
/*      This may already be done, if an earlier object required this    */
/*      object (as a field), and forced an early computation of the     */
/*      size.                                                           */
/* -------------------------------------------------------------------- */
    if( nBytes != 0 )
        return;

    if( bInCompleteDefn )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Recursion detected in HFAType::CompleteDefn()");
        return;
    }
    bInCompleteDefn = true;

/* -------------------------------------------------------------------- */
/*      Complete each of the fields, totaling up the sizes.  This       */
/*      isn't really accurate for object with variable sized            */
/*      subobjects.                                                     */
/* -------------------------------------------------------------------- */
    for( int i = 0; i < nFields; i++ )
    {
        papoFields[i]->CompleteDefn( poDict );
        if( papoFields[i]->nBytes < 0 || nBytes == -1 )
            nBytes = -1;
        else if( nBytes < INT_MAX - papoFields[i]->nBytes )
            nBytes += papoFields[i]->nBytes;
        else
            nBytes = -1;
    }

    bInCompleteDefn = false;
}

/************************************************************************/
/*                                Dump()                                */
/************************************************************************/

void HFAType::Dump( FILE * fp )

{
    CPL_IGNORE_RET_VAL(
        VSIFPrintf( fp, "HFAType %s/%d bytes\n", pszTypeName, nBytes ));

    for( int i = 0; i < nFields; i++ )
    {
        papoFields[i]->Dump( fp );
    }

    CPL_IGNORE_RET_VAL(VSIFPrintf( fp, "\n" ));
}

/************************************************************************/
/*                            SetInstValue()                            */
/************************************************************************/

CPLErr
HFAType::SetInstValue( const char * pszFieldPath,
                       GByte *pabyData, GUInt32 nDataOffset, int nDataSize,
                       char chReqType, void *pValue )

{
    int nArrayIndex = 0;
    int nNameLen = 0;
    const char *pszRemainder = NULL;

/* -------------------------------------------------------------------- */
/*      Parse end of field name, possible index value and               */
/*      establish where the remaining fields (if any) would start.      */
/* -------------------------------------------------------------------- */
    if( strchr(pszFieldPath, '[') != NULL )
    {
        const char *pszEnd = strchr(pszFieldPath, '[');

        nArrayIndex = atoi(pszEnd+1);
        nNameLen = static_cast<int>(pszEnd - pszFieldPath);

        pszRemainder = strchr(pszFieldPath, '.');
        if( pszRemainder != NULL )
            pszRemainder++;
    }
    else if( strchr(pszFieldPath, '.') != NULL )
    {
        const char *pszEnd = strchr(pszFieldPath, '.');

        nNameLen = static_cast<int>(pszEnd - pszFieldPath);

        pszRemainder = pszEnd + 1;
    }
    else
    {
        nNameLen = static_cast<int>(strlen(pszFieldPath));
        pszRemainder = pszFieldPath /* NULL */;
    }

/* -------------------------------------------------------------------- */
/*      Find this field within this type, if possible.                  */
/* -------------------------------------------------------------------- */
    int nByteOffset = 0;
    int iField = 0;
    for( ; iField < nFields && nByteOffset < nDataSize; iField++ )
    {
        if( EQUALN(pszFieldPath, papoFields[iField]->pszFieldName, nNameLen)
            && papoFields[iField]->pszFieldName[nNameLen] == '\0' )
        {
            break;
        }

        const int nInc =
            papoFields[iField]->GetInstBytes( pabyData+nByteOffset,
                                              nDataSize - nByteOffset );

        if( nInc < 0 ||
            nByteOffset > INT_MAX - nInc )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Invalid return value");
            return CE_Failure;
        }

        nByteOffset += nInc;
    }

    if( iField == nFields || nByteOffset >= nDataSize )
        return CE_Failure;

/* -------------------------------------------------------------------- */
/*      Extract this field value, and return.                           */
/* -------------------------------------------------------------------- */
    return papoFields[iField]->SetInstValue( pszRemainder, nArrayIndex,
                                             pabyData + nByteOffset,
                                             nDataOffset + nByteOffset,
                                             nDataSize - nByteOffset,
                                             chReqType, pValue );
}

/************************************************************************/
/*                            GetInstCount()                            */
/************************************************************************/

int
HFAType::GetInstCount( const char * pszFieldPath,
                       GByte *pabyData,
                       GUInt32 /* nDataOffset */,
                       int nDataSize )
{
    // int nArrayIndex = 0;
    int nNameLen = 0;
    // const char *pszRemainder;

/* -------------------------------------------------------------------- */
/*      Parse end of field name, possible index value and               */
/*      establish where the remaining fields (if any) would start.      */
/* -------------------------------------------------------------------- */
    if( strchr(pszFieldPath, '[') != NULL )
    {
        const char *pszEnd = strchr(pszFieldPath, '[');

        // nArrayIndex = atoi(pszEnd+1);
        nNameLen = static_cast<int>(pszEnd - pszFieldPath);

        // pszRemainder = strchr(pszFieldPath, '.');
        // if( pszRemainder != NULL )
        //    pszRemainder++;
    }
    else if( strchr(pszFieldPath, '.') != NULL )
    {
        const char *pszEnd = strchr(pszFieldPath, '.');

        nNameLen = static_cast<int>(pszEnd - pszFieldPath);

        // pszRemainder = pszEnd + 1;
    }
    else
    {
        nNameLen = static_cast<int>(strlen(pszFieldPath));
        // pszRemainder = NULL;
    }

/* -------------------------------------------------------------------- */
/*      Find this field within this type, if possible.                  */
/* -------------------------------------------------------------------- */
    int nByteOffset = 0;
    int iField = 0;
    for( ; iField < nFields && nByteOffset < nDataSize; iField++ )
    {
        if( EQUALN(pszFieldPath, papoFields[iField]->pszFieldName,nNameLen)
            && papoFields[iField]->pszFieldName[nNameLen] == '\0' )
        {
            break;
        }

        const int nInc =
            papoFields[iField]->GetInstBytes( pabyData+nByteOffset,
                                              nDataSize - nByteOffset );

        if( nInc < 0 || nByteOffset > INT_MAX - nInc )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Invalid return value");
            return -1;
        }

        nByteOffset += nInc;
    }

    if( iField == nFields || nByteOffset >= nDataSize )
        return -1;

/* -------------------------------------------------------------------- */
/*      Extract this field value, and return.                           */
/* -------------------------------------------------------------------- */
    return papoFields[iField]->GetInstCount( pabyData + nByteOffset,
                                             nDataSize - nByteOffset );
}

/************************************************************************/
/*                          ExtractInstValue()                          */
/*                                                                      */
/*      Extract the value of a field instance within this type.         */
/*      Most of the work is done by the ExtractInstValue() for the      */
/*      HFAField, but this method does the field name parsing.          */
/*                                                                      */
/*      field names have the form:                                      */
/*                                                                      */
/*        fieldname{[index]}{.fieldname...}                             */
/*                                                                      */
/*      eg.                                                             */
/*        abc                                   - field abc[0]          */
/*        abc[3]                                - field abc[3]          */
/*        abc[2].def                            - field def[0] of       */
/*                                                the third abc struct. */
/************************************************************************/

bool
HFAType::ExtractInstValue( const char * pszFieldPath,
                           GByte *pabyData, GUInt32 nDataOffset, int nDataSize,
                           char chReqType, void *pReqReturn,
                           int *pnRemainingDataSize )

{
    int nArrayIndex = 0;
    int nNameLen = 0;
    const char *pszRemainder = NULL;

/* -------------------------------------------------------------------- */
/*      Parse end of field name, possible index value and               */
/*      establish where the remaining fields (if any) would start.      */
/* -------------------------------------------------------------------- */
    const char *pszFirstArray = strchr(pszFieldPath, '[');
    const char *pszFirstDot = strchr(pszFieldPath, '.');

    if( pszFirstArray != NULL
        && (pszFirstDot == NULL
            || pszFirstDot > pszFirstArray) )
    {
        const char *pszEnd = pszFirstArray;

        nArrayIndex = atoi(pszEnd+1);
        nNameLen = static_cast<int>(pszEnd - pszFieldPath);

        pszRemainder = strchr(pszFieldPath, '.');
        if( pszRemainder != NULL )
            pszRemainder++;
    }
    else if( pszFirstDot != NULL )
    {
        const char *pszEnd = pszFirstDot;

        nNameLen = static_cast<int>(pszEnd - pszFieldPath);

        pszRemainder = pszEnd + 1;
    }
    else
    {
        nNameLen = static_cast<int>(strlen(pszFieldPath));
        pszRemainder = NULL;
    }

/* -------------------------------------------------------------------- */
/*      Find this field within this type, if possible.                  */
/* -------------------------------------------------------------------- */
    int nByteOffset = 0;
    int iField = 0;
    for( ; iField < nFields && nByteOffset < nDataSize; iField++ )
    {
        if( EQUALN(pszFieldPath, papoFields[iField]->pszFieldName, nNameLen)
            && papoFields[iField]->pszFieldName[nNameLen] == '\0' )
        {
            break;
        }

        const int nInc =
            papoFields[iField]->GetInstBytes( pabyData+nByteOffset,
                                              nDataSize - nByteOffset );

        if( nInc < 0 || nByteOffset > INT_MAX - nInc )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Invalid return value");
            return false;
        }

        nByteOffset += nInc;
    }

    if( iField == nFields || nByteOffset >= nDataSize )
        return false;

/* -------------------------------------------------------------------- */
/*      Extract this field value, and return.                           */
/* -------------------------------------------------------------------- */
    return papoFields[iField]->
        ExtractInstValue( pszRemainder, nArrayIndex,
                          pabyData + nByteOffset,
                          nDataOffset + nByteOffset,
                          nDataSize - nByteOffset,
                          chReqType, pReqReturn,
                          pnRemainingDataSize);
}


/************************************************************************/
/*                           DumpInstValue()                            */
/************************************************************************/

void HFAType::DumpInstValue( FILE * fpOut,
                             GByte *pabyData, GUInt32 nDataOffset,
                             int nDataSize, const char * pszPrefix )

{
    for( int iField = 0; iField < nFields && nDataSize > 0; iField++ )
    {
        HFAField *poField = papoFields[iField];

        poField->DumpInstValue( fpOut, pabyData, nDataOffset,
                                nDataSize, pszPrefix );

        const int nInstBytes = poField->GetInstBytes( pabyData, nDataSize );
        if( nInstBytes < 0 || nDataOffset > UINT_MAX - nInstBytes )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Invalid return value");
            return;
        }

        pabyData += nInstBytes;
        nDataOffset += nInstBytes;
        nDataSize -= nInstBytes;
    }
}

/************************************************************************/
/*                            GetInstBytes()                            */
/*                                                                      */
/*      How many bytes in this particular instance of this type?        */
/************************************************************************/

int HFAType::GetInstBytes( GByte *pabyData, int nDataSize )

{
    if( nBytes >= 0 )
        return nBytes;

    int nTotal = 0;

    for( int iField = 0; iField < nFields && nTotal < nDataSize; iField++ )
    {
        HFAField *poField = papoFields[iField];

        const int nInstBytes = poField->GetInstBytes( pabyData,
                                                      nDataSize - nTotal );
        if( nInstBytes < 0 || nTotal > INT_MAX - nInstBytes )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Invalid return value");
            return -1;
        }

        pabyData += nInstBytes;
        nTotal += nInstBytes;
    }

    return nTotal;
}
