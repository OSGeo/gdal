/******************************************************************************
 * $Id: ogrmemlayer.cpp 17807 2009-10-13 18:18:09Z rouault $
 *
 * Project:  DXF Translator
 * Purpose:  Implements OGRDXFLayer class.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2009, Frank Warmerdam <warmerdam@pobox.com>
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

#include "ogr_dxf.h"
#include "cpl_conv.h"

CPL_CVSID("$Id: ogrmemlayer.cpp 17807 2009-10-13 18:18:09Z rouault $");

/* -------------------------------------------------------------------- */
/*      DXF color table, derived from dxflib.                           */
/* -------------------------------------------------------------------- */

const double dxfColors[][3] = {
    {0,0,0},                // unused
    {1,0,0},                // 1
    {1,1,0},
    {0,1,0},
    {0,1,1},
    {0,0,1},
    {1,0,1},
    {1,1,1},                // black or white
    {0.5,0.5,0.5},
    {0.75,0.75,0.75},
    {1,0,0},                // 10
    {1,0.5,0.5},
    {0.65,0,0},
    {0.65,0.325,0.325},
    {0.5,0,0},
    {0.5,0.25,0.25},
    {0.3,0,0},
    {0.3,0.15,0.15},
    {0.15,0,0},
    {0.15,0.075,0.075},
    {1,0.25,0},             // 20
    {1,0.625,0.5},
    {0.65,0.1625,0},
    {0.65,0.4063,0.325},
    {0.5,0.125,0},
    {0.5,0.3125,0.25},
    {0.3,0.075,0},
    {0.3,0.1875,0.15},
    {0.15,0.0375,0},
    {0.15,0.0938,0.075},
    {1,0.5,0},              // 30
    {1,0.75,0.5},
    {0.65,0.325,0},
    {0.65,0.4875,0.325},
    {0.5,0.25,0},
    {0.5,0.375,0.25},
    {0.3,0.15,0},
    {0.3,0.225,0.15},
    {0.15,0.075,0},
    {0.15,0.1125,0.075},
    {1,0.75,0},             // 40
    {1,0.875,0.5},
    {0.65,0.4875,0},
    {0.65,0.5688,0.325},
    {0.5,0.375,0},
    {0.5,0.4375,0.25},
    {0.3,0.225,0},
    {0.3,0.2625,0.15},
    {0.15,0.1125,0},
    {0.15,0.1313,0.075},
    {1,1,0},                // 50
    {1,1,0.5},
    {0.65,0.65,0},
    {0.65,0.65,0.325},
    {0.5,0.5,0},
    {0.5,0.5,0.25},
    {0.3,0.3,0},
    {0.3,0.3,0.15},
    {0.15,0.15,0},
    {0.15,0.15,0.075},
    {0.75,1,0},             // 60
    {0.875,1,0.5},
    {0.4875,0.65,0},
    {0.5688,0.65,0.325},
    {0.375,0.5,0},
    {0.4375,0.5,0.25},
    {0.225,0.3,0},
    {0.2625,0.3,0.15},
    {0.1125,0.15,0},
    {0.1313,0.15,0.075},
    {0.5,1,0},              // 70
    {0.75,1,0.5},
    {0.325,0.65,0},
    {0.4875,0.65,0.325},
    {0.25,0.5,0},
    {0.375,0.5,0.25},
    {0.15,0.3,0},
    {0.225,0.3,0.15},
    {0.075,0.15,0},
    {0.1125,0.15,0.075},
    {0.25,1,0},             // 80
    {0.625,1,0.5},
    {0.1625,0.65,0},
    {0.4063,0.65,0.325},
    {0.125,0.5,0},
    {0.3125,0.5,0.25},
    {0.075,0.3,0},
    {0.1875,0.3,0.15},
    {0.0375,0.15,0},
    {0.0938,0.15,0.075},
    {0,1,0},                // 90
    {0.5,1,0.5},
    {0,0.65,0},
    {0.325,0.65,0.325},
    {0,0.5,0},
    {0.25,0.5,0.25},
    {0,0.3,0},
    {0.15,0.3,0.15},
    {0,0.15,0},
    {0.075,0.15,0.075},
    {0,1,0.25},             // 100
    {0.5,1,0.625},
    {0,0.65,0.1625},
    {0.325,0.65,0.4063},
    {0,0.5,0.125},
    {0.25,0.5,0.3125},
    {0,0.3,0.075},
    {0.15,0.3,0.1875},
    {0,0.15,0.0375},
    {0.075,0.15,0.0938},
    {0,1,0.5},              // 110
    {0.5,1,0.75},
    {0,0.65,0.325},
    {0.325,0.65,0.4875},
    {0,0.5,0.25},
    {0.25,0.5,0.375},
    {0,0.3,0.15},
    {0.15,0.3,0.225},
    {0,0.15,0.075},
    {0.075,0.15,0.1125},
    {0,1,0.75},             // 120
    {0.5,1,0.875},
    {0,0.65,0.4875},
    {0.325,0.65,0.5688},
    {0,0.5,0.375},
    {0.25,0.5,0.4375},
    {0,0.3,0.225},
    {0.15,0.3,0.2625},
    {0,0.15,0.1125},
    {0.075,0.15,0.1313},
    {0,1,1},                // 130
    {0.5,1,1},
    {0,0.65,0.65},
    {0.325,0.65,0.65},
    {0,0.5,0.5},
    {0.25,0.5,0.5},
    {0,0.3,0.3},
    {0.15,0.3,0.3},
    {0,0.15,0.15},
    {0.075,0.15,0.15},
    {0,0.75,1},             // 140
    {0.5,0.875,1},
    {0,0.4875,0.65},
    {0.325,0.5688,0.65},
    {0,0.375,0.5},
    {0.25,0.4375,0.5},
    {0,0.225,0.3},
    {0.15,0.2625,0.3},
    {0,0.1125,0.15},
    {0.075,0.1313,0.15},
    {0,0.5,1},              // 150
    {0.5,0.75,1},
    {0,0.325,0.65},
    {0.325,0.4875,0.65},
    {0,0.25,0.5},
    {0.25,0.375,0.5},
    {0,0.15,0.3},
    {0.15,0.225,0.3},
    {0,0.075,0.15},
    {0.075,0.1125,0.15},
    {0,0.25,1},             // 160
    {0.5,0.625,1},
    {0,0.1625,0.65},
    {0.325,0.4063,0.65},
    {0,0.125,0.5},
    {0.25,0.3125,0.5},
    {0,0.075,0.3},
    {0.15,0.1875,0.3},
    {0,0.0375,0.15},
    {0.075,0.0938,0.15},
    {0,0,1},                // 170
    {0.5,0.5,1},
    {0,0,0.65},
    {0.325,0.325,0.65},
    {0,0,0.5},
    {0.25,0.25,0.5},
    {0,0,0.3},
    {0.15,0.15,0.3},
    {0,0,0.15},
    {0.075,0.075,0.15},
    {0.25,0,1},             // 180
    {0.625,0.5,1},
    {0.1625,0,0.65},
    {0.4063,0.325,0.65},
    {0.125,0,0.5},
    {0.3125,0.25,0.5},
    {0.075,0,0.3},
    {0.1875,0.15,0.3},
    {0.0375,0,0.15},
    {0.0938,0.075,0.15},
    {0.5,0,1},              // 190
    {0.75,0.5,1},
    {0.325,0,0.65},
    {0.4875,0.325,0.65},
    {0.25,0,0.5},
    {0.375,0.25,0.5},
    {0.15,0,0.3},
    {0.225,0.15,0.3},
    {0.075,0,0.15},
    {0.1125,0.075,0.15},
    {0.75,0,1},             // 200
    {0.875,0.5,1},
    {0.4875,0,0.65},
    {0.5688,0.325,0.65},
    {0.375,0,0.5},
    {0.4375,0.25,0.5},
    {0.225,0,0.3},
    {0.2625,0.15,0.3},
    {0.1125,0,0.15},
    {0.1313,0.075,0.15},
    {1,0,1},                // 210
    {1,0.5,1},
    {0.65,0,0.65},
    {0.65,0.325,0.65},
    {0.5,0,0.5},
    {0.5,0.25,0.5},
    {0.3,0,0.3},
    {0.3,0.15,0.3},
    {0.15,0,0.15},
    {0.15,0.075,0.15},
    {1,0,0.75},             // 220
    {1,0.5,0.875},
    {0.65,0,0.4875},
    {0.65,0.325,0.5688},
    {0.5,0,0.375},
    {0.5,0.25,0.4375},
    {0.3,0,0.225},
    {0.3,0.15,0.2625},
    {0.15,0,0.1125},
    {0.15,0.075,0.1313},
    {1,0,0.5},              // 230
    {1,0.5,0.75},
    {0.65,0,0.325},
    {0.65,0.325,0.4875},
    {0.5,0,0.25},
    {0.5,0.25,0.375},
    {0.3,0,0.15},
    {0.3,0.15,0.225},
    {0.15,0,0.075},
    {0.15,0.075,0.1125},
    {1,0,0.25},             // 240
    {1,0.5,0.625},
    {0.65,0,0.1625},
    {0.65,0.325,0.4063},
    {0.5,0,0.125},
    {0.5,0.25,0.3125},
    {0.3,0,0.075},
    {0.3,0.15,0.1875},
    {0.15,0,0.0375},
    {0.15,0.075,0.0938},
    {0.33,0.33,0.33},       // 250
    {0.464,0.464,0.464},
    {0.598,0.598,0.598},
    {0.732,0.732,0.732},
    {0.866,0.866,0.866},
    {1,1,1}                 // 255
};

