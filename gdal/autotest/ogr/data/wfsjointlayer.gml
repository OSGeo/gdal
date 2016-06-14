<?xml version="1.0" encoding="UTF-8"?>
<wfs:FeatureCollection xmlns:xs="http://www.w3.org/2001/XMLSchema"
    xmlns:app="http://app.com"
    xmlns:wfs="http://www.opengis.net/wfs/2.0"
    xmlns:gml="http://www.opengis.net/gml/3.2"
    xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
    numberMatched="unknown" numberReturned="2" timeStamp="2015-01-01T00:00:00.000Z"
    xsi:schemaLocation="http://www.opengis.net/gml/3.2 http://schemas.opengis.net/gml/3.2.1/gml.xsd 
                        http://www.opengis.net/wfs/2.0 http://schemas.opengis.net/wfs/2.0/wfs.xsd">
  <wfs:member>
    <wfs:Tuple>
      <wfs:member>
        <app:table1 gml:id="table1-1">
          <app:foo>1</app:foo>
        </app:table1>
      </wfs:member>
      <wfs:member>
        <app:table2 gml:id="table2-1">
          <app:bar>2</app:bar>
          <app:baz>foo</app:baz>
          <app:geometry><gml:Point gml:id="table2-2.geom.0"><gml:pos>2 49</gml:pos></gml:Point></app:geometry>
        </app:table2>
      </wfs:member>
    </wfs:Tuple>
  </wfs:member>
  <wfs:member>
    <wfs:Tuple>
      <wfs:member>
        <app:table1 gml:id="table1-2">
          <app:bar>2</app:bar>
          <app:geometry><gml:Point gml:id="table1-1.geom.0"><gml:pos>3 50</gml:pos></gml:Point></app:geometry>
        </app:table1>
      </wfs:member>
      <wfs:member>
        <app:table2 gml:id="table2-2">
          <app:bar>2</app:bar>
          <app:baz>bar</app:baz>
          <app:geometry><gml:Point gml:id="table2-2.geom.0"><gml:pos>2 50</gml:pos></gml:Point></app:geometry>
        </app:table2>
      </wfs:member>
    </wfs:Tuple>
  </wfs:member>
</wfs:FeatureCollection>
