/******************************************************************************
 * $Id$
 *
 * Project:  PCIDSK Database File
 * Purpose:  Read/write PCIDSK Database File used by the PCI software, using
 *           the external PCIDSK library.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2009, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2009-2013, Even Rouault <even dot rouault at mines-paris dot org>
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

#ifndef PCIDSKDATASET2_H_INCLUDED
#define PCIDSKDATASET2_H_INCLUDED

#define GDAL_PCIDSK_DRIVER

#include "pcidsk.h"
#include "pcidsk_pct.h"
#include "ogrsf_frmts.h"
#include "pcidsk_vectorsegment.h"
#include "gdal_pam.h"
#include "cpl_string.h"
#include "ogr_spatialref.h"

using namespace PCIDSK;

class OGRPCIDSKLayer;

/************************************************************************/
/*                              PCIDSK2Dataset                           */
/************************************************************************/

class PCIDSK2Dataset : public GDALPamDataset
{
    friend class PCIDSK2Band;

    CPLString   osSRS;
    CPLString   osLastMDValue;
    char      **papszLastMDListValue;

    PCIDSKFile  *poFile;

    std::vector<OGRPCIDSKLayer*> apoLayers;

    static GDALDataType  PCIDSKTypeToGDAL( eChanType eType );
    void                 ProcessRPC();

  public:
                PCIDSK2Dataset();
                ~PCIDSK2Dataset();

    static int           Identify( GDALOpenInfo * );
    static GDALDataset  *Open( GDALOpenInfo * );
    static GDALDataset  *LLOpen( const char *pszFilename, PCIDSK::PCIDSKFile *,
                                 GDALAccess eAccess,
                                 char** papszSiblingFiles = NULL );
    static GDALDataset  *Create( const char * pszFilename,
                                 int nXSize, int nYSize, int nBands,
                                 GDALDataType eType,
                                 char **papszParmList );

    char              **GetFileList(void);
    CPLErr              GetGeoTransform( double * padfTransform );
    CPLErr              SetGeoTransform( double * );
    const char         *GetProjectionRef();
    CPLErr              SetProjection( const char * );

    virtual char      **GetMetadataDomainList();
    CPLErr              SetMetadata( char **, const char * );
    char              **GetMetadata( const char* );
    CPLErr              SetMetadataItem(const char*,const char*,const char*);
    const char         *GetMetadataItem( const char*, const char*);

    virtual void FlushCache(void);

    virtual CPLErr IBuildOverviews( const char *, int, int *,
                                    int, int *, GDALProgressFunc, void * );

    virtual int                 GetLayerCount() { return (int) apoLayers.size(); }
    virtual OGRLayer            *GetLayer( int );

    virtual int                 TestCapability( const char * );

    virtual OGRLayer           *ICreateLayer( const char *, OGRSpatialReference *,
                                     OGRwkbGeometryType, char ** );
};

/************************************************************************/
/*                             PCIDSK2Band                              */
/************************************************************************/

class PCIDSK2Band : public GDALPamRasterBand
{
    friend class PCIDSK2Dataset;

    PCIDSKChannel *poChannel;
    PCIDSKFile    *poFile;

    void        RefreshOverviewList();
    std::vector<PCIDSK2Band*> apoOverviews;
    
    CPLString   osLastMDValue;
    char      **papszLastMDListValue;

    bool        CheckForColorTable();
    GDALColorTable *poColorTable;
    bool        bCheckedForColorTable;
    int         nPCTSegNumber;

    char      **papszCategoryNames;

    void        Initialize();

  public:
                PCIDSK2Band( PCIDSK2Dataset *, PCIDSKFile *, int );
                PCIDSK2Band( PCIDSKChannel * );
                ~PCIDSK2Band();

    virtual CPLErr IReadBlock( int, int, void * );
    virtual CPLErr IWriteBlock( int, int, void * );

    virtual int        GetOverviewCount();
    virtual GDALRasterBand *GetOverview(int);

    virtual GDALColorInterp GetColorInterpretation();
    virtual GDALColorTable *GetColorTable();
    virtual CPLErr SetColorTable( GDALColorTable * ); 

    virtual void        SetDescription( const char * );

    virtual char      **GetMetadataDomainList();
    CPLErr              SetMetadata( char **, const char * );
    char              **GetMetadata( const char* );
    CPLErr              SetMetadataItem(const char*,const char*,const char*);
    const char         *GetMetadataItem( const char*, const char*);

    virtual char      **GetCategoryNames();
};

/************************************************************************/
/*                             OGRPCIDSKLayer                              */
/************************************************************************/

class OGRPCIDSKLayer : public OGRLayer
{
    PCIDSK::PCIDSKVectorSegment *poVecSeg;
    PCIDSK::PCIDSKSegment       *poSeg;

    OGRFeatureDefn     *poFeatureDefn;

    OGRFeature *        GetNextUnfilteredFeature();

    int                 iRingStartField;
    PCIDSK::ShapeId     hLastShapeId;

    bool                bUpdateAccess;

    OGRSpatialReference *poSRS;

  public:
    OGRPCIDSKLayer( PCIDSK::PCIDSKSegment*, bool bUpdate );
    ~OGRPCIDSKLayer();

    void                ResetReading();
    OGRFeature *        GetNextFeature();
    OGRFeature         *GetFeature( long nFeatureId );
    OGRErr              SetFeature( OGRFeature *poFeature );

    OGRFeatureDefn *    GetLayerDefn() { return poFeatureDefn; }

    int                 TestCapability( const char * );

    OGRErr              DeleteFeature( long nFID );
    OGRErr              CreateFeature( OGRFeature *poFeature );
    virtual OGRErr      CreateField( OGRFieldDefn *poField,
                                     int bApproxOK = TRUE );

    int                 GetFeatureCount( int );
    OGRErr              GetExtent( OGREnvelope *psExtent, int bForce );
};

#endif /*  PCIDSKDATASET2_H_INCLUDED */
