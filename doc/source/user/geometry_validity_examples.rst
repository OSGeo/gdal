

Self-intersecting polygon
^^^^^^^^^^^^^^^^^^^^^^^^^


.. list-table:: 
   :header-rows: 1
   :widths: 1 1 1

   * - Input geometry and result of ``gdal vector check-geometry``
     - ``gdal vector make-valid --method=linework``
     - ``gdal vector make-valid --method=structure``

    
   * - .. image:: ../../images/user/geometry_validity/geometry_source_1.svg

       Input geometry: ``POLYGON ((10 90,90 10,90 90,10 10,10 90))``

       Error message: ``Self-intersection``

       Error geometry: ``MULTIPOINT (50 50)``

     - .. image:: ../../images/user/geometry_validity/geometry_make_valid_linework_1.svg
       
       ``MULTIPOLYGON (((10 10,10 90,50 50,10 10)),((90 10,50 50,90 90,90 10)))``

     - .. image:: ../../images/user/geometry_validity/geometry_make_valid_structure_1.svg
       
       ``MULTIPOLYGON (((10 90,50 50,10 10,10 90)),((50 50,90 90,90 10,50 50)))``

.. _polygon_self_touching:
    

Polygon with self-touching ring
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^


.. list-table:: 
   :header-rows: 1
   :widths: 1 1 1

   * - Input geometry and result of ``gdal vector check-geometry``
     - ``gdal vector make-valid --method=linework``
     - ``gdal vector make-valid --method=structure``

    
   * - .. image:: ../../images/user/geometry_validity/geometry_source_2.svg

       Input geometry: ``POLYGON ((10 10,90 10,90 40,80 20,70 40,80 60,90 40,90 90,10 90,10 10))``

       Error message: ``Ring Self-intersection``

       Error geometry: ``MULTIPOINT (90 40)``

     - .. image:: ../../images/user/geometry_validity/geometry_make_valid_linework_2.svg
       
       ``POLYGON ((90 10,10 10,10 90,90 90,90 40,90 10),(80 60,70 40,80 20,90 40,80 60))``

     - .. image:: ../../images/user/geometry_validity/geometry_make_valid_structure_2.svg
       
       ``POLYGON ((10 10,10 90,90 90,90 40,90 10,10 10),(90 40,80 60,70 40,80 20,90 40))``


Polygon hole outside shell
^^^^^^^^^^^^^^^^^^^^^^^^^^


.. list-table:: 
   :header-rows: 1
   :widths: 1 1 1

   * - Input geometry and result of ``gdal vector check-geometry``
     - ``gdal vector make-valid --method=linework``
     - ``gdal vector make-valid --method=structure``

    
   * - .. image:: ../../images/user/geometry_validity/geometry_source_3.svg

       Input geometry: ``POLYGON ((10 90,50 90,50 10,10 10,10 90),(60 80,90 80,90 20,60 20,60 80))``

       Error message: ``Hole lies outside shell``

       Error geometry: ``MULTIPOINT (60 80)``

     - .. image:: ../../images/user/geometry_validity/geometry_make_valid_linework_3.svg
       
       ``MULTIPOLYGON (((50 10,10 10,10 90,50 90,50 10)),((90 20,60 20,60 80,90 80,90 20)))``

     - .. image:: ../../images/user/geometry_validity/geometry_make_valid_structure_3.svg
       
       ``MULTIPOLYGON (((50 90,50 10,10 10,10 90,50 90)),((90 80,90 20,60 20,60 80,90 80)))``


Hole partially outside polygon shell
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^


.. list-table:: 
   :header-rows: 1
   :widths: 1 1 1

   * - Input geometry and result of ``gdal vector check-geometry``
     - ``gdal vector make-valid --method=linework``
     - ``gdal vector make-valid --method=structure``

    
   * - .. image:: ../../images/user/geometry_validity/geometry_source_4.svg

       Input geometry: ``POLYGON ((10 90,60 90,60 10,10 10,10 90),(30 70,90 70,90 30,30 30,30 70))``

       Error message: ``Self-intersection``

       Error geometry: ``MULTIPOINT (60 70)``

     - .. image:: ../../images/user/geometry_validity/geometry_make_valid_linework_4.svg
       
       ``MULTIPOLYGON (((90 70,90 30,60 30,60 70,90 70)),((60 10,10 10,10 90,60 90,60 70,30 70,30 30,60 30,60 10)))``

     - .. image:: ../../images/user/geometry_validity/geometry_make_valid_structure_4.svg
       
       ``POLYGON ((60 90,60 70,30 70,30 30,60 30,60 10,10 10,10 90,60 90))``


