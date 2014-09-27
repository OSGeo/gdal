/*****************************************************************************
 * metaname.c
 *
 * DESCRIPTION
 *    This file contains the code necessary to parse the GRIB2 product
 * definition information into human readable text.  In addition to the
 * tables in the GRIB2 specs, it also attempts to handle local table
 * definitions that NCEP and NDFD have developed.
 *
 * HISTORY
 *    1/2004 Arthur Taylor (MDL / RSIS): Created.
 *
 * NOTES
 *****************************************************************************
 */
#include <string.h>
#include <stdlib.h>
#include "meta.h"
#include "metaname.h"
#include "myerror.h"
#include "myassert.h"
#include "myutil.h"

#include "cpl_port.h"

const char *centerLookup (unsigned short int center)
{
   /* see:
    * http://www.wmo.ch/web/www/WMOCodes/Operational/CommonTables/BUFRCommon-2005feb.pdf
    * http://www.wmo.int/web/www/WMOCodes/Operational/CommonTables/BUFRCommon-2005nov.pdf
    * also see:
    * http://www.nco.ncep.noaa.gov/pmb/docs/on388/table0.html 
    * I typed this in on 11/2/2006 */
/* *INDENT-OFF* */
   static struct {
      unsigned short int num;
      const char *name;
   } Center[] = {
      {0, "WMO Secretariat"},
      {1, "Melbourne"},
      {2, "Melbourne"},
      {3, ") Melbourne"},
      {4, "Moscow"},
      {5, "Moscow"},
      {6, ") Moscow"},
      {7, "US-NCEP"},
      {8, "US-NWSTG"},
      {9, "US-Other"},
      {10, "Cairo"},
      {11, ") Cairo"},
      {12, "Dakar"},
      {13, ") Dakar"},
      {14, "Nairobi"},
      {15, ") Nairobi"},
      {16, "Casablanca"},
      {17, "Tunis"},
      {18, "Tunis Casablanca"},
      {19, ") Tunis Casablanca"},
      {20, "Las Palmas"},
      {21, "Algiers"},
      {22, "ACMAD"},
      {23, "Mozambique"},
      {24, "Pretoria"},
      {25, "La Réunion"},
      {26, "Khabarovsk"},
      {27, ") Khabarovsk"},
      {28, "New Delhi"},
      {29, ") New Delhi"},
      {30, "Novosibirsk"},
      {31, ") Novosibirsk"},
      {32, "Tashkent"},
      {33, "Jeddah"},
      {34, "Tokyo"},
      {35, ") Tokyo"},
      {36, "Bangkok"},
      {37, "Ulan Bator"},
      {38, "Beijing"},
      {39, ") Beijing"},
      {40, "Seoul"},
      {41, "Buenos Aires"},
      {42, ") Buenos Aires"},
      {43, "Brasilia"},
      {44, ") Brasilia"},
      {45, "Santiago"},
      {46, "Brazilian Space Agency"},
      {47, "Colombia"},
      {48, "Ecuador"},
      {49, "Peru"},
      {50, "Venezuela"},
      {51, "Miami"},
      {52, "Miami-NHC"},
      {53, "Montreal"},
      {54, ") Montreal"},
      {55, "San Francisco"},
      {56, "ARINC Centre"},
      {57, "US-Air Force Weather"},
      {58, "US-Fleet Meteorology and Oceanography"},
      {59, "US-FSL"},
      {60, "US-NCAR"},
      {61, "US-Service ARGOS"},
      {62, "US-Naval Oceanographic Office"},
/*      {63, "Reserved for another centre in Region IV"},*/
      {64, "Honolulu"},
      {65, "Darwin"},
      {66, ") Darwin"},
      {67, "Melbourne"},
/*      {68, "Reserved"},*/
      {69, "Wellington"},
      {70, ") Wellington"},
      {71, "Nadi"},
      {72, "Singapore"},
      {73, "Malaysia"},
      {74, "UK-Met-Exeter"},
      {75, ") UK-Met-Exeter"},
      {76, "Moscow"},
/*      {77, "Reserved"},*/
      {78, "Offenbach"},
      {79, ") Offenbach"},
      {80, "Rome"},
      {81, ") Rome"},
      {82, "Norrköping"},
      {83, ") Norrköping"},
      {84, "Toulouse"},
      {85, "Toulouse"},
      {86, "Helsinki"},
      {87, "Belgrade"},
      {88, "Oslo"},
      {89, "Prague"},
      {90, "Episkopi"},
      {91, "Ankara"},
      {92, "Frankfurt/Main"},
      {93, "London"},
      {94, "Copenhagen"},
      {95, "Rota"},
      {96, "Athens"},
      {97, "ESA-European Space Agency"},
      {98, "ECMWF"},
      {99, "DeBilt"},
      {100, "Brazzaville"},
      {101, "Abidjan"},
      {102, "Libyan Arab Jamahiriya"},
      {103, "Madagascar"},
      {104, "Mauritius"},
      {105, "Niger"},
      {106, "Seychelles"},
      {107, "Uganda"},
      {108, "Tanzania"},
      {109, "Zimbabwe"},
      {110, "Hong-Kong, China"},
      {111, "Afghanistan"},
      {112, "Bahrain"},
      {113, "Bangladesh"},
      {114, "Bhutan"},
      {115, "Cambodia"},
      {116, "Democratic People's Republic of Korea"},
      {117, "Islamic Republic of Iran"},
      {118, "Iraq"},
      {119, "Kazakhstan"},
      {120, "Kuwait"},
      {121, "Kyrgyz Republic"},
      {122, "Lao People's Democratic Republic"},
      {123, "Macao, China"},
      {124, "Maldives"},
      {125, "Myanmar"},
      {126, "Nepal"},
      {127, "Oman"},
      {128, "Pakistan"},
      {129, "Qatar"},
      {130, "Republic of Yemen"},
      {131, "Sri Lanka"},
      {132, "Tajikistan"},
      {133, "Turkmenistan"},
      {134, "United Arab Emirates"},
      {135, "Uzbekistan"},
      {136, "Socialist Republic of Viet Nam"},
/*      {137, "Reserved"},*/
/*      {138, "Reserved"},*/
/*      {139, "Reserved"},*/
      {140, "Bolivia"},
      {141, "Guyana"},
      {142, "Paraguay"},
      {143, "Suriname"},
      {144, "Uruguay"},
      {145, "French Guyana"},
      {146, "Brazilian Navy Hydrographic Centre"},
/*      {147, "Reserved"},*/
/*      {148, "Reserved"},*/
/*      {149, "Reserved"},*/
      {150, "Antigua and Barbuda"},
      {151, "Bahamas"},
      {152, "Barbados"},
      {153, "Belize"},
      {154, "British Caribbean Territories"},
      {155, "San Jose"},
      {156, "Cuba"},
      {157, "Dominica"},
      {158, "Dominican Republic"},
      {159, "El Salvador"},
      {160, "US-NESDIS"},
      {161, "US-OAR"},
      {162, "Guatemala"},
      {163, "Haiti"},
      {164, "Honduras"},
      {165, "Jamaica"},
      {166, "Mexico"},
      {167, "Netherlands Antilles and Aruba"},
      {168, "Nicaragua"},
      {169, "Panama"},
      {170, "Saint Lucia NMC"},
      {171, "Trinidad and Tobago"},
      {172, "French Departments"},
/*      {173, "Reserved"},*/
/*      {174, "Reserved"},*/
/*      {175, "Reserved"},*/
/*      {176, "Reserved"},*/
/*      {177, "Reserved"},*/
/*      {178, "Reserved"},*/
/*      {179, "Reserved"},*/
/*      {180, "Reserved"},*/
/*      {181, "Reserved"},*/
/*      {182, "Reserved"},*/
/*      {183, "Reserved"},*/
/*      {184, "Reserved"},*/
/*      {185, "Reserved"},*/
/*      {186, "Reserved"},*/
/*      {187, "Reserved"},*/
/*      {188, "Reserved"},*/
/*      {189, "Reserved"},*/
      {190, "Cook Islands"},
      {191, "French Polynesia"},
      {192, "Tonga"},
      {193, "Vanuatu"},
      {194, "Brunei"},
      {195, "Indonesia"},
      {196, "Kiribati"},
      {197, "Federated States of Micronesia"},
      {198, "New Caledonia"},
      {199, "Niue"},
      {200, "Papua New Guinea"},
      {201, "Philippines"},
      {202, "Samoa"},
      {203, "Solomon Islands"},
/*      {204, "Reserved"},*/
/*      {205, "Reserved"},*/
/*      {206, "Reserved"},*/
/*      {207, "Reserved"},*/
/*      {208, "Reserved"},*/
/*      {209, "Reserved"},*/
      {210, "Frascati (ESA/ESRIN)"},
      {211, "Lanion"},
      {212, "Lisboa"},
      {213, "Reykiavik"},
      {214, "Madrid"},
      {215, "Zürich"},
      {216, "Service ARGOS Toulouse"},
      {217, "Bratislava"},
      {218, "Budapest"},
      {219, "Ljubljana"},
      {220, "Warsaw"},
      {221, "Zagreb"},
      {222, "Albania"},
      {223, "Armenia"},
      {224, "Austria"},
      {225, "Azerbaijan"},
      {226, "Belarus"},
      {227, "Belgium"},
      {228, "Bosnia and Herzegovina"},
      {229, "Bulgaria"},
      {230, "Cyprus"},
      {231, "Estonia"},
      {232, "Georgia"},
      {233, "Dublin"},
      {234, "Israel"},
      {235, "Jordan"},
      {236, "Latvia"},
      {237, "Lebanon"},
      {238, "Lithuania"},
      {239, "Luxembourg"},
      {240, "Malta"},
      {241, "Monaco"},
      {242, "Romania"},
      {243, "Syrian Arab Republic"},
      {244, "The former Yugoslav Republic of Macedonia"},
      {245, "Ukraine"},
      {246, "Republic of Moldova"},
/*      {247, "Reserved"},*/
/*      {248, "Reserved"},*/
/*      {249, "Reserved"},*/
/*      {250, "Reserved"},*/
/*      {251, "Reserved"},*/
/*      {252, "Reserved"},*/
/*      {253, "Reserved"},*/
      {254, "EUMETSAT Operation Centre"},
/*      {255, "Reserved"},*/
      {256, "Angola"},
      {257, "Benin"},
      {258, "Botswana"},
      {259, "Burkina Faso"},
      {260, "Burundi"},
      {261, "Cameroon"},
      {262, "Cape Verde"},
      {263, "Central African republic"},
      {264, "Chad"},
      {265, "Comoros"},
      {266, "Democratic Republic of the Congo"},
      {267, "Djibouti"},
      {268, "Eritrea"},
      {269, "Ethiopia"},
      {270, "Gabon"},
      {271, "Gambia"},
      {272, "Ghana"},
      {273, "Guinea"},
      {274, "Guinea Bissau"},
      {275, "Lesotho"},
      {276, "Liberia"},
      {277, "Malawi"},
      {278, "Mali"},
      {279, "Mauritania"},
      {280, "Namibia"},
      {281, "Nigeria"},
      {282, "Rwanda"},
      {283, "Sao Tome and Principe"},
      {284, "Sierra Leone"},
      {285, "Somalia"},
      {286, "Sudan"},
      {287, "Swaziland"},
      {288, "Togo"},
      {289, "Zambia"}
   };
/* *INDENT-ON* */
   int numCenter = sizeof (Center) / sizeof (Center[0]);
   int i;

   for (i = 0; i < numCenter; i++) {
      if (Center[i].num == center) {
         return Center[i].name;
      }
   }
   return NULL;
}

const char *subCenterLookup(unsigned short int center,
                            unsigned short int subcenter)
{
/* *INDENT-OFF* */
/* see:
 * http://www.wmo.ch/web/www/WMOCodes/Operational/CommonTables/
 *        BUFRCommon-2005feb.pdf as of 10/12/2005
 * http://www.nco.ncep.noaa.gov/pmb/docs/on388/tablec.html as of 11/9/2005
 * http://www.ecmwf.int/publications/manuals/libraries/gribex/
 *        localGRIBUsage.html as of 4/5/2006
 */
   static struct {
      unsigned short int center, subcenter;
      const char *name;
   } SubCenter[] = {
      {7, 1, "NCEP Re-Analysis Project"}, {7, 2, "NCEP Ensemble Products"},
      {7, 3, "NCEP Central Operations"},
      {7, 4, "Environmental Modeling Center"},
      {7, 5, "Hydrometeorological Prediction Center"},
      {7, 6, "Ocean Prediction Center"}, {7, 7, "Climate Prediction Center"},
      {7, 8, "Aviation Weather Center"}, {7, 9, "Storm Prediction Center"},
      {7, 10, "Tropical Prediction Center"},
      {7, 11, "Techniques Development Laboratory"},
      {7, 12, "NESDIS Office of Research and Applications"},
      {7, 13, "FAA"},
      {7, 14, "Meteorological Development Laboratory (MDL)"},
      {7, 15, "North American Regional Reanalysis (NARR) Project"},
      {7, 16, "Space Environment Center"},
      {8, 0, "National Digital Forecast Database"},
/*      {8, GRIB2MISSING_2, "National Digital Forecast Database"},*/
      {161, 1, "Great Lakes Environmental Research Laboratory"},
      {161, 2, "Forecast Systems Laboratory"},
      {74, 1, "Shanwick Oceanic Area Control Centre"},
      {74, 2, "Fucino"}, {74, 3, "Gatineau"}, {74, 4, "Maspalomas"},
      {74, 5, "ESA ERS Central Facility"}, {74, 6, "Prince Albert"},
      {74, 7, "West Freugh"}, {74, 13, "Tromso"},
      {74, 21, "Agenzia Spaziale Italiana (Italy)"},
      {74, 22, "Centre National de la Recherche Scientifique (France)"},
      {74, 23, "GeoForschungsZentrum (Germany)"},
      {74, 24, "Geodetic Observatory Pecny (Czech Republic)"},
      {74, 25, "Institut d'Estudis Espacials de Catalunya (Spain)"},
      {74, 26, "Swiss Federal Office of Topography"},
      {74, 27, "Nordic Commission of Geodesy (Norway)"},
      {74, 28, "Nordic Commission of Geodesy (Sweden)"},
      {74, 29, "Institute de Geodesie National (France)"},
      {74, 30, "Bundesamt für Kartographie und Geodäsie (Germany)"},
      {74, 31, "Institute of Engineering Satellite Surveying and Geodesy (U.K.)"},
      {254, 10, "Tromso (Norway)"}, {254, 10, "Maspalomas (Spain)"},
      {254, 30, "Kangerlussuaq (Greenland)"}, {254, 40, "Edmonton (Canada)"},
      {254, 50, "Bedford (Canada)"}, {254, 60, "Gander (Canada)"},
      {254, 70, "Monterey (USA)"}, {254, 80, "Wallops Island (USA)"},
      {254, 90, "Gilmor Creek (USA)"}, {254, 100, "Athens (Greece)"},
      {98, 231, "CNRM, Meteo France Climate Centre (HIRETYCS)"},
      {98, 232, "MPI, Max Planck Institute Climate Centre (HIRETYCS)"},
      {98, 233, "UKMO Climate Centre (HIRETYCS)"},
      {98, 234, "ECMWF (DEMETER)"},
      {98, 235, "INGV-CNR (Bologna, Italy)(DEMETER)"},
      {98, 236, "LODYC (Paris, France)(DEMETER)"},
      {98, 237, "DMI (Copenhagen, Denmark)(DEMETER)"},
      {98, 238, "INM (Madrid, Spain)(DEMETER)"},
      {98, 239, "CERFACS (Toulouse, France)(DEMETER)"},
      {98, 240, "ECMWF (PROVOST)"},
      {98, 241, "Meteo France (PROVOST)"},
      {98, 242, "EDF (PROVOST)"},
      {98, 243, "UKMO (PROVOST)"},
      {98, 244, "Biometeorology group, University of Veterinary Medicine, Vienna (ELDAS)"},
   };
/* *INDENT-ON* */
   int numSubCenter = sizeof (SubCenter) / sizeof (SubCenter[0]);
   int i;

   for (i = 0; i < numSubCenter; i++) {
      if ((SubCenter[i].center == center) &&
          (SubCenter[i].subcenter == subcenter)) {
         return SubCenter[i].name;
      }
   }
   return NULL;
}

