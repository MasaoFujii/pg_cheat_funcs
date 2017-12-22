CREATE EXTENSION pg_cheat_funcs;

\pset null '(null)'

PREPARE stmt AS SELECT * FROM pg_database WHERE datname = $1;
SELECT * FROM pg_cached_plan_source('stmt');

DROP EXTENSION pg_cheat_funcs;
