CREATE EXTENSION pg_cheat_funcs;

\pset null '(null)'

SELECT name FROM pg_stat_get_memory_context() WHERE parent IS NULL;

DROP EXTENSION pg_cheat_funcs;
