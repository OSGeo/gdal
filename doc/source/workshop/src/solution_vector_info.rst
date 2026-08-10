:orphan:

.. _solution_vector_info:

Exercise solution for vector info
=================================

::

    $ gdal vector info timisoara.osm.pbf --sql "SELECT *, ST_Distance(geometry, ST_Point(21.2322, 45.7558), true) distance_from_center FROM points WHERE other_tags LIKE '%bar%' ORDER BY distance_from_center LIMIT 5" --dialect sqlite --features

Output:

::

    INFO: Open of `timisoara.osm.pbf'
          using driver `OSM' successful.

    Layer name: SELECT
    Geometry: Point
    Feature Count: 5
    Extent: (21.228195, 45.755208) - (21.234416, 45.756671)
    Layer Coordinate Reference System:
      - name: WGS 84
      - ID: EPSG:4326
      - type: Geographic 2D
    Data axis to CRS axis mapping: 2,1
    Geometry Column = GEOMETRY
    osm_id: String (0.0)
    name: String (0.0)
    barrier: String (0.0)
    highway: String (0.0)
    ref: String (0.0)
    address: String (0.0)
    is_in: String (0.0)
    place: String (0.0)
    man_made: String (0.0)
    other_tags: String (0.0)
    distance_from_center: Real (0.0)
    OGRFeature(SELECT):0
      osm_id (String) = 4214502952
      name (String) = MGM Bastion
      barrier (String) = (null)
      highway (String) = (null)
      ref (String) = (null)
      address (String) = (null)
      is_in (String) = (null)
      place (String) = (null)
      man_made (String) = (null)
      other_tags (String) = "amenity"=>"bar","cuisine"=>"breakfast;burger;chicken;german;grill;hot_dog;local","internet_access"=>"wlan","opening_hours"=>"24/7"
      distance_from_center (Real) = 90.4872767351965
      POINT (21.2326735 45.7565436)

    OGRFeature(SELECT):1
      osm_id (String) = 7976845620
      name (String) = Barrio
      barrier (String) = (null)
      highway (String) = (null)
      ref (String) = (null)
      address (String) = (null)
      is_in (String) = (null)
      place (String) = (null)
      man_made (String) = (null)
      other_tags (String) = "addr:city"=>"Timișoara","addr:housenumber"=>"6","addr:street"=>"Strada Eugeniu de Savoya","amenity"=>"bar"
      distance_from_center (Real) = 167.767604090299
      POINT (21.2304389 45.756671)

    OGRFeature(SELECT):2
      osm_id (String) = 6930124470
      name (String) = The Post
      barrier (String) = (null)
      highway (String) = (null)
      ref (String) = (null)
      address (String) = (null)
      is_in (String) = (null)
      place (String) = (null)
      man_made (String) = (null)
      other_tags (String) = "amenity"=>"bar"
      distance_from_center (Real) = 184.50111354138
      POINT (21.2344156 45.7552083)

    OGRFeature(SELECT):3
      osm_id (String) = 3755520724
      name (String) = Enoteca de Savoya
      barrier (String) = (null)
      highway (String) = (null)
      ref (String) = (null)
      address (String) = (null)
      is_in (String) = (null)
      place (String) = (null)
      man_made (String) = (null)
      other_tags (String) = "addr:city"=>"Timișoara","addr:housenumber"=>"11","addr:postcode"=>"300085","addr:street"=>"Strada Eugeniu de Savoya","amenity"=>"bar","phone"=>"+40 256 433 644","website"=>"https://www.enotecadesavoya.ro/"
      distance_from_center (Real) = 315.856848810125
      POINT (21.2283292 45.7566568)

    OGRFeature(SELECT):4
      osm_id (String) = 5224522189
      name (String) = Storia
      barrier (String) = (null)
      highway (String) = (null)
      ref (String) = (null)
      address (String) = (null)
      is_in (String) = (null)
      place (String) = (null)
      man_made (String) = (null)
      other_tags (String) = "amenity"=>"bar","opening_hours"=>"Th-Sa 18:00-04:00; Tu 18:00-02:00"
      distance_from_center (Real) = 326.302012684119
      POINT (21.2281946 45.7566704)
