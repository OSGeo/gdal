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
#include <limits>
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
   static const struct {
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
      {25, "La R\xE9" "union"},
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
      {82, "Norrk\xF6" "ping"},
      {83, ") Norrk\xF6" "ping"},
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
      {215, "Z\xFC" "rich"},
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
   static const struct {
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
      {74, 30, "Bundesamt f\xFC" "r Kartographie und Geod\xE4" "sie (Germany)"},
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
   static const struct {
      unsigned short int center;
      unsigned char process;
      const char *name;
   } Process[] = {
      {7, 2, "Ultra Violet Index Model"},
      {7, 3, "NCEP/ARL Transport and Dispersion Model"},
      {7, 4, "NCEP/ARL Smoke Model"},
      {7, 5, "Satellite Derived Precipitation and temperatures, from IR"},
      {7, 6, "NCEP/ARL Dust Model"},
      {7, 10, "Global Wind-Wave Forecast Model"},
      {7, 11, "Global Multi-Grid Wave Model (Static Grids)"},
      {7, 12, "Probabilistic Storm Surge"},
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
      {7, 81, "Analysis from GFS (Global Forecast System)"},
      {7, 82, "Analysis from GDAS (Global Data Assimilation System)"},
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
      {7, 107, "Global Ensemble Forecast System (GEFS)"},
      {7, 108, "LAMP"},
      {7, 109, "RTMA (Real Time Mesoscale Analysis)"},
      {7, 110, "NAM Model - 15km version"},
      {7, 111, "NAM model, generic resolution"},
      {7, 112, "WRF-NMM (Nondydrostatic Mesoscale Model) model, generic resolution"},
      {7, 113, "Products from NCEP SREF processing"},
      {7, 114, "NAEFS Products from joined NCEP, CMC global ensembles"},
      {7, 115, "Downscaled GFS from NAM eXtension"},
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
      {7, 130, "Merge of fields from the RUC, NAM, and Spectral Model"},
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
      {7, 184, "Climatological Calibrated Precipiation Analysis - CCPA"},
      {7, 190, "National Convective Weather Diagnostic"},
      {7, 191, "Current Icing Potential automated product"},
      {7, 192, "Analysis product from NCEP/AWC"},
      {7, 193, "Forecast product from NCEP/AWC"},
      {7, 195, "Climate Data Assimilation System 2 (CDAS2)"},
      {7, 196, "Climate Data Assimilation System 2 (CDAS2)"},
      {7, 197, "Climate Data Assimilation System (CDAS)"},
      {7, 198, "Climate Data Assimilation System (CDAS)"},
      {7, 199, "Climate Forecast System Reanalysis (CFSR)"},
      {7, 200, "CPC Manual Forecast Product"},
      {7, 201, "CPC Automated Product"},
      {7, 210, "EPA Air Quality Forecast"},
      {7, 211, "EPA Air Quality Forecast"},
      {7, 215, "SPC Manual Forecast Product"},
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
 * 7/22/2008
 */
/* GRIB2 Code table 4.2 : 0.0 */
static const GRIB2ParmTable MeteoTemp[] = {
   /* 0 */ {"TMP", "Temperature", "K", UC_K2F},  /* Need NDFD override. T */
   /* 1 */ {"VTMP", "Virtual temperature", "K", UC_K2F},
   /* 2 */ {"POT", "Potential temperature", "K", UC_K2F},
   /* 3 */ {"EPOT", "Pseudo-adiabatic potential temperature", "K", UC_K2F},
   /* 4 */ {"TMAX", "Maximum temperature", "K", UC_K2F}, /* Need NDFD override MaxT */
   /* 5 */ {"TMIN", "Minimum temperature", "K", UC_K2F}, /* Need NDFD override MinT */
   /* 6 */ {"DPT", "Dew point temperature", "K", UC_K2F}, /* Need NDFD override Td */
   /* 7 */ {"DEPR", "Dew point depression", "K", UC_K2F},
   /* 8 */ {"LAPR", "Lapse rate", "K/m", UC_NONE},
   /* 9 */ {"TMPA", "Temperature anomaly", "K", UC_K2F},
   /* 10 */ {"LHTFL", "Latent heat net flux", "W/(m^2)", UC_NONE},
   /* 11 */ {"SHTFL", "Sensible heat net flux", "W/(m^2)", UC_NONE},
            /* NDFD */
   /* 12 */ {"HEATX", "Heat index", "K", UC_K2F},
            /* NDFD */
   /* 13 */ {"WCF", "Wind chill factor", "K", UC_K2F},
   /* 14 */ {"MINDPD", "Minimum dew point depression", "K", UC_K2F},
   /* 15 */ {"VPTMP", "Virtual potential temperature", "K", UC_K2F},
   /* 16 */ {"SNOHF", "Snow phase change heat flux", "W/m^2", UC_NONE},
   /* 17 */ {"SKINT", "Skin temperature", "K", UC_K2F},
};

/* GRIB2 Code table 4.2 : 0.1 */
/* NCEP added "Water" to items 22, 24, 25 */
static const GRIB2ParmTable MeteoMoist[] = {
   /* 0 */ {"SPFH", "Specific humidity", "kg/kg", UC_NONE},
   /* 1 */ {"RH", "Relative humidity", "%", UC_NONE},
   /* 2 */ {"MIXR", "Humidity mixing ratio", "kg/kg", UC_NONE},
   /* 3 */ {"PWAT", "Precipitable water", "kg/(m^2)", UC_NONE},
   /* 4 */ {"VAPP", "Vapor pressure", "Pa", UC_NONE},
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
   /* 18 */ {"ABSH", "Absolute humidity", "kg/(m^3)", UC_NONE},
   /* 19 */ {"PTYPE", "Precipitation type", "1=Rain; 2=Thunderstorm; "
             "3=Freezing Rain; 4=Mixed/ice; 5=snow; 255=missing", UC_NONE},
   /* 20 */ {"ILIQW", "Integrated liquid water", "kg/(m^2)", UC_NONE},
   /* 21 */ {"TCOND", "Condensate", "kg/kg", UC_NONE},
/* CLWMR Did not make it to tables yet should be "-" */
   /* 22 */ {"CLWMR", "Cloud mixing ratio", "kg/kg", UC_NONE},
   /* 23 */ {"ICMR", "Ice water mixing ratio", "kg/kg", UC_NONE},   /* ICMR? */
   /* 24 */ {"RWMR", "Rain mixing ratio", "kg/kg", UC_NONE},
   /* 25 */ {"SNMR", "Snow mixing ratio", "kg/kg", UC_NONE},
   /* 26 */ {"MCONV", "Horizontal moisture convergence", "kg/(kg s)", UC_NONE},
   /* 27 */ {"MAXRH", "Maximum relative humidity", "%", UC_NONE},
   /* 28 */ {"MAXAH", "Maximum absolute humidity", "kg/(m^3)", UC_NONE},
            /* NDFD */
   /* 29 */ {"ASNOW", "Total snowfall", "m", UC_M2Inch},
   /* 30 */ {"PWCAT", "Precipitable water category", "undefined", UC_NONE},
   /* 31 */ {"HAIL", "Hail", "m", UC_NONE},
   /* 32 */ {"GRLE", "Graupel (snow pellets)", "kg/kg", UC_NONE},
/* 33 */    {"CRAIN", "Categorical rain", "0=no; 1=yes", UC_NONE},
/* 34 */    {"CFRZR", "Categorical freezing rain", "0=no; 1=yes", UC_NONE},
/* 35 */    {"CICEP", "Categorical ice pellets", "0=no; 1=yes", UC_NONE},
/* 36 */    {"CSNOW", "Categorical snow", "0=no; 1=yes", UC_NONE},
/* 37 */    {"CPRAT", "Convective precipitation rate", "kg/(m^2*s)", UC_NONE},
/* 38 */    {"MCONV", "Horizontal moisture divergence", "kg/(kg*s)", UC_NONE},
/* 39 */    {"CPOFP", "Percent frozen precipitation", "%", UC_NONE},
/* 40 */    {"PEVAP", "Potential evaporation", "kg/m^2", UC_NONE},
/* 41 */    {"PEVPR", "Potential evaporation rate", "W/m^2", UC_NONE},
/* 42 */    {"SNOWC", "Snow cover", "%", UC_NONE},
/* 43 */    {"FRAIN", "Rain fraction of total cloud water", "-", UC_NONE},
/* 44 */    {"RIME", "Rime factor", "-", UC_NONE},
/* 45 */    {"TCOLR", "Total column integrated rain", "kg/m^2", UC_NONE},
/* 46 */    {"TCOLS", "Total column integrated snow", "kg/m^2", UC_NONE},
/* 47 */    {"LSWP", "Large scale water precipitation", "kg/m^2", UC_NONE},
/* 48 */    {"CWP", "Convective water precipitation", "kg/m^2", UC_NONE},
/* 49 */    {"TWATP", "Total water precipitation", "kg/m^2", UC_NONE},
/* 50 */    {"TSNOWP", "Total snow precipitation", "kg/m^2", UC_NONE},
/* 51 */    {"TCWAT", "Total column water", "kg/m^2", UC_NONE},
/* 52 */    {"TPRATE", "Total precipitation rate", "kg/(m^2*s)", UC_NONE},
/* 53 */    {"TSRWE", "Total snowfall rate water equivalent", "kg/(m^2*s)", UC_NONE},
/* 54 */    {"LSPRATE", "Large scale precipitation rate", "kg/(m^2*s)", UC_NONE},
/* 55 */    {"CSRWE", "Convective snowfall rate water equivalent", "kg/(m^2*s)", UC_NONE},
/* 56 */    {"LSSRWE", "Large scale snowfall rate water equivalent", "kg/(m^2*s)", UC_NONE},
/* 57 */    {"TSRATE", "Total snowfall rate", "m/s", UC_NONE},
/* 58 */    {"CSRATE", "Convective snowfall rate", "m/s", UC_NONE},
/* 59 */    {"LSSRWE", "Large scale snowfall rate", "m/s", UC_NONE},
/* 60 */    {"SDWE", "Snow depth water equivalent", "kg/m^2", UC_NONE},
/* 61 */    {"SDEN", "Snow density", "kg/m^3", UC_NONE},
/* 62 */    {"SEVAP", "Snow evaporation", "kg/m^2", UC_NONE},
/* 63 */    {"", "Reserved", "-", UC_NONE},
/* 64 */    {"TCIWV", "Total column integrated water vapour", "kg/m^2", UC_NONE},
/* 65 */    {"RPRATE", "Rain precipitation rate", "kg/(m^2*s)", UC_NONE},
/* 66 */    {"SPRATE", "Snow precipitation rate", "kg/(m^2*s)", UC_NONE},
/* 67 */    {"FPRATE", "Freezing rain precipitation rate", "kg/(m^2*s)", UC_NONE},
/* 68 */    {"IPRATE", "Ice pellets precipitation rate", "kg/(m^2*s)", UC_NONE},
};

/* GRIB2 Code table 4.2 : 0.2 */
static const GRIB2ParmTable MeteoMoment[] = {
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
   /* 14 */ {"PVORT", "Potential vorticity", "K(m^2)/(kg s)", UC_NONE},
   /* 15 */ {"VUCSH", "Vertical u-component shear", "1/s", UC_NONE},
   /* 16 */ {"VVCSH", "Vertical v-component shear", "1/s", UC_NONE},
   /* 17 */ {"UFLX", "Momentum flux; u component", "N/(m^2)", UC_NONE},
   /* 18 */ {"VFLX", "Momentum flux; v component", "N/(m^2)", UC_NONE},
   /* 19 */ {"WMIXE", "Wind mixing energy", "J", UC_NONE},
   /* 20 */ {"BLYDP", "Boundary layer dissipation", "W/(m^2)", UC_NONE},
   /* 21 */ {"MAXGUST", "Maximum wind speed", "m/s", UC_NONE},
   /* 22 */ {"GUST", "Wind speed (gust)", "m/s", UC_MS2Knots},  /* GUST? */
   /* 23 */ {"UGUST", "u-component of wind (gust)", "m/s", UC_NONE},
   /* 24 */ {"VGUST", "v-component of wind (gust)", "m/s", UC_NONE},
/* 25 */    {"VWSH", "Vertical speed shear", "1/s", UC_NONE},
/* 26 */    {"MFLX", "Horizontal momentum flux", "N/(m^2)", UC_NONE},
/* 27 */    {"USTM", "U-component storm motion", "m/s", UC_NONE},
/* 28 */    {"VSTM", "V-component storm motion", "m/s", UC_NONE},
/* 29 */    {"CD", "Drag coefficient", "-", UC_NONE},
/* 30 */    {"FRICV", "Frictional velocity", "m/s", UC_NONE},
};

/* GRIB2 Code table 4.2 : 0.3 */
static const GRIB2ParmTable MeteoMass[] = {
   /* 0 */ {"PRES", "Pressure", "Pa", UC_NONE},
   /* 1 */ {"PRMSL", "Pressure reduced to MSL", "Pa", UC_NONE},
   /* 2 */ {"PTEND", "Pressure tendency", "Pa/s", UC_NONE},
   /* 3 */ {"ICAHT", "ICAO Standard Atmosphere Reference Height", "m", UC_NONE},
   /* 4 */ {"GP", "Geopotential", "(m^2)/(s^2)", UC_NONE},
   /* 5 */ {"HGT", "Geopotential height", "gpm", UC_NONE},
   /* 6 */ {"DIST", "Geometric height", "m", UC_NONE},
   /* 7 */ {"HSTDV", "Standard deviation of height", "m", UC_NONE},
   /* 8 */ {"PRESA", "Pressure anomaly", "Pa", UC_NONE},
   /* 9 */ {"GPA", "Geopotential height anomaly", "gpm", UC_NONE},
   /* 10 */ {"DEN", "Density", "kg/(m^3)", UC_NONE},
   /* 11 */ {"ALTS", "Altimeter setting", "Pa", UC_NONE},
   /* 12 */ {"THICK", "Thickness", "m", UC_NONE},
   /* 13 */ {"PRESALT", "Pressure altitude", "m", UC_NONE},
   /* 14 */ {"DENALT", "Density altitude", "m", UC_NONE},
/* 15 */    {"5WAVH", "5-wave geopotential height", "gpm", UC_NONE},
/* 16 */    {"U-GWD", "Zonal flux of gravity wave stress", "N/(m^2)", UC_NONE},
/* 17 */    {"V-GWD", "Meridional flux of gravity wave stress", "N/(m^2)", UC_NONE},
/* 18 */    {"HPBL", "Planetary boundary layer height", "m", UC_NONE},
/* 19 */    {"5WAVA", "5-wave geopotential height anomaly", "gpm", UC_NONE},
/* 20 */    {"SDSGSO", "Standard deviation of sub-grid scale orography", "m", UC_NONE},
/* 21 */    {"AOSGSO", "Angle of sub-gridscale orography", "rad", UC_NONE},
/* 22 */    {"SSGSO", "Slope of sub-gridscale orography", "Numeric", UC_NONE},
/* 23 */    {"GSGSO", "Gravity wave dissipation", "W/m^2", UC_NONE},
/* 24 */    {"ASGSO", "Anisotrophy of sub-gridscale orography", "Numeric", UC_NONE},
};

/* GRIB2 Code table 4.2 : 0.4 */
static const GRIB2ParmTable MeteoShortRadiate[] = {
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
/* 9 */    {"NSWRF", "Net short wave radiation flux", "W/(m^2)", UC_NONE},
/* 10 */   {"PHOTAR", "Photosynthetically active radiation", "W/(m^2)", UC_NONE},
/* 11 */   {"NSWRFCS", "Net short-wave radiation flux; clear sky", "W/(m^2)", UC_NONE},
/* 12 */   {"DWUVR", "Downward UV radiation", "W/(m^2)", UC_NONE},
/* 13 */   {"", "Reserved", "-", UC_NONE},
/* 14 */   {"", "Reserved", "-", UC_NONE},
/* 15 */   {"", "Reserved", "-", UC_NONE},
/* 16 */   {"", "Reserved", "-", UC_NONE},
/* 17 */   {"", "Reserved", "-", UC_NONE},
/* 18 */   {"", "Reserved", "-", UC_NONE},
/* 19 */   {"", "Reserved", "-", UC_NONE},
/* 20 */   {"", "Reserved", "-", UC_NONE},
/* 21 */   {"", "Reserved", "-", UC_NONE},
/* 22 */   {"", "Reserved", "-", UC_NONE},
/* 23 */   {"", "Reserved", "-", UC_NONE},
/* 24 */   {"", "Reserved", "-", UC_NONE},
/* 25 */   {"", "Reserved", "-", UC_NONE},
/* 26 */   {"", "Reserved", "-", UC_NONE},
/* 27 */   {"", "Reserved", "-", UC_NONE},
/* 28 */   {"", "Reserved", "-", UC_NONE},
/* 29 */   {"", "Reserved", "-", UC_NONE},
/* 30 */   {"", "Reserved", "-", UC_NONE},
/* 31 */   {"", "Reserved", "-", UC_NONE},
/* 32 */   {"", "Reserved", "-", UC_NONE},
/* 33 */   {"", "Reserved", "-", UC_NONE},
/* 34 */   {"", "Reserved", "-", UC_NONE},
/* 35 */   {"", "Reserved", "-", UC_NONE},
/* 36 */   {"", "Reserved", "-", UC_NONE},
/* 37 */   {"", "Reserved", "-", UC_NONE},
/* 38 */   {"", "Reserved", "-", UC_NONE},
/* 39 */   {"", "Reserved", "-", UC_NONE},
/* 40 */   {"", "Reserved", "-", UC_NONE},
/* 41 */   {"", "Reserved", "-", UC_NONE},
/* 42 */   {"", "Reserved", "-", UC_NONE},
/* 43 */   {"", "Reserved", "-", UC_NONE},
/* 44 */   {"", "Reserved", "-", UC_NONE},
/* 45 */   {"", "Reserved", "-", UC_NONE},
/* 46 */   {"", "Reserved", "-", UC_NONE},
/* 47 */   {"", "Reserved", "-", UC_NONE},
/* 48 */   {"", "Reserved", "-", UC_NONE},
/* 49 */   {"", "Reserved", "-", UC_NONE},
/* 50 */   {"UVIUCS", "UV index (under clear sky)", "Numeric", UC_NONE},
/* 51 */   {"UVI", "UV index", "W/(m^2)", UC_UVIndex},
};

/* GRIB2 Code table 4.2 : 0.5 */
static const GRIB2ParmTable MeteoLongRadiate[] = {
   /* 0 */ {"NLWRS", "Net long wave radiation flux (surface)", "W/(m^2)", UC_NONE},
   /* 1 */ {"NLWRT", "Net long wave radiation flux (top of atmosphere)",
            "W/(m^2)", UC_NONE},
   /* 2 */ {"LWAVR", "Long wave radiation flux", "W/(m^2)", UC_NONE},
/* 3 */    {"DLWRF", "Downward long-wave radiation flux", "W/(m^2)", UC_NONE},
/* 4 */    {"ULWRF", "Upward long-wave radiation flux", "W/(m^2)", UC_NONE},
/* 5 */    {"NLWRF", "Net long wave radiation flux", "W/(m^2)", UC_NONE},
/* 6 */    {"NLWRCS", "Net long-wave radiation flux; clear sky", "W/(m^2)", UC_NONE},
};

/* GRIB2 Code table 4.2 : 0.6 */
static const GRIB2ParmTable MeteoCloud[] = {
   /* 0 */ {"CICE", "Cloud Ice", "kg/(m^2)", UC_NONE},
   /* 1 */ {"TCDC", "Total cloud cover", "%", UC_NONE}, /* Need NDFD override Sky */
   /* 2 */ {"CDCON", "Convective cloud cover", "%", UC_NONE},
   /* 3 */ {"LCDC", "Low cloud cover", "%", UC_NONE},
   /* 4 */ {"MCDC", "Medium cloud cover", "%", UC_NONE},
   /* 5 */ {"HCDC", "High cloud cover", "%", UC_NONE},
   /* 6 */ {"CWAT", "Cloud water", "kg/(m^2)", UC_NONE},
   /* 7 */ {"CDCA", "Cloud amount", "%", UC_NONE},
   /* 8 */ {"CDCT", "Cloud type", "0=clear; 1=Cumulonimbus; 2=Stratus; "
            "3=Stratocumulus; 4=Cumulus; 5=Altostratus; 6=Nimbostratus; "
            "7=Altocumulus; 8=Cirrostratus; 9=Cirrocumulus; 10=Cirrus; "
            "11=Cumulonimbus (fog); 12=Stratus (fog); 13=Stratocumulus (fog);"
            " 14=Cumulus (fog); 15=Altostratus (fog); 16=Nimbostratus (fog); "
            "17=Altocumulus (fog); 18=Cirrostratus (fog); "
            "19=Cirrocumulus (fog); 20=Cirrus (fog); 191=unknown; "
            "255=missing", UC_NONE},
   /* 9 */ {"TMAXT", "Thunderstorm maximum tops", "m", UC_NONE},
   /* 10 */ {"THUNC", "Thunderstorm coverage", "0=none; 1=isolated (1%-2%); "
             "2=few (3%-15%); 3=scattered (16%-45%); 4=numerous (> 45%); "
             "255=missing", UC_NONE},
   /* 11 */ {"CDCB", "Cloud base", "m", UC_M2Feet},
   /* 12 */ {"CDCT", "Cloud top", "m", UC_M2Feet},
   /* 13 */ {"CEIL", "Ceiling", "m", UC_M2Feet},
/* 14 */    {"CDLYR", "Non-convective cloud cover", "%", UC_NONE},
/* 15 */    {"CWORK", "Cloud work function", "J/kg", UC_NONE},
/* 16 */    {"CUEFI", "Convective cloud efficiency", "-", UC_NONE},
/* 17 */    {"TCOND", "Total condensate", "kg/kg", UC_NONE},
/* 18 */    {"TCOLW", "Total column-integrated cloud water", "kg/(m^2)", UC_NONE},
/* 19 */    {"TCOLI", "Total column-integrated cloud ice", "kg/(m^2)", UC_NONE},
/* 20 */    {"TCOLC", "Total column-integrated condensate", "kg/(m^2)", UC_NONE},
/* 21 */    {"FICE", "Ice fraction of total condensate", "-", UC_NONE},
/* 22 */    {"CDCC", "Cloud cover", "%", UC_NONE},
/* 23 */    {"CDCIMR", "Cloud ice mixing ratio", "kg/kg", UC_NONE},
/* 24 */    {"SUNS", "Sunshine", "Numeric", UC_NONE},
/* 25 */    {"CBHE", "Horizontal extent of cumulonimbus (CB)", "%", UC_NONE},
/* 26 */   {"", "Reserved", "-", UC_NONE},
/* 27 */   {"", "Reserved", "-", UC_NONE},
/* 28 */   {"", "Reserved", "-", UC_NONE},
/* 29 */   {"", "Reserved", "-", UC_NONE},
/* 30 */   {"", "Reserved", "-", UC_NONE},
/* 31 */   {"", "Reserved", "-", UC_NONE},
/* 32 */   {"", "Reserved", "-", UC_NONE},
/* 33 */    {"SUNSD", "SunShine Duration", "s", UC_NONE},

};

/* GRIB2 Code table 4.2 : 0.7 */
static const GRIB2ParmTable MeteoStability[] = {
   /* 0 */ {"PLI", "Parcel lifted index (to 500 hPa)", "K", UC_NONE},
   /* 1 */ {"BLI", "Best lifted index (to 500 hPa)", "K", UC_NONE},
   /* 2 */ {"KX", "K index", "K", UC_NONE},
   /* 3 */ {"KOX", "KO index", "K", UC_NONE},
   /* 4 */ {"TOTALX", "Total totals index", "K", UC_NONE},
   /* 5 */ {"SX", "Sweat index", "numeric", UC_NONE},
   /* 6 */ {"CAPE", "Convective available potential energy", "J/kg", UC_NONE},
   /* 7 */ {"CIN", "Convective inhibition", "J/kg", UC_NONE},
   /* 8 */ {"HLCY", "Storm relative helicity", "J/kg", UC_NONE},
   /* 9 */ {"EHLX", "Energy helicity index", "numeric", UC_NONE},
/* 10 */   {"LFTX", "Surface lifted index", "K", UC_NONE},
/* 11 */   {"4LFTX", "Best (4-layer) lifted index", "K", UC_NONE},
/* 12 */   {"RI", "Richardson number", "-", UC_NONE},
};

/* GRIB2 Code table 4.2 : 0.13 */
static const GRIB2ParmTable MeteoAerosols[] = {
   /* 0 */ {"AEROT", "Aerosol type", "0=Aerosol not present; 1=Aerosol present; "
            "255=missing", UC_NONE},
};

/* GRIB2 Code table 4.2 : 0.14 */
static const GRIB2ParmTable MeteoGases[] = {
   /* 0 */ {"TOZNE", "Total ozone", "Dobson", UC_NONE},
/* 1 */    {"O3MR", "Ozone mixing ratio", "kg/kg", UC_NONE},
/* 2 */    {"TCIOZ", "Total column integrated ozone", "Dobson", UC_NONE},
};

/* GRIB2 Code table 4.2 : 0.15 */
static const GRIB2ParmTable MeteoRadar[] = {
   /* 0 */ {"BSWID", "Base spectrum width", "m/s", UC_NONE},
   /* 1 */ {"BREF", "Base reflectivity", "dB", UC_NONE},
   /* 2 */ {"BRVEL", "Base radial velocity", "m/s", UC_NONE},
   /* 3 */ {"VERIL", "Vertically-integrated liquid", "kg/m", UC_NONE},
   /* 4 */ {"LMAXBR", "Layer maximum base reflectivity", "dB", UC_NONE},
   /* 5 */ {"PREC", "Precipitation", "kg/(m^2)", UC_NONE},
   /* 6 */ {"RDSP1", "Radar spectra (1)", "-", UC_NONE},
   /* 7 */ {"RDSP2", "Radar spectra (2)", "-", UC_NONE},
   /* 8 */ {"RDSP3", "Radar spectra (3)", "-", UC_NONE},
};

/* GRIB2 Code table 4.2 : 0.16 */
static const GRIB2ParmTable MeteoRadarImagery[] = {
   /* 0 */ {"REFZR", "Equivalent radar reflectivity for rain", "mm^6/m^3", UC_NONE},
   /* 1 */ {"REFZI", "Equivalent radar reflectivity for snow", "mm^6/m^3", UC_NONE},
   /* 2 */ {"REFZC", "Equivalent radar reflectivity for parameterized convection", "mm^6/m^3", UC_NONE},
   /* 3 */ {"RETOP", "Echo Top", "m", UC_NONE},
   /* 4 */ {"REFD", "Reflectivity", "dB", UC_NONE},
   /* 5 */ {"REFC", "Composity reflectivity", "dB", UC_NONE},
};

/* GRIB2 Code table 4.2 : 0.18 */
static const GRIB2ParmTable MeteoNuclear[] = {
   /* 0 */ {"ACCES", "Air concentration of Caesium 137", "Bq/(m^3)", UC_NONE},
   /* 1 */ {"ACIOD", "Air concentration of Iodine 131", "Bq/(m^3)", UC_NONE},
   /* 2 */ {"ACRADP", "Air concentration of radioactive pollutant", "Bq/(m^3)", UC_NONE},
   /* 3 */ {"GDCES", "Ground deposition of Caesium 137", "Bq/(m^2)", UC_NONE},
   /* 4 */ {"GDIOD", "Ground deposition of Iodine 131", "Bq/(m^2)", UC_NONE},
   /* 5 */ {"GDRADP", "Ground deposition of radioactive pollutant", "Bq/(m^2)", UC_NONE},
   /* 6 */ {"TIACCP", "Time-integrated air concentration of caesium pollutant",
            "(Bq s)/(m^3)", UC_NONE},
   /* 7 */ {"TIACIP", "Time-integrated air concentration of iodine pollutant",
            "(Bq s)/(m^3)", UC_NONE},
   /* 8 */ {"TIACRP", "Time-integrated air concentration of radioactive pollutant",
            "(Bq s)/(m^3)", UC_NONE},
};

/* GRIB2 Code table 4.2 : 0.19 */
/* NCEP capitalized items 11 */
static const GRIB2ParmTable MeteoAtmos[] = {
   /* 0 */ {"VIS", "Visibility", "m", UC_M2StatuteMile},
   /* 1 */ {"ALBDO", "Albedo", "%", UC_NONE},
   /* 2 */ {"TSTM", "Thunderstorm probability", "%", UC_NONE},
   /* 3 */ {"MIXHT", "Mixed layer depth", "m", UC_NONE},
   /* 4 */ {"VOLASH", "Volcanic ash", "0=not present; 1=present; 255=missing", UC_NONE},
   /* 5 */ {"ICIT", "Icing top", "m", UC_NONE},
   /* 6 */ {"ICIB", "Icing base", "m", UC_NONE},
   /* 7 */ {"ICI", "Icing", "0=None; 1=Light; 2=Moderate; 3=Severe; "
            "255=missing", UC_NONE},
   /* 8 */ {"TURBT", "Turbulance top", "m", UC_NONE},
   /* 9 */ {"TURBB", "Turbulence base", "m", UC_NONE},
   /* 10 */ {"TURB", "Turbulance", "0=None(smooth); 1=Light; 2=Moderate; "
             "3=Severe; 4=Extreme; 255=missing", UC_NONE},
   /* 11 */ {"TKE", "Turbulent kinetic energy", "J/kg", UC_NONE},
   /* 12 */ {"PBLREG", "Planetary boundary layer regime", "0=Reserved; 1=Stable; "
             "2=Mechanically driven turbulence; 3=Forced convection; "
             "4=Free convection; 255=missing", UC_NONE},
   /* 13 */ {"CONTI", "Contrail intensity", "0=Contrail not present; "
             "1=Contrail present; 255=missing", UC_NONE},
   /* 14 */ {"CONTET", "Contrail engine type", "0=Low bypass; 1=High bypass; "
             "2=Non bypass; 255=missing", UC_NONE},
   /* 15 */ {"CONTT", "Contrail top", "m", UC_NONE},
   /* 16 */ {"CONTB", "Contrail base", "m", UC_NONE},
/* 17 */    {"MXSALB", "Maximum snow albedo", "%", UC_NONE},
/* 18 */    {"SNFALB", "Snow free albedo", "%", UC_NONE},
/* 19 */    {"SALBD", "Snow albedo", "%", UC_NONE},
            {"ICIP", "Icing", "%", UC_NONE},
            {"CTP", "In-Cloud Turbulence", "%", UC_NONE},
            {"CAT", "Clear Air Turbulence", "%", UC_NONE},
            {"SLDP", "Supercooled Large Droplet Probability", "%", UC_NONE},
/* Mike added 3/2012 */
/* 24 */    {"CONTKE", "Convective Turbulent Kinetic Energy", "J/kg", UC_NONE},
/* 25 */    {"WIWW", "Weather Interpretation ww (WMO)", " ", UC_NONE},
/* 26 */    {"CONVO", "Convective Outlook",  "0=No Risk Area; 1=Reserved; "
             "2=General Thunderstorm Risk Area; 3=Reserved; 4=Slight Risk Area; "
             "5=Reserved; 6=Moderate Risk Area;  7=Reserved; 8=High Risk Area; "
             "9-10=Reserved; 11=Dry Thunderstorm (Dry Lightning) Risk Area; "
             "12-13=Reserved; 14=Critical Risk Area; 15-17=Reserved"
             "18=Extremely Critical Risk Area; 255=missing", UC_NONE},
};

/* GRIB2 Code table 4.2 : 0.20 */
static const GRIB2ParmTable MeteoAtmoChem[] = {
   /* 0 */  {"MASSDEN", "Mass Density (Concentration)", "kg/(m^3)", UC_NONE},
   /* 1 */  {"COLMD", "Column-Integrated Mass Density", "kg/(m^2)", UC_NONE},
   /* 2 */  {"MASSMR", "Mass Mixing Ratio (Mass Fraction in Air)", "kg/kg", UC_NONE},
   /* 3 */  {"AEMFLX", "Atmosphere Emission Mass Flux", "kg/(m^2*s)", UC_NONE},
   /* 4 */  {"ANPMFLX", "Atmosphere Net Production Mass Flux", "kg/(m^2*s)", UC_NONE},
   /* 5 */  {"ANPEMFLX", "Atmosphere Net Production and Emission Mass Flux", "kg/(m^2*s)", UC_NONE},
   /* 6 */  {"SDDMFLX", "Surface Dry Deposition Mass Flux", "kg/(m^2*s)", UC_NONE},
   /* 7 */  {"SWDMFLX", "Surface Wet Deposition Mass Flux", "kg/(m^2*s)", UC_NONE},
   /* 8 */  {"AREMFLX", "Atmosphere Re-Emission Mass Flux", "kg/(m^2*s)", UC_NONE},
   /* 9 */  {"", "Reserved", "-", UC_NONE},
  /* 10 */  {"", "Reserved", "-", UC_NONE},
  /* 11 */  {"", "Reserved", "-", UC_NONE},
  /* 12 */  {"", "Reserved", "-", UC_NONE},
  /* 13 */  {"", "Reserved", "-", UC_NONE},
  /* 14 */  {"", "Reserved", "-", UC_NONE},
  /* 15 */  {"", "Reserved", "-", UC_NONE},
  /* 16 */  {"", "Reserved", "-", UC_NONE},
  /* 17 */  {"", "Reserved", "-", UC_NONE},
  /* 18 */  {"", "Reserved", "-", UC_NONE},
  /* 19 */  {"", "Reserved", "-", UC_NONE},
  /* 20 */  {"", "Reserved", "-", UC_NONE},
  /* 21 */  {"", "Reserved", "-", UC_NONE},
  /* 22 */  {"", "Reserved", "-", UC_NONE},
  /* 23 */  {"", "Reserved", "-", UC_NONE},
  /* 24 */  {"", "Reserved", "-", UC_NONE},
  /* 25 */  {"", "Reserved", "-", UC_NONE},
  /* 26 */  {"", "Reserved", "-", UC_NONE},
  /* 27 */  {"", "Reserved", "-", UC_NONE},
  /* 28 */  {"", "Reserved", "-", UC_NONE},
  /* 29 */  {"", "Reserved", "-", UC_NONE},
  /* 30 */  {"", "Reserved", "-", UC_NONE},
  /* 31 */  {"", "Reserved", "-", UC_NONE},
  /* 32 */  {"", "Reserved", "-", UC_NONE},
  /* 33 */  {"", "Reserved", "-", UC_NONE},
  /* 34 */  {"", "Reserved", "-", UC_NONE},
  /* 35 */  {"", "Reserved", "-", UC_NONE},
  /* 36 */  {"", "Reserved", "-", UC_NONE},
  /* 37 */  {"", "Reserved", "-", UC_NONE},
  /* 38 */  {"", "Reserved", "-", UC_NONE},
  /* 39 */  {"", "Reserved", "-", UC_NONE},
  /* 40 */  {"", "Reserved", "-", UC_NONE},
  /* 41 */  {"", "Reserved", "-", UC_NONE},
  /* 42 */  {"", "Reserved", "-", UC_NONE},
  /* 43 */  {"", "Reserved", "-", UC_NONE},
  /* 44 */  {"", "Reserved", "-", UC_NONE},
  /* 45 */  {"", "Reserved", "-", UC_NONE},
  /* 46 */  {"", "Reserved", "-", UC_NONE},
  /* 47 */  {"", "Reserved", "-", UC_NONE},
  /* 48 */  {"", "Reserved", "-", UC_NONE},
  /* 49 */  {"", "Reserved", "-", UC_NONE},
  /* 50 */  {"AIA", "Amount in Atmosphere", "mol", UC_NONE},
  /* 51 */  {"CONAIR", "Concentration in Air", "mol/(m^3)", UC_NONE},
  /* 52 */  {"VMXR", "Volume Mixing Ratio (Fraction in Air)", "mol/mol", UC_NONE},
  /* 53 */  {"CGPRC", "Chemical Gross Production Rate of Concentration", "mol/(m^3*s)", UC_NONE},
  /* 54 */  {"CGDRC", "Chemical Gross Destruction Rate of Concentration", "mol/(m^3*s)", UC_NONE},
  /* 55 */  {"SFLUX", "Surface Flux", "mol/(m^2*s)", UC_NONE},
  /* 56 */  {"COAIA", "Changes of Amount in Atmosphere", "mol/s", UC_NONE},
  /* 57 */  {"TYABA", "Total Yearly Average Burden of the Atmosphere", "mol", UC_NONE},
  /* 58 */  {"TYAAL", "Total Yearly Average Atmospheric Loss", "mol/s", UC_NONE},
  /* 59 */  {"", "Reserved", "-", UC_NONE},
  /* 60 */  {"", "Reserved", "-", UC_NONE},
  /* 61 */  {"", "Reserved", "-", UC_NONE},
  /* 62 */  {"", "Reserved", "-", UC_NONE},
  /* 63 */  {"", "Reserved", "-", UC_NONE},
  /* 64 */  {"", "Reserved", "-", UC_NONE},
  /* 65 */  {"", "Reserved", "-", UC_NONE},
  /* 66 */  {"", "Reserved", "-", UC_NONE},
  /* 67 */  {"", "Reserved", "-", UC_NONE},
  /* 68 */  {"", "Reserved", "-", UC_NONE},
  /* 69 */  {"", "Reserved", "-", UC_NONE},
  /* 70 */  {"", "Reserved", "-", UC_NONE},
  /* 71 */  {"", "Reserved", "-", UC_NONE},
  /* 72 */  {"", "Reserved", "-", UC_NONE},
  /* 73 */  {"", "Reserved", "-", UC_NONE},
  /* 74 */  {"", "Reserved", "-", UC_NONE},
  /* 75 */  {"", "Reserved", "-", UC_NONE},
  /* 76 */  {"", "Reserved", "-", UC_NONE},
  /* 77 */  {"", "Reserved", "-", UC_NONE},
  /* 78 */  {"", "Reserved", "-", UC_NONE},
  /* 79 */  {"", "Reserved", "-", UC_NONE},
  /* 80 */  {"", "Reserved", "-", UC_NONE},
  /* 81 */  {"", "Reserved", "-", UC_NONE},
  /* 82 */  {"", "Reserved", "-", UC_NONE},
  /* 83 */  {"", "Reserved", "-", UC_NONE},
  /* 84 */  {"", "Reserved", "-", UC_NONE},
  /* 85 */  {"", "Reserved", "-", UC_NONE},
  /* 86 */  {"", "Reserved", "-", UC_NONE},
  /* 87 */  {"", "Reserved", "-", UC_NONE},
  /* 88 */  {"", "Reserved", "-", UC_NONE},
  /* 89 */  {"", "Reserved", "-", UC_NONE},
  /* 90 */  {"", "Reserved", "-", UC_NONE},
  /* 91 */  {"", "Reserved", "-", UC_NONE},
  /* 92 */  {"", "Reserved", "-", UC_NONE},
  /* 93 */  {"", "Reserved", "-", UC_NONE},
  /* 94 */  {"", "Reserved", "-", UC_NONE},
  /* 95 */  {"", "Reserved", "-", UC_NONE},
  /* 96 */  {"", "Reserved", "-", UC_NONE},
  /* 97 */  {"", "Reserved", "-", UC_NONE},
  /* 98 */  {"", "Reserved", "-", UC_NONE},
  /* 99 */  {"", "Reserved", "-", UC_NONE},
 /* 100 */  {"SADEN", "Surface Area Density (Aerosol)", "1/m", UC_NONE},
 /* 101 */  {"AOTK", "Atmosphere Optical Thickness", "m", UC_NONE},
 /* 102 */  {"", "Reserved", "-", UC_NONE},
 /* 103 */  {"", "Reserved", "-", UC_NONE},
 /* 104 */  {"", "Reserved", "-", UC_NONE},
 /* 105 */  {"", "Reserved", "-", UC_NONE},
 /* 106 */  {"", "Reserved", "-", UC_NONE},
 /* 107 */  {"", "Reserved", "-", UC_NONE},
 /* 108 */  {"", "Reserved", "-", UC_NONE},
 /* 109 */  {"", "Reserved", "-", UC_NONE},
 /* 110 */  {"", "Reserved", "-", UC_NONE},
 /* 111 */  {"", "Reserved", "-", UC_NONE},
 /* 112 */  {"", "Reserved", "-", UC_NONE},
 /* 113 */  {"", "Reserved", "-", UC_NONE},
 /* 114 */  {"", "Reserved", "-", UC_NONE},
 /* 115 */  {"", "Reserved", "-", UC_NONE},
 /* 116 */  {"", "Reserved", "-", UC_NONE},
 /* 117 */  {"", "Reserved", "-", UC_NONE},
 /* 118 */  {"", "Reserved", "-", UC_NONE},
 /* 119 */  {"", "Reserved", "-", UC_NONE},
 /* 120 */  {"", "Reserved", "-", UC_NONE},
 /* 121 */  {"", "Reserved", "-", UC_NONE},
 /* 122 */  {"", "Reserved", "-", UC_NONE},
 /* 123 */  {"", "Reserved", "-", UC_NONE},
 /* 124 */  {"", "Reserved", "-", UC_NONE},
 /* 125 */  {"", "Reserved", "-", UC_NONE},
 /* 126 */  {"", "Reserved", "-", UC_NONE},
 /* 127 */  {"", "Reserved", "-", UC_NONE},
 /* 128 */  {"", "Reserved", "-", UC_NONE},
 /* 129 */  {"", "Reserved", "-", UC_NONE},
 /* 130 */  {"", "Reserved", "-", UC_NONE},
 /* 131 */  {"NO2TROP", "Nitrogen Dioxide (NO2) Tropospheric Column", "mol/(cm^2)", UC_NONE},
 /* 132 */  {"NO2VCD", "Nitrogen Dioxide (NO2) Vertical Column Density", "mol/(cm^2)", UC_NONE},
 /* 133 */  {"BROVCD", "Bromine Monoxide (BrO) Vertical Column Density", "mol/(cm^2)", UC_NONE},
 /* 134 */  {"HCHOVCD", "Formaldehyde (HCHO) Vertical Column Density", "mol/(cm^2)", UC_NONE},
};

/* GRIB2 Code table 4.2 : 0.190 */
static const GRIB2ParmTable MeteoText[] = {
   /* 0 */ {"", "Arbitrary text string", "CCITTIA5", UC_NONE},
};

/* GRIB2 Code table 4.2 : 0.191 */
static const GRIB2ParmTable MeteoMisc[] = {
   /* 0 */ {"TSEC", "Seconds prior to initial reference time (defined in Section"
            " 1)", "s", UC_NONE},
};

/* GRIB2 Code table 4.2 : 1.0 */
static const GRIB2ParmTable HydroBasic[] = {
   /* 0 */ {"FFLDG", "Flash flood guidance", "kg/(m^2)", UC_NONE},
   /* 1 */ {"FFLDRO", "Flash flood runoff", "kg/(m^2)", UC_NONE},
   /* 2 */ {"RSSC", "Remotely sensed snow cover", "50=no-snow/no-cloud; "
            "100=Clouds; 250=Snow; 255=missing", UC_NONE},
   /* 3 */ {"ESCT", "Elevation of snow covered terrain", "0-90=elevation in "
            "increments of 100m; 254=clouds; 255=missing", UC_NONE},
   /* 4 */ {"SWEPON", "Snow water equivalent percent of normal", "%", UC_NONE},
/* 5 */    {"BGRUN", "Baseflow-groundwater runoff", "kg/(m^2)", UC_NONE},
/* 6 */    {"SSRUN", "Storm surface runoff", "kg/(m^2)", UC_NONE},
};

/* GRIB2 Code table 4.2 : 1.1 */
static const GRIB2ParmTable HydroProb[] = {
   /* 0 */ {"CPPOP", "Conditional percent precipitation amount fractile for an "
            "overall period", "kg/(m^2)", UC_NONE},
   /* 1 */ {"PPOSP", "Percent precipitation in a sub-period of an overall period",
            "%", UC_NONE},
   /* 2 */ {"PoP", "Probability of 0.01 inch of precipitation", "%", UC_NONE},
};

/* GRIB2 Code table 4.2 : 2.0 */
static const GRIB2ParmTable LandVeg[] = {
   /* 0 */ {"LAND", "Land cover (1=land; 2=sea)", "Proportion", UC_NONE},
   /* 1 */ {"SFCR", "Surface roughness", "m", UC_NONE}, /*NCEP override SFRC? */
   /* 2 */ {"TSOIL", "Soil temperature", "K", UC_NONE},
   /* 3 */ {"SOILM", "Soil moisture content", "kg/(m^2)", UC_NONE},
   /* 4 */ {"VEG", "Vegetation", "%", UC_NONE},
   /* 5 */ {"WATR", "Water runoff", "kg/(m^2)", UC_NONE},
   /* 6 */ {"EVAPT", "Evapotranspiration", "1/(kg^2 s)", UC_NONE},
   /* 7 */ {"MTERH", "Model terrain height", "m", UC_NONE},
   /* 8 */ {"LANDU", "Land use", "1=Urban land; 2=agriculture; 3=Range Land; "
            "4=Deciduous forest; 5=Coniferous forest; 6=Forest/wetland; "
            "7=Water; 8=Wetlands; 9=Desert; 10=Tundra; 11=Ice; "
            "12=Tropical forest; 13=Savannah", UC_NONE},
/* 9 */    {"SOILW", "Volumetric soil moisture content", "Proportion", UC_NONE},
/* 10 */   {"GFLUX", "Ground heat flux", "W/(m^2)", UC_NONE},
/* 11 */   {"MSTAV", "Moisture availability", "%", UC_NONE},
/* 12 */   {"SFEXC", "Exchange coefficient", "(kg/(m^3))(m/s)", UC_NONE},
/* 13 */   {"CNWAT", "Plant canopy surface water", "kg/(m^2)", UC_NONE},
/* 14 */   {"BMIXL", "Blackadar's mixing length scale", "m", UC_NONE},
/* 15 */   {"CCOND", "Canopy conductance", "m/s", UC_NONE},
/* 16 */   {"RSMIN", "Minimal stomatal resistance", "s/m", UC_NONE},
/* 17 */   {"WILT", "Wilting point", "Proportion", UC_NONE},
/* 18 */   {"RCS", "Solar parameter in canopy conductance", "Proportion", UC_NONE},
/* 19 */   {"RCT", "Temperature parameter in canopy conductance", "Proportion", UC_NONE},
/* 20 */   {"RCSOL", "Soil moisture parameter in canopy conductance", "Proportion", UC_NONE},
/* 21 */   {"RCQ", "Humidity parameter in canopy conductance", "Proportion", UC_NONE},
/* 22 */   {"SOILM", "Soil moisture", "kg/m^3", UC_NONE},
/* 23 */   {"CISOILW", "Column-integrated soil water", "kg/m^2", UC_NONE},
/* 24 */   {"HFLUX", "Heat flux", "W/m^2", UC_NONE},
/* 25 */   {"VSOILM", "Volumetric soil moisture", "m^3/m^3", UC_NONE},
/* 26 */   {"WILT", "Wilting point", "kg/m^3", UC_NONE},
/* 27 */   {"VWILTM", "Volumetric wilting moisture", "m^3/m^3", UC_NONE},
};

/* GRIB2 Code table 4.2 : 2.3 */
/* NCEP changed 0 to be "Soil type (as in Zobler)" I ignored them */
static const GRIB2ParmTable LandSoil[] = {
   /* 0 */ {"SOTYP", "Soil type", "1=Sand; 2=Loamy sand; 3=Sandy loam; "
            "4=Silt loam; 5=Organic (redefined); 6=Sandy clay loam; "
            "7=Silt clay loam; 8=Clay loam; 9=Sandy clay; 10=Silty clay; "
            "11=Clay", UC_NONE},
   /* 1 */ {"UPLST", "Upper layer soil temperature", "K", UC_NONE},
   /* 2 */ {"UPLSM", "Upper layer soil moisture", "kg/(m^3)", UC_NONE},
   /* 3 */ {"LOWLSM", "Lower layer soil moisture", "kg/(m^3)", UC_NONE},
   /* 4 */ {"BOTLST", "Bottom layer soil temperature", "K", UC_NONE},
/* 5 */ {"SOILL", "Liquid volumetric soil moisture (non-frozen)", "Proportion", UC_NONE},
/* 6 */ {"RLYRS", "Number of soil layers in root zone", "Numeric", UC_NONE},
/* 7 */ {"SMREF", "Transpiration stress-onset (soil moisture)", "Proportion", UC_NONE},
/* 8 */ {"SMDRY", "Direct evaporation cease (soil moisture)", "Proportion", UC_NONE},
/* 9 */ {"POROS", "Soil porosity", "Proportion", UC_NONE},
/* 10 */ {"LIQVSM", "Liquid volumetric soil moisture (non-frozen)", "m^3/m^3", UC_NONE},
/* 11 */ {"VOLTSO", "Volumetric transpiration stress-onset (soil moisture)", "m^3/m^3", UC_NONE},
/* 12 */ {"TRANSO", "Transpiration stress-onset (soil moisture)", "kg/m^3", UC_NONE},
/* 13 */ {"VOLDEC", "Volumetric direct evaporation cease (soil moisture)", "m^3/m^3", UC_NONE},
/* 14 */ {"DIREC", "Direct evaporation cease (soil moisture)", "kg/m^3", UC_NONE},
/* 15 */ {"SOILP", "Soil porosity", "m^3/m^3", UC_NONE},
/* 16 */ {"VSOSM", "Volumetric saturation of soil moisture", "m^3/m^3", UC_NONE},
/* 17 */ {"SATOSM", "Saturation of soil moisture", "kg/m^3", UC_NONE},
};

/* GRIB2 Code table 4.2 : 3.0 */
static const GRIB2ParmTable SpaceImage[] = {
   /* 0 */ {"SRAD", "Scaled radiance", "Numeric", UC_NONE},
   /* 1 */ {"SALBEDO", "Scaled albedo", "Numeric", UC_NONE},
   /* 2 */ {"SBTMP", "Scaled brightness temperature", "Numeric", UC_NONE},
   /* 3 */ {"SPWAT", "Scaled precipitable water", "Numeric", UC_NONE},
   /* 4 */ {"SLFTI", "Scaled lifted index", "Numeric", UC_NONE},
   /* 5 */ {"SCTPRES", "Scaled cloud top pressure", "Numeric", UC_NONE},
   /* 6 */ {"SSTMP", "Scaled skin temperature", "Numeric", UC_NONE},
   /* 7 */ {"CLOUDM", "Cloud mask", "0=clear over water; 1=clear over land; "
            "2=cloud", UC_NONE},
/* 8 */ {"PIXST", "Pixel scene type", "0=No scene; 1=needle; 2=broad-leafed; "
         "3=Deciduous needle; 4=Deciduous broad-leafed; 5=Deciduous mixed; "
         "6=Closed shrub; 7=Open shrub; 8=Woody savannah; 9=Savannah; "
         "10=Grassland; 11=wetland; 12=Cropland; 13=Urban; 14=crops; "
         "15=snow; 16=Desert; 17=Water; 18=Tundra; 97=Snow on land; "
         "98=Snow on water; 99=Sun-glint; 100=General cloud; "
         "101=fog Stratus; 102=Stratocumulus; 103=Low cloud; "
         "104=Nimbotratus; 105=Altostratus; 106=Medium cloud; 107=Cumulus; "
         "108=Cirrus; 109=High cloud; 110=Unknown cloud", UC_NONE},
/* 9 */ {"FIREDI", "Fire detection indicator", "0=No fire detected; "
         "1=Possible fire detected; 2=Probable fire detected", UC_NONE},
};

/* GRIB2 Code table 4.2 : 3.1 */
static const GRIB2ParmTable SpaceQuantitative[] = {
   /* 0 */ {"ESTP", "Estimated precipitation", "kg/(m^2)", UC_NONE},
/* 1 */ {"IRRATE", "Instantaneous rain rate", "kg/(m^2*s)", UC_NONE},
/* 2 */ {"CTOPH", "Cloud top height", "kg/(m^2*s)", UC_NONE},
/* 3 */ {"CTOPHQI", "Cloud top height quality indicator", "0=Nominal cloud top "
         "height quality; 1=Fog in segment; 2=Poor quality height estimation; "
         "3=Fog in segment and poor quality height estimation", UC_NONE},
/* 4 */ {"ESTUGRD", "Estimated u component of wind", "m/s", UC_NONE},
/* 5 */ {"ESTVGRD", "Estimated v component of wind", "m/s", UC_NONE},
/* 6 */ {"NPIXU", "Number of pixels used", "Numeric", UC_NONE},
/* 7 */ {"SOLZA", "Solar zenith angle", "Degree", UC_NONE},
/* 8 */ {"RAZA", "Relative azimuth angle", "Degree", UC_NONE},
/* 9 */ {"RFL06", "Reflectance in 0.6 micron channel", "%", UC_NONE},
/* 10 */ {"RFL08", "Reflectance in 0.8 micron channel", "%", UC_NONE},
/* 11 */ {"RFL16", "Reflectance in 1.6 micron channel", "%", UC_NONE},
/* 12 */ {"RFL39", "Reflectance in 3.9 micron channel", "%", UC_NONE},
/* 13 */ {"ATMDIV", "Atmospheric divergence", "1/s", UC_NONE},
};

/* GRIB2 Code table 4.2 : 10.0 */
static const GRIB2ParmTable OceanWaves[] = {
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
static const GRIB2ParmTable OceanCurrents[] = {
   /* 0 */ {"DIRC", "Current direction", "Degree true", UC_NONE},
   /* 1 */ {"SPC", "Current speed", "m/s", UC_NONE},
   /* 2 */ {"UOGRD", "u-component of current", "m/s", UC_NONE},
   /* 3 */ {"VOGRD", "v-component of current", "m/s", UC_NONE},
};

/* GRIB2 Code table 4.2 : 10.2 */
static const GRIB2ParmTable OceanIce[] = {
   /* 0 */ {"ICEC", "Ice cover", "Proportion", UC_NONE},
   /* 1 */ {"ICETK", "Ice thinkness", "m", UC_NONE},
   /* 2 */ {"DICED", "Direction of ice drift", "Degree true", UC_NONE},
   /* 3 */ {"SICED", "Speed of ice drift", "m/s", UC_NONE},
   /* 4 */ {"UICE", "u-component of ice drift", "m/s", UC_NONE},
   /* 5 */ {"VICE", "v-component of ice drift", "m/s", UC_NONE},
   /* 6 */ {"ICEG", "Ice growth rate", "m/s", UC_NONE},
   /* 7 */ {"ICED", "Ice divergence", "1/s", UC_NONE},
   /* 8 */ {"ICET", "Ice temperature", "K", UC_NONE},
};

/* GRIB2 Code table 4.2 : 10.3 */
static const GRIB2ParmTable OceanSurface[] = {
   /* 0 */ {"WTMP", "Water temperature", "K", UC_NONE},
   /* 1 */ {"DSLM", "Deviation of sea level from mean", "m", UC_NONE},
};

#ifdef unused
/* GRIB2 Code table 4.2 : 10.4 */
static const GRIB2ParmTable OceanSubSurface[] = {
   /* 0 */ {"MTHD", "Main thermocline depth", "m", UC_NONE},
   /* 1 */ {"MTHA", "Main thermocline anomaly", "m", UC_NONE},
   /* 2 */ {"TTHDP", "Transient thermocline depth", "m", UC_NONE},
   /* 3 */ {"SALTY", "Salinity", "kg/kg", UC_NONE},
};
#endif

/* GRIB2 Code table 4.2 : 10.191 */
static const GRIB2ParmTable OceanMisc[] = {
   /* 0 */ {"TSEC", "Seconds prior to initial reference time (defined in Section"
            " 1)", "s", UC_NONE},
   /* 1 */ {"MOSF", "Meridonal Overturning Stream Function", "m^3/s", UC_NONE},
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
static const GRIB2ParmTable *Choose_GRIB2ParmTable (int prodType, int cat,
                                              size_t *tableLen)
{
   enum { METEO_TEMP = 0, METEO_MOIST = 1, METEO_MOMENT = 2, METEO_MASS = 3,
      METEO_SW_RAD = 4, METEO_LW_RAD = 5, METEO_CLOUD = 6,
      METEO_THERMO_INDEX = 7, METEO_KINEMATIC_INDEX = 8, METEO_TEMP_PROB = 9,
      METEO_MOISTURE_PROB = 10, METEO_MOMENT_PROB = 11, METEO_MASS_PROB = 12,
      METEO_AEROSOL = 13, METEO_GAS = 14, METEO_RADAR = 15,
      METEO_RADAR_IMAGERY = 16, METEO_ELECTRO = 17, METEO_NUCLEAR = 18,
      METEO_ATMOS = 19, METEO_ATMO_CHEM = 20, METEO_CCITT = 190, METEO_MISC = 191,
      METEO_CCITT2 = 253
   };
   enum { HYDRO_BASIC = 0, HYDRO_PROB = 1 };
   enum { LAND_VEG = 0, LAND_SOIL = 3 };
   enum { SPACE_IMAGE = 0, SPACE_QUANTIT = 1 };
   enum { OCEAN_WAVES = 0, OCEAN_CURRENTS = 1, OCEAN_ICE = 2, OCEAN_SURF = 3,
      OCEAN_SUBSURF = 4, OCEAN_MISC = 191
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
               *tableLen = sizeof (MeteoRadarImagery) / sizeof (GRIB2ParmTable);
               return &MeteoRadarImagery[0];
            case METEO_ELECTRO:
               *tableLen = 0;
               return NULL;
            case METEO_NUCLEAR:
               *tableLen = sizeof (MeteoNuclear) / sizeof (GRIB2ParmTable);
               return &MeteoNuclear[0];
            case METEO_ATMOS:
               *tableLen = sizeof (MeteoAtmos) / sizeof (GRIB2ParmTable);
               return &MeteoAtmos[0];
            case METEO_ATMO_CHEM:
               *tableLen = sizeof (MeteoAtmoChem) / sizeof (GRIB2ParmTable);
               return &MeteoAtmoChem[0];
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
            case OCEAN_MISC:
               *tableLen = sizeof (OceanMisc) / sizeof (GRIB2ParmTable);
               return &OceanMisc[0];
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
static const NDFD_AbrevOverideTable NDFD_Overide[] = {
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
   /* 12 */ {"MAXRH", "MaxRH"},                /* Mike added 201202 */
};

static const GRIB2LocalTable NDFD_LclTable[] = {
   /* 0 */ {0, 0, 193, "ApparentT", "Apparent Temperature", "K", UC_K2F},
   /* 1 */ {0, 1, 192, "Wx", "Weather string", "-", UC_NONE},
           {0, 1, 227, "IceAccum", "Ice Accumulation", "kg/m^2", UC_InchWater},
   /* grandfather'ed in a NDFD choice for POP. */
   /* 2 */ {0, 10, 8, "PoP12", "Prob of 0.01 In. of Precip", "%", UC_NONE},
           {0, 13, 194, "smokes", "Surface level smoke from fires",
            "log10(µg/m^3)", UC_LOG10},
           {0, 13, 195, "smokec", "Average vertical column smoke from fires",
            "log10(µg/m^3)", UC_LOG10},
   /* 3 */ {0, 14, 192, "O3MR", "Ozone Mixing Ratio", "kg/kg", UC_NONE},
   /* 4 */ {0, 14, 193, "OZCON", "Ozone Concentration", "PPB", UC_NONE},
   /* Arthur adopted NCEP ozone values from NCEP local table to NDFD local tables. (11/14/2009) */
           {0, 14, 200, "OZMAX1", "Ozone Daily Max from 1-hour Average", "ppbV", UC_NONE},
           {0, 14, 201, "OZMAX8", "Ozone Daily Max from 8-hour Average", "ppbV", UC_NONE},
   /* Added 1/23/2007 in preparation for SPC NDFD Grids */
           {0, 19, 194, "ConvOutlook", "Convective Hazard Outlook", "0=none; 2=tstm; 4=slight; 6=moderate; 8=high", UC_NONE},
           {0, 19, 197, "TornadoProb", "Tornado Probability", "%", UC_NONE},
           {0, 19, 198, "HailProb", "Hail Probability", "%", UC_NONE},
           {0, 19, 199, "WindProb", "Damaging Thunderstorm Wind Probability", "%", UC_NONE},
           {0, 19, 200, "XtrmTornProb", "Extreme Tornado Probability", "%", UC_NONE},
           {0, 19, 201, "XtrmHailProb", "Extreme Hail Probability", "%", UC_NONE},
           {0, 19, 202, "XtrmWindProb", "Extreme Thunderstorm Wind Probability", "%", UC_NONE},
           {0, 19, 215, "TotalSvrProb", "Total Probability of Severe Thunderstorms", "%", UC_NONE},
           {0, 19, 216, "TotalXtrmProb", "Total Probability of Extreme Severe Thunderstorms", "%", UC_NONE},
           {0, 19, 217, "WWA", "Watch Warning Advisory", "-", UC_NONE},
/* Leaving next two lines in for grandfathering sake. 9/19/2007... Probably can remove in future. */
           {0, 19, 203, "TotalSvrProb", "Total Probability of Severe Thunderstorms", "%", UC_NONE},
           {0, 19, 204, "TotalXtrmProb", "Total Probability of Extreme Severe Thunderstorms", "%", UC_NONE},
           {0, 192, 192, "FireWx", "Critical Fire Weather", "%", UC_NONE},
           {0, 192, 194, "DryLightning", "Dry Lightning", "%", UC_NONE},
   /* Mike added 1/13 */
           {2, 1, 192, "CANL", "Cold Advisory for Newborn Livestock", "0=none; 2=slight; 4=mild; 6=moderate; 8=severe; 10=extreme", UC_NONE},
   /* Arthur Added this to both NDFD and NCEP local tables. (5/1/2006) */
           {10, 3, 192, "Surge", "Hurricane Storm Surge", "m", UC_M2Feet},
           {10, 3, 193, "ETSurge", "Extra Tropical Storm Surge", "m", UC_M2Feet},
   /* Mike added 2/2012 */
           {0, 1, 198, "MinRH", "Minimum Relative Humidity", "%", UC_NONE}
};

static const GRIB2LocalTable HPC_LclTable[] = {
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
Updated again on 3/26/2008
*/
static const GRIB2LocalTable NCEP_LclTable[] = {
   /*  0 */ {0, 0, 192, "SNOHF", "Snow Phase Change Heat Flux", "W/(m^2)", UC_NONE},
            {0, 0, 193, "TTRAD", "Temperature tendency by all radiation", "K/s", UC_NONE},
            {0, 0, 194, "REV", "Relative Error Variance", "-", UC_NONE},
            {0, 0, 195, "LRGHR", "Large Scale Condensate Heating rate", "K/s", UC_NONE},
            {0, 0, 196, "CNVHR", "Deep Convective Heating rate", "K/s", UC_NONE},
            {0, 0, 197, "THFLX", "Total Downward Heat Flux at Surface", "W/(m^2)", UC_NONE},
            {0, 0, 198, "TTDIA", "Temperature Tendency By All Physics", "K/s", UC_NONE},
            {0, 0, 199, "TTPHY", "Temperature Tendency By Non-radiation Physics", "K/s", UC_NONE},
            {0, 0, 200, "TSD1D", "Standard Dev. of IR Temp. over 1x1 deg. area", "K", UC_NONE},
            {0, 0, 201, "SHAHR", "Shallow Cnvective Heating rate", "K/s", UC_NONE},
            {0, 0, 202, "VDFHR", "Vertical Diffusion Heating rate", "K/s", UC_NONE},
            {0, 0, 203, "THZ0", "Potential temperature at top of viscus sublayer", "K", UC_NONE},
            {0, 0, 204, "TCHP", "Tropical Cyclone Heat Potential", "J/(m^2*K)", UC_NONE},

   /*  1 */ {0, 1, 192, "CRAIN", "Categorical Rain", "0=no; 1=yes", UC_NONE},
   /*  2 */ {0, 1, 193, "CFRZR", "Categorical Freezing Rain", "0=no; 1=yes", UC_NONE},
   /*  3 */ {0, 1, 194, "CICEP", "Categorical Ice Pellets", "0=no; 1=yes", UC_NONE},
   /*  4 */ {0, 1, 195, "CSNOW", "Categorical Snow", "0=no; 1=yes", UC_NONE},
   /*  5 */ {0, 1, 196, "CPRAT", "Convective Precipitation Rate", "kg/(m^2*s)", UC_NONE},
   /*  6 */ {0, 1, 197, "MCONV", "Horizontal Moisture Divergence", "kg/(kg*s)", UC_NONE},
   /*  7 */ {0, 1, 198, "MINRH", "Minimum Relative Humidity", "%", UC_NONE},
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
            {0, 1, 209, "TCLSW", "Total column-integrated supercooled liquid water", "kg/(m^2)", UC_NONE},
            {0, 1, 210, "TCOLM", "Total column-integrated melting ice", "kg/(m^2)", UC_NONE},
            {0, 1, 211, "EMNP", "Evaporation - Precipitation", "cm/day", UC_NONE},
            {0, 1, 212, "SBSNO", "Sublimination (evaporation from snow)", "W/(m^2)", UC_NONE},
            {0, 1, 213, "CNVMR", "Deep Convective Moistening Rate", "kg/(kg*s)", UC_NONE},
            {0, 1, 214, "SHAMR", "Shallow Convective Moistening Rate", "kg/(kg*s)", UC_NONE},
            {0, 1, 215, "VDFMR", "Vertical Diffusion Moistening Rate", "kg/(kg*s)", UC_NONE},
            {0, 1, 216, "CONDP", "Condensation Pressure of Parcali Lifted From Indicate Surface", "Pa", UC_NONE},
            {0, 1, 217, "LRGMR", "Large scale moistening rate", "kg/(kg/s)", UC_NONE},
            {0, 1, 218, "QZ0", "Specific humidity at top of viscous sublayer", "kg/kg", UC_NONE},
            {0, 1, 219, "QMAX", "Maximum specific humidity at 2m", "kg/kg", UC_NONE},
            {0, 1, 220, "QMIN", "Minimum specific humidity at 2m", "kg/kg", UC_NONE},
            {0, 1, 221, "ARAIN", "Liquid precipitation (rainfall)", "kg/(m^2)", UC_NONE},
            {0, 1, 222, "SNOWT", "Snow temperature, depth-avg", "K", UC_NONE},
            {0, 1, 223, "APCPN", "Total precipitation (nearest grid point)", "kg/(m^2)", UC_NONE},
            {0, 1, 224, "ACPCPN", "Convective precipitation (nearest grid point)", "kg/(m^2)", UC_NONE},
            {0, 1, 225, "FRZR", "Freezing rain", "kg/(m^2)", UC_NONE},
/* It is important to use 'Wx' instead of 'PWTHER' since the rest of the code
 * uses "Wx" to test if it should be dealing with weather strings.  Since these
 * are the same as the NDFD "Wx" strings, it was simpler to maintain the NDFD
 * convention on abbreviations.  We could use 'Predominant Weather' as the long
 * descriptor, but the NDFD 'Weather String' seems quite reasonable. */
/*            {0, 1, 226, "PWTHER", "Predominant Weather", "-", UC_NONE},*/
            {0, 1, 226, "Wx", "Weather String", "-", UC_NONE},

   /* 15 */ {0, 2, 192, "VWSH", "Vertical speed sheer", "1/s", UC_NONE},
   /* 16 */ {0, 2, 193, "MFLX", "Horizontal Momentum Flux", "N/(m^2)", UC_NONE},
   /* 17 */ {0, 2, 194, "USTM", "U-Component Storm Motion", "m/s", UC_NONE},
   /* 18 */ {0, 2, 195, "VSTM", "V-Component Storm Motion", "m/s", UC_NONE},
   /* 19 */ {0, 2, 196, "CD", "Drag Coefficient", "-", UC_NONE},
   /* 20 */ {0, 2, 197, "FRICV", "Frictional Velocity", "m/s", UC_NONE},
            {0, 2, 198, "LAUV", "Latitude of U Wind Component of Velocity", "deg", UC_NONE},
            {0, 2, 199, "LOUV", "Longitude of U Wind Component of Velocity", "deg", UC_NONE},
            {0, 2, 200, "LAVV", "Latitude of V Wind Component of Velocity", "deg", UC_NONE},
            {0, 2, 201, "LOVV", "Longitude of V Wind Component of Velocity", "deg", UC_NONE},
            {0, 2, 202, "LAPP", "Latitude of Presure Point", "deg", UC_NONE},
            {0, 2, 203, "LOPP", "Longitude of Presure Point", "deg", UC_NONE},
            {0, 2, 204, "VEDH", "Vertical Eddy Diffusivity Heat exchange", "m^2/s", UC_NONE},
            {0, 2, 205, "COVMZ", "Covariance between Meridional and Zonal Components of the wind", "m^2/s^2", UC_NONE},
            {0, 2, 206, "COVTZ", "Covariance between Temperature and Zonal Components of the wind", "K*m/s", UC_NONE},
            {0, 2, 207, "COVTM", "Covariance between Temperature and Meridional Components of the wind", "K*m/s", UC_NONE},
            {0, 2, 208, "VDFUA", "Vertical Diffusion Zonal Acceleration", "m/s^2", UC_NONE},
            {0, 2, 209, "VDFVA", "Vertical Diffusion Meridional Acceleration", "m/s^2", UC_NONE},
            {0, 2, 210, "GWDU", "Gravity wave drag zonal acceleration", "m/s^2", UC_NONE},
            {0, 2, 211, "GWDV", "Gravity wave drag meridional acceleration", "m/s^2", UC_NONE},
            {0, 2, 212, "CNVU", "Convective zonal momentum mixing acceleration", "m/s^2", UC_NONE},
            {0, 2, 213, "CNVV", "Convective meridional momentum mixing acceleration", "m/s^2", UC_NONE},
            {0, 2, 214, "WTEND", "Tendency of vertical velocity", "m/s^2", UC_NONE},
            {0, 2, 215, "OMGALF", "Omega (Dp/Dt) divide by density", "K", UC_NONE},
            {0, 2, 216, "CNGWDU", "Convective Gravity wave drag zonal acceleration", "m/s^2", UC_NONE},
            {0, 2, 217, "CNGWDV", "Convective Gravity wave drag meridional acceleration", "m/s^2", UC_NONE},
            {0, 2, 218, "LMV", "Velocity point model surface", "-", UC_NONE},
            {0, 2, 219, "PVMWW", "Potential vorticity (mass-weighted)", "1/(s/m)", UC_NONE},
/* Removed 8/19/2008 */ /*           {0, 2, 220, "MFLX", "Momentum flux", "N/m^2", UC_NONE},*/

   /* 21 */ {0, 3, 192, "MSLET", "MSLP (Eta model reduction)", "Pa", UC_NONE},
   /* 22 */ {0, 3, 193, "5WAVH", "5-Wave Geopotential Height", "gpm", UC_NONE},
   /* 23 */ {0, 3, 194, "U-GWD", "Zonal Flux of Gravity Wave Stress", "N/(m^2)", UC_NONE},
   /* 24 */ {0, 3, 195, "V-GWD", "Meridional Flux of Gravity Wave Stress", "N/(m^2)", UC_NONE},
   /* 25 */ {0, 3, 196, "HPBL", "Planetary Boundary Layer Height", "m", UC_NONE},
   /* 26 */ {0, 3, 197, "5WAVA", "5-Wave Geopotential Height Anomaly", "gpm", UC_NONE},
            {0, 3, 198, "MSLMA", "MSLP (MAPS System Reduction)", "Pa", UC_NONE},
            {0, 3, 199, "TSLSA", "3-hr pressure tendency (Std. Atmos. Reduction)", "Pa/s", UC_NONE},
            {0, 3, 200, "PLPL", "Pressure of level from which parcel was lifted", "Pa", UC_NONE},
            {0, 3, 201, "LPSX", "X-gradiant of Log Pressure", "1/m", UC_NONE},
            {0, 3, 202, "LPSY", "Y-gradiant of Log Pressure", "1/m", UC_NONE},
            {0, 3, 203, "HGTX", "X-gradiant of Height", "1/m", UC_NONE},
            {0, 3, 204, "HGTY", "Y-gradiant of Height", "1/m", UC_NONE},
            {0, 3, 205, "LAYTH", "Layer Thickness", "m", UC_NONE},
            {0, 3, 206, "NLGSP", "Natural Log of Surface Pressure", "ln(kPa)", UC_NONE},
            {0, 3, 207, "CNVUMF", "Convective updraft mass flux", "kg/m^2/s", UC_NONE},
            {0, 3, 208, "CNVDMF", "Convective downdraft mass flux", "kg/m^2/s", UC_NONE},
            {0, 3, 209, "CNVDEMF", "Convective detrainment mass flux", "kg/m^2/s", UC_NONE},
            {0, 3, 210, "LMH", "Mass point model surface", "-", UC_NONE},
            {0, 3, 211, "HGTN", "Geopotential height (nearest grid point)", "gpm", UC_NONE},
            {0, 3, 212, "PRESN", "Pressure (nearest grid point)", "Pa", UC_NONE},

   /* 27 */ {0, 4, 192, "DSWRF", "Downward Short-Wave Rad. Flux", "W/(m^2)", UC_NONE},
   /* 28 */ {0, 4, 193, "USWRF", "Upward Short-Wave Rad. Flux", "W/(m^2)", UC_NONE},
            {0, 4, 194, "DUVB", "UV-B downward solar flux", "W/(m^2)", UC_NONE},
            {0, 4, 195, "CDUVB", "Clear sky UV-B downward solar flux", "W/(m^2)", UC_NONE},
            {0, 4, 196, "CSDSF", "Clear sky Downward Solar Flux", "W/(m^2)", UC_NONE},
            {0, 4, 197, "SWHR", "Solar Radiative Heating Rate", "K/s", UC_NONE},
            {0, 4, 198, "CSUSF", "Clear Sky Upward Solar Flux", "W/(m^2)", UC_NONE},
            {0, 4, 199, "CFNSF", "Cloud Forcing Net Solar Flux", "W/(m^2)", UC_NONE},
            {0, 4, 200, "VBDSF", "Visible Beam Downward Solar Flux", "W/(m^2)", UC_NONE},
            {0, 4, 201, "VDDSF", "Visible Diffuse Downward Solar Flux", "W/(m^2)", UC_NONE},
            {0, 4, 202, "NBDSF", "Near IR Beam Downward Solar Flux", "W/(m^2)", UC_NONE},
            {0, 4, 203, "NDDSF", "Near IR Diffuse Downward Solar Flux", "W/(m^2)", UC_NONE},
            {0, 4, 204, "DTRF", "Downward Total radiation Flux", "W/(m^2)", UC_NONE},
            {0, 4, 205, "UTRF", "Upward Total radiation Flux", "W/(m^2)", UC_NONE},

   /* 29 */ {0, 5, 192, "DLWRF", "Downward Long-Wave Rad. Flux", "W/(m^2)", UC_NONE},
   /* 30 */ {0, 5, 193, "ULWRF", "Upward Long-Wave Rad. Flux", "W/(m^2)", UC_NONE},
            {0, 5, 194, "LWHR", "Long-Wave Radiative Heating Rate", "K/s", UC_NONE},
            {0, 5, 195, "CSULF", "Clear Sky Upward Long Wave Flux", "W/(m^2)", UC_NONE},
            {0, 5, 196, "CSDLF", "Clear Sky Downward Long Wave Flux", "W/(m^2)", UC_NONE},
            {0, 5, 197, "CFNLF", "Cloud Forcing Net Long Wave Flux", "W/(m^2)", UC_NONE},

   /* 31 */ {0, 6, 192, "CDLYR", "Non-Convective Cloud Cover", "%", UC_NONE},
   /* 32 */ {0, 6, 193, "CWORK", "Cloud Work Function", "J/kg", UC_NONE},
   /* 33 */ {0, 6, 194, "CUEFI", "Convective Cloud Efficiency", "-", UC_NONE},
   /* 34 */ {0, 6, 195, "TCOND", "Total Condensate", "kg/kg", UC_NONE},
   /* 35 */ {0, 6, 196, "TCOLW", "Total Column-Integrated Cloud Water", "kg/(m^2)", UC_NONE},
   /* 36 */ {0, 6, 197, "TCOLI", "Total Column-Integrated Cloud Ice", "kg/(m^2)", UC_NONE},
   /* 37 */ {0, 6, 198, "TCOLC", "Total Column-Integrated Condensate", "kg/(m^2)", UC_NONE},
   /* 38 */ {0, 6, 199, "FICE", "Ice fraction of total condensate", "-", UC_NONE},
            {0, 6, 200, "MFLUX", "Convective Cloud Mass Flux", "Pa/s", UC_NONE},
            {0, 6, 201, "SUNSD", "SunShine duration", "s", UC_NONE},

   /* 39 */ {0, 7, 192, "LFTX", "Surface Lifted Index", "K", UC_NONE},
   /* 40 */ {0, 7, 193, "4LFTX", "Best (4 layer) Lifted Index", "K", UC_NONE},
   /* 41 */ {0, 7, 194, "RI", "Richardson Number", "-", UC_NONE},
            {0, 7, 195, "CWDI", "Convective Weather Detection Index", "-", UC_NONE},
            {0, 7, 196, "UVI", "Ultra Violet Index", "W/(m^2)", UC_UVIndex},
            {0, 7, 197, "UPHL", "Updraft Helicity", "m^2/s^2", UC_NONE},
            {0, 7, 198, "LAI", "Leaf area index", "-", UC_NONE},

            {0, 13, 192, "PMTC", "Particulate matter (coarse)", "µg/m^3", UC_NONE},
            {0, 13, 193, "PMTF", "Particulate matter (fine)", "µg/m^3", UC_NONE},
            {0, 13, 194, "LPMTF", "Particulate matter (fine)",
             "log10(µg/m^3)", UC_LOG10},
            {0, 13, 195, "LIPMF", "Integrated column particulate matter " /* over-ride in the code based on surface */
             "(fine)", "log10(µg/m^3)", UC_LOG10},

   /* 42 */ {0, 14, 192, "O3MR", "Ozone Mixing Ratio", "kg/kg", UC_NONE},
   /* 43 */ {0, 14, 193, "OZCON", "Ozone Concentration", "PPB", UC_NONE},
   /* 44 */ {0, 14, 194, "OZCAT", "Categorical Ozone Concentration", "-", UC_NONE},
            {0, 14, 195, "VDFOZ", "Ozone Vertical Diffusion", "kg/kg/s", UC_NONE},
            {0, 14, 196, "POZ", "Ozone Production", "kg/kg/s", UC_NONE},
            {0, 14, 197, "TOZ", "Ozone Tendency", "kg/kg/s", UC_NONE},
            {0, 14, 198, "POZT", "Ozone Production from Temperature Term", "kg/kg/s", UC_NONE},
            {0, 14, 199, "POZO", "Ozone Production from Column Ozone Term", "kg/kg/s", UC_NONE},
            {0, 14, 200, "OZMAX1", "Ozone Daily Max from 1-hour Average", "ppbV", UC_NONE},
            {0, 14, 201, "OZMAX8", "Ozone Daily Max from 8-hour Average", "ppbV", UC_NONE},
            {0, 14, 202, "PDMAX1", "PM 2.5 Daily Max from 1-hour Average", "ug/(m^3)", UC_NONE},
            {0, 14, 203, "PDMAX24", "PM 2.5 Daily Max from 24-hour Average", "ug/(m^3)", UC_NONE},

            {0, 16, 192, "REFZR", "Derived radar reflectivity backscatter from rain", "mm^6/m^3", UC_NONE},
            {0, 16, 193, "REFZI", "Derived radar reflectivity backscatter from ice", "mm^6/m^3", UC_NONE},
            {0, 16, 194, "REFZC", "Derived radar reflectivity backscatter from parameterized convection", "mm^6/m^3", UC_NONE},
            {0, 16, 195, "REFD", "Derived radar reflectivity", "dB", UC_NONE},
            {0, 16, 196, "REFC", "Maximum / Composite radar reflectivity", "dB", UC_NONE},
            {0, 16, 197, "RETOP", "Radar Echo Top (18.3 DBZ)", "m", UC_NONE},

            {0, 17, 192, "LTNG", "Lightning", "-", UC_NONE},

   /* 45 */ {0, 19, 192, "MXSALB", "Maximum Snow Albedo", "%", UC_NONE},
   /* 46 */ {0, 19, 193, "SNFALB", "Snow-Free Albedo", "%", UC_NONE},
            {0, 19, 194, "SRCONO", "Slight risk convective outlook", "categorical", UC_NONE},
            {0, 19, 195, "MRCONO", "Moderate risk convective outlook", "categorical", UC_NONE},
            {0, 19, 196, "HRCONO", "High risk convective outlook", "categorical", UC_NONE},
            {0, 19, 197, "TORPROB", "Tornado probability", "%", UC_NONE},
            {0, 19, 198, "HAILPROB", "Hail probability", "%", UC_NONE},
            {0, 19, 199, "WINDPROB", "Wind probability", "%", UC_NONE},
            {0, 19, 200, "STORPROB", "Significant Tornado probability", "%", UC_NONE},
            {0, 19, 201, "SHAILPRO", "Significant Hail probability", "%", UC_NONE},
            {0, 19, 202, "SWINDPRO", "Significant Wind probability", "%", UC_NONE},
            {0, 19, 203, "TSTMC", "Categorical Thunderstorm", "0=no; 1=yes", UC_NONE},
            {0, 19, 204, "MIXLY", "Number of mixed layers next to surface", "integer", UC_NONE},
            {0, 19, 205, "FLGHT", "Flight Category", "-", UC_NONE},
            {0, 19, 206, "CICEL", "Confidence Ceiling", "-", UC_NONE},
            {0, 19, 207, "CIVIS", "Confidence Visibility", "-", UC_NONE},
            {0, 19, 208, "CIFLT", "Confidence Flight Category", "-", UC_NONE},
            {0, 19, 209, "LAVNI", "Low Level aviation interest", "-", UC_NONE},
            {0, 19, 210, "HAVNI", "High Level aviation interest", "-", UC_NONE},
            {0, 19, 211, "SBSALB", "Visible; Black Sky Albedo", "%", UC_NONE},
            {0, 19, 212, "SWSALB", "Visible; White Sky Albedo", "%", UC_NONE},
            {0, 19, 213, "NBSALB", "Near IR; Black Sky Albedo", "%", UC_NONE},
            {0, 19, 214, "NWSALB", "Near IR; White Sky Albedo", "%", UC_NONE},
            {0, 19, 215, "PRSVR", "Total Probability of Severe Thunderstorms (Days 2,3)", "%", UC_NONE},
            {0, 19, 216, "PRSIGSVR", "Total Probability of Extreme Severe Thunderstorms (Days 2,3)", "%", UC_NONE},
            {0, 19, 217, "SIPD", "Supercooled Large Droplet Icing",
                         "0=None; 1=Light; 2=Moderate; 3=Severe; 4=Trace; 5=Heavy; 255=missing", UC_NONE},
            {0, 19, 218, "EPSR", "Radiative emissivity", "", UC_NONE},
            {0, 19, 219, "TPFI", "Turbulence potential forecast index", "-", UC_NONE},
            {0, 19, 220, "SVRTS", "Categorical Severe Thunderstorm", "0=No; 1=Yes; "
             "2-3=Reserved; 4=Low; 5=Reserved; 6=Medium; 7=Reserved; 8=High; "
             "255=missing", UC_NONE},
            {0, 19, 221, "PROCON", "Probability of Convection", "%", UC_NONE},
            {0, 19, 222, "CONVP", "Convection Potential",  "0=No; 1=Yes; "
             "2-3=Reserved; 4=Low; 5=Reserved; 6=Medium; 7=Reserved; 8=High; "
             "255=missing", UC_NONE},
            {0, 19, 223, "", "Reserved", "-", UC_NONE},
            {0, 19, 224, "", "Reserved", "-", UC_NONE},
            {0, 19, 225, "", "Reserved", "-", UC_NONE},
            {0, 19, 226, "", "Reserved", "-", UC_NONE},
            {0, 19, 227, "", "Reserved", "-", UC_NONE},
            {0, 19, 228, "", "Reserved", "-", UC_NONE},
            {0, 19, 229, "", "Reserved", "-", UC_NONE},
            {0, 19, 230, "", "Reserved", "-", UC_NONE},
            {0, 19, 231, "", "Reserved", "-", UC_NONE},
/* These stopped being used? 3/26/2008 */
/*
            {0, 19, 217, "MEIP", "Mean Icing Potential", "kg/m^2", UC_NONE},
            {0, 19, 218, "MAIP", "Maximum Icing Potential", "kg/m^2", UC_NONE},
            {0, 19, 219, "MECTP", "Mean in-Cloud Turbulence Potential", "kg/m^2", UC_NONE},
            {0, 19, 220, "MACTP", "Max in-Cloud Turbulence Potential", "kg/m^2", UC_NONE},
            {0, 19, 221, "MECAT", "Mean Cloud Air Turbulence Potential", "kg/m^2", UC_NONE},
            {0, 19, 222, "MACAT", "Maximum Cloud Air Turbulence Potential", "kg/m^2", UC_NONE},
            {0, 19, 223, "CBHE", "Cumulonimbus Horizontal Extent", "%", UC_NONE},
            {0, 19, 224, "PCBB", "Pressure at Cumulonimbus Base", "Pa", UC_NONE},
            {0, 19, 225, "PCBT", "Pressure at Cumulonimbus Top", "Pa", UC_NONE},
            {0, 19, 226, "PECBB", "Pressure at Embedded Cumulonimbus Base", "Pa", UC_NONE},
            {0, 19, 227, "PECBT", "Pressure at Embedded Cumulonimbus Top", "Pa", UC_NONE},
            {0, 19, 228, "HCBB", "ICAO Height at Cumulonimbus Base", "m", UC_NONE},
            {0, 19, 229, "HCBT", "ICAO Height at Cumulonimbus Top", "m", UC_NONE},
            {0, 19, 230, "HECBB", "ICAO Height at Embedded Cumulonimbus Base", "m", UC_NONE},
            {0, 19, 231, "HECBT", "ICAO Height at Embedded Cumulonimbus Top", "m", UC_NONE},
*/
            {0, 19, 232, "VAFTD", "Volcanic Ash Forecast Transport and Dispersion", "log10(kg/m^3)", UC_NONE},
            {0, 19, 233, "ICPRB", "Icing probability", "-", UC_NONE},
            {0, 19, 234, "ICSEV", "Icing severity", "-", UC_NONE},

   /* 47 */ {0, 191, 192, "NLAT", "Latitude (-90 to 90)", "deg", UC_NONE},
   /* 48 */ {0, 191, 193, "ELON", "East Longitude (0 to 360)", "deg", UC_NONE},
   /* 49 */ {0, 191, 194, "TSEC", "Seconds prior to initial reference time", "s", UC_NONE},
            {0, 191, 195, "MLYNO", "Model Layer number (From bottom up)", "", UC_NONE},
            {0, 191, 196, "NLATN", "Latitude (nearest neighbor) (-90 to 90)", "deg", UC_NONE},
            {0, 191, 197, "ELONN", "East longitude (nearest neighbor) (0 to 360)", "deg", UC_NONE},

/* table 4.2 : 0.192 according to NCEP is "Covariance". */
            {0, 192, 1, "COVZM", "Covariance between zonal and meridonial components of the wind", "m^2/s^2", UC_NONE},
            {0, 192, 2, "COVTZ", "Covariance between zonal component of the wind and temperature", "K*m/s", UC_NONE},
            {0, 192, 3, "COVTM", "Covariance between meridonial component of the wind and temperature", "K*m/s", UC_NONE},
            {0, 192, 4, "COVTW", "Covariance between temperature and vertical component of the wind", "K*m/s", UC_NONE},
            {0, 192, 5, "COVZZ", "Covariance between zonal and zonal components of the wind", "m^2/s^2", UC_NONE},
            {0, 192, 6, "COVMM", "Covariance between meridonial and meridonial components of the wind", "m^2/s^2", UC_NONE},
            {0, 192, 7, "COVQZ", "Covariance between specific humidity and zonal components of the wind", "kg/kg*m/s", UC_NONE},
            {0, 192, 8, "COVQM", "Covariance between specific humidity and meridonial components of the wind", "kg/kg*m/s", UC_NONE},
            {0, 192, 9, "COVTVV", "Covariance between temperature and vertical components of the wind", "K*Pa/s", UC_NONE},
            {0, 192, 10, "COVQVV", "Covariance between specific humidity and vertical components of the wind", "kg/kg*Pa/s", UC_NONE},
            {0, 192, 11, "COVPSPS", "Covariance between surface pressure and surface pressure", "Pa*Pa", UC_NONE},
            {0, 192, 12, "COVQQ", "Covariance between specific humidity and specific humidity", "kg/kg*kg/kg", UC_NONE},
            {0, 192, 13, "COVVVVV", "Covariance between vertical and vertical components of the wind", "Pa^2/s^2", UC_NONE},
            {0, 192, 14, "COVTT", "Covariance between temperature and temperature", "K*K", UC_NONE},

   /* 50 */ {1, 0, 192, "BGRUN", "Baseflow-Groundwater Runoff", "kg/(m^2)", UC_NONE},
   /* 51 */ {1, 0, 193, "SSRUN", "Storm Surface Runoff", "kg/(m^2)", UC_NONE},

            {1, 1, 192, "CPOZP", "Probability of Freezing Precipitation", "%", UC_NONE},
            {1, 1, 193, "CPOFP", "Probability of Frozen Precipitation", "%", UC_NONE},
            {1, 1, 194, "PPFFG", "Probability of precipitation exceeding flash flood guidance values", "%", UC_NONE},
            {1, 1, 195, "CWR", "Probability of Wetting Rain; exceeding in 0.1 inch in a given time period", "%", UC_NONE},

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
            {2, 0, 208, "AKHS", "Surface exchange coefficients for T and Q divided by delta z", "m/s", UC_NONE},
            {2, 0, 209, "AKMS", "Surface exchange coefficients for U and V divided by delta z", "m/s", UC_NONE},
            {2, 0, 210, "VEGT", "Vegetation canopy temperature", "K", UC_NONE},
            {2, 0, 211, "SSTOR", "Surface water storage", "K g/m^2", UC_NONE},
            {2, 0, 212, "LSOIL", "Liquid soil moisture content (non-frozen)", "K g/m^2", UC_NONE},
            {2, 0, 213, "EWATR", "Open water evaporation (standing water)", "W/m^2", UC_NONE},
            {2, 0, 214, "GWREC", "Groundwater recharge", "kg/m^2", UC_NONE},
            {2, 0, 215, "QREC", "Flood plain recharge", "kg/m^2", UC_NONE},
            {2, 0, 216, "SFCRH", "Roughness length for heat", "m", UC_NONE},
            {2, 0, 217, "NDVI", "Normalized difference vegetation index", "-", UC_NONE},
            {2, 0, 218, "LANDN", "Land-sea coverage (nearest neighbor)", "0=sea; 1=land", UC_NONE},
            {2, 0, 219, "AMIXL", "Asymptotic mixing length scale", "m", UC_NONE},
            {2, 0, 220, "WVINC", "Water vapor added by precip assimilation", "kg/m^2", UC_NONE},
            {2, 0, 221, "WCINC", "Water condensate added by precip assimilation", "kg/m^2", UC_NONE},
            {2, 0, 222, "WVCONV", "Water vapor flux convergence (vertical int)", "kg/m^2", UC_NONE},
            {2, 0, 223, "WCCONV", "Water condensate flux convergence (vertical int)", "kg/m^2", UC_NONE},
            {2, 0, 224, "WVUFLX", "Water vapor zonal flux (vertical int)", "kg/m^2", UC_NONE},
            {2, 0, 225, "WVVFLX", "Water vapor meridional flux (vertical int)", "kg/m^2", UC_NONE},
            {2, 0, 226, "WCUFLX", "Water condensate zonal flux (vertical int)", "kg/m^2", UC_NONE},
            {2, 0, 227, "WCVFLX", "Water condensate meridional flux (vertical int)", "kg/m^2", UC_NONE},
            {2, 0, 228, "ACOND", "Aerodynamic conductance", "m/s", UC_NONE},
            {2, 0, 229, "EVCW", "Canopy water evaporation", "W/(m^2)", UC_NONE},
            {2, 0, 230, "TRANS", "Transpiration", "W/(m^2)", UC_NONE},
           /* Mike added 1/13 */
           {2, 1, 192, "CANL", "Cold Advisory for Newborn Livestock", "0=none; 2=slight; 4=mild; 6=moderate; 8=severe; 10=extreme", UC_NONE},
   /* 66 */ {2, 3, 192, "SOILL", "Liquid Volumetric Soil Moisture (non Frozen)", "Proportion", UC_NONE},
   /* 67 */ {2, 3, 193, "RLYRS", "Number of Soil Layers in Root Zone", "-", UC_NONE},
   /* 68 */ {2, 3, 194, "SLTYP", "Surface Slope Type", "Index", UC_NONE},
   /* 69 */ {2, 3, 195, "SMREF", "Transpiration Stress-onset (soil moisture)", "Proportion", UC_NONE},
   /* 70 */ {2, 3, 196, "SMDRY", "Direct Evaporation Cease (soil moisture)", "Proportion", UC_NONE},
   /* 71 */ {2, 3, 197, "POROS", "Soil Porosity", "Proportion", UC_NONE},
            {2, 3, 198, "EVBS", "Direct evaporation from bare soil", "W/m^2", UC_NONE},
            {2, 3, 199, "LSPA", "Land Surface Precipitation Accumulation", "kg/m^2", UC_NONE},
            {2, 3, 200, "BARET", "Bare soil surface skin temperature", "K", UC_NONE},
            {2, 3, 201, "AVSFT", "Average surface skin temperature", "K", UC_NONE},
            {2, 3, 202, "RADT", "Effective radiative skin temperature", "K", UC_NONE},
            {2, 3, 203, "FLDCP", "Field Capacity", "fraction", UC_NONE},

/* ScatEstUWind -> USCT, ScatEstVWind -> VSCT as of 7/5/2006 (pre 1.80) */
   /* 72 */ {3, 1, 192, "USCT", "Scatterometer Estimated U Wind", "m/s", UC_NONE},
   /* 73 */ {3, 1, 193, "VSCT", "Scatterometer Estimated V Wind", "m/s", UC_NONE},

/* table 4.2 : 3.192 according to NCEP is "Forecast Satellite Imagery". */
            {3, 192, 0, "SBT122", "Simulated Brightness Temperature for GOES 12, Channel 2", "K", UC_NONE},
            {3, 192, 1, "SBT123", "Simulated Brightness Temperature for GOES 12, Channel 3", "K", UC_NONE},
            {3, 192, 2, "SBT124", "Simulated Brightness Temperature for GOES 12, Channel 4", "K", UC_NONE},
            {3, 192, 3, "SBT125", "Simulated Brightness Temperature for GOES 12, Channel 5", "K", UC_NONE},
            {3, 192, 4, "SBC123", "Simulated Brightness Counts for GOES 12, Channel 3", "numeric", UC_NONE},
            {3, 192, 5, "SBC124", "Simulated Brightness Counts for GOES 12, Channel 4", "numeric", UC_NONE},

           {10, 0, 192, "WSTP", "Wave Steepness", "0", UC_NONE},

/* The following entry was moved to 10,3,196 */
/*
           {10, 1, 192, "P2OMLT", "Ocean Mixed Layer Potential Density (Reference 2000m)", "kg/(m^3)", UC_NONE},
*/
           {10, 1, 192, "OMLU", "Ocean Mixed Layer U Velocity", "m/s", UC_NONE},
           {10, 1, 193, "OMLV", "Ocean Mixed Layer V Velocity", "m/s", UC_NONE},
           {10, 1, 194, "UBARO", "Barotropic U Velocity", "m/s", UC_NONE},
           {10, 1, 195, "VBARO", "Barotropic V Velocity", "m/s", UC_NONE},

   /* Arthur Added this to both NDFD and NCEP local tables. (5/1/2006) */
           {10, 3, 192, "SURGE", "Hurricane Storm Surge", "m", UC_M2Feet},
           {10, 3, 193, "ETSRG", "Extra Tropical Storm Surge", "m", UC_M2Feet},
           {10, 3, 194, "ELEV", "Ocean Surface Elevation Relative to Geoid", "m", UC_NONE},
           {10, 3, 195, "SSHG", "Sea Surface Height Relative to Geoid", "m", UC_NONE},
/* The following entry were moved to 10,4,192, 10,4,193 */
/*
           {10, 3, 196, "WTMPC", "3-D Temperature", "deg C", UC_NONE},
           {10, 3, 197, "SALIN", "3-D Salinity", "", UC_NONE},
*/
           {10, 3, 196, "P2OMLT", "Ocean Mixed Layer Potential Density (Reference 2000m)", "kg/(m^3)", UC_NONE},
           {10, 3, 197, "AOHFLX", "Net Air-Ocean Heat Flux", "W/(m^2)", UC_NONE},
           {10, 3, 198, "ASHFL", "Assimilative Heat Flux", "W/(m^2)", UC_NONE},
           {10, 3, 199, "SSTT", "Surface Temperature Trend", "degree/day", UC_NONE},
           {10, 3, 200, "SSST", "Surface Salinity Trend", "psu/day", UC_NONE},
           {10, 3, 201, "KENG", "Kinetic Energy", "J/kg", UC_NONE},
           {10, 3, 202, "SLTFL", "Salt Flux", "kg/(m^2*s)", UC_NONE},

           {10, 3, 242, "TCSRG20", "20% Tropical Cyclone Storm Surge Exceedance", "m", UC_M2Feet},
           {10, 3, 243, "TCSRG30", "30% Tropical Cyclone Storm Surge Exceedance", "m", UC_M2Feet},
           {10, 3, 244, "TCSRG40", "40% Tropical Cyclone Storm Surge Exceedance", "m", UC_M2Feet},
           {10, 3, 245, "TCSRG50", "50% Tropical Cyclone Storm Surge Exceedance", "m", UC_M2Feet},
           {10, 3, 246, "TCSRG60", "60% Tropical Cyclone Storm Surge Exceedance", "m", UC_M2Feet},
           {10, 3, 247, "TCSRG70", "70% Tropical Cyclone Storm Surge Exceedance", "m", UC_M2Feet},
           {10, 3, 248, "TCSRG80", "80% Tropical Cyclone Storm Surge Exceedance", "m", UC_M2Feet},
           {10, 3, 249, "TCSRG90", "90% Tropical Cyclone Storm Surge Exceedance", "m", UC_M2Feet},

           {10, 4, 192, "WTMPC", "3-D Temperature", "deg C", UC_NONE},
           {10, 4, 193, "SALIN", "3-D Salinity", "", UC_NONE},
           {10, 4, 194, "BKENG", "Barotropic Kinetic Energy", "J/kg", UC_NONE},
           {10, 4, 195, "DBSS", "Geometric Depth Below Sea Surface", "m", UC_NONE},
           {10, 4, 196, "INTFD", "Interface Depths", "m", UC_NONE},
           {10, 4, 197, "OHC", "Ocean Heat Content", "J/m^2", UC_NONE},
};

/*
  From http://www.nssl.noaa.gov/projects/mrms/operational/tables.php
*/
static const GRIB2LocalTable MRMS_LclTable[] = {
   /* 0 */ {209, 2, 0, "LightningDensityNLDN1min", "CG Lightning Density 1-min - NLDN", "flashes/km^2/min", UC_NONE},
   /* 1 */ {209, 2, 1, "LightningDensityNLDN5min", "CG Lightning Density 5-min - NLDN", "flashes/km^2/min", UC_NONE},
   /* 2 */ {209, 2, 2, "LightningDensityNLDN15min", "CG Lightning Density 15-min - NLDN", "flashes/km^2/min", UC_NONE},
   /* 3 */ {209, 2, 3, "LightningDensityNLDN30min", "CG Lightning Density 30-min - NLDN", "flashes/km^2/min", UC_NONE},
   /* 4 */ {209, 2, 4, "LightningProbabilityNext30min", "Lightning Probability 0-30 minutes - NLDN", "%", UC_NONE},
   /* 5 */ {209, 3, 0, "MergedAzShear0to2kmAGL", "Azimuth Shear 0-2km AGL", ".001/s", UC_NONE},
   /* 6 */ {209, 3, 1, "MergedAzShear3to6kmAGL", "Azimuth Shear 3-6km AGL", ".001/s", UC_NONE},
   /* 7 */ {209, 3, 2, "RotationTrack30min", "Rotation Track 0-2km AGL 30-min", ".001/s", UC_NONE},
   /* 8 */ {209, 3, 3, "RotationTrack60min", "Rotation Track 0-2km AGL 60-min", ".001/s", UC_NONE},
   /* 9 */ {209, 3, 4, "RotationTrack120min", "Rotation Track 0-2km AGL 120-min", ".001/s", UC_NONE},
   /* 10 */ {209, 3, 5, "RotationTrack240min", "Rotation Track 0-2km AGL 240-min", ".001/s", UC_NONE},
   /* 11 */ {209, 3, 6, "RotationTrack360min", "Rotation Track 0-2km AGL 360-min", ".001/s", UC_NONE},
   /* 12 */ {209, 3, 7, "RotationTrack1440min", "Rotation Track 0-2km AGL 1440-min", ".001/s", UC_NONE},
   /* 13 */ {209, 3, 14, "RotationTrackML30min", "Rotation Track 0-2km AGL 30-min", ".001/s", UC_NONE},
   /* 14 */ {209, 3, 15, "RotationTrackML60min", "Rotation Track 0-2km AGL 60-min", ".001/s", UC_NONE},
   /* 15 */ {209, 3, 16, "RotationTrackML120min", "Rotation Track 0-2km AGL 120-min", ".001/s", UC_NONE},
   /* 16 */ {209, 3, 17, "RotationTrackML240min", "Rotation Track 0-2km AGL 240-min", ".001/s", UC_NONE},
   /* 17 */ {209, 3, 18, "RotationTrackML360min", "Rotation Track 0-2km AGL 360-min", ".001/s", UC_NONE},
   /* 18 */ {209, 3, 19, "RotationTrackML1440min", "Rotation Track 0-2km AGL 1440-min", ".001/s", UC_NONE},
   /* 19 */ {209, 3, 26, "SHI", "Severe Hail Index", "index", UC_NONE},
   /* 20 */ {209, 3, 27, "POSH", "Prob of Severe Hail", "%", UC_NONE},
   /* 21 */ {209, 3, 28, "MESH", "Maximum Estimated Size of Hail (MESH)", "mm", UC_NONE},
   /* 22 */ {209, 3, 29, "MESHMax30min", "MESH Hail Swath 30-min", "mm", UC_NONE},
   /* 23 */ {209, 3, 30, "MESHMax60min", "MESH Hail Swath 60-min", "mm", UC_NONE},
   /* 24 */ {209, 3, 31, "MESHMax120min", "MESH Hail Swath 120-min", "mm", UC_NONE},
   /* 25 */ {209, 3, 32, "MESHMax240min", "MESH Hail Swath 240-min", "mm", UC_NONE},
   /* 26 */ {209, 3, 33, "MESHMax360min", "MESH Hail Swath 360-min", "mm", UC_NONE},
   /* 27 */ {209, 3, 34, "MESHMax1440min", "MESH Hail Swath 1440-min", "mm", UC_NONE},
   /* 28 */ {209, 3, 41, "VIL", "Vertically Integrated Liquid", "kg/m^2", UC_NONE},
   /* 29 */ {209, 3, 42, "VILDensity", "Vertically Integrated Liquid Density", "g/m^3", UC_NONE},
   /* 30 */ {209, 3, 43, "VII", "Vertically Integrated Ice", "kg/m^2", UC_NONE},
   /* 31 */ {209, 3, 44, "EchoTop18", "Echo Top - 18 dBZ", "km", UC_NONE},
   /* 32 */ {209, 3, 45, "EchoTop30", "Echo Top - 30 dBZ", "km", UC_NONE},
   /* 33 */ {209, 3, 46, "EchoTop50", "Echo Top - 50 dBZ", "km", UC_NONE},
   /* 34 */ {209, 3, 47, "EchoTop60", "Echo Top - 60 dBZ", "km", UC_NONE},
   /* 35 */ {209, 3, 48, "H50AboveM20C", "Thickness [50 dBZ top - (-20C)]", "km", UC_NONE},
   /* 36 */ {209, 3, 49, "H50Above0C", "Thickness [50 dBZ top - 0C]", "km", UC_NONE},
   /* 37 */ {209, 3, 50, "H60AboveM20C", "Thickness [60 dBZ top - (-20C)]", "km", UC_NONE},
   /* 38 */ {209, 3, 51, "H60Above0C", "Thickness [60 dBZ top - 0C]", "km", UC_NONE},
   /* 39 */ {209, 3, 52, "Reflectivity0C", "Isothermal Reflectivity at 0C", "dBZ", UC_NONE},
   /* 40 */ {209, 3, 53, "ReflectivityM5C", "Isothermal Reflectivity at -5C", "dBZ", UC_NONE},
   /* 41 */ {209, 3, 54, "ReflectivityM10C", "Isothermal Reflectivity at -10C", "dBZ", UC_NONE},
   /* 42 */ {209, 3, 55, "ReflectivityM15C", "Isothermal Reflectivity at -15C", "dBZ", UC_NONE},
   /* 43 */ {209, 3, 56, "ReflectivityM20C", "Isothermal Reflectivity at -20C", "dBZ", UC_NONE},
   /* 44 */ {209, 3, 57, "ReflectivityAtLowestAltitude", "ReflectivityAtLowestAltitude", "dBZ", UC_NONE},
   /* 45 */ {209, 3, 58, "MergedReflectivityAtLowestAltitude", "Non Quality Controlled Reflectivity At Lowest Altitude", "dBZ", UC_NONE},
   /* 46 */ {209, 4, 0, "IRband4", "Infrared (E/W blend)", "K", UC_NONE},
   /* 47 */ {209, 4, 1, "Visible", "Visible (E/W blend)", "non-dim", UC_NONE},
   /* 48 */ {209, 4, 2, "WaterVapor", "Water Vapor (E/W blend)", "K", UC_NONE},
   /* 49 */ {209, 4, 3, "CloudCover", "Cloud Cover", "K", UC_NONE},
   /* 50 */ {209, 6, 0, "PrecipFlag", "Surface Precipitation Type (Convective; Stratiform; Tropical; Hail; Snow)", "flag", UC_NONE},
   /* 51 */ {209, 6, 1, "PrecipRate", "Radar Precipitation Rate", "mm/hr", UC_NONE},
   /* 52 */ {209, 6, 2, "RadarOnlyQPE01H", "Radar Precipitation Accumulation 1-hour", "mm", UC_NONE},
   /* 53 */ {209, 6, 3, "RadarOnlyQPE03H", "Radar Precipitation Accumulation 3-hour", "mm", UC_NONE},
   /* 54 */ {209, 6, 4, "RadarOnlyQPE06H", "Radar Precipitation Accumulation 6-hour", "mm", UC_NONE},
   /* 55 */ {209, 6, 5, "RadarOnlyQPE12H", "Radar Precipitation Accumulation 12-hour", "mm", UC_NONE},
   /* 56 */ {209, 6, 6, "RadarOnlyQPE24H", "Radar Precipitation Accumulation 24-hour", "mm", UC_NONE},
   /* 57 */ {209, 6, 7, "RadarOnlyQPE48H", "Radar Precipitation Accumulation 48-hour", "mm", UC_NONE},
   /* 58 */ {209, 6, 8, "RadarOnlyQPE72H", "Radar Precipitation Accumulation 72-hour", "mm", UC_NONE},
   /* 59 */ {209, 6, 9, "GaugeCorrQPE01H", "Local Gauge Bias Corrected Radar Precipitation Accumulation 1-hour", "mm", UC_NONE},
   /* 60 */ {209, 6, 10, "GaugeCorrQPE03H", "Local Gauge Bias Corrected Radar Precipitation Accumulation 3-hour", "mm", UC_NONE},
   /* 61 */ {209, 6, 11, "GaugeCorrQPE06H", "Local Gauge Bias Corrected Radar Precipitation Accumulation 6-hour", "mm", UC_NONE},
   /* 62 */ {209, 6, 12, "GaugeCorrQPE12H", "Local Gauge Bias Corrected Radar Precipitation Accumulation 12-hour", "mm", UC_NONE},
   /* 63 */ {209, 6, 13, "GaugeCorrQPE24H", "Local Gauge Bias Corrected Radar Precipitation Accumulation 24-hour", "mm", UC_NONE},
   /* 64 */ {209, 6, 14, "GaugeCorrQPE48H", "Local Gauge Bias Corrected Radar Precipitation Accumulation 48-hour", "mm", UC_NONE},
   /* 65 */ {209, 6, 15, "GaugeCorrQPE72H", "Local Gauge Bias Corrected Radar Precipitation Accumulation 72-hour", "mm", UC_NONE},
   /* 66 */ {209, 6, 16, "GaugeOnlyQPE01H", "Gauge Only Precipitation Accumulation 1-hour", "mm", UC_NONE},
   /* 67 */ {209, 6, 17, "GaugeOnlyQPE03H", "Gauge Only Precipitation Accumulation 3-hour", "mm", UC_NONE},
   /* 68 */ {209, 6, 18, "GaugeOnlyQPE06H", "Gauge Only Precipitation Accumulation 6-hour", "mm", UC_NONE},
   /* 69 */ {209, 6, 19, "GaugeOnlyQPE12H", "Gauge Only Precipitation Accumulation 12-hour", "mm", UC_NONE},
   /* 70 */ {209, 6, 20, "GaugeOnlyQPE24H", "Gauge Only Precipitation Accumulation 24-hour", "mm", UC_NONE},
   /* 71 */ {209, 6, 21, "GaugeOnlyQPE48H", "Gauge Only Precipitation Accumulation 48-hour", "mm", UC_NONE},
   /* 72 */ {209, 6, 22, "GaugeOnlyQPE72H", "Gauge Only Precipitation Accumulation 72-hour", "mm", UC_NONE},
   /* 73 */ {209, 6, 23, "MountainMapperQPE01H", "Mountain Mapper Precipitation Accumulation 1-hour", "mm", UC_NONE},
   /* 74 */ {209, 6, 24, "MountainMapperQPE03H", "Mountain Mapper Precipitation Accumulation 3-hour", "mm", UC_NONE},
   /* 75 */ {209, 6, 25, "MountainMapperQPE06H", "Mountain Mapper Precipitation Accumulation 6-hour", "mm", UC_NONE},
   /* 76 */ {209, 6, 26, "MountainMapperQPE12H", "Mountain Mapper Precipitation Accumulation 12-hour", "mm", UC_NONE},
   /* 77 */ {209, 6, 27, "MountainMapperQPE24H", "Mountain Mapper Precipitation Accumulation 24-hour", "mm", UC_NONE},
   /* 78 */ {209, 6, 28, "MountainMapperQPE48H", "Mountain Mapper Precipitation Accumulation 48-hour", "mm", UC_NONE},
   /* 79 */ {209, 6, 29, "MountainMapperQPE72H", "Mountain Mapper Precipitation Accumulation 72-hour", "mm", UC_NONE},
   /* 80 */ {209, 7, 0, "ModelSurfaceTemp", "Model Surface Temperature [RAP 13km]", "C", UC_NONE},
   /* 81 */ {209, 7, 1, "ModelWetBulbTemp", "Model Surface Wet Bulb Temperature [RAP 13km]", "C", UC_NONE},
   /* 82 */ {209, 7, 2, "WarmRainProbability", "Probability of Warm Rain [RAP 13km derived]", "%", UC_NONE},
   /* 83 */ {209, 7, 3, "ModelHeight0C", "Model Freezing Level Height [RAP 13km]", "m", UC_NONE},
   /* 84 */ {209, 7, 4, "BrightBandTopHeight", "Brightband Top Radar [RAP 13km derived]", "m", UC_NONE},
   /* 85 */ {209, 7, 5, "BrightBandBottomHeight", "Brightband Bottom Radar [RAP 13km derived]", "m", UC_NONE},
   /* 86 */ {209, 8, 0, "RadarQualityIndex", "Radar Quality Index", "non-dim", UC_NONE},
   /* 87 */ {209, 8, 1, "GaugeInflIndex01H", "Gauge Influence Index for 1-hour QPE", "non-dim", UC_NONE},
   /* 88 */ {209, 8, 2, "GaugeInflIndex03H", "Gauge Influence Index for 3-hour QPE", "non-dim", UC_NONE},
   /* 89 */ {209, 8, 3, "GaugeInflIndex06H", "Gauge Influence Index for 6-hour QPE", "non-dim", UC_NONE},
   /* 90 */ {209, 8, 4, "GaugeInflIndex12H", "Gauge Influence Index for 12-hour QPE", "non-dim", UC_NONE},
   /* 91 */ {209, 8, 5, "GaugeInflIndex24H", "Gauge Influence Index for 24-hour QPE", "non-dim", UC_NONE},
   /* 92 */ {209, 8, 6, "GaugeInflIndex48H", "Gauge Influence Index for 48-hour QPE", "non-dim", UC_NONE},
   /* 93 */ {209, 8, 7, "GaugeInflIndex72H", "Gauge Influence Index for 72-hour QPE", "non-dim", UC_NONE},
   /* 94 */ {209, 8, 8, "SeamlessHSR", "Seamless Hybrid Scan Reflectivity with VPR Correction", "dBZ", UC_NONE},
   /* 95 */ {209, 8, 9, "SeamlessHSRHeight", "Height of Seamless Hybrid Scan Reflectivity", "km", UC_NONE},
   /* 96 */ {209, 9, 0, "CONUSMergedReflectivityQC", "WSR-88D 3D Reflectivity Mosaic - 33 CAPPIS (500-19000m)", "dBZ", UC_NONE},
   /* 97 */ {209, 9, 1, "CONUSPlusMergedReflectivityQC", "All Radar 3D Reflectivity Mosaic - 33 CAPPIS (500-19000m)", "dBZ", UC_NONE},
   /* 98 */ {209, 10, 0, "MergedReflectivityQCComposite", "Composite Reflectivity Mosaic (optimal method)", "dBZ", UC_NONE},
   /* 99 */ {209, 10, 1, "HeightCompositeReflectivity", "Height of Composite Reflectivity Mosaic (optimal method)", "m", UC_NONE},
   /* 100 */ {209, 10, 2, "LowLevelCompositeReflectivity", "Low-Level Composite Reflectivity Mosaic (0-4 km)", "dBZ", UC_NONE},
   /* 101 */ {209, 10, 3, "HeightLowLevelCompositeReflectivity", "Height of Low-Level Composite Reflectivity Mosaic (0-4 km)", "m", UC_NONE},
   /* 102 */ {209, 10, 4, "LayerCompositeReflectivity_Low", "Layer Composite Reflectivity Mosaic 0-24kft (low altitude)", "dBZ", UC_NONE},
   /* 103 */ {209, 10, 5, "LayerCompositeReflectivity_High", "Layer Composite Reflectivity Mosaic 24-60kft (highest altitude)", "dBZ", UC_NONE},
   /* 104 */ {209, 10, 6, "LayerCompositeReflectivity_Super", "Layer Composite Reflectivity Mosaic 33-60kft (super high altitude)", "dBZ", UC_NONE},
   /* 105 */ {209, 10, 7, "ReflectivityCompositeHourlyMax", "Composite Reflectivity Hourly Maximum", "dBZ", UC_NONE},
   /* 106 */ {209, 10, 8, "ReflectivityMaxAboveM10C", "Maximum Reflectivity at -10 deg C height and above", "dBZ", UC_NONE},
   /* 107 */ {209, 11, 0, "MergedBaseReflectivityQC", "Mosaic Base Reflectivity (optimal method)", "dBZ", UC_NONE},
   /* 108 */ {209, 11, 1, "MergedReflectivityComposite", "UnQC'd Composite Reflectivity Mosaic (max ref)", "dBZ", UC_NONE},
   /* 109 */ {209, 11, 2, "MergedReflectivityQComposite", "Composite Reflectivity Mosaic (max ref)", "dBZ", UC_NONE},
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
static const GRIB2LocalTable *Choose_LocalParmTable (unsigned short int center,
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
      case 161:
         *tableLen = sizeof (MRMS_LclTable) / sizeof (GRIB2LocalTable);
         return &MRMS_LclTable[0];
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
 * ASCII string abbreviation of that variable.
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
                          uChar timeRangeUnit,
                          uChar timeIncrType,
                          CPL_UNUSED uChar genID,
                          uChar probType,
                          double lowerProb, double upperProb, char **name,
                          char **comment, char **unit, int *convert)
{
   const GRIB2ParmTable *table;
   const GRIB2LocalTable *local;
   size_t tableLen;
   size_t i;
   char f_isNdfd = IsData_NDFD (center, subcenter);
   char f_isMos = IsData_MOS (center, subcenter);

   *unit = (char *) malloc (strlen ("[%]") + 1);
   strcpy (*unit, "[%]");

   {
      // 25.4 mm = 1 inch
      const double tmp = upperProb * 25.4;

      // TODO(schwehr): Make a function and reuse it for other limit checks.
      if (upperProb > tmp ||
          tmp > std::numeric_limits<int>::max() ||
          tmp < std::numeric_limits<int>::min() ||
          CPLIsNan(tmp) ) {
         // TODO(schwehr): What is the correct response?
         errSprintf ("ERROR: upperProb out of range.  Setting to 0.\n");
         upperProb = 0.0;
      }
   }

   if (f_isNdfd || f_isMos) {
      /* Deal with NDFD/MOS handling of Prob Precip_Tot -> PoP12 */
      if ((prodType == 0) && (cat == 1) && (subcat == 8)) {
         if (probType == 0) {
            if (lenTime > 0) {
               if (timeRangeUnit == 3) {
                  mallocSprintf (name, "ProbPrcpBlw%02dm", lenTime);
                  mallocSprintf (comment, "%02d mon Prob of Precip below average", lenTime);
               } else if (timeRangeUnit == 4) {
                  mallocSprintf (name, "ProbPrcpBlw%02dy", lenTime);
                  mallocSprintf (comment, "%02d yr Prob of Precip below average", lenTime);
               } else {
                  mallocSprintf (name, "ProbPrcpBlw%02d", lenTime);
                  mallocSprintf (comment, "%02d hr Prob of Precip below average", lenTime);
               }
            } else {
               mallocSprintf (name, "ProbPrcpBlw");
               mallocSprintf (comment, "Prob of precip below average");
            }
            *convert = UC_NONE;
         } else if (probType == 3) {
            if (lenTime > 0) {
               if (timeRangeUnit == 3) {
                  mallocSprintf (name, "ProbPrcpAbv%02dm", lenTime);
                  mallocSprintf (comment, "%02d mon Prob of Precip above average", lenTime);
               } else if (timeRangeUnit == 4) {
                  mallocSprintf (name, "ProbPrcpAbv%02dy", lenTime);
                  mallocSprintf (comment, "%02d yr Prob of Precip above average", lenTime);
               } else {
                  mallocSprintf (name, "ProbPrcpAbv%02d", lenTime);
                  mallocSprintf (comment, "%02d hr Prob of Precip above average", lenTime);
               }
            } else {
               mallocSprintf (name, "ProbPrcpAbv");
               mallocSprintf (comment, "Prob of precip above average");
            }
            *convert = UC_NONE;
         } else {
            myAssert (probType == 1);
            if (lenTime > 0) {
               if (timeRangeUnit == 3) {
                  if (upperProb != (double) .254) {
                     mallocSprintf (name, "PoP%02dm-%03d", lenTime, (int) (upperProb / .254 + .5));
                  } else {
                     mallocSprintf (name, "PoP%02dm", lenTime);
                  }
                  mallocSprintf (comment, "%02d mon Prob of Precip > %g In.", lenTime, upperProb / 25.4);
               } else if (timeRangeUnit == 4) {
                  if (upperProb != (double) .254) {
                     mallocSprintf (name, "PoP%02dy-%03d", lenTime, (int) (upperProb / .254 + .5));
                  } else {
                     mallocSprintf (name, "PoP%02dy", lenTime);
                  }
                  mallocSprintf (comment, "%02d yr Prob of Precip > %g In.", lenTime, upperProb / 25.4);
               } else {
                  /* The 300 is to deal with an old NDFD encoding bug from 2002:
                   * PDS-S4 | Upper limit (scale value, scale factor) | 300 (3, -2)
                   * 25.4 mm = 1 inch.  Rain typically .01 inches = .254 mm
                   */
                  if ((upperProb != (double) .254) && (upperProb != (double) 300)) {
                     mallocSprintf (name, "PoP%02d-%03d", lenTime, (int) (upperProb / .254 + .5));
                  } else {
                     mallocSprintf (name, "PoP%02d", lenTime);
                  }
                  if (upperProb != (double) 300) {
                     mallocSprintf (comment, "%02d hr Prob of Precip > %g In.", lenTime, upperProb / 25.4);
                  } else {
                     mallocSprintf (comment, "%02d hr Prob of Precip > 0.01 In.", lenTime);
                  }
               }
            } else {
               if (upperProb != (double) .254) {
                  mallocSprintf (name, "PoP-p%03d", (int) (upperProb / .254 + .5));
               } else {
                  mallocSprintf (name, "PoP");
               }
               mallocSprintf (comment, "Prob of Precip > %g In.", upperProb / 25.4);
            }
            *convert = UC_NONE;
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
         if (timeRangeUnit == 3) {
            mallocSprintf (comment, "%02d mon Prob of Hurricane Storm Surge > %g "
                           "m", lenTime, upperProb);
         } else if (timeRangeUnit == 4) {
            mallocSprintf (comment, "%02d yr Prob of Hurricane Storm Surge > %g "
                           "m", lenTime, upperProb);
         } else {
            mallocSprintf (comment, "%02d hr Prob of Hurricane Storm Surge > %g "
                           "m", lenTime, upperProb);
         }
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
         if (timeRangeUnit == 3) {
            mallocSprintf (comment, "%02d mon Prob of Wind speed > %g m/s",
                           lenTime, upperProb);
         } else if (timeRangeUnit == 4) {
            mallocSprintf (comment, "%02d yr Prob of Wind speed > %g m/s",
                           lenTime, upperProb);
         } else {
            mallocSprintf (comment, "%02d hr Prob of Wind speed > %g m/s",
                           lenTime, upperProb);
         }
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
            if (timeRangeUnit == 3) {
               mallocSprintf (name, "Prob%s%02dm", table[subcat].name, lenTime);
               mallocSprintf (comment, "%02d mon Prob of %s ", lenTime,
                              table[subcat].comment);
            } else if (timeRangeUnit == 4) {
               mallocSprintf (name, "Prob%s%02dy", table[subcat].name, lenTime);
               mallocSprintf (comment, "%02d yr Prob of %s ", lenTime,
                              table[subcat].comment);
            } else {
               mallocSprintf (name, "Prob%s%02d", table[subcat].name, lenTime);
               mallocSprintf (comment, "%02d hr Prob of %s ", lenTime,
                              table[subcat].comment);
            }
         } else {
            mallocSprintf (name, "Prob%s", table[subcat].name);
            mallocSprintf (comment, "Prob of %s ", table[subcat].comment);
         }
         if (probType == 0) {
            if ((f_isNdfd || f_isMos) && (strcmp (table[subcat].name, "TMP") == 0)) {
               reallocSprintf (comment, "below average");
               free (*name);
               if (lenTime > 0) {
                  if (timeRangeUnit == 3) {
                     mallocSprintf (name, "Prob%sBlw%02dm", table[subcat].name, lenTime);
                  } else if (timeRangeUnit == 4) {
                     mallocSprintf (name, "Prob%sBlw%02dy", table[subcat].name, lenTime);
                  } else {
                     mallocSprintf (name, "Prob%sBlw%02d", table[subcat].name, lenTime);
                  }
               } else {
                  mallocSprintf (name, "Prob%sBlw", table[subcat].name);
               }
            } else {
               reallocSprintf (comment, "< %g %s", lowerProb, table[subcat].unit);
            }
         } else if (probType == 1) {
            if ((f_isNdfd || f_isMos) && (strcmp (table[subcat].name, "TMP") == 0)) {
               reallocSprintf (comment, "above average");
               free (*name);
               if (lenTime > 0) {
                  if (timeRangeUnit == 3) {
                     mallocSprintf (name, "Prob%sAbv%02dm", table[subcat].name, lenTime);
                  } else if (timeRangeUnit == 4) {
                     mallocSprintf (name, "Prob%sAbv%02dy", table[subcat].name, lenTime);
                  } else {
                     mallocSprintf (name, "Prob%sAbv%02d", table[subcat].name, lenTime);
                  }
               } else {
                  mallocSprintf (name, "Prob%sAbv", table[subcat].name);
               }
            } else {
               reallocSprintf (comment, "> %g %s", upperProb, table[subcat].unit);
            }
         } else if (probType == 2) {
            reallocSprintf (comment, ">= %g, < %g %s", lowerProb, upperProb,
                            table[subcat].unit);
         } else if (probType == 3) {
            if ((f_isNdfd || f_isMos) && (strcmp (table[subcat].name, "TMP") == 0)) {
               reallocSprintf (comment, "above average");
               free (*name);
               if (lenTime > 0) {
                  if (timeRangeUnit == 3) {
                     mallocSprintf (name, "Prob%sAbv%02dm", table[subcat].name, lenTime);
                  } else if (timeRangeUnit == 4) {
                     mallocSprintf (name, "Prob%sAbv%02dy", table[subcat].name, lenTime);
                  } else {
                     mallocSprintf (name, "Prob%sAbv%02d", table[subcat].name, lenTime);
                  }
               } else {
                  mallocSprintf (name, "Prob%sAbv", table[subcat].name);
               }
            } else {
               reallocSprintf (comment, "> %g %s", lowerProb, table[subcat].unit);
            }
         } else if (probType == 4) {
            if ((f_isNdfd || f_isMos) && (strcmp (table[subcat].name, "TMP") == 0)) {
               reallocSprintf (comment, "below average");
               free (*name);
               if (lenTime > 0) {
                  if (timeRangeUnit == 3) {
                     mallocSprintf (name, "Prob%sBlw%02dm", table[subcat].name, lenTime);
                  } else if (timeRangeUnit == 4) {
                     mallocSprintf (name, "Prob%sBlw%02dy", table[subcat].name, lenTime);
                  } else {
                     mallocSprintf (name, "Prob%sBlw%02d", table[subcat].name, lenTime);
                  }
               } else {
                  mallocSprintf (name, "Prob%sBlw", table[subcat].name);
               }
            } else {
               reallocSprintf (comment, "< %g %s", upperProb, table[subcat].unit);
            }
         } else {
            reallocSprintf (comment, "%s", table[subcat].unit);
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

            /* Ignore adding Prob prefix and "Probability of" to NDFD SPC prob
             * products. */
            if (lenTime > 0) {
               if (timeRangeUnit == 3) {
                  mallocSprintf (name, "Prob%s%02dm", local[i].name, lenTime);
                  mallocSprintf (comment, "%02d mon Prob of %s ", lenTime,
                                 local[i].comment);
               } else if (timeRangeUnit == 4) {
                  mallocSprintf (name, "Prob%s%02dy", local[i].name, lenTime);
                  mallocSprintf (comment, "%02d yr Prob of %s ", lenTime,
                                 local[i].comment);
               } else {
                  mallocSprintf (name, "Prob%s%02d", local[i].name, lenTime);
                  mallocSprintf (comment, "%02d hr Prob of %s ", lenTime,
                                 local[i].comment);
               }
            } else {
               mallocSprintf (name, "Prob%s", local[i].name);
               mallocSprintf (comment, "Prob of %s ", local[i].comment);
            }
            if (probType == 0) {
               reallocSprintf (comment, "< %g %s", lowerProb,
                               local[i].unit);
            } else if (probType == 1) {
               reallocSprintf (comment, "> %g %s", upperProb,
                               local[i].unit);
            } else if (probType == 2) {
               reallocSprintf (comment, ">= %g, < %g %s", lowerProb,
                               upperProb, local[i].unit);
            } else if (probType == 3) {
               reallocSprintf (comment, "> %g %s", lowerProb,
                               local[i].unit);
            } else if (probType == 4) {
               reallocSprintf (comment, "< %g %s", upperProb,
                               local[i].unit);
            } else {
               reallocSprintf (comment, "%s", local[i].unit);
            }
            *convert = UC_NONE;
            return;
         }
      }
   }

   *name = (char *) malloc (strlen ("ProbUnknown") + 1);
   strcpy (*name, "ProbUnknown");
   mallocSprintf (comment, "Prob of (prodType %d, cat %d, subcat %d)",
                  prodType, cat, subcat);
   *convert = UC_NONE;
   return;
}

/* Deal with percentile templates 5/1/2006 */
static void ElemNamePerc (uShort2 center, uShort2 subcenter, int prodType,
                          CPL_UNUSED int templat,
                          uChar cat, uChar subcat, sInt4 lenTime,
                          uChar timeRangeUnit,
                          sChar percentile, char **name, char **comment,
                          char **unit, int *convert)
{
   const GRIB2ParmTable *table;
   const GRIB2LocalTable *local;
   size_t tableLen;
   size_t i;
   size_t len;

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
                     if (timeRangeUnit == 3) {
                        mallocSprintf (comment, "%02d mon %s Percentile(%d)",
                                       lenTime, table[subcat].comment,
                                       percentile);
                     } else if (timeRangeUnit == 4) {
                        mallocSprintf (comment, "%02d yr %s Percentile(%d)",
                                       lenTime, table[subcat].comment,
                                       percentile);
                     } else {
                        mallocSprintf (comment, "%02d hr %s Percentile(%d)",
                                       lenTime, table[subcat].comment,
                                       percentile);
                     }
                  } else {
                     mallocSprintf (comment, "%s Percentile(%d)",
                                    table[subcat].comment, percentile);
                  }
                  mallocSprintf (unit, "[%s]", table[subcat].unit);
                  *convert = table[subcat].convert;
                  return;
               }
            }
         }
         mallocSprintf (name, "%s%02d", table[subcat].name, percentile);
         if (lenTime > 0) {
            if (timeRangeUnit == 3) {
               mallocSprintf (comment, "%02d mon %s Percentile(%d)",
                              lenTime, table[subcat].comment, percentile);
            } else if (timeRangeUnit == 4) {
               mallocSprintf (comment, "%02d yr %s Percentile(%d)",
                              lenTime, table[subcat].comment, percentile);
            } else {
               mallocSprintf (comment, "%02d hr %s Percentile(%d)",
                              lenTime, table[subcat].comment, percentile);
            }
         } else {
            mallocSprintf (comment, "%s Percentile(%d)",
                           table[subcat].comment, percentile);
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
/* If last two characters in name are numbers, then the name contains
 * the percentile (or exceedance value) so don't tack on percentile here.*/
            len = strlen(local[i].name);
            if (len >= 2 &&
                isdigit(local[i].name[len -1]) &&
                isdigit(local[i].name[len -2])) {
               mallocSprintf (name, "%s", local[i].name);
            } else if ((strcmp (local[i].name, "Surge") == 0) ||
                       (strcmp (local[i].name, "SURGE") == 0)) {
/* Provide a special exception for storm surge exceedance.
 * Want exceedance value rather than percentile value.
 */
               mallocSprintf (name, "%s%02d", local[i].name, 100 - percentile);
            } else {
               mallocSprintf (name, "%s%02d", local[i].name, percentile);
            }

            if (lenTime > 0) {
               if (timeRangeUnit == 3) {
                  mallocSprintf (comment, "%02d mon %s Percentile(%d)",
                                 lenTime, local[i].comment, percentile);
               } else if (timeRangeUnit == 4) {
                  mallocSprintf (comment, "%02d yr %s Percentile(%d)",
                                 lenTime, local[i].comment, percentile);
               } else {
                  mallocSprintf (comment, "%02d hr %s Percentile(%d)",
                                 lenTime, local[i].comment, percentile);
               }
            } else {
               mallocSprintf (comment, "%s Percentile(%d)",
                              local[i].comment, percentile);
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
                          uChar timeRangeUnit,
                          CPL_UNUSED uChar timeIncrType,
                          CPL_UNUSED uChar genID,
                          CPL_UNUSED uChar probType,
                          CPL_UNUSED double lowerProb,
                          CPL_UNUSED double upperProb,
                          char **name,
                          char **comment, char **unit, int *convert,
                          sChar f_fstValue, double fstSurfValue,
                          sChar f_sndValue, double sndSurfValue)
{
   const GRIB2ParmTable *table;
   const GRIB2LocalTable *local;
   size_t tableLen;
   size_t i;
   sChar f_accum;

   /* Check for over-ride case for ozone.  Originally just for NDFD, but I
    * think it is useful for ozone data that originated elsewhere. */
   if ((prodType == 0) && (templat == 8) && (cat == 14) && (subcat == 193)) {
      if (lenTime > 0) {
         if (timeRangeUnit == 3) {
            mallocSprintf (name, "Ozone%02dm", lenTime);
            mallocSprintf (comment, "%d mon Average Ozone Concentration", lenTime);
         } else if (timeRangeUnit == 4) {
            mallocSprintf (name, "Ozone%02dy", lenTime);
            mallocSprintf (comment, "%d yr Average Ozone Concentration", lenTime);
         } else {
            mallocSprintf (name, "Ozone%02d", lenTime);
            mallocSprintf (comment, "%d hr Average Ozone Concentration", lenTime);
         }
      } else {
         *name = (char *) malloc (strlen ("AVGOZCON") + 1);
         strcpy (*name, "AVGOZCON");
         *comment = (char *) malloc (strlen ("Average Ozone Concentration") +
                                     1);
         strcpy (*comment, "Average Ozone Concentration");
      }
      *unit = (char *) malloc (strlen ("[PPB]") + 1);
      strcpy (*unit, "[PPB]");
      *convert = UC_NONE;
      return;
   }
   /* Check for over-ride case for smokec / smokes. */
   if (center == 7) {
      if ((prodType == 0) && (cat == 13) && (subcat == 195)) {
         /* If NCEP/ARL (genID=6) then it is dust */
         if (genID == 6) {
            if (f_fstValue && f_sndValue) {
               const double delt = fstSurfValue - sndSurfValue;
               if ((delt <= 100) && (delt >= -100)) {
                  *name = (char *) malloc (strlen ("dusts") + 1);
                  strcpy (*name, "dusts");
                  *comment = (char *) malloc (strlen ("Surface level dust") + 1);
                  strcpy (*comment, "Surface level dust");
                  *unit = (char *) malloc (strlen ("[log10(µg/m^3)]") + 1);
                  strcpy (*unit, "[log10(µg/m^3)]");
                  *convert = UC_LOG10;
                  return;
               } else if ((delt <= 5000) && (delt >= -5000)) {
                  *name = (char *) malloc (strlen ("dustc") + 1);
                  strcpy (*name, "dustc");
                  *comment = (char *) malloc (strlen ("Average vertical column dust") + 1);
                  strcpy (*comment, "Average vertical column dust");
                  *unit = (char *) malloc (strlen ("[log10(µg/m^3)]") + 1);
                  strcpy (*unit, "[log10(µg/m^3)]");
                  *convert = UC_LOG10;
                  return;
               }
            }
         } else {
            if (f_fstValue && f_sndValue) {
               const double delt = fstSurfValue - sndSurfValue;
               if ((delt <= 100) && (delt >= -100)) {
                  *name = (char *) malloc (strlen ("smokes") + 1);
                  strcpy (*name, "smokes");
                  *comment = (char *) malloc (strlen ("Surface level smoke from fires") + 1);
                  strcpy (*comment, "Surface level smoke from fires");
                  *unit = (char *) malloc (strlen ("[log10(µg/m^3)]") + 1);
                  strcpy (*unit, "[log10(µg/m^3)]");
                  *convert = UC_LOG10;
                  return;
               } else if ((delt <= 5000) && (delt >= -5000)) {
                  *name = (char *) malloc (strlen ("smokec") + 1);
                  strcpy (*name, "smokec");
                  *comment = (char *) malloc (strlen ("Average vertical column smoke from fires") + 1);
                  strcpy (*comment, "Average vertical column smoke from fires");
                  *unit = (char *) malloc (strlen ("[log10(µg/m^3)]") + 1);
                  strcpy (*unit, "[log10(µg/m^3)]");
                  *convert = UC_LOG10;
                  return;
               }
            }
         }
      }
   }

   /* Generic tables. */
   table = Choose_GRIB2ParmTable (prodType, cat, &tableLen);
   if (table != NULL) {
      if (subcat < tableLen) {
         /* Check for NDFD over-rides. */
         if (IsData_MOS (center, subcenter)) {
            if (strcmp (table[subcat].name, "APCP") == 0) {
               if (timeRangeUnit == 3) {
                  mallocSprintf (name, "%s%02dm", "QPF", lenTime);
                  mallocSprintf (comment, "%02d mon %s", lenTime,
                                 table[subcat].comment);
               } else if (timeRangeUnit == 4) {
                  mallocSprintf (name, "%s%02dy", "QPF", lenTime);
                  mallocSprintf (comment, "%02d yr %s", lenTime,
                                 table[subcat].comment);
               } else {
                  mallocSprintf (name, "%s%02d", "QPF", lenTime);
                  mallocSprintf (comment, "%02d hr %s", lenTime,
                                 table[subcat].comment);
               }
               mallocSprintf (unit, "[%s]", table[subcat].unit);
               *convert = table[subcat].convert;
               return;
            }
            if (strcmp (table[subcat].name, "ASNOW") == 0) {
               if (timeRangeUnit == 3) {
                  mallocSprintf (name, "%s%02dm", "SnowAmt", lenTime);
                  mallocSprintf (comment, "%02d mon %s", lenTime,
                                 table[subcat].comment);
               } else if (timeRangeUnit == 4) {
                  mallocSprintf (name, "%s%02dy", "SnowAmt", lenTime);
                  mallocSprintf (comment, "%02d yr %s", lenTime,
                                 table[subcat].comment);
               } else {
                  mallocSprintf (name, "%s%02d", "SnowAmt", lenTime);
                  mallocSprintf (comment, "%02d hr %s", lenTime,
                                 table[subcat].comment);
               }
               mallocSprintf (unit, "[%s]", table[subcat].unit);
               *convert = table[subcat].convert;
               return;
            }
         }
         if (IsData_NDFD (center, subcenter) || IsData_MOS (center, subcenter)) {
            for (i = 0; i < (sizeof (NDFD_Overide) /
                             sizeof (NDFD_AbrevOverideTable)); i++) {
               if (strcmp (NDFD_Overide[i].GRIB2name, table[subcat].name) ==
                   0) {
                  *name = (char *) malloc (strlen (NDFD_Overide[i].NDFDname) + 1);
                  strcpy (*name, NDFD_Overide[i].NDFDname);
                  *comment = (char *) malloc (strlen (table[subcat].comment) + 1);
                  strcpy (*comment, table[subcat].comment);
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
            if (timeRangeUnit == 3) {
               mallocSprintf (name, "%s%02dm", table[subcat].name, lenTime);
               mallocSprintf (comment, "%02d mon %s", lenTime,
                              table[subcat].comment);
            } else if (timeRangeUnit == 4) {
               mallocSprintf (name, "%s%02dy", table[subcat].name, lenTime);
               mallocSprintf (comment, "%02d yr %s", lenTime,
                              table[subcat].comment);
            } else {
               mallocSprintf (name, "%s%02d", table[subcat].name, lenTime);
               mallocSprintf (comment, "%02d hr %s", lenTime,
                              table[subcat].comment);
            }
         } else {
            *name = (char *) malloc (strlen (table[subcat].name) + 1);
            strcpy (*name, table[subcat].name);
            *comment = (char *) malloc (strlen (table[subcat].comment) + 1);
            strcpy (*comment, table[subcat].comment);
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
               if (timeRangeUnit == 3) {
                  mallocSprintf (name, "%s%02dm", local[i].name, lenTime);
                  mallocSprintf (comment, "%02d mon %s", lenTime,
                                 local[i].comment);
               } else if (timeRangeUnit == 4) {
                  mallocSprintf (name, "%s%02dy", local[i].name, lenTime);
                  mallocSprintf (comment, "%02d yr %s", lenTime,
                                 local[i].comment);
               } else {
                  mallocSprintf (name, "%s%02d", local[i].name, lenTime);
                  mallocSprintf (comment, "%02d hr %s", lenTime,
                                 local[i].comment);
               }
            } else {
               *name = (char *) malloc (strlen (local[i].name) + 1);
               strcpy (*name, local[i].name);
               *comment = (char *) malloc (strlen (local[i].comment) + 1);
               strcpy (*comment, local[i].comment);
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
                    uChar timeRangeUnit,
                    uChar timeIncrType, uChar genID, uChar probType,
                    double lowerProb, double upperProb, char **name,
                    char **comment, char **unit, int *convert,
                    sChar percentile, uChar genProcess,
                    sChar f_fstValue, double fstSurfValue,
                    sChar f_sndValue, double sndSurfValue)
{
   char f_isNdfd = IsData_NDFD (center, subcenter);
   myAssert (*name == NULL);
   myAssert (*comment == NULL);
   myAssert (*unit == NULL);

   /* Check if this is Probability data */
   if ((templat == GS4_PROBABIL_TIME) || (templat == GS4_PROBABIL_PNT)) {
      if (f_isNdfd && (prodType == 0) && (cat == 19)) {
         /* don't use ElemNameProb. */
         ElemNameNorm (center, subcenter, prodType, templat, cat, subcat,
                       lenTime, timeRangeUnit, timeIncrType, genID, probType, lowerProb,
                       upperProb, name, comment, unit, convert, f_fstValue, fstSurfValue,
                       f_sndValue, sndSurfValue);

      } else {
         ElemNameProb (center, subcenter, prodType, templat, cat, subcat,
                       lenTime, timeRangeUnit, timeIncrType, genID, probType, lowerProb,
                       upperProb, name, comment, unit, convert);
      }
   } else if ((templat == GS4_PERCENT_TIME) || (templat == GS4_PERCENT_PNT)) {
      ElemNamePerc (center, subcenter, prodType, templat, cat, subcat,
                    lenTime, timeRangeUnit, percentile, name, comment, unit, convert);
   } else {
      ElemNameNorm (center, subcenter, prodType, templat, cat, subcat,
                    lenTime, timeRangeUnit, timeIncrType, genID, probType, lowerProb,
                    upperProb, name, comment, unit, convert, f_fstValue, fstSurfValue,
                       f_sndValue, sndSurfValue);
   }
   if ((genProcess == 6) || (genProcess == 7)) {
      *convert = UC_NONE;
      reallocSprintf (name, "ERR");
      reallocSprintf (comment, " error %s", *unit);
   } else {
      reallocSprintf (comment, " %s", *unit);
   }
}

/*****************************************************************************
 * ParseElemName2() -- Review 12/2002
 *
 * Arthur Taylor / MDL
 *
 * PURPOSE
 *   Converts a prodType, template, category and subcategory quadruple to the
 * ASCII string abbreviation of that variable.
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
 *   Same as 'strcpy', i.e. it returns name.
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
 *   For f_unit = 1 (English) we return Fahrenheit, knots, and inches for
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
      case UC_M2StatuteMile: /* Convert from meters to statute miles. */
         if (f_unit == 1) {
            strcpy (name, "[statute mile]");
            *unitM = 1. / 1609.344; /* mile / m */
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
      case UC_UVIndex: /* multiply by Watts/ m**2 by 40 for the UV index. */
         if (f_unit == 1) {
            strcpy (name, "[UVI]");
            *unitM = 40;
            *unitB = 0;
            return 0;
         }
         break;
      case UC_LOG10:   /* convert from log10 (x) to x */
         if ((f_unit == 1) || (f_unit == 2)) {
            origName[strlen (origName) - 2] = '\0';
            if (strlen (origName) > 21)
               origName[21] = '\0';
            snprintf (name, 15, "[%s]", origName + 7);
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
 *   For f_unit = 1 (English) we return Fahrenheit, knots, and inches for
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
static const GRIB2SurfTable Surface[] = {
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
static const GRIB2LocalSurface NCEP_Surface[] = {
   {200, {"EATM", "Entire atmosphere (considered as a single layer)", "-"}},
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
   snprintf (valBuff, sizeof(valBuff), "%f", value);
   strTrimRight (valBuff, '0');
   if (valBuff[strlen (valBuff) - 1] == '.') {
      valBuff[strlen (valBuff) - 1] = '\0';
   }
   if (f_sndValue) {
      snprintf (sndBuff, sizeof(sndBuff), "%f", sndValue);
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
