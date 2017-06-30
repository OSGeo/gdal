/******************************************************************************
 *
 * Project:  Erdas Imagine (.img) Translator
 * Purpose:  Implementation of the HFAField class for managing information
 *           about one field in a HFA dictionary type.  Managed by HFAType.
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
#include <algorithm>
#include <limits>
#include <vector>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_string.h"
#include "cpl_vsi.h"

CPL_CVSID("$Id$")

static const int MAX_ENTRY_REPORT = 16;

/************************************************************************/
/* ==================================================================== */
/*                              HFAField                                */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                              HFAField()                              */
/************************************************************************/

HFAField::HFAField() :
    nBytes(0),
    nItemCount(0),
    chPointer('\0'),
    chItemType('\0'),
    pszItemObjectType(NULL),
    poItemObjectType(NULL),
    papszEnumNames(NULL),
    pszFieldName(NULL)
{
    memset(szNumberString, 0, sizeof(szNumberString));
}

/************************************************************************/
/*                             ~HFAField()                              */
/************************************************************************/

HFAField::~HFAField()

{
    CPLFree(pszItemObjectType);
    CSLDestroy(papszEnumNames);
    CPLFree(pszFieldName);
}

/************************************************************************/
/*                             Initialize()                             */
/************************************************************************/

const char *HFAField::Initialize( const char *pszInput )

{
    // Read the number.
    nItemCount = atoi(pszInput);
    if( nItemCount < 0 )
        return NULL;

    while( *pszInput != '\0' && *pszInput != ':' )
        pszInput++;

    if( *pszInput == '\0' )
        return NULL;

    pszInput++;

    // Is this a pointer?
    if( *pszInput == 'p' || *pszInput == '*' )
        chPointer = *(pszInput++);

    // Get the general type.
    if( *pszInput == '\0' )
        return NULL;

    chItemType = *(pszInput++);

    if( strchr("124cCesStlLfdmMbox", chItemType) == NULL )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Unrecognized item type: %c", chItemType);
        return NULL;
    }

    // If this is an object, we extract the type of the object.
    int i = 0;  // TODO: Describe why i needs to span chItemType blocks.

    if( chItemType == 'o' )
    {
        for( i = 0; pszInput[i] != '\0' && pszInput[i] != ','; i++ ) {}
        if( pszInput[i] == '\0' )
            return NULL;

        pszItemObjectType = static_cast<char *>(CPLMalloc(i + 1));
        strncpy(pszItemObjectType, pszInput, i);
        pszItemObjectType[i] = '\0';

        pszInput += i + 1;
    }

    // If this is an inline object, we need to skip past the
    // definition, and then extract the object class name.
    //
    // We ignore the actual definition, so if the object type isn't
    // already defined, things will not work properly.  See the
    // file lceugr250_00_pct.aux for an example of inline defs.
    if( chItemType == 'x' && *pszInput == '{' )
    {
        int nBraceDepth = 1;
        pszInput++;

        // Skip past the definition.
        while( nBraceDepth > 0 && *pszInput != '\0' )
        {
            if( *pszInput == '{' )
                nBraceDepth++;
            else if( *pszInput == '}' )
                nBraceDepth--;

            pszInput++;
        }
        if( *pszInput == '\0' )
            return NULL;

        chItemType = 'o';

        // Find the comma terminating the type name.
        for( i = 0; pszInput[i] != '\0' && pszInput[i] != ','; i++ ) {}
        if( pszInput[i] == '\0' )
            return NULL;

        pszItemObjectType = static_cast<char *>(CPLMalloc(i + 1));
        strncpy(pszItemObjectType, pszInput, i);
        pszItemObjectType[i] = '\0';

        pszInput += i + 1;
    }

    // If this is an enumeration we have to extract all the
    // enumeration values.
    if( chItemType == 'e' )
    {
        const int nEnumCount = atoi(pszInput);

        if( nEnumCount < 0 || nEnumCount > 100000 )
            return NULL;

        pszInput = strchr(pszInput, ':');
        if( pszInput == NULL )
            return NULL;

        pszInput++;

        papszEnumNames =
            static_cast<char **>(VSICalloc(sizeof(char *), nEnumCount + 1));
        if( papszEnumNames == NULL )
            return NULL;

        for( int iEnum = 0; iEnum < nEnumCount; iEnum++ )
        {
            for( i = 0; pszInput[i] != '\0' && pszInput[i] != ','; i++ ) {}

            if( pszInput[i] != ',' )
                return NULL;

            char *pszToken = static_cast<char *>(CPLMalloc(i + 1));
            strncpy(pszToken, pszInput, i);
            pszToken[i] = '\0';

            papszEnumNames[iEnum] = pszToken;

            pszInput += i + 1;
        }
    }

    // Extract the field name.
    for( i = 0; pszInput[i] != '\0' && pszInput[i] != ','; i++ ) {}
    if( pszInput[i] == '\0' )
        return NULL;

    pszFieldName = static_cast<char *>(CPLMalloc(i + 1));
    strncpy(pszFieldName, pszInput, i);
    pszFieldName[i] = '\0';

    pszInput += i + 1;

    return pszInput;
}

/************************************************************************/
/*                            CompleteDefn()                            */
/*                                                                      */
/*      Establish size, and pointers to component types.                */
/************************************************************************/

bool HFAField::CompleteDefn( HFADictionary *poDict )

{
    // Get a reference to the type object if we have a type name
    // for this field (not a built in).
    if( pszItemObjectType != NULL )
        poItemObjectType = poDict->FindType(pszItemObjectType);

    // Figure out the size.
    if( chPointer == 'p' )
    {
        nBytes = -1;  // We can't know the instance size.
    }
    else if( poItemObjectType != NULL )
    {
        if( !poItemObjectType->CompleteDefn(poDict) )
            return false;
        if( poItemObjectType->nBytes == -1 )
            nBytes = -1;
        else if( poItemObjectType->nBytes != 0 &&
                 nItemCount > INT_MAX / poItemObjectType->nBytes )
            nBytes = -1;
        else
            nBytes = poItemObjectType->nBytes * nItemCount;

        // TODO(schwehr): What does the 8 represent?
        if( chPointer == '*' && nBytes != -1 )
        {
            if( nBytes > INT_MAX - 8 )
                nBytes = -1;
            else
                nBytes += 8;  // Count, and offset.
        }
    }
    else
    {
        const int nItemSize = poDict->GetItemSize(chItemType);
        if( nItemSize != 0 && nItemCount > INT_MAX / nItemSize )
            nBytes = -1;
        else
            nBytes = nItemSize * nItemCount;
    }
    return true;
}

