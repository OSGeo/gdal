/**********************************************************************
 * $Id: geoconcept.h
 *
 * Name:     geoconcept.h
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements Physical Access class.
 * Language: C
 *
 **********************************************************************
 * Copyright (c) 2007,  Geoconcept and IGN
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
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 **********************************************************************/
#ifndef _GEOCONCEPT_H_INCLUDED
#define _GEOCONCEPT_H_INCLUDED

#include "cpl_port.h"
#include "cpl_list.h"
#include "cpl_vsi.h"
#include "ogr_api.h"

#include "geoconcept_syscoord.h"

/* From shapefil.h :                                                    */
/* -------------------------------------------------------------------- */
/*      GCIOAPI_CALL                                                    */
/*                                                                      */
/*      The following two macros are present to allow forcing           */
/*      various calling conventions on the Geoconcept API.              */
/*                                                                      */
/*      To force __stdcall conventions (needed to call GCIOLib          */
/*      from Visual Basic and/or Dephi I believe) the makefile could    */
/*      be modified to define:                                          */
/*                                                                      */
/*        /DGCIOAPI_CALL=__stdcall                                      */
/*                                                                      */
/*      If it is desired to force export of the GCIOLib API without     */
/*      using the geoconcept.def file, use the following definition.    */
/*                                                                      */
/*        /DGCIO_DLLEXPORT                                              */
/*                                                                      */
/*      To get both at once it will be necessary to hack this           */
/*      include file to define:                                         */
/*                                                                      */
/*        #define GCIOAPI_CALL __declspec(dllexport) __stdcall          */
/*        #define GCIOAPI_CALL1 __declspec(dllexport) * __stdcall       */
/*                                                                      */
/*      The complexity of the situation is partly caused by the         */
/*      peculiar requirement of Visual C++ that __stdcall appear        */
/*      after any "*"'s in the return value of a function while the     */
/*      __declspec(dllexport) must appear before them.                  */
/* -------------------------------------------------------------------- */

#ifdef GCIO_DLLEXPORT
#  define GCIOAPI_CALL __declspec(dllexport)
#  define GCIOAPI_CALL1(x)  __declspec(dllexport) x
#endif

#ifndef GCIOAPI_CALL
#  define GCIOAPI_CALL
#endif

#ifndef GCIOAPI_CALL1
#  define GCIOAPI_CALL1(x)      x GCIOAPI_CALL
#endif

/* -------------------------------------------------------------------- */
/*      Macros for controlling CVSID and ensuring they don't appear     */
/*      as unreferenced variables resulting in lots of warnings.        */
/* -------------------------------------------------------------------- */
#ifndef DISABLE_CVSID
#if defined(__GNUC__) && __GNUC__ >= 4
#  define GCIO_CVSID(string)     static char cpl_cvsid[] __attribute__((used)) = string;
#else
#  define GCIO_CVSID(string)     static char gcio_cvsid[] = string; \
static char *cvsid_aw() { return( cvsid_aw() ? ((char *) NULL) : gcio_cvsid ); }
#endif
#else
#  define GCIO_CVSID(string)
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define  kCacheSize_GCIO          65535
#define  kCharsetMAX_GCIO            15
#define  kUnitMAX_GCIO                7
#define  kTAB_GCIO                "\t"
#define  kCom_GCIO                "//"
#define  kHeader_GCIO             "//#"
#define  kPragma_GCIO             "//$"
#define  kCartesianPlanarRadix        2
#define  kGeographicPlanarRadix       9
#define  kElevationRadix              2

