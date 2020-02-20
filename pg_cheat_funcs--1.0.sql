-- complain if script is sourced in psql, rather than via CREATE EXTENSION
\echo Use "CREATE EXTENSION pg_cheat_funcs" to load this file. \quit

/* pg_stat_get_memory_context function is available only in 9.6 or later */
DO $$
DECLARE
    pgversion INTEGER;
BEGIN
    SELECT current_setting('server_version_num')::INTEGER INTO pgversion;
    IF pgversion >= 90600 THEN
        CREATE FUNCTION pg_stat_get_memory_context(OUT name text,
            OUT parent text,
            OUT level integer,
            OUT total_bytes bigint,
            OUT total_nblocks bigint,
            OUT free_bytes bigint,
            OUT free_chunks bigint,
            OUT used_bytes bigint)
        RETURNS SETOF record
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

/* pg_cached_plan_source function is available only in 9.2 or later */
DO $$
DECLARE
    pgversion INTEGER;
BEGIN
    SELECT current_setting('server_version_num')::INTEGER INTO pgversion;
    IF pgversion >= 90200 THEN
			 CREATE FUNCTION pg_cached_plan_source(IN stmt text,
			 OUT generic_cost double precision,
			 OUT total_custom_cost double precision,
			 OUT num_custom_plans integer,
			 OUT force_generic boolean,
			 OUT force_custom boolean)
			 RETURNS record
			 AS 'MODULE_PATHNAME'
			 LANGUAGE C STRICT VOLATILE;
    END IF;
END;
$$;

CREATE FUNCTION pg_signal_process(integer, text)
RETURNS void
AS 'MODULE_PATHNAME'
LANGUAGE C STRICT VOLATILE;
REVOKE ALL ON FUNCTION pg_signal_process(integer, text) FROM PUBLIC;

CREATE FUNCTION pg_get_priority(integer)
RETURNS integer
AS 'MODULE_PATHNAME'
LANGUAGE C STRICT VOLATILE;
REVOKE ALL ON FUNCTION pg_get_priority(integer) FROM PUBLIC;

CREATE FUNCTION pg_set_priority(integer, integer)
RETURNS void
AS 'MODULE_PATHNAME'
LANGUAGE C STRICT VOLATILE;
REVOKE ALL ON FUNCTION pg_set_priority(integer, integer) FROM PUBLIC;

CREATE FUNCTION pg_process_config_file()
RETURNS void
AS 'MODULE_PATHNAME'
LANGUAGE C STRICT VOLATILE;
REVOKE ALL ON FUNCTION pg_process_config_file() FROM PUBLIC;

/* Any functions using pg_lsn data type are available only in 9.4 or later */
DO $$
DECLARE
    pgversion INTEGER;
BEGIN
    SELECT current_setting('server_version_num')::INTEGER INTO pgversion;
    IF pgversion >= 90400 THEN
        -- Use VOLATILE because the heading 8 digits of returned WAL file name
        -- (i.e., represents the timeline) can be changed during recovery.
        CREATE FUNCTION pg_xlogfile_name(pg_lsn, boolean)
        RETURNS text
        AS 'MODULE_PATHNAME'
        LANGUAGE C STRICT VOLATILE;

        -- Aggregate functions for pg_lsn data type.
        CREATE FUNCTION pg_lsn_larger(pg_lsn, pg_lsn)
        RETURNS pg_lsn
        AS 'MODULE_PATHNAME'
        LANGUAGE C STRICT IMMUTABLE;

        CREATE FUNCTION pg_lsn_smaller(pg_lsn, pg_lsn)
        RETURNS pg_lsn
        AS 'MODULE_PATHNAME'
        LANGUAGE C STRICT IMMUTABLE;

        CREATE AGGREGATE max(pg_lsn)  (
            SFUNC = pg_lsn_larger,
            STYPE = pg_lsn,
            SORTOP = >
        );

        CREATE AGGREGATE min(pg_lsn)  (
            SFUNC = pg_lsn_smaller,
            STYPE = pg_lsn,
            SORTOP = <
        );
    END IF;
END;
$$;

/* pg_stat_get_syncrep_waiters function is available only in 9.4 or later */
DO $$
DECLARE
    pgversion INTEGER;
BEGIN
    SELECT current_setting('server_version_num')::INTEGER INTO pgversion;
    IF pgversion >= 90400 THEN
        CREATE FUNCTION pg_stat_get_syncrep_waiters(OUT pid integer,
        OUT wait_lsn pg_lsn,
        OUT wait_mode text)
        RETURNS SETOF record
        AS 'MODULE_PATHNAME'
        LANGUAGE C STRICT VOLATILE;
        REVOKE ALL ON FUNCTION pg_stat_get_syncrep_waiters() FROM PUBLIC;
    END IF;
END;
$$;

/* pg_wait_syncrep function is available only in 9.4 or later */
DO $$
DECLARE
    pgversion INTEGER;
