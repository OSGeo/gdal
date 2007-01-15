/*-*-C++-*-*/
/******************************************************************************
 * $Id$
 *
 * Project:  TIGER/Line Translator
 * Purpose:  Main declarations for Tiger translator.
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
 ******************************************************************************
 *
 * $Log$
 * Revision 1.23  2006/03/29 00:46:20  fwarmerdam
 * update contact info
 *
 * Revision 1.22  2006/03/08 04:17:24  fwarmerdam
 * added logic to check RTC records when classifying version
 *
 * Revision 1.21  2005/04/06 16:05:29  fwarmerdam
 * added spatialmetadata (RTM) support
 *
 * Revision 1.20  2005/04/06 15:04:23  fwarmerdam
 * added TIGER2004 support
 *
 * Revision 1.19  2005/02/22 12:50:22  fwarmerdam
 * use OGRLayer base spatial filter support
 *
 * Revision 1.18  2005/02/15 02:24:45  fwarmerdam
 * fixed GetFeatureCount() when filters in place
 *
 * Revision 1.17  2004/02/12 06:39:16  warmerda
 * added preliminary support for some GDT quirks
 *
 * Revision 1.16  2004/01/13 17:23:49  warmerda
 * recover more gracefully from RT2 open errors
 *
 * Revision 1.15  2003/09/11 22:47:53  aamici
 * add class constructors and destructors where needed in order to
 * let the mingw/cygwin binutils produce sensible partially linked objet files
 * with 'ld -r'.
 *
 * Revision 1.14  2003/08/18 14:47:53  warmerda
 * upgraded with *untested* TIGER 2003 support
 *
 * Revision 1.13  2003/01/11 15:29:55  warmerda
 * expanded tabs
 *
 * Revision 1.12  2003/01/04 23:21:56  mbp
 * Minor bug fixes and field definition changes.  Cleaned
 * up and commented code written for TIGER 2002 support.
 *
 * Revision 1.11  2002/12/26 00:20:19  mbp
 * re-organized code to hold TIGER-version details in TigerRecordInfo structs;
 * first round implementation of TIGER_2002 support
 *
 * Revision 1.10  2001/07/19 16:05:49  warmerda
 * clear out tabs
 *
 * Revision 1.9  2001/07/19 13:26:32  warmerda
 * enable override of existing modules
 *
 * Revision 1.8  2001/07/04 23:25:32  warmerda
 * first round implementation of writer
 *
 * Revision 1.7  2001/07/04 05:40:35  warmerda
 * upgraded to support FILE, and Tiger2000 schema
 *
 * Revision 1.6  2001/01/19 21:15:20  warmerda
 * expanded tabs
 *
 * Revision 1.5  2000/01/13 05:18:11  warmerda
 * added support for multiple versions
 *
 * Revision 1.4  1999/12/22 15:38:15  warmerda
 * major update
 *
 * Revision 1.3  1999/12/15 19:59:52  warmerda
 * added new file types
 *
 * Revision 1.2  1999/11/04 21:14:31  warmerda
 * various improvements, and TestCapability()
 *
 * Revision 1.1  1999/10/07 18:19:21  warmerda
 * New
 */

#ifndef _OGR_TIGER_H_INCLUDED
#define _OGR_TIGER_H_INCLUDED

#include "cpl_conv.h"
#include "ogrsf_frmts.h"

class OGRTigerDataSource;

/*
** TIGER Versions
**
** 0000           TIGER/Line Precensus Files, 1990
** 0002           TIGER/Line Initial Voting District Codes Files, 1990
** 0003           TIGER/Line Files, 1990
** 0005           TIGER/Line Files, 1992
** 0021           TIGER/Line Files, 1994
** 0024           TIGER/Line Files, 1995
** 0697 to 1098   TIGER/Line Files, 1997
** 1298 to 0499   TIGER/Line Files, 1998
** 0600 to 0800   TIGER/Line Files, 1999
** 1000 to 1100   TIGER/Line Files, Redistricting Census 2000
** 0301 to 0801   TIGER/Line Files, Census 2000
**
** 0302 to 0502   TIGER/Line Files, UA 2000
** ????    ????
**
** 0602  & higher TIGER/Line Files, 2002
** ????    ????
*/

