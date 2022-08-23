#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include "aeroporto.h"

int torre();
int hangar();



int main() {
	unlink(MYPIPE_PATH);   //elimina la pipe nel caso in cui il programma fosse terminato in anticipo, prima della unlink su torre.c
	//elimina i semafori nel caso in cui il programma fosse terminato in anticipo, prima delle unlink su torre.c
	sem_unlink("/pista1");
	sem_unlink("/pista2");

	pid_t pid[2];   //memorizza i pid dei figli (torre e hangar)

	for(int n = 0; n < 2; n++) {
		pid[n] = fork();

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

	return 0;
}
