/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  SFCRowset class, client side abstraction for an OLE DB spatial
 *           table.
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
 * Revision 1.1  1999/06/02 16:27:51  warmerda
 * New
 *
 *
 */

#ifndef SFCROWSET_H_INCLUDED
#define SFCROWSET_H_INCLUDED

#include "oledb_sup.h"

class OGRFeature;
class OGRGeometry;

/************************************************************************/
/*                              SFCRowset                               */
/************************************************************************/

/**
 * Abstract representation of a rowset (table) with spatial features.
 *
 * This class is intended to simplify access to spatial rowsets, and to
 * centralize all the rules for selecting geometry columns, getting the
 * spatial reference system of a rowset, and special feature access short
 * cuts with selected providers.
 *
 * It is expected that installation of a spatial filter (when desired) will
 * be taken care of before the SFCRowset is instantiated (by
 * SFCProvider::CreateSFCRowset()).  Applications wouldn't normally be
 * creating an SFCRowset directly, but this is intended to be legal allowing
 * special handling of properties, or avoiding the use of SFCProvider.
 *
 * The SFCRowset is built on (derived from) OledbSupRowset, which is intended
 * to be a simplified interface to any kind of rowset.  I imagine substantial
 * changes will occur in OledbSupRowset in the future.
 *
 */

class SFCRowset : public OledbSupRowset
{
  private:
    int         bTriedToIdentify;
    int         iBindColumn;       
    int         iGeomColumn;       /* -1 means there is none
                                      this is paoColumnInfo index, not ord. */

    void        IdentifyGeometry(); /* find the geometry column */

    BYTE        *pabyLastGeometry;

  public:
    		SFCRowset();
    virtual     ~SFCRowset();

    /** Use an existing rowset */
    virtual HRESULT AccessRowset( IRowset * pIRowset );

    /** Get the spatial reference system of this rowset */
    const char *GetSpatialRefWKT();

    /** Which column contains the geometry? */
    int		GetGeometryColumn();

    /** Force use of a particular geometry column */
    HRESULT	SetGeometryColumn( int i );

    /** Get geometry type */
    OGRwkbGeometryType GetGeometryType();

    /** Fetch the raw geometry data for the last row read */
    BYTE        *GetWKBGeometry( int * pnSize );

    /** Fetch the geometry as an Object */
    OGRGeometry *GetOGRGeometry();

    /** Fetch the whole record as a feature */
    OGRFeature  *GetOGRFeature();
};

#endif /* ndef SFCROWSET_SF_H_INCLUDED */