const char *processLookup (unsigned short int center, unsigned char process)
{
   /* see: http://www.nco.ncep.noaa.gov/pmb/docs/on388/tablea.html I typed
    * this in on 10/12/2005 */
/* *INDENT-OFF* */
   static struct {
      unsigned short int center;
      unsigned char process;
      const char *name;
   } Process[] = {
      {7, 2, "Ultra Violet Index Model"},
      {7, 3, "NCEP/ARL Transport and Dispersion Model"},
      {7, 4, "NCEP/ARL Smoke Model"},
      {7, 5, "Satellite Derived Precipitation and temperatures, from IR"},
      {7, 10, "Global Wind-Wave Forecast Model"},
      {7, 19, "Limited-area Fine Mesh (LFM) analysis"},
      {7, 25, "Snow Cover Analysis"},
      {7, 30, "Forecaster generated field"},
      {7, 31, "Value added post processed field"},
      {7, 39, "Nested Grid forecast Model (NGM)"},
      {7, 42, "Global Optimum Interpolation Analysis (GOI) from GFS model"},
      {7, 43, "Global Optimum Interpolation Analysis (GOI) from 'Final' run"},
      {7, 44, "Sea Surface Temperature Analysis"},
      {7, 45, "Coastal Ocean Circulation Model"},
      {7, 46, "HYCOM - Global"},
      {7, 47, "HYCOM - North Pacific basin"},
      {7, 48, "HYCOM - North Atlantic basin"},
      {7, 49, "Ozone Analysis from TIROS Observations"},
      {7, 52, "Ozone Analysis from Nimbus 7 Observations"},
      {7, 53, "LFM-Fourth Order Forecast Model"},
      {7, 64, "Regional Optimum Interpolation Analysis (ROI)"},
      {7, 68, "80 wave triangular, 18-layer Spectral model from GFS model"},
      {7, 69, "80 wave triangular, 18 layer Spectral model from 'Medium Range Forecast' run"},
      {7, 70, "Quasi-Lagrangian Hurricane Model (QLM)"},
      {7, 73, "Fog Forecast model - Ocean Prod. Center"},
      {7, 74, "Gulf of Mexico Wind/Wave"},
      {7, 75, "Gulf of Alaska Wind/Wave"},
      {7, 76, "Bias corrected Medium Range Forecast"},
      {7, 77, "126 wave triangular, 28 layer Spectral model from GFS model"},
      {7, 78, "126 wave triangular, 28 layer Spectral model from 'Medium Range Forecast' run"},
      {7, 79, "Backup from the previous run"},
      {7, 80, "62 wave triangular, 28 layer Spectral model from 'Medium Range Forecast' run"},
      {7, 81, "Spectral Statistical Interpolation (SSI) analysis from GFS model"},
      {7, 82, "Spectral Statistical Interpolation (SSI) analysis from 'Final' run."},
      {7, 84, "MESO ETA Model (currently 12 km)"},
      {7, 86, "RUC Model from FSL (isentropic; scale: 60km at 40N)"},
      {7, 87, "CAC Ensemble Forecasts from Spectral (ENSMB)"},
      {7, 88, "NOAA Wave Watch III (NWW3) Ocean Wave Model"},
      {7, 89, "Non-hydrostatic Meso Model (NMM) Currently 8 km)"},
      {7, 90, "62 wave triangular, 28 layer spectral model extension of the 'Medium Range Forecast' run"},
      {7, 91, "62 wave triangular, 28 layer spectral model extension of the GFS model"},
      {7, 92, "62 wave triangular, 28 layer spectral model run from the 'Medium Range Forecast' final analysis"},
      {7, 93, "62 wave triangular, 28 layer spectral model run from the T62 GDAS analysis of the 'Medium Range Forecast' run"},
      {7, 94, "T170/L42 Global Spectral Model from MRF run"},
      {7, 95, "T126/L42 Global Spectral Model from MRF run"},
      {7, 96, "Global Forecast System Model"},
      {7, 98, "Climate Forecast System Model"},
      {7, 100, "RUC Surface Analysis (scale: 60km at 40N)"},
      {7, 101, "RUC Surface Analysis (scale: 40km at 40N)"},
      {7, 105, "RUC Model from FSL (isentropic; scale: 20km at 40N)"},
      {7, 110, "ETA Model - 15km version"},
      {7, 111, "Eta model, generic resolution"},
      {7, 112, "WRF-NMM (Nondydrostatic Mesoscale Model) model, generic resolution"},
      {7, 113, "Products from NCEP SREF processing"},
      {7, 115, "Downscaled GFS from Eta eXtension"},
      {7, 116, "WRF-EM (Eulerian Mass-core) model, generic resolution "},
      {7, 120, "Ice Concentration Analysis"},
      {7, 121, "Western North Atlantic Regional Wave Model"},
      {7, 122, "Alaska Waters Regional Wave Model"},
      {7, 123, "North Atlantic Hurricane Wave Model"},
      {7, 124, "Eastern North Pacific Regional Wave Model"},
      {7, 125, "North Pacific Hurricane Wave Model"},
      {7, 126, "Sea Ice Forecast Model"},
      {7, 127, "Lake Ice Forecast Model"},
      {7, 128, "Global Ocean Forecast Model"},
      {7, 129, "Global Ocean Data Analysis System (GODAS)"},
      {7, 130, "Merge of fields from the RUC, Eta, and Spectral Model"},
      {7, 131, "Great Lakes Wave Model"},
      {7, 140, "North American Regional Reanalysis (NARR)"},
      {7, 141, "Land Data Assimilation and Forecast System"},
      {7, 150, "NWS River Forecast System (NWSRFS)"},
      {7, 151, "NWS Flash Flood Guidance System (NWSFFGS)"},
      {7, 152, "WSR-88D Stage II Precipitation Analysis"},
      {7, 153, "WSR-88D Stage III Precipitation Analysis"},
      {7, 180, "Quantitative Precipitation Forecast"},
      {7, 181, "River Forecast Center Quantitative Precipitation Forecast mosaic"},
      {7, 182, "River Forecast Center Quantitative Precipitation estimate mosaic"},
      {7, 183, "NDFD product generated by NCEP/HPC"},
      {7, 190, "National Convective Weather Diagnostic"},
      {7, 191, "Current Icing Potential automated product"},
      {7, 192, "Analysis product from NCEP/AWC"},
      {7, 193, "Forecast product from NCEP/AWC"},
      {7, 195, "Climate Data Assimilation System 2 (CDAS2)"},
      {7, 196, "Climate Data Assimilation System 2 (CDAS2)"},
      {7, 197, "Climate Data Assimilation System (CDAS)"},
      {7, 198, "Climate Data Assimilation System (CDAS)"},
      {7, 200, "CPC Manual Forecast Product"},
      {7, 201, "CPC Automated Product"},
      {7, 210, "EPA Air Quality Forecast"},
      {7, 211, "EPA Air Quality Forecast"},
      {7, 220, "NCEP/OPC automated product"}
   };
/* *INDENT-ON* */
   int numProcess = sizeof (Process) / sizeof (Process[0]);
   int i;

   for (i = 0; i < numProcess; i++) {
      if ((Process[i].center == center) && (Process[i].process == process)) {
         return Process[i].name;
      }
   }
   return NULL;
}

typedef struct {
   const char *name, *comment, *unit;
   unit_convert convert;
} GRIB2ParmTable;

typedef struct {
   int prodType, cat, subcat;
   const char *name, *comment, *unit;
   unit_convert convert;
} GRIB2LocalTable;

typedef struct {
    const char *GRIB2name, *NDFDname;
} NDFD_AbrevOverideTable;

/* *INDENT-OFF* */
/* Updated based on:
 * http://www.wmo.ch/web/www/WMOCodes/Operational/GRIB2/FM92-GRIB2-2005nov.pdf
 * 1/3/2006
 */
/* GRIB2 Code table 4.2 : 0.0 */
GRIB2ParmTable MeteoTemp[] = {
   /* 0 */ {"TMP", "Temperature", "K", UC_K2F},  /* Need NDFD override. T */
   /* 1 */ {"VTMP", "Virtual temperature", "K", UC_K2F},
   /* 2 */ {"POT", "Potential temperature", "K", UC_K2F},
   /* 3 */ {"EPOT", "Pseudo-adiabatic potential temperature", "K", UC_K2F},
   /* 4 */ {"TMAX", "Maximum Temperature", "K", UC_K2F}, /* Need NDFD override MaxT */
   /* 5 */ {"TMIN", "Minimum Temperature", "K", UC_K2F}, /* Need NDFD override MinT */
   /* 6 */ {"DPT", "Dew point temperature", "K", UC_K2F}, /* Need NDFD override Td */
   /* 7 */ {"DEPR", "Dew point depression", "K", UC_K2F},
   /* 8 */ {"LAPR", "Lapse rate", "K/m", UC_NONE},
   /* 9 */ {"TMPA", "Temperature anomaly", "K", UC_K2F},
   /* 10 */ {"LHTFL", "Latent heat net flux", "W/(m^2)", UC_NONE},
   /* 11 */ {"SHTFL", "Sensible heat net flux", "W/(m^2)", UC_NONE},
            /* NDFD */
   /* 12 */ {"HeatIndex", "Heat index", "K", UC_K2F},
            /* NDFD */
   /* 13 */ {"WCI", "Wind chill factor", "K", UC_K2F},
   /* 14 */ {"", "Minimum dew point depression", "K", UC_K2F},
   /* 15 */ {"VPTMP", "Virtual potential temperature", "K", UC_K2F},
/* 16 */    {"SNOHF", "Snow phase change heat flux", "W/m^2", UC_NONE},
};

/* GRIB2 Code table 4.2 : 0.1 */
/* NCEP added "Water" to items 22, 24, 25 */
GRIB2ParmTable MeteoMoist[] = {
   /* 0 */ {"SPFH", "Specific humidity", "kg/kg", UC_NONE},
   /* 1 */ {"RH", "Relative Humidity", "%", UC_NONE},
   /* 2 */ {"MIXR", "Humidity mixing ratio", "kg/kg", UC_NONE},
   /* 3 */ {"PWAT", "Precipitable water", "kg/(m^2)", UC_NONE},
   /* 4 */ {"VAPP", "Vapor Pressure", "Pa", UC_NONE},
   /* 5 */ {"SATD", "Saturation deficit", "Pa", UC_NONE},
   /* 6 */ {"EVP", "Evaporation", "kg/(m^2)", UC_NONE},
   /* 7 */ {"PRATE", "Precipitation rate", "kg/(m^2 s)", UC_NONE},
   /* 8 */ {"APCP", "Total precipitation", "kg/(m^2)", UC_InchWater}, /* Need NDFD override QPF */
   /* 9 */ {"NCPCP", "Large scale precipitation", "kg/(m^2)", UC_NONE},
   /* 10 */ {"ACPCP", "Convective precipitation", "kg/(m^2)", UC_NONE},
   /* 11 */ {"SNOD", "Snow depth", "m", UC_M2Inch}, /* Need NDFD override SnowDepth ? */
   /* 12 */ {"SRWEQ", "Snowfall rate water equivalent", "kg/(m^2 s)", UC_NONE},
   /* 13 */ {"WEASD", "Water equivalent of accumulated snow depth",
             "kg/(m^2)", UC_NONE},
   /* 14 */ {"SNOC", "Convective snow", "kg/(m^2)", UC_NONE},
   /* 15 */ {"SNOL", "Large scale snow", "kg/(m^2)", UC_NONE},
   /* 16 */ {"SNOM", "Snow melt", "kg/(m^2)", UC_NONE},
   /* 17 */ {"SNOAG", "Snow age", "day", UC_NONE},
   /* 18 */ {"", "Absolute humidity", "kg/(m^3)", UC_NONE},
   /* 19 */ {"", "Precipitation type", "(1 Rain, 2 Thunderstorm, "
             "3 Freezing Rain, 4 Mixed/ice, 5 snow, 255 missing)", UC_NONE},
   /* 20 */ {"", "Integrated liquid water", "kg/(m^2)", UC_NONE},
   /* 21 */ {"TCOND", "Condensate", "kg/kg", UC_NONE},
/* CLWMR Did not make it to tables yet should be "-" */
   /* 22 */ {"CLWMR", "Cloud Water Mixing Ratio", "kg/kg", UC_NONE},
   /* 23 */ {"ICMR", "Ice water mixing ratio", "kg/kg", UC_NONE},   /* ICMR? */
   /* 24 */ {"RWMR", "Rain Water Mixing Ratio", "kg/kg", UC_NONE},
   /* 25 */ {"SNMR", "Snow Water Mixing Ratio", "kg/kg", UC_NONE},
   /* 26 */ {"MCONV", "Horizontal moisture convergence", "kg/(kg s)", UC_NONE},
   /* 27 */ {"", "Maximum relative humidity", "%", UC_NONE},
   /* 28 */ {"", "Maximum absolute humidity", "kg/(m^3)", UC_NONE},
            /* NDFD */
   /* 29 */ {"ASNOW", "Total snowfall", "m", UC_M2Inch},
   /* 30 */ {"", "Precipitable water category", "(undefined)", UC_NONE},
   /* 31 */ {"", "Hail", "m", UC_NONE},
   /* 32 */ {"", "Graupel (snow pellets)", "kg/kg", UC_NONE},
/* 33 */    {"CRAIN", "Categorical rain", "0=no, 1=yes", UC_NONE},
/* 34 */    {"CFRZR", "Categorical freezing rain", "0=no, 1=yes", UC_NONE},
/* 35 */    {"CICEP", "Categorical ice pellets", "0=no, 1=yes", UC_NONE},
/* 36 */    {"CSNOW", "Categorical snow", "0=no, 1=yes", UC_NONE},
/* 37 */    {"CPRAT", "Convective precipitation rate", "kg/(m^2*s)", UC_NONE},
/* 38 */    {"MCONV", "Horizontal moisture divergence", "kg/(kg*s)", UC_NONE},
/* 39 */    {"CPOFP", "Percent frozen precipitation", "%", UC_NONE},
/* 40 */    {"PEVAP", "Potential evaporation", "kg/m^2", UC_NONE},
/* 41 */    {"PEVPR", "Potential evaporation rate", "W/m^2", UC_NONE},
/* 42 */    {"SNOWC", "Snow Cover", "%", UC_NONE},
/* 43 */    {"FRAIN", "Rain fraction of total cloud water", "-", UC_NONE},
/* 44 */    {"RIME", "Rime factor", "-", UC_NONE},
/* 45 */    {"TCOLR", "Total column integrated rain", "kg/m^2", UC_NONE},
/* 46 */    {"TCOLS", "Total column integrated snow", "kg/m^2", UC_NONE},
};

