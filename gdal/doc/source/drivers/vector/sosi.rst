.. _vector.sosi:

================================================================================
Norwegian SOSI Standard
================================================================================

.. shortname:: SOSI

This driver requires the FYBA library.

 poDriver->SetMetadataItem( GDAL_DMD_OPENOPTIONLIST,
"<OpenOptionList>"
"  <Option name='appendFieldsMap' type='string' description='Default is that all rows for equal field names will be appended in a feature, but with this parameter you select what field this should be valid for. With appendFieldsMap="f1&f2", Append will be done for field f1 and f2 using a comma as delimiter. This list can more complicacted check the source code.' default=''/>"
"</OpenOptionList>");
