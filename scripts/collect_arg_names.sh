#!/bin/sh
# Collect all argument names
gdal --json-usage | jq -r '.. | (.input_arguments?, .output_arguments?) | .[]? | .name?' | LC_ALL=C sort -u