BEGIN
    SELECT current_setting('server_version_num')::INTEGER INTO pgversion;
    IF pgversion >= 90400 THEN
        CREATE FUNCTION pg_wait_syncrep(pg_lsn)
        RETURNS void
        AS 'MODULE_PATHNAME'
        LANGUAGE C STRICT VOLATILE;
        REVOKE ALL ON FUNCTION pg_wait_syncrep(pg_lsn) FROM PUBLIC;
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

CREATE FUNCTION pg_set_next_oid(oid)
RETURNS oid
AS 'MODULE_PATHNAME'
LANGUAGE C STRICT VOLATILE;
REVOKE ALL ON FUNCTION pg_set_next_oid(oid) FROM PUBLIC;

CREATE FUNCTION pg_oid_assignment(OUT next_oid oid,
    OUT oid_count integer)
AS 'MODULE_PATHNAME'
LANGUAGE C STRICT VOLATILE;
REVOKE ALL ON FUNCTION pg_oid_assignment() FROM PUBLIC;

CREATE FUNCTION pg_advance_vacuum_cleanup_age(integer DEFAULT NULL)
RETURNS integer
AS 'MODULE_PATHNAME'
LANGUAGE C CALLED ON NULL INPUT VOLATILE;
REVOKE ALL ON FUNCTION pg_advance_vacuum_cleanup_age(integer) FROM PUBLIC;

CREATE FUNCTION pg_checkpoint(bool DEFAULT true, bool DEFAULT true,
    bool DEFAULT true)
RETURNS void
AS 'MODULE_PATHNAME'
LANGUAGE C STRICT VOLATILE;
REVOKE ALL ON FUNCTION pg_checkpoint(bool, bool, bool) FROM PUBLIC;

CREATE FUNCTION pg_promote(bool DEFAULT true)
RETURNS void
AS 'MODULE_PATHNAME'
LANGUAGE C STRICT VOLATILE;
REVOKE ALL ON FUNCTION pg_promote(bool) FROM PUBLIC;

CREATE FUNCTION pg_recovery_settings(OUT name text, OUT setting text)
RETURNS SETOF record
AS 'MODULE_PATHNAME'
LANGUAGE C STRICT VOLATILE;
REVOKE ALL ON FUNCTION pg_recovery_settings() FROM PUBLIC;

CREATE FUNCTION pg_show_primary_conninfo()
RETURNS text
AS 'MODULE_PATHNAME'
LANGUAGE C STRICT STABLE;
REVOKE ALL ON FUNCTION pg_show_primary_conninfo() FROM PUBLIC;

CREATE FUNCTION pg_postmaster_pid()
RETURNS integer
AS 'MODULE_PATHNAME'
LANGUAGE C STRICT STABLE;

CREATE FUNCTION pg_backend_start_time()
RETURNS timestamptz
AS 'MODULE_PATHNAME'
LANGUAGE C STRICT STABLE;

CREATE FUNCTION pg_list_relation_filepath(relation regclass)
    RETURNS SETOF text AS $$
DECLARE
    segcount bigint := 1;
    relationpath text;
    pathname text;
BEGIN
    relationpath := pg_relation_filepath(relation);
    RETURN NEXT relationpath;
    IF current_setting('server_version_num')::integer < 90500 THEN
	RETURN;
    END IF;
    LOOP
        pathname := relationpath || '.' || segcount;
        EXIT WHEN pg_stat_file(pathname, true) IS NULL;
        RETURN NEXT pathname;
        segcount := segcount + 1;
    END LOOP;
    RETURN;
END;
$$ LANGUAGE plpgsql STRICT VOLATILE;

CREATE FUNCTION pg_tablespace_version_directory()
RETURNS text
AS 'MODULE_PATHNAME'
LANGUAGE C STRICT STABLE;
REVOKE ALL ON FUNCTION pg_tablespace_version_directory() FROM PUBLIC;

CREATE FUNCTION pg_file_write_binary(text, bytea)
RETURNS bigint
AS 'MODULE_PATHNAME'
LANGUAGE C STRICT VOLATILE;
REVOKE ALL ON FUNCTION pg_file_write_binary(text, bytea) FROM PUBLIC;

CREATE FUNCTION pg_file_fsync(text)
RETURNS void
AS 'MODULE_PATHNAME'
LANGUAGE C STRICT VOLATILE;
REVOKE ALL ON FUNCTION pg_file_fsync(text) FROM PUBLIC;

CREATE FUNCTION to_octal(integer)
RETURNS text
AS 'MODULE_PATHNAME', 'to_octal32'
LANGUAGE C STRICT IMMUTABLE;

CREATE FUNCTION to_octal(bigint)
RETURNS text
AS 'MODULE_PATHNAME', 'to_octal64'
LANGUAGE C STRICT IMMUTABLE;

CREATE FUNCTION pg_text_to_hex(text)
RETURNS text
AS 'MODULE_PATHNAME'
LANGUAGE C STRICT IMMUTABLE;

CREATE FUNCTION pg_hex_to_text(text)
RETURNS text
AS 'MODULE_PATHNAME'
LANGUAGE C STRICT IMMUTABLE;

