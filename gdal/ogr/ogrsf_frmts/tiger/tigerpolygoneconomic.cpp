/******************************************************************************
 *
 * Project:  TIGER/Line Translator
 * Purpose:  Implements TigerPolygonEconomic, providing access to .RTE files.
 * Author:   Mark Phillips, mbp@geomtech.com
 *
 ******************************************************************************
 * Copyright (c) 2002, Frank Warmerdam, Mark Phillips
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

#include "ogr_tiger.h"
#include "cpl_conv.h"

CPL_CVSID("$Id$")

static const char FILE_CODE[] = "E";

/* I think this was the expected RTE format, but was never deployed, leaving
   it in the code in case I am missing something.

static TigerFieldInfo rtE_fields[] = {
  // fieldname    fmt  type OFTType      beg  end  len  bDefine bSet bWrite
  { "MODULE",     ' ', ' ', OFTString,     0,   0,   8,       1,   0,     0 },
  { "FILE",       'L', 'N', OFTInteger,    6,  10,   5,       1,   1,     1 },
  { "CENID",      'L', 'A', OFTString,    11,  15,   5,       1,   1,     1 },
  { "POLYID",     'R', 'N', OFTInteger,   16,  25,  10,       1,   1,     1 },
  { "STATEEC",    'L', 'N', OFTInteger,   26,  27,   2,       1,   1,     1 },
  { "COUNTYEC",   'L', 'N', OFTInteger,   28,  30,   3,       1,   1,     1 },
  { "CONCITEC",   'L', 'N', OFTInteger,   31,  35,   5,       1,   1,     1 },
  { "COUSUBEC",   'L', 'N', OFTInteger,   36,  40,   5,       1,   1,     1 },
  { "PLACEEC",    'L', 'N', OFTInteger,   41,  45,   5,       1,   1,     1 },
  { "AIANHHFPEC", 'L', 'N', OFTInteger,   46,  50,   5,       1,   1,     1 },
  { "AIANHHEC",   'L', 'N', OFTInteger,   51,  54,   4,       1,   1,     1 },
  { "AIAHHTLIEC", 'L', 'A', OFTString,    55,  55,   1,       1,   1,     1 },
  { "RS_E1",      'L', 'A', OFTString,    56,  73,  18,       1,   1,     1 }
};
*/

static const TigerFieldInfo rtE_fields[] = {
  // fieldname    fmt  type OFTType      beg  end  len  bDefine bSet bWrite
  { "MODULE",     ' ', ' ', OFTString,     0,   0,   8,       1,   0,     0 },
  { "FILE",       'L', 'N', OFTInteger,    6,  10,   5,       1,   1,     1 },
  { "CENID",      'L', 'A', OFTString,    11,  15,   5,       1,   1,     1 },
  { "POLYID",     'R', 'N', OFTInteger,   16,  25,  10,       1,   1,     1 },
  { "STATEEC",    'L', 'N', OFTInteger,   26,  27,   2,       1,   1,     1 },
  { "COUNTYEC",   'L', 'N', OFTInteger,   28,  30,   3,       1,   1,     1 },
  { "RS_E1",      'L', 'A', OFTString,    31,  35,   5,       1,   1,     1 },
  { "RS_E2",      'L', 'A', OFTString,    36,  40,   5,       1,   1,     1 },
  { "PLACEEC",    'L', 'N', OFTInteger,   41,  45,   5,       1,   1,     1 },
  { "RS-E3",      'L', 'A', OFTString,    46,  50,   5,       1,   1,     1 },
  { "RS-E4",      'L', 'A', OFTString,    51,  54,   4,       1,   1,     1 },
  { "RS-E5",      'L', 'A', OFTString,    55,  55,   1,       1,   1,     1 },
  { "COMMREGEC",  'L', 'N', OFTInteger,   56,  56,   1,       1,   1,     1 },
  { "RS_E6",      'L', 'A', OFTString,    57,  73,  17,       1,   1,     1 }
};
static const TigerRecordInfo rtE_info =
  {
    rtE_fields,
    sizeof(rtE_fields) / sizeof(TigerFieldInfo),
    73
  };

/************************************************************************/
/*                           TigerPolygonEconomic()                           */
/************************************************************************/

TigerPolygonEconomic::TigerPolygonEconomic( OGRTigerDataSource * poDSIn,
                                            CPL_UNUSED const char * pszPrototypeModule ) :
    TigerFileBase(&rtE_info, FILE_CODE)
{
    poDS = poDSIn;
    poFeatureDefn = new OGRFeatureDefn( "PolygonEconomic" );
    poFeatureDefn->Reference();
    poFeatureDefn->SetGeomType( wkbNone );

    /* -------------------------------------------------------------------- */
    /*      Fields from type E record.                                      */
    /* -------------------------------------------------------------------- */

    AddFieldDefns( psRTInfo, poFeatureDefn );
}
