/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Defines class for accessing OpenGIS feature tables through
 *           OLE DB and extracting geometry information.
 * Author:   Frank Warmerdam, warmerda@home.com
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
 * Revision 1.1  1999/04/01 20:49:09  warmerda
 * New
 *
 */

#ifndef OLEDB_SF_H_INCLUDED
#define OLEDB_SF_H_INCLUDED

#include "oledb_sup.h"

/************************************************************************/
/*                             OledbSFTable                             */
/*                                                                      */
/*      Class representing a table with geometry associated.            */
/************************************************************************/
                          
class OledbSFTable : public OledbSupRowset
{
    int         bTriedToIdentify;
    int         iBindColumn;       
    int         iGeomColumn;       /* -1 means there is none
                                      this is paoColumnInfo index, not ord. */

    void        IdentifyGeometry(); /* find the geometry column */

    BYTE        *pabyLastGeometry;

  public:
                OledbSFTable();
                ~OledbSFTable();

    int         HasGeometry();                
    BYTE        *GetWKBGeometry( int * );

    int         SelectGeometryColumn( const char * );
};
   

#endif /* ndef OLEDB_SF_H_INCLUDED */
