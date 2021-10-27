/******************************************************************************
 *
 * Purpose:  Implementation of the CPCIDSKGeoref class.
 *
 ******************************************************************************
 * Copyright (c) 2009
 * PCI Geomatics, 90 Allstate Parkway, Markham, Ontario, Canada.
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

#include "pcidsk_exception.h"
#include "segment/cpcidskgeoref.h"
#include "core/pcidsk_utils.h"
#include <cassert>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <cctype>

using namespace PCIDSK;

static double PAK2PCI( double deg, int function );

#ifndef ABS
#  define ABS(x)        ((x<0) ? (-1*(x)) : x)
#endif

/************************************************************************/
/*                           CPCIDSKGeoref()                            */
/************************************************************************/

CPCIDSKGeoref::CPCIDSKGeoref( PCIDSKFile *fileIn, int segmentIn,
                              const char *segment_pointer )
        : CPCIDSKSegment( fileIn, segmentIn, segment_pointer )

{
    loaded = false;
    a1 = 0;
    a2 = 0;
    xrot = 0;
    b1 = 0;
    yrot = 0;
    b3 = 0;
}

/************************************************************************/
/*                           ~CPCIDSKGeoref()                           */
/************************************************************************/

CPCIDSKGeoref::~CPCIDSKGeoref()

{
}

/************************************************************************/
/*                             Initialize()                             */
/************************************************************************/

void CPCIDSKGeoref::Initialize()

{
    // Note: we depend on Load() reacting gracefully to an uninitialized
    // georeferencing segment.

    WriteSimple( "PIXEL", 0.0, 1.0, 0.0, 0.0, 0.0, 1.0 );
}

/************************************************************************/
/*                                Load()                                */
/************************************************************************/

void CPCIDSKGeoref::Load()

{
    if( loaded )
        return;

    // TODO: this should likely be protected by a mutex.

/* -------------------------------------------------------------------- */
/*      Load the segment contents into a buffer.                        */
/* -------------------------------------------------------------------- */
    // data_size < 1024 will throw an exception in SetSize()
    seg_data.SetSize( data_size < 1024 ? -1 : (int) (data_size - 1024) );

    ReadFromFile( seg_data.buffer, 0, data_size - 1024 );

/* -------------------------------------------------------------------- */
/*      Handle simple case of a POLYNOMIAL.                             */
/* -------------------------------------------------------------------- */
    if( seg_data.buffer_size >= static_cast<int>(strlen("POLYNOMIAL")) &&
        STARTS_WITH(seg_data.buffer, "POLYNOMIAL") )
    {
        seg_data.Get(32,16,geosys);

        if( seg_data.GetInt(48,8) != 3 || seg_data.GetInt(56,8) != 3 )
            return ThrowPCIDSKException( "Unexpected number of coefficients in POLYNOMIAL GEO segment." );

        a1   = seg_data.GetDouble(212+26*0,26);
        a2   = seg_data.GetDouble(212+26*1,26);
        xrot = seg_data.GetDouble(212+26*2,26);

        b1   = seg_data.GetDouble(1642+26*0,26);
        yrot = seg_data.GetDouble(1642+26*1,26);
        b3   = seg_data.GetDouble(1642+26*2,26);
    }

/* -------------------------------------------------------------------- */
/*      Handle the case of a PROJECTION segment - for now we ignore     */
/*      the actual projection parameters.                               */
/* -------------------------------------------------------------------- */
    else if( seg_data.buffer_size >= static_cast<int>(strlen("PROJECTION")) &&
             STARTS_WITH(seg_data.buffer, "PROJECTION") )
    {
        seg_data.Get(32,16,geosys);

        if( seg_data.GetInt(48,8) != 3 || seg_data.GetInt(56,8) != 3 )
            return ThrowPCIDSKException( "Unexpected number of coefficients in PROJECTION GEO segment." );

        a1   = seg_data.GetDouble(1980+26*0,26);
        a2   = seg_data.GetDouble(1980+26*1,26);
        xrot = seg_data.GetDouble(1980+26*2,26);

        b1   = seg_data.GetDouble(2526+26*0,26);
        yrot = seg_data.GetDouble(2526+26*1,26);
        b3   = seg_data.GetDouble(2526+26*2,26);
    }

/* -------------------------------------------------------------------- */
/*      Blank segment, just created and we just initialize things a bit.*/
/* -------------------------------------------------------------------- */
    else if( seg_data.buffer_size >= 16 && memcmp(seg_data.buffer,
                    "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0",16) == 0 )
    {
        geosys = "";

        a1 = 0.0;
        a2 = 1.0;
        xrot = 0.0;
        b1 = 0.0;
        yrot = 0.0;
        b3 = 1.0;
    }

    else
    {
        return ThrowPCIDSKException( "Unexpected GEO segment type: %s",
                              seg_data.Get(0,16) );
    }

    loaded = true;
}

/************************************************************************/
/*                             GetGeosys()                              */
/************************************************************************/

std::string CPCIDSKGeoref::GetGeosys()

{
    Load();
    return geosys;
}

/************************************************************************/
/*                            GetTransform()                            */
/************************************************************************/

void CPCIDSKGeoref::GetTransform( double &a1Out, double &a2Out, double &xrotOut,
                                  double &b1Out, double &yrotOut, double &b3Out )

{
    Load();

    a1Out   = this->a1;
    a2Out   = this->a2;
    xrotOut = this->xrot;
    b1Out   = this->b1;
    yrotOut = this->yrot;
    b3Out   = this->b3;
}

/************************************************************************/
/*                           GetParameters()                            */
/************************************************************************/

std::vector<double> CPCIDSKGeoref::GetParameters()

{
    unsigned int  i;
    std::vector<double> params;

    Load();

    params.resize(18);

    if( !STARTS_WITH(seg_data.buffer, "PROJECTION") )
    {
        for( i = 0; i < 17; i++ )
            params[i] = 0.0;
        params[17] = -1.0;
    }
    else
    {
        for( i = 0; i < 17; i++ )
            params[i] = seg_data.GetDouble(80+26*i,26);

        double dfUnitsCode = seg_data.GetDouble(1900, 26);

        // if the units code is undefined, use the IOUnits filed
        if(dfUnitsCode == -1)
        {
            std::string grid_units;
            seg_data.Get(64,16,grid_units);

            if( STARTS_WITH_CI( grid_units.c_str(), "DEG" /* "DEGREE" */) )
                params[17] = (double) (int) UNIT_DEGREE;
            else if( STARTS_WITH_CI( grid_units.c_str(), "MET") )
                params[17] = (double) (int) UNIT_METER;
            else if( STARTS_WITH_CI( grid_units.c_str(), "FOOT") )
                params[17] = (double) (int) UNIT_US_FOOT;
            else if( STARTS_WITH_CI( grid_units.c_str(), "FEET") )
                params[17] = (double) (int) UNIT_US_FOOT;
            else if( STARTS_WITH_CI( grid_units.c_str(), "INTL " /* "INTL FOOT" */) )
                params[17] = (double) (int) UNIT_INTL_FOOT;
            else
                params[17] = -1.0; /* unknown */
        }
        else
        {
            params[17] = dfUnitsCode;
        }

    }

    return params;
}

