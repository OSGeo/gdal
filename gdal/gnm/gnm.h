/******************************************************************************
 * $Id$
 *
 * Project:  GDAL/OGR Geography Network support (Geographic Network Model)
 * Purpose:  GNM general public declarations.
 * Authors:  Mikhail Gusev (gusevmihs at gmail dot com)
 *           Dmitry Baryshnikov, polimax@mail.ru
 *
 ******************************************************************************
 * Copyright (c) 2014, Mikhail Gusev
 * Copyright (c) 2014-2015, NextGIS <info@nextgis.com>
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

#ifndef GNM
#define GNM

#include "ogrsf_frmts.h"
#include "gnmgraph.h"

// Direction of an edge.
typedef int GNMDirection; // We use int values in order to save them to the
                          // network data.

// Network's metadata parameters names.
#define GNM_MD_NAME     "net_name"
#define GNM_MD_DESCR    "net_description"
#define GNM_MD_SRS      "net_srs"
#define GNM_MD_VERSION  "net_version"
#define GNM_MD_RULE     "net_rule"
#define GNM_MD_FORMAT   "FORMAT"
#define GNM_MD_FETCHEDGES   "fetch_edge"
#define GNM_MD_FETCHVERTEX  "fetch_vertex"
#define GNM_MD_NUM_PATHS    "num_paths"
#define GNM_MD_EMITTER   "emitter"

// TODO: Constants for capabilities.
//#define GNMCanChangeConnections "CanChangeConnections"

typedef enum
{
    /** Dijkstra shortest path */           GATDijkstraShortestPath = 1,
    /** KShortest Paths        */           GATKShortestPath,
    /** Recursive Breadth-first search */   GATConnectedComponents
} GNMGraphAlgorithmType;

/**
 * General GNM class which represents a geography network of common format.
 *
 * @since GDAL 2.1
 */

class CPL_DLL GNMNetwork : public GDALDataset
{
public:
    GNMNetwork();
    virtual ~GNMNetwork();

    // GDALDataset Interface
    virtual const char *GetProjectionRef(void);
    virtual char      **GetFileList(void);

    // GNMNetwork Interface

    /**
     * @brief Create network system layers
     *
     * Creates the connectivity (the "network path" of data) over the dataset
     * and returns the resulting network.
     * NOTE: This method does not create any connections among features
     * but creates the necessary set of fields, layers, etc.
     * NOTE: After the successful creation the passed dataset must not be
     * modified outside (but can be read as usual).
     * NOTE: For the common network format the creation is forbidden if the
     * passed dataset already has network system layers and OVERWRITE creation
     * option is FALSE.
     *
     * @param pszFilename - A path there the network folder (schema, etc.) will
     *                      be created. The folder (schema, etc.) name get
     *                      options.
     * @param papszOptions - create network options. The create options
     *                       specific for gnm driver.
     * @return CE_None on success
     */
    virtual CPLErr Create( const char* pszFilename, char** papszOptions ) = 0;

    /**
     * @brief Open a network
     * @param poOpenInfo GDALOpenInfo pointer
     * @return CE_None on success
     */
    virtual CPLErr Open( GDALOpenInfo* poOpenInfo ) = 0;

    /**
     * @brief Delete network. Delete all dependent layers
     * @return CE_None on success
     */
    virtual CPLErr Delete() = 0;

    /**
     * @brief GetName - a network name. The value provided to create function
     *        in GNM_MD_NAME key. While creation this value used to create the
     *        folder or db schema name. But can be changed after creation.
     * @return Network name string
     */
    virtual const char* GetName() const;

    /**
     * @brief GetVersion return the network version if applicable
     * @return version value
     */
    virtual int GetVersion() const { return 0;}

    /**
     * @brief DisconnectAll method clears the network graph
     * @return CE_None on success
     */
    virtual CPLErr DisconnectAll () = 0;

    /**
     * @brief GetFeatureByGlobalFID search all network layers for given feature
     *        identificator.
     * @param nGFID feature identificator.
     * @return OGRFeature pointer or NULL. The pointer should be freed via
     *         OGRFeature::DestroyFeature().
     */
    virtual OGRFeature *GetFeatureByGlobalFID (GNMGFID nGFID) = 0;

