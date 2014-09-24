#!/usr/bin/env ruby

require 'test/unit'
require 'gdal/osr'

class TestOsrEsri < Test::Unit::TestCase
  
  # This test verifies that morphToESRI() translates ideosyncratic datum names
  # from "EPSG" form to ESRI from when the exception list comes from the
  # gdal_datum.csv file. 
  def test_morph_to_esri
    srs = Gdal::Osr::SpatialReference.new()
    srs.import_from_epsg(4202)
    assert_equal('Australian_Geodetic_Datum_1966', srs.get_attr_value('DATUM'))
    
    srs.morph_to_esri()
    assert_equal('D_Australian_1966', srs.get_attr_value('DATUM'))

    srs.morph_from_esri()
    assert_equal('Australian_Geodetic_Datum_1966', srs.get_attr_value('DATUM'))
  end
  
  # Verify that exact correct form of UTM names is established when
  # translating certain GEOGCSes to ESRI format.
  def test_utm_names
    srs = Gdal::Osr::SpatialReference.new()
    srs.set_from_user_input('+proj=utm +zone=11 +south +datum=WGS84')
    
    srs.morph_to_esri()
    assert_equal('GCS_WGS_1984', srs.get_attr_value('GEOGCS'))
    assert_equal('WGS_1984_UTM_Zone_11S', srs.get_attr_value('PROJCS'))
  end
  
  # Verify that Unnamed is changed to Unknown in morphToESRI().
  def test_unamed_to_unknown
    srs = Gdal::Osr::SpatialReference.new()
    srs.set_from_user_input('+proj=mill +datum=WGS84')
    
    srs.morph_to_esri()
    assert_equal('Miller_Cylindrical', srs.get_attr_value('PROJCS'))
  end
end
