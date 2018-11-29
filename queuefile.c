#include <stdio.h> //stdin/out
#include <fcntl.h> //pliki
#include <stdlib.h> //exit()
#include <err.h> // errx
#include <unistd.h> //fork()
#include <signal.h> //sygnały
#include <string.h> //strlen
/* Pamiec dzielona */
#include <sys/shm.h>
#include <sys/ipc.h>
/* Kolejki */
#include <sys/types.h>
#include <sys/msg.h>
#include <errno.h>

#define SIZE 128 //regulacja rozmiaru bufora

int    qid;

struct mymsgbuf {
	long    mtype;          /* typ wiadomości */
	int     request;        /* numer żądania danego działania */
	char 		c;
} msg;

struct mymsgbuf buf;

int open_queue( key_t keyval ) {
	int     qid;
          
	if((qid = msgget( keyval, IPC_CREAT | 0660 )) == -1)
		return(-1);
          
	return(qid);
}

int send_message( int qid, struct mymsgbuf *qbuf ){
	int result, length;
  
	/* lenght jest rozmiarem struktury minus sizeof(mtype) */
	length = sizeof(struct mymsgbuf) - sizeof(long);        
  
	if((result = msgsnd( qid, qbuf, length, 0)) == -1)
          return(-1);
          
	return(result);
}

int remove_queue( int qid ){
	if( msgctl( qid, IPC_RMID, 0) == -1)
		return(-1);
          
	return(0);
}

int read_message( int qid, long type, struct mymsgbuf *qbuf ){
	int     result, length;
  
    /* lenght jest rozmiarem struktury minus sizeof(mtype) */
	length = sizeof(struct mymsgbuf) - sizeof(long);        
  
	if((result = msgrcv( qid, qbuf, length, type,  0)) == -1)
		return(-1);
          
	return(result);
}

int peek_message( int qid, long type ){
	int result;
  
	if((result = msgrcv( qid, NULL, 0, type,  IPC_NOWAIT)) == -1)
		if(errno == E2BIG)
			return(1);
          
	return(0);
}



/* ZMIENNE GLOBALNE */

	/* Zmienne indeksu, klucza i wskaznikow na pamiec dzielona */
	int shmid; // indeks pamieci dzielonej
	key_t key; // klucz pamieci dzielonej
	
	char *shm; // wskaznik na poczatek pamieci dzielonej
	char *semafor; //wskaźnik na lokalizację semafora wstrzymywania i wznawiania działania procesów
	int *p; //wskaźnik na wartość pidu matki
	int *p1; //wskaźnik na wartość pidu potomka#1
	int *p2; //wskaźnik na wartość pidu potomka#2
	int *p3; //wskaźnik na wartość pidu potomka#3
	
/* MECHANIZM SYGNAŁÓW */

void signals_handler(int signal)
{
	if(signal==3) //zamykanie procesów(SIGQUIT)
	{
		printf("\nZAMKNIECIE PROGRAMU\n");
		if(getpid()==*p1)
		{
			kill(*p2, SIGTERM); //zamyka drugi proces
			kill(*p3, SIGTERM); //zamyka trzeci proces
		}
		else if(getpid()==*p2)
		{
			kill(*p1, SIGTERM); //zamyka pierwszy proces
			kill(*p3, SIGTERM); //zamyka trzeci proces
		}
		else if(getpid()==*p3)
		{
			kill(*p1, SIGTERM); //zamyka pierwszy proces
			kill(*p2, SIGTERM); //zamyka drugi proces
		}

		remove("test"); //usuwamy plik
		remove_queue(qid);
		kill(*p, SIGTERM); //zamyka proces macierzysty
		kill(getpid(), SIGTERM); //zamyka sam siebie
	}
	else if(signal==20) //zatrzymywanie(SIGTSTP)
	{
		if(*semafor=='@')
		{
			printf("\nSTOP\n");
			*semafor='!';
			if(getpid()==*p1)
			{
				kill(*p2, SIGUSR1); //zatrzymuje drugi proces
				kill(*p3, SIGUSR1); //zatrzymuje trzeci proces
			}
			else if(getpid()==*p2)
			{
				kill(*p1, SIGUSR1); //zatrzymuje pierwszy proces
				kill(*p3, SIGUSR1); //zatrzymuje trzeci proces
			}
			else if(getpid()==*p3)
			{
				kill(*p1, SIGUSR1); //zatrzymuje pierwszy proces
				kill(*p2, SIGUSR1); //zatrzymuje drugi proces
			}
		}
	}
	else if(signal==2) //wznawianie (SIGINT)
	{
		if(*semafor=='!')
		{
			printf("\nSTART\n");
			*semafor='@';
			if(getpid()==*p1)
			{
				kill(*p2, SIGUSR1); //wznawia drugi proces
				kill(*p3, SIGUSR1); //wznawia trzeci proces
			}
			else if(getpid()==*p2)
			{
				kill(*p1, SIGUSR1); //wznawia pierwszy proces
				kill(*p3, SIGUSR1); //wznawia trzeci proces
			}
			else if(getpid()==*p3)
			{
				kill(*p1, SIGUSR1); //wznawia pierwszy proces
				kill(*p2, SIGUSR1); //wznawia drugi proces
			}	
		}
	}
	else if(signal==10) //przelacznik od semafora (SIGUSR1)
	{
		if(*semafor=='@') *semafor='!';
		else if(*semafor=='!') *semafor='@';
	}
	else puts("\nAVAILABLE SIGNALS:\n-SIGQUIT to terminate all processes\n-SIGTSTP to suspend all processes\n-SIGINT to resume all processes");
}

