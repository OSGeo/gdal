/**********************************************************************
 * $Id: geoconcept.c
 *
 * Name:     geoconcept.c
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

#include <math.h>
#include "geoconcept.h"
#include "cpl_conv.h"
#include "cpl_string.h"
#include "ogr_core.h"

GCIO_CVSID("$Id: geoconcept.c,v 1.0.0 2007-11-03 20:58:19 drichard Exp $")

#define kItemSize_GCIO      256
#define kExtraSize_GCIO    4096
#define kIdSize_GCIO         12
#define UNDEFINEDID_GCIO 199901L

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

static char* gkGCAccess[]=
{
/* 0 */ "",
/* 1 */ "NO",
/* 2 */ "READ",
/* 3 */ "UPDATE",
/* 4 */ "WRITE"
};

static char* gkGCStatus[]=
{
/* 0 */ "NONE",
/* 1 */ "MEMO",
/* 2 */ "EOF"
};

static char* gk3D[]=
{
/* 0 */ "",
/* 1 */ "2D",
/* 2 */ "3DM",
/* 3 */ "3D"
};

static char* gkGCTypeKind[]=
{
/* 0 */ "",
/* 1 */ "POINT",
/* 2 */ "LINE",
/* 3 */ "TEXT",
/* 4 */ "POLYGON",
/* 5 */ "MEMO",
/* 6 */ "INT",
/* 7 */ "REAL",
/* 8 */ "LENGTH",
/* 9 */ "AREA",
/*10 */ "POSITION",
/*11 */ "DATE",
/*12 */ "TIME",
/*13 */ "CHOICE",
/*14 */ "MEMO"
};

/* -------------------------------------------------------------------- */
/*      GCIO API Prototypes                                             */
/* -------------------------------------------------------------------- */

/* -------------------------------------------------------------------- */
static char GCIOAPI_CALL1(*) _getHeaderValue_GCIO ( const char *s )
{
  char *b, *e;

  if( (b= strchr(s,'='))==NULL ) return NULL;
  b++;
  while (isspace(*b)) b++;
  e= b;
  while (*e!='\0' && !isspace(*e)) e++;
  *e= '\0';
  return b;
}/* _getHeaderValue_GCIO */

/* -------------------------------------------------------------------- */
const char GCIOAPI_CALL1(*) GCAccessMode2str_GCIO ( GCAccessMode mode )
{
  switch (mode) {
  case vNoAccess_GCIO          :
  case vReadAccess_GCIO        :
  case vUpdateAccess_GCIO      :
  case vWriteAccess_GCIO       :
    return gkGCAccess[mode];
  default                      :
    return gkGCAccess[vUnknownAccessMode_GCIO];
  }
}/* GCAccessMode2str_GCIO */

/* -------------------------------------------------------------------- */
GCAccessMode GCIOAPI_CALL str2GCAccessMode_GCIO ( const char* s)
{
  if (strcmp(s,gkGCAccess[vNoAccess_GCIO])==0) return vNoAccess_GCIO;
  if (strcmp(s,gkGCAccess[vReadAccess_GCIO])==0) return vReadAccess_GCIO;
  if (strcmp(s,gkGCAccess[vUpdateAccess_GCIO])==0) return vUpdateAccess_GCIO;
  if (strcmp(s,gkGCAccess[vWriteAccess_GCIO])==0) return vWriteAccess_GCIO;
  return vUnknownAccessMode_GCIO;
}/* str2GCAccessMode_GCIO */

/* -------------------------------------------------------------------- */
const char GCIOAPI_CALL1(*) GCAccessStatus2str_GCIO ( GCAccessStatus stts )
{
  switch (stts) {
  case vMemoStatus_GCIO :
  case vEof_GCIO        :
    return gkGCStatus[stts];
  default               :
    return gkGCStatus[vNoStatus_GCIO];
  }
}/* GCAccessStatus2str_GCIO */

/* -------------------------------------------------------------------- */
GCAccessStatus GCIOAPI_CALL str2GCAccessStatus_GCIO ( const char* s)
{
  if (strcmp(s,gkGCStatus[vMemoStatus_GCIO])==0) return vMemoStatus_GCIO;
  if (strcmp(s,gkGCStatus[vEof_GCIO])==0) return vEof_GCIO;
  return vNoStatus_GCIO;
}/* str2GCAccessStatus_GCIO */

/* -------------------------------------------------------------------- */
const char GCIOAPI_CALL1(*) GCDim2str_GCIO ( GCDim sys )
{
  switch (sys) {
  case v2D_GCIO        :
  case v3D_GCIO        :
  case v3DM_GCIO       :
    return gk3D[sys];
  default              :
    return gk3D[vUnknown3D_GCIO];
  }
}/* GCDim2str_GCIO */

/* -------------------------------------------------------------------- */
GCDim GCIOAPI_CALL str2GCDim ( const char* s )
{
  if (strcmp(s,gk3D[v2D_GCIO])==0) return v2D_GCIO;
  if (strcmp(s,gk3D[v3D_GCIO])==0) return v3D_GCIO;
  if (strcmp(s,gk3D[v3DM_GCIO])==0) return v3DM_GCIO;
  return vUnknown3D_GCIO;
}/* str2GCDim */

/* -------------------------------------------------------------------- */
const char GCIOAPI_CALL1(*) GCTypeKind2str_GCIO ( GCTypeKind item )
{
  switch (item) {
  case vPoint_GCIO           :
  case vLine_GCIO            :
  case vText_GCIO            :
  case vPoly_GCIO            :
  case vMemoFld_GCIO         :
  case vIntFld_GCIO          :
  case vRealFld_GCIO         :
  case vLengthFld_GCIO       :
  case vAreaFld_GCIO         :
  case vPositionFld_GCIO     :
  case vDateFld_GCIO         :
  case vTimeFld_GCIO         :
  case vChoiceFld_GCIO       :
  case vInterFld_GCIO        :
    return gkGCTypeKind[item];
  default                    :
    return gkGCTypeKind[vUnknownItemType_GCIO];
  }
}/* GCTypeKind2str_GCIO */

/* -------------------------------------------------------------------- */
GCTypeKind GCIOAPI_CALL str2GCTypeKind_GCIO ( const char *s )
{
  if (strcmp(s,gkGCTypeKind[vPoint_GCIO])==0) return vPoint_GCIO;
  if (strcmp(s,gkGCTypeKind[vLine_GCIO])==0) return vLine_GCIO;
  if (strcmp(s,gkGCTypeKind[vText_GCIO])==0) return vText_GCIO;
  if (strcmp(s,gkGCTypeKind[vPoly_GCIO])==0) return vPoly_GCIO;
  if (strcmp(s,gkGCTypeKind[vMemoFld_GCIO])==0) return vMemoFld_GCIO;
  if (strcmp(s,gkGCTypeKind[vIntFld_GCIO])==0) return vIntFld_GCIO;
  if (strcmp(s,gkGCTypeKind[vRealFld_GCIO])==0) return vRealFld_GCIO;
  if (strcmp(s,gkGCTypeKind[vLengthFld_GCIO])==0) return vLengthFld_GCIO;
  if (strcmp(s,gkGCTypeKind[vAreaFld_GCIO])==0) return vAreaFld_GCIO;
  if (strcmp(s,gkGCTypeKind[vPositionFld_GCIO])==0) return vPositionFld_GCIO;
  if (strcmp(s,gkGCTypeKind[vDateFld_GCIO])==0) return vDateFld_GCIO;
  if (strcmp(s,gkGCTypeKind[vTimeFld_GCIO])==0) return vTimeFld_GCIO;
  if (strcmp(s,gkGCTypeKind[vChoiceFld_GCIO])==0) return vChoiceFld_GCIO;
  if (strcmp(s,gkGCTypeKind[vInterFld_GCIO])==0) return vInterFld_GCIO;
  return vUnknownItemType_GCIO;
}/* str2GCTypeKind_GCIO */

/* -------------------------------------------------------------------- */
static long GCIOAPI_CALL _read_GCIO (
                                      GCExportFileH* hGXT,
                                      const char* sep
                                    )
{
  FILE* h;
  long nread;
  int c;
  char *result;

  h= GetGCHandle_GCIO(hGXT);
  nread= 0L;
  result= GetGCCache_GCIO(hGXT);
  while ((c= VSIFGetc(h))!=EOF)
  {
    c= (0x00FF & (unsigned char)(c));
    switch (c) {
    case 0X1A : continue ; /* caractere de fin de fichier PC */
    case '\r' :            /* PC '\r\n' line */
      if ((c= VSIFGetc(h))!='\n')
      {
        VSIUngetc(c,h);
      }
      c= '\n';
    case '\n' :
      SetGCCurrentLinenum_GCIO(hGXT,GetGCCurrentLinenum_GCIO(hGXT)+1L);
      if (nread==0L) continue;
      *result= '\0';
      SetGCCurrentOffset_GCIO(hGXT, VSIFTell(h));
      return nread;
    default   :
      if (sep && strchr(sep,c)!=NULL)
      {
        if (nread>0L)
        {
          VSIUngetc(c,h);
          *result= '\0';
          SetGCCurrentOffset_GCIO(hGXT, VSIFTell(h));
          return nread;
        }
        *result= (char)c;
        result++;
        *result= '\0';
        SetGCCurrentOffset_GCIO(hGXT, VSIFTell(h));
        return 1L;
      }
      *result= (char)c;
      result++;
      nread++;
    }/* switch */
  }/* while */
  *result= '\0';
  SetGCCurrentOffset_GCIO(hGXT, VSIFTell(h));
  if (c==EOF)
  {
    SetGCStatus_GCIO(hGXT, vEof_GCIO);
    if (nread==0L)
    {
      return EOF;
    }
  }

  return nread;
}/* _read_GCIO */

/* -------------------------------------------------------------------- */
static long GCIOAPI_CALL _get_GCIO (
                                     GCExportFileH* hGXT
                                   )
{
  if (GetGCStatus_GCIO(hGXT)==vEof_GCIO)
  {
    SetGCCache_GCIO(hGXT,"");
    SetGCWhatIs_GCIO(hGXT, vUnknownIO_ItemType_GCIO);
    return EOF;
  }
  if (GetGCStatus_GCIO(hGXT)==vMemoStatus_GCIO)
  {
    SetGCStatus_GCIO(hGXT, vNoStatus_GCIO);
    return GetGCCurrentOffset_GCIO(hGXT);
  }
  if (_read_GCIO(hGXT,NULL)==EOF)
  {
    SetGCWhatIs_GCIO(hGXT, vUnknownIO_ItemType_GCIO);
    return EOF;
  }
  SetGCWhatIs_GCIO(hGXT, vStdCol_GCIO);
  if (strstr(GetGCCache_GCIO(hGXT),kCom_GCIO)==GetGCCache_GCIO(hGXT))
  { /* // */
    SetGCWhatIs_GCIO(hGXT, vComType_GCIO);
    if (strstr(GetGCCache_GCIO(hGXT),kHeader_GCIO)==GetGCCache_GCIO(hGXT))
    { /* //# */
      SetGCWhatIs_GCIO(hGXT, vHeader_GCIO);
    }
    else
    {
      if (strstr(GetGCCache_GCIO(hGXT),kPragma_GCIO)==GetGCCache_GCIO(hGXT))
      { /* //$ */
        SetGCWhatIs_GCIO(hGXT, vPragma_GCIO);
      }
    }
  }
  return GetGCCurrentOffset_GCIO(hGXT);
}/* _get_GCIO */

/* -------------------------------------------------------------------- */
static void GCIOAPI_CALL _InitExtent_GCIO (
                                            GCExtent* theExtent
                                          )
{
  theExtent->XUL=  HUGE_VAL;
  theExtent->YUL= -HUGE_VAL;
  theExtent->XLR= -HUGE_VAL;
  theExtent->YLR=  HUGE_VAL;
}/* _InitExtent_GCIO */

/* -------------------------------------------------------------------- */
static GCExtent GCIOAPI_CALL1(*) _CreateExtent_GCIO (
                                                      double Xmin,
                                                      double Ymin,
                                                      double Xmax,
                                                      double Ymax
                                                    )
{
  GCExtent* theExtent;

  if( !(theExtent= CPLMalloc(sizeof(GCExtent))) )
  {
    CPLError( CE_Failure, CPLE_OutOfMemory,
              "failed to create a Geoconcept extent for '[%g %g,%g %g]'.\n",
              Xmin, Ymin,Xmax, Ymax);
    return NULL;
  }
  _InitExtent_GCIO(theExtent);
  theExtent->XUL= Xmin;
  theExtent->YUL= Ymax;
  theExtent->XLR= Xmax;
  theExtent->YLR= Ymin;

  return theExtent;
}/* _CreateExtent_GCIO */

/* -------------------------------------------------------------------- */
static void GCIOAPI_CALL _ReInitExtent_GCIO (
                                              GCExtent* theExtent
                                            )
{
  _InitExtent_GCIO(theExtent);
}/* _ReInitExtent_GCIO */

/* -------------------------------------------------------------------- */
static void GCIOAPI_CALL _DestroyExtent_GCIO (
                                               GCExtent** theExtent
                                             )
{
  _ReInitExtent_GCIO(*theExtent);
  CPLFree(*theExtent);
  *theExtent= NULL;
}/* _DestroyExtent_GCIO */

/* -------------------------------------------------------------------- */
static void GCIOAPI_CALL _InitField_GCIO (
                                           GCField* theField
                                         )
{
  SetFieldName_GCIO(theField, NULL);
  SetFieldID_GCIO(theField, UNDEFINEDID_GCIO);
  SetFieldKind_GCIO(theField, vUnknownItemType_GCIO);
  SetFieldExtra_GCIO(theField, NULL);
  SetFieldList_GCIO(theField, NULL);
}/* _InitField_GCIO */

/* -------------------------------------------------------------------- */
static const char GCIOAPI_CALL1(*) _NormalizeFieldName_GCIO (
                                                              const char* name
                                                            )
{
  if( name[0]=='@' )
  {
    if( EQUAL(name, "@Identificateur") || EQUAL(name, kIdentifier_GCIO) )
    {
      return kIdentifier_GCIO;
    }
    else if( EQUAL(name, "@Type") || EQUAL(name, kClass_GCIO) )
    {
      return kClass_GCIO;
    }
    else if( EQUAL(name, "@Sous-type") || EQUAL(name, kSubclass_GCIO) )
    {
      return kSubclass_GCIO;
    }
    else if( EQUAL(name, "@Nom") || EQUAL(name, kName_GCIO) )
    {
      return kName_GCIO;
    }
    else if( EQUAL(name, kNbFields_GCIO) )
    {
      return kNbFields_GCIO;
    }
    else if( EQUAL(name, kX_GCIO) )
    {
      return kX_GCIO;
    }
    else if( EQUAL(name, kY_GCIO) )
    {
      return kY_GCIO;
    }
    else if( EQUAL(name, "@X'") || EQUAL(name, kXP_GCIO) )
    {
      return kXP_GCIO;
    }
    else if( EQUAL(name, "@Y'") || EQUAL(name, kYP_GCIO) )
    {
      return kYP_GCIO;
    }
    else if( EQUAL(name, kGraphics_GCIO) )
    {
      return kGraphics_GCIO;
    }
    else if( EQUAL(name, kAngle_GCIO) )
    {
      return kAngle_GCIO;
    }
    else
    {
      return name;
    }
  }
  else
  {
    return name;
  }
}/* _NormalizeFieldName_GCIO */

/* -------------------------------------------------------------------- */
static GCField GCIOAPI_CALL1(*) _CreateField_GCIO (
                                                    const char* name,
                                                    long        id,
                                                    GCTypeKind  knd,
                                                    const char* extra,
                                                    const char* enums
                                                  )
{
  GCField* theField;

  if( !(theField= CPLMalloc(sizeof(GCField))) )
  {
    CPLError( CE_Failure, CPLE_OutOfMemory,
              "failed to create a Geoconcept field for '%s'.\n",
              name);
    return NULL;
  }
  _InitField_GCIO(theField);
  SetFieldName_GCIO(theField, CPLStrdup(name));
  SetFieldID_GCIO(theField, id);
  SetFieldKind_GCIO(theField, knd);
  if( extra && extra[0]!='\0' ) SetFieldExtra_GCIO(theField, CPLStrdup(extra));
  if( enums && enums[0]!='\0' ) SetFieldList_GCIO(theField, CSLTokenizeString2(enums,";",0));

  return theField;
}/* _CreateField_GCIO */

/* -------------------------------------------------------------------- */
static void GCIOAPI_CALL _ReInitField_GCIO (
                                             GCField* theField
                                           )
{
  if( GetFieldName_GCIO(theField) )
  {
    CPLFree(GetFieldName_GCIO(theField));
  }
  if( GetFieldExtra_GCIO(theField) )
  {
    CPLFree( GetFieldExtra_GCIO(theField) );
  }
  if( GetFieldList_GCIO(theField) )
  {
    CSLDestroy( GetFieldList_GCIO(theField) );
  }
  _InitField_GCIO(theField);
}/* _ReInitField_GCIO */

/* -------------------------------------------------------------------- */
static void GCIOAPI_CALL _DestroyField_GCIO (
                                              GCField** theField
                                            )
{
  _ReInitField_GCIO(*theField);
  CPLFree(*theField);
  *theField= NULL;
}/* _DestroyField_GCIO */

/* -------------------------------------------------------------------- */
static int GCIOAPI_CALL _findFieldByName_GCIO (
                                                CPLList* fields,
                                                const char* name
                                              )
{
  GCField* theField;

  if( fields )
  {
    CPLList* e;
    int n, i;
    if( (n= CPLListCount(fields))>0 )
    {
      for( i= 0; i<n; i++)
      {
        if( (e= CPLListGet(fields,i)) )
        {
          if( (theField= (GCField*)CPLListGetData(e)) )
          {
            if( EQUAL(GetFieldName_GCIO(theField),name) )
            {
              return i;
            }
          }
        }
      }
    }
  }
  return -1;
}/* _findFieldByName_GCIO */

/* -------------------------------------------------------------------- */
static GCField GCIOAPI_CALL1(*) _getField_GCIO (
                                                 CPLList* fields,
                                                 int where
                                               )
{
  CPLList* e;

  if( (e= CPLListGet(fields,where)) )
    return (GCField*)CPLListGetData(e);
  return NULL;
}/* _getField_GCIO */

/* -------------------------------------------------------------------- */
static void GCIOAPI_CALL _InitSubType_GCIO (
                                             GCSubType* theSubType
                                           )
{
  SetSubTypeType_GCIO(theSubType, NULL);
  SetSubTypeName_GCIO(theSubType, NULL);
  SetSubTypeFields_GCIO(theSubType, NULL);     /* GCField */
  SetSubTypeKind_GCIO(theSubType, vUnknownItemType_GCIO);
  SetSubTypeID_GCIO(theSubType, UNDEFINEDID_GCIO);
  SetSubTypeDim_GCIO(theSubType, v2D_GCIO);
  SetSubTypeNbFields_GCIO(theSubType, -1);
}/* _InitSubType_GCIO */

