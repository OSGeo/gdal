/******************************************************************************
 * $Id$
 *
 * Project:  TIGER/Line Translator
 * Purpose:  Implements TigerPIP, providing access to .RTP files.
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

#define FILE_CODE "P"

static const TigerFieldInfo rtP_2002_fields[] = {
  // fieldname    fmt  type OFTType      beg  end  len  bDefine bSet bWrite
  { "MODULE",     ' ', ' ', OFTString,     0,   0,   8,       1,   0,     0 },
  { "FILE",       'L', 'N', OFTInteger,    6,  10,   5,       1,   1,     1 },
  { "CENID",      'L', 'A', OFTString,    11,  15,   5,       1,   1,     1 },
  { "POLYID",     'R', 'N', OFTInteger,   16,  25,  10,       1,   1,     1 },
  { "POLYLONG",   'R', 'N', OFTInteger,   26,  35,  10,       1,   1,     1 },
  { "POLYLAT",    'R', 'N', OFTInteger,   36,  44,   9,       1,   1,     1 },
  { "WATER",      'L', 'N', OFTInteger,   45,  45,   1,       1,   1,     1 },
};
static const TigerRecordInfo rtP_2002_info =
  {
    rtP_2002_fields,
    sizeof(rtP_2002_fields) / sizeof(TigerFieldInfo),
    45
  };

static const TigerFieldInfo rtP_fields[] = {
  // fieldname    fmt  type OFTType      beg  end  len  bDefine bSet bWrite
  { "MODULE",     ' ', ' ', OFTString,     0,   0,   8,       1,   0,     0 },
  { "FILE",       'L', 'N', OFTString,     6,  10,   5,       1,   1,     1 },
  { "STATE",      'L', 'N', OFTInteger,    6,   7,   2,       1,   1,     1 },
  { "COUNTY",     'L', 'N', OFTInteger,    8,  10,   3,       1,   1,     1 },
  { "CENID",      'L', 'A', OFTString,    11,  15,   5,       1,   1,     1 },
  { "POLYID",     'R', 'N', OFTInteger,   16,  25,  10,       1,   1,     1 }
};
static const TigerRecordInfo rtP_info =
  {
    rtP_fields,
    sizeof(rtP_fields) / sizeof(TigerFieldInfo),
    44
  };


/************************************************************************/
/*                              TigerPIP()                              */
/************************************************************************/

TigerPIP::TigerPIP( OGRTigerDataSource * poDSIn,
                            const char * pszPrototypeModule ) 
  : TigerPoint(TRUE, NULL, FILE_CODE)
{
    poDS = poDSIn;
    poFeatureDefn = new OGRFeatureDefn( "PIP" );
    poFeatureDefn->Reference();
    poFeatureDefn->SetGeomType( wkbPoint );

    if (poDS->GetVersion() >= TIGER_2002) {
        psRTInfo = &rtP_2002_info;
    } else {
        psRTInfo = &rtP_info;
    }
    AddFieldDefns( psRTInfo, poFeatureDefn );
}

OGRFeature *TigerPIP::GetFeature( int nRecordId )
{
  return TigerPoint::GetFeature( nRecordId,
                                 26, 35,
                                 36, 44 );
}

OGRErr TigerPIP::CreateFeature( OGRFeature *poFeature )
{
  return TigerPoint::CreateFeature( poFeature, 
                                    26 );
}

