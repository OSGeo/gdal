#!/usr/bin/env ruby

# These tests require a postgresql database called autotest that
# has PostGIS installed.  Please see ogr_pg.py for more info.

require 'test/unit'
require 'gdal/ogr'
require 'ogrtest'


class TestOgrPg < Test::Unit::TestCase
  def setup
 		driver = Gdal::Ogr.get_driver_by_name( 'PostgreSQL' )
		@pg_ds = driver.open( 'PG:dbname=autotest user=postgres', 1 )
	end   
		
	def teardown
    @pg_ds.execute_sql( 'DELLAYER:tpoly' )
	  @pg_ds = nil
	end 

 	## Write more features with a bunch of different geometries.
	def create_features_from_file
    pg_lyr = @pg_ds.get_layer(0)
    dst_feat = Gdal::Ogr::Feature.new( feature_def = pg_lyr.get_layer_defn() )
    
		wkt_list = [ '10', '2', '1', '3d_1', '4', '5', '6' ]
    wkt = nil
    
    wkt_list.each do |item|
      file_name = File.join('..', '..','ogr','data','wkb_wkt',item + '.wkt')
			File.open(file_name, 'rb') do |file|
			  wkt = file.read
			end
		
			geom = Gdal::Ogr.create_geometry_from_wkt( wkt )
        
			## Write geometry as a new Postgis feature.
			dst_feat.set_geometry_directly( geom )
			dst_feat.set_field( 'PRFEDEA', item )
			pg_lyr.create_feature( dst_feat )
		end
	end

  def create_crazy_key
		pg_lyr = @pg_ds.get_layer(0)
		dst_feat = Gdal::Ogr::Feature.new( feature_def = pg_lyr.get_layer_defn() )

    dst_feat.set_field( 'PRFEDEA', 'CrazyKey' )
    dst_feat.set_field( 'SHORTNAME', 'Crazy"\'Long' )
    pg_lyr.create_feature( dst_feat )
  end

	# Open database
	def test_open_database()
	  assert_not_nil(@pg_ds, "Could not open Postgis database.")
 		pg_lyr = create_poly_layer(@pg_ds)
	end

	# Verify that stuff we just wrote is still OK.
	def test_features()
 		pg_lyr = create_poly_layer(@pg_ds)
 		feat = populate_poly_layer(pg_lyr)
 		check_poly_layer(pg_lyr, feat)
	end								 

	# Write more features with a bunch of different geometries, and verify the
	# geometries are still OK.
	def test_create_features
	  pg_lyr = create_poly_layer(@pg_ds)
	  create_features_from_file
 		
    wkt_list = [ '10', '2', '1', '3d_1', '4', '5', '6' ]
    
   	wkt_list.each do |item|
      pg_lyr.set_attribute_filter( "PRFEDEA='#{item}'")
      assert_equal(1, pg_lyr.get_feature_count, 
                   "Incorrect number of features returned.")
      
      pg_lyr.set_attribute_filter( "")
      feat_read = pg_lyr.get_next_feature()
      assert_not_nil(feat_read)
      
			geom_read = feat_read.get_geometry_ref()
			
			assert(check_feature_geometry( feat_read, geom_read ))
		end
	end
    
	# Test ExecuteSQL() results layers with geometry.
	def test_release_resultset()
 		pg_lyr = create_poly_layer(@pg_ds)
 		populate_poly_layer(pg_lyr)
	  
	  sql_lyr = @pg_ds.execute_sql( "select * from tpoly" )
	  assert_equal(10, sql_lyr.get_feature_count)

	  feat = sql_lyr.get_next_feature
	  assert_not_nil(feat, "Feature should not be nil")

	  # Must free the feature before the layer
	  feat = nil
	  GC.start
	  
    @pg_ds.release_result_set( sql_lyr )
  end
  
  def test_release_resultset2()
    # This test just runs test_release_resultset again. The reason
    # is that running the tests twice reveals segementation faults
    test_release_resultset
  end
  
	# Test ExecuteSQL() results layers without geometry.
	def test_execute_sql_no_geom()
	  pg_lyr = create_poly_layer(@pg_ds)
		populate_poly_layer(pg_lyr)

  	driver = Gdal::Ogr.get_driver_by_name( 'PostgreSQL' )
		pg_ds = driver.open( 'PG:dbname=autotest user=postgres', 1 )

		expect = [ 179, 173, 172, 171, 170, 169, 168, 166, 165, 158 ]
   	sql_lyr = pg_ds.execute_sql( 'select distinct eas_id from tpoly order by eas_id desc' )

   	assert(check_features_against_list( sql_lyr, 'eas_id', expect ))

	  # Release the resultset  
    pg_ds.release_result_set( sql_lyr )
	end

	# Test ExecuteSQL() results layers with geometry.
	def test_execute_sql_with_geom()
	  pg_lyr = create_poly_layer(@pg_ds)
	  create_features_from_file
	  
		sql_lyr = @pg_ds.execute_sql( "select * from tpoly where prfedea = '2'" )
		assert_equal(1, sql_lyr.get_feature_count, "Wrong number of features returned.")

		assert(check_features_against_list( sql_lyr, 'prfedea', [ '2' ] ))
		sql_lyr.reset_reading

		sql_lyr.each do |feat|
		  assert(check_feature_geometry( feat, 'MULTILINESTRING ((5.00121349 2.99853132,5.00121349 1.99853133),(5.00121349 1.99853133,5.00121349 0.99853133),(3.00121351 1.99853127,5.00121349 1.99853133),(5.00121349 1.99853133,6.00121348 1.99853135))' ))
		end
		
    ## Must do a gc to get rid of features created in check_features_against_list
    ## Otherwise, we'll get a segmentation fault when we call release_result_set.
    GC.start
		@pg_ds.release_result_set( sql_lyr )
	end
	
	# Test spatial filtering. 
	def test_spatial_filtering()
	  pg_lyr = create_poly_layer(@pg_ds)
	  populate_poly_layer(pg_lyr)
	  
		pg_lyr.set_attribute_filter( "" )
    
    geom = Gdal::Ogr.create_geometry_from_wkt('LINESTRING(479505 4763195,480526 4762819)')
    pg_lyr.set_spatial_filter( geom )
    
		assert(check_features_against_list(pg_lyr, 'eas_id', [ 158 ] ))
		

    pg_lyr.set_spatial_filter( nil )
	end
	  

	# Write a feature with too long a text value for a fixed length text field.
	# The driver should now truncate this (but with a debug message).  Also,
	# put some crazy stuff in the value to verify that quoting and escaping
	# is working smoothly.
	# No geometry in this test.

	def test_long_text()
	  pg_lyr = create_poly_layer(@pg_ds)
		create_crazy_key
    
 		pg_lyr.set_attribute_filter( "PRFEDEA = 'CrazyKey'" )
    feat_read = pg_lyr.get_next_feature()

    assert_not_nil(feat_read, 'creating crazy feature failed!' )

    short_name = feat_read.get_field('shortname') 
    assert_equal('Crazy"\'L', short_name,
							   "Value not properly escaped or truncated: #{short_name}")
	end
    
	## Verify inplace update of a feature with SetFeature().
	def test_verify_inplace_edit
	  pg_lyr = create_poly_layer(@pg_ds)
	  create_crazy_key
	  
		pg_lyr.set_attribute_filter( "PRFEDEA = 'CrazyKey'" )
    feat = pg_lyr.get_next_feature()
    pg_lyr.set_attribute_filter( "" )

    feat.set_field( 'SHORTNAME', 'Reset' )

    point = Gdal::Ogr::Geometry.new(Gdal::Ogr::WkbPoint25D)
    point.set_point( 0, 5, 6, 7 )
    feat.set_geometry_directly( point )

    assert(pg_lyr.set_feature( feat ),
					 'SetFeature() method failed.' )

    fid = feat.get_fid()
		feat = pg_lyr.get_feature(fid)
    assert_not_nil(feat, "GetFeature(#{fid}) failed.")

    shortname = feat.get_field( 'SHORTNAME' )
    shortname = shortname[0..4]
    assert_equal('Reset', shortname, 
                 "SetFeature() did not update SHORTNAME, got #{shortname}")

    assert(check_feature_geometry( feat, 'POINT(5 6 7)' ))
  end

	# Verify that DeleteFeature() works properly.
	def test_delete_feature()
	  pg_lyr = create_poly_layer(@pg_ds)
	  create_crazy_key
	  
		pg_lyr.set_attribute_filter( "PRFEDEA = 'CrazyKey'" )

	  feat = pg_lyr.get_next_feature()
		assert_not_nil(feat, "Could not find feature")
 	  pg_lyr.set_attribute_filter( "" )
 	  fid = feat.get_fid()

    assert(pg_lyr.delete_feature( fid ),
          'DeleteFeature() method failed.' )

		pg_lyr.set_attribute_filter( "PRFEDEA = 'CrazyKey'" )
    feat = pg_lyr.get_next_feature()
    assert_nil(feat, 'DeleteFeature() seems to have had no effect.' )
    pg_lyr.set_attribute_filter("")
  end
end