typedef enum {
    TIGER_1990_Precensus = 0,
    TIGER_1990 = 1,
    TIGER_1992 = 2,
    TIGER_1994 = 3,
    TIGER_1995 = 4,
    TIGER_1997 = 5,
    TIGER_1998 = 6,
    TIGER_1999 = 7,
    TIGER_2000_Redistricting = 8,
    TIGER_2000_Census = 9,
    TIGER_UA2000 = 10,
    TIGER_2002 = 11,
    TIGER_2003 = 12,
    TIGER_2004 = 13,
    TIGER_Unknown
} TigerVersion;

TigerVersion TigerClassifyVersion( int );
char * TigerVersionString( TigerVersion );

/*****************************************************************************/
/* The TigerFieldInfo and TigerRecordInfo structures hold information about  */
/* the schema of a TIGER record type.  In each layer implementation file     */
/* there are statically initalized variables of these types that describe    */
/* the record types associated with that layer.  In the case where different */
/* TIGER versions have different schemas, there is a                         */
/* TigerFieldInfo/TigerRecordInfo for each version, and the constructor      */
/* for the layer chooses a pointer to the correct set based on the version.  */
/*****************************************************************************/

typedef struct TigerFieldInfo {
  char         *pszFieldName;   // name of the field
  char          cFmt;           // format of the field ('L' or 'R')
  char          cType;          // type of the field ('A' or 'N')
  OGRFieldType  OGRtype;        // OFTType of the field (OFTInteger, OFTString, ...?)
  int           nBeg;           // beginning column number for field
  int           nEnd;           // ending column number for field
  int           nLen;           // length of field

  int           bDefine;        // whether to add this field to the FeatureDefn
  int           bSet;           // whether to set this field in GetFeature()
  int           bWrite;         // whether to write this field in CreateFeature()
} TigerFieldInfo;

typedef struct TigerRecordInfo {
  TigerFieldInfo *pasFields;
  int             nFieldCount;
  int             nRecordLength;
} TigerRecordInfo;

// OGR_TIGER_RECBUF_LEN should be a number that is larger than the
// longest possible record length for any record type; it's used to
// create arrays to hold the records.  At the time of this writing the
// longest record (RT1) has length 228, but I'm choosing 500 because
// it's a good round number and will allow for growth without having
// to modify this file.  The code never holds more than a few records
// in memory at a time, so having OGR_TIGER_RECBUF_LEN be much larger
// than is really necessary won't affect the amount of memory required
// in a substantial way.
// mbp Fri Dec 20 19:19:59 2002
#define OGR_TIGER_RECBUF_LEN 500

/************************************************************************/
/*                            TigerFileBase                             */
/************************************************************************/

class TigerFileBase
{
protected:
  OGRTigerDataSource  *poDS;

  char                *pszModule;
  char                *pszShortModule;
  FILE                *fpPrimary;

  OGRFeatureDefn      *poFeatureDefn;

  int                 nFeatures;
  int                 nRecordLength;

  int                 OpenFile( const char *, const char * );
  void                EstablishFeatureCount();

  static int          EstablishRecordLength( FILE * );

  void                SetupVersion();

  int                 nVersionCode;
  TigerVersion        nVersion;

public:
                      TigerFileBase();
  virtual            ~TigerFileBase();

  TigerVersion        GetVersion() { return nVersion; }
  int                 GetVersionCode() { return nVersionCode; }

  virtual const char *GetShortModule() { return pszShortModule; }
  virtual const char *GetModule() { return pszModule; }
  virtual int         SetModule( const char * ) = 0;
  virtual int         SetWriteModule( const char *, int, OGRFeature * );

  virtual int         GetFeatureCount() { return nFeatures; }
  virtual OGRFeature *GetFeature( int ) = 0;

  virtual OGRErr      CreateFeature( OGRFeature *poFeature )
                                { return OGRERR_FAILURE; }

  OGRFeatureDefn     *GetFeatureDefn() { return poFeatureDefn; }