/* -------------------------------------------------------------------- */
static GCSubType GCIOAPI_CALL1(*) _CreateSubType_GCIO (
                                                        const char* subtypName,
                                                        long id,
                                                        GCTypeKind knd,
                                                        GCDim sys
                                                      )
{
  GCSubType* theSubType;

  if( !(theSubType= CPLMalloc(sizeof(GCSubType))) )
  {
    CPLError( CE_Failure, CPLE_OutOfMemory,
              "failed to create a Geoconcept subtype for '%s'.\n",
              subtypName);
    return NULL;
  }
  _InitSubType_GCIO(theSubType);
  SetSubTypeName_GCIO(theSubType, CPLStrdup(subtypName));
  SetSubTypeID_GCIO(theSubType, id);
  SetSubTypeKind_GCIO(theSubType, knd);
  SetSubTypeDim_GCIO(theSubType, sys);

  return theSubType;
}/* _CreateSubType_GCIO */

/* -------------------------------------------------------------------- */
static void GCIOAPI_CALL _ReInitSubType_GCIO (
                                               GCSubType* theSubType
                                             )
{
  if( GetSubTypeFields_GCIO(theSubType) )
  {
    CPLList* e;
    GCField* theField;
    int i, n;
    if( (n= CPLListCount(GetSubTypeFields_GCIO(theSubType)))>0 )
    {
      for (i= 0; i<n; i++)
      {
        if( (e= CPLListGet(GetSubTypeFields_GCIO(theSubType),i)) )
        {
          if( (theField= (GCField*)CPLListGetData(e)) )
          {
            _DestroyField_GCIO(&theField);
          }
        }
      }
    }
    CPLListDestroy(GetSubTypeFields_GCIO(theSubType));
  }
  if( GetSubTypeName_GCIO(theSubType) )
  {
    CPLFree( GetSubTypeName_GCIO(theSubType) );
  }
  _InitSubType_GCIO(theSubType);
}/* _ReInitSubType_GCIO */

/* -------------------------------------------------------------------- */
static void GCIOAPI_CALL _DestroySubType_GCIO (
                                                GCSubType** theSubType
                                              )
{
  _ReInitSubType_GCIO(*theSubType);
  CPLFree(*theSubType);
  *theSubType= NULL;
}/* _DestroySubType_GCIO */

/* -------------------------------------------------------------------- */
static int GCIOAPI_CALL _findSubTypeByName_GCIO (
                                                  GCType* theClass,
                                                  const char* subtypName
                                                )
{
  GCSubType* theSubType;

  if( GetTypeSubtypes_GCIO(theClass) )
  {
    CPLList* e;
    int n, i;
    if( (n= CPLListCount(GetTypeSubtypes_GCIO(theClass)))>0 )
    {
      for( i = 0; i < n; i++)
      {
        if( (e= CPLListGet(GetTypeSubtypes_GCIO(theClass),i)) )
        {
          if( (theSubType= (GCSubType*)CPLListGetData(e)) )
          {
            if( EQUAL(GetSubTypeName_GCIO(theSubType),subtypName) )
            {
              return i;
            }
          }
        }
      }
    }
  }
  return -1;
}/* _findSubTypeByName_GCIO */

/* -------------------------------------------------------------------- */
static GCSubType GCIOAPI_CALL1(*) _getSubType_GCIO (
                                                     GCType* theClass,
                                                     int where
                                                   )
{
  CPLList* e;

  if( (e= CPLListGet(GetTypeSubtypes_GCIO(theClass),where)) )
    return (GCSubType*)CPLListGetData(e);
  return NULL;
}/* _getSubType_GCIO */

/* -------------------------------------------------------------------- */
static void GCIOAPI_CALL _InitType_GCIO (
                                          GCType* theClass
                                        )
{
  SetTypeName_GCIO(theClass, NULL);
  SetTypeSubtypes_GCIO(theClass, NULL);/* GCSubType */
  SetTypeFields_GCIO(theClass, NULL); /* GCField */
  SetTypeID_GCIO(theClass, UNDEFINEDID_GCIO);
}/* _InitType_GCIO */

/* -------------------------------------------------------------------- */
static GCType GCIOAPI_CALL1(*) _CreateType_GCIO (
                                                  const char* typName,
                                                  long id
                                                )
{
  GCType* theClass;

  if( !(theClass= CPLMalloc(sizeof(GCType))) )
  {
    CPLError( CE_Failure, CPLE_OutOfMemory,
              "failed to create a Geoconcept type for '%s#%ld'.\n",
              typName, id);
    return NULL;
  }
  _InitType_GCIO(theClass);
  SetTypeName_GCIO(theClass, CPLStrdup(typName));
  SetTypeID_GCIO(theClass, id);

  return theClass;
}/* _CreateType_GCIO */

/* -------------------------------------------------------------------- */
static void GCIOAPI_CALL _ReInitType_GCIO (
                                            GCType* theClass
                                          )
{
  if( GetTypeSubtypes_GCIO(theClass) )
  {
    CPLList* e;
    GCSubType* theSubType;
    int i, n;
    if( (n= CPLListCount(GetTypeSubtypes_GCIO(theClass)))>0 )
    {
      for (i= 0; i<n; i++)
      {
        if( (e= CPLListGet(GetTypeSubtypes_GCIO(theClass),i)) )
        {
          if( (theSubType= (GCSubType*)CPLListGetData(e)) )
          {
            _DestroySubType_GCIO(&theSubType);
          }
        }
      }
    }
    CPLListDestroy(GetTypeSubtypes_GCIO(theClass));
  }
  if( GetTypeFields_GCIO(theClass) )
  {
    CPLList* e;
    GCField* theField;
    int i, n;
    if( (n= CPLListCount(GetTypeFields_GCIO(theClass)))>0 )
    {
      for (i= 0; i<n; i++)
      {
        if( (e= CPLListGet(GetTypeFields_GCIO(theClass),i)) )
        {
          if( (theField= (GCField*)CPLListGetData(e)) )
          {
            _DestroyField_GCIO(&theField);
          }
        }
      }
    }
    CPLListDestroy(GetTypeFields_GCIO(theClass));
  }
  if( GetTypeName_GCIO(theClass) )
  {
    CPLFree( GetTypeName_GCIO(theClass) );
  }
  _InitType_GCIO(theClass);
}/* _ReInitType_GCIO */

/* -------------------------------------------------------------------- */
static void GCIOAPI_CALL _DestroyType_GCIO (
                                             GCType** theClass
                                           )
{
  _ReInitType_GCIO(*theClass);
  CPLFree(*theClass);
  *theClass= NULL;
}/* _DestroyType_GCIO */

/* -------------------------------------------------------------------- */
static int GCIOAPI_CALL _findTypeByName_GCIO (
                                               GCExportFileH* hGXT,
                                               const char* typName
                                             )
{
  GCType* theClass;
  GCExportFileMetadata* header;

  header= GetGCMeta_GCIO(hGXT);
  if( GetMetaTypes_GCIO(header) )
  {
    CPLList* e;
    int n, i;
    if( (n= CPLListCount(GetMetaTypes_GCIO(header)))>0 )
    {
      for( i = 0; i < n; i++)
      {
        if( (e= CPLListGet(GetMetaTypes_GCIO(header),i)) )
        {
          if( (theClass= (GCType*)CPLListGetData(e)) )
          {
            if( EQUAL(GetTypeName_GCIO(theClass),typName) )
            {
              return i;
            }
          }
        }
      }
    }
  }
  return -1;
}/* _findTypeByName_GCIO */

/* -------------------------------------------------------------------- */
static GCType GCIOAPI_CALL1(*) _getType_GCIO (
                                               GCExportFileH* hGXT,
                                               int where
                                             )
{
  CPLList* e;

  if( (e= CPLListGet(GetMetaTypes_GCIO(GetGCMeta_GCIO(hGXT)),where)) )
    return (GCType*)CPLListGetData(e);
  return NULL;
}/* _getType_GCIO */

/* -------------------------------------------------------------------- */
static void GCIOAPI_CALL _InitHeader_GCIO (
                                            GCExportFileMetadata* header
                                          )
{
  SetMetaDelimiter_GCIO(header, kTAB_GCIO[0]);
  SetMetaQuotedText_GCIO(header, FALSE);
  SetMetaCharset_GCIO(header, "ANSI");/* FIXME */
  SetMetaUnit_GCIO(header, "m");
  SetMetaZUnit_GCIO(header,"m");
  SetMetaFormat_GCIO(header, 2);
  SetMetaSysCoord_GCIO(header, -1);
  SetMetaTypes_GCIO(header, NULL); /* GCType */
  SetMetaFields_GCIO(header, NULL); /* GCField */
  SetMetaResolution_GCIO(header, 0.1);
  SetMetaZResolution_GCIO(header, -1.0);
  SetMetaExtent_GCIO(header, NULL);
}/* _InitHeader_GCIO */

/* -------------------------------------------------------------------- */
static GCExportFileMetadata GCIOAPI_CALL1(*) _CreateHeader_GCIO ( )
{
  GCExportFileMetadata* m;

  if( !(m= CPLMalloc(sizeof(GCExportFileMetadata)) ) )
  {
    CPLError( CE_Failure, CPLE_OutOfMemory,
              "failed to create Geoconcept metadata.\n");
    return NULL;
  }
  _InitHeader_GCIO(m);

  return m;
}/* _CreateHeader_GCIO */

/* -------------------------------------------------------------------- */
static void GCIOAPI_CALL _ReInitHeader_GCIO (
                                              GCExportFileMetadata* header
                                            )
{
  if( GetMetaExtent_GCIO(header) )
  {
    _DestroyExtent_GCIO(&(GetMetaExtent_GCIO(header)));
  }
  if( GetMetaTypes_GCIO(header) )
  {
    CPLList* e;
    GCType* theClass;
    int i, n;
    if( (n= CPLListCount(GetMetaTypes_GCIO(header)))>0 )
    {
      for (i= 0; i<n; i++)
      {
        if( (e= CPLListGet(GetMetaTypes_GCIO(header),i)) )
        {
          if( (theClass= (GCType*)CPLListGetData(e)) )
          {
            _DestroyType_GCIO(&theClass);
          }
        }
      }
    }
    CPLListDestroy(GetMetaTypes_GCIO(header));
  }
  if( GetMetaFields_GCIO(header) )
  {
    CPLList* e;
    GCField* theField;
    int i, n;
    if( (n= CPLListCount(GetMetaFields_GCIO(header)))>0 )
    {
      for (i= 0; i<n; i++)
      {
        if( (e= CPLListGet(GetMetaFields_GCIO(header),i)) )
        {
          if( (theField= (GCField*)CPLListGetData(e)) )
          {
            _DestroyField_GCIO(&theField);
          }
        }
      }
    }
    CPLListDestroy(GetMetaFields_GCIO(header));
  }

  _InitHeader_GCIO(header);
}/* _ReInitHeader_GCIO */

/* -------------------------------------------------------------------- */
static void GCIOAPI_CALL _DestroyHeader_GCIO (
                                               GCExportFileMetadata** m
                                             )
{
  _ReInitHeader_GCIO(*m);
  CPLFree(*m);
  *m= NULL;
}/* _DestroyHeader_GCIO */

/* -------------------------------------------------------------------- */
static void GCIOAPI_CALL _Init_GCIO (
                                      GCExportFileH* H
                                    )
{
  SetGCCache_GCIO(H,"");
  SetGCPath_GCIO(H, NULL);
  SetGCBasename_GCIO(H, NULL);
  SetGCExtension_GCIO(H, NULL);
  SetGCHandle_GCIO(H, NULL);
  SetGCCurrentOffset_GCIO(H, 0L);
  SetGCCurrentLinenum_GCIO(H, 0L);
  SetGCNbObjects_GCIO(H, 0L);
  SetGCMeta_GCIO(H, NULL);
  SetGCMode_GCIO(H, vNoAccess_GCIO);
  SetGCStatus_GCIO(H, vNoStatus_GCIO);
  SetGCWhatIs_GCIO(H, vUnknownIO_ItemType_GCIO);
}/* _Init_GCIO */

/* -------------------------------------------------------------------- */
static GCExportFileH GCIOAPI_CALL1(*) _Create_GCIO (
                                                     const char* pszGeoconceptFile,
                                                     const char *ext,
                                                     const char* mode
                                                   )
{
  GCExportFileH* hGXT;

  /* Not yet implemented : */
  if( mode[0]=='r')
  {
    CPLError( CE_Failure, CPLE_NotSupported,
              "opening a Geoconcept file '%s' for reading is not yet supported.\n",
              pszGeoconceptFile);
    return NULL;
  }

  if( !(hGXT= CPLMalloc(sizeof(GCExportFileH)) ) )
  {
    CPLError( CE_Failure, CPLE_OutOfMemory,
              "failed to create a Geoconcept handle for '%s' (%s).\n",
              pszGeoconceptFile, mode);
    return NULL;
  }

  _Init_GCIO(hGXT);
  SetGCPath_GCIO(hGXT, CPLStrdup(CPLGetDirname(pszGeoconceptFile)));
  SetGCBasename_GCIO(hGXT, CPLStrdup(CPLGetBasename(pszGeoconceptFile)));
  SetGCExtension_GCIO(hGXT, CPLStrdup(ext? ext:"gxt"));
  SetGCMode_GCIO(hGXT, (mode[0]=='w'? vWriteAccess_GCIO : (mode[0]=='a'? vUpdateAccess_GCIO:vReadAccess_GCIO)));

  return hGXT;
}/* _Create_GCIO */

/* -------------------------------------------------------------------- */
static void GCIOAPI_CALL _ReInit_GCIO (
                                        GCExportFileH* hGXT
                                      )
{
  if( GetGCMeta_GCIO(hGXT) )
  {
    _DestroyHeader_GCIO(&(GetGCMeta_GCIO(hGXT)));
  }
  if( GetGCHandle_GCIO(hGXT) )
  {
    VSIFClose(GetGCHandle_GCIO(hGXT));
  }
  if( GetGCExtension_GCIO(hGXT) )
  {
    CPLFree(GetGCExtension_GCIO(hGXT));
  }
  if( GetGCBasename_GCIO(hGXT) )
  {
    CPLFree(GetGCBasename_GCIO(hGXT));
  }
  if( GetGCPath_GCIO(hGXT) )
  {
    CPLFree(GetGCPath_GCIO(hGXT));
  }
  SetGCCache_GCIO(hGXT,"");
  _Init_GCIO(hGXT);
}/* _ReInit_GCIO */

/* -------------------------------------------------------------------- */
static void GCIOAPI_CALL _Destroy_GCIO (
                                         GCExportFileH** hGXT
                                       )
{
  _ReInit_GCIO(*hGXT);
  CPLFree(*hGXT);
  *hGXT= NULL;
}/* _Destroy_GCIO */

