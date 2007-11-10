/******************************************************************************
 * $Id$
 *
 * Project:  ISO 8211 Access
 * Purpose:  Implements the DDFFieldDefn class.
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

#include "iso8211.h"
#include "cpl_string.h"
#include <ctype.h>

CPL_CVSID("$Id$");

#define CPLE_DiscardedFormat   1301

/************************************************************************/
/*                            DDFFieldDefn()                            */
/************************************************************************/

DDFFieldDefn::DDFFieldDefn()

{
    poModule = NULL;
    pszTag = NULL;
    _fieldName = NULL;
    _arrayDescr = NULL;
    _formatControls = NULL;
    nSubfieldCount = 0;
    papoSubfields = NULL;
    bRepeatingSubfields = FALSE;
    nFixedWidth = 0;
}

/************************************************************************/
/*                           ~DDFFieldDefn()                            */
/************************************************************************/

DDFFieldDefn::~DDFFieldDefn()

{
    int   i;

    CPLFree( pszTag );
    CPLFree( _fieldName );
    CPLFree( _arrayDescr );
    CPLFree( _formatControls );

    for( i = 0; i < nSubfieldCount; i++ )
        delete papoSubfields[i];
    CPLFree( papoSubfields );
}

/************************************************************************/
/*                            AddSubfield()                             */
/************************************************************************/

void DDFFieldDefn::AddSubfield( const char *pszName, 
                                const char *pszFormat )

{
    DDFSubfieldDefn *poSFDefn = new DDFSubfieldDefn;

    poSFDefn->SetName( pszName );
    poSFDefn->SetFormat( pszFormat );
    AddSubfield( poSFDefn );
}

/************************************************************************/
/*                            AddSubfield()                             */
/************************************************************************/

void DDFFieldDefn::AddSubfield( DDFSubfieldDefn *poNewSFDefn,
                                int bDontAddToFormat )

{
    nSubfieldCount++;
    papoSubfields = (DDFSubfieldDefn ** )
        CPLRealloc( papoSubfields, sizeof(void*) * nSubfieldCount );
    papoSubfields[nSubfieldCount-1] = poNewSFDefn;
    
    if( bDontAddToFormat )
        return;

/* -------------------------------------------------------------------- */
/*      Add this format to the format list.  We don't bother            */
/*      aggregating formats here.                                       */
/* -------------------------------------------------------------------- */
    if( _formatControls == NULL || strlen(_formatControls) == 0 )
    {
        CPLFree( _formatControls );
        _formatControls = CPLStrdup( "()" );
    }
    
    int nOldLen = strlen(_formatControls);
    
    char *pszNewFormatControls = (char *) 
        CPLMalloc(nOldLen+3+strlen(poNewSFDefn->GetFormat()));
    
    strcpy( pszNewFormatControls, _formatControls );
    pszNewFormatControls[nOldLen-1] = '\0';
    if( pszNewFormatControls[nOldLen-2] != '(' )
        strcat( pszNewFormatControls, "," );
    
    strcat( pszNewFormatControls, poNewSFDefn->GetFormat() );
    strcat( pszNewFormatControls, ")" );
    
    CPLFree( _formatControls );
    _formatControls = pszNewFormatControls;

/* -------------------------------------------------------------------- */
/*      Add the subfield name to the list.                              */
/* -------------------------------------------------------------------- */
    if( _arrayDescr == NULL )
        _arrayDescr = CPLStrdup("");

    _arrayDescr = (char *) 
        CPLRealloc(_arrayDescr, 
                   strlen(_arrayDescr)+strlen(poNewSFDefn->GetName())+2);
    if( strlen(_arrayDescr) > 0 )
        strcat( _arrayDescr, "!" );
    strcat( _arrayDescr, poNewSFDefn->GetName() );
}

/************************************************************************/
/*                               Create()                               */
/*                                                                      */
/*      Initialize a new field defn from application input, instead     */
/*      of from an existing file.                                       */
/************************************************************************/

int DDFFieldDefn::Create( const char *pszTag, const char *pszFieldName, 
                          const char *pszDescription, 
                          DDF_data_struct_code eDataStructCode,
                          DDF_data_type_code   eDataTypeCode,
                          const char *pszFormat )

