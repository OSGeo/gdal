#ifndef __MMSTRUCT_H
#define __MMSTRUCT_H
/* -------------------------------------------------------------------- */
/*      Necessary functions to read/write a MiraMon Vector File         */
/* -------------------------------------------------------------------- */
#ifdef APLICACIO_MM32
#include "memo.h"
#include "bd_xp.h"	    // For MAX_LON_CAMP_DBF
#include "DefTopMM.h"   // For GENERAT_PER_LINARC
#include "lbtopstr.h"   // For NODE_DANELL
#define CPL_C_START
#define CPL_C_END
#else
#include "cpl_conv.h"   // For CPLCalloc()
#endif

CPL_C_START // Necessary for compiling in GDAL project

#ifdef APLICACIO_MM32
    #define FILE_TYPE FILE_64
    #define TOLERANCE_DIFFERENT_DOUBLES TOLERANCIA_DOUBLES_DIFERENTS
    // #define DBL_EPSILON  already defined in MiraMon
    #define INF_LIN_METERS INF_LIN_METRES
    #define STATISTICAL_UNDEFINED_VALUE VALOR_ESTAD_INDEFINIT
#else
    #define FILE_TYPE VSILFILE
    #define TOLERANCE_DIFFERENT_DOUBLES 8
    #define DBL_EPSILON     2.2204460492503131e-016 /* smallest such that 1.0+DBL_EPSILON != 1.0 */
    #define INF_LIN_METERS (1.0e-5)
    #define STATISTICAL_UNDEFINED_VALUE (2.9E+301)
#endif

// Types of layers in MiraMon
#ifdef APLICACIO_MM32
    #define MM_MAX_BYTES_FIELD_DESC        MAX_LON_DESCRIPCIO_VALOR
    #define MM_MAX_LON_NAME_DBF_FIELD      MAX_LON_CAMP_DBF
    #define MM_MAX_BYTES_IN_A_FIELD        MAX_TIPUS_BYTES_PER_CAMP_DBF

    // BIT 1
    #define MM_LAYER_GENERATED_USING_MM     GENERAT_PER_LINARC  // For example
    // BIT 3
    #define MM_LAYER_MULTIPOLYGON               CICLAT_CONTE_GRUPS
    // BIT 4
    #define MM_LAYER_3D_INFO                    AMB_INFORMACIO_ALTIMETRICA
    // BIT 5
    #define MM_LAYER_EXPLICITAL_POLYGONS    POLIGONS_EXPLICITS

    #define MM_MAX_PATH                         _MAX_PATH
    #define MM_MESSAGE_LENGHT                   MIDA_MISSATGE
    
    #define MM_RING_NODE                NODE_DANELL
    #define MM_FINAL_NODE               NODE_FINAL
        
    #define MM_MAX_LEN_LAYER_NAME               MAX_LON_FILE_IDENTIFIER
    #define MM_MAX_ID_SNY                       MAX_ID_SNY

    // For MetaData
    #define SECTION_VERSIO          SECCIO_VERSIO
    #define KEY_Vers                CLAU_Vers
    #define KEY_SubVers             CLAU_SubVers
    #define MM_VERS                 VERS_REL
    #define MM_SUBVERS              SUBVERS_REL
    #define KEY_VersMetaDades       CLAU_VersMetaDades
    #define KEY_SubVersMetaDades    CLAU_SubVersMetaDades
    #define MM_VERS_METADADES       VERS_METADADES
    #define MM_SUBVERS_METADADES    SUBVERS_METADADES
    #define SECTION_METADADES       SECCIO_METADADES
    #define KEY_FileIdentifier      CLAU_FileIdentifier
    #define SECTION_IDENTIFICATION  SECCIO_IDENTIFICATION
    #define KEY_code                CLAU_code
    #define KEY_codeSpace           CLAU_codeSpace
    #define KEY_DatasetTitle        CLAU_DatasetTitle
    #define SECTION_OVERVIEW        SECCIO_OVERVIEW
    #define SECTION_OVVW_ASPECTES_TECNICS SECCIO_OVERVIEW_ASPECTES_TECNICS
    #define KEY_ArcSource           CLAU_ArcSource
    #define SECTION_EXTENT          SECCIO_EXTENT
    #define KEY_toler_env           CLAU_toler_env
    #define KEY_MinX                CLAU_MinX
    #define KEY_MaxX                CLAU_MaxX
    #define KEY_MinY                CLAU_MinY
    #define KEY_MaxY                CLAU_MaxY
    #define KEY_CreationDate        CLAU_CreationDate
    #define SECTION_SPATIAL_REFERENCE_SYSTEM SECCIO_SPATIAL_REFERENCE_SYSTEM
    #define SECTION_HORIZONTAL      SECCIO_HORIZONTAL
    #define KEY_HorizontalSystemIdentifier CLAU_HorizontalSystemIdentifier
    #define SECTION_TAULA_PRINCIPAL SECCIO_TAULA_PRINCIPAL
    #define KEY_IdGrafic            szNomClauIdGrafic
    #define KEY_TipusRelacio        CLAU_TipusRelacio
    #define KEY_descriptor          CLAU_descriptor
    #define KEY_HorizontalSystemDefinition CLAU_HorizontalSystemDefinition
    #define KEY_unitats             CLAU_unitats
    #define KEY_unitatsY            CLAU_unitatsY
