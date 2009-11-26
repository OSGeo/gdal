/******************************************************************************
 * $Id: ogr_csv.h 17495 2009-08-02 11:44:13Z rouault $
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

class OGRDXFDataSource;

/************************************************************************/
/*                             OGRDXFLayer                              */
/************************************************************************/
class OGRDXFDataSource;

class OGRDXFLayer : public OGRLayer
{
    OGRDXFDataSource   *poDS;

    OGRFeatureDefn     *poFeatureDefn;
    int                 iNextFID;

    void                TranslateGenericProperty( OGRFeature *poFeature, 
                                                  int nCode, char *pszValue );

    OGRFeature *        TranslatePOINT();
    OGRFeature *        TranslateLINE();
    OGRFeature *        TranslatePOLYLINE();
    OGRFeature *        TranslateLWPOLYLINE();
    OGRFeature *        TranslateCIRCLE();
    OGRFeature *        TranslateINSERT();

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

    std::map<CPLString,OGRGeometry*> oBlockMap;

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
    OGRGeometry        *LookupBlock( const char *pszName );
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
};

#endif /* ndef _OGR_DXF_H_INCLUDED */