{
    CPLAssert( this->pszTag == NULL );
    poModule = NULL;
    this->pszTag = CPLStrdup( pszTag );
    _fieldName = CPLStrdup( pszFieldName );
    _arrayDescr = CPLStrdup( pszDescription );
    _formatControls = CPLStrdup( "" );
    
    _data_struct_code = eDataStructCode;
    _data_type_code = eDataTypeCode;

    if( pszFormat != NULL )
        _formatControls = CPLStrdup( pszFormat );

    if( pszDescription != NULL && *pszDescription == '*' )
        bRepeatingSubfields = TRUE;

    return TRUE;
}

/************************************************************************/
/*                          GenerateDDREntry()                          */
/************************************************************************/

int DDFFieldDefn::GenerateDDREntry( char **ppachData, 
                                    int *pnLength )

{
    *pnLength = 9 + strlen(_fieldName) + 1 
        + strlen(_arrayDescr) + 1
        + strlen(_formatControls) + 1;

    if( strlen(_formatControls) == 0 )
        *pnLength -= 1;

    if( ppachData == NULL )
        return TRUE;

    *ppachData = (char *) CPLMalloc( *pnLength+1 );
    
    if( _data_struct_code == dsc_elementary )
        (*ppachData)[0] = '0';
    else if( _data_struct_code == dsc_vector )
        (*ppachData)[0] = '1';
    else if( _data_struct_code == dsc_array )
        (*ppachData)[0] = '2';
    else if( _data_struct_code == dsc_concatenated )
        (*ppachData)[0] = '3';
    
    if( _data_type_code == dtc_char_string )
        (*ppachData)[1] = '0';
    else if( _data_type_code == dtc_implicit_point )
        (*ppachData)[1] = '1';
    else if( _data_type_code == dtc_explicit_point )
        (*ppachData)[1] = '2';
    else if( _data_type_code == dtc_explicit_point_scaled )
        (*ppachData)[1] = '3';
    else if( _data_type_code == dtc_char_bit_string )
        (*ppachData)[1] = '4';
    else if( _data_type_code == dtc_bit_string )
        (*ppachData)[1] = '5';
    else if( _data_type_code == dtc_mixed_data_type )
        (*ppachData)[1] = '6';

    (*ppachData)[2] = '0';
    (*ppachData)[3] = '0';
    (*ppachData)[4] = ';';
    (*ppachData)[5] = '&';
    (*ppachData)[6] = ' ';
    (*ppachData)[7] = ' ';
    (*ppachData)[8] = ' ';
    sprintf( *ppachData + 9, "%s%c%s", 
             _fieldName, DDF_UNIT_TERMINATOR, _arrayDescr );

    if( strlen(_formatControls) > 0 )
        sprintf( *ppachData + strlen(*ppachData), "%c%s",
                 DDF_UNIT_TERMINATOR, _formatControls );
    sprintf( *ppachData + strlen(*ppachData), "%c", DDF_FIELD_TERMINATOR );

    return TRUE;
}

/************************************************************************/
/*                             Initialize()                             */
/*                                                                      */
/*      Initialize the field definition from the information in the     */
/*      DDR record.  This is called by DDFModule::Open().               */
/************************************************************************/

int DDFFieldDefn::Initialize( DDFModule * poModuleIn,
                              const char * pszTagIn, 
                              int nFieldEntrySize,
                              const char * pachFieldArea )

{
    int         iFDOffset = poModuleIn->GetFieldControlLength();
    int         nCharsConsumed;

    poModule = poModuleIn;
    
    pszTag = CPLStrdup( pszTagIn );

/* -------------------------------------------------------------------- */
/*      Set the data struct and type codes.                             */
/* -------------------------------------------------------------------- */
    switch( pachFieldArea[0] )
    {
      case ' ': /* for ADRG, DIGEST USRP, DIGEST ASRP files */
      case '0':
        _data_struct_code = dsc_elementary;
        break;

      case '1':
        _data_struct_code = dsc_vector;
        break;

      case '2':
        _data_struct_code = dsc_array;
        break;

      case '3':
        _data_struct_code = dsc_concatenated;
        break;

      default:
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Unrecognised data_struct_code value %c.\n"
                  "Field %s initialization incorrect.",
                  pachFieldArea[0], pszTag );
        _data_struct_code = dsc_elementary;
    }

    switch( pachFieldArea[1] )
    {
      case ' ': /* for ADRG, DIGEST USRP, DIGEST ASRP files */
      case '0':
        _data_type_code = dtc_char_string;
        break;
        
      case '1':
        _data_type_code = dtc_implicit_point;
        break;
        
      case '2':
        _data_type_code = dtc_explicit_point;
        break;
        
      case '3':
        _data_type_code = dtc_explicit_point_scaled;
        break;
        
      case '4':
        _data_type_code = dtc_char_bit_string;
        break;
        
      case '5':
        _data_type_code = dtc_bit_string;
        break;
        
      case '6':
        _data_type_code = dtc_mixed_data_type;
        break;

      default:
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Unrecognised data_type_code value %c.\n"
                  "Field %s initialization incorrect.",
                  pachFieldArea[1], pszTag );
        _data_type_code = dtc_char_string;
    }
    
