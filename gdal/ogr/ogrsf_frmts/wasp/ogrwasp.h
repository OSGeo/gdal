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

#ifndef _OGR_WASP_H_INCLUDED
#define _OGR_WASP_H_INCLUDED

#include "ogrsf_frmts.h"

#include <memory>
#include <fstream>
#include <vector>

/************************************************************************/
/*                             OGRWAsPLayer                             */
/************************************************************************/

class OGRWAsPLayer : public OGRLayer
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
    /* for elevation, one fiels (height) */
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

    std::auto_ptr<double> pdfTolerance;
    std::auto_ptr<double> pdfAdjacentPointTolerance;
    std::auto_ptr<double> pdfPointToCircleRadius;

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

    /* return a simplified line (caller is responsible for resource )
     *
     * if pdfTolerance is not NULL, 
     *     calls GEOS symplify
     *
     * if pdfAdjacentPointTolerance is not NULL, 
     *     remove consecutive points that are less than torelance appart 
     *     in x and y
     *
     * if pdfPointToCircleRadius is not NULL,
     *     lines that have been simplified to a point are converted to a 8 pt circle
     * */
    OGRLineString * Simplify( const OGRLineString & line ) const;
    
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

                        ~OGRWAsPLayer();

    virtual OGRFeatureDefn *    GetLayerDefn() { return poLayerDefn; }

    virtual void        ResetReading();
    virtual int         TestCapability( const char * );

    virtual OGRErr      CreateField( OGRFieldDefn *poField,
                                     int bApproxOK = TRUE );
    virtual OGRErr      CreateGeomField( OGRGeomFieldDefn *poGeomField,
                                         int bApproxOK = TRUE );

    virtual OGRErr      CreateFeature( OGRFeature * poFeature );

    virtual OGRFeature *GetNextFeature();
    OGRFeature *GetNextRawFeature();
    virtual OGRwkbGeometryType  GetGeomType() { return wkbLineString25D; }
    virtual OGRSpatialReference *GetSpatialRef() { return poSpatialReference; }
    virtual const char *GetName() { return sName.c_str(); }
};

/************************************************************************/
/*                           OGRWAsPDataSource                          */
/************************************************************************/

class OGRWAsPDataSource : public OGRDataSource
{
    CPLString                     sFilename;
    VSILFILE *                    hFile;
    std::auto_ptr<OGRWAsPLayer>   oLayer;

    void               GetOptions(CPLString & sFirstField, 
                                  CPLString & sSecondField,
                                  CPLString & sGeomField,
                                  bool &      bMerge) const;
  public:
                        /** @note takes ownership of hFile (i.e. responsibility for closing) */
                        OGRWAsPDataSource( const char * pszName, 
                                           VSILFILE * hFile );
                        ~OGRWAsPDataSource();

    virtual const char *GetName() { return sFilename.c_str(); }
    virtual int         GetLayerCount() { return oLayer.get() ? 1 : 0; }
    virtual OGRLayer   *GetLayer( int );
    virtual OGRLayer   *GetLayerByName( const char * );

    virtual OGRLayer   *ICreateLayer( const char *pszName, 
                                     OGRSpatialReference *poSpatialRef = NULL,
                                     OGRwkbGeometryType eGType = wkbUnknown,
                                     char ** papszOptions = NULL );
    
    virtual int        TestCapability( const char * );
    OGRErr             Load( bool bSilent = false );
};

/************************************************************************/
/*                             OGRWAsPDriver                            */
/************************************************************************/

class OGRWAsPDriver : public OGRSFDriver
{

  public:
                                ~OGRWAsPDriver() {}

    virtual const char*         GetName() { return "WAsP"; }
    virtual OGRDataSource*      Open( const char *, int );
    
    virtual OGRDataSource       *CreateDataSource( const char *pszName,
                                                   char ** = NULL );

    virtual OGRErr 	        DeleteDataSource (const char *pszName);

    virtual int                 TestCapability( const char * );
};


#endif /* ndef _OGR_WASP_H_INCLUDED */