/* GRIB2 Code table 4.2 : 0.2 */
GRIB2ParmTable MeteoMoment[] = {
   /* 0 */ {"WDIR", "Wind direction (from which blowing)", "deg true",
            UC_NONE}, /* Need NDFD override WindDir */
   /* 1 */ {"WIND", "Wind speed", "m/s", UC_MS2Knots}, /* Need NDFD override WindSpd */
   /* 2 */ {"UGRD", "u-component of wind", "m/s", UC_NONE},
   /* 3 */ {"VGRD", "v-component of wind", "m/s", UC_NONE},
   /* 4 */ {"STRM", "Stream function", "(m^2)/s", UC_NONE},
   /* 5 */ {"VPOT", "Velocity potential", "(m^2)/s", UC_NONE},
   /* 6 */ {"MNTSF", "Montgomery stream function", "(m^2)/(s^2)", UC_NONE},
   /* 7 */ {"SGCVV", "Sigma coordinate vertical velocity", "1/s", UC_NONE},
   /* 8 */ {"VVEL", "Vertical velocity (pressure)", "Pa/s", UC_NONE}, /* NCEP override WEL?  */
   /* 9 */ {"DZDT", "Verical velocity (geometric)", "m/s", UC_NONE},
   /* 10 */ {"ABSV", "Absolute vorticity", "1/s", UC_NONE},
   /* 11 */ {"ABSD", "Absolute divergence", "1/s", UC_NONE},
   /* 12 */ {"RELV", "Relative vorticity", "1/s", UC_NONE},
   /* 13 */ {"RELD", "Relative divergence", "1/s", UC_NONE},
   /* 14 */ {"PV", "Potential vorticity", "K(m^2)/(kg s)", UC_NONE},
   /* 15 */ {"VUCSH", "Vertical u-component shear", "1/s", UC_NONE},
   /* 16 */ {"VVCSH", "Vertical v-component shear", "1/s", UC_NONE},
   /* 17 */ {"UFLX", "Momentum flux; u component", "N/(m^2)", UC_NONE},
   /* 18 */ {"VFLX", "Momentum flux; v component", "N/(m^2)", UC_NONE},
   /* 19 */ {"WMIXE", "Wind mixing energy", "J", UC_NONE},
   /* 20 */ {"BLYDP", "Boundary layer dissipation", "W/(m^2)", UC_NONE},
   /* 21 */ {"", "Maximum wind speed", "m/s", UC_NONE},
   /* 22 */ {"GUST", "Wind speed (gust)", "m/s", UC_MS2Knots},  /* GUST? */
   /* 23 */ {"", "u-component of wind (gust)", "m/s", UC_NONE},
   /* 24 */ {"", "v-component of wind (gust)", "m/s", UC_NONE},
/* 25 */    {"VWSH", "Vertical speed shear", "1/s", UC_NONE},
/* 26 */    {"MFLX", "Horizontal momentum flux", "N/(m^2)", UC_NONE},
/* 27 */    {"USTM", "U-component storm motion", "m/s", UC_NONE},
/* 28 */    {"VSTM", "V-component storm motion", "m/s", UC_NONE},
/* 29 */    {"CD", "Drag coefficient", "-", UC_NONE},
/* 30 */    {"FRICV", "Frictional velocity", "m/s", UC_NONE},
};

/* GRIB2 Code table 4.2 : 0.3 */
GRIB2ParmTable MeteoMass[] = {
   /* 0 */ {"PRES", "Pressure", "Pa", UC_NONE},
   /* 1 */ {"PRMSL", "Pressure reduced to MSL", "Pa", UC_NONE},
   /* 2 */ {"PTEND", "Pressure tendency", "Pa/s", UC_NONE},
   /* 3 */ {"ICAHT", "ICAO Standard Atmosphere Reference Height", "m", UC_NONE},
   /* 4 */ {"GP", "Geopotential", "(m^2)/(s^2)", UC_NONE},
   /* 5 */ {"HGT", "Geopotential height", "gpm", UC_NONE},
   /* 6 */ {"DIST", "Geometric Height", "m", UC_NONE},
   /* 7 */ {"HSTDV", "Standard deviation of height", "m", UC_NONE},
   /* 8 */ {"PRESA", "Pressure anomaly", "Pa", UC_NONE},
   /* 9 */ {"GPA", "Geopotential height anomally", "gpm", UC_NONE},
   /* 10 */ {"DEN", "Density", "kg/(m^3)", UC_NONE},
   /* 11 */ {"", "Altimeter setting", "Pa", UC_NONE},
   /* 12 */ {"", "Thickness", "m", UC_NONE},
   /* 13 */ {"", "Pressure altitude", "m", UC_NONE},
   /* 14 */ {"", "Density altitude", "m", UC_NONE},
/* 15 */    {"5WAVH", "5-wave geopotential height", "gpm", UC_NONE},
/* 16 */    {"U-GWD", "Zonal flux of gravity wave stress", "N/(m^2)", UC_NONE},
/* 17 */    {"V-GWD", "Meridional flux of gravity wave stress", "N/(m^2)", UC_NONE},
/* 18 */    {"HPBL", "Planetary boundary layer height", "m", UC_NONE},
/* 19 */    {"5WAVA", "5-Wave geopotential height anomaly", "gpm", UC_NONE},
};

/* GRIB2 Code table 4.2 : 0.4 */
GRIB2ParmTable MeteoShortRadiate[] = {
   /* 0 */ {"NSWRS", "Net short-wave radiation flux (surface)", "W/(m^2)", UC_NONE},
   /* 1 */ {"NSWRT", "Net short-wave radiation flux (top of atmosphere)",
            "W/(m^2)", UC_NONE},
   /* 2 */ {"SWAVR", "Short wave radiation flux", "W/(m^2)", UC_NONE},
   /* 3 */ {"GRAD", "Global radiation flux", "W/(m^2)", UC_NONE},
   /* 4 */ {"BRTMP", "Brightness temperature", "K", UC_NONE},
   /* 5 */ {"LWRAD", "Radiance (with respect to wave number)", "W/(m sr)", UC_NONE},
   /* 6 */ {"SWRAD", "Radiance (with respect to wave length)", "W/(m^3 sr)", UC_NONE},
/* 7 */    {"DSWRF", "Downward short-wave radiation flux", "W/(m^2)", UC_NONE},
/* 8 */    {"USWRF", "Upward short-wave radiation flux", "W/(m^2)", UC_NONE},
};

/* GRIB2 Code table 4.2 : 0.5 */
GRIB2ParmTable MeteoLongRadiate[] = {
   /* 0 */ {"NLWRS", "Net long wave radiation flux (surface)", "W/(m^2)", UC_NONE},
   /* 1 */ {"NLWRT", "Net long wave radiation flux (top of atmosphere)",
            "W/(m^2)", UC_NONE},
   /* 2 */ {"LWAVR", "Long wave radiation flux", "W/(m^2)", UC_NONE},
/* 3 */    {"DLWRF", "Downward Long-Wave Rad. Flux", "W/(m^2)", UC_NONE},
/* 4 */    {"ULWRF", "Upward Long-Wave Rad. Flux", "W/(m^2)", UC_NONE},
};

/* GRIB2 Code table 4.2 : 0.6 */
GRIB2ParmTable MeteoCloud[] = {
   /* 0 */ {"CICE", "Cloud Ice", "kg/(m^2)", UC_NONE},
   /* 1 */ {"TCDC", "Total cloud cover", "%", UC_NONE}, /* Need NDFD override Sky */
   /* 2 */ {"CDCON", "Convective cloud cover", "%", UC_NONE},
   /* 3 */ {"LCDC", "Low cloud cover", "%", UC_NONE},
   /* 4 */ {"MCDC", "Medium cloud cover", "%", UC_NONE},
   /* 5 */ {"HCDC", "High cloud cover", "%", UC_NONE},
   /* 6 */ {"CWAT", "Cloud water", "kg/(m^2)", UC_NONE},
   /* 7 */ {"", "Cloud amount", "%", UC_NONE},
   /* 8 */ {"", "Cloud type", "(0 clear, 1 Cumulonimbus, 2 Stratus, "
            "3 Stratocumulus, 4 Cumulus, 5 Altostratus, 6 Nimbostratus, "
            "7 Altocumulus, 8 Cirrostratus, 9 Cirrocumulus, 10 Cirrus, "
            "11 Cumulonimbus (fog), 12 Stratus (fog), 13 Stratocumulus (fog),"
            " 14 Cumulus (fog), 15 Altostratus (fog), 16 Nimbostratus (fog), "
            "17 Altocumulus (fog), 18 Cirrostratus (fog), "
            "19 Cirrocumulus (fog), 20 Cirrus (fog), 191 unknown, "
            "255 missing)", UC_NONE},
   /* 9 */ {"", "Thunderstorm maximum tops", "m", UC_NONE},
   /* 10 */ {"", "Thunderstorm coverage", "(0 none, 1 isolated (1%-2%), "
             "2 few (3%-15%), 3 scattered (16%-45%), 4 numerous (> 45%), "
             "255 missing)", UC_NONE},
   /* 11 */ {"", "Cloud base", "m", UC_NONE},
   /* 12 */ {"", "Cloud top", "m", UC_NONE},
   /* 13 */ {"", "Ceiling", "m", UC_NONE},
/* 14 */    {"CDLYR", "Non-convective cloud cover", "%", UC_NONE},
/* 15 */    {"CWORK", "Cloud work function", "J/kg", UC_NONE},
/* 16 */    {"CUEFI", "Convective cloud efficiency", "-", UC_NONE},
/* 17 */    {"TCOND", "Total condensate", "kg/kg", UC_NONE},
/* 18 */    {"TCOLW", "Total column-integrated cloud water", "kg/(m^2)", UC_NONE},
/* 19 */    {"TCOLI", "Total column-integrated cloud ice", "kg/(m^2)", UC_NONE},
/* 20 */    {"TCOLC", "Total column-integrated condensate", "kg/(m^2)", UC_NONE},
/* 21 */    {"FICE", "Ice fraction of total condensate", "-", UC_NONE},
};

/* GRIB2 Code table 4.2 : 0.7 */
/* NCEP capitalized items 6, 7, 8 */
GRIB2ParmTable MeteoStability[] = {
   /* 0 */ {"PLI", "Parcel lifted index (to 500 hPa)", "K", UC_NONE},
   /* 1 */ {"BLI", "Best lifted index (to 500 hPa)", "K", UC_NONE},
   /* 2 */ {"KX", "K index", "K", UC_NONE},
   /* 3 */ {"", "KO index", "K", UC_NONE},
   /* 4 */ {"", "Total totals index", "K", UC_NONE},
   /* 5 */ {"SX", "Sweat index", "numeric", UC_NONE},
   /* 6 */ {"CAPE", "Convective available potential energy", "J/kg", UC_NONE},
   /* 7 */ {"CIN", "Convective inhibition", "J/kg", UC_NONE},
   /* 8 */ {"HLCY", "Storm relative helicity", "J/kg", UC_NONE},
   /* 9 */ {"", "Energy helicity index", "numeric", UC_NONE},
/* 10 */   {"LFTX", "Surface fifted index", "K", UC_NONE},
/* 11 */   {"4LFTX", "Best (4-layer) lifted index", "K", UC_NONE},
/* 12 */   {"RI", "Richardson number", "-", UC_NONE},
};

/* GRIB2 Code table 4.2 : 0.13 */
GRIB2ParmTable MeteoAerosols[] = {
   /* 0 */ {"", "Aerosol type", "(0 Aerosol not present, 1 Aerosol present, "
            "255 missing)", UC_NONE},
};

/* GRIB2 Code table 4.2 : 0.14 */
GRIB2ParmTable MeteoGases[] = {
   /* 0 */ {"TOZNE", "Total ozone", "Dobson", UC_NONE},
/* 1 */    {"O3MR", "Ozone Mixing Ratio", "kg/kg", UC_NONE},
};

/* GRIB2 Code table 4.2 : 0.15 */
GRIB2ParmTable MeteoRadar[] = {
   /* 0 */ {"", "Base spectrum width", "m/s", UC_NONE},
   /* 1 */ {"", "Base reflectivity", "dB", UC_NONE},
   /* 2 */ {"", "Base radial velocity", "m/s", UC_NONE},
   /* 3 */ {"", "Vertically-integrated liquid", "kg/m", UC_NONE},
   /* 4 */ {"", "Layer-maximum base reflectivity", "dB", UC_NONE},
   /* 5 */ {"", "Precipitation", "kg/(m^2)", UC_NONE},
   /* 6 */ {"RDSP1", "Radar spectra (1)", "-", UC_NONE},
   /* 7 */ {"RDSP2", "Radar spectra (2)", "-", UC_NONE},
   /* 8 */ {"RDSP3", "Radar spectra (3)", "-", UC_NONE},
};

/* GRIB2 Code table 4.2 : 0.18 */
GRIB2ParmTable MeteoNuclear[] = {
   /* 0 */ {"", "Air concentration of Caesium 137", "Bq/(m^3)", UC_NONE},
   /* 1 */ {"", "Air concentration of Iodine 131", "Bq/(m^3)", UC_NONE},
   /* 2 */ {"", "Air concentration of radioactive pollutant", "Bq/(m^3)", UC_NONE},
   /* 3 */ {"", "Ground deposition of Caesium 137", "Bq/(m^2)", UC_NONE},
   /* 4 */ {"", "Ground deposition of Iodine 131", "Bq/(m^2)", UC_NONE},
   /* 5 */ {"", "Ground deposition of radioactive pollutant", "Bq/(m^2)", UC_NONE},
   /* 6 */ {"", "Time-integrated air concentration of caesium pollutant",
            "(Bq s)/(m^3)", UC_NONE},
   /* 7 */ {"", "Time-integrated air concentration of iodine pollutant",
            "(Bq s)/(m^3)", UC_NONE},
   /* 8 */ {"", "Time-integrated air concentration of radioactive pollutant",
            "(Bq s)/(m^3)", UC_NONE},
};

/* GRIB2 Code table 4.2 : 0.19 */
/* NCEP capitalized items 11 */
GRIB2ParmTable MeteoAtmos[] = {
   /* 0 */ {"VIS", "Visibility", "m", UC_NONE},
   /* 1 */ {"ALBDO", "Albedo", "%", UC_NONE},
   /* 2 */ {"TSTM", "Thunderstorm probability", "%", UC_NONE},
   /* 3 */ {"MIXHT", "mixed layer depth", "m", UC_NONE},
   /* 4 */ {"", "Volcanic ash", "(0 not present, 1 present, 255 missing)", UC_NONE},
   /* 5 */ {"", "Icing top", "m", UC_NONE},
   /* 6 */ {"", "Icing base", "m", UC_NONE},
   /* 7 */ {"", "Icing", "(0 None, 1 Light, 2 Moderate, 3 Severe, "
            "255 missing)", UC_NONE},
   /* 8 */ {"", "Turbulance top", "m", UC_NONE},
   /* 9 */ {"", "Turbulence base", "m", UC_NONE},
   /* 10 */ {"", "Turbulance", "(0 None(smooth), 1 Light, 2 Moderate, "
             "3 Severe, 4 Extreme, 255 missing)", UC_NONE},
   /* 11 */ {"TKE", "Turbulent Kinetic Energy", "J/kg", UC_NONE},
   /* 12 */ {"", "Planetary boundary layer regime", "(0 Reserved, 1 Stable, "
             "2 Mechanically driven turbulence, 3 Forced convection, "
             "4 Free convection, 255 missing)", UC_NONE},
   /* 13 */ {"", "Contrail intensity", "(0 Contrail not present, "
             "1 Contrail present, 255 missing)", UC_NONE},
   /* 14 */ {"", "Contrail engine type", "(0 Low bypass, 1 High bypass, "
             "2 Non bypass, 255 missing)", UC_NONE},
   /* 15 */ {"", "Contrail top", "m", UC_NONE},
   /* 16 */ {"", "Contrail base", "m", UC_NONE},
/* 17 */    {"MXSALB", "Maximum snow albedo", "%", UC_NONE},
/* 18 */    {"SNFALB", "Snow free albedo", "%", UC_NONE},
};

/* GRIB2 Code table 4.2 : 0.253 or 0.190 (Document is inconsistant.) */
GRIB2ParmTable MeteoText[] = {
   /* 0 */ {"", "Arbitrary text string", "CCITTIA5", UC_NONE},
};

GRIB2ParmTable MeteoMisc[] = {
   /* 0 */ {"TSEC", "Seconds prior to initial reference time (defined in Section"
            " 1)", "s", UC_NONE},
};