/************************************************************************/
/*                            WriteSimple()                             */
/************************************************************************/

void CPCIDSKGeoref::WriteSimple( std::string const& geosysIn,
                                 double a1In, double a2In, double xrotIn,
                                 double b1In, double yrotIn, double b3In )

{
    Load();

    std::string geosys_clean(ReformatGeosys( geosysIn ));

/* -------------------------------------------------------------------- */
/*      Establish the appropriate units code when possible.             */
/* -------------------------------------------------------------------- */
    std::string units_code = "METER";

    if( STARTS_WITH_CI(geosys_clean.c_str(), "FOOT") )
        units_code = "FOOT";
    else if( STARTS_WITH_CI(geosys_clean.c_str(), "SPAF") )
        units_code = "FOOT";
    else if( STARTS_WITH_CI(geosys_clean.c_str(), "SPIF") )
        units_code = "INTL FOOT";
    else if( STARTS_WITH_CI(geosys_clean.c_str(), "LONG") )
        units_code = "DEGREE";

/* -------------------------------------------------------------------- */
/*      Write a fairly simple PROJECTION segment.                       */
/* -------------------------------------------------------------------- */
    seg_data.SetSize( 6 * 512 );

    seg_data.Put( " ", 0, seg_data.buffer_size );

    // SD.PRO.P1
    seg_data.Put( "PROJECTION", 0, 16 );

    // SD.PRO.P2
    seg_data.Put( "PIXEL", 16, 16 );

    // SD.PRO.P3
    seg_data.Put( geosys_clean.c_str(), 32, 16 );

    // SD.PRO.P4
    seg_data.Put( 3, 48, 8 );

    // SD.PRO.P5
    seg_data.Put( 3, 56, 8 );

    // SD.PRO.P6
    seg_data.Put( units_code.c_str(), 64, 16 );

    // SD.PRO.P7 - P22
    for( int i = 0; i < 17; i++ )
        seg_data.Put( 0.0,   80 + i*26, 26, "%26.18E" );

    // SD.PRO.P24
    PrepareGCTPFields();

    // SD.PRO.P26
    seg_data.Put( a1In,  1980 + 0*26, 26, "%26.18E" );
    seg_data.Put( a2In,  1980 + 1*26, 26, "%26.18E" );
    seg_data.Put( xrotIn,1980 + 2*26, 26, "%26.18E" );

    // SD.PRO.P27
    seg_data.Put( b1In,   2526 + 0*26, 26, "%26.18E" );
    seg_data.Put( yrotIn, 2526 + 1*26, 26, "%26.18E" );
    seg_data.Put( b3In,   2526 + 2*26, 26, "%26.18E" );

    WriteToFile( seg_data.buffer, 0, seg_data.buffer_size );

    loaded = false;
}

/************************************************************************/
/*                          WriteParameters()                           */
/************************************************************************/

void CPCIDSKGeoref::WriteParameters( std::vector<double> const& params )

{
    Load();

    if( params.size() < 17 )
        return ThrowPCIDSKException( "Did not get expected number of parameters in WriteParameters()" );

    unsigned int i;

    for( i = 0; i < 17; i++ )
        seg_data.Put(params[i],80+26*i,26,"%26.16f");

    if( params.size() >= 18 )
    {
        switch( (UnitCode) (int) params[17] )
        {
          case UNIT_DEGREE:
            seg_data.Put( "DEGREE", 64, 16 );
            break;

          case UNIT_METER:
            seg_data.Put( "METER", 64, 16 );
            break;

          case UNIT_US_FOOT:
            seg_data.Put( "FOOT", 64, 16 );
            break;

          case UNIT_INTL_FOOT:
            seg_data.Put( "INTL FOOT", 64, 16 );
            break;
        }
    }

    PrepareGCTPFields();

    WriteToFile( seg_data.buffer, 0, seg_data.buffer_size );

    // no need to mark loaded false, since we don't cache these parameters.
}

/************************************************************************/
/*                         GetUSGSParameters()                          */
/************************************************************************/

std::vector<double> CPCIDSKGeoref::GetUSGSParameters()

{
    unsigned int  i;
    std::vector<double> params;

    Load();

    params.resize(19);
    if( !STARTS_WITH(seg_data.buffer, "PROJECTION") )
    {
        for( i = 0; i < 19; i++ )
            params[i] = 0.0;
    }
    else
    {
        for( i = 0; i < 19; i++ )
            params[i] = seg_data.GetDouble(1458+26*i,26);
    }

    return params;
}

/************************************************************************/
/*                           ReformatGeosys()                           */
/*                                                                      */
/*      Put a geosys string into standard form.  Similar to what the    */
/*      DecodeGeosys() function in the PCI SDK does.                    */
/************************************************************************/

std::string CPCIDSKGeoref::ReformatGeosys( std::string const& geosysIn )

