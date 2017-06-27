///////////////////////////////////////////////////////////////////////////////
//
// Project:  C++ Test Suite for GDAL/OGR
// Purpose:  Test general GDAL features.
// Author:   Mateusz Loskot <mateusz@loskot.net>
//
///////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2006, Mateusz Loskot <mateusz@loskot.net>
//
// This library is free software; you can redistribute it and/or
// modify it under the terms of the GNU Library General Public
// License as published by the Free Software Foundation; either
// version 2 of the License, or (at your option) any later version.
//
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Library General Public License for more details.
//
// You should have received a copy of the GNU Library General Public
// License along with this library; if not, write to the
// Free Software Foundation, Inc., 59 Temple Place - Suite 330,
// Boston, MA 02111-1307, USA.
///////////////////////////////////////////////////////////////////////////////

#include "gdal_unit_test.h"

#include <gdal_priv.h>
#include <gdal_utils.h>
#include <gdal_priv_templates.hpp>
#include <gdal.h>

#include <limits>
#include <string>

namespace tut
{
    // Common fixture with test data
    struct test_gdal_data
    {
        // Expected number of drivers
        int drv_count_;

        test_gdal_data()
            : drv_count_(0)
        {
            // Windows CE port builds with fixed number of drivers
#ifdef FRMT_aaigrid
            drv_count_++;
#endif
#ifdef FRMT_dted
            drv_count_++;
#endif
#ifdef FRMT_gtiff
            drv_count_++;
#endif
        }
    };

    // Register test group
    typedef test_group<test_gdal_data> group;
    typedef group::object object;
    group test_gdal_group("GDAL");

    // Test GDAL driver manager access
    template<>
    template<>
    void object::test<1>()
    {
        GDALDriverManager* drv_mgr = NULL;
        drv_mgr = GetGDALDriverManager();
        ensure("GetGDALDriverManager() is NULL", NULL != drv_mgr);
    }

    // Test number of registered GDAL drivers
    template<>
    template<>
    void object::test<2>()
    {
#ifdef WIN32CE
        ensure_equals("GDAL registered drivers count doesn't match",
            GDALGetDriverCount(), drv_count_);
#endif
    }

    // Test if AAIGrid driver is registered
    template<>
    template<>
    void object::test<3>()
    {
        GDALDriverH drv = GDALGetDriverByName("AAIGrid");

#ifdef FRMT_aaigrid
        ensure("AAIGrid driver is not registered", NULL != drv);
#else
        (void)drv;
        ensure(true); // Skip
#endif
    }

    // Test if DTED driver is registered
    template<>
    template<>
    void object::test<4>()
    {
        GDALDriverH drv = GDALGetDriverByName("DTED");

#ifdef FRMT_dted
        ensure("DTED driver is not registered", NULL != drv);
#else
        (void)drv;
        ensure(true); // Skip
#endif
    }

    // Test if GeoTIFF driver is registered
    template<>
    template<>
    void object::test<5>()
    {
        GDALDriverH drv = GDALGetDriverByName("GTiff");

#ifdef FRMT_gtiff
        ensure("GTiff driver is not registered", NULL != drv);
#else
        (void)drv;
        ensure(true); // Skip
#endif
    }

#define ENSURE(cond) ensure(#cond, (cond))
#define ENSURE_EQUALS(a, b) ensure_equals(#a " == " #b, (a), (b))
    