/* GRIB2 Code table 4.2 : 1.0 */
GRIB2ParmTable HydroBasic[] = {
   /* 0 */ {"", "Flash flood guidance", "kg/(m^2)", UC_NONE},
   /* 1 */ {"", "Flash flood runoff", "kg/(m^2)", UC_NONE},
   /* 2 */ {"", "Remotely sensed snow cover", "(50 no-snow/no-cloud, "
            "100 Clouds, 250 Snow, 255 missing)", UC_NONE},
   /* 3 */ {"", "Elevation of snow covered terrain", "(0-90 elevation in "
            "increments of 100m, 254 clouds, 255 missing)", UC_NONE},
   /* 4 */ {"", "Snow water equivalent percent of normal", "%", UC_NONE},
/* 5 */    {"BGRUN", "Baseflow-groundwater runoff", "kg/(m^2)", UC_NONE},
/* 6 */    {"SSRUN", "Storm surface runoff", "kg/(m^2)", UC_NONE},
};

/* GRIB2 Code table 4.2 : 1.1 */
GRIB2ParmTable HydroProb[] = {
   /* 0 */ {"", "Conditional percent precipitation amount fractile for an "
            "overall period", "kg/(m^2)", UC_NONE},
   /* 1 */ {"", "Percent precipitation in a sub-period of an overall period",
            "%", UC_NONE},
   /* 2 */ {"PoP", "Probability of 0.01 inch of precipitation", "%", UC_NONE},
};

/* GRIB2 Code table 4.2 : 2.0 */
GRIB2ParmTable LandVeg[] = {
   /* 0 */ {"LAND", "Land cover (1=land; 2=sea)", "Proportion", UC_NONE},
   /* 1 */ {"SFCR", "Surface roughness", "m", UC_NONE}, /*NCEP override SFRC? */
   /* 2 */ {"TSOIL", "Soil temperature", "K", UC_NONE},
   /* 3 */ {"SOILM", "Soil moisture content", "kg/(m^2)", UC_NONE},
   /* 4 */ {"VEG", "Vegetation", "%", UC_NONE},
   /* 5 */ {"WATR", "Water runoff", "kg/(m^2)", UC_NONE},
   /* 6 */ {"", "Evapotranspiration", "1/(kg^2 s)", UC_NONE},
   /* 7 */ {"", "Model terrain height", "m", UC_NONE},
   /* 8 */ {"", "Land use", "(1 Urban land, 2 agriculture, 3 Range Land, "
            "4 Deciduous forest, 5 Coniferous forest, 6 Forest/wetland, "
            "7 Water, 8 Wetlands, 9 Desert, 10 Tundra, 11 Ice, "
            "12 Tropical forest, 13 Savannah)", UC_NONE},
/* 9 */    {"SOILW", "Volumetric soil moisture content", "fraction", UC_NONE},
/* 10 */   {"GFLUX", "Ground heat flux", "W/(m^2)", UC_NONE},
/* 11 */   {"MSTAV", "Moisture availability", "%", UC_NONE},
/* 12 */   {"SFEXC", "Exchange coefficient", "(kg/(m^3))(m/s)", UC_NONE},
/* 13 */   {"CNWAT", "Plant canopy surface water", "kg/(m^2)", UC_NONE},
/* 14 */   {"BMIXL", "Blackadar's mixing length scale", "m", UC_NONE},
/* 15 */   {"CCOND", "Canopy conductance", "m/s", UC_NONE},
/* 16 */   {"RSMIN", "Minimal stomatal resistance", "s/m", UC_NONE},
/* 17 */   {"WILT", "Wilting point", "fraction", UC_NONE},
/* 18 */   {"RCS", "Solar parameter in canopy conductance", "fraction", UC_NONE},
/* 19 */   {"RCT", "Temperature parameter in canopy conductance", "fraction", UC_NONE},
/* 20 */   {"RCSOL", "Soil moisture parameter in canopy conductance", "fraction", UC_NONE},
/* 21 */   {"RCQ", "Humidity parameter in canopy conductance", "fraction", UC_NONE},
};

/* GRIB2 Code table 4.2 : 2.3 */
/* NCEP changed 0 to be "Soil type (as in Zobler)" I ignored them */
GRIB2ParmTable LandSoil[] = {
   /* 0 */ {"SOTYP", "Soil type", "(1 Sand, 2 Loamy sand, 3 Sandy loam, "
            "4 Silt loam, 5 Organic (redefined), 6 Sandy clay loam, "
            "7 Silt clay loam, 8 Clay loam, 9 Sandy clay, 10 Silty clay, "
            "11 Clay)", UC_NONE},
   /* 1 */ {"", "Upper layer soil temperature", "K", UC_NONE},
   /* 2 */ {"", "Upper layer soil moisture", "kg/(m^3)", UC_NONE},
   /* 3 */ {"", "Lower layer soil moisture", "kg/(m^3)", UC_NONE},
   /* 4 */ {"", "Bottom layer soil temperature", "K", UC_NONE},
/* 5 */ {"SOILL", "Liquid volumetric soil moisture (non-frozen)", "fraction", UC_NONE},
/* 6 */ {"RLYRS", "Number of soil layers in root zone", "-", UC_NONE},
/* 7 */ {"SMREF", "Transpiration stress-onset (soil moisture)", "fraction", UC_NONE},
/* 8 */ {"SMDRY", "Direct evaporation cease (soil moisture)", "fraction", UC_NONE},
/* 9 */ {"POROS", "Soil porosity", "fraction", UC_NONE},
};

/* GRIB2 Code table 4.2 : 3.0 */
GRIB2ParmTable SpaceImage[] = {
   /* 0 */ {"", "Scaled radiance", "numeric", UC_NONE},
   /* 1 */ {"", "Scaled albedo", "numeric", UC_NONE},
   /* 2 */ {"", "Scaled brightness temperature", "numeric", UC_NONE},
   /* 3 */ {"", "Scaled precipitable water", "numeric", UC_NONE},
   /* 4 */ {"", "Scaled lifted index", "numeric", UC_NONE},
   /* 5 */ {"", "Scaled cloud top pressure", "numeric", UC_NONE},
   /* 6 */ {"", "Scaled skin temperature", "numeric", UC_NONE},
   /* 7 */ {"", "Cloud mask", "(0 clear over water, 1 clear over land, "
            "2 cloud)", UC_NONE},
/* 8 */ {"", "Pixel scene type", "(0 No scene, 1 needle, 2 broad-leafed, "
         "3 Deciduous needle, 4 Deciduous broad-leafed, 5 Deciduous mixed, "
         "6 Closed shrub, 7 Open shrub, 8 Woody savannah, 9 Savannah, "
         "10 Grassland, 11 wetland, 12 Cropland, 13 Urban, 14 crops, "
         "15 snow, 16 Desert, 17 Water, 18 Tundra, 97 Snow on land, "
         "98 Snow on water, 99 Sun-glint, 100 General cloud, "
         "101 (fog, Stratus), 102 Stratocumulus, 103 Low cloud, "
         "104 Nimbotratus, 105 Altostratus, 106 Medium cloud, 107 Cumulus, "
         "108 Cirrus, 109 High cloud, 110 Unknown cloud)", UC_NONE},
};

/* GRIB2 Code table 4.2 : 3.1 */
GRIB2ParmTable SpaceQuantitative[] = {
   /* 0 */ {"", "Estimated precipitation", "kg/(m^2)", UC_NONE},
/* 1 */ {"", "Instantaneous rain rate", "kg/(m^2*s)", UC_NONE},
/* 2 */ {"", "Cloud top height", "kg/(m^2*s)", UC_NONE},
/* 3 */ {"", "Cloud top height quality indicator", "(0 Nominal cloud top "
         "height quality, 1 Fog in segment, 2 Poor quality height estimation "
         "3 Fog in segment and poor quality height estimation)", UC_NONE},
/* 4 */ {"", "Estimated u component of wind", "m/s", UC_NONE},
/* 5 */ {"", "Estimated v component of wind", "m/s", UC_NONE},
};

/* GRIB2 Code table 4.2 : 10.0 */
GRIB2ParmTable OceanWaves[] = {
   /* 0 */ {"WVSP1", "Wave spectra (1)", "-", UC_NONE},
   /* 1 */ {"WVSP2", "Wave spectra (2)", "-", UC_NONE},
   /* 2 */ {"WVSP3", "Wave spectra (3)", "-", UC_NONE},
   /* 3 */ {"HTSGW", "Significant height of combined wind waves and swell", "m", UC_NONE},
   /* 4 */ {"WVDIR", "Direction of wind waves", "Degree true", UC_NONE},
   /* 5 */ {"WVHGT", "Significant height of wind waves", "m", UC_M2Feet}, /* NDFD override needed WaveHeight */
   /* 6 */ {"WVPER", "Mean period of wind waves", "s", UC_NONE},
   /* 7 */ {"SWDIR", "Direction of swell waves", "Degree true", UC_NONE},
   /* 8 */ {"SWELL", "Significant height of swell waves", "m", UC_NONE},
   /* 9 */ {"SWPER", "Mean period of swell waves", "s", UC_NONE},
   /* 10 */ {"DIRPW", "Primary wave direction", "Degree true", UC_NONE},
   /* 11 */ {"PERPW", "Primary wave mean period", "s", UC_NONE},
   /* 12 */ {"DIRSW", "Secondary wave direction", "Degree true", UC_NONE},
   /* 13 */ {"PERSW", "Secondary wave mean period", "s", UC_NONE},
};

/* GRIB2 Code table 4.2 : 10.1 */
GRIB2ParmTable OceanCurrents[] = {
   /* 0 */ {"DIRC", "Current direction", "Degree true", UC_NONE},
   /* 1 */ {"SPC", "Current speed", "m/s", UC_NONE},
   /* 2 */ {"UOGRD", "u-component of current", "m/s", UC_NONE},
   /* 3 */ {"VOGRD", "v-component of current", "m/s", UC_NONE},
};

/* GRIB2 Code table 4.2 : 10.2 */
GRIB2ParmTable OceanIce[] = {
   /* 0 */ {"ICEC", "Ice cover", "Proportion", UC_NONE},
   /* 1 */ {"ICETK", "Ice thinkness", "m", UC_NONE},
   /* 2 */ {"DICED", "Direction of ice drift", "Degree true", UC_NONE},
   /* 3 */ {"SICED", "Speed of ice drift", "m/s", UC_NONE},
   /* 4 */ {"UICE", "u-component of ice drift", "m/s", UC_NONE},
   /* 5 */ {"VICE", "v-component of ice drift", "m/s", UC_NONE},
   /* 6 */ {"ICEG", "Ice growth rate", "m/s", UC_NONE},
   /* 7 */ {"ICED", "Ice divergence", "1/s", UC_NONE},
};

/* GRIB2 Code table 4.2 : 10.3 */
GRIB2ParmTable OceanSurface[] = {
   /* 0 */ {"WTMP", "Water temperature", "K", UC_NONE},
   /* 1 */ {"DSLM", "Deviation of sea level from mean", "m", UC_NONE},
};

/* GRIB2 Code table 4.2 : 10.4 */
GRIB2ParmTable OceanSubSurface[] = {
   /* 0 */ {"MTHD", "Main thermocline depth", "m", UC_NONE},
   /* 1 */ {"MTHA", "Main thermocline anomaly", "m", UC_NONE},
   /* 2 */ {"TTHDP", "Transient thermocline depth", "m", UC_NONE},
   /* 3 */ {"SALTY", "Salinity", "kg/kg", UC_NONE},
};

/* *INDENT-ON* */

/*****************************************************************************
 * Choose_GRIB2ParmTable() --
 *
 * Arthur Taylor / MDL
 *
 * PURPOSE
 *   Chooses the correct Parameter table depending on what is in the GRIB2
 * message's "Product Definition Section".
 *
 * ARGUMENTS
 * prodType = The product type (meteo, hydro, land, space, ocean, etc) (In)
 *      cat = The category inside the product (Input)
 * tableLen = The length of the returned table (Output)
 *
 * FILES/DATABASES: None
 *
 * RETURNS: ParmTable (appropriate parameter table.)
 *
 * HISTORY
 *   1/2004 Arthur Taylor (MDL/RSIS): Created
 *
 * NOTES
 *****************************************************************************
 */
static GRIB2ParmTable *Choose_GRIB2ParmTable (int prodType, int cat,
                                              size_t *tableLen)
{
   enum { METEO_TEMP = 0, METEO_MOIST = 1, METEO_MOMENT = 2, METEO_MASS = 3,
      METEO_SW_RAD = 4, METEO_LW_RAD = 5, METEO_CLOUD = 6,
      METEO_THERMO_INDEX = 7, METEO_KINEMATIC_INDEX = 8, METEO_TEMP_PROB = 9,
      METEO_MOISTURE_PROB = 10, METEO_MOMENT_PROB = 11, METEO_MASS_PROB = 12,
      METEO_AEROSOL = 13, METEO_GAS = 14, METEO_RADAR = 15,
      METEO_RADAR_IMAGERY = 16, METEO_ELECTRO = 17, METEO_NUCLEAR = 18,
      METEO_ATMOS = 19, METEO_CCITT = 190, METEO_MISC = 191,
      METEO_CCITT2 = 253
   };
   enum { HYDRO_BASIC = 0, HYDRO_PROB = 1 };
   enum { LAND_VEG = 0, LAND_SOIL = 3 };
   enum { SPACE_IMAGE = 0, SPACE_QUANTIT = 1 };
   enum { OCEAN_WAVES = 0, OCEAN_CURRENTS = 1, OCEAN_ICE = 2, OCEAN_SURF = 3,
      OCEAN_SUBSURF = 4
   };

   switch (prodType) {
      case 0:          /* Meteo type. */
         switch (cat) {
            case METEO_TEMP:
               *tableLen = sizeof (MeteoTemp) / sizeof (GRIB2ParmTable);
               return &MeteoTemp[0];
            case METEO_MOIST:
               *tableLen = sizeof (MeteoMoist) / sizeof (GRIB2ParmTable);
               return &MeteoMoist[0];
            case METEO_MOMENT:
               *tableLen = sizeof (MeteoMoment) / sizeof (GRIB2ParmTable);
               return &MeteoMoment[0];
            case METEO_MASS:
               *tableLen = sizeof (MeteoMass) / sizeof (GRIB2ParmTable);
               return &MeteoMass[0];
            case METEO_SW_RAD:
               *tableLen = (sizeof (MeteoShortRadiate) /
                            sizeof (GRIB2ParmTable));
               return &MeteoShortRadiate[0];
            case METEO_LW_RAD:
               *tableLen = (sizeof (MeteoLongRadiate) /
                            sizeof (GRIB2ParmTable));
               return &MeteoLongRadiate[0];
            case METEO_CLOUD:
               *tableLen = sizeof (MeteoCloud) / sizeof (GRIB2ParmTable);
               return &MeteoCloud[0];
            case METEO_THERMO_INDEX:
               *tableLen = sizeof (MeteoStability) / sizeof (GRIB2ParmTable);
               return &MeteoStability[0];
            case METEO_KINEMATIC_INDEX:
            case METEO_TEMP_PROB:
            case METEO_MOISTURE_PROB:
            case METEO_MOMENT_PROB:
            case METEO_MASS_PROB:
               *tableLen = 0;
               return NULL;
            case METEO_AEROSOL:
               *tableLen = sizeof (MeteoAerosols) / sizeof (GRIB2ParmTable);
               return &MeteoAerosols[0];
            case METEO_GAS:
               *tableLen = sizeof (MeteoGases) / sizeof (GRIB2ParmTable);
               return &MeteoGases[0];
            case METEO_RADAR:
               *tableLen = sizeof (MeteoRadar) / sizeof (GRIB2ParmTable);
               return &MeteoRadar[0];
            case METEO_RADAR_IMAGERY:
            case METEO_ELECTRO:
               *tableLen = 0;
               return NULL;
            case METEO_NUCLEAR:
               *tableLen = sizeof (MeteoNuclear) / sizeof (GRIB2ParmTable);
               return &MeteoNuclear[0];
            case METEO_ATMOS:
               *tableLen = sizeof (MeteoAtmos) / sizeof (GRIB2ParmTable);
               return &MeteoAtmos[0];
            case METEO_CCITT:
            case METEO_CCITT2:
               *tableLen = sizeof (MeteoText) / sizeof (GRIB2ParmTable);
               return &MeteoText[0];
            case METEO_MISC:
               *tableLen = sizeof (MeteoMisc) / sizeof (GRIB2ParmTable);
               return &MeteoMisc[0];
            default:
               *tableLen = 0;
               return NULL;
         }
      case 1:          /* Hydro type. */
         switch (cat) {
            case HYDRO_BASIC:
               *tableLen = sizeof (HydroBasic) / sizeof (GRIB2ParmTable);
               return &HydroBasic[0];
            case HYDRO_PROB:
               *tableLen = sizeof (HydroProb) / sizeof (GRIB2ParmTable);
               return &HydroProb[0];
            default:
               *tableLen = 0;
               return NULL;
         }
      case 2:          /* Land type. */
         switch (cat) {
            case LAND_VEG:
               *tableLen = sizeof (LandVeg) / sizeof (GRIB2ParmTable);
               return &LandVeg[0];
            case LAND_SOIL:
               *tableLen = sizeof (LandSoil) / sizeof (GRIB2ParmTable);
               return &LandSoil[0];
            default:
               *tableLen = 0;
               return NULL;
         }
      case 3:          /* Space type. */
         switch (cat) {
            case SPACE_IMAGE:
               *tableLen = sizeof (SpaceImage) / sizeof (GRIB2ParmTable);
               return &SpaceImage[0];
            case SPACE_QUANTIT:
               *tableLen = (sizeof (SpaceQuantitative) /
                            sizeof (GRIB2ParmTable));
               return &SpaceQuantitative[0];
            default:
               *tableLen = 0;
               return NULL;
         }
      case 10:         /* ocean type. */
         switch (cat) {
            case OCEAN_WAVES:
               *tableLen = sizeof (OceanWaves) / sizeof (GRIB2ParmTable);
               return &OceanWaves[0];
            case OCEAN_CURRENTS:
               *tableLen = sizeof (OceanCurrents) / sizeof (GRIB2ParmTable);
               return &OceanCurrents[0];
            case OCEAN_ICE:
               *tableLen = sizeof (OceanIce) / sizeof (GRIB2ParmTable);
               return &OceanIce[0];
            case OCEAN_SURF:
               *tableLen = sizeof (OceanSurface) / sizeof (GRIB2ParmTable);
               return &OceanSurface[0];
            default:
               *tableLen = 0;
               return NULL;
         }
      default:
         *tableLen = 0;
         return NULL;
   }
}

