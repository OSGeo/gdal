/******************************************************************************
 * $Id$
 *
 * Project:  SDTS Translator
 * Purpose:  Include file for entire SDTS Abstraction Layer functions.
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
 * Revision 1.26  2006/04/10 16:34:18  fwarmerdam
 * updated contact info
 *
 * Revision 1.25  2001/09/10 19:27:36  warmerda
 * added GetMinMax() and raster data types
 *
 * Revision 1.24  2001/01/19 21:20:29  warmerda
 * expanded tabs
 *
 * Revision 1.23  2000/02/03 19:44:51  warmerda
 * added method for getting bounds of transfer
 *
 * Revision 1.22  1999/11/12 13:12:35  warmerda
 * make adfTransform public
 *
 * Revision 1.21  1999/11/04 22:52:53  warmerda
 * added dynamic ATID support
 *
 * Revision 1.20  1999/10/01 14:49:42  warmerda
 * added Getmodule()
 *
 * Revision 1.19  1999/09/22 13:35:31  warmerda
 * added SDTSPolygonReader::AssembleRings()
 *
 * Revision 1.18  1999/09/21 17:25:45  warmerda
 * Added DDSH fields to raster reader.
 * Moved SADR reading onto SDTS_IREF.
 *
 * Revision 1.17  1999/09/21 02:23:25  warmerda
 * added logic to put outer ring first, and set ring direction
 *
 * Revision 1.16  1999/09/03 19:03:45  warmerda
 * fixed up help
 *
 * Revision 1.15  1999/09/03 13:34:45  warmerda
 * added duplicate flag on GetNextRecord
 *
 * Revision 1.14  1999/09/03 13:01:39  warmerda
 * added docs
 *
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

#define SDTS_SIZEOF_SADR        8

char **SDTSScanModuleReferences( DDFModule *, const char * );

/************************************************************************/
/*                              SDTS_IREF                               */
/************************************************************************/

/**
  Class holding SDTS IREF (internal reference) information, internal
  coordinate system format, scaling and resolution.  This object isn't
  normally needed by applications. 
*/
class SDTS_IREF
{
    int         nDefaultSADRFormat;
    
  public:
                SDTS_IREF();
                ~SDTS_IREF();

    int         Read( const char *pszFilename );

    char        *pszXAxisName;                  /* XLBL */
    char        *pszYAxisName;                  /* YLBL */

    double      dfXScale;                       /* SFAX */
    double      dfYScale;                       /* SFAY */

    double      dfXOffset;                      /* XORG */
    double      dfYOffset;                      /* YORG */

    double      dfXRes;                         /* XHRS */
    double      dfYRes;                         /* YHRS */

    char        *pszCoordinateFormat;           /* HFMT */

    int         GetSADRCount( DDFField * );
    int         GetSADR( DDFField *, int, double *, double *, double * );
};

/************************************************************************/
/*                              SDTS_XREF                               */
/************************************************************************/

/**
  Class for reading the XREF (external reference) module containing the
  data projection definition.
*/

class SDTS_XREF
{
  public:
                SDTS_XREF();
                ~SDTS_XREF();

    int         Read( const char *pszFilename );

    /** Projection system name, from the RSNM field.  One of GEO, SPCS, UTM,
        UPS, OTHR, UNSP. */
    char        *pszSystemName;         
                                  

    /** Horizontal datum name, from the HDAT field.  One of NAS, NAX, WGA,
        WGB, WGC, WGE. */
    char        *pszDatum;              
                                  
    /** Zone number for UTM and SPCS projections, from the ZONE field. */
    int         nZone;                        
};

/************************************************************************/
/*                              SDTS_CATD                               */
/************************************************************************/
class SDTS_CATDEntry;

/**
  List of feature layer types.  See SDTSTransfer::GetLayerType().
  */

typedef enum {
  SLTUnknown,
  SLTPoint,
  SLTLine,
  SLTAttr,
  SLTPoly,
  SLTRaster
} SDTSLayerType;

/**
  Class for accessing the CATD (Catalog Directory) file containing a list of
  all other files (modules) in the transfer.
*/
class SDTS_CATD
{
    char        *pszPrefixPath;

    int         nEntries;
    SDTS_CATDEntry **papoEntries;
    
  public:
                SDTS_CATD();
                ~SDTS_CATD();

    int         Read( const char * pszFilename );

    const char  *GetModuleFilePath( const char * pszModule );

    int         GetEntryCount() { return nEntries; }
    const char * GetEntryModule(int);
    const char * GetEntryTypeDesc(int);
    const char * GetEntryFilePath(int);
    SDTSLayerType GetEntryType(int);
};

