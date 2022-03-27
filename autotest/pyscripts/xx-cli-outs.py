'''Build a dictionary of program name and expected console results when called with no parameters.

The expected results must be in pre-generated in files named:

    cli_outs/scriptname.stdout  # normal
    cli_outs/scriptname.stderr  # errors

These output files can be generated with:

    gdalcompare 1>gdalcompare.stdout 2>gdalcompare.stderr

The dict:

    >>> responses['gdalcompare.stdout']
    'Usage: gdalcompare.py [-sds] <golden_file> <new_file>\n'

    >>> responses['gdalcompare.stderr']
    ''
'''
from pathlib import Path

here = Path(r"C:\Users\Matt\code\gdal\autotest\pyscripts")

utils = ['gdal2tiles', 'gdal2xyz', 'gdal_calc', 'gdal_edit', 'gdal_fillnodata',
    'gdal_merge', 'gdal_pansharpen', 'gdal_polygonize', 'gdal_proximity',
    'gdal_retile', 'gdal_sieve', 'gdalattachpct', 'gdalcompare', 'gdalmove',
    'ogrmerge', 'pct2rgb', 'rgb2pct']

outputs_dir = "cli_outs"

# program: [stdout, stderr]


def get_utils_responses():
    responses = {}
    for x in utils:
        data_out = Path.joinpath(here, outputs_dir, f"{x}.stdout")
        data_err = Path.joinpath(here, outputs_dir, f"{x}.stderr")
        with open(data_out) as f:
            responses[x] = [f.read()]
        with open(data_err) as f:
            responses[x] = [responses[x][0], f.read()]
    return responses

responses = get_utils_responses()

# for x in utils:
#     pytest.param(x {
#         "returncode": 1,
#         "stdout": responses[f"{x}.stdout"],
#         "stderr": responses[f"{x}.stderr"],
#         })
