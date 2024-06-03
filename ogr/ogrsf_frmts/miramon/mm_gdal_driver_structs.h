#ifndef __MM_GDAL_DRIVER_STRUCTS_H
#define __MM_GDAL_DRIVER_STRUCTS_H
/* -------------------------------------------------------------------- */
/*      Necessary functions to read/write a MiraMon Vector File         */
/* -------------------------------------------------------------------- */

#ifdef GDAL_COMPILATION
#include "mm_gdal_constants.h"
#include "mm_gdal_structures.h"

CPL_C_START  // Necessary for compiling in GDAL project
#else
#include <stdio.h>  // For FILE
#include "mm_constants.h"
#include "mm_gdal\mm_gdal_structures.h"
#endif

// For MetaData
#define SECTION_VERSIO "VERSIO"
#define KEY_Vers "Vers"
#define KEY_SubVers "SubVers"
#define MM_VERS 4
#define MM_SUBVERS_ACCEPTED 0
#define MM_SUBVERS 3
#define KEY_VersMetaDades "VersMetaDades"
#define KEY_SubVersMetaDades "SubVersMetaDades"
#define MM_VERS_METADADES_ACCEPTED 4
#define MM_VERS_METADADES 5
#define MM_SUBVERS_METADADES 0
#define SECTION_METADADES "METADADES"
#define KEY_FileIdentifier "FileIdentifier"
#define SECTION_IDENTIFICATION "IDENTIFICATION"
#define KEY_code "code"
#define KEY_codeSpace "codeSpace"
#define KEY_DatasetTitle "DatasetTitle"
#define SECTION_OVERVIEW "OVERVIEW"
#define SECTION_OVVW_ASPECTES_TECNICS "OVERVIEW:ASPECTES_TECNICS"
#define KEY_ArcSource "ArcSource"
#define SECTION_EXTENT "EXTENT"
#define KEY_toler_env "toler_env"
#define KEY_MinX "MinX"
#define KEY_MaxX "MaxX"
#define KEY_MinY "MinY"
#define KEY_MaxY "MaxY"
#define KEY_CreationDate "CreationDate"
#define SECTION_SPATIAL_REFERENCE_SYSTEM "SPATIAL_REFERENCE_SYSTEM"
#define SECTION_HORIZONTAL "HORIZONTAL"
#define KEY_HorizontalSystemIdentifier "HorizontalSystemIdentifier"
#define SECTION_TAULA_PRINCIPAL "TAULA_PRINCIPAL"
#define KEY_IdGrafic "IdGrafic"
#define KEY_TipusRelacio "TipusRelacio"
#define KEY_descriptor "descriptor"
#define KEY_HorizontalSystemDefinition "HorizontalSystemDefinition"
#define KEY_unitats "unitats"
#define KEY_unitatsY "unitatsY"
#define KEY_language "language"
#define KEY_Value_eng "eng"
#define KEY_MDIdiom "MDIdiom"
#define KEY_characterSet "characterSet"
#define KEY_Value_characterSet "006"

// MiraMon feature field names
#define szMMNomCampIdGraficDefecte "ID_GRAFIC"
#define szMMNomCampPerimetreDefecte "PERIMETRE"
#define szMMNomCampAreaDefecte "AREA"
#define szMMNomCampLongitudArcDefecte "LONG_ARC"
#define szMMNomCampNodeIniDefecte "NODE_INI"
#define szMMNomCampNodeFiDefecte "NODE_FI"
#define szMMNomCampArcsANodeDefecte "ARCS_A_NOD"
#define szMMNomCampTipusNodeDefecte "TIPUS_NODE"
#define szMMNomCampNVertexsDefecte "N_VERTEXS"
#define szMMNomCampNArcsDefecte "N_ARCS"
#define szMMNomCampNPoligonsDefecte "N_POLIG"

#define MAX_RELIABLE_SF_DOUBLE                                                 \
    15  // Maximum nr. of reliable significant figures in any double.

// Maximum nr. of reliable significant figures
#define MM_MAX_XS_DOUBLE 17

