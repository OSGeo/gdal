/******************************************************************************
 * $Id$
 *
 * Project:  DXF Translator
 * Purpose:  Definition of classes for OGR .dxf driver.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2009,  Frank Warmerdam
 * Copyright (c) 2010-2013, Even Rouault <even dot rouault at mines-paris dot org>
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

#ifndef OGR_DXF_H_INCLUDED
#define OGR_DXF_H_INCLUDED

#include "ogrsf_frmts.h"
#include "ogr_autocad_services.h"
#include "cpl_conv.h"
#include <vector>
#include <map>
#include <set>
#include <queue>

class OGRDXFDataSource;
class OGRDXFFeature;

/************************************************************************/
/*                          DXFBlockDefinition                          */
/*                                                                      */
/*      Container for info about a block.                               */
/************************************************************************/

class DXFBlockDefinition
{
public:
    DXFBlockDefinition() {}
    ~DXFBlockDefinition();

    std::vector<OGRDXFFeature *> apoFeatures;
};

/************************************************************************/
/*                          OGRDXFBlocksLayer                           */
/************************************************************************/

class OGRDXFBlocksLayer : public OGRLayer
{
    OGRDXFDataSource   *poDS;

    OGRFeatureDefn     *poFeatureDefn;

    GIntBig             iNextFID;

    std::map<CPLString,DXFBlockDefinition>::iterator oIt;
    CPLString           osBlockName;

    std::queue<OGRDXFFeature *> apoPendingFeatures;

  public:
    explicit OGRDXFBlocksLayer( OGRDXFDataSource *poDS );
    ~OGRDXFBlocksLayer();

    void                ResetReading() override;
    OGRFeature *        GetNextFeature() override;

    OGRFeatureDefn *    GetLayerDefn() override { return poFeatureDefn; }

    int                 TestCapability( const char * ) override;

    OGRDXFFeature *     GetNextUnfilteredFeature();
};

/************************************************************************/
/*                       OGRDXFInsertTransformer                        */
/*                                                                      */
/*      Stores the transformation needed to insert a block reference.   */
/************************************************************************/

class OGRDXFInsertTransformer : public OGRCoordinateTransformation
{
public:
    OGRDXFInsertTransformer() :
        dfXOffset(0),dfYOffset(0),dfZOffset(0),
        dfXScale(1.0),dfYScale(1.0),dfZScale(1.0),
        dfAngle(0.0) {}

    double dfXOffset;
    double dfYOffset;
    double dfZOffset;
    double dfXScale;
    double dfYScale;
    double dfZScale;
    double dfAngle;

    OGRDXFInsertTransformer GetOffsetTransformer()
    {
        OGRDXFInsertTransformer oResult;
        oResult.dfXOffset = this->dfXOffset;
        oResult.dfYOffset = this->dfYOffset;
        oResult.dfZOffset = this->dfZOffset;
        return oResult;
    }
    OGRDXFInsertTransformer GetRotateScaleTransformer()
    {
        OGRDXFInsertTransformer oResult;
        oResult.dfXScale = this->dfXScale;
        oResult.dfYScale = this->dfYScale;
        oResult.dfZScale = this->dfZScale;
        oResult.dfAngle = this->dfAngle;
        return oResult;
    }

    OGRSpatialReference *GetSourceCS() override { return NULL; }
    OGRSpatialReference *GetTargetCS() override { return NULL; }
    int Transform( int nCount,
        double *x, double *y, double *z ) override
    { return TransformEx( nCount, x, y, z, NULL ); }

    int TransformEx( int nCount,
        double *x, double *y, double *z = NULL,
        int *pabSuccess = NULL ) override
    {
        for( int i = 0; i < nCount; i++ )
        {
            x[i] *= dfXScale;
            y[i] *= dfYScale;
            if( z )
                z[i] *= dfZScale;

            const double dfXNew = x[i] * cos(dfAngle) - y[i] * sin(dfAngle);
            const double dfYNew = x[i] * sin(dfAngle) + y[i] * cos(dfAngle);

            x[i] = dfXNew;
            y[i] = dfYNew;

            x[i] += dfXOffset;
            y[i] += dfYOffset;
            if( z )
                z[i] += dfZOffset;

            if( pabSuccess )
                pabSuccess[i] = TRUE;
        }
        return TRUE;
    }
};

