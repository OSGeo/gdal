#!/usr/bin/env ruby

require 'gdal/ogr'
require 'test/unit'

class TestOgrWkbWktGeom < Test::Unit::TestCase
  @@current_dir = File.expand_path(File.dirname(__FILE__)) 
  
  def path
    File.join(@@current_dir, '../../ogr/data/wkb_wkt')
  end
  
  def compare_files(id)
    raw_wkb = File.open(File.join(path, "#{id}.wkb"), 'rb').read
    raw_wkt = File.open(File.join(path, "#{id}.wkt"), 'r').read
    
    ## Compare the WKT derived from the WKB file to the WKT provided
    ## but reformatted (normalized).

    geom_wkb = Gdal::Ogr.create_geometry_from_wkb( raw_wkb )
    wkb_wkt = geom_wkb.export_to_wkt()

    geom_wkt = Gdal::Ogr.create_geometry_from_wkt( raw_wkt )
    normal_wkt = geom_wkt.export_to_wkt()

    assert_equal(wkb_wkt, normal_wkt, "Failure on comparing files #{id}.wkb and #{id}.wkt")

    ## Verify that the geometries appear to be the same.   This is
    ## intended to catch problems with the encoding too WKT that might
    ## cause passes above but that are mistaken.
    
    assert_equal(geom_wkb.get_coordinate_dimension(), geom_wkt.get_coordinate_dimension())
    assert_equal(geom_wkb.get_geometry_type(), geom_wkt.get_geometry_type())
    assert_equal(geom_wkb.get_geometry_name(), geom_wkt.get_geometry_name())

    ## Convert geometry to WKB and back to verify that WKB encoding is
    ## working smoothly.

    wkb_xdr = geom_wkt.export_to_wkb( Gdal::Ogr::WKBXDR )
    geom_wkb =Gdal:: Ogr.create_geometry_from_wkb( wkb_xdr )

    assert(geom_wkb.equal( geom_wkt ))

    wkb_ndr = geom_wkt.export_to_wkb( Gdal::Ogr::WKBNDR )
    geom_wkb = Gdal::Ogr.create_geometry_from_wkb( wkb_ndr )

    assert(geom_wkb.equal( geom_wkt ))
  end
        
  def test_files()
    ## When imported build a list of units based on the files available.
    Dir.chdir(path)
    Dir.glob('*.wkb') do |filename|
      id = filename[0..-5]
      compare_files(id)
    end
  end
end
