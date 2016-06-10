#ifndef OGR_CAD_H_INCLUDED
#define OGR_CAD_H_INCLUDED

#include "ogrsf_frmts.h"

#include "libopencad/opencad_api.h"
#include "libopencad/cadgeometry.h"

class OGRCADLayer : public OGRLayer
{
    OGRFeatureDefn  *poFeatureDefn;
    
    int             nNextFID;
    
public:
    OGRCADLayer( const char *pszFilename );
    ~OGRCADLayer();
    
    void            ResetReading();
    OGRFeature      *GetNextFeature();
    
    OGRFeatureDefn  *GetLayerDefn() { return poFeatureDefn; }
    
    int             TestCapability( const char * ) { return( FALSE ); }
};

class OGRCADDataSource : public GDALDataset
{
    CADFile         *poCADFile;
    
    OGRCADLayer     **papoLayers;
    int               nLayers;
    
public:
    OGRCADDataSource();
    ~OGRCADDataSource();
    
    int             Open( const char * pszFilename, int bUpdate );
    
    int             GetLayerCount() { return nLayers; }
    OGRLayer        *GetLayer( int );
    
    int             TestCapability( const char * ) { return( FALSE ); }
};

#endif