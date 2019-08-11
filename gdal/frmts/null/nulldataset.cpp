/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  NULL driver.
 * Author:   Even Rouault, <even dot rouault at spatialys dot org>
 *
 ******************************************************************************
 * Copyright (c) 2012-2017, Even Rouault, <even dot rouault at spatialys dot org>
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

#include "gdal_priv.h"
#include "ogrsf_frmts.h"

extern "C" void GDALRegister_NULL();

/************************************************************************/
/*                          GDALNullDataset                             */
/************************************************************************/

class GDALNullDataset final: public GDALDataset
{
            int                 m_nLayers;
            OGRLayer**          m_papoLayers;

    public:
            GDALNullDataset();
            virtual ~GDALNullDataset();

            virtual int         GetLayerCount() override { return m_nLayers; }
            virtual OGRLayer   *GetLayer( int ) override;

            virtual OGRLayer    *ICreateLayer( const char *pszLayerName,
                                            OGRSpatialReference *poSRS,
                                            OGRwkbGeometryType eType,
                                            char **papszOptions ) override;

            virtual int         TestCapability( const char * ) override;

            virtual CPLErr      _SetProjection(const char*) override;
            CPLErr SetSpatialRef(const OGRSpatialReference* poSRS) override {
                return OldSetProjectionFromSetSpatialRef(poSRS);
            }

            virtual CPLErr      SetGeoTransform(double*) override;

            static GDALDataset* Open(GDALOpenInfo* poOpenInfo);
            static GDALDataset* Create(const char *pszFilename,
                                     int nXSize, int nYSize, int nBands,
                                     GDALDataType eType, char **papszOptions);

};

/************************************************************************/
/*                           GDALNullLayer                              */
/************************************************************************/

class GDALNullRasterBand final: public GDALRasterBand
{
    public:
            explicit GDALNullRasterBand(GDALDataType eDT);

            virtual CPLErr IReadBlock(int, int, void*) override;
            virtual CPLErr IWriteBlock(int, int, void*) override;
            virtual CPLErr IRasterIO( GDALRWFlag eRWFlag,
                                  int nXOff, int nYOff, int nXSize, int nYSize,
                                  void * pData, int nBufXSize, int nBufYSize,
                                  GDALDataType eBufType,
                                  GSpacing nPixelSpace, GSpacing nLineSpace,
                                  GDALRasterIOExtraArg* psExtraArg ) override;

};

/************************************************************************/
/*                           GDALNullLayer                              */
/************************************************************************/

class GDALNullLayer final: public OGRLayer
{
    OGRFeatureDefn      *poFeatureDefn;
    OGRSpatialReference *poSRS;

  public:
                        GDALNullLayer( const char *pszLayerName,
                                      OGRSpatialReference *poSRS,
                                      OGRwkbGeometryType eType );
    virtual             ~GDALNullLayer();

    virtual OGRFeatureDefn *GetLayerDefn() override {return poFeatureDefn;}
    virtual OGRSpatialReference * GetSpatialRef() override { return poSRS; }

    virtual void        ResetReading() override {}
    virtual int         TestCapability( const char * ) override;

    virtual OGRFeature *GetNextFeature() override { return nullptr; }

    virtual OGRErr      ICreateFeature( OGRFeature * ) override { return OGRERR_NONE; }

    virtual OGRErr      CreateField( OGRFieldDefn *poField,
                                     int bApproxOK = TRUE ) override;
};


/************************************************************************/
/*                           GDALNullRasterBand()                       */
/************************************************************************/

GDALNullRasterBand::GDALNullRasterBand(GDALDataType eDT)
{
    eDataType = eDT;
    nBlockXSize = 256;
    nBlockYSize = 256;
}

/************************************************************************/
/*                             IRasterIO()                              */
/************************************************************************/