#else
    #define MM_MAX_BYTES_FIELD_DESC        360
	#define MM_MAX_LON_NAME_DBF_FIELD      129
    #define MM_MAX_BYTES_IN_A_FIELD        255

    // BIT 1
    #define MM_LAYER_GENERATED_USING_MM     0x02
    // BIT 3
    #define MM_LAYER_MULTIPOLYGON           0x08
    // BIT 4
    #define MM_LAYER_3D_INFO                0x10
    // BIT 5
    #define MM_LAYER_EXPLICITAL_POLYGONS    0x20	

    #define MM_MAX_PATH                     260
    #define MM_MESSAGE_LENGHT               512

    
    #define MM_RING_NODE                    2
    #define MM_FINAL_NODE                   3
        
    #define MM_MAX_LEN_LAYER_NAME           255
    #define MM_MAX_ID_SNY                    41

    // For MetaData
    #define SECTION_VERSIO         "VERSIO"
    #define KEY_Vers               "Vers"
    #define KEY_SubVers            "SubVers"
    #define MM_VERS                4
    #define MM_SUBVERS             3
    #define KEY_VersMetaDades      "VersMetaDades"
    #define KEY_SubVersMetaDades   "SubVersMetaDades"
    #define MM_VERS_METADADES      5
    #define MM_SUBVERS_METADADES   0
    #define SECTION_METADADES      "METADADES"
    #define KEY_FileIdentifier     "FileIdentifier"
    #define SECTION_IDENTIFICATION "IDENTIFICATION"
    #define KEY_code               "code"
    #define KEY_codeSpace          "codeSpace"
    #define KEY_DatasetTitle       "DatasetTitle"
    #define SECTION_OVERVIEW        "OVERVIEW"
    #define SECTION_OVVW_ASPECTES_TECNICS "OVERVIEW:ASPECTES_TECNICS"
    #define KEY_ArcSource           "ArcSource"
    #define SECTION_EXTENT          "EXTENT"
    #define KEY_toler_env           "toler_env"
    #define KEY_MinX                "MinX"
    #define KEY_MaxX                "MaxX"
    #define KEY_MinY                "MinY"
    #define KEY_MaxY                "MaxY"
    #define KEY_CreationDate        "CreationDate"
    #define SECTION_SPATIAL_REFERENCE_SYSTEM "SPATIAL_REFERENCE_SYSTEM"
    #define SECTION_HORIZONTAL      "HORIZONTAL"
    #define KEY_HorizontalSystemIdentifier "HorizontalSystemIdentifier"
    #define SECTION_TAULA_PRINCIPAL "TAULA_PRINCIPAL"
    #define KEY_IdGrafic            "IdGrafic"
    #define KEY_TipusRelacio        "TipusRelacio"
    #define KEY_descriptor          "descriptor"
    #define KEY_HorizontalSystemDefinition "HorizontalSystemDefinition"
    #define KEY_unitats             "unitats"
    #define KEY_unitatsY            "unitatsY"