/* -------------------------------------------------------------------- */
static int _checkSchema_GCIO (
                               GCExportFileH* hGXT
                             )
{
  GCExportFileMetadata* Meta;
  int nT, iT, nS, iS, nF, iF, nU, iId, iCl, iSu, iNa, iNb, iX, iY, iXP, iYP, iGr, iAn;
  GCField* theField;
  GCSubType* theSubType;
  GCType* theClass;
  CPLList* e;

  Meta= GetGCMeta_GCIO(hGXT);
  if( (nT= CPLListCount(GetMetaTypes_GCIO(Meta)))==0 )
  {
    CPLError( CE_Failure, CPLE_AppDefined,
              "Geoconcept schema without types!\n" );
    return FALSE;
  }
  for (iT= 0; iT<nT; iT++)
  {
    if( (e= CPLListGet(GetMetaTypes_GCIO(Meta),iT)) )
    {
      if( (theClass= (GCType*)CPLListGetData(e)) )
      {
        if( (nS= CPLListCount(GetTypeSubtypes_GCIO(theClass)))==0 )
        {
          CPLError( CE_Failure, CPLE_AppDefined,
                    "Geoconcept type %s without sub-types!\n",
                    GetTypeName_GCIO(theClass) );
          return FALSE;
        }
        for (iS= 0; iS<nS; iS++)
        {
          if( (e= CPLListGet(GetTypeSubtypes_GCIO(theClass),iS)) )
          {
            if( (theSubType= (GCSubType*)CPLListGetData(e)) )
            {
              if( (nF= CPLListCount(GetSubTypeFields_GCIO(theSubType)))==0 )
              {
                CPLError( CE_Failure, CPLE_AppDefined,
                          "Geoconcept sub-type %s.%s without fields!\n",
                          GetTypeName_GCIO(theClass), GetSubTypeName_GCIO(theSubType) );
                return FALSE;
              }
              nU= 0;
              iId= iCl= iSu= iNa= iNb= iX= iY= iXP= iYP= iGr= iAn= -1;
              for (iF= 0; iF<nF; iF++)
              {
                if( (e= CPLListGet(GetSubTypeFields_GCIO(theSubType),iF)) )
                {
                  if( (theField= (GCField*)CPLListGetData(e)) )
                  {
                    if( IsPrivateField_GCIO(theField) )
                    {
                      if( EQUAL(GetFieldName_GCIO(theField),kIdentifier_GCIO) )
                        iId= iF;
                      else if( EQUAL(GetFieldName_GCIO(theField),kClass_GCIO) )
                        iCl= iF;
                      else if( EQUAL(GetFieldName_GCIO(theField),kSubclass_GCIO) )
                        iSu= iF;
                      else if( EQUAL(GetFieldName_GCIO(theField),kName_GCIO) )
                        iNa= iF;
                      else if( EQUAL(GetFieldName_GCIO(theField),kNbFields_GCIO) )
                        iNb= iF;
                      else if( EQUAL(GetFieldName_GCIO(theField),kX_GCIO) )
                        iX= iF;
                      else if( EQUAL(GetFieldName_GCIO(theField),kY_GCIO) )
                        iY= iF;
                      else if( EQUAL(GetFieldName_GCIO(theField),kXP_GCIO) )
                        iXP= iF;
                      else if( EQUAL(GetFieldName_GCIO(theField),kYP_GCIO) )
                        iYP= iF;
                      else if( EQUAL(GetFieldName_GCIO(theField),kGraphics_GCIO) )
                        iGr= iF;
                      else if( EQUAL(GetFieldName_GCIO(theField),kAngle_GCIO) )
                        iAn= iF;
                    }
                    else
                    {
                      nU++;
                    }
                  }
                }
              }
              if( iId==-1 )
              {
                CPLError( CE_Failure, CPLE_AppDefined,
                          "Geoconcept mandatory field %s is missing on %s.%s!\n",
                          kIdentifier_GCIO, GetTypeName_GCIO(theClass), GetSubTypeName_GCIO(theSubType) );
                return FALSE;
              }
              else if( iId!=0 )
              {
                CPLError( CE_Failure, CPLE_AppDefined,
                          "Geoconcept mandatory field %s must be the first field of %s.%s!\n",
                          kIdentifier_GCIO, GetTypeName_GCIO(theClass), GetSubTypeName_GCIO(theSubType) );
                return FALSE;
              }
              if( iCl==-1 )
              {
                CPLError( CE_Failure, CPLE_AppDefined,
                          "Geoconcept mandatory field %s is missing on %s.%s!\n",
                          kClass_GCIO, GetTypeName_GCIO(theClass), GetSubTypeName_GCIO(theSubType) );
                return FALSE;
              }
              else if( iCl-iId!=1 )
              {
                CPLError( CE_Failure, CPLE_AppDefined,
                          "Geoconcept mandatory field %s must be the second field of %s.%s!\n",
                          kClass_GCIO, GetTypeName_GCIO(theClass), GetSubTypeName_GCIO(theSubType) );
                return FALSE;
              }
              if( iSu==-1 )
              {
                CPLError( CE_Failure, CPLE_AppDefined,
                          "Geoconcept mandatory field %s is missing on %s.%s!\n",
                          kSubclass_GCIO, GetTypeName_GCIO(theClass), GetSubTypeName_GCIO(theSubType) );
                return FALSE;
              }
              else if( iSu-iCl!=1 )
              {
                CPLError( CE_Failure, CPLE_AppDefined,
                          "Geoconcept mandatory field %s must be the third field of %s.%s!\n",
                          kSubclass_GCIO, GetTypeName_GCIO(theClass), GetSubTypeName_GCIO(theSubType) );
                return FALSE;
              }
              if( iNb==-1 )
              {
                CPLError( CE_Failure, CPLE_AppDefined,
                          "Geoconcept mandatory field %s is missing on %s.%s!\n",
                          kNbFields_GCIO, GetTypeName_GCIO(theClass), GetSubTypeName_GCIO(theSubType) );
                return FALSE;
              }
              if( iX==-1 )
              {
                CPLError( CE_Failure, CPLE_AppDefined,
                          "Geoconcept mandatory field %s is missing on %s.%s!\n",
                          kX_GCIO, GetTypeName_GCIO(theClass), GetSubTypeName_GCIO(theSubType) );
                return FALSE;
              }
              if( iY==-1 )
              {
                CPLError( CE_Failure, CPLE_AppDefined,
                          "Geoconcept mandatory field %s is missing on %s.%s!\n",
                          kY_GCIO, GetTypeName_GCIO(theClass), GetSubTypeName_GCIO(theSubType) );
                return FALSE;
              }
              if( iY-iX!=1 )
              {
                CPLError( CE_Failure, CPLE_AppDefined,
                          "Geoconcept geometry fields %s, %s must be consecutive for %s.%s!\n",
                          kX_GCIO, kY_GCIO, GetTypeName_GCIO(theClass), GetSubTypeName_GCIO(theSubType) );
                return FALSE;
              }
              if( GetSubTypeKind_GCIO(theSubType)==vLine_GCIO )
              {
                if( iXP==-1 )
                {
                  CPLError( CE_Failure, CPLE_AppDefined,
                            "Geoconcept mandatory field %s is missing on %s.%s!\n",
                            kXP_GCIO, GetTypeName_GCIO(theClass), GetSubTypeName_GCIO(theSubType) );
                  return FALSE;
                }
                if( iYP==-1 )
                {
                  CPLError( CE_Failure, CPLE_AppDefined,
                            "Geoconcept mandatory field %s is missing on %s.%s!\n",
                            kYP_GCIO, GetTypeName_GCIO(theClass), GetSubTypeName_GCIO(theSubType) );
                  return FALSE;
                }
                if( iYP-iXP!=1 )
                {
                  CPLError( CE_Failure, CPLE_AppDefined,
                            "Geoconcept geometry fields %s, %s must be consecutive for %s.%s!\n",
                            kXP_GCIO, kYP_GCIO, GetTypeName_GCIO(theClass), GetSubTypeName_GCIO(theSubType) );
                  return FALSE;
                }
                if( iXP-iY!=1 )
                {
                  CPLError( CE_Failure, CPLE_AppDefined,
                            "Geoconcept geometry fields %s, %s, %s, %s must be consecutive for %s.%s!\n",
                            kX_GCIO, kY_GCIO, kXP_GCIO, kYP_GCIO,
                            GetTypeName_GCIO(theClass), GetSubTypeName_GCIO(theSubType) );
                  return FALSE;
                }
              }
              else
              {
                if( iXP!=-1 )
                {
                  CPLError( CE_Failure, CPLE_AppDefined,
                            "Geoconcept sub-type %s.%s has a mandatory field %s only required on linear type!\n",
                            GetTypeName_GCIO(theClass), GetSubTypeName_GCIO(theSubType), kXP_GCIO );
                  return FALSE;
                }
                if( iYP!=-1 )
                {
                  CPLError( CE_Failure, CPLE_AppDefined,
                            "Geoconcept sub-type %s.%s has a mandatory field %s only required on linear type!\n",
                            GetTypeName_GCIO(theClass), GetSubTypeName_GCIO(theSubType), kYP_GCIO );
                  return FALSE;
                }
              }
              if( GetSubTypeKind_GCIO(theSubType)==vLine_GCIO ||
                  GetSubTypeKind_GCIO(theSubType)==vPoly_GCIO )
              {
                if( iGr==-1 )
                {
                    CPLError( CE_Failure, CPLE_AppDefined,
                              "Geoconcept mandatory field %s is missing on %s.%s!\n",
                              kGraphics_GCIO, GetTypeName_GCIO(theClass), GetSubTypeName_GCIO(theSubType) );
                    return FALSE;
                }
                else
                {
                  if( (iYP!=-1 && iGr-iYP!=1) || (iGr-iY!=1) )
                  {
                    CPLError( CE_Failure, CPLE_AppDefined,
                              "Geoconcept geometry fields %s, %s must be consecutive for %s.%s!\n",
                              iYP!=-1? kYP_GCIO:kY_GCIO, kGraphics_GCIO, GetTypeName_GCIO(theClass), GetSubTypeName_GCIO(theSubType) );
                    return FALSE;
                  }
                }
                if( iAn!=-1 )
                {
                  CPLError( CE_Failure, CPLE_AppDefined,
                            "Geoconcept sub-type %s.%s has a field %s only required on ponctual or text type!\n",
                            GetTypeName_GCIO(theClass), GetSubTypeName_GCIO(theSubType), kAngle_GCIO );
                  return FALSE;
                }
              }
              else
              {
                if( iGr!=-1 )
                {
                  CPLError( CE_Failure, CPLE_AppDefined,
                            "Geoconcept sub-type %s.%s has a mandatory field %s only required on linear or polygonal type!\n",
                            GetTypeName_GCIO(theClass), GetSubTypeName_GCIO(theSubType), kGraphics_GCIO );
                  return FALSE;
                }
              }
              SetSubTypeNbFields_GCIO(theSubType,nU);
            }
          }
        }
      }
    }
  }

  return TRUE;
}/* _checkSchema_GCIO */

/* -------------------------------------------------------------------- */
GCExportFileH GCIOAPI_CALL1(*) Open_GCIO (
                                           const char* pszGeoconceptFile,
                                           const char* ext,    /* gxt if NULL */
                                           const char* mode,
                                           const char* gctPath /* if !NULL, mode=w */
                                         )
{
  GCExportFileH* hGXT;

  if( !(hGXT= _Create_GCIO(pszGeoconceptFile,ext,mode)) )
  {
    return NULL;
  }

  SetGCHandle_GCIO(hGXT, VSIFOpen(CPLFormFilename(GetGCPath_GCIO(hGXT),GetGCBasename_GCIO(hGXT),GetGCExtension_GCIO(hGXT)), mode));
  if( !GetGCHandle_GCIO(hGXT) )
  {
    _Destroy_GCIO(&hGXT);
    return NULL;
  }

  if ( gctPath!=NULL && GetGCMode_GCIO(hGXT)==vWriteAccess_GCIO) {
    /* load Metadata */
    GCExportFileH* hGCT;

    hGCT= _Create_GCIO(gctPath,"gct","-");
    SetGCHandle_GCIO(hGCT, VSIFOpen(CPLFormFilename(GetGCPath_GCIO(hGCT),GetGCBasename_GCIO(hGCT),GetGCExtension_GCIO(hGCT)), "r"));
    if( !GetGCHandle_GCIO(hGCT) )
    {
      CPLError( CE_Failure, CPLE_NotSupported,
                "opening a Geoconcept config file '%s' failed.\n",
                gctPath);
      _Destroy_GCIO(&hGCT);
      _Destroy_GCIO(&hGXT);
      return NULL;
    }
    if( ReadConfig_GCIO(hGCT)==NULL )
    {
      _Destroy_GCIO(&hGCT);
      _Destroy_GCIO(&hGXT);
      return NULL;
    }
    SetGCMeta_GCIO(hGXT, GetGCMeta_GCIO(hGCT));
    SetGCMeta_GCIO(hGCT, NULL);
    _Destroy_GCIO(&hGCT);
    SetMetaExtent_GCIO(GetGCMeta_GCIO(hGXT), _CreateExtent_GCIO(HUGE_VAL,HUGE_VAL,-HUGE_VAL,-HUGE_VAL));
  }
  else
  {
    /* read basic Metadata from export */
    if( ReadHeader_GCIO(hGXT)==NULL )
    {
      _Destroy_GCIO(&hGXT);
      return NULL;
    }
  }
  /* check schema */
  if( !_checkSchema_GCIO(hGXT) )
  {
    _Destroy_GCIO(&hGXT);
    return NULL;
  }

  CPLDebug( "GEOCONCEPT",
            "Export =(\n"
            "  Path : %s\n"
            "  Basename : %s\n"
            "  Extension : %s\n"
            "  Mode : %s\n"
            "  Status : %s\n"
            ")\n",
            GetGCPath_GCIO(hGXT),
            GetGCBasename_GCIO(hGXT),
            GetGCExtension_GCIO(hGXT),
            GCAccessMode2str_GCIO(GetGCMode_GCIO(hGXT)),
            GCAccessStatus2str_GCIO(GetGCStatus_GCIO(hGXT))
            );

  return hGXT;
}/* Open_GCIO */

/* -------------------------------------------------------------------- */
void GCIOAPI_CALL Close_GCIO (
                               GCExportFileH** hGXT
                             )
{
  _Destroy_GCIO(hGXT);
}/* Close_GCIO */

/* -------------------------------------------------------------------- */
GCExportFileH GCIOAPI_CALL1(*) Rewind_GCIO (
                                             GCExportFileH* hGXT
                                           )
{
  if( hGXT )
  {
    if( GetGCHandle_GCIO(hGXT) )
    {
      VSIRewind(GetGCHandle_GCIO(hGXT));
    }
  }
  return hGXT;
}/* Rewind_GCIO */

/* -------------------------------------------------------------------- */
GCExportFileH GCIOAPI_CALL1(*) FFlush_GCIO (
                                             GCExportFileH* hGXT
                                           )
{
  if( hGXT )
  {
    if( GetGCHandle_GCIO(hGXT) )
    {
      VSIFFlush(GetGCHandle_GCIO(hGXT));
    }
  }
  return hGXT;
}/* FFlush_GCIO */

/* -------------------------------------------------------------------- */
GCAccessMode GCIOAPI_CALL GetMode_GCIO (
                                         GCExportFileH* hGXT
                                       )
{
  return hGXT? GetGCMode_GCIO(hGXT):vUnknownAccessMode_GCIO;
}/* GetMode_GCIO */

/* -------------------------------------------------------------------- */
GCSubType GCIOAPI_CALL1(*) AddSubType_GCIO (
                                             GCExportFileH* H,
                                             const char* typName,
                                             const char* subtypName,
                                             long id,
                                             GCTypeKind knd,
                                             GCDim sys
                                           )
{
  int whereClass;
  GCType* theClass;
  GCSubType* theSubType;
  CPLList* L;

  if( (whereClass = _findTypeByName_GCIO(H,typName))==-1 )
  {
    CPLError( CE_Failure, CPLE_OutOfMemory,
              "failed to find a Geoconcept type for '%s.%s#%ld'.\n",
              typName, subtypName, id);
    return NULL;
  }

  theClass= _getType_GCIO(H,whereClass);
  if( GetTypeSubtypes_GCIO(theClass) )
  {
    if( _findSubTypeByName_GCIO(theClass,subtypName)!=-1 )
    {
      CPLError( CE_Failure, CPLE_OutOfMemory,
                "Geoconcept subtype '%s.%s#%ld' already exists.\n",
                typName, subtypName, id);
      return NULL;
    }
  }

  if( !(theSubType= _CreateSubType_GCIO(subtypName,id,knd,sys)) )
  {
    return NULL;
  }
  if( (L= CPLListAppend(GetTypeSubtypes_GCIO(theClass),theSubType))==NULL )
  {
    _DestroySubType_GCIO(&theSubType);
    CPLError( CE_Failure, CPLE_OutOfMemory,
              "failed to add a Geoconcept subtype for '%s.%s#%ld'.\n",
              typName, subtypName, id);
    return NULL;
  }
  SetTypeSubtypes_GCIO(theClass, L);
  SetSubTypeType_GCIO(theSubType, theClass);

  CPLDebug("GEOCONCEPT", "SubType '%s.%s#%ld' added.\n", typName, subtypName, id);

  return theSubType;
}/* AddSubType_GCIO */

/* -------------------------------------------------------------------- */
void GCIOAPI_CALL DropSubType_GCIO (
                                     GCExportFileH* H,
                                     GCType* theClass,
                                     GCSubType** theSubType
                                   )
{
  int where;

  if( !theClass ) return;
  if( !theSubType || !(*theSubType) ) return;
  if( (where= _findSubTypeByName_GCIO(theClass,GetSubTypeName_GCIO(*theSubType)))==-1 )
  {
    CPLError( CE_Failure, CPLE_AppDefined,
              "subtype %s does not exist.\n",
              GetSubTypeName_GCIO(*theSubType)? GetSubTypeName_GCIO(*theSubType):"''");
    return;
  }
  CPLListRemove(GetTypeSubtypes_GCIO(theClass),where);
  _DestroySubType_GCIO(theSubType);

  return;
}/* DropSubType_GCIO */

/* -------------------------------------------------------------------- */
GCType GCIOAPI_CALL1(*) AddType_GCIO (
                                       GCExportFileH* H,
                                       const char* typName,
                                       long id
                                     )
{
  GCType* theClass;
  CPLList* L;

  if( _findTypeByName_GCIO(H,typName)!=-1 )
  {
    CPLError( CE_Failure, CPLE_AppDefined,
              "type %s already exists.\n",
              typName);
    return NULL;
  }

  if( !(theClass= _CreateType_GCIO(typName,id)) )
  {
    return NULL;
  }
  if( (L= CPLListAppend(GetMetaTypes_GCIO(GetGCMeta_GCIO(H)),theClass))==NULL )
  {
    _DestroyType_GCIO(&theClass);
    CPLError( CE_Failure, CPLE_OutOfMemory,
              "failed to add a Geoconcept type for '%s#%ld'.\n",
              typName, id);
    return NULL;
  }
  SetMetaTypes_GCIO(GetGCMeta_GCIO(H), L);

  CPLDebug("GEOCONCEPT", "Type '%s#%ld' added.\n", typName, id);

  return theClass;
}/* AddType_GCIO */

/* -------------------------------------------------------------------- */
void GCIOAPI_CALL DropType_GCIO (
                                  GCExportFileH* H,
                                  GCType **theClass
                                )
{
  int where;

  if( !theClass || !(*theClass) ) return;
  if( (where= _findTypeByName_GCIO(H,GetTypeName_GCIO(*theClass)))==-1 )
  {
    CPLError( CE_Failure, CPLE_AppDefined,
              "type %s does not exist.\n",
              GetTypeName_GCIO(*theClass)? GetTypeName_GCIO(*theClass):"''");
    return;
  }
  CPLListRemove(GetMetaTypes_GCIO(GetGCMeta_GCIO(H)),where);
  _DestroyType_GCIO(theClass);

  return;
}/* DropType_GCIO */

/* -------------------------------------------------------------------- */
GCField GCIOAPI_CALL1(*) AddTypeField_GCIO (
                                             GCExportFileH* H,
                                             const char* typName,
                                             int where, /* -1 : in the end */
                                             const char* name,
                                             long id,
                                             GCTypeKind knd,
                                             const char* extra,
                                             const char* enums
                                           )
{
  int whereClass;
  GCType* theClass;
  GCField* theField;
  CPLList* L;
  const char* normName;

  if( (whereClass = _findTypeByName_GCIO(H,typName))==-1 )
  {
    CPLError( CE_Failure, CPLE_AppDefined,
              "failed to find a Geoconcept type for '%s@%s#%ld'.\n",
              typName, name, id);
    return NULL;
  }
  theClass= _getType_GCIO(H,whereClass);

  normName= _NormalizeFieldName_GCIO(name);
  if( _findFieldByName_GCIO(GetTypeFields_GCIO(theClass),normName)!=-1 )
  {
    CPLError( CE_Failure, CPLE_AppDefined,
              "field '%s@%s#%ld' already exists.\n",
              typName, name, id);
    return NULL;
  }

  if( !(theField= _CreateField_GCIO(normName,id,knd,extra,enums)) )
  {
    return NULL;
  }
  if (
       where==-1 ||
       (where==0 && CPLListCount(GetTypeFields_GCIO(theClass))==0)
     )
  {
    L= CPLListAppend(GetTypeFields_GCIO(theClass),theField);
  }
  else
  {
    L= CPLListInsert(GetTypeFields_GCIO(theClass),theField,where);
  }
  if ( !L )
  {
    _DestroyField_GCIO(&theField);
    CPLError( CE_Failure, CPLE_OutOfMemory,
              "failed to add a Geoconcept field for '%s@%s#%ld'.\n",
              typName, name, id);
    return NULL;
  }
  SetTypeFields_GCIO(theClass, L);

  CPLDebug("GEOCONCEPT", "Field '%s@%s#%ld' added.\n", typName, name, id);

  return theField;
}/* AddTypeField_GCIO */

/* -------------------------------------------------------------------- */
GCField GCIOAPI_CALL1(*) AddSubTypeField_GCIO (
                                                GCExportFileH* H,
                                                const char* typName,
                                                const char* subtypName,
                                                int where, /* -1 : in the end */
                                                const char* name,
                                                long id,
                                                GCTypeKind knd,
                                                const char* extra,
                                                const char* enums
                                              )
{
  int whereClass, whereSubType;
  GCType* theClass;
  GCSubType* theSubType;
  GCField* theField;
  CPLList* L;
  const char* normName;

  if( (whereClass= _findTypeByName_GCIO(H,typName))==-1 )
  {
    CPLError( CE_Failure, CPLE_AppDefined,
              "failed to find a Geoconcept type for '%s.%s@%s#%ld'.\n",
              typName, subtypName, name, id);
    return NULL;
  }
  theClass= _getType_GCIO(H,whereClass);

  if( (whereSubType= _findSubTypeByName_GCIO(theClass,subtypName))==-1 )
  {
    CPLError( CE_Failure, CPLE_AppDefined,
              "failed to find a Geoconcept subtype for '%s.%s@%s#%ld'.\n",
              typName, subtypName, name, id);
    return NULL;
  }
  theSubType= _getSubType_GCIO(theClass,whereSubType);

  normName= _NormalizeFieldName_GCIO(name);
  if( _findFieldByName_GCIO(GetSubTypeFields_GCIO(theSubType),normName)!=-1 )
  {
    CPLError( CE_Failure, CPLE_AppDefined,
              "field '%s.%s@%s#%ld' already exists.\n",
              typName, subtypName, name, id);
    return NULL;
  }

  if( !(theField= _CreateField_GCIO(normName,id,knd,extra,enums)) )
  {
    return NULL;
  }
  if(
      where==-1 ||
      (where==0 && CPLListCount(GetSubTypeFields_GCIO(theSubType))==0)
    )
  {
    L= CPLListAppend(GetSubTypeFields_GCIO(theSubType),theField);
  }
  else
  {
    L= CPLListInsert(GetSubTypeFields_GCIO(theSubType),theField,where);
  }
  if( !L )
  {
    _DestroyField_GCIO(&theField);
    CPLError( CE_Failure, CPLE_OutOfMemory,
              "failed to add a Geoconcept field for '%s.%s@%s#%ld'.\n",
              typName, subtypName, name, id);
    return NULL;
  }
  SetSubTypeFields_GCIO(theSubType, L);

  CPLDebug("GEOCONCEPT", "Field '%s.%s@%s#%ld' added.\n", typName, subtypName, name, id);

  return theField;
}/* AddSubTypeField_GCIO */

