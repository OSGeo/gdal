require 'test/unit'
require 'tmpdir'
require 'gdal/ogr'
require 'ogrtest'

class TestOgrCsv < Test::Unit::TestCase
  def setup
    @csv_ds = Gdal::Ogr.open(File.join(data_directory, 'prime_meridian.csv'))
  end   
  
  def teardown
    begin
      @csv_ds = nil
      GC.start
    end
    
    delete_temp_ds
  end 

  def temp_dir
    File.join(Dir.tmpdir, 'csvwrk')
  end

  def create_temp_ds(layer_name, options = nil)
    csv_driver =  Gdal::Ogr.get_driver_by_name('CSV')
    csv_tmpds = csv_driver.create_data_source(temp_dir)

    # Create layer (.csv file)
    if not options.nil?  
      csv_lyr = csv_tmpds.create_layer(layer_name, # name
                                        nil,        # srs
                                        Gdal::Ogr::WkbUNKNOWN, # geom type
                                        options)    # options
    else
      csv_lyr = csv_tmpds.create_layer(layer_name)
    end        

    # Setup Schema
    quick_create_layer_def(csv_lyr,
                           [ ['PRIME_MERIDIAN_CODE', Gdal::Ogr::OFTINTEGER],
                             ['INFORMATION_SOURCE', Gdal::Ogr::OFTSTRING] ] )

    # Copy in matching prime meridian fields.
    dst_feat = Gdal::Ogr::Feature.new( feature_def = csv_lyr.get_layer_defn)

    srclyr = @csv_ds.get_layer( 'prime_meridian' )

    srclyr.each do |feat|
      dst_feat.set_from( feat )
      csv_lyr.create_feature( dst_feat )
    end

    return csv_tmpds
  end
  
  def delete_temp_ds
    if File.exist?(temp_dir)
      # Using GDAL doesn't seem to work on Windows
      Gdal::Ogr.get_driver_by_name('CSV').delete_data_source(temp_dir)
    end
  end
  
  def test_open()
    assert_not_nil(@csv_ds, "Could not open csv file")    
  end

  def test_verify_attributes()
    return
    lyr = @csv_ds.get_layer('prime_meridian')

    expect = ['8901', '8902', '8903', '8904']
    
    assert(check_features_against_list( lyr,'PRIME_MERIDIAN_CODE',expect))

    lyr.reset_reading()

    expect = [ '', 'Instituto Geografico e Cadastral; Lisbon',
               'Institut Geographique National (IGN), Paris',
               'Instituto Geografico "Augustin Cadazzi" (IGAC); Bogota' ]
    
    assert(check_features_against_list( lyr,'INFORMATION_SOURCE',expect))

    lyr.reset_reading()
  end
  

  # Verify the some attributes read properly.
  #
  # NOTE: one weird thing is that in this pass the prime_meridian_code field
  # is typed as integer instead of string since it is created literally.
  def test_attributes()
    return
    ds = create_temp_ds('pm1')
    lyr = ds.get_layer(0)

    expect = [8901, 8902, 8903, 8904 ]
    
    assert(check_features_against_list( lyr,'PRIME_MERIDIAN_CODE',expect))
    
    lyr.reset_reading()

    expect = [ '', 'Instituto Geografico e Cadastral; Lisbon',
               'Institut Geographique National (IGN), Paris',
               'Instituto Geografico "Augustin Cadazzi" (IGAC); Bogota' ]
    
    assert(check_features_against_list( lyr,'INFORMATION_SOURCE',expect))

    # Let go of the temp dataset
    lyr = nil
    ds = nil
  end

  def test_attributes2()
    return
    options = ['LINEFORMAT=CRLF',]
    ds = create_temp_ds('pm2', options)
    lyr = ds.get_layer(0)

    expect = [8901, 8902, 8903, 8904 ]
    
    assert(check_features_against_list( lyr,'PRIME_MERIDIAN_CODE',expect))
    
    lyr.reset_reading()

    expect = [ '', 'Instituto Geografico e Cadastral; Lisbon',
               'Institut Geographique National (IGN), Paris',
               'Instituto Geografico "Augustin Cadazzi" (IGAC); Bogota' ]
    
    assert(check_features_against_list( lyr,'INFORMATION_SOURCE',expect))
    
    # Let go of the temp dataset
    lyr = nil
    ds = nil
  end      

  def test_delete_layer
    ds = create_temp_ds('pm1')
    lyr = ds.get_layer(0)
    assert_equal('pm1', lyr.get_name)

    # Let go of the temp dataset
    lyr = nil

    assert(ds.delete_layer(0), 'Could not delete layer.')

    assert_equal(ds.get_layer_count, 0,
                 'Layer not destroyed properly?')
    
    lyr = nil
    ds = nil
  end
  

###############################################################################
# Verify the some attributes read properly.
#


  # Verify some capabilities and related stuff.
  #
  def test_capabilities()
    lyr = @csv_ds.get_layer( 'prime_meridian' )

    puts "cccc"
    puts lyr.test_capability( 'SequentialWrite')
    
    assert_equal(false, lyr.test_capability( 'SequentialWrite'),
                 'should not have write access to readonly layer')

    assert_equal(false, lyr.test_capability( 'RandomRead'),
                 'CSV files dont efficiently support random reading.')

    assert_equal(false, lyr.test_capability( 'FastGetExtent'),
                 'CSV files do not support getextent' )

    assert_equal(false, lyr.test_capability( 'FastFeatureCount'),
                 'CSV files do not support fast feature count')

    driver = Gdal::Ogr.get_driver_by_name('CSV')
        
    assert_equal(false, driver.test_capability( 'DeleteDataSource'),
                 'CSV files do support DeleteDataSource' )

    assert_equal(false, driver.test_capability( 'CreateDataSource'),
                 'CSV files do support CreateDataSource' )

    assert_equal(false, @csv_ds.test_capability( 'CreateLayer'),
                 'readonly datasource should not CreateLayer' )

    assert_equal(false, @csv_ds.test_capability( 'DeleteLayer'),
                 'should not have deletelayer on readonly ds.')
  end
end