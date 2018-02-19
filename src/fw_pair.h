// $Id: fw_pair.h,v 1.2 2017/12/27 21:37:35 skalak Exp $ 

#ifndef _FW_PAIR_H_
#define _FW_PAIR_H_

#include <libssh/libssh.h>
#include <libssh/callbacks.h>

typedef struct{
				char			*Addr;
				int			Port;
				ssh_channel	Channel;
			}tFwEndpoint;

typedef struct{
				void			*Context;
				int			Closed;
				tFwEndpoint	Source;
				tFwEndpoint	Destiny;

				// need separate ...
				struct ssh_channel_callbacks_struct	Callbacks;
			}tFwPair;

tFwPair	*FwPairAlloc(const struct ssh_channel_callbacks_struct *Callbacks);
void		FwPairRelease(tFwPair *Item);

#endif // _FW_PAIR_H_