    // Test GDALDataTypeUnion()
    template<> template<> void object::test<6>()
    {
        for(int i=GDT_Byte;i<GDT_TypeCount;i++)
        {
            for(int j=GDT_Byte;j<GDT_TypeCount;j++)
            {
                GDALDataType eDT1 = static_cast<GDALDataType>(i);
                GDALDataType eDT2 = static_cast<GDALDataType>(j);
                GDALDataType eDT = GDALDataTypeUnion(eDT1,eDT2 );
                ENSURE( eDT == GDALDataTypeUnion(eDT2,eDT1) );
                ENSURE( GDALGetDataTypeSize(eDT) >= GDALGetDataTypeSize(eDT1) );
                ENSURE( GDALGetDataTypeSize(eDT) >= GDALGetDataTypeSize(eDT2) );
                ENSURE( (GDALDataTypeIsComplex(eDT) && (GDALDataTypeIsComplex(eDT1) || GDALDataTypeIsComplex(eDT2))) ||
                        (!GDALDataTypeIsComplex(eDT) && !GDALDataTypeIsComplex(eDT1) && !GDALDataTypeIsComplex(eDT2)) );
                
                ENSURE( !(GDALDataTypeIsFloating(eDT1) || GDALDataTypeIsFloating(eDT2)) || GDALDataTypeIsFloating(eDT));
                ENSURE( !(GDALDataTypeIsSigned(eDT1) || GDALDataTypeIsSigned(eDT2)) || GDALDataTypeIsSigned(eDT));
            }
        }

        ENSURE_EQUALS(GDALDataTypeUnion(GDT_Int16, GDT_UInt16), GDT_Int32);
        ENSURE_EQUALS(GDALDataTypeUnion(GDT_Int16, GDT_UInt32), GDT_Float64);
        ENSURE_EQUALS(GDALDataTypeUnion(GDT_UInt32, GDT_Int16), GDT_Float64);
        ENSURE_EQUALS(GDALDataTypeUnion(GDT_UInt32, GDT_CInt16), GDT_CFloat64);
        ENSURE_EQUALS(GDALDataTypeUnion(GDT_Float32, GDT_CInt32), GDT_CFloat64);
        ENSURE_EQUALS(GDALDataTypeUnion(GDT_CInt16, GDT_UInt32), GDT_CFloat64);
        ENSURE_EQUALS(GDALDataTypeUnion(GDT_CInt16, GDT_CFloat32), GDT_CFloat32);
        ENSURE_EQUALS(GDALDataTypeUnion(GDT_CInt32, GDT_Byte), GDT_CInt32);
        ENSURE_EQUALS(GDALDataTypeUnion(GDT_CInt32, GDT_UInt16), GDT_CInt32);
        ENSURE_EQUALS(GDALDataTypeUnion(GDT_CInt32, GDT_Int16), GDT_CInt32);
        ENSURE_EQUALS(GDALDataTypeUnion(GDT_CInt32, GDT_UInt32), GDT_CFloat64);
        ENSURE_EQUALS(GDALDataTypeUnion(GDT_CInt32, GDT_Int32), GDT_CInt32);
        ENSURE_EQUALS(GDALDataTypeUnion(GDT_CInt32, GDT_Float32), GDT_CFloat64);
        ENSURE_EQUALS(GDALDataTypeUnion(GDT_CInt32, GDT_CInt16), GDT_CInt32);
        ENSURE_EQUALS(GDALDataTypeUnion(GDT_CInt32, GDT_CFloat32), GDT_CFloat64);
        ENSURE_EQUALS(GDALDataTypeUnion(GDT_CFloat32, GDT_Byte), GDT_CFloat32);
        ENSURE_EQUALS(GDALDataTypeUnion(GDT_CFloat32, GDT_UInt16), GDT_CFloat32);
        ENSURE_EQUALS(GDALDataTypeUnion(GDT_CFloat32, GDT_Int16), GDT_CFloat32);
        ENSURE_EQUALS(GDALDataTypeUnion(GDT_CFloat32, GDT_UInt32), GDT_CFloat64);
        ENSURE_EQUALS(GDALDataTypeUnion(GDT_CFloat32, GDT_Int32), GDT_CFloat64);
        ENSURE_EQUALS(GDALDataTypeUnion(GDT_CFloat32, GDT_Float32), GDT_CFloat32);
        ENSURE_EQUALS(GDALDataTypeUnion(GDT_CFloat32, GDT_CInt16), GDT_CFloat32);
        ENSURE_EQUALS(GDALDataTypeUnion(GDT_CFloat32, GDT_CInt32), GDT_CFloat64);
    }

#undef ENSURE
#undef ENSURE_EQUALS