// Initial width of MiraMon fields
#define MM_MIN_WIDTH_ID_GRAFIC 3
#define MM_MIN_WIDTH_N_VERTEXS 5
#define MM_MIN_WIDTH_INITIAL_NODE MM_MIN_WIDTH_ID_GRAFIC + 1
#define MM_MIN_WIDTH_FINAL_NODE MM_MIN_WIDTH_ID_GRAFIC + 1
#define MM_MIN_WIDTH_ARCS_TO_NODE 1
#define MM_MIN_WIDTH_LONG 14  // For LONG_ARC and PERIMETRE
#define MM_MIN_WIDTH_AREA 19  // For LONG_ARC and PERIMETRE

#define MM_MIN_WIDTH_N_ARCS 2
#define MM_MIN_WIDTH_N_POLIG 2

// Types of layers in MiraMon
#define MM_LayerType_Unknown 0  // Unknown type, or DBF alone
#define MM_LayerType_Point 1    // Layer of Points
#define MM_LayerType_Point3d 2  // Layer of 3D Points
#define MM_LayerType_Arc 3      // Layer of Arcs
#define MM_LayerType_Arc3d 4    // Layer of 3D Arcs
#define MM_LayerType_Pol 5      // Layer of Polygons
#define MM_LayerType_Pol3d 6    // Layer of 3D Polygons
#define MM_LayerType_Node 7     // Layer of Nodes (internal)
#define MM_LayerType_Raster 8   // Layer of Raster Type

// FIRST are used for a first allocation and
// INCR for needed memory increase
#define MM_FIRST_NUMBER_OF_POINTS 100000    // 3.5 Mb
#define MM_INCR_NUMBER_OF_POINTS 100000     // 3.5 Mb
#define MM_FIRST_NUMBER_OF_ARCS 100000      // 5.3 Mb
#define MM_INCR_NUMBER_OF_ARCS 100000       // 5.3 Mb
#define MM_FIRST_NUMBER_OF_NODES 200000     // 2*MM_FIRST_NUMBER_OF_ARCS 1.5 Mb
#define MM_INCR_NUMBER_OF_NODES 200000      // 1.5 Mb
#define MM_FIRST_NUMBER_OF_POLYGONS 100000  // 6 Mb
#define MM_INCR_NUMBER_OF_POLYGONS 100000   // 6 Mb
#define MM_FIRST_NUMBER_OF_VERTICES 10000
#define MM_INCR_NUMBER_OF_VERTICES 1000

#define MM_1MB 1048576  // 1 MB of buffer

// Version asked for user
#define MM_UNKNOWN_VERSION 0
#define MM_LAST_VERSION 1
#define MM_32BITS_VERSION 2
#define MM_64BITS_VERSION 3

// AddFeature returns
#define MM_CONTINUE_WRITING_FEATURES 0
#define MM_FATAL_ERROR_WRITING_FEATURES 1
#define MM_STOP_WRITING_FEATURES 2

// Size of the FID (and OFFSETS) in the current version
#define MM_SIZE_OF_FID_4BYTES_VERSION 4
#define MM_SIZE_OF_FID_8BYTES_VERSION 8

/*  Different values that first member of every PAL section element can take*/
#define MM_EXTERIOR_ARC_SIDE 0x01
#define MM_END_ARC_IN_RING 0x02
#define MM_ROTATE_ARC 0x04

#define ARC_VRT_INICI 0
#define ARC_VRT_FI 1

#define STATISTICAL_UNDEF_VALUE (2.9E+301)

#define MAXIMUM_OBJECT_INDEX_IN_2GB_VECTORS UINT32_MAX  //_UI32_MAX
#define MAXIMUM_OFFSET_IN_2GB_VECTORS UINT32_MAX        //_UI32_MAX

// Number of rings a polygon could have (it is just an initial approximation)
#define MM_MEAN_NUMBER_OF_RINGS 10

// Number of coordinates a feature could have (it is just an initial approximation)
#define MM_MEAN_NUMBER_OF_NCOORDS 100
#define MM_MEAN_NUMBER_OF_COORDS 1000

