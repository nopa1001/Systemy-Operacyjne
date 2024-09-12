#include <stdio.h>
#include <sys/ipc.h>
#include <sys/wait.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/msg.h>
#include <sys/sem.h>
#include <string.h>
#include <signal.h>
#include <sys/shm.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>

#define wielkoscPakietu 256 //ILe danych pobierzemy max z wejscia

int pidP3, pidP2, pidP1;//PId p2 - matka go porownuje ze zrodlem sygnalu
int procesInicjalizujacy, kolejnyProces = 0; //ID matki, ID procesu, do ktorego bedzie wysylal sygnal. Np. Proces 1, bedzie mial tutaj ID procesu 2
int trwaPrzesylanie = 1, sygnalZakonczenia = 0, sygnalZatrzymania = 0; //
//trwaPrzesylanie - jezeli JESTESMY w menu (przesyl == 0) odrazu mozna konczyc prace
//sygnalZakonczenia - jezeli NIE JESTESMY w menu, po zakonczeniu obecnego przesylu mozna zakonczyc prace
//sygnalZatrzymania - jezeli ==1, to proces kreci sie w nieskonczonej petli i nic nie robi
int identyfikatorFifo; //ID Fifo
int idPipe1[2], idPipe2[2];

void wyslijDoGlownego(int sygnal) // P2 -> Matki po otrzymaniu sygnalu (Jak operator wysyla sygnal)
{
	kill(procesInicjalizujacy, sygnal);
}

static void zapisz_do_FIFO(int sygnal, siginfo_t *siginfo, void *context)
//Matka wysyla sygnal do P1 i zapisuje dane do fifo
{
	if((int)siginfo->si_pid == pidP3)  //Jezeli pid procesu wysylajacego == pidowi P2
	{
		char sig = (char) sygnal;
		kill(kolejnyProces, 12); //wyslijWiadomosc sygnal do P1 o odczytaniu fifo
		write(identyfikatorFifo, &sig, sizeof(char)); //Zapisz numer sygnalu
		write(identyfikatorFifo, &sig, sizeof(char)); 
		write(identyfikatorFifo, &sig, sizeof(char)); 
	}		
}

void zawiadom() //Jak proces odbierze sygnal -12
{
	char sig;
	read(identyfikatorFifo, &sig, sizeof(char)); //Odczytaj fifo	
	
	if(kolejnyProces != 0) //P3 ma tutaj 0
		kill(kolejnyProces, 12);  //Wyslij info do kolejnego, ze ma odczytac kolejke

	if(sig == 15) //jak koniec -SIGTERM
	{
		sygnalZakonczenia = 1; //oznacz ze mozna konczyc prace
		if(!trwaPrzesylanie) //Jak jestesmy w menu - jak nie przesyla
			exit(0); //WYjdz odrazu			
	}
	
	if(sig == 20) //stop - SIGTSTP
		sygnalZatrzymania = 1;	
		
	if(sig == 18) //start -SIGCONT
		sygnalZatrzymania = 0;
}
 