/************************************************************************/
/*                              DXFTriple                               */
/*                                                                      */
/*     Represents a triple (X, Y, Z) used for various purposes in       */
/*     DXF files.  We do not use OGRPoint for this purpose, as the      */
/*     triple does not always represent a point as such (for            */
/*     example, it could contain a scale factor for each dimension).    */
/************************************************************************/
struct DXFTriple
{
    double dfX, dfY, dfZ;

    DXFTriple(): dfX(0.0), dfY(0.0), dfZ(0.0) {}
    DXFTriple( double x, double y, double z ): dfX(x), dfY(y), dfZ(z) {}

    void ToArray( double adfOut[3] ) const
    {
        adfOut[0] = dfX;
        adfOut[1] = dfY;
        adfOut[2] = dfZ;
    }
};

/************************************************************************/
/*                            OGRDXFFeature                             */
/*                                                                      */
/*     Extends OGRFeature with some DXF-specific members.               */
/************************************************************************/
class OGRDXFFeature : public OGRFeature
{
    friend class OGRDXFLayer;

  protected:
    // The feature's Object Coordinate System (OCS) unit normal vector
    DXFTriple         oOCS;

    // A list of properties that are used to construct the style string
    std::map<CPLString,CPLString> oStyleProperties;

    // Additional data for INSERT entities
    bool              bIsBlockReference;
    CPLString         osBlockName;
    double            dfBlockAngle;
    DXFTriple         oBlockScale;

    // Used for INSERT entities when DXF_INLINE_BLOCKS is false, to store
    // the OCS insertion point
    DXFTriple         oOriginalCoords;

    // Additional data for ATTRIB and ATTDEF entities
    CPLString         osAttributeTag;

  public:
    explicit OGRDXFFeature( OGRFeatureDefn * poFeatureDefn );

    OGRDXFFeature    *CloneDXFFeature();

    DXFTriple GetOCS() const { return oOCS; }
    bool IsBlockReference() const { return bIsBlockReference; }
    CPLString GetBlockName() const { return osBlockName; }
    double GetBlockAngle() const { return dfBlockAngle; }
    DXFTriple GetBlockScale() const { return oBlockScale; }
    DXFTriple GetInsertOCSCoords() const { return oOriginalCoords; }
    CPLString GetAttributeTag() const { return osAttributeTag; }

    void              ApplyOCSTransformer( OGRGeometry* const poGeometry ) const;
    const CPLString   GetColor( OGRDXFDataSource* const poDS,
                                OGRDXFFeature* const poBlockFeature = NULL );
};

/************************************************************************/
/*                             OGRDXFLayer                              */
/************************************************************************/
class OGRDXFLayer : public OGRLayer
{
    friend class OGRDXFBlocksLayer;

    OGRDXFDataSource   *poDS;

    OGRFeatureDefn     *poFeatureDefn;
    GIntBig             iNextFID;

    std::set<CPLString> oIgnoredEntities;

    std::queue<OGRDXFFeature*> apoPendingFeatures;
    void                ClearPendingFeatures();

    void                TranslateGenericProperty( OGRDXFFeature *poFeature,
                                                  int nCode, char *pszValue );

    void                PrepareFeatureStyle( OGRDXFFeature* const poFeature,
                            OGRDXFFeature* const poBlockFeature = NULL );
    void                PrepareHatchStyle( OGRDXFFeature* const poFeature,
                            OGRDXFFeature* const poBlockFeature = NULL );
    void                PrepareLineStyle( OGRDXFFeature* const poFeature,
                            OGRDXFFeature* const poBlockFeature = NULL );

    OGRDXFFeature *     TranslatePOINT();
    OGRDXFFeature *     TranslateLINE();
    OGRDXFFeature *     TranslatePOLYLINE();
    OGRDXFFeature *     TranslateLWPOLYLINE();
    OGRDXFFeature *     TranslateCIRCLE();
    OGRDXFFeature *     TranslateELLIPSE();
    OGRDXFFeature *     TranslateARC();
    OGRDXFFeature *     TranslateSPLINE();
    OGRDXFFeature *     Translate3DFACE();
    OGRDXFFeature *     TranslateINSERT();
    OGRDXFFeature *     TranslateMTEXT();
    OGRDXFFeature *     TranslateTEXT( const bool bIsAttribOrAttdef );
    OGRDXFFeature *     TranslateDIMENSION();
    OGRDXFFeature *     TranslateHATCH();
    OGRDXFFeature *     TranslateSOLID();
    OGRDXFFeature *     TranslateLEADER();
    OGRDXFFeature *     TranslateMLEADER();

