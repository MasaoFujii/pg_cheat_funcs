CREATE EXTENSION pg_cheat_funcs;

\pset null '(null)'

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

DROP EXTENSION pg_cheat_funcs;
