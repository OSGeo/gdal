#ifndef IdrisiRasterProj_h
#define IdrisiRasterProj_h

#include "IdrisiRasterDoc.h"

#define INVERSEFLATTENING		298.25722356300003
#define DEGREE2METERS			(M_PI / 180)
#define MAXPESTRING				2048

/* attribute labels in a Idrisi Reference System (and Projection) File */
#define LABEL_REF_REF_SYSTEM	"ref. system"
#define LABEL_REF_PROJECTION    "projection" 
#define LABEL_REF_DATUM         "datum"
#define LABEL_REF_DELTA_WGS84   "delta WGS84"
#define LABEL_REF_ELLIPSOID     "ellipsoid"
#define LABEL_REF_MAJOR_S_AX    "major s-ax"  
#define LABEL_REF_MINOR_S_AX    "minor s-ax"
#define LABEL_REF_ORIGIN_LONG   "origin long"
#define LABEL_REF_ORIGIN_LAT    "origin lat"
#define LABEL_REF_ORIGIN_X      "origin x"
#define LABEL_REF_ORIGIN_Y      "origin y"  
#define LABEL_REF_SCALE_FAC     "scale fac"  
#define LABEL_REF_UNITS         "units" 
#define LABEL_REF_PARAMETERS    "parameters"

/* ESRI standart strings */
#define ESRI_LATLONG			"Geographic (Lat/Lon)"
#define ESRI_DEGREE				"dd"

typedef struct _rst_RefName {
	char *name;					// Name
	char *esriName;				// Correspodente name on ArcGIS
} rst_RefName;

static rst_RefName rstRefNames[] = {
	{"Transverse Mercator",						"Transverse_Mercator"},
	{"Lambert Conformal Conic",					"Lambert_Conformal_Conic"},
	{"Lambert Oblique Azimuthal Equal Area",	"Lambert_Azimuthal_Equal_Area"},
	{"Gauss_Kruger",							"Gauss-Kruger"},
	{"Hammer Aitoff",							"Hammer_Aitoff"},
	{"Alber's Equal Area Conic",				"Albers"},
	{"Transverse Mercator",						"Transverse_Mercator"}
};
typedef struct _rst_Datum {
	char *name;					// Name
	char *esriName;				// Correspodente name on ArcGIS
	char *ellipsoidName;		// Ellipsoid Name {Idrisi only have one-to-one //??}
} rst_Datum;

static rst_Datum rstDatums[] = {
	{"Arc 1950", 			"Arc_1950",				"Clarke_1880"},
	{"Cape", 				"Cape",					"Clarke_1880"},
	{"Clabs_ae", 			"Clabs_ae",				"Clarke_1866"},
	{"Clabs_ha", 			"Clabs_ha",				"Sphere"},
	{"Indian", 				"Indian_1960",			"Everest_1830"},
	{"Indian 1954", 		"Indian_1954",			"Everest_1830"},
	{"Indian 1975", 		"Indian_1975",			"Everest_1830"},
	{"NAD27", 				"North_American_1927",	"Clarke_1866"},
	{"NAD83", 				"North_American_1983",	"GRS_1980"},
	{"NAD27(Michigan}", 	"NAD_1927_CGQ77",		"Clarke_1866"},
	{"Pulkovo 1942", 		"Pulkovo_1942",			"Krasovsky_1940"},
	{"WGS84", 				"WGS_1984",				"WGS_1984"},
	{"WGS 1984",			"WGS 84",				"WGS 84"},
};

typedef struct _rst_Unit {
	char *name;					// Name
	char *esriName;				// Correspodente name on ArcGIS
	double meters;				// Meter per unit
} rst_Unit;

static rst_Unit rstUnits[] = {
	{"meters",		"Meter",		1.0},
	{"feets",		"Feet",			0.3048006090122},
	{"miles",		"Mile",			1609.3439},
	{"kilometers",	"Kilometer",	1000.0},
	{"degrees",		"Degree",		(M_PI / 180)},
	{"m",			"Meter",		1.0},
	{"ft",			"Feet",			0.3048006090122},
	{"mi",			"Mile",			1609.3439},
	{"km",			"Kilometer",	1000.0},
	{"deg",			"Degree",		(M_PI / 180)},
};

typedef struct _rst_Ref {
	char *name;					// name
	char *fileName;				// reference file in Idrisi (without .ref)
	char *projection;			// Projection System
	rst_RefName *RefName;		// Coordenates System
	rst_Datum *Datum;			// Datum
	double majorSemiAxis;		// Major Semi Axis
	double minorSemiAxis;		// Minor Semi Axis
	double originLongitude;		// Origin Longitude
	double originLatitude;		// Origin Latitude
	double originX;				// Origin X
	double originY;				// Origin Y
	double scaleFactor;			// Scale Factor
	rst_Unit *Unit;				// Unit (m-miters, ft-feet, kg-kilomiters, deg-degrees)
} rst_Ref;

rst_Ref *CreateImgRef();
rst_Ref *ReadImgRef(const char *fileName);
void WriteimgRef(rst_Ref *imgRef, char *fileName);
void FreeimgRef(rst_Ref *imgRef);
char *ReadProjSystem(char *fileName);
void WriteProjSystem(char *peString, char *fileName);

#define PROJSARRAYCOUNT 	(sizeof(rstRefs) / sizeof(rst_Ref))
#define DATUMSARRAYCOUNT	(sizeof(rstDatums) / sizeof(rst_Datum))
#define REFNAMESARRAYCOUNT (sizeof(rstRefNames) / sizeof(rst_RefName))
#define UNITSARRAYCOUNT     (sizeof(rstUnits) / sizeof(rst_Unit))

#endif /* IdrisiRasterProj_h */