/************************************************************************/
/*                            OGRDXFLayer()                             */
/************************************************************************/

OGRDXFLayer::OGRDXFLayer( OGRDXFDataSource *poDS )

{
    this->poDS = poDS;

    iNextFID = 0;

    poFeatureDefn = new OGRFeatureDefn( "entities" );
    poFeatureDefn->Reference();

    OGRFieldDefn  oLayerField( "Layer", OFTString );
    poFeatureDefn->AddFieldDefn( &oLayerField );

    OGRFieldDefn  oClassField( "SubClass", OFTString );
    poFeatureDefn->AddFieldDefn( &oClassField );

    OGRFieldDefn  oExtendedField( "ExtendedEntity", OFTString );
    poFeatureDefn->AddFieldDefn( &oExtendedField );

    OGRFieldDefn  oColorField( "Color", OFTInteger );
    poFeatureDefn->AddFieldDefn( &oColorField );

    OGRFieldDefn  oLinetypeField( "Linetype", OFTString );
    poFeatureDefn->AddFieldDefn( &oLinetypeField );
}

/************************************************************************/
/*                           ~OGRDXFLayer()                           */
/************************************************************************/

OGRDXFLayer::~OGRDXFLayer()

{
    if( m_nFeaturesRead > 0 && poFeatureDefn != NULL )
    {
        CPLDebug( "DXF", "%d features read on layer '%s'.",
                  (int) m_nFeaturesRead, 
                  poFeatureDefn->GetName() );
    }

    if( poFeatureDefn )
        poFeatureDefn->Release();
}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGRDXFLayer::ResetReading()

