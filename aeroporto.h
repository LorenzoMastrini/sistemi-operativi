#define NUMERO_AEREI 5
#define MYPIPE_PATH "/tmp/aereo_torre"

struct tDati {
	int iCod;
	char s[64];
	pid_t pid_aereo;
};
