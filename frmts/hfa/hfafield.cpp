/******************************************************************************
 * $Id$
 *
 * Project:  Erdas Imagine (.img) Translator
 * Purpose:  Implementation of the HFAField class for managing information
 *           about one field in a HFA dictionary type.  Managed by HFAType.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
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
 ****************************************************************************/

#include "hfa_p.h"

CPL_CVSID("$Id$");

#define MAX_ENTRY_REPORT   16
                           
/************************************************************************/
/* ==================================================================== */
/*		                HFAField				*/
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                              HFAField()                              */
/************************************************************************/

HFAField::HFAField()

{
    nBytes = 0;

    nItemCount = 0;
    chPointer = '\0';
    chItemType = '\0';

    pszItemObjectType = NULL;
    poItemObjectType = NULL;

    papszEnumNames = NULL;

    pszFieldName = NULL;
}

/************************************************************************/
/*                             ~HFAField()                              */
/************************************************************************/

HFAField::~HFAField()

{
    CPLFree( pszItemObjectType );
    CSLDestroy( papszEnumNames );
    CPLFree( pszFieldName );
}

/************************************************************************/
/*                             Initialize()                             */
/************************************************************************/

const char *HFAField::Initialize( const char * pszInput )

{
    int		i;
    
/* -------------------------------------------------------------------- */
/*      Read the number.                                                */
/* -------------------------------------------------------------------- */
    nItemCount = atoi(pszInput);

    while( *pszInput != '\0' && *pszInput != ':' )
        pszInput++;

    if( *pszInput == '\0' )
        return NULL;
    
    pszInput++;

/* -------------------------------------------------------------------- */
/*      Is this a pointer?                                              */
/* -------------------------------------------------------------------- */
    if( *pszInput == 'p' || *pszInput == '*' )
        chPointer = *(pszInput++);

/* -------------------------------------------------------------------- */
/*      Get the general type                                            */
/* -------------------------------------------------------------------- */
    if( *pszInput == '\0' )
        return NULL;

    chItemType = *(pszInput++);

    if ( strchr( "124cCesStlLfdmMbox", chItemType) == NULL )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Unrecognized item type : %c", chItemType);
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      If this is an object, we extract the type of the object.        */
/* -------------------------------------------------------------------- */
    if( chItemType == 'o' )
    {
        for( i = 0; pszInput[i] != '\0' && pszInput[i] != ','; i++ ) {}
        if (pszInput[i] == '\0')
            return NULL;

        pszItemObjectType = (char *) CPLMalloc(i+1);
        strncpy( pszItemObjectType, pszInput, i );
        pszItemObjectType[i] = '\0';

        pszInput += i+1;
    }

/* -------------------------------------------------------------------- */
/*      If this is an inline object, we need to skip past the           */
/*      definition, and then extract the object class name.             */
/*                                                                      */
/*      We ignore the actual definition, so if the object type isn't    */
/*      already defined, things will not work properly.  See the        */
/*      file lceugr250_00_pct.aux for an example of inline defs.        */
/* -------------------------------------------------------------------- */
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
        if (*pszInput == '\0')
            return NULL;

        chItemType = 'o';

        // find the comma terminating the type name.
        for( i = 0; pszInput[i] != '\0' && pszInput[i] != ','; i++ ) {}
        if (pszInput[i] == '\0')
            return NULL;

        pszItemObjectType = (char *) CPLMalloc(i+1);
        strncpy( pszItemObjectType, pszInput, i );
        pszItemObjectType[i] = '\0';

        pszInput += i+1;
    }

/* -------------------------------------------------------------------- */
/*      If this is an enumeration we have to extract all the            */
/*      enumeration values.                                             */
/* -------------------------------------------------------------------- */
    if( chItemType == 'e' )
    {
        int	nEnumCount = atoi(pszInput);
        int	iEnum;

        if (nEnumCount < 0 || nEnumCount > 100000)
            return NULL;

        pszInput = strchr(pszInput,':');
        if( pszInput == NULL )
            return NULL;

        pszInput++;

        papszEnumNames = (char **) VSICalloc(sizeof(char *), nEnumCount+1);
        if (papszEnumNames == NULL)
            return NULL;
        
        for( iEnum = 0; iEnum < nEnumCount; iEnum++ )
        {
            char	*pszToken;
            
            for( i = 0; pszInput[i] != '\0' && pszInput[i] != ','; i++ ) {}

            if( pszInput[i] != ',' )
                return NULL;

            pszToken = (char *) CPLMalloc(i+1);
            strncpy( pszToken, pszInput, i );
            pszToken[i] = '\0';

            papszEnumNames[iEnum] = pszToken;

            pszInput += i+1;
        }
    }

/* -------------------------------------------------------------------- */
/*      Extract the field name.                                         */
/* -------------------------------------------------------------------- */
    for( i = 0; pszInput[i] != '\0' && pszInput[i] != ','; i++ ) {}
    if (pszInput[i] == '\0')
        return NULL;

    pszFieldName = (char *) CPLMalloc(i+1);
    strncpy( pszFieldName, pszInput, i );
    pszFieldName[i] = '\0';

    pszInput += i+1;
    
    return( pszInput );
}