/************************************************************************/
/*                                Dump()                                */
/************************************************************************/

void HFAField::Dump( FILE *fp )

{
    const char *pszTypeName;

    switch( chItemType )
    {
    case '1':
        pszTypeName = "U1";
        break;

    case '2':
        pszTypeName = "U2";
        break;

    case '4':
        pszTypeName = "U4";
        break;

    case 'c':
        pszTypeName = "UCHAR";
        break;

    case 'C':
        pszTypeName = "CHAR";
        break;

    case 'e':
        pszTypeName = "ENUM";
        break;

    case 's':
        pszTypeName = "USHORT";
        break;

    case 'S':
        pszTypeName = "SHORT";
        break;

    case 't':
        pszTypeName = "TIME";
        break;

    case 'l':
        pszTypeName = "ULONG";
        break;

    case 'L':
        pszTypeName = "LONG";
        break;

    case 'f':
        pszTypeName = "FLOAT";
        break;

    case 'd':
        pszTypeName = "DOUBLE";
        break;

    case 'm':
        pszTypeName = "COMPLEX";
        break;

    case 'M':
        pszTypeName = "DCOMPLEX";
        break;

    case 'b':
        pszTypeName = "BASEDATA";
        break;

    case 'o':
        pszTypeName = pszItemObjectType;
        break;

    case 'x':
        pszTypeName = "InlineType";
        break;

    default:
        CPLAssert(false);
        pszTypeName = "Unknown";
    }

    CPL_IGNORE_RET_VAL(
        VSIFPrintf(fp, "    %-19s %c %s[%d];\n",
                   pszTypeName,
                   chPointer ? chPointer : ' ',
                   pszFieldName, nItemCount));

    if( papszEnumNames != NULL )
    {
        for( int i = 0; papszEnumNames[i] != NULL; i++ )
        {
            CPL_IGNORE_RET_VAL(
                VSIFPrintf(fp, "        %s=%d\n", papszEnumNames[i], i));
        }
    }
}

/************************************************************************/
/*                            SetInstValue()                            */
/************************************************************************/

CPLErr
HFAField::SetInstValue( const char *pszField, int nIndexValue,
                        GByte *pabyData, GUInt32 nDataOffset, int nDataSize,
                        char chReqType, void *pValue )