    /**
     * @brief Create path between start and end GFIDs.
     * @param nStartGFID - start identificator
     * @param nEndGFID - end identificator
     * @param eAlgorithm - The algorithm to get path
     * @param papszOptions - algorithm specific options
     * @return In memory OGRLayer pointer with features constituting
     *         the shortest path (or paths). The caller have to free
     *         the pointer via @see ReleaseResultSet().
     */
    virtual OGRLayer *GetPath (GNMGFID nStartFID, GNMGFID nEndFID,
                     GNMGraphAlgorithmType eAlgorithm, char** papszOptions) = 0;
protected:
    /**
     * @brief Check if network already exist
     * @param pszFilename - path to network (folder or database
     * @param papszOptions - create options
     * @return TRUE if exist and not overwrite or FALSE
     */
    virtual int CheckNetworkExist( const char* pszFilename,
                                   char** papszOptions ) = 0;

protected:
    CPLString m_soName;
    CPLString m_soSRS;
};

class GNMRule;
class OGRGNMWrappedResultLayer;

/**
 * GNM class which represents a geography network of generic format.
 *
 * @since GDAL 2.1
 */

class CPL_DLL GNMGenericNetwork: public GNMNetwork
{
public:
    GNMGenericNetwork();
    virtual ~GNMGenericNetwork();

    // GDALDataset Interface

    virtual int         GetLayerCount();
    virtual OGRLayer    *GetLayer(int);
    virtual OGRErr      DeleteLayer(int);

    virtual int         TestCapability( const char * );

    virtual OGRLayer   *CopyLayer( OGRLayer *poSrcLayer,
                                   const char *pszNewName,
                                   char **papszOptions = NULL );

    virtual int CloseDependentDatasets();
    virtual void FlushCache(void);

    // GNMNetwork Interface

    virtual CPLErr Create( const char* pszFilename, char** papszOptions ) = 0;
    virtual CPLErr Delete();

    virtual int GetVersion() const;
    /**
     * @brief GetNewGlobalFID increase the global ID counter.
     * @return New global feature ID.
     */
    virtual GNMGFID GetNewGlobalFID();

    /**
     * @brief Get the algorithm name
     * @param eAlgorithm GNM algorithm type
     * @param bShortName Indicator which name to return - short or long
     * @return String with algorithm name
     */
    virtual CPLString GetAlgorithmName(GNMDirection eAlgorithm, bool bShortName);

    /**
     * @brief AddFeatureGlobalFID add the FID <-> Layer name link to fast access
     *        features by global FID.
     * @param nFID - global FID
     * @param pszLayerName - layer name
     * @return CE_None on success
     */
    virtual CPLErr AddFeatureGlobalFID(GNMGFID nFID, const char* pszLayerName);

    /**
     * @brief Connects two features via third feature (may be virtual, so the 
     *        identificator should be -1). The features may be at the same layer 
     *        or different layers. 
     * @param nSrcFID - source feature identificator
     * @param nTgtFID - target feature identificator
     * @param nConFID - connection feature identificator (-1 for virtual connection)
     * @param dfCost - cost moving from source to target (default 1)
     * @param dfInvCost - cost moving from target to source (default 1)
     * @param eDir - direction, may be source to target, traget to source or both.
     *               (default - both)
     * @return CE_None on success
     */
    virtual CPLErr ConnectFeatures (GNMGFID nSrcFID,
                                    GNMGFID nTgtFID,
                                    GNMGFID nConFID = -1,
                                    double dfCost = 1,
                                    double dfInvCost = 1,
                                    GNMDirection eDir = GNM_EDGE_DIR_BOTH);

    /**
     * @brief Remove features connection
     * @param nSrcFID - source feature identificator
     * @param nTgtFID - target feature identificator
     * @param nConFID - connection feature identificator
     * @return CE_None on success
     */
    virtual CPLErr DisconnectFeatures (GNMGFID nSrcFID,
                                       GNMGFID nTgtFID,
                                       GNMGFID nConFID);

    /**
     * @brief Find the corresponding identificator in graph (source, target,
     *        connector) and remove such connections.
     * @param nFID - identificator to find.
     * @return CE_None on success
     */
    virtual CPLErr DisconnectFeaturesWithId(GNMGFID nFID);

