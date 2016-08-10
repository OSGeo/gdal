/******************************************************************************
 * $Id$
 *
 * Project:  NAS Reader
 * Purpose:  Declarations for OGR wrapper classes for NAS, and NAS<->OGR
 *           translation of geometry.
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
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 ****************************************************************************/

#ifndef OGR_NAS_H_INCLUDED
#define OGR_NAS_H_INCLUDED

#include "ogrsf_frmts.h"
#include "nasreaderp.h"
#include "ogr_api.h"
#include <vector>

class OGRNASDataSource;

/************************************************************************/
/*                            OGRNASLayer                               */
/************************************************************************/

class OGRNASLayer : public OGRLayer
{
    OGRSpatialReference *poSRS;
    OGRFeatureDefn     *poFeatureDefn;

    int                 iNextNASId;

    OGRNASDataSource    *poDS;

    GMLFeatureClass     *poFClass;

  public:
                        OGRNASLayer( const char * pszName,
                                     OGRSpatialReference *poSRS,
                                     OGRwkbGeometryType eType,
                                     OGRNASDataSource *poDS );

                        ~OGRNASLayer();

    void                ResetReading();
    OGRFeature *        GetNextFeature();

    GIntBig             GetFeatureCount( int bForce = TRUE );
    OGRErr              GetExtent(OGREnvelope *psExtent, int bForce = TRUE);
    virtual OGRErr      GetExtent(int iGeomField, OGREnvelope *psExtent, int bForce)
                { return OGRLayer::GetExtent(iGeomField, psExtent, bForce); }

    OGRFeatureDefn *    GetLayerDefn() { return poFeatureDefn; }

    int                 TestCapability( const char * );
};

/************************************************************************/
/*                         OGRNASRelationLayer                          */
/************************************************************************/

class OGRNASRelationLayer : public OGRLayer
{
    OGRFeatureDefn     *poFeatureDefn;
    OGRNASDataSource    *poDS;

    bool                 bPopulated;
    int                  iNextFeature;
    std::vector<CPLString> aoRelationCollection;

  public:
                        OGRNASRelationLayer( OGRNASDataSource *poDS );
                        ~OGRNASRelationLayer();

    void                ResetReading();
    OGRFeature *        GetNextFeature();

    GIntBig             GetFeatureCount( int bForce = TRUE );
    OGRFeatureDefn *    GetLayerDefn() { return poFeatureDefn; }
    int                 TestCapability( const char * );

    // For use populating.
    void                AddRelation( const char *pszFromID,
                                     const char *pszType,
                                     const char *pszToID );
    void                MarkRelationsPopulated() { bPopulated = true; }
};

/************************************************************************/
/*                           OGRNASDataSource                           */
/************************************************************************/

class OGRNASDataSource : public OGRDataSource
{
    OGRLayer          **papoLayers;
    int                 nLayers;

    OGRNASRelationLayer *poRelationLayer;

    char                *pszName;

    OGRNASLayer         *TranslateNASSchema( GMLFeatureClass * );

    // input related parameters.
    IGMLReader          *poReader;

    void                InsertHeader();

  public:
                        OGRNASDataSource();
                        ~OGRNASDataSource();

    int                 Open( const char * );
    int                 Create( const char *pszFile, char **papszOptions );

    const char          *GetName() { return pszName; }
    int                 GetLayerCount() { return nLayers; }
    OGRLayer            *GetLayer( int );

    int                 TestCapability( const char * );

    IGMLReader          *GetReader() { return poReader; }

    void                GrowExtents( OGREnvelope *psGeomBounds );

    void                PopulateRelations();
};

#endif /* OGR_NAS_H_INCLUDED */
