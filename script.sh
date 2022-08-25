#!/bin/bash

gcc -o aeroporto aeroporto.c torre.c hangar.c aereo.c -pthread

if ! test -x ./aeroporto
then
	echo "Eseguibile non trovato"
	exit 1
fi

echo -e "Script (pid = $$): Inizio applicazione aeroporto\n"

./aeroporto
PID_APPLICAZIONE=$!

wait $PID_APPLICAZIONE

exit 0
