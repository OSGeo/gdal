#!/usr/bin/env ruby

require 'test/unit'
require 'gdal/osr'

class TestOsrProj4 < Test::Unit::TestCase
  def test_k_0_flag()
    srs = Gdal::Osr::SpatialReference.new()
    srs.import_from_proj4('+proj=tmerc +lat_0=53.5000000000 +lon_0=-8.0000000000 +k_0=1.0000350000 +x_0=200000.0000000000 +y_0=250000.0000000000 +a=6377340.189000 +rf=299.324965 +towgs84=482.530,-130.596,564.557,-1.042,-0.214,-0.631,8.15')
    assert_in_delta(srs.get_proj_parm(Gdal::Osr::SRS_PP_SCALE_FACTOR).abs(), -1.000035.abs(), 0.0000005)
  end

    
  # Verify that we can import strings with parameter values that are exponents
  # and contain a plus sign.  As per bug 355 in GDAL/OGR's bugzilla. 
  def test_exponents()
    srs = Gdal::Osr::SpatialReference.new()
    srs.import_from_proj4("+proj=lcc +x_0=0.6096012192024384e+06 +y_0=0 +lon_0=90dw +lat_0=42dn +lat_1=44d4'n +lat_2=42d44'n +a=6378206.400000 +rf=294.978698 +nadgrids=conus,ntv1_can.dat")
    assert_in_delta(srs.get_proj_parm(Gdal::Osr::SRS_PP_FALSE_EASTING), -609601.219.abs(), 0.0005)
  end
end