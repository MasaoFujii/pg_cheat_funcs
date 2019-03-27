CREATE EXTENSION pg_cheat_funcs;

\pset null '(null)'

SELECT pg_stat_print_memory_context();

SELECT pg_signal_process(pg_postmaster_pid(), 'HUP');

SET pg_cheat_funcs.scheduling_priority TO 1;
SELECT pg_get_priority(pg_backend_pid());
SELECT pg_set_priority(pg_backend_pid(), 2);
SHOW pg_cheat_funcs.scheduling_priority;

SELECT pg_eucjp('xa4', 'xa2');

SELECT pg_set_next_xid(next_xid) = next_xid FROM pg_xid_assignment();

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

SELECT pg_file_fsync('global');
SELECT pg_file_fsync('global/pg_control');

DROP EXTENSION pg_cheat_funcs;