/* -------------------------------------------------------------------- */
/*      Capture the field name, description (sub field names), and      */
/*      format statements.                                              */
/* -------------------------------------------------------------------- */

    _fieldName =
        DDFFetchVariable( pachFieldArea + iFDOffset,
                          nFieldEntrySize - iFDOffset,
                          DDF_UNIT_TERMINATOR, DDF_FIELD_TERMINATOR,
                          &nCharsConsumed );
    iFDOffset += nCharsConsumed;
    
    _arrayDescr =
        DDFFetchVariable( pachFieldArea + iFDOffset,
                          nFieldEntrySize - iFDOffset,
                          DDF_UNIT_TERMINATOR, DDF_FIELD_TERMINATOR, 
                          &nCharsConsumed );
    iFDOffset += nCharsConsumed;
    
    _formatControls =
        DDFFetchVariable( pachFieldArea + iFDOffset,
                          nFieldEntrySize - iFDOffset,
                          DDF_UNIT_TERMINATOR, DDF_FIELD_TERMINATOR, 
                          &nCharsConsumed );
    
/* -------------------------------------------------------------------- */
/*      Parse the subfield info.                                        */
/* -------------------------------------------------------------------- */
    if( _data_struct_code != dsc_elementary )
    {
        if( !BuildSubfields() )
            return FALSE;

        if( !ApplyFormats() )
            return FALSE;
    }
    
    return TRUE;
}

/************************************************************************/
/*                                Dump()                                */
/************************************************************************/

/**
 * Write out field definition info to debugging file.
 *
 * A variety of information about this field definition, and all it's
 * subfields is written to the give debugging file handle.
 *
 * @param fp The standard io file handle to write to.  ie. stderr
 */

void DDFFieldDefn::Dump( FILE * fp )

{
    const char  *pszValue = "";
    
    fprintf( fp, "  DDFFieldDefn:\n" );
    fprintf( fp, "      Tag = `%s'\n", pszTag );
    fprintf( fp, "      _fieldName = `%s'\n", _fieldName );
    fprintf( fp, "      _arrayDescr = `%s'\n", _arrayDescr );
    fprintf( fp, "      _formatControls = `%s'\n", _formatControls );

    switch( _data_struct_code )
    {
      case dsc_elementary:
        pszValue = "elementary";
        break;
        
      case dsc_vector:
        pszValue = "vector";
        break;
        
      case dsc_array:
        pszValue = "array";
        break;
        
      case dsc_concatenated:
        pszValue = "concatenated";
        break;
        
      default:
        CPLAssert( FALSE );
        pszValue = "(unknown)";
    }

    fprintf( fp, "      _data_struct_code = %s\n", pszValue );

    switch( _data_type_code )
    {
      case dtc_char_string:
        pszValue = "char_string";
        break;
        
      case dtc_implicit_point:
        pszValue = "implicit_point";
        break;
        
      case dtc_explicit_point:
        pszValue = "explicit_point";
        break;
        
      case dtc_explicit_point_scaled:
        pszValue = "explicit_point_scaled";
        break;
        
      case dtc_char_bit_string:
        pszValue = "char_bit_string";
        break;
        
      case dtc_bit_string:
        pszValue = "bit_string";
        break;
        
      case dtc_mixed_data_type:
        pszValue = "mixed_data_type";
        break;

      default:
        CPLAssert( FALSE );
        pszValue = "(unknown)";
        break;
    }
    
    fprintf( fp, "      _data_type_code = %s\n", pszValue );

    for( int i = 0; i < nSubfieldCount; i++ )
        papoSubfields[i]->Dump( fp );
}