#define  kConfigBeginConfig_GCIO  "SECTION CONFIG"
#define  kConfigEndConfig_GCIO    "ENDSECTION CONFIG"
#define  kConfigBeginMap_GCIO     "SECTION MAP"
#define  kConfigEndMap_GCIO       "ENDSECTION MAP"
#define  kConfigBeginType_GCIO    "SECTION TYPE"
#define  kConfigBeginSubType_GCIO "SECTION SUBTYPE"
#define  kConfigBeginField_GCIO   "SECTION FIELD"
#define  kConfigEndField_GCIO     "ENDSECTION FIELD"
#define  kConfigEndSubType_GCIO   "ENDSECTION SUBTYPE"
#define  kConfigEndType_GCIO      "ENDSECTION TYPE"

#define  kConfigUnit_GCIO         "Unit"
#define  kConfigPrecision_GCIO    "Precision"
#define  kConfigName_GCIO         "Name"
#define  kConfigID_GCIO           "ID"
#define  kConfigKind_GCIO         "Kind"
#define  kConfig3D_GCIO           "3D"
#define  kConfigExtra_GCIO        "Extra"
#define  kConfigExtraText_GCIO    "ExtraText"
#define  kConfigList_GCIO         "List"
/* unused config keywords : */
#define  kConfigZUnit_GCIO        "ZUnit"
#define  kConfigZPrecision_GCIO   "ZPrecision"

#define  kMetadataVERSION_GCIO    "VERSION"
#define  kMetadataDELIMITER_GCIO  "DELIMITER"
#define  kMetadataQUOTEDTEXT_GCIO "QUOTED-TEXT"
#define  kMetadataCHARSET_GCIO    "CHARSET"
#define  kMetadataUNIT_GCIO       "UNIT"
#define  kMetadataFORMAT_GCIO     "FORMAT"
#define  kMetadataSYSCOORD_GCIO   "SYSCOORD"
#define  kMetadataFIELDS_GCIO     "FIELDS"
#define  kPrivate_GCIO            "Private#"
#define  kPublic_GCIO             ""
#define  k3DOBJECTMONO_GCIO       "3DOBJECTMONO"
#define  k3DOBJECT_GCIO           "3DOBJECT"
#define  k2DOBJECT_GCIO           "2DOBJECT"

#define  kIdentifier_GCIO         "@Identifier"
#define  kClass_GCIO              "@Class"
#define  kSubclass_GCIO           "@Subclass"
#define  kName_GCIO               "@Name"
#define  kNbFields_GCIO           "@NbFields"
#define  kX_GCIO                  "@X"
#define  kY_GCIO                  "@Y"
#define  kXP_GCIO                 "@XP"
#define  kYP_GCIO                 "@YP"
#define  kGraphics_GCIO           "@Graphics"
#define  kAngle_GCIO              "@Angle"

#define  WRITEERROR_GCIO          -1
#define  GEOMETRYEXPECTED_GCIO    -2
#define  WRITECOMPLETED_GCIO      -3

/* -------------------------------------------------------------------- */
/*      GCIO API Enumerations                                           */
/* -------------------------------------------------------------------- */

typedef enum _tCharset_GCIO {
  vUnknownCharset_GCIO = 0,
  vANSI_GCIO, /* Windows */
  vDOS_GCIO,
  vMAC_GCIO
} GCCharset;

typedef enum _tAccessMode_GCIO {
   vUnknownAccessMode_GCIO = 0,
   vNoAccess_GCIO,
   vReadAccess_GCIO,
   vUpdateAccess_GCIO,
   vWriteAccess_GCIO
} GCAccessMode;

typedef enum _tAccessStatus_GCIO {
   vNoStatus_GCIO,
   vMemoStatus_GCIO,
   vEof_GCIO
} GCAccessStatus;

typedef enum _t3D_GCIO {
   vUnknown3D_GCIO = 0,
   v2D_GCIO,
   v3D_GCIO,
   v3DM_GCIO
} GCDim;

