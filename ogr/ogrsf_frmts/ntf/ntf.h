/******************************************************************************
 * $Id$
 *
 * Project:  NTF Translator
 * Purpose:  Main declarations for NTF translator.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Frank Warmerdam
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

#ifndef NTF_H_INCLUDED
#define NTF_H_INCLUDED

#include "cpl_conv.h"
#include "cpl_vsi.h"
#include "ogrsf_frmts.h"

/* -------------------------------------------------------------------- */
/*      Record types.                                                   */
/* -------------------------------------------------------------------- */
#define NRT_VHR       1                /* Volume Header Record */
#define NRT_DHR       2                /* Database Header Record */
#define NRT_FCR       5                /* Feature Classification Record */
#define NRT_SHR       7                /* Section Header Record */
#define NRT_NAMEREC  11                /* Name Record */
#define NRT_NAMEPOSTN 12               /* Name Position */
#define NRT_ATTREC   14                /* Attribute Record */
#define NRT_POINTREC 15                /* Point Record */
#define NRT_NODEREC  16                /* Node Record */
#define NRT_GEOMETRY 21                /* Geometry Record */
#define NRT_GEOMETRY3D 22              /* 3D Geometry Record */
#define NRT_LINEREC  23                /* Line Record */
#define NRT_CHAIN    24                /* Chain */
#define NRT_POLYGON  31                /* Polygon */
#define NRT_CPOLY    33                /* Complex Polygon */
#define NRT_COLLECT  34                /* Collection of features */
#define NRT_ADR      40                /* Attribute Description Record */
#define NRT_CODELIST 42                /* Codelist Record (i.e. BL2000) */
#define NRT_TEXTREC  43                /* Text */
#define NRT_TEXTPOS  44                /* Text position */
#define NRT_TEXTREP  45                /* Text representation */
#define NRT_GRIDHREC 50                /* Grid Header Record */
#define NRT_GRIDREC  51                /* Grid Data Record */
#define NRT_COMMENT  90                /* Comment record */
#define NRT_VTR      99                /* Volume Termination Record */

/* -------------------------------------------------------------------- */
/*      Product names (DBNAME) and codes.                               */
/* -------------------------------------------------------------------- */

#define NPC_UNKNOWN             0

#define NPC_LANDLINE            1
#define NPC_LANDLINE99          2
#define NTF_LANDLINE            "LAND-LINE.93"
#define NTF_LANDLINE_PLUS       "LAND-LINE.93+"

#define NPC_STRATEGI            3
#define NTF_STRATEGI            "Strategi_02.96"

#define NPC_MERIDIAN            4
#define NTF_MERIDIAN            "Meridian_01.95"

#define NPC_BOUNDARYLINE        5
#define NTF_BOUNDARYLINE        "Boundary-Line"

#define NPC_BASEDATA            6
#define NTF_BASEDATA            "BaseData.GB_01.96"

#define NPC_OSCAR_ASSET         7
#define NPC_OSCAR_TRAFFIC       8
#define NPC_OSCAR_ROUTE         9
#define NPC_OSCAR_NETWORK       10

#define NPC_ADDRESS_POINT       11

#define NPC_CODE_POINT          12
#define NPC_CODE_POINT_PLUS     13

#define NPC_LANDFORM_PROFILE_CONT 14

#define NPC_LANDRANGER_CONT     15
#define NTF_LANDRANGER_CONT     "OS_LANDRANGER_CONT"

#define NPC_LANDRANGER_DTM      16
#define NPC_LANDFORM_PROFILE_DTM 17

#define NPC_BL2000              18

#define NPC_MERIDIAN2           19
#define NTF_MERIDIAN2           "Meridian_02.01"

/************************************************************************/
/*                              NTFRecord                               */
/************************************************************************/

class NTFRecord
{
    int      nType;
    int      nLength;
    char    *pszData;

    static int      ReadPhysicalLine( VSILFILE *fp, char *pszLine );

  public:
    explicit  NTFRecord( VSILFILE * );
             ~NTFRecord();

    int      GetType() { return nType; }
    int      GetLength() { return nLength; }
    const char *GetData() { return pszData; }

    const char *GetField( int, int );
};

/************************************************************************/
/*                           NTFGenericClass                            */
/************************************************************************/

class NTFGenericClass
{
public:
    int         nFeatureCount;

    int         b3D;
    int         nAttrCount;
    char        **papszAttrNames;
    char        **papszAttrFormats;
    int         *panAttrMaxWidth;
    int         *pabAttrMultiple;

                NTFGenericClass();
                ~NTFGenericClass();

    void        CheckAddAttr( const char *, const char *, int );
    void        SetMultiple( const char * );
};

