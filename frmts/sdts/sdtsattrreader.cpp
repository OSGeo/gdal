/******************************************************************************
 * $Id$
 *
 * Project:  SDTS Translator
 * Purpose:  Implementation of SDTSAttrReader and SDTSAttrRecord classes.
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
 * Revision 1.2  1999/03/23 15:59:40  warmerda
 * implemented and working
 *
 */

#include "sdts_al.h"

#include <iostream>
#include <fstream>
#include "io/sio_Reader.h"
#include "io/sio_8211Converter.h"

/************************************************************************/
/* ==================================================================== */
/*			      SDTSAttrRecord				*/
/*									*/
/*	Note that virtually all the work on objects of this class	*/
/*	is done by the SDTSAttrReader.					*/
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                           SDTSAttrRecord()                           */
/************************************************************************/

SDTSAttrRecord::SDTSAttrRecord()

{
    poATTP = NULL;
}

/************************************************************************/
/*                          ~STDSAttrRecord()                           */
/************************************************************************/

SDTSAttrRecord::~SDTSAttrRecord()

{
}


/************************************************************************/
/* ==================================================================== */
/*			       SDTSAttrReader				*/
/*									*/
/*	This is the class used to read a primary attribute module.      */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                           SDTSAttrReader()                           */
/************************************************************************/

SDTSAttrReader::SDTSAttrReader( SDTS_IREF * poIREFIn )

{
    po8211Reader = NULL;
    poIter = NULL;
    poIREF = poIREFIn;
}

/************************************************************************/
/*                          ~SDTSAttrReader()                           */
/************************************************************************/

SDTSAttrReader::~SDTSAttrReader()
{
    Close();
}

/************************************************************************/
/*                               Close()                                */
/************************************************************************/

void SDTSAttrReader::Close()

{
    if( poIter != NULL )
    {
        delete poIter;
        poIter = NULL;
    }

    if( po8211Reader != NULL )
    {
        delete po8211Reader;
        po8211Reader = NULL;
    }

    if( ifs )
        ifs.close();
}

/************************************************************************/
/*                                Open()                                */
/*                                                                      */
/*      Open the requested attr file, and prepare to start reading      */
/*      data records.                                                   */
/************************************************************************/

int SDTSAttrReader::Open( string osFilename )

{
/* -------------------------------------------------------------------- */
/*      Open the file.                                                  */
/* -------------------------------------------------------------------- */
    ifs.open( osFilename.c_str() );
    if( !ifs )
    {
        printf( "Unable to open `%s'\n", osFilename.c_str() );
        return FALSE;
    }
    
/* -------------------------------------------------------------------- */
/*      Establish reader access to the file				*/
/* -------------------------------------------------------------------- */
    po8211Reader = new sio_8211Reader( ifs, NULL );
    poIter = new sio_8211ForwardIterator( *po8211Reader );
    
    if( !(*poIter) )
        return FALSE;
    else
        return TRUE;
}

/************************************************************************/
/*                           GetNextRecord()                            */
/*                                                                      */
/*      Fetch the record as an STDSAttrRecord.                          */
/************************************************************************/

SDTSAttrRecord * SDTSAttrReader::GetNextRecord()

{
    SDTSAttrRecord	*poSR;
    
/* -------------------------------------------------------------------- */
/*      Are we initialized?                                             */
/* -------------------------------------------------------------------- */
    if( poIter == NULL )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Is the record iterator at the end of the file?                  */
/* -------------------------------------------------------------------- */
    if( ! *poIter )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Read records till we find one with an ATTP section.             */
/*      Normally every record will have one.                            */
/* -------------------------------------------------------------------- */
    poSR = new SDTSAttrRecord;

    while( poSR->poATTP == NULL && *poIter )
    {
        sc_Record::const_iterator	oFieldIter;

        poIter->get( poSR->oRecord );
        ++(*poIter);

        for( oFieldIter = poSR->oRecord.begin();
             oFieldIter != poSR->oRecord.end();
             ++oFieldIter )
        {
            const sc_Field	&oField = *oFieldIter;

            if( oField.getMnemonic() == "ATTP" )
            {
                poSR->poATTP = &oField;
            }
            else if( oField.getMnemonic() == "ATPR" )
            {
                poSR->oRecordId.Set( &oField );
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Return the result, or NULL if we never found a valid record.    */
/* -------------------------------------------------------------------- */
    if( poSR->poATTP == NULL )
    {
        delete poSR;
        return NULL;
    }
    else
        return poSR;
}
