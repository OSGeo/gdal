/******************************************************************************
 * $Id$
 *
 * Project:  TIGER/Line Translator
 * Purpose:  Main declarations for Tiger translator.
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
*/

typedef enum {
    TIGER_1990_Precensus,
    TIGER_1990,
    TIGER_1992,
    TIGER_1994,
    TIGER_1995,
    TIGER_1997,
    TIGER_1998,
    TIGER_1999,
    TIGER_2000_Redistricting,
    TIGER_Unknown
} TigerVersion;

TigerVersion TigerClassifyVersion( int );

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

  static int	      EstablishRecordLength( FILE * );
 
  void		      SetupVersion();

  int		      nVersionCode;
  TigerVersion	      nVersion;

public:
                      TigerFileBase();
  virtual            ~TigerFileBase();

  TigerVersion        GetVersion() { return nVersion; }
  int                 GetVersionCode() { return nVersionCode; }

  virtual const char *GetShortModule() { return pszShortModule; }
  virtual const char *GetModule() { return pszModule; }
  virtual int         SetModule( const char * ) = 0;

  virtual int         GetFeatureCount() { return nFeatures; }
  virtual OGRFeature *GetFeature( int ) = 0;

  OGRFeatureDefn     *GetFeatureDefn() { return poFeatureDefn; }

  static const char * GetField( const char *, int, int );
  static void         SetField( OGRFeature *, const char *, const char *,
                                int, int );

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

  int                 GetShapeRecordId( int, int );
  void                AddShapePoints( int, int, OGRLineString *, int );
    
public:
                      TigerCompleteChain( OGRTigerDataSource *,
                                          const char * );
  virtual            ~TigerCompleteChain();

  virtual int         SetModule( const char * );

  virtual OGRFeature *GetFeature( int );
};

/************************************************************************/
/*                    TigerAltName (Type 4 records)                     */
/************************************************************************/

class TigerAltName : public TigerFileBase
{
public:
                      TigerAltName( OGRTigerDataSource *,
                                          const char * );
  virtual            ~TigerAltName();

  virtual int         SetModule( const char * );

  virtual OGRFeature *GetFeature( int );
};

/************************************************************************/
/*                    TigerFeatureIds (Type 5 records)                  */
/************************************************************************/

class TigerFeatureIds : public TigerFileBase
{
public:
                      TigerFeatureIds( OGRTigerDataSource *,
                                       const char * );
  virtual            ~TigerFeatureIds();

  virtual int         SetModule( const char * );

  virtual OGRFeature *GetFeature( int );
};

/************************************************************************/
/*                    TigerZipCodes (Type 6 records)                    */
/************************************************************************/

class TigerZipCodes : public TigerFileBase
{
public:
                      TigerZipCodes( OGRTigerDataSource *, const char * );
  virtual            ~TigerZipCodes();

  virtual int         SetModule( const char * );

  virtual OGRFeature *GetFeature( int );
};

/************************************************************************/
/*                   TigerLandmarks (Type 7 records)                    */
/************************************************************************/

class TigerLandmarks : public TigerFileBase
{
public:
                      TigerLandmarks( OGRTigerDataSource *, const char * );
  virtual            ~TigerLandmarks();

  virtual int         SetModule( const char * );

  virtual OGRFeature *GetFeature( int );
};

/************************************************************************/
/*                   TigerAreaLandmarks (Type 8 records)                */
/************************************************************************/

class TigerAreaLandmarks : public TigerFileBase
{
public:
                      TigerAreaLandmarks( OGRTigerDataSource *, const char * );
  virtual            ~TigerAreaLandmarks();

  virtual int         SetModule( const char * );

  virtual OGRFeature *GetFeature( int );
};

/************************************************************************/
/*                   TigerKeyFeatures (Type 9 records)                  */
/************************************************************************/

class TigerKeyFeatures : public TigerFileBase
{
public:
                      TigerKeyFeatures( OGRTigerDataSource *, const char * );
  virtual            ~TigerKeyFeatures();

  virtual int         SetModule( const char * );

  virtual OGRFeature *GetFeature( int );
};

/************************************************************************/
/*                   TigerPolygon (Type A&S records)                    */
/************************************************************************/

class TigerPolygon : public TigerFileBase
{
  FILE               *fpRTS;
  int                 bUsingRTS;
  int                 nRTSRecLen;

public:
                      TigerPolygon( OGRTigerDataSource *, const char * );
  virtual            ~TigerPolygon();

