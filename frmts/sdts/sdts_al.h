/******************************************************************************
 * $Id$
 *
 * Project:  SDTS Translator
 * Purpose:  Include file for entire SDTS Abstraction Layer functions.
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
 * Revision 1.13  1999/09/02 03:40:03  warmerda
 * added indexed readers
 *
 * Revision 1.12  1999/08/16 20:59:28  warmerda
 * added szOBRP support for SDTSModId
 *
 * Revision 1.11  1999/08/16 19:24:45  warmerda
 * upped ATID limits, added polyreader method
 *
 * Revision 1.10  1999/08/16 15:45:46  warmerda
 * added IsSecondary()
 *
 * Revision 1.9  1999/08/10 02:52:13  warmerda
 * introduce use of SDTSApplyModIdList to capture multi-attributes
 *
 * Revision 1.8  1999/07/30 19:15:56  warmerda
 * added module reference counting
 *
 * Revision 1.7  1999/06/03 21:13:02  warmerda
 * Added transform for rasters.
 *
 * Revision 1.6  1999/06/03 14:04:10  warmerda
 * Added SDTS_XREF and SDTSRasterReader
 *
 * Revision 1.5  1999/05/11 14:05:59  warmerda
 * added SDTSTransfer and SDTSPolygonreader
 *
 * Revision 1.4  1999/05/07 13:45:01  warmerda
 * major upgrade to use iso8211lib
 *
 * Revision 1.3  1999/04/21 04:38:32  warmerda
 * Added new classes including SDTSPoint
 *
 * Revision 1.2  1999/03/23 15:58:01  warmerda
 * some const fixes, and attr record fixes
 *
 * Revision 1.1  1999/03/23 13:56:14  warmerda
 * New
 *
 */

#ifndef SDTS_AL_H_INCLUDED
#define STDS_AL_H_INCLUDED

#include "cpl_conv.h"
#include "iso8211.h"

class SDTS_IREF;
class SDTSModId;
class SDTSTransfer;

#define SDTS_SIZEOF_SADR	8

int SDTSGetSADR( SDTS_IREF *, DDFField *, int, double *, double *, double * );
char **SDTSScanModuleReferences( DDFModule *, const char * );

/************************************************************************/
/*                              SDTS_IREF                               */
/*									*/
/*      Class for reading the IREF (internal reference) module.         */
/************************************************************************/
class SDTS_IREF
{
  public:
    		SDTS_IREF();
		~SDTS_IREF();

    int         Read( const char *pszFilename );

    char	*pszXAxisName;			/* XLBL */
    char  	*pszYAxisName;			/* YLBL */

    double	dfXScale;			/* SFAX */
    double      dfYScale;			/* SFAY */

    double	dfXOffset;			/* XORG */
    double	dfYOffset;			/* YORG */

    double      dfXRes;				/* XHRS */
    double      dfYRes;				/* YHRS */

    char  	*pszCoordinateFormat;		/* HFMT */
                
};

/************************************************************************/
/*                              SDTS_XREF                               */
/*                                                                      */
/*      Class for reading the XREF (projection) module.         	*/
/************************************************************************/
class SDTS_XREF
{
  public:
    		SDTS_XREF();
		~SDTS_XREF();

    int         Read( const char *pszFilename );

    char	*pszSystemName;			/* RSNM */
				  /* one of GEO, SPCS, UTM, UPS, OTHR, UNSP */

    char	*pszDatum;			/* HDAT */
				  /* one of NAS, NAX, WGA, WGB, WGC, WGE */

    int		nZone;		      		/* ZONE */
};

/************************************************************************/
/*                              SDTS_CATD                               */
/*                                                                      */
/*      Class for reading the directory of other files file.            */
/************************************************************************/
class SDTS_CATDEntry;

typedef enum {
  SLTUnknown,
  SLTPoint,
  SLTLine,
  SLTAttr,
  SLTPoly,
  SLTRaster
} SDTSLayerType;

class SDTS_CATD
{
    char	*pszPrefixPath;

    int		nEntries;
    SDTS_CATDEntry **papoEntries;
    
  public:
    		SDTS_CATD();
                ~SDTS_CATD();

    int         Read( const char * pszFilename );

    const char  *GetModuleFilePath( const char * pszModule );

    int		GetEntryCount() { return nEntries; }
    const char * GetEntryModule(int);
    const char * GetEntryTypeDesc(int);
    const char * GetEntryFilePath(int);
    SDTSLayerType GetEntryType(int);
};

/************************************************************************/
/*                              SDTSModId                               */
/*                                                                      */
/*      A simple class to encapsulate a model and record                */
/*      identifier.  Implemented in sdtslib.cpp.                        */
/************************************************************************/

