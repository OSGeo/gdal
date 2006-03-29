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
 ******************************************************************************
 *
 * $Log$
 * Revision 1.19  2006/03/29 14:24:04  fwarmerdam
 * added preliminary nodata support (readonly)
 *
 * Revision 1.18  2005/10/02 15:14:48  fwarmerdam
 * fixed size for <8bit basedata items
 *
 * Revision 1.17  2005/09/28 19:38:07  fwarmerdam
 * Added partial support for inline defined types.
 *
 * Revision 1.16  2005/05/10 00:56:17  fwarmerdam
 * fixed bug with setting entries in an array (with count setting)
 *
 * Revision 1.15  2004/02/13 15:58:11  warmerda
 * Fixed serious bug with GetInstBytes() for BASEDATA * fields with
 * a count of zero.  Such as the Excluded field of most stats nodes!
 *
 * Revision 1.14  2003/12/08 19:09:34  warmerda
 * implemented DumpInstValue and GetInstBytes for basedata
 *
 * Revision 1.13  2003/05/21 15:35:05  warmerda
 * cleanup type conversion warnings
 *
 * Revision 1.12  2003/04/22 19:40:36  warmerda
 * fixed email address
 *
 * Revision 1.11  2001/07/18 04:51:57  warmerda
 * added CPL_CVSID
 *
 * Revision 1.10  2000/12/29 16:37:32  warmerda
 * Use GUInt32 for all file offsets
 *
 * Revision 1.9  2000/10/12 19:30:32  warmerda
 * substantially improved write support
 *
 * Revision 1.8  2000/09/29 21:42:38  warmerda
 * preliminary write support implemented
 *
 * Revision 1.7  1999/06/01 13:07:59  warmerda
 * added speed up for indexing into fixes size object arrays
 *
 * Revision 1.6  1999/02/15 19:06:18  warmerda
 * Disable warning on field offsets for Intergraph delivery
 *
 * Revision 1.5  1999/01/28 18:28:28  warmerda
 * minor simplification of code
 *
 * Revision 1.4  1999/01/28 18:03:07  warmerda
 * Fixed some byte swapping problems, and problems with accessing data from
 * the file that isn't on a word boundary.
 *
 * Revision 1.3  1999/01/22 19:23:11  warmerda
 * Fixed bug with offset into arrays of structures.
 *
 * Revision 1.2  1999/01/22 17:37:59  warmerda
 * Fixed up support for variable sizes, and arrays of variable sized objects
 *
 * Revision 1.1  1999/01/04 22:52:10  warmerda
 * New
 */

#include "hfa_p.h"

CPL_CVSID("$Id$");

#define MAX_ENTRY_REPORT   16
                           
/************************************************************************/
/* ==================================================================== */
/*      		       HFAField					*/
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

    CPLAssert( strchr( "124cCesStlLfdmMbox", chItemType) != NULL );

