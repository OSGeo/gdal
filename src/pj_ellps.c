/* definition of standard geoids */
#define PJ_ELLPS__
#include "projects.h"

C_NAMESPACE_VAR struct PJ_ELLPS
pj_ellps[] = {
{"MERIT",	"a=6378137.0",		"rf=298.257",		"MERIT 1983"},
{"SGS85",	"a=6378136.0",		"rf=298.257",		"Soviet Geodetic System 85"},
{"GRS80",	"a=6378137.0",		"rf=298.257222101",	"GRS 1980(IUGG, 1980)"},
{"IAU76",	"a=6378140.0",		"rf=298.257",		"IAU 1976"},
{"airy",	"a=6377563.396",	"b=6356256.910",	"Airy 1830"},
{"APL4.9",	"a=6378137.0.",		"rf=298.25",		"Appl. Physics. 1965"},
{"NWL9D",	"a=6378145.0.",		"rf=298.25",		"Naval Weapons Lab., 1965"},
{"mod_airy",	"a=6377340.189",	"b=6356034.446",	"Modified Airy"},
{"andrae",	"a=6377104.43",		"rf=300.0",		"Andrae 1876 (Den., Iclnd.)"},
{"aust_SA",	"a=6378160.0",		"rf=298.25",		"Australian Natl & S. Amer. 1969"},
{"GRS67",	"a=6378160.0",		"rf=298.2471674270",	"GRS 67(IUGG 1967)"},
{"bessel",	"a=6377397.155",	"rf=299.1528128",	"Bessel 1841"},
{"bess_nam",	"a=6377483.865",	"rf=299.1528128",	"Bessel 1841 (Namibia)"},
{"clrk66",	"a=6378206.4",		"b=6356583.8",		"Clarke 1866"},
{"clrk80",	"a=6378249.145",	"rf=293.4663",		"Clarke 1880 mod."},
{"clrk80ign",	"a=6378249.2",		"rf=293.4660212936269",	"Clarke 1880 (IGN)."},
{"CPM", 	"a=6375738.7",		"rf=334.29",		"Comm. des Poids et Mesures 1799"},
{"delmbr",	"a=6376428.",		"rf=311.5",		"Delambre 1810 (Belgium)"},
{"engelis",	"a=6378136.05",		"rf=298.2566",		"Engelis 1985"},
{"evrst30",	"a=6377276.345",	"rf=300.8017",		"Everest 1830"},
{"evrst48",	"a=6377304.063",	"rf=300.8017",		"Everest 1948"},
{"evrst56",	"a=6377301.243",	"rf=300.8017",		"Everest 1956"},
{"evrst69",	"a=6377295.664",	"rf=300.8017",		"Everest 1969"},
{"evrstSS",	"a=6377298.556",	"rf=300.8017",		"Everest (Sabah & Sarawak)"},
{"fschr60",	"a=6378166.",		"rf=298.3",		"Fischer (Mercury Datum) 1960"},
{"fschr60m",	"a=6378155.",		"rf=298.3",		"Modified Fischer 1960"},
{"fschr68",	"a=6378150.",		"rf=298.3",		"Fischer 1968"},
{"helmert",	"a=6378200.",		"rf=298.3",		"Helmert 1906"},
{"hough",	"a=6378270.0",		"rf=297.",		"Hough"},
{"intl",	"a=6378388.0",		"rf=297.",		"International 1909 (Hayford)"},
{"krass",	"a=6378245.0",		"rf=298.3",		"Krassovsky, 1942"},
{"kaula",	"a=6378163.",		"rf=298.24",		"Kaula 1961"},
{"lerch",	"a=6378139.",		"rf=298.257",		"Lerch 1979"},
{"mprts",	"a=6397300.",		"rf=191.",		"Maupertius 1738"},
{"new_intl",	"a=6378157.5",		"b=6356772.2",		"New International 1967"},
{"plessis",	"a=6376523.",		"b=6355863.", 		"Plessis 1817 (France)"},
{"SEasia",	"a=6378155.0",		"b=6356773.3205",	"Southeast Asia"},
{"walbeck",	"a=6376896.0",		"b=6355834.8467",	"Walbeck"},
{"WGS60",	"a=6378165.0",		"rf=298.3",		"WGS 60"},
{"WGS66",	"a=6378145.0",		"rf=298.25",		"WGS 66"},
{"WGS72",	"a=6378135.0",		"rf=298.26", 		"WGS 72"},
{"WGS84",	"a=6378137.0",		"rf=298.257223563",	"WGS 84"},
{"sphere",	"a=6370997.0",		"b=6370997.0",		"Normal Sphere (r=6370997)"},
{NULL,		NULL,			NULL,			NULL}
};

struct PJ_ELLPS *pj_get_ellps_ref()

{
    return pj_ellps;
}
