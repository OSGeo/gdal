/******************************************************************************
 * $Id$
 *
 * Project:  Erdas Imagine (.img) Translator
 * Purpose:  Implementation of the HFAField class for managing information
 *           about one field in a HFA dictionary type.  Managed by HFAType.
 * Author:   Frank Warmerdam, warmerda@home.com
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
 * Revision 1.2  1999/01/22 17:37:59  warmerda
 * Fixed up support for variable sizes, and arrays of variable sized objects
 *
 * Revision 1.1  1999/01/04 22:52:10  warmerda
 * New
 */

#include "hfa_p.h"
                           
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
/*                          ExtractInstValue()                          */
/*                                                                      */
/*      Extract the value of an instance of a field.                    */
/*                                                                      */
/*      pszField should be NULL if this field is not a                  */
/*      substructure.                                                   */
/************************************************************************/

void *HFAField::ExtractInstValue( const char * pszField, int nIndexValue,
                               GByte *pabyData, int nDataOffset, int nDataSize,
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
        GUInt32		*panInfo = (GUInt32 *) pabyData;

        if( panInfo[1] != (GUInt32) (nDataOffset + 8) )
        {
            CPLError( CE_Warning, CPLE_AppDefined,
                      "%s.%s points at %d, not %d as expected\n",
                      pszFieldName, pszField ? pszField : "",
                      panInfo[1], nDataOffset+8 );
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

      case 'o':
        if( poItemObjectType != NULL )
        {
            int		nExtraOffset = 0;
            int		iIndexCounter;

            for( iIndexCounter = 0;
                 iIndexCounter < nIndexValue-1;
                 iIndexCounter++ )
            {
                nExtraOffset +=
                    poItemObjectType->GetInstBytes( pabyData + nExtraOffset );
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
        nCount = *((GUInt32 *) pabyData);
        pabyData += 8;
        nInstBytes += 8;
    }
    else
        nCount = 1;

    if( poItemObjectType == NULL )
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
    GUInt32	*panInfo = (GUInt32 *) pabyData;
    
    if( chPointer == '\0' )
        return nItemCount;
    else
        return panInfo[0];
}

/************************************************************************/
/*                           DumpInstValue()                            */
/************************************************************************/

void HFAField::DumpInstValue(  FILE *fpOut, 
                               GByte *pabyData, int nDataOffset, int nDataSize,
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
    if( (chItemType == 'c' || chItemType == 'C')
        && GetInstCount( pabyData ) > 0 )
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
    for( iEntry = 0; iEntry < MIN(8,nEntries); iEntry++ )
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
            VSIFPrintf( fpOut, "(basedata)\n" );
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

    if( nEntries > 8 )
        printf( "%s ... remaining instances omitted ...\n", pszPrefix );

    if( nEntries == 0 )
        VSIFPrintf( fpOut, "%s%s = (no values)\n", pszPrefix, pszFieldName );

}