CREATE FUNCTION pg_chr(integer)
RETURNS text
AS 'MODULE_PATHNAME'
LANGUAGE C STRICT IMMUTABLE;

CREATE FUNCTION pg_utf8(integer)
RETURNS text
AS 'MODULE_PATHNAME', 'pg_chr'
LANGUAGE C STRICT IMMUTABLE;

CREATE FUNCTION pg_all_utf8(OUT code integer, OUT utf8 text)
RETURNS SETOF record AS
'SELECT * FROM
    (SELECT code, pg_utf8(code) utf8 FROM generate_series(0, 1114111) code) tmp
 WHERE utf8 IS NOT NULL'
LANGUAGE SQL IMMUTABLE;

CREATE FUNCTION pg_eucjp(bit(8), bit(8) DEFAULT 'x00', bit(8) DEFAULT 'x00')
RETURNS text
AS 'MODULE_PATHNAME'
LANGUAGE C STRICT IMMUTABLE;

CREATE FUNCTION pg_all_eucjp(OUT code1 text, OUT code2 text,
    OUT code3 text, OUT eucjp text) RETURNS SETOF record AS $$
BEGIN
    RETURN QUERY
        SELECT CASE WHEN c1 <= 15 THEN 'x0' ELSE 'x' END || to_hex(c1),
        'x00'::text, 'x00'::text, pg_eucjp(c1::bit(8))
        FROM generate_series(0, 127) c1;
    RETURN QUERY SELECT 'x8e'::text, 'x' || to_hex(c2), 'x00'::text,
        pg_eucjp('x8e', c2::bit(8)) FROM generate_series(161, 223) c2;
    RETURN QUERY SELECT 'x' || to_hex(c1), 'x' || to_hex(c2), 'x00'::text,
        pg_eucjp(c1::bit(8), c2::bit(8))
        FROM generate_series(161, 254) c1, generate_series(161, 254) c2;
    RETURN QUERY SELECT 'x8f'::text, 'x' || to_hex(c2), 'x' || to_hex(c3),
        pg_eucjp('x8f', c2::bit(8), c3::bit(8))
        FROM generate_series(161, 254) c2, generate_series(161, 254) c3;
END
$$ LANGUAGE plpgsql IMMUTABLE;

CREATE FUNCTION pg_euc_jp_to_utf8(integer, integer, cstring, internal, integer)
RETURNS void
AS 'MODULE_PATHNAME'
LANGUAGE C STRICT VOLATILE;

CREATE CONVERSION pg_euc_jp_to_utf8
    FOR 'EUC_JP' TO 'UTF8' FROM pg_euc_jp_to_utf8;

/* PGLZ compression functions are available only in 9.5 or later */
DO $$
DECLARE
    pgversion INTEGER;
BEGIN
    SELECT current_setting('server_version_num')::INTEGER INTO pgversion;
    IF pgversion >= 90500 THEN
        CREATE FUNCTION pglz_compress(text)
        RETURNS bytea
        AS 'MODULE_PATHNAME', 'pglz_compress_text'
        LANGUAGE C STRICT IMMUTABLE;
    END IF;
END;
$$;

DO $$
DECLARE
    pgversion INTEGER;
BEGIN
    SELECT current_setting('server_version_num')::INTEGER INTO pgversion;
    IF pgversion >= 90500 THEN
        CREATE FUNCTION pglz_compress_bytea(bytea)
        RETURNS bytea
        AS 'MODULE_PATHNAME', 'pglz_compress_bytea'
        LANGUAGE C STRICT IMMUTABLE;
    END IF;
END;
$$;

DO $$
DECLARE
    pgversion INTEGER;
BEGIN
    SELECT current_setting('server_version_num')::INTEGER INTO pgversion;
    IF pgversion >= 90500 THEN
        CREATE FUNCTION pglz_decompress(bytea)
        RETURNS text
        AS 'MODULE_PATHNAME', 'pglz_decompress_text'
        LANGUAGE C STRICT IMMUTABLE;
    END IF;
END;
$$;

DO $$
DECLARE
    pgversion INTEGER;
BEGIN
    SELECT current_setting('server_version_num')::INTEGER INTO pgversion;
    IF pgversion >= 90500 THEN
        CREATE FUNCTION pglz_decompress_bytea(bytea)
        RETURNS bytea
        AS 'MODULE_PATHNAME', 'pglz_decompress_bytea'
        LANGUAGE C STRICT IMMUTABLE;
    END IF;
END;
$$;

DO $$
DECLARE
    pgversion INTEGER;
BEGIN
    SELECT current_setting('server_version_num')::INTEGER INTO pgversion;
    IF pgversion >= 100000 THEN
        CREATE FUNCTION pg_saslprep(text)
        RETURNS text
        AS 'MODULE_PATHNAME', 'pg_cheat_saslprep'
        LANGUAGE C STRICT IMMUTABLE;
    END IF;
END;
$$;
