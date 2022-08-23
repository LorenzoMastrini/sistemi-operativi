#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <time.h>
#include <stdlib.h>
#include <signal.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "aeroporto.h"



//funzione per ottenere un numero casuale tra lMin e lMax
long get_random(long lMin, long lMax) {
  static int bSeed = 0;
  if(!bSeed) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    srandom(tv.tv_usec % 1000);
    bSeed = 1;
  }
  long l = random();
  return ((l % (lMax - lMin + 1L)) + lMin);
}



int aereo(int n, pid_t pid_torre) {
  struct tDati stDati;
  //codice dedicato alla visualizzazione dell'orario corrente
  char s[256];
	time_t timet;
	time(&timet);
	struct tm *pTm = localtime(&timet);
	sprintf(s, "%02d:%02d:%02d", pTm->tm_hour, pTm->tm_min, pTm->tm_sec);

  printf("[%s] Aereo: avvio processo aereo %d\n", &s, n + 1);

  //preparazione aereo
  long tempo_preparazione = get_random(3, 8);
  printf("[%s] Aereo: inizio preparazione aereo %d: tempo necessario = %ld secondi\n", &s, n + 1, tempo_preparazione);
  sleep(tempo_preparazione);
  
  //richiesta di autorizzazione per il decollo
  int fd = open(MYPIPE_PATH, O_WRONLY);
  stDati.iCod = n + 1;
  stDati.pid_aereo = getpid();
	strcpy(stDati.s, "Richiesta di decollo");
	if(write(fd, &stDati, sizeof(struct tDati)) == -1) {
		perror("Aereo: Errore in write");
		return 1;
	}
  time(&timet);
	pTm = localtime(&timet);
	sprintf(s, "%02d:%02d:%02d", pTm->tm_hour, pTm->tm_min, pTm->tm_sec);
	printf("[%s] Aereo: Richiesta di decollo inviata dall'areo %d\n", &s, n + 1);

  if(n + 1 == NUMERO_AEREI) {
    sleep(6);   //assicura che la exit sia inviata dopo che anche il penultimo aereo sia stato gestito (nel caso in cui l'ultimo ci metta meno a prepararsi)
		strcpy(stDati.s, "exit");
		if(write(fd, &stDati, sizeof(stDati)) == -1) {
			perror("Child: Errore in write");
			return 1;
		}
	}
  close(fd);

  //ricevuta autorizzazione al decollo
  struct sigaction sa_aereo;
  memset(&sa_aereo, '\0', sizeof(struct sigaction));
  sa_aereo.sa_handler = SIG_IGN;
  if (sigaction(SIGRTMIN, &sa_aereo, NULL) == -1) {
    perror("Aereo: Errore in sigaction");
    return 1;
  }
  int signum;
  sigset_t sigset_aereo;
  sigemptyset(&sigset_aereo);
  sigaddset(&sigset_aereo, SIGRTMIN);
  sigprocmask(SIG_BLOCK, &sigset_aereo, NULL);
  sigwait(&sigset_aereo, &signum);

  //decollo
  long tempo_decollo = get_random(5, 15);
  time(&timet);
	pTm = localtime(&timet);
	sprintf(s, "%02d:%02d:%02d", pTm->tm_hour, pTm->tm_min, pTm->tm_sec);
  printf("[%s] Aereo: inizio decollo aereo %d: tempo necessario = %ld secondi\n", &s, n + 1, tempo_decollo);
  sleep(tempo_decollo);
  //sleep(5);

  //invio notifica di fine decollo (e terminazione aereo)
  time(&timet);
	pTm = localtime(&timet);
	sprintf(s, "%02d:%02d:%02d", pTm->tm_hour, pTm->tm_min, pTm->tm_sec);
  printf("[%s] Aereo: fine decollo aereo %d\n", &s, n + 1);

  union sigval value;
  value.sival_int = n + 1;
  if (sigqueue(pid_torre, SIGRTMIN + 1, value) == -1) {
		perror("Aereo: Errore in sigqueue");
		return 1;
	}

  return 0;
}