/************************************************************************/
/*                            CompleteDefn()                            */
/*                                                                      */
/*      Establish size, and pointers to component types.                */
/************************************************************************/

void HFAField::CompleteDefn( HFADictionary * poDict )

{
/* -------------------------------------------------------------------- */
/*      Get a reference to the type object if we have a type name       */
/*      for this field (not a built in).                                */
/* -------------------------------------------------------------------- */
    if( pszItemObjectType != NULL )
        poItemObjectType = poDict->FindType( pszItemObjectType );

/* -------------------------------------------------------------------- */
/*      Figure out the size.                                            */
/* -------------------------------------------------------------------- */
    if( chPointer == 'p' )
    {
        nBytes = -1; /* we can't know the instance size */
    }
    else if( poItemObjectType != NULL )
    {
        poItemObjectType->CompleteDefn( poDict );
        if( poItemObjectType->nBytes == -1 )
            nBytes = -1;
        else
            nBytes = poItemObjectType->nBytes * nItemCount;

        if( chPointer == '*' && nBytes != -1 )
            nBytes += 8; /* count, and offset */
    }
    else
    {
        nBytes = poDict->GetItemSize( chItemType ) * nItemCount;
    }
}

/************************************************************************/
/*                                Dump()                                */
/************************************************************************/

void HFAField::Dump( FILE * fp )

{
    const char	*pszTypeName;
    
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
        CPLAssert( FALSE );
        pszTypeName = "Unknown";
    }
    
    VSIFPrintf( fp, "    %-19s %c %s[%d];\n",
                pszTypeName,
                chPointer ? chPointer : ' ',
                pszFieldName, nItemCount );

    if( papszEnumNames != NULL )
    {
        int	i;
        
        for( i = 0; papszEnumNames[i] != NULL; i++ )
        {
            VSIFPrintf( fp, "        %s=%d\n",
                        papszEnumNames[i], i );
        }
    }
}

/************************************************************************/
/*                            SetInstValue()                            */
/************************************************************************/

CPLErr
HFAField::SetInstValue( const char * pszField, int nIndexValue,
                        GByte *pabyData, GUInt32 nDataOffset, int nDataSize,
                        char chReqType, void *pValue )

{
/* -------------------------------------------------------------------- */
/*	If this field contains a pointer, then we will adjust the	*/
/*	data offset relative to it.    					*/
/* -------------------------------------------------------------------- */
    if( chPointer != '\0' )
    {
        GUInt32		nCount;
        GUInt32		nOffset;

         /* set the count for fixed sized arrays */
        if( nBytes > -1 )
            nCount = nItemCount;

        // The count returned for BASEDATA's are the contents, 
        // but here we really want to mark it as one BASEDATA instance
        // (see #2144)
        if( chItemType == 'b' ) 
            nCount = 1;

        /* Set the size from string length */
        else if( chReqType == 's' && (chItemType == 'c' || chItemType == 'C'))
        {
            if( pValue == NULL )
                nCount = 0;
            else
                nCount = strlen((char *) pValue) + 1;
        }

        /* set size based on index ... assumes in-order setting of array */
        else
            nCount = nIndexValue+1;

        if( (int) nCount + 8 > nDataSize )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Attempt to extend field %s in node past end of data,\n"
                      "not currently supported.",
                      pszField );
            return CE_Failure;
        }

        // we will update the object count iff we are writing beyond the end
        memcpy( &nOffset, pabyData, 4 );
        HFAStandard( 4, &nOffset );
        if( nOffset < nCount )
        {
            nOffset = nCount;
            HFAStandard( 4, &nOffset );
            memcpy( pabyData, &nOffset, 4 );
        }

        if( pValue == NULL )
            nOffset = 0;
        else
            nOffset = nDataOffset + 8;
        HFAStandard( 4, &nOffset );
        memcpy( pabyData+4, &nOffset, 4 );

        pabyData += 8;

        nDataOffset += 8;
        nDataSize -= 8;
    }

/* -------------------------------------------------------------------- */
/*      pointers to char or uchar arrays requested as strings are       */
/*      handled as a special case.                                      */
/* -------------------------------------------------------------------- */
    if( (chItemType == 'c' || chItemType == 'C') && chReqType == 's' )
    {
        int	nBytesToCopy;
        
        if( nBytes == -1 )
        {
            if( pValue == NULL )
                nBytesToCopy = 0;
            else
                nBytesToCopy = strlen((char *) pValue) + 1;
        }
        else
            nBytesToCopy = nBytes;

        if( nBytesToCopy > nDataSize )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Attempt to extend field %s in node past end of data,\n"
                      "not currently supported.",
                      pszField );
            return CE_Failure;
        }

        memset( pabyData, 0, nBytesToCopy );

        if( pValue != NULL )
            strncpy( (char *) pabyData, (char *) pValue, nBytesToCopy );

        return CE_None;
    }