typedef enum _tItemType_GCIO {
   vUnknownItemType_GCIO = 0,
   vPoint_GCIO,
   vLine_GCIO,
   vText_GCIO,
   vPoly_GCIO,
   vMemoFld_GCIO,
   vIntFld_GCIO,
   vRealFld_GCIO,
   vLengthFld_GCIO,
   vAreaFld_GCIO,
   vPositionFld_GCIO,
   vDateFld_GCIO,
   vTimeFld_GCIO,
   vChoiceFld_GCIO,
   vInterFld_GCIO
} GCTypeKind;

typedef enum _tIO_MetadataType_GCIO {
   vUnknownIO_ItemType_GCIO = 0,
   vComType_GCIO,
   vStdCol_GCIO,
   vEndCol_GCIO,
   vHeader_GCIO,
   vPragma_GCIO,
   vConfigBeginConfig_GCIO,
   vConfigEndConfig_GCIO,
   vConfigBeginMap_GCIO,
   vConfigEndMap_GCIO,
   vConfigBeginType_GCIO,
   vConfigBeginSubType_GCIO,
   vConfigBeginField_GCIO,
   vConfigEndField_GCIO,
   vConfigEndSubType_GCIO,
   vConfigEndType_GCIO,
   vMetadataDELIMITER_GCIO,
   vMetadataQUOTEDTEXT_GCIO,
   vMetadataCHARSET_GCIO,
   vMetadataUNIT_GCIO,
   vMetadataFORMAT_GCIO,
   vMetadataSYSCOORD_GCIO,
   vMetadataFIELDS_GCIO
} MetadataType;

typedef enum _boolean_GCIO {vFALSE=0,vTRUE} tBOOLEAN_GCIO ;

/* -------------------------------------------------------------------- */
/*      GCIO API Types                                                  */
/* -------------------------------------------------------------------- */
typedef struct _tDocFrame_GCIO GCExtent;
typedef struct _tField_GCIO GCField;
typedef struct _tSubType_GCIO GCSubType;
typedef struct _tType_GCIO GCType;
typedef struct _tExportHeader_GCIO GCExportFileMetadata;
typedef struct _GCExportFileH GCExportFileH;

struct _tDocFrame_GCIO {
  double XUL;
  double YUL;
  double XLR;
  double YLR;
};

struct _tField_GCIO {
  char*      name; /* @name => private */
  char*      extra;
  char**     enums;
  long       id;
  GCTypeKind knd;
};

struct _tSubType_GCIO {
  GCExportFileH*  _h;
  GCType*         _type;     /* parent's type */
  char*           name;
  CPLList*        fields;    /* GCField */
  GCExtent*       frame;
  OGRFeatureDefnH _poFeaDefn;
  long            id;
  long            _foff;     /* offset 1st feature */
  unsigned long   _flin;     /* 1st ligne 1st feature */
  unsigned long   _nFeatures;
  GCTypeKind      knd;
  GCDim           sys;
  int             _nbf;      /* number of user's fields */
  int             _hdrW;     /* pragma field written */
};

struct _tType_GCIO {
  char*          name;
  CPLList*       subtypes;    /* GCSubType */
  CPLList*       fields;      /* GCField */
  long           id;
};

struct _tExportHeader_GCIO {
  CPLList*             types;         /* GCType */
  CPLList*             fields;        /* GCField */
  OGRSpatialReferenceH srs;
  GCExtent*            frame;
  char*                version;
  char                 unit[kUnitMAX_GCIO+1];
  double               resolution;
  GCCharset            charset;
  int                  quotedtext;
  int                  format;
  GCSysCoord*          sysCoord;
  int                  pCS;
  int                  hCS;
  char                 delimiter;
};

struct _GCExportFileH {
  char                  cache[kCacheSize_GCIO+1];
  char*                 path;
  char*                 bn;
  char*                 ext;
  FILE*                 H;
  GCExportFileMetadata* header;
  long                  coff;
  unsigned long         clin;
  unsigned long         nbObjects;
  GCAccessMode          mode;
  GCAccessStatus        status;
  GCTypeKind            whatIs;
};

/* -------------------------------------------------------------------- */
/*      GCIO API Prototypes                                             */
/* -------------------------------------------------------------------- */

