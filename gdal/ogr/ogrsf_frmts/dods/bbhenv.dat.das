Attributes {
    ogr_layer_info {
	string layer_name WaterQuality;
	string spatial_ref WGS84;
	string target_container Water_Quality;
       	layer_extents {
	    Float64 x_min -180;
	    Float64 y_min -90;
	    Float64 x_max 180;
	    Float64 y_max 90;
        }
        x_field {
            string name YR;
	    string scope dds;
        }
        y_field {
            string name JD;
	    string scope dds;
        }
    }
}
