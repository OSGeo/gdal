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
 * Revision 1.3  1999/12/15 19:59:52  warmerda
 * added new file types
 *
 * Revision 1.2  1999/11/04 21:14:31  warmerda
 * various improvements, and TestCapability()
 *
 * Revision 1.1  1999/10/07 18:19:21  warmerda
 * New
 *
 */

#ifndef _OGR_TIGER_H_INCLUDED
#define _OGR_TIGER_H_INCLUDED

#include "cpl_conv.h"
#include "ogrsf_frmts.h"

class OGRTigerDataSource;

/************************************************************************/
/*                            TigerFileBase                             */
/************************************************************************/

class TigerFileBase
{
protected:
  OGRTigerDataSource  *poDS;

  char		      *pszModule;
  FILE		      *fpPrimary;
    
  OGRFeatureDefn      *poFeatureDefn;

  int 		      nFeatures;
  int		      nRecordLength;
    
  int                 OpenFile( const char *, const char * );
  void		      EstablishFeatureCount();

public:
                      TigerFileBase();
  virtual            ~TigerFileBase();
    
  virtual const char *GetModule() { return pszModule; }
  virtual int         SetModule( const char * ) = 0;

  virtual int         GetFeatureCount() { return nFeatures; }
  virtual OGRFeature *GetFeature( int ) = 0;

  OGRFeatureDefn     *GetFeatureDefn() { return poFeatureDefn; }

  static const char * GetField( const char *, int, int );
};

/************************************************************************/
/*                          TigerCompleteChain                          */
/************************************************************************/

class TigerCompleteChain : public TigerFileBase
{
  FILE		     *fpShape;
  int		     *panShapeRecordId;

  FILE		     *fpRT3;
  int		      bUsingRT3;

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
/*                            OGRTigerLayer                             */
/************************************************************************/

class OGRTigerLayer : public OGRLayer
{
    TigerFileBase      *poReader;
    
    OGRGeometry	       *poFilterGeom;

    OGRTigerDataSource   *poDS;

    int			nFeatureCount;
    int			*panModuleFCount;
    int			*panModuleOffset;

    int			iLastFeatureId;
    int			iLastModule;

  public:
    			OGRTigerLayer( OGRTigerDataSource * poDS,
                                       TigerFileBase * );
    virtual		~OGRTigerLayer();

    OGRGeometry *	GetSpatialFilter() { return poFilterGeom; }
    void		SetSpatialFilter( OGRGeometry * );

    void		ResetReading();
    OGRFeature *	GetNextFeature();
    OGRFeature         *GetFeature( long nFeatureId );
#ifdef notdef    
    OGRErr              SetFeature( OGRFeature *poFeature );
    OGRErr              CreateFeature( OGRFeature *poFeature );
#endif
    
    OGRFeatureDefn *	GetLayerDefn();

    int                 GetFeatureCount( int ) { return nFeatureCount; }
    
    int                 TestCapability( const char * );

    virtual OGRSpatialReference *GetSpatialRef();
};

/************************************************************************/
/*                          OGRTigerDataSource                          */
/************************************************************************/

class OGRTigerDataSource : public OGRDataSource
{
    char		*pszName;

    int			nLayers;
    OGRTigerLayer	**papoLayers;

    OGRSpatialReference *poSpatialRef;

    char		**papszOptions;
    
    char                *pszPath;

    int			nModules;
    char		**papszModules;
    
  public:
    			OGRTigerDataSource();
    			~OGRTigerDataSource();

    void		SetOptionList( char ** );
    const char	       *GetOption( const char * );
    
    int                 Open( const char * pszName, int bTestOpen = FALSE,
                              char ** papszFileList = NULL );
    
    const char	        *GetName() { return pszName; }
    int			GetLayerCount();
    OGRLayer		*GetLayer( int );
    void                AddLayer( OGRTigerLayer * );
    int                 TestCapability( const char * );

    OGRSpatialReference *GetSpatialRef() { return poSpatialRef; }

    const char          *GetDirPath() { return pszPath; }
    char	     	*BuildFilename( const char * pszModule,
                                        const char * pszExtension );
    

    int			GetModuleCount() { return nModules; }
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
