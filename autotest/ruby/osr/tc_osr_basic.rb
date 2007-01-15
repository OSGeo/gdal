#!/usr/bin/env ruby

require 'test/unit'
require 'gdal/osr'

class TestOsrBasic < Test::Unit::TestCase
  def test_basic
    utm_srs = Gdal::Osr::SpatialReference.new()
    utm_srs.set_utm(11)
    utm_srs.set_well_known_geog_cs('WGS84')

    parm_list = [ 
                 [Gdal::Osr::SRS_PP_CENTRAL_MERIDIAN, -117.0],
                 [Gdal::Osr::SRS_PP_LATITUDE_OF_ORIGIN, 0.0],
                 [Gdal::Osr::SRS_PP_SCALE_FACTOR, 0.9996],
                 [Gdal::Osr::SRS_PP_FALSE_EASTING, 500000.0],
                 [Gdal::Osr::SRS_PP_FALSE_NORTHING, 0.0]
                ]

    parm_list.each() do |parm|
      value = utm_srs.get_proj_parm(parm[0], -1111)
      assert_in_delta(value, parm[1], 0.0000000000001)
    end
   
    auth_list = [ ['GEOGCS', '4326'], ['DATUM', '6326'] ]

    auth_list.each() do |auth|
      assert_equal(utm_srs.get_authority_name(auth[0]), 'EPSG')
      assert_equal(utm_srs.get_authority_code(auth[0]), auth[1])
    end
  end
  
  # Test simple default NAD83 State Plane zone.
  def test_nad83_state_plane()
    srs = Gdal::Osr::SpatialReference.new()
    srs.set_state_plane(403, 1)  # California III NAD83.

    parm_list = [
                 [Gdal::Osr::SRS_PP_STANDARD_PARALLEL_1, 38.43333333333333],
                 [Gdal::Osr::SRS_PP_STANDARD_PARALLEL_2, 37.06666666666667],
                 [Gdal::Osr::SRS_PP_LATITUDE_OF_ORIGIN, 36.5],
                 [Gdal::Osr::SRS_PP_CENTRAL_MERIDIAN, -120.5],
                 [Gdal::Osr::SRS_PP_FALSE_EASTING, 2000000.0],
                 [Gdal::Osr::SRS_PP_FALSE_NORTHING, 500000.0]
                ]

    parm_list.each() do |parm|
      value = srs.get_proj_parm(parm[0], -1111)
      assert_in_delta(parm[1], value, 0.0000001)
    end
    
    auth_list = [ ['GEOGCS', '4269'],
                  ['DATUM', '6269'],
                  ['PROJCS', '26943'],
                  ['PROJCS|UNIT', '9001']
                ]

    auth_list.each() do |auth|
      assert_equal(srs.get_authority_name(auth[0]), 'EPSG')
      assert_equal(srs.get_authority_code(auth[0]), auth[1])
    end
  end

  # Test simple default NAD83 State Plane zone.
  def test_nad83_state_plane_feet()
    srs = Gdal::Osr::SpatialReference.new()
    # California III NAD83 feet
    srs.set_state_plane(403, 1, 'Foot', 0.3048006096012192)

    parm_list = [
                 [Gdal::Osr::SRS_PP_STANDARD_PARALLEL_1, 38.43333333333333],
                 [Gdal::Osr::SRS_PP_STANDARD_PARALLEL_2, 37.06666666666667],
                 [Gdal::Osr::SRS_PP_LATITUDE_OF_ORIGIN, 36.5],
                 [Gdal::Osr::SRS_PP_CENTRAL_MERIDIAN, -120.5],
                 [Gdal::Osr::SRS_PP_FALSE_EASTING, 6561666.666666667],
                 [Gdal::Osr::SRS_PP_FALSE_NORTHING, 1640416.666666667]
                ]

    parm_list.each() do |parm|
      value = srs.get_proj_parm(parm[0], -1111)
      assert_in_delta(parm[1], value, 0.0000001)
    end
    
    auth_list = [ ['GEOGCS', '4269'],
                  ['DATUM', '6269']
                ]

    auth_list.each() do |auth|
      assert_equal(srs.get_authority_name(auth[0]), 'EPSG')
      assert_equal(srs.get_authority_code(auth[0]), auth[1])
    end

    assert_nil(srs.get_authority_name('PROJCS'))
    assert_not_equal(srs.get_authority_code('PROJCS|UNIT'), '9001',
                     'Got METER authority code on linear units.')

    assert_equal(srs.get_linear_units_name(), 'Foot')
  end

  # Translate a coordinate system with nad shift into to PROJ.4 and back
  # and verify that the TOWGS84 parameters are preserved.
  def test_translate_nad_shift()
    srs = Gdal::Osr::SpatialReference.new()
    srs.set_gs(-117.0, 100000.0, 100000)
    srs.set_geog_cs('Test GCS', 'Test Datum', 'WGS84', 
                    Gdal::Osr::SRS_WGS84_SEMIMAJOR, 
                    Gdal::Osr::SRS_WGS84_INVFLATTENING)
    
    srs.set_towgs84(1, 2, 3)

    assert_equal(srs.get_towgs84(), [1,2,3,0,0,0,0])
    proj4 = srs.export_to_proj4()
    
    srs2 = Gdal::Osr::SpatialReference.new()
    srs2.import_from_proj4(proj4)

    assert_equal(srs2.get_towgs84(), [1,2,3,0,0,0,0])
  end
end
