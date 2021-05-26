/******************************************************************************
 *
 * Project:  MVT Translator
 * Purpose:  Test Mapbox Vector Tile encoder
 * Author:   Even Rouault, Even Rouault <even dot rouault at spatialys dot com>
 *
 ******************************************************************************
 * Copyright (c) 2018, Even Rouault <even dot rouault at spatialys dot com>
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

#define ADD_MVT_TILE_READ
#include "mvt_tile.h"
#include "mvt_tile.cpp"

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_vsi.h"

#include <limits>
#include <memory>

int main()
{
    {
        // cppcheck-suppress unusedVariable
        MVTTile oEmptyTile;
        // cppcheck-suppress unassignedVariable
        CPLAssert( oEmptyTile.getSize() == 0 );
    }

    {
        MVTTile oTile;
        MVTTileLayer* poLayer = new MVTTileLayer();
        oTile.addLayer( std::shared_ptr<MVTTileLayer>(poLayer) );
        CPLAssert( oTile.getSize() ==
            1 /* layer key */ +
            1 /* layer size*/ +
            1 /* name key */ +
            1 /* version size */ +
            1 /* version key */ +
            1 /* version */
        );
    }

    {
        MVTTile oTile;
        MVTTileLayer* poLayer = new MVTTileLayer();
        oTile.addLayer( std::shared_ptr<MVTTileLayer>(poLayer) );
        MVTTileLayerFeature* poFeature = new MVTTileLayerFeature();
        poLayer->setVersion(2);
        poLayer->setName(std::string("my_layer"));
        poLayer->setExtent(4096);
        poLayer->addFeature( std::shared_ptr<MVTTileLayerFeature>(poFeature) );
        poLayer->addKey(std::string("key0"));
        {
            MVTTileLayerValue oValue;
            oValue.setStringValue(std::string(""));
            poLayer->addValue(oValue);
        }
        {
            MVTTileLayerValue oValue;
            oValue.setStringValue(std::string("x"));
            poLayer->addValue(oValue);
        }
        {
            MVTTileLayerValue oValue;
            oValue.setStringValue(std::string("1234567"));
            poLayer->addValue(oValue);
        }
        {
            MVTTileLayerValue oValue;
            oValue.setStringValue(std::string("12345678"));
            poLayer->addValue(oValue);
        }
        {
            MVTTileLayerValue oValue;
            oValue.setStringValue(std::string("123456789"));
            poLayer->addValue(oValue);
        }
        {
            MVTTileLayerValue oValue;
            oValue.setIntValue(-1);
            poLayer->addValue(oValue);
        }
        {
            MVTTileLayerValue oValue;
            oValue.setUIntValue(1);
            poLayer->addValue(oValue);
        }
        {
            MVTTileLayerValue oValue;
            oValue.setSIntValue(-1);
            poLayer->addValue(oValue);
        }
        {
            MVTTileLayerValue oValue;
            oValue.setFloatValue(1.25f);
            poLayer->addValue(oValue);
        }
        {
            MVTTileLayerValue oValue;
            oValue.setDoubleValue(1.25);
            poLayer->addValue(oValue);
        }
        {
            MVTTileLayerValue oValue;
            oValue.setBoolValue(true);
            poLayer->addValue(oValue);
        }

        {
            MVTTileLayerValue oValue;
            oValue.setValue(123456);
            poLayer->addValue(oValue);
        }
        {
            MVTTileLayerValue oValue;
            oValue.setValue(-123456);
            poLayer->addValue(oValue);
        }
        {
            MVTTileLayerValue oValue;
            oValue.setValue(123456.0);
            poLayer->addValue(oValue);
        }
        {
            MVTTileLayerValue oValue;
            oValue.setValue(-123456.0);
            poLayer->addValue(oValue);
        }
        {
            MVTTileLayerValue oValue;
            oValue.setValue(1.25);
            poLayer->addValue(oValue);
        }
        {
            MVTTileLayerValue oValue;
            oValue.setValue(1.256789);
            poLayer->addValue(oValue);
        }
        {
            MVTTileLayerValue oValue;
            oValue.setValue(std::numeric_limits<double>::infinity());
            poLayer->addValue(oValue);
        }

        poFeature->setId(1);
        poFeature->addTag(0);
        poFeature->addTag(0);
        poFeature->setType(MVTTileLayerFeature::GeomType::POINT);
        poFeature->addGeometry(9);
        poFeature->addGeometry(0);
        poFeature->addGeometry(0);

        poLayer->addFeature(
            std::shared_ptr<MVTTileLayerFeature>(new MVTTileLayerFeature()) );

        oTile.addLayer( std::shared_ptr<MVTTileLayer>(new MVTTileLayer()) );

        poLayer = new MVTTileLayer();
        oTile.addLayer( std::shared_ptr<MVTTileLayer>(poLayer) );
        poLayer->addValue(MVTTileLayerValue());

        size_t nSize = oTile.getSize();
        GByte* pabyBuffer = static_cast<GByte*>(CPLMalloc(nSize));
        oTile.write(pabyBuffer);
        VSILFILE* fp = VSIFOpenL("out.gpb", "wb");
        if( fp )
        {
            VSIFWriteL(pabyBuffer, 1, nSize, fp);
            VSIFCloseL(fp);
        }

        MVTTile oTileDeserialized;
        bool bRet = oTileDeserialized.read(pabyBuffer, pabyBuffer + nSize);
        CPL_IGNORE_RET_VAL(bRet);
        CPLAssert(bRet);
        size_t nSize2 = oTileDeserialized.getSize();
        CPLAssert(nSize == nSize2);
        GByte* pabyBuffer2 = static_cast<GByte*>(CPLMalloc(nSize2));
        oTileDeserialized.write(pabyBuffer2);
        CPLAssert(memcmp(pabyBuffer, pabyBuffer2, nSize) == 0);
        CPLFree(pabyBuffer);
        CPLFree(pabyBuffer2);
    }

    return 0;
}
