/******************************************************************************
 * $Id$
 *
 * Project:  SDTS Translator
 * Purpose:  Implementation of SDTSPolygonReader and SDTSRawPolygon classes.
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
 * Revision 1.1  1999/05/11 14:04:42  warmerda
 * New
 *
 */

#include "sdts_al.h"

/************************************************************************/
/* ==================================================================== */
/*			      SDTSRawPolygon				*/
/*									*/
/*	This is a simple class for holding the data related with a 	*/
/*	polygon feature.						*/
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                           SDTSRawPolygon()                           */
/************************************************************************/

SDTSRawPolygon::SDTSRawPolygon()

{
    nAttributes = 0;
}

/************************************************************************/
/*                                Read()                                */
/*                                                                      */
/*      Read a record from the passed SDTSPolygonReader, and assign the */
/*      values from that record to this object.  This is the bulk of    */
/*      the work in this whole file.                                    */
/************************************************************************/

int SDTSRawPolygon::Read( DDFRecord * poRecord )

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

        if( EQUAL(pszFieldName,"POLY") )
        {
            oPolyId.Set( poField );
        }

        else if( EQUAL(pszFieldName,"ATID") )
        {
            if( nAttributes < MAX_RAWPOLYGON_ATID )
            {
                aoATID[nAttributes].Set( poField );
                nAttributes++;
            }
        }
    }

    return TRUE;
}

/************************************************************************/
/* ==================================================================== */
/*			       SDTSPolygonReader			*/
/*									*/
/*	This is the class used to read a Polygon module.		*/
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                           SDTSPolygonReader()                          */
/************************************************************************/

SDTSPolygonReader::SDTSPolygonReader()

{
}

/************************************************************************/
/*                             ~SDTSPolygonReader()                     */
/************************************************************************/

SDTSPolygonReader::~SDTSPolygonReader()
{
}

/************************************************************************/
/*                               Close()                                */
/************************************************************************/

void SDTSPolygonReader::Close()

{
    oDDFModule.Close();
}

/************************************************************************/
/*                                Open()                                */
/*                                                                      */
/*      Open the requested line file, and prepare to start reading      */
/*      data records.                                                   */
/************************************************************************/

int SDTSPolygonReader::Open( const char * pszFilename )

{
    return( oDDFModule.Open( pszFilename ) );
}

/************************************************************************/
/*                            GetNextPolygon()                          */
/*                                                                      */
/*      Fetch the next feature as an STDSRawPolygon.                    */
/************************************************************************/

SDTSRawPolygon * SDTSPolygonReader::GetNextPolygon()

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
/*      Transform into a Polygon feature.                                 */
/* -------------------------------------------------------------------- */
    SDTSRawPolygon	*poRawPolygon = new SDTSRawPolygon();

    if( poRawPolygon->Read( poRecord ) )
    {
        return( poRawPolygon );
    }
    else
    {
        delete poRawPolygon;
        return NULL;
    }
}