  virtual int         SetModule( const char * );

  virtual OGRFeature *GetFeature( int );
};

/************************************************************************/
/*                  TigerEntityNames (Type C records)                   */
/************************************************************************/

class TigerEntityNames : public TigerFileBase
{
public:
                      TigerEntityNames( OGRTigerDataSource *, const char * );
  virtual            ~TigerEntityNames();

  virtual int         SetModule( const char * );

  virtual OGRFeature *GetFeature( int );
};

/************************************************************************/
/*                  TigerIDHistory (Type H records)                     */
/************************************************************************/

class TigerIDHistory : public TigerFileBase
{
public:
                      TigerIDHistory( OGRTigerDataSource *, const char * );
  virtual            ~TigerIDHistory();

  virtual int         SetModule( const char * );

  virtual OGRFeature *GetFeature( int );
};

/************************************************************************/
/*                   TigerPolyChainLink (Type I records)                */
/************************************************************************/

class TigerPolyChainLink : public TigerFileBase
{
public:
                      TigerPolyChainLink( OGRTigerDataSource *, const char * );
  virtual            ~TigerPolyChainLink();

  virtual int         SetModule( const char * );

  virtual OGRFeature *GetFeature( int );
};

/************************************************************************/
/*                   TigerPIP (Type P records)                          */
/************************************************************************/

class TigerPIP : public TigerFileBase
{
public:
                      TigerPIP( OGRTigerDataSource *, const char * );
  virtual            ~TigerPIP();

  virtual int         SetModule( const char * );

  virtual OGRFeature *GetFeature( int );
};

/************************************************************************/
/*                   TigerTLIDRange (Type R records)                    */
/************************************************************************/

class TigerTLIDRange : public TigerFileBase
{
public:
                      TigerTLIDRange( OGRTigerDataSource *, const char * );
  virtual            ~TigerTLIDRange();

  virtual int         SetModule( const char * );

  virtual OGRFeature *GetFeature( int );
};

/************************************************************************/
/*                    TigerZipPlus4 (Type Z records)                    */
/************************************************************************/

class TigerZipPlus4 : public TigerFileBase
{
public:
                      TigerZipPlus4( OGRTigerDataSource *, const char * );
  virtual            ~TigerZipPlus4();

  virtual int         SetModule( const char * );

  virtual OGRFeature *GetFeature( int );
};

/************************************************************************/
/*                            OGRTigerLayer                             */
/************************************************************************/

class OGRTigerLayer : public OGRLayer
{
    TigerFileBase      *poReader;
    
    OGRGeometry        *poFilterGeom;

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

    OGRGeometry *       GetSpatialFilter() { return poFilterGeom; }
    void                SetSpatialFilter( OGRGeometry * );

    void                ResetReading();
    OGRFeature *        GetNextFeature();
    OGRFeature         *GetFeature( long nFeatureId );
#ifdef notdef    
    OGRErr              SetFeature( OGRFeature *poFeature );
    OGRErr              CreateFeature( OGRFeature *poFeature );
#endif
    
    OGRFeatureDefn *    GetLayerDefn();

    int                 GetFeatureCount( int ) { return nFeatureCount; }
    
    int                 TestCapability( const char * );

    virtual OGRSpatialReference *GetSpatialRef();
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
    
    int		        nVersionCode;
    TigerVersion	nVersion;

  public:
                        OGRTigerDataSource();
                        ~OGRTigerDataSource();

    TigerVersion        GetVersion() { return nVersion; }
    int                 GetVersionCode() { return nVersionCode; }

    void                SetOptionList( char ** );
    const char         *GetOption( const char * );
    
    int                 Open( const char * pszName, int bTestOpen = FALSE,
                              char ** papszFileList = NULL );
    
    const char          *GetName() { return pszName; }
    int                 GetLayerCount();
    OGRLayer            *GetLayer( int );
    void                AddLayer( OGRTigerLayer * );
    int                 TestCapability( const char * );

    OGRSpatialReference *GetSpatialRef() { return poSpatialRef; }

    const char          *GetDirPath() { return pszPath; }
    char                *BuildFilename( const char * pszModule,
                                        const char * pszExtension );
    

    int                 GetModuleCount() { return nModules; }
    const char         *GetModule( int );
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
    int                 TestCapability( const char * );
};

#endif /* ndef _OGR_TIGER_H_INCLUDED */