    /**
     * @brief Change connection attributes. Search the connection by source 
     *        feature identificator, target feature identificator and connection
     *        identificator.
     * @param nSrcFID - source feature identificator
     * @param nTgtFID - target feature identificator
     * @param nConFID - connection feature identificator
     * @param dfCost - new cost moving from source to target
     * @param dfInvCost - new cost moving from target to source
     * @param eDir - new direction
     * @return CE_None on success
     */
    virtual CPLErr ReconnectFeatures (GNMGFID nSrcFID,
                                      GNMGFID nTgtFID,
                                      GNMGFID nConFID,
                                      double dfCost = 1,
                                      double dfInvCost = 1,
                                      GNMDirection eDir = GNM_EDGE_DIR_BOTH);

    virtual CPLErr DisconnectAll();

    virtual OGRFeature *GetFeatureByGlobalFID(GNMGFID nFID);

    /**
     * @brief Create network rule
     *
     * Creates the rule in the network according to the special syntax. These
     * rules are declarative and make an effect for the network when they exist.
     * Each rule for layer can be created only if the corresponding layer
     * existed and removed when the layer is being deleted.
     *
     * Rules syntax for the common network format in GNM contains the key words
     * (words in capital letters or signs) and the modifiers which refers to the
     * network objects. All the following combinations are available:
     *
     *  Notation:
     *	layer1, layer2, layer3 - a layer names (the corresponding layers must be
     *                           exist;
     *	field1 - a field name (field must be exist);
     *	constant1 - any double constant;
     *	string1 - any string;
     *
     *	Rules describing which layer can be connected or not connected with each
     *  other, and (optional) which layer must serve as a connector. By default
     *  all connections are forbidden. But while network creation process the
     *  rule to allow any connection added. During the connection process each
     *  rule tested if this connection can be created.
     *
     *    "ALLOW CONNECTS ANY"
     *    "DENY CONNECTS ANY"
     *	  "DENY CONNECTS layer1 WITH layer2"
     *	  "ALLOW CONNECTS layer1 WITH layer2 VIA layer3"
     *
     * @param pszRuleStr Rule string which will parsed. If the parsing was
     *        successful the the rule will start having effect immediately.
     * @return CE_None on success.
     */
    virtual CPLErr CreateRule (const char *pszRuleStr);

    /**
     * @brief Delete all rules from network
     * @return CE_None on success.
     */
    virtual CPLErr DeleteAllRules();

    /**
     * @brief Delete the specified rule
     * @param pszRuleStr - the rule to delete
     * @return CE_None on success.
     */
    virtual CPLErr DeleteRule(const char *pszRuleStr);

    /**
     * @brief Get the rule list
     * @return list of rule strings. The caller have to free the lis via CPLDestroy.
     */
    virtual char** GetRules() const;

    /**
     * @brief Attempts to build the network topology automatically
     *
     * The method simply gets point and line or multiline layers from the
     * papszLayerList and searches for each line which connects two points: start
     * and end, so it can be not so effective in performance when it is called
     * on huge networks.
     * Note, when passing your tolerance value: this value will depend of spatial
     * reference system of the network, and especially of its 0,0 position
     * because dfTolerance is just divided by 2 and added/subtracted to/from
     * both sides of each line-feature end point forming thus the square area
     * around it. The first point-feature occurred inside this area will be given
     * as a start/end point for the current connection. So it is also desirable
     * that at least two layers are passed in papszLayerList (one point and one
     * line), and they are already connected "visually" ("geometrically").
     *
     * @param papszLayerList A list of layers to connect. The list should be
     *                       freed via CSLDestroy.
     * @param dfTolerance Snapping tolerance.
     * @return CE_None on success
     */
    virtual CPLErr ConnectPointsByLines (char **papszLayerList, 
                                         double dfTolerance,
                                         double dfCost,
                                         double dfInvCost,
                                         GNMDirection eDir);

    /**
     * @brief Change the block state of edge or vertex
     * @param nFID Identificator
     * @param bIsBlock Block or unblock
     * @return CE_None on success
     */
    virtual CPLErr ChangeBlockState (GNMGFID nFID, bool bIsBlock);

