/******************************************************************************
 *
 * Purpose: Translation from ILWIS coordinate system information.
 * Author:   Lichun Wang, lichun@itc.nl
 *
 ******************************************************************************
 * Copyright (c) 2004, ITC
 * Copyright (c) 2008-2012, Even Rouault <even dot rouault at mines-paris dot org>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 ****************************************************************************/
#include "cpl_conv.h"
#include "ilwisdataset.h"

#include <string>

using std::string;

typedef struct
{
    const char  *pszIlwisDatum;
		const char  *pszWKTDatum;
    int   nEPSGCode;
} IlwisDatums;

typedef struct
{
    const char  *pszIlwisEllips;
    int   nEPSGCode;
		double semiMajor;
		double invFlattening;
} IlwisEllips;

string ReadElement(string section, string entry, string filename);
bool WriteElement(string sSection, string sEntry, string fn, string sValue);
bool WriteElement(string sSection, string sEntry, string fn, int nValue);
bool WriteElement(string sSection, string sEntry, string fn, double dValue);

static const IlwisDatums iwDatums[] =
{
    { "Adindan", "Adindan", 4201 },
    { "Afgooye", "Afgooye", 4205 },
		//AGREF --- skipped
    { "Ain el Abd 1970", "Ain_el_Abd_1970", 4204 },
		{ "American Samoa 1962", "American_Samoa_1962", 4169 },
		//Anna 1 Astro 1965 --- skipped
		{ "Antigua Island Astro 1943", "Antigua_1943", 4601 },
		{ "Arc 1950", "Arc_1950", 4209 },    //Arc 1950
		{ "Arc 1960", "Arc_1960", 4210 },    //Arc 1960
		//Ascension Island 1958
		//Astro Beacon E 1945
		//Astro DOS 71/4
		//Astro Tern Island (FRIG) 1961
		//Astronomical Station 1952
		{ "Australian Geodetic 1966", "Australian_Geodetic_Datum_1966", 4202 },
		{ "Australian Geodetic 1984", "Australian_Geodetic_Datum_1984", 4203 },
		//Ayabelle Lighthouse
		//Bellevue (IGN)
		{ "Bermuda 1957", "Bermuda_1957", 4216 },
		{ "Bissau", "Bissau", 4165 },
		{ "Bogota Observatory  (1975)", "Bogota", 4218 },
		{ "Bukit Rimpah", "Bukit_Rimpah", 4219 },
		//Camp Area Astro
		{ "Campo Inchauspe", "Campo_Inchauspe", 4221 },
		//Canton Astro 1966
		{ "Cape", "Cape", 4222 },
		//Cape Canaveral
		{ "Carthage", "Carthage", 4223 },
		{ "CH1903", "CH1903", 4149 },
		//Chatham Island Astro 1971
		{ "Chua Astro", "Chua", 4224 },
		{ "Corrego Alegre", "Corrego_Alegre", 4225 },
		//Croatia
		//D-PAF (Orbits)
		{ "Dabola", "Dabola_1981", 4155 },
		//Deception Island
		//Djakarta (Batavia)
		//DOS 1968
		//Easter Island 1967
		//Estonia 1937
		{ "European 1950 (ED 50)", "European_Datum_1950", 4154 },
		//European 1979 (ED 79
		//Fort Thomas 1955
		{ "Gan 1970", "Gandajika_1970", 4233 },
		//Geodetic Datum 1949
		//Graciosa Base SW 1948
		//Guam 1963
		{ "Gunung Segara", "Gunung_Segara", 4613 },
		//GUX 1 Astro
		{ "Herat North", "Herat_North", 4255 },
		//Hermannskogel
		//Hjorsey 1955
		//Hong Kong 1963
		{ "Hu-Tzu-Shan", "Hu_Tzu_Shan", 4236 },
		//Indian (Bangladesh)
		//Indian (India, Nepal)
		//Indian (Pakistan)
		{ "Indian 1954", "Indian_1954", 4239 },
		{ "Indian 1960", "Indian_1960", 4131 },
		{ "Indian 1975", "Indian_1975", 4240 },
		{ "Indonesian 1974", "Indonesian_Datum_1974", 4238 },
		//Ireland 1965
		//ISTS 061 Astro 1968
		//ISTS 073 Astro 1969
		//Johnston Island 1961
		{ "Kandawala", "Kandawala", 4244 },
		//Kerguelen Island 1949
		{ "Kertau 1948", "Kertau", 4245 },
		//Kusaie Astro 1951
		//L. C. 5 Astro 1961
		{ "Leigon", "Leigon", 4250 },
		{ "Liberia 1964", "Liberia_1964", 4251 },
		{ "Luzon", "Luzon_1911", 4253 },
		//M'Poraloko
		{ "Mahe 1971", "Mahe_1971", 4256 },
		{ "Massawa", "Massawa", 4262 },
		{ "Merchich", "Merchich", 4261 },
		{ "MGI (Hermannskogel)", "Militar_Geographische_Institute",4312 },
		//Midway Astro 1961
		{ "Minna", "Minna", 4263 },
		{ "Montserrat Island Astro 1958", "Montserrat_1958", 4604 },
		{ "Nahrwan", "Nahrwan_1967", 4270 },
		{ "Naparima BWI", "Naparima_1955", 4158 },
		{ "North American 1927 (NAD 27)", "North_American_Datum_1927", 4267 },
		{ "North American 1983 (NAD 83)", "North_American_Datum_1983", 4269 },
		//North Sahara 1959
		{ "NTF (Nouvelle Triangulation de France)", "Nouvelle_Triangulation_Francaise", 4807 },
		//Observatorio Meteorologico 1939
		//Old Egyptian 1907
		{ "Old Hawaiian", "Old_Hawaiian", 4135 },
		//Oman
		//Ordnance Survey Great Britain 1936
		//Pico de las Nieves
		//Pitcairn Astro 1967
		//Point 58
		{ "Pointe Noire 1948", "Pointe_Noire", 4282 },
		{ "Porto Santo 1936", "Porto_Santo",4615 },
		//Potsdam (Rauenburg)
		{ "Potsdam (Rauenburg)", "Deutsches_Hauptdreiecksnetz", 4314 },
		{ "Provisional South American 1956", "Provisional_South_American_Datum_1956", 4248 },
		//Provisional South Chilean 1963
		{ "Puerto Rico", "Puerto_Rico", 4139 },
		{ "Pulkovo 1942", "Pulkovo_1942", 4178 },
		//{ "Qatar National", "Qatar_National_Datum_1995", 4614 },
		{ "Qornoq", "Qornoq", 4287 },
		{ "Puerto Rico", "Puerto_Rico", 4139 },
		//Reunion
		{ "Rome 1940", "Monte_Mario", 4806 },
		{ "RT90", "Rikets_koordinatsystem_1990", 4124 },
		{ "Rijks Driehoeksmeting", "Amersfoort", 4289 },
		{ "S-42 (Pulkovo 1942)", "Pulkovo_1942", 4178 },
		//{ "S-JTSK", "Jednotne_Trigonometricke_Site_Katastralni", 4156 },
		//Santo (DOS) 1965
		//Sao Braz
		{ "Sapper Hill 1943", "Sapper_Hill_1943", 4292 },
		{ "Schwarzeck", "Schwarzeck", 4293 },
		{ "Selvagem Grande 1938", "Selvagem_Grande", 4616 },
		//vSGS 1985
		//Sierra Leone 1960
		{ "South American 1969", "South_American_Datum_1969", 4291 },
		//South Asia
		{ "Tananarive Observatory 1925", "Tananarive_1925", 4297 },
		{ "Timbalai 1948", "Timbalai_1948", 4298 },
		{ "Tokyo", "Tokyo", 4301 },
		//Tristan Astro 1968
		//Viti Levu 1916
		{ "Voirol 1874", "Voirol_1875", 4304 },
		//Voirol 1960
		//Wake Island Astro 1952
		//Wake-Eniwetok 1960
		{ "WGS 1972", "WGS_1972", 4322 },
		{ "WGS 1984", "WGS_1984", 4326 },
		{ "Yacare", "Yacare", 4309 },
		{ "Zanderij", "Zanderij", 4311 },
    { NULL, NULL, 0 }
};