/* -------------------------------------------------------------------- */
/*      Translate the passed type into different representations.       */
/* -------------------------------------------------------------------- */
    int		nIntValue;
    double      dfDoubleValue;

    if( chReqType == 's' )
    {
        nIntValue = atoi((char *) pValue);
        dfDoubleValue = atof((char *) pValue);
    }
    else if( chReqType == 'd' )
    {
        dfDoubleValue = *((double *) pValue);
        nIntValue = (int) dfDoubleValue;
    }
    else if( chReqType == 'i' )
    {
        dfDoubleValue = *((int *) pValue);
        nIntValue = *((int *) pValue);
    }
    else if( chReqType == 'p' )
    {
        CPLError( CE_Failure, CPLE_NotSupported, 
            "HFAField::SetInstValue() not supported yet for pointer values." );
        
        return CE_Failure;
    }
    else
    {
        CPLAssert( FALSE );
        return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      Handle by type.                                                 */
/* -------------------------------------------------------------------- */
    switch( chItemType )
    {
      case 'c':
      case 'C':
        if( nIndexValue + 1 > nDataSize )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Attempt to extend field %s in node past end of data,\n"
                      "not currently supported.",
                      pszField );
              return CE_Failure;
        }

        if( chReqType == 's' )
            pabyData[nIndexValue] = ((char *) pValue)[0];
        else
            pabyData[nIndexValue] = (char) nIntValue;
        break;

      case 'e':
      case 's':
      {
          if( chItemType == 'e' && chReqType == 's' )
          {
              nIntValue = CSLFindString( papszEnumNames, (char *) pValue );
              if( nIntValue == -1 )
              {
                  CPLError( CE_Failure, CPLE_AppDefined, 
                            "Attempt to set enumerated field with unknown"
                            " value `%s'.", 
                            (char *) pValue );
                  return CE_Failure;
              }
          }

          unsigned short nNumber = (unsigned short) nIntValue;

          if( nIndexValue*2 + 2 > nDataSize )
          {
              CPLError( CE_Failure, CPLE_AppDefined,
                        "Attempt to extend field %s in node past end of data,\n"
                        "not currently supported.",
                        pszField );
              return CE_Failure;
          }

          HFAStandard( 2, &nNumber );
          memcpy( pabyData + nIndexValue*2, &nNumber, 2 );
      }
      break;

      case 'S':
      {
          short nNumber;

          if( nIndexValue*2 + 2 > nDataSize )
          {
              CPLError( CE_Failure, CPLE_AppDefined,
                        "Attempt to extend field %s in node past end of data,\n"
                        "not currently supported.",
                        pszField );
              return CE_Failure;
          }

          nNumber = (short) nIntValue;
          HFAStandard( 2, &nNumber );
          memcpy( pabyData + nIndexValue*2, &nNumber, 2 );
      }
      break;
        
      case 't':
      case 'l':
      {
          GUInt32	nNumber = nIntValue;

          if( nIndexValue*4 + 4 > nDataSize )
          {
              CPLError( CE_Failure, CPLE_AppDefined,
                        "Attempt to extend field %s in node past end of data,\n"
                        "not currently supported.",
                        pszField );
              return CE_Failure;
          }

          HFAStandard( 4, &nNumber );
          memcpy( pabyData + nIndexValue*4, &nNumber, 4 );
      }
      break;
      
      case 'L':
      {
          GInt32	nNumber = nIntValue;
          
          if( nIndexValue*4 + 4 > nDataSize )
          {
              CPLError( CE_Failure, CPLE_AppDefined,
                        "Attempt to extend field %s in node past end of data,\n"
                        "not currently supported.",
                        pszField );
              return CE_Failure;
          }

          HFAStandard( 4, &nNumber );
          memcpy( pabyData + nIndexValue*4, &nNumber, 4 );
      }
      break;
      
      case 'f':
      {
          float		fNumber = (float) dfDoubleValue;
          
          if( nIndexValue*4 + 4 > nDataSize )
          {
              CPLError( CE_Failure, CPLE_AppDefined,
                        "Attempt to extend field %s in node past end of data,\n"
                        "not currently supported.",
                        pszField );
              return CE_Failure;
          }

          HFAStandard( 4, &fNumber );
          memcpy( pabyData + nIndexValue*4, &fNumber, 4 );
      }
      break;
        
      case 'd':
      {
          double	dfNumber = dfDoubleValue;
          
          if( nIndexValue*8 + 8 > nDataSize )
          {
              CPLError( CE_Failure, CPLE_AppDefined,
                        "Attempt to extend field %s in node past end of data,\n"
                        "not currently supported.",
                        pszField );
              return CE_Failure;
          }

          HFAStandard( 8, &dfNumber );
          memcpy( pabyData + nIndexValue*8, &dfNumber, 8 );
      }
      break;

    case 'b': 
    { 
        GInt32 nRows = 1; 
        GInt32 nColumns = 1; 
        GInt16 nBaseItemType;

        // Extract existing rows, columns, and datatype.
        memcpy( &nRows, pabyData, 4 );
        HFAStandard( 4, &nRows );
        memcpy( &nColumns, pabyData+4, 4 );
        HFAStandard( 4, &nColumns );
        memcpy( &nBaseItemType, pabyData+8, 2 );
        HFAStandard( 2, &nBaseItemType );

        // Are we using special index values to update the rows, columnrs
        // or type?
        
        if( nIndexValue == -3 )
            nBaseItemType = (GInt16) nIntValue;
        else if( nIndexValue == -2 )
            nColumns = nIntValue;
        else if( nIndexValue == -1 )
            nRows = nIntValue;

        if( nIndexValue < -3 || nIndexValue >= nRows * nColumns ) 
            return CE_Failure; 

        // Write back the rows, columns and basedatatype.
        HFAStandard( 4, &nRows ); 
        memcpy( pabyData, &nRows, 4 ); 
        HFAStandard( 4, &nColumns ); 
        memcpy( pabyData+4, &nColumns, 4 ); 
        HFAStandard( 2, &nBaseItemType ); 
        memcpy ( pabyData + 8, &nBaseItemType, 2 ); 
        HFAStandard( 2, &nBaseItemType ); // swap back for our use.

        // We ignore the 2 byte objecttype value.  

        nDataSize -= 12; 

        if( nIndexValue >= 0 )
        { 
            if( (nIndexValue+1) * (HFAGetDataTypeBits(nBaseItemType)/8)
                > nDataSize ) 
            { 
                CPLError( CE_Failure, CPLE_AppDefined, 
                          "Attempt to extend field %s in node past end of data,\n" 
                          "not currently supported.", 
                          pszField ); 
                return CE_Failure; 
            } 

            if( nBaseItemType == EPT_f64 )
            {
                double dfNumber = dfDoubleValue; 

                HFAStandard( 8, &dfNumber ); 
                memcpy( pabyData + 12 + nIndexValue * 8, &dfNumber, 8 ); 
            }
            else
            {
                CPLError( CE_Failure, CPLE_AppDefined, 
                          "Setting basedata field %s with type %s not currently supported.", 
                          pszField, HFAGetDataTypeName( nBaseItemType ) ); 
                return CE_Failure; 
            }
        } 
    } 
    break;               

      case 'o':
        if( poItemObjectType != NULL )
        {
            int		nExtraOffset = 0;
            int		iIndexCounter;

            if( poItemObjectType->nBytes > 0 )
            {
                if (nIndexValue != 0 && poItemObjectType->nBytes > INT_MAX / nIndexValue)
                    return CE_Failure;
                nExtraOffset = poItemObjectType->nBytes * nIndexValue;
            }
            else
            {
                for( iIndexCounter = 0;
                     iIndexCounter < nIndexValue && nExtraOffset < nDataSize;
                     iIndexCounter++ )
                {
                    int nInc = poItemObjectType->GetInstBytes(pabyData + nExtraOffset,
                                                              nDataSize - nExtraOffset);
                    if (nInc < 0 || nExtraOffset > INT_MAX - nInc)
                    {
                        CPLError(CE_Failure, CPLE_AppDefined, "Invalid return value");
                        return CE_Failure;
                    }

                    nExtraOffset += nInc;
                }
            }

            if (nExtraOffset >= nDataSize)
                return CE_Failure;

            if( pszField != NULL && strlen(pszField) > 0 )
            {
                return( poItemObjectType->
                            SetInstValue( pszField, pabyData + nExtraOffset,
                                          nDataOffset + nExtraOffset,
                                          nDataSize - nExtraOffset,
                                          chReqType, pValue ) );
            }
            else
            {
                CPLAssert( FALSE );
                return CE_Failure;
            }
        }
        break;

      default:
        CPLAssert( FALSE );
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