{
/* -------------------------------------------------------------------- */
/*      Put into a local buffer and pad out to 16 characters with       */
/*      spaces.                                                         */
/* -------------------------------------------------------------------- */
    char local_buf[33];

    strncpy( local_buf, geosysIn.c_str(), 16 );
    local_buf[16] = '\0';
    strcat( local_buf, "                " );
    local_buf[16] = '\0';

/* -------------------------------------------------------------------- */
/*      Extract the earth model from the geosys string.                 */
/* -------------------------------------------------------------------- */
    char earthmodel[5];
    const char *cp;
    int         i;
    char        last;

    cp = local_buf;
    while( cp < local_buf + 16 && cp[1] != '\0' )
        cp++;

    while( cp > local_buf && isspace(*cp) )
        cp--;

    last = '\0';
    while( cp > local_buf
           && (isdigit((unsigned char)*cp)
               || *cp == '-' || *cp == '+' ) )
    {
        if( last == '\0' )  last = *cp;
        cp--;
    }

    if( isdigit( (unsigned char)last ) &&
        ( *cp == 'D' || *cp == 'd' ||
          *cp == 'E' || *cp == 'e'    ) )
    {
        i = atoi( cp+1 );
        if(    i > -100 && i < 1000
               && (cp == local_buf
                   || ( cp >  local_buf && isspace( *(cp-1) ) )
                   )
               )
        {
            if( *cp == 'D' || *cp == 'd' )
                snprintf( earthmodel, sizeof(earthmodel), "D%03d", i );
            else
                snprintf( earthmodel, sizeof(earthmodel), "E%03d", i );
        }
        else
        {
            snprintf( earthmodel, sizeof(earthmodel), "    " );
        }
    }
    else
    {
        snprintf( earthmodel, sizeof(earthmodel), "    " );
    }

/* -------------------------------------------------------------------- */
/*      Identify by geosys string.                                      */
/* -------------------------------------------------------------------- */
    const char *ptr;
    int zone, ups_zone;
    char zone_code = ' ';

    if( STARTS_WITH_CI(local_buf, "PIX") )
    {
        strcpy( local_buf, "PIXEL           " );
    }
    else if( STARTS_WITH_CI(local_buf, "UTM") )
    {
        /* Attempt to find a zone and ellipsoid */
        for( ptr=local_buf+3; isspace(*ptr); ptr++ ) {}
        if( isdigit( (unsigned char)*ptr ) || *ptr == '-' )
        {
            zone = atoi(ptr);
            for( ; isdigit((unsigned char)*ptr) || *ptr == '-'; ptr++ ) {}
            for( ; isspace(*ptr); ptr++ ) {}
            if( isalpha(*ptr)
                && !isdigit((unsigned char)*(ptr+1))
                && ptr[1] != '-' )
                zone_code = *(ptr++);
        }
        else
            zone = -100;

        if( zone >= -60 && zone <= 60 && zone != 0 )
        {
            if( zone_code >= 'a' && zone_code <= 'z' )
                zone_code = zone_code - 'a' + 'A';

            if( zone_code == ' ' && zone < 0 )
                zone_code = 'C';

            zone = ABS(zone);

            snprintf( local_buf, sizeof(local_buf),
                     "UTM   %3d %c %4s",
                     zone, zone_code, earthmodel );
        }
        else
        {
            snprintf( local_buf, sizeof(local_buf),
                     "UTM         %4s",
                     earthmodel );
        }
        if( local_buf[14] == ' ' )
            local_buf[14] = '0';
        if( local_buf[13] == ' ' )
            local_buf[13] = '0';
    }
    else if( STARTS_WITH_CI(local_buf, "MET") )
    {
        snprintf( local_buf, sizeof(local_buf), "METRE       %4s", earthmodel );
    }
    else if( STARTS_WITH_CI(local_buf, "FEET") || STARTS_WITH_CI(local_buf, "FOOT"))
    {
        snprintf( local_buf, sizeof(local_buf), "FOOT        %4s", earthmodel );
    }
    else if( STARTS_WITH_CI(local_buf, "LAT") ||
             STARTS_WITH_CI(local_buf, "LON") )
    {
        snprintf( local_buf, sizeof(local_buf),
                 "LONG/LAT    %4s",
                 earthmodel );
    }
    else if( STARTS_WITH_CI(local_buf, "SPCS ") ||
             STARTS_WITH_CI(local_buf, "SPAF ") ||
             STARTS_WITH_CI(local_buf, "SPIF ") )
    {
        int nSPZone = 0;

        for( ptr=local_buf+4; isspace(*ptr); ptr++ ) {}
        nSPZone = atoi(ptr);

        if      ( STARTS_WITH_CI(local_buf, "SPCS ") )
            strcpy( local_buf, "SPCS " );
        else if ( STARTS_WITH_CI(local_buf, "SPAF ") )
            strcpy( local_buf, "SPAF " );
        else
            strcpy( local_buf, "SPIF " );

        if( nSPZone != 0 )
            snprintf( local_buf + 5, sizeof(local_buf)-5, "%4d   %4s",nSPZone,earthmodel);
        else
            snprintf( local_buf + 5, sizeof(local_buf)-5, "       %4s",earthmodel);

    }
    else if( STARTS_WITH_CI(local_buf, "ACEA ") )
    {
        snprintf( local_buf, sizeof(local_buf),
                 "ACEA        %4s",
                 earthmodel );
    }
    else if( STARTS_WITH_CI(local_buf, "AE ") )
    {
        snprintf( local_buf, sizeof(local_buf),
                 "AE          %4s",
                 earthmodel );
    }
    else if( STARTS_WITH_CI(local_buf, "EC ") )
    {
        snprintf( local_buf, sizeof(local_buf),
                 "EC          %4s",
                 earthmodel );
    }
    else if( STARTS_WITH_CI(local_buf, "ER ") )
    {
        snprintf( local_buf, sizeof(local_buf),
                 "ER          %4s",
                 earthmodel );
    }
    else if( STARTS_WITH_CI(local_buf, "GNO ") )
    {
        snprintf( local_buf, sizeof(local_buf),
                 "GNO         %4s",
                 earthmodel );
    }
    else if( STARTS_WITH_CI(local_buf, "GVNP") )
    {
        snprintf( local_buf, sizeof(local_buf),
                 "GVNP        %4s",
                 earthmodel );
    }
    else if( STARTS_WITH_CI(local_buf, "LAEA_ELL") )
    {
        snprintf( local_buf, sizeof(local_buf),
                 "LAEA_ELL    %4s",
                 earthmodel );
    }
    else if( STARTS_WITH_CI(local_buf, "LAEA") )
    {
        snprintf( local_buf, sizeof(local_buf),
                 "LAEA        %4s",
                 earthmodel );
    }
    else if( STARTS_WITH_CI(local_buf, "LCC_1SP") )
    {
        snprintf( local_buf, sizeof(local_buf),
                 "LCC_1SP     %4s",
                 earthmodel );
    }
    else if( STARTS_WITH_CI(local_buf, "LCC ") )
    {
        snprintf( local_buf, sizeof(local_buf),
                 "LCC         %4s",
                 earthmodel );
    }
    else if( STARTS_WITH_CI(local_buf, "MC ") )
    {
        snprintf( local_buf, sizeof(local_buf),
                 "MC          %4s",
                 earthmodel );
    }
    else if( STARTS_WITH_CI(local_buf, "MER ") )
    {
        snprintf( local_buf, sizeof(local_buf),
                 "MER         %4s",
                 earthmodel );
    }
    else if( STARTS_WITH_CI(local_buf, "MSC ") )
    {
        snprintf( local_buf, sizeof(local_buf),
                 "MSC         %4s",
                 earthmodel );
    }
    else if( STARTS_WITH_CI(local_buf, "OG ") )
    {
        snprintf( local_buf, sizeof(local_buf),
                 "OG          %4s",
                 earthmodel );
    }
    else if( STARTS_WITH_CI(local_buf, "OM ") )
    {
        snprintf( local_buf, sizeof(local_buf),
                 "OM          %4s",
                 earthmodel );
    }
    else if( STARTS_WITH_CI(local_buf, "PC ") )
    {
        snprintf( local_buf, sizeof(local_buf),
                 "PC          %4s",
                 earthmodel );
    }
    else if( STARTS_WITH_CI(local_buf, "PS ") )
    {
        snprintf( local_buf, sizeof(local_buf),
                 "PS          %4s",
                 earthmodel );
    }
    else if( STARTS_WITH_CI(local_buf, "ROB ") )
    {
        snprintf( local_buf, sizeof(local_buf),
                 "ROB         %4s",
                 earthmodel );
    }
    else if( STARTS_WITH_CI(local_buf, "SG ") )
    {
        snprintf( local_buf, sizeof(local_buf),
                 "SG          %4s",
                 earthmodel );
    }
    else if( STARTS_WITH_CI(local_buf, "SIN ") )
    {
        snprintf( local_buf, sizeof(local_buf),
                 "SIN         %4s",
                 earthmodel );
    }
    else if( STARTS_WITH_CI(local_buf, "SOM ") )
    {
        snprintf( local_buf, sizeof(local_buf),
                 "SOM         %4s",
                 earthmodel );
    }
    else if( STARTS_WITH_CI(local_buf, "TM ") )
    {
        snprintf( local_buf, sizeof(local_buf),
                 "TM          %4s",
                 earthmodel );
    }
    else if( STARTS_WITH_CI(local_buf, "VDG ") )
    {
        snprintf( local_buf, sizeof(local_buf),
                 "VDG         %4s",
                 earthmodel );
    }
    else if( STARTS_WITH_CI(local_buf, "UPSA") )
    {
        snprintf( local_buf, sizeof(local_buf),
                 "UPSA        %4s",
                 earthmodel );
    }
    else if( STARTS_WITH_CI(local_buf, "UPS ") )
    {
        /* Attempt to find UPS zone */
        for( ptr=local_buf+3; isspace(*ptr); ptr++ ) {}
        if( *ptr == 'A' || *ptr == 'B' || *ptr == 'Y' || *ptr == 'Z' )
            ups_zone = *ptr;
        else if( *ptr == 'a' || *ptr == 'b' || *ptr == 'y' || *ptr == 'z' )
            ups_zone = toupper( *ptr );
        else
            ups_zone = ' ';

        snprintf( local_buf, sizeof(local_buf),
                 "UPS       %c %4s",
                 ups_zone, earthmodel );
    }
    else if( STARTS_WITH_CI(local_buf, "GOOD") )
    {
        snprintf( local_buf, sizeof(local_buf),
                 "GOOD        %4s",
                 earthmodel );
    }
    else if( STARTS_WITH_CI(local_buf, "NZMG") )
    {
        snprintf( local_buf, sizeof(local_buf),
                 "NZMG        %4s",
                 earthmodel );
    }
    else if( STARTS_WITH_CI(local_buf, "CASS") )
    {
        if( STARTS_WITH_CI(earthmodel, "D000") )
            snprintf( local_buf, sizeof(local_buf),  "CASS        %4s", "E010" );
        else
            snprintf( local_buf, sizeof(local_buf),  "CASS        %4s", earthmodel );
    }
    else if( STARTS_WITH_CI(local_buf, "RSO ") )
    {
        if( STARTS_WITH_CI(earthmodel, "D000") )
            snprintf( local_buf, sizeof(local_buf),  "RSO         %4s", "E010" );
        else
            snprintf( local_buf, sizeof(local_buf),  "RSO         %4s", earthmodel );
    }
    else if( STARTS_WITH_CI(local_buf, "KROV") )
    {
        if( STARTS_WITH_CI(earthmodel, "D000") )
            snprintf( local_buf, sizeof(local_buf),  "KROV        %4s", "E002" );
        else
            snprintf( local_buf, sizeof(local_buf),  "KROV        %4s", earthmodel );
    }
    else if( STARTS_WITH_CI(local_buf, "KRON") )
    {
        if( STARTS_WITH_CI(earthmodel, "D000") )
            snprintf( local_buf, sizeof(local_buf),  "KRON        %4s", "E002" );
        else
            snprintf( local_buf, sizeof(local_buf),  "KRON        %4s", earthmodel );
    }
    else if( STARTS_WITH_CI(local_buf, "SGDO") )
    {
        if( STARTS_WITH_CI(earthmodel, "D000") )
            snprintf( local_buf, sizeof(local_buf),  "SGDO        %4s", "E910" );
        else
            snprintf( local_buf, sizeof(local_buf),  "SGDO        %4s", earthmodel );
    }
    else if( STARTS_WITH_CI(local_buf, "LBSG") )
    {
        if( STARTS_WITH_CI(earthmodel, "D000") )
            snprintf( local_buf, sizeof(local_buf),  "LBSG        %4s", "E202" );
        else
            snprintf( local_buf, sizeof(local_buf),  "LBSG        %4s", earthmodel );
    }
    else if( STARTS_WITH_CI(local_buf, "ISIN") )
    {
        if( STARTS_WITH_CI(earthmodel, "D000") )
            snprintf( local_buf, sizeof(local_buf),  "ISIN        %4s", "E700" );
        else
            snprintf( local_buf, sizeof(local_buf),  "ISIN        %4s", earthmodel );
    }
/* -------------------------------------------------------------------- */
/*      This may be a user projection. Just reformat the earth model    */
/*      portion.                                                        */
/* -------------------------------------------------------------------- */
    else
    {
        snprintf( local_buf, sizeof(local_buf), "%-11.11s %4s", geosysIn.c_str(), earthmodel );
    }

    return local_buf;
}