  static const char * GetField( const char *, int, int );
  static void         SetField( OGRFeature *, const char *, const char *,
                                int, int );

  int                 WriteField( OGRFeature *, const char *, char *,
                                  int, int, char, char );
  int                 WriteRecord( char *pachRecord, int nRecLen,
                                   const char *pszType, FILE *fp = NULL );
  int                 WritePoint( char *pachRecord, int nStart,
                                  double dfX, double dfY );

 protected:
  void                WriteFields(TigerRecordInfo *psRTInfo,
                                  OGRFeature      *poFeature,
                                  char            *szRecord);

  void                AddFieldDefns(TigerRecordInfo *psRTInfo,
                                    OGRFeatureDefn  *poFeatureDefn);


  void                SetFields(TigerRecordInfo *psRTInfo,
                                OGRFeature      *poFeature,
                                char            *achRecord);

};

/************************************************************************/
/*                          TigerCompleteChain                          */
/************************************************************************/

class TigerCompleteChain : public TigerFileBase
{
  FILE               *fpShape;
  int                *panShapeRecordId;

  FILE               *fpRT3;
  int                 bUsingRT3;
  int                 nRT1RecOffset;

  int                 GetShapeRecordId( int, int );
  int                 AddShapePoints( int, int, OGRLineString *, int );

  void                AddFieldDefnsPre2002();
  OGRFeature         *GetFeaturePre2002( int );
  OGRErr              WriteRecordsPre2002( OGRFeature *, OGRLineString * );

  OGRErr              WriteRecords2002( OGRFeature *, OGRLineString * );
  OGRFeature         *GetFeature2002( int );
  void                AddFieldDefns2002();

  TigerRecordInfo    *psRT1Info;
  TigerRecordInfo    *psRT2Info;
  TigerRecordInfo    *psRT3Info;

public:
                      TigerCompleteChain( OGRTigerDataSource *,
                                          const char * );
  virtual            ~TigerCompleteChain();

  virtual int         SetModule( const char * );

  virtual OGRFeature *GetFeature( int );

  virtual OGRErr      CreateFeature( OGRFeature *poFeature );

  virtual int         SetWriteModule( const char *, int, OGRFeature * );
};

/************************************************************************/
/*                    TigerAltName (Type 4 records)                     */
/************************************************************************/

class TigerAltName : public TigerFileBase
{
 private:
  TigerRecordInfo    *psRT4Info;

 public:
                      TigerAltName( OGRTigerDataSource *,
                                          const char * );
  virtual            ~TigerAltName();

  virtual int         SetModule( const char * );

  virtual OGRFeature *GetFeature( int );

  virtual OGRErr      CreateFeature( OGRFeature *poFeature );
};

/************************************************************************/
/*                    TigerFeatureIds (Type 5 records)                  */
/************************************************************************/

class TigerFeatureIds : public TigerFileBase
{
 private:
  TigerRecordInfo    *psRT5Info;

 public:
                      TigerFeatureIds( OGRTigerDataSource *,
                                       const char * );
  virtual            ~TigerFeatureIds();

  virtual int         SetModule( const char * );

  virtual OGRFeature *GetFeature( int );

  virtual OGRErr      CreateFeature( OGRFeature *poFeature );
};

/************************************************************************/
/*                    TigerZipCodes (Type 6 records)                    */
/************************************************************************/

class TigerZipCodes : public TigerFileBase
{
 private:
  TigerRecordInfo    *psRT6Info;

public:
                      TigerZipCodes( OGRTigerDataSource *, const char * );
  virtual            ~TigerZipCodes();

  virtual int         SetModule( const char * );

  virtual OGRFeature *GetFeature( int );

  virtual OGRErr      CreateFeature( OGRFeature *poFeature );
};

/************************************************************************/
/*                    TigerPoint                                        */
/* This is an abstract base class for TIGER layers with point geometry. */
/* Since much of the implementation of these layers is similar, I've    */
/* put it into this base class to avoid duplication in the actual       */
/* layer classes.  mbp Sat Jan  4 16:41:19 2003.                        */
/************************************************************************/

class TigerPoint : public TigerFileBase
{
 protected:
                      TigerPoint(int bRequireGeom);