/************************************************************************/
/*                           BuildSubfields()                           */
/*                                                                      */
/*      Based on the _arrayDescr build a set of subfields.              */
/************************************************************************/

int DDFFieldDefn::BuildSubfields()

{
    char        **papszSubfieldNames;
    const char  *pszSublist = _arrayDescr;

/* -------------------------------------------------------------------- */
/*      It is valid to define a field with _arrayDesc                   */
/*      '*STPT!CTPT!ENPT*YCOO!XCOO' and formatControls '(2b24)'.        */
/*      This basically indicates that there are 3 (YCOO,XCOO)           */
/*      structures named STPT, CTPT and ENPT.  But we can't handle      */
/*      such a case gracefully here, so we just ignore the              */
/*      "structure names" and treat such a thing as a repeating         */
/*      YCOO/XCOO array.  This occurs with the AR2D field of some       */
/*      AML S-57 files for instance.                                    */
/*                                                                      */
/*      We accomplish this by ignoring everything before the last       */
/*      '*' in the subfield list.                                       */
/* -------------------------------------------------------------------- */
    if( strrchr(pszSublist, '*') != NULL )
        pszSublist = strrchr(pszSublist,'*');

/* -------------------------------------------------------------------- */
/*      Strip off the repeating marker, when it occurs, but mark our    */
/*      field as repeating.                                             */
/* -------------------------------------------------------------------- */
    if( pszSublist[0] == '*' )
    {
        bRepeatingSubfields = TRUE;
        pszSublist++;
    }

/* -------------------------------------------------------------------- */
/*      split list of fields .                                          */
/* -------------------------------------------------------------------- */
    papszSubfieldNames = CSLTokenizeStringComplex( pszSublist, "!",
                                                   FALSE, FALSE );

/* -------------------------------------------------------------------- */
/*      minimally initialize the subfields.  More will be done later.   */
/* -------------------------------------------------------------------- */
    int nSFCount = CSLCount( papszSubfieldNames );
    for( int iSF = 0; iSF < nSFCount; iSF++ )
    {
        DDFSubfieldDefn *poSFDefn = new DDFSubfieldDefn;
        
        poSFDefn->SetName( papszSubfieldNames[iSF] );
        AddSubfield( poSFDefn, TRUE );
    }

    CSLDestroy( papszSubfieldNames );

    return TRUE;
}

/************************************************************************/
/*                          ExtractSubstring()                          */
/*                                                                      */
/*      Extract a substring terminated by a comma (or end of            */
/*      string).  Commas in brackets are ignored as terminated with     */
/*      bracket nesting understood gracefully.  If the returned         */
/*      string would being and end with a bracket then strip off the    */
/*      brackets.                                                       */
/*                                                                      */
/*      Given a string like "(A,3(B,C),D),X,Y)" return "A,3(B,C),D".    */
/*      Give a string like "3A,2C" return "3A".                         */
/************************************************************************/

char *DDFFieldDefn::ExtractSubstring( const char * pszSrc )

{
    int         nBracket=0, i;
    char        *pszReturn;

    for( i = 0;
         pszSrc[i] != '\0' && (nBracket > 0 || pszSrc[i] != ',');
         i++ )
    {
        if( pszSrc[i] == '(' )
            nBracket++;
        else if( pszSrc[i] == ')' )
            nBracket--;
    }

    if( pszSrc[0] == '(' )
    {
        pszReturn = CPLStrdup( pszSrc + 1 );
        pszReturn[i-2] = '\0';
    }
    else
    {
        pszReturn = CPLStrdup( pszSrc  );
        pszReturn[i] = '\0';
    }

    return pszReturn;
}

/************************************************************************/
/*                            ExpandFormat()                            */
/************************************************************************/

char *DDFFieldDefn::ExpandFormat( const char * pszSrc )

