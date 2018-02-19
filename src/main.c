#define _GNU_SOURCE

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "debug.h"
#include "global.h"
#include "config.h"
#include "proxy.h"

static int		MasterPid;
volatile int	ShouldStop;

static
void SigHandler(int Signal){
	qprintf("Caught signal %d '%s' !!!!\n",Signal,strsignal(Signal));

	if ((Signal==SIGHUP)||(Signal==SIGINT)){
		qprintf("Stopping .....\n");
		ShouldStop=1;
	}

	while (Signal==SIGCHLD){
		int	ChildPid,Status;
	
		ChildPid=waitpid(-1,&Status,WNOHANG);
		if (ChildPid<=0){
			break;
		}
		qprintf("Child %d ended with %d ....\n",ChildPid,Status);
	}
}

////////////////////////////////////////////////////////////////////////////////

static
int CreateTCPServer(int Port){
	int	Socket;
	int	Back=-1;

	Socket=socket(PF_INET,SOCK_STREAM,IPPROTO_TCP);
	if (Socket==-1){
		qprintf("socket(INET,STREAM,TCP) -> %d '%s' !!!\n",errno,strerror(errno));
	}
	else{
		int	On=1;
		int	i;

		i=setsockopt(Socket,SOL_SOCKET,SO_REUSEADDR,&On,sizeof(On));
		if (i!=0){
			qprintf("setsockopt(SO_REUSEADDR) -> %d '%s' !!!\n",errno,strerror(errno));
		}
		else{
			struct sockaddr_in	Sin;

			Sin.sin_family=AF_INET;
			Sin.sin_port=htons(Port);
			Sin.sin_addr.s_addr=htonl(INADDR_ANY);

			i=bind(Socket,(struct sockaddr *)&Sin,sizeof(Sin));	
			if (i!=0){	
				qprintf("bind(%d) -> %d '%s' !!!\n",Port,errno,strerror(errno));
			}
			else{
				i=listen(Socket,10);
				if (i!=0){
					qprintf("listen() -> %d '%s' !!!\n",errno,strerror(errno));
				}
				else{
					Back=Socket;
					Socket=-1;
				}
			}
		}
	}

	if (Socket!=-1){
		close(Socket);
	}

	return Back;
}

////////////////////////////////////////////////////////////////////////////////

static
int ProcessConsole(int fd){
	unsigned char	Tmp;
	int				i;
	int				Back=0;

	do{
		i=read(fd,&Tmp,1);
	}while ((i==-1)&&(errno==EINTR));
					
	if (i<0){
		qprintf("recv(STDIN) -> %d '%s' !!!\n",errno,strerror(errno));
		Back=-1;
	}

	if (i==0){
		qprintf("recv(STDIN) -> EOF !?!?!\n");
		Back=-1;
	}

	if (i>0){
		qprintf("recv(STDIN) -> %c %d 0x%02x\n",
							(((Tmp<' ')||(Tmp>'~'))?'.':Tmp),Tmp,Tmp);

		if (Tmp==0x1B){	// ESC
			Back=1;
		}
	}

	return Back;
}


////////////////////////////////////////////////////////////////////////////////

