drop table mt
\g

create table mt (id integer, cost decimal(7,2), code c, name char(12), longname vchar(100), vname varchar(100), distance float4, smallval smallint, lvname long varchar)
\g

insert into mt (id, cost, code, name, longname, vname, distance, smallval, lvname)
  values (17,1.23,'x', 'xyz', 'The xyz corp', 'The xyz corp', 12332.772, 112, 
          'longvchar')
\g

insert into mt (id, cost, code, name, longname, vname, distance, smallval, lvname)
  values (18,3.22,'x', 'abc', 'ABC Cleaners', 'ABC Cleaning', 22.1232, 32000,
          'very longggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggg text')
\g


drop table oge_point
\g
create table oge_point (id integer, geom point)
\g
insert into oge_point values (1,'(100,200)')
\g
insert into oge_point values (2,'(300.5,400)')
\g

drop table oge_ipoint
\g
create table oge_ipoint (id integer, geom ipoint)
\g
insert into oge_ipoint values (1,'(100,200)')
\g
insert into oge_ipoint values (2,'(300,400)')
\g


drop table oge_box
\g

create table oge_box (id integer, geom box)
\g

insert into oge_box values (1, '((0,0), (10,20))' )
\g

insert into oge_box values (2, '((30.5,50), (34,55))' )
\g


drop table oge_ibox
\g

create table oge_ibox (id integer, geom ibox)
\g

insert into oge_ibox values (1, '((0,0), (10,20))' )
\g

insert into oge_ibox values (2, '((30,50), (34,55))' )
\g


drop table oge_lseg
\g

create table oge_lseg (id integer, geom lseg)
\g

insert into oge_lseg values (1, '((0,0), (10,20))' )
\g

insert into oge_lseg values (2, '((30.5,50), (34,55))' )
\g


drop table oge_line
\g

create table oge_line (id integer, geom line(4))
\g

insert into oge_line values
(1, '((0,0), (10,20), (70,-20))' )
\g

insert into oge_line values
(2, '((30.5,50), (34,55), (100,200), (125,200))' )
\g


drop table oge_iline
\g

create table oge_iline (id integer, geom iline(4))
\g

insert into oge_iline values
(1, '((0,0), (10,20), (70,-20))' )
\g

insert into oge_iline values
(2, '((30,50), (34,55), (100,200), (125,200))' )
\g


drop table oge_longline
\g

create table oge_longline (id integer, geom long line)
\g

insert into oge_longline values
(1, '((0,0), (10,20), (70,-20))' )
\g

insert into oge_longline values 
(2, '((30.5,50), (34,55), (100,200), (125,200))' )
\g



drop table oge_polygon
\g
create table oge_polygon (id integer, geom polygon(4))
\g
insert into oge_polygon values 
(1, '((0,0), (10,20), (70,-20))' )
\g
insert into oge_polygon values 
(2, '((30.5,50), (20,200), (100,200), (125,15.5))' )
\g

drop table oge_ipolygon
\g
create table oge_ipolygon (id integer, geom ipolygon(4))
\g
insert into oge_ipolygon values 
(1, '((0,0), (10,20), (70,-20))' )
\g
insert into oge_ipolygon values 
(2, '((30,50), (20,200), (100,200), (125,15))' )
\g

drop table oge_longpolygon
\g
create table oge_longpolygon (id integer, geom long polygon)
\g
insert into oge_longpolygon values 
(1, '((0,0), (10,20), (70,-20))' )
\g
insert into oge_longpolygon values 
(2, '((30.5,50), (20,200), (100,200), (125,15.5))' )
\g