/* -------------------------------------------------------------------- */
static OGRErr GCIOAPI_CALL _readConfigField_GCIO (
                                                   GCExportFileH* hGCT
                                                 )
{
  int eof, res;
  char *k, n[kItemSize_GCIO], x[kExtraSize_GCIO], e[kExtraSize_GCIO];
  const char* normName;
  long id;
  GCTypeKind knd;
  CPLList* L;
  GCField* theField;

  eof= 0;
  n[0]= '\0';
  x[0]= '\0';
  e[0]= '\0';
  id= UNDEFINEDID_GCIO;
  knd= vUnknownItemType_GCIO;
  theField= NULL;
  while( _get_GCIO(hGCT)!=EOF )
  {
    if( GetGCWhatIs_GCIO(hGCT)==vComType_GCIO )
    {
      continue;
    }
    if( GetGCWhatIs_GCIO(hGCT)==vHeader_GCIO )
    {
      if( strstr(GetGCCache_GCIO(hGCT),kConfigEndField_GCIO)!=NULL)
      {
        eof= 1;
        if( n[0]=='\0' || id==UNDEFINEDID_GCIO || knd==vUnknownItemType_GCIO )
        {
          CPLError( CE_Failure, CPLE_AppDefined,
                    "Missing %s.\n",
                    n[0]=='\0'? "Name": id==UNDEFINEDID_GCIO? "ID": "Kind");
          res= OGRERR_CORRUPT_DATA;
          goto onError;
        }
        normName= _NormalizeFieldName_GCIO(n);
        if( _findFieldByName_GCIO(GetMetaFields_GCIO(GetGCMeta_GCIO(hGCT)),normName)!=-1 )
        {
          CPLError( CE_Failure, CPLE_AppDefined,
                    "field '@%s#%ld' already exists.\n",
                    n, id);
          res= OGRERR_CORRUPT_DATA;
          goto onError;
        }
        if( !(theField= _CreateField_GCIO(normName,id,knd,x,e)) )
        {
          res= OGRERR_CORRUPT_DATA;
          goto onError;
        }
        if( (L= CPLListAppend(GetMetaFields_GCIO(GetGCMeta_GCIO(hGCT)),theField))==NULL )
        {
          _DestroyField_GCIO(&theField);
          CPLError( CE_Failure, CPLE_OutOfMemory,
                    "failed to add a Geoconcept field for '@%s#%ld'.\n",
                    n, id);
          res= OGRERR_CORRUPT_DATA;
          goto onError;
        }
        SetMetaFields_GCIO(GetGCMeta_GCIO(hGCT), L);
        break;
      }
      res= OGRERR_NONE;
      if( (k= strstr(GetGCCache_GCIO(hGCT),kConfigName_GCIO))!=NULL )
      {
        if( n[0]!='\0' )
        {
          CPLError( CE_Failure, CPLE_AppDefined,
                    "Duplicate Name found : '%s'.\n",
                    GetGCCache_GCIO(hGCT));
          res= OGRERR_CORRUPT_DATA;
          goto onError;
        }
        if( (k= _getHeaderValue_GCIO(k))==NULL )
        {
          CPLError( CE_Failure, CPLE_AppDefined,
                    "Invalid Name found : '%s'.\n",
                    GetGCCache_GCIO(hGCT));
          res= OGRERR_CORRUPT_DATA;
          goto onError;
        }
        strncpy(n,k,kItemSize_GCIO-1), n[kItemSize_GCIO-1]= '\0';
      }
      else
        if( (k= strstr(GetGCCache_GCIO(hGCT),kConfigID_GCIO))!=NULL )
        {
          if( id!=UNDEFINEDID_GCIO )
          {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Duplicate ID found : '%s'.\n",
                      GetGCCache_GCIO(hGCT));
            res= OGRERR_CORRUPT_DATA;
            goto onError;
          }
          if( (k= _getHeaderValue_GCIO(k))==NULL )
          {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Invalid ID found : '%s'.\n",
                      GetGCCache_GCIO(hGCT));
            res= OGRERR_CORRUPT_DATA;
            goto onError;
          }
          if( sscanf(k,"%ld", &id)!=1 )
          {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Invalid ID found : '%s'.\n",
                      GetGCCache_GCIO(hGCT));
            res= OGRERR_CORRUPT_DATA;
            goto onError;
          }
        }
        else
          if( (k= strstr(GetGCCache_GCIO(hGCT),kConfigKind_GCIO))!=NULL )
          {
            if( knd!=vUnknownItemType_GCIO )
            {
              CPLError( CE_Failure, CPLE_AppDefined,
                        "Duplicate Kind found : '%s'.\n",
                        GetGCCache_GCIO(hGCT));
              res= OGRERR_CORRUPT_DATA;
              goto onError;
            }
            if( (k= _getHeaderValue_GCIO(k))==NULL )
            {
              CPLError( CE_Failure, CPLE_AppDefined,
                        "Invalid Kind found : '%s'.\n",
                        GetGCCache_GCIO(hGCT));
              res= OGRERR_CORRUPT_DATA;
              goto onError;
            }
            if( (knd= str2GCTypeKind_GCIO(k))==vUnknownItemType_GCIO )
            {
              CPLError( CE_Failure, CPLE_AppDefined,
                        "Not supported Kind found : '%s'.\n",
                        GetGCCache_GCIO(hGCT));
              res= OGRERR_CORRUPT_DATA;
              goto onError;
            }
          }
          else
            if( (k= strstr(GetGCCache_GCIO(hGCT),kConfigExtra_GCIO))!=NULL ||
                (k= strstr(GetGCCache_GCIO(hGCT),kConfigExtraText_GCIO))!=NULL )
            {
              if( x[0]!='\0' )
              {
                CPLError( CE_Failure, CPLE_AppDefined,
                          "Duplicate Extra information found : '%s'.\n",
                          GetGCCache_GCIO(hGCT));
                res= OGRERR_CORRUPT_DATA;
                goto onError;
              }
              if( (k= _getHeaderValue_GCIO(k))==NULL )
              {
                CPLError( CE_Failure, CPLE_AppDefined,
                          "Invalid Extra information found : '%s'.\n",
                          GetGCCache_GCIO(hGCT));
                res= OGRERR_CORRUPT_DATA;
                goto onError;
              }
              strncpy(x,k,kExtraSize_GCIO-1), n[kExtraSize_GCIO-1]= '\0';
            }
            else
              if( (k= strstr(GetGCCache_GCIO(hGCT),kConfigList_GCIO))!=NULL )
              {
                if( e[0]!='\0' )
                {
                  CPLError( CE_Failure, CPLE_AppDefined,
                            "Duplicate List found : '%s'.\n",
                            GetGCCache_GCIO(hGCT));
                  res= OGRERR_CORRUPT_DATA;
                  goto onError;
                }
                if( (k= _getHeaderValue_GCIO(k))==NULL )
                {
                  CPLError( CE_Failure, CPLE_AppDefined,
                            "Invalid List found : '%s'.\n",
                            GetGCCache_GCIO(hGCT));
                  res= OGRERR_CORRUPT_DATA;
                  goto onError;
                }
                strncpy(e,k,kExtraSize_GCIO-1), e[kExtraSize_GCIO-1]= '\0';
              }
              else
              { /* Skipping ... */
                res= OGRERR_NONE;
              }
      if( res != OGRERR_NONE )
      {
        goto onError;
      }
      continue;
    }
onError:
    return OGRERR_CORRUPT_DATA;
  }
  if (eof!=1)
  {
    CPLError( CE_Failure, CPLE_AppDefined,
              "Geoconcept config field end block %s not found.\n",
              kConfigEndField_GCIO);
    return OGRERR_CORRUPT_DATA;
  }

  return OGRERR_NONE;
}/* _readConfigField_GCIO */

/* -------------------------------------------------------------------- */
static OGRErr GCIOAPI_CALL _readConfigFieldType_GCIO (
                                                       GCExportFileH* hGCT,
                                                       GCType* theClass
                                                     )
{
  int eof, res;
  char *k, n[kItemSize_GCIO], x[kExtraSize_GCIO], e[kExtraSize_GCIO];
  long id;
  GCTypeKind knd;
  GCField* theField;

  eof= 0;
  n[0]= '\0';
  x[0]= '\0';
  e[0]= '\0';
  id= UNDEFINEDID_GCIO;
  knd= vUnknownItemType_GCIO;
  theField= NULL;
  while( _get_GCIO(hGCT)!=EOF ) {
    if( GetGCWhatIs_GCIO(hGCT)==vComType_GCIO )
    {
      continue;
    }
    if( GetGCWhatIs_GCIO(hGCT)==vHeader_GCIO )
    {
      if( strstr(GetGCCache_GCIO(hGCT),kConfigEndField_GCIO)!=NULL)
      {
        eof= 1;
        if( n[0]=='\0' || id==UNDEFINEDID_GCIO || knd==vUnknownItemType_GCIO )
        {
          CPLError( CE_Failure, CPLE_AppDefined,
                    "Missing %s.\n",
                    n[0]=='\0'? "Name": id==UNDEFINEDID_GCIO? "ID": "Kind");
          res= OGRERR_CORRUPT_DATA;
          goto onError;
        }
        if( (theField= AddTypeField_GCIO(hGCT,GetTypeName_GCIO(theClass),-1,n,id,knd,x,e))==NULL )
        {
          res= OGRERR_CORRUPT_DATA;
          goto onError;
        }
        break;
      }
      res= OGRERR_NONE;
      if( (k= strstr(GetGCCache_GCIO(hGCT),kConfigName_GCIO))!=NULL )
      {
        if( n[0]!='\0' )
        {
          CPLError( CE_Failure, CPLE_AppDefined,
                    "Duplicate Name found : '%s'.\n",
                    GetGCCache_GCIO(hGCT));
          res= OGRERR_CORRUPT_DATA;
          goto onError;
        }
        if( (k= _getHeaderValue_GCIO(k))==NULL )
        {
          CPLError( CE_Failure, CPLE_AppDefined,
                    "Invalid Name found : '%s'.\n",
                    GetGCCache_GCIO(hGCT));
          res= OGRERR_CORRUPT_DATA;
          goto onError;
        }
        strncpy(n,k,kItemSize_GCIO-1), n[kItemSize_GCIO-1]= '\0';
      }
      else
        if( (k= strstr(GetGCCache_GCIO(hGCT),kConfigID_GCIO))!=NULL )
        {
          if( id!=UNDEFINEDID_GCIO )
          {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Duplicate ID found : '%s'.\n",
                      GetGCCache_GCIO(hGCT));
            res= OGRERR_CORRUPT_DATA;
            goto onError;
          }
          if( (k= _getHeaderValue_GCIO(k))==NULL )
          {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Invalid ID found : '%s'.\n",
                      GetGCCache_GCIO(hGCT));
            res= OGRERR_CORRUPT_DATA;
            goto onError;
          }
          if( sscanf(k,"%ld", &id)!=1 )
          {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Invalid ID found : '%s'.\n",
                      GetGCCache_GCIO(hGCT));
            res= OGRERR_CORRUPT_DATA;
            goto onError;
          }
        }
        else
          if( (k= strstr(GetGCCache_GCIO(hGCT),kConfigKind_GCIO))!=NULL )
          {
            if( knd!=vUnknownItemType_GCIO )
            {
              CPLError( CE_Failure, CPLE_AppDefined,
                        "Duplicate Kind found : '%s'.\n",
                        GetGCCache_GCIO(hGCT));
              res= OGRERR_CORRUPT_DATA;
              goto onError;
            }
            if( (k= _getHeaderValue_GCIO(k))==NULL )
            {
              CPLError( CE_Failure, CPLE_AppDefined,
                        "Invalid Kind found : '%s'.\n",
                        GetGCCache_GCIO(hGCT));
              res= OGRERR_CORRUPT_DATA;
              goto onError;
            }
            if( (knd= str2GCTypeKind_GCIO(k))==vUnknownItemType_GCIO )
            {
              CPLError( CE_Failure, CPLE_AppDefined,
                        "Not supported Kind found : '%s'.\n",
                        GetGCCache_GCIO(hGCT));
              res= OGRERR_CORRUPT_DATA;
              goto onError;
            }
          }
          else
            if( (k= strstr(GetGCCache_GCIO(hGCT),kConfigExtra_GCIO))!=NULL ||
                (k= strstr(GetGCCache_GCIO(hGCT),kConfigExtraText_GCIO))!=NULL )
            {
              if( x[0]!='\0' )
              {
                CPLError( CE_Failure, CPLE_AppDefined,
                          "Duplicate Extra information found : '%s'.\n",
                          GetGCCache_GCIO(hGCT));
                res= OGRERR_CORRUPT_DATA;
                goto onError;
              }
              if( (k= _getHeaderValue_GCIO(k))==NULL )
              {
                CPLError( CE_Failure, CPLE_AppDefined,
                          "Invalid extra information found : '%s'.\n",
                          GetGCCache_GCIO(hGCT));
                res= OGRERR_CORRUPT_DATA;
                goto onError;
              }
              strncpy(x,k,kExtraSize_GCIO-1), n[kExtraSize_GCIO-1]= '\0';
            }
            else
              if( (k= strstr(GetGCCache_GCIO(hGCT),kConfigList_GCIO))!=NULL )
              {
                if( e[0]!='\0' )
                {
                  CPLError( CE_Failure, CPLE_AppDefined,
                            "Duplicate List found : '%s'.\n",
                            GetGCCache_GCIO(hGCT));
                  res= OGRERR_CORRUPT_DATA;
                  goto onError;
                }
                if( (k= _getHeaderValue_GCIO(k))==NULL )
                {
                  CPLError( CE_Failure, CPLE_AppDefined,
                            "Invalid List found : '%s'.\n",
                            GetGCCache_GCIO(hGCT));
                  res= OGRERR_CORRUPT_DATA;
                  goto onError;
                }
                strncpy(e,k,kExtraSize_GCIO-1), e[kExtraSize_GCIO-1]= '\0';
              }
              else
              { /* Skipping ... */
                res= OGRERR_NONE;
              }
      if( res != OGRERR_NONE )
      {
        goto onError;
      }
      continue;
    }
onError:
    return OGRERR_CORRUPT_DATA;
  }
  if (eof!=1)
  {
    CPLError( CE_Failure, CPLE_AppDefined,
              "Geoconcept config field end block %s not found.\n",
              kConfigEndField_GCIO);
    return OGRERR_CORRUPT_DATA;
  }

  return OGRERR_NONE;
}/* _readConfigFieldType_GCIO */

/* -------------------------------------------------------------------- */
static OGRErr GCIOAPI_CALL _readConfigFieldSubType_GCIO (
                                                          GCExportFileH* hGCT,
                                                          GCType* theClass,
                                                          GCSubType* theSubType
                                                        )
{
  int eof, res;
  char *k, n[kItemSize_GCIO], x[kExtraSize_GCIO], e[kExtraSize_GCIO];
  long id;
  GCTypeKind knd;
  GCField* theField;

  eof= 0;
  n[0]= '\0';
  x[0]= '\0';
  e[0]= '\0';
  id= UNDEFINEDID_GCIO;
  knd= vUnknownItemType_GCIO;
  theField= NULL;
  while( _get_GCIO(hGCT)!=EOF ) {
    if( GetGCWhatIs_GCIO(hGCT)==vComType_GCIO )
    {
      continue;
    }
    if( GetGCWhatIs_GCIO(hGCT)==vHeader_GCIO )
    {
      if( strstr(GetGCCache_GCIO(hGCT),kConfigEndField_GCIO)!=NULL)
      {
        eof= 1;
        if( n[0]=='\0' || id==UNDEFINEDID_GCIO || knd==vUnknownItemType_GCIO )
        {
          CPLError( CE_Failure, CPLE_AppDefined,
                    "Missing %s.\n",
                    n[0]=='\0'? "Name": id==UNDEFINEDID_GCIO? "ID": "Kind");
          res= OGRERR_CORRUPT_DATA;
          goto onError;
        }
        if( (theField= AddSubTypeField_GCIO(hGCT,GetTypeName_GCIO(theClass),GetSubTypeName_GCIO(theSubType),-1,n,id,knd,x,e))==NULL )
        {
          res= OGRERR_CORRUPT_DATA;
          goto onError;
        }
        break;
      }
      res= OGRERR_NONE;
      if( (k= strstr(GetGCCache_GCIO(hGCT),kConfigName_GCIO))!=NULL )
      {
        if( n[0]!='\0' )
        {
          CPLError( CE_Failure, CPLE_AppDefined,
                    "Duplicate Name found : '%s'.\n",
                    GetGCCache_GCIO(hGCT));
          res= OGRERR_CORRUPT_DATA;
          goto onError;
        }
        if( (k= _getHeaderValue_GCIO(k))==NULL )
        {
          CPLError( CE_Failure, CPLE_AppDefined,
                    "Invalid Name found : '%s'.\n",
                    GetGCCache_GCIO(hGCT));
          res= OGRERR_CORRUPT_DATA;
          goto onError;
        }
        strncpy(n,k,kItemSize_GCIO-1), n[kItemSize_GCIO-1]= '\0';
      }
      else
        if( (k= strstr(GetGCCache_GCIO(hGCT),kConfigID_GCIO))!=NULL )
        {
          if( id!=UNDEFINEDID_GCIO )
          {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Duplicate ID found : '%s'.\n",
                      GetGCCache_GCIO(hGCT));
            res= OGRERR_CORRUPT_DATA;
            goto onError;
          }
          if( (k= _getHeaderValue_GCIO(k))==NULL )
          {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Invalid ID found : '%s'.\n",
                      GetGCCache_GCIO(hGCT));
            res= OGRERR_CORRUPT_DATA;
            goto onError;
          }
          if( sscanf(k,"%ld", &id)!=1 )
          {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Invalid ID found : '%s'.\n",
                      GetGCCache_GCIO(hGCT));
            res= OGRERR_CORRUPT_DATA;
            goto onError;
          }
        }
        else
          if( (k= strstr(GetGCCache_GCIO(hGCT),kConfigKind_GCIO))!=NULL )
          {
            if( knd!=vUnknownItemType_GCIO )
            {
              CPLError( CE_Failure, CPLE_AppDefined,
                        "Duplicate Kind found : '%s'.\n",
                        GetGCCache_GCIO(hGCT));
              res= OGRERR_CORRUPT_DATA;
              goto onError;
            }
            if( (k= _getHeaderValue_GCIO(k))==NULL )
            {
              CPLError( CE_Failure, CPLE_AppDefined,
                        "Invalid Kind found : '%s'.\n",
                        GetGCCache_GCIO(hGCT));
              res= OGRERR_CORRUPT_DATA;
              goto onError;
            }
            if( (knd= str2GCTypeKind_GCIO(k))==vUnknownItemType_GCIO )
            {
              CPLError( CE_Failure, CPLE_AppDefined,
                        "Not supported Kind found : '%s'.\n",
                        GetGCCache_GCIO(hGCT));
              res= OGRERR_CORRUPT_DATA;
              goto onError;
            }
          }
          else
            if( (k= strstr(GetGCCache_GCIO(hGCT),kConfigExtra_GCIO))!=NULL ||
                (k= strstr(GetGCCache_GCIO(hGCT),kConfigExtraText_GCIO))!=NULL )
            {
              if( x[0]!='\0' )
              {
                CPLError( CE_Failure, CPLE_AppDefined,
                          "Duplicate Extra information found : '%s'.\n",
                          GetGCCache_GCIO(hGCT));
                res= OGRERR_CORRUPT_DATA;
                goto onError;
              }
              if( (k= _getHeaderValue_GCIO(k))==NULL )
              {
                CPLError( CE_Failure, CPLE_AppDefined,
                          "Invalid extra information found : '%s'.\n",
                          GetGCCache_GCIO(hGCT));
                res= OGRERR_CORRUPT_DATA;
                goto onError;
              }
              strncpy(x,k,kExtraSize_GCIO-1), n[kExtraSize_GCIO-1]= '\0';
            }
            else
              if( (k= strstr(GetGCCache_GCIO(hGCT),kConfigList_GCIO))!=NULL )
              {
                if( e[0]!='\0' )
                {
                  CPLError( CE_Failure, CPLE_AppDefined,
                            "Duplicate List found : '%s'.\n",
                            GetGCCache_GCIO(hGCT));
                  res= OGRERR_CORRUPT_DATA;
                  goto onError;
                }
                if( (k= _getHeaderValue_GCIO(k))==NULL )
                {
                  CPLError( CE_Failure, CPLE_AppDefined,
                            "Invalid List found : '%s'.\n",
                            GetGCCache_GCIO(hGCT));
                  res= OGRERR_CORRUPT_DATA;
                  goto onError;
                }
                strncpy(e,k,kExtraSize_GCIO-1), e[kExtraSize_GCIO-1]= '\0';
              }
              else
              { /* Skipping ... */
                res= OGRERR_NONE;
              }
      if( res != OGRERR_NONE )
      {
        goto onError;
      }
      continue;
    }