#endif


#define BOOL_CHAR int

// Types of layers in MiraMon
#define MM_LayerType_Unknown    0 // Unknown type
#define MM_LayerType_Point      1 // Layer of Points
#define MM_LayerType_Point3d    2 // Layer of 3D Points
#define MM_LayerType_Arc        3 // Layer of Arcs
#define MM_LayerType_Arc3d      4 // Layer of 3D Arcs
#define MM_LayerType_Pol        5 // Layer of Polygons
#define MM_LayerType_Pol3d      6 // Layer of 3D Polygons
#define MM_LayerType_Raster     7 // Layer of Raster Type

#define MM_FIRST_NUMBER_OF_POINTS 10000
#define MM_INCR_NUMBER_OF_POINTS    1000
#define MM_FIRST_NUMBER_OF_ARCS     10000
#define MM_INCR_NUMBER_OF_ARCS      1000
#define MM_FIRST_NUMBER_OF_NODES     20000  // 2*MM_FIRST_NUMBER_OF_ARCS
#define MM_INCR_NUMBER_OF_NODES      2000
#define MM_FIRST_NUMBER_OF_POLYGONS 10000
#define MM_INCR_NUMBER_OF_POLYGONS  1000
#define MM_INCR_NUMBER_OF_VERTICES      1000

#define MM_500MB   524288000

// Version asked for user
#define MM_UNKNOWN_VERSION    0
#define MM_LAST_VERSION       1
#define MM_32BITS_VERSION     2
#define MM_64BITS_VERSION     3

// AddFeature returns
#define MM_CONTINUE_WRITING_FEATURES        0
#define MM_FATAL_ERROR_WRITING_FEATURES     1
#define MM_STOP_WRITING_FEATURES            2

// Size of the FID (and OFFSETS) in the current version
#define MM_SIZE_OF_FID_4BYTES_VERSION   4
#define MM_SIZE_OF_FID_8BYTES_VERSION   8


/*  Different values that first member of every PAL section element can take*/
#define MM_EXTERIOR_ARC_SIDE    0x01
#define MM_END_ARC_IN_RING      0x02
#define MM_ROTATE_ARC           0x04

#define ARC_VRT_INICI  0
#define ARC_VRT_FI     1

#define STATISTICAL_UNDEF_VALUE (2.9E+301)

#define MAXIMUM_OBJECT_INDEX_IN_2GB_VECTORS _UI32_MAX 
#define MAXIMUM_OFFSET_IN_2GB_VECTORS _UI32_MAX

// Number of rings a polygon could have (it's just an aproximation)
#define MM_MEAN_NUMBER_OF_RINGS 10  

// Number of coordinates a feature could have (it's just an aproximation)
#define MM_MEAN_NUMBER_OF_COORDS 1000  

enum FieldType
{
  /*! Numeric Field                                         */ MM_Numeric=0,
  /*! Character Fi eld                                      */ MM_Character=1,
  /*! Data Field                                            */ MM_Data=2,
  /*! Logic Field                                           */ MM_Logic=3
};


// Size of disk parts of the MiraMon vectorial format
// Common header
#define MM_HEADER_SIZE_32_BITS  48
#define MM_HEADER_SIZE_64_BITS  64

// Points
#define MM_SIZE_OF_TL           16

// Nodes
#define MM_SIZE_OF_NH_32BITS    8
#define MM_SIZE_OF_NH_64BITS    12
#define MM_SIZE_OF_NL_32BITS    4
#define MM_SIZE_OF_NL_64BITS    8

