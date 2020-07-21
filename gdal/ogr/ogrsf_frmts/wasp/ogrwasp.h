/******************************************************************************
 *
 * Project:  WAsP Translator
 * Purpose:  Definition of classes for OGR .map driver.
 * Author:   Vincent Mora, vincent dot mora at oslandia dot com
 *
 ******************************************************************************
 * Copyright (c) 2014, Oslandia <info at oslandia dot com>
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

#ifndef OGR_WASP_H_INCLUDED
#define OGR_WASP_H_INCLUDED

#include "ogrsf_frmts.h"

#include <memory>
#include <fstream>
#include <vector>

/************************************************************************/
/*                             OGRWAsPLayer                             */
/************************************************************************/

class OGRWAsPLayer final: public OGRLayer, public OGRGetNextFeatureThroughRaw<OGRWAsPLayer>
{
    /* stuff for polygon processing */

    /* note if shared ptr are available, replace the ptr in the two structs */
    /* and remove deletion of array elements in ~OGRWAsPLayer() */
    struct Zone
    {
        OGREnvelope       oEnvelope;
        OGRPolygon *      poPolygon;
        double            dfZ;
    };

    struct Boundary
    {
        OGRLineString * poLine;
        double          dfLeft;
        double          dfRight;
    };

    const bool              bMerge;
    std::vector< Zone >     oZones;
    std::vector< Boundary > oBoundaries;

    static bool isEqual( const double & dfRouhness1, const double & dfRouhness2 ) { return fabs( dfRouhness1 - dfRouhness2 ) < 1e-3; }

    /* end of stuff for polygon processing */

    int iFeatureCount;

    const CPLString       sName;
    VSILFILE *            hFile;

    /* for roughness zone, two fields for linestrings (left/right), one for polygons */
    /* for elevation, one field (height) */
    const CPLString       sFirstField;
    const CPLString       sSecondField;
    const CPLString       sGeomField;
    int                   iFirstFieldIdx;
    int                   iSecondFieldIdx;
    int                   iGeomFieldIdx;

    OGRFeatureDefn *      poLayerDefn;
    OGRSpatialReference * poSpatialReference;

    vsi_l_offset          iOffsetFeatureBegin;

    enum OpenMode {READ_ONLY, WRITE_ONLY};
    OpenMode              eMode;

    std::unique_ptr<double> pdfTolerance;
    std::unique_ptr<double> pdfAdjacentPointTolerance;
    std::unique_ptr<double> pdfPointToCircleRadius;

    OGRErr                WriteRoughness( OGRLineString *,
                                          const double & dfZleft,
                                          const double & dfZright );
    OGRErr                WriteRoughness( OGRPolygon *,
                                          const double & dfZ );
    OGRErr                WriteRoughness( OGRGeometry *,
                                          const double & dfZleft,
                                          const double & dfZright );

    OGRErr                WriteElevation( OGRLineString *, const double & dfZ );
    OGRErr                WriteElevation( OGRGeometry *, const double & dfZ );

    static double AvgZ( OGRLineString * poGeom );
    static double AvgZ( OGRPolygon * poGeom );
    static double AvgZ( OGRGeometryCollection * poGeom );
    static double AvgZ( OGRGeometry * poGeom );

    /* return a simplified line (caller is responsible for resource)
     *
     * if pdfTolerance is not NULL,
     *     calls GEOS simplify
     *
     * if pdfAdjacentPointTolerance is not NULL,
     *     remove consecutive points that are less than tolerance apart
     *     in x and y
     *
     * if pdfPointToCircleRadius is not NULL,
     *     lines that have been simplified to a point are converted to a 8 pt circle
     * */
    OGRLineString * Simplify( const OGRLineString & line ) const;

    OGRFeature *GetNextRawFeature();

  public:
                        /* For writing */
                        /* Takes ownership of poTolerance */
                        OGRWAsPLayer( const char * pszName,
                                      VSILFILE * hFile,
                                      OGRSpatialReference * poSpatialRef,
                                      const CPLString & sFirstField,
                                      const CPLString & sSecondField,
                                      const CPLString & sGeomField,
                                      bool bMerge,
                                      double * pdfTolerance,
                                      double * pdfAdjacentPointTolerance,
                                      double * pdfPointToCircleRadius );

                        /* For reading */
                        OGRWAsPLayer( const char * pszName,
                                      VSILFILE * hFile,
                                      OGRSpatialReference * poSpatialRef );

                        virtual ~OGRWAsPLayer();

    virtual OGRFeatureDefn *    GetLayerDefn() override { return poLayerDefn; }

    virtual void        ResetReading() override;
    virtual int         TestCapability( const char * ) override;

    virtual OGRErr      CreateField( OGRFieldDefn *poField,
                                     int bApproxOK = TRUE ) override;
    virtual OGRErr      CreateGeomField( OGRGeomFieldDefn *poGeomField,
                                         int bApproxOK = TRUE ) override;

    virtual OGRErr      ICreateFeature( OGRFeature * poFeature ) override;

    DEFINE_GET_NEXT_FEATURE_THROUGH_RAW(OGRWAsPLayer)
    virtual const char *GetName() override { return sName.c_str(); }
};

/************************************************************************/
/*                           OGRWAsPDataSource                          */
/************************************************************************/

class OGRWAsPDataSource final: public OGRDataSource
{
    CPLString                     sFilename;
    VSILFILE *                    hFile;
    std::unique_ptr<OGRWAsPLayer>   oLayer;

    void               GetOptions(CPLString & sFirstField,
                                  CPLString & sSecondField,
                                  CPLString & sGeomField,
                                  bool &      bMerge) const;
  public:
                        /** @note takes ownership of hFile (i.e. responsibility for closing) */
                        OGRWAsPDataSource( const char * pszName,
                                           VSILFILE * hFile );
                        virtual ~OGRWAsPDataSource();

    virtual const char *GetName() override { return sFilename.c_str(); }
    virtual int         GetLayerCount() override { return oLayer.get() ? 1 : 0; }
    virtual OGRLayer   *GetLayer( int ) override;
    virtual OGRLayer   *GetLayerByName( const char * ) override;

    virtual OGRLayer   *ICreateLayer( const char *pszName,
                                     OGRSpatialReference *poSpatialRef = nullptr,
                                     OGRwkbGeometryType eGType = wkbUnknown,
                                     char ** papszOptions = nullptr ) override;

    virtual int        TestCapability( const char * ) override;
    OGRErr             Load( bool bSilent = false );
};

/************************************************************************/
/*                             OGRWAsPDriver                            */
/************************************************************************/

class OGRWAsPDriver final: public OGRSFDriver
{

  public:
                                ~OGRWAsPDriver() {}

    virtual const char*         GetName() override { return "WAsP"; }
    virtual OGRDataSource*      Open( const char *, int ) override;

    virtual OGRDataSource       *CreateDataSource( const char *pszName,
                                                   char ** = nullptr ) override;

    virtual OGRErr              DeleteDataSource (const char *pszName) override;

    virtual int                 TestCapability( const char * ) override;
};

#endif /* ndef OGR_WASP_H_INCLUDED */
