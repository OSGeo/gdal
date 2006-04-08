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
 ******************************************************************************
 *
 * $Log$
 * Revision 1.8  2006/04/08 06:14:31  fwarmerdam
 * added proper (I believe) support for spatial and attribute filter on gensql layers
 *
 * Revision 1.7  2005/02/02 20:00:29  fwarmerdam
 * added SetNextByIndex support
 *
 * Revision 1.6  2003/03/20 19:13:21  warmerda
 * Added ClearFilters() method to cleanup spatial or attribute filters on the
 * target layer, and any joined layers.  Used in destructor and after all
 * features have been read from source layer.
 *
 * Revision 1.5  2003/03/19 20:34:23  warmerda
 * add support for tables from external datasources
 *
 * Revision 1.4  2003/03/05 05:10:17  warmerda
 * implement join support
 *
 * Revision 1.3  2002/04/29 19:35:50  warmerda
 * fixes for selecting FID
 *
 * Revision 1.2  2002/04/25 16:07:55  warmerda
 * fleshed out DISTINCT support
 *
 * Revision 1.1  2002/04/25 02:24:37  warmerda
 * New
 *
 */

#ifndef _OGR_GENSQL_H_INCLUDED
#define _OGR_GENSQL_H_INCLUDED

#include "ogrsf_frmts.h"

/************************************************************************/
/*                        OGRGenSQLResultsLayer                         */
/************************************************************************/

class CPL_DLL OGRGenSQLResultsLayer : public OGRLayer
{
  private:
    OGRDataSource *poSrcDS;
    OGRLayer    *poSrcLayer;
    void        *pSelectInfo;

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
    
  public:
                OGRGenSQLResultsLayer( OGRDataSource *poSrcDS, 
                                       void *pSelectInfo, 
                                       OGRGeometry *poSpatFilter );
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