{
    iNextFID = 0;
    poDS->RestartEntities();
}

/************************************************************************/
/*                      TranslateGenericProperty()                      */
/*                                                                      */
/*      Try and convert entity properties handled similarly for most    */
/*      or all entity types.                                            */
/************************************************************************/

void OGRDXFLayer::TranslateGenericProperty( OGRFeature *poFeature, 
                                            int nCode, char *pszValue )

{
    switch( nCode )
    {
      case 8: 
        poFeature->SetField( "Layer", pszValue );
        break;
            
      case 100: 
        poFeature->SetField( "SubClass", pszValue );
        break;

      case 62:
        poFeature->SetField( "Color", pszValue );
        break;

      case 6:
        poFeature->SetField( "Linetype", pszValue );
        break;

        // Extended entity data
      case 1000:
      case 1002:
      case 1004:
      case 1005:
      case 1040:
      case 1041:
      case 1070:
      case 1071:
      {
          CPLString osAggregate = poFeature->GetFieldAsString("ExtendedEntity");

          if( osAggregate.size() > 0 )
              osAggregate += " ";
          osAggregate += pszValue;
            
          poFeature->SetField( "ExtendedEntity", osAggregate );
      }
      break;

      default:
        break;
    }
}

/************************************************************************/
/*                           TranslatePOINT()                           */
/************************************************************************/

