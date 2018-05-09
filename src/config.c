static const char ___[]=" $Id: config.c,v 1.14 2018/05/08 21:39:46 skalak Exp $ ";

#define _GNU_SOURCE

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>

#include "config.h"

void PrintUsage(const char *argv0){
	const char	*p;

	p=strrchr(argv0,'/');
	if (p==NULL){
		p=argv0;
	}
	else{
		p+=1;
	}

	printf("Usage:\n");
	printf("   %s [switches] [options] -t TOKEN\n",p);
	printf("\n");
	printf("options:\n");
	printf("     [-b BANNER] - banner to send to client (default is '%s')\n",DEFAULT_BANNER_STR);
	printf("     [-p LOCAL_PORT]   - where to run the 'trap' (default is %d)\n",DEFAULT_TRAP_PORT);
	printf("     [-a HAAS_HOST]    - where to connect (default is '%s')\n",DEFAULT_HAAS_HOST);
	printf("     [-r HAAS_PORT]    - where to connect (default is %d)\n",DEFAULT_HAAS_PORT);
	printf("     [-k KEYS_DIR]     - where to look for RSA/DSA keys (default is '%s')\n",DEFAULT_KEYS_FOLDER);
	printf("     [-c CONN_LIMIT]   - connection limit (default is %d clients)\n",DEFAULT_CONN_LIMIT);
	printf("     [-i IDLE_TIMEOUT] - inactivity timeout (default is %d sec)\n",DEFAULT_IDLE_TIMEOUT);
	printf("     [-s SESSION_TIMEOUT] - maximum session length in sec (default is unlimited)\n");
	printf("     [-u CPU_USAGE_LIMIT] - maximum cpu suage limit in sec (default is unlimited)\n");
	printf("     [-n PROCESS_NICE]    - nice of the process (default is %d)\n",DEFAULT_PROCESS_NICE);
	printf("     [-m FORWARD_MODE] - how to handle port forwrading (default mode is %d)\n",DEFAULT_FORWARD_MODE);
	printf("                         0-deny, 1-allow, 2-fake, 3-null, 4-echo\n");
	printf("\n");
	printf("switches:\n");
	printf("     [-f] - foreground mode (don't fork)\n");
	printf("     [-d] - increase debug level\n");
	printf("     [-h] - this help :-)\n");
	printf("     [-?] - this help :-)\n");
	printf("\n");
}

void ConfigInit(tConfig *Config){
	memset(Config,0,sizeof(tConfig));

	Config->WantHelp=0;
	Config->Foreground=0;

	Config->IdleTimeout=-1;
	Config->SessionTimeout=-1;
	Config->CpuUsage=-1;
	Config->ProcessNice=-1;

	Config->ConnLimit=-1;

	Config->DebugLevel=0;

	Config->TrapPort=-1;

	Config->ForwardMode=-1;

	Config->Banner=NULL;

	Config->KeysDir=NULL;

	Config->HaasAddr=NULL;
	Config->HaasPort=-1;
	Config->HaasToken=NULL;

	Config->TrapLog="5";		// NULL;
	Config->HaasLog=NULL;	// "5";

}

////////////////////////////////////////////////////////////////////////////////

static
int ParseInt(int *Value,const char *Name,const char *optarg){
	int	Back=0;

	if (*Value!=-1){
		printf("Error, multiple %s argument '%s' !!!\n",Name,optarg);
	}
	else{
		char	*p;
		int	i;

		i=strtol(optarg,&p,0);
		if (*p!=0){
			printf("Error, parsing %s value '%s' !!!\n",Name,optarg);
		}
		else{
			if (i<=0){
				printf("Error, %s %d out of range !!!\n",Name,i);
			}
			else{
				*Value=i;
				Back=1;
			}
		}
	}

	return Back;
}

static
int ParsePort(int *Value,const char *Name,const char *optarg){
	int	Back=0;
	int	i;

	i=*Value;
	if (ParseInt(&i,Name,optarg)){
		if (i>=65535){
			printf("Error, %s %d out of range !!!\n",Name,i);
		}
		else{
			*Value=i;
			Back=1;
		}
	}

	return Back;
}

////////////////////////////////////////////////////////////////////////////////

