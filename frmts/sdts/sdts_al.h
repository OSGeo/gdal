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

#define SDTS_SIZEOF_SADR	8

int SDTSGetSADR( SDTS_IREF *, DDFField *, int, double *, double *, double * );
char **SDTSScanModuleReferences( DDFModule *, const char * );
void SDTSApplyModIdList( DDFField * poField, int nMaxAttributes,
                         int * pnAttributes, SDTSModId *paoATID );

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
/*                            SDTSLineReader                            */
/*                                                                      */
/*      Class for reading any of the files lines.                       */
/************************************************************************/

class SDTSRawLine;

class SDTSLineReader
{
    DDFModule   oDDFModule;

    SDTS_IREF	*poIREF;
    
  public:
    		SDTSLineReader( SDTS_IREF * );
                ~SDTSLineReader();

    int         Open( const char * );
    SDTSRawLine *GetNextLine( void );
    void	Close();

    char **	ScanModuleReferences( const char * = "ATID" );
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
/*                             SDTSRawLine                              */
/*                                                                      */
/*      Simple container for the information from a line read from a    */
/*      line file.                                                      */
/************************************************************************/
class SDTSRawLine
{
  public:
    		SDTSRawLine();
                ~SDTSRawLine();

    int         Read( SDTS_IREF *, DDFRecord * );

    SDTSModId	oLine;			/* LINE field */
                
    int		nVertices;

    double	*padfX;
    double	*padfY;
    double	*padfZ;

    SDTSModId	oLeftPoly;		/* PIDL */
    SDTSModId	oRightPoly;		/* PIDR */
    SDTSModId	oStartNode;		/* SNID */
    SDTSModId	oEndNode;		/* ENID */

#define MAX_RAWLINE_ATID	4    
    int		nAttributes;
    SDTSModId	aoATID[MAX_RAWLINE_ATID];  /* ATID (attribute) references */

    void	Dump( FILE * );
};

/************************************************************************/
/*                            SDTSAttrReader                            */
/*                                                                      */
/*      Class for reading any of the primary attribute files.           */
/************************************************************************/

class SDTSAttrRecord;

class SDTSAttrReader
{
    DDFModule	oDDFModule;

    SDTS_IREF	*poIREF;

    int		bIsSecondary;
    
  public:
    		SDTSAttrReader( SDTS_IREF * );
                ~SDTSAttrReader();

    int         Open( const char * );
    DDFField	*GetNextRecord( SDTSModId * = NULL,
                                DDFRecord ** = NULL );
    void	Close();

    int		IsSecondary() { return bIsSecondary; }
};

/************************************************************************/
/*                           SDTSPointReader                            */
/*                                                                      */
/*      Class for reading any of the point files.                       */
/************************************************************************/

class SDTSRawPoint;

class SDTSPointReader
{
    DDFModule	oDDFModule;
    SDTS_IREF	*poIREF;
    
  public:
    		SDTSPointReader( SDTS_IREF * );
                ~SDTSPointReader();

    int         Open( const char * );
    SDTSRawPoint *GetNextPoint( void );
    void	Close();

    char **	ScanModuleReferences( const char * = "ATID" );
};

/************************************************************************/
/*                             SDTSRawPoint                             */
/*                                                                      */
/*      Simple container for the information from a point.              */
/************************************************************************/

class SDTSRawPoint
{
  public:
    		SDTSRawPoint();
                ~SDTSRawPoint();

    int         Read( SDTS_IREF *, DDFRecord * );

    SDTSModId	oPoint;			/* PNTS field */
                
    double	dfX;
    double	dfY;
    double	dfZ;

#define MAX_RAWPOINT_ATID	4
    int		nAttributes;
    SDTSModId	aoATID[MAX_RAWPOINT_ATID];  /* ATID (attribute) references */
    SDTSModId   oAreaId;		/* ARID */
};

/************************************************************************/
/*                          SDTSPolygonReader                           */
/*                                                                      */
/*      Class for reading any of the polygon files.                     */
/************************************************************************/

class SDTSRawPolygon;

class SDTSPolygonReader
{
    DDFModule	oDDFModule;
    
  public:
    		SDTSPolygonReader();
                ~SDTSPolygonReader();

    int         Open( const char * );
    SDTSRawPolygon *GetNextPolygon( void );
    void	Close();

    char **	ScanModuleReferences( const char * = "ATID" );
};

/************************************************************************/
/*                             SDTSRawPolygon                           */
/*                                                                      */
/*      Simple container for the information from a polygon             */
/************************************************************************/

class SDTSRawPolygon
{
  public:
    		SDTSRawPolygon();

    int         Read( DDFRecord * );

    SDTSModId	oPolyId;
     
#define MAX_RAWPOLYGON_ATID	4
    int		nAttributes;
    SDTSModId	aoATID[MAX_RAWPOLYGON_ATID];  /* ATID (attribute) references */
    SDTSModId   oAreaId;		      /* ARID */
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

    int		GetLayerCount() { return nLayers; }
    SDTSLayerType GetLayerType( int );
    int		GetLayerCATDEntry( int );

    SDTSLineReader *GetLayerLineReader( int );
    SDTSPointReader *GetLayerPointReader( int );
    SDTSPolygonReader *GetLayerPolygonReader( int );
    SDTSAttrReader *GetLayerAttrReader( int );
    SDTSRasterReader *GetLayerRasterReader( int );
    DDFModule	*GetLayerModuleReader( int );

    SDTS_CATD	*GetCATD() { return &oCATD ; }
    SDTS_IREF	*GetIREF() { return &oIREF; }
    SDTS_XREF   *GetXREF() { return &oXREF; }
                
  private:

    SDTS_CATD	oCATD;
    SDTS_IREF	oIREF;
    SDTS_XREF   oXREF;

    int		nLayers;
    int		*panLayerCATDEntry;
};

#endif /* ndef SDTS_AL_H_INCLUDED */
