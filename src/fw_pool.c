static const char ___[]=" $Id: fw_pool.c,v 1.1 2017/12/27 21:06:14 skalak Exp $ ";

#define _GNU_SOURCE
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

#include <libssh/libssh.h>

#include "debug.h"

#include "fw_pair.h"
#include "fw_pool.h"

#define POOL_ALLOC_SIZE		10

struct _t_Fw_Pool_{
				int		Used;
				int		Alloc;
				tFwPair	**Items;
			};

tFwPool	*FwPoolInit(void){
	tFwPool	*Pool;

	Pool=malloc(sizeof(tFwPool));
	if (Pool!=NULL){
		Pool->Used=0;	
		Pool->Alloc=0;
		Pool->Items=NULL;
	}

	return Pool;
}

void		FwPoolDone(tFwPool *Pool){
	if (Pool!=NULL){
		if (Pool->Items!=NULL){
			int	i;

			for (i=0;i<Pool->Used;i++){
				tFwPair	*Item;

				Item=Pool->Items[i];
				if (Item!=NULL){
					FwPairRelease(Item);	// not yet done
					Pool->Items[i]=NULL;
				}
			}
			free(Pool->Items);
			Pool->Items=NULL;
		}
		Pool->Used=0;
		Pool->Alloc=0;

		free(Pool);
	}
}

////////////////////////////////////////

int		FwPoolCount(const tFwPool *Pool){
	int	Back=-1;

	if (Pool!=NULL){
		Back=Pool->Used;
	}

	return Back;
}

tFwPair	*FwPoolGet(const tFwPool *Pool,int Index){
	tFwPair	*Back=NULL;

	if (Pool!=NULL){
		if ((Index>=0)&&(Index<Pool->Used)){
			Back=Pool->Items[Index];
		}
	}

	return Back;
}

////////////////////////////////////////

// Index, -1 if error
int		FwPoolAdd(tFwPool *Pool,tFwPair *Item){
	int	Back=-1;

	if (Pool!=NULL){
		if (Pool->Used>=Pool->Alloc){
			void	*p;
			int	Size;

			Size=Pool->Alloc+POOL_ALLOC_SIZE;
			p=realloc(Pool->Items,Size*sizeof(tFwPair *));
			if (p!=NULL){
				Pool->Alloc=Size;
				Pool->Items=p;
			}
		}

		if (Pool->Used<=Pool->Alloc){
			Back=Pool->Used;
			Pool->Items[Back]=Item;
			Pool->Used+=1;
		}
	}

	return Back;
}

int		FwPoolRemove(tFwPool *Pool,int Index){
	int	Back=0;

	if (Pool!=NULL){
		if ((Index>=0)&&(Index<Pool->Used)){
			Pool->Used-=1;
			
			if (Index<Pool->Used){	// not last ...
				Pool->Items[Index]=Pool->Items[Pool->Used];
			}
			Back=1;
		}
	}

	return Back;
}

////////////////////////////////////////

// Index, -1 if none
int		FwPoolFindClosed(const tFwPool *Pool){
	int	Back=-1;

	if (Pool!=NULL){
		int	i;
	
		for (i=0;i<Pool->Used;i++){
			tFwPair	*Item;

			Item=Pool->Items[i];
			if (Item!=NULL){
				if (Item->Closed){
					Back=i;
					break;
				}
			}
		}
	}

	return Back;
}