onError:
    return OGRERR_CORRUPT_DATA;
  }
  if (eof!=1)
  {
    CPLError( CE_Failure, CPLE_AppDefined,
              "Geoconcept config field end block %s not found.\n",
              kConfigEndField_GCIO);
    return OGRERR_CORRUPT_DATA;
  }

  return OGRERR_NONE;
}/* _readConfigFieldSubType_GCIO */

/* -------------------------------------------------------------------- */
static OGRErr GCIOAPI_CALL _readConfigSubTypeType_GCIO (
                                                         GCExportFileH* hGCT,
                                                         GCType* theClass
                                                       )
{
  int eost, res;
  char *k, n[kItemSize_GCIO];
  long id;
  GCTypeKind knd;
  GCDim sys;
  GCSubType* theSubType;

  eost= 0;
  n[0]= '\0';
  id= UNDEFINEDID_GCIO;
  knd= vUnknownItemType_GCIO;
  sys= v2D_GCIO;
  theSubType= NULL;
  while( _get_GCIO(hGCT)!=EOF ) {
    if( GetGCWhatIs_GCIO(hGCT)==vComType_GCIO )
    {
      continue;
    }
    if( GetGCWhatIs_GCIO(hGCT)==vHeader_GCIO )
    {
      if( strstr(GetGCCache_GCIO(hGCT),kConfigEndSubType_GCIO)!=NULL)
      {
        eost= 1;
        break;
      }
      res= OGRERR_NONE;
      if( (k= strstr(GetGCCache_GCIO(hGCT),kConfigName_GCIO))!=NULL )
      {
        if( n[0]!='\0' )
        {
          CPLError( CE_Failure, CPLE_AppDefined,
                    "Duplicate Name found : '%s'.\n",
                    GetGCCache_GCIO(hGCT));
          res= OGRERR_CORRUPT_DATA;
          goto onError;
        }
        if( (k= _getHeaderValue_GCIO(k))==NULL )
        {
          CPLError( CE_Failure, CPLE_AppDefined,
                    "Invalid Name found : '%s'.\n",
                    GetGCCache_GCIO(hGCT));
          res= OGRERR_CORRUPT_DATA;
          goto onError;
        }
        strncpy(n,k,kItemSize_GCIO-1), n[kItemSize_GCIO-1]= '\0';
      }
      else
        if( (k= strstr(GetGCCache_GCIO(hGCT),kConfigID_GCIO))!=NULL )
        {
          if( id!=UNDEFINEDID_GCIO )
          {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Duplicate ID found : '%s'.\n",
                      GetGCCache_GCIO(hGCT));
            res= OGRERR_CORRUPT_DATA;
            goto onError;
          }
          if( (k= _getHeaderValue_GCIO(k))==NULL )
          {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Invalid ID found : '%s'.\n",
                      GetGCCache_GCIO(hGCT));
            res= OGRERR_CORRUPT_DATA;
            goto onError;
          }
          if( sscanf(k,"%ld", &id)!=1 )
          {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Invalid ID found : '%s'.\n",
                      GetGCCache_GCIO(hGCT));
            res= OGRERR_CORRUPT_DATA;
            goto onError;
          }
        }
        else
          if( (k= strstr(GetGCCache_GCIO(hGCT),kConfigKind_GCIO))!=NULL )
          {
            if( knd!=vUnknownItemType_GCIO )
            {
              CPLError( CE_Failure, CPLE_AppDefined,
                        "Duplicate Kind found : '%s'.\n",
                        GetGCCache_GCIO(hGCT));
              res= OGRERR_CORRUPT_DATA;
              goto onError;
            }
            if( (k= _getHeaderValue_GCIO(k))==NULL )
            {
              CPLError( CE_Failure, CPLE_AppDefined,
                        "Invalid Kind found : '%s'.\n",
                        GetGCCache_GCIO(hGCT));
              res= OGRERR_CORRUPT_DATA;
              goto onError;
            }
            if( (knd= str2GCTypeKind_GCIO(k))==vUnknownItemType_GCIO )
            {
              CPLError( CE_Failure, CPLE_AppDefined,
                        "Not supported Kind found : '%s'.\n",
                        GetGCCache_GCIO(hGCT));
              res= OGRERR_CORRUPT_DATA;
              goto onError;
            }
          }
          else
            if( (k= strstr(GetGCCache_GCIO(hGCT),kConfig3D_GCIO))!=NULL )
            {
              if( sys!=vUnknown3D_GCIO && sys!=v2D_GCIO)
              {
                CPLError( CE_Failure, CPLE_AppDefined,
                          "Duplicate Dimension found : '%s'.\n",
                          GetGCCache_GCIO(hGCT));
                res= OGRERR_CORRUPT_DATA;
                goto onError;
              }
              if( (k= _getHeaderValue_GCIO(k))==NULL )
              {
                CPLError( CE_Failure, CPLE_AppDefined,
                          "Invalid Dimension found : '%s'.\n",
                          GetGCCache_GCIO(hGCT));
                res= OGRERR_CORRUPT_DATA;
                goto onError;
              }
              if( (sys= str2GCDim(k))==vUnknown3D_GCIO )
              {
                CPLError( CE_Failure, CPLE_AppDefined,
                          "Not supported Dimension found : '%s'.\n",
                          GetGCCache_GCIO(hGCT));
                res= OGRERR_CORRUPT_DATA;
                goto onError;
              }
            }
            else
              if( strstr(GetGCCache_GCIO(hGCT),kConfigBeginField_GCIO)!=NULL )
              {
                if( theSubType==NULL )
                {
                  if( n[0]=='\0' || id==UNDEFINEDID_GCIO || knd==vUnknownItemType_GCIO || sys==vUnknown3D_GCIO )
                  {
                    CPLError( CE_Failure, CPLE_AppDefined,
                              "Missing %s.\n",
                              n[0]=='\0'? "Name": id==UNDEFINEDID_GCIO? "ID": knd==vUnknownItemType_GCIO? "Kind": "3D");
                    res= OGRERR_CORRUPT_DATA;
                    goto onError;
                  }
                  if( (theSubType= AddSubType_GCIO(hGCT,GetTypeName_GCIO(theClass),n,id,knd,sys))==NULL )
                  {
                    res= OGRERR_CORRUPT_DATA;
                    goto onError;
                  }
                }
                res= _readConfigFieldSubType_GCIO(hGCT,theClass,theSubType);
              }
              else
              { /* Skipping ... */
                res= OGRERR_NONE;
              }
      if( res != OGRERR_NONE )
      {
        goto onError;
      }
      continue;
    }
onError:
    if( theSubType )
    {
      DropSubType_GCIO(hGCT,theClass,&theSubType);
    }
    return OGRERR_CORRUPT_DATA;
  }
  if (eost!=1)
  {
    if( theSubType )
    {
      DropSubType_GCIO(hGCT,theClass,&theSubType);
    }
    CPLError( CE_Failure, CPLE_AppDefined,
              "Geoconcept config subtype end block %s not found.\n",
              kConfigEndSubType_GCIO);
    return OGRERR_CORRUPT_DATA;
  }

  return OGRERR_NONE;
}/* _readConfigSubTypeType_GCIO */

/* -------------------------------------------------------------------- */
static OGRErr GCIOAPI_CALL _readConfigType_GCIO (
                                                  GCExportFileH* hGCT
                                                )
{
  int eot, res;
  char *k, n[kItemSize_GCIO];
  long id;
  GCType *theClass;

  eot= 0;
  n[0]= '\0';
  id= UNDEFINEDID_GCIO;
  theClass= NULL;
  while( _get_GCIO(hGCT)!=EOF ) {
    if( GetGCWhatIs_GCIO(hGCT)==vComType_GCIO )
    {
      continue;
    }
    if( GetGCWhatIs_GCIO(hGCT)==vHeader_GCIO )
    {
      if( strstr(GetGCCache_GCIO(hGCT),kConfigEndType_GCIO)!=NULL )
      {
        eot= 1;
        break;
      }
      res= OGRERR_NONE;
      if( (k= strstr(GetGCCache_GCIO(hGCT),kConfigName_GCIO))!=NULL )
      {
        if( n[0]!='\0' )
        {
          CPLError( CE_Failure, CPLE_AppDefined,
                    "Duplicate Name found : '%s'.\n",
                    GetGCCache_GCIO(hGCT));
          res= OGRERR_CORRUPT_DATA;
          goto onError;
        }
        if( (k= _getHeaderValue_GCIO(k))==NULL )
        {
          CPLError( CE_Failure, CPLE_AppDefined,
                    "Invalid Name found : '%s'.\n",
                    GetGCCache_GCIO(hGCT));
          res= OGRERR_CORRUPT_DATA;
          goto onError;
        }
        strncpy(n,k,kItemSize_GCIO-1), n[kItemSize_GCIO-1]= '\0';
      }
      else
        if( (k= strstr(GetGCCache_GCIO(hGCT),kConfigID_GCIO))!=NULL )
        {
          if( id!=UNDEFINEDID_GCIO )
          {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Duplicate ID found : '%s'.\n",
                      GetGCCache_GCIO(hGCT));
            res= OGRERR_CORRUPT_DATA;
            goto onError;
          }
          if( (k= _getHeaderValue_GCIO(k))==NULL )
          {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Invalid ID found : '%s'.\n",
                      GetGCCache_GCIO(hGCT));
            res= OGRERR_CORRUPT_DATA;
            goto onError;
          }
          if( sscanf(k,"%ld", &id)!=1 )
          {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Not supported ID found : '%s'.\n",
                      GetGCCache_GCIO(hGCT));
            res= OGRERR_CORRUPT_DATA;
            goto onError;
          }
        }
        else
          if( strstr(GetGCCache_GCIO(hGCT),kConfigBeginSubType_GCIO)!=NULL )
          {
            if( theClass==NULL )
            {
              if( n[0]=='\0' || id==UNDEFINEDID_GCIO || (theClass= AddType_GCIO(hGCT,n,id))==NULL )
              {
                res= OGRERR_CORRUPT_DATA;
                goto onError;
              }
            }
            res= _readConfigSubTypeType_GCIO(hGCT,theClass);
          }
          else
            if( strstr(GetGCCache_GCIO(hGCT),kConfigBeginField_GCIO)!=NULL)
            {
              if( theClass==NULL )
              {
                if( n[0]=='\0' || id==UNDEFINEDID_GCIO || (theClass= AddType_GCIO(hGCT,n,id))==NULL )
                {
                  res= OGRERR_CORRUPT_DATA;
                  goto onError;
                }
              }
              res= _readConfigFieldType_GCIO(hGCT,theClass);
            }
            else
            { /* Skipping ... */
              res= OGRERR_NONE;
            }
      if( res != OGRERR_NONE )
      {
        goto onError;
      }
      continue;
    }
onError:
    if( theClass )
    {
      DropType_GCIO(hGCT,&theClass);
    }
    return OGRERR_CORRUPT_DATA;
  }
  if (eot!=1)
  {
    if( theClass )
    {
      DropType_GCIO(hGCT,&theClass);
    }
    CPLError( CE_Failure, CPLE_AppDefined,
              "Geoconcept config type end block %s not found.\n",
              kConfigEndType_GCIO);
    return OGRERR_CORRUPT_DATA;
  }

  return OGRERR_NONE;
}/* _readConfigType_GCIO */

/* -------------------------------------------------------------------- */
static OGRErr GCIOAPI_CALL _readConfigMap_GCIO (
                                                 GCExportFileH* hGCT
                                               )
{
  int eom, res;
  char* k;

  eom= 0;
  while( _get_GCIO(hGCT)!=EOF ) {
    if( GetGCWhatIs_GCIO(hGCT)==vComType_GCIO )
    {
      continue;
    }
    if( GetGCWhatIs_GCIO(hGCT)==vHeader_GCIO )
    {
      if( strstr(GetGCCache_GCIO(hGCT),kConfigEndMap_GCIO)!=NULL )
      {
        eom= 1;
        break;
      }
      res= OGRERR_NONE;
      if( (k= strstr(GetGCCache_GCIO(hGCT),kConfigUnit_GCIO))!=NULL &&
          strstr(GetGCCache_GCIO(hGCT),kConfigZUnit_GCIO)==NULL )
      {
        if( (k= _getHeaderValue_GCIO(k))==NULL )
        {
          CPLError( CE_Failure, CPLE_AppDefined,
                    "Invalid Unit found : '%s'.\n",
                    GetGCCache_GCIO(hGCT));
          res= OGRERR_CORRUPT_DATA;
          goto onError;
        }
        SetMetaUnit_GCIO(GetGCMeta_GCIO(hGCT),k);
      }
      else
        if( (k= strstr(GetGCCache_GCIO(hGCT),kConfigPrecision_GCIO))!=NULL &&
            strstr(GetGCCache_GCIO(hGCT),kConfigZPrecision_GCIO)==NULL )
        {
          double r;
          if( (k= _getHeaderValue_GCIO(k))==NULL )
          {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Invalid Precision found : '%s'.\n",
                      GetGCCache_GCIO(hGCT));
            res= OGRERR_CORRUPT_DATA;
            goto onError;
          }
          if( sscanf(k,"%lf", &r)!=1 )
          {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Invalid Precision found : '%s'.\n",
                      GetGCCache_GCIO(hGCT));
            res= OGRERR_CORRUPT_DATA;
            goto onError;
          }
          SetMetaResolution_GCIO(GetGCMeta_GCIO(hGCT),r);
        }
        else
          if( (k= strstr(GetGCCache_GCIO(hGCT),kConfigZUnit_GCIO))!=NULL )
          {
            if( (k= _getHeaderValue_GCIO(k))==NULL )
            {
              CPLError( CE_Failure, CPLE_AppDefined,
                        "Invalid ZUnit found : '%s'.\n",
                        GetGCCache_GCIO(hGCT));
              res= OGRERR_CORRUPT_DATA;
              goto onError;
            }
            SetMetaZUnit_GCIO(GetGCMeta_GCIO(hGCT),k);
          }
          else
            if( (k= strstr(GetGCCache_GCIO(hGCT),kConfigZPrecision_GCIO))!=NULL )
            {
              double r;
              if( (k= _getHeaderValue_GCIO(k))==NULL )
              {
                CPLError( CE_Failure, CPLE_AppDefined,
                          "Invalid ZPrecision found : '%s'.\n",
                          GetGCCache_GCIO(hGCT));
                res= OGRERR_CORRUPT_DATA;
                goto onError;
              }
              if( sscanf(k,"%lf", &r)!=1 )
              {
                CPLError( CE_Failure, CPLE_AppDefined,
                          "Invalid ZPrecision found : '%s'.\n",
                          GetGCCache_GCIO(hGCT));
                res= OGRERR_CORRUPT_DATA;
                goto onError;
              }
              SetMetaZResolution_GCIO(GetGCMeta_GCIO(hGCT), r);
            }
            else
            { /* Skipping ... */
              res= OGRERR_NONE;
            }
      if( res != OGRERR_NONE )
      {
        goto onError;
      }
      continue;
    }
onError:
    return OGRERR_CORRUPT_DATA;
  }
  if (eom!=1)
  {
    CPLError( CE_Failure, CPLE_AppDefined,
              "Geoconcept config map end block %s not found.\n",
              kConfigEndMap_GCIO);
    return OGRERR_CORRUPT_DATA;
  }

  return OGRERR_NONE;
}/* _readConfigMap_GCIO */