static const IlwisEllips iwEllips[] =
{
    { "Sphere", 7035, 6371007, 0.0  },	//rad 6370997 m (normal sphere)
    { "Airy 1830", 7031, 6377563.396, 299.3249646 },
    { "Modified Airy", 7002, 6377340.189, 299.3249646 },
    { "ATS77", 7204, 6378135.0, 298.257000006 },
    { "Australian National", 7003, 6378160, 298.249997276 },
    { "Bessel 1841", 7042, 6377397.155, 299.1528128},
		{ "Bessel 1841 (Japan By Law)", 7046 , 6377397.155, 299.152815351 },
    { "Bessel 1841 (Namibia)", 7006, 6377483.865, 299.1528128 },
    { "Clarke 1866", 7008, 6378206.4, 294.9786982 },
    { "Clarke 1880", 7034, 6378249.145, 293.465 },
    { "Clarke 1880 (IGN)", 7011, 6378249.2, 293.466 },
    // FIXME: D-PAF (Orbits) --- skipped
    // FIXME: Du Plessis Modified --- skipped
    // FIXME: Du Plessis Reconstituted --- skipped
    { "Everest (India 1830)", 7015, 6377276.345, 300.8017 },
    // Everest (India 1956) --- skipped
		// Everest (Malaysia 1969) --- skipped
    { "Everest (E. Malaysia and Brunei)", 7016, 6377298.556, 300.8017 },
    { "Everest (Malay. and Singapore 1948)", 7018, 6377304.063, 300.8017 },
		{ "Everest (Pakistan)", 7044, 6377309.613, 300.8017 },
		// Everest (Sabah Sarawak) --- skipped
		// Fischer 1960 --- skipped
		// Fischer 1960 (Modified) --- skipped
		// Fischer 1968 --- skipped
		{ "GRS 80", 7019, 6378137, 298.257222101  },
		{ "Helmert 1906", 7020, 6378200, 298.3 },
		// Hough 1960 --- skipped
		{ "Indonesian 1974", 7021, 6378160, 298.247 },
		{ "International 1924", 7022, 6378388, 297 },
		{ "Krassovsky 1940", 7024, 6378245, 298.3 },
		// New_International 1967
		// SGS 85
		// South American 1969
		// WGS 60
		// WGS 66
		{ "WGS 72", 7020, 6378135.0, 298.259998590  },
		{ "WGS 84", 7030, 6378137, 298.257223563 },
    { NULL, 0, 0.0, 0.0 }
};

#ifndef R2D
#  define R2D	(180/M_PI)
#endif
#ifndef D2R
#  define D2R	(M_PI/180)
#endif

/* ==================================================================== */
/*      Some "standard" strings.                                        */
/* ==================================================================== */

#define ILW_False_Easting "False Easting"
#define ILW_False_Northing "False Northing"
#define ILW_Central_Meridian "Central Meridian"
#define ILW_Central_Parallel "Central Parallel"
#define ILW_Standard_Parallel_1 "Standard Parallel 1"
#define ILW_Standard_Parallel_2 "Standard Parallel 2"
#define ILW_Scale_Factor "Scale Factor"
#define ILW_Latitude_True_Scale "Latitude of True Scale"
#define ILW_Height_Persp_Center "Height Persp. Center"

static double ReadPrjParms(string section, string entry, string filename)
{
    string str = ReadElement(section, entry, filename);
    //string str="";
    if (str.length() != 0)
        return CPLAtof(str.c_str());

    return 0.0;
}

static int fetchParms(string csyFileName, double * padfPrjParams)
{
    //Fill all projection parameters with zero
    for ( int i = 0; i < 13; i++ )
        padfPrjParams[i] = 0.0;

    string pszProj = ReadElement("CoordSystem", "Projection", csyFileName);
    string pszEllips = ReadElement("CoordSystem", "Ellipsoid", csyFileName);

    //fetch info about a custom ellipsoid
    if( STARTS_WITH_CI(pszEllips.c_str(), "User Defined") )
    {
        padfPrjParams[0] = ReadPrjParms("Ellipsoid", "a", csyFileName);
        padfPrjParams[2] = ReadPrjParms("Ellipsoid", "1/f", csyFileName);
    }
    else if( STARTS_WITH_CI(pszEllips.c_str(), "Sphere") )
    {
        padfPrjParams[0] = ReadPrjParms("CoordSystem", "Sphere Radius", csyFileName);
    }

    padfPrjParams[3] = ReadPrjParms("Projection", "False Easting", csyFileName);
    padfPrjParams[4] = ReadPrjParms("Projection", "False Northing", csyFileName);

    padfPrjParams[5] = ReadPrjParms("Projection", "Central Parallel", csyFileName);
    padfPrjParams[6] = ReadPrjParms("Projection", "Central Meridian", csyFileName);

    padfPrjParams[7] = ReadPrjParms("Projection", "Standard Parallel 1", csyFileName);
    padfPrjParams[8] = ReadPrjParms("Projection", "Standard Parallel 2", csyFileName);

    padfPrjParams[9] = ReadPrjParms("Projection", "Scale Factor", csyFileName);
    padfPrjParams[10] = ReadPrjParms("Projection", "Latitude of True Scale", csyFileName);
    padfPrjParams[11] = ReadPrjParms("Projection", "Zone", csyFileName);
    padfPrjParams[12] = ReadPrjParms("Projection", ILW_Height_Persp_Center, csyFileName);

    return true;
}