{
    // If this field contains a pointer, then we will adjust the
    // data offset relative to it.
    if( chPointer != '\0' )
    {
        GUInt32 nCount = 0;

        // The count returned for BASEDATA's are the contents,
        // but here we really want to mark it as one BASEDATA instance
        // (see #2144).
        if( chItemType == 'b' )
        {
            nCount = 1;
        }
        // Set the size from string length.
        else if( chReqType == 's' && (chItemType == 'c' || chItemType == 'C'))
        {
            if( pValue != NULL )
                nCount = static_cast<GUInt32>(strlen((char *)pValue) + 1);
        }
        // Set size based on index. Assumes in-order setting of array.
        else
        {
            nCount = nIndexValue + 1;
        }

        // TODO(schwehr): What does the 8 represent?
        if( static_cast<int>(nCount) + 8 > nDataSize )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Attempt to extend field %s in node past end of data, "
                     "not currently supported.",
                     pszField);
            return CE_Failure;
        }

        // We will update the object count iff we are writing beyond the end.
        GUInt32 nOffset = 0;
        memcpy(&nOffset, pabyData, 4);
        HFAStandard(4, &nOffset);
        if( nOffset < nCount )
        {
            nOffset = nCount;
            HFAStandard(4, &nOffset);
            memcpy(pabyData, &nOffset, 4);
        }

        if( pValue == NULL )
            nOffset = 0;
        else
            nOffset = nDataOffset + 8;
        HFAStandard(4, &nOffset);
        memcpy(pabyData + 4, &nOffset, 4);

        pabyData += 8;

        nDataOffset += 8;
        nDataSize -= 8;
    }

    // Pointers to char or uchar arrays requested as strings are
    // handled as a special case.
    if( (chItemType == 'c' || chItemType == 'C') && chReqType == 's' )
    {
        int nBytesToCopy = 0;

        if( nBytes == -1 )
        {
            if( pValue != NULL )
                nBytesToCopy = static_cast<int>(strlen((char *)pValue) + 1);
        }
        else
        {
            nBytesToCopy = nBytes;
        }

        if( nBytesToCopy > nDataSize )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Attempt to extend field %s in node past end of data "
                     "not currently supported.",
                     pszField);
            return CE_Failure;
        }

        memset(pabyData, 0, nBytesToCopy);

        if( pValue != NULL )
            strncpy((char *)pabyData, (char *)pValue, nBytesToCopy);

        return CE_None;
    }

    // Translate the passed type into different representations.
    int nIntValue = 0;
    double dfDoubleValue = 0.0;

    if( chReqType == 's' )
    {
        CPLAssert(pValue != NULL);
        nIntValue = atoi((char *)pValue);
        dfDoubleValue = CPLAtof((char *)pValue);
    }
    else if( chReqType == 'd' )
    {
        CPLAssert(pValue != NULL);
        dfDoubleValue = *((double *)pValue);
        if( dfDoubleValue > INT_MAX )
            nIntValue = INT_MAX;
        else if( dfDoubleValue < INT_MIN )
            nIntValue = INT_MIN;
        else
            nIntValue = static_cast<int>(dfDoubleValue);
    }
    else if( chReqType == 'i' )
    {
        CPLAssert(pValue != NULL);
        nIntValue = *((int *)pValue);
        dfDoubleValue = nIntValue;
    }
    else if( chReqType == 'p' )
    {
        CPLError(
            CE_Failure, CPLE_NotSupported,
            "HFAField::SetInstValue() not supported yet for pointer values.");

        return CE_Failure;
    }
    else
    {
        CPLAssert(false);
        return CE_Failure;
    }

    // Handle by type.
    switch( chItemType )
    {
      case 'c':
      case 'C':
        if( nIndexValue + 1 > nDataSize )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Attempt to extend field %s in node past end of data, "
                     "not currently supported.",
                     pszField);
              return CE_Failure;
        }

        if( chReqType == 's' )
        {
            CPLAssert(pValue != NULL);
            pabyData[nIndexValue] = ((char *)pValue)[0];
        }
        else
        {
            pabyData[nIndexValue] = static_cast<char>(nIntValue);
        }
        break;

      case 'e':
      case 's':
      {
          if( chItemType == 'e' && chReqType == 's' )
          {
              CPLAssert(pValue != NULL);
              nIntValue = CSLFindString(papszEnumNames, (char *) pValue);
              if( nIntValue == -1 )
              {
                  CPLError(CE_Failure, CPLE_AppDefined,
                            "Attempt to set enumerated field with unknown"
                            " value `%s'.",
                            (char *)pValue);
                  return CE_Failure;
              }
          }

          if( nIndexValue*2 + 2 > nDataSize )
          {
              CPLError(CE_Failure, CPLE_AppDefined,
                       "Attempt to extend field %s in node past end of data, "
                       "not currently supported.",
                       pszField);
              return CE_Failure;
          }

          // TODO(schwehr): Warn on clamping.
          unsigned short nNumber = static_cast<unsigned short>(nIntValue);
          // TODO(schwehr): What is this 2?
          HFAStandard(2, &nNumber);
          memcpy(pabyData + nIndexValue * 2, &nNumber, 2);
      }
      break;

      case 'S':
      {
          if( nIndexValue * 2 + 2 > nDataSize )
          {
              CPLError(CE_Failure, CPLE_AppDefined,
                       "Attempt to extend field %s in node past end of data, "
                       "not currently supported.",
                       pszField);
              return CE_Failure;
          }

          // TODO(schwehr): Warn on clamping.
          short nNumber = static_cast<short>(nIntValue);
          // TODO(schwehr): What is this 2?
          HFAStandard(2, &nNumber);
          memcpy(pabyData + nIndexValue * 2, &nNumber, 2);
      }
      break;

      case 't':
      case 'l':
      {
          if( nIndexValue * 4 + 4 > nDataSize )
          {
              CPLError(CE_Failure, CPLE_AppDefined,
                       "Attempt to extend field %s in node past end of data, "
                       "not currently supported.",
                       pszField);
              return CE_Failure;
          }

          GUInt32 nNumber = nIntValue;
          // TODO(schwehr): What is this 4?
          HFAStandard(4, &nNumber);
          memcpy(pabyData + nIndexValue * 4, &nNumber, 4);
      }
      break;

      case 'L':
      {
          if( nIndexValue * 4 + 4 > nDataSize )
          {
              CPLError(CE_Failure, CPLE_AppDefined,
                       "Attempt to extend field %s in node past end of data, "
                       "not currently supported.",
                       pszField);
              return CE_Failure;
          }

          GInt32 nNumber = nIntValue;
          HFAStandard(4, &nNumber);
          memcpy(pabyData + nIndexValue * 4, &nNumber, 4);
      }
      break;

      case 'f':
      {
          if( nIndexValue * 4 + 4 > nDataSize )
          {
              CPLError(CE_Failure, CPLE_AppDefined,
                       "Attempt to extend field %s in node past end of data, "
                       "not currently supported.",
                       pszField);
              return CE_Failure;
          }

          // TODO(schwehr): Warn on clamping.
          float fNumber = static_cast<float>(dfDoubleValue);
          // TODO(schwehr): 4 == sizeof(float)?
          HFAStandard(4, &fNumber);
          memcpy(pabyData + nIndexValue * 4, &fNumber, 4);
      }
      break;

      case 'd':
      {
          if( nIndexValue * 8 + 8 > nDataSize )
          {
              CPLError(CE_Failure, CPLE_AppDefined,
                       "Attempt to extend field %s in node past end of data, "
                       "not currently supported.",
                       pszField);
              return CE_Failure;
          }

          double dfNumber = dfDoubleValue;
          HFAStandard(8, &dfNumber);
          memcpy(pabyData + nIndexValue * 8, &dfNumber, 8);
      }
      break;

    case 'b':
    {
        // Extract existing rows, columns, and datatype.
        GInt32 nRows = 1;  // TODO(schwehr): Why init to 1 instead of 0?
        memcpy(&nRows, pabyData, 4);
        HFAStandard(4, &nRows);

        GInt32 nColumns = 1;  // TODO(schwehr): Why init to 1 instead of 0?
        memcpy(&nColumns, pabyData + 4, 4);
        HFAStandard(4, &nColumns);

        GInt16 nBaseItemType = 0;
        memcpy(&nBaseItemType, pabyData + 8, 2);
        HFAStandard(2, &nBaseItemType);

        // Are we using special index values to update the rows, columns
        // or type?

        if( nIndexValue == -3 )
            nBaseItemType = static_cast<GInt16>(nIntValue);
        else if( nIndexValue == -2 )
            nColumns = nIntValue;
        else if( nIndexValue == -1 )
            nRows = nIntValue;

        if( nIndexValue < -3 || nIndexValue >= nRows * nColumns )
            return CE_Failure;

        // Write back the rows, columns and basedatatype.
        HFAStandard(4, &nRows);
        memcpy(pabyData, &nRows, 4);
        HFAStandard(4, &nColumns);
        memcpy(pabyData + 4, &nColumns, 4);
        HFAStandard(2, &nBaseItemType);
        memcpy(pabyData + 8, &nBaseItemType, 2);
        HFAStandard(2, &nBaseItemType);  // Swap back for our use.

        if( nBaseItemType < EPT_MIN || nBaseItemType > EPT_MAX )
            return CE_Failure;
        const EPTType eBaseItemType = static_cast<EPTType>(nBaseItemType);

        // We ignore the 2 byte objecttype value.

        nDataSize -= 12;

        if( nIndexValue >= 0 )
        {
            if( (nIndexValue + 1) * (HFAGetDataTypeBits(eBaseItemType) / 8)
                > nDataSize )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Attempt to extend field %s in node past end of "
                         "data, not currently supported.",
                         pszField);
                return CE_Failure;
            }

            if( eBaseItemType == EPT_f64 )
            {
                double dfNumber = dfDoubleValue;

                HFAStandard(8, &dfNumber);
                memcpy(pabyData + 12 + nIndexValue * 8, &dfNumber, 8);
            }
            else if( eBaseItemType == EPT_u8 )
            {
                // TODO(schwehr): Warn on clamping.
                unsigned char nNumber =
                    static_cast<unsigned char>(dfDoubleValue);
                memcpy(pabyData + 12 + nIndexValue, &nNumber, 1);
            }
            else
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Setting basedata field %s with type %s "
                         "not currently supported.",
                         pszField, HFAGetDataTypeName(eBaseItemType));
                return CE_Failure;
            }
        }
    }
    break;

      case 'o':
        if( poItemObjectType != NULL )
        {
            int nExtraOffset = 0;

            if( poItemObjectType->nBytes > 0 )
            {
                if( nIndexValue != 0 &&
                    poItemObjectType->nBytes > INT_MAX / nIndexValue )
                {
                    return CE_Failure;
                }
                nExtraOffset = poItemObjectType->nBytes * nIndexValue;
            }
            else
            {
                for( int iIndexCounter = 0;
                     iIndexCounter < nIndexValue && nExtraOffset < nDataSize;
                     iIndexCounter++ )
                {
                    const int nInc =
                        poItemObjectType->
                            GetInstBytes(pabyData + nExtraOffset,
                                         nDataSize - nExtraOffset);
                    if( nInc <= 0 || nExtraOffset > INT_MAX - nInc )
                    {
                        CPLError(CE_Failure, CPLE_AppDefined,
                                 "Invalid return value");
                        return CE_Failure;
                    }

                    nExtraOffset += nInc;
                }
            }

            if( nExtraOffset >= nDataSize )
                return CE_Failure;

            if( pszField != NULL && strlen(pszField) > 0 )
            {
                return poItemObjectType->
                    SetInstValue(pszField, pabyData + nExtraOffset,
                                 nDataOffset + nExtraOffset,
                                 nDataSize - nExtraOffset,
                                 chReqType, pValue);
            }
            else
            {
                CPLAssert(false);
                return CE_Failure;
            }
        }
        break;

      default:
        CPLAssert(false);
        return CE_Failure;
        break;
    }

    return CE_None;
}

