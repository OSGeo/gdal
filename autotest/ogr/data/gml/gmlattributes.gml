<?xml version="1.0" encoding="utf-8" ?>
<ogr:FeatureCollection xmlns:gml="http://www.opengis.net/gml" xmlns:ogr="http://ogr.maptools.org/" xmlns:prefix="http://prefix">
  <gml:featureMember>
    <ogr:test>
      <ogr:element attr1="1"/>
      <ogr:element2 prefix:attr1="a">foo</ogr:element2>
    </ogr:test>
  </gml:featureMember>
  <gml:featureMember>
    <ogr:test>
      <ogr:nested>
        <ogr:element3 attr1="1"/>
      </ogr:nested>
    </ogr:test>
  </gml:featureMember>
  <gml:featureMember>
    <ogr:test>
      <ogr:element attr1="a"/>
    </ogr:test>
  </gml:featureMember>
</ogr:FeatureCollection>
