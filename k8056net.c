/* K8056 Client By FG 2011 v1.3c*/
/* Lecture des parametres
Usage k8056net [-i IP] [-p PORT] [-h] [-v] [-V] [-a card_address] [-S|-C|-P|-T relay#] [-B relay_status] [-A new_address] [-E|-D|-F]

parametres emis:
- instuction (A.B.C.E.D.F.S.T.P.)
- relais (1..8)

recupere parametre
convertion parametre
transmission vers adresse et port
sortie*/

// Includes
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <stdio.h>  /* Standard input/output definitions */
#include <unistd.h>  /* UNIX standard function definitions */
#include <termios.h> /* POSIX terminal control definitions */
#include <string.h>  /* String function definitions */
#include <math.h>
#include <fcntl.h>
#include <errno.h>   /* Error number definitions */

// define
#define MSGSIZE 500

// Variables
struct 	termios oldtio, newtio ;
unsigned char 	k8056_instruction = 'E';
unsigned char 	k8056_addr = 1 ;
unsigned char	k8056_relay_address = '0'  ;
int		k8056_trame_number = 5 ;		/*  Increase value with a long serial cable or in wireless operation. */
int		verbose = 0 ;
int		debug = 0 ;
char 	cmd[6] ;
int		Taillecmd ;
u_short portnum = 18000;
char *msg, *htoname = "192.168.0.1";


/*------------------------------------------------------------------------------*/
/*  Fonctions Usage et Aides                                                    */
/*------------------------------------------------------------------------------*/
void usage (void) {
printf("\nk8056	   Usage k8056net [-i IP] [-p PORT] [-h] [-v] [-V] [-a card_address] [-S|-C|-P|-T relay#] [-B relay_status] [-A new_address] [-E|-D|-F]\n");
printf("      Pour plus de details utiliser l'aide: k8056 -h\n\n");
printf("      Version 1.3c\n\n");
}

void help (void) {
printf("\nk8056  est un programme qui controle la carte 'Velleman K8056' via udp (Version Francaise par FOURNIER Ghislain - 2011 \n\n");
printf("Usage k8056net [-i IP] [-p PORT] [-h] [-v] [-V] [-a card_address] [-S|-C|-T relay#] [-B relay_status] [-A new_address] [-E|-D|-F]\n\n");
printf("      Version 1.3c\n\n");
printf("        -h                : Aide\n");
printf("        -v                : Mode verbeux\n");
printf("        -V                : Debug\n");
printf("        -i addresse IP    : Adresse IP du serveur\n");
printf("        -p port           : Port UDP, par defaut 18000 \n");
printf("        -a addresse_carte : Adresse de la carte (1..255) en decimal\n");
printf("        -S|-C|-P|-T relay# : Instructions to Active/Relache/Impulsion/Basculer le relais numero ('1'..'9')\n");
printf("        -A new_address    : Regler la nouvelle adresse de la carte (1..255)\n");
printf("        -B status_relais  : Reglez le relais huit avec octet status_relais (1..255)\n");
printf("        -E|-D|-F          : Stop d'Urgence / Afficher l'adresse / Forcer l'adresse Ã \n\n");
printf("	** Command examples **\n");
printf("        k8056 -i 192.168.0.1 -v -E       : ArrÃªd'urgence toutes les cartes relais (mode verbeux).\n");
printf("        k8056 -i 192.168.0.1 -v -S6      : Active relais #6 (mode verbeux).\n");
printf("        k8056 -i 192.168.0.1 -v -C5      : Relache relais #5 (mode verbeux).\n");
printf("        k8056 -i 192.168.0.1 -v -P3      : Impulsion relais #5 (mode verbeux).\n");
printf("        k8056 -i 192.168.0.1 -v -T5      : Basculer le relais#5 => Active relay #5 (mode verbeux).\n");
printf("        k8056 -i 192.168.0.1 -B 127      : Relache relay #1, Active relais #2..8\n");
printf("        k8056 -i 192.168.0.1 -p 1000 -S1 : Active relais #1 sur le port udp 1000.\n\n");
}


/*------------------------------------------------------------------------------*/
/* Parse the command line to find additionnal arguments                         */
/*------------------------------------------------------------------------------*/
char parse_cmdline(int argc,char *argv[]) {
   int helpflg=0;
   int errflg=0;
   int c;

   while ((c = getopt(argc, argv, "S:C:P:T:A:B:a:d:i:p:EDFhvV")) != EOF)
   {
      switch (c)
      {
         case 'S':
         case 'C':
         case 'P':
         case 'T':
	 	k8056_instruction = c ;
            	k8056_relay_address = optarg[0] ;
            	break;
         case 'A':
         case 'B':
	 	k8056_instruction = c ;
            	k8056_relay_address = atoi(optarg);
            	break;
         case 'E':
         case 'D':
         case 'F':
	 	k8056_instruction = c ;			/* The 3rd byte = 0 (k8056_relay_address) */
            	break;
		case 'a':
            	k8056_addr = atoi(optarg);
            	break;
		case 'i':
            	//strlcpy(htoname, optarg, sizeof(htoname));
		htoname = optarg;
            	break;
		case 'p':
                portnum = atoi(optarg);
            	break;
         case 'v':
            	verbose = 1;
            	break;
         case 'V':
            	debug = 1;
            	break;
         case 'h':
            	helpflg++;
         	break;
         case '?':
            	errflg++;
         	break;
      }
        if (helpflg)
	{
           help();
           exit (1);
        }
     }
     if (optind < 2 || errflg)			/* if optind = 1, option missing. */
   	{
        	usage();
        	exit (1);
   	}
     return 1;
}


/*------------------------------------------------------------------------------*/
/* MAIN				       														*/
/*------------------------------------------------------------------------------*/
int main(int argc, char *argv[]) {
 	parse_cmdline(argc,argv) ;

	  int sockfd, ok, addr_in_size;

  struct sockaddr_in *to;
  struct hostent *toinfo;

  u_long toaddr;

  msg = (char *)malloc(MSGSIZE);
  to = (struct sockaddr_in *)malloc(sizeof(struct sockaddr_in));
	msg[0] = k8056_instruction ;				/* Instruction				*/
	msg[1] = k8056_relay_address ;				/* # relais, address or status		*/

		if (verbose) printf("Envoi commande vers la carte k8056:\n	Port ip: %u, Addresse ip: %s, Instruction: %c, Relais #%c\n"
		, portnum, htoname, msg[0], msg[1]) ;

  if((toinfo = gethostbyname(htoname)) == NULL){
    fprintf(stderr,"Error %d in gethostbyname: %s\n",
      errno,sys_errlist[errno]);
    exit(errno);
  };
  toaddr = *(u_long *)toinfo->h_addr_list[0];

  addr_in_size = sizeof(struct sockaddr_in);
  memset((char *)to,(char)0,addr_in_size);

  to->sin_family = AF_INET;
  to->sin_addr.s_addr = toaddr;
  to->sin_port = htons(portnum);

  if((sockfd = socket (PF_INET, SOCK_DGRAM, 0)) == -1){
    fprintf(stderr,"Error %d in socket: %s\n",errno,sys_errlist[errno]);
    exit(errno);
  };

	   ok = 1;

    if(sendto(sockfd,msg,strlen(msg),0,(struct sockaddr *)to,
        addr_in_size) == -1){
      fprintf(stderr,"Error %d in sendto: %s\n",errno,sys_errlist[errno]);
      exit(errno);
    }
        /* Quitt... */
        exit(0);
}
/*------------------------------------------------------------------------------*/