OGRFeature *OGRDXFLayer::TranslatePOINT()

{
    char szLineBuf[257];
    int nCode;
    OGRFeature *poFeature = new OGRFeature( poFeatureDefn );
    double dfX = 0.0, dfY = 0.0, dfZ = 0.0;

    while( (nCode = poDS->ReadValue(szLineBuf,sizeof(szLineBuf))) > 0 )
    {
        switch( nCode )
        {
          case 10:
            dfX = atof(szLineBuf);
            break;

          case 20:
            dfY = atof(szLineBuf);
            break;

          case 30:
            dfZ = atof(szLineBuf);
            break;

          default:
            TranslateGenericProperty( poFeature, nCode, szLineBuf );
            break;
        }
    }

    poFeature->SetGeometryDirectly( new OGRPoint( dfX, dfY, dfZ ) );

    if( nCode == 0 )
        poDS->UnreadValue();

    return poFeature;
}

/************************************************************************/
/*                           TranslateLINE()                            */
/************************************************************************/

OGRFeature *OGRDXFLayer::TranslateLINE()

{
    char szLineBuf[257];
    int nCode;
    OGRFeature *poFeature = new OGRFeature( poFeatureDefn );
    double dfX1 = 0.0, dfY1 = 0.0, dfZ1 = 0.0;
    double dfX2 = 0.0, dfY2 = 0.0, dfZ2 = 0.0;

/* -------------------------------------------------------------------- */
/*      Process values.                                                 */
/* -------------------------------------------------------------------- */
    while( (nCode = poDS->ReadValue(szLineBuf,sizeof(szLineBuf))) > 0 )
    {
        switch( nCode )
        {
          case 10:
            dfX1 = atof(szLineBuf);
            break;

          case 11:
            dfX2 = atof(szLineBuf);
            break;

          case 20:
            dfY1 = atof(szLineBuf);
            break;

          case 21:
            dfY2 = atof(szLineBuf);
            break;

          case 30:
            dfZ1 = atof(szLineBuf);
            break;

          case 31:
            dfZ2 = atof(szLineBuf);
            break;

          default:
            TranslateGenericProperty( poFeature, nCode, szLineBuf );
            break;
        }
    }

    if( nCode == 0 )
        poDS->UnreadValue();

/* -------------------------------------------------------------------- */
/*      Create geometry                                                 */
/* -------------------------------------------------------------------- */
    OGRLineString *poLS = new OGRLineString();
    poLS->addPoint( dfX1, dfY1, dfZ1 );
    poLS->addPoint( dfX2, dfY2, dfZ2 );

    poFeature->SetGeometryDirectly( poLS );

    return poFeature;
}

/************************************************************************/
/*                         TranslateLWPOLYLINE()                        */
/************************************************************************/

OGRFeature *OGRDXFLayer::TranslateLWPOLYLINE()