/************************************************************************/
/*                             NTFCodeList                              */
/************************************************************************/

class NTFCodeList
{
public:
    explicit     NTFCodeList( NTFRecord * );
                ~NTFCodeList();

    const char  *Lookup( const char * );

    char        szValType[3];   /* attribute code for list, i.e. AC */
    char        szFInter[6];    /* format of code values */

    int         nNumCode;
    char        **papszCodeVal; /* Short code value */
    char        **papszCodeDes; /* Long description of code */
};

/************************************************************************/
/*                              NTFAttDesc                              */
/************************************************************************/
typedef struct
{
  char  val_type     [ 2 +1];
  char  fwidth       [ 3 +1];
  char  finter       [ 5 +1];
  char  att_name     [ 100 ];

  NTFCodeList *poCodeList;

} NTFAttDesc;

class OGRNTFLayer;
class OGRNTFRasterLayer;
class OGRNTFDataSource;
class NTFFileReader;

#define MAX_REC_GROUP   100
typedef OGRFeature *(*NTFFeatureTranslator)(NTFFileReader *,
                                            OGRNTFLayer *,
                                            NTFRecord **);
typedef int (*NTFRecordGrouper)(NTFFileReader *, NTFRecord **, NTFRecord *);

/************************************************************************/
/*                            NTFFileReader                             */
/************************************************************************/

class NTFFileReader
{
    char             *pszFilename;
    OGRNTFDataSource *poDS;

    VSILFILE         *fp;

    // feature class list.
    int               nFCCount;
    char            **papszFCNum;
    char            **papszFCName;

    // attribute definitions
    int               nAttCount;
    NTFAttDesc       *pasAttDesc;

    char             *pszTileName;
    int               nCoordWidth;
    int               nZWidth;
    int               nNTFLevel;

    double            dfXYMult;
    double            dfZMult;

    double            dfXOrigin;
    double            dfYOrigin;

    double            dfTileXSize;
    double            dfTileYSize;

    double            dfScale;
    double            dfPaperToGround;

    vsi_l_offset      nStartPos;
    vsi_l_offset      nPreSavedPos;
    vsi_l_offset      nPostSavedPos;
    NTFRecord        *poSavedRecord;

    long              nSavedFeatureId;
    long              nBaseFeatureId;
    long              nFeatureCount;

    NTFRecord         *apoCGroup[MAX_REC_GROUP+1];

    char             *pszProduct;
    char             *pszPVName;
    int               nProduct;

    void              EstablishLayers();

    void              ClearCGroup();
    void              ClearDefs();

    OGRNTFLayer       *apoTypeTranslation[100];

    NTFRecordGrouper  pfnRecordGrouper;

    int               anIndexSize[100];
    NTFRecord         **apapoRecordIndex[100];
    int               bIndexBuilt;
    int               bIndexNeeded;

    void              EstablishRasterAccess();
    int               nRasterXSize;
    int               nRasterYSize;
    int               nRasterDataType;
    double            adfGeoTransform[6];

    OGRNTFRasterLayer *poRasterLayer;

    vsi_l_offset     *panColumnOffset;

    int               bCacheLines;
    int               nLineCacheSize;
    OGRGeometry     **papoLineCache;
    
    void              AddToIndexGroup( NTFRecord * poRecord );

  public:
    explicit           NTFFileReader( OGRNTFDataSource * );
                      ~NTFFileReader();

    int               Open( const char * pszFilename = nullptr );
    void              Close();
    VSILFILE         *GetFP() { return fp; }
    void              GetFPPos( vsi_l_offset *pnPos, long * pnFeatureId);
    int               SetFPPos( vsi_l_offset nPos, long nFeatureId );
    void              Reset();
    void              SetBaseFID( long nFeatureId );

    OGRGeometry      *ProcessGeometry( NTFRecord *, int * = nullptr );
    OGRGeometry      *ProcessGeometry3D( NTFRecord *, int * = nullptr );
    static int               ProcessAttDesc( NTFRecord *, NTFAttDesc * );
    int               ProcessAttRec( NTFRecord *, int *, char ***, char ***);
    int               ProcessAttRecGroup( NTFRecord **, char ***, char ***);

    NTFAttDesc       *GetAttDesc( const char * );

    void              ApplyAttributeValues( OGRFeature *, NTFRecord **, ... );

    int               ApplyAttributeValue( OGRFeature *, int, const char *,
                                           char **, char ** );

    int               ProcessAttValue( const char *pszValType,
                                       const char *pszRawValue,
                                       const char **ppszAttName,
                                       const char **ppszAttValue,
                                       const char **ppszCodeDesc );

