CREATE EXTENSION pg_cheat_funcs;

\pset null '(null)'

SELECT pg_signal_process(pg_postmaster_pid(), 'HUP');

SET pg_cheat_funcs.scheduling_priority TO 1;
SELECT pg_get_priority(pg_backend_pid());
SELECT pg_set_priority(pg_backend_pid(), 2);
SHOW pg_cheat_funcs.scheduling_priority;

SELECT pg_eucjp('xa4', 'xa2');

SELECT pg_set_next_xid(next_xid) = next_xid FROM pg_xid_assignment();

BEGIN TRANSACTION ISOLATION LEVEL REPEATABLE READ;
SELECT pg_refresh_snapshot();
SELECT 1;
SELECT pg_refresh_snapshot();
COMMIT;

DO $$
DECLARE
  orig_cleanup_age text := current_setting('vacuum_defer_cleanup_age');
BEGIN
  PERFORM pg_advance_vacuum_cleanup_age(99);
  IF current_setting('vacuum_defer_cleanup_age') <> '-99' THEN
    RAISE WARNING 'could not advance vacuum cleanup age properly.';
  END IF;
  PERFORM pg_advance_vacuum_cleanup_age();
  IF current_setting('vacuum_defer_cleanup_age') <> orig_cleanup_age THEN
    RAISE NOTICE 'could not reset vacuum cleanup age properly.';
  END IF;
END;
$$ LANGUAGE plpgsql;

SELECT to_octal(num) FROM generate_series(1, 10) num;
SELECT to_octal(2147483647::integer);
SELECT to_octal(9223372036854775807::bigint);

SELECT pg_text_to_hex('PostgreSQL');
SELECT pg_hex_to_text('506f737467726553514c');
SELECT pg_hex_to_text(upper('506f737467726553514c'));

SELECT pg_backend_start_time() = backend_start FROM pg_stat_get_activity(pg_backend_pid());

SELECT substring(pg_tablespace_version_directory(), 1, 3);

BEGIN;
SELECT pg_advisory_xact_lock(111);
SELECT pg_advisory_xact_lock(222);
SELECT pg_advisory_xact_lock_shared(333);
SELECT pg_advisory_xact_lock(444, 555);
SELECT pg_advisory_xact_lock_shared(666, 777);
SELECT classid, objid, objsubid, mode FROM pg_locks WHERE locktype = 'advisory' ORDER BY classid, objid, objsubid, mode;
SELECT pg_advisory_xact_unlock(111);
SELECT classid, objid, objsubid, mode FROM pg_locks WHERE locktype = 'advisory' ORDER BY classid, objid, objsubid, mode;
SELECT pg_advisory_xact_unlock_shared(666, 777);
SELECT classid, objid, objsubid, mode FROM pg_locks WHERE locktype = 'advisory' ORDER BY classid, objid, objsubid, mode;
COMMIT;
SELECT classid, objid, objsubid, mode FROM pg_locks WHERE locktype = 'advisory' ORDER BY classid, objid, objsubid, mode;

SET pg_cheat_funcs.exit_on_segv TO on;
SELECT pg_segmentation_fault(true);
SET pg_cheat_funcs.exit_on_segv TO off;

DROP EXTENSION pg_cheat_funcs;
