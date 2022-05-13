/******************************************************************************
 *
 * Project:  TIGER/Line Translator
 * Purpose:  Implements TigerZipCodes, providing access to .RT6 files.
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

#include "ogr_tiger.h"
#include "cpl_conv.h"

CPL_CVSID("$Id$")

static const char FILE_CODE[] = "6";

static const TigerFieldInfo rt6_fields[] = {
  // fieldname    fmt  type OFTType     beg  end  len  bDefine bSet
  { "MODULE",     ' ', ' ', OFTString,    0,   0,   8,       1,   0 },
  { "TLID",       'R', 'N', OFTInteger,   6,  15,  10,       1,   1 },
  { "RTSQ",       'R', 'N', OFTInteger,  16,  18,   3,       1,   1 },
  { "FRADDL",     'R', 'A', OFTString,   19,  29,  11,       1,   1 },
  { "TOADDL",     'R', 'A', OFTString,   30,  40,  11,       1,   1 },
  { "FRADDR",     'R', 'A', OFTString,   41,  51,  11,       1,   1 },
  { "TOADDR",     'R', 'A', OFTString,   52,  62,  11,       1,   1 },
  { "FRIADDL",    'L', 'A', OFTInteger,  63,  63,   1,       1,   1 },
  { "TOIADDL",    'L', 'A', OFTInteger,  64,  64,   1,       1,   1 },
  { "FRIADDR",    'L', 'A', OFTInteger,  65,  65,   1,       1,   1 },
  { "TOIADDR",    'L', 'A', OFTInteger,  66,  66,   1,       1,   1 },
  { "ZIPL",       'L', 'N', OFTInteger,  67,  71,   5,       1,   1 },
  { "ZIPR",       'L', 'N', OFTInteger,  72,  76,   5,       1,   1 }
};
static const TigerRecordInfo rt6_info =
  {
    rt6_fields,
    sizeof(rt6_fields) / sizeof(TigerFieldInfo),
    76
  };

/************************************************************************/
/*                            TigerZipCodes()                           */
/************************************************************************/

TigerZipCodes::TigerZipCodes( OGRTigerDataSource * poDSIn,
                              CPL_UNUSED const char * pszPrototypeModule ) :
    TigerFileBase(&rt6_info, FILE_CODE)

{
    poDS = poDSIn;
    poFeatureDefn = new OGRFeatureDefn( "ZipCodes" );
    poFeatureDefn->Reference();
    poFeatureDefn->SetGeomType( wkbNone );

    /* -------------------------------------------------------------------- */
    /*      Fields from type 6 record.                                      */
    /* -------------------------------------------------------------------- */

    AddFieldDefns( psRTInfo, poFeatureDefn );
}
