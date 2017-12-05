/******************************************************************************
 *
 * Project:  TIGER/Line Translator
 * Purpose:  Implements TigerEntityNames, providing access to .RTC files.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Frank Warmerdam
 * Copyright (c) 2011, Even Rouault <even dot rouault at mines-paris dot org>
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

static const char FILE_CODE[] = "C";

static const TigerFieldInfo rtC_2002_fields[] = {
  // fieldname    fmt  type OFTType      beg  end  len  bDefine bSet bWrite
  { "MODULE",     ' ', ' ', OFTString,     0,   0,   8,       1,   0,     0 },
  { "STATE",      'L', 'N', OFTInteger,    6,   7,   2,       1,   1,     1 },
  { "COUNTY",     'L', 'N', OFTInteger,    8,  10,   3,       1,   1,     1 },
  { "DATAYR",     'L', 'A', OFTString,    11,  14,   4,       1,   1,     1 },
  { "FIPS",       'L', 'N', OFTInteger,   15,  19,   5,       1,   1,     1 },
  { "FIPSCC",     'L', 'A', OFTString,    20,  21,   2,       1,   1,     1 },
  { "PLACEDC",    'L', 'A', OFTString,    22,  22,   1,       1,   1,     1 },
  { "LSADC",      'L', 'A', OFTString,    23,  24,   2,       1,   1,     1 },
  { "ENTITY",     'L', 'A', OFTString,    25,  25,   1,       1,   1,     1 },
  { "MA",         'L', 'N', OFTInteger,   26,  29,   4,       1,   1,     1 },
  { "SD",         'L', 'N', OFTInteger,   30,  34,   5,       1,   1,     1 },
  { "AIANHH",     'L', 'N', OFTInteger,   35,  38,   4,       1,   1,     1 },
  { "VTDTRACT",   'R', 'A', OFTString,    39,  44,   6,       1,   1,     1 },
  { "UAUGA",      'L', 'N', OFTInteger,   45,  49,   5,       1,   1,     1 },
  { "AITSCE",     'L', 'N', OFTInteger,   50,  52,   3,       1,   1,     1 },
  { "RS_C1",      'L', 'N', OFTInteger,   53,  54,   2,       1,   1,     1 },
  { "RS_C2",      'L', 'N', OFTInteger,   55,  62,   8,       1,   1,     1 },
  { "NAME",       'L', 'A', OFTString,    63, 122,  60,       1,   1,     1 },
};
static const TigerRecordInfo rtC_2002_info =
  {
    rtC_2002_fields,
    sizeof(rtC_2002_fields) / sizeof(TigerFieldInfo),
    122
  };

static const TigerFieldInfo rtC_2000_Redistricting_fields[] = {
  // fieldname    fmt  type OFTType      beg  end  len  bDefine bSet bWrite
  { "MODULE",     ' ', ' ', OFTString,     0,   0,   8,       1,   0,     0 },
  { "STATE",      'L', 'N', OFTInteger,    6,   7,   2,       1,   1,     1 },
  { "COUNTY",     'L', 'N', OFTInteger,    8,  10,   3,       1,   1,     1 },
  { "FIPSYR",     'L', 'N', OFTString,    11,  14,   4,       1,   1,     1 },
  { "FIPS",       'L', 'N', OFTInteger,   15,  19,   5,       1,   1,     1 },
  { "FIPSCC",     'L', 'A', OFTString,    20,  21,   2,       1,   1,     1 },
  { "PDC",        'L', 'A', OFTString,    22,  22,   1,       1,   1,     1 },
  { "LASAD",      'L', 'A', OFTString,    23,  24,   2,       1,   1,     1 },
  { "ENTITY",     'L', 'A', OFTString,    25,  25,   1,       1,   1,     1 },
  { "MA",         'L', 'N', OFTInteger,   26,  29,   4,       1,   1,     1 },
  { "SD",         'L', 'N', OFTInteger,   30,  34,   5,       1,   1,     1 },
  { "AIR",        'L', 'N', OFTInteger,   35,  38,   4,       1,   1,     1 },
  { "VTD",        'R', 'A', OFTString,    39,  44,   6,       1,   1,     1 },
  { "UA",         'L', 'N', OFTInteger,   45,  49,   5,       1,   1,     1 },
  { "AITSCE",     'L', 'N', OFTInteger,   50,  52,   3,       1,   1,     1 },
  { "NAME",       'L', 'A', OFTString,    53, 112,  66,       1,   1,     1 }
};
static const TigerRecordInfo rtC_2000_Redistricting_info =
  {
    rtC_2000_Redistricting_fields,
    sizeof(rtC_2000_Redistricting_fields) / sizeof(TigerFieldInfo),
    112
  };

static const TigerFieldInfo rtC_fields[] = {
  // fieldname    fmt  type OFTType      beg  end  len  bDefine bSet bWrite
  { "MODULE",     ' ', ' ', OFTString,     0,   0,   8,       1,   0,     0 },
  { "STATE",      'L', 'N', OFTInteger,    6,   7,   2,       1,   1,     1 },
  { "COUNTY",     'L', 'N', OFTInteger,    8,  10,   3,       1,   1,     1 },
  { "FIPSYR",     'L', 'N', OFTString,    11,  12,   4,       1,   1,     1 },
  { "FIPS",       'L', 'N', OFTInteger,   13,  17,   5,       1,   1,     1 },
  { "FIPSCC",     'L', 'A', OFTString,    18,  19,   2,       1,   1,     1 },
  { "PDC",        'L', 'A', OFTString,    20,  20,   1,       1,   1,     1 },
  { "LASAD",      'L', 'A', OFTString,    21,  22,   2,       1,   1,     1 },
  { "ENTITY",     'L', 'A', OFTString,    23,  23,   1,       1,   1,     1 },
  { "MA",         'L', 'N', OFTInteger,   24,  27,   4,       1,   1,     1 },
  { "SD",         'L', 'N', OFTInteger,   28,  32,   5,       1,   1,     1 },
  { "AIR",        'L', 'N', OFTInteger,   33,  36,   4,       1,   1,     1 },
  { "VTD",        'R', 'A', OFTString,    37,  42,   6,       1,   1,     1 },
  { "UA",         'L', 'N', OFTInteger,   43,  46,   4,       1,   1,     1 },
  { "NAME",       'L', 'A', OFTString,    47, 112,  66,       1,   1,     1 }
};
static const TigerRecordInfo rtC_info =
  {
    rtC_fields,
    sizeof(rtC_fields) / sizeof(TigerFieldInfo),
    112
  };

/************************************************************************/
/*                          TigerEntityNames()                          */
/************************************************************************/

TigerEntityNames::TigerEntityNames( OGRTigerDataSource * poDSIn,
                                    CPL_UNUSED const char * pszPrototypeModule ) :
    TigerFileBase(NULL, FILE_CODE)
{
    poDS = poDSIn;
    poFeatureDefn = new OGRFeatureDefn( "EntityNames" );
    poFeatureDefn->Reference();
    poFeatureDefn->SetGeomType( wkbPoint );

    if( poDS->GetVersion() >= TIGER_2002 ) {
      psRTInfo = &rtC_2002_info;
    } else if( poDS->GetVersion() >= TIGER_2000_Redistricting ) {
      psRTInfo = &rtC_2000_Redistricting_info;
    } else {
      psRTInfo = &rtC_info;
    }

    AddFieldDefns( psRTInfo, poFeatureDefn );
}