// Initial and increment number of records and fields.
#define MM_INIT_NUMBER_OF_RECORDS 1
#define MM_INC_NUMBER_OF_RECORDS 5
#define MM_INIT_NUMBER_OF_FIELDS 20
#define MM_INC_NUMBER_OF_FIELDS 10

    enum FieldType {
        /*! Numeric Field                                         */
        MM_Numeric = 0,
        /*! Character Fi eld                                      */
        MM_Character = 1,
        /*! Data Field                                            */ MM_Data =
            2,
        /*! Logic Field                                           */ MM_Logic =
            3
    };

// Size of disk parts of the MiraMon vector format
// Common header
#define MM_HEADER_SIZE_32_BITS 48
#define MM_HEADER_SIZE_64_BITS 64

// Points
#define MM_SIZE_OF_TL 16

// Nodes
#define MM_SIZE_OF_NH_32BITS 8
#define MM_SIZE_OF_NH_64BITS 12
#define MM_SIZE_OF_NL_32BITS 4
#define MM_SIZE_OF_NL_64BITS 8

// Arcs
#define MM_SIZE_OF_AH_32BITS 56
#define MM_SIZE_OF_AH_64BITS 72
#define MM_SIZE_OF_AL 16

// Polygons
#define MM_SIZE_OF_PS_32BITS 8
#define MM_SIZE_OF_PS_64BITS 16
#define MM_SIZE_OF_PH_32BITS 64
#define MM_SIZE_OF_PH_64BITS 80
#define MM_SIZE_OF_PAL_32BITS 5
#define MM_SIZE_OF_PAL_64BITS 9

// 3D part
#define MM_SIZE_OF_ZH 32
#define MM_SIZE_OF_ZD_32_BITS 24
#define MM_SIZE_OF_ZD_64_BITS 32

// Coordinates
#define MM_SIZE_OF_COORDINATE 16

// Recode in DBF's
#define MM_RECODE_UTF8 0
#define MM_RECODE_ANSI 1

// Language in REL files:
// It is the language of the MiraMon generated descriptors.
// Metadata will not be translated but these descriptors are
// generated from scratch and it is good to use a custom language.
#define MM_DEF_LANGUAGE 0
#define MM_ENG_LANGUAGE 1  // English language
#define MM_CAT_LANGUAGE 2  // Catalan language
#define MM_SPA_LANGUAGE 3  // Spanish language

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
    MM_BOOLEAN bIniFi;      // boolean:  0=initial, 1=final
    MM_INTERNAL_FID nIArc;  // Internal arc index
    MM_INTERNAL_FID nINod;  // Internal node index, empty at the beginning */
};

struct MM_VARIABLES_LLEGEIX_POLS
{
    size_t Nomb_Max_Coord;
    size_t Bloc_Max_Coord;
    size_t Nomb_Max_Coord_Z;
    size_t Nomb_Max_avnp;
    size_t Nomb_Max_Elem;
    size_t Nomb_Max_vora_de_qui;
};

struct MM_FLUSH_INFO
{
    size_t nMyDiskSize;
    GUInt64 NTimesFlushed;

    // Pointer to an OPEN file where to flush.
    FILE_TYPE *pF;
    // Offset in the disk where to flush
    MM_FILE_OFFSET OffsetWhereToFlush;

    GUInt64 TotalSavedBytes;  // Internal use

    // Block where to be saved
    size_t SizeOfBlockToBeSaved;
    void *pBlockToBeSaved;

    // Block where to save the pBlockToBeSaved or read from
    void *pBlockWhereToSaveOrRead;
    // Number of full bytes: flushed every time it is needed
    GUInt64 nNumBytes;
    // Number of bytes allocated: flushed every time it is needed
    GUInt64 nBlockSize;

    // Internal Use
    MM_FILE_OFFSET CurrentOffset;
};

// MIRAMON METADATA
struct MiraMonVectorMetaData
{
    char *szLayerTitle;
    char *aLayerName;
    char *aArcFile;  // Polygon's arc name or arc's polygon name.
    int ePlainLT;    // Plain layer type (no 3D specified): MM_LayerType_Point,
                     // MM_LayerType_Arc, MM_LayerType_Node, MM_LayerType_Pol
    char *pSRS;      // EPSG code of the spatial reference system.
    char *pXUnit;    // X units if pszSRS is empty.
    char *pYUnit;    // Y units if pszSRS is empty. If Y units is empty,
                     // X unit will be assigned as Y unit by default.

