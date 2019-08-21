import os

# https://tech.signavio.com/2017/managing-sphinx-redirects


template="""<html>
  <head>
    <meta http-equiv="refresh" content="1; url=%s" />
    <script>
      window.location.href = "%s"
    </script>
  </head>
</html>"""


def gather_redirects():
    output = {}

    def fetch(path, d, prefix):
        files = os.listdir(path)
        for f in files:
            driver = f.split('.')[0]
            if driver not in 'gpkg':
                entry = {'%s_%s.html'% (prefix,driver) : os.path.join(path,driver)+'.html' }
                d.update(entry)

    fetch('./drivers/raster', output, 'frmt')
    fetch('./drivers/vector', output, 'drv')

    output.update({ 'drv_geopackage.html' : os.path.join('./drivers/vector', 'gpkg') + '.html' })
    output.update({ 'geopackage_aspatial.html' : os.path.join('./driver/vector', 'aspatial') + '.html' })
    output.update({ 'drv_geopackage_raster.html' : os.path.join('./drivers/raster', 'gpkg') + '.html' })
    output.update({ 'ogr_feature_style.html' : os.path.join('./user', 'ogr_feature_style') + '.html' })
    output.update({ 'gdal_virtual_file_systems.html' : os.path.join('./user', 'virtual_file_systems') + '.html' })
    output.update({ 'ogr_formats.html' : os.path.join('./drivers/vector', 'index') + '.html' })
    output.update({ 'formats_list.html' : os.path.join('./drivers/raster', 'index') + '.html' })
    output.update({ 'frmt_various.html' : os.path.join('./drivers/raster', 'index') + '.html' })
    output.update({ 'gdal_vrttut.html' : os.path.join('./drivers/raster', 'vrt') + '.html' })

    raster_tools = [
        'gdal2tiles',
        'gdaladdo',
        'gdalbuildvrt',
        'gdal_calc',
        'gdalcompare',
        'gdal-config',
        'gdal_contour',
        'gdaldem',
        'gdal_edit',
        'gdal_fillnodata',
        'gdal_grid',
        'gdalinfo',
        'gdallocationinfo',
        'gdalmanage',
        'gdal_merge',
        'gdalmove',
        'gdal_pansharpen',
        'gdal_polygonize',
        'gdal_proximity',
        'gdal_rasterize',
        'gdal_retile',
        'gdal_sieve',
        'gdalsrsinfo',
        'gdaltindex',
        'gdaltransform',
        'gdal_translate',
        'gdalwarp',
        'nearblack',
        'rgb2pct',
        'pct2rgb',
    ]
    for utility in raster_tools:
        output.update({ utility + '.html' : os.path.join('./programs/', utility) + '.html' })

    vector_tools = [
        'ogr2ogr',
        'ogrinfo',
        'ogrlineref',
        'ogrmerge',
        'ogrtindex',
    ]
    for utility in vector_tools:
        output.update({ utility + '.html' : os.path.join('./programs/', utility) + '.html' })

    gnm_tools = [
        'gnmanalyse',
        'gnmmanage',
    ]
    for utility in gnm_tools:
        output.update({ utility + '.html' : os.path.join('./programs/', utility) + '.html' })


    return output




from shutil import copyfile
# copy legacy redirects
def copy_legacy_redirects(app, docname): # Sphinx expects two arguments
    if app.builder.name == 'html':
        for key in app.config.redirect_files:
            src = key
            tgt = app.config.redirect_files[key]
            html = template % (tgt, tgt)
            with open(os.path.join(app.outdir, src), 'wb') as f:
                f.write(html.encode('utf-8'))
                f.close()



def setup(app):
    app.add_config_value('redirect_files', {}, 'html')
    app.connect('build-finished', copy_legacy_redirects)
    return { 'parallel_read_safe': False, 'parallel_write_safe': True }