CPLErr GDALNullRasterBand::IRasterIO( GDALRWFlag eRWFlag,
                                  int nXOff, int nYOff, int nXSize, int nYSize,
                                  void * pData, int nBufXSize, int nBufYSize,
                                  GDALDataType eBufType,
                                  GSpacing nPixelSpace, GSpacing nLineSpace,
                                  GDALRasterIOExtraArg* psExtraArg )
{
    if( eRWFlag == GF_Write )
        return CE_None;
    if( psExtraArg->eResampleAlg != GRIORA_NearestNeighbour &&
        (nBufXSize != nXSize || nBufYSize != nYSize) )
    {
        return GDALRasterBand::IRasterIO(eRWFlag, nXOff, nYOff, nXSize, nYSize,
                                         pData, nBufXSize, nBufYSize,
                                         eBufType,
                                         nPixelSpace, nLineSpace,
                                         psExtraArg);
    }
    if( nPixelSpace == GDALGetDataTypeSizeBytes(eBufType) &&
        nLineSpace == nPixelSpace * nBufXSize )
    {
        memset(pData, 0, nLineSpace * nBufYSize);
    }
    else
    {
        for( int iY = 0; iY < nBufYSize; iY++ )
        {
            double dfZero = 0;
            GDALCopyWords(&dfZero, GDT_Float64, 0,
                        reinterpret_cast<GByte*>(pData) + iY * nLineSpace,
                        eBufType, static_cast<int>(nPixelSpace),
                        nBufXSize);
        }
    }
    return CE_None;
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr GDALNullRasterBand::IReadBlock(int, int, void* pData)
{
    memset(pData, 0,
           nBlockXSize * nBlockYSize * GDALGetDataTypeSizeBytes(eDataType));
    return CE_None;
}

/************************************************************************/
/*                             IWriteBlock()                            */
/************************************************************************/

CPLErr GDALNullRasterBand::IWriteBlock(int, int, void*)
{
    return CE_None;
}

/************************************************************************/
/*                          GDALNullDataset()                           */
/************************************************************************/

GDALNullDataset::GDALNullDataset() :
    m_nLayers(0),
    m_papoLayers(nullptr)
{
    eAccess = GA_Update;
}

/************************************************************************/
/*                         ~GDALNullDataset()                           */
/************************************************************************/

GDALNullDataset::~GDALNullDataset()
{
    for(int i=0;i<m_nLayers;i++)
        delete m_papoLayers[i];
    CPLFree(m_papoLayers);
}

/************************************************************************/
/*                           ICreateLayer()                             */
/************************************************************************/

OGRLayer    *GDALNullDataset::ICreateLayer( const char *pszLayerName,
                                             OGRSpatialReference *poSRS,
                                             OGRwkbGeometryType eType,
                                             char ** )
{
    m_papoLayers = static_cast<OGRLayer**>(CPLRealloc(m_papoLayers,
                                    sizeof(OGRLayer*) * (m_nLayers + 1)));
    m_papoLayers[m_nLayers] = new GDALNullLayer(pszLayerName, poSRS, eType);
    m_nLayers ++;
    return m_papoLayers[m_nLayers-1];
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int GDALNullDataset::TestCapability( const char * pszCap )

{
    if( EQUAL(pszCap, ODsCCreateLayer) )
        return TRUE;
    if( EQUAL(pszCap, ODsCRandomLayerWrite) )
        return TRUE;
    return FALSE;
}

/************************************************************************/
/*                              GetLayer()                              */
/************************************************************************/

OGRLayer *GDALNullDataset::GetLayer( int iLayer )

{
    if( iLayer < 0 || iLayer >= m_nLayers )
        return nullptr;
    else
        return m_papoLayers[iLayer];
}

/************************************************************************/
/*                           SetProjection()                            */
/************************************************************************/

CPLErr GDALNullDataset::_SetProjection(const char*)

{
    return CE_None;
}

/************************************************************************/
/*                          SetGeoTransform()                           */
/************************************************************************/

CPLErr GDALNullDataset::SetGeoTransform(double *)

{
    return CE_None;
}

/************************************************************************/
/*                               Open()                                 */
/************************************************************************/

GDALDataset* GDALNullDataset::Open(GDALOpenInfo* poOpenInfo)
{
    if( !STARTS_WITH_CI(poOpenInfo->pszFilename, "NULL:") )
        return nullptr;

    const char* pszStr = poOpenInfo->pszFilename + strlen("NULL:");
    char** papszTokens = CSLTokenizeString2(pszStr, ",", 0);
    int nXSize = atoi(CSLFetchNameValueDef(papszTokens, "width", "512"));
    int nYSize = atoi(CSLFetchNameValueDef(papszTokens, "height", "512"));
    int nBands = atoi(CSLFetchNameValueDef(papszTokens, "bands", "1"));
    const char* pszDTName = CSLFetchNameValueDef(papszTokens, "type", "Byte");
    GDALDataType eDT = GDT_Byte;
    for( int iType = 1; iType < GDT_TypeCount; iType++ )
    {
        if( GDALGetDataTypeName((GDALDataType)iType) != nullptr
            && EQUAL(GDALGetDataTypeName((GDALDataType)iType),
                        pszDTName) )
        {
           eDT = (GDALDataType) iType;
           break;
        }
    }
    CSLDestroy(papszTokens);

    return Create("", nXSize, nYSize, nBands, eDT, nullptr);
}

/************************************************************************/
/*                              Create()                                */
/************************************************************************/

GDALDataset *GDALNullDataset::Create(const char *,
                                     int nXSize, int nYSize, int nBands,
                                     GDALDataType eType, char **)
{
    GDALNullDataset* poDS = new GDALNullDataset();
    poDS->nRasterXSize = nXSize;
    poDS->nRasterYSize = nYSize;
    for( int i = 0; i < nBands; i++ )
        poDS->SetBand(i + 1, new GDALNullRasterBand(eType));
    return poDS;
}

/************************************************************************/
/*                           GDALNullLayer()                            */
/************************************************************************/

GDALNullLayer::GDALNullLayer( const char *pszLayerName,
                            OGRSpatialReference *poSRSIn,
                            OGRwkbGeometryType eType ) :
    poFeatureDefn(new OGRFeatureDefn(pszLayerName)),
    poSRS(poSRSIn)
{
    SetDescription( poFeatureDefn->GetName() );
    poFeatureDefn->SetGeomType(eType);
    poFeatureDefn->Reference();

    if( poSRS )
        poSRS->Reference();
}

/************************************************************************/
/*                           ~GDALNullLayer()                           */
/************************************************************************/

GDALNullLayer::~GDALNullLayer()
{
    poFeatureDefn->Release();

    if (poSRS)
        poSRS->Release();
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int GDALNullLayer::TestCapability( const char * pszCap )

{
    if( EQUAL(pszCap, OLCSequentialWrite) )
        return TRUE;
    if( EQUAL(pszCap, OLCCreateField) )
        return TRUE;
    return FALSE;
}

/************************************************************************/
/*                             CreateField()                            */
/************************************************************************/

OGRErr GDALNullLayer::CreateField( OGRFieldDefn *poField, int )
{
    poFeatureDefn->AddFieldDefn(poField);
    return OGRERR_NONE;
}

/************************************************************************/
/*                        GDALRegister_NULL()                           */
/************************************************************************/

void GDALRegister_NULL()

{
    if( GDALGetDriverByName( "NULL" ) != nullptr )
        return;

    GDALDriver  *poDriver;
    poDriver = new GDALDriver();

    poDriver->SetDescription( "NULL" );
    poDriver->SetMetadataItem( GDAL_DMD_CONNECTION_PREFIX, "NULL: ");
    poDriver->SetMetadataItem( GDAL_DCAP_RASTER, "YES" );
    poDriver->SetMetadataItem( GDAL_DCAP_VECTOR, "YES" );
    poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, "NULL" );

    poDriver->SetMetadataItem( GDAL_DMD_CREATIONFIELDDATATYPES,
                               "Integer Integer64 Real String Date DateTime "
                               "Binary IntegerList Integer64List "
                               "RealList StringList" );

    poDriver->pfnOpen = GDALNullDataset::Open;
    poDriver->pfnCreate = GDALNullDataset::Create;

    GetGDALDriverManager()->RegisterDriver( poDriver );
}
