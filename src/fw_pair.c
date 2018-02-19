static const char ___[]=" $Id: fw_pair.c,v 1.2 2017/12/27 21:37:35 skalak Exp $ ";

#define _GNU_SOURCE
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

#include <libssh/libssh.h>
#include <libssh/callbacks.h>

#include "debug.h"

#include "fw_pair.h"

/*
typedef struct{
				char			*Addr;
				int			Port;
				ssh_channel	Channel;
			}tFwEndpoint;
*/

static
void FwEndpointInit(tFwEndpoint *End){
	End->Addr=NULL;
	End->Port=0;
	End->Channel=NULL;
}

static
void FwEndpointDone(tFwEndpoint *End){
	if (End->Addr!=NULL){
		free(End->Addr);
		End->Addr=NULL;
	}
	End->Port=0;
	if (End->Channel!=NULL){
		if (ssh_channel_is_open(End->Channel)){
			ssh_channel_close(End->Channel);
		}
		ssh_channel_free(End->Channel);
		End->Channel=NULL;
	}
}

////////////////////////////////////////
/*
typedef struct{
				void			*Context;
				int			Closed;
				tFwPair		Source;
				tFwPair		Destiny;
			}tFwPair;
*/

tFwPair	*FwPairAlloc(const struct ssh_channel_callbacks_struct *Callbacks){
	tFwPair	*Pair;

	Pair=malloc(sizeof(tFwPair));
	if (Pair!=NULL){
		Pair->Context=NULL;
		Pair->Closed=0;
		
		FwEndpointInit(&Pair->Source);
		FwEndpointInit(&Pair->Destiny);

		memcpy(&Pair->Callbacks,Callbacks,sizeof(Pair->Callbacks));
		ssh_callbacks_init(&Pair->Callbacks);

		Pair->Callbacks.userdata=Pair;	// myself :-)
	}

	return Pair;
}

void		FwPairRelease(tFwPair *Pair){
	if (Pair!=NULL){
		Pair->Context=NULL;
		Pair->Closed=0;
	
		FwEndpointDone(&Pair->Source);
		FwEndpointDone(&Pair->Destiny);

		free(Pair);
	}
}


