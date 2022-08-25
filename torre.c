#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <semaphore.h>
#include "aeroporto.h"



int torre() {
	//mascheramento del segnale SIGRTMIN + 1 inviato dagli aerei (fine decollo)
	struct sigaction sa_torre;
	sigset_t sigset_torre;
	sigemptyset(&sigset_torre);
	sigaddset(&sigset_torre, SIGRTMIN + 1);
	sigprocmask(SIG_BLOCK, &sigset_torre, NULL);
	//per gestire il segnale; lo faccio qui perché lo uso sia nel while sia fuori
	memset(&sa_torre, '\0', sizeof(struct sigaction));
	sa_torre.sa_handler = SIG_IGN;
	sa_torre.sa_flags = SA_SIGINFO;
	if (sigaction(SIGRTMIN + 1, &sa_torre, NULL) == -1) {
		perror("Torre: Errore in sigaction");
		return 1;
	}
	int pid_aereo_decollato;   //memorizza il pid dell'aereo che ha finito il decollo e ha inviato la notifica

	//mascheramento del segnale SIGRTMIN + 2 inviato dall'hangar (fine creazione aerei) che termina prima della torre
	struct sigaction sa_torre2;
	int signum2;
	sigset_t sigset_torre2;
	sigemptyset(&sigset_torre2);
	sigaddset(&sigset_torre2, SIGRTMIN + 2);
	sigprocmask(SIG_BLOCK, &sigset_torre2, NULL);   //il mascheramento serve a bloccare il segnale e renderlo pendente

	//variabili per visualizzare l'orario corrente
	char s[256];
	time_t timet;
	time(&timet);
	struct tm *pTm = localtime(&timet);
	sprintf(s, "%02d:%02d:%02d", pTm->tm_hour, pTm->tm_min, pTm->tm_sec);

	printf("[%s] Torre: creazione processo torre\n", s);

	//crezione named pipe per ricevere le richieste di decollo dagli aerei
	if(mkfifo(MYPIPE_PATH, S_IRWXU | S_IRGRP | S_IXGRP) == -1) {   //creazione pipe
		perror("Errore in mkfifo\n");
		fprintf(stderr, "errno = %d\n", errno);
		exit(1);
	}
	int fd;   //file descriptor per la pipe delle richieste di decollo
	struct tDati stDati;   //struttura dati passata con la pipe

	//apertura semafori per le due piste
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
	//memorizzano il valore dei semafori delle piste come interi
	int valore_pista1;
	int valore_pista2;

	//memorizzano i pid degli aerei che occupano le piste
	int p1_occupata_da;
	int p2_occupata_da;
	fd = open(MYPIPE_PATH, O_RDONLY);   //apertura pipe in sola lettura
	stDati.s[0] = '\0';
	while(strcmp(stDati.s, "exit")) {   //inizia un ciclo in più rispetto al numero di aerei perché legge la stringa "exit"
		if(read(fd, &stDati, sizeof(stDati)) == -1) {
			perror("Torre: Errore in read");
			return 1;
		}

		if (strcmp(stDati.s, "exit")) {   //se legge "exit" non fa la stampa ed esce dal ciclo
			sleep(1);   //fa si che non si scambino di ordine le printf degli aerei e della torre di invio/ricezione della notifica di decollo
			time(&timet);
			pTm = localtime(&timet);
			sprintf(s, "%02d:%02d:%02d", pTm->tm_hour, pTm->tm_min, pTm->tm_sec);
			printf("[%s] Torre: %s dell'aereo %d ricevuta\n", s, stDati.s, stDati.iCod);
		} else {
			break;   //esce quando legge la stringa "exit"
		}

		//se entrambe le piste sono occupate, attende la fine del decollo di uno dei due aerei che le impegnano
		sem_getvalue(pista1, &valore_pista1);
		sem_getvalue(pista2, &valore_pista2);
		if (valore_pista1 == 0 && valore_pista2 == 0) {
			siginfo_t info;   //salva varie informazioni inviate dall'aereo, tra cui il pid e il suo numero

			if (sigwaitinfo(&sigset_torre, &info) == -1) {   //attesa fine decollo
				perror("Torre: Errore in pause");
				return 1;
			}
			pid_aereo_decollato = info.si_pid;
			time(&timet);
			pTm = localtime(&timet);
			sprintf(s, "%02d:%02d:%02d", pTm->tm_hour, pTm->tm_min, pTm->tm_sec);
			printf("[%s] Torre: ricevimento notifica di fine decollo dall'aereo %d\n", s, info.si_value.sival_int);   //info.si_value.sival_int è un intero (serve ad evitare un warning)

			//libera la pista occupata dall'aereo appena decollato
			if (p1_occupata_da == pid_aereo_decollato) {
				p1_occupata_da = 0;
				sem_post(pista1);   //libera la pista 1
			} else if (p2_occupata_da == pid_aereo_decollato) {
				p2_occupata_da = 0;
				sem_post(pista2);   //libera la pista 2
			}
		}

		//invio autorizzazione di decollo (con segnale SIGRTMIN inviato con kill()) in base alla pista libera
		sem_getvalue(pista1, &valore_pista1);
		sem_getvalue(pista2, &valore_pista2);
		if (valore_pista1 == 1) {   //decollo aereo dalla pista 1
			sem_wait(pista1);
			p1_occupata_da = stDati.pid_aereo;   //salva il pid dell'aereo appena decollato dalla pista 1

			time(&timet);
			pTm = localtime(&timet);
			sprintf(s, "%02d:%02d:%02d", pTm->tm_hour, pTm->tm_min, pTm->tm_sec);
			printf("[%s] Torre: invio autorizzazione di decollo all'aereo %d dalla pista 1\n", s, stDati.iCod);
			if (kill(stDati.pid_aereo, SIGRTMIN) == -1) {
				perror("Torre: Errore in kill");
				return 1;
			}
		} else if (valore_pista2 == 1) {   //decollo aereo dalla pista 2
			sem_wait(pista2);
			p2_occupata_da = stDati.pid_aereo;   //salva il pid dell'aereo appena decollato dalla pista 2

			time(&timet);
			pTm = localtime(&timet);
			sprintf(s, "%02d:%02d:%02d", pTm->tm_hour, pTm->tm_min, pTm->tm_sec);
			printf("[%s] Torre: invio autorizzazione di decollo all'aereo %d dalla pista 2\n", s, stDati.iCod);
			if (kill(stDati.pid_aereo, SIGRTMIN) == -1) {
				perror("Torre: Errore in kill");
				return 1;
			}
		}
	}
	close(fd);   //finito di leggere dalla pipe, chiudo il file descriptor

	sem_getvalue(pista1, &valore_pista1);
	sem_getvalue(pista2, &valore_pista2);
	//IN TEORIA, per come è costruito il while, non entro MAI nei primi due if perché ho sempre entrambe le piste occupate dagli ultimi due aerei; li ho lasciati per sicurezza
	if (valore_pista1 == 0 && valore_pista2 == 1) {
		siginfo_t info;
		if (sigwaitinfo(&sigset_torre, &info) == -1) {   //attesa fine decollo dalla sola pista 1
			perror("Torre: Errore in sigwatinfo");
			return 1;
		}
		time(&timet);
		pTm = localtime(&timet);
		sprintf(s, "%02d:%02d:%02d", pTm->tm_hour, pTm->tm_min, pTm->tm_sec);
		printf("[%s] Torre: ricevimento notifica di fine decollo dall'aereo %d\n", s, info.si_value.sival_int);
		sem_post(pista1);
	} else if (valore_pista1 == 1 && valore_pista2 == 0) {
		siginfo_t info;
		if (sigwaitinfo(&sigset_torre, &info) == -1) {   //attesa fine decollo dalla sola pista 2
			perror("Torre: Errore in pause");
			return 1;
		}
		time(&timet);
		pTm = localtime(&timet);
		sprintf(s, "%02d:%02d:%02d", pTm->tm_hour, pTm->tm_min, pTm->tm_sec);
		printf("[%s] Torre: ricevimento notifica di fine decollo dall'aereo %d\n", s, info.si_value.sival_int);
		sem_post(pista2);
	} else if (valore_pista1 == 0 && valore_pista2 == 0) {   //attesa fine decollo da entrambe le piste (stesso approccio usato nel while)
		//entra sempre (e solo) in questo if
		siginfo_t info1;
		siginfo_t info2;

		if (sigwaitinfo(&sigset_torre, &info1) == -1) {   //attesa fine decollo dalla pista 1
			perror("Torre: Errore in pause");
			return 1;
		}
		time(&timet);
		pTm = localtime(&timet);
		sprintf(s, "%02d:%02d:%02d", pTm->tm_hour, pTm->tm_min, pTm->tm_sec);
		printf("[%s] Torre: ricevimento notifica di fine decollo dall'aereo %d\n", s, info1.si_value.sival_int);
		sem_post(pista1);

		if (sigwaitinfo(&sigset_torre, &info2) == -1) {   //attesa fine decollo dalla pista 2
			perror("Torre: Errore in pause");
			return 1;
		}
		time(&timet);
		pTm = localtime(&timet);
		sprintf(s, "%02d:%02d:%02d", pTm->tm_hour, pTm->tm_min, pTm->tm_sec);
		printf("[%s] Torre: ricevimento notifica di fine decollo dall'aereo %d\n", s, info2.si_value.sival_int);
		sem_post(pista2);
	}

	// Gestione di SIGRTMIN + 2 (notifica di fine decollo aerei dall'hangar)
	memset(&sa_torre2, '\0', sizeof(struct sigaction));
	sa_torre2.sa_handler = SIG_IGN;
	if (sigaction(SIGRTMIN + 2, &sa_torre2, NULL) == -1) {
		perror("Torre: Errore in sigaction");
		return 1;
	}
	sigwait(&sigset_torre2, &signum2);
	time(&timet);
	pTm = localtime(&timet);
	sprintf(s, "%02d:%02d:%02d", pTm->tm_hour, pTm->tm_min, pTm->tm_sec);
	printf("[%s] Torre: ricevimento notifica 'non ci sono più aerei' dall'hangar\n", s);

	//chiusura dei semafori
	sem_close(pista1);
	sem_close(pista2);

	//eliminazione della pipe
	unlink(MYPIPE_PATH);

	time(&timet);
	pTm = localtime(&timet);
	sprintf(s, "%02d:%02d:%02d", pTm->tm_hour, pTm->tm_min, pTm->tm_sec);
	printf("[%s] Torre: terminazione processo torre\n", s);

	return 0;
}