const char GCIOAPI_CALL1(*) GCCharset2str_GCIO ( GCCharset cs );
GCCharset GCIOAPI_CALL str2GCCharset_GCIO ( const char* s);
const char GCIOAPI_CALL1(*) GCAccessMode2str_GCIO ( GCAccessMode mode );
GCAccessMode str2GCAccessMode_GCIO ( const char* s);
const char GCIOAPI_CALL1(*) GCAccessStatus2str_GCIO ( GCAccessStatus stts );
GCAccessStatus str2GCAccessStatus_GCIO ( const char* s);
const char GCIOAPI_CALL1(*) GCDim2str_GCIO ( GCDim sys );
GCDim str2GCDim ( const char* s );
const char GCIOAPI_CALL1(*) GCTypeKind2str_GCIO ( GCTypeKind item );
GCTypeKind str2GCTypeKind_GCIO ( const char *s );

GCExportFileH GCIOAPI_CALL1(*) Open_GCIO ( const char* pszGeoconceptFile, const char* ext, const char* mode, const char* gctPath );
void GCIOAPI_CALL Close_GCIO ( GCExportFileH** hGCT );
GCExportFileH GCIOAPI_CALL1(*) Rewind_GCIO ( GCExportFileH* H, GCSubType* theSubType );
GCExportFileH GCIOAPI_CALL1(*) FFlush_GCIO ( GCExportFileH* H );

GCType GCIOAPI_CALL1(*) AddType_GCIO (  GCExportFileH* H, const char* typName, long id );
GCSubType GCIOAPI_CALL1(*) AddSubType_GCIO ( GCExportFileH* H, const char* typName, const char* subtypName, long id, GCTypeKind knd, GCDim sys );
GCField GCIOAPI_CALL1(*) AddTypeField_GCIO ( GCExportFileH* H, const char* typName, int where, const char* name, long id, GCTypeKind knd, const char* extra, const char* enums );
GCField GCIOAPI_CALL1(*) AddSubTypeField_GCIO ( GCExportFileH* H, const char* typName, const char* subtypName, int where, const char* name, long id, GCTypeKind knd, const char* extra, const char* enums );
GCExportFileMetadata GCIOAPI_CALL1(*) ReadConfig_GCIO ( GCExportFileH* H );
GCExportFileH GCIOAPI_CALL1(*) WriteHeader_GCIO ( GCExportFileH* H );
GCExportFileMetadata GCIOAPI_CALL1(*) CreateHeader_GCIO ( );
void GCIOAPI_CALL DestroyHeader_GCIO ( GCExportFileMetadata** m );
GCExtent GCIOAPI_CALL1(*) CreateExtent_GCIO ( double Xmin, double Ymin, double Xmax, double Ymax );
void GCIOAPI_CALL DestroyExtent_GCIO ( GCExtent** theExtent );
GCExportFileMetadata GCIOAPI_CALL1(*) ReadHeader_GCIO ( GCExportFileH* H );
GCSubType GCIOAPI_CALL1(*) FindFeature_GCIO ( GCExportFileH* H, const char* typDOTsubtypName );
int GCIOAPI_CALL FindFeatureFieldIndex_GCIO ( GCSubType* theSubType, const char *fieldName );
GCField GCIOAPI_CALL1(*) FindFeatureField_GCIO ( GCSubType* theSubType, const char *fieldName );
int GCIOAPI_CALL StartWritingFeature_GCIO ( GCSubType* theSubType, long id );
int GCIOAPI_CALL WriteFeatureFieldAsString_GCIO ( GCSubType* theSubType, int iField, const char* theValue );
int GCIOAPI_CALL WriteFeatureGeometry_GCIO ( GCSubType* theSubType, OGRGeometryH poGeom );
void GCIOAPI_CALL StopWritingFeature_GCIO ( GCSubType* theSubType );
OGRFeatureH GCIOAPI_CALL ReadNextFeature_GCIO ( GCSubType* theSubType );