                      // The boolean bRequireGeom indicates whether
                      // the layer requires each feature to actual
                      // have a geom.  It's used in CreateFeature() to
                      // decide whether to report an error when a
                      // missing geom is detected.

  virtual             ~TigerPoint();

 private:
 int                  bRequireGeom;

 public:
  virtual int         SetModule( const char *,
                                 char *pszFileCode );

  virtual OGRFeature *GetFeature( int              nRecordId,
                                  TigerRecordInfo *psRTInfo,
                                  int nX0, int nX1,
                                  int nY0, int nY1 );


  virtual OGRErr CreateFeature( OGRFeature      *poFeature,
                                TigerRecordInfo *psRTInfo,
                                int nIndex,
                                char *pszFileCode );

};

/************************************************************************/
/*                   TigerLandmarks (Type 7 records)                    */
/************************************************************************/

class TigerLandmarks : public TigerPoint
{
 private:
  TigerRecordInfo    *psRT7Info;

 public:
                      TigerLandmarks( OGRTigerDataSource *, const char * );
  virtual            ~TigerLandmarks();

  virtual int         SetModule( const char * );

  virtual OGRFeature *GetFeature( int );

  virtual OGRErr      CreateFeature( OGRFeature *poFeature );
};

/************************************************************************/
/*                   TigerAreaLandmarks (Type 8 records)                */
/************************************************************************/

class TigerAreaLandmarks : public TigerFileBase
{
 private:
  TigerRecordInfo    *psRT8Info;

public:
                      TigerAreaLandmarks( OGRTigerDataSource *, const char * );
  virtual            ~TigerAreaLandmarks();

  virtual int         SetModule( const char * );

  virtual OGRFeature *GetFeature( int );

  virtual OGRErr      CreateFeature( OGRFeature *poFeature );
};

/************************************************************************/
/*                   TigerKeyFeatures (Type 9 records)                  */
/************************************************************************/

class TigerKeyFeatures : public TigerFileBase
{
 private:
  TigerRecordInfo    *psRT9Info;

public:
                      TigerKeyFeatures( OGRTigerDataSource *, const char * );
  virtual            ~TigerKeyFeatures();

  virtual int         SetModule( const char * );

  virtual OGRFeature *GetFeature( int );

  virtual OGRErr      CreateFeature( OGRFeature *poFeature );
};

/************************************************************************/
/*                   TigerPolygon (Type A&S records)                    */
/************************************************************************/

class TigerPolygon : public TigerFileBase
{
 private:
  TigerRecordInfo    *psRTAInfo;
  TigerRecordInfo    *psRTSInfo;

  FILE               *fpRTS;
  int                 bUsingRTS;
  int                 nRTSRecLen;

public:
                      TigerPolygon( OGRTigerDataSource *, const char * );
  virtual            ~TigerPolygon();

  virtual int         SetModule( const char * );

  virtual OGRFeature *GetFeature( int );

  virtual int         SetWriteModule( const char *, int, OGRFeature * );
  virtual OGRErr      CreateFeature( OGRFeature *poFeature );
};

/************************************************************************/
/*                    TigerPolygonCorrections (Type B records)          */
/************************************************************************/

class TigerPolygonCorrections : public TigerFileBase
{
 private:
  TigerRecordInfo    *psRTBInfo;

public:
                      TigerPolygonCorrections( OGRTigerDataSource *, const char * );
  virtual            ~TigerPolygonCorrections();

  virtual int         SetModule( const char * );

  virtual OGRFeature *GetFeature( int );

  virtual OGRErr      CreateFeature( OGRFeature *poFeature );
};

/************************************************************************/
/*                  TigerEntityNames (Type C records)                   */
/************************************************************************/

class TigerEntityNames : public TigerFileBase
{
 private:
  TigerRecordInfo    *psRTCInfo;

public:
                      TigerEntityNames( OGRTigerDataSource *, const char * );
  virtual            ~TigerEntityNames();

  virtual int         SetModule( const char * );

  virtual OGRFeature *GetFeature( int );

  virtual OGRErr      CreateFeature( OGRFeature *poFeature );
};

