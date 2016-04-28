%init %{
    SWIGTYPE_p_GDALRasterBand->clientdata = (void*)"Geo::GDAL::Band";
    SWIGTYPE_p_GDALDataset->clientdata = (void*)"Geo::GDAL::Dataset";
    SWIGTYPE_p_GDALDriver->clientdata = (void*)"Geo::GDAL::Driver";
%}
