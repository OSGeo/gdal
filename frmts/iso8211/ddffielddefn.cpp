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
 * Revision 1.9  2003/02/06 03:21:04  warmerda
 * Modified ExpandFormat() to dynamically allocate the target buffer. It was
 * overrunning the 400 character szDest buffer on some files, such as
 * the data/sdts/gainsville/BEDRCATD.DDF dataset.
 *
 * Revision 1.8  2001/07/18 04:51:57  warmerda
 * added CPL_CVSID
 *
 * Revision 1.7  2001/06/22 19:22:16  warmerda
 * Made some oddidies in field definitions non-fatal.
 *
 * Revision 1.6  2000/11/30 20:33:18  warmerda
 * make having more formats than data items a warning, not an error
 *
 * Revision 1.5  2000/06/16 18:05:02  warmerda
 * expanded tabs
 *
 * Revision 1.4  2000/01/31 18:03:38  warmerda
 * completely rewrote format expansion to make more general
 *
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
#include <ctype.h>

CPL_CVSID("$Id$");

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
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Unrecognised data_struct_code value %c.\n"
                  "Field %s initialization incorrect.\n",
                  pachFieldArea[0], pszTag );
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
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Unrecognised data_type_code value %c.\n"
                  "Field %s initialization incorrect.\n",
                  pachFieldArea[1], pszTag );
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

            if( (int) (strlen(pszExpandedContents) + strlen(pszDest))
                > nDestMax )
            {
                nDestMax = 2 * (strlen(pszExpandedContents) + strlen(pszDest));
                pszDest = (char *) CPLRealloc(pszDest,nDestMax);
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
                if( (int) (strlen(pszExpandedContents) + strlen(pszDest))
                    > nDestMax )
                {
                    nDestMax = 
                        2 * (strlen(pszExpandedContents) + strlen(pszDest));
                    pszDest = (char *) CPLRealloc(pszDest,nDestMax);
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
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Format controls for `%s' field missing brackets:%s\n",
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
            CPLError( CE_Warning, CPLE_AppDefined,
                      "Got more formats than subfields for field `%s'.\n",
                      pszTag );
            break;
        }
        
        if( !paoSubfields[iFormatItem].SetFormat(pszPastPrefix) )
            return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Verify that we got enough formats, cleanup and return.          */
/* -------------------------------------------------------------------- */
    CSLDestroy( papszFormatItems );

    if( iFormatItem < nSubfieldCount )
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