/************************************************************************/
/*                          mapTMParms                                  */
/************************************************************************/
/**
 * fetch the parameters from ILWIS projection definition for
 * --- Gauss-Krueger Germany.
 * --- Gauss Colombia
 * --- Gauss-Boaga Italy
**/
static int mapTMParms(string sProj, double dfZone, double &dfFalseEasting, double &dfCentralMeridian)
{
    if( STARTS_WITH_CI(sProj.c_str(), "Gauss-Krueger Germany") )
    {
        //Zone number must be in the range 1 to 3
        dfCentralMeridian = 6.0 + (dfZone - 1) * 3;
        dfFalseEasting = 2500000 + (dfZone - 1) * 1000000;
    }
    else if( STARTS_WITH_CI(sProj.c_str(), "Gauss-Boaga Italy") )
    {
        if ( dfZone == 1)
        {
            dfCentralMeridian = 9;
            dfFalseEasting = 1500000;
        }
        else if ( dfZone == 2)
        {
            dfCentralMeridian = 15;
            dfFalseEasting = 2520000;
        }
        else
            return false;
    }
    else if( STARTS_WITH_CI(sProj.c_str(), "Gauss Colombia") )
    {
        //Zone number must be in the range 1 to 4
        dfCentralMeridian = -77.08097220 + (dfZone - 1) * 3;
    }
    return true;
}

/************************************************************************/
/*                          scaleFromLATTS()                             */
/************************************************************************/
/**
 * Compute the scale factor from Latitude_Of_True_Scale parameter.
 *
**/
static void scaleFromLATTS( string sEllips, double phits, double &scale )
{
    if( STARTS_WITH_CI(sEllips.c_str(), "Sphere") )
    {
        scale = cos(phits);
    }
    else
    {
        const IlwisEllips *piwEllips =  iwEllips;
        double e2 = 0.0;
        while ( piwEllips->pszIlwisEllips )
        {
            if( EQUALN( sEllips.c_str(), piwEllips->pszIlwisEllips, strlen(piwEllips->pszIlwisEllips) ) )
            {
                double a = piwEllips->semiMajor;
                double b = a * ( 1 - piwEllips->invFlattening);
                e2 = ( a*a - b*b ) /( a*a );
                break;
            }
            piwEllips++;
        }
        scale = cos(phits) / sqrt (1. - e2 * sin(phits) * sin(phits));
    }
}

/************************************************************************/
/*                          ReadProjection()                           */
/************************************************************************/

/**
 * Import coordinate system from ILWIS projection definition.
 *
 * The method will import projection definition in ILWIS,
 * It uses 13 parameters to define the coordinate system
 * and datum/ellipsoid specified in the padfPrjParams array.
 *
 * @param csyFileName Name of .csy file
**/

