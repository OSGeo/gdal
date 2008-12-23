static const char *apszGcsNameMapping[] = {
"North_American_Datum_1983", "GCS_North_American_1983",
"North_American_Datum_1927", "GCS_North_American_1927",
"NAD27_CONUS", "GCS_North_American_1927",
"NAD27[CONUS]", "GCS_North_American_1927",
"Reseau_Geodesique_de_Nouvelle_Caledonie_1991-93", "GCS_RGNC_1991-93",
"Reseau_Geodesique_de_la_Polynesie_Francaise", "GCS_RGPF",
"Rauenberg_1983", "GCS_RD/83",
"Phillipine_Reference_System_1992", "GCS_PRS_1992",
"Potsdam_1983", "GCS_PD/83",
"Datum_Geodesi_Nasional_1995", "GCS_DGN_1995",
"Islands_Network_1993", "GCS_ISN_1993",
"Institut_Geographique_du_Congo_Belge_1955", "GCS_IGCB_1955",
"IGC_1962_Arc_of_the_6th_Parallel_South", "GCS_IGC_1962_6th_Parallel_South",
"Jamaica_2001", "GCS_JAD_2001",
"European_Libyan_1979", "GCS_European_Libyan_Datum_1979",
"Madrid_1870", "GCS_Madrid_1870_Madrid",
"Azores_Occidental_Islands_1939", "GCS_Azores_Occidental_1939",
"Azores_Central_Islands_1948", "GCS_Azores_Central_1948",
"Azores_Oriental_Islands_1940", "GCS_Azores_Oriental_1940",
"Lithuania_1994", "GCS_LKS_1994",
"Libyan_Geodetic_Datum_2006", "GCS_LGD2006",
"Lisbon", "GCS_Lisbon_Lisbon",
"Stockholm_1938", "GCS_RT38",
"Latvia_1992", "GCS_LKS_1992",
"Azores_Oriental_Islands_1995", "GCS_Azores_Oriental_1995",
"Azores_Central_Islands_1948", "GCS_Azores_Central_1948", 
"Azores_Central_Islands_1995", "GCS_Azores_Central_1995",
"ATF", "GCS_ATF_Paris",
"ITRF_2000", "GCS_MONREF_1997",
"Faroe_Datum_1954", "GCS_FD_1954",
"Vietnam_2000", "GCS_VN_2000",
"Belge_1950", "GCS_Belge_1950_Brussels",
"Qatar_1948", "GCS_Qatar_1948",
"Qatar", "GCS_Qatar_1974",
"Kuwait_Utility", "GCS_KUDAMS",
NULL, NULL};

static const char *apszGcsNameMappingBasedOnProjCS[] = {
"EUREF_FIN_TM35FIN", "GCS_ETRS_1989", "GCS_EUREF_FIN",
NULL, NULL, NULL};

static const char *apszGcsNameMappingBasedOnUnit[] = {
"Merchich", "Degree", "GCS_Merchich_Degree",
"Voirol_Unifie_1960", "Degree", "GCS_Voirol_Unifie_1960_Degree",
"NTF", "Grad", "GCS_NTF_Paris",
NULL, NULL, NULL};

static const char *apszGcsNameMappingBasedPrime[] = {
"S_JTSK", "Ferro", "GCS_S_JTSK_Ferro",
"MGI", "Ferro", "GCS_MGI_Ferro",
"Madrid_1870", "Madrid", "GCS_Madrid_1870_Madrid",
"Monte_Mario", "Rome", "GCS_Monte_Mario_Rome",
"NGO_1948", "Oslo", "GCS_NGO_1948_Oslo",
"MGI", "Stockholm", "GCS_RT38_Stockholm",
"Stockholm_1938", "Stockholm", "GCS_RT38_Stockholm",
"Bern_1898", "Bern", "GCS_Bern_1898_Bern",
NULL, NULL, NULL};

static const char *apszInvFlatteningMapping[] = {
"293.464999999", "293.465", 
"293.466020000", "293.46602",
"294.26067636900", "294.260676369", 
"294.9786981999", "294.9786982", 
"294.978698213", "294.9786982",
"295.9999999999", "296.0", 
"297.0000000000", "297.0",
"298.256999999", "298.257",
"298.2600000000", "298.26",
"298.25722210100", "298.257222101",
"298.25722356299", "298.257223563",
"298.2684109950054", "298.268410995005",
"298.299999999", "298.3",
"299.15281280000", "299.1528128",
"300.80169999999", "300.8017",
"300.80170000000", "300.8017",
NULL, NULL};

static const char *apszParamValueMapping[] = {
"Cassini", "false_easting", "283799.9999", "283800.0",
"Cassini", "false_easting", "132033.9199", "132033.92",
"Cassini", "false_northing", "214499.9999", "214500.0",
"Cassini", "false_northing", "62565.9599", "62565.95", 
"Transverse_Mercator", "false_easting", "499999.1331", "500000.0",  
"Transverse_Mercator", "false_easting", "299999.4798609", "300000.0", 
"Transverse_Mercator", "false_northing", "399999.30648", "400000.0",
"Transverse_Mercator", "false_northing", "499999.1331", "500000.0",
NULL, NULL, NULL, NULL};

static const char *apszParamNameMapping[] = {
"Lambert_Azimuthal_Equal_Area", "longitude_of_center", "Central_Meridian",
"Lambert_Azimuthal_Equal_Area", "Latitude_Of_Center", "Latitude_Of_Origin",
"Miller_Cylindrical", "longitude_of_center", "Central_Meridian",
"Gnomonic", "central_meridian", "Longitude_Of_Center",
"Gnomonic", "latitude_of_origin", "Latitude_Of_Center",
"Orthographic", "central_meridian", "Longitude_Of_Center",
"Orthographic", "latitude_of_origin", "Latitude_Of_Center",
"New_Zealand_Map_Grid", "central_meridian", "Longitude_Of_Origin",
NULL, NULL, NULL};

static const char *apszDeleteParametersBasedOnProjection[] = {
"Stereographic_South_Pole", "scale_factor",
"Stereographic_North_Pole", "scale_factor",
"Mercator", "scale_factor",
"Miller_Cylindrical", "latitude_of_center",
"Equidistant_Cylindrical", "pseudo_standard_parallel_1", 
"Plate_Carree", "latitude_of_origin",
"Plate_Carree", "pseudo_standard_parallel_1",
"Plate_Carree", "standard_parallel_1",
"Hotine_Oblique_Mercator_Azimuth_Center", "rectified_grid_angle", 
"Hotine_Oblique_Mercator_Azimuth_Natural_Origin", "rectified_grid_angle", 
NULL, NULL};

static const char *apszAddParametersBasedOnProjection[] = {
"Cassini", "scale_factor", "1.0", 
"Lambert_Conformal_Conic", "scale_factor", "1.0",
"Mercator", "standard_parallel_1", "0.0", 
NULL, NULL, NULL};
