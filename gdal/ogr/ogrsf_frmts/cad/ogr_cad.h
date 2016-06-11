#ifndef OGR_CAD_H_INCLUDED
#define OGR_CAD_H_INCLUDED

#include "ogrsf_frmts.h"

#include "libopencad/opencad_api.h"
#include "libopencad/cadgeometry.h"

class OGRCADLayer : public OGRLayer
{
    OGRFeatureDefn  *poFeatureDefn;
    
    int             nNextFID; // FIXME: its legal to make it size_t?

    CADLayer        &poCADLayer ;
public:
    OGRCADLayer( CADLayer &poCADLayer );
    ~OGRCADLayer();
    
    void            ResetReading();
    OGRFeature      *GetNextFeature();
    OGRFeature      *GetFeature( GIntBig nFID );
    GIntBig         GetFeatureCount( int bForce );
    
    
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