CPLErr ILWISDataset::ReadProjection( string csyFileName )
{
    string pszEllips;
    string pszDatum;
    string pszProj;

    //translate ILWIS pre-defined coordinate systems
    if( STARTS_WITH_CI(csyFileName.c_str(), "latlon.csy"))
    {
        pszProj = "LatLon";
        pszDatum = "";
        pszEllips = "Sphere";
    }
    else if ( STARTS_WITH_CI(csyFileName.c_str(), "LatlonWGS84.csy"))
    {
        pszProj = "LatLon";
        pszDatum = "WGS 1984";
        pszEllips = "WGS 84";
    }
    else
    {
        pszProj = ReadElement("CoordSystem", "Type", csyFileName);
        if( !STARTS_WITH_CI( pszProj.c_str(), "LatLon" ) )
            pszProj = ReadElement("CoordSystem", "Projection", csyFileName);
        pszDatum = ReadElement("CoordSystem", "Datum", csyFileName);
        pszEllips = ReadElement("CoordSystem", "Ellipsoid", csyFileName);
    }

/* -------------------------------------------------------------------- */
/*      Fetch array containing 13 coordinate system parameters          */
/* -------------------------------------------------------------------- */
    double     padfPrjParams[13];
    fetchParms(csyFileName, padfPrjParams);

    OGRSpatialReference oSRS;
/* -------------------------------------------------------------------- */
/*      Operate on the basis of the projection name.                    */
/* -------------------------------------------------------------------- */
    if( STARTS_WITH_CI(pszProj.c_str(), "LatLon") )
    {
        //set datum later
    }
    else if( STARTS_WITH_CI(pszProj.c_str(), "Albers EqualArea Conic") )
    {
        oSRS.SetProjCS("Albers EqualArea Conic");
        oSRS.SetACEA( padfPrjParams[7], padfPrjParams[8],
                      padfPrjParams[5], padfPrjParams[6],
                      padfPrjParams[3], padfPrjParams[4] );
    }
    else if( STARTS_WITH_CI(pszProj.c_str(), "Azimuthal Equidistant") )
    {
        oSRS.SetProjCS("Azimuthal Equidistant");
        oSRS.SetAE( padfPrjParams[5], padfPrjParams[6],
                    padfPrjParams[3], padfPrjParams[4] );
    }
    else if( STARTS_WITH_CI(pszProj.c_str(), "Central Cylindrical") )
    {
        //Use Central Parallel for dfStdP1
        //padfPrjParams[5] is always to zero
        oSRS.SetProjCS("Central Cylindrical");
        oSRS.SetCEA( padfPrjParams[5], padfPrjParams[6],
                     padfPrjParams[3], padfPrjParams[4] );
    }
    else if( STARTS_WITH_CI(pszProj.c_str(), "Cassini") )
    {
        //Use Latitude_Of_True_Scale for dfCenterLat
        //Scale Factor 1.0 should always be defined
        oSRS.SetProjCS("Cassini");
        oSRS.SetCS(  padfPrjParams[10], padfPrjParams[6],
                     padfPrjParams[3], padfPrjParams[4] );
    }
    else if( STARTS_WITH_CI(pszProj.c_str(), "DutchRD") )
    {
        oSRS.SetProjCS("DutchRD");
        oSRS.SetStereographic  (  52.156160556,  5.387638889,
                                  0.9999079,
                                  155000,  463000);
    }
    else if( STARTS_WITH_CI(pszProj.c_str(), "Equidistant Conic") )
    {
        oSRS.SetProjCS("Equidistant Conic");
        oSRS.SetEC(  padfPrjParams[7], padfPrjParams[8],
                     padfPrjParams[5], padfPrjParams[6],
                     padfPrjParams[3], padfPrjParams[4] );
    }
    else if( STARTS_WITH_CI(pszProj.c_str(), "Gauss-Krueger Germany") )
    {
        //FalseNorthing and CenterLat are always set to 0
        //Scale 1.0 is defined
        //FalseEasting and CentralMeridian are defined by the selected zone
        mapTMParms("Gauss-Krueger Germany", padfPrjParams[11],
                   padfPrjParams[3], padfPrjParams[6]);
        oSRS.SetProjCS("Gauss-Krueger Germany");
        oSRS.SetTM(  0, padfPrjParams[6],
                     1.0,
                     padfPrjParams[3], 0 );
    }
    else if ( STARTS_WITH_CI(pszProj.c_str(), "Gauss-Boaga Italy") )
    {
        //FalseNorthing and CenterLat are always set to 0
        //Scale 0.9996 is defined
        //FalseEasting and CentralMeridian are defined by the selected zone
        mapTMParms("Gauss-Boaga Italy", padfPrjParams[11],
                   padfPrjParams[3], padfPrjParams[6]);
        oSRS.SetProjCS("Gauss-Boaga Italy");
        oSRS.SetTM(  0, padfPrjParams[6],
                     0.9996,
                     padfPrjParams[3], 0 );
    }
    else if ( STARTS_WITH_CI(pszProj.c_str(), "Gauss Colombia"))
    {
        // 1000000 used for FalseNorthing and FalseEasting
        // 1.0 used for scale
        // CenterLat is defined 45.1609259259259
        // CentralMeridian is defined by the selected zone
        mapTMParms("Gauss Colombia", padfPrjParams[11],
                   padfPrjParams[3], padfPrjParams[6]);
        oSRS.SetProjCS("Gauss Colombia");
        oSRS.SetTM(  45.1609259259259, padfPrjParams[6],
                     1.0,
                     1000000, 1000000 );
    }
    else if( STARTS_WITH_CI(pszProj.c_str(), "Gnomonic") )
    {
        oSRS.SetProjCS("Gnomonic");
        oSRS.SetGnomonic( padfPrjParams[5], padfPrjParams[6],
                          padfPrjParams[3], padfPrjParams[4] );
    }
    else if( STARTS_WITH_CI(pszProj.c_str(), "Lambert Conformal Conic") )
    {
        // should use 1.0 for scale factor in Ilwis definition
        oSRS.SetProjCS("Lambert Conformal Conic");
        oSRS.SetLCC(	padfPrjParams[7], padfPrjParams[8],
                        padfPrjParams[5], padfPrjParams[6],
                        padfPrjParams[3], padfPrjParams[4] );
    }
    else if( STARTS_WITH_CI(pszProj.c_str(), "Lambert Cylind EqualArea") )
    {
        // Latitude_Of_True_Scale used for dfStdP1 ?
        oSRS.SetProjCS("Lambert Conformal Conic");
        oSRS.SetCEA(	padfPrjParams[10],
                        padfPrjParams[6],
                        padfPrjParams[3], padfPrjParams[4] );
    }
    else if( STARTS_WITH_CI(pszProj.c_str(), "Mercator") )
    {
        // use 0 for CenterLat, scale is computed from the
        // Latitude_Of_True_Scale
        scaleFromLATTS( pszEllips, padfPrjParams[10], padfPrjParams[9] );
        oSRS.SetProjCS("Mercator");
        oSRS.SetMercator(	0, padfPrjParams[6],
                                padfPrjParams[9],
                                padfPrjParams[3], padfPrjParams[4] );
    }
    else if( STARTS_WITH_CI(pszProj.c_str(), "Miller") )
    {
        // use 0 for CenterLat
        oSRS.SetProjCS("Miller");
        oSRS.SetMC(	0, padfPrjParams[6],
                        padfPrjParams[3], padfPrjParams[4] );
    }
    else if( STARTS_WITH_CI(pszProj.c_str(), "Mollweide") )
    {
        oSRS.SetProjCS("Mollweide");
        oSRS.SetMollweide(	padfPrjParams[6],
                                padfPrjParams[3], padfPrjParams[4] );
    }
    else if( STARTS_WITH_CI(pszProj.c_str(), "Orthographic") )
    {
        oSRS.SetProjCS("Orthographic");
        oSRS.SetOrthographic (	padfPrjParams[5], padfPrjParams[6],
                                padfPrjParams[3], padfPrjParams[4] );
    }
    else if( STARTS_WITH_CI(pszProj.c_str(), "Plate Carree") ||
             STARTS_WITH_CI(pszProj.c_str(), "Plate Rectangle"))
    {
        // set 0.0 for CenterLat for Plate Carree projection
        // skipp Latitude_Of_True_Scale for Plate Rectangle projection definition
        oSRS.SetProjCS(pszProj.c_str());
        oSRS.SetEquirectangular(	padfPrjParams[5], padfPrjParams[6],
                                        padfPrjParams[3], padfPrjParams[4] );
    }
    else if( STARTS_WITH_CI(pszProj.c_str(), "PolyConic") )
    {
        // skipp scale factor
        oSRS.SetProjCS("PolyConic");
        oSRS.SetPolyconic(	padfPrjParams[5], padfPrjParams[6],
                                padfPrjParams[3], padfPrjParams[4] );
    }
    else if( STARTS_WITH_CI(pszProj.c_str(), "Robinson") )
    {
        oSRS.SetProjCS("Robinson");
        oSRS.SetRobinson(	padfPrjParams[6],
                                padfPrjParams[3], padfPrjParams[4] );
    }
    else if( STARTS_WITH_CI(pszProj.c_str(), "Sinusoidal") )
    {
        oSRS.SetProjCS("Sinusoidal");
        oSRS.SetSinusoidal(	padfPrjParams[6],
                                padfPrjParams[3], padfPrjParams[4] );
    }
    else if( STARTS_WITH_CI(pszProj.c_str(), "Stereographic") )
    {
        oSRS.SetProjCS("Stereographic");
        oSRS.SetStereographic(	padfPrjParams[5], padfPrjParams[6],
                                padfPrjParams[9],
                                padfPrjParams[3], padfPrjParams[4] );
    }
    else if( STARTS_WITH_CI(pszProj.c_str(), "Transverse Mercator") )
    {
        oSRS.SetProjCS("Transverse Mercator");
        oSRS.SetStereographic(	padfPrjParams[5], padfPrjParams[6],
                                padfPrjParams[9],
                                padfPrjParams[3], padfPrjParams[4] );
    }
    else if( STARTS_WITH_CI(pszProj.c_str(), "UTM") )
    {
        string pszNH = ReadElement("Projection", "Northern Hemisphere", csyFileName);
        oSRS.SetProjCS("UTM");
        if( STARTS_WITH_CI(pszNH.c_str(), "Yes") )
            oSRS.SetUTM( (int) padfPrjParams[11], 1);
        else
            oSRS.SetUTM( (int) padfPrjParams[11], 0);
    }
    else if( STARTS_WITH_CI(pszProj.c_str(), "VanderGrinten") )
    {
        oSRS.SetVDG(	padfPrjParams[6],
                        padfPrjParams[3], padfPrjParams[4] );
    }
    else if( STARTS_WITH_CI(pszProj.c_str(), "GeoStationary Satellite") )
    {
        oSRS.SetGEOS( padfPrjParams[6],
                      padfPrjParams[12],
                      padfPrjParams[3],
                      padfPrjParams[4] );
    }
    else if( STARTS_WITH_CI(pszProj.c_str(), "MSG Perspective") )
    {
        oSRS.SetGEOS( padfPrjParams[6],
                      padfPrjParams[12],
                      padfPrjParams[3],
                      padfPrjParams[4] );
    }
    else
    {
        oSRS.SetLocalCS( pszProj.c_str() );
    }
/* -------------------------------------------------------------------- */
/*      Try to translate the datum/spheroid.                            */
/* -------------------------------------------------------------------- */

    if ( !oSRS.IsLocal() )
    {
        const IlwisDatums   *piwDatum = iwDatums;

        // Search for matching datum
        while ( piwDatum->pszIlwisDatum )
        {
            if( EQUALN( pszDatum.c_str(), piwDatum->pszIlwisDatum, strlen(piwDatum->pszIlwisDatum) ) )
            {
                OGRSpatialReference oOGR;
                oOGR.importFromEPSG( piwDatum->nEPSGCode );
                oSRS.CopyGeogCSFrom( &oOGR );
                break;
            }
            piwDatum++;
        } // End of searching for matching datum.


/* -------------------------------------------------------------------- */
/*      If no matching for datum definition, fetch info about an        */
/*      ellipsoid.  semi major axis is always returned in meters        */
/* -------------------------------------------------------------------- */
        const IlwisEllips *piwEllips =  iwEllips;
        if (pszEllips.length() == 0)
            pszEllips="Sphere";
        if ( !piwDatum->pszIlwisDatum )
        {
            while ( piwEllips->pszIlwisEllips )
            {
                if( EQUALN( pszEllips.c_str(), piwEllips->pszIlwisEllips, strlen(piwEllips->pszIlwisEllips) ) )
                {
                    double dfSemiMajor = piwEllips->semiMajor;
                    if( STARTS_WITH_CI(pszEllips.c_str(), "Sphere") && padfPrjParams[0] != 0 )
                    {
                        dfSemiMajor = padfPrjParams[0];
                    }
                    oSRS.SetGeogCS( CPLSPrintf(
                                        "Unknown datum based upon the %s ellipsoid",
                                        piwEllips->pszIlwisEllips ),
                                    CPLSPrintf(
                                        "Not specified (based on %s spheroid)",
                                        piwEllips->pszIlwisEllips ),
                                    piwEllips->pszIlwisEllips,
                                    dfSemiMajor,
                                    piwEllips->invFlattening,
                                    NULL, 0.0, NULL, 0.0 );
                    oSRS.SetAuthority( "SPHEROID", "EPSG", piwEllips->nEPSGCode );

                    break;
                }
                piwEllips++;
            } //end of searching for matching ellipsoid
        }

/* -------------------------------------------------------------------- */
/*      If no matching for ellipsoid definition, fetch info about an    */
/*			user defined ellipsoid. If cannot find, default to WGS 84       */
/* -------------------------------------------------------------------- */
        if ( !piwEllips->pszIlwisEllips )
        {

            if( STARTS_WITH_CI(pszEllips.c_str(), "User Defined") )
            {
                oSRS.SetGeogCS( "Unknown datum based upon the custom ellipsoid",
                                "Not specified (based on custom ellipsoid)",
                                "Custom ellipsoid",
                                padfPrjParams[0], padfPrjParams[2],
                                NULL, 0, NULL, 0 );
            }
            else
            {
                //if cannot find the user defined ellips, default to WGS84
                oSRS.SetWellKnownGeogCS( "WGS84" );
            }
        }

    } // end of if ( !IsLocal() )

/* -------------------------------------------------------------------- */
/*      Units translation                                          */
/* -------------------------------------------------------------------- */
    if( oSRS.IsLocal() || oSRS.IsProjected() )
    {
        oSRS.SetLinearUnits( SRS_UL_METER, 1.0 );
    }
    oSRS.FixupOrdering();
    CPLFree(pszProjection);
    oSRS.exportToWkt( &pszProjection );

    return CE_None;
}