/************************************************************************/
/*                          ExtractInstValue()                          */
/*                                                                      */
/*      Extract the value of an instance of a field.                    */
/*                                                                      */
/*      pszField should be NULL if this field is not a                  */
/*      substructure.                                                   */
/************************************************************************/

bool
HFAField::ExtractInstValue( const char *pszField, int nIndexValue,
                            GByte *pabyData, GUInt32 nDataOffset, int nDataSize,
                            char chReqType, void *pReqReturn,
                            int *pnRemainingDataSize )

{
    const int nInstItemCount = GetInstCount(pabyData, nDataSize);

    if( pnRemainingDataSize )
        *pnRemainingDataSize = -1;

    // Check the index value is valid.
    // Eventually this will have to account for variable fields.
    if( nIndexValue < 0 || nIndexValue >= nInstItemCount )
    {
        if( chItemType == 'b' && nIndexValue >= -3 && nIndexValue < 0 )
            /* ok - special index values */;
        else
            return false;
    }

    // If this field contains a pointer, then we will adjust the
    // data offset relative to it.
    if( chPointer != '\0' )
    {
        if( nDataSize < 8 )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Buffer too small");
            return false;
        }

        GUInt32 nOffset = 0;
        memcpy(&nOffset, pabyData + 4, 4);
        HFAStandard(4, &nOffset);

#if DEBUG_VERBOSE
        if( nOffset != static_cast<GUInt32>(nDataOffset + 8) )
        {
            // TODO(schwehr): Debug why this is happening.
            CPLError(CE_Warning, CPLE_AppDefined,
                     "ExtractInstValue: "
                     "%s.%s points at %d, not %d as expected",
                     pszFieldName, pszField ? pszField : "",
                     nOffset, nDataOffset + 8);
        }
#endif

        pabyData += 8;
        nDataOffset += 8;
        nDataSize -= 8;
    }

    // Pointers to char or uchar arrays requested as strings are
    // handled as a special case.
    if( (chItemType == 'c' || chItemType == 'C') && chReqType == 's' )
    {
        *((GByte **)pReqReturn) = pabyData;
        if( pnRemainingDataSize )
            *pnRemainingDataSize = nDataSize;
        return pabyData != NULL;
    }

    // Handle by type.
    char *pszStringRet = NULL;
    int nIntRet = 0;
    double dfDoubleRet = 0.0;
    GByte *pabyRawData = NULL;

    switch( chItemType )
    {
      case 'c':
      case 'C':
        if( nIndexValue >= nDataSize )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Buffer too small");
            return false;
        }
        nIntRet = pabyData[nIndexValue];
        dfDoubleRet = nIntRet;
        break;

      case 'e':
      case 's':
      {
          if( nIndexValue * 2 + 2 > nDataSize )
          {
              CPLError(CE_Failure, CPLE_AppDefined, "Buffer too small");
              return false;
          }
          unsigned short nNumber = 0;
          memcpy(&nNumber, pabyData + nIndexValue * 2, 2);
          HFAStandard(2, &nNumber);
          nIntRet = nNumber;
          dfDoubleRet = nIntRet;

          if( chItemType == 'e'
              && nIntRet >= 0 && nIntRet < CSLCount(papszEnumNames) )
          {
              pszStringRet = papszEnumNames[nIntRet];
          }
      }
      break;

      case 'S':
      {
          if( nIndexValue * 2 + 2 > nDataSize )
          {
              CPLError(CE_Failure, CPLE_AppDefined, "Buffer too small");
              return false;
          }
          short nNumber = 0;
          memcpy(&nNumber, pabyData + nIndexValue * 2, 2);
          HFAStandard(2, &nNumber);
          nIntRet = nNumber;
          dfDoubleRet = nIntRet;
      }
      break;

      case 't':
      case 'l':
      {
          if( nIndexValue * 4 + 4 > nDataSize )
          {
              CPLError(CE_Failure, CPLE_AppDefined, "Buffer too small");
              return false;
          }
          GUInt32 nNumber = 0;
          memcpy(&nNumber, pabyData + nIndexValue * 4, 4);
          HFAStandard(4, &nNumber);
          nIntRet = nNumber;
          dfDoubleRet = nIntRet;
      }
      break;

      case 'L':
      {
          if( nIndexValue * 4 + 4 > nDataSize )
          {
              CPLError(CE_Failure, CPLE_AppDefined, "Buffer too small");
              return false;
          }
          GInt32 nNumber = 0;
          // TODO(schwehr): What is 4?
          memcpy(&nNumber, pabyData + nIndexValue * 4, 4);
          HFAStandard(4, &nNumber);
          nIntRet = nNumber;
          dfDoubleRet = nIntRet;
      }
      break;

      case 'f':
      {
          if( nIndexValue * 4 + 4 > nDataSize )
          {
              CPLError(CE_Failure, CPLE_AppDefined, "Buffer too small");
              return false;
          }
          float fNumber = 0.0f;
          // TODO(schwehr): What is 4?
          memcpy(&fNumber, pabyData + nIndexValue * 4, 4);
          HFAStandard(4, &fNumber);
          dfDoubleRet = fNumber;
          if( dfDoubleRet > static_cast<double>(
                                            std::numeric_limits<int>::max()) ||
              dfDoubleRet < static_cast<double>(
                                            std::numeric_limits<int>::min()) ||
              CPLIsNan(fNumber) )
          {
              CPLError(CE_Failure, CPLE_AppDefined,
                       "Too large for int: %f", fNumber);
              return false;
          }
          nIntRet = static_cast<int>(fNumber);
      }
      break;

      case 'd':
      {
          if( nIndexValue * 8 + 8 > nDataSize )
          {
              CPLError(CE_Failure, CPLE_AppDefined, "Buffer too small");
              return false;
          }
          double dfNumber = 0;
          memcpy(&dfNumber, pabyData + nIndexValue * 8, 8);
          HFAStandard(8, &dfNumber);
          dfDoubleRet = dfNumber;
          if( dfNumber > std::numeric_limits<int>::max() ||
              dfNumber < std::numeric_limits<int>::min() ||
              CPLIsNan(dfNumber) )
          {
              CPLError(CE_Failure, CPLE_AppDefined,
                       "Too large for int: %f", dfNumber);
              return false;
          }
          nIntRet = static_cast<int>(dfNumber);
      }
      break;

      case 'b':
      {
          if( nDataSize < 12 )
              return false;

          GInt32 nRows = 0;
          memcpy(&nRows, pabyData, 4);
          HFAStandard(4, &nRows);

          GInt32 nColumns = 0;
          memcpy(&nColumns, pabyData + 4, 4);
          HFAStandard(4, &nColumns);

          GInt16 nBaseItemType = 0;
          memcpy(&nBaseItemType, pabyData + 8, 2);
          HFAStandard(2, &nBaseItemType);
          // We ignore the 2 byte objecttype value.

          if( nIndexValue < -3 ||
              nRows <= 0 ||
              nColumns <= 0 ||
              nRows > INT_MAX / nColumns ||
              nIndexValue >= nRows * nColumns )
              return false;

          pabyData += 12;
          nDataSize -= 12;

          if( nIndexValue == -3 )
          {
              dfDoubleRet = nBaseItemType;
              nIntRet = nBaseItemType;
          }
          else if( nIndexValue == -2 )
          {
              dfDoubleRet = nColumns;
              nIntRet = nColumns;
          }
          else if( nIndexValue == -1 )
          {
              dfDoubleRet = nRows;
              nIntRet = nRows;
          }
          else if( nBaseItemType == EPT_u1 )
          {
              // TODO(schwehr): What are these constants like 8 and 0x7?
              if( nIndexValue * 8 >= nDataSize )
              {
                  CPLError(CE_Failure, CPLE_AppDefined, "Buffer too small");
                  return false;
              }

              if( pabyData[nIndexValue >> 3] & (1 << (nIndexValue & 0x7)) )
              {
                  dfDoubleRet = 1;
                  nIntRet = 1;
              }
              else
              {
                  dfDoubleRet = 0.0;
                  nIntRet = 0;
              }
          }
          else if( nBaseItemType == EPT_u2 )
          {
              const int nBitOffset = nIndexValue & 0x3;
              const int nByteOffset = nIndexValue >> 2;

              if( nByteOffset >= nDataSize )
              {
                  CPLError(CE_Failure, CPLE_AppDefined, "Buffer too small");
                  return false;
              }

              const int nMask = 0x3;
              nIntRet = (pabyData[nByteOffset] >> nBitOffset) & nMask;
              dfDoubleRet = nIntRet;
          }
          else if( nBaseItemType == EPT_u4 )
          {
              const int nBitOffset = nIndexValue & 0x7;
              const int nByteOffset = nIndexValue >> 3;

              if( nByteOffset >= nDataSize )
              {
                  CPLError(CE_Failure, CPLE_AppDefined, "Buffer too small");
                  return false;
              }

              const int nMask = 0x7;
              nIntRet = (pabyData[nByteOffset] >> nBitOffset) & nMask;
              dfDoubleRet = nIntRet;
          }
          else if( nBaseItemType == EPT_u8 )
          {
              if( nIndexValue >= nDataSize )
              {
                  CPLError(CE_Failure, CPLE_AppDefined, "Buffer too small");
                  return false;
              }
              dfDoubleRet = pabyData[nIndexValue];
              nIntRet = pabyData[nIndexValue];
          }
          else if( nBaseItemType == EPT_s8 )
          {
              if( nIndexValue >= nDataSize )
              {
                  CPLError(CE_Failure, CPLE_AppDefined, "Buffer too small");
                  return false;
              }
              dfDoubleRet = ((signed char *)pabyData)[nIndexValue];
              nIntRet = ((signed char *)pabyData)[nIndexValue];
          }
          else if( nBaseItemType == EPT_s16 )
          {
              if( nIndexValue * 2 + 2 > nDataSize )
              {
                  CPLError(CE_Failure, CPLE_AppDefined, "Buffer too small");
                  return false;
              }
              GInt16 nValue = 0;
              memcpy(&nValue, pabyData + 2 * nIndexValue, 2);
              HFAStandard(2, &nValue);

              dfDoubleRet = nValue;
              nIntRet = nValue;
          }
          else if( nBaseItemType == EPT_u16 )
          {
              if( nIndexValue * 2 + 2 > nDataSize )
              {
                  CPLError(CE_Failure, CPLE_AppDefined, "Buffer too small");
                  return false;
              }
              GUInt16 nValue = 0;
              memcpy(&nValue, pabyData + 2 * nIndexValue, 2);
              HFAStandard(2, &nValue);

              dfDoubleRet = nValue;
              nIntRet = nValue;
          }
          else if( nBaseItemType == EPT_s32 )
          {
              if( nIndexValue * 4 + 4 > nDataSize)
              {
                  CPLError(CE_Failure, CPLE_AppDefined, "Buffer too small");
                  return false;
              }
              GInt32 nValue = 0;
              memcpy(&nValue, pabyData + 4 * nIndexValue, 4);
              HFAStandard(4, &nValue);

              dfDoubleRet = nValue;
              nIntRet = nValue;
          }
          else if( nBaseItemType == EPT_u32 )
          {
              if( nIndexValue * 4 + 4 > nDataSize )
              {
                  CPLError(CE_Failure, CPLE_AppDefined, "Buffer too small");
                  return false;
              }
              GUInt32 nValue = 0;
              memcpy(&nValue, pabyData + 4 * nIndexValue, 4);
              HFAStandard(4, &nValue);

              dfDoubleRet = nValue;
              nIntRet = nValue;
          }
          else if( nBaseItemType == EPT_f32 )
          {
              if( nIndexValue * 4 + 4 > nDataSize )
              {
                  CPLError(CE_Failure, CPLE_AppDefined, "Buffer too small");
                  return false;
              }
              float fValue = 0.0f;
              memcpy(&fValue, pabyData + 4 * nIndexValue, 4);
              HFAStandard(4, &fValue);

              dfDoubleRet = fValue;
              nIntRet = static_cast<int>(fValue);
          }
          else if( nBaseItemType == EPT_f64 )
          {
              if( nIndexValue * 8 + 8 > nDataSize )
              {
                  CPLError(CE_Failure, CPLE_AppDefined, "Buffer too small");
                  return false;
              }
              double dfValue = 0.0;
              memcpy(&dfValue, pabyData + 8 * nIndexValue, 8);
              HFAStandard(8, &dfValue);

              dfDoubleRet = dfValue;
              const int nMax = std::numeric_limits<int>::max();
              const int nMin = std::numeric_limits<int>::min();
              if( dfDoubleRet >= nMax )
              {
                  nIntRet = nMax;
              }
              else if( dfDoubleRet <= nMin )
              {
                  nIntRet = nMin;
              }
              else if( CPLIsNan(dfDoubleRet) )
              {
                  CPLError(CE_Warning, CPLE_AppDefined,
                           "NaN converted to INT_MAX.");
                  nIntRet = nMax;
              }
              else
              {
                  nIntRet = static_cast<int>(dfDoubleRet);
              }
          }
          else
          {
              CPLError(CE_Failure, CPLE_AppDefined,
                       "Unknown base item type: %d", nBaseItemType);
              return false;
          }
      }
      break;

      case 'o':
        if( poItemObjectType != NULL )
        {
            int nExtraOffset = 0;

            if( poItemObjectType->nBytes > 0 )
            {
                if( nIndexValue != 0 &&
                    poItemObjectType->nBytes > INT_MAX / nIndexValue )
                    // TODO(schwehr): Why was this CE_Failure when the others
                    // are false?
                    return false;
                nExtraOffset = poItemObjectType->nBytes * nIndexValue;
            }
            else
            {
                for( int iIndexCounter = 0;
                     iIndexCounter < nIndexValue && nExtraOffset < nDataSize;
                     iIndexCounter++ )
                {
                    const int nInc =
                        poItemObjectType->GetInstBytes(
                            pabyData + nExtraOffset,
                            nDataSize - nExtraOffset);
                    if( nInc <= 0 || nExtraOffset > INT_MAX - nInc )
                    {
                        CPLError(CE_Failure, CPLE_AppDefined,
                                 "Invalid return value");
                        // TODO(schwehr): Verify this false is okay.
                        return false;
                    }

                    nExtraOffset += nInc;
                }
            }

            if( nExtraOffset >= nDataSize )
                return false;

            pabyRawData = pabyData + nExtraOffset;

            if( pszField != NULL && strlen(pszField) > 0 )
            {
                return
                    poItemObjectType->
                        ExtractInstValue(pszField, pabyRawData,
                                          nDataOffset + nExtraOffset,
                                          nDataSize - nExtraOffset,
                                          chReqType, pReqReturn,
                                          pnRemainingDataSize);
            }
        }
        else
        {
            // E. Rouault: not completely sure about this, but helps avoid
            // DoS timeouts in cases like
            // https://bugs.chromium.org/p/oss-fuzz/issues/detail?id=1806
            return false;
        }
        break;

      default:
        return false;
        break;
    }

    // Return the appropriate representation.
    if( chReqType == 's' )
    {
        if( pszStringRet == NULL )
        {
            // HFAEntry:: BuildEntryFromMIFObject() expects to have always 8
            // bytes before the data. In normal situations, it should not go
            // here, but that can happen if the file is corrupted so reserve the
            // first 8 bytes before the string to contain null bytes.
            memset(szNumberString, 0, 8);
            CPLsnprintf(szNumberString + 8, sizeof(szNumberString) - 8,
                        "%.14g", dfDoubleRet);
            pszStringRet = szNumberString + 8;
        }

        *((char **)pReqReturn) = pszStringRet;
        return true;
    }
    else if( chReqType == 'd' )
    {
        *((double *)pReqReturn) = dfDoubleRet;
        return true;
    }
    else if( chReqType == 'i' )
    {
        *((int *)pReqReturn) = nIntRet;
        return true;
    }
    else if( chReqType == 'p' )
    {
        *((GByte **)pReqReturn) = pabyRawData;
        return true;
    }
    else
    {
        CPLAssert(false);
        return false;
    }
}

