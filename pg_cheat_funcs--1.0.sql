-- complain if script is sourced in psql, rather than via CREATE EXTENSION
\echo Use "CREATE EXTENSION pg_cheat_funcs" to load this file. \quit

/* pg_stat_get_memory_context function is available only in 9.6 or later */
DO $$
DECLARE
    pgversion TEXT;
BEGIN
    SELECT current_setting('server_version_num') INTO pgversion;
    IF pgversion >= '90600' THEN
        CREATE FUNCTION pg_stat_get_memory_context(OUT name text,
            OUT parent text,
            OUT level integer,
            OUT total_bytes bigint,
            OUT total_nblocks bigint,
            OUT free_bytes bigint,
            OUT free_chunks bigint,
            OUT used_bytes bigint)
        AS 'MODULE_PATHNAME'
        LANGUAGE C STRICT VOLATILE;
        REVOKE ALL ON FUNCTION pg_stat_get_memory_context() FROM PUBLIC;
    END IF;
END;
$$;

CREATE FUNCTION pg_stat_print_memory_context()
RETURNS void
AS 'MODULE_PATHNAME'
LANGUAGE C STRICT VOLATILE;
REVOKE ALL ON FUNCTION pg_stat_print_memory_context() FROM PUBLIC;

CREATE FUNCTION pg_signal_process(integer, text)
RETURNS void
AS 'MODULE_PATHNAME'
LANGUAGE C STRICT VOLATILE;
REVOKE ALL ON FUNCTION pg_signal_process(integer, text) FROM PUBLIC;

/* pg_xlogfile_name function is available only in 9.4 or later */
DO $$
DECLARE
    pgversion TEXT;
BEGIN
    SELECT current_setting('server_version_num') INTO pgversion;
    IF pgversion >= '90400' THEN
        -- Use VOLATILE because the heading 8 digits of returned WAL file name
        -- (i.e., represents the timeline) can be changed during recovery.
        CREATE FUNCTION pg_xlogfile_name(pg_lsn, boolean)
        RETURNS text
        AS 'MODULE_PATHNAME'
        LANGUAGE C STRICT VOLATILE;
    END IF;
END;
$$;

CREATE FUNCTION pg_set_next_xid(xid)
RETURNS xid
AS 'MODULE_PATHNAME'
LANGUAGE C STRICT VOLATILE;
REVOKE ALL ON FUNCTION pg_set_next_xid(xid) FROM PUBLIC;

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
LANGUAGE C STRICT STABLE;
REVOKE ALL ON FUNCTION pg_show_primary_conninfo() FROM PUBLIC;

CREATE FUNCTION pg_postmaster_pid()
RETURNS integer
AS 'MODULE_PATHNAME'
LANGUAGE C STRICT STABLE;

CREATE FUNCTION pg_file_write_binary(text, bytea)
RETURNS bigint
AS 'MODULE_PATHNAME'
LANGUAGE C STRICT VOLATILE;
REVOKE ALL ON FUNCTION pg_file_write_binary(text, bytea) FROM PUBLIC;

CREATE FUNCTION pg_chr(integer)
RETURNS text
AS 'MODULE_PATHNAME'
LANGUAGE C STRICT IMMUTABLE;

/* PGLZ compression functions are available only in 9.5 or later */
DO $$
DECLARE
    pgversion TEXT;
BEGIN
    SELECT current_setting('server_version_num') INTO pgversion;
    IF pgversion >= '90500' THEN
        CREATE FUNCTION pglz_compress(text)
        RETURNS bytea
        AS 'MODULE_PATHNAME', 'pglz_compress_text'
        LANGUAGE C STRICT IMMUTABLE;
    END IF;
END;
$$;

DO $$
DECLARE
    pgversion TEXT;
BEGIN
    SELECT current_setting('server_version_num') INTO pgversion;
    IF pgversion >= '90500' THEN
        CREATE FUNCTION pglz_compress_bytea(bytea)
        RETURNS bytea
        AS 'MODULE_PATHNAME', 'pglz_compress_bytea'
        LANGUAGE C STRICT IMMUTABLE;
    END IF;
END;
$$;

DO $$
DECLARE
    pgversion TEXT;
BEGIN
    SELECT current_setting('server_version_num') INTO pgversion;
    IF pgversion >= '90500' THEN
        CREATE FUNCTION pglz_decompress(bytea)
        RETURNS text
        AS 'MODULE_PATHNAME', 'pglz_decompress_text'
        LANGUAGE C STRICT IMMUTABLE;
    END IF;
END;
$$;

DO $$
DECLARE
    pgversion TEXT;
BEGIN
    SELECT current_setting('server_version_num') INTO pgversion;
    IF pgversion >= '90500' THEN
        CREATE FUNCTION pglz_decompress_bytea(bytea)
        RETURNS bytea
        AS 'MODULE_PATHNAME', 'pglz_decompress_bytea'
        LANGUAGE C STRICT IMMUTABLE;
    END IF;
END;
$$;
