/******************************************************************************
 *
 * Project:  C++ Test Suite for GDAL/OGR
 * Purpose:  Test geometry stealing from OGR feature
 * Author:   mathieu17g (https://github.com/mathieu17g/)
 *
 *****************************************************************************/

#include "gdal_unit_test.h"
#include "gdal.h"
#include "ogr_api.h"

namespace tut
{

    // Test data
    struct test_ogr_geometry_stealing_data
    {
        GDALDatasetH hDS;
        OGRLayerH hLayer;

        test_ogr_geometry_stealing_data()
        {
            // Build data file path name
            std::string test_data_file_name(tut::common::data_basedir);
            test_data_file_name += SEP;
            test_data_file_name += "multi_geom.csv";
            
            // Open data file with multi geometries feature layer
            const char *const papszOpenOptions[] = {"AUTODETECT_TYPE=YES", "GEOM_POSSIBLE_NAMES=point,linestring", "KEEP_GEOM_COLUMNS=NO", NULL};
            hDS = GDALOpenEx(test_data_file_name.c_str(), GDAL_OF_VECTOR, NULL, papszOpenOptions, NULL);
            if (hDS == NULL)
            {
                printf("Can't open layer file %s.\n", test_data_file_name.c_str());
                exit(1);
            }
            hLayer = GDALDatasetGetLayer(hDS, 0);
            if (hLayer == NULL)
            {
                printf("Can't get layer in file %s.\n", test_data_file_name.c_str());
                exit(1);
            }
        }

        ~test_ogr_geometry_stealing_data()
        {
            GDALClose(hDS);
        }
    };

    // Register test group
    typedef test_group<test_ogr_geometry_stealing_data> group;
    typedef group::object object;
    group test_ogr_geometry_stealing_group("OGR::GeomStealing");

    // Test 1st geometry stealing from a multigeom csv file
    template <>
    template <>
    void object::test<1>()
    {
        set_test_name("Steal 1st geometry");
        OGRFeatureH hFeature = OGR_L_GetNextFeature(hLayer);
        OGRGeometryH hGeometryOrig = OGR_G_Clone(OGR_F_GetGeometryRef(hFeature));
        OGRGeometryH hGeometryStolen = OGR_F_StealGeometry(hFeature);
        ensure_equal_geometries(hGeometryOrig, hGeometryStolen, 0.000000001);
        OGR_G_DestroyGeometry(hGeometryOrig);
        OGR_G_DestroyGeometry(hGeometryStolen);
        OGR_F_Destroy(hFeature);
    }

    // Test 2nd geometry stealing from a multigeom csv file
    template <>
    template <>
    void object::test<2>()
    {
        set_test_name("Steal 2nd geometry");
        OGRFeatureH hFeature = OGR_L_GetNextFeature(hLayer);
        OGRGeometryH hGeometryOrig = OGR_G_Clone(OGR_F_GetGeomFieldRef(hFeature, 1));
        OGRGeometryH hGeometryStolen = OGR_F_StealGeometryEx(hFeature, 1);
        ensure_equal_geometries(hGeometryOrig, hGeometryStolen, 0.000000001);
        OGR_G_DestroyGeometry(hGeometryOrig);
        OGR_G_DestroyGeometry(hGeometryStolen);
        OGR_F_Destroy(hFeature);
    }

} // namespace tut