class SDTSModId
{
  public:
    		SDTSModId() { szModule[0] = '\0';
                              nRecord = -1;
                	      szOBRP[0] = '\0'; }

    int		Set( DDFField * );
    const char *GetName();
    
    char	szModule[8];
    long	nRecord;
    char	szOBRP[8]; 
};

/************************************************************************/
/*                             SDTSFeature                              */
/*                                                                      */
/*      Base class for points, lines, polygons and attribute            */
/*      records.                                                        */
/************************************************************************/

class SDTSFeature
{
public:

    virtual            ~SDTSFeature();
    
    SDTSModId		oModId;

#define MAX_ATID	4    
    int		nAttributes;
    SDTSModId	aoATID[MAX_ATID];

    void        ApplyATID( DDFField * );
};

/************************************************************************/
/*                          SDTSIndexedReader                           */
/************************************************************************/

class SDTSIndexedReader
{
    int			nIndexSize;
    SDTSFeature	      **papoFeatures;

    int			iCurrentFeature;

protected:    
    DDFModule   	oDDFModule;

public:
    			SDTSIndexedReader();
    virtual            ~SDTSIndexedReader();
    
    virtual SDTSFeature  *GetNextRawFeature() = 0;
    
    SDTSFeature	       *GetNextFeature();

    virtual void        Rewind();
    
    void		FillIndex();

    SDTSFeature	       *GetIndexedFeatureRef( int );
    char **		ScanModuleReferences( const char * = "ATID" );
};


/************************************************************************/
/*                             SDTSRawLine                              */
/*                                                                      */
/*      Simple container for the information from a line read from a    */
/*      line file.                                                      */
/************************************************************************/
class SDTSRawLine : public SDTSFeature
{
  public:
    		SDTSRawLine();
    virtual    ~SDTSRawLine();

    int         Read( SDTS_IREF *, DDFRecord * );

    int		nVertices;

    double	*padfX;
    double	*padfY;
    double	*padfZ;

    SDTSModId	oLeftPoly;		/* PIDL */
    SDTSModId	oRightPoly;		/* PIDR */
    SDTSModId	oStartNode;		/* SNID */
    SDTSModId	oEndNode;		/* ENID */

    void	Dump( FILE * );
};

/************************************************************************/
/*                            SDTSLineReader                            */
/*                                                                      */
/*      Class for reading any of the files lines.                       */
/************************************************************************/

class SDTSLineReader : public SDTSIndexedReader
{
    SDTS_IREF	*poIREF;
    
  public:
    		SDTSLineReader( SDTS_IREF * );
                ~SDTSLineReader();

    int         Open( const char * );
    SDTSRawLine *GetNextLine( void );
    void	Close();
    
    SDTSFeature *GetNextRawFeature( void ) { return GetNextLine(); }

    void        AttachToPolygons( SDTSTransfer * );
};

/************************************************************************/
/*                            SDTSAttrRecord                            */
/*                                                                      */
/*      Simple "SDTSFeature" enabled contain for an attribute record.   */
/************************************************************************/

class SDTSAttrRecord : public SDTSFeature
{
  public:
    		SDTSAttrRecord();
    virtual    ~SDTSAttrRecord();

    DDFRecord   *poWholeRecord;
    DDFField    *poATTR;

    virtual void Dump( FILE * );
};

/************************************************************************/
/*                            SDTSAttrReader                            */
/*                                                                      */
/*      Class for reading any of the primary attribute files.           */
/************************************************************************/

class SDTSAttrReader : public SDTSIndexedReader
{
    SDTS_IREF	*poIREF;

    int		bIsSecondary;
    
  public:
    		SDTSAttrReader( SDTS_IREF * );
   virtual     ~SDTSAttrReader();

    int         Open( const char * );
    DDFField	*GetNextRecord( SDTSModId * = NULL,
                                DDFRecord ** = NULL );
    SDTSAttrRecord *GetNextAttrRecord();
    void	Close();

    int		IsSecondary() { return bIsSecondary; }
    
    SDTSFeature *GetNextRawFeature( void ) { return GetNextAttrRecord(); }
};

/************************************************************************/
/*                             SDTSRawPoint                             */
/*                                                                      */
/*      Simple container for the information from a point.              */
/************************************************************************/

class SDTSRawPoint : public SDTSFeature
{
  public:
    		SDTSRawPoint();
    virtual    ~SDTSRawPoint();

    int         Read( SDTS_IREF *, DDFRecord * );

    double	dfX;
    double	dfY;
    double	dfZ;