    /**
     * @brief Change all vertice and edges block state.
     *
     * This is mainly use for unblock all vertices and edges.
     *
     * @param bIsBlock Block or unblock
     * @return CE_None on success
     */
    virtual CPLErr ChangeAllBlockState (bool bIsBlock = false);

    virtual OGRLayer *GetPath (GNMGFID nStartFID, GNMGFID nEndFID,
                     GNMGraphAlgorithmType eAlgorithm, char** papszOptions);
protected:
    /**
     * @brief Check or create layer OGR driver
     * @param pszDefaultDriverName - default driver name
     * @param papszOptions - create options
     * @return CE_None if driver is exist or CE_Failure
     */
    virtual CPLErr CheckLayerDriver(const char* pszDefaultDriverName,
                                 char** papszOptions);
    /**
     * @brief Check if provided OGR driver acceptad as storage for network data
     * @param pszDriverName The driver name
     * @return true if supported, else false
     */
    virtual bool CheckStorageDriverSupport(const char* pszDriverName) = 0;
protected:
    virtual CPLErr CreateMetadataLayer( GDALDataset* const pDS, int nVersion,
                                     size_t nFieldSize = 1024 );
    virtual CPLErr StoreNetworkSrs();
    virtual CPLErr LoadNetworkSrs();
    virtual CPLErr CreateGraphLayer( GDALDataset* const pDS );
    virtual CPLErr CreateFeaturesLayer( GDALDataset* const pDS );
    virtual CPLErr LoadMetadataLayer( GDALDataset* const pDS );
    virtual CPLErr LoadGraphLayer( GDALDataset* const pDS );
    virtual CPLErr LoadGraph();
    virtual CPLErr LoadFeaturesLayer( GDALDataset* const pDS );
    virtual CPLErr DeleteMetadataLayer() = 0;
    virtual CPLErr DeleteGraphLayer() = 0;
    virtual CPLErr DeleteFeaturesLayer() = 0;
    virtual CPLErr LoadNetworkLayer(const char* pszLayername) = 0;
    virtual CPLErr DeleteNetworkLayers() = 0;
    virtual void ConnectPointsByMultiline(GIntBig nFID,
                                  const OGRMultiLineString *poMultiLineString,
                                  const std::vector<OGRLayer *> &paPointLayers,
                                  double dfTolerance, double dfCost,
                                  double dfInvCost, GNMDirection eDir);
    virtual void ConnectPointsByLine(GIntBig nFID, 
                                     const OGRLineString *poLineString, 
                                     const std::vector<OGRLayer *> &paPointLayers, 
                                     double dfTolerance, double dfCost,
                                     double dfInvCost, GNMDirection eDir);
    virtual GNMGFID FindNearestPoint(const OGRPoint* poPoint, 
                                     const std::vector<OGRLayer*>& paPointLayers,
                                     double dfTolerance);
    virtual OGRFeature* FindConnection(GNMGFID nSrcFID, GNMGFID nTgtFID,
                                       GNMGFID nConFID);
    virtual void SaveRules();
    virtual GNMGFID GetNewVirtualFID();
    virtual void FillResultLayer(OGRGNMWrappedResultLayer* poResLayer,
                                 const GNMPATH &path, int nNoOfPath,
                                 bool bReturnVertices, bool bReturnEdges);
protected:
    int m_nVersion;
    GNMGFID m_nGID;    
    GNMGFID m_nVirtualConnectionGID;
    OGRLayer* m_poMetadataLayer;
    OGRLayer* m_poGraphLayer;
    OGRLayer* m_poFeaturesLayer;

    GDALDriver *m_poLayerDriver;

    std::map<GNMGFID, CPLString> m_moFeatureFIDMap;
    std::vector<OGRLayer*> m_apoLayers;
    std::vector<GNMRule> m_asRules;
    bool m_bIsRulesChanged;

    GNMGraph m_oGraph;
    bool m_bIsGraphLoaded;
};

/**
 * GNM layer which represents a geography network layer of generic format.
 * The class override some OGRLayer methods to fullfill the network requirements.
 *
 * @since GDAL 2.1
 */

class GNMGenericLayer : public OGRLayer
{
public:
    GNMGenericLayer(OGRLayer* poLayer, GNMGenericNetwork* poNetwork);
    virtual ~GNMGenericLayer();

