/*******************************************************************************
 *  Project: libopencad
 *  Purpose: OpenSource CAD formats support library
 *  Author: Alexandr Borzykh, mush3d at gmail.com
 *  Author: Dmitry Baryshnikov, bishop.dev@gmail.com
 *  Language: C++
 *******************************************************************************
 *  The MIT License (MIT)
 *
 *  Copyright (c) 2016 Alexandr Borzykh
 *  Copyright (c) 2016 NextGIS, <info@nextgis.com>
 *
 *  Permission is hereby granted, free of charge, to any person obtaining a copy
 *  of this software and associated documentation files (the "Software"), to deal
 *  in the Software without restriction, including without limitation the rights
 *  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *  copies of the Software, and to permit persons to whom the Software is
 *  furnished to do so, subject to the following conditions:
 *
 *  The above copyright notice and this permission notice shall be included in all
 *  copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 *  SOFTWARE.
 *******************************************************************************/
#ifndef CADHEADER_H
#define CADHEADER_H

#include "opencad.h"
#include <map>
#include <string>
#include <vector>
#include <ctime>

class OCAD_EXTERN CADHandle final
{
public:
    explicit CADHandle( unsigned char codeIn = 0 );
    CADHandle( const CADHandle& other );
    CADHandle& operator=( const CADHandle& other );

    void addOffset( unsigned char val );
    bool isNull() const;
    long getAsLong() const;
    long getAsLong( const CADHandle& ref_handle ) const;
private:
    static long getAsLong(const std::vector<unsigned char>& handle);
protected:
    unsigned char              code;
    std::vector<unsigned char> handleOrOffset;
};

class OCAD_EXTERN CADVariant final
{
public:
    enum class DataType
    {
        INVALID = 0, DECIMAL, REAL, STRING, DATETIME, COORDINATES, HANDLE
    };

public:
    CADVariant();
    // cppcheck-suppress noExplicitConstructor
    CADVariant( const char * val );
    // cppcheck-suppress noExplicitConstructor
    CADVariant( int val );
    // cppcheck-suppress noExplicitConstructor
    CADVariant( short val );
    // cppcheck-suppress noExplicitConstructor
    CADVariant( double val );
    CADVariant( double x, double y, double z = 0 );
    // cppcheck-suppress noExplicitConstructor
    CADVariant( const CADHandle& val );
    // cppcheck-suppress noExplicitConstructor
    CADVariant( const std::string& val );
    // cppcheck-suppress noExplicitConstructor
    CADVariant( long julianday, long milliseconds );
public:
    long                getDecimal() const;
    double              getReal() const;
    const std::string&  getString() const;
    DataType            getType() const;
    double              getX() const;
    double              getY() const;
    double              getZ() const;
    const CADHandle&    getHandle() const;
protected:
    DataType            type;
    long                decimalVal;
    double              xVal;
    double              yVal;
    double              zVal;
    std::string         stringVal;
    CADHandle           handleVal;
    time_t              dateTimeVal;
};

/**
 * @brief The common CAD header class
 */
