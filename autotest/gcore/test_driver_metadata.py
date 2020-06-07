import pytest
from osgeo import gdal
from xml.etree.ElementTree import fromstring

all_driver_names = [gdal.GetDriver(i).GetDescription() for i in range(gdal.GetDriverCount())]
ogr_driver_names = [driver_name for driver_name in all_driver_names
                    if gdal.GetDriverByName(driver_name).GetMetadataItem('DCAP_VECTOR') == 'YES']
gdal_driver_names = [driver_name for driver_name in all_driver_names
                     if gdal.GetDriverByName(driver_name).GetMetadataItem('DCAP_RASTER') == 'YES']


@pytest.mark.parametrize('driver_name', all_driver_names)
def test_metadata_openoptionlist(driver_name):
    """ Test if DMD_OPENOPTIONLIST metadataitem is present and can be parsed """

    driver = gdal.GetDriverByName(driver_name)
    openoptionlist_xml = driver.GetMetadataItem('DMD_OPENOPTIONLIST')

    if openoptionlist_xml is not None and len(openoptionlist_xml) > 0:
        assert "OpenOptionList" in openoptionlist_xml

        # do not fail
        print(openoptionlist_xml)
        fromstring(openoptionlist_xml)


@pytest.mark.parametrize('driver_name', all_driver_names)
def test_metadata_creationoptionslist(driver_name):
    """ Test if DMD_CREATIONOPTIONLIST metadataitem is present and can be parsed """

    driver = gdal.GetDriverByName(driver_name)
    creationoptions_xml = driver.GetMetadataItem('DMD_CREATIONOPTIONLIST')

    if creationoptions_xml is not None and len(creationoptions_xml) > 0:
        assert "CreationOptionList" in creationoptions_xml

        # do not fail
        print(creationoptions_xml)
        fromstring(creationoptions_xml)


@pytest.mark.parametrize('driver_name', ogr_driver_names)
def test_metadata_layer_creationoptionslist(driver_name):
    """ Test if DS_LAYER_CREATIONOPTIONLIST metadataitem is present and can be parsed """

    driver = gdal.GetDriverByName(driver_name)
    layer_creationoptions_xml = driver.GetMetadataItem('DS_LAYER_CREATIONOPTIONLIST')

    if layer_creationoptions_xml is not None and len(layer_creationoptions_xml) > 0:
        assert "LayerCreationOptionList" in layer_creationoptions_xml

        # do not fail
        print(layer_creationoptions_xml)
        fromstring(layer_creationoptions_xml)
