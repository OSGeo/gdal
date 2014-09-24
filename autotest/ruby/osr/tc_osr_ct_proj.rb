#!/usr/bin/env ruby

require 'test/unit'
require 'gdal/osr'

class TestOsrCtProj < Test::Unit::TestCase
  def setup
    @bonne = 'PROJCS["bonne",GEOGCS["GCS_WGS_1984",DATUM["D_WGS_1984",SPHEROID["WGS_1984",6378137.0,298.257223563]],PRIMEM["Greenwich",0.0],UNIT["Degree",0.0174532925199433]],PROJECTION["bonne"],PARAMETER["False_Easting",0.0],PARAMETER["False_Northing",0.0],PARAMETER["Central_Meridian",0.0],PARAMETER["Standard_Parallel_1",60.0],UNIT["Meter",1.0]]'
  end
  
  def do_transform(src_srs, src_xyz, src_error,
                   dst_srs, dst_xyz, dst_error,
                   options)
    
    src = Gdal::Osr::SpatialReference.new()
    result = src.set_from_user_input(src_srs)
    assert(result)

    dst = Gdal::Osr::SpatialReference.new()
    result = dst.set_from_user_input(dst_srs)
    assert(result)
    
    ct = Gdal::Osr::CoordinateTransformation.new(src, dst)
    
    # Tranform source point to destination SRS.
    result = ct.transform_point(src_xyz[0], src_xyz[1], src_xyz[2])

    error = (result[0] - dst_xyz[0]).abs() +
            (result[1] - dst_xyz[1]).abs() +
            (result[2] - dst_xyz[2]).abs()

    assert(error < dst_error)

    # Now transform back.
    ct = Gdal::Osr::CoordinateTransformation.new(dst, src)
    result = ct.transform_point(result[0], result[1], result[2])

    error = (result[0] - src_xyz[0]).abs() +
            (result[1] - src_xyz[1]).abs() +
            (result[2] - src_xyz[2]).abs()

    assert(error < src_error)
  end
  
  def test_transforms()
    # Table of transformations, inputs and expected results (with a threshold)
    #
    # Each entry in the list should have a tuple with:
    #
    # - src_srs: any form that SetFromUserInput() will take.
    # - (src_x, src_y, src_z): location in src_srs.
    # - src_error: threshold for error when srs_x/y is transformed into dst_srs and
    #              then back into srs_src.
    # - dst_srs: destination srs.
    # - (dst_x,dst_y,dst_z): point that src_x/y should transform to.
    # - dst_error: acceptable error threshold for comparing to dst_x/y.
    # - unit_name: the display name for this unit test.
    # - options: eventually we will allow a list of special options here (like one
    #   way transformation).  For now just put nil. 

    transform_list = [
          ['+proj=utm +zone=11 +datum=WGS84', [398285.45, 2654587.59, 0.0], 0.02, 
           'WGS84', [-118.0, 24.0, 0.0], 0.00001, 'UTM_WGS84', nil ],
    
          ['WGS84', [1.0, 65.0, 0.0], 0.00001, @bonne, [47173.75, 557621.30, 0.0],
            0.02, 'Bonne_WGS84', nil]
        ]
    
    transform_list.each do |item|
      self.do_transform(item[0], item[1], item[2], 
                        item[3], item[4], item[5], 
                        item[7])
    end
  end
end