/* -------------------------------------------------------------------- */
GCExportFileMetadata GCIOAPI_CALL1(*) ReadConfig_GCIO (
                                                        GCExportFileH* hGCT
                                                      )
{
  int eoc, res, it, nt;
  int i, n, il, nl, ll;
  int is, ns;
  char l[kExtraSize_GCIO];
  const char *v;
  GCField* theField;
  GCSubType* theSubType;
  GCType* theClass;
  CPLList *e, *es, *et;
  GCExportFileMetadata* Meta;

  eoc= 0;
  if( _get_GCIO(hGCT)==EOF )
  {
    return NULL;
  }
  if( GetGCWhatIs_GCIO(hGCT)!=vHeader_GCIO &&
      strstr(GetGCCache_GCIO(hGCT),kConfigBeginConfig_GCIO)==NULL )
  {
    CPLError( CE_Failure, CPLE_AppDefined,
              "Geoconcept config begin block %s not found.\n",
              kConfigBeginConfig_GCIO);
    return NULL;
  }
  SetGCMeta_GCIO(hGCT, _CreateHeader_GCIO());
  if( (Meta= GetGCMeta_GCIO(hGCT))==NULL )
  {
    return NULL;
  }
  while( _get_GCIO(hGCT)!=EOF )
  {
    if( GetGCWhatIs_GCIO(hGCT)==vComType_GCIO )
    {
      continue;
    }
    if( GetGCWhatIs_GCIO(hGCT)==vHeader_GCIO )
    {
      if( strstr(GetGCCache_GCIO(hGCT),kConfigEndConfig_GCIO)!=NULL )
      {
        eoc= 1;
        break;
      }
      res= OGRERR_NONE;
      if( strstr(GetGCCache_GCIO(hGCT),kConfigBeginMap_GCIO)!=NULL )
      {
        res= _readConfigMap_GCIO(hGCT);
      }
      else
        if( strstr(GetGCCache_GCIO(hGCT),kConfigBeginType_GCIO)!=NULL )
        {
          res= _readConfigType_GCIO(hGCT);
        }
        else
          if( strstr(GetGCCache_GCIO(hGCT),kConfigBeginField_GCIO)!=NULL )
          {
            res= _readConfigField_GCIO(hGCT);
          }
          else
          { /* Skipping : Version, Origin, ... */
            res= OGRERR_NONE;
          }
      if( res != OGRERR_NONE )
      {
        goto onError;
      }
      continue;
    }
onError:
    _DestroyHeader_GCIO(&(GetGCMeta_GCIO(hGCT)));
    CPLError( CE_Failure, CPLE_AppDefined,
              "Geoconcept config syntax error at line %ld.\n",
              GetGCCurrentLinenum_GCIO(hGCT) );
    return NULL;
  }
  if (eoc!=1)
  {
    _DestroyHeader_GCIO(&(GetGCMeta_GCIO(hGCT)));
    CPLError( CE_Failure, CPLE_AppDefined,
              "Geoconcept config end block %s not found.\n",
              kConfigEndConfig_GCIO);
    return NULL;
  }

  if( (nt= CPLListCount(GetMetaTypes_GCIO(Meta)))==0 )
  {
    _DestroyHeader_GCIO(&(GetGCMeta_GCIO(hGCT)));
    CPLError( CE_Failure, CPLE_AppDefined,
              "No types found.\n");
    return NULL;
  }
  /* for each general fields add it on top of types' fields list          */
  if( GetMetaFields_GCIO(Meta) )
  {
    if( (n= CPLListCount(GetMetaFields_GCIO(Meta)))>0 )
    {
      for (i= n-1; i>=0; i--)
      {
        if( (e= CPLListGet(GetMetaFields_GCIO(Meta),i)) )
        {
          if( (theField= (GCField*)CPLListGetData(e)) )
          {
            l[0]= '\0';
            ll= 0;
            if( (nl= CSLCount(GetFieldList_GCIO(theField)))>0 )
            {
              for (il= 0; il<nl; il++)
              {
                v= CSLGetField(GetFieldList_GCIO(theField),il);
                snprintf(l+ll,kExtraSize_GCIO-ll-1,"%s;", v), l[kExtraSize_GCIO-1]= '\0';
                ll+= strlen(v);
              }
            }
            for (it= 0; it<nt; it++)
            {
              if( (et= CPLListGet(GetMetaTypes_GCIO(Meta),it)) )
              {
                if( (theClass= (GCType*)CPLListGetData(et)) )
                {
                  if( AddTypeField_GCIO(hGCT,GetTypeName_GCIO(theClass),
                                             0,
                                             GetFieldName_GCIO(theField),
                                             GetFieldID_GCIO(theField),
                                             GetFieldKind_GCIO(theField),
                                             GetFieldExtra_GCIO(theField),
                                             l)==NULL )
                  {
                    _DestroyHeader_GCIO(&(GetGCMeta_GCIO(hGCT)));
                    return NULL;
                  }
                }
              }
            }
          }
        }
      }
      for (i= n-1; i>=0; i--)
      {
        if( (e= CPLListGet(GetMetaFields_GCIO(Meta),i)) )
        {
          if( (theField= (GCField*)CPLListGetData(e)) )
          {
            _DestroyField_GCIO(&theField);
          }
        }
      }
    }
    CPLListDestroy(GetMetaFields_GCIO(Meta));
    SetMetaFields_GCIO(Meta, NULL);
  }

  /* for each field of types add it on top of types' subtypes field list */
  for (it= 0; it<nt; it++)
  {
    if( (et= CPLListGet(GetMetaTypes_GCIO(Meta),it)) )
    {
      if( (theClass= (GCType*)CPLListGetData(et)) )
      {
        if( (ns= CPLListCount(GetTypeSubtypes_GCIO(theClass)))==0 )
        {
          CPLError( CE_Failure, CPLE_AppDefined,
                    "No subtypes found for type %s.\n",
                    GetTypeName_GCIO(theClass));
          _DestroyHeader_GCIO(&(GetGCMeta_GCIO(hGCT)));
          return NULL;
        }
        for (is= 0; is<ns; is++)
        {
          if( (es= CPLListGet(GetTypeSubtypes_GCIO(theClass),is)) )
          {
            if( (theSubType= (GCSubType*)CPLListGetData(es)) )
            {
              if( _findFieldByName_GCIO(GetTypeFields_GCIO(theClass),kNbFields_GCIO)==-1 &&
                  _findFieldByName_GCIO(GetSubTypeFields_GCIO(theSubType),kNbFields_GCIO)==-1 )
              {
                if( AddSubTypeField_GCIO(hGCT,GetTypeName_GCIO(theClass),
                                              GetSubTypeName_GCIO(theSubType),
                                              0,
                                              kNbFields_GCIO,
                                              -9999L,
                                              vIntFld_GCIO,
                                              NULL,
                                              NULL)==NULL )
                {
                  _DestroyHeader_GCIO(&(GetGCMeta_GCIO(hGCT)));
                  return NULL;
                }
              }
              if( (n= CPLListCount(GetTypeFields_GCIO(theClass)))>0 )
              {
                for (i= n-1; i>=0; i--)
                {
                  if( (e= CPLListGet(GetTypeFields_GCIO(theClass),i)) )
                  {
                    if( (theField= (GCField*)CPLListGetData(e)) )
                    {
                      l[0]= '\0';
                      ll= 0;
                      if( (nl= CSLCount(GetFieldList_GCIO(theField)))>0 )
                      {
                        for (il= 0; il<nl; il++)
                        {
                          v= CSLGetField(GetFieldList_GCIO(theField),il);
                          snprintf(l+ll,kExtraSize_GCIO-ll-1,"%s;", v), l[kExtraSize_GCIO-1]= '\0';
                          ll+= strlen(v);
                        }
                      }
                      if( AddSubTypeField_GCIO(hGCT,GetTypeName_GCIO(theClass),
                                                    GetSubTypeName_GCIO(theSubType),
                                                    0,
                                                    GetFieldName_GCIO(theField),
                                                    GetFieldID_GCIO(theField),
                                                    GetFieldKind_GCIO(theField),
                                                    GetFieldExtra_GCIO(theField),
                                                    l)==NULL )
                      {
                        _DestroyHeader_GCIO(&(GetGCMeta_GCIO(hGCT)));
                        return NULL;
                      }
                    }
                  }
                }
              }
            }
          }
        }
        if( (n= CPLListCount(GetTypeFields_GCIO(theClass)))>0 )
        {
          for (i= n-1; i>=0; i--)
          {
            if( (e= CPLListGet(GetTypeFields_GCIO(theClass),i)) )
            {
              if( (theField= (GCField*)CPLListGetData(e)) )
              {
                _DestroyField_GCIO(&theField);
              }
            }
          }
        }
        CPLListDestroy(GetTypeFields_GCIO(theClass));
        SetTypeFields_GCIO(theClass, NULL);
      }
    }
  }

  /* let's reorder sub-types fields : */
  for (it= 0; it<nt; it++)
  {
    if( (et= CPLListGet(GetMetaTypes_GCIO(Meta),it)) )
    {
      if( (theClass= (GCType*)CPLListGetData(et)) )
      {
        ns= CPLListCount(GetTypeSubtypes_GCIO(theClass));
        for (is= 0; is<ns; is++)
        {
          if( (es= CPLListGet(GetTypeSubtypes_GCIO(theClass),is)) )
          {
            if( (theSubType= (GCSubType*)CPLListGetData(es)) )
            {
              CPLList* orderedFields= NULL;
              if( (n= CPLListCount(GetSubTypeFields_GCIO(theSubType)))>0 )
              {
                if( (i= _findFieldByName_GCIO(GetSubTypeFields_GCIO(theSubType),kIdentifier_GCIO))!=-1 )
                {
                  e= CPLListGet(GetSubTypeFields_GCIO(theSubType),i);
                  if( !(orderedFields= CPLListAppend(orderedFields,(GCField*)CPLListGetData(e))) )
                  {
                    CPLError( CE_Failure, CPLE_OutOfMemory,
                              "failed to arrange Geoconcept subtype '%s.%s' fields list.\n",
                              GetTypeName_GCIO(theClass), GetSubTypeName_GCIO(theSubType));
                    _DestroyHeader_GCIO(&(GetGCMeta_GCIO(hGCT)));
                    return NULL;
                  }
                }
                if( (i= _findFieldByName_GCIO(GetSubTypeFields_GCIO(theSubType),kClass_GCIO))!=-1 )
                {
                  e= CPLListGet(GetSubTypeFields_GCIO(theSubType),i);
                  if( !(orderedFields= CPLListAppend(orderedFields,(GCField*)CPLListGetData(e))) )
                  {
                    CPLError( CE_Failure, CPLE_OutOfMemory,
                              "failed to arrange Geoconcept subtype '%s.%s' fields list.\n",
                              GetTypeName_GCIO(theClass), GetSubTypeName_GCIO(theSubType));
                    _DestroyHeader_GCIO(&(GetGCMeta_GCIO(hGCT)));
                    return NULL;
                  }
                }
                if( (i= _findFieldByName_GCIO(GetSubTypeFields_GCIO(theSubType),kSubclass_GCIO))!=-1 )
                {
                  e= CPLListGet(GetSubTypeFields_GCIO(theSubType),i);
                  if( !(orderedFields= CPLListAppend(orderedFields,(GCField*)CPLListGetData(e))) )
                  {
                    CPLError( CE_Failure, CPLE_OutOfMemory,
                              "failed to arrange Geoconcept subtype '%s.%s' fields list.\n",
                              GetTypeName_GCIO(theClass), GetSubTypeName_GCIO(theSubType));
                    _DestroyHeader_GCIO(&(GetGCMeta_GCIO(hGCT)));
                    return NULL;
                  }
                }
                if( (i= _findFieldByName_GCIO(GetSubTypeFields_GCIO(theSubType),kName_GCIO))!=-1 )
                {
                  e= CPLListGet(GetSubTypeFields_GCIO(theSubType),i);
                  if( !(orderedFields= CPLListAppend(orderedFields,(GCField*)CPLListGetData(e))) )
                  {
                    CPLError( CE_Failure, CPLE_OutOfMemory,
                              "failed to arrange Geoconcept subtype '%s.%s' fields list.\n",
                              GetTypeName_GCIO(theClass), GetSubTypeName_GCIO(theSubType));
                    _DestroyHeader_GCIO(&(GetGCMeta_GCIO(hGCT)));
                    return NULL;
                  }
                }
                if( (i= _findFieldByName_GCIO(GetSubTypeFields_GCIO(theSubType),kNbFields_GCIO))!=-1 )
                {
                  e= CPLListGet(GetSubTypeFields_GCIO(theSubType),i);
                  if( !(orderedFields= CPLListAppend(orderedFields,(GCField*)CPLListGetData(e))) )
                  {
                    CPLError( CE_Failure, CPLE_OutOfMemory,
                              "failed to arrange Geoconcept subtype '%s.%s' fields list.\n",
                              GetTypeName_GCIO(theClass), GetSubTypeName_GCIO(theSubType));
                    _DestroyHeader_GCIO(&(GetGCMeta_GCIO(hGCT)));
                    return NULL;
                  }
                }
                for( i= 0; i<n; i++ )
                {
                  e= CPLListGet(GetSubTypeFields_GCIO(theSubType),i);
                  if( !IsPrivateField_GCIO((GCField*)CPLListGetData(e)) )
                  {
                    if( !(orderedFields= CPLListAppend(orderedFields,(GCField*)CPLListGetData(e))) )
                    {
                      CPLError( CE_Failure, CPLE_OutOfMemory,
                                "failed to arrange Geoconcept subtype '%s.%s' fields list.\n",
                                GetTypeName_GCIO(theClass), GetSubTypeName_GCIO(theSubType));
                      _DestroyHeader_GCIO(&(GetGCMeta_GCIO(hGCT)));
                      return NULL;
                    }
                  }
                }
                if( (i= _findFieldByName_GCIO(GetSubTypeFields_GCIO(theSubType),kX_GCIO))!=-1 )
                {
                  e= CPLListGet(GetSubTypeFields_GCIO(theSubType),i);
                  if( !(orderedFields= CPLListAppend(orderedFields,(GCField*)CPLListGetData(e))) )
                  {
                    CPLError( CE_Failure, CPLE_OutOfMemory,
                              "failed to arrange Geoconcept subtype '%s.%s' fields list.\n",
                              GetTypeName_GCIO(theClass), GetSubTypeName_GCIO(theSubType));
                    _DestroyHeader_GCIO(&(GetGCMeta_GCIO(hGCT)));
                    return NULL;
                  }
                }
                if( (i= _findFieldByName_GCIO(GetSubTypeFields_GCIO(theSubType),kY_GCIO))!=-1 )
                {
                  e= CPLListGet(GetSubTypeFields_GCIO(theSubType),i);
                  if( !(orderedFields= CPLListAppend(orderedFields,(GCField*)CPLListGetData(e))) )
                  {
                    CPLError( CE_Failure, CPLE_OutOfMemory,
                              "failed to arrange Geoconcept subtype '%s.%s' fields list.\n",
                              GetTypeName_GCIO(theClass), GetSubTypeName_GCIO(theSubType));
                    _DestroyHeader_GCIO(&(GetGCMeta_GCIO(hGCT)));
                    return NULL;
                  }
                }
                if( (i= _findFieldByName_GCIO(GetSubTypeFields_GCIO(theSubType),kXP_GCIO))!=-1 )
                {
                  e= CPLListGet(GetSubTypeFields_GCIO(theSubType),i);
                  if( !(orderedFields= CPLListAppend(orderedFields,(GCField*)CPLListGetData(e))) )
                  {
                    CPLError( CE_Failure, CPLE_OutOfMemory,
                              "failed to arrange Geoconcept subtype '%s.%s' fields list.\n",
                              GetTypeName_GCIO(theClass), GetSubTypeName_GCIO(theSubType));
                    _DestroyHeader_GCIO(&(GetGCMeta_GCIO(hGCT)));
                    return NULL;
                  }
                }
                if( (i= _findFieldByName_GCIO(GetSubTypeFields_GCIO(theSubType),kYP_GCIO))!=-1 )
                {
                  e= CPLListGet(GetSubTypeFields_GCIO(theSubType),i);
                  if( !(orderedFields= CPLListAppend(orderedFields,(GCField*)CPLListGetData(e))) )
                  {
                    CPLError( CE_Failure, CPLE_OutOfMemory,
                              "failed to arrange Geoconcept subtype '%s.%s' fields list.\n",
                              GetTypeName_GCIO(theClass), GetSubTypeName_GCIO(theSubType));
                    _DestroyHeader_GCIO(&(GetGCMeta_GCIO(hGCT)));
                    return NULL;
                  }
                }
                if( (i= _findFieldByName_GCIO(GetSubTypeFields_GCIO(theSubType),kGraphics_GCIO))!=-1 )
                {
                  e= CPLListGet(GetSubTypeFields_GCIO(theSubType),i);
                  if( !(orderedFields= CPLListAppend(orderedFields,(GCField*)CPLListGetData(e))) )
                  {
                    CPLError( CE_Failure, CPLE_OutOfMemory,
                              "failed to arrange Geoconcept subtype '%s.%s' fields list.\n",
                              GetTypeName_GCIO(theClass), GetSubTypeName_GCIO(theSubType));
                    _DestroyHeader_GCIO(&(GetGCMeta_GCIO(hGCT)));
                    return NULL;
                  }
                }
                if( (i= _findFieldByName_GCIO(GetSubTypeFields_GCIO(theSubType),kAngle_GCIO))!=-1 )
                {
                  e= CPLListGet(GetSubTypeFields_GCIO(theSubType),i);
                  if( !(orderedFields= CPLListAppend(orderedFields,(GCField*)CPLListGetData(e))) )
                  {
                    CPLError( CE_Failure, CPLE_OutOfMemory,
                              "failed to arrange Geoconcept subtype '%s.%s' fields list.\n",
                              GetTypeName_GCIO(theClass), GetSubTypeName_GCIO(theSubType));
                    _DestroyHeader_GCIO(&(GetGCMeta_GCIO(hGCT)));
                    return NULL;
                  }
                }
                CPLListDestroy(GetSubTypeFields_GCIO(theSubType));
                SetSubTypeFields_GCIO(theSubType,orderedFields);
              }
            }
          }
        }
      }
    }
  }

  CPLDebug( "GEOCONCEPT",
            "Metadata = (\n"
            "  nb Types : %d\n"
            "  Charset : %s\n"
            "  Delimiter : 0x%x\n"
            "  Unit : %s\n"
            "  Resolution : %g\n"
            "  ZUnit : %s\n"
            "  ZResolution : %g\n"
            "  Quoted-Text : %s\n"
            "  Format : %s\n"
            "  SysCoord : %d\n"
            ")\n",
            CPLListCount(GetMetaTypes_GCIO(Meta)),
            GetMetaCharset_GCIO(Meta),
            GetMetaDelimiter_GCIO(Meta),
            GetMetaUnit_GCIO(Meta),
            GetMetaResolution_GCIO(Meta),
            GetMetaZUnit_GCIO(Meta),
            GetMetaZResolution_GCIO(Meta),
            GetMetaQuotedText_GCIO(Meta)? "yes":"no",
            GetMetaFormat_GCIO(Meta)==1? "relative":"absolute",
            GetMetaSysCoord_GCIO(Meta));

  return Meta;
}/* ReadConfig_GCIO */

/* -------------------------------------------------------------------- */
GCExportFileH GCIOAPI_CALL1(*) WriteHeader_GCIO (
                                                  GCExportFileH* H
                                                )
{
  GCExportFileMetadata* Meta;
  int nT, iT, nS, iS, nF, iF;
  GCField* theField;
  GCSubType* theSubType;
  GCType* theClass;
  CPLList* e;
  FILE* gc;

  /* FIXME : howto change default values ? */
  Meta= GetGCMeta_GCIO(H);
  gc= GetGCHandle_GCIO(H);
  fprintf(gc,"%s%s \"%c\"\n", kPragma_GCIO, kMetadataDELIMITER_GCIO, GetMetaDelimiter_GCIO(Meta));
  fprintf(gc,"%s%s \"%s\"\n", kPragma_GCIO, kMetadataQUOTEDTEXT_GCIO, GetMetaQuotedText_GCIO(Meta)? "yes":"no");
  fprintf(gc,"%s%s %s\n", kPragma_GCIO, kMetadataCHARSET_GCIO, GetMetaCharset_GCIO(Meta));
  fprintf(gc,"%s%s Distance=%s\n", kPragma_GCIO, kMetadataUNIT_GCIO, GetMetaUnit_GCIO(Meta));
  fprintf(gc,"%s%s %d\n", kPragma_GCIO, kMetadataFORMAT_GCIO, GetMetaFormat_GCIO(Meta));
  fprintf(gc,"%s%s {Type: %d}\n", kPragma_GCIO, kMetadataSYSCOORD_GCIO, GetMetaSysCoord_GCIO(Meta));

  if( (nT= CPLListCount(GetMetaTypes_GCIO(Meta)))>0 )
  {
    for (iT= 0; iT<nT; iT++)
    {
      if( (e= CPLListGet(GetMetaTypes_GCIO(Meta),iT)) )
      {
        if( (theClass= (GCType*)CPLListGetData(e)) )
        {
          fprintf(gc,"%s%s Class=%s;", kPragma_GCIO, kMetadataFIELDS_GCIO, GetTypeName_GCIO(theClass));
          if( (nS= CPLListCount(GetTypeSubtypes_GCIO(theClass)))>0 )
          {
            for (iS= 0; iS<nS; iS++)
            {
              if( (e= CPLListGet(GetTypeSubtypes_GCIO(theClass),iS)) )
              {
                if( (theSubType= (GCSubType*)CPLListGetData(e)) )
                {
                  fprintf(gc,"Subclass=%s;Kind=%d;Fields=",
                             GetSubTypeName_GCIO(theSubType),
                             (int)GetSubTypeKind_GCIO(theSubType));
                  if( (nF= CPLListCount(GetSubTypeFields_GCIO(theSubType)))>0 )
                  {
                    for (iF= 0; iF<nF; iF++)
                    {
                      if( (e= CPLListGet(GetSubTypeFields_GCIO(theSubType),iF)) )
                      {
                        if( (theField= (GCField*)CPLListGetData(e)) )
                        {
                          if (iF>0) fprintf(gc,"%c", GetMetaDelimiter_GCIO(Meta));
                          if( IsPrivateField_GCIO(theField) )
                          {
                            fprintf(gc,"%s%s", kPrivate_GCIO, GetFieldName_GCIO(theField)+1);
                          }
                          else
                          {
                            fprintf(gc,"%s%s", kPublic_GCIO, GetFieldName_GCIO(theField));
                          }
                        }
                      }
                    }
                  }
                  fprintf(gc,"\n");
                }
              }
            }
          }
        }
      }
    }
  }

  return H;
}/* WriteHeader_GCIO */

