<?xml version="1.0" encoding="utf-8"?>
<gml:FeatureCollection xmlns:xlink="http://www.w3.org/1999/xlink"
                       xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
                       gml:id="foo" xmlns:gml="http://www.opengis.net/gml/3.2">
  <gml:featureMember>
    <TopLevelObject gml:id="TopLevelObject.1">
      <content>
        <Object gml:id="Object.1">
          <geometry>
            <gml:Polygon gml:id="Object.1.Geometry" srsName="urn:ogc:def:crs:EPSG::4326">
              <gml:exterior>
                <gml:LinearRing>
                  <gml:posList srsDimension="2">48 2 49 2 49 3 48 3 48 2</gml:posList>
                </gml:LinearRing>
              </gml:exterior>
            </gml:Polygon>
          </geometry>
          <foo>bar</foo>
        </Object>
      </content>
      <content>
        <Object gml:id="Object.2">
          <geometry>
            <gml:Polygon gml:id="Object.2.Geometry" srsName="urn:ogc:def:crs:EPSG::4326">
              <gml:exterior>
                <gml:LinearRing>
                  <gml:posList srsDimension="2">-48 2 -49 2 -49 3 -48 3 -48 2</gml:posList>
                </gml:LinearRing>
              </gml:exterior>
            </gml:Polygon>
          </geometry>
          <foo>baz</foo>
        </Object>
      </content>
    </TopLevelObject>
  </gml:featureMember>
</gml:FeatureCollection>