// Arcs
#define MM_SIZE_OF_AH_32BITS    56
#define MM_SIZE_OF_AH_64BITS    72
#define MM_SIZE_OF_AL           16

// Polygons
#define MM_SIZE_OF_PS_32BITS    8
#define MM_SIZE_OF_PS_64BITS    16
#define MM_SIZE_OF_PH_32BITS    64
#define MM_SIZE_OF_PH_64BITS    80
#define MM_SIZE_OF_PAL_32BITS   5
#define MM_SIZE_OF_PAL_64BITS   9

// 3D part
#define MM_SIZE_OF_ZH           32
#define MM_SIZE_OF_ZD_32_BITS   24
#define MM_SIZE_OF_ZD_64_BITS   32

/* -------------------------------------------------------------------- */
/*      Structures                                                      */
/* -------------------------------------------------------------------- */
// Auxiliary structures
struct MMBoundingBox
{
    double dfMinX;
    double dfMaxX;
    double dfMinY;
    double dfMaxY;
};

struct MM_POINT_2D
{
    double dfX;
    double dfY;
};

struct ARC_VRT_STRUCTURE
{
	struct MM_POINT_2D vertice;   
	BOOL_CHAR bIniFi; // boolean:  0=inicial, 1=final
	unsigned __int64 nIArc;  // Internal arc index 
	unsigned __int64 nINod;	// Internal node index, empty at the beginning */
};

struct MM_REINDEXATOR
{
	unsigned __int64 nIOri;
	unsigned __int64 nIFinal;
};

// Top Header section
struct MM_TH 
{
    char aLayerVersion[2];
    char aLayerSubVersion;
    
    char aFileType[3]; // (PNT, ARC, NOD, POL)

    int bIs3d;
    int bIsMultipolygon; // Only apply to polygons

    unsigned char Flag;  // 1 byte: defined at DefTopMM.H
    struct MMBoundingBox hBB;
    unsigned __int64 nElemCount;  // 4/8 bytes depending on the version
    // 8/4 reserved bytes depending on the version
};

struct MM_FLUSH_INFO
{
    __int32 nMyDiskSize;
    unsigned __int64 NTimesFlushed;

    // Pointer to an OPEN file where to flush.
    FILE_TYPE *pF; 
    // Offset in the disk where to flush 
    unsigned __int64 OffsetWhereToFlush;
    
    unsigned __int64 TotalSavedBytes;  // Internal use


    // Block where to be saved
    unsigned __int64 SizeOfBlockToBeSaved;
    void *pBlockToBeSaved;

    // Block where to save the pBlockToBeSaved
    void *pBlockWhereToSave;
    // Number of full bytes: flushed every time it's needed
    unsigned __int64 nNumBytes;
    // Number of bytes allocated: flushed every time it's needed
    unsigned __int64 nBlockSize;

    // Internal Use
    unsigned __int64 CurrentOffset;
};

// Z Header (32 bytes)
struct MM_ZH 
{
    __int32 nMyDiskSize;
    // 16 bytes reserved
    double dfBBminz; // 8 bytes Minimum Z
    double dfBBmaxz; // 8 bytes Maximum Z
};

// Z Description
struct MM_ZD 
{
    double dfBBminz; // 8 bytes Minimum Z
    double dfBBmaxz; // 8 bytes Maximum Z
    unsigned __int32 nZCount;  // 4 bytes
    // 4 bytes reserved (Only in version 2.0)
    unsigned __int64 nOffsetZ; // 4 or 8 bytes depending on the version
};
   
struct MM_ZSection
{
    // Offset where the section begins in disk. It's a precalculated value
    // using nElemCount from LayerInfo. TH+n*CL
    unsigned __int64 ZSectionOffset;
    struct MM_ZH ZHeader;   // (I mode)
    