    int               TestForLayer( OGRNTFLayer * );
    OGRFeature       *ReadOGRFeature( OGRNTFLayer * = nullptr );
    NTFRecord       **ReadRecordGroup();
    NTFRecord        *ReadRecord();
    void              SaveRecord( NTFRecord * );

    void              DumpReadable( FILE * );

    int               GetXYLen() { return nCoordWidth; }
    double            GetXYMult() { return dfXYMult; }
    double            GetXOrigin() { return dfXOrigin; }
    double            GetYOrigin() { return dfYOrigin; }
    double            GetZMult() { return dfZMult; }
    const char       *GetTileName() { return pszTileName; }
    const char       *GetFilename() { return pszFilename; }
    int               GetNTFLevel() { return nNTFLevel; }
    const char       *GetProduct() { return pszProduct; }
    const char       *GetPVName() { return pszPVName; }
    int               GetProductId() { return nProduct; }
    double            GetScale() { return dfScale; }
    double            GetPaperToGround() { return dfPaperToGround; }

    int               GetFCCount() { return nFCCount; }
    int               GetFeatureClass( int, char **, char ** );

    void              OverrideTileName( const char * );

    // Generic file index
    void              IndexFile();
    void              FreshenIndex();
    void              DestroyIndex();
    NTFRecord        *GetIndexedRecord( int, int );
    NTFRecord       **GetNextIndexedRecordGroup( NTFRecord ** );

    // Line geometry cache
    OGRGeometry      *CacheGetByGeomId( int );
    void              CacheAddByGeomId( int, OGRGeometry * );
    void              CacheClean();
    void              CacheLineGeometryInGroup( NTFRecord ** );

    int               FormPolygonFromCache( OGRFeature * );

    // just for use of OGRNTFDatasource
    void              EstablishLayer( const char *, OGRwkbGeometryType,
                                      NTFFeatureTranslator, int,
                                      NTFGenericClass *, ... );

    // Raster related
    int               IsRasterProduct();
    int               GetRasterXSize() { return nRasterXSize; }
    int               GetRasterYSize() { return nRasterYSize; }
    int               GetRasterDataType() { return nRasterDataType; }
    double           *GetGeoTransform() { return adfGeoTransform; }
    CPLErr            ReadRasterColumn( int, float * );

};

/************************************************************************/
/*                             OGRNTFLayer                              */
/************************************************************************/

class OGRNTFLayer final: public OGRLayer
{
    OGRFeatureDefn     *poFeatureDefn;
    NTFFeatureTranslator pfnTranslator;

    OGRNTFDataSource   *poDS;

    int                 iCurrentReader;
    vsi_l_offset        nCurrentPos;
    long                nCurrentFID;

  public:
                        OGRNTFLayer( OGRNTFDataSource * poDS,
                                     OGRFeatureDefn * poFeatureDefine,
                                     NTFFeatureTranslator pfnTranslator );

                        ~OGRNTFLayer();

    void                ResetReading() override;
    OGRFeature *        GetNextFeature() override;

#ifdef notdef
    OGRFeature         *GetFeature( GIntBig nFeatureId );
    OGRErr              ISetFeature( OGRFeature *poFeature );
    OGRErr              ICreateFeature( OGRFeature *poFeature );
#endif

    OGRFeatureDefn *    GetLayerDefn() override { return poFeatureDefn; }

#ifdef notdef
    GIntBig             GetFeatureCount( int );
#endif

    int                 TestCapability( const char * ) override;

    // special to NTF
    OGRFeature         *FeatureTranslate( NTFFileReader *, NTFRecord ** );
};

/************************************************************************/
/*                       OGRNTFFeatureClassLayer                        */
/************************************************************************/

class OGRNTFFeatureClassLayer final: public OGRLayer
{
    OGRFeatureDefn     *poFeatureDefn;
    OGRGeometry        *poFilterGeom;

    OGRNTFDataSource   *poDS;

    GIntBig            iCurrentFC;

  public:
    explicit             OGRNTFFeatureClassLayer( OGRNTFDataSource * poDS );
                        ~OGRNTFFeatureClassLayer();

    OGRGeometry *       GetSpatialFilter() override { return poFilterGeom; }
    void                SetSpatialFilter( OGRGeometry * ) override;
    virtual void        SetSpatialFilter( int iGeomField, OGRGeometry *poGeom ) override
                { OGRLayer::SetSpatialFilter(iGeomField, poGeom); }

    void                ResetReading() override;
    OGRFeature *        GetNextFeature() override;

    OGRFeature         *GetFeature( GIntBig nFeatureId ) override;

    OGRFeatureDefn *    GetLayerDefn() override { return poFeatureDefn; }

    GIntBig             GetFeatureCount( int = TRUE ) override;