void proces1()
{
	identyfikatorFifo = open("fifo", O_RDONLY); //OTwieramy fifo na odczyt
	close(idPipe1[0]);
	
	struct sigaction zadanie; //Struktura sygnalu
	memset (&zadanie, 0, sizeof (zadanie)); //ZAalokuj pamiec
	zadanie.sa_handler = &zawiadom; //Wywolaj funkcje zawiadom
	zadanie.sa_flags = SA_RESTART; //Jak proces cos grzebal gdy dostal sygnal, to ma powtorzyc dzialanie
	sigaction(12, &zadanie, NULL);	//Wywolaj jak otrzymasz sygnal - 12
	
	sigset_t maska; 
	sigfillset(&maska);  //Tworze pelna maske
	sigdelset(&maska, 12); // Usuwa z maski sygnaly
	sigprocmask(SIG_BLOCK, &maska, NULL); //Blokuj sygnaly, ktore sa w masce*/
	//Hint
	//Niektorych sygnalow nie da sie zablokowac/przechwycic/zignorowac np SIGKILL -9	
		
	while(1)
	{
		trwaPrzesylanie = 0; // Oznaczamy ze jestesmy w menu		
		int tryb; //Zapisze tu tryb dzialania

		//Sprawdzamy czy user wprowadzil 1 / 2
		while(tryb != '1' && tryb != '2')
		{
			printf("TRYB:\n 1 - klawiatura 2 - plik\n");
			tryb = getc(stdin);		
		}
		if(tryb == '2') // Jeśli plik
		{
			char plik[60] = {0}; 
			int i = 1; // Do oznaczenia konca petli
				
			do{
				memset(plik, 0, sizeof(plik));
				fgets (plik, sizeof(plik), stdin); //Pobierz linijke z wejscia
				plik[strlen(plik) - 1] = 0; //Usuwamy enter z nazwy
				stdin = fopen(plik, "r"); // otworz plik jako wejscie
				if(stdin != NULL) // Jak jest co czytac (PLIK ISTNIEJE)
				{
					i = 0; //Wyjdz z petli
				}
				else //Nie ma takiego pliku
				{
					printf("Podaj nazwe:\n");			
					stdin = fopen("/dev/tty", "r"); // Zamień wejscie z powrotem na konsole
				}
			} while(i);
			tryb = 2;
		}
		else 
			tryb = 1;


		char buforWejsciowy[wielkoscPakietu]; // buforWejsciowy do zapisu danych z stdin
		
		do{						
			fgets(buforWejsciowy, sizeof(buforWejsciowy), stdin); //Czytaj  linijke	 z wejscia
				
			trwaPrzesylanie = 1;//Oznacz ze zaczal sie przesyl
			while(sygnalZatrzymania); //JAk P1 dostanie stop to tutaj stanie	
			//Jak tryb == 1 i user posłał .ENTER lub tryb == 2 i plik sie skonczyl
			if((tryb == 1 && buforWejsciowy[0] == '.' && buforWejsciowy[1] == '\n') || (tryb == 2 && feof(stdin))) //jak klawiatura i user nacisnal kropka enter
			{
				//Wyslij info do P2 ze koniec
				buforWejsciowy[0] = '\n';
				buforWejsciowy[1] = '\n';
				buforWejsciowy[2] = '\n';				
				write(idPipe1[1], buforWejsciowy, sizeof(buforWejsciowy));
				
				trwaPrzesylanie = 0; //Oznacz ze przejscie do menu
				
				if(tryb == 2)
					stdin = fopen("/dev/tty", "r"); //Jak czytalismy z pliku, zamienmy wejscie na konsole
				if(sygnalZakonczenia) //Jak dostalismy koniec to wyjdz 
					exit(0);
			}
			else if(!sygnalZatrzymania) // - powtorka bo proces moze dostac stop-a w oczekiwaniu w fgets
				write(idPipe1[1], buforWejsciowy, sizeof(buforWejsciowy));
				
		}while(trwaPrzesylanie);
	}
}

void proces2()
{
	char buforWejsciowy[wielkoscPakietu];
	close(idPipe1[1]);
	
	struct sigaction zadanie; //Struktura sygnalu
	memset (&zadanie, 0, sizeof (zadanie)); //ZAalokuj pamiec
	zadanie.sa_handler = &zawiadom; //Wywolaj funkcje zawiadom
	zadanie.sa_flags = SA_RESTART; //Jak proces cos grzebal gdy dostal sygnal, to ma powtorzyc dzialanie
	sigaction(12, &zadanie, NULL);	//Wywolaj jak otrzymasz sygnal -12 - 12
	
	sigset_t maska; 
	sigfillset(&maska);  //Tworze pelna maske
	sigdelset(&maska, 12); // Usuwa z maski sygnaly
	sigprocmask(SIG_BLOCK, &maska, NULL); //Blokuj sygnaly, ktore sa w masce*/
	//Hint
	//Niektorych sygnalow nie da sie zablokowac/przechwycic/zignorowac np SIGKILL -9	
	
	identyfikatorFifo = open("fifo", O_RDONLY); //OTwieramy identyfikatorFifo na zapis
	
	while(1)
	{
		trwaPrzesylanie = 0;
		
		do{	
			
			if(read(idPipe1[0], buforWejsciowy, sizeof(buforWejsciowy)) != -1) //Jak poprawnie odebral wiadomosc (np sygnal mu nei przerwal)
			{						
				trwaPrzesylanie = 1;
				while(sygnalZatrzymania);
				
				if(buforWejsciowy[0] == '\n' && buforWejsciowy[1] == '\n' && buforWejsciowy[2] == '\n') 
				//Jak P1 wyslal -1 tzn ze koniec obecnego zadania - czyli user skonczyl pisac na klawiaturze lub
				{
					sprintf(buforWejsciowy, "-1");
					write(idPipe2[1], buforWejsciowy, sizeof(buforWejsciowy));
					
					trwaPrzesylanie = 0;
					if(sygnalZakonczenia)
						exit(0);
				}
				else //JAk zwykla paczka
				{
					sprintf(buforWejsciowy, "%lu", strlen(buforWejsciowy) - 1);
					write(idPipe2[1], buforWejsciowy, sizeof(buforWejsciowy));
				}	
			}	
		}while(trwaPrzesylanie);
	}
}