    // Test GDALAdjustValueToDataType()
    template<> template<> void object::test<7>()
    {
        int bClamped, bRounded;

        ensure( GDALAdjustValueToDataType(GDT_Byte,255.0,NULL,NULL) == 255.0);
        ensure( GDALAdjustValueToDataType(GDT_Byte,255.0,&bClamped,&bRounded) == 255.0 && !bClamped && !bRounded);
        ensure( GDALAdjustValueToDataType(GDT_Byte,254.4,&bClamped,&bRounded) == 254.0 && !bClamped && bRounded);
        ensure( GDALAdjustValueToDataType(GDT_Byte,-1,&bClamped,&bRounded) == 0.0 && bClamped && !bRounded);
        ensure( GDALAdjustValueToDataType(GDT_Byte,256.0,&bClamped,&bRounded) == 255.0 && bClamped && !bRounded);

        ensure( GDALAdjustValueToDataType(GDT_UInt16,65535.0,&bClamped,&bRounded) == 65535.0 && !bClamped && !bRounded);
        ensure( GDALAdjustValueToDataType(GDT_UInt16,65534.4,&bClamped,&bRounded) == 65534.0 && !bClamped && bRounded);
        ensure( GDALAdjustValueToDataType(GDT_UInt16,-1,&bClamped,&bRounded) == 0.0 && bClamped && !bRounded);
        ensure( GDALAdjustValueToDataType(GDT_UInt16,65536.0,&bClamped,&bRounded) == 65535.0 && bClamped && !bRounded);

        ensure( GDALAdjustValueToDataType(GDT_Int16,-32768.0,&bClamped,&bRounded) == -32768.0 && !bClamped && !bRounded);
        ensure( GDALAdjustValueToDataType(GDT_Int16,32767.0,&bClamped,&bRounded) == 32767.0 && !bClamped && !bRounded);
        ensure( GDALAdjustValueToDataType(GDT_Int16,-32767.4,&bClamped,&bRounded) == -32767.0 && !bClamped && bRounded);
        ensure( GDALAdjustValueToDataType(GDT_Int16,32766.4,&bClamped,&bRounded) == 32766.0 && !bClamped && bRounded);
        ensure( GDALAdjustValueToDataType(GDT_Int16,-32769.0,&bClamped,&bRounded) == -32768.0 && bClamped && !bRounded);
        ensure( GDALAdjustValueToDataType(GDT_Int16,32768.0,&bClamped,&bRounded) == 32767.0 && bClamped && !bRounded);

        ensure( GDALAdjustValueToDataType(GDT_UInt32,10000000.0,&bClamped,&bRounded) == 10000000.0 && !bClamped && !bRounded);
        ensure( GDALAdjustValueToDataType(GDT_UInt32,10000000.4,&bClamped,&bRounded) == 10000000.0 && !bClamped && bRounded);
        ensure( GDALAdjustValueToDataType(GDT_UInt32,-1,&bClamped,&bRounded) == 0.0 && bClamped && !bRounded);

        ensure( GDALAdjustValueToDataType(GDT_Int32,-10000000.0,&bClamped,&bRounded) == -10000000.0 && !bClamped && !bRounded);
        ensure( GDALAdjustValueToDataType(GDT_Int32,10000000.0,&bClamped,&bRounded) == 10000000.0 && !bClamped && !bRounded);

        ensure( GDALAdjustValueToDataType(GDT_Float32, 0.0,&bClamped,&bRounded) == 0.0 && !bClamped && !bRounded);
        ensure( GDALAdjustValueToDataType(GDT_Float32, 1e-50,&bClamped,&bRounded) == 0.0 && !bClamped && !bRounded);
        ensure( GDALAdjustValueToDataType(GDT_Float32, 1.23,&bClamped,&bRounded) == static_cast<double>(1.23f) && !bClamped && !bRounded);
        ensure( GDALAdjustValueToDataType(GDT_Float32, -1e300,&bClamped,&bRounded) == -std::numeric_limits<float>::max() && bClamped && !bRounded);
        ensure( GDALAdjustValueToDataType(GDT_Float32, 1e300,&bClamped,&bRounded) == std::numeric_limits<float>::max() && bClamped && !bRounded);

        ensure( GDALAdjustValueToDataType(GDT_Float64, 0.0,&bClamped,&bRounded) == 0.0 && !bClamped && !bRounded);
        ensure( GDALAdjustValueToDataType(GDT_Float64, 1e-50,&bClamped,&bRounded) == 1e-50 && !bClamped && !bRounded);
        ensure( GDALAdjustValueToDataType(GDT_Float64, -1e40,&bClamped,&bRounded) == -1e40 && !bClamped && !bRounded);
        ensure( GDALAdjustValueToDataType(GDT_Float64, 1e40,&bClamped,&bRounded) == 1e40 && !bClamped && !bRounded);
    }

    class FakeBand: public GDALRasterBand
    {
        protected:
            virtual CPLErr IReadBlock(int, int, void*) { return CE_None; }
            virtual CPLErr IWriteBlock( int, int, void * ) { return CE_None; }

        public:
                    FakeBand(int nXSize, int nYSize) { nBlockXSize = nXSize;
                                                       nBlockYSize = nYSize; }
    };

    class DatasetWithErrorInFlushCache: public GDALDataset
    {
            bool bHasFlushCache;
        public:
            DatasetWithErrorInFlushCache() : bHasFlushCache(false) { }
           ~DatasetWithErrorInFlushCache() { FlushCache(); }
            virtual void FlushCache(void)
            {
                if( !bHasFlushCache)
                    CPLError(CE_Failure, CPLE_AppDefined, "some error");
                GDALDataset::FlushCache();
                bHasFlushCache = true;
            }
            virtual CPLErr SetProjection(const char*) { return CE_None; }
            virtual CPLErr SetGeoTransform(double*) { return CE_None; }