{
    int         nDestMax = 32;
    char       *pszDest = (char *) CPLMalloc(nDestMax+1);
    int         iSrc, iDst;
    int         nRepeat = 0;

    iSrc = 0;
    iDst = 0;
    pszDest[0] = '\0';

    while( pszSrc[iSrc] != '\0' )
    {
        /* This is presumably an extra level of brackets around some
           binary stuff related to rescaning which we don't care to do
           (see 6.4.3.3 of the standard.  We just strip off the extra
           layer of brackets */
        if( (iSrc == 0 || pszSrc[iSrc-1] == ',') && pszSrc[iSrc] == '(' )
        {
            char       *pszContents = ExtractSubstring( pszSrc+iSrc );
            char       *pszExpandedContents = ExpandFormat( pszContents );

            if( (int) (strlen(pszExpandedContents) + strlen(pszDest) + 1)
                > nDestMax )
            {
                nDestMax = 2 * (strlen(pszExpandedContents) + strlen(pszDest));
                pszDest = (char *) CPLRealloc(pszDest,nDestMax+1);
            }

            strcat( pszDest, pszExpandedContents );
            iDst = strlen(pszDest);
            
            iSrc = iSrc + strlen(pszContents) + 2;

            CPLFree( pszContents );
            CPLFree( pszExpandedContents );
        }

        /* this is a repeated subclause */
        else if( (iSrc == 0 || pszSrc[iSrc-1] == ',')
                 && isdigit(pszSrc[iSrc]) )
        {
            const char *pszNext;
            nRepeat = atoi(pszSrc+iSrc);
            
            // skip over repeat count.
            for( pszNext = pszSrc+iSrc; isdigit(*pszNext); pszNext++ )
                iSrc++;

            char       *pszContents = ExtractSubstring( pszNext );
            char       *pszExpandedContents = ExpandFormat( pszContents );
                
            for( int i = 0; i < nRepeat; i++ )
            {
                if( (int) (strlen(pszExpandedContents) + strlen(pszDest) + 1)
                    > nDestMax )
                {
                    nDestMax = 
                        2 * (strlen(pszExpandedContents) + strlen(pszDest));
                    pszDest = (char *) CPLRealloc(pszDest,nDestMax+1);
                }

                strcat( pszDest, pszExpandedContents );
                if( i < nRepeat-1 )
                    strcat( pszDest, "," );
            }

            iDst = strlen(pszDest);
            
            if( pszNext[0] == '(' )
                iSrc = iSrc + strlen(pszContents) + 2;
            else
                iSrc = iSrc + strlen(pszContents);

            CPLFree( pszContents );
            CPLFree( pszExpandedContents );
        }
        else
        {
            if( iDst+1 >= nDestMax )
            {
                nDestMax = 2 * iDst;
                pszDest = (char *) CPLRealloc(pszDest,nDestMax);
            }

            pszDest[iDst++] = pszSrc[iSrc++];
            pszDest[iDst] = '\0';
        }
    }

    return pszDest;
}
                                 
/************************************************************************/
/*                            ApplyFormats()                            */
/*                                                                      */
/*      This method parses the format string partially, and then        */
/*      applies a subfield format string to each subfield object.       */
/*      It in turn does final parsing of the subfield formats.          */
/************************************************************************/

int DDFFieldDefn::ApplyFormats()