Polygon hole equal to shell
^^^^^^^^^^^^^^^^^^^^^^^^^^^


.. list-table:: 
   :header-rows: 1
   :widths: 1 1 1

   * - Input geometry and result of ``gdal vector check-geometry``
     - ``gdal vector make-valid --method=linework``
     - ``gdal vector make-valid --method=structure``

    
   * - .. image:: ../../images/user/geometry_validity/geometry_source_5.svg

       Input geometry: ``POLYGON ((10 90,90 90,90 10,10 10,10 90),(10 90,90 90,90 10,10 10,10 90))``

       Error message: ``Self-intersection``

       Error geometry: ``MULTIPOINT (10 90)``

     - .. image:: ../../images/user/geometry_validity/geometry_make_valid_linework_5.svg
       
       ``POLYGON ((90 90,90 10,10 10,10 90,90 90))``

     - .. image:: ../../images/user/geometry_validity/geometry_make_valid_structure_5.svg
       
       ``POLYGON EMPTY``


Polygon holes overlap
^^^^^^^^^^^^^^^^^^^^^


.. list-table:: 
   :header-rows: 1
   :widths: 1 1 1

   * - Input geometry and result of ``gdal vector check-geometry``
     - ``gdal vector make-valid --method=linework``
     - ``gdal vector make-valid --method=structure``

    
   * - .. image:: ../../images/user/geometry_validity/geometry_source_6.svg

       Input geometry: ``POLYGON ((10 90,90 90,90 10,10 10,10 90),(80 80,80 30,30 30,30 80,80 80),(20 20,20 70,70 70,70 20,20 20))``

       Error message: ``Self-intersection``

       Error geometry: ``MULTIPOINT (70 30)``

     - .. image:: ../../images/user/geometry_validity/geometry_make_valid_linework_6.svg
       
       ``MULTIPOLYGON (((90 90,90 10,10 10,10 90,90 90),(80 30,80 80,30 80,30 70,20 70,20 20,70 20,70 30,80 30)),((30 30,30 70,70 70,70 30,30 30)))``

     - .. image:: ../../images/user/geometry_validity/geometry_make_valid_structure_6.svg
       
       ``POLYGON ((90 90,90 10,10 10,10 90,90 90),(20 20,70 20,70 30,80 30,80 80,30 80,30 70,20 70,20 20))``


Polygon shell inside hole
^^^^^^^^^^^^^^^^^^^^^^^^^


.. list-table:: 
   :header-rows: 1
   :widths: 1 1 1

   * - Input geometry and result of ``gdal vector check-geometry``
     - ``gdal vector make-valid --method=linework``
     - ``gdal vector make-valid --method=structure``

    
   * - .. image:: ../../images/user/geometry_validity/geometry_source_7.svg

       Input geometry: ``POLYGON ((30 70,70 70,70 30,30 30,30 70),(10 90,90 90,90 10,10 10,10 90))``

       Error message: ``Hole lies outside shell``

       Error geometry: ``MULTIPOINT (10 90)``

     - .. image:: ../../images/user/geometry_validity/geometry_make_valid_linework_7.svg
       
       ``POLYGON ((90 90,90 10,10 10,10 90,90 90),(30 30,70 30,70 70,30 70,30 30))``

     - .. image:: ../../images/user/geometry_validity/geometry_make_valid_structure_7.svg
       
       ``POLYGON EMPTY``


Self-crossing polygon shell
^^^^^^^^^^^^^^^^^^^^^^^^^^^


