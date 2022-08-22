#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include "aeroporto.h"

int aereo(int numero_aereo, pid_t pid_torre);



int hangar(pid_t pid_torre) {
	pid_t pid[NUMERO_AEREI];

	//codice dedicato alla visualizzazione dell'orario corrente
	char s[256];
	time_t timet;
	time(&timet);
	struct tm *pTm = localtime(&timet);
	sprintf(s, "%02d:%02d:%02d", pTm->tm_hour, pTm->tm_min, pTm->tm_sec);

	printf("[%s] Hangar: creazione processo hangar\n", &s);

	for(int n = 0; n < NUMERO_AEREI; n++) {
		sleep(2);   //attesa tra la creazione di due aerei diversi

		time(&timet);
		pTm = localtime(&timet);
		sprintf(s, "%02d:%02d:%02d", pTm->tm_hour, pTm->tm_min, pTm->tm_sec);
		printf("[%s] Hangar: creazione processo aereo %d\n", &s, n + 1);

		pid[n] = fork();

		if (pid[n] < 0) {   //errore
			fprintf(stderr, "Fork fallita");
			return 1;
		}
		else if (pid[n] == 0) { //processi figli
			return aereo(n, pid_torre);
		}
	}
	time(&timet);
	pTm = localtime(&timet);
	sprintf(s, "%02d:%02d:%02d", pTm->tm_hour, pTm->tm_min, pTm->tm_sec);
	printf("[%s] Hangar: fine creazione aerei; inizio attesa\n", &s);

	for (int n = 0; n < NUMERO_AEREI; n++) {
			waitpid(pid[n], NULL, 0);   //attesa terminazione di tutti i processi aerei
			/*automaticamente viene atteso il decollo dell'ultimo aereo, dopo il quale
			l'hangar notifica alla torre che gli aerei sono teminati*/
	}

	//notifica "non ci sono più aerei da far decollare"
	time(&timet);
	pTm = localtime(&timet);
	sprintf(s, "%02d:%02d:%02d", pTm->tm_hour, pTm->tm_min, pTm->tm_sec);
	printf("[%s] Hangar: non ci sono più aerei da far decollare\n", &s);
	if (kill(pid_torre, SIGRTMIN) == -1) {
		perror("Hangar: Errore in kill");
		return 1;
	}

	time(&timet);
	pTm = localtime(&timet);
	sprintf(s, "%02d:%02d:%02d", pTm->tm_hour, pTm->tm_min, pTm->tm_sec);
	printf("[%s] Hangar: terminazione del processo hangar\n", &s);

	return 0;
}
