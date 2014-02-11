/******************************************************************************
 * $Id$
 *
 * Project:  DXF Translator
 * Purpose:  Definition of classes for OGR .dxf driver.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2009,  Frank Warmerdam
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
 ****************************************************************************/

#ifndef _OGR_DXF_H_INCLUDED
#define _OGR_DXF_H_INCLUDED

#include "ogrsf_frmts.h"
#include "ogr_autocad_services.h"
#include "cpl_conv.h"
#include <vector>
#include <map>
#include <set>
#include <queue>

class OGRDXFDataSource;

/************************************************************************/
/*                          DXFBlockDefinition                          */
/*                                                                      */
/*      Container for info about a block.                               */
/************************************************************************/

class DXFBlockDefinition
{
public:
    DXFBlockDefinition() : poGeometry(NULL) {}
    ~DXFBlockDefinition();

    OGRGeometry                *poGeometry;
    std::vector<OGRFeature *>  apoFeatures;
};

/************************************************************************/
/*                         OGRDXFBlocksLayer()                          */
/************************************************************************/

class OGRDXFBlocksLayer : public OGRLayer
{
    OGRDXFDataSource   *poDS;

    OGRFeatureDefn     *poFeatureDefn;
    
    int                 iNextFID;
    unsigned int        iNextSubFeature;

    std::map<CPLString,DXFBlockDefinition>::iterator oIt;

  public:
    OGRDXFBlocksLayer( OGRDXFDataSource *poDS );
    ~OGRDXFBlocksLayer();

    void                ResetReading();
    OGRFeature *        GetNextFeature();

    OGRFeatureDefn *    GetLayerDefn() { return poFeatureDefn; }

    int                 TestCapability( const char * );

    OGRFeature *        GetNextUnfilteredFeature();
};

/************************************************************************/
/*                             OGRDXFLayer                              */
/************************************************************************/
class OGRDXFLayer : public OGRLayer
{
    OGRDXFDataSource   *poDS;

    OGRFeatureDefn     *poFeatureDefn;
    int                 iNextFID;

    std::set<CPLString> oIgnoredEntities;

    std::queue<OGRFeature*> apoPendingFeatures;
    void                ClearPendingFeatures();

    std::map<CPLString,CPLString> oStyleProperties;
    
    void                TranslateGenericProperty( OGRFeature *poFeature, 
                                                  int nCode, char *pszValue );
    void                PrepareLineStyle( OGRFeature *poFeature );
    void                ApplyOCSTransformer( OGRGeometry * );

    OGRFeature *        TranslatePOINT();
    OGRFeature *        TranslateLINE();
    OGRFeature *        TranslatePOLYLINE();
    OGRFeature *        TranslateLWPOLYLINE();
    OGRFeature *        TranslateCIRCLE();
    OGRFeature *        TranslateELLIPSE();
    OGRFeature *        TranslateARC();
    OGRFeature *        TranslateSPLINE();
    OGRFeature *        Translate3DFACE();
    OGRFeature *        TranslateINSERT();
    OGRFeature *        TranslateMTEXT();
    OGRFeature *        TranslateTEXT();
    OGRFeature *        TranslateDIMENSION();
    OGRFeature *        TranslateHATCH();
    OGRFeature *        TranslateSOLID();

    void                FormatDimension( CPLString &osText, double dfValue );
    OGRErr              CollectBoundaryPath( OGRGeometryCollection * );
    OGRErr              CollectPolylinePath( OGRGeometryCollection * );

    CPLString           TextUnescape( const char * );

  public:
    OGRDXFLayer( OGRDXFDataSource *poDS );
    ~OGRDXFLayer();

    void                ResetReading();
    OGRFeature *        GetNextFeature();

    OGRFeatureDefn *    GetLayerDefn() { return poFeatureDefn; }

    int                 TestCapability( const char * );

    OGRFeature *        GetNextUnfilteredFeature();
};

/************************************************************************/
/*                             OGRDXFReader                             */
/*                                                                      */
/*      A class for very low level DXF reading without interpretation.  */
/************************************************************************/

class OGRDXFReader
{
public:
    OGRDXFReader();
    ~OGRDXFReader();
    
    void                Initialize( VSILFILE * fp );
    
    VSILFILE           *fp;