int ConfigParse(int argc,char **argv,tConfig *Config){
	int	Back=1;	// optimist 

	opterr=0;

	while(Back){
		int	i;

		i=getopt(argc,argv,":fhdp:a:r:t:k:i:c:b:m:s:u:n:");
		if (i==-1){
			break;
		}

		switch (i){
			case 'f':	Config->Foreground=1;
						break;

			case 'p':	if (!ParsePort(&Config->TrapPort,"LOCAL_PORT",optarg)){
								Back=0;
							}
						break;

			case 'a':	if (Config->HaasAddr!=NULL){
								printf("Error, multiple HAAS_ADDR argument '%s' !!!\n",optarg);
								Back=0;
							}
							else{
								Config->HaasAddr=optarg;
							}
						break;

			case 'r':	if (!ParsePort(&Config->HaasPort,"HAAS_PORT",optarg)){
								Back=0;
							}
						break;

			case 'b':	if (Config->Banner!=NULL){
								printf("Error, multiple BANNER argument '%s' !!!\n",optarg);
								Back=0;
							}
							else{
								Config->Banner=optarg;
							}
						break;

			case 't':	if (Config->HaasToken!=NULL){
								printf("Error, multiple HAAS_TOKEN argument '%s' !!!\n",optarg);
								Back=0;
							}
							else{
								Config->HaasToken=optarg;
							}
						break;

			case 'k':	if (Config->KeysDir!=NULL){
								printf("Error, multiple KEYS_DIR argument '%s' !!!\n",optarg);
								Back=0;
							}
							else{
								Config->KeysDir=optarg;
							}
						break;

			case 'd':	Config->DebugLevel++;
						break;

			case 'i':	if (!ParseInt(&Config->IdleTimeout,"IDLE_TIMEOUT",optarg)){
								Back=0;
							}
						break;

			case 's':	if (!ParseInt(&Config->SessionTimeout,"SESSION_TIMEOUT",optarg)){
								Back=0;
							}
						break;

			case 'u':	if (!ParseInt(&Config->CpuUsage,"CPU_USAGE",optarg)){
								Back=0;
							}
						break;

			case 'n':	if (!ParseInt(&Config->ProcessNice,"PROCESS_NICE",optarg)){
								Back=0;
							}
						break;

			case 'c':	if (!ParseInt(&Config->ConnLimit,"CONN_LIMIT",optarg)){
								Back=0;
							}
						break;

			case 'm':	if (Config->ForwardMode!=-1){
								printf("Error, multiple FORWARD_MODE argument '%s' !!!\n",optarg);
								Back=0;
							}
							else{
								char	*p;

								i=strtol(optarg,&p,0);
								if (*p!=0){
									printf("Error, parsing FORWARD_MODE value '%s' !!!\n",optarg);
									Back=0;
								}
								else{
									if ((i<0)||(i>=FORWARD_MODE_MAX)){
										printf("Error, FORWARD_MODE %d out of range !!!\n",i);
										Back=0;
									}
									else{
										Config->ForwardMode=i;
									}
								}
							}
						break;

			case ':':	printf("Error, missing an argument for -%c option !!!\n",optopt);
							Back=0;
						break;

			case 'h':	Config->WantHelp=1;
						break;

			case '?':	if (optopt=='?'){
								Config->WantHelp=1;
							}
							else{
								printf("Error, unknown option -%c !!!\n",optopt);
								Back=0;
							}
						break;

			default:		printf("Unexpected return from getopt: %d, 0x%02x, '%c'\n",
											i,i,((i<' ')||(i>'~'))?'.':i);
//							Back=0;
						break;
		}	// case ...
	}	// while(1)

	if (!Config->WantHelp){
		if (Back){
			if (optind<argc){
				printf("Error, some extra arguments on command line !!!\n");
				Back=0;
			}
		}

		if (Back){
			if (Config->HaasToken==NULL){
				printf("Error, missing HAAS_TOKEN !!!\n");
				Back=0;
			}
		}

		if (Back){
			if (Config->TrapPort==-1){
				printf("Using default LOCAL_PORT %d\n",DEFAULT_TRAP_PORT);
				Config->TrapPort=DEFAULT_TRAP_PORT;
			}

			if (Config->HaasAddr==NULL){
				printf("Using default HAAS_HOST '%s'\n",DEFAULT_HAAS_HOST);
				Config->HaasAddr=DEFAULT_HAAS_HOST;
			}

			if (Config->HaasPort==-1){
				printf("Using defualt HAAS PORT %d\n",DEFAULT_HAAS_PORT);
				Config->HaasPort=DEFAULT_HAAS_PORT;
			}

			if (Config->KeysDir==NULL){
				printf("Using defualt KEYS_DIR '%s'\n",DEFAULT_KEYS_FOLDER);
				Config->KeysDir=DEFAULT_KEYS_FOLDER;
			}

			if (Config->IdleTimeout==-1){
				printf("Using default IDLE_TIMEOUT of %d sec\n",DEFAULT_IDLE_TIMEOUT);
				Config->IdleTimeout=DEFAULT_IDLE_TIMEOUT;
			}

			if (Config->ConnLimit==-1){
				printf("Using default CONN_LIMIT of %d clients\n",DEFAULT_CONN_LIMIT);
				Config->ConnLimit=DEFAULT_CONN_LIMIT;
			}

			if (Config->Banner==NULL){
				printf("Using defualt BANNER '%s'\n",DEFAULT_BANNER_STR);
				Config->Banner=DEFAULT_BANNER_STR;
			}

			if (Config->ForwardMode==-1){
				printf("Using default FORWARD_MODE %d\n",DEFAULT_FORWARD_MODE);
				Config->ForwardMode=DEFAULT_FORWARD_MODE;
			}
		}
	}

	return Back;
}

