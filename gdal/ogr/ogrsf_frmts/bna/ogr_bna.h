/******************************************************************************
 * $Id$
 *
 * Project:  BNA Translator
 * Purpose:  Definition of classes for OGR .bna driver.
 * Author:   Even Rouault, even dot rouault at mines dash paris dot org
 *
 ******************************************************************************
 * Copyright (c) 2007-2010, Even Rouault <even dot rouault at mines-paris dot org>
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

#ifndef OGR_BNA_H_INCLUDED
#define OGR_BNA_H_INCLUDED

#include "ogrsf_frmts.h"

#include "ogrbnaparser.h"

class OGRBNADataSource;

/************************************************************************/
/*                             OGRBNALayer                              */
/************************************************************************/

typedef struct
{
  int   offset;
  int   line;
} OffsetAndLine;

class OGRBNALayer : public OGRLayer
{
    OGRFeatureDefn*    poFeatureDefn;

    OGRBNADataSource*  poDS;
    int                bWriter;

    int                nIDs;
    bool               eof;
    bool               failed;
    int                curLine;
    int                nNextFID;
    VSILFILE*          fpBNA;
    int                nFeatures;
    bool               partialIndexTable;
    OffsetAndLine*     offsetAndLineFeaturesTable;

    BNAFeatureType     bnaFeatureType;

    OGRFeature *       BuildFeatureFromBNARecord (BNARecord* record, long fid);
    void               FastParseUntil ( int interestFID);
    void               WriteFeatureAttributes(VSILFILE* fp, OGRFeature *poFeature);
    void               WriteCoord(VSILFILE* fp, double dfX, double dfY);

  public:
                        OGRBNALayer(const char *pszFilename,
                                    const char* layerName,
                                    BNAFeatureType bnaFeatureType,
                                    OGRwkbGeometryType eLayerGeomType,
                                    int bWriter,
                                    OGRBNADataSource* poDS,
                                    int nIDs = NB_MAX_BNA_IDS);
                        ~OGRBNALayer();

    void                SetFeatureIndexTable(int nFeatures,
                                             OffsetAndLine* offsetAndLineFeaturesTable,
                                             int partialIndexTable);

    void                ResetReading() override;
    OGRFeature *        GetNextFeature() override;

    OGRErr              ICreateFeature( OGRFeature *poFeature ) override;
    OGRErr              CreateField( OGRFieldDefn *poField, int bApproxOK ) override;

    OGRFeatureDefn *    GetLayerDefn() override { return poFeatureDefn; }

    OGRFeature *        GetFeature( GIntBig nFID ) override;

    int                 TestCapability( const char * ) override;
};

/************************************************************************/
/*                           OGRBNADataSource                           */
/************************************************************************/

class OGRBNADataSource : public OGRDataSource
{
    char*               pszName;

    OGRBNALayer**       papoLayers;
    int                 nLayers;

    int                 bUpdate;

    /*  Export related */
    VSILFILE                *fpOutput; /* Virtual file API */
    bool                bUseCRLF;
    int                 bMultiLine;
    int                 nbOutID;
    bool                bEllipsesAsEllipses;
    int                 nbPairPerLine;
    int                 coordinatePrecision;
    char*               pszCoordinateSeparator;

  public:
                        OGRBNADataSource();
                        ~OGRBNADataSource();

    VSILFILE                *GetOutputFP() { return fpOutput; }
    bool                GetUseCRLF() { return bUseCRLF; }
    int                 GetMultiLine() { return bMultiLine; }
    int                 GetNbOutId() { return nbOutID; }
    bool                GetEllipsesAsEllipses() { return bEllipsesAsEllipses; }
    int                 GetNbPairPerLine() { return nbPairPerLine; }
    int                 GetCoordinatePrecision() { return coordinatePrecision; }
    const char*         GetCoordinateSeparator() { return pszCoordinateSeparator; }

    int                 Open( const char * pszFilename,
                              int bUpdate );

    int                 Create( const char *pszFilename,
                              char **papszOptions );

    const char*         GetName() override { return pszName; }

    int                 GetLayerCount() override { return nLayers; }
    OGRLayer*           GetLayer( int ) override;

    OGRLayer *          ICreateLayer( const char * pszLayerName,
                                    OGRSpatialReference *poSRS,
                                    OGRwkbGeometryType eType,
                                    char ** papszOptions ) override;

    int                 TestCapability( const char * ) override;
};

#endif /* ndef OGR_BNA_H_INCLUDED */
