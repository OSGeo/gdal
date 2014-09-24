<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<CityModel xmlns="http://www.opengis.net/citygml/1.0" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
           xmlns:trans="http://www.opengis.net/citygml/transportation/1.0"
           xmlns:gml="http://www.opengis.net/gml"
           xmlns:smil20lang="http://www.w3.org/2001/SMIL20/Language"
           xmlns:xlink="http://www.w3.org/1999/xlink"
           xmlns:gen="http://www.opengis.net/citygml/generics/1.0"
           xsi:schemaLocation="http://www.opengis.net/citygml/transportation/1.0 http://schemas.opengis.net/citygml/transportation/1.0/transportation.xsdn.xsd http://www.opengis.net/citygml/cityobjectgroup/1.0 http://schemas.opengis.net/citygml/cityobjectgroup/1.0/cityObjectGroup.xsd http://www.opengis.net/citygml/generics/1.0 http://schemas.opengis.net/citygml/generics/1.0/generics.xsd">
  <cityObjectMember>
    <trans:Road gml:id="id1">
      <gml:name>aname</gml:name>
      <gml:boundedBy>
        <gml:Envelope srsDimension="3" srsName="urn:ogc:def:crs,crs:EPSG:6.12:3068,crs:EPSG:6.12:5783">
          <gml:lowerCorner>0 0 0</gml:lowerCorner>
          <gml:upperCorner>1 1 0</gml:upperCorner>
        </gml:Envelope>
      </gml:boundedBy>
      <creationDate>2010-01-01+00:00</creationDate>
      <gen:stringAttribute name="Name">
        <gen:value>aname</gen:value>
      </gen:stringAttribute>
      <gen:intAttribute name="a_int_attr">
        <gen:value>2</gen:value>
      </gen:intAttribute>
      <gen:doubleAttribute name="a_double_attr">
        <gen:value>3.45</gen:value>
      </gen:doubleAttribute>
      <trans:lod3MultiSurface>
        <gml:MultiSurface gml:id="id2">
          <gml:surfaceMember>
            <gml:Polygon gml:id="id3">
              <gml:exterior>
                <gml:LinearRing gml:id="id4">
                    <gml:posList srsDimension="3">0 0 0 0 1 0 1 1 0 1 0 0 0 0 0</gml:posList>
                </gml:LinearRing>
              </gml:exterior>
            </gml:Polygon>
          </gml:surfaceMember>
        </gml:MultiSurface>
      </trans:lod3MultiSurface>
    </trans:Road>
  </cityObjectMember>
</CityModel>
