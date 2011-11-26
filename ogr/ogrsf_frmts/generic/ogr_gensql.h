/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Classes related to generic implementation of ExecuteSQL().
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2002, Frank Warmerdam
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

#ifndef _OGR_GENSQL_H_INCLUDED
#define _OGR_GENSQL_H_INCLUDED

#include "ogrsf_frmts.h"
#include "swq.h"
#include "cpl_hash_set.h"

/************************************************************************/
/*                        OGRGenSQLResultsLayer                         */
/************************************************************************/

class CPL_DLL OGRGenSQLResultsLayer : public OGRLayer
{
  private:
    OGRDataSource *poSrcDS;
    OGRLayer    *poSrcLayer;
    void        *pSelectInfo;

    char        *pszWHERE;

    OGRLayer   **papoTableLayers;

    OGRFeatureDefn *poDefn;

    int         PrepareSummary();

    int         nIndexSize;
    long       *panFIDIndex;

    int         nNextIndexFID;
    OGRFeature  *poSummaryFeature;

    int         iFIDFieldIndex;

    OGRField    *pasOrderByIndex;

    int         nExtraDSCount;
    OGRDataSource **papoExtraDS;

    OGRFeature *TranslateFeature( OGRFeature * );
    void        CreateOrderByIndex();
    void        SortIndexSection( OGRField *pasIndexFields, 
                                  int nStart, int nEntries );
    int         Compare( OGRField *pasFirst, OGRField *pasSecond );

    void        ClearFilters();

    void        FindAndSetIgnoredFields();
    void        ExploreExprForIgnoredFields(swq_expr_node* expr, CPLHashSet* hSet);
    void        AddFieldDefnToSet(int iTable, int iColumn, CPLHashSet* hSet);
    
  public:
                OGRGenSQLResultsLayer( OGRDataSource *poSrcDS, 
                                       void *pSelectInfo, 
                                       OGRGeometry *poSpatFilter,
                                       const char *pszWHERE,
                                       const char *pszDialect );
    virtual     ~OGRGenSQLResultsLayer();

    virtual OGRGeometry *GetSpatialFilter();

    virtual void        ResetReading();
    virtual OGRFeature *GetNextFeature();
    virtual OGRErr      SetNextByIndex( long nIndex );
    virtual OGRFeature *GetFeature( long nFID );

    virtual OGRFeatureDefn *GetLayerDefn();

    virtual OGRSpatialReference *GetSpatialRef();

    virtual int         GetFeatureCount( int bForce = TRUE );
    virtual OGRErr      GetExtent(OGREnvelope *psExtent, int bForce = TRUE);

    virtual int         TestCapability( const char * );
};

#endif /* ndef _OGR_GENSQL_H_INCLUDED */