static void WriteFalseEastNorth(string csFileName, OGRSpatialReference oSRS)
{
    WriteElement("Projection", ILW_False_Easting, csFileName,
                 oSRS.GetNormProjParm(SRS_PP_FALSE_EASTING, 0.0));
    WriteElement("Projection", ILW_False_Northing, csFileName,
                 oSRS.GetNormProjParm(SRS_PP_FALSE_NORTHING, 0.0));
}

static void WriteProjectionName(string csFileName, string stProjection)
{
    WriteElement("CoordSystem", "Type", csFileName, "Projection");
    WriteElement("CoordSystem", "Projection", csFileName, stProjection);
}

static void WriteUTM(string csFileName, OGRSpatialReference oSRS)
{
    int	bNorth;

    int nZone = oSRS.GetUTMZone( &bNorth );
    WriteElement("CoordSystem", "Type", csFileName, "Projection");
    WriteElement("CoordSystem", "Projection", csFileName, "UTM");
    if (bNorth)
        WriteElement("Projection", "Northern Hemisphere", csFileName, "Yes");
    else
        WriteElement("Projection", "Northern Hemisphere", csFileName, "No");
    WriteElement("Projection", "Zone", csFileName, nZone);
}

static void WriteAlbersConicEqualArea(string csFileName, OGRSpatialReference oSRS)
{
    WriteProjectionName(csFileName, "Albers EqualArea Conic");
    WriteFalseEastNorth(csFileName, oSRS);
    WriteElement("Projection", ILW_Central_Meridian, csFileName,
                 oSRS.GetNormProjParm(SRS_PP_CENTRAL_MERIDIAN, 0.0));
    WriteElement("Projection", ILW_Central_Parallel, csFileName,
                 oSRS.GetNormProjParm(SRS_PP_LATITUDE_OF_ORIGIN, 0.0));
    WriteElement("Projection", ILW_Standard_Parallel_1, csFileName,
                 oSRS.GetNormProjParm(SRS_PP_STANDARD_PARALLEL_1, 0.0));
    WriteElement("Projection", ILW_Standard_Parallel_2, csFileName,
                 oSRS.GetNormProjParm(SRS_PP_STANDARD_PARALLEL_2, 0.0));
}

