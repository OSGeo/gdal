/******************************************************************************
 * $Id$
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
 ******************************************************************************
 *
 * $Log$
 * Revision 1.13  2006/03/29 00:46:20  fwarmerdam
 * update contact info
 *
 * Revision 1.12  2005/09/21 00:53:19  fwarmerdam
 * fixup OGRFeatureDefn and OGRSpatialReference refcount handling
 *
 * Revision 1.11  2003/01/11 15:29:55  warmerda
 * expanded tabs
 *
 * Revision 1.10  2003/01/04 23:21:56  mbp
 * Minor bug fixes and field definition changes.  Cleaned
 * up and commented code written for TIGER 2002 support.
 *
 * Revision 1.9  2002/12/26 00:20:19  mbp
 * re-organized code to hold TIGER-version details in TigerRecordInfo structs;
 * first round implementation of TIGER_2002 support
 *
 * Revision 1.8  2001/07/19 16:05:49  warmerda
 * clear out tabs
 *
 * Revision 1.7  2001/07/18 04:55:16  warmerda
 * added CPL_CSVID
 *
 * Revision 1.6  2001/07/04 23:25:32  warmerda
 * first round implementation of writer
 *
 * Revision 1.5  2001/07/04 05:40:35  warmerda
 * upgraded to support FILE, and Tiger2000 schema
 *
 * Revision 1.4  2001/01/19 21:15:20  warmerda
 * expanded tabs
 *
 * Revision 1.3  2000/01/13 05:18:11  warmerda
 * added support for multiple versions
 *
 * Revision 1.2  1999/12/22 15:38:15  warmerda
 * major update
 *
 * Revision 1.1  1999/12/15 19:59:17  warmerda
 * New
 *
 */

#include "ogr_tiger.h"
#include "cpl_conv.h"

CPL_CVSID("$Id$");

#define FILE_CODE "7"

static TigerFieldInfo rt7_2002_fields[] = {
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
static TigerRecordInfo rt7_2002_info =
  {
    rt7_2002_fields,
    sizeof(rt7_2002_fields) / sizeof(TigerFieldInfo),
    74
  };

static TigerFieldInfo rt7_fields[] = {
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
static TigerRecordInfo rt7_info =
  {
    rt7_fields,
    sizeof(rt7_fields) / sizeof(TigerFieldInfo),
    74
  };

/************************************************************************/
/*                            TigerLandmarks()                          */
/************************************************************************/

TigerLandmarks::TigerLandmarks( OGRTigerDataSource * poDSIn,
                                const char * pszPrototypeModule )
  : TigerPoint(FALSE)
{
    poDS = poDSIn;
    poFeatureDefn = new OGRFeatureDefn( "Landmarks" );
    poFeatureDefn->Reference();
    poFeatureDefn->SetGeomType( wkbPoint );

    if (poDS->GetVersion() >= TIGER_2002) {
        psRT7Info = &rt7_2002_info;
    } else {
        psRT7Info = &rt7_info;
    }

    AddFieldDefns( psRT7Info, poFeatureDefn );
}

TigerLandmarks::~TigerLandmarks()
{}


int TigerLandmarks::SetModule( const char * pszModule )
{
  return TigerPoint::SetModule( pszModule, FILE_CODE );
}

OGRFeature *TigerLandmarks::GetFeature( int nRecordId )
{
  return TigerPoint::GetFeature( nRecordId,
                                 psRT7Info,
                                 55, 64,
                                 65, 73 );
}

OGRErr TigerLandmarks::CreateFeature( OGRFeature *poFeature )
{
  return TigerPoint::CreateFeature( poFeature, 
                                    psRT7Info,
                                    55,
                                    FILE_CODE );
}
