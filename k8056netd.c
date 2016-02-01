/* k8056ned UDP SERVER By FG 2011 v1.3d */
/* Usage : k8056netd [-h] [-v] [-V] [-d device] [-p PORT] */

/* -- Synoptique -- */
/* On Demonize */
/*	Ecoute Port */
/*	Si données */
/*	Appel le fils */
/*		Verification */
/*		Mise en forme */
/*		transmission vers k8056 */
/* tue le fils */
/*	retour ecoute */

/* modif 369 et 373 pf_net par af_net + IPPROTO_UDP + verbose dans la console debug dans le syslog */

/*------------------------------------------------------------------------------*/
/*  Includes	                        	       			                	*/
/*------------------------------------------------------------------------------*/

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <netdb.h>
#include <errno.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <termios.h>
#include <math.h>
#include <fcntl.h>
#include <pwd.h>
#include <signal.h>
#include <syslog.h>


/*------------------------------------------------------------------------------*/
/*  Definitions	                        	       			                	*/
/*------------------------------------------------------------------------------*/
#define MSGSIZE 2
#define BAUDRATE B2400
#define NPACK 10
/* Change this to whatever your daemon is called */
#define DAEMON_NAME "k8056netd"

/* Change this to the user under which to run */
#define RUN_AS_USER "root"

#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1

/*------------------------------------------------------------------------------*/
/*  Variables	                        	       			                  	*/
/*------------------------------------------------------------------------------*/
struct 	termios oldtio, newtio ;
int     	k8056_port ;
char		k8056_device[16] = "/dev/k8056" ;	/*  default device. */
unsigned char 	k8056_instruction = 'E';
unsigned char 	k8056_addr = 1 ;
unsigned char	k8056_relay_address = '0'  ;
int		k8056_trame_number = 5 ;		/* Increase value with a long serial cable or in wireless operation. */
int		verbose = 0 ;
int		debug = 0 ;
char 		cmd[6] ;
int		Taillecmd ;

int sockfd, cc;
unsigned int addr_in_size;
u_short portnum = 18000;
struct sockaddr_in *my_addr, *from;
char *msg;
u_long fromaddr;

/*------------------------------------------------------------------------------*/
/*  Affiche l'aide                                                              */
/*------------------------------------------------------------------------------*/
void usage (void) {
	printf("\nk8056ned   Usage:    k8056netd [-h] [-v] [-V] [-d device] [-p PORT] \n");
	printf("      Pour plus de details utiliser l'aide: k8056netd -h\n\n");
printf("      Version 1.3c\n\n");
}

void help (void) {
	printf("\nk8056netd controle la carte 'Velleman K8056' via UDP (Version Francaise par FOURNIER Ghislain - 2011 \n\n");
	printf("Usage:   k8056netd [-h] [-v] [-d device] [-p PORT] \n\n");
	printf("        -h                : Aide\n");
	printf("        -v                : Mode verbeux\n");
	printf("        -V                : Debug\n");
	printf("        -d device         : Rs232 port, par defaut /dev/k8056 (A faire: : ln -s /dev/ttyS0 /dev/k8056 par exemple)\n");
	printf("        -p port           : UDP port, par defaut 180000 \n");
	printf("  ** Command examples **\n");
	printf("  k8056netd -d /dev/ttyS14 -p 1234  : Controle la carte  sur port série /dev/ttyS14, recoie les données sur le port udp 1234.\n\n");
printf("      Version 1.3c\n\n");
}


/*------------------------------------------------------------------------------*/
/*  Ouverture port serie                   	       				                */
/*------------------------------------------------------------------------------*/
void initserie (void) {
	if (verbose) printf("Ouverture du port\n");
	if (debug) syslog( LOG_INFO, "Ouverture du port serie" );
	k8056_port = open(k8056_device, O_RDWR | O_NOCTTY );
	if (k8056_port < 0) {
 		if (verbose) { fprintf(stderr, "Erreur d'ouverture du peripherique %s !\n", k8056_device) ; }
		exit(-1);
	}
 	tcgetattr(k8056_port,&oldtio); 		/* Sauvegarde des parametres courants du port  */

 	newtio.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;
 	newtio.c_oflag = 0;
 	tcflush(k8056_port, TCOFLUSH);		/* Flushes written but not transmitted. */
 	tcsetattr(k8056_port, TCSANOW, &newtio);
}


