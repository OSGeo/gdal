/******************************************************************************
 * $Id$
 *
 * Name:     hfadictionary.cpp
 * Project:  Erdas Imagine (.img) Translator
 * Purpose:  Implementation of the HFADictionary, HFAType, and HFAField classes
 *           for parsing, and interpreting HFA data dictionaries.
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
 * Revision 1.1  1999/01/04 05:28:12  warmerda
 * New
 *
 */

#include "hfa_p.h"
#include "cpl_conv.h"

/************************************************************************/
/* ==================================================================== */
/*      		       HFADictionary                            */
/* ==================================================================== */
/************************************************************************/


/************************************************************************/
/*                           HFADictionary()                            */
/************************************************************************/

HFADictionary::HFADictionary( const char * pszString )

{
    int		i;
    
    nTypes = 0;
    papoTypes = NULL;

/* -------------------------------------------------------------------- */
/*      Read all the types.                                             */
/* -------------------------------------------------------------------- */
    while( pszString != NULL && *pszString != '.' )
    {
        HFAType		*poNewType;

        poNewType = new HFAType();
        pszString = poNewType->Initialize( pszString );

        if( pszString != NULL )
        {
            papoTypes = (HFAType **)
                CPLRealloc(papoTypes, sizeof(HFAType*) * (nTypes+1));
            papoTypes[nTypes++] = poNewType;
        }
        else
            delete poNewType;
    }

/* -------------------------------------------------------------------- */
/*      Complete the definitions.                                       */
/* -------------------------------------------------------------------- */
    for( i = 0; i < nTypes; i++ )
    {
        papoTypes[i]->CompleteDefn( this );
    }
}

/************************************************************************/
/*                           ~HFADictionary()                           */
/************************************************************************/

HFADictionary::~HFADictionary()

{
    int		i;

    for( i = 0; i < nTypes; i++ )
        delete papoTypes[i];
    
    CPLFree( papoTypes );
}

/************************************************************************/
/*                              FindType()                              */
/************************************************************************/

HFAType * HFADictionary::FindType( const char * pszName )

{
    int		i;

    for( i = 0; i < nTypes; i++ )
    {
        if( strcmp(pszName,papoTypes[i]->pszTypeName) == 0 )
            return( papoTypes[i] );
    }

    return NULL;
}

/************************************************************************/
/*                            GetItemSize()                             */
/*                                                                      */
/*      Get the size of a basic (atomic) item.                          */
/************************************************************************/

int HFADictionary::GetItemSize( char chType )

{
    switch( chType )
    {
      case '1':
      case '2':
      case '4':
      case 'c':
      case 'C':
        return 1;

      case 'e':
      case 's':
      case 'S':
        return 2;

      case 't':
      case 'l':
      case 'L':
      case 'f':
        return 4;

      case 'd':
      case 'm':
        return 8;

      case 'M':
        return 16;

      case 'b':
      case 'o':
      case 'x':
        return 0;

      default:
        CPLAssert( FALSE );
    }

    return 0;
}

/************************************************************************/
/*                                Dump()                                */
/************************************************************************/

void HFADictionary::Dump( FILE * fp )

{
    int		i;
    
    VSIFPrintf( fp, "\nHFADictionary:\n" );

    for( i = 0; i < nTypes; i++ )
    {
        papoTypes[i]->Dump( fp );
    }
}


/************************************************************************/
/* ==================================================================== */
/*      		       HFAType					*/
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                              HFAType()                               */
/************************************************************************/

HFAType::HFAType()

{
    nBytes = 0;
    nFields = 0;
    papoFields = NULL;
    pszTypeName = NULL;
}

/************************************************************************/
/*                              ~HFAType()                              */
/************************************************************************/

HFAType::~HFAType()

{
    int		i;

    for( i = 0; i < nFields; i++ )
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
    int		i;
    
    if( *pszInput != '{' )
        return NULL;

    pszInput++;

/* -------------------------------------------------------------------- */
/*      Read the field definitions.                                     */
/* -------------------------------------------------------------------- */
    while( pszInput != NULL && *pszInput != '}' )
    {
        HFAField	*poNewField = new HFAField();

        pszInput = poNewField->Initialize( pszInput );
        if( pszInput != NULL )
        {
            papoFields = (HFAField **)
                CPLRealloc(papoFields, sizeof(void*) * (nFields+1) );
            papoFields[nFields++] = poNewField;
        }
        else
            delete poNewField;
    }

    if( pszInput == NULL )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Collect the name.                                               */
/* -------------------------------------------------------------------- */
    pszInput++; /* skip `}' */

    for( i = 0; pszInput[i] != '\0' && pszInput[i] != ','; i++ ) {}

    pszTypeName = (char *) CPLMalloc(i+1);
    strncpy( pszTypeName, pszInput, i );
    pszTypeName[i] = '\0';
    
    pszInput += i+1;

    return( pszInput );
}

/************************************************************************/
/*                            CompleteDefn()                            */
/************************************************************************/

void HFAType::CompleteDefn( HFADictionary * poDict )

{
    int		i;

/* -------------------------------------------------------------------- */
/*      This may already be done, if an earlier object required this    */
/*      object (as a field), and forced an early computation of the     */
/*      size.                                                           */
/* -------------------------------------------------------------------- */
    if( nBytes != 0 )
        return;
    
/* -------------------------------------------------------------------- */
/*      Complete each of the fields, totaling up the sizes.             */
/* -------------------------------------------------------------------- */
    for( i = 0; i < nFields; i++ )
    {
        papoFields[i]->CompleteDefn( poDict );
        nBytes += papoFields[i]->nBytes;
    }
}

/************************************************************************/
/*                                Dump()                                */
/************************************************************************/

void HFAType::Dump( FILE * fp )

{
    int		i;
    
    VSIFPrintf( fp, "HFAType %s/%d bytes\n", pszTypeName, nBytes );

    for( i = 0; i < nFields; i++ )
    {
        papoFields[i]->Dump( fp );
    }

    VSIFPrintf( fp, "\n" );
}


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
    if( chPointer != '\0' )
    {
        nBytes = 4;
    }
    else if( poItemObjectType != NULL )
    {
        poItemObjectType->CompleteDefn( poDict );
        nBytes = poItemObjectType->nBytes * nItemCount;
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
    
    VSIFPrintf( fp, "    %-18s	%c%s[%d];\n",
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