    struct MMBoundingBox hBB;  // Bounding box of the entire layer

    // Pointer to a Layer DataBase, used to create MiraMon DBF (extended) file.
    struct MiraMonDataBase *pLayerDB;

    // Language in REL files:
    // It is the language of the MiraMon generated descriptors.
    // Metadata will not be translated but these descriptors are
    // generated from scratch and it is good to use a custom language.
    char nMMLanguage;
};

// MIRAMON DATA BASE
#define MM_GRAPHICAL_ID_INIT_SIZE 5
#define MM_N_VERTEXS_INIT_SIZE 12
#define MM_LONG_ARC_INIT_SIZE 12
#define MM_LONG_ARC_DECIMALS_SIZE 6
#define MM_NODE_INI_INIT_SIZE 5
#define MM_NODE_FI_INIT_SIZE 5

#define MM_PERIMETRE_INIT_SIZE 13
#define MM_PERIMETRE_DECIMALS_SIZE 6
#define MM_AREA_INIT_SIZE 14
#define MM_AREA_DECIMALS_SIZE 6

#define MM_N_ARCS_INIT_SIZE 3
#define MM_N_ARCS_DECIMALS_SIZE 3

#define MM_ARCS_A_NOD_INIT_SIZE 1

struct MiraMonFieldValue
{
    MM_BOOLEAN bIsValid;  // If 1 the value is filled. If 0, there is no value.
    MM_EXT_DBF_N_FIELDS nNumDinValue;  // Size of the reserved string value
    char *pDinValue;                   // Used to store the value as string
    GInt64 iValue;                     // For 64 bit integer values.
};

struct MiraMonRecord
{
    MM_EXT_DBF_N_FIELDS nMaxField;     // Number of reserved fields
    MM_EXT_DBF_N_FIELDS nNumField;     // Number of fields
    struct MiraMonFieldValue *pField;  // Value of the fields.
};

struct MiraMonDataBaseField
{
    char pszFieldName[MM_MAX_LON_FIELD_NAME_DBF + 1];
    char pszFieldDescription[MM_MAX_BYTES_FIELD_DESC + 1];
    enum FieldType eFieldType;   // See enum FieldType
    GUInt32 nFieldSize;          // MM_MAX_BYTES_IN_A_FIELD_EXT as maximum
    GUInt32 nNumberOfDecimals;   // MM_MAX_BYTES_IN_A_FIELD_EXT as maximum
    MM_BOOLEAN bIs64BitInteger;  // For 64 bits integer fields
};

struct MiraMonDataBase
{
    MM_EXT_DBF_N_FIELDS nNFields;
    struct MiraMonDataBaseField *pFields;
};

struct MMAdmDatabase
{
    // MiraMon table (extended DBF)
    // Name of the extended DBF file
    char pszExtDBFLayerName[MM_CPL_PATH_BUF_SIZE];
    // Pointer to a MiraMon table (auxiliary)
    struct MM_DATA_BASE_XP *pMMBDXP;
    // How to write all it to disk
    struct MM_FLUSH_INFO FlushRecList;
    char *pRecList;  // Records list  // (II mode)

    // Temporary space where to mount the DBF record.
    // Reused every time a feature is created
    GUInt64 nNumRecordOnCourse;
    char *szRecordOnCourse;
};

struct MM_ID_GRAFIC_MULTIPLE_RECORD
{
    MM_FILE_OFFSET offset;
    MM_EXT_DBF_N_MULTIPLE_RECORDS
    nMR;  // Determines the number of the list (multiple record)
};

// MIRAMON GEOMETRY

// Top Header section
struct MM_TH
{
    char aLayerVersion[2];
    char aLayerSubVersion;

    char aFileType[3];  // (PNT, ARC, NOD, POL)

    unsigned short int bIs3d;
    unsigned short int bIsMultipolygon;  // Only apply to polygons

    unsigned char Flag;  // 1 byte: defined at DefTopMM.H
    struct MMBoundingBox hBB;
    MM_INTERNAL_FID nElemCount;  // 4/8 bytes depending on the version
    // 8/4 reserved bytes depending on the version
};

