/******************************************************************************
 * $Id$
 *
 * Project:  SDTS Translator
 * Purpose:  Implementation of SDTSPointReader and SDTSRawPoint classes.
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
 * Revision 1.5  1999/09/02 03:40:03  warmerda
 * added indexed readers
 *
 * Revision 1.4  1999/08/10 02:52:13  warmerda
 * introduce use of SDTSApplyModIdList to capture multi-attributes
 *
 * Revision 1.3  1999/07/30 19:15:56  warmerda
 * added module reference counting
 *
 * Revision 1.2  1999/05/11 14:09:00  warmerda
 * fixed up to use PNTS for point record id
 *
 * Revision 1.1  1999/05/07 13:44:57  warmerda
 * New
 *
 */

#include "sdts_al.h"

/************************************************************************/
/* ==================================================================== */
/*			      SDTSRawPoint				*/
/*									*/
/*	This is a simple class for holding the data related with a 	*/
/*	point feature.							*/
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                            SDTSRawPoint()                            */
/************************************************************************/

SDTSRawPoint::SDTSRawPoint()

{
    nAttributes = 0;
}

/************************************************************************/
/*                           ~STDSRawPoint()                            */
/************************************************************************/

SDTSRawPoint::~SDTSRawPoint()

{
}

/************************************************************************/
/*                                Read()                                */
/*                                                                      */
/*      Read a record from the passed SDTSPointReader, and assign the   */
/*      values from that record to this point.  This is the bulk of     */
/*      the work in this whole file.                                    */
/************************************************************************/

int SDTSRawPoint::Read( SDTS_IREF * poIREF, DDFRecord * poRecord )

{
/* ==================================================================== */
/*      Loop over fields in this record, looking for those we           */
/*      recognise, and need.						*/
/* ==================================================================== */
    for( int iField = 0; iField < poRecord->GetFieldCount(); iField++ )
    {
        DDFField	*poField = poRecord->GetField( iField );
        const char	*pszFieldName;

        CPLAssert( poField != NULL );
        pszFieldName = poField->GetFieldDefn()->GetName();

        if( EQUAL(pszFieldName,"PNTS") )
            oModId.Set( poField );

        else if( EQUAL(pszFieldName,"ATID") )
            ApplyATID( poField );

        else if( EQUAL(pszFieldName,"ARID") )
        {
            oAreaId.Set( poField );
        }
        else if( EQUAL(pszFieldName,"SADR") )
        {
            SDTSGetSADR( poIREF, poField, 1, &dfX, &dfY, &dfZ );
        }
    }

    return TRUE;
}

/************************************************************************/
/* ==================================================================== */
/*			       SDTSPointReader				*/
/*									*/
/*	This is the class used to read a point module.			*/
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                           SDTSPointReader()                          */
/************************************************************************/

SDTSPointReader::SDTSPointReader( SDTS_IREF * poIREFIn )

{
    poIREF = poIREFIn;
}

/************************************************************************/
/*                             ~SDTSLineReader()                        */
/************************************************************************/

SDTSPointReader::~SDTSPointReader()
{
}

/************************************************************************/
/*                               Close()                                */
/************************************************************************/

void SDTSPointReader::Close()

{
    oDDFModule.Close();
}

/************************************************************************/
/*                                Open()                                */
/*                                                                      */
/*      Open the requested line file, and prepare to start reading      */
/*      data records.                                                   */
/************************************************************************/

int SDTSPointReader::Open( const char * pszFilename )

{
    return( oDDFModule.Open( pszFilename ) );
}

/************************************************************************/
/*                            GetNextPoint()                            */
/*                                                                      */
/*      Fetch the next feature as an STDSRawPoint.                      */
/************************************************************************/

SDTSRawPoint * SDTSPointReader::GetNextPoint()

{
    DDFRecord	*poRecord;
    
/* -------------------------------------------------------------------- */
/*      Read a record.                                                  */
/* -------------------------------------------------------------------- */
    if( oDDFModule.GetFP() == NULL )
        return NULL;

    poRecord = oDDFModule.ReadRecord();

    if( poRecord == NULL )
        return NULL;
    
/* -------------------------------------------------------------------- */
/*      Transform into a point feature.                                 */
/* -------------------------------------------------------------------- */
    SDTSRawPoint	*poRawPoint = new SDTSRawPoint();

    if( poRawPoint->Read( poIREF, poRecord ) )
    {
        return( poRawPoint );
    }
    else
    {
        delete poRawPoint;
        return NULL;
    }
}

