static const char ___[]=" $Id: config.c,v 1.11 2018/01/19 20:01:35 skalak Exp $ ";

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
	printf("options:\n");
	printf("     [-b BANNER] - banner to send to client (default is '%s')\n",DEFAULT_BANNER_STR);
	printf("     [-p LOCAL_PORT] - where to run the 'trap' (default is %d)\n",DEFAULT_TRAP_PORT);
	printf("     [-a HAAS_HOST] -  where to connect (default is '%s')\n",DEFAULT_HAAS_HOST);
	printf("     [-r HAAS_PORT] -  where to connect (default is %s)\n",DEFAULT_HAAS_PORT);
	printf("     [-k KEYS_DIR]  -  where to look for RSA/DSA keys (default is '%s')\n",DEFAULT_KEYS_FOLDER);
	printf("     [-i IDLE_TIMEOUT] - inactivity timeout (default is %d sec)\n",DEFAULT_IDLE_TIMEOUT);
	printf("     [-m FORWARD_MODE] - how to handle port forwrading (default mode is %d)\n",DEFAULT_FORWARD_MODE);
	printf("                         0-deny, 1-allow, 2-fake, 3-null, 4-echo\n");
	printf("\n");
}

void ConfigInit(tConfig *Config){
	memset(Config,0,sizeof(tConfig));

	Config->WantHelp=0;
	Config->Foreground=0;

	Config->IdleTimeout=-1;

	Config->DebugLevel=0;

	Config->TrapPort=0;

	Config->ForwardMode=-1;

	Config->Banner=NULL;

	Config->KeysDir=NULL;

	Config->HaasAddr=NULL;
	Config->HaasPort=NULL;
	Config->HaasToken=NULL;

	Config->TrapLog="5";		// NULL;
	Config->HaasLog=NULL;	// "5";

}

int ConfigParse(int argc,char **argv,tConfig *Config){
	int	Back=1;	// optimist 

	opterr=0;

	while(Back){
		int	i;

		i=getopt(argc,argv,":fhp:a:r:t:k:di:b:m:");
		if (i==-1){
			break;
		}

		switch (i){
			case 'f':	Config->Foreground=1;
						break;

			case 'p':	if (Config->TrapPort!=0){
								printf("Error, multiple LOCAL_PORT argument '%s' !!!\n",optarg);
								Back=0;
							}
							else{
								char	*p;

								i=strtol(optarg,&p,0);
								if (*p!=0){
									printf("Error, parsing LOCAL_PORT value '%s' !!!\n",optarg);
									Back=0;
								}
								else{
									if ((i<=0)||(i>=65535)){
										printf("Error, LOCAL_PORT %d out of range !!!\n",i);
										Back=0;
									}
									else{
										Config->TrapPort=i;
									}
								}
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

			case 'r':	if (Config->HaasPort!=NULL){
								printf("Error, multiple HAAS_PORT argument '%s' !!!\n",optarg);
								Back=0;
							}
							else{
								char	*p;

								i=strtol(optarg,&p,0);
								if (*p!=0){
									printf("Error, parsing HAAS_PORT value '%s' !!!\n",optarg);
									Back=0;
								}
								else{
									if ((i<=0)||(i>=65535)){
										printf("Error, HAAS_PORT %d out of range !!!\n",i);
										Back=0;
									}
									else{
										Config->HaasPort=optarg;
									}
								}
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

			case 'i':	if (Config->IdleTimeout!=-1){
								printf("Error, multiple IDLE_TIMEOUT argument '%s' !!!\n",optarg);
								Back=0;
							}
							else{
								char	*p;

								i=strtol(optarg,&p,0);
								if (*p!=0){
									printf("Error, parsing IDLE_TIMEOUT value '%s' !!!\n",optarg);
									Back=0;
								}
								else{
									if (i<=0){
										printf("Error, IDLE_TIMEOUT %d out of range !!!\n",i);
										Back=0;
									}
									else{
										Config->IdleTimeout=i;
									}
								}
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
			if (Config->TrapPort==0){
				printf("Using default LOCAL_PORT %d\n",DEFAULT_TRAP_PORT);
				Config->TrapPort=DEFAULT_TRAP_PORT;
			}

			if (Config->HaasAddr==NULL){
				printf("Using default HAAS_HOST '%s'\n",DEFAULT_HAAS_HOST);
				Config->HaasAddr=DEFAULT_HAAS_HOST;
			}

			if (Config->HaasPort==NULL){
				printf("Using defualt HAAS PORT %s\n",DEFAULT_HAAS_PORT);
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