void proces3()
{		
		
	struct sigaction zadanie; //Struktura sygnalu
	memset (&zadanie, 0, sizeof (zadanie)); //ZAalokuj pamiec
	zadanie.sa_handler = &zawiadom; //Wywolaj funkcje zawiadom
	zadanie.sa_flags = SA_RESTART; //Jak proces cos grzebal gdy dostal sygnal, to ma powtorzyc dzialanie
	sigaction(12, &zadanie, NULL);	//Wywolaj jak otrzymasz sygnal -12 - 12
		
	//P2 jak otrzyma sig TERM & TSTP & CONT ma je wysylac do glownego
	struct sigaction zadanie2;
	memset (&zadanie2, 0, sizeof (zadanie2));
	zadanie2.sa_handler = &wyslijDoGlownego;
	zadanie2.sa_flags = SA_RESTART;
	sigaction(20, &zadanie2, NULL);
	sigaction(18, &zadanie2, NULL);	
	sigaction(15, &zadanie2, NULL);
	
	sigset_t maska;
	sigfillset(&maska);  //Tworze pusta maske
	sigdelset(&maska, 12); // Dodaje do maski sygnaly
	sigdelset(&maska, 18); // Dodaje do maski sygnaly
	sigdelset(&maska, 15); // Dodaje do maski sygnaly
	sigdelset(&maska, 20); // Dodaje do maski sygnaly	
	sigprocmask(SIG_BLOCK, &maska, NULL); //Blokuj sygnaly z maski
		
	identyfikatorFifo = open("fifo", O_RDONLY); //OTwieramy identyfikatorFifo na zapis
	
	close(idPipe2[1]);
	char buforWejsciowy[wielkoscPakietu];
		
	while(1)
	{		
		trwaPrzesylanie = 0;
		do{	
			if(read(idPipe2[0], buforWejsciowy, sizeof(buforWejsciowy)) != -1)
			{				
				trwaPrzesylanie = 1;
				while(sygnalZatrzymania);	
				
				if(buforWejsciowy[0] == '-' && buforWejsciowy[1] == '1') //Paczka oznaczajaca koniec
				{
					trwaPrzesylanie = 0;
					if(sygnalZakonczenia)
						exit(0);
				}	
				else
				{
					if(buforWejsciowy[0] != '0')
						printf("%s\n", buforWejsciowy);//Jak zwykla - wyp[isz wartosc na ekran
				}							
			}		
		}while(trwaPrzesylanie);
	}
}

int main(void){
	pipe(idPipe1);
	pipe(idPipe2);
			
	mkfifo("fifo", 0666); //TWorzymy Fifo

   
    procesInicjalizujacy = getpid(); //pobierz pid
 
	//Procesy tworzymy "od konca", dzieki czemu P2 zna pid P3 itd. - nie trzeba ich oddzielnie przekazywac
	
	if((pidP3 = fork()) == 0)
	{
		proces3();
		return 0;
	}
	kolejnyProces = pidP3;
	
	if((pidP2 = fork()) == 0)
	{
		proces2();
		return 0;
	}
	
	kolejnyProces = pidP2;
	
	if((pidP1 = fork()) == 0)
	{
		proces1();
		return 0;
	}
	
	kolejnyProces = pidP1;
	
	identyfikatorFifo = open("fifo", O_WRONLY); //OTwieramy Fifo na zapis

	//Rozszerzona struktura do sygnalow - pozwala na pobranie pidu wysylajacego
	//Trzeba to tak zrobic, poniewaz nie da sie jednoczesnie zablokowac sygnal dla konsoli i odblokowac dla P2
	
	struct sigaction zadanie;
	memset (&zadanie, 0, sizeof (zadanie));
	zadanie.sa_sigaction = &zapisz_do_FIFO;
	zadanie.sa_flags = SA_RESTART | SA_SIGINFO;
	sigaction(20, &zadanie, NULL);
	sigaction(18, &zadanie, NULL);
	sigaction(15, &zadanie, NULL);
	
	sigset_t maska;
	sigfillset(&maska);  
	sigdelset(&maska, 18); 
	sigdelset(&maska, 15); 
	sigdelset(&maska, 20); 
	sigprocmask(SIG_BLOCK, &maska, NULL); 

	//Czekaj na koniec procesu 3 (tego co pisze liczby na ekran)
	waitpid(pidP3, NULL, 0);
	waitpid(pidP2, NULL, 0);		

	close(idPipe1[0]);
	close(idPipe1[1]);	
	close(idPipe2[0]);	
	close(idPipe2[1]);
	
	//Odlacz Fifo
	unlink("fifo");

	return 0;
}