// Z Header (32 bytes)
struct MM_ZH
{
    size_t nMyDiskSize;
    // 16 bytes reserved
    double dfBBminz;  // 8 bytes Minimum Z
    double dfBBmaxz;  // 8 bytes Maximum Z
};

// Z Description
struct MM_ZD
{
    double dfBBminz;  // 8 bytes Minimum Z
    double dfBBmaxz;  // 8 bytes Maximum Z
    GInt32 nZCount;   // 4 bytes (signed)
    // 4 bytes reserved (only in version 2.0)
    MM_FILE_OFFSET nOffsetZ;  // 4 or 8 bytes depending on the version
};

struct MM_ZSection
{
    // Offset where the section begins in disk. It is a precalculated value
    // using nElemCount from LayerInfo. TH+n*CL
    MM_FILE_OFFSET ZSectionOffset;
    struct MM_ZH ZHeader;  // (I mode)

    // Number of pZDescription allocated
    // nMaxZDescription = nElemCount from LayerInfo
    MM_FILE_OFFSET ZDOffset;
    size_t nZDDiskSize;
    GUInt64 nMaxZDescription;
    struct MM_ZD *pZDescription;  //(I mode)

    struct MM_FLUSH_INFO FlushZL;
    char *pZL;  // (II mode)
};

// Header of Arcs
struct MM_AH
{
    struct MMBoundingBox dfBB;
    MM_N_VERTICES_TYPE nElemCount;  // 4/8 bytes depending on the version
    MM_FILE_OFFSET nOffset;         // 4/8 bytes depending on the version
    MM_INTERNAL_FID nFirstIdNode;   // 4/8 bytes depending on the version
    MM_INTERNAL_FID nLastIdNode;    // 4/8 bytes depending on the version
    double dfLength;
};

// Header of Nodes
struct MM_NH
{
    short int nArcsCount;
    char cNodeType;
    // 1 reserved byte
    MM_FILE_OFFSET nOffset;  // 4/8 bytes depending on the version
};

// Header of Polygons
struct MM_PH
{
    // Common Arc & Polyons section
    struct MMBoundingBox dfBB;
    MM_POLYGON_ARCS_COUNT nArcsCount;  // 4/8 bytes depending on the version
    MM_POLYGON_RINGS_COUNT
    nExternalRingsCount;                 // 4/8 bytes depending on the version
    MM_POLYGON_RINGS_COUNT nRingsCount;  // 4/8 bytes depending on the version
    MM_FILE_OFFSET nOffset;              // 4/8 bytes depending on the version
    double dfPerimeter;
    double dfArea;
    //struct GEOMETRIC_I_TOPOLOGIC_POL GeoTopoPol;
};

struct MM_PAL_MEM
{
    unsigned char VFG;
    MM_INTERNAL_FID nIArc;  // 4/8 bytes depending on the version
};

/*  Every MiraMon file is composed as is specified in documentation.
    Here are the structures to every file where we can find two ways
    of keeping the information in memory (to be, finally, flushed to the disk)
        * (I mode) Pointers to structs that keep information that changes every
          time a feature is added. They will be written at the end to the disk.
        * (II mode) Memory blocks that are used as buffer blocks to store
          information that is going to be flushed (as are) to the disk
          periodically instead of writing them to the disk every time a Feature
          is added (not efficient). The place where they are going to be flushed
          depends on one variable: the number of elements of the layer.
*/

// MiraMon Point Layer: TH, List of CL (coordinates), ZH, ZD, ZL
struct MiraMonPointLayer
{
    // Name of the layer with extension
    char pszLayerName[MM_CPL_PATH_BUF_SIZE];
    FILE_TYPE *pF;

    // Coordinates x,y of the points
    struct MM_FLUSH_INFO FlushTL;
    char *pTL;                             // (II mode)
    char pszTLName[MM_CPL_PATH_BUF_SIZE];  // Temporary file where to flush
    FILE_TYPE *pFTL;  // Pointer to temporary file where to flush

    // Z section
    // Temporary file where the Z coordinates are stored
    // if necessary
    char psz3DLayerName[MM_CPL_PATH_BUF_SIZE];
    FILE_TYPE *pF3d;
    struct MM_ZSection pZSection;