    // Number of pZDescription allocated
    // nMaxZDescription = nElemCount from LayerInfo
    unsigned __int64 ZDOffset;
    __int32 nZDDiskSize;
    unsigned __int64 nMaxZDescription; 
    struct MM_ZD *pZDescription; //(I mode)

    struct MM_FLUSH_INFO FlushZL;
    char *pZL;  // (II mode)
};

// Header of Arcs
struct MM_AH
{
    struct MMBoundingBox dfBB;
    unsigned __int64 nElemCount; // 4/8 bytes depending on the version
    unsigned __int64 nOffset; // 4/8 bytes depending on the version
    unsigned __int64 nFirstIdNode; // 4/8 bytes depending on the version
    unsigned __int64 nLastIdNode; // 4/8 bytes depending on the version
    double dfLenght;
};

// Header of Nodes
struct MM_NH
{
    short int nArcsCount;
    char cNodeType;
    // 1 reserved byte
    unsigned __int64 nOffset; // 4/8 bytes depending on the version
};

// Header of Polygons
struct MM_PH
{
    // Common Arc & Polyons section
    struct MMBoundingBox dfBB;
    unsigned __int64 nArcsCount; // 4/8 bytes depending on the version
    unsigned __int64 nExternalRingsCount; // 4/8 bytes depending on the version
    unsigned __int64 nRingsCount; // 4/8 bytes depending on the version
    unsigned __int64 nOffset; // 4/8 bytes depending on the version
    double dfPerimeter;
    double dfArea;
    //struct GEOMETRIC_I_TOPOLOGIC_POL GeoTopoPol;
};

/*  Every MiraMon file is composed as is specified in documentation.
    Here are the structures to every file where we can find two ways
    of keeping the information in memory (to be, finally, flushed to the disk)
        * (I mode)Pointers to structs that keep information that changes every time
          a feature is added. They will be written at the end on disk.
        * (II mode)Memory blocs that are used as buffer blocs to store information that
          is going to be flushed (as are) at the disc periodically instead 
          of writing them to the disc every time a Feature is added (not 
          eficient). The place where they are going to be flushed depends
          on one variable: the number of elements of the layer.
*/

// MiraMon Point Layer: TH, List of CL (coordiantes), ZH, ZD, ZL
struct MiraMonPointLayer
{
    // Name of the layer with extension
    char *pszLayerName; 
    FILE_TYPE *pF;
    
    // Coordinates x,y of the points
    struct MM_FLUSH_INFO FlushTL;
    char *pTL; // (II mode)
    char *pszTLName; // Temporary file where to flush
    FILE_TYPE *pFTL; // Pointer to temporary file where to flush
    
    // Z section
    // Temporal file where the Z coordinates are stored
    // if necessary
    char *psz3DLayerName;
    FILE_TYPE *pF3d; 
    struct MM_ZSection pZSection;
};

struct MiraMonNodeLayer
{
    char *pszLayerName; // Name of the layer with extension
    FILE_TYPE *pF;
    
    // Header of every node
    __int32 nSizeNodeHeader;
    unsigned __int64 nMaxNodeHeader; // Number of pNodeHeader allocated
    struct MM_NH *pNodeHeader;// (I mode)
    
    // NL: arcs confuent to node 
    struct MM_FLUSH_INFO FlushNL; // (II mode)
    char *pNL; // 
    char *pszNLName; // Temporary file where to flush
    FILE_TYPE *pFNL; // Pointer to temporary file where to flush
};

struct MiraMonArcLayer
{
    char *pszLayerName; // Name of the layer with extension
    FILE_TYPE *pF;

    // Temporal file where the Z coordinates are stored
    // if necessary
    char *psz3DLayerName;
    FILE_TYPE *pF3d; 
                
    // Header of every arc
    __int32 nSizeArcHeader;
    unsigned __int64 nMaxArcHeader; // Number of allocated pArcHeader 
    struct MM_AH *pArcHeader;// (I mode)