{
    char szLineBuf[257];
    int nCode;
    int nPolylineFlag = 0;
    OGRFeature *poFeature = new OGRFeature( poFeatureDefn );
    OGRLineString *poLS = new OGRLineString();
    double dfX = 0.0, dfY = 0.0, dfZ = 0.0;
    int    bHaveX = FALSE, bHaveY = FALSE;

/* -------------------------------------------------------------------- */
/*      Collect information from the LWPOLYLINE object itself.          */
/* -------------------------------------------------------------------- */
    while( (nCode = poDS->ReadValue(szLineBuf,sizeof(szLineBuf))) > 0 )
    {
        switch( nCode )
        {
          case 70:
            nPolylineFlag = atoi(szLineBuf);
            break;

          case 10:
            if( bHaveX && bHaveY )
            {
                poLS->addPoint( dfX, dfY, dfZ );
                bHaveY = FALSE;
            }
            dfX = atof(szLineBuf);
            bHaveX = TRUE;
            break;

          case 20:
            if( bHaveX && bHaveY )
            {
                poLS->addPoint( dfX, dfY, dfZ );
                bHaveX = FALSE;
            }
            dfY = atof(szLineBuf);
            bHaveY = TRUE;
            break;

          case 30:
            dfZ = atof(szLineBuf);
            break;

          default:
            TranslateGenericProperty( poFeature, nCode, szLineBuf );
            break;
        }
    }

    poDS->UnreadValue();

    if( bHaveX && bHaveY )
        poLS->addPoint( dfX, dfY, dfZ );

/* -------------------------------------------------------------------- */
/*      Close polyline as polygon if necessary.                         */
/* -------------------------------------------------------------------- */
    if( (nPolylineFlag & 0x01)
        && poLS->getNumPoints() > 0 
        && (poLS->getX(poLS->getNumPoints()-1) != poLS->getX(0)
            || poLS->getY(poLS->getNumPoints()-1) != poLS->getY(0)) )
    {
        poLS->addPoint( poLS->getX(0), poLS->getY(0), poLS->getZ(0) );
    }

    poFeature->SetGeometryDirectly( poLS );

    return poFeature;
}

/************************************************************************/
/*                         TranslatePOLYLINE()                          */
/*                                                                      */
/*      We also capture the following VERTEXes.                         */
/************************************************************************/

OGRFeature *OGRDXFLayer::TranslatePOLYLINE()

{
    char szLineBuf[257];
    int nCode;
    int nPolylineFlag = 0;
    OGRFeature *poFeature = new OGRFeature( poFeatureDefn );

/* -------------------------------------------------------------------- */
/*      Collect information from the POLYLINE object itself.            */
/* -------------------------------------------------------------------- */
    while( (nCode = poDS->ReadValue(szLineBuf,sizeof(szLineBuf))) > 0 )
    {
        switch( nCode )
        {
          case 70:
            nPolylineFlag = atoi(szLineBuf);
            break;

          default:
            TranslateGenericProperty( poFeature, nCode, szLineBuf );
            break;
        }
    }

/* -------------------------------------------------------------------- */
/*      Collect VERTEXes as a linestring.                               */
/* -------------------------------------------------------------------- */
    OGRLineString *poLS = new OGRLineString();
    double dfX = 0.0, dfY = 0.0, dfZ = 0.0;

    while( nCode == 0 && !EQUAL(szLineBuf,"SEQEND") )
    {
        // Eat non-vertex objects.
        if( !EQUAL(szLineBuf,"VERTEX") )
        {
            while( (nCode = poDS->ReadValue(szLineBuf,sizeof(szLineBuf)))>0 ) {}
            continue;
        }

        // process a Vertex
        while( (nCode = poDS->ReadValue(szLineBuf,sizeof(szLineBuf))) > 0 )
        {
            switch( nCode )
            {
              case 10:
                dfX = atof(szLineBuf);
                break;
                
              case 20:
                dfY = atof(szLineBuf);
                break;
                
              case 30:
                dfZ = atof(szLineBuf);
                break;

              default:
                break;
            }
        }

        poLS->addPoint( dfX, dfY, dfZ );
    }

/* -------------------------------------------------------------------- */
/*      Close polyline as polygon if necessary.                         */
/* -------------------------------------------------------------------- */
    if( (nPolylineFlag & 0x01)
        && poLS->getNumPoints() > 0 
        && (poLS->getX(poLS->getNumPoints()-1) != poLS->getX(0)
            || poLS->getY(poLS->getNumPoints()-1) != poLS->getY(0)) )
    {
        poLS->addPoint( poLS->getX(0), poLS->getY(0), poLS->getZ(0) );
    }

    poFeature->SetGeometryDirectly( poLS );

    return poFeature;
}

