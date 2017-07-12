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
#include "io.h"

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
    virtual             ~DWGFileR2000() = default;

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
    CADBlockObject * getBlock(unsigned int dObjectSize,
                               const CADCommonED& stCommonEntityData,
                               CADBuffer& buffer);
    CADEllipseObject * getEllipse(unsigned int dObjectSize,
                                  const CADCommonED& stCommonEntityData,
                                  CADBuffer &buffer);
    CADSolidObject * getSolid( unsigned int dObjectSize,
                               const CADCommonED& stCommonEntityData,
                               CADBuffer& buffer);
    CADPointObject * getPoint(unsigned int dObjectSize,
                               const CADCommonED& stCommonEntityData,
                               CADBuffer &buffer);
    CADPolyline3DObject * getPolyLine3D(unsigned int dObjectSize,
                                        const CADCommonED& stCommonEntityData,
                                        CADBuffer &buffer);
    CADRayObject * getRay( unsigned int dObjectSize,
                           const CADCommonED& stCommonEntityData,
                           CADBuffer &buffer);
    CADXLineObject * getXLine( unsigned int dObjectSize,
                               const CADCommonED& stCommonEntityData,
                               CADBuffer &buffer );
    CADLineObject * getLine( unsigned int dObjectSize,
                             const CADCommonED& stCommonEntityData,
                             CADBuffer &buffer );
    CADTextObject * getText( unsigned int dObjectSize,
                             const CADCommonED& stCommonEntityData,
                             CADBuffer &buffer );
    CADVertex3DObject * getVertex3D( unsigned int dObjectSize,
                                     const CADCommonED& stCommonEntityData,
                                     CADBuffer &buffer );
    CADCircleObject * getCircle( unsigned int dObjectSize,
                                 const CADCommonED& stCommonEntityData,
                                 CADBuffer &buffer );
    CADEndblkObject * getEndBlock( unsigned int dObjectSize,
                                   const CADCommonED& stCommonEntityData,
                                   CADBuffer &buffer );
    CADPolyline2DObject * getPolyline2D( unsigned int dObjectSize,
                                         const CADCommonED& stCommonEntityData,
                                         CADBuffer &buffer );
    CADAttribObject * getAttributes( unsigned int dObjectSize,
                                     const CADCommonED& stCommonEntityData,
                                     CADBuffer &buffer );
    CADAttdefObject * getAttributesDefn( unsigned int dObjectSize,
                                         const CADCommonED& stCommonEntityData,
                                         CADBuffer &buffer );
    CADLWPolylineObject * getLWPolyLine( unsigned int dObjectSize,
                                         const CADCommonED& stCommonEntityData,
                                         CADBuffer &buffer );
    CADArcObject * getArc( unsigned int dObjectSize,
                           const CADCommonED& stCommonEntityData,
                           CADBuffer &buffer );
    CADSplineObject * getSpline( unsigned int dObjectSize,
                                 const CADCommonED& stCommonEntityData,
                                 CADBuffer &buffer );
    CADEntityObject * getEntity( int dObjectType, unsigned int dObjectSize,
                                 const CADCommonED& stCommonEntityData,
                                 CADBuffer &buffer );
    CADInsertObject * getInsert( int dObjectType, unsigned int dObjectSize,
                                 const CADCommonED& stCommonEntityData,
                                 CADBuffer &buffer );
    CADDictionaryObject * getDictionary( unsigned int dObjectSize,
                                         CADBuffer &buffer );
    CADXRecordObject * getXRecord( unsigned int dObjectSize,
                                   CADBuffer &buffer );
    CADLayerObject * getLayerObject( unsigned int dObjectSize,
                                     CADBuffer &buffer );
    CADLayerControlObject * getLayerControl( unsigned int dObjectSize,
                                             CADBuffer &buffer );
    CADBlockControlObject * getBlockControl( unsigned int dObjectSize,
                                             CADBuffer &buffer );
    CADBlockHeaderObject * getBlockHeader( unsigned int dObjectSize,
                                           CADBuffer &buffer );
    CADLineTypeControlObject * getLineTypeControl( unsigned int dObjectSize,
                                                   CADBuffer &buffer );
    CADLineTypeObject * getLineType1( unsigned int dObjectSize,
                                      CADBuffer &buffer );
    CADMLineObject * getMLine( unsigned int dObjectSize,
                               const CADCommonED& stCommonEntityData,
                               CADBuffer &buffer );
    CADPolylinePFaceObject * getPolylinePFace( unsigned int dObjectSize,
                                               const CADCommonED& stCommonEntityData,
                                               CADBuffer &buffer );
    CADImageObject * getImage( unsigned int dObjectSize,
                               const CADCommonED& stCommonEntityData,
                               CADBuffer &buffer );
    CAD3DFaceObject * get3DFace( unsigned int dObjectSize,
                                 const CADCommonED& stCommonEntityData,
                                 CADBuffer &buffer );
    CADVertexMeshObject * getVertexMesh( unsigned int dObjectSize,
                                         const CADCommonED& stCommonEntityData,
                                         CADBuffer &buffer );
    CADVertexPFaceObject * getVertexPFace( unsigned int dObjectSize,
                                           const CADCommonED& stCommonEntityData,
                                           CADBuffer &buffer );
    CADDimensionObject * getDimension( short dObjectType, unsigned int dObjectSize,
                                       const CADCommonED& stCommonEntityData,
                                       CADBuffer &buffer );
    CADMTextObject * getMText( unsigned int dObjectSize,
                               const CADCommonED& stCommonEntityData,
                               CADBuffer &buffer );
    CADImageDefObject * getImageDef( unsigned int dObjectSize,
                                     CADBuffer &buffer );
    CADImageDefReactorObject * getImageDefReactor( unsigned int dObjectSize,
                                                   CADBuffer &buffer );
    void fillCommonEntityHandleData(CADEntityObject * pEnt, CADBuffer &buffer);
    unsigned short validateEntityCRC(CADBuffer& buffer, unsigned int dObjectSize,
                                     const char * entityName = "ENTITY",
                                     bool bSwapEndianness = false );
protected:
    int                               imageSeeker;
    std::vector<SectionLocatorRecord> sectionLocatorRecords;
};

#endif // DWG_R2000_H_H