    // AL Section
    struct MM_FLUSH_INFO FlushAL;
    int nALElementSize; //    16 // Two double coordinates
    char *pAL; // Arc List  // (II mode)
    char *pszALName; // Temporary file where to flush
    FILE_TYPE *pFAL; // Pointer to temporary file where to flush
        
    // Z section
    struct MM_ZSection pZSection;

    // An arc layer needs node structures.
    struct MM_TH TopNodeHeader;
    struct MiraMonNodeLayer MMNode;

    // An Arc layer can store some ellipsoidal information
    double *pEllipLong;
    struct SNY_TRANSFORMADOR_GEODESIA *GeodesiaTransform;

    // Private data
    unsigned __int64 nMaxArcVrt; // Number of allocated 
    struct ARC_VRT_STRUCTURE *pArcVrt;
    unsigned __int64 nOffsetArc; // It's an auxiliary offset
};

struct MiraMonPolygonLayer
{
    char *pszLayerName; // Name of the layer with extension
    FILE_TYPE *pF;

    // PS part
    struct MM_FLUSH_INFO FlushPS;
    int nPSElementSize; 
    char *pPS;  // Polygon side (II mode)
    char *pszPSName; // Temporary file where to flush
    FILE_TYPE *pFPS; // Pointer to temporary file where to flush
    
    // Header of every polygon
    unsigned __int64 nMaxPolHeader; // Number of pPolHeader allocated
    int nPHElementSize;
    struct MM_PH *pPolHeader;// (I mode)
    
    // PAL
    struct MM_FLUSH_INFO FlushPAL;
    char *pPAL; // Polygon Arc List  // (II mode)
    char *pszPALName; // Temporary file where to flush
    FILE_TYPE *pFPAL; // Pointer to temporary file where to flush

    // A polygon layer needs arcs structure.
    struct MM_TH TopArcHeader;
    struct MiraMonArcLayer MMArc;
};

#define MM_VECTOR_LAYER_LAST_VERSION    1
#define CheckMMVectorLayerVersion(a,r){if((a)->Version!=MM_VECTOR_LAYER_LAST_VERSION)return (r);}


// MIRAMON METADATA
struct MiraMonMetaData
{
    char aLayerName[MM_MAX_LEN_LAYER_NAME];
    int eLT; //LayerType
	char *pSRS; // EPSG code of the coordinate system information.
    char *pXUnit; // X units if pszSRS is empty. 
    char *pYUnit; // Y units if pszSRS is empty. If Y units is empty,
                  // X unit will be assigned as Y unit by default.

	unsigned __int32 nBands; // number of bands (in vector layer, always 1).

    struct MMBoundingBox *hBB; // Bounding box of every band of a raster file
                               // or bounding box of the entire layer in case
                               // of a vector file.

    // Only for rasters, for each band:
    char **pBandName;
    unsigned __int32 *pnBandNumber;
    char **pBandDescription;
    unsigned __int64 *nNCol;
    unsigned __int64 *nNFil;
    double *pXResolution;
    double *pYResolution;
    enum DataType *peDataType;
    enum TreatmentVariable *peTreatmentVariable;
    char **pValueUnit;
    BOOL_CHAR *pHasANoDataVaule;
    double *pNoDataValue;

	/*
    ·$· Metadata que s'ha de calcular/revisar mentre s'afegeixen features:    
    · Cas ràster:
        Nom fitxer de sortida per cada banda
        Mínim i màxim dels valors dels píxels
        DonaMD_CoverageContentType? (revisar GDALMM2.c sobre la línia 632
            
            */
};

// MIRAMON DATA BASE
struct MiraMonField
{
	char *pValue;		// For string records (or value of a type
						// of field not included in the following lines).
	double dfValue;		// For numeric records (Integer,Integer64 or Double). 
	char aValue[8];		// For data records with YYYYMMDD format.
	BOOL_CHAR bValue;	// For binary records.
};

