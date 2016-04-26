-- complain if script is sourced in psql, rather than via CREATE EXTENSION
\echo Use "CREATE EXTENSION pg_cheat_funcs" to load this file. \quit

-- Use VOLATILE because the heading 8 digits of returned WAL file name
-- (i.e., represents the timeline) can be changed during recovery.
CREATE FUNCTION pg_xlogfile_name(pg_lsn, boolean)
RETURNS text
AS 'MODULE_PATHNAME'
LANGUAGE C STRICT VOLATILE;

CREATE FUNCTION pg_set_nextxid(xid)
RETURNS xid
AS 'MODULE_PATHNAME'
LANGUAGE C STRICT IMMUTABLE;
REVOKE ALL ON FUNCTION pg_set_nextxid(xid) FROM PUBLIC;

CREATE FUNCTION pg_xid_assignment(OUT next_xid xid,
    OUT oldest_xid xid,
    OUT xid_vac_limit xid,
    OUT xid_warn_limit xid,
    OUT xid_stop_limit xid,
    OUT xid_wrap_limit xid,
    OUT oldest_xid_db oid)
AS 'MODULE_PATHNAME'
LANGUAGE C STRICT VOLATILE;
REVOKE ALL ON FUNCTION pg_xid_assignment() FROM PUBLIC;

CREATE FUNCTION pg_show_primary_conninfo()
RETURNS text
AS 'MODULE_PATHNAME'
LANGUAGE C STRICT IMMUTABLE;
REVOKE ALL ON FUNCTION pg_show_primary_conninfo() FROM PUBLIC;