static void WriteAzimuthalEquidistant(string csFileName, OGRSpatialReference oSRS)
{
    WriteProjectionName(csFileName, "Azimuthal Equidistant");
    WriteFalseEastNorth(csFileName, oSRS);
    WriteElement("Projection", ILW_Central_Meridian, csFileName,
                 oSRS.GetNormProjParm(SRS_PP_CENTRAL_MERIDIAN, 0.0));
    WriteElement("Projection", ILW_Central_Parallel, csFileName,
                 oSRS.GetNormProjParm(SRS_PP_LATITUDE_OF_ORIGIN, 0.0));
    WriteElement("Projection", ILW_Scale_Factor, csFileName, "1.0000000000");
}

static void WriteCylindricalEqualArea(string csFileName, OGRSpatialReference oSRS)
{
    WriteProjectionName(csFileName, "Central Cylindrical");
    WriteFalseEastNorth(csFileName, oSRS);
    WriteElement("Projection", ILW_Central_Meridian, csFileName,
                 oSRS.GetNormProjParm(SRS_PP_CENTRAL_MERIDIAN, 0.0));
}

static void WriteCassiniSoldner(string csFileName, OGRSpatialReference oSRS)
{
    WriteProjectionName(csFileName, "Cassini");
    WriteFalseEastNorth(csFileName, oSRS);
    WriteElement("Projection", ILW_Central_Meridian, csFileName,
                 oSRS.GetNormProjParm(SRS_PP_CENTRAL_MERIDIAN, 0.0));
    WriteElement("Projection", ILW_Latitude_True_Scale, csFileName,
                 oSRS.GetNormProjParm(SRS_PP_LATITUDE_OF_ORIGIN, 0.0));
    WriteElement("Projection", ILW_Scale_Factor, csFileName, "1.0000000000");
}

static void WriteStereographic(string csFileName, OGRSpatialReference oSRS)
{
    WriteProjectionName(csFileName, "Stereographic");
    WriteFalseEastNorth(csFileName, oSRS);
    WriteElement("Projection", ILW_Central_Meridian, csFileName,
                 oSRS.GetNormProjParm(SRS_PP_CENTRAL_MERIDIAN, 0.0));
    WriteElement("Projection", ILW_Central_Parallel, csFileName,
                 oSRS.GetNormProjParm(SRS_PP_LATITUDE_OF_ORIGIN, 0.0));
    WriteElement("Projection", ILW_Scale_Factor, csFileName,
                 oSRS.GetNormProjParm(SRS_PP_SCALE_FACTOR, 0.0));
}

static void WriteEquidistantConic(string csFileName, OGRSpatialReference oSRS)
{
    WriteProjectionName(csFileName, "Equidistant Conic");
    WriteFalseEastNorth(csFileName, oSRS);
    WriteElement("Projection", ILW_Central_Meridian, csFileName,
                 oSRS.GetNormProjParm(SRS_PP_CENTRAL_MERIDIAN, 0.0));
    WriteElement("Projection", ILW_Central_Parallel, csFileName,
                 oSRS.GetNormProjParm(SRS_PP_LATITUDE_OF_ORIGIN, 0.0));
    WriteElement("Projection", ILW_Standard_Parallel_1, csFileName,
                 oSRS.GetNormProjParm(SRS_PP_STANDARD_PARALLEL_1, 0.0));
    WriteElement("Projection", ILW_Standard_Parallel_2, csFileName,
                 oSRS.GetNormProjParm(SRS_PP_STANDARD_PARALLEL_2, 0.0));
}

static void WriteTransverseMercator(string csFileName, OGRSpatialReference oSRS)
{
    WriteProjectionName(csFileName, "Transverse Mercator");
    WriteFalseEastNorth(csFileName, oSRS);
    WriteElement("Projection", ILW_Central_Meridian, csFileName,
                 oSRS.GetNormProjParm(SRS_PP_CENTRAL_MERIDIAN, 0.0));
    WriteElement("Projection", ILW_Central_Parallel, csFileName,
                 oSRS.GetNormProjParm(SRS_PP_LATITUDE_OF_ORIGIN, 0.0));
    WriteElement("Projection", ILW_Scale_Factor, csFileName,
                 oSRS.GetNormProjParm(SRS_PP_SCALE_FACTOR, 0.0));
}

static void WriteGnomonic(string csFileName, OGRSpatialReference oSRS)
{
    WriteProjectionName(csFileName, "Gnomonic");
    WriteFalseEastNorth(csFileName, oSRS);
    WriteElement("Projection", ILW_Central_Meridian, csFileName,
                 oSRS.GetNormProjParm(SRS_PP_CENTRAL_MERIDIAN, 0.0));
    WriteElement("Projection", ILW_Central_Parallel, csFileName,
                 oSRS.GetNormProjParm(SRS_PP_LATITUDE_OF_ORIGIN, 0.0));
}

static void WriteLambertConformalConic(string csFileName, OGRSpatialReference oSRS)
{
    WriteProjectionName(csFileName, "Lambert Conformal Conic");
    WriteFalseEastNorth(csFileName, oSRS);
    WriteElement("Projection", ILW_Central_Meridian, csFileName,
                 oSRS.GetNormProjParm(SRS_PP_CENTRAL_MERIDIAN, 0.0));
    WriteElement("Projection", ILW_Central_Parallel, csFileName,
                 oSRS.GetNormProjParm(SRS_PP_LATITUDE_OF_ORIGIN, 0.0));
    WriteElement("Projection", ILW_Scale_Factor, csFileName, "1.0000000000");
}

static void WriteLambertConformalConic2SP(string csFileName, OGRSpatialReference oSRS)
{
    WriteProjectionName(csFileName, "Lambert Conformal Conic");
    WriteFalseEastNorth(csFileName, oSRS);
    WriteElement("Projection", ILW_Central_Meridian, csFileName,
                 oSRS.GetNormProjParm(SRS_PP_CENTRAL_MERIDIAN, 0.0));
    WriteElement("Projection", ILW_Central_Parallel, csFileName,
                 oSRS.GetNormProjParm(SRS_PP_LATITUDE_OF_ORIGIN, 0.0));
    WriteElement("Projection", ILW_Scale_Factor, csFileName, "1.0000000000");
    WriteElement("Projection", ILW_Standard_Parallel_1, csFileName,
                 oSRS.GetNormProjParm(SRS_PP_STANDARD_PARALLEL_1, 0.0));
    WriteElement("Projection", ILW_Standard_Parallel_2, csFileName,
                 oSRS.GetNormProjParm(SRS_PP_STANDARD_PARALLEL_2, 0.0));
}