            static GDALDataset* CreateCopy(const char*, GDALDataset*,
                                    int, char **,
                                    GDALProgressFunc,
                                    void *)
            {
                return new DatasetWithErrorInFlushCache();
            }

            static GDALDataset* Create(const char*, int nXSize, int nYSize, int, GDALDataType, char ** )
            {
                DatasetWithErrorInFlushCache* poDS = new DatasetWithErrorInFlushCache();
                poDS->eAccess = GA_Update;
                poDS->nRasterXSize = nXSize;
                poDS->nRasterYSize = nYSize;
                poDS->SetBand(1, new FakeBand(nXSize, nYSize));
                return poDS;
            }
    };

    // Test that GDALTranslate() detects error in flush cache
    template<> template<> void object::test<8>()
    {
        GDALDriver* poDriver = new GDALDriver();
        poDriver->SetDescription("DatasetWithErrorInFlushCache");
        poDriver->pfnCreateCopy = DatasetWithErrorInFlushCache::CreateCopy;
        GetGDALDriverManager()->RegisterDriver( poDriver );
        const char* args[] = { "-of", "DatasetWithErrorInFlushCache", NULL };
        GDALTranslateOptions* psOptions = GDALTranslateOptionsNew((char**)args, NULL);
        GDALDatasetH hSrcDS = GDALOpen("../gcore/data/byte.tif", GA_ReadOnly);
        CPLErrorReset();
        CPLPushErrorHandler(CPLQuietErrorHandler);
        GDALDatasetH hOutDS = GDALTranslate("", hSrcDS, psOptions, NULL);
        CPLPopErrorHandler();
        GDALClose(hSrcDS);
        GDALTranslateOptionsFree(psOptions);
        ensure(hOutDS == NULL);
        ensure(CPLGetLastErrorType() != CE_None);
        GetGDALDriverManager()->DeregisterDriver( poDriver );
        delete poDriver;
    }

    // Test that GDALWarp() detects error in flush cache
    template<> template<> void object::test<9>()
    {
        GDALDriver* poDriver = new GDALDriver();
        poDriver->SetDescription("DatasetWithErrorInFlushCache");
        poDriver->pfnCreate = DatasetWithErrorInFlushCache::Create;
        GetGDALDriverManager()->RegisterDriver( poDriver );
        const char* args[] = { "-of", "DatasetWithErrorInFlushCache", NULL };
        GDALWarpAppOptions* psOptions = GDALWarpAppOptionsNew((char**)args, NULL);
        GDALDatasetH hSrcDS = GDALOpen("../gcore/data/byte.tif", GA_ReadOnly);
        CPLErrorReset();
        CPLPushErrorHandler(CPLQuietErrorHandler);
        GDALDatasetH hOutDS = GDALWarp("/", NULL, 1, &hSrcDS, psOptions, NULL);
        CPLPopErrorHandler();
        GDALClose(hSrcDS);
        GDALWarpAppOptionsFree(psOptions);
        ensure(hOutDS == NULL);
        ensure(CPLGetLastErrorType() != CE_None);
        GetGDALDriverManager()->DeregisterDriver( poDriver );
        delete poDriver;
    }

    // Test that GDALSwapWords() with unaligned buffers
    template<> template<> void object::test<10>()
    {
        GByte abyBuffer[ 8 * 2 + 1 ] = { 0, 1, 2, 3, 4, 5, 6, 7, 255, 7, 6, 5, 4, 3, 2, 1, 0 };
        GDALSwapWords(abyBuffer, 4, 2, 9 );
        ensure( abyBuffer[0] == 3 );
        ensure( abyBuffer[1] == 2 );
        ensure( abyBuffer[2] == 1 );
        ensure( abyBuffer[3] == 0 );

        ensure( abyBuffer[9] == 4 );
        ensure( abyBuffer[10] == 5 );
        ensure( abyBuffer[11] == 6 );
        ensure( abyBuffer[12] == 7 );
        GDALSwapWords(abyBuffer, 4, 2, 9 );

        GDALSwapWords(abyBuffer, 8, 2, 9 );
        ensure( abyBuffer[0] == 7 );
        ensure( abyBuffer[1] == 6 );
        ensure( abyBuffer[2] == 5 );
        ensure( abyBuffer[3] == 4 );
        ensure( abyBuffer[4] == 3 );
        ensure( abyBuffer[5] == 2 );
        ensure( abyBuffer[6] == 1 );
        ensure( abyBuffer[7] == 0 );

        ensure( abyBuffer[9] == 0 );
        ensure( abyBuffer[10] == 1 );
        ensure( abyBuffer[11] == 2 );
        ensure( abyBuffer[12] == 3 );
        ensure( abyBuffer[13] == 4 );
        ensure( abyBuffer[14] == 5 );
        ensure( abyBuffer[15] == 6 );
        ensure( abyBuffer[16] == 7 );
        GDALSwapWords(abyBuffer, 4, 2, 9 );

    }