/* *INDENT-OFF* */
NDFD_AbrevOverideTable NDFD_Overide[] = {
   /*  0 */ {"TMP", "T"},
   /*  1 */ {"TMAX", "MaxT"},
   /*  2 */ {"TMIN", "MinT"},
   /*  3 */ {"DPT", "Td"},
   /*  4 */ {"APCP", "QPF"},
   /* Don't need SNOD for now. */
   /*  5 */ /* {"SNOD", "SnowDepth"}, */
   /*  6 */ {"WDIR", "WindDir"},
   /*  7 */ {"WIND", "WindSpd"},
   /*  8 */ {"TCDC", "Sky"},
   /*  9 */ {"WVHGT", "WaveHeight"},
   /* 10 */ {"ASNOW", "SnowAmt"},
   /* 11 */ {"GUST", "WindGust"},
};

GRIB2LocalTable NDFD_LclTable[] = {
   /* 0 */ {0, 1, 192, "Wx", "Weather string", "-", UC_NONE},
   /* 1 */ {0, 0, 193, "ApparentT", "Apparent Temperature", "K", UC_K2F},
   /* 2 */ {0, 14, 192, "O3MR", "Ozone Mixing Ratio", "kg/kg", UC_NONE},
   /* 3 */ {0, 14, 193, "OZCON", "Ozone Concentration", "PPB", UC_NONE},
   /* grandfather'ed in a NDFD choice for POP. */
   /* 4 */ {0, 10, 8, "PoP12", "Prob of 0.01 In. of Precip", "%", UC_NONE},
           {0, 13, 194, "smokes", "Surface level smoke from fires",
            "log10(µg/m^3)", UC_LOG10},
           {0, 13, 195, "smokec", "Average vertical column smoke from fires",
            "log10(µg/m^3)", UC_LOG10},
   /* Arthur Added this to both NDFD and NCEP local tables. (5/1/2006) */
           {10, 3, 192, "Surge", "Hurricane Storm Surge", "m", UC_M2Feet},
           {10, 3, 193, "ETSurge", "Extra Tropical Storm Surge", "m", UC_M2Feet},
};

GRIB2LocalTable HPC_LclTable[] = {
   /* 0 */ {0, 1, 192, "HPC-Wx", "HPC Code", "-", UC_NONE},
};

/*
Updated this table last on 12/29/2005
Based on:
http://www.nco.ncep.noaa.gov/pmb/docs/grib2/GRIB2_parmeter_conversion_table.html
Better source is:
http://www.nco.ncep.noaa.gov/pmb/docs/grib2/grib2_table4-1.shtml
For abreviations see:
http://www.nco.ncep.noaa.gov/pmb/docs/on388/table2.html

Updated again on 2/14/2006
Updated again on 3/15/2006
*/
GRIB2LocalTable NCEP_LclTable[] = {
   /*  0 */ {0, 0, 192, "SNOHF", "Snow Phase Change Heat Flux", "W/(m^2)", UC_NONE},
            {0, 0, 193, "TTRAD", "Temperature tendency by all radiation", "K/s", UC_NONE},

   /*  1 */ {0, 1, 192, "CRAIN", "Categorical Rain", "(0 no; 1 yes)", UC_NONE},
   /*  2 */ {0, 1, 193, "CFRZR", "Categorical Freezing Rain", "(0 no; 1 yes)", UC_NONE},
   /*  3 */ {0, 1, 194, "CICEP", "Categorical Ice Pellets", "(0 no; 1 yes)", UC_NONE},
   /*  4 */ {0, 1, 195, "CSNOW", "Categorical Snow", "(0 no; 1 yes)", UC_NONE},
   /*  5 */ {0, 1, 196, "CPRAT", "Convective Precipitation Rate", "kg/(m^2*s)", UC_NONE},
   /*  6 */ {0, 1, 197, "MCONV", "Horizontal Moisture Divergence", "kg/(kg*s)", UC_NONE},
/* Following was grandfathered in... Should use: 1, 1, 193 */
   /*  7 */ {0, 1, 198, "CPOFP", "Percent Frozen Precipitation", "%", UC_NONE},
   /*  8 */ {0, 1, 199, "PEVAP", "Potential Evaporation", "kg/(m^2)", UC_NONE},
   /*  9 */ {0, 1, 200, "PEVPR", "Potential Evaporation Rate", "W/(m^2)", UC_NONE},
   /* 10 */ {0, 1, 201, "SNOWC", "Snow Cover", "%", UC_NONE},
   /* 11 */ {0, 1, 202, "FRAIN", "Rain Fraction of Total Liquid Water", "-", UC_NONE},
/* FRIME -> RIME 12/29/2005 */
   /* 12 */ {0, 1, 203, "RIME", "Rime Factor", "-", UC_NONE},
   /* 13 */ {0, 1, 204, "TCOLR", "Total Column Integrated Rain", "kg/(m^2)", UC_NONE},
   /* 14 */ {0, 1, 205, "TCOLS", "Total Column Integrated Snow", "kg/(m^2)", UC_NONE},
            {0, 1, 206, "TIPD", "Total Icing Potential Diagnostic", "-", UC_NONE},
            {0, 1, 207, "NCIP", "Number concentration for ice particles", "-", UC_NONE},
            {0, 1, 208, "SNOT", "Snow temperature", "K", UC_NONE},

   /* 15 */ {0, 2, 192, "VWSH", "Vertical speed sheer", "1/s", UC_NONE},
   /* 16 */ {0, 2, 193, "MFLX", "Horizontal Momentum Flux", "N/(m^2)", UC_NONE},
   /* 17 */ {0, 2, 194, "USTM", "U-Component Storm Motion", "m/s", UC_NONE},
   /* 18 */ {0, 2, 195, "VSTM", "V-Component Storm Motion", "m/s", UC_NONE},
   /* 19 */ {0, 2, 196, "CD", "Drag Coefficient", "-", UC_NONE},
   /* 20 */ {0, 2, 197, "FRICV", "Frictional Velocity", "m/s", UC_NONE},

   /* 21 */ {0, 3, 192, "MSLET", "Mean Sea Level Pressure (Eta Reduction)", "Pa", UC_NONE},
   /* 22 */ {0, 3, 193, "5WAVH", "5-Wave Geopotential Height", "gpm", UC_NONE},
   /* 23 */ {0, 3, 194, "U-GWD", "Zonal Flux of Gravity Wave Stress", "N/(m^2)", UC_NONE},
   /* 24 */ {0, 3, 195, "V-GWD", "Meridional Flux of Gravity Wave Stress", "N/(m^2)", UC_NONE},
   /* 25 */ {0, 3, 196, "HPBL", "Planetary Boundary Layer Height", "m", UC_NONE},
   /* 26 */ {0, 3, 197, "5WAVA", "5-Wave Geopotential Height Anomaly", "gpm", UC_NONE},
            {0, 3, 198, "MSLMA", "Mean Sea Level Pressure (MAPS System Reduction)", "Pa", UC_NONE},
            {0, 3, 199, "TSLSA", "3-hr pressure tendency (Std. Atmos. Reduction)", "Pa/s", UC_NONE},
            {0, 3, 200, "PLPL", "Pressure of level from which parcel was lifted", "Pa", UC_NONE},

   /* 27 */ {0, 4, 192, "DSWRF", "Downward Short-Wave Rad. Flux", "W/(m^2)", UC_NONE},
   /* 28 */ {0, 4, 193, "USWRF", "Upward Short-Wave Rad. Flux", "W/(m^2)", UC_NONE},
            {0, 4, 194, "DUVB", "UV-B downward solar flux", "W/(m^2)", UC_NONE},
            {0, 4, 195, "CDUVB", "Clear sky UV-B downward solar flux", "W/(m^2)", UC_NONE},

   /* 29 */ {0, 5, 192, "DLWRF", "Downward Long-Wave Rad. Flux", "W/(m^2)", UC_NONE},
   /* 30 */ {0, 5, 193, "ULWRF", "Upward Long-Wave Rad. Flux", "W/(m^2)", UC_NONE},

   /* 31 */ {0, 6, 192, "CDLYR", "Non-Convective Cloud Cover", "%", UC_NONE},
   /* 32 */ {0, 6, 193, "CWORK", "Cloud Work Function", "J/kg", UC_NONE},
   /* 33 */ {0, 6, 194, "CUEFI", "Convective Cloud Efficiency", "-", UC_NONE},
   /* 34 */ {0, 6, 195, "TCOND", "Total Condensate", "kg/kg", UC_NONE},
   /* 35 */ {0, 6, 196, "TCOLW", "Total Column-Integrated Cloud Water", "kg/(m^2)", UC_NONE},
   /* 36 */ {0, 6, 197, "TCOLI", "Total Column-Integrated Cloud Ice", "kg/(m^2)", UC_NONE},
   /* 37 */ {0, 6, 198, "TCOLC", "Total Column-Integrated Condensate", "kg/(m^2)", UC_NONE},
   /* 38 */ {0, 6, 199, "FICE", "Ice fraction of total condensate", "-", UC_NONE},

   /* 39 */ {0, 7, 192, "LFTX", "Surface Lifted Index", "K", UC_NONE},
   /* 40 */ {0, 7, 193, "4LFTX", "Best (4 layer) Lifted Index", "K", UC_NONE},
   /* 41 */ {0, 7, 194, "RI", "Richardson Number", "-", UC_NONE},

            {0, 13, 192, "PMTC", "Particulate matter (coarse)", "µg/m^3", UC_NONE},
            {0, 13, 193, "PMTF", "Particulate matter (fine)", "µg/m^3", UC_NONE},
            {0, 13, 194, "LPMTF", "Particulate matter (fine)",
             "log10(µg/m^3)", UC_LOG10},
            {0, 13, 195, "LIPMF", "Integrated column particulate matter "
             "(fine)", "log10(µg/m^3)", UC_LOG10},

   /* 42 */ {0, 14, 192, "O3MR", "Ozone Mixing Ratio", "kg/kg", UC_NONE},
   /* 43 */ {0, 14, 193, "OZCON", "Ozone Concentration", "PPB", UC_NONE},
   /* 44 */ {0, 14, 194, "OZCAT", "Categorical Ozone Concentration", "-", UC_NONE},

            {0, 16, 192, "REFZR", "Derived radar reflectivity backscatter from rain", "mm^6/m^3", UC_NONE},
            {0, 16, 193, "REFZI", "Derived radar reflectivity backscatter from ice", "mm^6/m^3", UC_NONE},
            {0, 16, 194, "REFZC", "Derived radar reflectivity backscatter from parameterized convection", "mm^6/m^3", UC_NONE},
            {0, 16, 195, "REFD", "Derived radar reflectivity", "dB", UC_NONE},
            {0, 16, 196, "REFC", "Maximum / Composite radar reflectivity", "dB", UC_NONE},

            {0, 17, 192, "LTNG", "Lightning", "-", UC_NONE},

   /* 45 */ {0, 19, 192, "MXSALB", "Maximum Snow Albedo", "%", UC_NONE},
   /* 46 */ {0, 19, 193, "SNFALB", "Snow-Free Albedo", "%", UC_NONE},
            {0, 19, 194, "", "Slight risk convective outlook", "categorical", UC_NONE},
            {0, 19, 195, "", "Moderate risk convective outlook", "categorical", UC_NONE},
            {0, 19, 196, "", "High risk convective outlook", "categorical", UC_NONE},
            {0, 19, 197, "", "Tornado probability", "%", UC_NONE},
            {0, 19, 198, "", "Hail probability", "%", UC_NONE},
            {0, 19, 199, "", "Wind probability", "%", UC_NONE},
            {0, 19, 200, "", "Significant Tornado probability", "%", UC_NONE},
            {0, 19, 201, "", "Significant Hail probability", "%", UC_NONE},
            {0, 19, 202, "", "Significant Wind probability", "%", UC_NONE},
            {0, 19, 203, "TSTMC", "Categorical Thunderstorm", "0=no, 1=yes", UC_NONE},
            {0, 19, 204, "MIXLY", "Number of mixed layers next to surface", "integer", UC_NONE},

   /* 47 */ {0, 191, 192, "NLAT", "Latitude (-90 to 90)", "deg", UC_NONE},
   /* 48 */ {0, 191, 193, "ELON", "East Longitude (0 to 360)", "deg", UC_NONE},
   /* 49 */ {0, 191, 194, "TSEC", "Seconds prior to initial reference time", "s", UC_NONE},

   /* 50 */ {1, 0, 192, "BGRUN", "Baseflow-Groundwater Runoff", "kg/(m^2)", UC_NONE},
   /* 51 */ {1, 0, 193, "SSRUN", "Storm Surface Runoff", "kg/(m^2)", UC_NONE},

            {1, 1, 192, "CPOZP", "Probability of Freezing Precipitation", "%", UC_NONE},
            {1, 1, 193, "CPOFP", "Probability of Frozen Precipitation", "%", UC_NONE},
            {1, 1, 194, "PPFFG", "Probability of precipitation exceeding flash flood guidance values", "%", UC_NONE},

   /* 52 */ {2, 0, 192, "SOILW", "Volumetric Soil Moisture Content", "Fraction", UC_NONE},
   /* 53 */ {2, 0, 193, "GFLUX", "Ground Heat Flux", "W/(m^2)", UC_NONE},
   /* 54 */ {2, 0, 194, "MSTAV", "Moisture Availability", "%", UC_NONE},
   /* 55 */ {2, 0, 195, "SFEXC", "Exchange Coefficient", "(kg/(m^3))(m/s)", UC_NONE},
   /* 56 */ {2, 0, 196, "CNWAT", "Plant Canopy Surface Water", "kg/(m^2)", UC_NONE},
   /* 57 */ {2, 0, 197, "BMIXL", "Blackadar's Mixing Length Scale", "m", UC_NONE},
   /* 58 */ {2, 0, 198, "VGTYP", "Vegetation Type", "0..13", UC_NONE},
   /* 59 */ {2, 0, 199, "CCOND", "Canopy Conductance", "m/s", UC_NONE},
   /* 60 */ {2, 0, 200, "RSMIN", "Minimal Stomatal Resistance", "s/m", UC_NONE},
   /* 61 */ {2, 0, 201, "WILT", "Wilting Point", "Fraction", UC_NONE},
   /* 62 */ {2, 0, 202, "RCS", "Solar parameter in canopy conductance", "Fraction", UC_NONE},
   /* 63 */ {2, 0, 203, "RCT", "Temperature parameter in canopy conductance", "Fraction", UC_NONE},
   /* 64 */ {2, 0, 204, "RCQ", "Humidity parameter in canopy conductance", "Fraction", UC_NONE},
   /* 65 */ {2, 0, 205, "RCSOL", "Soil moisture parameter in canopy conductance", "Fraction", UC_NONE},
            {2, 0, 206, "RDRIP", "Rate of water dropping from canopy to ground", "unknown", UC_NONE},
            {2, 0, 207, "ICWAT", "Ice-free water surface", "%", UC_NONE},

   /* 66 */ {2, 3, 192, "SOILL", "Liquid Volumetric Soil Moisture (non Frozen)", "Proportion", UC_NONE},
   /* 67 */ {2, 3, 193, "RLYRS", "Number of Soil Layers in Root Zone", "-", UC_NONE},
   /* 68 */ {2, 3, 194, "SLTYP", "Surface Slope Type", "Index", UC_NONE},
   /* 69 */ {2, 3, 195, "SMREF", "Transpiration Stress-onset (soil moisture)", "Proportion", UC_NONE},
   /* 70 */ {2, 3, 196, "SMDRY", "Direct Evaporation Cease (soil moisture)", "Proportion", UC_NONE},
   /* 71 */ {2, 3, 197, "POROS", "Soil Porosity", "Proportion", UC_NONE},

/* ScatEstUWind -> USCT, ScatEstVWind -> VSCT as of 7/5/2006 (pre 1.80) */
   /* 72 */ {3, 1, 192, "USCT", "Scatterometer Estimated U Wind", "m/s", UC_NONE},
   /* 73 */ {3, 1, 193, "VSCT", "Scatterometer Estimated V Wind", "m/s", UC_NONE},

   /* Arthur Added this to both NDFD and NCEP local tables. (5/1/2006) */
           {10, 3, 192, "SURGE", "Hurricane Storm Surge", "m", UC_M2Feet},
           {10, 3, 193, "ETSRG", "Extra Tropical Storm Surge", "m", UC_M2Feet},
};
/* *INDENT-ON* */