static void WriteLambertAzimuthalEqualArea(string csFileName, OGRSpatialReference oSRS)
{
    WriteProjectionName(csFileName, "Lambert Azimuthal EqualArea");
    WriteFalseEastNorth(csFileName, oSRS);
    WriteElement("Projection", ILW_Central_Meridian, csFileName,
                 oSRS.GetNormProjParm(SRS_PP_CENTRAL_MERIDIAN, 0.0));
    WriteElement("Projection", ILW_Central_Parallel, csFileName,
                 oSRS.GetNormProjParm(SRS_PP_LATITUDE_OF_ORIGIN, 0.0));
}

static void WriteMercator_1SP(string csFileName, OGRSpatialReference oSRS)
{
    WriteProjectionName(csFileName, "Mercator");
    WriteFalseEastNorth(csFileName, oSRS);
    WriteElement("Projection", ILW_Central_Meridian, csFileName,
                 oSRS.GetNormProjParm(SRS_PP_CENTRAL_MERIDIAN, 0.0));
    WriteElement("Projection", ILW_Latitude_True_Scale, csFileName,
                 oSRS.GetNormProjParm(SRS_PP_LATITUDE_OF_ORIGIN, 0.0));
}

static void WriteMillerCylindrical(string csFileName, OGRSpatialReference oSRS)
{
    WriteProjectionName(csFileName, "Miller");
    WriteFalseEastNorth(csFileName, oSRS);
    WriteElement("Projection", ILW_Central_Meridian, csFileName,
                 oSRS.GetNormProjParm(SRS_PP_CENTRAL_MERIDIAN, 0.0));
}

static void WriteMolleweide(string csFileName, OGRSpatialReference oSRS)
{
    WriteProjectionName(csFileName, "Mollweide");
    WriteFalseEastNorth(csFileName, oSRS);
    WriteElement("Projection", ILW_Central_Meridian, csFileName,
                 oSRS.GetNormProjParm(SRS_PP_CENTRAL_MERIDIAN, 0.0));
}

static void WriteOrthographic(string csFileName, OGRSpatialReference oSRS)
{
    WriteProjectionName(csFileName, "Orthographic");
    WriteFalseEastNorth(csFileName, oSRS);
    WriteElement("Projection", ILW_Central_Meridian, csFileName,
                 oSRS.GetNormProjParm(SRS_PP_CENTRAL_MERIDIAN, 0.0));
    WriteElement("Projection", ILW_Central_Parallel, csFileName,
                 oSRS.GetNormProjParm(SRS_PP_LATITUDE_OF_ORIGIN, 0.0));
}

static void WritePlateRectangle(string csFileName, OGRSpatialReference oSRS)
{
    WriteProjectionName(csFileName, "Plate Rectangle");
    WriteFalseEastNorth(csFileName, oSRS);
    WriteElement("Projection", ILW_Central_Meridian, csFileName,
                 oSRS.GetNormProjParm(SRS_PP_CENTRAL_MERIDIAN, 0.0));
    WriteElement("Projection", ILW_Central_Parallel, csFileName,
                 oSRS.GetNormProjParm(SRS_PP_LATITUDE_OF_ORIGIN, 0.0));
    WriteElement("Projection", ILW_Latitude_True_Scale, csFileName, "0.0000000000");
}

static void WritePolyConic(string csFileName, OGRSpatialReference oSRS)
{
    WriteProjectionName(csFileName, "PolyConic");
    WriteFalseEastNorth(csFileName, oSRS);
    WriteElement("Projection", ILW_Central_Meridian, csFileName,
                 oSRS.GetNormProjParm(SRS_PP_CENTRAL_MERIDIAN, 0.0));
    WriteElement("Projection", ILW_Central_Parallel, csFileName,
                 oSRS.GetNormProjParm(SRS_PP_LATITUDE_OF_ORIGIN, 0.0));
    WriteElement("Projection", ILW_Scale_Factor, csFileName, "1.0000000000");
}

static void WriteRobinson(string csFileName, OGRSpatialReference oSRS)
{
    WriteProjectionName(csFileName, "Robinson");
    WriteFalseEastNorth(csFileName, oSRS);
    WriteElement("Projection", ILW_Central_Meridian, csFileName,
                 oSRS.GetNormProjParm(SRS_PP_CENTRAL_MERIDIAN, 0.0));
}

static void WriteSinusoidal(string csFileName, OGRSpatialReference oSRS)
{
    WriteProjectionName(csFileName, "Sinusoidal");
    WriteFalseEastNorth(csFileName, oSRS);
    WriteElement("Projection", ILW_Central_Meridian, csFileName,
                 oSRS.GetNormProjParm(SRS_PP_CENTRAL_MERIDIAN, 0.0));
}

static void WriteVanderGrinten(string csFileName, OGRSpatialReference oSRS)
{
    WriteProjectionName(csFileName, "VanderGrinten");
    WriteFalseEastNorth(csFileName, oSRS);
    WriteElement("Projection", ILW_Central_Meridian, csFileName,
                 oSRS.GetNormProjParm(SRS_PP_CENTRAL_MERIDIAN, 0.0));
}

static void WriteGeoStatSat(string csFileName, OGRSpatialReference oSRS)
{
    WriteProjectionName(csFileName, "GeoStationary Satellite");
    WriteFalseEastNorth(csFileName, oSRS);
    WriteElement("Projection", ILW_Central_Meridian, csFileName,
                 oSRS.GetNormProjParm(SRS_PP_CENTRAL_MERIDIAN, 0.0));
    WriteElement("Projection", ILW_Scale_Factor, csFileName, "1.0000000000");
    WriteElement("Projection", ILW_Height_Persp_Center, csFileName,
                 oSRS.GetNormProjParm(SRS_PP_SATELLITE_HEIGHT, 35785831.0));
}

/************************************************************************/
/*                          WriteProjection()                           */
/************************************************************************/
/**
 * Export coordinate system in ILWIS projection definition.
 *
 * Converts the loaded coordinate reference system into ILWIS projection
 * definition to the extent possible.
 */
CPLErr ILWISDataset::WriteProjection()