    SDTSModId   oAreaId;		/* ARID */
};

/************************************************************************/
/*                           SDTSPointReader                            */
/*                                                                      */
/*      Class for reading any of the point files.                       */
/************************************************************************/

class SDTSPointReader : public SDTSIndexedReader
{
    SDTS_IREF	*poIREF;
    
  public:
    		SDTSPointReader( SDTS_IREF * );
    virtual    ~SDTSPointReader();

    int         Open( const char * );
    SDTSRawPoint *GetNextPoint( void );
    void	Close();

    SDTSFeature *GetNextRawFeature( void ) { return GetNextPoint(); }
};

/************************************************************************/
/*                             SDTSRawPolygon                           */
/*                                                                      */
/*      Simple container for the information from a polygon             */
/************************************************************************/

class SDTSRawPolygon : public SDTSFeature
{
    void        AddEdgeToRing( SDTSRawLine *, int, int );
    
  public:
    		SDTSRawPolygon();
    virtual    ~SDTSRawPolygon();

    int         Read( DDFRecord * );

    int		nEdges;
    SDTSRawLine **papoEdges;

    void	AddEdge( SDTSRawLine * );

    int		AssembleRings();
    
    int		nRings;
    int		nVertices;
    int		*panRingStart;
    double	*padfX;
    double      *padfY;
    double      *padfZ;
};

/************************************************************************/
/*                          SDTSPolygonReader                           */
/*                                                                      */
/*      Class for reading any of the polygon files.                     */
/************************************************************************/

class SDTSPolygonReader : public SDTSIndexedReader
{
  public:
    		SDTSPolygonReader();
    virtual    ~SDTSPolygonReader();

    int         Open( const char * );
    SDTSRawPolygon *GetNextPolygon( void );
    void	Close();

    SDTSFeature *GetNextRawFeature( void ) { return GetNextPolygon(); }
};

/************************************************************************/
/*                           SDTSRasterReader                           */
/*                                                                      */
/*      Class for reading any of the raster cell files.                 */
/************************************************************************/

class SDTSRasterReader
{
    DDFModule	oDDFModule;

    char	szModule[20];

    int		nXSize;
    int		nYSize;
    int		nXBlockSize;
    int		nYBlockSize;

    int		nXStart;		/* SOCI */
    int		nYStart;		/* SORI */

    char	szINTR[4];		/* CE is center, TL is top left */

    double	adfTransform[6];
    
  public:
    		SDTSRasterReader();
                ~SDTSRasterReader();

    int         Open( SDTS_CATD * poCATD, SDTS_IREF *,
                      const char * pszModule  );
    void	Close();

    int		GetRasterType() { return 1; }  /* 1 = int16 */

    int		GetTransform( double * );

    int		GetXSize() { return nXSize; }
    int		GetYSize() { return nYSize; }
    
    int		GetBlockXSize() { return nXBlockSize; }
    int		GetBlockYSize() { return nYBlockSize; }
    
    int		GetBlock( int nXOffset, int nYOffset, void * pData );
};

/************************************************************************/
/*                             SDTSTransfer                             */
/************************************************************************/

class SDTSTransfer
{
  public:
    		SDTSTransfer();
                ~SDTSTransfer();

    int		Open( const char * );
    void	Close();

    int		FindLayer( const char * );
    int		GetLayerCount() { return nLayers; }
    SDTSLayerType GetLayerType( int );
    int		GetLayerCATDEntry( int );

    SDTSLineReader *GetLayerLineReader( int );
    SDTSPointReader *GetLayerPointReader( int );
    SDTSPolygonReader *GetLayerPolygonReader( int );
    SDTSAttrReader *GetLayerAttrReader( int );
    SDTSRasterReader *GetLayerRasterReader( int );
    DDFModule	*GetLayerModuleReader( int );

    SDTSIndexedReader *GetLayerIndexedReader( int );

    SDTS_CATD	*GetCATD() { return &oCATD ; }
    SDTS_IREF	*GetIREF() { return &oIREF; }
    SDTS_XREF   *GetXREF() { return &oXREF; }

    SDTSFeature *GetIndexedFeatureRef( SDTSModId *,
                                       SDTSLayerType *peType = NULL);
                

    DDFField *GetAttr( SDTSModId * );
    
  private:

    SDTS_CATD	oCATD;
    SDTS_IREF	oIREF;
    SDTS_XREF   oXREF;

    int		nLayers;
    int		*panLayerCATDEntry;
    SDTSIndexedReader **papoLayerReader;
};

#endif /* ndef SDTS_AL_H_INCLUDED */