    // MiraMon table (extended DBF)
    struct MMAdmDatabase MMAdmDB;

    // Metadata name
    char pszREL_LayerName[MM_CPL_PATH_BUF_SIZE];
};

struct MiraMonNodeLayer
{
    char
        pszLayerName[MM_CPL_PATH_BUF_SIZE];  // Name of the layer with extension
    FILE_TYPE *pF;

    // Header of every node
    GUInt32 nSizeNodeHeader;
    MM_INTERNAL_FID nMaxNodeHeader;  // Number of pNodeHeader allocated
    struct MM_NH *pNodeHeader;       // (I mode)

    // NL: arcs confuent to node
    struct MM_FLUSH_INFO FlushNL;          // (II mode)
    char *pNL;                             //
    char pszNLName[MM_CPL_PATH_BUF_SIZE];  // Temporary file where to flush
    FILE_TYPE *pFNL;  // Pointer to temporary file where to flush

    struct MMAdmDatabase MMAdmDB;

    // Metadata name
    char pszREL_LayerName[MM_CPL_PATH_BUF_SIZE];
};

struct MiraMonArcLayer
{
    char
        pszLayerName[MM_CPL_PATH_BUF_SIZE];  // Name of the layer with extension
    FILE_TYPE *pF;

    // Temporal file where the Z coordinates are stored
    // if necessary
    char psz3DLayerName[MM_CPL_PATH_BUF_SIZE];
    FILE_TYPE *pF3d;

    // Header of every arc
    GUInt32 nSizeArcHeader;
    MM_INTERNAL_FID nMaxArcHeader;  // Number of allocated pArcHeader
    struct MM_AH *pArcHeader;       // (I mode)

    // AL Section
    struct MM_FLUSH_INFO FlushAL;
    unsigned short int nALElementSize;     // 16 bytes: 2 doubles (coordinates)
    char *pAL;                             // Arc List  // (II mode)
    char pszALName[MM_CPL_PATH_BUF_SIZE];  // Temporary file where to flush
    FILE_TYPE *pFAL;  // Pointer to temporary file where to flush

    // Z section
    struct MM_ZSection pZSection;

    // Node layer associated to the arc layer
    struct MM_TH TopNodeHeader;
    struct MiraMonNodeLayer MMNode;

    // Private data
    GUInt64 nMaxArcVrt;  // Number of allocated
    struct ARC_VRT_STRUCTURE *pArcVrt;
    MM_FILE_OFFSET nOffsetArc;  // It is an auxiliary offset

    struct MMAdmDatabase MMAdmDB;

    // Metadata name
    char pszREL_LayerName[MM_CPL_PATH_BUF_SIZE];
};

struct MiraMonPolygonLayer
{
    char
        pszLayerName[MM_CPL_PATH_BUF_SIZE];  // Name of the layer with extension
    FILE_TYPE *pF;

    // PS part
    struct MM_FLUSH_INFO FlushPS;
    unsigned short int nPSElementSize;
    char *pPS;                             // Polygon side (II mode)
    char pszPSName[MM_CPL_PATH_BUF_SIZE];  // Temporary file where to flush
    FILE_TYPE *pFPS;  // Pointer to temporary file where to flush

    // Header of every polygon
    MM_INTERNAL_FID nMaxPolHeader;  // Number of pPolHeader allocated
    unsigned short int nPHElementSize;
    struct MM_PH *pPolHeader;  // (I mode)

    // PAL
    struct MM_FLUSH_INFO FlushPAL;
    unsigned short int nPALElementSize;
    char *pPAL;                             // Polygon Arc List  // (II mode)
    char pszPALName[MM_CPL_PATH_BUF_SIZE];  // Temporary file where to flush
    FILE_TYPE *pFPAL;  // Pointer to temporary file where to flush

    // Arc layer associated to the arc layer
    struct MM_TH TopArcHeader;
    struct MiraMonArcLayer MMArc;

    struct MMAdmDatabase MMAdmDB;

    // Metadata name
    char pszREL_LayerName[MM_CPL_PATH_BUF_SIZE];
};

