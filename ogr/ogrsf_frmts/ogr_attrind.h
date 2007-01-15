/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Classes related to generic implementation of attribute indexing.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2003, Frank Warmerdam <warmerdam@pobox.com>
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
 * Revision 1.4  2003/12/03 15:11:05  warmerda
 * added iField to IndexAllFeature, and default to -1
 *
 * Revision 1.3  2003/03/21 02:58:00  warmerda
 * added drop index support
 *
 * Revision 1.2  2003/03/07 18:24:12  warmerda
 * expand tabs
 *
 * Revision 1.1  2003/03/04 14:16:10  warmerda
 * New
 *
 */

#ifndef _OGR_ATTRIND_H_INCLUDED
#define _OGR_ATTRIND_H_INCLUDED

#include "ogrsf_frmts.h"

/************************************************************************/
/*                             OGRAttrIndex                             */
/*                                                                      */
/*      Base class for accessing the indexing info about one field.     */
/************************************************************************/

class CPL_DLL OGRAttrIndex
{
protected:
                OGRAttrIndex();

public:
    virtual     ~OGRAttrIndex();

    virtual long   GetFirstMatch( OGRField *psKey ) = 0;
    virtual long  *GetAllMatches( OGRField *psKey ) = 0;
    
    virtual OGRErr AddEntry( OGRField *psKey, long nFID ) = 0;
    virtual OGRErr RemoveEntry( OGRField *psKey, long nFID ) = 0;

    virtual OGRErr Clear() = 0;
};

/************************************************************************/
/*                          OGRLayerAttrIndex                           */
/*                                                                      */
/*      Base class representing attribute indexes for all indexed       */
/*      fields in a layer.                                              */
/************************************************************************/

class CPL_DLL OGRLayerAttrIndex
{
protected:
    OGRLayer    *poLayer;
    char        *pszIndexPath;

                OGRLayerAttrIndex();

public:
    virtual     ~OGRLayerAttrIndex();

    virtual OGRErr Initialize( const char *pszIndexPath, OGRLayer * ) = 0;

    virtual OGRErr CreateIndex( int iField ) = 0;
    virtual OGRErr DropIndex( int iField ) = 0;
    virtual OGRErr IndexAllFeatures( int iField = -1 ) = 0;

    virtual OGRErr AddToIndex( OGRFeature *poFeature, int iField = -1 ) = 0;
    virtual OGRErr RemoveFromIndex( OGRFeature *poFeature ) = 0;

    virtual OGRAttrIndex *GetFieldIndex( int iField ) = 0;
};

OGRLayerAttrIndex CPL_DLL *OGRCreateDefaultLayerIndex();


#endif /* ndef _OGR_ATTRIND_H_INCLUDED */

