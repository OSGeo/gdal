Attributes {
    opendap_org_gdal {
        Float64 Northernmost_Latitude 89.5;
	Float64 Southernmost_Latitude -89.5;
	Float64	Easternmost_Longitude 179.5;
	Float64	Westernmost_Longitude -179.5;
	# see http://remotesensing.org/geotiff/proj_list/equirectangular.html
	String ProjectionCS Equirectangular;
	String GeographicCS WGS84;
	Norm_Proj_Param {
	    Float64 latitude_of_origin 0.0;
	    Float64 central_meridian 0.0;
	    Float64 false_easting 0.0;
	    Float64 false_northing 0.0;
        }
    }
}