/* -------------------------------------------------------------------- */
/*      If this is an object, we extract the type of the object.        */
/* -------------------------------------------------------------------- */
    if( chItemType == 'o' )
    {
        for( i = 0; pszInput[i] != '\0' && pszInput[i] != ','; i++ ) {}

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

        chItemType = 'o';

        // find the comma terminating the type name.
        for( i = 0; pszInput[i] != '\0' && pszInput[i] != ','; i++ ) {}

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

        pszInput = strchr(pszInput,':');
        if( pszInput == NULL )
            return NULL;

        pszInput++;

        papszEnumNames = (char **) CPLCalloc(sizeof(char *), nEnumCount+1);
        
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

        nOffset = nCount;
        HFAStandard( 4, &nOffset );
        memcpy( pabyData, &nOffset, 4 );

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

          HFAStandard( 2, &nNumber );
          memcpy( pabyData + nIndexValue*2, &nNumber, 2 );
      }
      break;

      case 'S':
      {
          short nNumber;

          nNumber = (short) nIntValue;
          HFAStandard( 2, &nNumber );
          memcpy( pabyData + nIndexValue*2, &nNumber, 2 );
      }
      break;
        
      case 't':
      case 'l':
      {
          GUInt32	nNumber = nIntValue;

          HFAStandard( 4, &nNumber );
          memcpy( pabyData + nIndexValue*4, &nNumber, 4 );
      }
      break;
      
      case 'L':
      {
          GInt32	nNumber = nIntValue;
          
          HFAStandard( 4, &nNumber );
          memcpy( pabyData + nIndexValue*4, &nNumber, 4 );
      }
      break;
      
      case 'f':
      {
          float		fNumber = (float) dfDoubleValue;
          
          HFAStandard( 4, &fNumber );
          memcpy( pabyData + nIndexValue*4, &fNumber, 4 );
      }
      break;
        
      case 'd':
      {
          double	dfNumber = dfDoubleValue;
          
          HFAStandard( 8, &dfNumber );
          memcpy( pabyData + nIndexValue*8, &dfNumber, 8 );
      }
      break;

      case 'o':
        if( poItemObjectType != NULL )
        {
            int		nExtraOffset = 0;
            int		iIndexCounter;

            if( poItemObjectType->nBytes > 0 )
            {
                nExtraOffset = poItemObjectType->nBytes * nIndexValue;
            }
            else
            {
                for( iIndexCounter = 0;
                     iIndexCounter < nIndexValue;
                     iIndexCounter++ )
                {
                    nExtraOffset +=
                        poItemObjectType->GetInstBytes(pabyData+nExtraOffset);
                }
            }

            if( pszField != NULL && strlen(pszField) > 0 )
            {
                return( poItemObjectType->
                            SetInstValue( pszField, pabyData + nExtraOffset,
                                          nDataOffset + nExtraOffset,
                                          nDataSize - nExtraOffset,
                                          chReqType, pValue ) );
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

void *
HFAField::ExtractInstValue( const char * pszField, int nIndexValue,
                           GByte *pabyData, GUInt32 nDataOffset, int nDataSize,
                           char chReqType )

{
    char		*pszStringRet = NULL;
    static int		nIntRet = 0;
    static double	dfDoubleRet = 0.0;
    int			nInstItemCount = GetInstCount( pabyData );
    GByte		*pabyRawData = NULL;

/* -------------------------------------------------------------------- */
/*      Check the index value is valid.                                 */
/*                                                                      */
/*      Eventually this will have to account for variable fields.       */
/* -------------------------------------------------------------------- */
    if( nIndexValue < 0 || nIndexValue >= nInstItemCount )
        return NULL;

/* -------------------------------------------------------------------- */
/*	If this field contains a pointer, then we will adjust the	*/
/*	data offset relative to it.    					*/
/* -------------------------------------------------------------------- */
    if( chPointer != '\0' )
    {
        GUInt32		nOffset;

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
        return( pabyData );

/* -------------------------------------------------------------------- */
/*      Handle by type.                                                 */
/* -------------------------------------------------------------------- */
    switch( chItemType )
    {
      case 'c':
      case 'C':
        nIntRet = pabyData[nIndexValue];
        dfDoubleRet = nIntRet;
        break;

      case 'e':
      case 's':
      {
          unsigned short nNumber;

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
          
          memcpy( &nNumber, pabyData + nIndexValue*4, 4 );
          HFAStandard( 4, &nNumber );
          nIntRet = nNumber;
          dfDoubleRet = nIntRet;
      }
      break;
      
      case 'L':
      {
          GInt32	nNumber;
          
          memcpy( &nNumber, pabyData + nIndexValue*4, 4 );
          HFAStandard( 4, &nNumber );
          nIntRet = nNumber;
          dfDoubleRet = nIntRet;
      }
      break;
      
      case 'f':
      {
          float		fNumber;
          
          memcpy( &fNumber, pabyData + nIndexValue*4, 4 );
          HFAStandard( 4, &fNumber );
          dfDoubleRet = fNumber;
          nIntRet = (int) fNumber;
      }
      break;
        
      case 'd':
      {
          double	dfNumber;
          
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
        
          memcpy( &nRows, pabyData, 4 );
          HFAStandard( 4, &nRows );
          memcpy( &nColumns, pabyData+4, 4 );
          HFAStandard( 4, &nColumns );
          memcpy( &nBaseItemType, pabyData+8, 2 );
          HFAStandard( 2, &nBaseItemType );

          pabyData += 10;

          CPLAssert( nRows >= 1 && nColumns >= 1 );

          if( nBaseItemType == EPT_u8 )
          {
              dfDoubleRet = pabyData[0];
              nIntRet = pabyData[0];
          }
          else if( nBaseItemType == EPT_s16 )
          {
              GInt16  nValue;
              
              memcpy( &nValue, pabyData, 2 );
              HFAStandard( 2, &nValue );

              dfDoubleRet = nValue;
              nIntRet = nValue;
          }
          else if( nBaseItemType == EPT_u16 )
          {
              GUInt16  nValue;
              
              memcpy( &nValue, pabyData, 2 );
              HFAStandard( 2, &nValue );

              dfDoubleRet = nValue;
              nIntRet = nValue;
          }
          else if( nBaseItemType == EPT_f32 )
          {
              float fValue;
              
              memcpy( &fValue, pabyData, 4 );
              HFAStandard( 4, &fValue );

              dfDoubleRet = fValue;
              nIntRet = (int) fValue;
          }
          else
          {
              CPLAssert( FALSE );
              return NULL;
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
                nExtraOffset = poItemObjectType->nBytes * nIndexValue;
            }
            else
            {
                for( iIndexCounter = 0;
                     iIndexCounter < nIndexValue;
                     iIndexCounter++ )
                {
                    nExtraOffset +=
                        poItemObjectType->GetInstBytes(pabyData+nExtraOffset);
                }
            }

            pabyRawData = pabyData + nExtraOffset;

            if( pszField != NULL && strlen(pszField) > 0 )
            {
                return( poItemObjectType->
                        ExtractInstValue( pszField, pabyRawData,
                                          nDataOffset + nExtraOffset,
                                          nDataSize - nExtraOffset,
                                          chReqType ) );
            }
        }
        break;

      default:
        return NULL;
        break;
    }

/* -------------------------------------------------------------------- */
/*      Return the appropriate representation.                          */
/* -------------------------------------------------------------------- */
    if( chReqType == 's' )
    {
        if( pszStringRet == NULL )
        {
            static char	szNumber[28];

            sprintf( szNumber, "%d", nIntRet );
            pszStringRet = szNumber;
        }
        
        return( pszStringRet );
    }
    else if( chReqType == 'd' )
        return( &dfDoubleRet );
    else if( chReqType == 'i' )
        return( &nIntRet );
    else if( chReqType == 'p' )
        return( pabyRawData );
    else
    {
        CPLAssert( FALSE );
        return NULL;
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

int HFAField::GetInstBytes( GByte * pabyData )

{
    int		nCount;
    int		nInstBytes = 0;
    
    if( nBytes > -1 )
        return nBytes;

    if( chPointer != '\0' )
    {
        memcpy( &nCount, pabyData, 4 );
        HFAStandard( 4, &nCount );

        pabyData += 8;
        nInstBytes += 8;
    }
    else
        nCount = 1;

    if( chItemType == 'b' && nCount != 0 ) // BASEDATA
    {
        GInt32 nRows, nColumns;
        GInt16 nBaseItemType;
        
        memcpy( &nRows, pabyData, 4 );
        HFAStandard( 4, &nRows );
        memcpy( &nColumns, pabyData+4, 4 );
        HFAStandard( 4, &nColumns );
        memcpy( &nBaseItemType, pabyData+8, 2 );
        HFAStandard( 2, &nBaseItemType );

        nInstBytes += 12;

        nInstBytes += 
            ((HFAGetDataTypeBits(nBaseItemType) + 7) / 8) * nRows * nColumns;
    }
    else if( poItemObjectType == NULL )
    {
        nInstBytes += nCount * HFADictionary::GetItemSize(chItemType);
    }
    else
    {
        int		i;

        for( i = 0; i < nCount; i++ )
        {
            int	nThisBytes;

            nThisBytes = poItemObjectType->GetInstBytes( pabyData );
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

int HFAField::GetInstCount( GByte * pabyData )

{
    if( chPointer == '\0' )
        return nItemCount;
    else
    {
        GInt32 nCount;

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

    nEntries = GetInstCount( pabyData );

/* -------------------------------------------------------------------- */
/*      Special case for arrays of chars or uchars which are printed    */
/*      as a string.                                                    */
/* -------------------------------------------------------------------- */
    if( (chItemType == 'c' || chItemType == 'C') && nEntries > 0 )
    {
        pReturn = ExtractInstValue( NULL, 0,
                                    pabyData, nDataOffset, nDataSize,
                                    's' );
        if( pReturn != NULL )
            VSIFPrintf( fpOut, "%s%s = `%s'\n",
                        pszPrefix, pszFieldName,
                        (char *) pReturn );
        else
            VSIFPrintf( fpOut, "%s%s = (access failed)\n",
                        pszPrefix, pszFieldName );

        return;
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
            pReturn = ExtractInstValue( NULL, iEntry,
                                        pabyData, nDataOffset, nDataSize,
                                        'd' );
            if( pReturn != NULL )
                VSIFPrintf( fpOut, "%f\n",
                            *((double *) pReturn) );
            else
                VSIFPrintf( fpOut, "(access failed)\n" );
            break;

          case 'b':
          {
              GInt32 nRows, nColumns;

              GInt16 nBaseItemType;
        
              memcpy( &nRows, pabyData+8, 4 );
              HFAStandard( 4, &nRows );
              memcpy( &nColumns, pabyData+12, 4 );
              HFAStandard( 4, &nColumns );
              memcpy( &nBaseItemType, pabyData+16, 2 );
              HFAStandard( 2, &nBaseItemType );
              
              VSIFPrintf( fpOut, "%dx%d basedata of type %d\n",
                          nRows, nColumns, nBaseItemType );
          }
          break;

          case 'e':
            pReturn = ExtractInstValue( NULL, iEntry,
                                        pabyData, nDataOffset, nDataSize,
                                        's' );
            if( pReturn != NULL )
                VSIFPrintf( fpOut, "%s\n",
                            (char *) pReturn );
            else
                VSIFPrintf( fpOut, "(access failed)\n" );
            break;

          case 'o':
            pReturn = ExtractInstValue( NULL, iEntry,
                                        pabyData, nDataOffset, nDataSize,
                                        'p' );

            if( pReturn == NULL )
            {
                VSIFPrintf( fpOut, "(access failed)\n" );
            }
            else
            {
                int		nByteOffset;

                VSIFPrintf( fpOut, "\n" );
                
                nByteOffset = ((GByte *) pReturn) - pabyData;
            
                sprintf( szLongFieldName, "%s    ", pszPrefix );
            
                poItemObjectType->DumpInstValue( fpOut,
                                                 pabyData + nByteOffset,
                                                 nDataOffset + nByteOffset,
                                                 nDataSize - nByteOffset,
                                                 szLongFieldName );
            }
            break;

          default:
            pReturn = ExtractInstValue( NULL, iEntry,
                                        pabyData, nDataOffset, nDataSize,
                                        'i' );
            if( pReturn != NULL )
                VSIFPrintf( fpOut, "%d\n",
                            *((int *) pReturn) );
            else
                VSIFPrintf( fpOut, "(access failed)\n" );
            break;
        }
    }

    if( nEntries > MAX_ENTRY_REPORT )
        printf( "%s ... remaining instances omitted ...\n", pszPrefix );

    if( nEntries == 0 )
        VSIFPrintf( fpOut, "%s%s = (no values)\n", pszPrefix, pszFieldName );

}