/************************************************************************/
/*                              SDTSModId                               */
/************************************************************************/

/**
  Object representing a unique module/record identifier within an SDTS
  transfer.
*/
class SDTSModId
{
  public:
                SDTSModId() { szModule[0] = '\0';
                              nRecord = -1;
                              szOBRP[0] = '\0'; }

    int         Set( DDFField * );

    const char *GetName();

    /** The module name, such as PC01, containing the indicated record. */
    char        szModule[8];

    /** The record within the module referred to.  This is -1 for unused
        SDTSModIds. */
    long        nRecord;

    /** The "role" of this record within the module.  This is normally empty
        for references, but set in the oModId member of a feature.  */
    char        szOBRP[8]; 
};

/************************************************************************/
/*                             SDTSFeature                              */
/************************************************************************/

/**
  Base class for various SDTS features classes, providing a generic
  module identifier, and list of attribute references.
*/
class SDTSFeature
{
public:

                        SDTSFeature();
    virtual            ~SDTSFeature();

    /** Unique identifier for this record/feature within transfer. */
    SDTSModId           oModId;

    /** Number of attribute links (aoATID[]) on this feature. */
    int         nAttributes;

    /** List of nAttributes attribute record identifiers related to this
        feature.  */
    SDTSModId   *paoATID;

    void        ApplyATID( DDFField * );


    /** Dump reable description of feature to indicated stream. */
    virtual void Dump( FILE * ) = 0;
};

/************************************************************************/
/*                          SDTSIndexedReader                           */
/************************************************************************/

/**
  Base class for all the SDTSFeature type readers.  Provides feature
  caching semantics and fetching based on a record number.
  */

class SDTSIndexedReader
{
    int                 nIndexSize;
    SDTSFeature       **papoFeatures;

    int                 iCurrentFeature;

protected:    
    DDFModule           oDDFModule;

public:
                        SDTSIndexedReader();
    virtual            ~SDTSIndexedReader();
    
    virtual SDTSFeature  *GetNextRawFeature() = 0;
    
    SDTSFeature        *GetNextFeature();

    virtual void        Rewind();
    
    void                FillIndex();
    void                ClearIndex();
    int                 IsIndexed();

    SDTSFeature        *GetIndexedFeatureRef( int );
    char **             ScanModuleReferences( const char * = "ATID" );

    DDFModule          *GetModule() { return &oDDFModule; }
};


/************************************************************************/
/*                             SDTSRawLine                              */
/************************************************************************/

/** SDTS line feature, as read from LE* modules by SDTSLineReader. */

class SDTSRawLine : public SDTSFeature
{
  public:
                SDTSRawLine();
    virtual    ~SDTSRawLine();

    int         Read( SDTS_IREF *, DDFRecord * );

    /** Number of vertices in the padfX, padfY and padfZ arrays. */
    int         nVertices;

    /** List of nVertices X coordinates. */
    double      *padfX;
    /** List of nVertices Y coordinates. */
    double      *padfY;
    /** List of nVertices Z coordinates - currently always zero. */
    double      *padfZ;

    /** Identifier of polygon to left of this line.  This is the SDTS PIDL
        subfield. */
    SDTSModId   oLeftPoly;             

    /** Identifier of polygon to right of this line.  This is the SDTS PIDR
        subfield. */
    SDTSModId   oRightPoly;

    /** Identifier for the start node of this line.  This is the SDTS SNID
        subfield. */
    SDTSModId   oStartNode;             /* SNID */

    /** Identifier for the end node of this line.  This is the SDTS ENID
        subfield. */
    SDTSModId   oEndNode;               /* ENID */

    void        Dump( FILE * );
};

/************************************************************************/
/*                            SDTSLineReader                            */
/*                                                                      */
/*      Class for reading any of the files lines.                       */
/************************************************************************/

/**
  Reader for SDTS line modules.

  Returns SDTSRawLine features. Normally readers are instantiated with
  the SDTSTransfer::GetIndexedReader() method.

  */

class SDTSLineReader : public SDTSIndexedReader
{
    SDTS_IREF   *poIREF;
    
  public:
                SDTSLineReader( SDTS_IREF * );
                ~SDTSLineReader();

    int         Open( const char * );
    SDTSRawLine *GetNextLine( void );
    void        Close();
    
    SDTSFeature *GetNextRawFeature( void ) { return GetNextLine(); }

    void        AttachToPolygons( SDTSTransfer * );
};

/************************************************************************/
/*                            SDTSAttrRecord                            */
/************************************************************************/

/**
  SDTS attribute record feature, as read from A* modules by
  SDTSAttrReader.

  Note that even though SDTSAttrRecord is derived from SDTSFeature, there
  are never any attribute records associated with attribute records using
  the aoATID[] mechanism.  SDTSFeature::nAttributes will always be zero.
  */

