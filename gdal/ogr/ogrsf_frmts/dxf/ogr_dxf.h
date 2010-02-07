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
#include "cpl_conv.h"
#include <vector>
#include <map>
#include <stack>

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
/*                             OGRDXFLayer                              */
/************************************************************************/
class OGRDXFDataSource;

class OGRDXFLayer : public OGRLayer
{
    OGRDXFDataSource   *poDS;

    OGRFeatureDefn     *poFeatureDefn;
    int                 iNextFID;

    std::stack<OGRFeature*> apoPendingFeatures;
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
    OGRFeature *        TranslateINSERT();
    OGRFeature *        TranslateMTEXT();
    OGRFeature *        TranslateTEXT();
    OGRFeature *        TranslateDIMENSION();

    void                FormatDimension( CPLString &osText, double dfValue );

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
/*                           OGRDXFDataSource                           */
/************************************************************************/

class OGRDXFDataSource : public OGRDataSource
{
    CPLString           osName;
    std::vector<OGRDXFLayer*> apoLayers;

    FILE                *fp;

    int                 iEntitiesSectionOffset;

    int                 iSrcBufferOffset;
    int                 nSrcBufferBytes;
    int                 iSrcBufferFileOffset;
    char                achSrcBuffer[1025];
    
    int                 nLastValueSize;

    std::map<CPLString,DXFBlockDefinition> oBlockMap;
    std::map<CPLString,CPLString> oHeaderVariables;

    // indexed by layer name, then by property name.
    std::map< CPLString, std::map<CPLString,CPLString> > 
                        oLayerTable;

  public:
                        OGRDXFDataSource();
                        ~OGRDXFDataSource();

    int                 Open( const char * pszFilename );
    
    const char          *GetName() { return osName; }

    int                 GetLayerCount() { return apoLayers.size(); }
    OGRLayer            *GetLayer( int );

    int                 TestCapability( const char * );

    // The following is only used by OGRDXFLayer

    // Implemented in ogrdxf_diskio.cpp 
    int                 ReadValue( char *pszValueBuffer, 
                                   int nValueBufferSize = 81 );
    void                UnreadValue();
    void                LoadDiskChunk();
    void                ResetReadPointer( int iNewOffset );
    void                RestartEntities() 
                        { ResetReadPointer(iEntitiesSectionOffset); }

    // Implemented in ogrdxf_blockmap.cpp
    void                ReadBlocksSection();
    OGRGeometry        *SimplifyBlockGeometry( OGRGeometryCollection * );
    DXFBlockDefinition *LookupBlock( const char *pszName );

    // Layer Table Handling (ogrdxf_tables.cpp)
    void                ReadTablesSection();
    void                ReadLayerDefinition();
    const char         *LookupLayerProperty( const char *pszLayer, 
                                             const char *pszProperty );

    // Header variables. 
    void                ReadHeaderSection();
    const char         *GetVariable(const char *pszName, 
                                    const char *pszDefault=NULL );
};

/************************************************************************/
/*                          OGRDXFWriterLayer                           */
/************************************************************************/

class OGRDXFWriterDS;

class OGRDXFWriterLayer : public OGRLayer
{
    FILE               *fp;
    OGRFeatureDefn     *poFeatureDefn;
    int                 nNextFID;

    int                 WriteValue( int nCode, const char *pszValue );
    int                 WriteValue( int nCode, int nValue );
    int                 WriteValue( int nCode, double dfValue );

    OGRErr              WriteCore( OGRFeature* );
    OGRErr              WritePOINT( OGRFeature* );
    OGRErr              WriteTEXT( OGRFeature* );
    OGRErr              WritePOLYLINE( OGRFeature*, OGRGeometry* = NULL );

    int                 ColorStringToDXFColor( const char * );

  public:
    OGRDXFWriterLayer( FILE *fp );
    ~OGRDXFWriterLayer();

    void                ResetReading() {}
    OGRFeature         *GetNextFeature() { return NULL; }

    OGRFeatureDefn *    GetLayerDefn() { return poFeatureDefn; }

    int                 TestCapability( const char * );
    OGRErr              CreateFeature( OGRFeature *poFeature );
    OGRErr              CreateField( OGRFieldDefn *poField,
                                     int bApproxOK = TRUE );
};

/************************************************************************/
/*                           OGRDXFWriterDS                             */
/************************************************************************/

class OGRDXFWriterDS : public OGRDataSource
{
    CPLString           osName;
    OGRDXFWriterLayer  *poLayer;
    FILE               *fp;
    CPLString           osTrailerFile;

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

};

/************************************************************************/
/*                             OGRDXFDriver                             */
/************************************************************************/

class OGRDXFDriver : public OGRSFDriver
{
  public:
                ~OGRDXFDriver();

    static const unsigned char *GetDXFColorTable();

    const char *GetName();
    OGRDataSource *Open( const char *, int );
    int         TestCapability( const char * );

    OGRDataSource      *CreateDataSource( const char *pszName,
                                          char ** = NULL );
};

#endif /* ndef _OGR_DXF_H_INCLUDED */