int IsData_NDFD (unsigned short int center, unsigned short int subcenter)
{
   return ((center == 8) &&
           ((subcenter == GRIB2MISSING_u2) || (subcenter == 0)));
}

int IsData_MOS (unsigned short int center, unsigned short int subcenter)
{
   return ((center == 7) && (subcenter == 14));
}

/*****************************************************************************
 * Choose_LocalParmTable() --
 *
 * Arthur Taylor / MDL
 *
 * PURPOSE
 *   Chooses the local parameter table for a given center/subcenter.
 * Typically this is called after the default Choose_ParmTable was tried,
 * since it consists of all the local specs, and one has to linearly walk
 * through the table.
 *
 * ARGUMENTS
 *    center = The center that created the data. (Input)
 * subcenter = The subcenter that created the data. (Input)
 *  tableLen = The length of the returned table (Output)
 *
 * FILES/DATABASES: None
 *
 * RETURNS: LocalParmTable (appropriate parameter table.)
 *
 * HISTORY
 *   1/2004 Arthur Taylor (MDL/RSIS): Created
 *
 * NOTES
 *****************************************************************************
 */
static GRIB2LocalTable *Choose_LocalParmTable (unsigned short int center,
                                               unsigned short int subcenter,
                                               size_t *tableLen)
{
   switch (center) {
      case 7:          /* NWS NCEP */
         /* Check if all subcenters of NCEP use the same local table. */
/*
         *tableLen = sizeof (NCEP_LclTable) / sizeof (GRIB2LocalTable);
         return &NCEP_LclTable[0];
*/
         switch (subcenter) {
            case 5:    /* Have HPC use NDFD table. */
               *tableLen = sizeof (HPC_LclTable) / sizeof (GRIB2LocalTable);
               return &HPC_LclTable[0];
            default:
               *tableLen = sizeof (NCEP_LclTable) / sizeof (GRIB2LocalTable);
               return &NCEP_LclTable[0];
/*
               *tableLen = 0;
               return NULL;
*/
         }
      case 8:          /* NWS Telecomunications gateway */
         switch (subcenter) {
            case GRIB2MISSING_u2: /* NDFD */
            case 0:    /* NDFD */
               *tableLen = sizeof (NDFD_LclTable) / sizeof (GRIB2LocalTable);
               return &NDFD_LclTable[0];
            default:
               *tableLen = 0;
               return NULL;
         }
      default:
         *tableLen = 0;
         return NULL;
   }
}

/*****************************************************************************
 * ParseElemName() --
 *
 * Arthur Taylor / MDL
 *
 * PURPOSE
 *   Converts a prodType, template, category and subcategory quadruple to the
 * ASCII string abreviation of that variable.
 *   For example: 0, 0, 0, 0, = "T" for temperature.
 *
 * ARGUMENTS
 *    center = The center that created the data. (Input)
 * subcenter = The subcenter that created the data. (Input)
 *  prodType = The GRIB2, section 0 product type. (Input)
 *   templat = The GRIB2 section 4 template number. (Input)
 *       cat = The GRIB2 section 4 "General category of Product." (Input)
 *    subcat = The GRIB2 section 4 "Specific subcategory of Product". (Input)
 *   lenTime = The length of time over which statistics are done
 *             (see template 4.8). (Input)
 *     genID = The Generating process ID (used for GFS MOS) (Input)
 *  probType = For Probability templates (Input)
 * lowerProb = Lower Limit for probability templates. (Input)
 * upperProb = Upper Limit for probability templates. (Input)
 *      name = Short name for the data set (T, MaxT, etc) (Output)
 *   comment = Long form of the name (Temperature, etc) (Output)
 *      unit = What unit this variable is originally in (Output)
 *
 * FILES/DATABASES: None
 *
 * RETURNS: void
 *
 * HISTORY
 *   1/2004 Arthur Taylor (MDL/RSIS): Re-Created.
 *   6/2004 AAT: Added deltTime (because of Ozone issues).
 *   8/2004 AAT: Adjusted so template 9 gets units of % and no convert.
 *   3/2005 AAT: ReWrote to handle template 5, 9 and MOS.
 *   9/2005 AAT: Added code to handle MOS PoP06 vs MOS PoP12.
 *
 * NOTES
 *****************************************************************************
 */
/* Deal with probability templates 2/16/2006 */
static void ElemNameProb (uShort2 center, uShort2 subcenter, int prodType,
                          CPL_UNUSED int templat,
                          uChar cat, uChar subcat, sInt4 lenTime,
                          uChar timeIncrType,
                          CPL_UNUSED uChar genID,
                          uChar probType,
                          double lowerProb, double upperProb, char **name,
                          char **comment, char **unit, int *convert)
{
   GRIB2ParmTable *table;
   GRIB2LocalTable *local;
   size_t tableLen;
   size_t i;
   char f_isNdfd = IsData_NDFD (center, subcenter);
   char f_isMos = IsData_MOS (center, subcenter);

   *unit = (char *) malloc (strlen ("[%]") + 1);
   strcpy (*unit, "[%]");

   if (f_isNdfd || f_isMos) {
      /* Deal with NDFD/MOS handling of Prob Precip_Tot -> PoP12 */
      if ((prodType == 0) && (cat == 1) && (subcat == 8)) {
         myAssert (probType == 1);
         if (lenTime > 0) {
            mallocSprintf (name, "PoP%02d", lenTime);
            mallocSprintf (comment, "%02d hr Prob of Precip > 0.01 "
                           "In. [%%]", lenTime);
         } else {
            *name = (char *) malloc (strlen ("PoP") + 1);
            strcpy (*name, "PoP");
            *comment =
                  (char *) malloc (strlen ("Prob of Precip > 0.01 In. [%]") +
                                   1);
            strcpy (*comment, "Prob of Precip > 0.01 In. [%]");
         }
         *convert = UC_NONE;
         return;
      }
      /* 
       * Deal with NDFD handling of Prob. Wind speeds.
       * There are different solutions for naming the Prob. Wind fields
       * AAT(Mine): ProbSurge5c
       */
      if ((prodType == 10) && (cat == 3) && (subcat == 192)) {
         myAssert (probType == 1);
         myAssert (lenTime > 0);
         if (timeIncrType == 2) {
            /* Incremental */
            mallocSprintf (name, "ProbSurge%02di",
                           (int) ((upperProb / 0.3048) + .5));
         } else {
            /* Cumulative */
            myAssert (timeIncrType == 192);
            mallocSprintf (name, "ProbSurge%02dc",
                           (int) ((upperProb / 0.3048) + .5));
         }
         mallocSprintf (comment, "%02d hr Prob of Hurricane Storm Surge > %g "
                        "m [%%]", lenTime, upperProb);
         *convert = UC_NONE;
         return;
      }
   }
   if (f_isNdfd) {
      /* 
       * Deal with NDFD handling of Prob. Wind speeds.
       * There are different solutions for naming the Prob. Wind fields
       * Tim Boyer: TCWindSpdIncr34 TCWindSpdIncr50 TCWindSpdIncr64
       *            TCWindSpdCumu34 TCWindSpdCumu50 TCWindSpdCumu64
       * Dave Ruth: tcwspdabv34i tcwspdabv50i tcwspdabv64i
       *            tcwspdabv34c tcwspdabv50c tcwspdabv64c
       * AAT(Mine): ProbWindSpd34c ProbWindSpd50c ProbWindSpd64c
       *            ProbWindSpd34i ProbWindSpd50i ProbWindSpd64i
       */
      if ((prodType == 0) && (cat == 2) && (subcat == 1)) {
         myAssert (probType == 1);
         myAssert (lenTime > 0);
         if (timeIncrType == 2) {
            /* Incremental */
            mallocSprintf (name, "ProbWindSpd%02di",
                           (int) ((upperProb * 3600. / 1852.) + .5));
         } else {
            /* Cumulative */
            myAssert (timeIncrType == 192);
            mallocSprintf (name, "ProbWindSpd%02dc",
                           (int) ((upperProb * 3600. / 1852.) + .5));
         }
         mallocSprintf (comment, "%02d hr Prob of Wind speed > %g m/s [%%]",
                        lenTime, upperProb);
         *convert = UC_NONE;
         return;
      }
   }

   /* Generic tables. */
   table = Choose_GRIB2ParmTable (prodType, cat, &tableLen);
   if (table != NULL) {
      if (subcat < tableLen) {
         /* Check for NDFD over-rides. */
         /* The NDFD over-rides for probability templates have already been
          * handled. */
         if (lenTime > 0) {
            mallocSprintf (name, "Prob%s%02d", table[subcat].name, lenTime);
            mallocSprintf (comment, "%02d hr Prob of %s ", lenTime,
                           table[subcat].comment);
         } else {
            mallocSprintf (name, "Prob%s", table[subcat].name);
            mallocSprintf (comment, "Prob of %s ", table[subcat].comment);
         }
         if (probType == 0) {
            reallocSprintf (comment, "< %g %s [%%]", lowerProb,
                            table[subcat].unit);
         } else if (probType == 1) {
            reallocSprintf (comment, "> %g %s [%%]", upperProb,
                            table[subcat].unit);
         } else if (probType == 2) {
            reallocSprintf (comment, ">= %g, < %g %s [%%]", lowerProb,
                            upperProb, table[subcat].unit);
         } else if (probType == 3) {
            reallocSprintf (comment, "> %g %s [%%]", lowerProb,
                            table[subcat].unit);
         } else if (probType == 4) {
            reallocSprintf (comment, "< %g %s [%%]", upperProb,
                            table[subcat].unit);
         } else {
            reallocSprintf (comment, "%s [%%]", table[subcat].unit);
         }
         *convert = UC_NONE;
         return;
      }
   }

   /* Local use tables. */
   local = Choose_LocalParmTable (center, subcenter, &tableLen);
   if (local != NULL) {
      for (i = 0; i < tableLen; i++) {
         if ((prodType == local[i].prodType) && (cat == local[i].cat) &&
             (subcat == local[i].subcat)) {
            if (lenTime > 0) {
               mallocSprintf (name, "Prob%s%02d", local[i].name, lenTime);
               mallocSprintf (comment, "%02d hr Prob of %s ", lenTime,
                              local[i].comment);
            } else {
               mallocSprintf (name, "Prob%s", local[i].name);
               mallocSprintf (comment, "Prob of %s ", local[i].comment);
            }
            if (probType == 0) {
               reallocSprintf (comment, "< %g %s [%%]", lowerProb,
                               local[i].unit);
            } else if (probType == 1) {
               reallocSprintf (comment, "> %g %s [%%]", upperProb,
                               local[i].unit);
            } else if (probType == 2) {
               reallocSprintf (comment, ">= %g, < %g %s [%%]", lowerProb,
                               upperProb, local[i].unit);
            } else if (probType == 3) {
               reallocSprintf (comment, "> %g %s [%%]", lowerProb,
                               local[i].unit);
            } else if (probType == 4) {
               reallocSprintf (comment, "< %g %s [%%]", upperProb,
                               local[i].unit);
            } else {
               reallocSprintf (comment, "%s [%%]", local[i].unit);
            }
            *convert = UC_NONE;
            return;
         }
      }
   }

   *name = (char *) malloc (strlen ("ProbUnknown") + 1);
   strcpy (*name, "ProbUnknown");
   mallocSprintf (comment, "Prob of (prodType %d, cat %d, subcat %d) [-]",
                  prodType, cat, subcat);
   *convert = UC_NONE;
   return;
}

/* Deal with percentile templates 5/1/2006 */
static void ElemNamePerc (uShort2 center, uShort2 subcenter, int prodType,
                          CPL_UNUSED int templat,
                          uChar cat, uChar subcat, sInt4 lenTime,
                          sChar percentile, char **name, char **comment,
                          char **unit, int *convert)
{
   GRIB2ParmTable *table;
   GRIB2LocalTable *local;
   size_t tableLen;
   size_t i;

   /* Generic tables. */
   table = Choose_GRIB2ParmTable (prodType, cat, &tableLen);
   if (table != NULL) {
      if (subcat < tableLen) {
         /* Check for NDFD over-rides. */
         if (IsData_NDFD (center, subcenter) ||
             IsData_MOS (center, subcenter)) {
            for (i = 0; i < (sizeof (NDFD_Overide) /
                             sizeof (NDFD_AbrevOverideTable)); i++) {
               if (strcmp (NDFD_Overide[i].GRIB2name, table[subcat].name) ==
                   0) {
                  mallocSprintf (name, "%s%02d", NDFD_Overide[i].NDFDname,
                                 percentile);
                  if (lenTime > 0) {
                     mallocSprintf (comment, "%02d hr %s Percentile(%d) [%s]",
                                    lenTime, table[subcat].comment,
                                    percentile, table[subcat].unit);
                  } else {
                     mallocSprintf (comment, "%s Percentile(%d) [%s]",
                                    table[subcat].comment, percentile,
                                    table[subcat].unit);
                  }
                  mallocSprintf (unit, "[%s]", table[subcat].unit);
                  *convert = table[subcat].convert;
                  return;
               }
            }
         }
         mallocSprintf (name, "%s%02d", table[subcat].name, percentile);
         if (lenTime > 0) {
            mallocSprintf (comment, "%02d hr %s Percentile(%d) [%s]",
                           lenTime, table[subcat].comment, percentile,
                           table[subcat].unit);
         } else {
            mallocSprintf (comment, "%s Percentile(%d) [%s]",
                           table[subcat].comment, percentile,
                           table[subcat].unit);
         }
         mallocSprintf (unit, "[%s]", table[subcat].unit);
         *convert = table[subcat].convert;
         return;
      }
   }

   /* Local use tables. */
   local = Choose_LocalParmTable (center, subcenter, &tableLen);
   if (local != NULL) {
      for (i = 0; i < tableLen; i++) {
         if ((prodType == local[i].prodType) && (cat == local[i].cat) &&
             (subcat == local[i].subcat)) {
            mallocSprintf (name, "%s%02d", local[i].name, percentile);
            if (lenTime > 0) {
               mallocSprintf (comment, "%02d hr %s Percentile(%d) [%s]",
                              lenTime, local[i].comment, percentile,
                              local[i].unit);
            } else {
               mallocSprintf (comment, "%s Percentile(%d) [%s]",
                              local[i].comment, percentile, local[i].unit);
            }
            mallocSprintf (unit, "[%s]", local[i].unit);
            *convert = local[i].convert;
            return;
         }
      }
   }

   *name = (char *) malloc (strlen ("unknown") + 1);
   strcpy (*name, "unknown");
   mallocSprintf (comment, "(prodType %d, cat %d, subcat %d) [-]", prodType,
                  cat, subcat);
   *unit = (char *) malloc (strlen ("[-]") + 1);
   strcpy (*unit, "[-]");
   *convert = UC_NONE;
   return;
}

