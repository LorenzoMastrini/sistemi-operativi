#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <semaphore.h>
#include <fcntl.h>
#include "aeroporto.h"

int torre();
int hangar();



int main() {
	unlink(MYPIPE_PATH);   //elimina la pipe nel caso in cui il programma fosse terminato in anticipo, prima della unlink su torre.c
	//elimina i semafori nel caso in cui il programma fosse terminato in anticipo, prima delle unlink su torre.c
	sem_unlink("/pista1");
	sem_unlink("/pista2");

	pid_t pid[2];   //memorizza i pid dei figli (torre e hangar)

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

	//creazione figli (torre e hangar) con fork()
	for(int n = 0; n < 2; n++) {
		pid[n] = fork();   //nel parent la fork() torna il pid del figlio creato

		if (pid[n] < 0) {   //errore
			fprintf(stderr, "Fork fallita");
			return 1;
		}
		else if (pid[n] == 0) { //processi figli
			if (n == 0) {   //processo torre
				return torre();
			}
			else if(n == 1) {   //processo hangar
				return hangar(pid[0]);   //passa all'hangar il pid della torre
			}
		}
	}

	waitpid(pid[1], NULL, 0);   //aspetta la terminazione del processo hangar
	waitpid(pid[0], NULL, 0);   //aspetta la terminazione del processo torre

	//eliminazione dei semafori
	sem_unlink("/pista1");
	sem_unlink("/pista2");
	
	return 0;
}