/************************************************************************/
/*                    TigerPolygonEconomic (Type E records)             */
/************************************************************************/

class TigerPolygonEconomic : public TigerFileBase
{
 private:
  TigerRecordInfo    *psRTEInfo;

public:
                      TigerPolygonEconomic( OGRTigerDataSource *, const char * );
  virtual            ~TigerPolygonEconomic();

  virtual int         SetModule( const char * );

  virtual OGRFeature *GetFeature( int );

  virtual OGRErr      CreateFeature( OGRFeature *poFeature );
};

/************************************************************************/
/*                  TigerIDHistory (Type H records)                     */
/************************************************************************/

class TigerIDHistory : public TigerFileBase
{
 private:
  TigerRecordInfo    *psRTHInfo;

public:
                      TigerIDHistory( OGRTigerDataSource *, const char * );
  virtual            ~TigerIDHistory();

  virtual int         SetModule( const char * );

  virtual OGRFeature *GetFeature( int );

  virtual OGRErr      CreateFeature( OGRFeature *poFeature );
};

/************************************************************************/
/*                   TigerPolyChainLink (Type I records)                */
/************************************************************************/

class TigerPolyChainLink : public TigerFileBase
{
 private:
  TigerRecordInfo    *psRTIInfo;

public:
                      TigerPolyChainLink( OGRTigerDataSource *, const char * );
  virtual            ~TigerPolyChainLink();

  virtual int         SetModule( const char * );

  virtual OGRFeature *GetFeature( int );

  virtual OGRErr      CreateFeature( OGRFeature *poFeature );
};

/************************************************************************/
/*                TigerSpatialMetadata (Type M records)                 */
/************************************************************************/

class TigerSpatialMetadata : public TigerFileBase
{
 private:
  TigerRecordInfo    *psRTMInfo;

public:
                      TigerSpatialMetadata( OGRTigerDataSource *, const char * );
  virtual            ~TigerSpatialMetadata();

  virtual int         SetModule( const char * );

  virtual OGRFeature *GetFeature( int );

  virtual OGRErr      CreateFeature( OGRFeature *poFeature );
};

/************************************************************************/
/*                   TigerPIP (Type P records)                          */
/************************************************************************/

class TigerPIP : public TigerPoint
{
 private:
  TigerRecordInfo    *psRTPInfo;

public:
                      TigerPIP( OGRTigerDataSource *, const char * );
  virtual            ~TigerPIP();

  virtual int         SetModule( const char * );

  virtual OGRFeature *GetFeature( int );

  virtual OGRErr      CreateFeature( OGRFeature *poFeature );
};

/************************************************************************/
/*                   TigerTLIDRange (Type R records)                    */
/************************************************************************/

class TigerTLIDRange : public TigerFileBase
{
 private:
  TigerRecordInfo    *psRTRInfo;

public:
                      TigerTLIDRange( OGRTigerDataSource *, const char * );
  virtual            ~TigerTLIDRange();

  virtual int         SetModule( const char * );

  virtual OGRFeature *GetFeature( int );

  virtual OGRErr      CreateFeature( OGRFeature *poFeature );
};

/************************************************************************/
/*                    TigerZeroCellID (Type T records)                  */
/************************************************************************/

class TigerZeroCellID : public TigerFileBase
{
 private:
  TigerRecordInfo    *psRTTInfo;

public:
                      TigerZeroCellID( OGRTigerDataSource *, const char * );
  virtual            ~TigerZeroCellID();

  virtual int         SetModule( const char * );

  virtual OGRFeature *GetFeature( int );

  virtual OGRErr      CreateFeature( OGRFeature *poFeature );
};

/************************************************************************/
/*                    TigerOverUnder (Type U records)                   */
/************************************************************************/

class TigerOverUnder : public TigerPoint
{
 private:
  TigerRecordInfo    *psRTUInfo;

public:
                      TigerOverUnder( OGRTigerDataSource *, const char * );
  virtual            ~TigerOverUnder();

  virtual int         SetModule( const char * );

  virtual OGRFeature *GetFeature( int );

