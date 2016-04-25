-- complain if script is sourced in psql, rather than via CREATE EXTENSION
\echo Use "CREATE EXTENSION pg_cheat_funcs" to load this file. \quit

-- Use VOLATILE because the heading 8 digits of returned WAL file name
-- (i.e., represents the timeline) can be changed during recovery.
CREATE FUNCTION pg_xlogfile_name(pg_lsn, boolean)
RETURNS text
AS 'MODULE_PATHNAME'
LANGUAGE C STRICT VOLATILE;