/*------------------------------------------------------------------------------*/
/* Checksum	(complement à 2 de la somme de 4 octets + 1)                        */
/*------------------------------------------------------------------------------*/
unsigned char checksum(char cmd[]) {
        unsigned char checksum ;
        /*  Ex. VB: checksum = ( 255 - ( ( ( (a+b+c+d / 256 ) - Int( (13 + cmd[1] + cmd[2] + cmd[3]) / 256 ) ) * 256 ) ) + 1 ;	*/
        /* ( 255 - ((a+b+c+d) modulo 256) ) + 1		Calcul de checkum (complement à 2 de la somme de 4 octets + 1).  	*/
	if (verbose) printf("Calcul Checksum\n");
	checksum = ~((13 + cmd[1] + cmd[2] + cmd[3]) & 0xFF) + 1 ;
        return checksum ;
}


/*------------------------------------------------------------------------------*/
/* SendCommand	Send command to k8056 card                                      */
/*------------------------------------------------------------------------------*/
void SendCommand(char cmd[]) {
	if (debug) syslog( LOG_INFO, "Send command to k8056" );
	int	i ;
	tcflush(k8056_port, TCOFLUSH) ;		/* Flushes written but not transmitted. */
	for (i=0 ; i < k8056_trame_number ; i++) {
		write(k8056_port, cmd, 5) ;
		if (verbose) printf(". %d \n",i) ;
		if (verbose) {
			printf("	Donnees envoyer = %s \n",cmd) ;
			Taillecmd = strlen (cmd);
			printf("	Taille Donnees  = %d \n",Taillecmd) ;
		}
		if (verbose) printf("Ecriture sur le port\n");
		if (verbose) printf("Pause entre deux Ecriture\n");
		usleep(5000) ;
	}
}