/************************************************************************/
/*                          TranslateCIRCLE()                           */
/************************************************************************/

OGRFeature *OGRDXFLayer::TranslateCIRCLE()

{
    char szLineBuf[257];
    int nCode;
    OGRFeature *poFeature = new OGRFeature( poFeatureDefn );
    double dfX1 = 0.0, dfY1 = 0.0, dfZ1 = 0.0, dfRadius = 0.0;

/* -------------------------------------------------------------------- */
/*      Process values.                                                 */
/* -------------------------------------------------------------------- */
    while( (nCode = poDS->ReadValue(szLineBuf,sizeof(szLineBuf))) > 0 )
    {
        switch( nCode )
        {
          case 10:
            dfX1 = atof(szLineBuf);
            break;

          case 20:
            dfY1 = atof(szLineBuf);
            break;

          case 30:
            dfZ1 = atof(szLineBuf);
            break;

          case 40:
            dfRadius = atof(szLineBuf);
            break;

          default:
            TranslateGenericProperty( poFeature, nCode, szLineBuf );
            break;
        }
    }

    if( nCode == 0 )
        poDS->UnreadValue();

/* -------------------------------------------------------------------- */
/*      Create geometry                                                 */
/* -------------------------------------------------------------------- */
    poFeature->SetGeometryDirectly( 
        OGRGeometryFactory::approximateArcAngles( dfX1, dfY1, dfZ1, 
                                                  dfRadius, 0.0, 360.0, 
                                                  6.0 ) );

    return poFeature;
}

/************************************************************************/
/*                      GeometryInsertTransformer                       */
/************************************************************************/

class GeometryInsertTransformer : public OGRCoordinateTransformation
{
public:
    GeometryInsertTransformer() : dfXOffset(0),dfYOffset(0),dfZOffset(0) {}

    double dfXOffset;
    double dfYOffset;
    double dfZOffset;

    OGRSpatialReference *GetSourceCS() { return NULL; }
    OGRSpatialReference *GetTargetCS() { return NULL; }
    int Transform( int nCount, 
                   double *x, double *y, double *z )
        { return TransformEx( nCount, x, y, z, NULL ); }
    
    int TransformEx( int nCount, 
                     double *x, double *y, double *z = NULL,
                     int *pabSuccess = NULL )
        {
            int i;
            for( i = 0; i < nCount; i++ )
            {
                x[i] += dfXOffset;
                y[i] += dfYOffset;
                z[i] += dfZOffset;

                if( pabSuccess )
                    pabSuccess[i] = TRUE;
            }
            return TRUE;
        }
};

/************************************************************************/
/*                          TranslateINSERT()                           */
/************************************************************************/

OGRFeature *OGRDXFLayer::TranslateINSERT()

