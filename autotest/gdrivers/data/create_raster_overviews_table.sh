#!/bin/bash
# Create table raster overviews in "public" schema

DATABASE_NAME=gisdb
HOST_NAME=localhost
USER_NAME=postgres	# Need a user with permission to create tables here!

CREATE_OV_TABLE="CREATE TABLE public.raster_overviews (o_table_catalog character varying(256) NOT NULL, o_table_schema character varying(256) NOT NULL, o_table_name character varying(256) NOT NULL, o_column character varying(256) NOT NULL, r_table_catalog character varying(256) NOT NULL, r_table_schema character varying(256) NOT NULL, r_table_name character varying(256) NOT NULL, r_column character varying(256) NOT NULL, out_db boolean NOT NULL, overview_factor integer NOT NULL, CONSTRAINT raster_overviews_pk PRIMARY KEY (o_table_catalog, o_table_schema, o_table_name, o_column, overview_factor));"

psql -h $HOST_NAME -U $USER_NAME -d $DATABASE_NAME -c "$CREATE_OV_TABLE"
psql -h $HOST_NAME -U $USER_NAME -d $DATABASE_NAME -c "ALTER TABLE public.raster_overviews OWNER TO gis;"
