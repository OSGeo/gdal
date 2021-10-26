/******************************************************************************
 * $Id$
 *
 * Project:  GXF Reader
 * Purpose:  Handle GXF to PROJ.4 projection transformation.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1998, Global Geomatics
 * Copyright (c) 1998, Frank Warmerdam
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

#include "gxfopen.h"

CPL_CVSID("$Id$")


#define SAFE_strcat(x,y) snprintf(x + strlen(x),sizeof(x) - strlen(x), "%s", y)

/************************************************************************/
/*                     GXFGetMapProjectionAsPROJ4()                     */
/************************************************************************/

/**
 * Return the GXF Projection in PROJ.4 format.
 *
 * The returned string becomes owned by the caller, and should be freed
 * with CPLFree() or VSIFree().  The return value will be "unknown" if
 * no projection information is passed.
 *
 * The mapping of GXF projections to PROJ.4 format is not complete.  Please
 * see the gxf_proj4.c code to better understand limitations of this
 * translation.  In particular, PROJ.4 knows little about datums.
 *
 * For example, the following GXF definitions:
 * <pre>
 * #UNIT_LENGTH
 * m,1
 * #MAP_PROJECTION
 * "NAD83 / UTM zone 19N"
 * "GRS 1980",6378137,0.081819191,0
 * "Transverse Mercator",0,-69,0.9996,500000,0
 * </pre>
 *
 * Would translate to:
 * <pre>
 * +proj=tmerc +lat_0=0 +lon_0=-69 +k=0.9996 +x_0=500000 +y_0=0 +ellps=GRS80
 * </pre>
 *
 * @param hGXF handle to GXF file, as returned by GXFOpen().
 *
 * @return string containing PROJ.4 projection.
 */

char *GXFGetMapProjectionAsPROJ4( GXFHandle hGXF )