/*
#define MM_VECTOR_LAYER_LAST_VERSION 1
#define CheckMMVectorLayerVersion(a, r)                                        \
    {                                                                          \
        if ((a)->Version != MM_VECTOR_LAYER_LAST_VERSION)                      \
            return (r);                                                        \
    }
*/

// Information that allows to reuse memory stuff when
// features are being read
struct MiraMonFeature
{
    // Number of parts
    MM_POLYGON_RINGS_COUNT nNRings;  // =1 for lines and points
    MM_POLYGON_RINGS_COUNT nIRing;   // The ring is being processed

    // Number of reserved elements in *pNCoord (a vector with number of vertices in each ring)
    MM_N_VERTICES_TYPE nMaxpNCoordRing;
    MM_N_VERTICES_TYPE *pNCoordRing;  // [0]=1 for lines and points

    // Number of reserved elements in *pCoord
    MM_N_VERTICES_TYPE nMaxpCoord;
    // Number of used elements in *pCoord (only for reading features)
    MM_N_VERTICES_TYPE nNumpCoord;
    // Coordinate index that is being processed
    MM_N_VERTICES_TYPE nICoord;
    // List of the coordinates of the feature
    struct MM_POINT_2D *pCoord;

    // Number of reserved elements in *flag_VFG
    MM_INTERNAL_FID nMaxVFG;
    char *flag_VFG;  // In case of multipolygons, for each ring:
        // if flag_VFG[i]|MM_EXTERIOR_ARC_SIDE: outer ring if set
        // if flag_VFG[i]|MM_END_ARC_IN_RING: always set (every ring has only
        //                                    one arc)
        // if flag_VFG[i]|MM_ROTATE_ARC: coordinates are in the inverse order
        //                               of the read ones

    // List of the Z-coordinates (as many as pCoord)
    // Number of reserved elements in *pZCoord
    MM_N_VERTICES_TYPE nMaxpZCoord;
    // Number of used elements in *pZCoord
    MM_N_VERTICES_TYPE nNumpZCoord;
    MM_COORD_TYPE *pZCoord;
    MM_BOOLEAN bAllZHaveSameValue;

    // Records of the feature
    MM_EXT_DBF_N_MULTIPLE_RECORDS nNumMRecords;
    // Number of reserved elements in *pRecords
    MM_EXT_DBF_N_MULTIPLE_RECORDS nMaxMRecords;
    struct MiraMonRecord *pRecords;

    // Number of features just processed (for writing)
    MM_INTERNAL_FID nReadFeatures;
};

// There is the possibility of creating a map with all layers
// to visualize it with only one click
struct MiraMonVectMapInfo
{
    char pszMapName[MM_CPL_PATH_BUF_SIZE];
    FILE_TYPE *fMMMap;
    int nNumberOfLayers;
};

// MIRAMON OBJECT: Contains everything
struct MiraMonVectLayerInfo
{
    // Version of the structure
    //GUInt32 Version;

    // Version of the layer
    // MM_32BITS_LAYER_VERSION: less than 2 Gbyte files
    // MM_64BITS_LAYER_VERSION: more than 2 Gbyte files
    char LayerVersion;

    // Layer name
    char *pszSrcLayerName;

    // Layer title in metadata
    char *szLayerTitle;

    // Pointer to the main REL name (do not free it)
    char *pszMainREL_LayerName;

// To know if we are writing or reading
#define MM_READING_MODE 0  // Reading MiraMon layer
#define MM_WRITING_MODE 1  // Writing MiraMon layer
    MM_BOOLEAN ReadOrWrite;

    char pszFlags[10];  // To Open the file
    unsigned short int bIsPolygon;
    unsigned short int bIsArc;   // Also 1 in a polygon layer
    unsigned short int bIsNode;  // Not used in GDAL
    unsigned short int bIsPoint;
    unsigned short int bIsDBF;  // When there is no geometry

    // In writing mode when one of the features is 3D, the MM layer will be 3D,
    // but if none of the features are 3D, then the layer will not be 3D.
    unsigned short int bIsReal3d;

    // Final number of elements of the layer.
    MM_INTERNAL_FID nFinalElemCount;  // Real element count after conversion

    // Header of the layer
    size_t nHeaderDiskSize;
    struct MM_TH TopHeader;

