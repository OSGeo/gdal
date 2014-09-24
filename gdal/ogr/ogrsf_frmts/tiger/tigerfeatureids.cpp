/******************************************************************************
 * $Id$
 *
 * Project:  TIGER/Line Translator
 * Purpose:  Implements TigerFeatureIds, providing access to .RT5 files.
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

#define FILE_CODE "5"

static const TigerFieldInfo rt5_2002_fields[] = {
  // fieldname    fmt  type OFTType      beg  end  len  bDefine bSet bWrite
  { "MODULE",     ' ', ' ', OFTString,     0,   0,   8,       1,   0,     0 },
  { "FILE",       'L', 'N', OFTInteger,    6,  10,   5,       1,   1,     1 },
  { "FEAT",       'R', 'N', OFTInteger,   11,  18,   8,       1,   1,     1 },
  { "FEDIRP",     'L', 'A', OFTString,    19,  20,   2,       1,   1,     1 },
  { "FENAME",     'L', 'A', OFTString,    21,  50,  30,       1,   1,     1 },
  { "FETYPE",     'L', 'A', OFTString,    51,  54,   4,       1,   1,     1 },
  { "FEDIRS",     'L', 'A', OFTString,    55,  56,   2,       1,   1,     1 },
};
static const TigerRecordInfo rt5_2002_info =
  {
    rt5_2002_fields,
    sizeof(rt5_2002_fields) / sizeof(TigerFieldInfo),
    56
  };

static const TigerFieldInfo rt5_fields[] = {
  // fieldname    fmt  type OFTType      beg  end  len  bDefine bSet bWrite
  { "MODULE",     ' ', ' ', OFTString,     0,   0,   8,       1,   0,     0 },
  { "FILE",       'L', 'N', OFTString,     2,   6,   5,       1,   1,     1 },
  { "STATE",      'L', 'N', OFTInteger,    2,   3,   2,       1,   1,     1 },
  { "COUNTY",     'L', 'N', OFTInteger,    4,   6,   3,       1,   1,     1 },
  { "FEAT",       'R', 'N', OFTInteger,    7,  14,   8,       1,   1,     1 },
  { "FEDIRP",     'L', 'A', OFTString,    15,  16,   2,       1,   1,     1 },
  { "FENAME",     'L', 'A', OFTString,    17,  46,  30,       1,   1,     1 },
  { "FETYPE",     'L', 'A', OFTString,    47,  50,   4,       1,   1,     1 },
  { "FEDIRS",     'L', 'A', OFTString,    51,  52,   2,       1,   1,     1 }
};

static const TigerRecordInfo rt5_info =
  {
    rt5_fields,
    sizeof(rt5_fields) / sizeof(TigerFieldInfo),
    52
  };

/************************************************************************/
/*                            TigerFeatureIds()                         */
/************************************************************************/

TigerFeatureIds::TigerFeatureIds( OGRTigerDataSource * poDSIn,
                                  CPL_UNUSED const char * pszPrototypeModule ) : TigerFileBase(NULL, FILE_CODE)
{
  poDS = poDSIn;
  poFeatureDefn = new OGRFeatureDefn( "FeatureIds" );
    poFeatureDefn->Reference();
  poFeatureDefn->SetGeomType( wkbNone );

  if (poDS->GetVersion() >= TIGER_2002) {
    psRTInfo = &rt5_2002_info;
  } else {
    psRTInfo = &rt5_info;
  }

  AddFieldDefns( psRTInfo, poFeatureDefn );
}