/* -------------------------------------------------------------------- */
GCExportFileMetadata GCIOAPI_CALL1(*) ReadHeader_GCIO (
                                                        GCExportFileH* hGXT
                                                      )
{
  GCExportFileMetadata* Meta;
  char* p, *e;

  if( _get_GCIO(hGXT)==EOF )
  {
    return NULL;
  }
  if( GetGCWhatIs_GCIO(hGXT)!=vPragma_GCIO )
  {
    CPLError( CE_Failure, CPLE_AppDefined,
              "Geoconcept export badly formatted.\n"
              "%s expected.\n",
              kPragma_GCIO);
    return NULL;
  }
  SetGCMeta_GCIO(hGXT, _CreateHeader_GCIO());
  if( (Meta= GetGCMeta_GCIO(hGXT))==NULL )
  {
    return NULL;
  }
  while( _get_GCIO(hGXT)!=EOF )
  {
    if( GetGCWhatIs_GCIO(hGXT)==vComType_GCIO )
    {
      continue;
    }
    if( GetGCWhatIs_GCIO(hGXT)==vPragma_GCIO )
    {
      if( (p= strstr(GetGCCache_GCIO(hGXT),kMetadataDELIMITER_GCIO))!=NULL )
      {
        /* //$DELIMITER "char" */
        p= strchr(p,'"'), p++;
        SetMetaDelimiter_GCIO(Meta,*p);
        continue;
      }
      if( (p= strstr(GetGCCache_GCIO(hGXT),kMetadataQUOTEDTEXT_GCIO))!=NULL )
      {
        /* //$QUOTED-TEXT "char*" */
        p= strchr(p,'"'), p++;
        e= p;
        while( *p!='"' && *p!='\0' ) p++;
        *p= '\0';
        if( EQUAL(e,"no") )
        {
          SetMetaQuotedText_GCIO(Meta,FALSE);
        }
        else
        {
          SetMetaQuotedText_GCIO(Meta,TRUE);
        }
        continue;
      }
      if( (p= strstr(GetGCCache_GCIO(hGXT),kMetadataCHARSET_GCIO))!=NULL )
      {
        /* //$CHARSET char* */
        p+= strlen(kMetadataCHARSET_GCIO);
        while( isspace(*p) ) p++;
        e= p;
        while( isalnum(*p) ) p++;
        *p= '\0';
        SetMetaCharset_GCIO(Meta,e);
        continue;
      }
      if( (p= strstr(GetGCCache_GCIO(hGXT),kMetadataUNIT_GCIO))!=NULL )
      {
        /* //$UNIT Distance=char* */
        p= strchr(p,'='), p++;
        while( isspace(*p) ) p++;
        e= p;
        while( isalnum(*p) ) p++;
        *p= '\0';
        SetMetaUnit_GCIO(Meta,e);
        continue;
      }
      if( (p= strstr(GetGCCache_GCIO(hGXT),kMetadataFORMAT_GCIO))!=NULL )
      {
        /* //$FORMAT 1|2 */
        p+= strlen(kMetadataFORMAT_GCIO);
        while( isspace(*p) ) p++;
        e= p;
        if( *e=='1' )
        {
          SetMetaFormat_GCIO(Meta,1);
        }
        else
        {
          SetMetaFormat_GCIO(Meta,2);
        }
        continue;
      }
      if( (p= strstr(GetGCCache_GCIO(hGXT),kMetadataSYSCOORD_GCIO))!=NULL )
      {
        int v;
        /* //$SYSCOORD {Type: int} [ { TimeZone: TimeZoneValue } ] */
        p= strchr(p,':'), p++;
        while( isspace(*p) ) p++;
        e= p;
        while( isdigit(*p) ) p++;
        *p= '\0';
        if( sscanf(e,"%d",&v)!= 1 )
        {
          _DestroyHeader_GCIO(&(GetGCMeta_GCIO(hGXT)));
          CPLError( CE_Failure, CPLE_AppDefined,
                    "Invalid SRS.\n"
                    "Geoconcept export syntax error at line %ld.\n",
                    GetGCCurrentLinenum_GCIO(hGXT) );
          return NULL;
        }
        SetMetaSysCoord_GCIO(Meta,v);
        continue;
      }
      if( (p= strstr(GetGCCache_GCIO(hGXT),kMetadataFIELDS_GCIO))!=NULL )
      {
        char **kv, **vl, *nm, **fl;
        int whereClass, v, i, n;
        GCType* theClass;
        GCSubType* theSubType;
        GCField* theField;
        /* //$FIELDS Class=char*;Subclass=char*;Kind=1..4;Fields=(Private#)?char*\s((Private#)?char*)* */
        p+= strlen(kMetadataFIELDS_GCIO);
        while( isspace(*p) ) p++;
        kv= CSLTokenizeString2(p,";",0);
        if( !kv || CSLCount(kv)!=4 )
        {
          CSLDestroy(kv);
          _DestroyHeader_GCIO(&(GetGCMeta_GCIO(hGXT)));
          CPLError( CE_Failure, CPLE_AppDefined,
                    "Geoconcept export syntax error at line %ld.\n",
                    GetGCCurrentLinenum_GCIO(hGXT) );
          return NULL;
        }
        /* Class=char* */
        vl= CSLTokenizeString2(kv[0],"=",0);
        if( !vl || CSLCount(vl)!=2 )
        {
          CSLDestroy(vl);
          CSLDestroy(kv);
          _DestroyHeader_GCIO(&(GetGCMeta_GCIO(hGXT)));
          CPLError( CE_Failure, CPLE_AppDefined,
                    "Geoconcept export syntax error at line %ld.\n",
                    GetGCCurrentLinenum_GCIO(hGXT) );
          return NULL;
        }
        if( !EQUAL(vl[0], "Class") )
        {
          CSLDestroy(vl);
          CSLDestroy(kv);
          _DestroyHeader_GCIO(&(GetGCMeta_GCIO(hGXT)));
          CPLError( CE_Failure, CPLE_AppDefined,
                    "'Class' expected.\n"
                    "Geoconcept export syntax error at line %ld.\n",
                    GetGCCurrentLinenum_GCIO(hGXT) );
          return NULL;
        }
        p= vl[1];
        while( isspace(*p) ) p++;
        e= p;
        while( isalnum(*p) ) p++;
        *p= '\0';
        if( (whereClass = _findTypeByName_GCIO(hGXT,e))==-1 )
        {
          if( !(theClass= AddType_GCIO(hGXT,e,-1)) )
          {
            CSLDestroy(vl);
            CSLDestroy(kv);
            _DestroyHeader_GCIO(&(GetGCMeta_GCIO(hGXT)));
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Geoconcept export syntax error at line %ld.\n",
                      GetGCCurrentLinenum_GCIO(hGXT) );
            return NULL;
          }
        }
        else
        {
          theClass= _getType_GCIO(hGXT,whereClass);
        }
        CSLDestroy(vl);
        /* Subclass=char* */
        vl= CSLTokenizeString2(kv[1],"=",0);
        if( !vl || CSLCount(vl)!=2 )
        {
          CSLDestroy(vl);
          CSLDestroy(kv);
          _DestroyHeader_GCIO(&(GetGCMeta_GCIO(hGXT)));
          CPLError( CE_Failure, CPLE_AppDefined,
                    "Geoconcept export syntax error at line %ld.\n",
                    GetGCCurrentLinenum_GCIO(hGXT) );
          return NULL;
        }
        if( !EQUAL(vl[0], "Subclass") )
        {
          CSLDestroy(vl);
          CSLDestroy(kv);
          _DestroyHeader_GCIO(&(GetGCMeta_GCIO(hGXT)));
          CPLError( CE_Failure, CPLE_AppDefined,
                    "'Subclass' expected.\n"
                    "Geoconcept export syntax error at line %ld.\n",
                    GetGCCurrentLinenum_GCIO(hGXT) );
          return NULL;
        }
        p= vl[1];
        while( isspace(*p) ) p++;
        e= p;
        while( isalnum(*p) ) p++;
        *p= '\0';
        if( _findSubTypeByName_GCIO(theClass,e)!=-1 )
        {
          CSLDestroy(vl);
          CSLDestroy(kv);
          _DestroyHeader_GCIO(&(GetGCMeta_GCIO(hGXT)));
          CPLError( CE_Failure, CPLE_AppDefined,
                    "%s already exists.\n"
                    "Geoconcept export syntax error at line %ld.\n",
                    e, GetGCCurrentLinenum_GCIO(hGXT) );
          return NULL;
        }
        nm= CPLStrdup(e);
        CSLDestroy(vl);
        /* Kind=1..4 */
        vl= CSLTokenizeString2(kv[2],"=",0);
        if( !vl || CSLCount(vl)!=2 )
        {
          CSLDestroy(vl);
          CSLDestroy(kv);
          _DestroyHeader_GCIO(&(GetGCMeta_GCIO(hGXT)));
          CPLError( CE_Failure, CPLE_AppDefined,
                    "Geoconcept export syntax error at line %ld.\n",
                    GetGCCurrentLinenum_GCIO(hGXT) );
          return NULL;
        }
        if( !EQUAL(vl[0], "Kind") )
        {
          CPLFree(nm);
          CSLDestroy(vl);
          CSLDestroy(kv);
          _DestroyHeader_GCIO(&(GetGCMeta_GCIO(hGXT)));
          CPLError( CE_Failure, CPLE_AppDefined,
                    "'Kind' expected.\n"
                    "Geoconcept export syntax error at line %ld.\n",
                    GetGCCurrentLinenum_GCIO(hGXT) );
          return NULL;
        }
        p= vl[1];
        while( isspace(*p) ) p++;
        e= p;
        while( isdigit(*p) ) p++;
        *p= '\0';
        if( sscanf(e,"%d",&v)!= 1 || v<1 || v>4 )
        {
          CPLFree(nm);
          CSLDestroy(vl);
          CSLDestroy(kv);
          _DestroyHeader_GCIO(&(GetGCMeta_GCIO(hGXT)));
          CPLError( CE_Failure, CPLE_AppDefined,
                    "Invalid Geometry type.\n"
                    "Geoconcept export syntax error at line %ld.\n",
                    GetGCCurrentLinenum_GCIO(hGXT) );
          return NULL;
        }
        CSLDestroy(vl);
        if( !(theSubType= AddSubType_GCIO(hGXT,GetTypeName_GCIO(theClass),
                                               nm,
                                               -1,
                                               (GCTypeKind)v,
                                               vUnknown3D_GCIO)) )
        {
          CPLFree(nm);
          CSLDestroy(kv);
          _DestroyHeader_GCIO(&(GetGCMeta_GCIO(hGXT)));
          CPLError( CE_Failure, CPLE_AppDefined,
                    "Geoconcept export syntax error at line %ld.\n",
                    GetGCCurrentLinenum_GCIO(hGXT) );
          return NULL;
        }
        CPLFree(nm);
        /* Fields=(Private#)?char*\s((Private#)?char*)* */
        vl= CSLTokenizeString2(kv[3],"=",0);
        CSLDestroy(kv);
        if( !vl || CSLCount(vl)!=2 )
        {
          CSLDestroy(vl);
          _DestroyHeader_GCIO(&(GetGCMeta_GCIO(hGXT)));
          CPLError( CE_Failure, CPLE_AppDefined,
                    "Geoconcept export syntax error at line %ld.\n",
                    GetGCCurrentLinenum_GCIO(hGXT) );
          return NULL;
        }
        if( !EQUAL(vl[0], "Fields") )
        {
          CSLDestroy(vl);
          _DestroyHeader_GCIO(&(GetGCMeta_GCIO(hGXT)));
          CPLError( CE_Failure, CPLE_AppDefined,
                    "'Fields' expected.\n"
                    "Geoconcept export syntax error at line %ld.\n",
                    GetGCCurrentLinenum_GCIO(hGXT) );
          return NULL;
        }
        fl= CSLTokenizeString2(vl[1]," 	",CSLT_HONOURSTRINGS);
        CSLDestroy(vl);
        if( !fl || (n= CSLCount(fl))==0 )
        {
          CSLDestroy(fl);
          _DestroyHeader_GCIO(&(GetGCMeta_GCIO(hGXT)));
          CPLError( CE_Failure, CPLE_AppDefined,
                    "Geoconcept export syntax error at line %ld.\n",
                    GetGCCurrentLinenum_GCIO(hGXT) );
          return NULL;
        }
        for (i= 0; i<n; i++)
        {
          p= fl[i];
          while( isspace(*p) ) p++;
          e= p;
          if( EQUALN(p,kPrivate_GCIO,strlen(kPrivate_GCIO)) )
          {
            p+= strlen(kPrivate_GCIO);
            e= p-1, *e= '@';
          }
          while( isalnum(*p) ) p++;
          *p= '\0';
          nm= CPLStrdup(e);
          if( (theField= AddSubTypeField_GCIO(hGXT,GetTypeName_GCIO(theClass),
                                                   GetSubTypeName_GCIO(theSubType),
                                                   -1,
                                                   nm,
                                                   -1,
                                                   vUnknownItemType_GCIO,
                                                   NULL,
                                                   NULL))==NULL )
          {
            CPLFree(nm);
            CSLDestroy(fl);
            _DestroyHeader_GCIO(&(GetGCMeta_GCIO(hGXT)));
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Geoconcept export syntax error at line %ld.\n",
                      GetGCCurrentLinenum_GCIO(hGXT) );
            return NULL;
          }
          CPLFree(nm);
        }
        CSLDestroy(fl);
        continue;
      }
      /* end of definitions ... */ /* FIXME */
      if( (p= strstr(GetGCCache_GCIO(hGXT),k3DOBJECTMONO_GCIO)) ||
          (p= strstr(GetGCCache_GCIO(hGXT),k3DOBJECT_GCIO))     ||
          (p= strstr(GetGCCache_GCIO(hGXT),k2DOBJECT_GCIO)) )
        break;
      /* unknown pragma ... */
      continue;
    }
    /* neither a comment, nor a pragma ... */
    /* next reading will be in cache ! */
    SetGCStatus_GCIO(hGXT,vMemoStatus_GCIO);
    break;
  }
  if( CPLListCount(GetMetaTypes_GCIO(Meta))==0 )
  {
    _DestroyHeader_GCIO(&(GetGCMeta_GCIO(hGXT)));
    CPLError( CE_Failure, CPLE_AppDefined,
              "Geoconcept export syntax error at line %ld.\n",
              GetGCCurrentLinenum_GCIO(hGXT) );
    return NULL;
  }
  SetMetaExtent_GCIO(GetGCMeta_GCIO(hGXT), _CreateExtent_GCIO(HUGE_VAL,HUGE_VAL,-HUGE_VAL,-HUGE_VAL));
  // TODO : read up to the end to count feature and compute extent ?
#if 1
  while( _get_GCIO(hGXT)!=EOF )
  {
    if( GetGCWhatIs_GCIO(hGXT)==vComType_GCIO )
    {
      continue;
    }
    /* analyze the line according to schema */
    if( GetGCWhatIs_GCIO(hGXT)==vPragma_GCIO )
    {
      continue;
    }
    SetGCNbObjects_GCIO(hGXT,GetGCNbObjects_GCIO(hGXT)+1L);
    /* find out geometry ...                */
    ;
  }
#else
  VSIFSeek(hGXT->H,0L,SEEK_END);
#endif /* 0 */

  CPLDebug( "GEOCONCEPT",
            "Metadata = (\n"
            "  nb Types : %d\n"
            "  Charset : %s\n"
            "  Delimiter : 0x%x\n"
            "  Unit : %s\n"
            "  Resolution : %g\n"
            "  ZUnit : %s\n"
            "  ZResolution : %g\n"
            "  Quoted-Text : %s\n"
            "  Format : %s\n"
            "  SysCoord : %d\n"
            ")\n",
            CPLListCount(GetMetaTypes_GCIO(Meta)),
            GetMetaCharset_GCIO(Meta),
            GetMetaDelimiter_GCIO(Meta),
            GetMetaUnit_GCIO(Meta),
            GetMetaResolution_GCIO(Meta),
            GetMetaZUnit_GCIO(Meta),
            GetMetaZResolution_GCIO(Meta),
            GetMetaQuotedText_GCIO(Meta)? "yes":"no",
            GetMetaFormat_GCIO(Meta)==1? "relative":"absolute",
            GetMetaSysCoord_GCIO(Meta));

  return Meta;
}/* ReadHeader_GCIO */

/* -------------------------------------------------------------------- */
GCSubType GCIOAPI_CALL1(*) FindFeature_GCIO ( 
                                              GCExportFileH* hGCT,
                                              const char* typDOTsubtypName
                                            )
{
  char **fe;
  int whereClass, whereSubType;
  GCType* theClass;
  GCSubType* theSubType;
  if( hGCT==NULL ) return NULL;
  if( typDOTsubtypName==NULL ) return NULL;
  if( !(fe= CSLTokenizeString2(typDOTsubtypName,".",0)) ||
      CSLCount(fe)!=2 )
  {
    CSLDestroy(fe);
    return NULL;
  }
  if( (whereClass= _findTypeByName_GCIO(hGCT,fe[0]))==-1 )
  {
    CSLDestroy(fe);
    return NULL;
  }
  theClass= _getType_GCIO(hGCT,whereClass);
  if( (whereSubType= _findSubTypeByName_GCIO(theClass,fe[1]))==-1 )
  {
    CSLDestroy(fe);
    return NULL;
  }
  theSubType= _getSubType_GCIO(theClass,whereSubType);
  CSLDestroy(fe);
  return theSubType;
}/* FindFeature_GCIO */

/* -------------------------------------------------------------------- */
GCField GCIOAPI_CALL1(*) FindFeatureField_GCIO (
                                                 GCSubType* theSubType,
                                                 const char *fieldName
                                               )
{
  int whereField;
  GCField* theField;
  if( theSubType==NULL ) return NULL;
  if( fieldName==NULL ) return NULL;
  if( (whereField= _findFieldByName_GCIO(GetSubTypeFields_GCIO(theSubType),fieldName))==-1 )
  {
    return NULL;
  }
  theField= _getField_GCIO(GetSubTypeFields_GCIO(theSubType),whereField);
  return theField;
}/* FindFeatureField_GCIO */

