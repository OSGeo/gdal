#!/usr/bin/env ruby

require 'test/unit'
require 'gdal/ogr'
require 'ogrtest'

class TestOgrRefCount < Test::Unit::TestCase
  def setup
  end

  def teardown
    GC.start
  end
  
	def test_open_datasets()
		assert_equal(Gdal::Ogr.get_open_dscount, 0, 
					       'Initial Open DS count is not zero!' )

    ds_1 = Gdal::Ogr.open_shared(File.join(data_directory, 'idlink.dbf'))
    ds_2 = Gdal::Ogr.open_shared(File.join(data_directory, 'poly.shp'))

		assert_equal(2, Gdal::Ogr.get_open_dscount, 
		             'Open DS count not 2 after shared opens.' )

		assert_equal(1, ds_1.get_ref_count,
		             'Reference count not 1 on ds_1')
		
		assert_equal(1, ds_2.get_ref_count,
								'Reference count not 1 on ds_2')
	end

	#def test_reopen_datasets
    #ds_3 = Ogr.open_shared( '../ogr/data/idlink.dbf' )

		#assert_equal(2, Ogr.get_open_dscount, 
		             #'Open DS count not 2 after shared opens.' )
		
		#assert_equal(2, ds_3.get_ref_count,
		             #'Reference count not 2 on ds_3')

	#end

	### Verify that releasing the datasources has the expected behaviour.
	#def test_release_datasource
		#assert_equal(0, Ogr.get_open_dscount,
		             #'All datasets should be released.')
	#end

################################################################################
## Verify that we can walk the open datasource list.

#def ogr_refcount_4():

    #ds = ogr.GetOpenDS( 0 )
    #if ds._o != gdaltest.ds_2._o:
        #gdaltest.post_reason( 'failed to fetch expected datasource' )
        #return 'failed'

    #return 'success'

end