{
    GXFInfo_t	*psGXF = (GXFInfo_t *) hGXF;
    char	**papszMethods = NULL;
    char	szPROJ4[512] = { 0 };

/* -------------------------------------------------------------------- */
/*      If there was nothing in the file return "unknown".              */
/* -------------------------------------------------------------------- */
    if( CSLCount(psGXF->papszMapProjection) < 2 )
        return( CPLStrdup( "unknown" ) );

/* -------------------------------------------------------------------- */
/*      Parse the third line, looking for known projection methods.     */
/* -------------------------------------------------------------------- */
    if( psGXF->papszMapProjection[2] != NULL )
    {
        if( strlen(psGXF->papszMapProjection[2]) > 80 )
            return( CPLStrdup( "" ) );
        papszMethods = CSLTokenizeStringComplex(psGXF->papszMapProjection[2],
                                                ",", TRUE, TRUE );
    }

    if( papszMethods == NULL
        || papszMethods[0] == NULL
        || EQUAL(papszMethods[0],"Geographic") )
    {
        SAFE_strcat( szPROJ4, "+proj=longlat" );
    }

#ifdef notdef
    else if( EQUAL(papszMethods[0],"Lambert Conic Conformal (1SP)")
             && CSLCount(papszMethods) > 5 )
    {
        /* notdef: It isn't clear that this 1SP + scale method is even
           supported by PROJ.4
           Later note: It is not. */

        SAFE_strcat( szPROJ4, "+proj=lcc" );

        SAFE_strcat( szPROJ4, " +lat_0=" );
        SAFE_strcat( szPROJ4, papszMethods[1] );

        SAFE_strcat( szPROJ4, " +lon_0=" );
        SAFE_strcat( szPROJ4, papszMethods[2] );

        SAFE_strcat( szPROJ4, " +k=" );
        SAFE_strcat( szPROJ4, papszMethods[3] );

        SAFE_strcat( szPROJ4, " +x_0=" );
        SAFE_strcat( szPROJ4, papszMethods[4] );

        SAFE_strcat( szPROJ4, " +y_0=" );
        SAFE_strcat( szPROJ4, papszMethods[5] );
    }
#endif
    else if( EQUAL(papszMethods[0],"Lambert Conic Conformal (2SP)")
             || EQUAL(papszMethods[0],"Lambert Conformal (2SP Belgium)") )
    {
        /* notdef: Note we are apparently losing whatever makes the
           Belgium variant different than normal LCC, but hopefully
           they are close! */

        SAFE_strcat( szPROJ4, "+proj=lcc" );

        if( CSLCount(papszMethods) > 1 )
        {
            SAFE_strcat( szPROJ4, " +lat_1=" );
            SAFE_strcat( szPROJ4, papszMethods[1] );
        }

        if( CSLCount(papszMethods) > 2 )
        {
            SAFE_strcat( szPROJ4, " +lat_2=" );
            SAFE_strcat( szPROJ4, papszMethods[2] );
        }

        if( CSLCount(papszMethods) > 3 )
        {
            SAFE_strcat( szPROJ4, " +lat_0=" );
            SAFE_strcat( szPROJ4, papszMethods[3] );
        }

        if( CSLCount(papszMethods) > 4 )
        {
            SAFE_strcat( szPROJ4, " +lon_0=" );
            SAFE_strcat( szPROJ4, papszMethods[4] );
        }

        if( CSLCount(papszMethods) > 5 )
        {
            SAFE_strcat( szPROJ4, " +x_0=" );
            SAFE_strcat( szPROJ4, papszMethods[5] );
        }

        if( CSLCount(papszMethods) > 6 )
        {
            SAFE_strcat( szPROJ4, " +y_0=" );
            SAFE_strcat( szPROJ4, papszMethods[6] );
        }
    }

    else if( EQUAL(papszMethods[0],"Mercator (1SP)")
             && CSLCount(papszMethods) > 5 )
    {
        /* notdef: it isn't clear that +proj=merc support a scale of other
           than 1.0 in PROJ.4 */

        SAFE_strcat( szPROJ4, "+proj=merc" );

        SAFE_strcat( szPROJ4, " +lat_ts=" );
        SAFE_strcat( szPROJ4, papszMethods[1] );

        SAFE_strcat( szPROJ4, " +lon_0=" );
        SAFE_strcat( szPROJ4, papszMethods[2] );

        SAFE_strcat( szPROJ4, " +k=" );
        SAFE_strcat( szPROJ4, papszMethods[3] );

        SAFE_strcat( szPROJ4, " +x_0=" );
        SAFE_strcat( szPROJ4, papszMethods[4] );

        SAFE_strcat( szPROJ4, " +y_0=" );
        SAFE_strcat( szPROJ4, papszMethods[5] );
    }

    else if( EQUAL(papszMethods[0],"Mercator (2SP)")
             && CSLCount(papszMethods) > 4 )
    {
        /* notdef: it isn't clear that +proj=merc support a scale of other
           than 1.0 in PROJ.4 */

        SAFE_strcat( szPROJ4, "+proj=merc" );

        SAFE_strcat( szPROJ4, " +lat_ts=" );
        SAFE_strcat( szPROJ4, papszMethods[1] );

        SAFE_strcat( szPROJ4, " +lon_0=" );
        SAFE_strcat( szPROJ4, papszMethods[2] );

        SAFE_strcat( szPROJ4, " +x_0=" );
        SAFE_strcat( szPROJ4, papszMethods[3] );

        SAFE_strcat( szPROJ4, " +y_0=" );
        SAFE_strcat( szPROJ4, papszMethods[4] );
    }

    else if( EQUAL(papszMethods[0],"Hotine Oblique Mercator")
             && CSLCount(papszMethods) > 7 )
    {
        /* Note that only the second means of specifying omerc is supported
           by this code in GXF. */
        SAFE_strcat( szPROJ4, "+proj=omerc" );

        SAFE_strcat( szPROJ4, " +lat_0=" );
        SAFE_strcat( szPROJ4, papszMethods[1] );

        SAFE_strcat( szPROJ4, " +lonc=" );
        SAFE_strcat( szPROJ4, papszMethods[2] );

        SAFE_strcat( szPROJ4, " +alpha=" );
        SAFE_strcat( szPROJ4, papszMethods[3] );

        if( CPLAtof(papszMethods[4]) < 0.00001 )
        {
            SAFE_strcat( szPROJ4, " +not_rot" );
        }
        else
        {
#ifdef notdef
            if( CPLAtof(papszMethods[4]) + CPLAtof(papszMethods[3]) < 0.00001 )
                /* ok */;
            else
                /* notdef: no way to specify arbitrary angles! */;
#endif
        }

        SAFE_strcat( szPROJ4, " +k=" );
        SAFE_strcat( szPROJ4, papszMethods[5] );

        SAFE_strcat( szPROJ4, " +x_0=" );
        SAFE_strcat( szPROJ4, papszMethods[6] );

        SAFE_strcat( szPROJ4, " +y_0=" );
        SAFE_strcat( szPROJ4, papszMethods[7] );
    }

    else if( EQUAL(papszMethods[0],"Laborde Oblique Mercator")
             && CSLCount(papszMethods) > 6 )
    {
        SAFE_strcat( szPROJ4, "+proj=labrd" );

        SAFE_strcat( szPROJ4, " +lat_0=" );
        SAFE_strcat( szPROJ4, papszMethods[1] );

        SAFE_strcat( szPROJ4, " +lon_0=" );
        SAFE_strcat( szPROJ4, papszMethods[2] );

        SAFE_strcat( szPROJ4, " +azi=" );
        SAFE_strcat( szPROJ4, papszMethods[3] );

        SAFE_strcat( szPROJ4, " +k=" );
        SAFE_strcat( szPROJ4, papszMethods[4] );

        SAFE_strcat( szPROJ4, " +x_0=" );
        SAFE_strcat( szPROJ4, papszMethods[5] );

        SAFE_strcat( szPROJ4, " +y_0=" );
        SAFE_strcat( szPROJ4, papszMethods[6] );
    }

    else if( EQUAL(papszMethods[0],"New Zealand Map Grid")
             && CSLCount(papszMethods) > 4 )
    {
        SAFE_strcat( szPROJ4, "+proj=nzmg" );

        SAFE_strcat( szPROJ4, " +lat_0=" );
        SAFE_strcat( szPROJ4, papszMethods[1] );

        SAFE_strcat( szPROJ4, " +lon_0=" );
        SAFE_strcat( szPROJ4, papszMethods[2] );

        SAFE_strcat( szPROJ4, " +x_0=" );
        SAFE_strcat( szPROJ4, papszMethods[3] );

        SAFE_strcat( szPROJ4, " +y_0=" );
        SAFE_strcat( szPROJ4, papszMethods[4] );
    }

    else if( EQUAL(papszMethods[0],"New Zealand Map Grid")
             && CSLCount(papszMethods) > 4 )
    {
        SAFE_strcat( szPROJ4, "+proj=nzmg" );

        SAFE_strcat( szPROJ4, " +lat_0=" );
        SAFE_strcat( szPROJ4, papszMethods[1] );

        SAFE_strcat( szPROJ4, " +lon_0=" );
        SAFE_strcat( szPROJ4, papszMethods[2] );

        SAFE_strcat( szPROJ4, " +x_0=" );
        SAFE_strcat( szPROJ4, papszMethods[3] );

        SAFE_strcat( szPROJ4, " +y_0=" );
        SAFE_strcat( szPROJ4, papszMethods[4] );
    }

    else if( EQUAL(papszMethods[0],"Oblique Stereographic")
             && CSLCount(papszMethods) > 5 )
    {
        /* there is an option to produce +lat_ts, which we ignore */

        SAFE_strcat( szPROJ4, "+proj=stere" );

        SAFE_strcat( szPROJ4, " +lat_0=45" );

        SAFE_strcat( szPROJ4, " +lat_ts=" );
        SAFE_strcat( szPROJ4, papszMethods[1] );

        SAFE_strcat( szPROJ4, " +lon_0=" );
        SAFE_strcat( szPROJ4, papszMethods[2] );

        SAFE_strcat( szPROJ4, " +k=" );
        SAFE_strcat( szPROJ4, papszMethods[3] );

        SAFE_strcat( szPROJ4, " +x_0=" );
        SAFE_strcat( szPROJ4, papszMethods[4] );

        SAFE_strcat( szPROJ4, " +y_0=" );
        SAFE_strcat( szPROJ4, papszMethods[5] );
    }

    else if( EQUAL(papszMethods[0],"Polar Stereographic")
             && CSLCount(papszMethods) > 5 )
    {
        /* there is an option to produce +lat_ts, which we ignore */

        SAFE_strcat( szPROJ4, "+proj=stere" );

        SAFE_strcat( szPROJ4, " +lat_0=90" );

        SAFE_strcat( szPROJ4, " +lat_ts=" );
        SAFE_strcat( szPROJ4, papszMethods[1] );

        SAFE_strcat( szPROJ4, " +lon_0=" );
        SAFE_strcat( szPROJ4, papszMethods[2] );

        SAFE_strcat( szPROJ4, " +k=" );
        SAFE_strcat( szPROJ4, papszMethods[3] );

        SAFE_strcat( szPROJ4, " +x_0=" );
        SAFE_strcat( szPROJ4, papszMethods[4] );

        SAFE_strcat( szPROJ4, " +y_0=" );
        SAFE_strcat( szPROJ4, papszMethods[5] );
    }

    else if( EQUAL(papszMethods[0],"Swiss Oblique Cylindrical")
             && CSLCount(papszMethods) > 4 )
    {
        /* notdef: geotiff's geo_ctrans.inc says this is the same as
           ObliqueMercator_Rosenmund, which GG's geotiff support just
           maps directly to +proj=omerc, though I find that questionable. */

        SAFE_strcat( szPROJ4, "+proj=omerc" );

        SAFE_strcat( szPROJ4, " +lat_0=" );
        SAFE_strcat( szPROJ4, papszMethods[1] );

        SAFE_strcat( szPROJ4, " +lonc=" );
        SAFE_strcat( szPROJ4, papszMethods[2] );

        SAFE_strcat( szPROJ4, " +x_0=" );
        SAFE_strcat( szPROJ4, papszMethods[3] );

        SAFE_strcat( szPROJ4, " +y_0=" );
        SAFE_strcat( szPROJ4, papszMethods[4] );
    }

    else if( EQUAL(papszMethods[0],"Transverse Mercator")
             && CSLCount(papszMethods) > 5 )
    {
        /* notdef: geotiff's geo_ctrans.inc says this is the same as
           ObliqueMercator_Rosenmund, which GG's geotiff support just
           maps directly to +proj=omerc, though I find that questionable. */

        SAFE_strcat( szPROJ4, "+proj=tmerc" );

        SAFE_strcat( szPROJ4, " +lat_0=" );
        SAFE_strcat( szPROJ4, papszMethods[1] );

        SAFE_strcat( szPROJ4, " +lon_0=" );
        SAFE_strcat( szPROJ4, papszMethods[2] );

        SAFE_strcat( szPROJ4, " +k=" );
        SAFE_strcat( szPROJ4, papszMethods[3] );

        SAFE_strcat( szPROJ4, " +x_0=" );
        SAFE_strcat( szPROJ4, papszMethods[4] );

        SAFE_strcat( szPROJ4, " +y_0=" );
        SAFE_strcat( szPROJ4, papszMethods[5] );
    }

    else if( EQUAL(papszMethods[0],"Transverse Mercator (South Oriented)")
             && CSLCount(papszMethods) > 5 )
    {
        /* notdef: I don't know how south oriented is different from
           normal, and I don't find any mention of it in Geotiff;s geo_ctrans.
           Translating as tmerc, but that is presumably wrong. */

        SAFE_strcat( szPROJ4, "+proj=tmerc" );

        SAFE_strcat( szPROJ4, " +lat_0=" );
        SAFE_strcat( szPROJ4, papszMethods[1] );

        SAFE_strcat( szPROJ4, " +lon_0=" );
        SAFE_strcat( szPROJ4, papszMethods[2] );

        SAFE_strcat( szPROJ4, " +k=" );
        SAFE_strcat( szPROJ4, papszMethods[3] );

        SAFE_strcat( szPROJ4, " +x_0=" );
        SAFE_strcat( szPROJ4, papszMethods[4] );

        SAFE_strcat( szPROJ4, " +y_0=" );
        SAFE_strcat( szPROJ4, papszMethods[5] );
    }

    else if( EQUAL(papszMethods[0],"*Equidistant Conic")
             && CSLCount(papszMethods) > 6 )
    {
        SAFE_strcat( szPROJ4, "+proj=eqdc" );

        SAFE_strcat( szPROJ4, " +lat_1=" );
        SAFE_strcat( szPROJ4, papszMethods[1] );

        SAFE_strcat( szPROJ4, " +lat_2=" );
        SAFE_strcat( szPROJ4, papszMethods[2] );

        SAFE_strcat( szPROJ4, " +lat_0=" );
        SAFE_strcat( szPROJ4, papszMethods[3] );

        SAFE_strcat( szPROJ4, " +lon_0=" );
        SAFE_strcat( szPROJ4, papszMethods[4] );

        SAFE_strcat( szPROJ4, " +x_0=" );
        SAFE_strcat( szPROJ4, papszMethods[5] );

        SAFE_strcat( szPROJ4, " +y_0=" );
        SAFE_strcat( szPROJ4, papszMethods[6] );
    }

    else if( EQUAL(papszMethods[0],"*Polyconic")
             && CSLCount(papszMethods) > 5 )
    {
        SAFE_strcat( szPROJ4, "+proj=poly" );

        SAFE_strcat( szPROJ4, " +lat_0=" );
        SAFE_strcat( szPROJ4, papszMethods[1] );

        SAFE_strcat( szPROJ4, " +lon_0=" );
        SAFE_strcat( szPROJ4, papszMethods[2] );

#ifdef notdef
        /*not supported by PROJ.4 */
        SAFE_strcat( szPROJ4, " +k=" );
        SAFE_strcat( szPROJ4, papszMethods[3] );
#endif
        SAFE_strcat( szPROJ4, " +x_0=" );
        SAFE_strcat( szPROJ4, papszMethods[4] );

        SAFE_strcat( szPROJ4, " +y_0=" );
        SAFE_strcat( szPROJ4, papszMethods[5] );
    }

    else
    {
        SAFE_strcat( szPROJ4, "unknown" );
    }

    CSLDestroy( papszMethods );

/* -------------------------------------------------------------------- */
/*      Now get the ellipsoid parameters.  For a bunch of common        */
/*      ones we preserve the name.  For the rest we just carry over     */
/*      the parameters.                                                 */
/* -------------------------------------------------------------------- */
    if( CSLCount(psGXF->papszMapProjection) > 1 )
    {
        char	**papszTokens;

        if( strlen(psGXF->papszMapProjection[1]) > 80 )
            return CPLStrdup("");

        papszTokens = CSLTokenizeStringComplex(psGXF->papszMapProjection[1],
                                               ",", TRUE, TRUE );


        if( EQUAL(papszTokens[0],"WGS 84") )
            SAFE_strcat( szPROJ4, " +ellps=WGS84" );
        else if( EQUAL(papszTokens[0],"*WGS 72") )
            SAFE_strcat( szPROJ4, " +ellps=WGS72" );
        else if( EQUAL(papszTokens[0],"*WGS 66") )
            SAFE_strcat( szPROJ4, " +ellps=WGS66" );
        else if( EQUAL(papszTokens[0],"*WGS 60") )
            SAFE_strcat( szPROJ4, " +ellps=WGS60" );
        else if( EQUAL(papszTokens[0],"Clarke 1866") )
            SAFE_strcat( szPROJ4, " +ellps=clrk66" );
        else if( EQUAL(papszTokens[0],"Clarke 1880") )
            SAFE_strcat( szPROJ4, " +ellps=clrk80" );
        else if( EQUAL(papszTokens[0],"GRS 1980") )
            SAFE_strcat( szPROJ4, " +ellps=GRS80" );
        else if( CSLCount( papszTokens ) > 2 )
        {
            snprintf( szPROJ4+strlen(szPROJ4), sizeof(szPROJ4)-strlen(szPROJ4),
                     " +a=%s +e=%s",
                     papszTokens[1], papszTokens[2] );
        }

        CSLDestroy(papszTokens);
    }

/* -------------------------------------------------------------------- */
/*      Extract the units specification.                                */
/* -------------------------------------------------------------------- */
    if( psGXF->pszUnitName != NULL )
    {
        if( EQUAL(psGXF->pszUnitName,"ft") )
        {
            SAFE_strcat( szPROJ4, " +units=ft" );
        }
        else if( EQUAL(psGXF->pszUnitName,"ftUS") )
        {
            SAFE_strcat( szPROJ4, " +units=us-ft" );
        }
        else if( EQUAL(psGXF->pszUnitName,"km") )
        {
            SAFE_strcat( szPROJ4, " +units=km" );
        }
        else if( EQUAL(psGXF->pszUnitName,"mm") )
        {
            SAFE_strcat( szPROJ4, " +units=mm" );
        }
        else if( EQUAL(psGXF->pszUnitName,"in") )
        {
            SAFE_strcat( szPROJ4, " +units=in" );
        }
        else if( EQUAL(psGXF->pszUnitName,"ftInd") )
        {
            SAFE_strcat( szPROJ4, " +units=ind-ft" );
        }
        else if( EQUAL(psGXF->pszUnitName,"lk") )
        {
            SAFE_strcat( szPROJ4, " +units=link" );
        }
    }

    return( CPLStrdup( szPROJ4 ) );
}