int
HFAField::ExtractInstValue( const char * pszField, int nIndexValue,
                           GByte *pabyData, GUInt32 nDataOffset, int nDataSize,
                           char chReqType, void *pReqReturn, int *pnRemainingDataSize )

{
    char		*pszStringRet = NULL;
    int			nIntRet = 0;
    double		dfDoubleRet = 0.0;
    int			nInstItemCount = GetInstCount( pabyData, nDataSize );
    GByte		*pabyRawData = NULL;

    if (pnRemainingDataSize)
        *pnRemainingDataSize = -1;

/* -------------------------------------------------------------------- */
/*      Check the index value is valid.                                 */
/*                                                                      */
/*      Eventually this will have to account for variable fields.       */
/* -------------------------------------------------------------------- */
    if( nIndexValue < 0 || nIndexValue >= nInstItemCount )
    {
        if( chItemType == 'b' && nIndexValue >= -3 && nIndexValue < 0 )
            /* ok - special index values */;
        else
            return FALSE;
    }

/* -------------------------------------------------------------------- */
/*	If this field contains a pointer, then we will adjust the	*/
/*	data offset relative to it.    					*/
/* -------------------------------------------------------------------- */
    if( chPointer != '\0' )
    {
        GUInt32		nOffset;

        if (nDataSize < 8)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Buffer too small");
            return FALSE;
        }

        memcpy( &nOffset, pabyData+4, 4 );
        HFAStandard( 4, &nOffset );

        if( nOffset != (GUInt32) (nDataOffset + 8) )
        {
#ifdef notdef            
            CPLError( CE_Warning, CPLE_AppDefined,
                      "%s.%s points at %d, not %d as expected\n",
                      pszFieldName, pszField ? pszField : "",
                      nOffset, nDataOffset+8 );
#endif            
        }
        
        pabyData += 8;

        nDataOffset += 8;
        nDataSize -= 8;
    }