{
    char        *pszFormatList;
    char        **papszFormatItems;
    
/* -------------------------------------------------------------------- */
/*      Verify that the format string is contained within brackets.     */
/* -------------------------------------------------------------------- */
    if( strlen(_formatControls) < 2
        || _formatControls[0] != '('
        || _formatControls[strlen(_formatControls)-1] != ')' )
    {
        CPLError( CE_Warning, CPLE_DiscardedFormat,
                  "Format controls for `%s' field missing brackets:%s",
                  pszTag, _formatControls );
        
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Duplicate the string, and strip off the brackets.               */
/* -------------------------------------------------------------------- */

    pszFormatList = ExpandFormat( _formatControls );

/* -------------------------------------------------------------------- */
/*      Tokenize based on commas.                                       */
/* -------------------------------------------------------------------- */
    papszFormatItems =
        CSLTokenizeStringComplex(pszFormatList, ",", FALSE, FALSE );

    CPLFree( pszFormatList );

/* -------------------------------------------------------------------- */
/*      Apply the format items to subfields.                            */
/* -------------------------------------------------------------------- */
    int iFormatItem;
    
    for( iFormatItem = 0;
         papszFormatItems[iFormatItem] != NULL;
         iFormatItem++ )
    {
        const char      *pszPastPrefix;

        pszPastPrefix = papszFormatItems[iFormatItem];
        while( *pszPastPrefix >= '0' && *pszPastPrefix <= '9' )
            pszPastPrefix++;

        ///////////////////////////////////////////////////////////////
        // Did we get too many formats for the subfields created
        // by names?  This may be legal by the 8211 specification, but
        // isn't encountered in any formats we care about so we just
        // blow.
        
        if( iFormatItem >= nSubfieldCount )
        {
            CPLError( CE_Warning, CPLE_DiscardedFormat,
                      "Got more formats than subfields for field `%s'.",
                      pszTag );
            break;
        }
        
        if( !papoSubfields[iFormatItem]->SetFormat(pszPastPrefix) )
            return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Verify that we got enough formats, cleanup and return.          */
/* -------------------------------------------------------------------- */
    CSLDestroy( papszFormatItems );

    if( iFormatItem < nSubfieldCount )
    {
        CPLError( CE_Warning, CPLE_DiscardedFormat,
                  "Got less formats than subfields for field `%s'.",
                  pszTag );
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      If all the fields are fixed width, then we are fixed width      */
/*      too.  This is important for repeating fields.                   */
/* -------------------------------------------------------------------- */
    nFixedWidth = 0;
    for( int i = 0; i < nSubfieldCount; i++ )
    {
        if( papoSubfields[i]->GetWidth() == 0 )
        {
            nFixedWidth = 0;
            break;
        }
        else
            nFixedWidth += papoSubfields[i]->GetWidth();
    }

    return TRUE;
}

/************************************************************************/
/*                          FindSubfieldDefn()                          */
/************************************************************************/

/**
 * Find a subfield definition by it's mnemonic tag.  
 *
 * @param pszMnemonic The name of the field.
 *
 * @return The subfield pointer, or NULL if there isn't any such subfield.
 */
 

DDFSubfieldDefn *DDFFieldDefn::FindSubfieldDefn( const char * pszMnemonic )

{
    for( int i = 0; i < nSubfieldCount; i++ )
    {
        if( EQUAL(papoSubfields[i]->GetName(),pszMnemonic) )
            return papoSubfields[i];
    }

    return NULL;
}

/************************************************************************/
/*                            GetSubfield()                             */
/*                                                                      */
/*      Fetch a subfield by it's index.                                 */
/************************************************************************/

/**
 * Fetch a subfield by index.
 *
 * @param i The index subfield index. (Between 0 and GetSubfieldCount()-1)
 *
 * @return The subfield pointer, or NULL if the index is out of range.
 */

DDFSubfieldDefn *DDFFieldDefn::GetSubfield( int i )

{
    if( i < 0 || i >= nSubfieldCount )
    {
        CPLAssert( FALSE );
        return NULL;
    }
             
    return papoSubfields[i];
}

/************************************************************************/
/*                          GetDefaultValue()                           */
/************************************************************************/

/**
 * Return default data for field instance.
 */

char *DDFFieldDefn::GetDefaultValue( int *pnSize )

{                                                                       
/* -------------------------------------------------------------------- */
/*      Loop once collecting the sum of the subfield lengths.           */
/* -------------------------------------------------------------------- */
    int iSubfield;
    int nTotalSize = 0;

    for( iSubfield = 0; iSubfield < nSubfieldCount; iSubfield++ )
    {
        int nSubfieldSize;

        if( !papoSubfields[iSubfield]->GetDefaultValue( NULL, 0, 
                                                        &nSubfieldSize ) )
            return NULL;
        nTotalSize += nSubfieldSize;
    }

/* -------------------------------------------------------------------- */
/*      Allocate buffer.                                                */
/* -------------------------------------------------------------------- */
    char *pachData = (char *) CPLMalloc( nTotalSize );

    if( pnSize != NULL )
        *pnSize = nTotalSize;

/* -------------------------------------------------------------------- */
/*      Loop again, collecting actual default values.                   */
/* -------------------------------------------------------------------- */
    int nOffset = 0;
    for( iSubfield = 0; iSubfield < nSubfieldCount; iSubfield++ )
    {
        int nSubfieldSize;

        if( !papoSubfields[iSubfield]->GetDefaultValue( 
                pachData + nOffset, nTotalSize - nOffset, &nSubfieldSize ) )
        {
            CPLAssert( FALSE );
            return NULL;
        }

        nOffset += nSubfieldSize;
    }

    CPLAssert( nOffset == nTotalSize );

    return pachData;
}
