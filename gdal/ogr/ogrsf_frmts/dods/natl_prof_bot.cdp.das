Attributes {
    ogr_layer_info_1 {
	string layer_name normalized;
	string spatial_ref WGS84;
	string target_container location.profile;
        x_field {
            string name location.lon;
	    string scope dds;
        }
        y_field {
            string name location.lat;
	    string scope dds;
        }
    }
    ogr_layer_info_2 {
	string layer_name profiles;
	string spatial_ref WGS84;
	string target_container location;
        x_field {
            string name location.lon;
	    string scope dds;
        }
        y_field {
            string name location.lat;
	    string scope dds;
        }
    }
    ogr_layer_info_3 {
	string layer_name lines;
	string spatial_ref WGS84;
	string target_container location;
        x_field {
            string name location.profile.depth;
	    string scope dds;
        }
        y_field {
            string name location.profile.T_20;
	    string scope dds;
        }
    }
}
