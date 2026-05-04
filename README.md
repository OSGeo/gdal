# GDAL new command line interface: introduction and advanced topics

GDAL 3.11 introduced a new command line interface (CLI), simply called "gdal", supplementing the traditional well-known GDAL utilities (gdal_translate, ogr2ogr, etc.), to provide users with a more uniform, predictable and user-friendly experience.
This workshop will give the opportunity to participants to get a hands-one experience to discover the capabilities of the new CLI through a series of exercices, including how to leverage them from Python.

Are you a workshop participant or want to dive-in individually? Go to https://gdal-cli-workshop.github.io/

## Build HTML pages and PDF

```
python3 -m venv venv
source venv/bin/activate
python3 -m pip install -r requirements.txt
make html && make latexpdf && cp build/latex/gdal-cli-workshop.pdf build/html/
```