.. list-table:: 
   :header-rows: 1
   :widths: 1 1 1

   * - Input geometry and result of ``gdal vector check-geometry``
     - ``gdal vector make-valid --method=linework``
     - ``gdal vector make-valid --method=structure``

    
   * - .. image:: ../../images/user/geometry_validity/geometry_source_8.svg

       Input geometry: ``POLYGON ((10 70,90 70,90 50,30 50,30 30,50 30,50 90,70 90,70 10,10 10,10 70))``

       Error message: ``Self-intersection``

       Error geometry: ``MULTIPOINT (50 70)``

     - .. image:: ../../images/user/geometry_validity/geometry_make_valid_linework_8.svg
       
       ``MULTIPOLYGON (((10 70,50 70,50 50,70 50,70 10,10 10,10 70),(30 50,30 30,50 30,50 50,30 50)),((50 90,70 90,70 70,50 70,50 90)),((90 70,90 50,70 50,70 70,90 70)))``

     - .. image:: ../../images/user/geometry_validity/geometry_make_valid_structure_8.svg
       
       ``POLYGON ((10 70,50 70,50 90,70 90,70 70,90 70,90 50,70 50,70 10,10 10,10 70),(50 50,30 50,30 30,50 30,50 50))``


Self-overlapping polygon shell
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^


.. list-table:: 
   :header-rows: 1
   :widths: 1 1 1

   * - Input geometry and result of ``gdal vector check-geometry``
     - ``gdal vector make-valid --method=linework``
     - ``gdal vector make-valid --method=structure``

    
   * - .. image:: ../../images/user/geometry_validity/geometry_source_9.svg

       Input geometry: ``POLYGON ((10 90,50 90,50 30,70 30,70 50,30 50,30 70,90 70,90 10,10 10,10 90))``

       Error message: ``Self-intersection``

       Error geometry: ``MULTIPOINT (50 70)``

     - .. image:: ../../images/user/geometry_validity/geometry_make_valid_linework_9.svg
       
       ``POLYGON ((50 90,50 70,90 70,90 10,10 10,10 90,50 90),(30 70,30 50,50 50,50 70,30 70),(50 30,70 30,70 50,50 50,50 30))``

     - .. image:: ../../images/user/geometry_validity/geometry_make_valid_structure_9.svg
       
       ``POLYGON ((10 90,50 90,50 70,90 70,90 10,10 10,10 90),(50 50,50 30,70 30,70 50,50 50))``


Nested MultiPolygons
^^^^^^^^^^^^^^^^^^^^


.. list-table:: 
   :header-rows: 1
   :widths: 1 1 1

   * - Input geometry and result of ``gdal vector check-geometry``
     - ``gdal vector make-valid --method=linework``
     - ``gdal vector make-valid --method=structure``

    
   * - .. image:: ../../images/user/geometry_validity/geometry_source_10.svg

       Input geometry: ``MULTIPOLYGON (((30 70,70 70,70 30,30 30,30 70)),((10 90,90 90,90 10,10 10,10 90)))``

       Error message: ``Nested shells``

       Error geometry: ``MULTIPOINT (30 70)``

     - .. image:: ../../images/user/geometry_validity/geometry_make_valid_linework_10.svg
       
       ``POLYGON ((90 90,90 10,10 10,10 90,90 90),(30 30,70 30,70 70,30 70,30 30))``

     - .. image:: ../../images/user/geometry_validity/geometry_make_valid_structure_10.svg
       
       ``MULTIPOLYGON (((90 90,90 10,10 10,10 90,90 90)))``


Overlapping MultiPolygons
^^^^^^^^^^^^^^^^^^^^^^^^^


.. list-table:: 
   :header-rows: 1
   :widths: 1 1 1

   * - Input geometry and result of ``gdal vector check-geometry``
     - ``gdal vector make-valid --method=linework``
     - ``gdal vector make-valid --method=structure``

    
   * - .. image:: ../../images/user/geometry_validity/geometry_source_11.svg

       Input geometry: ``MULTIPOLYGON (((10 90,60 90,60 10,10 10,10 90)),((90 80,90 20,40 20,40 80,90 80)))``

       Error message: ``Self-intersection``

       Error geometry: ``MULTIPOINT (60 80)``

     - .. image:: ../../images/user/geometry_validity/geometry_make_valid_linework_11.svg
       
       ``MULTIPOLYGON (((90 80,90 20,60 20,60 80,90 80)),((60 10,10 10,10 90,60 90,60 80,40 80,40 20,60 20,60 10)))``

     - .. image:: ../../images/user/geometry_validity/geometry_make_valid_structure_11.svg
       
       ``MULTIPOLYGON (((60 90,60 80,90 80,90 20,60 20,60 10,10 10,10 90,60 90)))``


MultiPolygon with multiple overlapping Polygons
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^


