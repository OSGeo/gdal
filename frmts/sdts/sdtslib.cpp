/******************************************************************************
 * $Id$
 *
 * Project:  SDTS Translator
 * Purpose:  Various utility functions that apply to all SDTS profiles.
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
 * Revision 1.1  1999/03/23 13:56:13  warmerda
 * New
 *
 */

#include "sdts_al.h"

#include "io/sio_Reader.h"
#include "io/sio_8211Converter.h"

/************************************************************************/
/*                      scal_Record::getSubfield()                      */
/*                                                                      */
/*      Fetch a subfield by name.                                       */
/************************************************************************/

sc_Subfield *scal_Record::getSubfield( const string osFieldName, int iField,
                                       const string osSubfieldName,
                                       				int iSubfield)

{
    sc_Record::const_iterator	oFieldIter;
    int				nFieldHitCounter = 0;

    for( oFieldIter = this->begin();
         oFieldIter != this->end();
         ++oFieldIter )
    {
        const sc_Field	&oField = *oFieldIter;

        if( (osFieldName == "" || osFieldName == oField.getMnemonic())
            && nFieldHitCounter++ == iField )
        {
            sc_Field::const_iterator  oSFIter;
            int			      nSFHitCounter = 0;

            for( oSFIter = oField.begin(); oSFIter != oField.end(); ++oSFIter )
            {
                const sc_Subfield	&oSubfield = *oSFIter;

                if( (osSubfieldName == ""
                       || osSubfieldName == oSubfield.getMnemonic())
                    && nSFHitCounter++ == iSubfield )
                {
                    return( (sc_Subfield *) &oSubfield );
                }                
            }
        }
    }

    return NULL;
}

/************************************************************************/
/*                       SDTSGetSubfieldOfField()                       */
/************************************************************************/

sc_Subfield *SDTSGetSubfieldOfField( const sc_Field * poField,
                                     const string osSubfieldName,
                                     int iSubfield )

{
    sc_Field::const_iterator  oSFIter;
    int			      nSFHitCounter = 0;

    for( oSFIter = poField->begin(); oSFIter != poField->end(); ++oSFIter )
    {
        const sc_Subfield	&oSubfield = *oSFIter;

        if( (osSubfieldName == ""
             || osSubfieldName == oSubfield.getMnemonic())
            && nSFHitCounter++ == iSubfield )
        {
            return( (sc_Subfield *) &oSubfield );
        }                
    }

    return NULL;
}

/************************************************************************/
/*                           SDTSModId::Set()                           */
/*                                                                      */
/*      Set a module from a field.                                      */
/************************************************************************/

int SDTSModId::Set( const sc_Field * poField )

{
    sc_Field::const_iterator  oSFIter;

    for( oSFIter = poField->begin(); oSFIter != poField->end(); ++oSFIter )
    {
        const sc_Subfield	&oSubfield = *oSFIter;

        if( oSubfield.getMnemonic() == "MODN" )
        {
            string	osTemp;

            oSubfield.getA( osTemp );
            strcpy( szModule, osTemp.c_str() );
        }
        else if( oSubfield.getMnemonic() == "RCID" )
        {
            oSubfield.getI( nRecord );
            return( szModule[0] != '\0' );
        }
    }

    return FALSE;
}

/************************************************************************/
/*                            SDTSGetSADR()                             */
/*                                                                      */
/*      Extract the contents of a Spatial Address field.  Eventually    */
/*      this code should also apply the scaling according to the        */
/*      internal reference file ... it will have to be passed in        */
/*      then.                                                           */
/************************************************************************/

int SDTSGetSADR( SDTS_IREF *poIREF,  const sc_Field * poField,
                 double *pdfX, double * pdfY, double * pdfZ )

{
    sc_Field::const_iterator  oSFIter;

    *pdfX = *pdfY = *pdfZ = 0;

    for( oSFIter = poField->begin(); oSFIter != poField->end(); ++oSFIter )
    {
        const sc_Subfield	&oSubfield = *oSFIter;

        if( oSubfield.getMnemonic() == "X" )
        {
            long		x;
            
            oSubfield.getBI32( x );
            *pdfX = x * poIREF->dfXScale;
        }
        else if( oSubfield.getMnemonic() == "Y" )
        {
            long		y;
            
            oSubfield.getBI32( y );
            *pdfY = y * poIREF->dfYScale;
        }
        else if( oSubfield.getMnemonic() == "Z" )
        {
            long		z;
            
            oSubfield.getBI32( z );
            *pdfZ = z; /* we should add vertical scaling */
        }
        else
        {
            assert( FALSE );
        }
    }

    return TRUE;
}
