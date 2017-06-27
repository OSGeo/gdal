/******************************************************************************
 *
 * Project:  SDTS Translator
 * Purpose:  Implementation of SDTSAttrReader class.
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

#include "sdts_al.h"

CPL_CVSID("$Id$")

/************************************************************************/
/* ==================================================================== */
/*                             SDTSAttrRecord                           */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                           SDTSAttrRecord()                           */
/************************************************************************/

SDTSAttrRecord::SDTSAttrRecord() :
    poWholeRecord(NULL),
    poATTR(NULL)
{}

/************************************************************************/
/*                          ~SDTSAttrRecord()                           */
/************************************************************************/

SDTSAttrRecord::~SDTSAttrRecord()

{
    if( poWholeRecord != NULL )
        delete poWholeRecord;
}

/************************************************************************/
/*                                Dump()                                */
/************************************************************************/

void SDTSAttrRecord::Dump( FILE * fp )

{
    if( poATTR != NULL )
        poATTR->Dump( fp );
}

/************************************************************************/
/* ==================================================================== */
/*                             SDTSAttrReader                           */
/*                                                                      */
/*      This is the class used to read a primary attribute module.      */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                           SDTSAttrReader()                           */
/************************************************************************/

SDTSAttrReader::SDTSAttrReader() :
    bIsSecondary(FALSE)
{}

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
    ClearIndex();
    oDDFModule.Close();
}

/************************************************************************/
/*                                Open()                                */
/*                                                                      */
/*      Open the requested attr file, and prepare to start reading      */
/*      data records.                                                   */
/************************************************************************/

int SDTSAttrReader::Open( const char *pszFilename )

{
    bool bSuccess = CPL_TO_BOOL(oDDFModule.Open( pszFilename ));

    if( bSuccess )
        bIsSecondary = (oDDFModule.FindFieldDefn("ATTS") != NULL);

    return bSuccess;
}

/************************************************************************/
/*                           GetNextRecord()                            */
/************************************************************************/

DDFField *SDTSAttrReader::GetNextRecord( SDTSModId * poModId,
                                         DDFRecord ** ppoRecord,
                                         int bDuplicate )

{
/* -------------------------------------------------------------------- */
/*      Fetch a record.                                                 */
/* -------------------------------------------------------------------- */
    if( ppoRecord != NULL )
        *ppoRecord = NULL;

    if( oDDFModule.GetFP() == NULL )
        return NULL;

    DDFRecord *poRecord = oDDFModule.ReadRecord();

    if( poRecord == NULL )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Make a copy of the record for persistent use if requested by    */
/*      the caller.                                                     */
/* -------------------------------------------------------------------- */
    if( bDuplicate )
        poRecord = poRecord->Clone();

/* -------------------------------------------------------------------- */
/*      Find the ATTP field.                                            */
/* -------------------------------------------------------------------- */
    DDFField *poATTP = poRecord->FindField( "ATTP", 0 );
    if( poATTP == NULL )
    {
        poATTP = poRecord->FindField( "ATTS", 0 );
    }

    if( poATTP == NULL )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Update the module ID if required.                               */
/* -------------------------------------------------------------------- */
    if( poModId != NULL )
    {
        DDFField *poATPR = poRecord->FindField( "ATPR" );

        if( poATPR == NULL )
            poATPR = poRecord->FindField( "ATSC" );

        if( poATPR != NULL )
            poModId->Set( poATPR );
    }

/* -------------------------------------------------------------------- */
/*      return proper answer.                                           */
/* -------------------------------------------------------------------- */
    if( ppoRecord != NULL )
        *ppoRecord = poRecord;

    return poATTP;
}

/************************************************************************/
/*                         GetNextAttrRecord()                          */
/************************************************************************/

SDTSAttrRecord *SDTSAttrReader::GetNextAttrRecord()

{
    SDTSModId   oModId;
    DDFRecord   *poRawRecord = NULL;

    DDFField *poATTRField = GetNextRecord( &oModId, &poRawRecord, TRUE );

    if( poATTRField == NULL )
        return NULL;

    SDTSAttrRecord *poAttrRecord = new SDTSAttrRecord();

    poAttrRecord->poWholeRecord = poRawRecord;
    poAttrRecord->poATTR = poATTRField;
    memcpy( &(poAttrRecord->oModId), &oModId, sizeof(SDTSModId) );

    return poAttrRecord;
}
