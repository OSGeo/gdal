/******************************************************************************
 * $Id$
 *
 * Project:  ISO 8211 Access
 * Purpose:  Implements the DDFFieldDefn class.
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
 * Revision 1.3  1999/11/18 19:03:04  warmerda
 * expanded tabs
 *
 * Revision 1.2  1999/09/20 19:29:16  warmerda
 * make forgiving of UNIT/FIELD terminator mixup in Tiger SDTS files
 *
 * Revision 1.1  1999/04/27 18:45:05  warmerda
 * New
 *
 */

#include "iso8211.h"
#include "cpl_string.h"

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
    paoSubfields = NULL;
    bRepeatingSubfields = FALSE;
}

/************************************************************************/
/*                           ~DDFFieldDefn()                            */
/************************************************************************/

DDFFieldDefn::~DDFFieldDefn()

{
    CPLFree( pszTag );
    CPLFree( _fieldName );
    CPLFree( _arrayDescr );
    CPLFree( _formatControls );

    delete[] paoSubfields;
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
      case '0':
        _data_struct_code = elementary;
        break;

      case '1':
        _data_struct_code = vector;
        break;

      case '2':
        _data_struct_code = array;
        break;

      case '3':
        _data_struct_code = concatenated;
        break;

      default:
        CPLAssert( FALSE );
        _data_struct_code = elementary;
    }

    switch( pachFieldArea[1] )
    {
      case '0':
        _data_type_code = char_string;
        break;
        
      case '1':
        _data_type_code = implicit_point;
        break;
        
      case '2':
        _data_type_code = explicit_point;
        break;
        
      case '3':
        _data_type_code = explicit_point_scaled;
        break;
        
      case '4':
        _data_type_code = char_bit_string;
        break;
        
      case '5':
        _data_type_code = bit_string;
        break;
        
      case '6':
        _data_type_code = mixed_data_type;
        break;

      default:
        CPLAssert( FALSE );
        _data_type_code = char_string;
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
    if( _data_struct_code != elementary )
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
      case elementary:
        pszValue = "elementary";
        break;
        
      case vector:
        pszValue = "vector";
        break;
        
      case array:
        pszValue = "array";
        break;
        
      case concatenated:
        pszValue = "concatenated";
        break;
        
      default:
        CPLAssert( FALSE );
        pszValue = "(unknown)";
    }

    fprintf( fp, "      _data_struct_code = %s\n", pszValue );

    switch( _data_type_code )
    {
      case char_string:
        pszValue = "char_string";
        break;
        
      case implicit_point:
        pszValue = "implicit_point";
        break;
        
      case explicit_point:
        pszValue = "explicit_point";
        break;
        
      case explicit_point_scaled:
        pszValue = "explicit_point_scaled";
        break;
        
      case char_bit_string:
        pszValue = "char_bit_string";
        break;
        
      case bit_string:
        pszValue = "bit_string";
        break;
        
      case mixed_data_type:
        pszValue = "mixed_data_type";
        break;

      default:
        CPLAssert( FALSE );
        pszValue = "(unknown)";
        break;
    }
    
    fprintf( fp, "      _data_type_code = %s\n", pszValue );

    for( int i = 0; i < nSubfieldCount; i++ )
    {
        paoSubfields[i].Dump( fp );
    }
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

    if( pszSublist[0] == '*' )
    {
        bRepeatingSubfields = TRUE;
        pszSublist++;
    }

    papszSubfieldNames = CSLTokenizeStringComplex( pszSublist, "!",
                                                   FALSE, FALSE );

    nSubfieldCount = CSLCount( papszSubfieldNames );
    paoSubfields = new DDFSubfieldDefn[nSubfieldCount];
    
    for( int iSF = 0; iSF < nSubfieldCount; iSF++ )
    {
        paoSubfields[iSF].SetName( papszSubfieldNames[iSF] );
    }

    CSLDestroy( papszSubfieldNames );

    return TRUE;
}

/************************************************************************/
/*                         FixupExtraBrackets()                         */
/*                                                                      */
/*      As per note b) of 6.4.3.3 of the standard, sequences of bit     */
/*      string fields should be included in an extra layer of           */
/*      brackets to indicate that rescaning the format list should      */
/*      start on a byte boundary.                                       */
/*                                                                      */
/*      Since we don't intend to support bitstrings of other than       */
/*      multiples of eight bits at a time, this is not relavent to      */
/*      us.  However, we do need to strip the extra brackets away to    */
/*      avoid screwing up interpretation of the formats.  This          */
/*      function will strip a preceeding open bracket, and/or an        */
/*      _extra_ bracket at the end of a token.                          */
/*                                                                      */
/*      For instance the format string ((B(32),B(32))) would first      */
/*      be stripped down to (B(32),B(32)) by ApplyFormats(), then       */
/*      split into "(B(32)" and "B(32))" by the tokenize function.      */
/*      This function FixupExtraBrackets) would then doctor each of     */
/*      these tokens into "B(32)".   Note that end brackets that        */
/*      match open brackets (other than the first character) are not    */
/*      stripped off.                                                   */
/************************************************************************/