/*------------------------------------------------------------------------------*/
/* Parse de la ligne de commande pour les arguments otionnels 	            	*/
/*------------------------------------------------------------------------------*/
char parse_cmdline(int argc,char *argv[]) {
   int helpflg=0;
   int errflg=0;
   int c;

   while ((c = getopt(argc, argv, "d:p:hvV")) != EOF)
   {
      switch (c)
      {
         case 'd':
            	strcpy(k8056_device, optarg) ;
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
        if (helpflg)	/* Aide */
	{
           help();
           exit (1);
        }
     }
		//if (optind < 2 || errflg)		/* Usage */
		if (errflg)		/* Usage */
   	{
        	usage();
        	exit (1);
   	}
     return 1;
}

/*------------------------------------------------------------------------------*/
/* Preparation a l'envoi des données                                    		*/
/*------------------------------------------------------------------------------*/
void sendserie(char msg[])
{

if (verbose) { printf("	Données reçues = %d \n",msg[0]) ; }

if (verbose) { fprintf(stdout,"Réception de %s port %d: %s\n",
      (gethostbyaddr((char *)&fromaddr,
         sizeof(fromaddr),
         PF_INET))->h_name,
       from->sin_port,msg); }

	if (verbose) printf("Envoi commande vers la carte k8056:\n	Port: %s, Addresse carte: %d, Instruction: %c, Relais #%c\n"
		, k8056_device, k8056_addr, msg[0], msg[1]) ;

	cmd[0] = 13 ;					/* 13										*/
	cmd[1] = k8056_addr ;			/* Adresse									*/
	cmd[2] = msg[0] ;				/* Instruction								*/
	cmd[3] = msg[1] ;				/* # relais, addresse ou status				*/
	cmd[4] = checksum(cmd) ;		/* Checksum									*/

	if (verbose) printf("	Checksum = %d\n", checksum(cmd)) ;

	SendCommand(cmd) ;
		if (debug) syslog( LOG_INFO, "Send command fait" );


        /* Fermeture port serie */
	tcsetattr(k8056_port, TCSANOW, &oldtio);	/* Sauvegarde des anciens parametres du port  */
	if (verbose) printf("Fermeture du port\n");
        close(k8056_port);
	if (verbose) printf("Pause entre deux commandes\n");
	usleep(300000) ;					/* Pause entre 2 commandes. */
    	if (verbose) printf("Fait\n");
	if (verbose) printf(".\n");

}


/*------------------------------------------------------------------------------*/
/* Gestion des zombies                                                          */
/*------------------------------------------------------------------------------*/
static void child_handler(int signum)
{
    switch(signum) {
    case SIGALRM: exit(EXIT_FAILURE); break;
    case SIGUSR1: exit(EXIT_SUCCESS); break;
    case SIGCHLD: exit(EXIT_FAILURE); break;
    }
}


/*------------------------------------------------------------------------------*/
/* Daemonization                                                                */
/*------------------------------------------------------------------------------*/
static void daemonize( const char *lockfile )
{
    pid_t pid, sid, parent;
    int lfp = -1;

    /* already a daemon */
    if ( getppid() == 1 ) return;

    /* Create the lock file as the current user */
    if ( lockfile && lockfile[0] ) {
        lfp = open(lockfile,O_RDWR|O_CREAT,0640);
        if ( lfp < 0 ) {
            syslog( LOG_ERR, "unable to create lock file %s, code=%d (%s)",
                    lockfile, errno, strerror(errno) );
            exit(EXIT_FAILURE);
        }
    }

    /* Drop user if there is one, and we were run as root */
    if ( getuid() == 0 || geteuid() == 0 ) {
        struct passwd *pw = getpwnam(RUN_AS_USER);
        if ( pw ) {
            syslog( LOG_NOTICE, "setting user to " RUN_AS_USER );
            setuid( pw->pw_uid );
        }
    }

    /* Trap signals that we expect to recieve */
    signal(SIGCHLD,child_handler);
    signal(SIGUSR1,child_handler);
    signal(SIGALRM,child_handler);

    /* Fork off the parent process */
    pid = fork();
    if (pid < 0) {
        syslog( LOG_ERR, "unable to fork daemon, code=%d (%s)",
                errno, strerror(errno) );
        exit(EXIT_FAILURE);
    }
    /* If we got a good PID, then we can exit the parent process. */
    if (pid > 0) {

        /* Wait for confirmation from the child via SIGTERM or SIGCHLD, or
           for two seconds to elapse (SIGALRM).  pause() should not return. */
        alarm(2);
        pause();

        exit(EXIT_FAILURE);
    }

    /* At this point we are executing as the child process */
    parent = getppid();

    /* Cancel certain signals */
    signal(SIGCHLD,SIG_DFL); /* A child process dies */
    signal(SIGTSTP,SIG_IGN); /* Various TTY signals */
    signal(SIGTTOU,SIG_IGN);
    signal(SIGTTIN,SIG_IGN);
    signal(SIGHUP, SIG_IGN); /* Ignore hangup signal */
    signal(SIGTERM,SIG_DFL); /* Die on SIGTERM */

    /* Change the file mode mask */
    umask(0222);

    /* Create a new SID for the child process */
    sid = setsid();
    if (sid < 0) {
        syslog( LOG_ERR, "unable to create a new session, code %d (%s)",
                errno, strerror(errno) );
        exit(EXIT_FAILURE);
    }

    /* Change the current working directory.  This prevents the current
       directory from being locked; hence not being able to remove it. */
    if ((chdir("/")) < 0) {
        syslog( LOG_ERR, "unable to change directory to %s, code %d (%s)",
                "/", errno, strerror(errno) );
        exit(EXIT_FAILURE);
    }

    /* Redirect standard files to /dev/null */
    freopen( "/dev/null", "r", stdin);
    freopen( "/dev/null", "w", stdout);
    freopen( "/dev/null", "w", stderr);

    /* Tell the parent process that we are A-okay */
    kill( parent, SIGUSR1 );
}


/*------------------------------------------------------------------------------*/
/* MAIN Ecoute reception données UDP                                            */
/*------------------------------------------------------------------------------*/
int main(int argc, char *argv[]) {
    /* Initialize the logging interface */
    openlog( DAEMON_NAME, LOG_PID, LOG_LOCAL5 );
    syslog( LOG_INFO, "starting k8056netd" );

    /* One may wish to process command line arguments here */
 	parse_cmdline(argc,argv) ;

	addr_in_size = sizeof(struct sockaddr_in);

	// Détournement du signal émis à la mort du fils (il ne reste pas zombie)
//	signal(SIGCHLD, SIG_IGN);

	msg = (char *)malloc(MSGSIZE);

	from = (struct sockaddr_in *)malloc(addr_in_size);
	my_addr = (struct sockaddr_in *)malloc(addr_in_size);

	memset((char *)my_addr,(char)0,addr_in_size);
	my_addr->sin_family = AF_INET;
	my_addr->sin_addr.s_addr = htonl(INADDR_ANY);
	my_addr->sin_port = htons(portnum);

	if((sockfd = socket (PF_INET, SOCK_DGRAM, 0)) < 0){

		if (verbose) fprintf(stderr,"Erreur %d dans socket: %s\n",errno,strerror(errno));
		exit(errno);
	};

	if(bind(sockfd, (struct sockaddr *)my_addr, addr_in_size) < 0){
		if (verbose)  fprintf(stderr,"Erreur %d dans bind: %s\n",errno,strerror(errno));
		if(errno != EADDRINUSE) exit(errno);
	};

	/* On daemonize */
	if (debug)	syslog( LOG_INFO, "daemonize k8056netd" );
    daemonize( "/var/lock/" DAEMON_NAME );

    /* Now we are a daemon -- do the work for which we were paid */

	if (verbose)  fprintf(stdout,"Prêt a reçevoir\n");
	if (debug) syslog( LOG_INFO, "On ecoute le port UDP" );
  	while(1){
  		if (debug) syslog( LOG_INFO, "On est dans la boucle" );
		if((cc = recvfrom (sockfd,msg,MSGSIZE,0,(struct sockaddr *)from,
           &addr_in_size)) == -1){
			if (verbose)  fprintf(stderr,"Erreur %d dans recvfrom: %s\n",
				errno,strerror(errno));
			exit(errno);
		};

		fromaddr = from->sin_addr.s_addr;
		msg[cc] = '\0';

		if (verbose)  fprintf(stdout,"Reçue de  %s port %d: %s\n",
		(gethostbyaddr((char *)&fromaddr,
         sizeof(fromaddr),
         PF_INET))->h_name,
		from->sin_port,msg);

	if (debug) syslog( LOG_INFO, "Reçue de  %s port %d: %s\n",
		(gethostbyaddr((char *)&fromaddr,
         sizeof(fromaddr),
         PF_INET))->h_name,
		from->sin_port,msg);
		/* Ouverture port serie */
		initserie() ;
		if (debug)	syslog( LOG_INFO, "init serie fait" );

		/* Formate msg vers cmd et envoie */
		sendserie(msg);
			if (debug) syslog( LOG_INFO, "Send serie fait" );

		/* on attend avant de revenir a l'ecoute */
		sleep(3);
		if (debug)	syslog( LOG_INFO, "On repart" );
		// signal(SIGCHLD,SigCatcher);
       /* Finish up */
    if (debug) syslog( LOG_NOTICE, "terminated" );
    closelog();

	}
        /* Quitt... */
	if (verbose) printf("----- ICI ON SORT -----\n");

    return 0;

}

/*------------------------------------------------------------------------------*/

