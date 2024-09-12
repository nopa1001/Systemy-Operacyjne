#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int main()
{
	int seed = 15;
	srand(seed);
	
	FILE *fp;
	
	fp = fopen("random.txt", "w");
	int counter;
	for(counter = 300000000 ; counter > 0 ; counter --)
		if(rand()%200 < 2)
		{
			fputc( 10, fp);				
		}
		else fputc( 49 + rand()%77 , fp);
		
	fclose(fp);
	printf("Koniec generowania");
	return 0;
	
}