/* -------------------------------------------------------------------- */
/*      pointers to char or uchar arrays requested as strings are       */
/*      handled as a special case.                                      */
/* -------------------------------------------------------------------- */
    if( (chItemType == 'c' || chItemType == 'C') && chReqType == 's' )
    {
        *((GByte **)pReqReturn) = pabyData;
        if (pnRemainingDataSize)
            *pnRemainingDataSize = nDataSize;
        return( pabyData != NULL );
    }

/* -------------------------------------------------------------------- */
/*      Handle by type.                                                 */
/* -------------------------------------------------------------------- */
    switch( chItemType )
    {
      case 'c':
      case 'C':
        if (nIndexValue >= nDataSize)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Buffer too small");
            return FALSE;
        }
        nIntRet = pabyData[nIndexValue];
        dfDoubleRet = nIntRet;
        break;

      case 'e':
      case 's':
      {
          unsigned short nNumber;
          if (nIndexValue*2 + 2 > nDataSize)
          {
              CPLError(CE_Failure, CPLE_AppDefined, "Buffer too small");
              return FALSE;
          }
          memcpy( &nNumber, pabyData + nIndexValue*2, 2 );
          HFAStandard( 2, &nNumber );
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
          short nNumber;
          if (nIndexValue*2 + 2 > nDataSize)
          {
              CPLError(CE_Failure, CPLE_AppDefined, "Buffer too small");
              return FALSE;
          }
          memcpy( &nNumber, pabyData + nIndexValue*2, 2 );
          HFAStandard( 2, &nNumber );
          nIntRet = nNumber;
          dfDoubleRet = nIntRet;
      }
      break;
        
      case 't':
      case 'l':
      {
          GUInt32	nNumber;
          if (nIndexValue*4 + 4 > nDataSize)
          {
              CPLError(CE_Failure, CPLE_AppDefined, "Buffer too small");
              return FALSE;
          }
          memcpy( &nNumber, pabyData + nIndexValue*4, 4 );
          HFAStandard( 4, &nNumber );
          nIntRet = nNumber;
          dfDoubleRet = nIntRet;
      }
      break;
      
      case 'L':
      {
          GInt32	nNumber;
          if (nIndexValue*4 + 4 > nDataSize)
          {
              CPLError(CE_Failure, CPLE_AppDefined, "Buffer too small");
              return FALSE;
          }
          memcpy( &nNumber, pabyData + nIndexValue*4, 4 );
          HFAStandard( 4, &nNumber );
          nIntRet = nNumber;
          dfDoubleRet = nIntRet;
      }
      break;
      
      case 'f':
      {
          float		fNumber;
          if (nIndexValue*4 + 4 > nDataSize)
          {
              CPLError(CE_Failure, CPLE_AppDefined, "Buffer too small");
              return FALSE;
          }
          memcpy( &fNumber, pabyData + nIndexValue*4, 4 );
          HFAStandard( 4, &fNumber );
          dfDoubleRet = fNumber;
          nIntRet = (int) fNumber;
      }
      break;
        
      case 'd':
      {
          double	dfNumber;
          if (nIndexValue*8 + 8 > nDataSize)
          {
              CPLError(CE_Failure, CPLE_AppDefined, "Buffer too small");
              return FALSE;
          }
          memcpy( &dfNumber, pabyData + nIndexValue*8, 8 );
          HFAStandard( 8, &dfNumber );
          dfDoubleRet = dfNumber;
          nIntRet = (int) dfNumber;
      }
      break;

      case 'b':
      {
          GInt32 nRows, nColumns;
          GInt16 nBaseItemType;

          if( nDataSize < 12 )
              return FALSE;
        
          memcpy( &nRows, pabyData, 4 );
          HFAStandard( 4, &nRows );
          memcpy( &nColumns, pabyData+4, 4 );
          HFAStandard( 4, &nColumns );
          memcpy( &nBaseItemType, pabyData+8, 2 );
          HFAStandard( 2, &nBaseItemType );
          // We ignore the 2 byte objecttype value. 

          if( nIndexValue < -3 || nIndexValue >= nRows * nColumns )
              return FALSE;

          pabyData += 12;
          nDataSize -= 12;

          if( nIndexValue == -3 ) 
          {
              dfDoubleRet = nIntRet = nBaseItemType;
          }
          else if( nIndexValue == -2 )
          {
              dfDoubleRet = nIntRet = nColumns;
          }
          else if( nIndexValue == -1 )
          {
              dfDoubleRet = nIntRet = nRows;
          }
          else if( nBaseItemType == EPT_u1 )
          {
              if (nIndexValue*8 >= nDataSize)
              {
                  CPLError(CE_Failure, CPLE_AppDefined, "Buffer too small");
                  return FALSE;
              }

              if( pabyData[nIndexValue>>3] & (1 << (nIndexValue & 0x7)) )
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
              int nBitOffset = nIndexValue & 0x3;
              int nByteOffset = nIndexValue >> 2;
              int nMask = 0x3;

              if (nByteOffset >= nDataSize)
              {
                  CPLError(CE_Failure, CPLE_AppDefined, "Buffer too small");
                  return FALSE;
              }

              nIntRet = (pabyData[nByteOffset] >> nBitOffset) & nMask;
              dfDoubleRet = nIntRet;
          }
          else if( nBaseItemType == EPT_u4 )
          {
              int nBitOffset = nIndexValue & 0x7;
              int nByteOffset = nIndexValue >> 3;
              int nMask = 0x7;

              if (nByteOffset >= nDataSize)
              {
                  CPLError(CE_Failure, CPLE_AppDefined, "Buffer too small");
                  return FALSE;
              }

              nIntRet = (pabyData[nByteOffset] >> nBitOffset) & nMask;
              dfDoubleRet = nIntRet;
          }
          else if( nBaseItemType == EPT_u8 )
          {
              if (nIndexValue >= nDataSize)
              {
                  CPLError(CE_Failure, CPLE_AppDefined, "Buffer too small");
                  return FALSE;
              }
              dfDoubleRet = pabyData[nIndexValue];
              nIntRet = pabyData[nIndexValue];
          }
          else if( nBaseItemType == EPT_s8 )
          {
              if (nIndexValue >= nDataSize)
              {
                  CPLError(CE_Failure, CPLE_AppDefined, "Buffer too small");
                  return FALSE;
              }
              dfDoubleRet = ((signed char *)pabyData)[nIndexValue];
              nIntRet = ((signed char *)pabyData)[nIndexValue];
          }
          else if( nBaseItemType == EPT_s16 )
          {
              GInt16  nValue;
              if (nIndexValue*2 + 2 > nDataSize)
              {
                  CPLError(CE_Failure, CPLE_AppDefined, "Buffer too small");
                  return FALSE;
              }
              memcpy( &nValue, pabyData + 2*nIndexValue, 2 );
              HFAStandard( 2, &nValue );

              dfDoubleRet = nValue;
              nIntRet = nValue;
          }
          else if( nBaseItemType == EPT_u16 )
          {
              GUInt16  nValue;
              if (nIndexValue*2 + 2 > nDataSize)
              {
                  CPLError(CE_Failure, CPLE_AppDefined, "Buffer too small");
                  return FALSE;
              }
              memcpy( &nValue, pabyData + 2*nIndexValue, 2 );
              HFAStandard( 2, &nValue );

              dfDoubleRet = nValue;
              nIntRet = nValue;
          }
          else if( nBaseItemType == EPT_s32 )
          {
              GInt32  nValue;
              if (nIndexValue*4 + 4 > nDataSize)
              {
                  CPLError(CE_Failure, CPLE_AppDefined, "Buffer too small");
                  return FALSE;
              }
              memcpy( &nValue, pabyData + 4*nIndexValue, 4 );
              HFAStandard( 4, &nValue );

              dfDoubleRet = nValue;
              nIntRet = nValue;
          }
          else if( nBaseItemType == EPT_u32 )
          {
              GUInt32  nValue;
              if (nIndexValue*4 + 4 > nDataSize)
              {
                  CPLError(CE_Failure, CPLE_AppDefined, "Buffer too small");
                  return FALSE;
              }
              memcpy( &nValue, pabyData + 4*nIndexValue, 4 );
              HFAStandard( 4, &nValue );

              dfDoubleRet = nValue;
              nIntRet = nValue;
          }
          else if( nBaseItemType == EPT_f32 )
          {
              float fValue;
              if (nIndexValue*4 + 4 > nDataSize)
              {
                  CPLError(CE_Failure, CPLE_AppDefined, "Buffer too small");
                  return FALSE;
              }
              memcpy( &fValue, pabyData + 4*nIndexValue, 4 );
              HFAStandard( 4, &fValue );

              dfDoubleRet = fValue;
              nIntRet = (int) fValue;
          }
          else if( nBaseItemType == EPT_f64 )
          {
              double dfValue;
              if (nIndexValue*8 + 8 > nDataSize)
              {
                  CPLError(CE_Failure, CPLE_AppDefined, "Buffer too small");
                  return FALSE;
              }
              memcpy( &dfValue, pabyData+8*nIndexValue, 8 );
              HFAStandard( 8, &dfValue );

              dfDoubleRet = dfValue;
              nIntRet = (int) dfValue;
          }
          else
          {
              CPLError(CE_Failure, CPLE_AppDefined, "Unknown base item type : %d", nBaseItemType);
              return FALSE;
          }
      }
      break;

      case 'o':
        if( poItemObjectType != NULL )
        {
            int		nExtraOffset = 0;
            int		iIndexCounter;

            if( poItemObjectType->nBytes > 0 )
            {
                if (nIndexValue != 0 && poItemObjectType->nBytes > INT_MAX / nIndexValue)
                    return CE_Failure;
                nExtraOffset = poItemObjectType->nBytes * nIndexValue;
            }
            else
            {
                for( iIndexCounter = 0;
                     iIndexCounter < nIndexValue && nExtraOffset < nDataSize;
                     iIndexCounter++ )
                {
                    int nInc = poItemObjectType->GetInstBytes(pabyData + nExtraOffset,
                                                              nDataSize - nExtraOffset);
                    if (nInc < 0 || nExtraOffset > INT_MAX - nInc)
                    {
                        CPLError(CE_Failure, CPLE_AppDefined, "Invalid return value");
                        return CE_Failure;
                    }

                    nExtraOffset += nInc;
                }
            }

            if (nExtraOffset >= nDataSize)
                return CE_Failure;

            pabyRawData = pabyData + nExtraOffset;

            if( pszField != NULL && strlen(pszField) > 0 )
            {
                return( poItemObjectType->
                        ExtractInstValue( pszField, pabyRawData,
                                          nDataOffset + nExtraOffset,
                                          nDataSize - nExtraOffset,
                                          chReqType, pReqReturn, pnRemainingDataSize ) );
            }
        }
        break;

      default:
        return FALSE;
        break;
    }

