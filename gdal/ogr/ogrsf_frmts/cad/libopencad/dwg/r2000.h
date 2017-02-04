/*******************************************************************************
 *  Project: libopencad
 *  Purpose: OpenSource CAD formats support library
 *  Author: Alexandr Borzykh, mush3d at gmail.com
 *  Author: Dmitry Baryshnikov, bishop.dev@gmail.com
 *  Language: C++
 *******************************************************************************
 *  The MIT License (MIT)
 *
 *  Copyright (c) 2016 Alexandr Borzykh
 *  Copyright (c) 2016 NextGIS, <info@nextgis.com>
 *
 *  Permission is hereby granted, free of charge, to any person obtaining a copy
 *  of this software and associated documentation files (the "Software"), to deal
 *  in the Software without restriction, including without limitation the rights
 *  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *  copies of the Software, and to permit persons to whom the Software is
 *  furnished to do so, subject to the following conditions:
 *
 *  The above copyright notice and this permission notice shall be included in all
 *  copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 *  SOFTWARE.
 *******************************************************************************/
#ifndef DWG_R2000_H_H
#define DWG_R2000_H_H

#include "cadfile.h"

struct SectionLocatorRecord
{
    char byRecordNumber = 0;
    int  dSeeker        = 0;
    int  dSize          = 0;
};

struct DWG2000Ced
{
    long        dLength;
    short       dType;
    int         dObjSizeInBits;
    CADHandle   hHandle;
    CADEedArray eEED;
    bool        bGraphicPresentFlag;

    char dEntMode;
    int  dNumReactors;

    bool   bNoLinks;
    short  dCMColorIndex;
    double dfLtypeScale;

    char ltype_flags;
    char plotstyle_flags;

    short dInvisibility;
    char  cLineWeight;
};

struct DWG2000Cehd
{
    CADHandle hOwner;
    CADHandle hReactors;
    CADHandle hxdibobjhandle;
    CADHandle hprev_entity, hnext_entity;
    CADHandle hlayer;
    CADHandle hltype;
    CADHandle hplotstyle;
};

class DWGFileR2000 : public CADFile
{
public:
    explicit             DWGFileR2000( CADFileIO * poFileIO );
    virtual             ~DWGFileR2000();

protected:
    virtual int ReadSectionLocators() override;
    virtual int ReadHeader( enum OpenOptions eOptions ) override;
    virtual int ReadClasses( enum OpenOptions eOptions ) override;
    virtual int CreateFileMap() override;

    CADObject   * GetObject( long dHandle, bool bHandlesOnly = false ) override;
    CADGeometry * GetGeometry( size_t iLayerIndex, long dHandle,
                               long dBlockRefHandle = 0 ) override;