    // OGRLayer Interface

    virtual OGRGeometry *GetSpatialFilter();
    virtual void        SetSpatialFilter( OGRGeometry * );
    virtual void        SetSpatialFilterRect( double dfMinX, double dfMinY,
                                              double dfMaxX, double dfMaxY );

    virtual void        SetSpatialFilter( int iGeomField, OGRGeometry * );
    virtual void        SetSpatialFilterRect( int iGeomField,
                                            double dfMinX, double dfMinY,
                                            double dfMaxX, double dfMaxY );

    virtual OGRErr      SetAttributeFilter( const char * );

    virtual void        ResetReading();
    virtual OGRFeature *GetNextFeature();
    virtual OGRErr      SetNextByIndex( GIntBig nIndex );

    virtual OGRErr      DeleteFeature( GIntBig nFID );

    virtual const char *GetName();
    virtual OGRwkbGeometryType GetGeomType();
    virtual OGRFeatureDefn *GetLayerDefn();
    virtual int         FindFieldIndex( const char *pszFieldName, int bExactMatch );

    virtual OGRSpatialReference *GetSpatialRef();

    virtual GIntBig     GetFeatureCount( int bForce = TRUE );
    virtual OGRErr      GetExtent(OGREnvelope *psExtent, int bForce = TRUE);
    virtual OGRErr      GetExtent(int iGeomField, OGREnvelope *psExtent,
                                  int bForce = TRUE);

    virtual int         TestCapability( const char * );

    virtual OGRErr      CreateField( OGRFieldDefn *poField,
                                     int bApproxOK = TRUE );
    virtual OGRErr      DeleteField( int iField );
    virtual OGRErr      ReorderFields( int* panMap );
    virtual OGRErr      AlterFieldDefn( int iField, OGRFieldDefn* poNewFieldDefn,
                                        int nFlagsIn );

    virtual OGRErr      CreateGeomField( OGRGeomFieldDefn *poField,
                                     int bApproxOK = TRUE );

    virtual OGRErr      SyncToDisk();

    virtual OGRStyleTable *GetStyleTable();
    virtual void        SetStyleTableDirectly( OGRStyleTable *poStyleTable );

    virtual void        SetStyleTable(OGRStyleTable *poStyleTable);

    virtual OGRErr      StartTransaction();
    virtual OGRErr      CommitTransaction();
    virtual OGRErr      RollbackTransaction();

    virtual const char *GetFIDColumn();
    virtual const char *GetGeometryColumn();

    virtual OGRErr      SetIgnoredFields( const char **papszFields );

    OGRErr              Intersection( OGRLayer *pLayerMethod,
                                      OGRLayer *pLayerResult,
                                      char** papszOptions = NULL,
                                      GDALProgressFunc pfnProgress = NULL,
                                      void * pProgressArg = NULL );
    OGRErr              Union( OGRLayer *pLayerMethod,
                               OGRLayer *pLayerResult,
                               char** papszOptions = NULL,
                               GDALProgressFunc pfnProgress = NULL,
                               void * pProgressArg = NULL );
    OGRErr              SymDifference( OGRLayer *pLayerMethod,
                                       OGRLayer *pLayerResult,
                                       char** papszOptions,
                                       GDALProgressFunc pfnProgress,
                                       void * pProgressArg );
    OGRErr              Identity( OGRLayer *pLayerMethod,
                                  OGRLayer *pLayerResult,
                                  char** papszOptions = NULL,
                                  GDALProgressFunc pfnProgress = NULL,
                                  void * pProgressArg = NULL );
    OGRErr              Update( OGRLayer *pLayerMethod,
                                OGRLayer *pLayerResult,
                                char** papszOptions = NULL,
                                GDALProgressFunc pfnProgress = NULL,
                                void * pProgressArg = NULL );
    OGRErr              Clip( OGRLayer *pLayerMethod,
                              OGRLayer *pLayerResult,
                              char** papszOptions = NULL,
                              GDALProgressFunc pfnProgress = NULL,
                              void * pProgressArg = NULL );
    OGRErr              Erase( OGRLayer *pLayerMethod,
                               OGRLayer *pLayerResult,
                               char** papszOptions = NULL,
                               GDALProgressFunc pfnProgress = NULL,
                               void * pProgressArg = NULL );