{
    OGRSpatialReference oSRS;
    OGRSpatialReference *poGeogSRS = NULL;
    char		*pszP = pszProjection;

    string csFileName = CPLResetExtension(osFileName, "csy" );
    string pszBaseName = string(CPLGetBasename( osFileName ));
    string pszPath = string(CPLGetPath( osFileName ));
    bool bProjection = ((pszProjection != NULL) && (strlen(pszProjection)>0));
    bool bHaveSRS;
    if( bProjection && (oSRS.importFromWkt( &pszP ) == OGRERR_NONE) )
    {
        bHaveSRS = true;
    }
    else
        bHaveSRS = false;

    const IlwisDatums   *piwDatum = iwDatums;
    string pszEllips;
    string pszDatum;
    string pszProj;

/* -------------------------------------------------------------------- */
/*      Collect datum/ellips information.                                      */
/* -------------------------------------------------------------------- */
    if( bHaveSRS )
    {
        poGeogSRS = oSRS.CloneGeogCS();
    }

    string grFileName = CPLResetExtension(osFileName, "grf" );
    string csy;
    if( poGeogSRS )
    {
        csy = pszBaseName + ".csy";

        WriteElement("Ilwis", "Type", csFileName, "CoordSystem");
        pszDatum = poGeogSRS->GetAttrValue( "GEOGCS|DATUM" );

        /* WKT to ILWIS translation */
        while ( piwDatum->pszWKTDatum)
        {
            if( EQUALN( pszDatum.c_str(), piwDatum->pszWKTDatum, strlen(piwDatum->pszWKTDatum) ) )
            {
                WriteElement("CoordSystem", "Datum", csFileName, piwDatum->pszIlwisDatum);
                break;
            }
            piwDatum++;
        } // End of searching for matching datum.
        WriteElement("CoordSystem", "Width", csFileName, 28);
        pszEllips = poGeogSRS->GetAttrValue( "GEOGCS|DATUM|SPHEROID" );
        double a = poGeogSRS->GetSemiMajor();
        /* b = */ poGeogSRS->GetSemiMinor();
        double f = poGeogSRS->GetInvFlattening();
        WriteElement("CoordSystem", "Ellipsoid", csFileName, "User Defined");
        WriteElement("Ellipsoid", "a", csFileName, a);
        WriteElement("Ellipsoid", "1/f", csFileName, f);
    }
    else
        csy = "unknown.csy";

/* -------------------------------------------------------------------- */
/*	Determine to write a geo-referencing file for the dataset to create */
/* -------------------------------------------------------------------- */
    if( adfGeoTransform[0] != 0.0 || adfGeoTransform[1] != 1.0
        || adfGeoTransform[2] != 0.0 || adfGeoTransform[3] != 0.0
        || adfGeoTransform[4] != 0.0 || fabs(adfGeoTransform[5]) != 1.0)
        WriteElement("GeoRef", "CoordSystem", grFileName, csy);

/* -------------------------------------------------------------------- */
/*  Recognise various projections.                                      */
/* -------------------------------------------------------------------- */
    const char * pszProjName = NULL;

    if( bHaveSRS )
        pszProjName = oSRS.GetAttrValue( "PROJCS|PROJECTION" );

    if( pszProjName == NULL )
    {
        if( bHaveSRS && oSRS.IsGeographic() )
        {
            WriteElement("CoordSystem", "Type", csFileName, "LatLon");
        }
    }
    else if( oSRS.GetUTMZone( NULL ) != 0 )
    {
        WriteUTM(csFileName, oSRS);
    }
    else if( EQUAL(pszProjName,SRS_PT_ALBERS_CONIC_EQUAL_AREA) )
    {
        WriteAlbersConicEqualArea(csFileName, oSRS);
    }
    else if( EQUAL(pszProjName,SRS_PT_AZIMUTHAL_EQUIDISTANT) )
    {
        WriteAzimuthalEquidistant(csFileName, oSRS);
    }
    else if( EQUAL(pszProjName,SRS_PT_CYLINDRICAL_EQUAL_AREA) )
    {
        WriteCylindricalEqualArea(csFileName, oSRS);
    }
    else if( EQUAL(pszProjName,SRS_PT_CASSINI_SOLDNER) )
    {
        WriteCassiniSoldner(csFileName, oSRS);
    }
    else if( EQUAL(pszProjName,SRS_PT_STEREOGRAPHIC) )
    {
        WriteStereographic(csFileName, oSRS);
    }
    else if( EQUAL(pszProjName,SRS_PT_EQUIDISTANT_CONIC) )
    {
        WriteEquidistantConic(csFileName, oSRS);
    }
    else if( EQUAL(pszProjName,SRS_PT_TRANSVERSE_MERCATOR) )
    {
        WriteTransverseMercator(csFileName, oSRS);
    }
    else if( EQUAL(pszProjName,SRS_PT_GNOMONIC) )
    {
        WriteGnomonic(csFileName, oSRS);
    }
    else if( EQUAL(pszProjName,"Lambert_Conformal_Conic") )
    {
        WriteLambertConformalConic(csFileName, oSRS);
    }
    else if( EQUAL(pszProjName,SRS_PT_LAMBERT_CONFORMAL_CONIC_1SP) )
    {
        WriteLambertConformalConic(csFileName, oSRS);
    }
    else if( EQUAL(pszProjName,SRS_PT_LAMBERT_CONFORMAL_CONIC_2SP) )
    {
        WriteLambertConformalConic2SP(csFileName, oSRS);
    }
    else if( EQUAL(pszProjName,SRS_PT_LAMBERT_AZIMUTHAL_EQUAL_AREA) )
    {
        WriteLambertAzimuthalEqualArea(csFileName, oSRS);
    }
    else if( EQUAL(pszProjName,SRS_PT_MERCATOR_1SP) )
    {
        WriteMercator_1SP(csFileName, oSRS);
    }
    else if( EQUAL(pszProjName,SRS_PT_MILLER_CYLINDRICAL) )
    {
        WriteMillerCylindrical(csFileName, oSRS);
    }
    else if( EQUAL(pszProjName,SRS_PT_MOLLWEIDE) )
    {
        WriteMolleweide(csFileName, oSRS);
    }
    else if( EQUAL(pszProjName,SRS_PT_ORTHOGRAPHIC) )
    {
        WriteOrthographic(csFileName, oSRS);
    }
    else if( EQUAL(pszProjName,SRS_PT_EQUIRECTANGULAR) )
    {
        WritePlateRectangle(csFileName, oSRS);
    }
    else if( EQUAL(pszProjName,SRS_PT_POLYCONIC) )
    {
        WritePolyConic(csFileName, oSRS);
    }
    else if( EQUAL(pszProjName,SRS_PT_ROBINSON) )
    {
        WriteRobinson(csFileName, oSRS);
    }
    else if( EQUAL(pszProjName,SRS_PT_SINUSOIDAL) )
    {
        WriteSinusoidal(csFileName, oSRS);
    }
    else if( EQUAL(pszProjName,SRS_PT_VANDERGRINTEN) )
    {
        WriteVanderGrinten(csFileName, oSRS);
    }
    else if( EQUAL(pszProjName,SRS_PT_GEOSTATIONARY_SATELLITE) )
    {
        WriteGeoStatSat(csFileName, oSRS);
    }
    else
    {
        // Projection unknown by ILWIS
    }

/* -------------------------------------------------------------------- */
/*      Cleanup                                                         */
/* -------------------------------------------------------------------- */
    if( poGeogSRS != NULL )
        delete poGeogSRS;

    return CE_None;
}
