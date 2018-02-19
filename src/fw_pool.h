// $Id: fw_pool.h,v 1.2 2017/12/27 23:06:13 skalak Exp $ 

#ifndef _FW_POOL_H_
#define _FW_POOL_H_

#include "fw_pair.h"

struct _t_Fw_Pool_;
typedef struct _t_Fw_Pool_ tFwPool;

tFwPool	*FwPoolInit(void);
void		FwPoolDone(tFwPool *Pool);

int		FwPoolCount(const tFwPool *Pool);
tFwPair	*FwPoolGet(const tFwPool *Pool,int Index);

// Index, -1 if error
int		FwPoolAdd(tFwPool *Pool,tFwPair *Pair);
int		FwPoolRemove(tFwPool *Pool,int Index);

// Index, -1 if none
int		FwPoolFindClosed(const tFwPool *Pool);

#endif // _FORWARD_H_