#define GetGCCache_GCIO(gc) (gc)->cache
#define SetGCCache_GCIO(gc,v) strncpy((gc)->cache, (v), kCacheSize_GCIO), (gc)->cache[kCacheSize_GCIO]= '\0';
#define GetGCPath_GCIO(gc) (gc)->path
#define SetGCPath_GCIO(gc,v) (gc)->path= (v)
#define GetGCBasename_GCIO(gc) (gc)->bn
#define SetGCBasename_GCIO(gc,v) (gc)->bn= (v)
#define GetGCExtension_GCIO(gc) (gc)->ext
#define SetGCExtension_GCIO(gc,v) (gc)->ext= (v)
#define GetGCHandle_GCIO(gc) (gc)->H
#define SetGCHandle_GCIO(gc,v) (gc)->H= (v)
#define GetGCMeta_GCIO(gc) (gc)->header
#define SetGCMeta_GCIO(gc,v) (gc)->header= (v)
#define GetGCCurrentOffset_GCIO(gc) (gc)->coff
#define SetGCCurrentOffset_GCIO(gc,v) (gc)->coff= (v)
#define GetGCCurrentLinenum_GCIO(gc) (gc)->clin
#define SetGCCurrentLinenum_GCIO(gc,v) (gc)->clin= (v)
#define GetGCNbObjects_GCIO(gc) (gc)->nbObjects
#define SetGCNbObjects_GCIO(gc,v) (gc)->nbObjects= (v)
#define GetGCMode_GCIO(gc) (gc)->mode
#define SetGCMode_GCIO(gc,v) (gc)->mode= (v)
#define GetGCStatus_GCIO(gc) (gc)->status
#define SetGCStatus_GCIO(gc,v) (gc)->status= (v)
#define GetGCWhatIs_GCIO(gc) (gc)->whatIs
#define SetGCWhatIs_GCIO(gc,v) (gc)->whatIs= (v)

#define GetMetaTypes_GCIO(header) (header)->types
#define SetMetaTypes_GCIO(header,v) (header)->types= (v)
#define CountMetaTypes_GCIO(header) CPLListCount((header)->types)
#define GetMetaType_GCIO(header,rank) (GCType*)CPLListGetData(CPLListGet((header)->types,(rank)))
#define GetMetaFields_GCIO(header) (header)->fields
#define SetMetaFields_GCIO(header,v) (header)->fields= (v)
#define GetMetaCharset_GCIO(header) (header)->charset
#define SetMetaCharset_GCIO(header,v) (header)->charset= (v)
#define GetMetaDelimiter_GCIO(header) (header)->delimiter
#define SetMetaDelimiter_GCIO(header,v) (header)->delimiter= (v)
#define GetMetaUnit_GCIO(header) (header)->unit
#define SetMetaUnit_GCIO(header,v) strncpy((header)->unit, (v), kUnitMAX_GCIO), (header)->unit[kUnitMAX_GCIO]= '\0';
#define GetMetaZUnit_GCIO(header) (header)->zUnit
#define SetMetaZUnit_GCIO(header,v) strncpy((header)->zUnit, (v), kUnitMAX_GCIO), (header)->zUnit[kUnitMAX_GCIO]= '\0';
#define GetMetaResolution_GCIO(header) (header)->resolution
#define SetMetaResolution_GCIO(header,v) (header)->resolution= (v)
#define GetMetaZResolution_GCIO(header) (header)->zResolution
#define SetMetaZResolution_GCIO(header,v) (header)->zResolution= (v)
#define GetMetaQuotedText_GCIO(header) (header)->quotedtext
#define SetMetaQuotedText_GCIO(header,v) (header)->quotedtext= (v)
#define GetMetaFormat_GCIO(header) (header)->format
#define SetMetaFormat_GCIO(header,v) (header)->format= (v)
#define GetMetaSysCoord_GCIO(header) (header)->sysCoord
#define SetMetaSysCoord_GCIO(header,v) (header)->sysCoord= (v)
#define GetMetaPlanarFormat_GCIO(header) (header)->pCS
#define SetMetaPlanarFormat_GCIO(header, v) (header)->pCS= (v)
#define GetMetaHeightFormat_GCIO(header) (header)->hCS
#define SetMetaHeightFormat_GCIO(header, v) (header)->hCS= (v)