/************************************************************************/
/*                            GetInstBytes()                            */
/*                                                                      */
/*      Get the number of bytes in a particular instance of a           */
/*      field.  This will normally be the fixed internal nBytes         */
/*      value, but for pointer objects will include the variable        */
/*      portion.                                                        */
/************************************************************************/

int HFAField::GetInstBytes( GByte *pabyData, int nDataSize )

{
    if( nBytes > -1 )
        return nBytes;

    int nCount = 1;
    int nInstBytes = 0;

    if( chPointer != '\0' )
    {
        if( nDataSize < 4 )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Buffer too small");
            return -1;
        }

        memcpy(&nCount, pabyData, 4);
        HFAStandard(4, &nCount);

        pabyData += 8;
        nInstBytes += 8;
    }

    if( chItemType == 'b' && nCount != 0 )  // BASEDATA
    {
        if( nDataSize - nInstBytes < 4 + 4 + 2 )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Buffer too small");
            return -1;
        }

        GInt32 nRows = 0;
        memcpy(&nRows, pabyData, 4);
        HFAStandard(4, &nRows);
        GInt32 nColumns = 0;
        memcpy(&nColumns, pabyData + 4, 4);
        HFAStandard(4, &nColumns);
        GInt16 nBaseItemType = 0;
        memcpy(&nBaseItemType, pabyData + 8, 2);
        HFAStandard(2, &nBaseItemType);
        if( nBaseItemType < EPT_MIN || nBaseItemType > EPT_MAX )
            return -1;

        EPTType eBaseItemType = static_cast<EPTType>(nBaseItemType);

        nInstBytes += 12;

        if( nRows < 0 || nColumns < 0 )
            return -1;
        if( nColumns != 0 && nRows > INT_MAX / nColumns )
            return -1;
        if( nRows != 0 &&
            ((HFAGetDataTypeBits(eBaseItemType) + 7) / 8) > INT_MAX / nRows )
            return -1;
        if( nColumns != 0 &&
            ((HFAGetDataTypeBits(eBaseItemType) + 7) / 8) * nRows >
            INT_MAX / nColumns )
            return -1;
        if( ((HFAGetDataTypeBits(eBaseItemType) + 7) / 8) * nRows * nColumns >
            INT_MAX - nInstBytes )
            return -1;

        nInstBytes +=
            ((HFAGetDataTypeBits(eBaseItemType) + 7) / 8) * nRows * nColumns;
    }
    else if( poItemObjectType == NULL )
    {
        if( nCount != 0 &&
            HFADictionary::GetItemSize(chItemType) > INT_MAX / nCount )
            return -1;
        if( nCount * HFADictionary::GetItemSize(chItemType) >
            INT_MAX - nInstBytes )
            return -1;
        nInstBytes += nCount * HFADictionary::GetItemSize(chItemType);
    }
    else
    {
        for( int i = 0;
             i < nCount && nInstBytes < nDataSize && nInstBytes >= 0;
             i++ )
        {
            const int nThisBytes =
                poItemObjectType->GetInstBytes(pabyData,
                                                nDataSize - nInstBytes);
            if( nThisBytes <= 0 || nInstBytes > INT_MAX - nThisBytes )
            {
                CPLError(CE_Failure, CPLE_AppDefined, "Invalid return value");
                return -1;
            }

            nInstBytes += nThisBytes;
            pabyData += nThisBytes;
        }
    }

    return nInstBytes;
}

