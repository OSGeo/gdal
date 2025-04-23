/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Classes related to generic implementation of attribute indexing.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2003, Frank Warmerdam <warmerdam@pobox.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef OGR_ATTRIND_H_INCLUDED
#define OGR_ATTRIND_H_INCLUDED

#include "ogrsf_frmts.h"

//! @cond Doxygen_Suppress

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
    virtual ~OGRAttrIndex();

    virtual GIntBig GetFirstMatch(OGRField *psKey) = 0;
    virtual GIntBig *GetAllMatches(OGRField *psKey) = 0;
    virtual GIntBig *GetAllMatches(OGRField *psKey, GIntBig *panFIDList,
                                   int *nFIDCount, int *nLength) = 0;

    virtual OGRErr AddEntry(OGRField *psKey, GIntBig nFID) = 0;
    virtual OGRErr RemoveEntry(OGRField *psKey, GIntBig nFID) = 0;

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
    OGRLayer *poLayer;
    char *pszIndexPath;

    OGRLayerAttrIndex();
    CPL_DISALLOW_COPY_ASSIGN(OGRLayerAttrIndex)

  public:
    virtual ~OGRLayerAttrIndex();

    virtual OGRErr Initialize(const char *pszIndexPath, OGRLayer *) = 0;

    virtual OGRErr CreateIndex(int iField) = 0;
    virtual OGRErr DropIndex(int iField) = 0;
    virtual OGRErr IndexAllFeatures(int iField = -1) = 0;

    virtual OGRErr AddToIndex(OGRFeature *poFeature, int iField = -1) = 0;
    virtual OGRErr RemoveFromIndex(OGRFeature *poFeature) = 0;

    virtual OGRAttrIndex *GetFieldIndex(int iField) = 0;
};

OGRLayerAttrIndex CPL_DLL *OGRCreateDefaultLayerIndex();

//! @endcond

#endif /* ndef OGR_ATTRIND_H_INCLUDED */