    int                 iSrcBufferOffset;
    int                 nSrcBufferBytes;
    int                 iSrcBufferFileOffset;
    char                achSrcBuffer[1025];

    int                 nLastValueSize;
    int                 nLineNumber;

    int                 ReadValue( char *pszValueBuffer, 
                                   int nValueBufferSize = 81 );
    void                UnreadValue();
    void                LoadDiskChunk();
    void                ResetReadPointer( int iNewOffset );
};


/************************************************************************/
/*                           OGRDXFDataSource                           */
/************************************************************************/

class OGRDXFDataSource : public OGRDataSource
{
    VSILFILE           *fp;

    CPLString           osName;
    std::vector<OGRLayer*> apoLayers;

    int                 iEntitiesSectionOffset;

    std::map<CPLString,DXFBlockDefinition> oBlockMap;
    std::map<CPLString,CPLString> oHeaderVariables;

    CPLString           osEncoding;

    // indexed by layer name, then by property name.
    std::map< CPLString, std::map<CPLString,CPLString> > 
                        oLayerTable;

    std::map<CPLString,CPLString> oLineTypeTable;

    int                 bInlineBlocks;

    OGRDXFReader        oReader;

  public:
                        OGRDXFDataSource();
                        ~OGRDXFDataSource();

    int                 Open( const char * pszFilename, int bHeaderOnly=FALSE );

    const char          *GetName() { return osName; }

    int                 GetLayerCount() { return apoLayers.size(); }
    OGRLayer            *GetLayer( int );

    int                 TestCapability( const char * );

    // The following is only used by OGRDXFLayer

    int                 InlineBlocks() { return bInlineBlocks; }
    void                AddStandardFields( OGRFeatureDefn *poDef );

    // Implemented in ogrdxf_blockmap.cpp
    void                ReadBlocksSection();
    OGRGeometry        *SimplifyBlockGeometry( OGRGeometryCollection * );
    DXFBlockDefinition *LookupBlock( const char *pszName );
    std::map<CPLString,DXFBlockDefinition> &GetBlockMap() { return oBlockMap; }

    // Layer and other Table Handling (ogrdatasource.cpp)
    void                ReadTablesSection();
    void                ReadLayerDefinition();
    void                ReadLineTypeDefinition();
    const char         *LookupLayerProperty( const char *pszLayer, 
                                             const char *pszProperty );
    const char         *LookupLineType( const char *pszName );

    // Header variables. 
    void                ReadHeaderSection();
    const char         *GetVariable(const char *pszName, 
                                    const char *pszDefault=NULL );

    const char         *GetEncoding() { return osEncoding; }

    // reader related.
    int  ReadValue( char *pszValueBuffer, int nValueBufferSize = 81 )
        { return oReader.ReadValue( pszValueBuffer, nValueBufferSize ); }
    void RestartEntities() 
        { oReader.ResetReadPointer(iEntitiesSectionOffset); }
    void UnreadValue()
        { oReader.UnreadValue(); }
    void ResetReadPointer( int iNewOffset )
        { oReader.ResetReadPointer( iNewOffset ); }
};

/************************************************************************/
/*                          OGRDXFWriterLayer                           */
/************************************************************************/

class OGRDXFWriterDS;

class OGRDXFWriterLayer : public OGRLayer
{
    VSILFILE           *fp;
    OGRFeatureDefn     *poFeatureDefn;

    OGRDXFWriterDS     *poDS;

    int                 WriteValue( int nCode, const char *pszValue );
    int                 WriteValue( int nCode, int nValue );
    int                 WriteValue( int nCode, double dfValue );

    OGRErr              WriteCore( OGRFeature* );
    OGRErr              WritePOINT( OGRFeature* );
    OGRErr              WriteTEXT( OGRFeature* );
    OGRErr              WritePOLYLINE( OGRFeature*, OGRGeometry* = NULL );
    OGRErr              WriteHATCH( OGRFeature*, OGRGeometry* = NULL );
    OGRErr              WriteINSERT( OGRFeature* );

    static CPLString    TextEscape( const char * );
    int                 ColorStringToDXFColor( const char * );
    CPLString           PrepareLineTypeDefinition( OGRFeature*, OGRStyleTool* );

    std::map<CPLString,CPLString> oNewLineTypes;
    int                 nNextAutoID;
    int                 bWriteHatch;