/************************************************************************/
/*                            GetInstCount()                            */
/*                                                                      */
/*      Get the count for a particular instance of a field.  This       */
/*      will normally be the built in value, but for variable fields    */
/*      this is extracted from the data itself.                         */
/************************************************************************/

int HFAField::GetInstCount( GByte * pabyData, int nDataSize )

{
    if( chPointer == '\0' )
        return nItemCount;

    if( chItemType == 'b' )
    {
        if( nDataSize < 20 )
            return 0;

        GInt32 nRows = 0;
        memcpy(&nRows, pabyData + 8, 4);
        HFAStandard(4, &nRows);
        GInt32 nColumns = 0;
        memcpy(&nColumns, pabyData + 12, 4);
        HFAStandard(4, &nColumns);

        if( nRows < 0 || nColumns < 0 )
            return 0;
        if( nColumns != 0 && nRows > INT_MAX / nColumns )
            return 0;

        return nRows * nColumns;
    }

    if( nDataSize < 4 )
        return 0;

    GInt32 nCount = 0;
    memcpy(&nCount, pabyData, 4);
    HFAStandard(4, &nCount);
    return nCount;
}

/************************************************************************/
/*                           DumpInstValue()                            */
/************************************************************************/

void HFAField::DumpInstValue( FILE *fpOut,
                              GByte *pabyData,
                              GUInt32 nDataOffset, int nDataSize,
                              const char *pszPrefix )