    int                 TestCapability( const char * ) override;
};

/************************************************************************/
/*                          OGRNTFRasterLayer                           */
/************************************************************************/

class OGRNTFRasterLayer final: public OGRLayer
{
    OGRFeatureDefn     *poFeatureDefn;
    OGRGeometry        *poFilterGeom;

    NTFFileReader      *poReader;

    float              *pafColumn;
    int                 iColumnOffset;

    GIntBig             iCurrentFC;

    int                 nDEMSample;
    GIntBig             nFeatureCount;

  public:
                        OGRNTFRasterLayer( OGRNTFDataSource * poDS,
                                           NTFFileReader * poReaderIn );
                        virtual ~OGRNTFRasterLayer();

    OGRGeometry *       GetSpatialFilter() override { return poFilterGeom; }
    void                SetSpatialFilter( OGRGeometry * ) override;
    virtual void        SetSpatialFilter( int iGeomField, OGRGeometry *poGeom ) override
                { OGRLayer::SetSpatialFilter(iGeomField, poGeom); }

    void                ResetReading() override;
    OGRFeature *        GetNextFeature() override;

    OGRFeature         *GetFeature( GIntBig nFeatureId ) override;

    OGRFeatureDefn *    GetLayerDefn() override { return poFeatureDefn; }

    GIntBig             GetFeatureCount( int = TRUE ) override;

    int                 TestCapability( const char * ) override;
};

/************************************************************************/
/*                           OGRNTFDataSource                           */
/************************************************************************/

class OGRNTFDataSource final: public OGRDataSource
{
    char                *pszName;

    int                 nLayers;
    OGRLayer            **papoLayers;

    OGRNTFFeatureClassLayer *poFCLayer;

    int                 iCurrentFC;
    int                 iCurrentReader;
    vsi_l_offset        nCurrentPos;
    long                nCurrentFID;

    int                 nNTFFileCount;
    NTFFileReader       **papoNTFFileReader;

    int                 nFCCount;
    char              **papszFCNum;
    char              **papszFCName;

    OGRSpatialReference *poSpatialRef;

    NTFGenericClass     aoGenericClass[100];

    char                **papszOptions;

    void                EnsureTileNameUnique( NTFFileReader * );

    CPL_DISALLOW_COPY_ASSIGN(OGRNTFDataSource)

  public:
                        OGRNTFDataSource();
                        ~OGRNTFDataSource();

    void                SetOptionList( char ** );
    const char         *GetOption( const char * );

    int                 Open( const char * pszName, int bTestOpen = FALSE,
                              char ** papszFileList = nullptr );

    const char          *GetName() override { return pszName; }
    int                 GetLayerCount() override;
    OGRLayer            *GetLayer( int ) override;
    int                 TestCapability( const char * ) override;

    // Note: these are specific to NTF for now, but eventually might
    // might be available as part of a more object oriented approach to
    // features like that in FME or SFCORBA.
    virtual void        ResetReading() override;
    virtual OGRFeature* GetNextFeature( OGRLayer** ppoBelongingLayer,
                                        double* pdfProgressPct,
                                        GDALProgressFunc pfnProgress,
                                        void* pProgressData ) override;

    // these are only for the use of the NTFFileReader class.
    OGRNTFLayer         *GetNamedLayer( const char * );
    void                 AddLayer( OGRLayer * );

    // Mainly for OGRNTFLayer class
    int                  GetFileCount() { return nNTFFileCount; }
    NTFFileReader       *GetFileReader(int i) { return papoNTFFileReader[i]; }

    int                  GetFCCount() { return nFCCount; }
    int                  GetFeatureClass( int, char **, char ** );

    OGRSpatialReference *DSGetSpatialRef() { return poSpatialRef; }

    NTFGenericClass     *GetGClass( int i ) { return aoGenericClass + i; }
    void                WorkupGeneric( NTFFileReader * );
    void                EstablishGenericLayers();
};

/************************************************************************/
/*                          Support functions.                          */
/************************************************************************/
int NTFArcCenterFromEdgePoints( double x_c0, double y_c0,
                                double x_c1, double y_c1,
                                double x_c2, double y_c2,
                                double *x_center, double *y_center );
OGRGeometry *
NTFStrokeArcToOGRGeometry_Points( double dfStartX, double dfStartY,
                                  double dfAlongX, double dfAlongY,
                                  double dfEndX, double dfEndY,
                                  int nVertexCount );
OGRGeometry *
NTFStrokeArcToOGRGeometry_Angles( double dfCenterX, double dfCenterY,
                                  double dfRadius,
                                  double dfStartAngle, double dfEndAngle,
                                  int nVertexCount );

#endif /* ndef NTF_H_INCLUDED */
