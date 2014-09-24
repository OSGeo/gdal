Attributes {
    ogr_layer_info_1 {
	string layer_name air;
	string spatial_ref WGS84;
	string target_container air_temperature;
       	layer_extents {
	    Float64 x_min -180;
	    Float64 y_min -90;
	    Float64 x_max 180;
	    Float64 y_max 90;
        }
	extra_containers {
	    string c0 air_temperature_qc;
        }
        x_field {
            string name lon;
	    string scope dds;
        }
        y_field {
            string name lat;
	    string scope dds;
        }
    }
    ogr_layer_info_2 {
	string layer_name wind;
	string spatial_ref WGS84;
	string target_container wind_min;
       	layer_extents {
	    Float64 x_min -180;
	    Float64 y_min -90;
	    Float64 x_max 180;
	    Float64 y_max 90;
        }
	extra_containers {
	    string c0 win_min_qc;
	    string c1 wind_gust;
	    string c2 wind_gust_qc;
	    string c3 wind_speed;
	    string c4 wind_speed_qc;
	    string c5 wind_speed_sc;
	    string c6 wind_speed_sc_qc;
	    string c7 wind_speed_ve;
	    string c8 wind_speed_ve_qc;
        }
        x_field {
            string name lon;
	    string scope dds;
        }
        y_field {
            string name lat;
	    string scope dds;
        }
    }
    ogr_layer_info_3 {
	string layer_name wind_direction;
	string spatial_ref WGS84;
	string target_container wind_direction;
       	layer_extents {
	    Float64 x_min -180;
	    Float64 y_min -90;
	    Float64 x_max 180;
	    Float64 y_max 90;
        }
	extra_containers {
	    string c0 wind_direction_qc;
	    string c1 wind_direction_ve;
	    string c2 wind_direction_ve_qc;
	    string c3 wind_direction_stddev;
	    string c4 wind_direction_stddev_qc;
	    string c5 wind_direction_ve_stddev;
	    string c6 wind_direction_ve_stddev_qc;
	    string c7 wind_direction_uv_stddev;
	    string c8 wind_direction_uv_stddev_qc;
        }
        x_field {
            string name lon;
	    string scope dds;
        }
        y_field {
            string name lat;
	    string scope dds;
        }
    }
    ogr_layer_info_4 {
	string layer_name compass;
	string spatial_ref WGS84;
	string target_container compass;
       	layer_extents {
	    Float64 x_min -180;
	    Float64 y_min -90;
	    Float64 x_max 180;
	    Float64 y_max 90;
        }
	extra_containers {
	    string c0 compass_qc;
	    string c1 compass_stddev;
	    string c2 compass_stddev_qc;
        }
        x_field {
            string name lon;
	    string scope dds;
        }
        y_field {
            string name lat;
	    string scope dds;
        }
    }
    ogr_layer_info_5 {
	string layer_name visibility;
	string spatial_ref WGS84;
	string target_container visibility_counts;
       	layer_extents {
	    Float64 x_min -180;
	    Float64 y_min -90;
	    Float64 x_max 180;
	    Float64 y_max 90;
        }
	extra_containers {
	    string c0 visibility_counts_qc;
	    string c1 visibility_counts_qc;
	    string c2 visibility;
	    string c3 visibility_qc;
	    string c4 min_visibility_counts;
	    string c5 min_visibility_counts_qc;
	    string c6 min_visibility;
	    string c7 min_visibility_qc;
	    string c8 max_visibility_counts;
	    string c9 max_visibility_counts_qc;
	    string c10 max_visibility;
	    string c11 max_visibility_qc;
        }
        x_field {
            string name lon;
	    string scope dds;
        }
        y_field {
            string name lat;
	    string scope dds;
        }
    }
}
