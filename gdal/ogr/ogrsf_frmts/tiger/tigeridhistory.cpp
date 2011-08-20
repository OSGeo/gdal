/******************************************************************************
 * $Id$
 *
 * Project:  TIGER/Line Translator
 * Purpose:  Implements TigerIDHistory, providing access to .RTH files.
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

CPL_CVSID("$Id$");

#define FILE_CODE "H"

static const TigerFieldInfo rtH_fields[] = {
  // fieldname    fmt  type OFTType      beg  end  len  bDefine bSet bWrite
  { "MODULE",     ' ', ' ', OFTString,     0,   0,   8,       1,   0,     0 },
  { "FILE",       'L', 'N', OFTString,     6,  10,   5,       1,   1,     1 },
  { "STATE",      'L', 'N', OFTInteger,    6,   7,   2,       1,   1,     1 },
  { "COUNTY",     'L', 'N', OFTInteger,    8,  10,   3,       1,   1,     1 },
  { "TLID",       'R', 'N', OFTInteger,   11,  20,  10,       1,   1,     1 },
  { "HIST",       'L', 'A', OFTString,    21,  21,   1,       1,   1,     1 },
  { "SOURCE",     'L', 'A', OFTString,    22,  22,   1,       1,   1,     1 },
  { "TLIDFR1",    'R', 'N', OFTInteger,   23,  32,  10,       1,   1,     1 },
  { "TLIDFR2",    'R', 'N', OFTInteger,   33,  42,  10,       1,   1,     1 },
  { "TLIDTO1",    'R', 'N', OFTInteger,   43,  52,  10,       1,   1,     1 },
  { "TLIDTO2",    'R', 'N', OFTInteger,   53,  62,  10,       1,   1,     1 }
};
static const TigerRecordInfo rtH_info =
  {
    rtH_fields,
    sizeof(rtH_fields) / sizeof(TigerFieldInfo),
    62
  };

/************************************************************************/
/*                           TigerIDHistory()                           */
/************************************************************************/

TigerIDHistory::TigerIDHistory( OGRTigerDataSource * poDSIn,
                            const char * pszPrototypeModule ) : TigerFileBase(&rtH_info, FILE_CODE)

{
    poDS = poDSIn;
    poFeatureDefn = new OGRFeatureDefn( "IDHistory" );
    poFeatureDefn->Reference();
    poFeatureDefn->SetGeomType( wkbNone );

    /* -------------------------------------------------------------------- */
    /*      Fields from record type H                                       */
    /* -------------------------------------------------------------------- */

    AddFieldDefns(psRTInfo, poFeatureDefn);
}
