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
#include "tilematrixset.hpp"

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

        ENSURE_EQUALS(GDALFindDataType(0, false /* signed */, false /* floating */, false /* complex */), GDT_Byte);
        ENSURE_EQUALS(GDALFindDataType(0, true /* signed */, false /* floating */, false /* complex */), GDT_Int16);
        ENSURE_EQUALS(GDALFindDataType(0, false /* signed */, false /* floating */, true /* complex */), GDT_CInt32);
        ENSURE_EQUALS(GDALFindDataType(0, true /* signed */, false /* floating */, true /* complex */), GDT_CInt16);
        ENSURE_EQUALS(GDALFindDataType(0, false /* signed */, true /* floating */, false /* complex */), GDT_Float32);
        ENSURE_EQUALS(GDALFindDataType(0, true /* signed */, true /* floating */, false /* complex */), GDT_Float32);
        ENSURE_EQUALS(GDALFindDataType(0, false /* signed */, true /* floating */, true /* complex */), GDT_CFloat32);
        ENSURE_EQUALS(GDALFindDataType(0, true /* signed */, true /* floating */, true /* complex */), GDT_CFloat32);

        ENSURE_EQUALS(GDALFindDataType(8, false /* signed */, false /* floating */, false /* complex */), GDT_Byte);
        ENSURE_EQUALS(GDALFindDataType(8, true /* signed */, false /* floating */, false /* complex */), GDT_Int16);

        ENSURE_EQUALS(GDALFindDataType(16, false /* signed */, false /* floating */, false /* complex */), GDT_UInt16);
        ENSURE_EQUALS(GDALFindDataType(16, true /* signed */, false /* floating */, false /* complex */), GDT_Int16);

        ENSURE_EQUALS(GDALFindDataType(32, false /* signed */, false /* floating */, false /* complex */), GDT_UInt32);
        ENSURE_EQUALS(GDALFindDataType(32, true /* signed */, false /* floating */, false /* complex */), GDT_Int32);

        ENSURE_EQUALS(GDALFindDataType(64, false /* signed */, true /* floating */, false /* complex */), GDT_Float64);
        ENSURE_EQUALS(GDALFindDataType(64, false /* signed */, true /* floating */, true /* complex */), GDT_CFloat64);
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
                const std::string m_osEmptyFilename{};

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

                const std::string& GetFilename() const override { return m_osEmptyFilename; }

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

    // Test GDALDataset::GetRawBinaryLayout() implementations
    template<> template<> void object::test<19>()
    {
        // ENVI
        {
            GDALDatasetUniquePtr poDS(
                GDALDataset::Open(GDRIVERS_DATA_DIR "envi/envi_rgbsmall_bip.img"));
            ensure( poDS != nullptr );
            GDALDataset::RawBinaryLayout sLayout;
            ensure( poDS->GetRawBinaryLayout(sLayout) );
            ensure_equals( sLayout.osRawFilename, poDS->GetDescription() );
            ensure_equals( static_cast<int>(sLayout.eInterleaving),
                           static_cast<int>(GDALDataset::RawBinaryLayout::Interleaving::BIP) );
            ensure_equals( sLayout.eDataType, GDT_Byte );
            ensure( sLayout.bLittleEndianOrder );
            ensure_equals( sLayout.nImageOffset, 0U );
            ensure_equals( sLayout.nPixelOffset, 3 );
            ensure_equals( sLayout.nLineOffset, 3 * 50 );
            ensure_equals( sLayout.nBandOffset, 1 );
        }
        {
            GDALDatasetUniquePtr poDS(
                GDALDataset::Open(GDRIVERS_DATA_DIR "envi/envi_rgbsmall_bil.img"));
            ensure( poDS != nullptr );
            GDALDataset::RawBinaryLayout sLayout;
            ensure( poDS->GetRawBinaryLayout(sLayout) );
            ensure_equals( sLayout.osRawFilename, poDS->GetDescription() );
            ensure_equals( static_cast<int>(sLayout.eInterleaving),
                           static_cast<int>(GDALDataset::RawBinaryLayout::Interleaving::BIL) );
            ensure_equals( sLayout.eDataType, GDT_Byte );
            ensure( sLayout.bLittleEndianOrder );
            ensure_equals( sLayout.nImageOffset, 0U );
            ensure_equals( sLayout.nPixelOffset, 1 );
            ensure_equals( sLayout.nLineOffset, 3 * 50 );
            ensure_equals( sLayout.nBandOffset, 50 );
        }
        {
            GDALDatasetUniquePtr poDS(
                GDALDataset::Open(GDRIVERS_DATA_DIR "envi/envi_rgbsmall_bsq.img"));
            ensure( poDS != nullptr );
            GDALDataset::RawBinaryLayout sLayout;
            ensure( poDS->GetRawBinaryLayout(sLayout) );
            ensure_equals( sLayout.osRawFilename, poDS->GetDescription() );
            ensure_equals( static_cast<int>(sLayout.eInterleaving),
                           static_cast<int>(GDALDataset::RawBinaryLayout::Interleaving::BSQ) );
            ensure_equals( sLayout.eDataType, GDT_Byte );
            ensure( sLayout.bLittleEndianOrder );
            ensure_equals( sLayout.nImageOffset, 0U );
            ensure_equals( sLayout.nPixelOffset, 1 );
            ensure_equals( sLayout.nLineOffset, 50 );
            ensure_equals( sLayout.nBandOffset, 50 * 49 );
        }

        // GTiff
        {
            GDALDatasetUniquePtr poDS(
                GDALDataset::Open(GCORE_DATA_DIR "byte.tif"));
            ensure( poDS != nullptr );
            GDALDataset::RawBinaryLayout sLayout;
            ensure( poDS->GetRawBinaryLayout(sLayout) );
            ensure_equals( sLayout.osRawFilename, poDS->GetDescription() );
            ensure_equals( static_cast<int>(sLayout.eInterleaving),
                           static_cast<int>(GDALDataset::RawBinaryLayout::Interleaving::UNKNOWN) );
            ensure_equals( sLayout.eDataType, GDT_Byte );
            ensure( sLayout.bLittleEndianOrder );
            ensure_equals( sLayout.nImageOffset, 8U );
            ensure_equals( sLayout.nPixelOffset, 1 );
            ensure_equals( sLayout.nLineOffset, 20 );
            ensure_equals( sLayout.nBandOffset, 0 );
        }
        {
            GDALDatasetUniquePtr poDS(
                GDALDataset::Open(GCORE_DATA_DIR "rgbsmall.tif"));
            ensure( poDS != nullptr );
            GDALDataset::RawBinaryLayout sLayout;
            // Compressed
            ensure( !poDS->GetRawBinaryLayout(sLayout) );
        }
        {
            GDALDatasetUniquePtr poDS(
                GDALDataset::Open(GCORE_DATA_DIR "stefan_full_rgba.tif"));
            ensure( poDS != nullptr );
            GDALDataset::RawBinaryLayout sLayout;
            ensure( poDS->GetRawBinaryLayout(sLayout) );
            ensure_equals( sLayout.osRawFilename, poDS->GetDescription() );
            ensure_equals( static_cast<int>(sLayout.eInterleaving),
                           static_cast<int>(GDALDataset::RawBinaryLayout::Interleaving::BIP) );
            ensure_equals( sLayout.eDataType, GDT_Byte );
            ensure( sLayout.bLittleEndianOrder );
            ensure_equals( sLayout.nImageOffset, 278U );
            ensure_equals( sLayout.nPixelOffset, 4 );
            ensure_equals( sLayout.nLineOffset, 162 * 4 );
            ensure_equals( sLayout.nBandOffset, 1 );
        }
        {
            GDALDatasetUniquePtr poSrcDS(
                GDALDataset::Open(GCORE_DATA_DIR "rgbsmall.tif"));
            ensure( poSrcDS != nullptr );
            auto tmpFilename = "/vsimem/tmp.tif";
            auto poDrv = GDALDriver::FromHandle(GDALGetDriverByName("GTiff"));
            const char* options [] = { "INTERLEAVE=BAND", nullptr };
            auto poDS(GDALDatasetUniquePtr(poDrv->CreateCopy(
                tmpFilename, poSrcDS.get(), false, const_cast<char**>(options), nullptr, nullptr)));
            ensure( poDS != nullptr );
            GDALDataset::RawBinaryLayout sLayout;
            ensure( poDS->GetRawBinaryLayout(sLayout) );
            ensure_equals( sLayout.osRawFilename, poDS->GetDescription() );
            ensure_equals( static_cast<int>(sLayout.eInterleaving),
                           static_cast<int>(GDALDataset::RawBinaryLayout::Interleaving::BSQ) );
            ensure_equals( sLayout.eDataType, GDT_Byte );
            ensure( sLayout.bLittleEndianOrder );
            ensure( sLayout.nImageOffset >= 396U );
            ensure_equals( sLayout.nPixelOffset, 1 );
            ensure_equals( sLayout.nLineOffset, 50 );
            ensure_equals( sLayout.nBandOffset, 50 * 50 );
            poDS.reset();
            VSIUnlink(tmpFilename);
        }
        {
            GDALDatasetUniquePtr poSrcDS(
                GDALDataset::Open(GCORE_DATA_DIR "rgbsmall.tif"));
            ensure( poSrcDS != nullptr );
            auto tmpFilename = "/vsimem/tmp.tif";
            const char* options [] = { "-srcwin", "0", "0", "48", "32",
                                       "-co", "TILED=YES",
                                       "-co", "BLOCKXSIZE=48",
                                       "-co", "BLOCKYSIZE=32", nullptr };
            auto psOptions = GDALTranslateOptionsNew( const_cast<char**>(options), nullptr );
            auto poDS(GDALDatasetUniquePtr(GDALDataset::FromHandle(GDALTranslate(
                tmpFilename, GDALDataset::ToHandle(poSrcDS.get()), psOptions, nullptr))));
            GDALTranslateOptionsFree(psOptions);
            ensure( poDS != nullptr );
            GDALDataset::RawBinaryLayout sLayout;
            ensure( poDS->GetRawBinaryLayout(sLayout) );
            ensure_equals( sLayout.osRawFilename, poDS->GetDescription() );
            ensure_equals( static_cast<int>(sLayout.eInterleaving),
                           static_cast<int>(GDALDataset::RawBinaryLayout::Interleaving::BIP) );
            ensure_equals( sLayout.eDataType, GDT_Byte );
            ensure( sLayout.bLittleEndianOrder );
            ensure( sLayout.nImageOffset >= 390U );
            ensure_equals( sLayout.nPixelOffset, 3 );
            ensure_equals( sLayout.nLineOffset, 48 * 3);
            ensure_equals( sLayout.nBandOffset, 1 );
            poDS.reset();
            VSIUnlink(tmpFilename);
        }
        {
            GDALDatasetUniquePtr poSrcDS(
                GDALDataset::Open(GCORE_DATA_DIR "rgbsmall.tif"));
            ensure( poSrcDS != nullptr );
            auto tmpFilename = "/vsimem/tmp.tif";
            const char* options [] = { "-srcwin", "0", "0", "48", "32",
                                       "-co", "TILED=YES",
                                       "-co", "BLOCKXSIZE=48",
                                       "-co", "BLOCKYSIZE=32",
                                       "-co", "INTERLEAVE=BAND", nullptr };
            auto psOptions = GDALTranslateOptionsNew( const_cast<char**>(options), nullptr );
            auto poDS(GDALDatasetUniquePtr(GDALDataset::FromHandle(GDALTranslate(
                tmpFilename, GDALDataset::ToHandle(poSrcDS.get()), psOptions, nullptr))));
            GDALTranslateOptionsFree(psOptions);
            ensure( poDS != nullptr );
            GDALDataset::RawBinaryLayout sLayout;
            ensure( poDS->GetRawBinaryLayout(sLayout) );
            ensure_equals( sLayout.osRawFilename, poDS->GetDescription() );
            ensure_equals( static_cast<int>(sLayout.eInterleaving),
                           static_cast<int>(GDALDataset::RawBinaryLayout::Interleaving::BSQ) );
            ensure_equals( sLayout.eDataType, GDT_Byte );
            ensure( sLayout.bLittleEndianOrder );
            ensure( sLayout.nImageOffset >= 408U );
            ensure_equals( sLayout.nPixelOffset, 1 );
            ensure_equals( sLayout.nLineOffset, 48);
            ensure_equals( sLayout.nBandOffset, 48 * 32 );
            poDS.reset();
            VSIUnlink(tmpFilename);
        }

        // ISIS3
        {
            GDALDatasetUniquePtr poDS(
                GDALDataset::Open(GDRIVERS_DATA_DIR "isis3/isis3_detached.lbl"));
            ensure( poDS != nullptr );
            GDALDataset::RawBinaryLayout sLayout;
            ensure( poDS->GetRawBinaryLayout(sLayout) );
            ensure( sLayout.osRawFilename.find("isis3_detached.cub") != std::string::npos );
            ensure_equals( static_cast<int>(sLayout.eInterleaving),
                           static_cast<int>(GDALDataset::RawBinaryLayout::Interleaving::UNKNOWN) );
            ensure_equals( sLayout.eDataType, GDT_Byte );
            ensure( sLayout.bLittleEndianOrder );
            ensure_equals( sLayout.nImageOffset, 0U );
            ensure_equals( sLayout.nPixelOffset, 1 );
            ensure_equals( sLayout.nLineOffset, 317 );
            // ensure_equals( sLayout.nBandOffset, 9510 ); // doesn't matter on single band
        }

        // VICAR
        {
            GDALDatasetUniquePtr poDS(
                GDALDataset::Open(GDRIVERS_DATA_DIR "vicar/test_vicar_truncated.bin"));
            ensure( poDS != nullptr );
            GDALDataset::RawBinaryLayout sLayout;
            ensure( poDS->GetRawBinaryLayout(sLayout) );
            ensure_equals( sLayout.osRawFilename, poDS->GetDescription() );
            ensure_equals( static_cast<int>(sLayout.eInterleaving),
                           static_cast<int>(GDALDataset::RawBinaryLayout::Interleaving::UNKNOWN) );
            ensure_equals( sLayout.eDataType, GDT_Byte );
            ensure( sLayout.bLittleEndianOrder );
            ensure_equals( sLayout.nImageOffset, 9680U );
            ensure_equals( sLayout.nPixelOffset, 1 );
            ensure_equals( sLayout.nLineOffset, 400 );
            ensure_equals( sLayout.nBandOffset, 0 ); // doesn't matter on single band
        }

        // FITS
        {
            GDALDatasetUniquePtr poSrcDS(
                GDALDataset::Open(GCORE_DATA_DIR "int16.tif"));
            ensure( poSrcDS != nullptr );
            CPLString tmpFilename(CPLGenerateTempFilename(nullptr));
            tmpFilename += ".fits";
            auto poDrv = GDALDriver::FromHandle(GDALGetDriverByName("FITS"));
            if( poDrv )
            {
                auto poDS(GDALDatasetUniquePtr(poDrv->CreateCopy(
                    tmpFilename, poSrcDS.get(), false, nullptr, nullptr, nullptr)));
                ensure( poDS != nullptr );
                poDS.reset();
                poDS.reset(GDALDataset::Open(tmpFilename));
                ensure( poDS != nullptr );
                GDALDataset::RawBinaryLayout sLayout;
                ensure( poDS->GetRawBinaryLayout(sLayout) );
                ensure_equals( sLayout.osRawFilename, poDS->GetDescription() );
                ensure_equals( static_cast<int>(sLayout.eInterleaving),
                            static_cast<int>(GDALDataset::RawBinaryLayout::Interleaving::UNKNOWN) );
                ensure_equals( sLayout.eDataType, GDT_Int16 );
                ensure( !sLayout.bLittleEndianOrder );
                ensure_equals( sLayout.nImageOffset, 2880U );
                ensure_equals( sLayout.nPixelOffset, 2 );
                ensure_equals( sLayout.nLineOffset, 2 * 20 );
                ensure_equals( sLayout.nBandOffset, 2 * 20 * 20 );
                poDS.reset();
                VSIUnlink(tmpFilename);
            }
        }

        // PDS 3
        {
            GDALDatasetUniquePtr poDS(
                GDALDataset::Open(GDRIVERS_DATA_DIR "pds/mc02_truncated.img"));
            ensure( poDS != nullptr );
            GDALDataset::RawBinaryLayout sLayout;
            ensure( poDS->GetRawBinaryLayout(sLayout) );
            ensure_equals( sLayout.osRawFilename, poDS->GetDescription() );
            ensure_equals( static_cast<int>(sLayout.eInterleaving),
                           static_cast<int>(GDALDataset::RawBinaryLayout::Interleaving::UNKNOWN) );
            ensure_equals( sLayout.eDataType, GDT_Byte );
            ensure( sLayout.bLittleEndianOrder );
            ensure_equals( sLayout.nImageOffset, 3840U );
            ensure_equals( sLayout.nPixelOffset, 1 );
            ensure_equals( sLayout.nLineOffset, 3840 );
            ensure_equals( sLayout.nBandOffset, 0 ); // doesn't matter on single band
        }

        // PDS 4
        {
            GDALDatasetUniquePtr poDS(
                GDALDataset::Open(GDRIVERS_DATA_DIR "pds4/byte_pds4_cart_1700.xml"));
            ensure( poDS != nullptr );
            GDALDataset::RawBinaryLayout sLayout;
            ensure( poDS->GetRawBinaryLayout(sLayout) );
            ensure( sLayout.osRawFilename.find("byte_pds4_cart_1700.img") != std::string::npos );
            ensure_equals( static_cast<int>(sLayout.eInterleaving),
                           static_cast<int>(GDALDataset::RawBinaryLayout::Interleaving::UNKNOWN) );
            ensure_equals( sLayout.eDataType, GDT_Byte );
            ensure( !sLayout.bLittleEndianOrder );
            ensure_equals( sLayout.nImageOffset, 0U );
            ensure_equals( sLayout.nPixelOffset, 1 );
            ensure_equals( sLayout.nLineOffset, 20 );
            ensure_equals( sLayout.nBandOffset, 0 ); // doesn't matter on single band
        }
    }

    // Test TileMatrixSet
    template<> template<> void object::test<20>()
    {
        {
            auto l = gdal::TileMatrixSet::listPredefinedTileMatrixSets();
            ensure( l.find("GoogleMapsCompatible") != l.end() );
            ensure( l.find("NZTM2000") != l.end() );
        }

        {
            CPLPushErrorHandler(CPLQuietErrorHandler);
            ensure( gdal::TileMatrixSet::parse("i_dont_exist") == nullptr );
            CPLPopErrorHandler();
        }

        {
            CPLErrorReset();
            CPLPushErrorHandler(CPLQuietErrorHandler);
            // Invalid JSON
            ensure( gdal::TileMatrixSet::parse("http://127.0.0.1:32767/example.json") == nullptr );
            CPLPopErrorHandler();
            ensure( CPLGetLastErrorType() != 0 );
        }

        {
            CPLPushErrorHandler(CPLQuietErrorHandler);
            // Invalid JSON
            ensure( gdal::TileMatrixSet::parse("{\"type\": \"TileMatrixSetType\" invalid") == nullptr );
            CPLPopErrorHandler();
        }

        {
            CPLPushErrorHandler(CPLQuietErrorHandler);
            // No tileMatrix
            ensure( gdal::TileMatrixSet::parse("{\"type\": \"TileMatrixSetType\" }") == nullptr );
            CPLPopErrorHandler();
        }

        {
            auto poTMS = gdal::TileMatrixSet::parse("LINZAntarticaMapTileGrid");
            ensure( poTMS != nullptr );
            ensure( poTMS->haveAllLevelsSameTopLeft() );
            ensure( poTMS->haveAllLevelsSameTileSize() );
            ensure( poTMS->hasOnlyPowerOfTwoVaryingScales() );
            ensure( !poTMS->hasVariableMatrixWidth() );
        }

        {
            auto poTMS = gdal::TileMatrixSet::parse("NZTM2000");
            ensure( poTMS != nullptr );
            ensure( poTMS->haveAllLevelsSameTopLeft() );
            ensure( poTMS->haveAllLevelsSameTileSize() );
            ensure( !poTMS->hasOnlyPowerOfTwoVaryingScales() );
            ensure( !poTMS->hasVariableMatrixWidth() );
        }

        // Inline JSON with minimal structure
        {
            auto poTMS = gdal::TileMatrixSet::parse("{\"type\": \"TileMatrixSetType\", \"supportedCRS\": \"http://www.opengis.net/def/crs/OGC/1.3/CRS84\", \"tileMatrix\": [{ \"topLeftCorner\": [-180, 90],\"scaleDenominator\":1.0}] }");
            ensure( poTMS != nullptr );
            ensure( poTMS->haveAllLevelsSameTopLeft() );
            ensure( poTMS->haveAllLevelsSameTileSize() );
            ensure( poTMS->hasOnlyPowerOfTwoVaryingScales() );
            ensure( !poTMS->hasVariableMatrixWidth() );
        }

        // Invalid scaleDenominator
        {
            CPLPushErrorHandler(CPLQuietErrorHandler);
            ensure( gdal::TileMatrixSet::parse("{\"type\": \"TileMatrixSetType\", \"supportedCRS\": \"http://www.opengis.net/def/crs/OGC/1.3/CRS84\", \"tileMatrix\": [{ \"topLeftCorner\": [-180, 90],\"scaleDenominator\":0.0}] }") == nullptr);
            CPLPopErrorHandler();
        }

        {
            const char* pszJSON =
            "{"
            "    \"type\": \"TileMatrixSetType\","
            "    \"title\": \"CRS84 for the World\","
            "    \"identifier\": \"WorldCRS84Quad\","
            "    \"abstract\": \"my abstract\","
            "    \"boundingBox\":"
            "    {"
            "        \"type\": \"BoundingBoxType\","
            "        \"crs\": \"http://www.opengis.net/def/crs/OGC/1.X/CRS84\"," // 1.3 modified to 1.X to test difference with supportedCRS
            "        \"lowerCorner\": [-180, -90],"
            "        \"upperCorner\": [180, 90]"
            "    },"
            "    \"supportedCRS\": \"http://www.opengis.net/def/crs/OGC/1.3/CRS84\","
            "    \"wellKnownScaleSet\": \"http://www.opengis.net/def/wkss/OGC/1.0/GoogleCRS84Quad\","
            "    \"tileMatrix\":"
            "    ["
            "        {"
            "            \"type\": \"TileMatrixType\","
            "            \"identifier\": \"0\","
            "            \"scaleDenominator\": 279541132.014358,"
            "            \"topLeftCorner\": [-180, 90],"
            "            \"tileWidth\": 256,"
            "            \"tileHeight\": 256,"
            "            \"matrixWidth\": 2,"
            "            \"matrixHeight\": 1"
            "        },"
            "        {"
            "            \"type\": \"TileMatrixType\","
            "            \"identifier\": \"1\","
            "            \"scaleDenominator\": 139770566.007179,"
            "            \"topLeftCorner\": [-180, 90],"
            "            \"tileWidth\": 256,"
            "            \"tileHeight\": 256,"
            "            \"matrixWidth\": 4,"
            "            \"matrixHeight\": 2"
            "        }"
            "    ]"
            "}";
            VSIFCloseL(VSIFileFromMemBuffer("/vsimem/tmp.json",
                                            reinterpret_cast<GByte*>(const_cast<char*>(pszJSON)),
                                            strlen(pszJSON),
                                            false));
            auto poTMS = gdal::TileMatrixSet::parse("/vsimem/tmp.json");
            VSIUnlink("/vsimem/tmp.json");

            ensure( poTMS != nullptr );
            ensure_equals( poTMS->title(), "CRS84 for the World" );
            ensure_equals( poTMS->identifier(), "WorldCRS84Quad" );
            ensure_equals( poTMS->abstract(), "my abstract" );
            ensure_equals( poTMS->crs(), "http://www.opengis.net/def/crs/OGC/1.3/CRS84" );
            ensure_equals( poTMS->wellKnownScaleSet(), "http://www.opengis.net/def/wkss/OGC/1.0/GoogleCRS84Quad" );
            ensure_equals( poTMS->bbox().mCrs, "http://www.opengis.net/def/crs/OGC/1.X/CRS84" );
            ensure_equals( poTMS->bbox().mLowerCornerX, -180.0 );
            ensure_equals( poTMS->bbox().mLowerCornerY, -90.0 );
            ensure_equals( poTMS->bbox().mUpperCornerX, 180.0 );
            ensure_equals( poTMS->bbox().mUpperCornerY, 90.0 );
            ensure_equals( poTMS->tileMatrixList().size(), 2U );
            ensure( poTMS->haveAllLevelsSameTopLeft() );
            ensure( poTMS->haveAllLevelsSameTileSize() );
            ensure( poTMS->hasOnlyPowerOfTwoVaryingScales() );
            ensure( !poTMS->hasVariableMatrixWidth() );
            const auto &tm = poTMS->tileMatrixList()[0];
            ensure_equals( tm.mId, "0" );
            ensure_equals( tm.mScaleDenominator, 279541132.014358 );
            ensure( fabs(tm.mResX - tm.mScaleDenominator * 0.28e-3 / (6378137. * M_PI / 180)) < 1e-10 );
            ensure( fabs(tm.mResX - 180. / 256) < 1e-10 );
            ensure_equals( tm.mResY, tm.mResX );
            ensure_equals( tm.mTopLeftX, -180.0 );
            ensure_equals( tm.mTopLeftY, 90.0 );
            ensure_equals( tm.mTileWidth, 256 );
            ensure_equals( tm.mTileHeight, 256 );
            ensure_equals( tm.mMatrixWidth, 2 );
            ensure_equals( tm.mMatrixHeight, 1 );
        }

        {
            auto poTMS = gdal::TileMatrixSet::parse(
            "{"
            "    \"type\": \"TileMatrixSetType\","
            "    \"title\": \"CRS84 for the World\","
            "    \"identifier\": \"WorldCRS84Quad\","
            "    \"boundingBox\":"
            "    {"
            "        \"type\": \"BoundingBoxType\","
            "        \"crs\": \"http://www.opengis.net/def/crs/OGC/1.X/CRS84\"," // 1.3 modified to 1.X to test difference with supportedCRS
            "        \"lowerCorner\": [-180, -90],"
            "        \"upperCorner\": [180, 90]"
            "    },"
            "    \"supportedCRS\": \"http://www.opengis.net/def/crs/OGC/1.3/CRS84\","
            "    \"wellKnownScaleSet\": \"http://www.opengis.net/def/wkss/OGC/1.0/GoogleCRS84Quad\","
            "    \"tileMatrix\":"
            "    ["
            "        {"
            "            \"type\": \"TileMatrixType\","
            "            \"identifier\": \"0\","
            "            \"scaleDenominator\": 279541132.014358,"
            "            \"topLeftCorner\": [-180, 90],"
            "            \"tileWidth\": 256,"
            "            \"tileHeight\": 256,"
            "            \"matrixWidth\": 2,"
            "            \"matrixHeight\": 1"
            "        },"
            "        {"
            "            \"type\": \"TileMatrixType\","
            "            \"identifier\": \"1\","
            "            \"scaleDenominator\": 100000000,"
            "            \"topLeftCorner\": [-123, 90],"
            "            \"tileWidth\": 128,"
            "            \"tileHeight\": 256,"
            "            \"matrixWidth\": 4,"
            "            \"matrixHeight\": 2,"
            "            \"variableMatrixWidth\": [{"
            "               \"type\": \"VariableMatrixWidthType\","
            "               \"coalesce\" : 2,"
            "               \"minTileRow\": 0,"
            "               \"maxTileRow\": 1"
            "            }]"
            "        }"
            "    ]"
            "}");
            ensure( poTMS != nullptr );
            ensure_equals( poTMS->tileMatrixList().size(), 2U );
            ensure( !poTMS->haveAllLevelsSameTopLeft() );
            ensure( !poTMS->haveAllLevelsSameTileSize() );
            ensure( !poTMS->hasOnlyPowerOfTwoVaryingScales() );
            ensure( poTMS->hasVariableMatrixWidth() );
            const auto &tm = poTMS->tileMatrixList()[1];
            ensure_equals( tm.mVariableMatrixWidthList.size(), 1U );
            const auto& vmw = tm.mVariableMatrixWidthList[0];
            ensure_equals( vmw.mCoalesce, 2 );
            ensure_equals( vmw.mMinTileRow, 0 );
            ensure_equals( vmw.mMaxTileRow, 1 );
        }

        {
            auto poTMS = gdal::TileMatrixSet::parse(
                "{"
                "    \"identifier\" : \"CDBGlobalGrid\","
                "    \"title\" : \"CDBGlobalGrid\","
                "    \"boundingBox\" : {"
                "        \"crs\" : \"http://www.opengis.net/def/crs/EPSG/0/4326\","
                "        \"lowerCorner\" : ["
                "            -90,"
                "            -180"
                "        ],"
                "        \"upperCorner\" : ["
                "            90,"
                "            180"
                "        ]"
                "    },"
                "    \"supportedCRS\" : \"http://www.opengis.net/def/crs/EPSG/0/4326\","
                "    \"wellKnownScaleSet\" : \"http://www.opengis.net/def/wkss/OGC/1.0/CDBGlobalGrid\","
                "    \"tileMatrices\" : ["
                "        {"
                "            \"identifier\" : \"-10\","
                "            \"scaleDenominator\" : 397569609.975977063179,"
                "            \"matrixWidth\" : 360,"
                "            \"matrixHeight\" : 180,"
                "            \"tileWidth\" : 1,"
                "            \"tileHeight\" : 1,"
                "            \"topLeftCorner\" : ["
                "                90,"
                "                -180"
                "            ],"
                "            \"variableMatrixWidths\" : ["
                "                {"
                "                \"coalesce\" : 12,"
                "                \"minTileRow\" : 0,"
                "                \"maxTileRow\" : 0"
                "                },"
                "                {"
                "                \"coalesce\" : 12,"
                "                \"minTileRow\" : 179,"
                "                \"maxTileRow\" : 179"
                "                }"
                "            ]"
                "        }"
                "    ]"
                "}");
            ensure( poTMS != nullptr );
            ensure_equals( poTMS->tileMatrixList().size(), 1U );
            const auto &tm = poTMS->tileMatrixList()[0];
            ensure_equals( tm.mVariableMatrixWidthList.size(), 2U );
            const auto& vmw = tm.mVariableMatrixWidthList[0];
            ensure_equals( vmw.mCoalesce, 12 );
            ensure_equals( vmw.mMinTileRow, 0 );
            ensure_equals( vmw.mMaxTileRow, 0 );
        }
    }

    // Test that PCIDSK GetMetadataItem() return is stable
    template<> template<> void object::test<21>()
    {
        GDALDatasetUniquePtr poDS(
            GDALDriver::FromHandle(
                GDALGetDriverByName("PCIDSK"))->Create("/vsimem/tmp.pix", 1, 1, 1, GDT_Byte, nullptr));
        ensure( poDS != nullptr );
        poDS->SetMetadataItem("FOO", "BAR");
        poDS->SetMetadataItem("BAR", "BAZ");
        poDS->GetRasterBand(1)->SetMetadataItem("FOO", "BAR");
        poDS->GetRasterBand(1)->SetMetadataItem("BAR", "BAZ");

        {
            const char* psz1 = poDS->GetMetadataItem("FOO");
            const char* psz2 = poDS->GetMetadataItem("BAR");
            const char* pszNull = poDS->GetMetadataItem("I_DONT_EXIST");
            const char* psz3 = poDS->GetMetadataItem("FOO");
            const char* pszNull2 = poDS->GetMetadataItem("I_DONT_EXIST");
            const char* psz4 = poDS->GetMetadataItem("BAR");
            ensure( psz1 != nullptr );
            ensure( psz2 != nullptr );
            ensure( psz3 != nullptr );
            ensure( psz4 != nullptr );
            ensure( pszNull == nullptr );
            ensure( pszNull2 == nullptr );
            ensure_equals( psz1, psz3 );
            ensure( psz1 != psz2 );
            ensure_equals( psz2, psz4 );
            ensure_equals( std::string(psz1), "BAR" );
            ensure_equals( std::string(psz2), "BAZ" );
        }

        {
            auto poBand = poDS->GetRasterBand(1);
            const char* psz1 = poBand->GetMetadataItem("FOO");
            const char* psz2 = poBand->GetMetadataItem("BAR");
            const char* pszNull = poBand->GetMetadataItem("I_DONT_EXIST");
            const char* psz3 = poBand->GetMetadataItem("FOO");
            const char* pszNull2 = poBand->GetMetadataItem("I_DONT_EXIST");
            const char* psz4 = poBand->GetMetadataItem("BAR");
            ensure( psz1 != nullptr );
            ensure( psz2 != nullptr );
            ensure( psz3 != nullptr );
            ensure( psz4 != nullptr );
            ensure( pszNull == nullptr );
            ensure( pszNull2 == nullptr );
            ensure_equals( psz1, psz3 );
            ensure( psz1 != psz2 );
            ensure_equals( psz2, psz4 );
            ensure_equals( std::string(psz1), "BAR" );
            ensure_equals( std::string(psz2), "BAZ" );
        }

        poDS.reset();
        VSIUnlink("/vsimem/tmp.pix");
    }

    // Test GDALBufferHasOnlyNoData()
    template<> template<> void object::test<22>()
    {
        /* bool CPL_DLL GDALBufferHasOnlyNoData(const void* pBuffer,
                                     double dfNoDataValue,
                                     size_t nWidth, size_t nHeight,
                                     size_t nLineStride,
                                     size_t nComponents,
                                     int nBitsPerSample,
                                     GDALBufferSampleFormat nSampleFormat);
         */
        ensure( GDALBufferHasOnlyNoData("\x00", 0.0, 1, 1, 1, 1, 8, GSF_UNSIGNED_INT) );
        ensure( !GDALBufferHasOnlyNoData("\x01", 0.0, 1, 1, 1, 1, 8, GSF_UNSIGNED_INT) );
        ensure( GDALBufferHasOnlyNoData("\x00", 0.0, 1, 1, 1, 1, 1, GSF_UNSIGNED_INT) );
        ensure( GDALBufferHasOnlyNoData("\x00\x00", 0.0, 1, 1, 1, 1, 16, GSF_UNSIGNED_INT) );
        ensure( !GDALBufferHasOnlyNoData("\x00\x01", 0.0, 1, 1, 1, 1, 16, GSF_UNSIGNED_INT) );
        ensure( GDALBufferHasOnlyNoData("\x00\x01", 0.0, 1, 2, 2, 1, 8, GSF_UNSIGNED_INT) );
        ensure( GDALBufferHasOnlyNoData("\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00",
                                        0.0, 14, 1, 14, 1, 8, GSF_UNSIGNED_INT) );
        ensure( !GDALBufferHasOnlyNoData("\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00",
                                         0.0, 14, 1, 14, 1, 8, GSF_UNSIGNED_INT) );
        ensure( !GDALBufferHasOnlyNoData("\x00\x00\x00\x00\x00\x01\x00\x00\x00\x00\x00\x00\x00\x00",
                                         0.0, 14, 1, 14, 1, 8, GSF_UNSIGNED_INT) );
        ensure( !GDALBufferHasOnlyNoData("\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x01",
                                         0.0, 14, 1, 14, 1, 8, GSF_UNSIGNED_INT) );

        uint8_t uint8val = 1;
        ensure( GDALBufferHasOnlyNoData(&uint8val, 1.0, 1, 1, 1, 1, 8, GSF_UNSIGNED_INT) );
        ensure( !GDALBufferHasOnlyNoData(&uint8val, 0.0, 1, 1, 1, 1, 8, GSF_UNSIGNED_INT) );
        ensure( !GDALBufferHasOnlyNoData(&uint8val, 128 + 1, 1, 1, 1, 1, 8, GSF_UNSIGNED_INT) );

        int8_t int8val = -1;
        ensure( GDALBufferHasOnlyNoData(&int8val, -1.0, 1, 1, 1, 1, 8, GSF_SIGNED_INT) );
        ensure( !GDALBufferHasOnlyNoData(&int8val, 0.0, 1, 1, 1, 1, 8, GSF_SIGNED_INT) );
        ensure( !GDALBufferHasOnlyNoData(&int8val, 256, 1, 1, 1, 1, 8, GSF_SIGNED_INT) );

        uint16_t uint16val = 1;
        ensure( GDALBufferHasOnlyNoData(&uint16val, 1.0, 1, 1, 1, 1, 16, GSF_UNSIGNED_INT) );
        ensure( !GDALBufferHasOnlyNoData(&uint16val, 0.0, 1, 1, 1, 1, 16, GSF_UNSIGNED_INT) );
        ensure( !GDALBufferHasOnlyNoData(&uint16val, 65536 + 1, 1, 1, 1, 1, 16, GSF_UNSIGNED_INT) );

        int16_t int16val = -1;
        ensure( GDALBufferHasOnlyNoData(&int16val, -1.0, 1, 1, 1, 1, 16, GSF_SIGNED_INT) );
        ensure( !GDALBufferHasOnlyNoData(&int16val, 0.0, 1, 1, 1, 1, 16, GSF_SIGNED_INT) );
        ensure( !GDALBufferHasOnlyNoData(&int16val, 32768, 1, 1, 1, 1, 16, GSF_SIGNED_INT) );

        uint32_t uint32val = 1;
        ensure( GDALBufferHasOnlyNoData(&uint32val, 1.0, 1, 1, 1, 1, 32, GSF_UNSIGNED_INT) );
        ensure( !GDALBufferHasOnlyNoData(&uint32val, 0.0, 1, 1, 1, 1, 32, GSF_UNSIGNED_INT) );
        ensure( !GDALBufferHasOnlyNoData(&uint32val, static_cast<double>(0x100000000LL + 1),
                                         1, 1, 1, 1, 32, GSF_UNSIGNED_INT) );

        int32_t int32val = -1;
        ensure( GDALBufferHasOnlyNoData(&int32val, -1.0, 1, 1, 1, 1, 32, GSF_SIGNED_INT) );
        ensure( !GDALBufferHasOnlyNoData(&int32val, 0.0, 1, 1, 1, 1, 32, GSF_SIGNED_INT) );
        ensure( !GDALBufferHasOnlyNoData(&int32val, 0x80000000, 1, 1, 1, 1, 32, GSF_SIGNED_INT) );

        float float32val = -1;
        ensure( GDALBufferHasOnlyNoData(&float32val, -1.0, 1, 1, 1, 1, 32, GSF_FLOATING_POINT) );
        ensure( !GDALBufferHasOnlyNoData(&float32val, 0.0, 1, 1, 1, 1, 32, GSF_FLOATING_POINT) );
        ensure( !GDALBufferHasOnlyNoData(&float32val, 1e50, 1, 1, 1, 1, 32, GSF_FLOATING_POINT) );

        float float32nan = std::numeric_limits<float>::quiet_NaN();
        ensure( GDALBufferHasOnlyNoData(&float32nan, float32nan, 1, 1, 1, 1, 32, GSF_FLOATING_POINT) );
        ensure( !GDALBufferHasOnlyNoData(&float32nan, 0.0, 1, 1, 1, 1, 32, GSF_FLOATING_POINT) );

        double float64val = -1;
        ensure( GDALBufferHasOnlyNoData(&float64val, -1.0, 1, 1, 1, 1, 64, GSF_FLOATING_POINT) );
        ensure( !GDALBufferHasOnlyNoData(&float64val, 0.0, 1, 1, 1, 1, 64, GSF_FLOATING_POINT) );

        double float64nan = std::numeric_limits<double>::quiet_NaN();
        ensure( GDALBufferHasOnlyNoData(&float64nan, float64nan, 1, 1, 1, 1, 64, GSF_FLOATING_POINT) );
        ensure( !GDALBufferHasOnlyNoData(&float64nan, 0.0, 1, 1, 1, 1, 64, GSF_FLOATING_POINT) );
    }

    // Test GDALRasterBand::GetIndexColorTranslationTo()
    template<> template<> void object::test<23>()
    {
        GDALDatasetUniquePtr poSrcDS(
            GDALDriver::FromHandle(
                GDALGetDriverByName("MEM"))->Create("", 1, 1, 1, GDT_Byte, nullptr));
        {
            GDALColorTable oCT;
            {
                GDALColorEntry e;
                e.c1 = 0;
                e.c2 = 0;
                e.c3 = 0;
                e.c4 = 255;
                oCT.SetColorEntry(0, &e);
            }
            {
                GDALColorEntry e;
                e.c1 = 1;
                e.c2 = 0;
                e.c3 = 0;
                e.c4 = 255;
                oCT.SetColorEntry(1, &e);
            }
            {
                GDALColorEntry e;
                e.c1 = 255;
                e.c2 = 255;
                e.c3 = 255;
                e.c4 = 255;
                oCT.SetColorEntry(2, &e);
            }
            {
                GDALColorEntry e;
                e.c1 = 125;
                e.c2 = 126;
                e.c3 = 127;
                e.c4 = 0;
                oCT.SetColorEntry(3, &e);
                poSrcDS->GetRasterBand(1)->SetNoDataValue(3);
            }
            poSrcDS->GetRasterBand(1)->SetColorTable(&oCT);
        }

        GDALDatasetUniquePtr poDstDS(
            GDALDriver::FromHandle(
                GDALGetDriverByName("MEM"))->Create("", 1, 1, 1, GDT_Byte, nullptr));
        {
            GDALColorTable oCT;
            {
                GDALColorEntry e;
                e.c1 = 255;
                e.c2 = 255;
                e.c3 = 255;
                e.c4 = 255;
                oCT.SetColorEntry(0, &e);
            }
            {
                GDALColorEntry e;
                e.c1 = 0;
                e.c2 = 0;
                e.c3 = 1;
                e.c4 = 255;
                oCT.SetColorEntry(1, &e);
            }
            {
                GDALColorEntry e;
                e.c1 = 12;
                e.c2 = 13;
                e.c3 = 14;
                e.c4 = 0;
                oCT.SetColorEntry(2, &e);
                poSrcDS->GetRasterBand(1)->SetNoDataValue(2);
            }
            poDstDS->GetRasterBand(1)->SetColorTable(&oCT);
        }

        unsigned char* panTranslationTable = poSrcDS->GetRasterBand(1)->GetIndexColorTranslationTo(poDstDS->GetRasterBand(1));
        ensure_equals(static_cast<int>(panTranslationTable[0]), 1);
        ensure_equals(static_cast<int>(panTranslationTable[1]), 1);
        ensure_equals(static_cast<int>(panTranslationTable[2]), 0);
        ensure_equals(static_cast<int>(panTranslationTable[3]), 2); // special nodata mapping
        CPLFree(panTranslationTable);
    }
} // namespace tut
