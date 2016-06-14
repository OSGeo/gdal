#!/usr/bin/env ruby

require 'test/unit'
require 'gdal/gdal'
require 'gdal/osr'

class TestOsrUSGS < Test::Unit::TestCase
  def test_import_from_usgs
    srs = Gdal::Osr::SpatialReference.new()
    srs.import_from_usgs(8, 0,  
           [0.0, 0.0, 
            Gdal::Gdal::dec_to_packed_dms(47.0), Gdal::Gdal::dec_to_packed_dms(62.0), 
            Gdal::Gdal::dec_to_packed_dms(45.0), Gdal::Gdal::dec_to_packed_dms(54.5), 
            0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0
           ],15)

    assert_in_delta(47.0, srs.get_proj_parm(Gdal::Osr::SRS_PP_STANDARD_PARALLEL_1), 0.0000005)
    assert_in_delta(62.0, srs.get_proj_parm(Gdal::Osr::SRS_PP_STANDARD_PARALLEL_2), 0.0000005)
    assert_in_delta(54.5, srs.get_proj_parm(Gdal::Osr::SRS_PP_LATITUDE_OF_CENTER), 0.0000005)
    assert_in_delta(45.0, srs.get_proj_parm(Gdal::Osr::SRS_PP_LONGITUDE_OF_CENTER), 0.0000005)
    assert_in_delta(0.0, srs.get_proj_parm(Gdal::Osr::SRS_PP_FALSE_EASTING), 0.0000005)
    assert_in_delta(0.0, srs.get_proj_parm(Gdal::Osr::SRS_PP_FALSE_NORTHING), 0.0000005)
  end
  
  def test_export_to_usgs
    srs = Gdal::Osr::SpatialReference.new()
    srs.import_from_wkt('PROJCS["unnamed",GEOGCS["NAD27",
        DATUM["North_American_Datum_1927",
        SPHEROID["Clarke 1866",6378206.4,294.9786982139006,
        AUTHORITY["EPSG","7008"]],AUTHORITY["EPSG","6267"]],
        PRIMEM["Greenwich",0],UNIT["degree",0.0174532925199433],
        AUTHORITY["EPSG","4267"]],PROJECTION["Lambert_Conformal_Conic_2SP"],
        PARAMETER["standard_parallel_1",33.90363402777778],
        PARAMETER["standard_parallel_2",33.62529002777778],
        PARAMETER["latitude_of_origin",33.76446202777777],
        PARAMETER["central_meridian",-117.4745428888889],
        PARAMETER["false_easting",0],PARAMETER["false_northing",0],
        UNIT["metre",1,AUTHORITY["EPSG","9001"]]]')

    (proj_code, zone, parms, datum_code) = srs.export_to_usgs()

    assert_equal(4, proj_code)
    assert_equal(0, zone)
    assert_equal(0, datum_code)

    assert_in_delta(Gdal::Gdal::packed_dms_to_dec(parms[2]),  33.90363403, 0.0000005)
    assert_in_delta(Gdal::Gdal::packed_dms_to_dec(parms[3]), 33.62529003, 0.0000005)
    assert_in_delta(Gdal::Gdal::packed_dms_to_dec(parms[4]), -117.4745429, 0.0000005)
    assert_in_delta(Gdal::Gdal::packed_dms_to_dec(parms[5]), 33.76446203, 0.0000005)
  end
end
