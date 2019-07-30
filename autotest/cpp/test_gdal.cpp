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

#include "gdal_priv.h"
#include "gdal_utils.h"
#include "gdal_priv_templates.hpp"
#include "gdal.h"

#include <limits>
#include <string>

#include "test_data.h"

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
        GDALDriverManager* drv_mgr = nullptr;
        drv_mgr = GetGDALDriverManager();
        ensure("GetGDALDriverManager() is NULL", nullptr != drv_mgr);
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

        ensure( GDALAdjustValueToDataType(GDT_Byte,255.0,nullptr,nullptr) == 255.0);
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
        ensure( GDALAdjustValueToDataType(GDT_Float32, std::numeric_limits<float>::infinity(),&bClamped,&bRounded) == std::numeric_limits<float>::infinity() && !bClamped && !bRounded);
        ensure( GDALAdjustValueToDataType(GDT_Float32, -std::numeric_limits<float>::infinity(),&bClamped,&bRounded) == -std::numeric_limits<float>::infinity() && !bClamped && !bRounded);
        {
            double dfNan = std::numeric_limits<double>::quiet_NaN();
            double dfGot = GDALAdjustValueToDataType(GDT_Float32, dfNan,&bClamped,&bRounded);
            ensure( memcmp(&dfNan, &dfGot, sizeof(double)) == 0 && !bClamped && !bRounded);
        }

        ensure( GDALAdjustValueToDataType(GDT_Float64, 0.0,&bClamped,&bRounded) == 0.0 && !bClamped && !bRounded);
        ensure( GDALAdjustValueToDataType(GDT_Float64, 1e-50,&bClamped,&bRounded) == 1e-50 && !bClamped && !bRounded);
        ensure( GDALAdjustValueToDataType(GDT_Float64, -1e40,&bClamped,&bRounded) == -1e40 && !bClamped && !bRounded);
        ensure( GDALAdjustValueToDataType(GDT_Float64, 1e40,&bClamped,&bRounded) == 1e40 && !bClamped && !bRounded);
        ensure( GDALAdjustValueToDataType(GDT_Float64, std::numeric_limits<float>::infinity(),&bClamped,&bRounded) == std::numeric_limits<float>::infinity() && !bClamped && !bRounded);
        ensure( GDALAdjustValueToDataType(GDT_Float64, -std::numeric_limits<float>::infinity(),&bClamped,&bRounded) == -std::numeric_limits<float>::infinity() && !bClamped && !bRounded);
        {
            double dfNan = std::numeric_limits<double>::quiet_NaN();
            double dfGot = GDALAdjustValueToDataType(GDT_Float64, dfNan,&bClamped,&bRounded);
            ensure( memcmp(&dfNan, &dfGot, sizeof(double)) == 0 && !bClamped && !bRounded);
        }
    }

    class FakeBand: public GDALRasterBand
    {
        protected:
            virtual CPLErr IReadBlock(int, int, void*) override { return CE_None; }
            virtual CPLErr IWriteBlock( int, int, void * ) override { return CE_None; }

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
            virtual void FlushCache(void) override
            {
                if( !bHasFlushCache)
                    CPLError(CE_Failure, CPLE_AppDefined, "some error");
                GDALDataset::FlushCache();
                bHasFlushCache = true;
            }
            virtual CPLErr _SetProjection(const char*) override { return CE_None; }
            CPLErr SetSpatialRef(const OGRSpatialReference* poSRS) override {
                return OldSetProjectionFromSetSpatialRef(poSRS);
            }
            virtual CPLErr SetGeoTransform(double*) override { return CE_None; }

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
        const char* args[] = { "-of", "DatasetWithErrorInFlushCache", nullptr };
        GDALTranslateOptions* psOptions = GDALTranslateOptionsNew((char**)args, nullptr);
        GDALDatasetH hSrcDS = GDALOpen(GCORE_DATA_DIR "byte.tif", GA_ReadOnly);
        CPLErrorReset();
        CPLPushErrorHandler(CPLQuietErrorHandler);
        GDALDatasetH hOutDS = GDALTranslate("", hSrcDS, psOptions, nullptr);
        CPLPopErrorHandler();
        GDALClose(hSrcDS);
        GDALTranslateOptionsFree(psOptions);
        ensure(hOutDS == nullptr);
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
        const char* args[] = { "-of", "DatasetWithErrorInFlushCache", nullptr };
        GDALWarpAppOptions* psOptions = GDALWarpAppOptionsNew((char**)args, nullptr);
        GDALDatasetH hSrcDS = GDALOpen(GCORE_DATA_DIR "byte.tif", GA_ReadOnly);
        CPLErrorReset();
        CPLPushErrorHandler(CPLQuietErrorHandler);
        GDALDatasetH hOutDS = GDALWarp("/", nullptr, 1, &hSrcDS, psOptions, nullptr);
        CPLPopErrorHandler();
        GDALClose(hSrcDS);
        GDALWarpAppOptionsFree(psOptions);
        ensure(hOutDS == nullptr);
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

    // Test GDALDataTypeIsInteger()
    template<> template<> void object::test<13>()
    {
        ensure( !GDALDataTypeIsInteger(GDT_Unknown) );
        ensure_equals( GDALDataTypeIsInteger(GDT_Byte), TRUE );
        ensure_equals( GDALDataTypeIsInteger(GDT_UInt16), TRUE );
        ensure_equals( GDALDataTypeIsInteger(GDT_Int16), TRUE );
        ensure_equals( GDALDataTypeIsInteger(GDT_UInt32), TRUE );
        ensure_equals( GDALDataTypeIsInteger(GDT_Int32), TRUE );
        ensure( !GDALDataTypeIsInteger(GDT_Float32) );
        ensure( !GDALDataTypeIsInteger(GDT_Float64) );
        ensure_equals( GDALDataTypeIsInteger(GDT_CInt16), TRUE );
        ensure_equals( GDALDataTypeIsInteger(GDT_CInt32), TRUE );
        ensure( !GDALDataTypeIsInteger(GDT_CFloat32) );
        ensure( !GDALDataTypeIsInteger(GDT_CFloat64) );
    }

    // Test GDALDataTypeIsFloating()
    template<> template<> void object::test<14>()
    {
        ensure( !GDALDataTypeIsFloating(GDT_Unknown) );
        ensure( !GDALDataTypeIsFloating(GDT_Byte) );
        ensure( !GDALDataTypeIsFloating(GDT_UInt16) );
        ensure( !GDALDataTypeIsFloating(GDT_Int16) );
        ensure( !GDALDataTypeIsFloating(GDT_UInt32) );
        ensure( !GDALDataTypeIsFloating(GDT_Int32) );
        ensure_equals( GDALDataTypeIsFloating(GDT_Float32), TRUE );
        ensure_equals( GDALDataTypeIsFloating(GDT_Float64), TRUE );
        ensure( !GDALDataTypeIsFloating(GDT_CInt16) );
        ensure( !GDALDataTypeIsFloating(GDT_CInt32) );
        ensure_equals( GDALDataTypeIsFloating(GDT_CFloat32), TRUE );
        ensure_equals( GDALDataTypeIsFloating(GDT_CFloat64), TRUE );
    }

    // Test GDALDataTypeIsComplex()
    template<> template<> void object::test<15>()
    {
        ensure( !GDALDataTypeIsComplex(GDT_Unknown) );
        ensure( !GDALDataTypeIsComplex(GDT_Byte) );
        ensure( !GDALDataTypeIsComplex(GDT_UInt16) );
        ensure( !GDALDataTypeIsComplex(GDT_Int16) );
        ensure( !GDALDataTypeIsComplex(GDT_UInt32) );
        ensure( !GDALDataTypeIsComplex(GDT_Int32) );
        ensure( !GDALDataTypeIsComplex(GDT_Float32) );
        ensure( !GDALDataTypeIsComplex(GDT_Float64) );
        ensure_equals( GDALDataTypeIsComplex(GDT_CInt16), TRUE );
        ensure_equals( GDALDataTypeIsComplex(GDT_CInt32), TRUE );
        ensure_equals( GDALDataTypeIsComplex(GDT_CFloat32), TRUE );
        ensure_equals( GDALDataTypeIsComplex(GDT_CFloat64), TRUE );
    }

    // Test GDALDataTypeIsConversionLossy()
    template<> template<> void object::test<16>()
    {
        ensure( !GDALDataTypeIsConversionLossy(GDT_Byte, GDT_Byte) );
        ensure( !GDALDataTypeIsConversionLossy(GDT_Byte, GDT_UInt16) );
        ensure( !GDALDataTypeIsConversionLossy(GDT_Byte, GDT_Int16) );
        ensure( !GDALDataTypeIsConversionLossy(GDT_Byte, GDT_UInt32) );
        ensure( !GDALDataTypeIsConversionLossy(GDT_Byte, GDT_Int32) );
        ensure( !GDALDataTypeIsConversionLossy(GDT_Byte, GDT_Float32) );
        ensure( !GDALDataTypeIsConversionLossy(GDT_Byte, GDT_Float64) );
        ensure( !GDALDataTypeIsConversionLossy(GDT_Byte, GDT_CInt16) );
        ensure( !GDALDataTypeIsConversionLossy(GDT_Byte, GDT_CInt32) );
        ensure( !GDALDataTypeIsConversionLossy(GDT_Byte, GDT_CFloat32) );
        ensure( !GDALDataTypeIsConversionLossy(GDT_Byte, GDT_CFloat64) );

        ensure( GDALDataTypeIsConversionLossy(GDT_UInt16, GDT_Byte) );
        ensure( !GDALDataTypeIsConversionLossy(GDT_UInt16, GDT_UInt16) );
        ensure( GDALDataTypeIsConversionLossy(GDT_UInt16, GDT_Int16) );
        ensure( !GDALDataTypeIsConversionLossy(GDT_UInt16, GDT_UInt32) );
        ensure( !GDALDataTypeIsConversionLossy(GDT_UInt16, GDT_Int32) );
        ensure( !GDALDataTypeIsConversionLossy(GDT_UInt16, GDT_Float32) );
        ensure( !GDALDataTypeIsConversionLossy(GDT_UInt16, GDT_Float64) );
        ensure( GDALDataTypeIsConversionLossy(GDT_UInt16, GDT_CInt16) );
        ensure( !GDALDataTypeIsConversionLossy(GDT_UInt16, GDT_CInt32) );
        ensure( !GDALDataTypeIsConversionLossy(GDT_UInt16, GDT_CFloat32) );
        ensure( !GDALDataTypeIsConversionLossy(GDT_UInt16, GDT_CFloat64) );

        ensure( GDALDataTypeIsConversionLossy(GDT_Int16, GDT_Byte) );
        ensure( GDALDataTypeIsConversionLossy(GDT_Int16, GDT_UInt16) );
        ensure( !GDALDataTypeIsConversionLossy(GDT_Int16, GDT_Int16) );
        ensure( GDALDataTypeIsConversionLossy(GDT_Int16, GDT_UInt32) );
        ensure( !GDALDataTypeIsConversionLossy(GDT_Int16, GDT_Int32) );
        ensure( !GDALDataTypeIsConversionLossy(GDT_Int16, GDT_Float32) );
        ensure( !GDALDataTypeIsConversionLossy(GDT_Int16, GDT_Float64) );
        ensure( !GDALDataTypeIsConversionLossy(GDT_Int16, GDT_CInt16) );
        ensure( !GDALDataTypeIsConversionLossy(GDT_Int16, GDT_CInt32) );
        ensure( !GDALDataTypeIsConversionLossy(GDT_Int16, GDT_CFloat32) );
        ensure( !GDALDataTypeIsConversionLossy(GDT_Int16, GDT_CFloat64) );

        ensure( GDALDataTypeIsConversionLossy(GDT_UInt32, GDT_Byte) );
        ensure( GDALDataTypeIsConversionLossy(GDT_UInt32, GDT_UInt16) );
        ensure( GDALDataTypeIsConversionLossy(GDT_UInt32, GDT_Int16) );
        ensure( !GDALDataTypeIsConversionLossy(GDT_UInt32, GDT_UInt32) );
        ensure( GDALDataTypeIsConversionLossy(GDT_UInt32, GDT_Int32) );
        ensure( GDALDataTypeIsConversionLossy(GDT_UInt32, GDT_Float32) );
        ensure( !GDALDataTypeIsConversionLossy(GDT_UInt32, GDT_Float64) );
        ensure( GDALDataTypeIsConversionLossy(GDT_UInt32, GDT_CInt16) );
        ensure( GDALDataTypeIsConversionLossy(GDT_UInt32, GDT_CInt32) );
        ensure( GDALDataTypeIsConversionLossy(GDT_UInt32, GDT_CFloat32) );
        ensure( !GDALDataTypeIsConversionLossy(GDT_UInt32, GDT_CFloat64) );

        ensure( GDALDataTypeIsConversionLossy(GDT_Int32, GDT_Byte) );
        ensure( GDALDataTypeIsConversionLossy(GDT_Int32, GDT_UInt16) );
        ensure( GDALDataTypeIsConversionLossy(GDT_Int32, GDT_Int16) );
        ensure( GDALDataTypeIsConversionLossy(GDT_Int32, GDT_UInt32) );
        ensure( !GDALDataTypeIsConversionLossy(GDT_Int32, GDT_Int32) );
        ensure( GDALDataTypeIsConversionLossy(GDT_Int32, GDT_Float32) );
        ensure( !GDALDataTypeIsConversionLossy(GDT_Int32, GDT_Float64) );
        ensure( GDALDataTypeIsConversionLossy(GDT_Int32, GDT_CInt16) );
        ensure( !GDALDataTypeIsConversionLossy(GDT_Int32, GDT_CInt32) );
        ensure( GDALDataTypeIsConversionLossy(GDT_Int32, GDT_CFloat32) );
        ensure( !GDALDataTypeIsConversionLossy(GDT_Int32, GDT_CFloat64) );

        ensure( GDALDataTypeIsConversionLossy(GDT_Float32, GDT_Byte) );
        ensure( GDALDataTypeIsConversionLossy(GDT_Float32, GDT_UInt16) );
        ensure( GDALDataTypeIsConversionLossy(GDT_Float32, GDT_Int16) );
        ensure( GDALDataTypeIsConversionLossy(GDT_Float32, GDT_UInt32) );
        ensure( GDALDataTypeIsConversionLossy(GDT_Float32, GDT_Int32) );
        ensure( !GDALDataTypeIsConversionLossy(GDT_Float32, GDT_Float32) );
        ensure( !GDALDataTypeIsConversionLossy(GDT_Float32, GDT_Float64) );
        ensure( GDALDataTypeIsConversionLossy(GDT_Float32, GDT_CInt16) );
        ensure( GDALDataTypeIsConversionLossy(GDT_Float32, GDT_CInt32) );
        ensure( !GDALDataTypeIsConversionLossy(GDT_Float32, GDT_CFloat32) );
        ensure( !GDALDataTypeIsConversionLossy(GDT_Float32, GDT_CFloat64) );

        ensure( GDALDataTypeIsConversionLossy(GDT_Float64, GDT_Byte) );
        ensure( GDALDataTypeIsConversionLossy(GDT_Float64, GDT_UInt16) );
        ensure( GDALDataTypeIsConversionLossy(GDT_Float64, GDT_Int16) );
        ensure( GDALDataTypeIsConversionLossy(GDT_Float64, GDT_UInt32) );
        ensure( GDALDataTypeIsConversionLossy(GDT_Float64, GDT_Int32) );
        ensure( GDALDataTypeIsConversionLossy(GDT_Float64, GDT_Float32) );
        ensure( !GDALDataTypeIsConversionLossy(GDT_Float64, GDT_Float64) );
        ensure( GDALDataTypeIsConversionLossy(GDT_Float64, GDT_CInt16) );
        ensure( GDALDataTypeIsConversionLossy(GDT_Float64, GDT_CInt32) );
        ensure( GDALDataTypeIsConversionLossy(GDT_Float64, GDT_CFloat32) );
        ensure( !GDALDataTypeIsConversionLossy(GDT_Float64, GDT_CFloat64) );

        ensure( GDALDataTypeIsConversionLossy(GDT_CInt16, GDT_Byte) );
        ensure( GDALDataTypeIsConversionLossy(GDT_CInt16, GDT_UInt16) );
        ensure( GDALDataTypeIsConversionLossy(GDT_CInt16, GDT_Int16) );
        ensure( GDALDataTypeIsConversionLossy(GDT_CInt16, GDT_UInt32) );
        ensure( GDALDataTypeIsConversionLossy(GDT_CInt16, GDT_Int32) );
        ensure( GDALDataTypeIsConversionLossy(GDT_CInt16, GDT_Float32) );
        ensure( GDALDataTypeIsConversionLossy(GDT_CInt16, GDT_Float64) );
        ensure( !GDALDataTypeIsConversionLossy(GDT_CInt16, GDT_CInt16) );
        ensure( !GDALDataTypeIsConversionLossy(GDT_CInt16, GDT_CInt32) );
        ensure( !GDALDataTypeIsConversionLossy(GDT_CInt16, GDT_CFloat32) );
        ensure( !GDALDataTypeIsConversionLossy(GDT_CInt16, GDT_CFloat64) );

        ensure( GDALDataTypeIsConversionLossy(GDT_CInt32, GDT_Byte) );
        ensure( GDALDataTypeIsConversionLossy(GDT_CInt32, GDT_UInt16) );
        ensure( GDALDataTypeIsConversionLossy(GDT_CInt32, GDT_Int16) );
        ensure( GDALDataTypeIsConversionLossy(GDT_CInt32, GDT_UInt32) );
        ensure( GDALDataTypeIsConversionLossy(GDT_CInt32, GDT_Int32) );
        ensure( GDALDataTypeIsConversionLossy(GDT_CInt32, GDT_Float32) );
        ensure( GDALDataTypeIsConversionLossy(GDT_CInt32, GDT_Float64) );
        ensure( GDALDataTypeIsConversionLossy(GDT_CInt32, GDT_CInt16) );
        ensure( !GDALDataTypeIsConversionLossy(GDT_CInt32, GDT_CInt32) );
        ensure( GDALDataTypeIsConversionLossy(GDT_CInt32, GDT_CFloat32) );
        ensure( !GDALDataTypeIsConversionLossy(GDT_CInt32, GDT_CFloat64) );

        ensure( GDALDataTypeIsConversionLossy(GDT_CFloat32, GDT_Byte) );
        ensure( GDALDataTypeIsConversionLossy(GDT_CFloat32, GDT_UInt16) );
        ensure( GDALDataTypeIsConversionLossy(GDT_CFloat32, GDT_Int16) );
        ensure( GDALDataTypeIsConversionLossy(GDT_CFloat32, GDT_UInt32) );
        ensure( GDALDataTypeIsConversionLossy(GDT_CFloat32, GDT_Int32) );
        ensure( GDALDataTypeIsConversionLossy(GDT_CFloat32, GDT_Float32) );
        ensure( GDALDataTypeIsConversionLossy(GDT_CFloat32, GDT_Float64) );
        ensure( GDALDataTypeIsConversionLossy(GDT_CFloat32, GDT_CInt16) );
        ensure( GDALDataTypeIsConversionLossy(GDT_CFloat32, GDT_CInt32) );
        ensure( !GDALDataTypeIsConversionLossy(GDT_CFloat32, GDT_CFloat32) );
        ensure( !GDALDataTypeIsConversionLossy(GDT_CFloat32, GDT_CFloat64) );

        ensure( GDALDataTypeIsConversionLossy(GDT_CFloat64, GDT_Byte) );
        ensure( GDALDataTypeIsConversionLossy(GDT_CFloat64, GDT_UInt16) );
        ensure( GDALDataTypeIsConversionLossy(GDT_CFloat64, GDT_Int16) );
        ensure( GDALDataTypeIsConversionLossy(GDT_CFloat64, GDT_UInt32) );
        ensure( GDALDataTypeIsConversionLossy(GDT_CFloat64, GDT_Int32) );
        ensure( GDALDataTypeIsConversionLossy(GDT_CFloat64, GDT_Float32) );
        ensure( GDALDataTypeIsConversionLossy(GDT_CFloat64, GDT_Float64) );
        ensure( GDALDataTypeIsConversionLossy(GDT_CFloat64, GDT_CInt16) );
        ensure( GDALDataTypeIsConversionLossy(GDT_CFloat64, GDT_CInt32) );
        ensure( GDALDataTypeIsConversionLossy(GDT_CFloat64, GDT_CFloat32) );
        ensure( !GDALDataTypeIsConversionLossy(GDT_CFloat64, GDT_CFloat64) );
    }

    // Test GDALDataset::GetBands()
    template<> template<> void object::test<17>()
    {
        GDALDatasetUniquePtr poDS(
            GDALDriver::FromHandle(
                GDALGetDriverByName("MEM"))->Create("", 1, 1, 3, GDT_Byte, nullptr));
        int nExpectedNumber = 1;
        for( auto&& poBand: poDS->GetBands() )
        {
            ensure_equals( poBand->GetBand(), nExpectedNumber );
            nExpectedNumber ++;
        }
        ensure_equals( nExpectedNumber, 3 + 1 );

        ensure_equals( poDS->GetBands().size(), 3U );
        ensure_equals( poDS->GetBands()[0], poDS->GetRasterBand(1) );
        ensure_equals( poDS->GetBands()[static_cast<size_t>(0)], poDS->GetRasterBand(1) );

    }

    template<> template<> void object::test<18>()
    {
        class myArray: public GDALMDArray
        {
                GDALExtendedDataType m_dt;
                std::vector<std::shared_ptr<GDALDimension>> m_dims;
                std::vector<GUInt64> m_blockSize;

                static std::vector<std::shared_ptr<GDALDimension>> BuildDims(
                    const std::vector<GUInt64>& sizes)
                {
                    std::vector<std::shared_ptr<GDALDimension>> dims;
                    for( const auto sz: sizes )
                    {
                        dims.emplace_back(
                            std::make_shared<GDALDimension>("", "", "", "", sz));
                    }
                    return dims;
                }

            protected:
                bool IRead(const GUInt64*,
                               const size_t*,
                               const GInt64*,
                               const GPtrDiff_t*,
                               const GDALExtendedDataType&,
                               void*) const override { return false; }
            public:
                myArray(GDALDataType eDT,
                        const std::vector<GUInt64>& sizes,
                        const std::vector<GUInt64>& blocksizes):
                    GDALAbstractMDArray("", "array"),
                    GDALMDArray("", "array"),
                    m_dt(GDALExtendedDataType::Create(eDT)),
                    m_dims(BuildDims(sizes)),
                    m_blockSize(blocksizes)
                {
                }

                myArray(const GDALExtendedDataType& dt,
                        const std::vector<GUInt64>& sizes,
                        const std::vector<GUInt64>& blocksizes):
                    GDALAbstractMDArray("", "array"),
                    GDALMDArray("", "array"),
                    m_dt(dt),
                    m_dims(BuildDims(sizes)),
                    m_blockSize(blocksizes)
                {
                }

                bool IsWritable() const override { return true; }

                static std::shared_ptr<myArray> Create(GDALDataType eDT,
                                const std::vector<GUInt64>& sizes,
                                const std::vector<GUInt64>& blocksizes)
                {
                    auto ar(std::shared_ptr<myArray>(new myArray(eDT, sizes, blocksizes)));
                    ar->SetSelf(ar);
                    return ar;
                }

                static std::shared_ptr<myArray> Create(
                                const GDALExtendedDataType& dt,
                                const std::vector<GUInt64>& sizes,
                                const std::vector<GUInt64>& blocksizes)
                {
                    auto ar(std::shared_ptr<myArray>(new myArray(dt, sizes, blocksizes)));
                    ar->SetSelf(ar);
                    return ar;
                }

                const std::vector<std::shared_ptr<GDALDimension>>& GetDimensions() const override
                {
                    return m_dims;
                }

                const GDALExtendedDataType& GetDataType() const override
                {
                    return m_dt;
                }

                std::vector<GUInt64> GetBlockSize() const override
                {
                    return m_blockSize;
                }
        };

        {
            auto ar(myArray::Create(GDT_UInt16, {3000,1000,2000},{32,64,128}));
            ensure_equals(ar->at(0)->GetDimensionCount(), 2U);
            ensure_equals(ar->at(2999,999,1999)->GetDimensionCount(), 0U);
            CPLPushErrorHandler(CPLQuietErrorHandler);
            ensure(ar->at(3000,0,0) == nullptr);
            ensure(ar->at(0,0,0,0) == nullptr);
            ensure((*ar)["foo"] == nullptr);
            CPLPopErrorHandler();
        }

        {
            std::vector<std::unique_ptr<GDALEDTComponent>> comps;
            comps.emplace_back(std::unique_ptr<GDALEDTComponent>(new
                GDALEDTComponent("f\\o\"o", 0, GDALExtendedDataType::Create(GDT_Int32))));
            auto dt(GDALExtendedDataType::Create("", 4, std::move(comps)));
            auto ar(myArray::Create(dt, {3000,1000,2000},{32,64,128}));
            ensure((*ar)["f\\o\"o"] != nullptr);
        }

        {
            myArray ar(GDT_UInt16, {}, {});

            CPLPushErrorHandler(CPLQuietErrorHandler);
            ensure(ar.GetView("[...]") == nullptr);
            CPLPopErrorHandler();

            auto cs = ar.GetProcessingChunkSize(0);
            ensure_equals( cs.size(), 0U );

            struct TmpStructNoDim
            {
                static bool func(GDALAbstractMDArray* p_ar,
                                const GUInt64* chunk_array_start_idx,
                                const size_t* chunk_count,
                                GUInt64 iCurChunk,
                                GUInt64 nChunkCount,
                                void* user_data)
                {
                    ensure( p_ar->GetName() == "array" );
                    ensure(chunk_array_start_idx == nullptr);
                    ensure(chunk_count == nullptr);
                    ensure_equals(iCurChunk, 1U);
                    ensure_equals(nChunkCount, 1U);
                    *static_cast<bool*>(user_data) = true;
                    return true;
                }
            };

            bool b = false;
            ar.ProcessPerChunk(nullptr, nullptr, nullptr, TmpStructNoDim::func, &b);
            ensure(b);
        }

        struct ChunkDef
        {
            std::vector<GUInt64> array_start_idx;
            std::vector<GUInt64> count;
        };

        struct TmpStruct
        {
            static bool func(GDALAbstractMDArray* p_ar,
                            const GUInt64* chunk_array_start_idx,
                            const size_t* chunk_count,
                            GUInt64 iCurChunk,
                            GUInt64 nChunkCount,
                            void* user_data)
            {
                ensure( p_ar->GetName() == "array" );
                std::vector<ChunkDef>* p_chunkDefs =
                    static_cast<std::vector<ChunkDef>*>(user_data);
                std::vector<GUInt64> v_chunk_array_start_idx;
                v_chunk_array_start_idx.insert(v_chunk_array_start_idx.end(),
                        chunk_array_start_idx,
                        chunk_array_start_idx + p_ar->GetDimensionCount());
                std::vector<GUInt64> v_chunk_count;
                v_chunk_count.insert(v_chunk_count.end(),
                            chunk_count,
                            chunk_count + p_ar->GetDimensionCount());
                ChunkDef chunkDef;
                chunkDef.array_start_idx = std::move(v_chunk_array_start_idx);
                chunkDef.count = std::move(v_chunk_count);
                p_chunkDefs->emplace_back(std::move(chunkDef));
                ensure_equals(p_chunkDefs->size(), iCurChunk);
                ensure( iCurChunk > 0 );
                ensure( iCurChunk <= nChunkCount );
                return true;
            }
        };


        {
            myArray ar(GDT_UInt16, {3000,1000,2000},{32,64,128});
            {
                auto cs = ar.GetProcessingChunkSize(0);
                ensure_equals( cs.size(), 3U );
                ensure_equals( cs[0], 32U );
                ensure_equals( cs[1], 64U );
                ensure_equals( cs[2], 128U );
            }
            {
                auto cs = ar.GetProcessingChunkSize(40*1000*1000);
                ensure_equals( cs.size(), 3U );
                ensure_equals( cs[0], 32U );
                ensure_equals( cs[1], 256U );
                ensure_equals( cs[2], 2000U );

                std::vector<ChunkDef> chunkDefs;

                // Error cases of input parameters of ProcessPerChunk()
                {
                    // array_start_idx[0] + count[0] > 3000
                    std::vector<GUInt64> array_start_idx{ 1, 0, 0 };
                    std::vector<GUInt64> count{ 3000, 1000, 2000 };
                    CPLPushErrorHandler(CPLQuietErrorHandler);
                    ensure(!ar.ProcessPerChunk(
                        array_start_idx.data(), count.data(), cs.data(),
                        TmpStruct::func, &chunkDefs));
                    CPLPopErrorHandler();

                }
                {
                    // array_start_idx[0] >= 3000
                    std::vector<GUInt64> array_start_idx{ 3000, 0, 0 };
                    std::vector<GUInt64> count{ 1, 1000, 2000 };
                    CPLPushErrorHandler(CPLQuietErrorHandler);
                    ensure(!ar.ProcessPerChunk(
                        array_start_idx.data(), count.data(), cs.data(),
                        TmpStruct::func, &chunkDefs));
                    CPLPopErrorHandler();

                }
                {
                    // count[0] > 3000
                    std::vector<GUInt64> array_start_idx{ 0, 0, 0 };
                    std::vector<GUInt64> count{ 3001, 1000, 2000 };
                    CPLPushErrorHandler(CPLQuietErrorHandler);
                    ensure(!ar.ProcessPerChunk(
                        array_start_idx.data(), count.data(), cs.data(),
                        TmpStruct::func, &chunkDefs));
                    CPLPopErrorHandler();

                }
                {
                    // count[0] == 0
                    std::vector<GUInt64> array_start_idx{ 0, 0, 0 };
                    std::vector<GUInt64> count{ 0, 1000, 2000 };
                    CPLPushErrorHandler(CPLQuietErrorHandler);
                    ensure(!ar.ProcessPerChunk(
                        array_start_idx.data(), count.data(), cs.data(),
                        TmpStruct::func, &chunkDefs));
                    CPLPopErrorHandler();
                }
                {
                    // myCustomChunkSize[0] == 0
                    std::vector<GUInt64> array_start_idx{ 0, 0, 0 };
                    std::vector<GUInt64> count{ 3000, 1000, 2000 };
                    std::vector<size_t> myCustomChunkSize{ 0, 1000, 2000 };
                    CPLPushErrorHandler(CPLQuietErrorHandler);
                    ensure(!ar.ProcessPerChunk(
                        array_start_idx.data(), count.data(),
                        myCustomChunkSize.data(),
                        TmpStruct::func, &chunkDefs));
                    CPLPopErrorHandler();
                }
                {
                    // myCustomChunkSize[0] > 3000
                    std::vector<GUInt64> array_start_idx{ 0, 0, 0 };
                    std::vector<GUInt64> count{ 3000, 1000, 2000 };
                    std::vector<size_t> myCustomChunkSize{ 3001, 1000, 2000 };
                    CPLPushErrorHandler(CPLQuietErrorHandler);
                    ensure(!ar.ProcessPerChunk(
                        array_start_idx.data(), count.data(),
                        myCustomChunkSize.data(),
                        TmpStruct::func, &chunkDefs));
                    CPLPopErrorHandler();
                }

                std::vector<GUInt64> array_start_idx{ 1500, 256, 0 };
                std::vector<GUInt64> count{ 99, 512, 2000 };
                ensure(ar.ProcessPerChunk(
                    array_start_idx.data(), count.data(), cs.data(),
                    TmpStruct::func, &chunkDefs));

                size_t nExpectedChunks = 1;
                for( size_t i = 0; i < ar.GetDimensionCount(); i++ )
                {
                    nExpectedChunks *= static_cast<size_t>(
                        1+((array_start_idx[i]+count[i]-1)/cs[i])-(array_start_idx[i]/cs[i]));
                }
                ensure_equals( chunkDefs.size(), nExpectedChunks );

                CPLString osChunks;
                for( const auto& chunkDef: chunkDefs )
                {
                    osChunks += CPLSPrintf(
                           "{%u, %u, %u}, {%u, %u, %u}\n",
                           (unsigned)chunkDef.array_start_idx[0],
                           (unsigned)chunkDef.array_start_idx[1],
                           (unsigned)chunkDef.array_start_idx[2],
                           (unsigned)chunkDef.count[0],
                           (unsigned)chunkDef.count[1],
                           (unsigned)chunkDef.count[2]);
                }
                ensure_equals(osChunks,
                                "{1500, 256, 0}, {4, 256, 2000}\n"
                                "{1500, 512, 0}, {4, 256, 2000}\n"
                                "{1504, 256, 0}, {32, 256, 2000}\n"
                                "{1504, 512, 0}, {32, 256, 2000}\n"
                                "{1536, 256, 0}, {32, 256, 2000}\n"
                                "{1536, 512, 0}, {32, 256, 2000}\n"
                                "{1568, 256, 0}, {31, 256, 2000}\n"
                                "{1568, 512, 0}, {31, 256, 2000}\n");
            }
        }

        // Another error case of ProcessPerChunk
        {
            const auto M64 = std::numeric_limits<GUInt64>::max();
            const auto Msize_t = std::numeric_limits<size_t>::max();
            myArray ar(GDT_UInt16, {M64,M64,M64},{32,256,128});

            // Product of myCustomChunkSize[] > Msize_t
            std::vector<GUInt64> array_start_idx{ 0, 0, 0 };
            std::vector<GUInt64> count{ 3000, 1000, 2000 };
            std::vector<size_t> myCustomChunkSize{ Msize_t, Msize_t, Msize_t };
            std::vector<ChunkDef> chunkDefs;
            CPLPushErrorHandler(CPLQuietErrorHandler);
            ensure(!ar.ProcessPerChunk(
                array_start_idx.data(), count.data(),
                myCustomChunkSize.data(),
                TmpStruct::func, &chunkDefs));
            CPLPopErrorHandler();
        }

        {
            const auto BIG = GUInt64(5000) * 1000* 1000;
            myArray ar(GDT_UInt16, {BIG + 3000,BIG + 1000,BIG + 2000},{32,256,128});
            std::vector<GUInt64> array_start_idx{ BIG + 1500, BIG + 256, BIG + 0 };
            std::vector<GUInt64> count{ 99, 512, 2000 };
            std::vector<ChunkDef> chunkDefs;
            auto cs = ar.GetProcessingChunkSize(40*1000*1000);
            ensure(ar.ProcessPerChunk(
                array_start_idx.data(), count.data(), cs.data(),
                TmpStruct::func, &chunkDefs));

            size_t nExpectedChunks = 1;
            for( size_t i = 0; i < ar.GetDimensionCount(); i++ )
            {
                nExpectedChunks *= static_cast<size_t>(
                    1+((array_start_idx[i]+count[i]-1)/cs[i])-(array_start_idx[i]/cs[i]));
            }
            ensure_equals( chunkDefs.size(), nExpectedChunks );

            CPLString osChunks;
            for( const auto& chunkDef: chunkDefs )
            {
                osChunks += CPLSPrintf(
                        "{" CPL_FRMT_GUIB ", " CPL_FRMT_GUIB ", " CPL_FRMT_GUIB "}, {%u, %u, %u}\n",
                        (GUIntBig)chunkDef.array_start_idx[0],
                        (GUIntBig)chunkDef.array_start_idx[1],
                        (GUIntBig)chunkDef.array_start_idx[2],
                        (unsigned)chunkDef.count[0],
                        (unsigned)chunkDef.count[1],
                        (unsigned)chunkDef.count[2]);
            }
            ensure_equals(osChunks,
                            "{5000001500, 5000000256, 5000000000}, {4, 256, 2000}\n"
                            "{5000001500, 5000000512, 5000000000}, {4, 256, 2000}\n"
                            "{5000001504, 5000000256, 5000000000}, {32, 256, 2000}\n"
                            "{5000001504, 5000000512, 5000000000}, {32, 256, 2000}\n"
                            "{5000001536, 5000000256, 5000000000}, {32, 256, 2000}\n"
                            "{5000001536, 5000000512, 5000000000}, {32, 256, 2000}\n"
                            "{5000001568, 5000000256, 5000000000}, {31, 256, 2000}\n"
                            "{5000001568, 5000000512, 5000000000}, {31, 256, 2000}\n");
        }

        {
            // Test with 0 in GetBlockSize()
            myArray ar(GDT_UInt16, {500,1000,2000},{0,0,128});
            {
                auto cs = ar.GetProcessingChunkSize(300*2);
                ensure_equals( cs.size(), 3U );
                ensure_equals( cs[0], 1U );
                ensure_equals( cs[1], 1U );
                ensure_equals( cs[2], 256U );
            }
            {
                auto cs = ar.GetProcessingChunkSize(40*1000*1000);
                ensure_equals( cs.size(), 3U );
                ensure_equals( cs[0], 10U );
                ensure_equals( cs[1], 1000U );
                ensure_equals( cs[2], 2000U );
            }
            {
                auto cs = ar.GetProcessingChunkSize(500U*1000*2000*2);
                ensure_equals( cs.size(), 3U );
                ensure_equals( cs[0], 500U );
                ensure_equals( cs[1], 1000U );
                ensure_equals( cs[2], 2000U );
            }
            {
                auto cs = ar.GetProcessingChunkSize(500U*1000*2000*2 - 1);
                ensure_equals( cs.size(), 3U );
                ensure_equals( cs[0], 499U );
                ensure_equals( cs[1], 1000U );
                ensure_equals( cs[2], 2000U );
            }
        }
        {
            const auto M = std::numeric_limits<GUInt64>::max();
            myArray ar(GDT_UInt16,{M,M,M},{M,M,M/2});
            {
                auto cs = ar.GetProcessingChunkSize(0);
                ensure_equals( cs.size(), 3U );
                ensure_equals( cs[0], 1U );
                ensure_equals( cs[1], 1U );
#if SIZEOF_VOIDP == 8
                ensure_equals( cs[2], static_cast<size_t>(M/2) );
#else
                ensure_equals( cs[2], 1U );
#endif
            }
        }
#if SIZEOF_VOIDP == 8
        {
            const auto M = std::numeric_limits<GUInt64>::max();
            myArray ar(GDT_UInt16,{M,M,M},{M,M,M/4});
            {
                auto cs = ar.GetProcessingChunkSize(std::numeric_limits<size_t>::max());
                ensure_equals( cs.size(), 3U );
                ensure_equals( cs[0], 1U );
                ensure_equals( cs[1], 1U );
                ensure_equals( cs[2], (std::numeric_limits<size_t>::max() / 4) * 2 );
            }
        }
#endif
    }

} // namespace tut
