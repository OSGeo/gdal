<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<foo:FeatureCollection xmlns:foo="http://example.com" xmlns:gml="http://www.opengis.net/gml">
  <foo:myMember>
    <foo:Foo fid="fid1">
      <foo:boundedBy>
        <gml:coordinates>0,1 2,3</gml:coordinates>
        <something_unexpected/>
      </foo:boundedBy>
    </foo:Foo>
  </foo:myMember>
</foo:FeatureCollection>