/************************************************************************/
/*                        GXFGetPROJ4Position()                         */
/*                                                                      */
/*      Get the same information as GXFGetPosition(), but adjust        */
/*      to units to meters if we don't ``know'' the indicated           */
/*      units.                                                          */
/************************************************************************/

CPLErr GXFGetPROJ4Position( GXFHandle hGXF,
                            double * pdfXOrigin, double * pdfYOrigin,
                            double * pdfXPixelSize, double * pdfYPixelSize,
                            double * pdfRotation )

{
    GXFInfo_t	*psGXF = (GXFInfo_t *) hGXF;
    char	*pszProj;

/* -------------------------------------------------------------------- */
/*      Get the raw position.                                           */
/* -------------------------------------------------------------------- */
    if( GXFGetPosition( hGXF,
                        pdfXOrigin, pdfYOrigin,
                        pdfXPixelSize, pdfYPixelSize,
                        pdfRotation ) == CE_Failure )
        return( CE_Failure );

/* -------------------------------------------------------------------- */
/*      Do we know the units in PROJ.4?  Get the PROJ.4 string, and     */
/*      check for a +units definition.                                  */
/* -------------------------------------------------------------------- */
    pszProj = GXFGetMapProjectionAsPROJ4( hGXF );
    if( strstr(pszProj,"+unit") == NULL && psGXF->pszUnitName != NULL )
    {
        if( pdfXOrigin != NULL )
            *pdfXOrigin *= psGXF->dfUnitToMeter;
        if( pdfYOrigin != NULL )
            *pdfYOrigin *= psGXF->dfUnitToMeter;
        if( pdfXPixelSize != NULL )
            *pdfXPixelSize *= psGXF->dfUnitToMeter;
        if( pdfYPixelSize != NULL )
            *pdfYPixelSize *= psGXF->dfUnitToMeter;
    }
    CPLFree( pszProj );

    return( CE_None );
}
