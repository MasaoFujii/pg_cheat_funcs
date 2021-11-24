CREATE EXTENSION pg_cheat_funcs;

\pset null '(null)'

BEGIN;
SELECT 1 FROM pg_current_xact_id();
SELECT pg_current_xact_id() = pg_xid_to_xid8(backend_xid)
  FROM pg_stat_activity WHERE pid = pg_backend_pid();
COMMIT;

DROP EXTENSION pg_cheat_funcs;
