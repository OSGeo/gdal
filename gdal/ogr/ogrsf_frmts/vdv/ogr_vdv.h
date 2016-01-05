/******************************************************************************
 * $Id$
 *
 * Project:  VDV Translator
 * Purpose:  Implements OGRVDVDriver.
 * Author:   Even Rouault, even.rouault at spatialys.com
 *
 ******************************************************************************
 * Copyright (c) 2015, Even Rouault <even.rouault at spatialys.com>
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

#ifndef OGR_VDV_H_INCLUDED
#define OGR_VDV_H_INCLUDED

#include "ogrsf_frmts.h"
#include <vector>
#include <map>

class OGRVDVDataSource;

/************************************************************************/
/*                        OGRIDFDataSource                              */
/************************************************************************/

class OGRIDFDataSource : public GDALDataset
{
    VSILFILE*           m_fpL;
    bool                m_bHasParsed;
    GDALDataset        *m_poMemDS;

    void                Parse();

  public:
                        OGRIDFDataSource(VSILFILE* fpL);
                        ~OGRIDFDataSource();

    virtual int                 GetLayerCount();
    virtual OGRLayer*           GetLayer( int );
};

/************************************************************************/
/*                          OGRVDVLayer                                 */
/************************************************************************/

class OGRVDVLayer: public OGRLayer
{
    VSILFILE*           m_fpL;
    bool                m_bOwnFP;
    bool                m_bRecodeFromLatin1;
    vsi_l_offset        m_nStartOffset;
    vsi_l_offset        m_nCurOffset;
    GIntBig             m_nTotalFeatureCount;
    GIntBig             m_nFID;
    OGRFeatureDefn*     m_poFeatureDefn;
    bool                m_bEOF;
    int                 m_iLongitudeVDV452;
    int                 m_iLatitudeVDV452;

  public:
                        OGRVDVLayer(const CPLString& osTableName,
                                    VSILFILE* fpL,
                                    bool bOwnFP,
                                    bool bRecodeFromLatin1,
                                    vsi_l_offset nStartOffset);
                        ~OGRVDVLayer();

        virtual void            ResetReading();
        virtual OGRFeature     *GetNextFeature();
        virtual GIntBig         GetFeatureCount(int bForce);
        virtual OGRFeatureDefn *GetLayerDefn() { return m_poFeatureDefn; }
        virtual int             TestCapability(const char* pszCap);

        void                    SetFeatureCount(GIntBig nTotalFeatureCount)
                            { m_nTotalFeatureCount = nTotalFeatureCount; }
};


class OGRVDV452Field
{
    public:
        CPLString osEnglishName;
        CPLString osGermanName;
        CPLString osType;
        int       nWidth;

            OGRVDV452Field() : nWidth(0) {}
};

class OGRVDV452Table
{
    public:
        CPLString osEnglishName;
        CPLString osGermanName;
        std::vector<OGRVDV452Field> aosFields;
};

class OGRVDV452Tables
{
    public:
        std::vector<OGRVDV452Table*> aosTables;
        std::map<CPLString, OGRVDV452Table*> oMapEnglish;
        std::map<CPLString, OGRVDV452Table*> oMapGerman;

            OGRVDV452Tables() {}
            ~OGRVDV452Tables()
            {
                for(size_t i=0;i<aosTables.size();i++)
                    delete aosTables[i];
            }
};

/************************************************************************/
/*                          OGRVDVWriterLayer                           */
/************************************************************************/

class OGRVDVWriterLayer: public OGRLayer
{
    OGRVDVDataSource*   m_poDS;
    OGRFeatureDefn*     m_poFeatureDefn;
    bool                m_bWritePossible; 
    VSILFILE*           m_fpL;
    bool                m_bOwnFP;
    GIntBig             m_nFeatureCount;
    OGRVDV452Table     *m_poVDV452Table;
    CPLString           m_osVDV452Lang;
    bool                m_bProfileStrict;
    int                 m_iLongitudeVDV452;
    int                 m_iLatitudeVDV452;

        bool                    WriteSchemaIfNeeded();

  public:
                        OGRVDVWriterLayer(OGRVDVDataSource *poDS,
                                          const char* pszName,
                                          VSILFILE* fpL,
                                          bool bOwnFP,
                                          OGRVDV452Table* poVDV452Table = NULL,
                                          const CPLString& osVDV452Lang = "",
                                          bool bProfileStrict = false
                                          );
                        ~OGRVDVWriterLayer();

        virtual void            ResetReading();
        virtual OGRFeature     *GetNextFeature();
        virtual OGRFeatureDefn *GetLayerDefn() { return m_poFeatureDefn; }
        virtual int             TestCapability(const char* pszCap);
        virtual OGRErr          CreateField(OGRFieldDefn* poFieldDefn, int bApproxOK = TRUE);
        virtual OGRErr          ICreateFeature(OGRFeature* poFeature);
        virtual GIntBig         GetFeatureCount(int bForce = TRUE);

        void                    StopAsCurrentLayer();
};

/************************************************************************/
/*                        OGRVDVDataSource                              */
/************************************************************************/

class OGRVDVDataSource : public GDALDataset
{
    CPLString           m_osFilename;
    VSILFILE*           m_fpL;
    bool                m_bUpdate;
    bool                m_bSingleFile;
    bool                m_bNew;
    bool                m_bLayersDetected;
    int                 m_nLayerCount;
    OGRLayer          **m_papoLayers;
    OGRVDVWriterLayer  *m_poCurrentWriterLayer;
    bool                m_bMustWriteEof;
    bool                m_bVDV452Loaded;
    OGRVDV452Tables     m_oVDV452Tables;

    void                DetectLayers();

  public:
                        OGRVDVDataSource(const char* pszFilename,
                                         VSILFILE* fpL,
                                         bool bUpdate,
                                         bool bSingleFile,
                                         bool bNew);
                        ~OGRVDVDataSource();

    virtual int                 GetLayerCount();
    virtual OGRLayer*           GetLayer( int );
    virtual OGRLayer*           ICreateLayer( const char *pszLayerName,
                                      OGRSpatialReference * /*poSpatialRef*/,
                                      OGRwkbGeometryType /*eGType*/,
                                      char ** papszOptions  );
    virtual int                 TestCapability( const char * pszCap );

    void                        SetCurrentWriterLayer(OGRVDVWriterLayer* poLayer);

    static GDALDataset*    Open( GDALOpenInfo* poOpenInfo );
    static GDALDataset*    Create( const char * pszName,
                                        int /*nXSize*/, int /*nYSize*/, int /*nBands*/,
                                        GDALDataType /*eType*/,
                                        char ** papszOptions );
};


#endif /* ndef OGR_VDV_H_INCLUDED */
