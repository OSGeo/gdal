/******************************************************************************
 * $Id$
 *
 * Project:  NTF Translator
 * Purpose:  Main declarations for NTF translator.
 * Author:   Frank Warmerdam, warmerda@home.com
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
 ******************************************************************************
 *
 * $Log$
 * Revision 1.6  1999/09/14 01:34:36  warmerda
 * added scale support, and generation of TEXT_HT_GROUND
 *
 * Revision 1.5  1999/09/13 14:07:21  warmerda
 * added landline99 and geometry3d
 *
 * Revision 1.4  1999/09/08 00:59:09  warmerda
 * Added limiting list of files for OGRNTFDataSource::Open() for FME IDs.
 *
 * Revision 1.3  1999/08/30 16:47:29  warmerda
 * added feature class layer, and new product codes
 *
 * Revision 1.2  1999/08/28 18:24:42  warmerda
 * added TestForLayer() optimization
 *
 * Revision 1.1  1999/08/28 03:13:35  warmerda
 * New
 *
 */

#ifndef _NTF_H_INCLUDED
#define _NTF_H_INCLUDED

#include "cpl_conv.h"
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
#define NRT_GEOMETRY3D 22	       /* 3D Geometry Record */
#define NRT_LINEREC  23                /* Line Record */
#define NRT_CHAIN    24                /* Chain */
#define NRT_POLYGON  31                /* Polygon */
#define NRT_CPOLY    33                /* Complex Polygon */
#define NRT_COLLECT  34                /* Collection of featues */
#define NRT_ADR      40                /* Attribute Description Record */
#define NRT_TEXTREC  43		       /* Text */
#define NRT_TEXTPOS  44		       /* Text position */
#define NRT_TEXTREP  45		       /* Text representation */
#define NRT_COMMENT  90		       /* Comment record */
#define NRT_VTR      99                /* Volume Termination Record */

/* -------------------------------------------------------------------- */
/*      Product names (DBNAME) and codes.                               */
/* -------------------------------------------------------------------- */

#define NPC_UNKNOWN		0

#define NPC_LANDLINE            1
#define NPC_LANDLINE99          2
#define NTF_LANDLINE		"LAND-LINE.93"
#define NTF_LANDLINE_PLUS	"LAND-LINE.93+"

#define NPC_STRATEGI		3
#define NTF_STRATEGI            "Strategi_02.96"

#define NPC_MERIDIAN		4
#define NTF_MERIDIAN            "Meridian_01.95"

#define NPC_BOUNDARYLINE	5
#define NTF_BOUNDARYLINE        "Boundary-Line"

#define NPC_BASEDATA		6
#define NTF_BASEDATA		"BaseData.GB_01.96"

#define NPC_OSCAR_ASSET		7
#define NPC_OSCAR_TRAFFIC       8
#define NPC_OSCAR_ROUTE         9
#define NPC_OSCAR_NETWORK       10

#define NPC_ADDRESS_POINT       11

#define NPC_CODE_POINT		12
#define NPC_CODE_POINT_PLUS     13

#define NPC_LANDFORM_PROFILE_CONT 14

#define NPC_LANDRANGER_CONT     15
#define NTF_LANDRANGER_CONT     "OS_LANDRANGER_CONT"

/************************************************************************/
/*                              NTFRecord                               */
/************************************************************************/

class NTFRecord
{
    int      nType;
    int      nLength;
    char    *pszData;
    
  public:
             NTFRecord( FILE * );
             ~NTFRecord();

    int      GetType() { return nType; }
    int      GetLength() { return nLength; }
    const char *GetData() { return pszData; }

    const char *GetField( int, int );
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
} NTFAttDesc;

class OGRNTFLayer;
class OGRNTFDataSource;
class NTFFileReader;

#define MAX_REC_GROUP 	100
typedef OGRFeature *(*NTFFeatureTranslator)(NTFFileReader *,
                                            OGRNTFLayer *,
                                            NTFRecord **);
typedef int (*NTFRecordGrouper)(NTFFileReader *, NTFRecord **, NTFRecord *);