/* Deal with non-prob templates 2/16/2006 */
static void ElemNameNorm (uShort2 center, uShort2 subcenter, int prodType,
                          int templat, uChar cat, uChar subcat, sInt4 lenTime,
                          CPL_UNUSED uChar timeIncrType,
                          CPL_UNUSED uChar genID,
                          CPL_UNUSED uChar probType,
                          CPL_UNUSED double lowerProb,
                          CPL_UNUSED double upperProb,
                          char **name,
                          char **comment, char **unit, int *convert)
{
   GRIB2ParmTable *table;
   GRIB2LocalTable *local;
   size_t tableLen;
   size_t i;
   sChar f_accum;

   /* Check for over-ride case for ozone.  Originally just for NDFD, but I
    * think it is useful for ozone data that originated elsewhere. */
   if ((prodType == 0) && (templat == 8) && (cat == 14) && (subcat == 193)) {
      if (lenTime > 0) {
         mallocSprintf (name, "Ozone%02d", lenTime);
         mallocSprintf (comment, "%d hr Average Ozone Concentration "
                        "[PPB]", lenTime);
      } else {
         *name = (char *) malloc (strlen ("AVGOZCON") + 1);
         strcpy (*name, "AVGOZCON");
         *comment =
               (char *) malloc (strlen ("Average Ozone Concentration [PPB]") +
                                1);
         strcpy (*comment, "Average Ozone Concentration [PPB]");
      }
      *unit = (char *) malloc (strlen ("[PPB]") + 1);
      strcpy (*unit, "[PPB]");
      *convert = UC_NONE;
      return;
   }

   /* Generic tables. */
   table = Choose_GRIB2ParmTable (prodType, cat, &tableLen);
   if (table != NULL) {
      if (subcat < tableLen) {
         /* Check for NDFD over-rides. */
         if (IsData_NDFD (center, subcenter) ||
             IsData_MOS (center, subcenter)) {
            for (i = 0; i < (sizeof (NDFD_Overide) /
                             sizeof (NDFD_AbrevOverideTable)); i++) {
               if (strcmp (NDFD_Overide[i].GRIB2name, table[subcat].name) ==
                   0) {
                  *name =
                        (char *) malloc (strlen (NDFD_Overide[i].NDFDname) +
                                         1);
                  strcpy (*name, NDFD_Overide[i].NDFDname);
                  mallocSprintf (comment, "%s [%s]", table[subcat].comment,
                                 table[subcat].unit);
                  mallocSprintf (unit, "[%s]", table[subcat].unit);
                  *convert = table[subcat].convert;
                  return;
               }
            }
         }
         /* Allow hydrologic PoP, thunderstorm probability (TSTM), or APCP to 
          * have lenTime labels. */
         f_accum = (((prodType == 1) && (cat == 1) && (subcat == 2)) ||
                    ((prodType == 0) && (cat == 19) && (subcat == 2)) ||
                    ((prodType == 0) && (cat == 1) && (subcat == 8)) ||
                    ((prodType == 0) && (cat == 19) && (subcat == 203)));
         if (f_accum && (lenTime > 0)) {
            mallocSprintf (name, "%s%02d", table[subcat].name, lenTime);
            mallocSprintf (comment, "%02d hr %s [%s]", lenTime,
                           table[subcat].comment, table[subcat].unit);
         } else {
            *name = (char *) malloc (strlen (table[subcat].name) + 1);
            strcpy (*name, table[subcat].name);
            mallocSprintf (comment, "%s [%s]", table[subcat].comment,
                           table[subcat].unit);
         }
         mallocSprintf (unit, "[%s]", table[subcat].unit);
         *convert = table[subcat].convert;
         return;
      }
   }

   /* Local use tables. */
   local = Choose_LocalParmTable (center, subcenter, &tableLen);
   if (local != NULL) {
      for (i = 0; i < tableLen; i++) {
         if ((prodType == local[i].prodType) && (cat == local[i].cat) &&
             (subcat == local[i].subcat)) {
            /* Allow specific products with non-zero lenTime to reflect that. 
             */
            f_accum = 0;
            if (f_accum && (lenTime > 0)) {
               mallocSprintf (name, "%s%02d", local[i].name, lenTime);
               mallocSprintf (comment, "%02d hr %s [%s]", lenTime,
                              local[i].comment, local[i].unit);
            } else {
               *name = (char *) malloc (strlen (local[i].name) + 1);
               strcpy (*name, local[i].name);
               mallocSprintf (comment, "%s [%s]", local[i].comment,
                              local[i].unit);
            }
            mallocSprintf (unit, "[%s]", local[i].unit);
            *convert = local[i].convert;
            return;
         }
      }
   }

   *name = (char *) malloc (strlen ("unknown") + 1);
   strcpy (*name, "unknown");
   mallocSprintf (comment, "(prodType %d, cat %d, subcat %d) [-]", prodType,
                  cat, subcat);
   *unit = (char *) malloc (strlen ("[-]") + 1);
   strcpy (*unit, "[-]");
   *convert = UC_NONE;
   return;
}

void ParseElemName (uShort2 center, uShort2 subcenter, int prodType,
                    int templat, int cat, int subcat, sInt4 lenTime,
                    uChar timeIncrType, uChar genID, uChar probType,
                    double lowerProb, double upperProb, char **name,
                    char **comment, char **unit, int *convert,
                    sChar percentile)
{
   myAssert (*name == NULL);
   myAssert (*comment == NULL);
   myAssert (*unit == NULL);

   /* Check if this is Probability data */
   if ((templat == GS4_PROBABIL_TIME) || (templat == GS4_PROBABIL_PNT)) {
      ElemNameProb (center, subcenter, prodType, templat, cat, subcat,
                    lenTime, timeIncrType, genID, probType, lowerProb,
                    upperProb, name, comment, unit, convert);
   } else if (templat == GS4_PERCENTILE) {
      ElemNamePerc (center, subcenter, prodType, templat, cat, subcat,
                    lenTime, percentile, name, comment, unit, convert);
   } else {
      ElemNameNorm (center, subcenter, prodType, templat, cat, subcat,
                    lenTime, timeIncrType, genID, probType, lowerProb,
                    upperProb, name, comment, unit, convert);
   }
}

/*****************************************************************************
 * ParseElemName2() -- Review 12/2002
 *
 * Arthur Taylor / MDL
 *
 * PURPOSE
 *   Converts a prodType, template, category and subcategory quadruple to the
 * ASCII string abreviation of that variable.
 *   For example: 0, 0, 0, 0, = "T" for temperature.
 *
 * ARGUMENTS
 * prodType = The GRIB2, section 0 product type. (Input)
 *  templat = The GRIB2 section 4 template number. (Input)
 *      cat = The GRIB2 section 4 "General category of Product." (Input)
 *   subcat = The GRIB2 section 4 "Specific subcategory of Product". (Input)
 *     name = Where to store the result (assumed already allocated to at
 *            least 15 bytes) (Output)
 *  comment = Extra info about variable (assumed already allocated to at
 *            least 100 bytes) (Output)
 *     unit = What unit this variable is in. (assumed already allocated to at
 *            least 20 bytes) (Output)
 *
 * FILES/DATABASES: None
 *
 * RETURNS: char *
 *   Same as 'strcpy', ie it returns name.
 *
 * HISTORY
 *   9/2002 Arthur Taylor (MDL/RSIS): Created.
 *  11/2002 AAT: Added MOIST_TOT_SNOW (and switched MOIST_SNOWAMT to
 *               SnowDepth)
 *  12/2002 (TK,AC,TB,&MS): Code Review.
 *   2/2003 AAT: moved from degrib.c to metaparse.c
 *              (Reason: primarily for Sect2 Parsing)
 *              (renamed from ElementName to ParseElemName)
 *   4/2003 AAT: Added the comment as a return element.(see GRIB2 discipline)
 *   6/2003 AAT: Added the unit as a return element.
 *   6/2003 AAT: Added Wave Height.
 *
 * NOTES
 *   Similar to GRIB1_Table2LookUp... May want to take this and the unit
 * stuff and combine them into a module.
 *****************************************************************************
 */
/*
static void ParseElemName2 (int prodType, int templat, int cat, int subcat,
                            char *name, char *comment, char *unit)
{
   if (prodType == 0) {
      if (cat == CAT_TEMP) { * 0 *
         switch (subcat) {
            case TEMP_TEMP: * 0 *
               strcpy (comment, "Temperature [K]");
               strcpy (name, "T");
               strcpy (unit, "[K]");
               return;
            case TEMP_MAXT: * 4 *
               strcpy (comment, "Maximum temperature [K]");
               strcpy (name, "MaxT");
               strcpy (unit, "[K]");
               return;
            case TEMP_MINT: * 5 *
               strcpy (comment, "Minimum temperature [K]");
               strcpy (name, "MinT");
               strcpy (unit, "[K]");
               return;
            case TEMP_DEW_TEMP: * 6 *
               strcpy (comment, "Dew point temperature [K]");
               strcpy (name, "Td");
               strcpy (unit, "[K]");
               return;
            case TEMP_WINDCHILL: * 13 *
               strcpy (comment, "Wind chill factor [K]");
               strcpy (name, "WCI");
               strcpy (unit, "[K]");
               return;
            case TEMP_HEAT: * 12 *
               strcpy (comment, "Heat index [K]");
               strcpy (name, "HeatIndex");
               strcpy (unit, "[K]");
               return;
         }
      } else if (cat == CAT_MOIST) { * 1 *
         switch (subcat) {
            case MOIST_REL_HUMID: * 1 *
               strcpy (comment, "Relative Humidity [%]");
               strcpy (name, "RH");
               strcpy (unit, "[%]");
               return;
            case MOIST_PRECIP_TOT: * 8 *
               if (templat == GS4_PROBABIL_TIME) { * template number 9 implies prob. *
                  strcpy (comment, "Prob of 0.01 In. of Precip [%]");
                  strcpy (name, "PoP12");
                  strcpy (unit, "[%]");
                  return;
               } else {
                  strcpy (comment, "Total precipitation [kg/(m^2)]");
                  strcpy (name, "QPF");
                  strcpy (unit, "[kg/(m^2)]");
                  return;
               }
            case MOIST_SNOWAMT: * 11 *
               strcpy (comment, "Snow Depth [m]");
               strcpy (name, "SnowDepth");
               strcpy (unit, "[m]");
               return;
            case MOIST_TOT_SNOW: * 29 *
               strcpy (comment, "Total snowfall [m]");
               strcpy (name, "SnowAmt");
               strcpy (unit, "[m]");
               return;
            case 192:  * local use moisture. *
               strcpy (comment, "Weather (local use moisture) [-]");
               strcpy (name, "Wx");
               strcpy (unit, "[-]");
               return;
         }
      } else if (cat == CAT_MOMENT) { * 2 *
         switch (subcat) {
            case MOMENT_WINDDIR: * 0 *
               strcpy (comment, "Wind direction (from which blowing) "
                       "[deg true]");
               strcpy (name, "WindDir");
               strcpy (unit, "[deg true]");
               return;
            case MOMENT_WINDSPD: * 1 *
               strcpy (comment, "Wind speed [m/s]");
               strcpy (name, "WindSpd");
               strcpy (unit, "[m/s]");
               return;
         }
      } else if (cat == CAT_CLOUD) { * 6 *
         switch (subcat) {
            case CLOUD_COVER: * 1 *
               strcpy (comment, "Total cloud cover [%]");
               strcpy (name, "Sky");
               strcpy (unit, "[%]");
               return;
         }
      } else if (cat == CAT_MOISTURE_PROB) { * 10 *
         if (subcat == 8) { * grandfather'ed in. *
            strcpy (comment, "Prob of 0.01 In. of Precip [%]");
            strcpy (name, "PoP12");
            strcpy (unit, "[%]");
            return;
         }
      }
   } else if (prodType == 10) {
      if (cat == OCEAN_CAT_WAVES) { * 0 *
         if (subcat == OCEAN_WAVE_SIG_HT_WV) { * 5 *
            strcpy (comment, "Significant height of wind waves [m]");
            strcpy (name, "WaveHeight");
            strcpy (unit, "[m]");
            return;
         }
      }
   }
   strcpy (name, "");
   strcpy (comment, "unknown");
   strcpy (unit, "[-]");
   return;
}
*/

/*****************************************************************************
 * ComputeUnit() --
 *
 * Arthur Taylor / MDL
 *
 * PURPOSE
 *   Sets m, and b for equation y = mx + b, where x is in the unit
 * specified by GRIB2, and y is the one specified by f_unit.  The default
 * is m = 1, b = 0.
 *
 * Currently:
 *   For f_unit = 1 (english) we return Fahrenheit, knots, and inches for
 * temperature, wind speed, and amount of snow or rain.  The original units
 * are Kelvin, m/s, kg/m**2.
 *   For f_unit = 2 (metric) we return Celsius instead of Kelvin.
 *
 * ARGUMENTS
 *  convert = The enumerated type describing the type of conversion. (Input)
 * origName = Original unit name (needed for log10 option) (Input)
 *   f_unit = What type of unit to return (see above) (Input).
 *    unitM = M in equation y = m x + b (Output)
 *    unitB = B in equation y = m x + b (Output)
 *     name = Where to store the result (assumed already allocated to at
 *           least 15 bytes) (Output)
 *
 * FILES/DATABASES: None
 *
 * RETURNS: int
 *   0 if we set M and B, 1 if we used defaults.
 *
 * HISTORY
 *   1/2004 Arthur Taylor (MDL/RSIS): Re-Created.
 *
 * NOTES
 *****************************************************************************
 */