    static OGRGeometry *SimplifyBlockGeometry( OGRGeometryCollection * );
    OGRDXFFeature *     InsertBlockInline( const CPLString& osBlockName,
                                           OGRDXFInsertTransformer oTransformer,
                                           OGRDXFFeature* const poFeature,
                                           std::queue<OGRDXFFeature *>& apoExtraFeatures,
                                           const bool bInlineNestedBlocks,
                                           const bool bMergeGeometry );
    OGRDXFFeature *     InsertBlockReference( const CPLString& osBlockName,
                                              const OGRDXFInsertTransformer& oTransformer,
                                              OGRDXFFeature* const poFeature );
    static void         FormatDimension( CPLString &osText, const double dfValue,
                                         int nPrecision );
    void                InsertArrowhead( OGRDXFFeature* const poFeature,
                                         const CPLString& osBlockName,
                                         const OGRPoint& oPoint1,
                                         const OGRPoint& oPoint2,
                                         const double dfArrowheadSize );
    OGRErr              CollectBoundaryPath( OGRGeometryCollection *poGC,
                                             const double dfElevation );
    OGRErr              CollectPolylinePath( OGRGeometryCollection *poGC,
                                             const double dfElevation );

    CPLString           TextRecode( const char * );
    CPLString           TextUnescape( const char *, bool );

  public:
    explicit OGRDXFLayer( OGRDXFDataSource *poDS );
    ~OGRDXFLayer();

    void                ResetReading() override;
    OGRFeature *        GetNextFeature() override;

    OGRFeatureDefn *    GetLayerDefn() override { return poFeatureDefn; }

    int                 TestCapability( const char * ) override;

    OGRDXFFeature *     GetNextUnfilteredFeature();

    // General DXF utility function
    static void         ApplyOCSTransformer( OGRGeometry *, const DXFTriple& );
};

/************************************************************************/
/*                             OGRDXFReader                             */
/*                                                                      */
/*      A class for very low level DXF reading without interpretation.  */
/************************************************************************/

#define DXF_READER_ERROR()\
    do { CPLError(CE_Failure, CPLE_AppDefined, "%s, %d: error at line %d of %s", \
         __FILE__, __LINE__, GetLineNumber(), GetName()); } while(0)