class SDTSAttrRecord : public SDTSFeature
{
  public:
                SDTSAttrRecord();
    virtual    ~SDTSAttrRecord();

    /** The entire DDFRecord read from the file. */
    DDFRecord   *poWholeRecord;

    /** The ATTR DDFField with the user attribute.  Each subfield is a
        attribute value. */

    DDFField    *poATTR;

    virtual void Dump( FILE * );
};

/************************************************************************/
/*                            SDTSAttrReader                            */
/************************************************************************/

/**
  Class for reading SDTSAttrRecord features from a primary or secondary
  attribute module.
  */

class SDTSAttrReader : public SDTSIndexedReader
{
    SDTS_IREF   *poIREF;

    int         bIsSecondary;
    
  public:
                SDTSAttrReader( SDTS_IREF * );
   virtual     ~SDTSAttrReader();

    int         Open( const char * );
    DDFField    *GetNextRecord( SDTSModId * = NULL,
                                DDFRecord ** = NULL,
                                int bDuplicate = FALSE );
    SDTSAttrRecord *GetNextAttrRecord();
    void        Close();

    /**
      Returns TRUE if this is a Attribute Secondary layer rather than
      an Attribute Primary layer.
      */
    int         IsSecondary() { return bIsSecondary; }
    
    SDTSFeature *GetNextRawFeature( void ) { return GetNextAttrRecord(); }
};

/************************************************************************/
/*                             SDTSRawPoint                             */
/************************************************************************/

/**
  Object containing a point feature (type NA, NO or NP).
  */
class SDTSRawPoint : public SDTSFeature
{
  public:
                SDTSRawPoint();
    virtual    ~SDTSRawPoint();

    int         Read( SDTS_IREF *, DDFRecord * );

    /** X coordinate of point. */
    double      dfX;
    /** Y coordinate of point. */
    double      dfY;
    /** Z coordinate of point. */
    double      dfZ;

    /** Optional identifier of area marked by this point (ie. PC01:27). */
    SDTSModId   oAreaId;                /* ARID */

    virtual void Dump( FILE * );
};

/************************************************************************/
/*                           SDTSPointReader                            */
/************************************************************************/

/**
  Class for reading SDTSRawPoint features from a point module (type NA, NO
  or NP).
  */

class SDTSPointReader : public SDTSIndexedReader
{
    SDTS_IREF   *poIREF;
    
  public:
                SDTSPointReader( SDTS_IREF * );
    virtual    ~SDTSPointReader();

    int         Open( const char * );
    SDTSRawPoint *GetNextPoint( void );
    void        Close();

    SDTSFeature *GetNextRawFeature( void ) { return GetNextPoint(); }
};

/************************************************************************/
/*                             SDTSRawPolygon                           */
/************************************************************************/

/**
  Class for holding information about a polygon feature.

  When directly read from a polygon module, the polygon has no concept
  of it's geometry.  Just it's ID, and references to attribute records.
  However, if the SDTSLineReader::AttachToPolygons() method is called on
  the module containing the lines forming the polygon boundaries, then the
  nEdges/papoEdges information on the SDTSRawPolygon will be filled in.

  Once this is complete the AssembleRings() method can be used to fill in the
  nRings/nVertices/panRingStart/padfX/padfY/padfZ information defining the
  ring geometry.

  Note that the rings may not appear in any particular order, nor with any
  meaningful direction (clockwise or counterclockwise).  
  */

class SDTSRawPolygon : public SDTSFeature
{
    void        AddEdgeToRing( int, double *, double *, double *, int, int );
    
  public:
                SDTSRawPolygon();
    virtual    ~SDTSRawPolygon();

    int         Read( DDFRecord * );

    int         nEdges;
    SDTSRawLine **papoEdges;

    void        AddEdge( SDTSRawLine * );

    /** This method will assemble the edges associated with a polygon into
      rings, returning FALSE if problems are encountered during assembly. */
    int         AssembleRings();

    /** Number of rings in assembled polygon. */
    int         nRings;
    /** Total number of vertices in all rings of assembled polygon. */
    int         nVertices;
    /** Offsets into padfX/padfY/padfZ for the beginning of each ring in the
      polygon.  This array is nRings long. */
    int         *panRingStart;

    /** List of nVertices X coordinates for the polygon (split over multiple
      rings via panRingStart. */
    double      *padfX;
    /** List of nVertices Y coordinates for the polygon (split over multiple
      rings via panRingStart. */
    double      *padfY;
    /** List of nVertices Z coordinates for the polygon (split over multiple
      rings via panRingStart.  The values are almost always zero. */
    double      *padfZ;