.. list-table:: 
   :header-rows: 1
   :widths: 1 1 1

   * - Input geometry and result of ``gdal vector check-geometry``
     - ``gdal vector make-valid --method=linework``
     - ``gdal vector make-valid --method=structure``

    
   * - .. image:: ../../images/user/geometry_validity/geometry_source_12.svg

       Input geometry: ``MULTIPOLYGON (((90 90,90 30,30 30,30 90,90 90)),((20 20,20 80,80 80,80 20,20 20)),((10 10,10 70,70 70,70 10,10 10)))``

       Error message: ``Self-intersection``

       Error geometry: ``MULTIPOINT (70 20)``

     - .. image:: ../../images/user/geometry_validity/geometry_make_valid_linework_12.svg
       
       ``MULTIPOLYGON (((10 10,10 70,20 70,20 20,70 20,70 10,10 10)),((30 80,30 70,20 70,20 80,30 80)),((90 90,90 30,80 30,80 80,30 80,30 90,90 90)),((70 20,70 30,80 30,80 20,70 20)),((30 30,30 70,70 70,70 30,30 30)))``

     - .. image:: ../../images/user/geometry_validity/geometry_make_valid_structure_12.svg
       
       ``MULTIPOLYGON (((10 70,20 70,20 80,30 80,30 90,90 90,90 30,80 30,80 20,70 20,70 10,10 10,10 70)))``


MultiPolygon with two adjacent Polygons
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^


.. list-table:: 
   :header-rows: 1
   :widths: 1 1 1

   * - Input geometry and result of ``gdal vector check-geometry``
     - ``gdal vector make-valid --method=linework``
     - ``gdal vector make-valid --method=structure``

    
   * - .. image:: ../../images/user/geometry_validity/geometry_source_13.svg

       Input geometry: ``MULTIPOLYGON (((10 90,50 90,50 10,10 10,10 90)),((90 80,90 20,50 20,50 80,90 80)))``

       Error message: ``Self-intersection``

       Error geometry: ``MULTIPOINT (50 80)``

     - .. image:: ../../images/user/geometry_validity/geometry_make_valid_linework_13.svg
       
       ``POLYGON ((50 80,90 80,90 20,50 20,50 10,10 10,10 90,50 90,50 80))``

     - .. image:: ../../images/user/geometry_validity/geometry_make_valid_structure_13.svg
       
       ``MULTIPOLYGON (((50 90,50 80,90 80,90 20,50 20,50 10,10 10,10 90,50 90)))``


Single-point polygon
^^^^^^^^^^^^^^^^^^^^


.. list-table:: 
   :header-rows: 1
   :widths: 1 1 1

   * - Input geometry and result of ``gdal vector check-geometry``
     - ``gdal vector make-valid --method=linework``
     - ``gdal vector make-valid --method=structure``

    
   * - .. image:: ../../images/user/geometry_validity/geometry_source_14.svg

       Input geometry: ``POLYGON ((70 30))``

       Error message: ``point array must contain 0 or >1 elements``

       Error geometry: ``MULTIPOINT (70 30)``

     -
 
     -
 

Two-point polygon
^^^^^^^^^^^^^^^^^


.. list-table:: 
   :header-rows: 1
   :widths: 1 1 1

   * - Input geometry and result of ``gdal vector check-geometry``
     - ``gdal vector make-valid --method=linework``
     - ``gdal vector make-valid --method=structure``

    
   * - .. image:: ../../images/user/geometry_validity/geometry_source_15.svg

       Input geometry: ``POLYGON ((10 10,90 90))``

       Error message: ``Points of LinearRing do not form a closed linestring``

       Error geometry: ``MULTIPOINT (10 10)``

     -
 
     -
 

Non-closed ring
^^^^^^^^^^^^^^^


.. list-table:: 
   :header-rows: 1
   :widths: 1 1 1

   * - Input geometry and result of ``gdal vector check-geometry``
     - ``gdal vector make-valid --method=linework``
     - ``gdal vector make-valid --method=structure``

    
   * - .. image:: ../../images/user/geometry_validity/geometry_source_16.svg

       Input geometry: ``POLYGON ((10 10,90 10,90 90,10 90))``

       Error message: ``Points of LinearRing do not form a closed linestring``

       Error geometry: ``MULTIPOINT (10 10)``

     -
 
     -
 