/* -------------------------------------------------------------------- */
static char GCIOAPI_CALL1(*) _escapeString_GCIO (
                                                  GCExportFileH* H,
                                                  const char *theString
                                                )
{
  int l, i, o;
  char *res;
  if( !GetMetaQuotedText_GCIO(GetGCMeta_GCIO(H)) ||
      !theString ||
      strchr(theString,'"')==NULL ||
      (l= strlen(theString))==0 )
  {
    return CPLStrdup(theString);
  }
  if( (res= (char *)CPLMalloc(l*2)) )
  {
    for (i= 0, o= 0; i<l; i++, o++)
    {
      if( theString[i]=='"' )
      {
        res[o]= '\\';
        o++;
      }
      res[o]= theString[i];
    }
  }
  return res;
}/* _escapeString_GCIO */

/* -------------------------------------------------------------------- */
static int GCIOAPI_CALL _findNextFeatureField_GCIO (
                                                     GCExportFileH* H,
                                                     GCSubType* theSubType,
                                                     int from,
                                                     long id
                                                   )
{
  FILE *h;
  int n, i;
  GCField* theField;
  char* fieldName, *quotes, *escapedValue, delim;

  if( (n= CountSubTypeFields_GCIO(theSubType))==0 )
  {
    return WRITECOMPLETED_GCIO;
  }
  if( !(from<n) )
  {
    return WRITECOMPLETED_GCIO;
  }

  h= GetGCHandle_GCIO(H);
  /* Dimension pragma for 3DM et 3D : */
  if( GetSubTypeDim_GCIO(theSubType)==v3DM_GCIO )
  {
    if( VSIFPrintf(h,"%s%s\n", kPragma_GCIO, k3DOBJECTMONO_GCIO)<=0 )
    {
      CPLError( CE_Failure, CPLE_AppDefined, "Write failed.\n");
      return WRITEERROR_GCIO;
    }
    SetGCCurrentLinenum_GCIO(H,GetGCCurrentLinenum_GCIO(H)+1L);
  }
  else if( GetSubTypeDim_GCIO(theSubType)==v3D_GCIO )
  {
    if( VSIFPrintf(h,"%s%s\n", kPragma_GCIO, k3DOBJECT_GCIO)<=0 )
    {
      CPLError( CE_Failure, CPLE_AppDefined, "Write failed.\n");
      return WRITEERROR_GCIO;
    }
    SetGCCurrentLinenum_GCIO(H,GetGCCurrentLinenum_GCIO(H)+1L);
  }
  if( GetMetaQuotedText_GCIO(GetGCMeta_GCIO(H)) )
  {
    quotes= "\"";
  }
  else
  {
    quotes= "";
  }
  delim= GetMetaDelimiter_GCIO(GetGCMeta_GCIO(H));
  /* Fields are written in the same order as in the sub-type definition.           */
  /* Check for Private# fields :                                                   */
  for (i= from; i<n; i++)
  {
    theField= GetSubTypeField_GCIO(theSubType,i);
    if( !IsPrivateField_GCIO(theField) )
    {
      return i;/* needs a call to WriteFeatureField_GCIO() for the ith field */
    }
    fieldName= GetFieldName_GCIO(theField);
    if( EQUAL(fieldName,kX_GCIO)        ||
        EQUAL(fieldName,kY_GCIO)        ||
        EQUAL(fieldName,kXP_GCIO)       ||
        EQUAL(fieldName,kYP_GCIO)       ||
        EQUAL(fieldName,kGraphics_GCIO) ||
        EQUAL(fieldName,kAngle_GCIO)    )
    {
      return GEOMETRYEXPECTED_GCIO;/* needs a call to WriteFeatureGeometry_GCIO() now */
    }
    if( EQUAL(fieldName,kIdentifier_GCIO) ) /* FIXME : must be the first field ? */
    {
      /* long integer which GeoConcept may use as a key for the object it will create. */
      /* If set to '-1', it will be ignored.                                           */
      if( VSIFPrintf(h,"%s%d%s", quotes, id, quotes)<=0 )
      {
        CPLError( CE_Failure, CPLE_AppDefined, "Write failed.\n");
        return WRITEERROR_GCIO;
      }
    }
    else if( EQUAL(fieldName,kClass_GCIO) ) /* FIXME : must be the second field ? */
    {
      if( !(escapedValue= _escapeString_GCIO(H,GetTypeName_GCIO(GetSubTypeType_GCIO(theSubType)))) )
      {
        return WRITEERROR_GCIO;
      }
      if( VSIFPrintf(h,"%s%s%s", quotes, escapedValue, quotes)<=0 )
      {
        CPLError( CE_Failure, CPLE_AppDefined, "Write failed.\n");
        return WRITEERROR_GCIO;
      }
      CPLFree(escapedValue);
    }
    else if( EQUAL(fieldName,kSubclass_GCIO) ) /* FIXME : must be the third field ? */
    {
      if( !(escapedValue= _escapeString_GCIO(H,GetSubTypeName_GCIO(theSubType))) )
      {
        return WRITEERROR_GCIO;
      }
      if( VSIFPrintf(h,"%s%s%s", quotes, escapedValue, quotes)<=0 )
      {
        CPLError( CE_Failure, CPLE_AppDefined, "Write failed.\n");
        return WRITEERROR_GCIO;
      }
      CPLFree(escapedValue);
    }
    else if( EQUAL(fieldName,kName_GCIO) )
    {
      /* FIXME */
      if( !(escapedValue= _escapeString_GCIO(H,GetSubTypeName_GCIO(theSubType))) )
      {
        return WRITEERROR_GCIO;
      }
      if( VSIFPrintf(h,"%s%s%s", quotes, escapedValue, quotes)<=0 )
      {
        CPLError( CE_Failure, CPLE_AppDefined, "Write failed.\n");
        return WRITEERROR_GCIO;
      }
      CPLFree(escapedValue);
    }
    else if( EQUAL(fieldName,kNbFields_GCIO) )
    {
      if( VSIFPrintf(h,"%s%d%s", quotes, GetSubTypeNbFields_GCIO(theSubType), quotes)<=0 )
      {
        CPLError( CE_Failure, CPLE_AppDefined, "Write failed.\n");
        return WRITEERROR_GCIO;
      }
    }
    else
    {
      CPLError( CE_Failure, CPLE_NotSupported,
                "Writing %s field is not implemented.\n",
                fieldName );
      return WRITEERROR_GCIO;
    }
    if( i!=n-1 )
    {
      if( VSIFPrintf(h,"%c", delim)<=0 )
      {
        CPLError( CE_Failure, CPLE_AppDefined, "Write failed.\n");
        return WRITEERROR_GCIO;
      }
    }
  }

  return WRITECOMPLETED_GCIO;
}/* _findNextFeatureField_GCIO */

/* -------------------------------------------------------------------- */
int GCIOAPI_CALL StartWritingFeature_GCIO (
                                            GCExportFileH* H,
                                            GCSubType* theSubType,
                                            long id
                                          )
{
  return _findNextFeatureField_GCIO(H,theSubType,0,id);
}/* StartWritingFeature_GCIO */

/* -------------------------------------------------------------------- */
static int GCIOAPI_CALL _writePoint_GCIO (
                                           FILE* h,
                                           const char* quotes,
                                           char delim,
                                           double x, double y, double z,
                                           GCDim dim,
                                           GCExtent* e
                                         )
{
  SetExtentULAbscissa_GCIO(e,x);
  SetExtentULOrdinate_GCIO(e,y);
  SetExtentLRAbscissa_GCIO(e,x);
  SetExtentLROrdinate_GCIO(e,y);
  if( dim==v3DM_GCIO || dim==v3D_GCIO )
  {
    if( VSIFPrintf(h,"%s%g%s%c%s%g%s%c%s%g%s", quotes, x, quotes,
                                               delim,
                                               quotes, y, quotes,
                                               delim,
                                               quotes, z, quotes)<=0 )
    {
      CPLError( CE_Failure, CPLE_AppDefined, "Write failed.\n");
      return FALSE;
    }
  }
  else
  {
    if( VSIFPrintf(h,"%s%g%s%c%s%g%s", quotes, x, quotes,
                                       delim,
                                       quotes, y, quotes)<=0 )
    {
      CPLError( CE_Failure, CPLE_AppDefined, "Write failed.\n");
      return FALSE;
    }
  }
  return TRUE;
}/* _writePoint_GCIO */

/* -------------------------------------------------------------------- */
static int GCIOAPI_CALL _writeLine_GCIO (
                                           FILE* h,
                                           const char* quotes,
                                           char delim,
                                           OGRGeometryH* poArc,
                                           GCTypeKind knd,
                                           GCDim dim,
                                           int fmt,
                                           GCExtent* e
                                        )
{
  int iP, nP;
  double dX, dY, dZ;
  /* 1st point */
  if( !_writePoint_GCIO(h,quotes,delim,
                          OGR_G_GetX(poArc,0),
                          OGR_G_GetY(poArc,0),
                          OGR_G_GetZ(poArc,0),
                          dim,e) )
  {
    return FALSE;
  }
  if( VSIFPrintf(h,"%c", delim)<=0 )
  {
    CPLError( CE_Failure, CPLE_AppDefined, "Write failed.\n");
    return FALSE;
  }
  nP= OGR_G_GetPointCount(poArc);
  if( knd==vLine_GCIO )
  {
    /* last point */
    if( !_writePoint_GCIO(h,quotes,delim,
                            OGR_G_GetX(poArc,nP-1),
                            OGR_G_GetY(poArc,nP-1),
                            OGR_G_GetZ(poArc,nP-1),
                            dim,e) )
    {
      return FALSE;
    }
    if( VSIFPrintf(h,"%c", delim)<=0 )
    {
      CPLError( CE_Failure, CPLE_AppDefined, "Write failed.\n");
      return FALSE;
    }
  }
  /* number of remaining points : */
  if( VSIFPrintf(h,"%s%d%s%c", quotes, nP-1, quotes, delim)<=0 )
  {
    CPLError( CE_Failure, CPLE_AppDefined, "Write failed.\n");
    return FALSE;
  }
  /* 2nd up to the last point ... */
  for( iP= 1; iP<nP; iP++ )
  {
    if( fmt==1 )
    { /* relative coordinates ... */
      dX= OGR_G_GetX(poArc,iP-1) - OGR_G_GetX(poArc,iP);
      dY= OGR_G_GetY(poArc,iP-1) - OGR_G_GetY(poArc,iP);
      dZ= OGR_G_GetZ(poArc,iP-1) - OGR_G_GetZ(poArc,iP);
    }
    else
    { /* absolute coordinates ... */
      dX= OGR_G_GetX(poArc,iP);
      dY= OGR_G_GetY(poArc,iP);
      dZ= OGR_G_GetZ(poArc,iP);
    }
    if( !_writePoint_GCIO(h,quotes,delim,
                            dX,
                            dY,
                            dZ,
                            dim,e) )
    {
      return FALSE;
    }
    if( iP!=nP-1 )
    {
      if( VSIFPrintf(h,"%c", delim)<=0 )
      {
        CPLError( CE_Failure, CPLE_AppDefined, "Write failed.\n");
        return FALSE;
      }
    }
  }
  return TRUE;
}/* _writeLine_GCIO */

/* -------------------------------------------------------------------- */
static int GCIOAPI_CALL _writePolygon_GCIO (
                                             FILE* h,
                                             const char* quotes,
                                             char delim,
                                             OGRGeometryH* poPoly,
                                             GCDim dim,
                                             int fmt,
                                             GCExtent* e
                                           )
{
  int iR, nR;
  OGRGeometryH* poRing;
  /*
   * X<>Y[<>Z]{Single Polygon{<>NrPolys=j[<>X<>Y[<>Z]<>Single Polygon]j}}
   * with :
   * Single Polygon = Nr points=k[<>PointX<>PointY[<>Z]]k...
   */
  if( (nR= OGR_G_GetGeometryCount(poPoly))==0 )
  {
    CPLError( CE_Warning, CPLE_AppDefined,
              "Ignore POLYGON EMPTY in Geoconcept writer.\n" );
    return TRUE;
  }
  poRing= OGR_G_GetGeometryRef(poPoly,0);
  if( !_writeLine_GCIO(h,quotes,delim,poRing,vPoly_GCIO,dim,fmt,e) )
  {
    return FALSE;
  }
  /* number of interior rings : */
  if( nR>1 )
  {
    if( VSIFPrintf(h,"%c%d%c", delim, nR-1, delim)<=0 )
    {
      CPLError( CE_Failure, CPLE_AppDefined, "Write failed.\n");
      return FALSE;
    }
    for( iR= 1; iR<nR; iR++ )
    {
      poRing= OGR_G_GetGeometryRef(poPoly,iR);
      if( !_writeLine_GCIO(h,quotes,delim,poRing,vPoly_GCIO,dim,fmt,e) )
      {
        return FALSE;
      }
      if( iR!=nR-1 )
      {
        if( VSIFPrintf(h,"%c", delim)<=0 )
        {
          CPLError( CE_Failure, CPLE_AppDefined, "Write failed.\n");
          return FALSE;
        }
      }
    }
  }
  return TRUE;
}/* _writePolygon_GCIO */

/* -------------------------------------------------------------------- */
int GCIOAPI_CALL WriteFeatureGeometry_GCIO (
                                             GCExportFileH* H,
                                             GCSubType* theSubType,
                                             OGRGeometryH poGeom
                                           )
{
  FILE *h;
  int n, i, iAn;
  char *quotes, delim;

  h= GetGCHandle_GCIO(H);
  n= CountSubTypeFields_GCIO(theSubType);
  iAn= -1;
  if( (i= _findFieldByName_GCIO(GetSubTypeFields_GCIO(theSubType),kGraphics_GCIO))==-1 )
  {
    if( (i= _findFieldByName_GCIO(GetSubTypeFields_GCIO(theSubType),kAngle_GCIO))==-1 )
    {
      i= _findFieldByName_GCIO(GetSubTypeFields_GCIO(theSubType),kY_GCIO);
    }
    else
    {
      iAn= i;
    }
  }

  if( GetMetaQuotedText_GCIO(GetGCMeta_GCIO(H)) )
  {
    quotes= "\"";
  }
  else
  {
    quotes= "";
  }
  delim= GetMetaDelimiter_GCIO(GetGCMeta_GCIO(H));

  switch( OGR_G_GetGeometryType(poGeom) ) {
  case wkbPoint                 :
  case wkbPoint25D              :
    if( !_writePoint_GCIO(h,quotes,delim,
                            OGR_G_GetX(poGeom,0),
                            OGR_G_GetY(poGeom,0),
                            OGR_G_GetZ(poGeom,0),
                            GetSubTypeDim_GCIO(theSubType),
                            GetMetaExtent_GCIO(GetGCMeta_GCIO(H))) )
    {
      return WRITEERROR_GCIO;
    }
    break;
  case wkbLineString            :
  case wkbLineString25D         :
    if( !_writeLine_GCIO(h,quotes,delim,
                           poGeom,
                           vLine_GCIO,
                           GetSubTypeDim_GCIO(theSubType),
                           GetMetaFormat_GCIO(GetGCMeta_GCIO(H)),
                           GetMetaExtent_GCIO(GetGCMeta_GCIO(H))) )
    {
      return WRITEERROR_GCIO;
    }
    break;
  case wkbPolygon               :
  case wkbPolygon25D            :
    if( !_writePolygon_GCIO(h,quotes,delim,
                              poGeom,
                              GetSubTypeDim_GCIO(theSubType),
                              GetMetaFormat_GCIO(GetGCMeta_GCIO(H)),
                              GetMetaExtent_GCIO(GetGCMeta_GCIO(H))) )
    {
      return WRITEERROR_GCIO;
    }
    break;
  case wkbMultiPoint            :
  case wkbMultiPoint25D         :
  case wkbMultiLineString       :
  case wkbMultiLineString25D    :
  case wkbMultiPolygon          :
  case wkbMultiPolygon25D       :
  case wkbUnknown               :
  case wkbGeometryCollection    :
  case wkbGeometryCollection25D :
  case wkbNone                  :
  case wkbLinearRing            :
  default                       :
    CPLError( CE_Warning, CPLE_AppDefined,
              "Geometry type %d not supported in Geoconcept, feature skipped.\n",
              OGR_G_GetGeometryType(poGeom) );
    break;
  }
  /* Angle= 0 !! */
  if( iAn!=-1 )
  {
    if( VSIFPrintf(h,"%c%s%1d%s", delim, quotes, 0, quotes)<=0 )
    {
      CPLError( CE_Failure, CPLE_AppDefined, "Write failed.\n");
      return WRITEERROR_GCIO;
    }
  }
  /* if it is not the last field ... */
  if( i!=n-1 )
  {
    if( VSIFPrintf(h,"%c", delim)<=0 )
    {
      CPLError( CE_Failure, CPLE_AppDefined, "Write failed.\n");
      return WRITEERROR_GCIO;
    }
  }

  /* find out next field to write ... */
  return _findNextFeatureField_GCIO(H,theSubType,i+1,OGRNullFID);
}/* WriteFeatureGeometry_GCIO */

/* -------------------------------------------------------------------- */
int GCIOAPI_CALL WriteFeatureFieldAsString_GCIO (
                                                  GCExportFileH* H,
                                                  GCSubType* theSubType,
                                                  int iField,
                                                  const char* theValue
                                                )
{
  FILE *h;
  int n;
  char *quotes, *escapedValue, delim;
  GCField* theField;

  h= GetGCHandle_GCIO(H);
  n= CountSubTypeFields_GCIO(theSubType);
  if( GetMetaQuotedText_GCIO(GetGCMeta_GCIO(H)) )
  {
    quotes= "\"";
  }
  else
  {
    quotes= "";
  }
  delim= GetMetaDelimiter_GCIO(GetGCMeta_GCIO(H));
  if( !(theField= GetSubTypeField_GCIO(theSubType,iField)) )
  {
    CPLError( CE_Failure, CPLE_NotSupported,
              "Attempt to write a field #%d that does not exist on feature %s.%s.\n",
              iField,
              GetTypeName_GCIO(GetSubTypeType_GCIO(theSubType)),
              GetSubTypeName_GCIO(theSubType) );
    return WRITEERROR_GCIO;
  }
  if( !(escapedValue= _escapeString_GCIO(H,theValue)) )
  {
    return WRITEERROR_GCIO;
  }
  if( VSIFPrintf(h,"%s%s%s", quotes, escapedValue, quotes)<=0 )
  {
    /* it is really an error if one of the parameters is not empty ... */
    if( *quotes!='\0' || *escapedValue!='\0')
    {
      CPLError( CE_Failure, CPLE_AppDefined, "Write failed.\n");
      return WRITEERROR_GCIO;
    }
  }
  if( iField!=n-1 )
  {
    if( VSIFPrintf(h,"%c", delim)<=0 )
    {
      CPLError( CE_Failure, CPLE_AppDefined, "Write failed.\n");
      return WRITEERROR_GCIO;
    }
  }
  CPLFree(escapedValue);

  return _findNextFeatureField_GCIO(H,theSubType,iField+1,OGRNullFID);
}/* WriteFeatureFieldAsString_GCIO */

/* -------------------------------------------------------------------- */
void GCIOAPI_CALL StopWritingFeature_GCIO (
                                            GCExportFileH* H,
                                            GCSubType* theSubType
                                          )
{
  if( VSIFPrintf(GetGCHandle_GCIO(H),"\n")<=0 )
  {
    CPLError( CE_Failure, CPLE_AppDefined, "Write failed.\n");
  }
  SetGCNbObjects_GCIO(H, GetGCNbObjects_GCIO(H)+1L);
  SetGCCurrentLinenum_GCIO(H,GetGCCurrentLinenum_GCIO(H)+1L);
}/* StopWritingFeature_GCIO */

/* -------------------------------------------------------------------- */
