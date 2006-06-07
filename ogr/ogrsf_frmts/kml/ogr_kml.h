#ifndef _OGR_KML_H_INCLUDED
#define _OGR_KML_H_INCLUDED

#include "ogrsf_frmts.h"
#include "kmlreader.h"

class OGRKMLDataSource;

/************************************************************************/
/*                            OGRKMLLayer                               */
/************************************************************************/

class OGRKMLLayer : public OGRLayer
{
    OGRSpatialReference *poSRS;
    OGRFeatureDefn     *poFeatureDefn;

    int                 iNextKMLId;
    int                 nTotalKMLCount;

    int                 bWriter;

    OGRKMLDataSource    *poDS;

    KMLFeatureClass     *poFClass;

  public:
                        OGRKMLLayer( const char * pszName, 
                                     OGRSpatialReference *poSRS, 
                                     int bWriter,
                                     OGRwkbGeometryType eType,
                                     OGRKMLDataSource *poDS );

                        ~OGRKMLLayer();

    void                ResetReading();
    OGRFeature *        GetNextFeature();

    int                 GetFeatureCount( int bForce = TRUE );
    OGRErr              GetExtent(OGREnvelope *psExtent, int bForce = TRUE);

    OGRErr              CreateFeature( OGRFeature *poFeature );
    
    OGRFeatureDefn *    GetLayerDefn() { return poFeatureDefn; }

    virtual OGRErr      CreateField( OGRFieldDefn *poField,
                                     int bApproxOK = TRUE );

    virtual OGRSpatialReference *GetSpatialRef();
    
    int                 TestCapability( const char * );
};

/************************************************************************/
/*                           OGRKMLDataSource                           */
/************************************************************************/

class OGRKMLDataSource : public OGRDataSource
{
    OGRKMLLayer     **papoLayers;
    int                 nLayers;
    
    char                *pszName;
    
    OGRKMLLayer         *TranslateKMLSchema( KMLFeatureClass * );

    char               **papszCreateOptions;

    // output related parameters 
    FILE                *fpOutput;
    OGREnvelope         sBoundingRect;
    int                 nBoundedByLocation;
    
    int                 nSchemaInsertLocation;

    // input related parameters.
    //IKMLReader          *poReader;

    void                InsertHeader();

  public:
                        OGRKMLDataSource();
                        ~OGRKMLDataSource();

    int                 Open( const char *, int bTestOpen );
    int                 Create( const char *pszFile, char **papszOptions );

    const char          *GetName() { return pszName; }
    int                 GetLayerCount() { return nLayers; }
    OGRLayer            *GetLayer( int );

    virtual OGRLayer    *CreateLayer( const char *, 
                                      OGRSpatialReference * = NULL,
                                      OGRwkbGeometryType = wkbUnknown,
                                      char ** = NULL );

    int                 TestCapability( const char * );

    FILE                *GetOutputFP() { return fpOutput; }
    //IKMLReader          *GetReader() { return poReader; }

    void                GrowExtents( OGREnvelope *psGeomBounds );
};

/************************************************************************/
/*                             OGRKMLDriver                             */
/************************************************************************/

class OGRKMLDriver : public OGRSFDriver
{
  public:
                ~OGRKMLDriver();
                
    const char *GetName();
    OGRDataSource *Open( const char *, int );

    virtual OGRDataSource *CreateDataSource( const char *pszName,
                                             char ** = NULL );
    
    int                 TestCapability( const char * );
};

#endif /* _OGR_KML_H_INCLUDED */