{
    const int nEntries = GetInstCount(pabyData, nDataSize);

    // Special case for arrays of chars or uchars which are printed
    // as a string.
    if( (chItemType == 'c' || chItemType == 'C') && nEntries > 0 )
    {
        void *pReturn = NULL;
        if( ExtractInstValue(NULL, 0,
                             pabyData, nDataOffset, nDataSize,
                             's', &pReturn) )
            CPL_IGNORE_RET_VAL(
                VSIFPrintf(fpOut, "%s%s = `%s'\n",
                           pszPrefix, pszFieldName,
                           static_cast<char *>(pReturn)));
        else
            CPL_IGNORE_RET_VAL(
                VSIFPrintf(fpOut, "%s%s = (access failed)\n",
                           pszPrefix, pszFieldName));

        return;
    }

    // For BASEDATA objects, we want to first dump their dimension and type.
    if( chItemType == 'b' )
    {
        int nDataType = 0;
        const bool bSuccess =
            ExtractInstValue(NULL, -3, pabyData, nDataOffset,
                             nDataSize, 'i', &nDataType);
        if( bSuccess )
        {
            int nColumns = 0;
            ExtractInstValue(NULL, -2, pabyData, nDataOffset,
                             nDataSize, 'i', &nColumns);
            int nRows = 0;
            ExtractInstValue(NULL, -1, pabyData, nDataOffset,
                             nDataSize, 'i', &nRows);
            CPL_IGNORE_RET_VAL(VSIFPrintf(
                fpOut, "%sBASEDATA(%s): %dx%d of %s\n",
                pszPrefix, pszFieldName,
                nColumns, nRows,
                (nDataType >= EPT_MIN && nDataType <= EPT_MAX)
                ? HFAGetDataTypeName(static_cast<EPTType>(nDataType))
                : "invalid type"));
        }
        else
        {
            CPL_IGNORE_RET_VAL(VSIFPrintf(fpOut, "%sBASEDATA(%s): empty\n",
                                          pszPrefix, pszFieldName));
        }
    }

    // Dump each entry in the field array.
    void *pReturn = NULL;

    const int nMaxEntry = std::min(MAX_ENTRY_REPORT, nEntries);
    for( int iEntry = 0; iEntry < nMaxEntry; iEntry++ )
    {
        if( nEntries == 1 )
            CPL_IGNORE_RET_VAL(
                VSIFPrintf(fpOut, "%s%s = ", pszPrefix, pszFieldName));
        else
            CPL_IGNORE_RET_VAL(
                VSIFPrintf(fpOut, "%s%s[%d] = ",
                           pszPrefix, pszFieldName, iEntry));

        switch( chItemType )
        {
          case 'f':
          case 'd':
          {
              double dfValue = 0.0;
              if( ExtractInstValue(NULL, iEntry,
                                    pabyData, nDataOffset, nDataSize,
                                    'd', &dfValue) )
                  CPL_IGNORE_RET_VAL(VSIFPrintf(fpOut, "%f\n", dfValue));
              else
                  CPL_IGNORE_RET_VAL(VSIFPrintf(fpOut, "(access failed)\n"));
          }
          break;

          case 'b':
          {
              double dfValue = 0.0;

              if( ExtractInstValue(NULL, iEntry,
                                    pabyData, nDataOffset, nDataSize,
                                    'd', &dfValue) )
                  CPL_IGNORE_RET_VAL(
                      VSIFPrintf(fpOut, "%s%.15g\n", pszPrefix, dfValue));
              else
                  CPL_IGNORE_RET_VAL(
                      VSIFPrintf(fpOut, "%s(access failed)\n", pszPrefix));
          }
          break;

          case 'e':
            if( ExtractInstValue(NULL, iEntry,
                                  pabyData, nDataOffset, nDataSize,
                                  's', &pReturn) )
                CPL_IGNORE_RET_VAL(VSIFPrintf(fpOut, "%s\n",
                                               (char *) pReturn));
            else
                CPL_IGNORE_RET_VAL(VSIFPrintf(fpOut, "(access failed)\n"));
            break;

          case 'o':
            if( !ExtractInstValue(NULL, iEntry,
                                   pabyData, nDataOffset, nDataSize,
                                   'p', &pReturn) )
            {
                CPL_IGNORE_RET_VAL(VSIFPrintf(fpOut, "(access failed)\n"));
            }
            else
            {
                CPL_IGNORE_RET_VAL(VSIFPrintf(fpOut, "\n"));

                const int nByteOffset =
                    static_cast<int>(((GByte *) pReturn) - pabyData);

                char szLongFieldName[256] = {};
                snprintf(szLongFieldName, sizeof(szLongFieldName),
                          "%s    ", pszPrefix);

                if( poItemObjectType )
                    poItemObjectType->DumpInstValue(fpOut,
                                                    pabyData + nByteOffset,
                                                    nDataOffset + nByteOffset,
                                                    nDataSize - nByteOffset,
                                                    szLongFieldName);
            }
            break;

          default:
          {
              GInt32 nIntValue = 0;

              if( ExtractInstValue(NULL, iEntry,
                                   pabyData, nDataOffset, nDataSize,
                                   'i', &nIntValue) )
                  CPL_IGNORE_RET_VAL(VSIFPrintf(fpOut, "%d\n", nIntValue));
              else
                  CPL_IGNORE_RET_VAL(VSIFPrintf(fpOut, "(access failed)\n"));
          }
          break;
        }
    }

    if( nEntries > MAX_ENTRY_REPORT )
        CPL_IGNORE_RET_VAL(VSIFPrintf(
            fpOut, "%s ... remaining instances omitted ...\n", pszPrefix));

    if( nEntries == 0 )
        CPL_IGNORE_RET_VAL(
            VSIFPrintf(fpOut, "%s%s = (no values)\n", pszPrefix, pszFieldName));
}