/************************************************************************/
/*                            NTFFileReader                             */
/************************************************************************/

class NTFFileReader
{
    char	     *pszFilename;
    OGRNTFDataSource *poDS;
        
    FILE             *fp;

    // feature class list.
    int               nFCCount;
    int              *panFCNum;
    char            **papszFCName;

    // attribute definitions
    int               nAttCount;
    NTFAttDesc       *pasAttDesc;
    
    char             *pszTileName;
    int               nCoordWidth;
    int		      nZWidth;
    int		      nNTFLevel;

    double            dfXYMult;
    double	      dfZMult;

    double            dfXOrigin;
    double            dfYOrigin;

    double            dfTileXSize;
    double            dfTileYSize;

    double	      dfScale;
    double	      dfPaperToGround;

    long              nStartPos;
    long	      nPreSavedPos;
    long	      nPostSavedPos;
    NTFRecord        *poSavedRecord;

    long	      nSavedFeatureId;
    long	      nBaseFeatureId;
    long	      nFeatureCount; 
    
    NTFRecord	      *apoCGroup[MAX_REC_GROUP+1];

    char	     *pszProduct;
    char	     *pszPVName;
    int		      nProduct;

    void	      EstablishLayer( const char *, OGRwkbGeometryType,
                                      NTFFeatureTranslator, int, ... );
    void	      EstablishLayers();

    void	      ClearCGroup();
    void	      ClearDefs();

    OGRNTFLayer       *apoTypeTranslation[100];

    NTFRecordGrouper  pfnRecordGrouper;

  public:
                      NTFFileReader( OGRNTFDataSource * );
                      ~NTFFileReader();

    int               Open( const char * pszFilename = NULL );
    void              Close();
    FILE	      *GetFP() { return fp; }
    void 	      GetFPPos( long *pnPos, long * pnFeatureId);
    int		      SetFPPos( long nPos, long nFeatureId );
    void              Reset();
    void              SetBaseFID( long nFeatureId );
  
    
    OGRGeometry      *ProcessGeometry( NTFRecord *, int * = NULL );
    OGRGeometry      *ProcessGeometry3D( NTFRecord *, int * = NULL );
    int               ProcessAttDesc( NTFRecord *, NTFAttDesc * );
    int               ProcessAttRec( NTFRecord *, int *, char ***, char ***);
    int               ProcessAttRecGroup( NTFRecord **, char ***, char ***);

    NTFAttDesc       *GetAttDesc( const char * );

    void	      ApplyAttributeValues( OGRFeature *, NTFRecord **, ... );
     
    int		      ApplyAttributeValue( OGRFeature *, int, const char *,
                                           char **, char ** );
    
    int               ProcessAttValue( const char *pszValType, 
                                       const char *pszRawValue,
                                       char **ppszAttName, 
                                       char **ppszAttValue );

    int		      TestForLayer( OGRNTFLayer * );
    OGRFeature       *ReadOGRFeature( OGRNTFLayer * = NULL );
    NTFRecord	    **ReadRecordGroup();
    NTFRecord        *ReadRecord();
    void              SaveRecord( NTFRecord * );

    void              DumpReadable( FILE * );

    int		      GetXYLen() { return nCoordWidth; }
    double            GetXYMult() { return dfXYMult; }
    double            GetXOrigin() { return dfXOrigin; }
    double            GetYOrigin() { return dfYOrigin; }
    const char       *GetTileName() { return pszTileName; }
    int		      GetNTFLevel() { return nNTFLevel; }
    const char       *GetProduct() { return pszProduct; }
    const char       *GetPVName() { return pszPVName; }
    int               GetProductId() { return nProduct; }
    double	      GetScale() { return dfScale; }
    double            GetPaperToGround() { return dfPaperToGround; }

    int		      GetFCCount() { return nFCCount; }
    int               GetFeatureClass( int, int *, char ** );
};

/************************************************************************/
/*                             OGRNTFLayer                              */
/************************************************************************/

class OGRNTFLayer : public OGRLayer
{
    OGRFeatureDefn     *poFeatureDefn;
    OGRGeometry	       *poFilterGeom;
    NTFFeatureTranslator pfnTranslator;