/*
C       PAK2PCI converts a Latitude or Longitude value held in decimal
C       form to or from the standard packed DMS format DDDMMMSSS.SSS.
C       The standard packed DMS format is the required format for any
C       Latitude or Longitude value in the projection parameter array
C       (TPARIN and/or TPARIO) in calling the U.S.G.S. GCTP package,
C       but is not required for the actual coordinates to be converted.
C       This routine has been converted from the PAKPCI FORTRAN routine.
C
C       When function is 1, the value returned is made up as follows:
C
C       PACK2PCI = (DDD * 1000000) + (MMM * 1000) + SSS.SSS
C
C       When function is 0, the value returned is made up as follows:
C
C       PACK2PCI = DDD + MMM/60 + SSS/3600
C
C       where:   DDD     are the degrees
C                MMM     are the minutes
C                SSS.SSS are the seconds
C
C       The sign of the input value is retained and will denote the
C       hemisphere (For longitude, (-) is West and (+) is East of
C       Greenwich;  For latitude, (-) is South and (+) is North of
C       the equator).
C
C
C       CALL SEQUENCE
C
C       double = PACK2PCI (degrees, function)
C
C       degrees  - (double) Latitude or Longitude value in decimal
C                           degrees.
C
C       function - (Int)    Function to perform
C                           1, convert decimal degrees to DDDMMMSSS.SSS
C                           0, convert DDDMMMSSS.SSS to decimal degrees
C
C
C       EXAMPLE
C
C       double              degrees, packed, unpack
C
C       degrees = -125.425              ! Same as 125d 25' 30" W
C       packed = PACK2PCI (degrees, 1)  ! PACKED will equal -125025030.000
C       unpack = PACK2PCI (degrees, 0)  ! UNPACK will equal -125.425
*/

