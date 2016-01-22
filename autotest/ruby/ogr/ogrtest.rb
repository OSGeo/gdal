require 'test/unit'
require 'gdal/ogr'

RUBY_OGR_DIRECTORY = File.expand_path(File.dirname(__FILE__)) 

def data_directory
  return File.join(RUBY_OGR_DIRECTORY, "..", "..", "ogr", "data")
end

# ------ Helper Methods to create, populate and test a poly layer ----
def create_poly_layer(ds)
  layer = ds.create_layer('tpoly')

  # Setup Schema
  quick_create_layer_def( layer,
                          [ ['AREA', Gdal::Ogr::OFTREAL],
                            ['EAS_ID', Gdal::Ogr::OFTINTEGER],
                            ['PRFEDEA', Gdal::Ogr::OFTSTRING],
                            ['SHORTNAME', Gdal::Ogr::OFTSTRING, 8] ] )
  return layer
end

def populate_poly_layer(layer)
  file_name = File.join(data_directory, '../../ogr/data/poly.shp')
  poly_ds = Gdal::Ogr.open(file_name)
  poly_lyr = poly_ds.get_layer(0)

  # Create a feature to hold data
  dst_feat = Gdal::Ogr::Feature.new(feature_def = layer.get_layer_defn())

  # Copy features over    
  poly_feat = []

  poly_lyr.each do |feat|
    poly_feat.push( feat )
    dst_feat.set_from( feat )
    layer.create_feature(dst_feat)
  end

  poly_lyr = nil
  poly_ds = nil

  return poly_feat
end

def check_poly_layer(layer, poly_feat)
  # Verify that stuff we just wrote is still OK.
  expect = [168, 169, 166, 158, 165]
    
  layer.set_attribute_filter( 'eas_id < 170' )
  assert_equal(expect.size, layer.get_feature_count)
  layer.reset_reading()
    
  tr = check_features_against_list(layer,
                                   'eas_id', expect )

  layer.set_attribute_filter("")

  poly_feat.each do |orig_feat|
    read_feat = layer.get_next_feature

    assert(check_feature_geometry(read_feat, orig_feat.get_geometry_ref()),
                                  max_error = 0.000000001 )

    0.upto(2) do |i|
      assert_equal(orig_feat.get_field(i),
                   read_feat.get_field(i),
                   "Attribute #{i} does not match")
    end        
  end
end

def check_features_against_list( layer, field_name, value_list )

  # Get the index for the specified field name
  field_index = layer.get_layer_defn().get_field_index( field_name )

  assert(field_index >= 0, 'Did not find required field ' + field_name )

  i = 0
  value_list.each do |expected_value|
    # Get a feature
    feat = layer.get_next_feature()

    assert_not_nil(feat, "Got only #{i} features, not the expected #{value_list.size} features.")

    # Get the value for the field
    actual_value = feat.get_field(field_index)
    
    assert_equal(expected_value, actual_value,
                 "Field #{field_name} feature #{i} did not match expected value #{expected_value}, got #{actual_value}")
      
    i += 1
  end

  feat = layer.get_next_feature()
  assert_nil(feat, "Got more features than expected")
  return true
end

def check_feature_geometry(feat, geom, max_error = 0.0001)
  begin
    f_geom = feat.get_geometry_ref
  rescue
    f_geom = feat
  end

  if geom.kind_of?(String)
    geom = Gdal::Ogr.create_geometry_from_wkt(geom)
  else
    geom = geom.clone()
  end
  
  if not f_geom.nil? and geom.nil?
    flunk('expected NULL geometry but got one.')
  elsif f_geom.nil? and not geom.nil?
    flunk('expected geometry but got NULL.')
  end

  assert_equal(f_geom.get_geometry_name,
               geom.get_geometry_name,
               'geometry names do not match')
  
  assert_equal(f_geom.get_geometry_count,
               geom.get_geometry_count,
               'sub-geometry counts do not match')

  assert_equal(f_geom.get_point_count,
               geom.get_point_count,
               'point counts do not match')

  if f_geom.get_geometry_count > 0
    count = f_geom.get_geometry_count
    0.upto(count-1) do |i|
      assert(check_feature_geometry(f_geom.get_geometry_ref(i),
                                    geom.get_geometry_ref(i),
                                    max_error ))
    end
  else
    #count = f_geom.get_point_count

    #0.upto(count-1) do |i|
      # x_dist = (f_geom.get_x(i) - geom.get_x(i)).abs
      #y_dist = (f_geom.get_y(i) - geom.get_y(i)).abs
      #z_dist = (f_geom.get_z(i) - geom.get_z(i)).abs

      #max = [x_dist, y_dist, z_dist].max
      #if max > max_error
        #fail("Error in vertex #{i}, off by #{max}.")
      #end
    #end
  end

  return true
end


def quick_create_layer_def( lyr, field_list)
  ## Each field is a tuple of (name, type, width, precision)
  ## Any of type, width and precision can be skipped.  Default type is string.

  field_list.each do |field_array|
    name = field_array[0]
      
    if field_array.size > 1
      type = field_array[1]
    else
      type = Gdal::Ogr.OFTString
    end
      
    field_defn = Gdal::Ogr::FieldDefn.new( name, type )
        
    field_defn.set_width(field_array[2].to_int) if field_array.size > 2
    field_defn.set_precision(field_array[3].to_int) if field_array.size > 3

    lyr.create_field( field_defn )
  end
end

#def quick_create_feature( layer, field_values, wkt_geometry ):
    #feature = ogr.Feature( feature_def = layer.GetLayerDefn() )

    #for i in range(len(field_values)):
        #feature.SetField( i, field_values[i] )

    #if wkt_geometry is not None:
        #geom = ogr.CreateGeometryFromWkt( wkt_geometry )
        #if geom is None:
            #raise ValueError, 'Failed to create geometry from: ' + wkt_geometry
        #feature.SetGeometryDirectly( geom )

    #result = layer.CreateFeature( feature )

    #feature.Destroy()
    
    #if result != 0:
    #raise ValueError, 'CreateFeature() failed in ogrtest.quick_create_feature()'
    