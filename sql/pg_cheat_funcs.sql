CREATE EXTENSION pg_cheat_funcs;

\pset null '(null)'

SELECT pg_stat_print_memory_context();

SELECT pg_signal_process(pg_postmaster_pid(), 'HUP');

SELECT chr(88) = pg_chr(88);
SELECT chr(12354) = pg_chr(12354);
SELECT pg_chr(57000);
SELECT pg_chr(2000000);

DROP EXTENSION pg_cheat_funcs;