#define DXF_LAYER_READER_ERROR()\
    do { CPLError(CE_Failure, CPLE_AppDefined, "%s, %d: error at line %d of %s", \
         __FILE__, __LINE__, poDS->GetLineNumber(), poDS->GetName()); } while(0)

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
    std::map<CPLString,CPLString> oBlockRecordHandles;
    std::map<CPLString,CPLString> oHeaderVariables;

    CPLString           osEncoding;

    // indexed by layer name, then by property name.
    std::map< CPLString, std::map<CPLString,CPLString> >
                        oLayerTable;

    // indexed by style name, then by property name.
    std::map< CPLString, std::map<CPLString,CPLString> >
                        oTextStyleTable;
    std::map<CPLString,CPLString> oTextStyleHandles;

    // indexed by dimstyle name, then by DIM... variable name
    std::map< CPLString, std::map<CPLString,CPLString> >
                        oDimStyleTable;

    std::map<CPLString, std::vector<double> > oLineTypeTable;

    bool                bInlineBlocks;
    bool                bMergeBlockGeometries;
    bool                bTranslateEscapeSequences;

    OGRDXFReader        oReader;

    std::vector<CPLString> aosBlockInsertionStack;

  public:
                        OGRDXFDataSource();
                        ~OGRDXFDataSource();

    int                 Open( const char * pszFilename, int bHeaderOnly=FALSE );

    const char          *GetName() override { return osName; }

    int                 GetLayerCount() override { return static_cast<int>(apoLayers.size()); }
    OGRLayer            *GetLayer( int ) override;

    int                 TestCapability( const char * ) override;

    // The following is only used by OGRDXFLayer

    bool                InlineBlocks() const { return bInlineBlocks; }
    bool                ShouldMergeBlockGeometries() const { return bMergeBlockGeometries; }
    bool                ShouldTranslateEscapes() const { return bTranslateEscapeSequences; }
    void                AddStandardFields( OGRFeatureDefn *poDef );

    // Implemented in ogrdxf_blockmap.cpp
    bool                ReadBlocksSection();
    DXFBlockDefinition *LookupBlock( const char *pszName );
    CPLString           GetBlockNameByRecordHandle( const char *pszID );
    std::map<CPLString,DXFBlockDefinition> &GetBlockMap() { return oBlockMap; }

    bool                PushBlockInsertion( const CPLString& osBlockName );
    void                PopBlockInsertion() { aosBlockInsertionStack.pop_back(); }

    // Layer and other Table Handling (ogrdatasource.cpp)
    bool                ReadTablesSection();
    bool                ReadLayerDefinition();
    bool                ReadLineTypeDefinition();
    bool                ReadTextStyleDefinition();
    bool                ReadDimStyleDefinition();
    const char         *LookupLayerProperty( const char *pszLayer,
                                             const char *pszProperty );
    const char         *LookupTextStyleProperty( const char *pszTextStyle,
                                                 const char *pszProperty,
                                                 const char *pszDefault );
    bool                LookupDimStyle( const char *pszDimstyle,
                         std::map<CPLString, CPLString>& oDimStyleProperties );
    std::vector<double> LookupLineType( const char *pszName );
    CPLString           GetTextStyleNameByHandle( const char *pszID );
    static void         PopulateDefaultDimStyleProperties(
                         std::map<CPLString, CPLString>& oDimStyleProperties );

    // Header variables.
    bool               ReadHeaderSection();
    const char         *GetVariable(const char *pszName,
                                    const char *pszDefault=NULL );

    const char         *GetEncoding() { return osEncoding; }

    // reader related.
    int  GetLineNumber() { return oReader.nLineNumber; }
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
    static int          ColorStringToDXFColor( const char * );
    static CPLString    PrepareLineTypeDefinition( OGRFeature*, OGRStyleTool* );

    std::map<CPLString,CPLString> oNewLineTypes;
    int                 nNextAutoID;
    int                 bWriteHatch;

  public:
    OGRDXFWriterLayer( OGRDXFWriterDS *poDS, VSILFILE *fp );
    ~OGRDXFWriterLayer();

    void                ResetReading() override {}
    OGRFeature         *GetNextFeature() override { return NULL; }

    OGRFeatureDefn *    GetLayerDefn() override { return poFeatureDefn; }

    int                 TestCapability( const char * ) override;
    OGRErr              ICreateFeature( OGRFeature *poFeature ) override;
    OGRErr              CreateField( OGRFieldDefn *poField,
                                     int bApproxOK = TRUE ) override;

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
    explicit OGRDXFBlocksWriterLayer( OGRDXFWriterDS *poDS );
    ~OGRDXFBlocksWriterLayer();

    void                ResetReading() override {}
    OGRFeature         *GetNextFeature() override { return NULL; }

    OGRFeatureDefn *    GetLayerDefn() override { return poFeatureDefn; }

    int                 TestCapability( const char * ) override;
    OGRErr              ICreateFeature( OGRFeature *poFeature ) override;
    OGRErr              CreateField( OGRFieldDefn *poField,
                                     int bApproxOK = TRUE ) override;

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

    bool                WriteNewLineTypeRecords( VSILFILE *fp );
    bool                WriteNewBlockRecords( VSILFILE * );
    bool                WriteNewBlockDefinitions( VSILFILE * );
    bool                WriteNewLayerDefinitions( VSILFILE * );
    bool                TransferUpdateHeader( VSILFILE * );
    bool                TransferUpdateTrailer( VSILFILE * );
    bool                FixupHANDSEED( VSILFILE * );

    OGREnvelope         oGlobalEnvelope;

  public:
                        OGRDXFWriterDS();
                        ~OGRDXFWriterDS();

    int                 Open( const char * pszFilename,
                              char **papszOptions );

    const char          *GetName() override { return osName; }

    int                 GetLayerCount() override;
    OGRLayer            *GetLayer( int ) override;

    int                 TestCapability( const char * ) override;

    OGRLayer           *ICreateLayer( const char *pszName,
                                     OGRSpatialReference *poSpatialRef = NULL,
                                     OGRwkbGeometryType eGType = wkbUnknown,
                                     char ** papszOptions = NULL ) override;

    bool                CheckEntityID( const char *pszEntityID );
    long                WriteEntityID( VSILFILE * fp,
                                       long nPreferredFID = OGRNullFID );

    void                UpdateExtent( OGREnvelope* psEnvelope );
};

#endif /* ndef OGR_DXF_H_INCLUDED */
