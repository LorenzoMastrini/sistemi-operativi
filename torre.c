#include <stdio.h>
#include <time.h>
#include <signal.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <semaphore.h>
#include "aeroporto.h"

//variabili per visualizzare l'orario corrente (globali perché necessarie anche nell'eventuale handler)
time_t timet;
struct tm *pTm;
char s[256];



int pid_aereo_decollato;   //memorizza il pid dell'aereo che ha finito il decollo e ha inviato la notifica
void handler_fine_decollo (int iSignalCode, siginfo_t *si, void *p) {
	pid_aereo_decollato = si -> si_pid;
	time(&timet);
	pTm = localtime(&timet);
	sprintf(s, "%02d:%02d:%02d", pTm->tm_hour, pTm->tm_min, pTm->tm_sec);
	printf("[%s] Torre: ricevimento notifica di fine decollo dall'aereo %d\n", &s, si->si_value);
	return;
}



int torre() {
	int valore_pista1;
	int valore_pista2;
	struct sigaction sa_torre;
	int fd;   //file descriptor per la pipe delle richieste di autorizzazione al decollo
	struct tDati stDati;

	//mascheramento del segnale inviato dall'hangar (fine creazione aerei) che termina prima della torre
	int signum;
	sigset_t sigset_torre;
	sigemptyset(&sigset_torre);
	sigaddset(&sigset_torre, SIGRTMIN);
	sigprocmask(SIG_BLOCK, &sigset_torre, NULL);   //il mascheramento serve a bloccare il segnale e renderlo pendente

	struct sigaction sa_torre2;
	int signum2;
	sigset_t sigset_torre2;
	sigemptyset(&sigset_torre2);
	sigaddset(&sigset_torre2, SIGRTMIN + 1);
	//sigprocmask(SIG_BLOCK, &sigset_torre2, NULL);

	//codice dedicato alla visualizzazione dell'orario corrente
	time(&timet);
	pTm = localtime(&timet);
	sprintf(s, "%02d:%02d:%02d", pTm->tm_hour, pTm->tm_min, pTm->tm_sec);
	printf("[%s] Torre: creazione processo torre\n", &s);

	//creazione semafori per la gestione delle due piste
	sem_t *pista1;
	sem_t *pista2;
	if ((pista1 = sem_open("/pista1", O_CREAT, S_IRWXU|S_IRGRP|S_IROTH, 1)) == SEM_FAILED) {
		perror("Torre: Errore in sem_open (1)");
		return 1;
	}
	if ((pista2 = sem_open("/pista2", O_CREAT, S_IRWXU|S_IRGRP|S_IROTH, 1)) == SEM_FAILED) {
		perror("Torre: Errore in sem_open (2)");
		return 1;
	}

	//named pipe per ricevere le richieste di decollo dagli aerei
	if(mkfifo(MYPIPE_PATH, S_IRWXU | S_IRGRP | S_IXGRP) == -1) {   //creazione pipe
		perror("Errore in mkfifo\n");
		fprintf(stderr, "errno = %d\n", errno);
		exit(1);
	}

	fd = open(MYPIPE_PATH, O_RDONLY);
	stDati.s[0] = '\0';
	while(strcmp(stDati.s, "exit")) {   //fa un ciclo in più rispetto al numero di aerei perché legge la stringa "exit"
		if(read(fd, &stDati, sizeof(stDati)) == -1) {
			perror("Torre: Errore in read");
			return 1;
		}
		if (strcmp(stDati.s, "exit")) {   //non stampa l'ultima stringa iniviata (exit)
			time(&timet);
			pTm = localtime(&timet);
			sprintf(s, "%02d:%02d:%02d", pTm->tm_hour, pTm->tm_min, pTm->tm_sec);
			printf("[%s] Torre: %s ricevuta dall'aereo %d\n", &s, stDati.s, stDati.iCod);
		} else {
			break;   //esce dal ciclo quando arriva la stringa "exit" dalla pipe (altrimenti rimane in attesa all'infinito)
		}

		//invio autorizzazione di decollo
		//int valore_pista1;
		//int valore_pista2;
		sem_getvalue(pista1, &valore_pista1);
		sem_getvalue(pista2, &valore_pista2);
		int p1_occupata_da;
		int p2_occupata_da;

		if (valore_pista1 == 1) {   //decollo aereo dalla pista 1
			sem_wait(pista1);
			sem_getvalue(pista1, &valore_pista1);
			printf("%d (sem_wait pista 1)\n", valore_pista1);
			p1_occupata_da = stDati.pid_aereo;

			time(&timet);
			pTm = localtime(&timet);
			sprintf(s, "%02d:%02d:%02d", pTm->tm_hour, pTm->tm_min, pTm->tm_sec);
			printf("[%s] Torre: invio autorizzazione di decollo all'aereo %d dalla pista 1\n", &s, stDati.iCod);
			if (kill(stDati.pid_aereo, SIGRTMIN + 1) == -1) {
				perror("Torre: Errore in kill");
				return 1;
			}
		} else if (valore_pista2 == 1) {   //decollo aereo dalla pista 2
			sem_wait(pista2);
			sem_getvalue(pista2, &valore_pista2);
			printf("%d (sem_wait pista 2)\n", valore_pista2);
			p2_occupata_da = stDati.pid_aereo;

			time(&timet);
			pTm = localtime(&timet);
			sprintf(s, "%02d:%02d:%02d", pTm->tm_hour, pTm->tm_min, pTm->tm_sec);
			printf("[%s] Torre: invio autorizzazione di decollo all'aereo %d dalla pista 2\n", &s, stDati.iCod);
			if (kill(stDati.pid_aereo, SIGRTMIN + 1) == -1) {
				perror("Torre: Errore in kill");
				return 1;
			}
		}

		//se entrambe le piste sono occupate, attende la fine del decollo di uno dei due aerei che le impegnano
		sem_getvalue(pista1, &valore_pista1);
		sem_getvalue(pista2, &valore_pista2);
		if (valore_pista1 == 0 && valore_pista2 == 0) {
			memset(&sa_torre2, '\0', sizeof(struct sigaction));
			sa_torre2.sa_handler = handler_fine_decollo;
			sa_torre2.sa_flags = SA_SIGINFO;
			if (sigaction(SIGRTMIN + 1, &sa_torre2, NULL) == -1) {
				perror("Torre: Errore in sigaction");
				return 1;
			}
			pause();   //attesa fine decollo

			if (p1_occupata_da == pid_aereo_decollato) {
				sem_post(pista1);   //libera la pista 1
				sem_getvalue(pista1, &valore_pista1);
			  printf("%d (sem_post pista 1)\n", valore_pista1);
			} else if (p2_occupata_da == pid_aereo_decollato) {
				sem_post(pista2);   //libera la pista 2
				sem_getvalue(pista2, &valore_pista2);
			  printf("%d (sem_post pista 2)\n", valore_pista2);
			}
		}
	}


	// Gestione con handler di SIGRTMIN (notifica di fine decollo aerei dall'hangar)
	memset(&sa_torre, '\0', sizeof(struct sigaction));
	sa_torre.sa_handler = SIG_IGN;   //o handler_torre
	if (sigaction(SIGRTMIN, &sa_torre, NULL) == -1) {
		perror("Torre: Errore in sigaction");
		return 1;
	}
	sigwait(&sigset_torre, &signum);
	time(&timet);
	pTm = localtime(&timet);
	sprintf(s, "%02d:%02d:%02d", pTm->tm_hour, pTm->tm_min, pTm->tm_sec);
	printf("[%s] Torre: ricevimento notifica 'non ci sono più aerei'\n", &s);

	//uscendo in anticipo dall'ultimo ciclo del while, non esegue l'ultima sem_post, quindi la faccio manualmente
	sem_getvalue(pista1, &valore_pista1);
	sem_getvalue(pista2, &valore_pista2);
	if (valore_pista1 == 0) {   //ovviamente libero solo l'ultima pista occupata
		sem_post(pista1);
	} else if (valore_pista2 == 0){
		sem_post(pista2);
	}
	sem_getvalue(pista1, &valore_pista1);
	sem_getvalue(pista2, &valore_pista2);
	printf("valore pista1 prima di terminare: %d\n", valore_pista1);
	printf("valore pista2 prima di terminare: %d\n", valore_pista2);







	sem_close(pista1);
	sem_close(pista2);
	sem_unlink("/pista1");
	sem_unlink("/pista2");

	unlink(MYPIPE_PATH);

	time(&timet);
	pTm = localtime(&timet);
	sprintf(s, "%02d:%02d:%02d", pTm->tm_hour, pTm->tm_min, pTm->tm_sec);
	printf("[%s] Torre: terminazione processo torre\n", &s);

	return 0;
}
