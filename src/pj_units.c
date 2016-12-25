/* definition of standard cartesian units */
#define PJ_UNITS__
#include <projects.h>
/* Field 2 that contains the multiplier to convert named units to meters
** may be expressed by either a simple floating point constant or a
** numerator/denomenator values (e.g. 1/1000) */
C_NAMESPACE_VAR struct PJ_UNITS
pj_units[] = {
	{"km",		"1000.",		"Kilometer"},
	{"m",		"1.",			"Meter"},
	{"dm",		"1/10",			"Decimeter"},
	{"cm",		"1/100",		"Centimeter"},
	{"mm",		"1/1000",		"Millimeter"},
	{"kmi",		"1852.0",		"International Nautical Mile"},
	{"in",		"0.0254",		"International Inch"},
	{"ft",		"0.3048",		"International Foot"},
	{"yd",		"0.9144",		"International Yard"},
	{"mi",		"1609.344",		"International Statute Mile"},
	{"fath",	"1.8288",		"International Fathom"},
	{"ch",		"20.1168",		"International Chain"},
	{"link",	"0.201168",		"International Link"},
	{"us-in",	"1./39.37",		"U.S. Surveyor's Inch"},
	{"us-ft",	"0.304800609601219",	"U.S. Surveyor's Foot"},
	{"us-yd",	"0.914401828803658",	"U.S. Surveyor's Yard"},
	{"us-ch",	"20.11684023368047",	"U.S. Surveyor's Chain"},
	{"us-mi",	"1609.347218694437",	"U.S. Surveyor's Statute Mile"},
	{"ind-yd",	"0.91439523",		"Indian Yard"},
	{"ind-ft",	"0.30479841",		"Indian Foot"},
	{"ind-ch",	"20.11669506",		"Indian Chain"},
	{NULL,		NULL,			NULL}
};

struct PJ_UNITS *pj_get_units_ref()

{
    return pj_units;
}