    CADDictionary GetNOD() override;
protected:
    CADBlockObject * getBlock( unsigned int dObjectSize,
                               const CADCommonED& stCommonEntityData,
                               const char * pabyInput,
                               size_t& nBitOffsetFromStart );
    CADEllipseObject * getEllipse( unsigned int dObjectSize,
                                   const CADCommonED& stCommonEntityData,
                                   const char * pabyInput,
                                   size_t& nBitOffsetFromStart );
    CADSolidObject * getSolid( unsigned int dObjectSize,
                               const CADCommonED& stCommonEntityData,
                               const char * pabyInput, size_t& nBitOffsetFromStart );
    CADPointObject * getPoint( unsigned int dObjectSize,
                               const CADCommonED& stCommonEntityData,
                               const char * pabyInput, size_t& nBitOffsetFromStart );
    CADPolyline3DObject * getPolyLine3D( unsigned int dObjectSize,
                                         const CADCommonED& stCommonEntityData,
                                         const char * pabyInput,
                                         size_t& nBitOffsetFromStart );
    CADRayObject * getRay( unsigned int dObjectSize,
                           const CADCommonED& stCommonEntityData,
                           const char * pabyInput, size_t& nBitOffsetFromStart );
    CADXLineObject * getXLine( unsigned int dObjectSize,
                               const CADCommonED& stCommonEntityData,
                               const char * pabyInput, size_t& nBitOffsetFromStart );
    CADLineObject * getLine( unsigned int dObjectSize,
                             const CADCommonED& stCommonEntityData,
                             const char * pabyInput, size_t& nBitOffsetFromStart );
    CADTextObject * getText( unsigned int dObjectSize,
                             const CADCommonED& stCommonEntityData,
                             const char * pabyInput, size_t& nBitOffsetFromStart );
    CADVertex3DObject * getVertex3D( unsigned int dObjectSize,
                                     const CADCommonED& stCommonEntityData,
                                     const char * pabyInput,
                                     size_t& nBitOffsetFromStart );
    CADCircleObject * getCircle( unsigned int dObjectSize,
                                 const CADCommonED& stCommonEntityData,
                                 const char * pabyInput,
                                 size_t& nBitOffsetFromStart );
    CADEndblkObject * getEndBlock( unsigned int dObjectSize,
                                   const CADCommonED& stCommonEntityData,
                                   const char * pabyInput,
                                   size_t& nBitOffsetFromStart );
    CADPolyline2DObject * getPolyline2D( unsigned int dObjectSize,
                                         const CADCommonED& stCommonEntityData,
                                         const char * pabyInput,
                                         size_t& nBitOffsetFromStart );
    CADAttribObject * getAttributes( unsigned int dObjectSize,
                                     const CADCommonED& stCommonEntityData,
                                     const char * pabyInput,
                                     size_t& nBitOffsetFromStart );
    CADAttdefObject * getAttributesDefn( unsigned int dObjectSize,
                                         const CADCommonED& stCommonEntityData,
                                         const char * pabyInput,
                                         size_t& nBitOffsetFromStart );
    CADLWPolylineObject * getLWPolyLine( unsigned int dObjectSize,
                                         const CADCommonED& stCommonEntityData,
                                         const char * pabyInput,
                                         size_t& nBitOffsetFromStart );
    CADArcObject * getArc( unsigned int dObjectSize,
                           const CADCommonED& stCommonEntityData,
                           const char * pabyInput, size_t& nBitOffsetFromStart );
    CADSplineObject * getSpline( unsigned int dObjectSize,
                                 const CADCommonED& stCommonEntityData,
                                 const char * pabyInput, size_t& nBitOffsetFromStart );
    CADEntityObject * getEntity( int dObjectType, unsigned int dObjectSize,
                                 const CADCommonED& stCommonEntityData,
                                 const char * pabyInput, size_t& nBitOffsetFromStart );
    CADInsertObject * getInsert( int dObjectType, unsigned int dObjectSize,
                                 const CADCommonED& stCommonEntityData,
                                 const char * pabyInput, size_t& nBitOffsetFromStart );
    CADDictionaryObject * getDictionary( unsigned int dObjectSize,
                                         const char * pabyInput,
                                         size_t& nBitOffsetFromStart );
    CADXRecordObject * getXRecord( unsigned int dObjectSize,
                                   const char * pabyInput,
                                   size_t& nBitOffsetFromStart );
    CADLayerObject * getLayerObject( unsigned int dObjectSize,
                                     const char * pabyInput,
                                     size_t& nBitOffsetFromStart );
    CADLayerControlObject * getLayerControl( unsigned int dObjectSize,
                                             const char * pabyInput,
                                             size_t& nBitOffsetFromStart );
    CADBlockControlObject * getBlockControl( unsigned int dObjectSize,
                                             const char * pabyInput,
                                             size_t& nBitOffsetFromStart );
    CADBlockHeaderObject * getBlockHeader( unsigned int dObjectSize,
                                           const char * pabyInput,
                                           size_t& nBitOffsetFromStart );
    CADLineTypeControlObject * getLineTypeControl( unsigned int dObjectSize,
                                                   const char * pabyInput,
                                                   size_t& nBitOffsetFromStart );
    CADLineTypeObject * getLineType1( unsigned int dObjectSize,
                                      const char * pabyInput,
                                      size_t& nBitOffsetFromStart );
    CADMLineObject * getMLine( unsigned int dObjectSize,
                               const CADCommonED& stCommonEntityData,
                               const char * pabyInput,
                               size_t& nBitOffsetFromStart );
    CADPolylinePFaceObject * getPolylinePFace( unsigned int dObjectSize,
                                               const CADCommonED& stCommonEntityData,
                                               const char * pabyInput,
                                               size_t& nBitOffsetFromStart );
    CADImageObject * getImage( unsigned int dObjectSize,
                               const CADCommonED& stCommonEntityData,
                               const char * pabyInput,
                               size_t& nBitOffsetFromStart );
    CAD3DFaceObject * get3DFace( unsigned int dObjectSize,
                                 const CADCommonED& stCommonEntityData,
                                 const char * pabyInput,
                                 size_t& nBitOffsetFromStart );
    CADVertexMeshObject * getVertexMesh( unsigned int dObjectSize,
                                         const CADCommonED& stCommonEntityData,
                                         const char * pabyInput,
                                         size_t& nBitOffsetFromStart );
    CADVertexPFaceObject * getVertexPFace( unsigned int dObjectSize,
                                           const CADCommonED& stCommonEntityData,
                                           const char * pabyInput,
                                           size_t& nBitOffsetFromStart );
    CADDimensionObject * getDimension( short dObjectType, unsigned int dObjectSize,
                                       const CADCommonED& stCommonEntityData,
                                       const char * pabyInput,
                                       size_t& nBitOffsetFromStart );
    CADMTextObject * getMText( unsigned int dObjectSize,
                               const CADCommonED& stCommonEntityData,
                               const char * pabyInput,
                               size_t& nBitOffsetFromStart );
    CADImageDefObject * getImageDef( unsigned int dObjectSize,
                                     const char * pabyInput,
                                     size_t& nBitOffsetFromStart );
    CADImageDefReactorObject * getImageDefReactor( unsigned int dObjectSize,
                                                   const char * pabyInput,
                                                   size_t& nBitOffsetFromStart );
    void fillCommonEntityHandleData( CADEntityObject * pEnt, const char * pabyInput,
                                                         size_t& nBitOffsetFromStart );
    unsigned short validateEntityCRC(const char * pabyInput,
        unsigned int dObjectSize, size_t & nBitOffsetFromStart,
        const char * entityName = "ENTITY", bool bSwapEndianness = false );
protected:
    int                               imageSeeker;
    std::vector<SectionLocatorRecord> sectionLocatorRecords;
};

#endif // DWG_R2000_H_H