    OGRNTFDataSource   *poDS;

    int			iCurrentReader;
    long		nCurrentPos;
    long		nCurrentFID;
  
  public:
    			OGRNTFLayer( OGRNTFDataSource * poDS,
                                     OGRFeatureDefn * poFeatureDefine,
                                     NTFFeatureTranslator pfnTranslator );

    			~OGRNTFLayer();

    OGRGeometry *	GetSpatialFilter() { return poFilterGeom; }
    void		SetSpatialFilter( OGRGeometry * );

    void		ResetReading();
    OGRFeature *	GetNextFeature();

#ifdef notdef    
    OGRFeature         *GetFeature( long nFeatureId );
    OGRErr              SetFeature( OGRFeature *poFeature );
    OGRErr              CreateFeature( OGRFeature *poFeature );
#endif
    
    OGRFeatureDefn *	GetLayerDefn() { return poFeatureDefn; }

#ifdef notdef    
    int                 GetFeatureCount( int );
#endif
    
    int                 TestCapability( const char * );

    // special to NTF
    OGRFeature         *FeatureTranslate( NTFFileReader *, NTFRecord ** );
};

/************************************************************************/
/*                       OGRNTFFeatureClassLayer                        */
/************************************************************************/

class OGRNTFFeatureClassLayer : public OGRLayer
{
    OGRFeatureDefn     *poFeatureDefn;
    OGRGeometry	       *poFilterGeom;

    OGRNTFDataSource   *poDS;

    int			iCurrentFC;
  
  public:
    			OGRNTFFeatureClassLayer( OGRNTFDataSource * poDS );
    			~OGRNTFFeatureClassLayer();

    OGRGeometry *	GetSpatialFilter() { return poFilterGeom; }
    void		SetSpatialFilter( OGRGeometry * );

    void		ResetReading();
    OGRFeature *	GetNextFeature();

    OGRFeature         *GetFeature( long nFeatureId );
    
    OGRFeatureDefn *	GetLayerDefn() { return poFeatureDefn; }

    int                 GetFeatureCount( int = TRUE );
    
    int                 TestCapability( const char * );
};

/************************************************************************/
/*                           OGRNTFDataSource                           */
/************************************************************************/

class OGRNTFDataSource : public OGRDataSource
{
    char		*pszName;

    int			nLayers;
    OGRNTFLayer		**papoLayers;

    OGRNTFFeatureClassLayer *poFCLayer;

    int                 iCurrentFC;
    int			iCurrentReader;
    long		nCurrentPos;
    long		nCurrentFID;
  
    int			nNTFFileCount;
    NTFFileReader	**papoNTFFileReader;

    int			nFCCount;
    int                *panFCNum;
    char              **papszFCName;
    
  public:
    			OGRNTFDataSource();
    			~OGRNTFDataSource();

    int                 Open( const char * pszName, int bTestOpen = FALSE,
                              char ** papszFileList = NULL );
    
    const char	        *GetName() { return pszName; }
    int			GetLayerCount();
    OGRLayer		*GetLayer( int );

    // Note: these are specific to NTF for now, but eventually might
    // might be available as part of a more object oriented approach to
    // features like that in FME or SFCORBA.
    void		ResetReading();
    OGRFeature *	GetNextFeature();

    // these are only for the use of the NTFFileReader class.
    OGRNTFLayer         *GetNamedLayer( const char * );
    void                 AddLayer( OGRNTFLayer * );

    // Mainly for OGRNTFLayer class
    int			 GetFileCount() { return nNTFFileCount; }
    NTFFileReader       *GetFileReader(int i) { return papoNTFFileReader[i]; }

    int		         GetFCCount() { return nFCCount; }
    int                  GetFeatureClass( int, int *, char ** );
};

/************************************************************************/
/*                             OGRNTFDriver                             */
/************************************************************************/

class OGRNTFDriver : public OGRSFDriver
{
  public:
    		~OGRNTFDriver();
                
    const char *GetName();
    OGRDataSource *Open( const char *, int );
};

#endif /* ndef _NTF_H_INCLUDED */
