/******************************************************************************
 * $Id$
 *
 * Project:  Arc/Info Coverage (E00 & Binary) Reader
 * Purpose:  Declarations for OGR wrapper classes for coverage access.
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

#ifndef OGR_AVC_H_INCLUDED
#define OGR_AVC_H_INCLUDED

#include "ogrsf_frmts.h"
#include "avc.h"

class OGRAVCDataSource;

/************************************************************************/
/*                             OGRAVCLayer                              */
/************************************************************************/

class OGRAVCLayer CPL_NON_FINAL: public OGRLayer
{
  protected:
    OGRFeatureDefn      *poFeatureDefn;

    OGRAVCDataSource    *poDS;

    AVCFileType         eSectionType;

    bool                m_bEOF = false;

    int                 SetupFeatureDefinition( const char *pszName );
    bool                AppendTableDefinition( AVCTableDef *psTableDef );

    bool                MatchesSpatialFilter( void * );
    OGRFeature          *TranslateFeature( void * );

    bool                TranslateTableFields( OGRFeature *poFeature,
                                              int nFieldBase,
                                              AVCTableDef *psTableDef,
                                              AVCField *pasFields );

  public:
                        OGRAVCLayer( AVCFileType eSectionType,
                                     OGRAVCDataSource *poDS );
    virtual ~OGRAVCLayer();

    OGRFeatureDefn *    GetLayerDefn() override { return poFeatureDefn; }

    virtual int         TestCapability( const char * ) override;
};

/************************************************************************/
/*                         OGRAVCDataSource                             */
/************************************************************************/

class OGRAVCDataSource CPL_NON_FINAL: public OGRDataSource
{
  protected:
    bool                 m_bSRSFetched = false;
    OGRSpatialReference *poSRS;
    char                *pszCoverageName;

  public:
                        OGRAVCDataSource();
    virtual ~OGRAVCDataSource();

    virtual OGRSpatialReference *DSGetSpatialRef();

    const char          *GetCoverageName();
};

/* ==================================================================== */
/*      Binary Coverage Classes                                         */
/* ==================================================================== */

class OGRAVCBinDataSource;

/************************************************************************/
/*                            OGRAVCBinLayer                            */
/************************************************************************/

class OGRAVCBinLayer final: public OGRAVCLayer
{
    AVCE00Section       *m_psSection;
    AVCBinFile          *hFile;

    OGRAVCBinLayer      *poArcLayer;
    bool                bNeedReset;

    char                szTableName[128];
    AVCBinFile          *hTable;
    int                 nTableBaseField;
    int                 nTableAttrIndex;

    int                 nNextFID;

    bool                FormPolygonGeometry( OGRFeature *poFeature,
                                             AVCPal *psPAL );

    bool                CheckSetupTable();
    bool                AppendTableFields( OGRFeature *poFeature );

  public:
                        OGRAVCBinLayer( OGRAVCBinDataSource *poDS,
                                        AVCE00Section *psSectionIn );

                        ~OGRAVCBinLayer();

    void                ResetReading() override;
    OGRFeature *        GetNextFeature() override;
    OGRFeature *        GetFeature( GIntBig nFID ) override;

    int                 TestCapability( const char * ) override;
};

/************************************************************************/
/*                         OGRAVCBinDataSource                          */
/************************************************************************/

class OGRAVCBinDataSource final: public OGRAVCDataSource
{
    OGRLayer            **papoLayers;
    int                 nLayers;

    char                *pszName;

    AVCE00ReadPtr       psAVC;

  public:
                        OGRAVCBinDataSource();
                        ~OGRAVCBinDataSource();

    int                 Open( const char *, int bTestOpen );

    const char          *GetName() override { return pszName; }
    int                 GetLayerCount() override { return nLayers; }
    OGRLayer            *GetLayer( int ) override;

    int                 TestCapability( const char * ) override;

    AVCE00ReadPtr       GetInfo() { return psAVC; }
};

/* ==================================================================== */
/*      E00 (ASCII) Coverage Classes                                    */
/* ==================================================================== */

/************************************************************************/
/*                            OGRAVCE00Layer                            */
/************************************************************************/
class OGRAVCE00Layer final: public OGRAVCLayer
{
    AVCE00Section       *psSection;
    AVCE00ReadE00Ptr    psRead;
    OGRAVCE00Layer      *poArcLayer;
    int                 nFeatureCount;
    bool                bNeedReset;
    bool                bLastWasSequential = false;
    int                 nNextFID;

    AVCE00Section       *psTableSection;
    AVCE00ReadE00Ptr    psTableRead;
    char                *pszTableFilename;
    int                 nTablePos;
    int                 nTableBaseField;
    int                 nTableAttrIndex;

    bool                FormPolygonGeometry( OGRFeature *poFeature,
                                             AVCPal *psPAL );
  public:
                        OGRAVCE00Layer( OGRAVCDataSource *poDS,
                                        AVCE00Section *psSectionIn );

                        ~OGRAVCE00Layer();

    void                ResetReading() override;
    OGRFeature *        GetNextFeature() override;
    OGRFeature *GetFeature( GIntBig nFID ) override;
    GIntBig GetFeatureCount(int bForce) override;
    bool CheckSetupTable(AVCE00Section *psTblSectionIn);
    bool AppendTableFields( OGRFeature *poFeature );
};

/************************************************************************/
/*                         OGRAVCE00DataSource                          */
/************************************************************************/

class OGRAVCE00DataSource final: public OGRAVCDataSource
{
    int nLayers;
    char *pszName;
    AVCE00ReadE00Ptr psE00;
    OGRAVCE00Layer **papoLayers;

  protected:
    int CheckAddTable(AVCE00Section *psTblSection);

  public:
    OGRAVCE00DataSource();
    virtual ~OGRAVCE00DataSource();

    int Open(const char *, int bTestOpen);

    AVCE00ReadE00Ptr GetInfo() { return psE00; }
    const char *GetName() override { return pszName; }
    int GetLayerCount() override { return nLayers; }

    OGRLayer *GetLayer( int ) override;
    int TestCapability( const char * ) override;
    virtual OGRSpatialReference *DSGetSpatialRef() override;
};

#endif /* OGR_AVC_H_INCLUDED */