static void FixupExtraBrackets( char * pszToken )

{
/* -------------------------------------------------------------------- */
/*      If the first character is an open bracket, move all             */
/*      characters down one.                                            */
/* -------------------------------------------------------------------- */
    if( pszToken[0] == '(' )
    {
        for( int i = 0; pszToken[i] != '\0'; i++ )
        {
            pszToken[i] = pszToken[i+1];
        }
    }

/* -------------------------------------------------------------------- */
/*      Count open and close brackets.                                  */
/* -------------------------------------------------------------------- */
    int         nOpenCount=0, nCloseCount=0;

    for( int i = 0; pszToken[i] != '\0'; i++ )
    {
        if( pszToken[i] == '(' )
            nOpenCount++;

        if( pszToken[i] == ')' )
            nCloseCount++;
    }

/* -------------------------------------------------------------------- */
/*      Strip off an extra trailing close bracket.                      */
/* -------------------------------------------------------------------- */
    if( nOpenCount < nCloseCount && pszToken[strlen(pszToken)-1] == ')' )
        pszToken[strlen(pszToken)-1] = '\0';
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
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Format controls for `%s' field missing brackets:%s\n",
                  pszTag, _formatControls );
        
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Duplicate the string, and strip off the brackets.               */
/* -------------------------------------------------------------------- */
    pszFormatList = CPLStrdup( _formatControls+1 );
    pszFormatList[strlen(pszFormatList)-1] = '\0';

/* -------------------------------------------------------------------- */
/*      Tokenize based on commas.                                       */
/* -------------------------------------------------------------------- */
    papszFormatItems =
        CSLTokenizeStringComplex(pszFormatList, ",", FALSE, FALSE );

    CPLFree( pszFormatList );

/* -------------------------------------------------------------------- */
/*      Loop over format items, parsing out the prefix number (if       */
/*      any), and then applying the format to the indicated number      */
/*      of subfields.  For instance a format like "3A" would result     */
/*      in the format "A" being applied to three subfields.             */
/* -------------------------------------------------------------------- */
    int         iNextSubfield = 0;

    for( int iFormatItem = 0;
         papszFormatItems[iFormatItem] != NULL;
         iFormatItem++ )
    {
        const char      *pszPastPrefix;

        FixupExtraBrackets( papszFormatItems[iFormatItem] );

        pszPastPrefix = papszFormatItems[iFormatItem];
        while( *pszPastPrefix >= '0' && *pszPastPrefix <= '9' )
            pszPastPrefix++;

        int     nRepeatCount = atoi(papszFormatItems[iFormatItem]);

        if( nRepeatCount == 0 )
            nRepeatCount = 1;

        while( nRepeatCount > 0 )
        {
            ///////////////////////////////////////////////////////////////
            // Did we get too many formats for the subfields created
            // by names?  This may be legal by the 8211 specification, but
            // isn't encountered in any formats we care about so we just
            // blow.
            
            if( iNextSubfield >= nSubfieldCount )
            {
                CPLError( CE_Failure, CPLE_AppDefined,
                          "Got more formats than subfields for field `%s'.\n",
                          pszTag );
                return FALSE;
            }

            if( !paoSubfields[iNextSubfield].SetFormat(pszPastPrefix) )
                return FALSE;
            
            iNextSubfield++;
            nRepeatCount--;
        }
    }

/* -------------------------------------------------------------------- */
/*      Verify that we got enough formats, cleanup and return.          */
/* -------------------------------------------------------------------- */
    CSLDestroy( papszFormatItems );

    if( iNextSubfield < nSubfieldCount )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Got less formats than subfields for field `%s',\n",
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
        if( paoSubfields[i].GetWidth() == 0 )
        {
            nFixedWidth = 0;
            break;
        }
        else
            nFixedWidth += paoSubfields[i].GetWidth();
    }

    return TRUE;
}

/************************************************************************/
/*                          FindSubfieldDefn()                          */
/************************************************************************/

/**
 * Find a subfield definition by it's mnemonic tag.  
 *
 * @param The name of the field.
 *
 * @return The subfield pointer, or NULL if there isn't any such subfield.
 */
 

DDFSubfieldDefn *DDFFieldDefn::FindSubfieldDefn( const char * pszMnemonic )

{
    for( int i = 0; i < nSubfieldCount; i++ )
    {
        if( EQUAL(paoSubfields[i].GetName(),pszMnemonic) )
            return paoSubfields + i;
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
 * @param The index subfield index. (Between 0 and GetSubfieldCount()-1)
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
             
    return paoSubfields + i;
}