/************************************************************************/
/*                              PAK2PCI()                               */
/************************************************************************/

static double PAK2PCI( double deg, int function )
{
        double    new_deg;
        int       sign;
        double    result;

        double    degrees;
        double    temp1, temp2, temp3;
        int       dd, mm;
        double    ss;

        sign = 1;
        degrees = deg;

        if ( degrees < 0 )
        {
           sign = -1;
           degrees = degrees * sign;
        }

/* -------------------------------------------------------------------- */
/*      Unpack the value.                                               */
/* -------------------------------------------------------------------- */
        if ( function == 0 )
        {
           new_deg = (double) ABS( degrees );

           dd =  (int)( new_deg / 1000000.0);

           new_deg = ( new_deg - (dd * 1000000) );
           mm = (int)(new_deg/(1000));

           new_deg = ( new_deg - (mm * 1000) );

           ss = new_deg;

           result = (double) sign * ( dd + mm/60.0 + ss/3600.0 );
        }
        else
        {
/* -------------------------------------------------------------------- */
/*      Procduce DDDMMMSSS.SSS from decimal degrees.                    */
/* -------------------------------------------------------------------- */
           new_deg = (double) ((int)degrees % 360);
           temp1 =  degrees - new_deg;

           temp2 = temp1 * 60;

           mm = (int)((temp2 * 60) / 60);

           temp3 = temp2 - mm;
           ss = temp3 * 60;

           result = (double) sign *
                ( (new_deg * 1000000) + (mm * 1000) + ss);
        }
        return result;
}

/************************************************************************/
/*                         PrepareGCTPFields()                          */
/*                                                                      */
/*      Fill the GCTP fields in the seg_data image based on the         */
/*      non-GCTP values.                                                */
/************************************************************************/

void CPCIDSKGeoref::PrepareGCTPFields()