/* -------------------------------------------------------------------- */
/*      Return the appropriate representation.                          */
/* -------------------------------------------------------------------- */
    if( chReqType == 's' )
    {
        if( pszStringRet == NULL )
        {
            /* HFAEntry:: BuildEntryFromMIFObject() expects to have always */
            /* 8 bytes before the data. In normal situations, it should */
            /* not go here, but that can happen if the file is corrupted */
            /* so reserve the first 8 bytes before the string to contain null bytes */
            memset(szNumberString, 0, 8);
            sprintf( szNumberString + 8, "%.14g", dfDoubleRet );
            pszStringRet = szNumberString + 8;
        }
        
        *((char **) pReqReturn) = pszStringRet;
        return( TRUE );
    }
    else if( chReqType == 'd' )
    {
        *((double *)pReqReturn) = dfDoubleRet;
        return( TRUE );
    }
    else if( chReqType == 'i' )
    {
        *((int *) pReqReturn) = nIntRet;
        return( TRUE );
    }
    else if( chReqType == 'p' )
    {
        *((GByte **) pReqReturn) = pabyRawData;
        return( TRUE );
    }
    else
    {
        CPLAssert( FALSE );
        return FALSE;
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
    int		nCount;
    int		nInstBytes = 0;
    
    if( nBytes > -1 )
        return nBytes;

    if( chPointer != '\0' )
    {
        if (nDataSize < 4)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Buffer too small");
            return -1;
        }

        memcpy( &nCount, pabyData, 4 );
        HFAStandard( 4, &nCount );

        pabyData += 8;
        nInstBytes += 8;
    }
    else
        nCount = 1;

    if( chItemType == 'b' && nCount != 0 ) // BASEDATA
    {
        if (nDataSize - nInstBytes < 4+4+2)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Buffer too small");
            return -1;
        }

        GInt32 nRows, nColumns;
        GInt16 nBaseItemType;
        
        memcpy( &nRows, pabyData, 4 );
        HFAStandard( 4, &nRows );
        memcpy( &nColumns, pabyData+4, 4 );
        HFAStandard( 4, &nColumns );
        memcpy( &nBaseItemType, pabyData+8, 2 );
        HFAStandard( 2, &nBaseItemType );

        nInstBytes += 12;
        
        if (nRows < 0 || nColumns < 0)
            return -1;
        if (nColumns != 0 && nRows > INT_MAX / nColumns)
            return -1;
        if (nColumns != 0 && ((HFAGetDataTypeBits(nBaseItemType) + 7) / 8) * nRows > INT_MAX / nColumns)
            return -1;
        if (((HFAGetDataTypeBits(nBaseItemType) + 7) / 8) * nRows * nColumns > INT_MAX - nInstBytes)
            return -1;

        nInstBytes += 
            ((HFAGetDataTypeBits(nBaseItemType) + 7) / 8) * nRows * nColumns;
    }
    else if( poItemObjectType == NULL )
    {
        if (nCount != 0 && HFADictionary::GetItemSize(chItemType) > INT_MAX / nCount)
            return -1;
        nInstBytes += nCount * HFADictionary::GetItemSize(chItemType);
    }
    else
    {
        int		i;

        for( i = 0; i < nCount &&
                    nInstBytes < nDataSize &&
                    nInstBytes >= 0; i++ )
        {
            int	nThisBytes;

            nThisBytes =
                poItemObjectType->GetInstBytes( pabyData,
                                                nDataSize - nInstBytes );
            if (nThisBytes < 0 || nInstBytes > INT_MAX - nThisBytes)
            {
                CPLError(CE_Failure, CPLE_AppDefined, "Invalid return value");
                return -1;
            }

            nInstBytes += nThisBytes;
            pabyData += nThisBytes;
        }
    }

    return( nInstBytes );
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
    else if( chItemType == 'b' )
    {
        GInt32 nRows, nColumns;

        if( nDataSize < 20 )
            return 0;

        memcpy( &nRows, pabyData+8, 4 );
        HFAStandard( 4, &nRows );
        memcpy( &nColumns, pabyData+12, 4 );
        HFAStandard( 4, &nColumns );

        if (nRows < 0 || nColumns < 0)
            return 0;
        if (nColumns != 0 && nRows > INT_MAX / nColumns)
            return 0;

        return nRows * nColumns;
    }
    else
    {
        GInt32 nCount;

        if( nDataSize < 4 )
            return 0;

        memcpy( &nCount, pabyData, 4 );
        HFAStandard( 4, &nCount );
        return nCount;
    }
}