    virtual void Dump( FILE * );
};

/************************************************************************/
/*                          SDTSPolygonReader                           */
/************************************************************************/

/** Class for reading SDTSRawPolygon features from a polygon (PC*) module. */

class SDTSPolygonReader : public SDTSIndexedReader
{
    int         bRingsAssembled;
    
  public:
                SDTSPolygonReader();
    virtual    ~SDTSPolygonReader();

    int         Open( const char * );
    SDTSRawPolygon *GetNextPolygon( void );
    void        Close();

    SDTSFeature *GetNextRawFeature( void ) { return GetNextPolygon(); }

    void        AssembleRings( SDTSTransfer * );
};

/************************************************************************/
/*                           SDTSRasterReader                           */
/************************************************************************/

/**
  Class for reading raster data from a raster layer.

  This class is somewhat unique amoung the reader classes in that it isn't
  derived from SDTSIndexedFeature, and it doesn't return "features".  Instead
  it is used to read raster blocks, in the natural block size of the dataset.
  */

class SDTSRasterReader
{
    DDFModule   oDDFModule;

    char        szModule[20];

    int         nXSize;
    int         nYSize;
    int         nXBlockSize;
    int         nYBlockSize;

    int         nXStart;                /* SOCI */
    int         nYStart;                /* SORI */

    double      adfTransform[6];
    
  public:
    char        szINTR[4];              /* CE is center, TL is top left */
    char        szFMT[32];
    char        szUNITS[64];
    char        szLabel[64];

                SDTSRasterReader();
                ~SDTSRasterReader();

    int         Open( SDTS_CATD * poCATD, SDTS_IREF *,
                      const char * pszModule  );
    void        Close();

    int         GetRasterType();        /* 1 = int16, see GDAL types */
#define SDTS_RT_INT16   1   
#define SDTS_RT_FLOAT32 6

    int         GetTransform( double * );

    int         GetMinMax( double * pdfMin, double * pdfMax,
                           double dfNoData );

    /**
      Fetch the raster width.

      @return the width in pixels.
      */
    int         GetXSize() { return nXSize; }
    /**
      Fetch the raster height.

      @return the height in pixels.
      */
    int         GetYSize() { return nYSize; }

    /** Fetch the width of a source block (usually same as raster width). */
    int         GetBlockXSize() { return nXBlockSize; }
    /** Fetch the height of a source block (usually one). */
    int         GetBlockYSize() { return nYBlockSize; }

    int         GetBlock( int nXOffset, int nYOffset, void * pData );
};

/************************************************************************/
/*                             SDTSTransfer                             */
/************************************************************************/

/**
  Master class representing an entire SDTS transfer.

  This class is used to open the transfer, to get a list of available
  feature layers, and to instantiate readers for those layers.

  */

class SDTSTransfer
{
  public:
                SDTSTransfer();
                ~SDTSTransfer();

    int         Open( const char * );
    void        Close();

    int         FindLayer( const char * );
    int         GetLayerCount() { return nLayers; }
    SDTSLayerType GetLayerType( int );
    int         GetLayerCATDEntry( int );

    SDTSLineReader *GetLayerLineReader( int );
    SDTSPointReader *GetLayerPointReader( int );
    SDTSPolygonReader *GetLayerPolygonReader( int );
    SDTSAttrReader *GetLayerAttrReader( int );
    SDTSRasterReader *GetLayerRasterReader( int );
    DDFModule   *GetLayerModuleReader( int );

    SDTSIndexedReader *GetLayerIndexedReader( int );

    /**
      Fetch the catalog object for this transfer.

      @return pointer to the internally managed SDTS_CATD for the transfer.
      */
    SDTS_CATD   *GetCATD() { return &oCATD ; }

    SDTS_IREF   *GetIREF() { return &oIREF; }

    /**
      Fetch the external reference object for this transfer.

      @return pointer to the internally managed SDTS_XREF for the transfer.
      */
    SDTS_XREF   *GetXREF() { return &oXREF; }

    SDTSFeature *GetIndexedFeatureRef( SDTSModId *,
                                       SDTSLayerType *peType = NULL);
                
    DDFField *GetAttr( SDTSModId * );

    int          GetBounds( double *pdfMinX, double *pdfMinY,
                            double *pdfMaxX, double *pdfMaxY );
    
  private:

    SDTS_CATD   oCATD;
    SDTS_IREF   oIREF;
    SDTS_XREF   oXREF;

    int         nLayers;
    int         *panLayerCATDEntry;
    SDTSIndexedReader **papoLayerReader;
};

#endif /* ndef SDTS_AL_H_INCLUDED */
