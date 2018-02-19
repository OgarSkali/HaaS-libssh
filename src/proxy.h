// $Id: proxy.h,v 1.2 2017/12/12 21:17:43 skalak Exp $ 

#ifndef _PROXY_H_
#define _PROXY_H_

#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
//#include <netinet/in.h>
//#include <arpa/inet.h>

#include "config.h"

void ProxyInit(void);
void ProxyDone(void);

void ProxyProcess(int TrapSock,struct sockaddr_in *Sin,const tConfig *Config);

#endif // _PROXY_H_