  public:
    OGRDXFWriterLayer( OGRDXFWriterDS *poDS, VSILFILE *fp );
    ~OGRDXFWriterLayer();

    void                ResetReading() {}
    OGRFeature         *GetNextFeature() { return NULL; }

    OGRFeatureDefn *    GetLayerDefn() { return poFeatureDefn; }

    int                 TestCapability( const char * );
    OGRErr              CreateFeature( OGRFeature *poFeature );
    OGRErr              CreateField( OGRFieldDefn *poField,
                                     int bApproxOK = TRUE );

    void                ResetFP( VSILFILE * );

    std::map<CPLString,CPLString>& GetNewLineTypeMap() { return oNewLineTypes;}
};

/************************************************************************/
/*                       OGRDXFBlocksWriterLayer                        */
/************************************************************************/

class OGRDXFBlocksWriterLayer : public OGRLayer
{
    OGRFeatureDefn     *poFeatureDefn;

  public:
    OGRDXFBlocksWriterLayer( OGRDXFWriterDS *poDS );
    ~OGRDXFBlocksWriterLayer();

    void                ResetReading() {}
    OGRFeature         *GetNextFeature() { return NULL; }

    OGRFeatureDefn *    GetLayerDefn() { return poFeatureDefn; }

    int                 TestCapability( const char * );
    OGRErr              CreateFeature( OGRFeature *poFeature );
    OGRErr              CreateField( OGRFieldDefn *poField,
                                     int bApproxOK = TRUE );

    std::vector<OGRFeature*> apoBlocks;
    OGRFeature          *FindBlock( const char * );
};

/************************************************************************/
/*                           OGRDXFWriterDS                             */
/************************************************************************/

class OGRDXFWriterDS : public OGRDataSource
{
    friend class OGRDXFWriterLayer;
    
    int                 nNextFID;

    CPLString           osName;
    OGRDXFWriterLayer  *poLayer;
    OGRDXFBlocksWriterLayer *poBlocksLayer;
    VSILFILE           *fp;
    CPLString           osTrailerFile;

    CPLString           osTempFilename;
    VSILFILE           *fpTemp;

    CPLString           osHeaderFile;
    OGRDXFDataSource    oHeaderDS;
    char               **papszLayersToCreate;

    vsi_l_offset        nHANDSEEDOffset;

    std::vector<int>    anDefaultLayerCode;
    std::vector<CPLString> aosDefaultLayerText;

    std::set<CPLString> aosUsedEntities;
    void                ScanForEntities( const char *pszFilename,
                                         const char *pszTarget );

    int                 WriteNewLineTypeRecords( VSILFILE *fp );
    int                 WriteNewBlockRecords( VSILFILE * );
    int                 WriteNewBlockDefinitions( VSILFILE * );
    int                 WriteNewLayerDefinitions( VSILFILE * );
    int                 TransferUpdateHeader( VSILFILE * );
    int                 TransferUpdateTrailer( VSILFILE * );
    int                 FixupHANDSEED( VSILFILE * );

    OGREnvelope         oGlobalEnvelope;

  public:
                        OGRDXFWriterDS();
                        ~OGRDXFWriterDS();

    int                 Open( const char * pszFilename, 
                              char **papszOptions );
    
    const char          *GetName() { return osName; }

    int                 GetLayerCount();
    OGRLayer            *GetLayer( int );

    int                 TestCapability( const char * );

    OGRLayer           *CreateLayer( const char *pszName, 
                                     OGRSpatialReference *poSpatialRef = NULL,
                                     OGRwkbGeometryType eGType = wkbUnknown,
                                     char ** papszOptions = NULL );

    int                 CheckEntityID( const char *pszEntityID );
    long                WriteEntityID( VSILFILE * fp,
                                       long nPreferredFID = OGRNullFID );

    void                UpdateExtent( OGREnvelope* psEnvelope );
};

/************************************************************************/
/*                             OGRDXFDriver                             */
/************************************************************************/

class OGRDXFDriver : public OGRSFDriver
{
  public:
                ~OGRDXFDriver();

    const char *GetName();
    OGRDataSource *Open( const char *, int );
    int         TestCapability( const char * );

    OGRDataSource      *CreateDataSource( const char *pszName,
                                          char ** = NULL );
};


#endif /* ndef _OGR_DXF_H_INCLUDED */