    // Test ARE_REAL_EQUAL()
    template<> template<> void object::test<11>()
    {
        ensure( ARE_REAL_EQUAL(0.0, 0.0) );
        ensure( !ARE_REAL_EQUAL(0.0, 0.1) );
        ensure( !ARE_REAL_EQUAL(0.1, 0.0) );
        ensure( ARE_REAL_EQUAL(1.0, 1.0) );
        ensure( !ARE_REAL_EQUAL(1.0, 0.99) );
        ensure( ARE_REAL_EQUAL(-std::numeric_limits<double>::min(), -std::numeric_limits<double>::min()) );
        ensure( ARE_REAL_EQUAL(std::numeric_limits<double>::min(), std::numeric_limits<double>::min()) );
        ensure( !ARE_REAL_EQUAL(std::numeric_limits<double>::min(), 0.0) );
        ensure( ARE_REAL_EQUAL(-std::numeric_limits<double>::max(), -std::numeric_limits<double>::max()) );
        ensure( ARE_REAL_EQUAL(std::numeric_limits<double>::max(), std::numeric_limits<double>::max()) );
        ensure( ARE_REAL_EQUAL(-std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity()) );
        ensure( ARE_REAL_EQUAL(std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity()) );
        ensure( !ARE_REAL_EQUAL(std::numeric_limits<double>::infinity(), std::numeric_limits<double>::max()) );
        ensure( ARE_REAL_EQUAL(-std::numeric_limits<double>::min(), -std::numeric_limits<double>::min()) );

        ensure( ARE_REAL_EQUAL(0.0f, 0.0f) );
        ensure( !ARE_REAL_EQUAL(0.0f, 0.1f) );
        ensure( !ARE_REAL_EQUAL(0.1f, 0.0f) );
        ensure( ARE_REAL_EQUAL(1.0f, 1.0f) );
        ensure( !ARE_REAL_EQUAL(1.0f, 0.99f) );
        ensure( ARE_REAL_EQUAL(-std::numeric_limits<float>::min(), -std::numeric_limits<float>::min()) );
        ensure( ARE_REAL_EQUAL(std::numeric_limits<float>::min(), std::numeric_limits<float>::min()) );
        ensure( !ARE_REAL_EQUAL(std::numeric_limits<float>::min(), 0.0f) );
        ensure( ARE_REAL_EQUAL(-std::numeric_limits<float>::max(), -std::numeric_limits<float>::max()) );
        ensure( ARE_REAL_EQUAL(std::numeric_limits<float>::max(), std::numeric_limits<float>::max()) );
        ensure( ARE_REAL_EQUAL(-std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity()) );
        ensure( ARE_REAL_EQUAL(std::numeric_limits<float>::infinity(), std::numeric_limits<float>::infinity()) );
        ensure( !ARE_REAL_EQUAL(std::numeric_limits<float>::infinity(), std::numeric_limits<float>::max()) );
    }

    // Test GDALIsValueInRange()
    template<> template<> void object::test<12>()
    {
        ensure( GDALIsValueInRange<GByte>(0) );
        ensure( GDALIsValueInRange<GByte>(255) );
        ensure( !GDALIsValueInRange<GByte>(-1) );
        ensure( !GDALIsValueInRange<GByte>(256) );
        ensure( GDALIsValueInRange<float>(std::numeric_limits<float>::max()) );
        ensure( GDALIsValueInRange<float>(std::numeric_limits<float>::infinity()) );
        ensure( !GDALIsValueInRange<float>(std::numeric_limits<double>::max()) );
        ensure( GDALIsValueInRange<double>(std::numeric_limits<double>::infinity()) );
        ensure( !GDALIsValueInRange<double>(CPLAtof("nan")) );
        ensure( !GDALIsValueInRange<float>(CPLAtof("nan")) );
        ensure( !GDALIsValueInRange<GByte>(CPLAtof("nan")) );
    }

} // namespace tut
