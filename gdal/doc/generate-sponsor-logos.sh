#!/bin/sh
PLATINIUM_WIDTH=250
GOLD_WIDTH=200
SILVER_WIDTH=150

inkscape --export-png=images/sponsors/logo-microsoft.png --export-width=$PLATINIUM_WIDTH images/sponsors/logo-microsoft.svg
inkscape --export-png=images/sponsors/logo-planet.png --export-width=$PLATINIUM_WIDTH images/sponsors/logo-planet.svg
inkscape --export-png=images/sponsors/logo-maxar.png --export-width=$PLATINIUM_WIDTH images/sponsors/logo-maxar.

inkscape --export-png=images/sponsors/logo-esri.png --export-width=$GOLD_WIDTH images/sponsors/logo-esri.svg
inkscape --export-png=images/sponsors/logo-google.png --export-width=$GOLD_WIDTH images/sponsors/logo-google.svg
inkscape --export-png=images/sponsors/logo-safe.png --export-width=$GOLD_WIDTH images/sponsors/logo-safe.svg

inkscape --export-png=images/sponsors/logo-koordinates.png --export-width=$SILVER_WIDTH images/sponsors/logo-koordinates.svg
inkscape --export-png=images/sponsors/logo-frontiersi.png --export-width=$SILVER_WIDTH images/sponsors/logo-FrontierSI.svg
