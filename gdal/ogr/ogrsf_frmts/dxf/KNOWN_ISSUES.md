OGR DXF driver: Known issues
============================

This is a brief list of known issues (mainly missing features) in the DXF
driver. Anyone is welcome to contribute code that addresses these issues.

## Reader

* The following DXF entities are not supported at all: MESH, MPOLYGON, RAY,
TABLE, TOLERANCE, WIPEOUT, XLINE.

### 3DFACE

* There seems to be some ambivalence as to whether 3DFACE entities should be
represented as outlined polygons, as we currently do, or filled polygons. Any
input on this issue would be appreciated.

### DIMENSION

* 3D (z-coordinate) and OCS (extrusion) support is absent from both the
anonymous block logic and the fallback renderer.

* The DIMENSION fallback renderer, used when the anonymous block is absent, is
pretty rudimentary. I personally don't plan to work on it any further. Note
that AutoCAD refuses to open DXF files that have a DIMENSION with no anonymous
block.

### HATCH

* Fill types are not read. Any implementation of fill types (i.e. patterns)
would need to be somewhat heuristic, given the basic set of choices available
in the OGR style string spec.

### LEADER

* 3D (z-coordinate) and OCS (extrusion) support is absent.

### LWPOLYLINE

* Width support has not been added.

### MLINE

* Support for MLINESTYLE is missing. This means that only the basic geometries
of MLINE entities are translated, not the more complex styling properties,
such as different colors or linetypes for the different elements of the line.

### MULTILEADER

* 3D (z-coordinate) and OCS (extrusion) support is missing. This entity is
very complex and adding 3D/OCS support would take a long time.

### Text entities

* Text using AutoCAD-specific SHX (plotter) fonts, such as "txt", is
reported as being in Arial.

* Text with a nonzero oblique angle, or that is upside down and/or backwards,
will appear normal, due to the lack of support for these properties in an OGR
style string. I don't think there is too much point adding these to the style
string spec, as these attributes are little-used and not commonly available in
other software.

## Writer

* The writer doesn't know about transparency. This means that hidden objects,
which are represented using fully transparent colors by the reader, do not
round-trip correctly.

* As noted above, 3DFACE entities are represented by POLYGON geometries with a
PEN() style tool, a combination which the writer handles poorly.

* The writer does not write out ATTRIB entities for a POINT feature with a
non-empty BlockAttributes field, that is to say, block attributes are not
round-tripped. This is certainly a gap in the feature set, but I only plan to
write this code if someone expresses a need for it.

---

Alan Thomas, ThinkSpatial  
athomas@thinkspatial.com.au  
June 2018