#define GetMetaExtent_GCIO(header) (header)->frame
#define SetMetaExtent_GCIO(header,v) (header)->frame= (v)
#define GetMetaSRS_GCIO(header) (header)->srs
#define SetMetaSRS_GCIO(header,v) (header)->srs= (v)
#define GetMetaVersion_GCIO(header) (header)->version
#define SetMetaVersion_GCIO(header,v) (header)->version= (v)

#define GetTypeName_GCIO(theClass) (theClass)->name
#define SetTypeName_GCIO(theClass,v) (theClass)->name= (v)
#define GetTypeID_GCIO(theClass) (theClass)->id
#define SetTypeID_GCIO(theClass,v) (theClass)->id= (v)
#define GetTypeSubtypes_GCIO(theClass) (theClass)->subtypes
#define SetTypeSubtypes_GCIO(theClass,v) (theClass)->subtypes= (v)
#define GetTypeFields_GCIO(theClass) (theClass)->fields
#define SetTypeFields_GCIO(theClass,v) (theClass)->fields= (v)
#define CountTypeSubtypes_GCIO(theClass) CPLListCount((theClass)->subtypes)
#define GetTypeSubtype_GCIO(theClass,rank) (GCSubType*)CPLListGetData(CPLListGet((theClass)->subtypes,(rank)))
#define CountTypeFields_GCIO(theClass) CPLListCount((theClass)->fields)
#define GetTypeField_GCIO(theClass,rank) (GCField*)CPLListGetData(CPLListGet((theClass)->fields,(rank)))

#define GetSubTypeType_GCIO(theSubType) (theSubType)->_type
#define SetSubTypeType_GCIO(theSubType,v) (theSubType)->_type= (v)
#define GetSubTypeName_GCIO(theSubType) (theSubType)->name
#define SetSubTypeName_GCIO(theSubType,v) (theSubType)->name= (v)
#define GetSubTypeID_GCIO(theSubType) (theSubType)->id
#define SetSubTypeID_GCIO(theSubType,v) (theSubType)->id= (v)
#define GetSubTypeKind_GCIO(theSubType) (theSubType)->knd
#define SetSubTypeKind_GCIO(theSubType,v) (theSubType)->knd= (v)
#define GetSubTypeDim_GCIO(theSubType) (theSubType)->sys
#define SetSubTypeDim_GCIO(theSubType,v) (theSubType)->sys= (v)
#define GetSubTypeFields_GCIO(theSubType) (theSubType)->fields
#define SetSubTypeFields_GCIO(theSubType,v) (theSubType)->fields= (v)
#define CountSubTypeFields_GCIO(theSubType) CPLListCount((theSubType)->fields)
#define GetSubTypeField_GCIO(theSubType,rank) (GCField*)CPLListGetData(CPLListGet((theSubType)->fields,(rank)))
#define GetSubTypeNbFields_GCIO(theSubType) (theSubType)->_nbf
#define SetSubTypeNbFields_GCIO(theSubType,v) (theSubType)->_nbf= (v)
#define GetSubTypeNbFeatures_GCIO(theSubType) (theSubType)->_nFeatures
#define SetSubTypeNbFeatures_GCIO(theSubType,v) (theSubType)->_nFeatures= (v)
#define GetSubTypeBOF_GCIO(theSubType) (theSubType)->_foff
#define SetSubTypeBOF_GCIO(theSubType,v) (theSubType)->_foff= (v)
#define GetSubTypeBOFLinenum_GCIO(theSubType) (theSubType)->_flin
#define SetSubTypeBOFLinenum_GCIO(theSubType,v) (theSubType)->_flin= (v)
#define GetSubTypeExtent_GCIO(theSubType) (theSubType)->frame
#define SetSubTypeExtent_GCIO(theSubType,v) (theSubType)->frame= (v)
#define GetSubTypeGCHandle_GCIO(theSubType) (theSubType)->_h
#define SetSubTypeGCHandle_GCIO(theSubType,v) (theSubType)->_h= (v)
#define GetSubTypeFeatureDefn_GCIO(theSubType) (theSubType)->_poFeaDefn
#define SetSubTypeFeatureDefn_GCIO(theSubType,v) (theSubType)->_poFeaDefn= (v)
#define IsSubTypeHeaderWritten_GCIO(theSubType) (theSubType)->_hdrW
#define SetSubTypeHeaderWritten_GCIO(theSubType,v) (theSubType)->_hdrW= (v)