  virtual OGRErr      CreateFeature( OGRFeature *poFeature );
};

/************************************************************************/
/*                    TigerZipPlus4 (Type Z records)                    */
/************************************************************************/

class TigerZipPlus4 : public TigerFileBase
{
 private:
  TigerRecordInfo    *psRTZInfo;

 public:
                      TigerZipPlus4( OGRTigerDataSource *, const char * );
  virtual            ~TigerZipPlus4();

  virtual int         SetModule( const char * );

  virtual OGRFeature *GetFeature( int );

  virtual OGRErr      CreateFeature( OGRFeature *poFeature );
};

/************************************************************************/
/*                            OGRTigerLayer                             */
/************************************************************************/

class OGRTigerLayer : public OGRLayer
{
    TigerFileBase      *poReader;

    OGRTigerDataSource   *poDS;

    int                 nFeatureCount;
    int                 *panModuleFCount;
    int                 *panModuleOffset;

    int                 iLastFeatureId;
    int                 iLastModule;

  public:
                        OGRTigerLayer( OGRTigerDataSource * poDS,
                                       TigerFileBase * );
    virtual             ~OGRTigerLayer();

    void                ResetReading();
    OGRFeature *        GetNextFeature();
    OGRFeature         *GetFeature( long nFeatureId );

    OGRFeatureDefn *    GetLayerDefn();

    int                 GetFeatureCount( int );

    int                 TestCapability( const char * );

    virtual OGRSpatialReference *GetSpatialRef();

    virtual OGRErr      CreateFeature( OGRFeature *poFeature );
    virtual OGRErr      CreateField( OGRFieldDefn *poField,
                                     int bApproxOK = TRUE );
};

/************************************************************************/
/*                          OGRTigerDataSource                          */
/************************************************************************/

class OGRTigerDataSource : public OGRDataSource
{
    char                *pszName;

    int                 nLayers;
    OGRTigerLayer       **papoLayers;

    OGRSpatialReference *poSpatialRef;

    char                **papszOptions;

    char                *pszPath;

    int                 nModules;
    char                **papszModules;

    int                 nVersionCode;
    TigerVersion        nVersion;

    int                 bWriteMode;

    TigerVersion        TigerCheckVersion( TigerVersion, const char * );

  public:
                        OGRTigerDataSource();
                        ~OGRTigerDataSource();

    int                 GetWriteMode() { return bWriteMode; }

    TigerVersion        GetVersion() { return nVersion; }
    int                 GetVersionCode() { return nVersionCode; }

    void                SetOptionList( char ** );
    const char         *GetOption( const char * );

    int                 Open( const char * pszName, int bTestOpen = FALSE,
                              char ** papszFileList = NULL );

    int                 Create( const char *pszName, char **papszOptions );

    const char          *GetName() { return pszName; }
    int                 GetLayerCount();
    OGRLayer            *GetLayer( int );
    OGRLayer            *GetLayer( const char *pszLayerName );

    void                AddLayer( OGRTigerLayer * );
    int                 TestCapability( const char * );

    OGRSpatialReference *GetSpatialRef() { return poSpatialRef; }

    const char          *GetDirPath() { return pszPath; }
    char                *BuildFilename( const char * pszModule,
                                        const char * pszExtension );


    int                 GetModuleCount() { return nModules; }
    const char         *GetModule( int );
    int                 CheckModule( const char *pszModule );
    void                AddModule( const char *pszModule );

    void                DeleteModuleFiles( const char *pszModule );

    virtual OGRLayer    *CreateLayer( const char *,
                                      OGRSpatialReference * = NULL,
                                      OGRwkbGeometryType = wkbUnknown,
                                      char ** = NULL );
};

/************************************************************************/
/*                            OGRTigerDriver                            */
/************************************************************************/

class OGRTigerDriver : public OGRSFDriver
{
  public:
                ~OGRTigerDriver();

    const char *GetName();

    OGRDataSource *Open( const char *, int );

    virtual OGRDataSource *CreateDataSource( const char *pszName,
                                             char ** = NULL );

    int         TestCapability( const char * );
};

#endif /* ndef _OGR_TIGER_H_INCLUDED */
