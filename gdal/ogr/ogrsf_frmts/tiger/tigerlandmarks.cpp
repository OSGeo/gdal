/******************************************************************************
 *
 * Project:  TIGER/Line Translator
 * Purpose:  Implements TigerLandmarks, providing access to .RT7 files.
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

static const char FILE_CODE[] = "7";

static const TigerFieldInfo rt7_2002_fields[] = {
  // fieldname    fmt  type OFTType      beg  end  len  bDefine bSet bWrite
  { "MODULE",     ' ', ' ', OFTString,     0,   0,   8,       1,   0,     0 },
  { "FILE",       'L', 'N', OFTInteger,    6,  10,   5,       1,   1,     1 },
  { "LAND",       'R', 'N', OFTInteger,   11,  20,  10,       1,   1,     1 },
  { "SOURCE",     'L', 'A', OFTString,    21,  21,   1,       1,   1,     1 },
  { "CFCC",       'L', 'A', OFTString,    22,  24,   3,       1,   1,     1 },
  { "LANAME",     'L', 'A', OFTString,    25,  54,  30,       1,   1,     1 },
  { "LALONG",     'R', 'N', OFTInteger,   55,  64,  10,       1,   1,     1 },
  { "LALAT",      'R', 'N', OFTInteger,   65,  73,   9,       1,   1,     1 },
  { "FILLER",     'L', 'A', OFTString,    74,  74,   1,       1,   1,     1 },
};
static const TigerRecordInfo rt7_2002_info =
  {
    rt7_2002_fields,
    sizeof(rt7_2002_fields) / sizeof(TigerFieldInfo),
    74
  };

static const TigerFieldInfo rt7_fields[] = {
  // fieldname    fmt  type OFTType      beg  end  len  bDefine bSet bWrite
  { "MODULE",     ' ', ' ', OFTString,     0,   0,   8,       1,   0,     0 },
  { "FILE",       'L', 'N', OFTString,     6,  10,   5,       1,   0,     1 },
  { "STATE",      'L', 'N', OFTInteger,    6,   7,   2,       1,   1,     1 },
  { "COUNTY",     'L', 'N', OFTInteger,    8,  10,   3,       1,   1,     1 },
  { "LAND",       'R', 'N', OFTInteger,   11,  20,  10,       1,   1,     1 },
  { "SOURCE",     'L', 'A', OFTString,    21,  21,   1,       1,   1,     1 },
  { "CFCC",       'L', 'A', OFTString,    22,  24,   3,       1,   1,     1 },
  { "LANAME",     'L', 'A', OFTString,    25,  54,  30,       1,   1,     1 }
};
static const TigerRecordInfo rt7_info =
  {
    rt7_fields,
    sizeof(rt7_fields) / sizeof(TigerFieldInfo),
    74
  };

/************************************************************************/
/*                            TigerLandmarks()                          */
/************************************************************************/

TigerLandmarks::TigerLandmarks( OGRTigerDataSource * poDSIn,
                                CPL_UNUSED const char * pszPrototypeModule )
  : TigerPoint(FALSE, NULL, FILE_CODE)
{
    poDS = poDSIn;
    poFeatureDefn = new OGRFeatureDefn( "Landmarks" );
    poFeatureDefn->Reference();
    poFeatureDefn->SetGeomType( wkbPoint );

    if (poDS->GetVersion() >= TIGER_2002) {
        psRTInfo = &rt7_2002_info;
    } else {
        psRTInfo = &rt7_info;
    }

    AddFieldDefns( psRTInfo, poFeatureDefn );
}

OGRFeature *TigerLandmarks::GetFeature( int nRecordId )
{
  return TigerPoint::GetFeature( nRecordId,
                                 55, 64,
                                 65, 73 );
}

OGRErr TigerLandmarks::CreateFeature( OGRFeature *poFeature )
{
  return TigerPoint::CreateFeature( poFeature,
                                    55 );
}
