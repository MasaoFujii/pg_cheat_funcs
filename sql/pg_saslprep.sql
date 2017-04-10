CREATE EXTENSION pg_cheat_funcs;

\pset null '(null)'

SELECT pg_saslprep(chr(8470) || chr(9316) || '4' || chr(12892));

DROP EXTENSION pg_cheat_funcs;