{
    enum GCTP_UNIT_CODES {
        GCTP_UNIT_UNKNOWN = -1, /*    Default, NOT a valid code     */
        GCTP_UNIT_RADIAN  =  0, /* 0, NOT used at present           */
        GCTP_UNIT_US_FOOT,      /* 1, Used for GEO_SPAF             */
        GCTP_UNIT_METRE,        /* 2, Used for most map projections */
        GCTP_UNIT_SECOND,       /* 3, NOT used at present           */
        GCTP_UNIT_DEGREE,       /* 4, Used for GEO_LONG             */
        GCTP_UNIT_INTL_FOOT,    /* 5, Used for GEO_SPIF             */
        GCTP_UNIT_TABLE         /* 6, NOT used at present           */
    };

    seg_data.Get(32,16,geosys);
    std::string geosys_clean(ReformatGeosys( geosys ));

/* -------------------------------------------------------------------- */
/*      Establish the GCTP units code.                                  */
/* -------------------------------------------------------------------- */
    double IOmultiply = 1.0;
    int UnitsCode = GCTP_UNIT_METRE;

    std::string grid_units;
    seg_data.Get(64,16,grid_units);

    if( STARTS_WITH_CI(grid_units.c_str(), "MET") )
        UnitsCode = GCTP_UNIT_METRE;
    else if( STARTS_WITH_CI(grid_units.c_str(), "FOOT") )
    {
        UnitsCode = GCTP_UNIT_US_FOOT;
        IOmultiply = 1.0 / 0.3048006096012192;
    }
    else if( STARTS_WITH_CI(grid_units.c_str(), "INTL FOOT") )
    {
        UnitsCode = GCTP_UNIT_INTL_FOOT;
        IOmultiply = 1.0 / 0.3048;
    }
    else if( STARTS_WITH_CI(grid_units.c_str(), "DEGREE") )
        UnitsCode = GCTP_UNIT_DEGREE;

/* -------------------------------------------------------------------- */
/*      Extract the non-GCTP style parameters.                          */
/* -------------------------------------------------------------------- */
    double pci_params[17];
    int i;

    for( i = 0; i < 17; i++ )
        pci_params[i] = seg_data.GetDouble(80+26*i,26);

#define Dearth0                 pci_params[0]
#define Dearth1                 pci_params[1]
#define RefLong                 pci_params[2]
#define RefLat                  pci_params[3]
#define StdParallel1            pci_params[4]
#define StdParallel2            pci_params[5]
#define FalseEasting            pci_params[6]
#define FalseNorthing           pci_params[7]
#define Scale                   pci_params[8]
#define Height                  pci_params[9]
#define Long1                   pci_params[10]
#define Lat1                    pci_params[11]
#define Long2                   pci_params[12]
#define Lat2                    pci_params[13]
#define Azimuth                 pci_params[14]
#define LandsatNum              pci_params[15]
#define LandsatPath             pci_params[16]

/* -------------------------------------------------------------------- */
/*      Get the zone code.                                              */
/* -------------------------------------------------------------------- */
    int ProjectionZone = 0;

    if( STARTS_WITH(geosys_clean.c_str(), "UTM ")
        || STARTS_WITH(geosys_clean.c_str(), "SPCS ")
        || STARTS_WITH(geosys_clean.c_str(), "SPAF ")
        || STARTS_WITH(geosys_clean.c_str(), "SPIF ") )
    {
        ProjectionZone = atoi(geosys_clean.c_str() + 5);
    }

/* -------------------------------------------------------------------- */
/*      Handle the ellipsoid.  We depend on applications properly       */
/*      setting proj_params[0], and proj_params[1] with the semi-major    */
/*      and semi-minor axes in all other cases.                         */
/*                                                                      */
/*      I wish we could lookup datum codes to find their GCTP           */
/*      ellipsoid values here!                                          */
/* -------------------------------------------------------------------- */
    int Spheroid = -1;
    if( geosys_clean[12] == 'E' )
        Spheroid = atoi(geosys_clean.c_str() + 13);

    if( Spheroid < 0 || Spheroid > 19 )
        Spheroid = -1;

/* -------------------------------------------------------------------- */
/*      Initialize the USGS Parameters.                                 */
/* -------------------------------------------------------------------- */
    double USGSParms[15];
    int gsys;

    for ( i = 0; i < 15; i++ )
        USGSParms[i] = 0;

/* -------------------------------------------------------------------- */
/*      Projection 0: Geographic (no projection)                        */
/* -------------------------------------------------------------------- */
    if( STARTS_WITH(geosys_clean.c_str(), "LON")
        || STARTS_WITH(geosys_clean.c_str(), "LAT") )
    {
        gsys = 0;
        UnitsCode = GCTP_UNIT_DEGREE;
    }

/* -------------------------------------------------------------------- */
/*      Projection 1: Universal Transverse Mercator                     */
/* -------------------------------------------------------------------- */
    else if( STARTS_WITH(geosys_clean.c_str(), "UTM ") )
    {
        char row_char = geosys_clean[10];

        // Southern hemisphere?
        if( (row_char >= 'C') && (row_char <= 'M') && ProjectionZone > 0 )
        {
            ProjectionZone *= -1;
        }

/* -------------------------------------------------------------------- */
/*      Process UTM as TM.  The reason for this is the GCTP software    */
/*      does not provide for input of an Earth Model for UTM, but does  */
/*      for TM.                                                         */
/* -------------------------------------------------------------------- */
        gsys = 9;
        USGSParms[0] = Dearth0;
        USGSParms[1] = Dearth1;
        USGSParms[2] = 0.9996;

        USGSParms[4] = PAK2PCI(
            ( ABS(ProjectionZone) * 6.0 - 183.0 ), 1 );
        USGSParms[5] = PAK2PCI( 0.0, 1 );
        USGSParms[6] =   500000.0;
        USGSParms[7] = ( ProjectionZone < 0 ) ? 10000000.0 : 0.0;
    }

/* -------------------------------------------------------------------- */
/*      Projection 2: State Plane Coordinate System                     */
/* -------------------------------------------------------------------- */
    else if( STARTS_WITH(geosys_clean.c_str(), "SPCS ") )
    {
        gsys = 2;
        if(    UnitsCode != GCTP_UNIT_METRE
               && UnitsCode != GCTP_UNIT_US_FOOT
               && UnitsCode != GCTP_UNIT_INTL_FOOT )
            UnitsCode = GCTP_UNIT_METRE;
    }

    else if( STARTS_WITH(geosys_clean.c_str(), "SPAF ") )
    {
        gsys = 2;
        if(    UnitsCode != GCTP_UNIT_METRE
               && UnitsCode != GCTP_UNIT_US_FOOT
               && UnitsCode != GCTP_UNIT_INTL_FOOT )
            UnitsCode = GCTP_UNIT_US_FOOT;
    }

    else if( STARTS_WITH(geosys_clean.c_str(), "SPIF ") )
    {
        gsys = 2;
        if(    UnitsCode != GCTP_UNIT_METRE
               && UnitsCode != GCTP_UNIT_US_FOOT
               && UnitsCode != GCTP_UNIT_INTL_FOOT )
            UnitsCode = GCTP_UNIT_INTL_FOOT;
    }

/* -------------------------------------------------------------------- */
/*      Projection 3: Albers Conical Equal-Area                         */
/* -------------------------------------------------------------------- */
    else if( STARTS_WITH(geosys_clean.c_str(), "ACEA ") )
    {
        gsys = 3;
        USGSParms[0] = Dearth0;
        USGSParms[1] = Dearth1;
        USGSParms[2] = PAK2PCI(StdParallel1, 1);
        USGSParms[3] = PAK2PCI(StdParallel2, 1);
        USGSParms[4] = PAK2PCI(RefLong, 1);
        USGSParms[5] = PAK2PCI(RefLat, 1);
        USGSParms[6] = FalseEasting * IOmultiply;
        USGSParms[7] = FalseNorthing * IOmultiply;
    }

/* -------------------------------------------------------------------- */
/*      Projection 4: Lambert Conformal Conic                           */
/* -------------------------------------------------------------------- */
    else if( STARTS_WITH(geosys_clean.c_str(), "LCC  ") )
    {
        gsys = 4;
        USGSParms[0] = Dearth0;
        USGSParms[1] = Dearth1;
        USGSParms[2] = PAK2PCI(StdParallel1, 1);
        USGSParms[3] = PAK2PCI(StdParallel2, 1);
        USGSParms[4] = PAK2PCI(RefLong, 1);
        USGSParms[5] = PAK2PCI(RefLat, 1);
        USGSParms[6] = FalseEasting * IOmultiply;
        USGSParms[7] = FalseNorthing * IOmultiply;
    }

/* -------------------------------------------------------------------- */
/*      Projection 5: Mercator                                          */
/* -------------------------------------------------------------------- */
    else if( STARTS_WITH(geosys_clean.c_str(), "MER  ") )
    {
        gsys = 5;
        USGSParms[0] = Dearth0;
        USGSParms[1] = Dearth1;

        USGSParms[4] = PAK2PCI(RefLong, 1);
        USGSParms[5] = PAK2PCI(RefLat, 1);
        USGSParms[6] = FalseEasting * IOmultiply;
        USGSParms[7] = FalseNorthing * IOmultiply;
    }

/* -------------------------------------------------------------------- */
/*      Projection 6: Polar Stereographic                               */
/* -------------------------------------------------------------------- */
    else if( STARTS_WITH(geosys_clean.c_str(), "PS   ") )
    {
        gsys = 6;
        USGSParms[0] = Dearth0;
        USGSParms[1] = Dearth1;

        USGSParms[4] = PAK2PCI(RefLong, 1);
        USGSParms[5] = PAK2PCI(RefLat, 1);
        USGSParms[6] = FalseEasting * IOmultiply;
        USGSParms[7] = FalseNorthing * IOmultiply;
    }

/* -------------------------------------------------------------------- */
/*      Projection 7: Polyconic                                         */
/* -------------------------------------------------------------------- */
    else if( STARTS_WITH(geosys_clean.c_str(), "PC   ") )
    {
        gsys = 7;
        USGSParms[0] = Dearth0;
        USGSParms[1] = Dearth1;

        USGSParms[4] = PAK2PCI(RefLong, 1);
        USGSParms[5] = PAK2PCI(RefLat, 1);
        USGSParms[6] = FalseEasting * IOmultiply;
        USGSParms[7] = FalseNorthing * IOmultiply;
    }

/* -------------------------------------------------------------------- */
/*      Projection 8: Equidistant Conic                                 */
/*      Format A, one standard parallel,  usgs_params[8] = 0            */
/*      Format B, two standard parallels, usgs_params[8] = not 0        */
/* -------------------------------------------------------------------- */
    else if( STARTS_WITH(geosys_clean.c_str(), "EC   ") )
    {
        gsys = 8;
        USGSParms[0] = Dearth0;
        USGSParms[1] = Dearth1;
        USGSParms[2] = PAK2PCI(StdParallel1, 1);
        USGSParms[3] = PAK2PCI(StdParallel2, 1);
        USGSParms[4] = PAK2PCI(RefLong, 1);
        USGSParms[5] = PAK2PCI(RefLat, 1);
        USGSParms[6] = FalseEasting * IOmultiply;
        USGSParms[7] = FalseNorthing * IOmultiply;

        if ( StdParallel2 != 0 )
        {
            USGSParms[8] = 1;
        }
    }

/* -------------------------------------------------------------------- */
/*      Projection 9: Transverse Mercator                               */
/* -------------------------------------------------------------------- */
    else if( STARTS_WITH(geosys_clean.c_str(), "TM   ") )
    {
        gsys = 9;
        USGSParms[0] = Dearth0;
        USGSParms[1] = Dearth1;
        USGSParms[2] = Scale;

        USGSParms[4] = PAK2PCI(RefLong, 1);
        USGSParms[5] = PAK2PCI(RefLat, 1);
        USGSParms[6] = FalseEasting * IOmultiply;
        USGSParms[7] = FalseNorthing * IOmultiply;
    }

/* -------------------------------------------------------------------- */
/*      Projection 10: Stereographic                                    */
/* -------------------------------------------------------------------- */
    else if( STARTS_WITH(geosys_clean.c_str(), "SG   ") )
    {
        gsys = 10;
        USGSParms[0] = Dearth0;

        USGSParms[4] = PAK2PCI(RefLong, 1);
        USGSParms[5] = PAK2PCI(RefLat, 1);
        USGSParms[6] = FalseEasting * IOmultiply;
        USGSParms[7] = FalseNorthing * IOmultiply;
    }

/* -------------------------------------------------------------------- */
/*      Projection 11: Lambert Azimuthal Equal-Area                     */
/* -------------------------------------------------------------------- */
    else if( STARTS_WITH(geosys_clean.c_str(), "LAEA ") )
    {
        gsys = 11;

        USGSParms[0] = Dearth0;

        USGSParms[4] = PAK2PCI(RefLong, 1);
        USGSParms[5] = PAK2PCI(RefLat, 1);
        USGSParms[6] = FalseEasting * IOmultiply;
        USGSParms[7] = FalseNorthing * IOmultiply;
    }

/* -------------------------------------------------------------------- */
/*      Projection 12: Azimuthal Equidistant                            */
/* -------------------------------------------------------------------- */
    else if( STARTS_WITH(geosys_clean.c_str(), "AE   ") )
    {
        gsys = 12;
        USGSParms[0] = Dearth0;

        USGSParms[4] = PAK2PCI(RefLong, 1);
        USGSParms[5] = PAK2PCI(RefLat, 1);
        USGSParms[6] = FalseEasting * IOmultiply;
        USGSParms[7] = FalseNorthing * IOmultiply;
    }

/* -------------------------------------------------------------------- */
/*      Projection 13: Gnomonic                                         */
/* -------------------------------------------------------------------- */
    else if( STARTS_WITH(geosys_clean.c_str(), "GNO  ") )
    {
        gsys = 13;
        USGSParms[0] = Dearth0;

        USGSParms[4] = PAK2PCI(RefLong, 1);
        USGSParms[5] = PAK2PCI(RefLat, 1);
        USGSParms[6] = FalseEasting * IOmultiply;
        USGSParms[7] = FalseNorthing * IOmultiply;
    }

/* -------------------------------------------------------------------- */
/*      Projection 14: Orthographic                                     */
/* -------------------------------------------------------------------- */
    else if( STARTS_WITH(geosys_clean.c_str(), "OG   ") )
    {
        gsys = 14;
        USGSParms[0] = Dearth0;

        USGSParms[4] = PAK2PCI(RefLong, 1);
        USGSParms[5] = PAK2PCI(RefLat, 1);
        USGSParms[6] = FalseEasting * IOmultiply;
        USGSParms[7] = FalseNorthing * IOmultiply;
    }

/* -------------------------------------------------------------------- */
/*      Projection  15: General Vertical Near-Side Perspective          */
/* -------------------------------------------------------------------- */
    else if( STARTS_WITH(geosys_clean.c_str(), "GVNP ") )
    {
        gsys = 15;
        USGSParms[0] = Dearth0;

        USGSParms[2] = Height;

        USGSParms[4] = PAK2PCI(RefLong, 1);
        USGSParms[5] = PAK2PCI(RefLat, 1);
        USGSParms[6] = FalseEasting * IOmultiply;
        USGSParms[7] = FalseNorthing * IOmultiply;
    }

/* -------------------------------------------------------------------- */
/*      Projection 16: Sinusoidal                                       */
/* -------------------------------------------------------------------- */
    else if( STARTS_WITH(geosys_clean.c_str(), "SIN  ") )
    {
        gsys = 16;
        USGSParms[0] = Dearth0;
        USGSParms[4] = PAK2PCI(RefLong, 1);
        USGSParms[6] = FalseEasting * IOmultiply;
        USGSParms[7] = FalseNorthing * IOmultiply;
    }

/* -------------------------------------------------------------------- */
/*      Projection 17: Equirectangular                                  */
/* -------------------------------------------------------------------- */
    else if( STARTS_WITH(geosys_clean.c_str(), "ER   ") )
    {
        gsys = 17;
        USGSParms[0] = Dearth0;
        USGSParms[4] = PAK2PCI(RefLong, 1);
        USGSParms[5] = PAK2PCI(RefLat, 1);
        USGSParms[6] = FalseEasting * IOmultiply;
        USGSParms[7] = FalseNorthing * IOmultiply;
    }
/* -------------------------------------------------------------------- */
/*      Projection 18: Miller Cylindrical                               */
/* -------------------------------------------------------------------- */
    else if( STARTS_WITH(geosys_clean.c_str(), "MC   ") )
    {
        gsys = 18;
        USGSParms[0] = Dearth0;

        USGSParms[4] = PAK2PCI(RefLong, 1);

        USGSParms[6] = FalseEasting * IOmultiply;
        USGSParms[7] = FalseNorthing * IOmultiply;
    }

/* -------------------------------------------------------------------- */
/*      Projection 19: Van der Grinten                                  */
/* -------------------------------------------------------------------- */
    else if( STARTS_WITH(geosys_clean.c_str(), "VDG  ") )
    {
        gsys = 19;
        USGSParms[0] = Dearth0;

        USGSParms[4] = PAK2PCI(RefLong, 1);

        USGSParms[6] = FalseEasting * IOmultiply;
        USGSParms[7] = FalseNorthing * IOmultiply;
    }

/* -------------------------------------------------------------------- */
/*      Projection 20:  Oblique Mercator (Hotine)                       */
/*        Format A, Azimuth and RefLong defined (Long1, Lat1,           */
/*           Long2, Lat2 not defined), usgs_params[12] = 0              */
/*        Format B, Long1, Lat1, Long2, Lat2 defined (Azimuth           */
/*           and RefLong not defined), usgs_params[12] = not 0          */
/* -------------------------------------------------------------------- */
    else if( STARTS_WITH(geosys_clean.c_str(), "OM   ") )
    {
        gsys = 20;
        USGSParms[0] = Dearth0;
        USGSParms[1] = Dearth1;
        USGSParms[2] = Scale;
        USGSParms[3] = PAK2PCI(Azimuth ,1);

        USGSParms[4] = PAK2PCI(RefLong, 1);
        USGSParms[5] = PAK2PCI(RefLat, 1);
        USGSParms[6] = FalseEasting * IOmultiply;
        USGSParms[7] = FalseNorthing * IOmultiply;

        USGSParms[8] = PAK2PCI(Long1, 1);
        USGSParms[9] = PAK2PCI(Lat1, 1);
        USGSParms[10] = PAK2PCI(Long2, 1);
        USGSParms[11] = PAK2PCI(Lat2, 1);
        if ( (Long1 != 0) || (Lat1 != 0) ||
             (Long2 != 0) || (Lat2 != 0)    )
            USGSParms[12] = 0.0;
        else
            USGSParms[12] = 1.0;
    }
/* -------------------------------------------------------------------- */
/*      Projection 21: Robinson                                         */
/* -------------------------------------------------------------------- */
    else if( STARTS_WITH(geosys_clean.c_str(), "ROB  ") )
    {
          gsys = 21;
          USGSParms[0] = Dearth0;

          USGSParms[4] = PAK2PCI(RefLong, 1);
          USGSParms[6] = FalseEasting
              * IOmultiply;
          USGSParms[7] = FalseNorthing
              * IOmultiply;

      }
/* -------------------------------------------------------------------- */
/*      Projection 22: Space Oblique Mercator                           */
/* -------------------------------------------------------------------- */
    else if( STARTS_WITH(geosys_clean.c_str(), "SOM  ") )
    {
          gsys = 22;
          USGSParms[0] = Dearth0;
          USGSParms[1] = Dearth1;
          USGSParms[2] = LandsatNum;
          USGSParms[3] = LandsatPath;
          USGSParms[6] = FalseEasting
              * IOmultiply;
          USGSParms[7] = FalseNorthing
              * IOmultiply;
    }
/* -------------------------------------------------------------------- */
/*      Projection 23: Modified Stereographic Conformal (Alaska)        */
/* -------------------------------------------------------------------- */
    else if( STARTS_WITH(geosys_clean.c_str(), "MSC  ") )
    {
          gsys = 23;
          USGSParms[0] = Dearth0;
          USGSParms[1] = Dearth1;
          USGSParms[6] = FalseEasting
              * IOmultiply;
          USGSParms[7] = FalseNorthing
              * IOmultiply;
    }

/* -------------------------------------------------------------------- */
/*      Projection 6: Universal Polar Stereographic is just Polar       */
/*      Stereographic with certain assumptions.                         */
/* -------------------------------------------------------------------- */
    else if( STARTS_WITH(geosys_clean.c_str(), "UPS  ") )
    {
          gsys = 6;

          USGSParms[0] = Dearth0;
          USGSParms[1] = Dearth1;

          USGSParms[4] = PAK2PCI(0.0, 1);

          USGSParms[6] = 2000000.0;
          USGSParms[7] = 2000000.0;

          double dwork = 81.0 + 6.0/60.0 + 52.3/3600.0;

          if( geosys_clean[10] == 'A' || geosys_clean[10] == 'B' )
          {
              USGSParms[5] = PAK2PCI(-dwork,1);
          }
          else if( geosys_clean[10] == 'Y' || geosys_clean[10]=='Z')
          {
              USGSParms[5] = PAK2PCI(dwork,1);
          }
          else
          {
              USGSParms[4] = PAK2PCI(RefLong, 1);
              USGSParms[5] = PAK2PCI(RefLat, 1);
              USGSParms[6] = FalseEasting
                  * IOmultiply;
              USGSParms[7] = FalseNorthing
                  * IOmultiply;
          }
      }

/* -------------------------------------------------------------------- */
/*      Unknown code.                                                   */
/* -------------------------------------------------------------------- */
    else
    {
        gsys = -1;
    }

    if( ProjectionZone == 0 )
        ProjectionZone = 10000 + gsys;

/* -------------------------------------------------------------------- */
/*      Place USGS values in the formatted segment.                     */
/* -------------------------------------------------------------------- */
    seg_data.Put( (double) gsys,           1458   , 26, "%26.18lE" );
    seg_data.Put( (double) ProjectionZone, 1458+26, 26, "%26.18lE" );

    for( i = 0; i < 15; i++ )
        seg_data.Put( USGSParms[i], 1458+26*(2+i), 26, "%26.18lE" );

    seg_data.Put( (double) UnitsCode, 1458+26*17, 26, "%26.18lE" );
    seg_data.Put( (double) Spheroid,  1458+26*18, 26, "%26.18lE" );
}