#define GetFieldName_GCIO(theField) (theField)->name
#define SetFieldName_GCIO(theField,v) (theField)->name= (v)
#define GetFieldID_GCIO(theField) (theField)->id
#define SetFieldID_GCIO(theField,v) (theField)->id= (v)
#define GetFieldKind_GCIO(theField) (theField)->knd
#define SetFieldKind_GCIO(theField,v) (theField)->knd= (v)
#define GetFieldExtra_GCIO(theField) (theField)->extra
#define SetFieldExtra_GCIO(theField,v) (theField)->extra= (v)
#define GetFieldList_GCIO(theField) (theField)->enums
#define SetFieldList_GCIO(theField,v) (theField)->enums= (v)
#define IsPrivateField_GCIO(theField) (*(GetFieldName_GCIO(theField))=='@')

#define GetExtentULAbscissa_GCIO(theExtent) (theExtent)->XUL
#define GetExtentULOrdinate_GCIO(theExtent) (theExtent)->YUL
#define SetExtentULAbscissa_GCIO(theExtent,v) (theExtent)->XUL= (v) < (theExtent)->XUL? (v): (theExtent)->XUL
#define SetExtentULOrdinate_GCIO(theExtent,v) (theExtent)->YUL= (v) > (theExtent)->YUL? (v): (theExtent)->YUL
#define GetExtentLRAbscissa_GCIO(theExtent) (theExtent)->XLR
#define GetExtentLROrdinate_GCIO(theExtent) (theExtent)->YLR
#define SetExtentLRAbscissa_GCIO(theExtent,v) (theExtent)->XLR= (v) > (theExtent)->XLR? (v): (theExtent)->XLR
#define SetExtentLROrdinate_GCIO(theExtent,v) (theExtent)->YLR= (v) < (theExtent)->YLR? (v): (theExtent)->YLR

/* OGREnvelope C API : */
#define InitOGREnvelope_GCIO(poEvlp) \
  if( poEvlp!=NULL ) \
  {\
    (poEvlp)->MinX= (poEvlp)->MinY= HUGE_VAL;\
    (poEvlp)->MaxX= (poEvlp)->MaxY= -HUGE_VAL;\
  }

#define MergeOGREnvelope_GCIO(poEvlp,x,y) \
  if( poEvlp!=NULL ) \
  {\
    if( (x) < (poEvlp)->MinX ) (poEvlp)->MinX= (x);\
    if( (x) > (poEvlp)->MaxX ) (poEvlp)->MaxX= (x);\
    if( (y) < (poEvlp)->MinY ) (poEvlp)->MinY= (y);\
    if( (y) > (poEvlp)->MaxY ) (poEvlp)->MaxY= (y);\
  }

#ifdef __cplusplus
}
#endif


#endif /* ndef _GEOCONCEPT_H_INCLUDED */