/* PORÓWNYWARKA CIAGOW ZNAKOW */

int cmp(char *tab1,char *tab2)
{
	int k=1;
	int i=0;
	int len=0;
	
	len=strlen(tab1)-1;
	if(strlen(tab2)>strlen(tab1)-1) len=strlen(tab2);
	for(i=0;i<len;i++) if(tab1[i]!=tab2[i]) k=0;
	if(strlen(tab1)!=strlen(tab2)+1) return 0;
	return k;
}

int main(void)
{
	int state=0;
	
	key_t  msgkey;
	
	/* tworzymy wartość klucza IPC */
	msgkey = ftok(".", 'm');
  
	/* otwieramy/tworzymy kolejkę */
	if(( qid = open_queue( msgkey)) == -1) {
		perror("Otwieranie_kolejki");
		exit(1);
	}
	
	
	/* MENU */
	printf("MENU:\n1 - tryb interaktywny\n2 - tryb odczytu danych z pliku\n3 - tryb odczytu danych z /dev/urandom");
	while(state<1 || state>3)
	{
		scanf("%d",&state);
		if(state==1) printf("tryb interaktywny");
		else if(state==2) printf("tryb odczytu danych z pliku, podaj lokalizację:");
		else if(state==3) printf("tryb odczytu danych z /dev/urandom");
		else printf("zla wartosc");
	}
		
		
	/* Tworzymy klucz dla pamieci dzielonej. */
	if ((key = ftok(".", 'A')) == -1) errx(1, "Blad tworzenia klucza!");
	else printf("\nklucz stworzony");
	
	/* Tworzymy segment pamieci dzielonej o wielkości 18B */
	/*		            	    ________________________________________________________________________________________________________________________________
	   18Bajtów: |1|1|4|4|4|4| |deskryptor pliku *-write #-read|semafor procesów @-otwarty !-zamknięty|pid matki|pid 1 potomnego|pid 2 potomnego|pid 3 potomnego|
				                ————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
	 */
	
	if ((shmid = shmget(key, 18, IPC_CREAT | 0666)) < 0) errx(2, "Blad tworzenia segmentu pamieci dzielonej!");
	else printf("\nsegment pamieci stworzony");

	/* Dolaczamy pamiec dzielona do naszej przestrzeni danych. */
    if ((shm = shmat(shmid, NULL, 0)) == (char *) -1) errx(3, "Blad przylaczania pamieci dzielonej!");
	else printf("\npamiec pomyslnie dolaczona do przestrzeni danych\n");
	
	*shm='#';
	
	/* ustawienie wskaźników */
	semafor=shm+1; //na drugim bajcie pamięci dzielonej
	*semafor='@'; //domyślnie otwarty na start
	
	p=shm+2; //3B-6B
	*p=getpid(); //uzupełnienie tablicy pidów
	
	p1=shm+6; //7-10B
	
	p2=shm+10; //11B-14B
	
	p3=shm+14; //15B-18B
	
	FILE *K1; //wskaźnik na początek pliku łącza danych K1
	
	/* PROCES #1 WCZYTYWANIE Z STDIN */
	if(!fork())
	{
		//uzupełnienie tablicy pidów:
		*p1=getpid();
		/* INICJALIZACJA */
		FILE *pFile; //wskaźnik na początek pliku z danymi	
		
		// Odbieranie sygnałów
		signal(SIGUSR1, signals_handler); //switch
		signal(SIGINT, signals_handler); //wznawianie
		signal(SIGTSTP, signals_handler); //zatrzymywanie
		signal(SIGQUIT, signals_handler); //zamykanie
				
		if(state==1)
		{
			char string[SIZE];
			char keyquit[5]="exit";
			char keypause[6]="pause";
			char keyunpause[8]="unpause";
			int i=0;
			//*shm='*';
			
			do
			{
				if(*semafor=='@')
				{
					//scanf("%s",&string);
					fgets(string,SIZE,stdin);
					for(i=0;i<strlen(string)-1;i++)
					{
							msg.mtype   = 1;
							msg.request = 1;
							msg.c=string[i];
							// Wysylanie
							if((send_message( qid, &msg )) == -1) {
					perror("Wysylanie");
					exit(1);
				}
					}
					i=0;
					//printf("%d",LengthOfString(string));
					//printf("%d",strlen(string));
					//printf("test");
					if(cmp(string,keypause)==1)
					{
						kill(getpid(),SIGTSTP);
						while(cmp(string,keyunpause)==0 && cmp(string,keyquit)==0)
						{
							puts("#paused");
							fgets(string,SIZE,stdin);
						}
						kill(getpid(),SIGINT);
					}
				}
			}while(cmp(string,keyquit)!=1);
			puts("#exit");
		}
		else if(state==2)
		{
			//*shm='*';
			char string[SIZE];
			char urandom[13]="/dev/urandom ";
			scanf("%s",&string);
			pFile = fopen (string,"r");
			if(cmp(urandom,string)) goto JUMP;
			while(1)
			{
				if(*semafor=='@')
				{
					msg.mtype   = 1;
					msg.request = 1;
					if((msg.c=fgetc(pFile))==EOF) break; //koniec pliku
					
					if((send_message( qid, &msg )) == -1) {
					perror("Wysylanie");
					//exit(1);
				}
				}
			}
			sleep(1);
		}
		else if(state==3)
		{
			JUMP:
			pFile = fopen ("/dev/urandom","r");
			//*shm='*';
			while(1)
			{
				if(*semafor=='@')
				{
						msg.mtype   = 1;
						msg.request = 1;
						msg.c = fgetc(pFile);;
						// Wysylanie
						if((send_message( qid, &msg )) == -1) {
						perror("Wysylanie");
						//exit(1);
						
						}
				}
			}
		}
		kill(getpid(),SIGQUIT);
	
	}
	/* PROCES #2 KONWERSJA DO POSTACI HEXADECYMALNEJ */
	if(!fork())
	{
		//uzupełnienie tablicy pidów:
		*p2=getpid();
		*shm='*';
		
		// Odbieranie sygnałów
		signal(SIGUSR1, signals_handler); //switch
		signal(SIGINT, signals_handler); //wznawianie
		signal(SIGTSTP, signals_handler); //zatrzymywanie
		signal(SIGQUIT, signals_handler); //zamykanie
		
		while(1)
		{
			if(*semafor=='@')
			{
					if(*shm=='*')
					{
						// Odbieranie
						buf.mtype   = 1;
						buf.request = 1;
						read_message(qid, msg.mtype, &buf);
						// Wysylanie
						K1=fopen("test", "w");
						fprintf(K1, "%c", buf.c); /* Zapisujemy do pliku */
						fclose(K1); /* Zamykamy plik */
						*shm='#';
					}
			}
		}
	}
	/* PROCES #3 WYPROWADZA W OSOBNYCH WIERSZACH PO 15 DANE Z PROCESU 2 */
	if(!fork())
	{
		//uzupełnienie tablicy pidów:
		*p3=getpid();
		/* INICJALIZACJA */
		char c;
		int i=0;

		
		// Odbieranie sygnałów
		signal(SIGUSR1, signals_handler); //switch
		signal(SIGINT, signals_handler); //wznawianie
		signal(SIGTSTP, signals_handler); //zatrzymywanie
		signal(SIGQUIT, signals_handler); //zamykanie

		while(1)
		{
			if(*semafor=='@')
			{
				// Odbieranie
				if(*shm=='#')
				{
					K1=fopen("test","r");
					fscanf(K1, "%c", &c); /* Wczytujemy z pliku */
					fclose(K1); /* Zamykamy plik */
					//*shm='$';
					if(c<255 && c>0)
					{
						if(c<16)printf("0"); //wymuszenie dwuznakowości dla wartości poniżej 16
						printf("%X ",c); //drukowanie wartości wielkimi znakami oddzielonymi spacją
						fflush(stdout);
						i++;
					}
					if(i==15)
					{
						printf("\n");
						fflush(stdout);
						i=0;
					}
					*shm='*';
				}
			}
		}
	}
	//sleep(1);
	printf("Pidy procesów wynoszą:\nmatka:%d\npotomny#1:%d\npotomny#2:%d\npotomny#3:%d\n",*p,*p1,*p2,*p3);
	fflush(stdout);
	shmdt(shm); // Odlaczamy sie od pamieci dzielonej.
	shmctl(shmid, IPC_RMID, NULL); // Kasujemy segment pamieci dzielonej
	while(1){}
	exit(0);
}
