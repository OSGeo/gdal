/******************************************************************************
 *
 * Project:  TIGER/Line Translator
 * Purpose:  Implements TigerPolygonCorrections, providing access to .RTB files.
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

static const char FILE_CODE[] = "B";

static const TigerFieldInfo rtB_fields[] = {
  // fieldname    fmt  type OFTType      beg  end  len  bDefine bSet
  { "MODULE",     ' ', ' ', OFTString,     0,   0,   8,       1,   0 },
  { "FILE",       'L', 'N', OFTInteger,    6,  10,   5,       1,   1 },
  { "CENID",      'L', 'A', OFTString,    11,  15,   5,       1,   1 },
  { "POLYID",     'R', 'N', OFTInteger,   16,  25,  10,       1,   1 },
  { "STATECQ",    'L', 'N', OFTInteger,   26,  27,   2,       1,   1 },
  { "COUNTYCQ",   'L', 'N', OFTInteger,   28,  30,   3,       1,   1 },
  { "TRACTCQ",    'L', 'N', OFTInteger,   31,  36,   6,       1,   1 },
  { "BLOCKCQ",    'L', 'A', OFTString,    37,  41,   5,       1,   1 },
  { "AIANHHFPCQ", 'L', 'N', OFTInteger,   42,  46,   5,       1,   1 },
  { "AIANHHCQ",   'L', 'N', OFTInteger,   47,  50,   4,       1,   1 },
  { "AIHHTLICQ",  'L', 'A', OFTString,    51,  51,   1,       1,   1 },
  { "AITSCECQ",   'L', 'N', OFTInteger,   52,  54,   3,       1,   1 },
  { "AITSCQ",     'L', 'N', OFTInteger,   55,  59,   5,       1,   1 },
  { "ANRCCQ",     'L', 'N', OFTInteger,   60,  64,   5,       1,   1 },
  { "CONCITCQ",   'L', 'N', OFTInteger,   65,  69,   5,       1,   1 },
  { "COUSUBCQ",   'L', 'N', OFTInteger,   70,  74,   5,       1,   1 },
  { "SUBMCDCQ",   'L', 'N', OFTInteger,   75,  79,   5,       1,   1 },
  { "PLACECQ",    'L', 'N', OFTInteger,   80,  84,   5,       1,   1 },
  { "UACC",       'L', 'N', OFTInteger,   85,  89,   5,       1,   1 },
  { "URCC",       'L', 'A', OFTString,    90,  90,   1,       1,   1 },
  { "RS-B1",      'L', 'A', OFTString,    91,  98,  12,       1,   1 },
};
static const TigerRecordInfo rtB_info =
  {
    rtB_fields,
    sizeof(rtB_fields) / sizeof(TigerFieldInfo),
    98
  };

/************************************************************************/
/*                     TigerPolygonCorrections()                        */
/************************************************************************/

TigerPolygonCorrections::TigerPolygonCorrections(
    OGRTigerDataSource * poDSIn,
    const char * /* pszPrototypeModule */ ) :
    TigerFileBase(&rtB_info, FILE_CODE)
{
    OGRFieldDefn        oField("",OFTInteger);

    poDS = poDSIn;
    poFeatureDefn = new OGRFeatureDefn( "PolygonCorrections" );
    poFeatureDefn->Reference();
    poFeatureDefn->SetGeomType( wkbNone );

    /* -------------------------------------------------------------------- */
    /*      Fields from type B record.                                      */
    /* -------------------------------------------------------------------- */
    AddFieldDefns( psRTInfo, poFeatureDefn );
}