/************************************************************************/
/*                           DumpInstValue()                            */
/************************************************************************/

void HFAField::DumpInstValue( FILE *fpOut, 
                           GByte *pabyData, GUInt32 nDataOffset, int nDataSize,
                           const char *pszPrefix )

{
    int		iEntry, nEntries;
    void	*pReturn;
    char	szLongFieldName[256];

    nEntries = GetInstCount( pabyData, nDataSize );

/* -------------------------------------------------------------------- */
/*      Special case for arrays of chars or uchars which are printed    */
/*      as a string.                                                    */
/* -------------------------------------------------------------------- */
    if( (chItemType == 'c' || chItemType == 'C') && nEntries > 0 )
    {
        if( ExtractInstValue( NULL, 0,
                              pabyData, nDataOffset, nDataSize,
                              's', &pReturn ) )
            VSIFPrintf( fpOut, "%s%s = `%s'\n",
                        pszPrefix, pszFieldName,
                        (char *) pReturn );
        else
            VSIFPrintf( fpOut, "%s%s = (access failed)\n",
                        pszPrefix, pszFieldName );

        return;
    }
            
/* -------------------------------------------------------------------- */
/*      For BASEDATA objects, we want to first dump their dimension     */
/*      and type.                                                       */
/* -------------------------------------------------------------------- */
    if( chItemType == 'b' )
    {
        int nDataType, nRows, nColumns;
        int bSuccess = ExtractInstValue( NULL, -3, pabyData, nDataOffset, 
                          nDataSize, 'i', &nDataType );
        if (bSuccess)
        {
            ExtractInstValue( NULL, -2, pabyData, nDataOffset, 
                            nDataSize, 'i', &nColumns );
            ExtractInstValue( NULL, -1, pabyData, nDataOffset, 
                            nDataSize, 'i', &nRows );
            VSIFPrintf( fpOut, "%sBASEDATA(%s): %dx%d of %s\n", 
                        pszPrefix, pszFieldName,
                        nColumns, nRows, HFAGetDataTypeName( nDataType ) );
        }
        else
        {
            VSIFPrintf( fpOut, "%sBASEDATA(%s): empty\n", 
                        pszPrefix, pszFieldName );
        }
    }
        
/* -------------------------------------------------------------------- */
/*      Dump each entry in the field array.                             */
/* -------------------------------------------------------------------- */
    for( iEntry = 0; iEntry < MIN(MAX_ENTRY_REPORT,nEntries); iEntry++ )
    {
        if( nEntries == 1 )
            VSIFPrintf( fpOut, "%s%s = ", pszPrefix, pszFieldName );
        else
            VSIFPrintf( fpOut, "%s%s[%d] = ",
                        pszPrefix, pszFieldName, iEntry );
        
        switch( chItemType )
        {
          case 'f':
          case 'd':
          {
              double  dfValue;
              if( ExtractInstValue( NULL, iEntry,
                                    pabyData, nDataOffset, nDataSize,
                                    'd', &dfValue ) )
                  VSIFPrintf( fpOut, "%f\n", dfValue );
              else
                  VSIFPrintf( fpOut, "(access failed)\n" );
          }
          break;

          case 'b':
          {
              double dfValue;

              if( ExtractInstValue( NULL, iEntry, 
                                    pabyData, nDataOffset, nDataSize, 
                                    'd', &dfValue ) )
                  VSIFPrintf( fpOut, "%s%.15g\n", pszPrefix, dfValue );
              else
                  VSIFPrintf( fpOut, "%s(access failed)\n", pszPrefix );
          }
          break;

          case 'e':
            if( ExtractInstValue( NULL, iEntry,
                                  pabyData, nDataOffset, nDataSize,
                                  's', &pReturn ) )
                VSIFPrintf( fpOut, "%s\n", (char *) pReturn );
            else
                VSIFPrintf( fpOut, "(access failed)\n" );
            break;

          case 'o':
            if( !ExtractInstValue( NULL, iEntry,
                                   pabyData, nDataOffset, nDataSize,
                                   'p', &pReturn ) )
            {
                VSIFPrintf( fpOut, "(access failed)\n" );
            }
            else
            {
                int		nByteOffset;

                VSIFPrintf( fpOut, "\n" );
                
                nByteOffset = ((GByte *) pReturn) - pabyData;
            
                sprintf( szLongFieldName, "%s    ", pszPrefix );
            
                if( poItemObjectType )
                    poItemObjectType->DumpInstValue( fpOut,
                                                     pabyData + nByteOffset,
                                                     nDataOffset + nByteOffset,
                                                     nDataSize - nByteOffset,
                                                     szLongFieldName );
            }
            break;

          default:
          {
              GInt32 nIntValue;

              if( ExtractInstValue( NULL, iEntry,
                                    pabyData, nDataOffset, nDataSize,
                                    'i', &nIntValue ) )
                  VSIFPrintf( fpOut, "%d\n", nIntValue );
              else
                  VSIFPrintf( fpOut, "(access failed)\n" );
          }
          break;
        }
    }

    if( nEntries > MAX_ENTRY_REPORT )
        printf( "%s ... remaining instances omitted ...\n", pszPrefix );

    if( nEntries == 0 )
        VSIFPrintf( fpOut, "%s%s = (no values)\n", pszPrefix, pszFieldName );

}