int ComputeUnit (int convert, char *origName, sChar f_unit, double *unitM,
                 double *unitB, char *name)
{
   switch (convert) {
      case UC_NONE:
         break;
      case UC_K2F:     /* Convert from Kelvin to F or C. */
         if (f_unit == 1) {
            strcpy (name, "[F]");
            *unitM = 9. / 5.;
            /* 32 - (9/5 * 273.15) = 32 - 491.67 = -459.67. */
            *unitB = -459.67;
            return 0;
         } else if (f_unit == 2) {
            strcpy (name, "[C]");
            *unitM = 1;
            *unitB = -273.15;
            return 0;
         }
         break;
      case UC_InchWater: /* Convert from kg/(m^2) to inches water. */
         if (f_unit == 1) {
            strcpy (name, "[inch]");
            /* 
             * kg/m**2 / density of water (1000 kg/m**3)
             * 1/1000 m * 1/2.54 in/cm * 100 cm/m = 1/25.4 inches
             */
            *unitM = 1. / 25.4;
            *unitB = 0;
            return 0;
         }
         break;
      case UC_M2Feet:  /* Convert from meters to feet. */
         if (f_unit == 1) {
            /* 1 (m) * (100cm/m) * (inch/2.54cm) * (ft/12inch) = X (ft) */
            strcpy (name, "[feet]");
            *unitM = 100. / 30.48;
            *unitB = 0;
            return 0;
         }
         break;
      case UC_M2Inch:  /* Convert from meters to inches. */
         if (f_unit == 1) {
            strcpy (name, "[inch]");
            *unitM = 100. / 2.54; /* inch / m */
            *unitB = 0;
            return 0;
         }
         break;
         /* NCEP goes with a convention of 1 nm = 1853.248 m.
          * http://www.sizes.com/units/mile_USnautical.htm Shows that on
          * 7/1/1954 US Department of Commerce switched to 1 nm = 1852 m
          * (International standard.) */
      case UC_MS2Knots: /* Convert from m/s to knots. */
         if (f_unit == 1) {
            strcpy (name, "[knots]");
            *unitM = 3600. / 1852.; /* knot / m s**-1 */
            *unitB = 0;
            return 0;
         }
         break;
      case UC_LOG10:   /* convert from log10 (x) to x */
         if ((f_unit == 1) || (f_unit == 2)) {
            origName[strlen (origName) - 2] = '\0';
            if (strlen (origName) > 21)
               origName[21] = '\0';
            sprintf (name, "[%s]", origName + 7);
            *unitM = -10; /* M = -10 => take 10^(x) */
            *unitB = 0;
            return 0;
         }
         break;
   }
   /* Default case is for the unit in the GRIB2 document. */
   strcpy (name, "[GRIB2 unit]");
   *unitM = 1;
   *unitB = 0;
   return 1;
}

/*****************************************************************************
 * ComputeUnit2() --
 *
 * Arthur Taylor / MDL
 *
 * PURPOSE
 *   Sets m, and b for equation y = mx + b, where x is in the unit
 * specified by GRIB2, and y is the one specified by f_unit.  The default
 * is m = 1, b = 0.
 *
 * Currently:
 *   For f_unit = 1 (english) we return Fahrenheit, knots, and inches for
 * temperature, wind speed, and amount of snow or rain.  The original units
 * are Kelvin, m/s, kg/m**2.
 *   For f_unit = 2 (metric) we return Celsius instead of Kelvin.
 *
 * ARGUMENTS
 * prodType = The GRIB2, section 0 product type. (Input)
 *  templat = The GRIB2 section 4 template number. (Input)
 *      cat = The GRIB2 section 4 "General category of Product." (Input)
 *   subcat = The GRIB2 section 4 "Specific subcategory of Product". (Input)
 *   f_unit = What type of unit to return (see above) (Input).
 *    unitM = M in equation y = m x + b (Output)
 *    unitB = B in equation y = m x + b (Output)
 *     name = Where to store the result (assumed already allocated to at
 *            least 15 bytes) (Output)
 *
 * FILES/DATABASES: None
 *
 * RETURNS: int
 *   0 if we set M and B, 1 if we used defaults.
 *
 * HISTORY
 *  11/2002 Arthur Taylor (MDL/RSIS): Created.
 *
 * NOTES
 *****************************************************************************
 */
/*
static int ComputeUnit2 (int prodType, int templat, int cat, int subcat,
                         sChar f_unit, double *unitM, double *unitB,
                         char *name)
{
   if (prodType == 0) {
      switch (cat) {
         case CAT_TEMP:
            * subcat 8 is K/m, 10, 11 is W/m**2 *
            if ((subcat < 16) && (subcat != 8) &&
                (subcat != 10) && (subcat != 11)) {
               if (f_unit == 1) {
                  strcpy (name, "[F]");
                  *unitM = 9. / 5.;
                  * 32 - (9/5 * 273.15) = 32 - 491.67 = -459.67. *
                  *unitB = -459.67;
                  return 0;
               } else if (f_unit == 2) {
                  strcpy (name, "[C]");
                  *unitM = 1;
                  *unitB = -273.15;
                  return 0;
               }
            }
            break;
         case CAT_MOIST:
            if (subcat == MOIST_PRECIP_TOT) {
               if (templat != 9) { * template number != 9 implies QPF. *
                  if (f_unit == 1) {
                     strcpy (name, "[inch]");
                     *
                      * kg/m**2 / density of water (1000 kg/m**3)
                      * 1/1000 m * 1/2.54 in/cm * 100 cm/m = 1/25.4 inches
                      *
                     *unitM = 1. / 25.4;
                     *unitB = 0;
                     return 0;
                  }
               }
            }
            if ((subcat == MOIST_SNOWAMT) || (subcat == MOIST_TOT_SNOW)) {
               if (f_unit == 1) {
                  strcpy (name, "[inch]");
                  *unitM = 100. / 2.54; * inch / m *
                  *unitB = 0;
                  return 0;
               }
            }
            break;
         case CAT_MOMENT:
            if (subcat == MOMENT_WINDSPD) {
               if (f_unit == 1) {
                  strcpy (name, "[knots]");
                  *unitM = 3600. / 1852.; * knot / m s**-1 *
                  *unitB = 0;
                  return 0;
               }
            }
            break;
      }
   } else if (prodType == 10) {
      if (cat == OCEAN_CAT_WAVES) { * 0 *
         if (subcat == OCEAN_WAVE_SIG_HT_WV) { * 5 *
            if (f_unit == 1) {
               * 1 (m) * (100cm/m) * (inch/2.54cm) * (ft/12inch) = X (ft) *
               strcpy (name, "[feet]");
               *unitM = 100. / 30.48;
               *unitB = 0;
               return 0;
            }
         }
      }
   }
   * Default case is for the unit in the GRIB2 document. *
   strcpy (name, "[GRIB2 unit]");
   *unitM = 1;
   *unitB = 0;
   return 1;
}
*/

/* GRIB2 Code Table 4.5 */
/* *INDENT-OFF* */
GRIB2SurfTable Surface[] = {
   /* 0 */ {"RESERVED", "Reserved", "-"},
   /* 1 */ {"SFC", "Ground or water surface", "-"},
   /* 2 */ {"CBL", "Cloud base level", "-"},
   /* 3 */ {"CTL", "Level of cloud tops", "-"},
   /* 4 */ {"0DEG", "Level of 0 degree C isotherm", "-"},
   /* 5 */ {"ADCL", "Level of adiabatic condensation lifted from the surface", "-"},
   /* 6 */ {"MWSL", "Maximum wind level", "-"},
   /* 7 */ {"TRO", "Tropopause", "-"},
   /* 8 */ {"NTAT", "Nominal top of atmosphere", "-"},
   /* 9 */ {"SEAB", "Sea bottom", "-"},
   /* 10: 10-19 */ {"RESERVED", "Reserved", "-"},
   /* 11: 20 */ {"TMPL", "Isothermal level", "K"},
   /* 12: 21-99 */ {"RESERVED", "Reserved", "-"},
   /* 13: 100 */ {"ISBL", "Isobaric surface", "Pa"},
   /* 14: 101 */ {"MSL", "Mean sea level", "-"},
   /* 15: 102 */ {"GPML", "Specific altitude above mean sea level", "m"},
   /* 16: 103 */ {"HTGL", "Specified height level above ground", "m"},
   /* 17: 104 */ {"SIGL", "Sigma level", "'sigma' value"},
   /* 18: 105 */ {"HYBL", "Hybrid level", "-"},
   /* 19: 106 */ {"DBLL", "Depth below land surface", "m"},
   /* 20: 107 */ {"THEL", "Isentropic (theta) level", "K"},
   /* 21: 108 */ {"SPDL", "Level at specified pressure difference from ground to level", "Pa"},
   /* 22: 109 */ {"PVL", "Potential vorticity surface", "(K m^2)/(kg s)"},
   /* 23: 110 */ {"RESERVED", "Reserved", "-"},
   /* 24: 111 */ {"EtaL", "Eta* level", "-"},
   /* 25: 112-116 */ {"RESERVED", "Reserved", "-"},
   /* 26: 117 */ {"unknown", "Mixed layer depth", "m"}, /* unknown abbrev */
   /* 27: 118-159 */ {"RESERVED", "Reserved", "-"},
   /* 28: 160 */ {"DBSL", "Depth below sea level", "m"},
   /* 29: 161-191 */ {"RESERVED", "Reserved", "-"},
   /* 30: 192-254 */ {"RESERVED", "Reserved Local use", "-"},
   /* 31: 255 */ {"MISSING", "Missing", "-"},
};

typedef struct {
   int index;
   GRIB2SurfTable surface;
} GRIB2LocalSurface;

/* based on http://www.nco.ncep.noaa.gov/pmb/docs/grib2/grib2_table4-5.shtml
 * updated last on 3/14/2006 */
GRIB2LocalSurface NCEP_Surface[] = {
   {200, {"EATM", "Entire atmosphere (considerd as a single layer)", "-"}},
   {201, {"EOCN", "Entire ocean (considered as a single layer)", "-"}},
   {204, {"HTFL", "Highest tropospheric freezing level", "-"}},
   {206, {"GCBL", "Grid scale cloud bottom level", "-"}},
   {207, {"GCTL", "Grid scale cloud top level", "-"}},
   {209, {"BCBL", "Boundary layer cloud bottom level", "-"}},
   {210, {"BCTL", "Boundary layer cloud top level", "-"}},
   {211, {"BCY", "Boundary layer cloud level", "-"}},
   {212, {"LCBL", "Low cloud bottom level", "-"}},
   {213, {"LCTL", "Low cloud top level", "-"}},
   {214, {"LCY", "Low cloud level", "-"}},
   {215, {"CEIL", "Cloud ceiling", "-"}},
   {222, {"MCBL", "Middle cloud bottom level", "-"}},
   {223, {"MCTL", "Middle cloud top level", "-"}},
   {224, {"MCY", "Middle cloud level", "-"}},
   {232, {"HCBL", "High cloud bottom level", "-"}},
   {233, {"HCTL", "High cloud top level", "-"}},
   {234, {"HCY", "High cloud level", "-"}},
   {235, {"OITL", "Ocean Isotherm Level (1/10 deg C)", "-"}},
   {236, {"OLYR", "Layer between two depths below ocean surface", "-"}},
   {237, {"OBML", "Bottom of Ocean Mixed Layer (m)", "-"}},
   {238, {"OBIL", "Bottom of Ocean Isothermal Layer (m)", "-"}},
   {242, {"CCBL", "Convective cloud bottom level", "-"}},
   {243, {"CCTL", "Convective cloud top level", "-"}},
   {244, {"CCY", "Convective cloud level", "-"}},
   {245, {"LLTW", "Lowest level of the wet bulb zero", "-"}},
   {246, {"MTHE", "Maximum equivalent potential temperature level", "-"}},
   {247, {"EHLT", "Equilibrium level", "-"}},
   {248, {"SCBL", "Shallow convective cloud bottom level", "-"}},
   {249, {"SCTL", "Shallow convective cloud top level", "-"}},
   {251, {"DCBL", "Deep convective cloud bottom level", "-"}},
   {252, {"DCTL", "Deep convective cloud top level", "-"}},
   {253, {"LBLSW", "Lowest bottom level of supercooled liquid water layer", "-"}},
   {254, {"HTLSW", "Highest top level of supercooled liquid water layer", "-"}},
};
/* *INDENT-ON* */

/*****************************************************************************
 * Table45Index() --
 *
 * Arthur Taylor / MDL
 *
 * PURPOSE
 *   To figure out the entry in the "Surface" table (used for Code Table 4.5)
 *
 * ARGUMENTS
 *          i = The original index to look up. (Input)
 * f_reserved = If the index is a "reserved" index (Output)
 *
 * FILES/DATABASES: None
 *
 * RETURNS: GRIB2SurfTable
 *
 * HISTORY
 *   9/2002 Arthur Taylor (MDL/RSIS): Created.
 *  12/2004 Arthur Taylor (RSIS): Modified to return SurfaceTable.
 *
 * NOTES
 *****************************************************************************
 */
GRIB2SurfTable Table45Index (int i,
                             int *f_reserved,
                             uShort2 center,
                             CPL_UNUSED uShort2 subcenter)
{
   size_t j;

   *f_reserved = 1;
   if ((i > 255) || (i < 0)) {
#ifdef DEBUG
      printf ("Surface index is out of 0..255 range?\n");
#endif
      return Surface[0];
   }
   if (i == 255)
      return Surface[31];
   if (i > 191) {
      if (center == 7) {
         for (j = 0; j < sizeof (NCEP_Surface) / sizeof (NCEP_Surface[0]);
              j++) {
            if (i == NCEP_Surface[j].index) {
               *f_reserved = 0;
               return (NCEP_Surface[j].surface);
            }
         }
      }
      return Surface[30];
   }
   if (i > 160)
      return Surface[29];
   if (i == 160) {
      *f_reserved = 0;
      return Surface[28];
   }
   if (i > 117)
      return Surface[27];
   if (i == 117) {
      *f_reserved = 0;
      return Surface[26];
   }
   if (i > 111)
      return Surface[25];
   if (i == 111) {
      *f_reserved = 0;
      return Surface[i - 87];
   }
   if (i == 110)
      return Surface[i - 87];
   if (i > 99) {
      *f_reserved = 0;
      return Surface[i - 87];
   }
   if (i > 20)
      return Surface[12];
   if (i == 20) {
      *f_reserved = 0;
      return Surface[11];
   }
   if (i > 9)
      return Surface[10];
   if (i > 0) {
      *f_reserved = 0;
      return Surface[i];
   }
   return Surface[0];
}

void ParseLevelName (unsigned short int center, unsigned short int subcenter,
                     uChar surfType, double value, sChar f_sndValue,
                     double sndValue, char **shortLevelName,
                     char **longLevelName)
{
   int f_reserved;
   char valBuff[512];
   char sndBuff[512];
   GRIB2SurfTable surf = Table45Index (surfType, &f_reserved, center,
                                       subcenter);

   /* Check if index is defined... 191 is undefined. */
   free (*shortLevelName);
   *shortLevelName = NULL;
   free (*longLevelName);
   *longLevelName = NULL;
   sprintf (valBuff, "%f", value);
   strTrimRight (valBuff, '0');
   if (valBuff[strlen (valBuff) - 1] == '.') {
      valBuff[strlen (valBuff) - 1] = '\0';
   }
   if (f_sndValue) {
      sprintf (sndBuff, "%f", sndValue);
      strTrimRight (sndBuff, '0');
      if (sndBuff[strlen (sndBuff) - 1] == '.') {
         sndBuff[strlen (sndBuff) - 1] = '\0';
      }
      if (f_reserved) {
         reallocSprintf (shortLevelName, "%s-%s-%s(%d)", valBuff, sndBuff,
                         surf.name, surfType);
         reallocSprintf (longLevelName, "%s-%s[%s] %s(%d) (%s)", valBuff,
                         sndBuff, surf.unit, surf.name, surfType,
                         surf.comment);
      } else {
         reallocSprintf (shortLevelName, "%s-%s-%s", valBuff, sndBuff,
                         surf.name);
         reallocSprintf (longLevelName, "%s-%s[%s] %s=\"%s\"", valBuff,
                         sndBuff, surf.unit, surf.name, surf.comment);
      }
   } else {
      if (f_reserved) {
         reallocSprintf (shortLevelName, "%s-%s(%d)", valBuff, surf.name,
                         surfType);
         reallocSprintf (longLevelName, "%s[%s] %s(%d) (%s)", valBuff,
                         surf.unit, surf.name, surfType, surf.comment);
      } else {
         reallocSprintf (shortLevelName, "%s-%s", valBuff, surf.name);
         reallocSprintf (longLevelName, "%s[%s] %s=\"%s\"", valBuff,
                         surf.unit, surf.name, surf.comment);
      }
   }
}
