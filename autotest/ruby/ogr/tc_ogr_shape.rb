#!/usr/bin/env ruby

require 'tmpdir'
require 'gdal/ogr'
require 'test/unit'
require 'ogrtest'

class TestOgrShape < Test::Unit::TestCase
  def setup
  	shape_drv = Gdal::Ogr.get_driver_by_name('ESRI Shapefile')
    @shape_ds = shape_drv.create_data_source(temp_dir)
	end   
	
	def teardown
	  @shape_ds = nil
	  GC.start
		shape_drv = Gdal::Ogr.get_driver_by_name('ESRI Shapefile')
		shape_drv.delete_data_source(temp_dir)
	end 

	def temp_dir
	  Dir.tmpdir
	end

 	def add_geometryless_feature(layer)
		dst_feat = Gdal::Ogr::Feature.new(layer.get_layer_defn)
    dst_feat.set_field( 'PRFEDEA', 'nulled' )
    layer.create_feature(dst_feat)
		layer.reset_reading
  end

	# Open Shapefile 
  def test_open()
    assert_not_nil(@shape_ds)
  end
  
	# Create table from data/poly.shp
	def test_create_table
 		layer = create_poly_layer(@shape_ds)
		poly_feat = populate_poly_layer(layer)
	  check_poly_layer(layer, poly_feat)
  end
      
	def test_geometryless_feature()
    # Create feature without geometry.
		shape_lyr = create_poly_layer(@shape_ds)

	  add_geometryless_feature(shape_lyr)
   
    # Read back the feature and get the geometry.
		shape_lyr.set_attribute_filter( "PRFEDEA = 'nulled'" )
		feat_read = shape_lyr.get_next_feature

		assert_not_nil(feat_read,
		               'Didnt get feature with null geometry back.')

		unless feat_read.get_geometry_ref.nil?
    	print feat_read.get_geometry_ref()
      print feat_read.get_geometry_ref().export_to_wkt
      flunk('Didnt get null geometry as expected.')
		end
  end
    
	# Test ExecuteSQL() results layers without geometry.
	def test_geometryless_sql
 		shape_lyr = create_poly_layer(@shape_ds)
 		feat = populate_poly_layer(shape_lyr)

		begin
 		  # Add geometryless feature
			add_geometryless_feature(shape_lyr)

		  expect = [ 179, 173, 172, 171, 170, 169, 168, 166, 165, 158, 0 ]
      sql_lyr = @shape_ds.execute_sql( 'select distinct eas_id from tpoly order by eas_id desc' )

      tr = check_features_against_list(sql_lyr, 'eas_id', expect)

      # TODO - remove this code once reference counting is updated in OGR library.
      # Free the feature before we release the result set
      ds_feat = nil
  		GC.start
	  end
    
		@shape_ds.release_result_set(sql_lyr)
	end

	def test_sql_layers()
 		shape_lyr = create_poly_layer(@shape_ds)
 		populate_poly_layer(shape_lyr)
 		
    sql_lyr = @shape_ds.execute_sql('select * from tpoly where prfedea = "35043413"' )

    begin
      assert(check_features_against_list( sql_lyr, 'prfedea', [ '35043413' ] ))

		  sql_lyr.reset_reading()
      feat_read = sql_lyr.get_next_feature

      assert(check_feature_geometry( feat_read, 'POLYGON ((479750.688 4764702.000,479658.594 4764670.000,479640.094 4764721.000,479735.906 4764752.000,479750.688 4764702.000))', max_error = 0.001 ))

      # TODO - remove this code once reference counting is updated in OGR library.
      # must free feat_read before releasing the result set
      feat_read = nil
      GC.start
    end
        
 	  @shape_ds.release_result_set( sql_lyr )
  end
    
	# Test spatial filtering. 
	def test_spatial_filtering()
 		shape_lyr = create_poly_layer(@shape_ds)
		feat = populate_poly_layer(shape_lyr)

		add_geometryless_feature(shape_lyr)
		shape_lyr.set_attribute_filter("")
    
    geom = Gdal::Ogr.create_geometry_from_wkt('LINESTRING(479505 4763195,480526 4762819)')
    shape_lyr.set_spatial_filter(geom)

    assert(check_features_against_list(shape_lyr, 'eas_id', [ 158, nil ] ))
		shape_lyr.set_spatial_filter(nil)
	end    

	## Create spatial index, and verify we get the same results.
	def test_spatial_index
 		shape_lyr = create_poly_layer(@shape_ds)
 		feat = populate_poly_layer(shape_lyr)
 		
		add_geometryless_feature(shape_lyr)

		shape_lyr.set_attribute_filter("")
		@shape_ds.execute_sql('CREATE SPATIAL INDEX ON tpoly')

		assert(File.exist?(File.join(temp_dir, 'tpoly.qix')),
				   'tpoly.qix not created' )
    
  	geom = Gdal::Ogr.create_geometry_from_wkt('LINESTRING(479505 4763195,480526 4762819)')
    shape_lyr.set_spatial_filter(geom)

    assert(check_features_against_list(shape_lyr, 'eas_id', [ 158, nil ]))
		shape_lyr.set_spatial_filter(nil)
    @shape_ds.execute_sql( 'DROP SPATIAL INDEX ON tpoly' )

		assert(!File.exist?(File.join(temp_dir, 'tpoly.qix')),
				   'tpoly.qix not deleted')
	end
    
	# Test that we don't return a polygon if we are "inside" but non-overlapping.
	# For now we actually do return this shape, but eventually we won't. 
	def test_inside_non_overlapping()
		shape_ds = Gdal::Ogr.open('../../ogr/data/testpoly.shp')
    shape_lyr = shape_ds.get_layer(0)

    shape_lyr.set_spatial_filter_rect( -10, -130, 10, -110 )
    assert(check_features_against_list( shape_lyr, 'FID',
                                       [ 13 ] ))
	end           

	# Do a fair size query that should pull in a few shapes. 
	def test_query()
		shape_ds = Gdal::Ogr.open( '../../ogr/data/testpoly.shp' )
    shape_lyr = shape_ds.get_layer(0)

    shape_lyr.set_spatial_filter_rect(-400, 22, -120, 400  )
    assert(check_features_against_list( shape_lyr, 'FID',
                                       [ 0, 4, 8 ] ))
  end
    
	# Do a mixed indexed attribute and spatial query.
	def test_mixed_query()
		shape_ds = Gdal::Ogr.open( '../../ogr/data/testpoly.shp' )
    shape_lyr = shape_ds.get_layer(0)
    shape_lyr.set_attribute_filter( 'FID = 5' )
    shape_lyr.set_spatial_filter_rect(-400, 22, -120, 400  )
    
    assert(check_features_against_list( shape_lyr, 'FID',
                                              [] ))

    shape_lyr.set_attribute_filter( 'FID = 4' )
    shape_lyr.set_spatial_filter_rect( -400, 22, -120, 400 )
    
    assert(check_features_against_list( shape_lyr, 'FID',
                                              [ 4 ] ))

    shape_lyr.set_attribute_filter( "" )
    shape_lyr.set_spatial_filter( nil )
  end
end