    GIntBig             GetFeaturesRead();

    int                 AttributeFilterEvaluationNeedsGeometry();

    /* consider these private */
    OGRErr               InitializeIndexSupport( const char * );
    OGRLayerAttrIndex   *GetIndex();

protected:
    virtual OGRErr      ISetFeature( OGRFeature *poFeature );
    virtual OGRErr      ICreateFeature( OGRFeature *poFeature );

protected:
    CPLString m_soLayerName;
    OGRLayer *m_poLayer;
    GNMGenericNetwork* m_poNetwork;
    std::map<GNMGFID, GIntBig> m_mnFIDMap;
};

typedef enum
{
    /** Rule for connect features */   GRTConnection = 0
} GNMRuleType;

/**
 * @brief The simple class for rules
 *
 * By now we have only connect rules, so the one class is enough. Maybe in
 * future the set of classes for different rule types will be needed.
 *
 * @since GDAL 2.1
 */

class CPL_DLL GNMRule : public CPLString
{
public:
    GNMRule();
    GNMRule(const std::string &oRule );
    GNMRule(const char* pszRule);
    GNMRule(const GNMRule &oRule);
    virtual ~GNMRule();
    /**
     * @brief  This function indicate if rule string was parsed successfully
     * @return true if rule is valid
     */
    virtual bool IsValid() const;
    /**
     * @brief Indicator of any layer state
     * @return true if accept any layer
     */
    virtual bool IsAcceptAny() const;
    /**
     * @brief This is for future use to indicate the rule type/ Now return only
     * GRTConnection type.
     * @return the rule type
     */
    virtual GNMRuleType GetType() const;
    /**
     * @brief Check if connection can take place.
     * @param soSrcLayerName - the layer name
     * @param soTgtLayerName - the layer name
     * @param soConnLayerName - the layer name
     * @return true if can connect features from soSrcLayerName and soTgtLayerName
     *         via soConnLayerName
     */
    virtual bool CanConnect(const CPLString &soSrcLayerName,
                            const CPLString &soTgtLayerName,
                            const CPLString &soConnLayerName = "");
    virtual CPLString GetSourceLayerName() const;
    virtual CPLString GetTargetLayerName() const;
    virtual CPLString GetConnectorLayerName() const;
protected:
    virtual bool ParseRuleString();
protected:
    CPLString m_soSrcLayerName;
    CPLString m_soTgtLayerName;
    CPLString m_soConnLayerName;
    bool m_bAllow;
    bool m_bValid;
    bool m_bAny;
};

/**
 * @brief The OGRGNMWrappedResultLayer class for search paths queries results.
 *
 * @since GDAL 2.1
 */

class OGRGNMWrappedResultLayer : public OGRLayer
{
public:
    OGRGNMWrappedResultLayer(GDALDataset* poDS, OGRLayer* poLayer);
    ~OGRGNMWrappedResultLayer();

    // OGRLayer
    virtual void ResetReading();
    virtual OGRFeature *GetNextFeature();
    virtual OGRErr SetNextByIndex( GIntBig nIndex );
    virtual OGRFeature *GetFeature( GIntBig nFID );
    virtual OGRFeatureDefn *GetLayerDefn();
    virtual GIntBig GetFeatureCount( int bForce = TRUE );
    virtual int TestCapability( const char * pszCap );
    virtual OGRErr CreateField( OGRFieldDefn *poField, int bApproxOK = TRUE );
    virtual OGRErr      CreateGeomField( OGRGeomFieldDefn *poField,
                                     int bApproxOK = TRUE );
    virtual const char *GetFIDColumn();
    virtual const char *GetGeometryColumn();
    virtual OGRSpatialReference *GetSpatialRef();

    // OGRGNMWrappedResultLayer
    virtual OGRErr InsertFeature(OGRFeature* poFeature,
                                const CPLString &soLayerName, int nPathNo,
                                bool bIsEdge);
protected:
    virtual OGRErr      ISetFeature( OGRFeature *poFeature );
    virtual OGRErr      ICreateFeature( OGRFeature *poFeature );
protected:
    GDALDataset *poDS;
    OGRLayer    *poLayer;
};

#endif // GNM
