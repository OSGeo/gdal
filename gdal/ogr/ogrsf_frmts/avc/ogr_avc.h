/******************************************************************************
 * $Id$
 *
 * Project:  Arc/Info Coverage (E00 & Binary) Reader
 * Purpose:  Declarations for OGR wrapper classes for coverage access.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2002, Frank Warmerdam
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
 ****************************************************************************/

#ifndef _OGR_AVC_H_INCLUDED
#define _OGR_AVC_H_INCLUDED

#include "ogrsf_frmts.h"
#include "avc.h"

class OGRAVCDataSource;

/************************************************************************/
/*                             OGRAVCLayer                              */
/************************************************************************/

class OGRAVCLayer : public OGRLayer
{
  protected:
    OGRFeatureDefn      *poFeatureDefn;

    OGRAVCDataSource    *poDS;
    
    AVCFileType         eSectionType;

    int                 SetupFeatureDefinition( const char *pszName ); 
    int                 AppendTableDefinition( AVCTableDef *psTableDef );

    int                 MatchesSpatialFilter( void * );
    OGRFeature          *TranslateFeature( void * );

    int                 TranslateTableFields( OGRFeature *poFeature, 
                                              int nFieldBase, 
                                              AVCTableDef *psTableDef,
                                              AVCField *pasFields );

  public:
                        OGRAVCLayer( AVCFileType eSectionType, 
                                     OGRAVCDataSource *poDS );
    			~OGRAVCLayer();

    OGRFeatureDefn *	GetLayerDefn() { return poFeatureDefn; }

    virtual OGRSpatialReference *GetSpatialRef();
    
    virtual int         TestCapability( const char * );
};

/************************************************************************/
/*                         OGRAVCDataSource                             */
/************************************************************************/

class OGRAVCDataSource : public OGRDataSource
{
  protected:
    OGRSpatialReference *poSRS;
    char		*pszCoverageName;

  public:
		        OGRAVCDataSource();
    			~OGRAVCDataSource();

    virtual OGRSpatialReference *GetSpatialRef();

    const char          *GetCoverageName();
};

/* ==================================================================== */
/*      Binary Coverage Classes                                         */
/* ==================================================================== */

class OGRAVCBinDataSource;

/************************************************************************/
/*                            OGRAVCBinLayer                            */
/************************************************************************/

class OGRAVCBinLayer : public OGRAVCLayer
{
    AVCE00Section       *psSection;
    AVCBinFile          *hFile;

    OGRAVCBinLayer      *poArcLayer;
    int                 bNeedReset;

    char		szTableName[128];
    AVCBinFile          *hTable;
    int                 nTableBaseField;
    int                 nTableAttrIndex;

    int                 nNextFID;

    int                 FormPolygonGeometry( OGRFeature *poFeature,
                                             AVCPal *psPAL );

    int                 CheckSetupTable();
    int                 AppendTableFields( OGRFeature *poFeature );

  public:
                        OGRAVCBinLayer( OGRAVCBinDataSource *poDS,
                                        AVCE00Section *psSectionIn );

    			~OGRAVCBinLayer();

    void		ResetReading();
    OGRFeature *	GetNextFeature();
    OGRFeature *	GetFeature( long nFID );

    int                 TestCapability( const char * );
};

/************************************************************************/
/*                         OGRAVCBinDataSource                          */
/************************************************************************/

class OGRAVCBinDataSource : public OGRAVCDataSource
{
    OGRLayer            **papoLayers;
    int			nLayers;
    
    char		*pszName;

    AVCE00ReadPtr       psAVC;
    
  public:
    			OGRAVCBinDataSource();
    			~OGRAVCBinDataSource();

    int			Open( const char *, int bTestOpen );

    const char	        *GetName() { return pszName; }
    int			GetLayerCount() { return nLayers; }
    OGRLayer		*GetLayer( int );

    int                 TestCapability( const char * );

    AVCE00ReadPtr       GetInfo() { return psAVC; }
};

/************************************************************************/
/*                           OGRAVCBinDriver                            */
/************************************************************************/

class OGRAVCBinDriver : public OGRSFDriver
{
  public:
    		~OGRAVCBinDriver();
                
    const char *GetName();
    OGRDataSource *Open( const char *, int );

    int                 TestCapability( const char * );
};

/* ==================================================================== */
/*      E00 (ASCII) Coverage Classes                                    */
/* ==================================================================== */

/************************************************************************/
/*                            OGRAVCE00Layer                            */
/************************************************************************/
class OGRAVCE00Layer : public OGRAVCLayer
{
    AVCE00Section       *psSection;
    AVCE00ReadE00Ptr    psRead;
    OGRAVCE00Layer      *poArcLayer;
    int                 nFeatureCount;
    int                 bNeedReset;
    int                 nNextFID;

    AVCE00Section       *psTableSection;
    AVCE00ReadE00Ptr    psTableRead;
    char                *pszTableFilename;
    int                 nTablePos;
    int                 nTableBaseField;
    int                 nTableAttrIndex;

    int                 FormPolygonGeometry( OGRFeature *poFeature,
                                             AVCPal *psPAL );
  public:
                        OGRAVCE00Layer( OGRAVCDataSource *poDS,
                                        AVCE00Section *psSectionIn );

    			~OGRAVCE00Layer();

    void		ResetReading();
    OGRFeature *	GetNextFeature();
    OGRFeature *GetFeature( long nFID );
    int GetFeatureCount(int bForce);
    int CheckSetupTable(AVCE00Section *psTblSectionIn);
    int AppendTableFields( OGRFeature *poFeature );
};

/************************************************************************/
/*                         OGRAVCE00DataSource                          */
/************************************************************************/

class OGRAVCE00DataSource : public OGRAVCDataSource
{
    int nLayers;
    char *pszName;
    AVCE00ReadE00Ptr psE00;
    OGRAVCE00Layer **papoLayers;

  protected:
    int CheckAddTable(AVCE00Section *psTblSection);

  public:
    OGRAVCE00DataSource();
    ~OGRAVCE00DataSource();

    int Open(const char *, int bTestOpen);

    AVCE00ReadE00Ptr GetInfo() { return psE00; }
    const char *GetName() { return pszName; }
    int GetLayerCount() { return nLayers; }

    OGRLayer *GetLayer( int );
    int TestCapability( const char * );
    virtual OGRSpatialReference *GetSpatialRef();
};

/************************************************************************/
/*                           OGRAVCE00Driver                            */
/************************************************************************/

class OGRAVCE00Driver : public OGRSFDriver
{
  public:
    		~OGRAVCE00Driver();
                
    const char *GetName();
    OGRDataSource *Open( const char *, int );

    int                 TestCapability( const char * );
};

#endif /* _OGR_AVC_H_INCLUDED */