{
    char szLineBuf[257];
    int nCode;
    OGRFeature *poFeature = new OGRFeature( poFeatureDefn );
    GeometryInsertTransformer oTransformer;
    CPLString osBlockName;

/* -------------------------------------------------------------------- */
/*      Process values.                                                 */
/* -------------------------------------------------------------------- */
    while( (nCode = poDS->ReadValue(szLineBuf,sizeof(szLineBuf))) > 0 )
    {
        switch( nCode )
        {
          case 10:
            oTransformer.dfXOffset = atof(szLineBuf);
            break;

          case 20:
            oTransformer.dfYOffset = atof(szLineBuf);
            break;

          case 30:
            oTransformer.dfZOffset = atof(szLineBuf);
            break;

          case 2: 
            osBlockName = szLineBuf;
            break;
            
          default:
            TranslateGenericProperty( poFeature, nCode, szLineBuf );
            break;
        }
    }

    if( nCode == 0 )
        poDS->UnreadValue();

/* -------------------------------------------------------------------- */
/*      Try to fetch geometry corresponding to the blockname.           */
/* -------------------------------------------------------------------- */
    OGRGeometry* poGeometry = poDS->LookupBlock( osBlockName );
    if( poGeometry == NULL )
    {
        delete poFeature;
        return NULL;
    }

    poGeometry = poGeometry->clone();

/* -------------------------------------------------------------------- */
/*      Transform the geometry.  For now we just offset, but            */
/*      eventually we should also apply scaling.                        */
/* -------------------------------------------------------------------- */
    poGeometry->transform( &oTransformer );

    poFeature->SetGeometryDirectly( poGeometry );

    return poFeature;
}

/************************************************************************/
/*                      GetNextUnfilteredFeature()                      */
/************************************************************************/

OGRFeature *OGRDXFLayer::GetNextUnfilteredFeature()

{
/* -------------------------------------------------------------------- */
/*      Read the entity type.                                           */
/* -------------------------------------------------------------------- */
    OGRFeature *poFeature = NULL;
    char szLineBuf[257];
    int nCode;

    while( poFeature == NULL )
    {
        // read ahead to an entity.
        while( (nCode = poDS->ReadValue(szLineBuf,sizeof(szLineBuf))) > 0 ) {}

        if( nCode == -1 )
        {
            CPLDebug( "DXF", "Unexpected end of data without ENDSEC." );
            return NULL;
        }
        
        if( EQUAL(szLineBuf,"ENDSEC") )
        {
            //CPLDebug( "DXF", "Clean end of features at ENDSEC." );
            poDS->UnreadValue();
            return NULL;
        }

        if( EQUAL(szLineBuf,"ENDBLK") )
        {
            //CPLDebug( "DXF", "Clean end of block at ENDBLK." );
            poDS->UnreadValue();
            return NULL;
        }

/* -------------------------------------------------------------------- */
/*      Handle the entity.                                              */
/* -------------------------------------------------------------------- */
        if( EQUAL(szLineBuf,"POINT") )
        {
            poFeature = TranslatePOINT();
        }
        else if( EQUAL(szLineBuf,"LINE") )
        {
            poFeature = TranslateLINE();
        }
        else if( EQUAL(szLineBuf,"POLYLINE") )
        {
            poFeature = TranslatePOLYLINE();
        }
        else if( EQUAL(szLineBuf,"LWPOLYLINE") )
        {
            poFeature = TranslateLWPOLYLINE();
        }
        else if( EQUAL(szLineBuf,"CIRCLE") )
        {
            poFeature = TranslateCIRCLE();
        }
        else if( EQUAL(szLineBuf,"INSERT") )
        {
            poFeature = TranslateINSERT();
        }
        else
        {
            CPLDebug( "DXF", "Ignoring entity '%s'.", szLineBuf );
        }
    }

/* -------------------------------------------------------------------- */
/*      Set FID.                                                        */
/* -------------------------------------------------------------------- */
    poFeature->SetFID( iNextFID++ );
    m_nFeaturesRead++;

    return poFeature;
}

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature *OGRDXFLayer::GetNextFeature()

{
    while( TRUE )
    {
        OGRFeature *poFeature = GetNextUnfilteredFeature();

        if( poFeature == NULL )
            return NULL;

        if( (m_poFilterGeom == NULL
             || FilterGeometry( poFeature->GetGeometryRef() ) )
            && (m_poAttrQuery == NULL
                || m_poAttrQuery->Evaluate( poFeature ) ) )
        {
            return poFeature;
        }

        delete poFeature;
    }
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRDXFLayer::TestCapability( const char * pszCap )

{
    return FALSE;
}