struct MiraMonRecord
{
	unsigned __int32 nField;	// Number of fields
	struct MiraMonField *pField;	// Info of the fields.
};


// Information that allows to reuse memory stuff when
// features are being read
struct MiraMonFeature
{
    // A MiraMon Feature 
    unsigned __int64 nNRings; // =1 for lines and points
    unsigned __int64 nIRing; // The ring is being processed
    
    // Number of reserved elements in *pNCoord
    unsigned __int64 nMaxpNCoord; 
    unsigned __int64 *pNCoord; // [0]=1 for lines and points

    // Number of reserved elements in *pCoord
    unsigned __int64 nMaxpCoord; 
    // Coordinate index thats is being processed
    unsigned __int64 nICoord; 
    // List of the coordinates of the feature
    struct MM_POINT_2D *pCoord; 

    // Number of reserved elements in *pbArcInfo
    unsigned __int64 nMaxpbArcInfo;
	int *pbArcInfo; // In case of multipolygons, for each ring:
						 // TRUE if it's a outer ring,
						 // FALSE if it's a inner ring.

    // List of the Z-coordinates (as many as pCoord)
    // Number of reserved elements in *pZCoord
    unsigned __int64 nMaxpZCoord; 
    double *pZCoord; 

    // Records of the feature
	unsigned __int32 nRecords;
    // Number of reserved elements in *pRecords
    unsigned __int32 nMaxpRecords; 
	struct MiraMonRecord *pRecords;

};

struct MiraMonDataBaseField
{
    char pszFieldName[MM_MAX_LON_NAME_DBF_FIELD];
    char pszFieldDescription[MM_MAX_BYTES_FIELD_DESC];
    enum FieldType eFieldType;
    unsigned __int32 nFieldSize; // MM_MAX_BYTES_IN_A_FIELD as maximum
    unsigned __int32 nNumberOfDecimals; // MM_MAX_BYTES_IN_A_FIELD as maximum
    BOOL_CHAR bIsIdGraph; // For integer fields, if it is a IdentityGraph field
};

struct MiraMonDataBase
{
    unsigned __int32 nNFields;
    struct MiraMonDataBaseField *pFields;
};

// MIRAMON OBJECT: Contains everything
struct MiraMonLayerInfo
{
    // Version of the structure
    __int32 Version; 

    // Version of the layer
    // MM_32BITS_LAYER_VERSION: less than 2 Gb files
    // MM_64BITS_LAYER_VERSION: more than 2 Gb files
    char LayerVersion;
    
    char pszFlags[10]; // To Open the file
    int bIsPolygon;
    int bIsArc;
    int bIsPoint;
    
    // Number of elements of the layer. It's mandatory knowing 
    // it before writing the layer on disk.
    unsigned __int64 nSuposedElemCount; 
    unsigned __int64 nFinalElemCount; // Real element count after conversion
    
    // Header of the layer
    __int32 nHeaderDiskSize;
    struct MM_TH TopHeader;

    // If is a point layer
    struct MiraMonPointLayer MMPoint;

    // If is an arc layer
    struct MiraMonArcLayer MMArc;
    
    // If is a polygon layer 
    struct MiraMonPolygonLayer MMPolygon;

    // Atributes of the layer
    struct MiraMonDataBase *attributes;

    // Offset used to write features.
    unsigned __int64 OffsetCheck;

    // MiraMon Metadata: allows to write REL files.
    struct MiraMonMetaData hMMMD;

    // MiraMon database: allows to write DBF (extended id nedeed) files
    struct MiraMonDataBase hMMDB; // estructura per definicions camps de la base de dades.
};
enum DataType {MMDTByte, MMDTInteger, MMDTuInteger, 
               MMDTLong, MMDTReal, MMDTDouble, MMDT4bits};
enum TreatmentVariable {MMTVQuantitativeContinuous, MMTVOrdinal, MMTVCategorical};

CPL_C_END // Necessary for compiling in GDAL project
#endif //__MMSTRUCT_H