    int eLT;          // Type of layer: Point, line or polygon (3D or not)
    int bIsBeenInit;  // 1 if layer has already been initialized

    // Point layer
    struct MiraMonPointLayer MMPoint;

    // Arc layer
    struct MiraMonArcLayer MMArc;

    // Polygon layer
    struct MiraMonPolygonLayer MMPolygon;

    // Offset used to write features.
    MM_FILE_OFFSET OffsetCheck;

    // EPSG code of the spatial reference system.
    char *pSRS;
    int nSRS_EPSG;  // Ref. system if has EPSG code.

// Used to write the precision of the reserved fields in the DBF
#define MM_SRS_LAYER_IS_UNKNOWN_TYPE 0
#define MM_SRS_LAYER_IS_PROJECTED_TYPE 1
#define MM_SRS_LAYER_IS_GEOGRAPHIC_TYPE 2
    int nSRSType;

    // In GDAL->MiraMon sense:
    // Transformed table from input layer to a MiraMon table.
    // This table has to be merged with private MiraMon fields to obtain
    // a MiraMon extended DBF
    struct MiraMonDataBase *pLayerDB;

    // In MiraMon->GDAL sense:
    // MiraMon extended DBF header
    // In GDAL->MiraMon, used when there is no geometry
    struct MM_DATA_BASE_XP *pMMBDXP;

    // In GDAL->MiraMon, used when there is no geometry
    struct MMAdmDatabase MMAdmDBWriting;

    // Offset of every FID in the table
    MM_BOOLEAN
    isListField;  // It determines if fields are list or simple (multirecord).
    MM_EXT_DBF_N_RECORDS
    nMaxN;  // Max number of elements in a field features list
    struct MM_ID_GRAFIC_MULTIPLE_RECORD *pMultRecordIndex;
// In case of multirecord, if user wants only one Record 'iMultiRecord'
// specifies which one: 0, 1, 2,... or "Last". There is also the "JSON" option
// that writes a serialized JSON array like (``[1,2]``).
#define MM_MULTIRECORD_LAST -1
#define MM_MULTIRECORD_NO_MULTIRECORD -2
#define MM_MULTIRECORD_JSON -3
    int iMultiRecord;

    // Charset of DBF files (same for all) when writing it.
    //  MM_JOC_CARAC_UTF8_DBF
    //  MM_JOC_CARAC_ANSI_DBASE;
    MM_BYTE nCharSet;

    // Language in REL files:
    // It is the language of the MiraMon generated descriptors.
    // Metadata will not be translated but these descriptors are
    // generated from scratch and it is good to use a custom language.
    char nMMLanguage;

    // This is used only to write temporary stuff
    char szNFieldAux[MM_MAX_AMPLADA_CAMP_N_DBF];
    // Dynamic string that is used as temporary buffer
    // with variable size as needed. Its value is
    // highly temporary. Copy in a safe place to save its value.
    GUInt64 nNumStringToOperate;
    char *szStringToOperate;

    // Temporary elements when reading features from MiraMon files
    struct MiraMonFeature ReadFeature;

    MM_SELEC_COORDZ_TYPE nSelectCoordz;  // MM_SELECT_FIRST_COORDZ
                                         // MM_SELECT_HIGHEST_COORDZ
                                         // MM_SELECT_LOWEST_COORDZ

    // For polygon layers this is an efficient space to read
    // the PAL section
    MM_POLYGON_ARCS_COUNT nMaxArcs;
    MM_POLYGON_ARCS_COUNT nNumArcs;
    struct MM_PAL_MEM *pArcs;

    struct MM_FLUSH_INFO FlushPAL;

    struct MiraMonVectMapInfo *MMMap;  // Do not free
};

enum DataType
{
    MMDTByte,
    MMDTInteger,
    MMDTuInteger,
    MMDTLong,
    MMDTReal,
    MMDTDouble,
    MMDT4bits
};

enum TreatmentVariable
{
    MMTVQuantitativeContinuous,
    MMTVOrdinal,
    MMTVCategorical
};

#ifdef GDAL_COMPILATION
CPL_C_END  // Necessary for compiling in GDAL project
#endif
#endif  //__MM_GDAL_DRIVER_STRUCTS_H
