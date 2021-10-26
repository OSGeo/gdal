#!/bin/sh
GOLD_WIDTH=250
SILVER_WIDTH=200
BRONZE_WIDTH=150

inkscape --export-png=images/sponsors/logo-microsoft.png --export-width=$GOLD_WIDTH images/sponsors/logo-microsoft.svg
inkscape --export-png=images/sponsors/logo-planet.png --export-width=$GOLD_WIDTH images/sponsors/logo-planet.svg
inkscape --export-png=images/sponsors/logo-maxar.png --export-width=$GOLD_WIDTH images/sponsors/logo-maxar.svg
# Given the square shape of th AWS logo compared to the other ones, we have to reduce its width a bit so it doesn't look bigger overall
inkscape --export-png=images/sponsors/logo-aws.png --export-width=225 images/sponsors/AWS_logo_RGB.svg
inkscape --export-png=images/sponsors/logo-esri.png --export-width=$GOLD_WIDTH images/sponsors/logo-esri.svg

inkscape --export-png=images/sponsors/logo-google.png --export-width=$SILVER_WIDTH images/sponsors/logo-google.svg
inkscape --export-png=images/sponsors/logo-safe.png --export-width=$SILVER_WIDTH images/sponsors/logo-safe.svg

inkscape --export-png=images/sponsors/logo-koordinates.png --export-width=$BRONZE_WIDTH images/sponsors/logo-koordinates.svg
inkscape --export-png=images/sponsors/logo-frontiersi.png --export-width=$BRONZE_WIDTH images/sponsors/logo-FrontierSI.svg
inkscape --export-png=images/sponsors/logo-aerometrex.png --export-width=$BRONZE_WIDTH images/sponsors/logo-aerometrex.svg