class OCAD_EXTERN CADHeader
{
public:
    /**
     * @brief The CAD нeader сonstants enum get from dxf reference:
     *        http://help.autodesk.com/view/ACD/2016/ENU/?guid=GUID-A85E8E67-27CD-4C59-BE61-4DC9FADBE74A
     */
    enum CADHeaderConstants
    {
        OPENCADVER          = 1, /**< enum CADVersions value*/
        ACADMAINTVER, /**< Maintenance version number (should be ignored) */
        ACADVER, /**< The AutoCAD drawing database version number:
                              AC1006 = R10
                              AC1009 = R11 and R12
                              AC1012 = R13
                              AC1014 = R14
                              AC1015 = AutoCAD 2000
                              AC1018 = AutoCAD 2004
                              AC1021 = AutoCAD 2007
                              AC1024 = AutoCAD 2010
                              AC1027 = AutoCAD 2013 */
        ANGBASE, /**< Angle 0 direction */
        ANGDIR, /**< 1 (Clockwise angles) or 0 (Counterclockwise angles) */
        ATTMODE, /**< Attribute visibility: 0,1,2 */
        ATTREQ, /**< todo: */
        ATTDIA, /**< todo: */
        AUNITS, /**< Units format for angles */
        AUPREC, /**< Units precision for angles */
        CECOLOR, /**< 0 = BYBLOCK; 256 = BYLAYER */
        CELTSCALE, /**< Current entity linetype scale */
        CELTYPE, /**< Entity linetype name, or BYBLOCK or BYLAYER */
        CELWEIGHT, /**< Lineweight of new objects */
        CEPSNID, /**< Plotstyle handle of new objects;
                              if CEPSNTYPE is 3, then this value indicates the
                              handle */
        CEPSNTYPE, /**< Plot style type of new objects:
                              0 = Plot style by layer,
                              1 = Plot style by block,
                              2 = Plot style by dictionary default,
                              3 = Plot style by object ID/handle */
        CHAMFERA, /**< First chamfer distance */
        CHAMFERB, /**< Second chamfer distance */
        CHAMFERC, /**< Chamfer length */
        CHAMFERD, /**< Chamfer angle */
        CLAYER, /**< Current layer name */
        CMLJUST, /**< Current multiline justification:
                              0 = Top;
                              1 = Middle;
                              2 = Bottom */
        CMLSCALE, /**< Current multiline scale */
        CMLSTYLE, /**< Current multiline style name */
        CSHADOW, /**< Shadow mode for a 3D object:
                              0 = Casts and receives shadows
                              1 = Casts shadows
                              2 = Receives shadows
                              3 = Ignores shadows */
        DIMADEC, /**< Number of precision places displayed in angular
                              dimensions */
        DIMALT, /**< Alternate unit dimensioning performed if nonzero */
        DIMALTD, /**< Alternate unit decimal places */
        DIMALTF, /**< Alternate unit scale factor */
        DIMALTRND, /**< Determines rounding of alternate units */
        DIMALTTD, /**< Number of decimal places for tolerance values of
                              an alternate units dimension */
        DIMALTTZ, /**< Controls suppression of zeros for alternate
                              tolerance values:
                              0 = Suppresses zero feet and precisely zero inches
                              1 = Includes zero feet and precisely zero inches
                              2 = Includes zero feet and suppresses zero inches
                              3 = Includes zero inches and suppresses zero feet */
        DIMALTU, /**< Units format for alternate units of all dimension
                              style family members except angular:
                              1 = Scientific
                              2 = Decimal
                              3 = Engineering
                              4 = Architectural (stacked)
                              5 = Fractional (stacked)
                              6 = Architectural
                              7 = Fractional */
        DIMALTZ, /**< Controls suppression of zeros for alternate unit
                              dimension, values:
                              0 = Suppresses zero feet and precisely zero inches
                              1 = Includes zero feet and precisely zero inches
                              2 = Includes zero feet and suppresses zero inches
                              3 = Includes zero inches and suppresses zero feet */
        DIMAPOST, /**< Alternate dimensioning suffix */
        DIMASO, /**< 1 = Create associative dimensioning,
                              0 = Draw individual entities */
        DIMASSOC, /**< Controls the associativity of dimension objects:
                              0 = Creates exploded dimensions; there is no
                                  association between elements of the dimension,
                                  and the lines, arcs, arrowheads, and text of a
                                  dimension are drawn as separate objects
                              1 = Creates non-associative dimension objects;
                                  the elements of the dimension are formed into
                                  a single object, and if the definition point
                                  on the object moves, then the dimension value
                                  is updated
                              2 = Creates associative dimension objects; the
                                  elements of the dimension are formed into a
                                  single object and one or more definition
                                  points of the dimension are coupled with
                                  association points on geometric objects */
        DIMASZ, /**< Dimensioning arrow size */
        DIMATFIT, /**< Controls dimension text and arrow placement when
                              space is not sufficient to place both within the
                              extension lines:
                              0 = Places both text and arrows outside extension
                                  lines
                              1 = Moves arrows first, then text
                              2 = Moves text first, then arrows
                              3 = Moves either text or arrows, whichever fits
                                  best AutoCAD adds a leader to moved dimension
                                  text when DIMTMOVE is set to 1 */
        DIMAUNIT, /**< Angle format for angular dimensions:
                              0 = Decimal degrees
                              1 = Degrees/minutes/seconds
                              2 = Gradians
                              3 = Radians
                              4 = Surveyor's units */
        DIMAZIN, /**< Controls suppression of zeros for angular
                              dimensions:
                              0 = Displays all leading and trailing zeros
                              1 = Suppresses leading zeros in decimal dimensions
                              2 = Suppresses trailing zeros in decimal dimensions
                              3 = Suppresses leading and trailing zeros */
        DIMBLK, /**< Arrow block name */
        DIMBLK1, /**< First arrow block name */
        DIMBLK2, /**< Second arrow block name */
        DIMCEN, /**< Size of center mark/lines */
        DIMCLRD, /**< Dimension line color:
                              range is 0 = BYBLOCK; 256 = BYLAYER */
        DIMCLRE, /**< Dimension extension line color:
                              range is 0 = BYBLOCK; 256 = BYLAYER */
        DIMCLRT, /**< Dimension text color:
                              range is 0 = BYBLOCK; 256 = BYLAYER */
        DIMDEC, /**< Number of decimal places for the tolerance values
                              of a primary units dimension */
        DIMDLE, /**< Dimension line extension */
        DIMDLI, /**< Dimension line increment */
        DIMDSEP, /**< Single-character decimal separator used when
                              creating dimensions whose unit format is decimal */
        DIMEXE, /**< Extension line extension */
        DIMEXO, /**< Extension line offset */
        DIMFAC, /**< Scale factor used to calculate the height of text
                              for dimension fractions and tolerances. AutoCAD
                              multiplies DIMTXT by DIMTFAC to set the fractional
                              or tolerance text height */
        DIMGAP, /**< Dimension line gap */
        DIMJUST, /**< Horizontal dimension text position:
                              0 = Above dimension line and center-justified
                                  between extension lines
                              1 = Above dimension line and next to first
                                  extension line
                              2 = Above dimension line and next to second
                                  extension line
                              3 = Above and center-justified to first extension
                                  line
                              4 = Above and center-justified to second
                                  extension line */
        DIMLDRBLK, /**< Arrow block name for leaders */
        DIMLFAC, /**< Linear measurements scale factor */
        DIMLIM, /**< Dimension limits generated if nonzero */
        DIMLUNIT, /**< Sets units for all dimension types except Angular:
                              1 = Scientific
                              2 = Decimal
                              3 = Engineering
                              4 = Architectural
                              5 = Fractional
                              6 = Windows desktop */
        DIMLWD, /**< Dimension line lineweight:
                              -3 = Standard
                              -2 = ByLayer
                              -1 = ByBlock
                              0-211 = an integer representing 100th of mm */
        DIMLWE, /**< Extension line lineweight:
                              -3 = Standard
                              -2 = ByLayer
                              -1 = ByBlock
                              0-211 = an integer representing 100th of mm */
        DIMPOST, /**< General dimensioning suffix */
        DIMRND, /**< Rounding value for dimension distances */
        DIMSAH, /**< Use separate arrow blocks if nonzero */
        DIMSCALE, /**< Overall dimensioning scale factor */
        DIMSD1, /**< Suppression of first extension line:
                              0 = Not suppressed
                              1 = Suppressed */
        DIMSD2, /**< Suppression of second extension line:
                              0 = Not suppressed
                              1 = Suppressed */
        DIMSE1, /**< First extension line suppressed if nonzero */
        DIMSE2, /**< Second extension line suppressed if nonzero */
        DIMSHO, /**< 1 = Recompute dimensions while dragging
                              0 = Drag original image */
        DIMSOXD, /**< Suppress outside-extensions dimension lines if
                              nonzero */
        DIMSTYLE, /**< Dimension style name */
        DIMTAD, /**< Text above dimension line if nonzero */
        DIMTDEC, /**< Number of decimal places to display the tolerance
                              values */
        DIMTFAC, /**< Dimension tolerance display scale factor */
        DIMTIH, /**< Text inside horizontal if nonzero */
        DIMTIX, /**< Force text inside extensions if nonzero */
        DIMTM, /**< Minus tolerance */
        DIMTMOVE, /**< Dimension text movement rules:
                              0 = Moves the dimension line with dimension text
                              1 = Adds a leader when dimension text is moved
                              2 = Allows text to be moved freely without a leader */
        DIMTOFL, /**< If text is outside extensions, force line
                              extensions between extensions if nonzero */
        DIMTOH, /**< Text outside horizontal if nonzero */
        DIMTOL, /**< Dimension tolerances generated if nonzero */
        DIMTOLJ, /**< Vertical justification for tolerance values:
                              0 = Top
                              1 = Middle
                              2 = Bottom */
        DIMTP, /**< Plus tolerance */
        DIMTSZ, /**< Dimensioning tick size:
                              0 = No ticks */
        DIMTVP, /**< Text vertical position */
        DIMTXSTY, /**< Dimension text style */
        DIMTXT, /**< Dimensioning text height */
        DIMTZIN, /**< Controls suppression of zeros for tolerance values:
                              0 = Suppresses zero feet and precisely zero inches
                              1 = Includes zero feet and precisely zero inches
                              2 = Includes zero feet and suppresses zero inches
                              3 = Includes zero inches and suppresses zero feet */
        DIMUPT, /**< Cursor functionality for user-positioned text:
                              0 = Controls only the dimension line location
                              1 = Controls the text position as well as the
                                  dimension line location */
        DIMZIN, /**< Controls suppression of zeros for primary unit
                              values:
                              0 = Suppresses zero feet and precisely zero inches
                              1 = Includes zero feet and precisely zero inches
                              2 = Includes zero feet and suppresses zero inches
                              3 = Includes zero inches and suppresses zero feet */
        DISPSILH, /**< Controls the display of silhouette curves of body
                              objects in Wireframe mode:
                              0 = Off
                              1 = On */
        DRAGVS, /**< Hard-pointer ID to visual style while creating 3D
                              solid primitives. The default value is NULL */
        DWGCODEPAGE, /**< Drawing code page; set to the system code page
                              when a new drawing is created, but not otherwise
                              maintained by AutoCAD */
        ELEVATION, /**< Current elevation set by ELEV command */
        ENDCAPS, /**< Lineweight endcaps setting for new objects:
                              0 = none
                              1 = round
                              2 = angle
                              3 = square */
        EXTMAX, /**< X, Y, and Z drawing extents upper-right corner
                              (in WCS) */
        EXTMIN, /**< X, Y, and Z drawing extents lower-left corner
                              (in WCS) */
        EXTNAMES, /**< Controls symbol table naming:
                              0 = Release 14 compatibility. Limits names to 31
                                  characters in length. Names can include the
                                  letters A to Z, the numerals 0 to 9, and the
                                  special characters dollar sign ($), underscore
                                  (_), and hyphen (-).
                              1 = AutoCAD 2000. Names can be up to 255 characters
                                  in length, and can include the letters A to Z,
                                  the numerals 0 to 9, spaces, and any special
                                  characters not used for other purposes by
                                  Microsoft Windows and AutoCAD */
        FILLETRAD, /**< Fillet radius */
        FILLMODE, /**< Fill mode on if nonzero */
        FINGERPRINTGUID, /**< Set at creation time, uniquely identifies a
                              particular drawing */
        HALOGAP, /**< Specifies a gap to be displayed where an object is
                              hidden by another object; the value is specified
                              as a percent of one unit and is independent of the
                              zoom level. A haloed line is shortened at the
                              point where it is hidden when HIDE or the Hidden
                              option of SHADEMODE is used */
        HANDSEED, /**< Next available handle */
        HIDETEXT, /**< Specifies HIDETEXT system variable:
                              0 = HIDE ignores text objects when producing the
                                  hidden view
                              1 = HIDE does not ignore text objects */
        HYPERLINKBASE, /**< Path for all relative hyperlinks in the drawing.
                              If null, the drawing path is used */
        INDEXCTL, /**< Controls whether layer and spatial indexes are
                              created and saved in drawing files:
                              0 = No indexes are created
                              1 = Layer index is created
                              2 = Spatial index is created
                              3 = Layer and spatial indexes are created */
        INSBASE, /**< Insertion base set by BASE command (in WCS) */
        INSUNITS, /**< Default drawing units for AutoCAD DesignCenter
                              blocks:
                              0 = Unitless
                              1 = Inches
                              2 = Feet
                              3 = Miles
                              4 = Millimeters
                              5 = Centimeters
                              6 = Meters
                              7 = Kilometers
                              8 = Microinches
                              9 = Mils
                             10 = Yards
                             11 = Angstroms
                             12 = Nanometers
                             13 = Microns
                             14 = Decimeters
                             15 = Decameters
                             16 = Hectometers
                             17 = Gigameters
                             18 = Astronomical units
                             19 = Light years
                             20 = Parsecs */
        INTERFERECOLOR, /**< Represents the ACI color index of the
                              "interference objects" created during the
                              interfere command. Default value is 1 */
        INTERFEREOBJVS, /**< Hard-pointer ID to the visual style for
                              interference objects. Default visual style is
                              Conceptual */
        INTERFEREVPVS, /**< Hard-pointer ID to the visual style for the
                              viewport during interference checking. Default
                              visual style is 3d Wireframe. */
        INTERSECTIONCOLOR, /**< Specifies the entity color of intersection
                                 polylines:
                                 Values 1-255 designate an AutoCAD color index (ACI)
                                 0 = Color BYBLOCK
                               256 = Color BYLAYER
                               257 = Color BYENTITY */
        INTERSECTIONDISPLAY, /**< Specifies the display of intersection polylines:
                                 0 = Turns off the display of intersection
                                     polylines
                                 1 = Turns on the display of intersection
                                     polylines */
        JOINSTYLE, /**< Lineweight joint setting for new objects:
                              0 = none
                              1 = round
                              2 = angle
                              3 = flat */
        LIMCHECK, /**< Nonzero if limits checking is on */
        LIMMAX, /**< XY drawing limits upper-right corner (in WCS) */
        LIMMIN, /**< XY drawing limits lower-left corner (in WCS) */
        LTSCALE, /**< Global linetype scale */
        LUNITS, /**< Units format for coordinates and distances */
        LUPREC, /**< Units precision for coordinates and distances */
        LWDISPLAY, /**< Controls the display of lineweights on the Model
                              or Layout tab:
                              0 = Lineweight is not displayed
                              1 = Lineweight is displayed */
        MAXACTVP, /**< Sets maximum number of viewports to be regenerated */
        MEASUREMENT, /**< Sets drawing units:
                              0 = English
                              1 = Metric */
        MENU, /**< Name of menu file */
        MIRRTEXT, /**< Mirror text if nonzero */
        OBSCOLOR, /**< Specifies the color of obscured lines. An obscured
                              line is a hidden line made visible by changing its
                              color and linetype and is visible only when the
                              HIDE or SHADEMODE command is used. The OBSCUREDCOLOR
                              setting is visible only if the OBSCUREDLTYPE is
                              turned ON by setting it to a value other than 0.
                              0 and 256 = Entity color
                              1-255 = An AutoCAD color index (ACI) */
        OBSLTYPE, /**< Specifies the linetype of obscured lines. Obscured
                              linetypes are independent of zoom level, unlike
                              regular AutoCAD linetypes. Value 0 turns off
                              display of obscured lines and is the default.
                              Linetype values are defined as follows:
                              0 = Off
                              1 = Solid
                              2 = Dashed
                              3 = Dotted
                              4 = Short Dash
                              5 = Medium Dash
                              6 = Long Dash
                              7 = Double Short Dash
                              8 = Double Medium Dash
                              9 = Double Long Dash
                             10 = Medium Long Dash
                             11 = Sparse Dot */
        ORTHOMODE, /**< Ortho mode on if nonzero */
        PDMODE, /**< Point display mode */
        PDSIZE, /**< Point display size */
        PELEVATION, /**< Current paper space elevation */
        PEXTMAX, /**< Maximum X, Y, and Z extents for paper space */
        PEXTMIN, /**< Minimum X, Y, and Z extents for paper space */
        PINSBASE, /**< Paper space insertion base point */
        PLIMCHECK, /**< Limits checking in paper space when nonzero */
        PLIMMAX, /**< Maximum X and Y limits in paper space */
        PLIMMIN, /**< Minimum X and Y limits in paper space */
        PLINEGEN, /**< Governs the generation of linetype patterns around
                              the vertices of a 2D polyline:
                              1 = Linetype is generated in a continuous pattern
                                  around vertices of the polyline
                              0 = Each segment of the polyline starts and ends
                                  with a dash */
        PLINEWID, /**< Default polyline width */
        PROJECTNAME, /**< Assigns a project name to the current drawing.
                              Used when an external reference or image is not
                              found on its original path. The project name
                              points to a section in the registry that can
                              contain one or more search paths for each project
                              name defined. Project names and their search
                              directories are created from the Files tab of the
                              Options dialog box */
        PROXYGRAPHICS, /**< Controls the saving of proxy object images */
        PSLTSCALE, /**< Controls paper space linetype scaling:
                              1 = No special linetype scaling
                              0 = Viewport scaling governs linetype scaling */
        PSTYLEMODE, /**< Indicates whether the current drawing is in a
                              Color-Dependent or Named Plot Style mode:
                              0 = Uses named plot style tables in the current
                                  drawing
                              1 = Uses color-dependent plot style tables in the
                                  current drawing */
        PSVPSCALE, /**< View scale factor for new viewports:
                              0 = Scaled to fit
                             >0 = Scale factor (a positive real value) */
        PUCSBASE, /**< Name of the UCS that defines the origin and
                              orientation of orthographic UCS settings (paper
                              space only) */
        PUCSNAME, /**< Current paper space UCS name */
        PUCSORG, /**< Current paper space UCS origin */
        PUCSORGBACK, /**< Point which becomes the new UCS origin after
                              changing paper space UCS to BACK when PUCSBASE is
                              set to WORLD */
        PUCSORGBOTTOM, /**< Point which becomes the new UCS origin after
                              changing paper space UCS to BOTTOM when PUCSBASE
                              is set to WORLD */
        PUCSORGFRONT, /**< Point which becomes the new UCS origin after
                              changing paper space UCS to FRONT when PUCSBASE is
                              set to WORLD */
        PUCSORGLEFT, /**< Point which becomes the new UCS origin after
                              changing paper space UCS to LEFT when PUCSBASE is
                              set to WORLD */
        PUCSORGRIGHT, /**< Point which becomes the new UCS origin after
                              changing paper space UCS to RIGHT when PUCSBASE is
                              set to WORLD */
        PUCSORGTOP, /**< Point which becomes the new UCS origin after
                              changing paper space UCS to TOP when PUCSBASE is
                              set to WORLD */
        PUCSORTHOREF, /**< If paper space UCS is orthographic (PUCSORTHOVIEW
                              not equal to 0), this is the name of the UCS that
                              the orthographic UCS is relative to. If blank, UCS
                              is relative to WORLD */
        PUCSORTHOVIEW, /**< Orthographic view type of paper space UCS:
                              0 = UCS is not orthographic
                              1 = Top
                              2 = Bottom
                              3 = Front
                              4 = Back
                              5 = Left
                              6 = Right */
        PUCSXDIR, /**< Current paper space UCS X axis */
        PUCSYDIR, /**< Current paper space UCS Y axis */
        QTEXTMODE, /**< Quick Text mode on if nonzero */
        REGENMODE, /**< REGENAUTO mode on if nonzero */
        SHADEDGE, /**< 0 = Faces shaded, edges not highlighted
                              1 = Faces shaded, edges highlighted in black
                              2 = Faces not filled, edges in entity color
                              3 = Faces in entity color, edges in black */
        SHADEDIF, /**< Percent ambient/diffuse light range 1-100
                              default 70 */
        SHADOWPLANELOCATION, /**< Location of the ground shadow plane. This is a
                                  Z axis ordinate. */
        SKETCHINC, /**< Sketch record increment */
        SKPOLY, /**< 0 = Sketch lines
                              1 = Sketch polylines */
        SORTENTS, /**< Controls the object sorting methods; accessible
                              from the Options dialog box User Preferences tab.
                              SORTENTS uses the following bitcodes:
                              0 = Disables SORTENTS
                              1 = Sorts for object selection
                              2 = Sorts for object snap
                              4 = Sorts for redraws
                              8 = Sorts for MSLIDE command slide creation
                             16 = Sorts for REGEN commands
                             32 = Sorts for plotting
                             64 = Sorts for PostScript output */
        SPLINESEGS, /**< Number of line segments per spline patch */
        SPLINETYPE, /**< Spline curve type for PEDIT Spline */
        SURFTAB1, /**< Number of mesh tabulations in first direction */
        SURFTAB2, /**< Number of mesh tabulations in second direction */
        SURFTYPE, /**< Surface type for PEDIT Smooth */
        SURFU, /**< Surface density (for PEDIT Smooth) in M direction */
        SURFV, /**< Surface density (for PEDIT Smooth) in N direction */
        TDCREATE, /**< Local date/time of drawing creation (see “Special
                              Handling of Date/Time Variables”) */
        TDINDWG, /**< Cumulative editing time for this drawing */
        TDUCREATE, /**< Universal date/time the drawing was created */
        TDUPDATE, /**< Local date/time of last drawing update */
        TDUSRTIMER, /**< User-elapsed timer */
        TDUUPDATE, /**< Universal date/time of the last update/save */
        TEXTSIZE, /**< Default text height */
        TEXTSTYLE, /**< Current text style name */
        THICKNESS, /**< Current thickness set by ELEV command */
        TILEMODE, /**< 1 for previous release compatibility mode
                              0 otherwise */
        TRACEWID, /**< Default trace width */
        TREEDEPTH, /**< Specifies the maximum depth of the spatial index */
        UCSBASE, /**< Name of the UCS that defines the origin and
                              orientation of orthographic UCS settings */
        UCSNAME, /**< Name of current UCS */
        UCSORG, /**< Origin of current UCS (in WCS) */
        UCSORGBACK, /**< Point which becomes the new UCS origin after
                              changing model space UCS to BACK when UCSBASE is
                              set to WORLD */
        UCSORGBOTTOM, /**< Point which becomes the new UCS origin after
                              changing model space UCS to BOTTOM when UCSBASE is
                              set to WORLD */
        UCSORGFRONT, /**< Point which becomes the new UCS origin after
                              changing model space UCS to FRONT when UCSBASE is
                              set to WORLD */
        UCSORGLEFT, /**< Point which becomes the new UCS origin after
                              changing model space UCS to LEFT when UCSBASE is
                              set to WORLD */
        UCSORGRIGHT, /**< Point which becomes the new UCS origin after
                              changing model space UCS to RIGHT when UCSBASE is
                              set to WORLD */
        UCSORGTOP, /**< Point which becomes the new UCS origin after
                              changing model space UCS to TOP when UCSBASE is
                              set to WORLD */
        UCSORTHOREF, /**< If model space UCS is orthographic (UCSORTHOVIEW
                              not equal to 0), this is the name of the UCS that
                              the orthographic UCS is relative to. If blank, UCS
                              is relative to WORLD */
        UCSORTHOVIEW, /**< Orthographic view type of model space UCS:
                              0 = UCS is not orthographic
                              1 = Top
                              2 = Bottom
                              3 = Front
                              4 = Back
                              5 = Left
                              6 = Right */
        UCSXDIR, /**< Direction of the current UCS X axis (in WCS) */
        UCSYDIR, /**< Direction of the current UCS Y axis (in WCS) */
        UNITMODE, /**< Low bit set = Display fractions, feet-and-inches,
                              and surveyor's angles in input format */
        USERI1, /**< Five integer variables intended for use by
                              third-party developers */
        USERI2, USERI3, USERI4, USERI5, USERR1, /**< Five real variables intended for use by
                              third-party developers */
        USERR2, USERR3, USERR4, USERR5, USRTIMER, /**< 0 = Timer off
                              1 = Timer on */
        VERSIONGUID, /**< Uniquely identifies a particular version of a
                              drawing. Updated when the drawing is modified */
        VISRETAIN, /**< 0 = Don't retain xref-dependent visibility settings
                              1 = Retain xref-dependent visibility settings */
        WORLDVIEW, /**< 1 = Set UCS to WCS during DVIEW/VPOINT
                              0 = Don't change UCS */
        XCLIPFRAME, /**< Controls the visibility of xref clipping
                              boundaries:
                              0 = Clipping boundary is not visible
                              1 = Clipping boundary is visible */
        XEDIT, /**< Controls whether the current drawing can be edited
                              in-place when being referenced by another drawing.
                              0 = Can't use in-place reference editing
                              1 = Can use in-place reference editing */
        SPLFRAME, /** ? */
        WORDLVIEW, /** ? */
        PELLIPSE, /** ? */
        ISOLINES, /** ? */
        TEXTQLTY, /** ? */
        FACETRES, /** ? */
        DIMFRAC, /** ? */
        OLESTARTUP, /** ? */
        STYLESHEET, /** ? */
        TSTACKALIGN, /**< default = 1 (not present in DXF) */
        TSTACKSIZE, /**< default = 70 (not present in DXF) */
        MAX_HEADER_CONSTANT = 1000 /**< max + num for user constants */

    };
public:
                     CADHeader();
    /**
     * @brief Add new value to the CAD file header
     * @param code The code from constants enum
     * @param val Value to add
     * @return SUCCESS or some value from CADErrorCodes
     */
    int              addValue( short code, const CADVariant& val );
    int              addValue( short code, const char * val );
    //int              addValue( short code, long val );
    int              addValue( short code, int val );
    int              addValue( short code, short val );
    int              addValue( short code, double val );
    int              addValue( short code, const std::string& val );
    int              addValue( short code, bool val );
    int              addValue( short code, double x, double y, double z = 0 );
    int              addValue( short code, long julianday, long milliseconds );
    static int              getGroupCode( short code );
    const CADVariant getValue( short code, const CADVariant& val = CADVariant() ) const;
    static const char * getValueName( short code );
    void   print() const;
    size_t getSize() const;
    short  getCode( int index ) const;
protected:
    std::map<short, CADVariant> valuesMap;
};

#endif // CADHEADER_H