static
int daemonize(int ProtectHandle){
	int	Back=0;
	int	i;

	i=fork();
	if (i==-1){
		qprintf("Error, 1st fork() -> %d '%s'\n",errno,strerror(errno));
	}
	if (i==0){	// child ...
		i=setsid();
		if (i==-1){
			qprintf("Error, setsid() -> %d '%s'\n",errno,strerror(errno));
		}
		else{
			i=fork();
			if (i==-1){
				qprintf("Error, 1st fork() -> %d '%s'\n",errno,strerror(errno));
			}
			if (i==0){	// child ...
				if (chdir("/")<0){
					qprintf("Error, chdir('/') -> %d '%s'\n",errno,strerror(errno));
				}
				else{
					int	Handle;

					umask(0);

					Handle=open("/dev/null",O_RDWR);
					if (Handle==-1){
						qprintf("Error, open('/dev/null',RW) -> %d '%s'\n",errno,strerror(errno));
					}
					else{
						for (i=0;i<3;i++){
							if (i!=ProtectHandle){
								dup2(Handle,i);
							}
						}
						close(Handle);

						MasterPid=getpid();
						Back=1;
					}
				}
			}
		}
	}

	return Back;
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

int main(int argc,char *argv[]){
	int		Back=0;
	int		Work=1;
	tConfig	Config;

	MasterPid=getpid();

	signal(SIGPIPE,SigHandler);
	signal(SIGINT,SigHandler);
	signal(SIGHUP,SigHandler);
	signal(SIGCHLD,SigHandler);

	ConfigInit(&Config);
	if (!ConfigParse(argc,argv,&Config)){
		PrintUsage(argv[0]);
		Back=1;
	}
	if (Back==0){	// still good ...
		if (Config.WantHelp){
			PrintUsage(argv[0]);
			Work=0;
		}
	}

	if ((Back==0)&&(Work)){
		if (Config.Foreground){
			qinit_file_handle(stderr);
		}
		else{
			qinit_syslog("HaaS");
		}

		qsetlevel(Config.DebugLevel);

		qprintf("Starting on port %d\n",Config.TrapPort);
		qprintf("with HaaS at '%s:%s' and token '%s'\n",
						Config.HaasAddr,Config.HaasPort,Config.HaasToken);
	}

	if ((Back==0)&&(Work)){		
		if (!Config.Foreground){
			int	i;

			i=daemonize(-1);
			if (i<0){
				printf("Error, going to daemon ....\n");
			}
			if (i<=0){
				Work=0;
			}
		}
	}
	
	if ((Back==0)&&(Work)){
		int	MasterSock;

		MasterSock=CreateTCPServer(Config.TrapPort);
		if (MasterSock<0){
			Back=2;
		}
		else{
			int	Max;

			ProxyInit();

			Max=MasterSock+1;
			while(!ShouldStop){
				fd_set			FDSet;
				struct timeval	Tv;
				int				i;

				FD_ZERO(&FDSet);
				if (Config.Foreground){
					FD_SET(0,&FDSet);
				}
				FD_SET(MasterSock,&FDSet);

				Tv.tv_sec=Tv.tv_usec=1;

				i=select(Max,&FDSet,0,0,&Tv);
				if (i<0){
					if (errno!=EINTR){
						qprintf("select(%d) -> %d '%s' !!!\n",
											Max,errno,strerror(errno));
						break;
					}
				}

				if (i>0){
					if (FD_ISSET(MasterSock,&FDSet)){
						struct sockaddr_in	Sin;
						int						Size;
						int						ChildSock;

						do{
							Size=sizeof(Sin);
							ChildSock=accept(MasterSock,(struct sockaddr *)&Sin,&Size);
						}while ((ChildSock==-1)&&(errno==EINTR));

						if (ChildSock<0){
							qprintf("accept(%d) -> %d '%s' !!!\n",
											MasterSock,errno,strerror(errno));
						}
						else{
							int	Pid;
	
							qprintf("Accepted client '%s:%d' on socket %d\n",
											inet_ntoa(Sin.sin_addr),htons(Sin.sin_port),ChildSock);

							Pid=fork();
							if (Pid==-1){
								qprintf("fork() -> %d '%s' !!!\n",errno,strerror(errno));
							}
							else{
								if (Pid==0){	// child ...
									close(MasterSock);
									MasterSock=-1;
									
									ProxyProcess(ChildSock,&Sin,&Config);
								}
							}
	
							close(ChildSock);	// always :-)
							if (Pid==0){		// child ...
								break;
							}
						}	// ChildSock
					}	// FD_ISSET(MasterSock)

					if (Config.Foreground){
						if (FD_ISSET(0,&FDSet)){
							i=ProcessConsole(0);
							if (i!=0){
								break;
							}
						}
					}	// Config.Foreground
				}	// select()
			}	// while (!ShouldStop)
		
			if (MasterSock!=-1){
				close(MasterSock);
			}

			ProxyDone();
		}	// MasterSock
	}	// Config

	qdone();

	return Back;
}

