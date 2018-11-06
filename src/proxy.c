static const char ___[]=" $Id: proxy.c,v 1.40 2018/05/08 21:39:46 skalak Exp $ ";

#define _GNU_SOURCE

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>

#include <errno.h>
#include <stdarg.h>
#include <poll.h>

#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/resource.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <libssh/libssh.h>
#include <libssh/ssh2.h>
#include <libssh/server.h>
#include <libssh/callbacks.h>
#include <libssh/server.h>

//#include <libssh/session.h>
//#include <libssh/message.h>
//#include <libssh/packet.h>

#include "debug.h"
#include "global.h"
#include "config.h"

#include "fw_pair.h"
#include "fw_pool.h"

#include "proxy.h"

#ifndef MAX
#define MAX(x,y)		(((x)>(y))?(x):(y))
#endif // MAX

#define MAX_RETRIES		3

////////////////////////////////////////////////////////////////////////////////

typedef struct{
					const struct sockaddr_in	*Sin;
					const tConfig	*Config;

					ssh_session		TrapSession;
					ssh_channel		TrapChannel;

					ssh_bind			Bind;
					int				Retries; 
					int				Error;
					int				Authenticated;
					int				Closed;

					struct ssh_channel_callbacks_struct *TrapChannelCB;
					struct ssh_channel_callbacks_struct *HaasChannelCB;

					int				HavePty;
					int				HaveShell;

					struct timeval	IdleTimer;
					struct timeval	SessionTimer;

					ssh_session		HaasSession;
					ssh_channel		HaasChannel;
		
					struct ssh_callbacks_struct			*ClientSshCB;

					// PORT FORWARDING ...
					struct ssh_channel_callbacks_struct *ForwardChannelCB;
					tFwPool			*ForwardPool;
				}tSshContext;

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

static
void TimerStart(struct timeval *Timer,int Sec){
	gettimeofday(Timer,NULL);
	Timer->tv_sec+=Sec;
}

static
int TimerGet(const struct timeval *Timer){
	struct timeval	Now;
	int				Back=0;

	gettimeofday(&Now,NULL);
	if (timercmp(&Now,Timer,<)){
		Back=Timer->tv_sec;
		Back-=Now.tv_sec;
		Back*=1000;
		Back+=Timer->tv_usec/1000;
		Back-=Now.tv_usec/1000;
	}

	return Back;
}

////////////////////////////////////////////////////////////////////////////////

static
void IdleTimerReset(tSshContext *Context){
	TimerStart(&Context->IdleTimer,Context->Config->IdleTimeout);
}

static
int IdleTimerGet(const tSshContext *Context){
	return TimerGet(&Context->IdleTimer);
}


////////////////////////////////////////////////////////////////////////////////

static
int CheckUsage(tSshContext *Context){
	int	Back=1;

	if (Context->Config->CpuUsage>0){
		struct rusage	Usage;
		int				i;

		i=getrusage(RUSAGE_SELF,&Usage);
		if (i!=0){
			qdebugf(QLEVEL_INFO,"Error, rusage(SELF) -> %d '%s' !!!\n",
									errno,strerror(errno));
			Back=0;
		}
		else{
			if (Usage.ru_utime.tv_sec>Context->Config->CpuUsage){
				Back=0;
			}
		}
	}

	return Back;
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

/*
static
void LoggingCallback(int Priority,const char *Function,const char *Buffer,
							void *UserData){
	qdebugf(QLEVEL_INFO,"libssh: %d %s() '%s'\n",Priority,Function,Buffer);
}
*/

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

#define CASE_STR(x)		case x: Str=#x; break;

static
const char *GetMsgTypeStr(int Type){
	const char	*Str;

	switch (Type){
		CASE_STR(SSH_REQUEST_AUTH);
		CASE_STR(SSH_REQUEST_CHANNEL_OPEN);
		CASE_STR(SSH_REQUEST_CHANNEL);
		CASE_STR(SSH_REQUEST_SERVICE);
		CASE_STR(SSH_REQUEST_GLOBAL);
		default:	Str="?!?Msg-Type?!?"; break;
	}

	return Str;
}

static
const char *GetMsgAuthSubtypeStr(int Subtype){
	const char	*Str;

	switch (Subtype){
		CASE_STR( SSH_AUTH_METHOD_UNKNOWN );
		CASE_STR( SSH_AUTH_METHOD_NONE );
		CASE_STR( SSH_AUTH_METHOD_PASSWORD );
		CASE_STR( SSH_AUTH_METHOD_PUBLICKEY );
		CASE_STR( SSH_AUTH_METHOD_HOSTBASED );
		CASE_STR( SSH_AUTH_METHOD_INTERACTIVE );
		CASE_STR( SSH_AUTH_METHOD_GSSAPI_MIC );
		default:	Str="?!?Auth-Subtype?!?"; break;
	}

	return Str;
}

static
const char *GetMsgChannelOpenSubtypeStr(int Subtype){
	const char	*Str;

	switch (Subtype){
		CASE_STR( SSH_CHANNEL_UNKNOWN );
		CASE_STR( SSH_CHANNEL_SESSION );
		CASE_STR( SSH_CHANNEL_DIRECT_TCPIP );
		CASE_STR( SSH_CHANNEL_FORWARDED_TCPIP );
		CASE_STR( SSH_CHANNEL_X11 );
		default:	Str="?!?Channel-Open-Subtype?!?"; break;
	}

	return Str;
}

static
const char *GetMsgChannelSubtypeStr(int Subtype){
	const char	*Str;

	switch (Subtype){
		CASE_STR( SSH_CHANNEL_REQUEST_UNKNOWN );
		CASE_STR( SSH_CHANNEL_REQUEST_PTY );
		CASE_STR( SSH_CHANNEL_REQUEST_EXEC );
		CASE_STR( SSH_CHANNEL_REQUEST_SHELL );
		CASE_STR( SSH_CHANNEL_REQUEST_ENV );
		CASE_STR( SSH_CHANNEL_REQUEST_SUBSYSTEM );
		CASE_STR( SSH_CHANNEL_REQUEST_WINDOW_CHANGE );
		CASE_STR( SSH_CHANNEL_REQUEST_X11 );
		default:	Str="?!?Channel-Subtype?!?"; break;
	}

	return Str;
}

static
const char *GetMsgGlobalSubtypeStr(int Subtype){
	const char	*Str;

	switch (Subtype){
		CASE_STR( SSH_GLOBAL_REQUEST_UNKNOWN );
		CASE_STR( SSH_GLOBAL_REQUEST_TCPIP_FORWARD );
		CASE_STR( SSH_GLOBAL_REQUEST_CANCEL_TCPIP_FORWARD );
		default:	Str="?!?Global-Subtype?!?"; break;
	}

	return Str;
}

static
void DebugMessage(const char *Name,int Type,int Subtype){
	const char	*TypeStr;

	TypeStr=GetMsgTypeStr(Type);
	qdebugf(QLEVEL_INFO,"\tType: %d '%s'\n",Type,TypeStr);

	if (Subtype!=-1){
		const char	*SubtypeStr;
		
		switch (Type){
			case SSH_REQUEST_AUTH:		
								SubtypeStr=GetMsgAuthSubtypeStr(Subtype);
							break;
			case SSH_REQUEST_CHANNEL_OPEN:
								SubtypeStr=GetMsgChannelOpenSubtypeStr(Subtype);
							break;
			case SSH_REQUEST_CHANNEL:
								SubtypeStr=GetMsgChannelSubtypeStr(Subtype);
							break;
			case SSH_REQUEST_GLOBAL:
								SubtypeStr=GetMsgGlobalSubtypeStr(Subtype);
							break;
			default:			
								SubtypeStr="?!?Msg-Type-Subtype?!?";
							break;
		}

		qdebugf(QLEVEL_INFO,"\tSubType: %d '%s'\n",Subtype,SubtypeStr);
	}
}

////////////////////////////////////////////////////////////////////////////////

/*
static const char *KbdIntTitle = "\n\nKeyboard-Interactive Authentication\n";
static const char *KbdIntInstruction = "Please enter your name and your password";
static const char *(KbdIntPrompts[2])={ "Name: ", "Password: " };
static char			KbdIntEcho[]={1,0};
*/

static
int TrapMessageCallback(ssh_session Session, ssh_message Msg, void *UserData){
	tSshContext	*Context=(tSshContext *)UserData;
	int			Back=1;	// don't handle, just log :-)

	qprintf("Trap MESSAGE Callback (%p,%p)\n",Msg,UserData);

	if (Msg!=NULL){
		int	Type,Subtype;

		Type=ssh_message_type(Msg);
		Subtype=ssh_message_subtype(Msg);

		DebugMessage("Trap",Type,Subtype);
/*
		if (Type==SSH_REQUEST_AUTH){
			if (Subtype==SSH_AUTH_METHOD_INTERACTIVE){
				if (!ssh_message_auth_kbdint_is_response(Msg)){
					const char	*User;

					User=ssh_message_auth_user(Msg);
					qprintf("User '%s' wants to auth with kbdint\n",User);
					ssh_message_auth_interactive_request(Msg,KbdIntTitle,KbdIntInstruction,
																	2,KbdIntPrompts,KbdIntEcho);				
				}
			}
		}
*/


		if (Type==SSH_REQUEST_CHANNEL_OPEN){
			if (Subtype==SSH_CHANNEL_DIRECT_TCPIP){
				const char	*Originator;
				int			OriginatorPort;
				const char	*Destination;
				int			DestinationPort;
				int			Mode;

				Originator=ssh_message_channel_request_open_originator(Msg);
				OriginatorPort=ssh_message_channel_request_open_originator_port(Msg);
				Destination=ssh_message_channel_request_open_destination(Msg);
				DestinationPort=ssh_message_channel_request_open_destination_port(Msg);

				qprintf("Originator: '%s':%d\n",Originator,OriginatorPort);
				qprintf("Destination: '%s':%d\n",Destination,DestinationPort);

				Mode=Context->Config->ForwardMode;
				if (Mode!=FORWARD_MODE_DENY){
					ssh_channel	SrcChannel=NULL;
					ssh_channel	DstChannel=NULL;
					int			Accept=0;
					int			Close=1;

					if (Mode!=FORWARD_MODE_ALLOW){
						Accept=1;
					}
					else{
						DstChannel=ssh_channel_new(Context->HaasSession);
						qprintf("ssh_channel new(HaasSession) -> %p\n",DstChannel);
						if (DstChannel!=NULL){
							int	i;

							i=ssh_channel_open_forward(DstChannel,
																Destination,DestinationPort,
																Originator,OriginatorPort);
							qprintf("ssh_channel_open_forward() -> %d\n",i);
							if (i){
								qdebugf(QLEVEL_DEBUG,"\terror: '%s'\n",ssh_get_error(Context->HaasSession));
								ssh_channel_free(DstChannel);
							}
							else{
								Accept=1;
							}
						}
					}

					if (Accept){
						SrcChannel=ssh_message_channel_request_open_reply_accept(Msg);
						qprintf("ssh_channel ssh_message_channel_request_open_reply_accept() -> %p\n",SrcChannel);
						if (SrcChannel==NULL){
							qdebugf(QLEVEL_DEBUG,"\terror: '%s'\n",ssh_get_error(Context->TrapSession));
						}
						else{
							Close=0;
						}
					}

					if (Mode==FORWARD_MODE_FAKE){
						usleep(500000);	// wait 0.5 sec ....
						Close=1;
					}

					if (!Close){
						tFwPair	*Pair;
						int		Index;

						Pair=FwPairAlloc(Context->ForwardChannelCB);
						qprintf("FwPairAlloc() -> %p\n",Pair);
						Index=FwPoolAdd(Context->ForwardPool,Pair);
						qprintf("FwPoolAdd() -> %d\n",Index);
						if (Index!=-1){
							Pair->Context=Context;

							Pair->Source.Addr=strdup(Originator);
							Pair->Source.Port=OriginatorPort;
							Pair->Source.Channel=SrcChannel;

							Pair->Destiny.Addr=strdup(Destination);
							Pair->Destiny.Port=DestinationPort;
							Pair->Destiny.Channel=DstChannel;

							ssh_set_channel_callbacks(SrcChannel,&Pair->Callbacks);

							if (DstChannel!=NULL){
								ssh_set_channel_callbacks(DstChannel,&Pair->Callbacks);
							}
						}
						else{
							FwPairRelease(Pair);
							Close=1;
						}
					}


					if (Close){
						int	Blocking,i;

						if (DstChannel!=NULL){
							Blocking=ssh_is_blocking(Context->HaasSession);
							ssh_set_blocking(Context->HaasSession,0);

							i=ssh_channel_close(DstChannel);
							qdebugf(QLEVEL_INFO,"ssh_channel_close(Forward) -> %d\n",i);
							if (i){
								qdebugf(QLEVEL_DEBUG,"\terror: '%s'\n",ssh_get_error(Context->HaasSession));
							}

							ssh_channel_free(DstChannel);

							ssh_set_blocking(Context->HaasSession,Blocking);
						}

						Blocking=ssh_is_blocking(Context->TrapSession);
						ssh_set_blocking(Context->TrapSession,0);

						i=ssh_channel_request_send_exit_status(SrcChannel,ECONNREFUSED);
						qdebugf(QLEVEL_INFO,"ssh_channel_request_send_exit_status(Forward) -> %d\n",i);
						if (i){
							qdebugf(QLEVEL_DEBUG,"\terror: '%s'\n",ssh_get_error(Context->TrapSession));
						}

						i=ssh_channel_close(SrcChannel);
						qdebugf(QLEVEL_INFO,"ssh_channel_close(Forward) -> %d\n",i);
						if (i){
							qdebugf(QLEVEL_DEBUG,"\terror: '%s'\n",ssh_get_error(Context->TrapSession));
						}

						ssh_channel_free(SrcChannel);

						ssh_set_blocking(Context->TrapSession,Blocking);
					}	// Close

					if (Accept){
						Back=0;	// Handled ...
					}
				}	// ! MODE_DENY
			}
		}
	}

	IdleTimerReset(Context);

	qprintf("Trap MESSAGE Callback () -> %d\n",Back);

	return Back;
}

static
int HaasMessageCallback(ssh_session Session, ssh_message Msg, void *UserData){
	tSshContext	*Context=(tSshContext *)UserData;
	int			Back=1;	// don't handle, just log :-)

	qprintf("Haas MESSAGE Callback (%p,%p)\n",Msg,UserData);

	if (Msg!=NULL){
		int	Type,Subtype;

		Type=ssh_message_type(Msg);
		Subtype=ssh_message_subtype(Msg);
		DebugMessage("HaaS",Type,Subtype);
	}

	IdleTimerReset(Context);

	qprintf("Haas MESSAGE Callback () -> %d\n",Back);

	return Back;
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

static
int HaasAuthenticate(ssh_session Session,const char *Password){
	int	Back=0;
	int	Ret;

	Ret=ssh_userauth_none(Session,NULL);
	qprintf("ssh_userauth_none() -> %d, 0x%04x\n",Ret,Ret);
	if (Ret==SSH_AUTH_ERROR){
		qprintf("\tSSH_AUTH_ERROR: '%s' !!!\n",ssh_get_error(Session));
	}
	else{
		char	*Banner;
		int	Method;

		Banner=ssh_get_issue_banner(Session);
		if (Banner!=NULL){
			qprintf("Banner: '%s'\n",Banner);
			ssh_string_free_char(Banner);
		}

		Method=ssh_userauth_list(Session,NULL);
		if (!(Method & SSH_AUTH_METHOD_PASSWORD)){
			qprintf("No SSH_AUTH_PASSWORD -> give up ....\n");
		}
		else{
			int	i;

			qprintf("Trying SSH_AUTH_PASSWORD ....\n");
			i=ssh_userauth_password(Session,NULL,Password);
			qprintf("ssh_userauth_password('%s') -> %d\n",Password,i);
			if (i==SSH_AUTH_SUCCESS){
				qprintf("\tSSH_AUTH_SUCCESS .... :-)\n");
				Back=1;
			}
			else{
				if (i==SSH_AUTH_ERROR){
					qprintf("\tSSH_AUTH_ERROR: '%s' !!!\n",ssh_get_error(Session));
				}
				if (i==SSH_AUTH_DENIED){
					qprintf("\tSSH_AUTH_DENIED: '%s' !!!\n",ssh_get_error(Session));
				}
			}
		}

	}

	return Back;
}

//#ifdef SSH_OPTIONS_STATUS_CALLBACK
static
void ClientStatusCallback(void *Context,float Progress){
	qprintf("ClientStatusCallback(%p,%1.1f)\n",Context,Progress);
}
//#endif // SSH_OPTIONS_STATUS_CALLBACK

static
int HaasConnect(tSshContext *Context,const char *User,const char *Password){
	ssh_session	Session;
	int			Back=0;

	Session=ssh_new();
	qdebugf(QLEVEL_INFO,"ssh_new(Haas) -> %p\n",Session);

	if (Session!=NULL){
		int	i;

		i=ssh_options_set(Session, SSH_OPTIONS_HOST, Context->Config->HaasAddr);
		qdebugf(QLEVEL_INFO,"ssh_options_set(HOST,'%s') -> %d\n",Context->Config->HaasAddr,i);

		i=ssh_options_set(Session, SSH_OPTIONS_PORT, &Context->Config->HaasPort);
		qdebugf(QLEVEL_INFO,"ssh_options_set(PORT_STR,%d) -> %d\n",Context->Config->HaasPort,i);

		{
			int	TimeOut=60;	// 60 sec ...

			i=ssh_options_set(Session, SSH_OPTIONS_TIMEOUT, &TimeOut);
			qdebugf(QLEVEL_INFO,"ssh_options_set(TIMEOUT,%d) -> %d\n",TimeOut,i);
		}

#ifdef SSH_OPTIONS_STATUS_CALLBACK
		i=ssh_options_set(Session, SSH_OPTIONS_STATUS_CALLBACK, &ClientStatusCallback);
		qdebugf(QLEVEL_INFO,"ssh_options_set(STATUS_CLB) -> %d\n",i);
#endif // SSH_OPTIONS_STATUS_CALLBACK

		ssh_set_callbacks(Session,Context->ClientSshCB);

		if (Context->Config->Foreground){
			i=ssh_options_set(Session, SSH_OPTIONS_USER, User);
			qdebugf(QLEVEL_INFO,"ssh_options_set(USER_STR,'%s') -> %d\n",User,i);
		}
		
		if (Context->Config->HaasLog!=NULL){
			i=ssh_options_set(Session, SSH_OPTIONS_LOG_VERBOSITY_STR, Context->Config->HaasLog);
			qdebugf(QLEVEL_INFO,"ssh_options_set(LOG_STR,'%s') -> %d\n",Context->Config->HaasLog,i);
		}

		ssh_set_message_callback(Session,&HaasMessageCallback,Context);

		i=ssh_connect(Session);
		qprintf("ssh_connect(Haas) -> %d\n",i);
		if (i!=0){
			qprintf("ssh_connect(Haas): failed '%s'\n",ssh_get_error(Session));
		}
		else{
			char	HaasPassword[1024];
			int	Pos=0;

			if (Context->Config->HaasToken[0]!='-'){
				Pos+=sprintf(HaasPassword+Pos,"{");
				Pos+=sprintf(HaasPassword+Pos," \"pass\": \"%s\",",Password);
				Pos+=sprintf(HaasPassword+Pos," \"device_token\": \"%s\",",Context->Config->HaasToken);
				Pos+=sprintf(HaasPassword+Pos," \"remote\": \"%s\",",inet_ntoa(Context->Sin->sin_addr));
				Pos+=sprintf(HaasPassword+Pos," \"remote_port\": \"%d\"",ntohs(Context->Sin->sin_port));
				Pos+=sprintf(HaasPassword+Pos," }");
			}
			else{
				Pos+=sprintf(HaasPassword+Pos,"%s",Password);
			}

			i=HaasAuthenticate(Session,HaasPassword);
			qprintf("HaasAuthenticate('%s') -> %d\n",HaasPassword,i);
			if (i>0){
				Context->HaasSession=Session;
				Session=NULL;
				Back=1;
			}
		}

		if (Session!=NULL){
			ssh_disconnect(Session);
			ssh_free(Session);
		}
	}

	return Back;
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////


static
int TrapChannelPtyRequest(ssh_session Session,ssh_channel Channel,
								const char *Term,int x,int y,int px,int py,
								void *UserData){
	tSshContext	*Context=(tSshContext *)UserData;
	int			Back=1;

	qdebugf(QLEVEL_INFO,"Trap Channel PTY Request ('%s',[%d,%d][%d,%d],%p)\n",
									Term,x,y,px,py,UserData);
	if (!Context->HavePty){
		if (Context->HaasChannel!=NULL){
			int	i;

			i=ssh_channel_request_pty_size(Context->HaasChannel,Term,x,y);	//,px,py);
			qdebugf(QLEVEL_INFO,"ssh_channel_request_pty() -> %d\n",i);
			if (i){
				qdebugf(QLEVEL_DEBUG,"\terror: '%s'\n",ssh_get_error(Context->HaasSession));
			}
			else{
				Context->HavePty=1;
			}
		}
	}

	if (Context->HavePty){
		Back=0;
	}

	IdleTimerReset(Context);

	qdebugf(QLEVEL_INFO,"Trap Channel PTY Request() -> %d\n",Back);

	return Back;
}

static
int TrapChannelPtyWindowChange(ssh_session Session, ssh_channel Channel, 
											int width, int height, int pxwidth, int pwheight,
											void *UserData){
	tSshContext	*Context=(tSshContext *)UserData;
	int			Back=1;

	qdebugf(QLEVEL_INFO,"Trap Channel PTY Window Change ([%d,%d][%d,%d],%p)\n",
									width,height,pxwidth,pwheight,UserData);
	if (Context->HavePty){
		if (Context->HaasChannel!=NULL){
			int	i;

			i=ssh_channel_change_pty_size(Context->HaasChannel,width,height);
			qdebugf(QLEVEL_DEBUG,"ssh_channel_change_pty_size() -> %d\n",i);
			if (i){
				qdebugf(QLEVEL_DEBUG,"\terror: '%s'\n",ssh_get_error(Context->HaasSession));
			}
			else{
				Back=0;
			}
		}
	}

	IdleTimerReset(Context);

	qdebugf(QLEVEL_INFO,"Trap Channel PTY Window Change() -> %d\n",Back);

	return Back;
}

static
int TrapChannelShellRequest(ssh_session Session,ssh_channel Channel,
								void *UserData){
	tSshContext	*Context=(tSshContext *)UserData;
	int			Back=1;	// error ....

	qdebugf(QLEVEL_INFO,"Trap Channel SHELL Request (%p)\n",UserData);

	if (!Context->HaveShell){
		if (Context->HaasChannel!=NULL){
			int	i;

			i=ssh_channel_request_shell(Context->HaasChannel);
			qprintf("ssh_channel_request_shell() -> %d\n",i);
			if (i){
				qdebugf(QLEVEL_DEBUG,"\terror: '%s'\n",ssh_get_error(Context->HaasSession));
			}
			else{
				Context->HaveShell=1;
			}
		}
	}

	if (Context->HaveShell){
		Back=0;
	}

	IdleTimerReset(Context);

	qdebugf(QLEVEL_INFO,"Trap Channel SHELL Request() -> %d\n",Back);

	return Back;
}

static
int TrapChannelExecRequest(ssh_session Session,ssh_channel Channel,
								const char *Command,void *UserData){
	tSshContext	*Context=(tSshContext *)UserData;
	int			Back=1;	// error ....

	qdebugf(QLEVEL_INFO,"Trap Channel EXEC Request ('%s',%p)\n",Command,UserData);

	if (Context->HaasChannel!=NULL){
		int	i;

		i=ssh_channel_request_exec(Context->HaasChannel,Command);
		qprintf("ssh_channel_request_exec() -> %d\n",i);
		if (i){
			qdebugf(QLEVEL_DEBUG,"\terror: '%s'\n",ssh_get_error(Context->HaasSession));
		}
		else{
			Back=0;
		}
	}

	IdleTimerReset(Context);

	qdebugf(QLEVEL_INFO,"Trap Channel EXEC Request() -> %d\n",Back);

	return Back;
}

static
int TrapChannelEnvRequest(ssh_session Session, ssh_channel Channel, 
										const char *EnvName, const char *EnvValue, void *UserData){
	tSshContext	*Context=(tSshContext *)UserData;
	int			Back=1;

	qdebugf(QLEVEL_INFO,"Trap Channel ENV Request ('%s','%s',%p)\n",
										EnvName,EnvValue,UserData);
	if (Context->HaasChannel!=NULL){
		int	i;

		i=ssh_channel_request_env(Context->HaasChannel,EnvName,EnvValue);
		qdebugf(QLEVEL_INFO,"ssh_channel_request_env() -> %d\n",i);
		if (i){
			qdebugf(QLEVEL_DEBUG,"\terror: '%s'\n",ssh_get_error(Context->HaasSession));
		}
		else{
			Back=0;
		}
	}

	IdleTimerReset(Context);

	qdebugf(QLEVEL_INFO,"Trap Channel ENV Request() -> %d\n",Back);

	return Back;
}

static
void TrapChannelSignalCallback(ssh_session Session, ssh_channel Channel, 
											const char *Signal, void *UserData){
	tSshContext	*Context=(tSshContext *)UserData;

	qprintf("Trap Channel SIGNAL Request ('%s',%p)\n",
											Signal,UserData);

	if (Context->HaasChannel!=NULL){
		int	i;

		i=ssh_channel_request_send_signal(Context->HaasChannel,Signal);
		qdebugf(QLEVEL_INFO,"ssh_channel_request_send_signal(Haas) -> %d\n",i);
		if (i){
			qdebugf(QLEVEL_DEBUG,"\terror: '%s'\n",ssh_get_error(Context->HaasSession));
		}
	}

	IdleTimerReset(Context);
}

static
void TrapChannelExitSignalCallback(ssh_session Session, ssh_channel Channel, 
											const char *Signal, int Core, const char *ErrMsg, 
											const char *Lang, void *UserData){
	tSshContext	*Context=(tSshContext *)UserData;

	qprintf("Trap Channel EXIT SIGNAL Callback(%s,%d,%s,%s)\n",
											Signal,Core,ErrMsg,Lang);

	if (Context->HaasChannel!=NULL){
		int	i;

		i=ssh_channel_request_send_exit_signal(Context->HaasChannel,Signal,
															Core,ErrMsg,Lang);
		qdebugf(QLEVEL_INFO,"ssh_channel_request_send_exit_signal(Haas) -> %d\n",i);
		if (i){
			qdebugf(QLEVEL_DEBUG,"\terror: '%s'\n",ssh_get_error(Context->HaasSession));
		}
	}

	IdleTimerReset(Context);
}

static
void TrapChannelExitStatusCallback(ssh_session Session, ssh_channel Channel, 
											int Status, void *UserData){
	tSshContext	*Context=(tSshContext *)UserData;

	qprintf("Trap Channel EXIT STATUS Callback(%d)\n",Status);

	if (Context->HaasChannel!=NULL){
		int	i;

		i=ssh_channel_request_send_exit_status(Context->HaasChannel,Status);
		qdebugf(QLEVEL_INFO,"ssh_channel_request_send_exit_status(Haas) -> %d\n",i);
		if (i){
			qdebugf(QLEVEL_DEBUG,"\terror: '%s'\n",ssh_get_error(Context->HaasSession));
		}
	}

	IdleTimerReset(Context);
}

static 
int TrapChannelSubsystemRequest(ssh_session Session, ssh_channel Channel,
                             const char *Subsystem, void *UserData) {
	tSshContext	*Context=(tSshContext *)UserData;

	qdebugf(QLEVEL_INFO,"Trap Channel SUBSYSTEM Request ('%s',%p)\n",
										Subsystem,UserData);

    /* subsystem requests behave simillarly to exec requests. */
//    if (strcmp(subsystem, "sftp") == 0) {
//        return exec_request(session, channel, SFTP_SERVER_PATH, userdata);
//    }

	IdleTimerReset(Context);

    return SSH_ERROR;
}

static
void TrapChannelAuthAgentCallback(ssh_session Session, ssh_channel Channel,
                                  void *UserData){
	tSshContext	*Context=(tSshContext *)UserData;

	qdebugf(QLEVEL_INFO,"Trap Channel AUTH_AGENT Callback (%p)\n",UserData);

	IdleTimerReset(Context);
}

static
void TrapChannelX11Callback(ssh_session Session, ssh_channel Channel,
                            int SingleConnection,
                            const char *AuthProtocol,const char *AuthCookie,
                            uint32_t ScreenNumber,void *UserData){
	tSshContext	*Context=(tSshContext *)UserData;

	qdebugf(QLEVEL_INFO,"Trap Channel X11 Callback (%d,'%s','%s',%d,%p)\n",
										SingleConnection,AuthProtocol,AuthCookie,
										ScreenNumber,UserData);

	IdleTimerReset(Context);
}

static
int TrapChannelDataCallback(ssh_session Session, ssh_channel Channel,
										void *Data,uint32_t Len,int IsStderr,void *UserData){
	tSshContext	*Context=(tSshContext *)UserData;
	int			Back=SSH_ERROR;

	qdebugf(QLEVEL_DEBUG,"Trap Channel DATA Callback(%p,%d,%d)\n",Data,Len,IsStderr);

	qhexdebug(QLEVEL_DEBUG,"TrapData",Len,Data);

	if (Context->HaasChannel!=NULL){
		int	Blocking;

		Blocking=ssh_is_blocking(Context->HaasSession);
		ssh_set_blocking(Context->HaasSession,0);
		if (!IsStderr){
			Back=ssh_channel_write(Context->HaasChannel,Data,Len);
			qdebugf(QLEVEL_INFO,"ssh_channel_write(Haas,%d) -> %d\n",Len,Back);
		}
		else{
			Back=ssh_channel_write_stderr(Context->HaasChannel,Data,Len);
			qdebugf(QLEVEL_INFO,"ssh_channel_write_stderr(Haas,%d) -> %d\n",Len,Back);
		}
		if (Back<0){
			qdebugf(QLEVEL_DEBUG,"\terror: '%s'\n",ssh_get_error(Context->HaasSession));
		}
		ssh_set_blocking(Context->HaasSession,Blocking);
	}

	IdleTimerReset(Context);

	qdebugf(QLEVEL_DEBUG,"Trap Channel DATA Callback() -> %d\n",Back);

	return Back;
}

static
void TrapChannelEofCallback(ssh_session Session, ssh_channel Channel, 
									void *UserData){
	tSshContext	*Context=(tSshContext *)UserData;

	qprintf("Trap Channel EOF Callback(%p)\n",UserData);

	if (Context->HaasChannel!=NULL){
		int	i;

		i=ssh_channel_send_eof(Context->HaasChannel);
		qprintf("ssh_channel_send_eof(Haas) -> %d\n",i);
		if (i){
			qdebugf(QLEVEL_DEBUG,"\terror: '%s'\n",ssh_get_error(Context->HaasSession));
		}
	}

	IdleTimerReset(Context);
}

static
void TrapChannelCloseCallback(ssh_session Session, ssh_channel Channel, 
									void *UserData){
	tSshContext	*Context=(tSshContext *)UserData;

	qprintf("Trap Channel CLOSE Callback(%p)\n",UserData);
	Context->Closed=1;
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

static
void HaasChannelSignalCallback(ssh_session Session, ssh_channel Channel, 
										const char *Signal, void *UserData){
	tSshContext	*Context=(tSshContext *)UserData;

	qprintf("Haas Channel SIGNAL Callback(%s)\n",Signal);

	if (Context->TrapChannel!=NULL){
		int	i;

		i=ssh_channel_request_send_signal(Context->TrapChannel,Signal);
		qdebugf(QLEVEL_INFO,"ssh_channel_request_send_signal(Trap) -> %d\n",i);
		if (i){
			qdebugf(QLEVEL_DEBUG,"\terror: '%s'\n",ssh_get_error(Context->TrapSession));
		}
	}

	IdleTimerReset(Context);
}


static
void HaasChannelExitSignalCallback(ssh_session Session, ssh_channel Channel, 
											const char *Signal, int Core, const char *ErrMsg, 
											const char *Lang, void *UserData){
	tSshContext	*Context=(tSshContext *)UserData;

	qprintf("Haas Channel EXIT SIGNAL Callback(%s,%d,%s,%s)\n",
											Signal,Core,ErrMsg,Lang);

	if (Context->TrapChannel!=NULL){
		int	i;

		i=ssh_channel_request_send_exit_signal(Context->TrapChannel,Signal,
															Core,ErrMsg,Lang);
		qdebugf(QLEVEL_INFO,"ssh_channel_request_send_exit_signal(Trap) -> %d\n",i);
		if (i){
			qdebugf(QLEVEL_DEBUG,"\terror: '%s'\n",ssh_get_error(Context->TrapSession));
		}
	}

	IdleTimerReset(Context);
}

static
void HaasChannelExitStatusCallback(ssh_session Session, ssh_channel Channel, 
											int Status, void *UserData){
	tSshContext	*Context=(tSshContext *)UserData;

	qprintf("Haas Channel EXIT STATUS Callback(%d)\n",Status);

	if (Context->TrapChannel!=NULL){
		int	i;

		i=ssh_channel_request_send_exit_status(Context->TrapChannel,Status);
		qdebugf(QLEVEL_INFO,"ssh_channel_request_send_exit_status(Trap) -> %d\n",i);
		if (i){
			qdebugf(QLEVEL_DEBUG,"\terror: '%s'\n",ssh_get_error(Context->TrapSession));
		}
	}

	IdleTimerReset(Context);
}

static
int HaasChannelDataCallback(ssh_session Session, ssh_channel Channel,
										void *Data,uint32_t Len,int IsStderr,void *UserData){
	tSshContext	*Context=(tSshContext *)UserData;
	int			Back=SSH_ERROR;

	qdebugf(QLEVEL_DEBUG,"Haas Channel DATA Callback(%p,%d,%d)\n",Data,Len,IsStderr);

	qhexdebug(QLEVEL_DEBUG,"HassData",Len,Data);

	if (Context->TrapChannel!=NULL){
		int	Blocking;

		Blocking=ssh_is_blocking(Context->TrapSession);
		ssh_set_blocking(Context->TrapSession,0);
		if (!IsStderr){
			Back=ssh_channel_write(Context->TrapChannel,Data,Len);
			qdebugf(QLEVEL_INFO,"ssh_channel_write(Trap,%d) -> %d\n",Len,Back);
		}
		else{
			Back=ssh_channel_write_stderr(Context->TrapChannel,Data,Len);
			qdebugf(QLEVEL_INFO,"ssh_channel_write_stderr(Trap,%d) -> %d\n",Len,Back);
		}
		if (Back<0){
			qdebugf(QLEVEL_DEBUG,"\terror: '%s'\n",ssh_get_error(Context->TrapSession));
		}
		ssh_set_blocking(Context->TrapSession,Blocking);
	}

	IdleTimerReset(Context);

	qdebugf(QLEVEL_DEBUG,"Haas Channel DATA Callback() -> %d\n",Back);

	return Back;
}

static
void HaasChannelEofCallback(ssh_session Session, ssh_channel Channel, 
									void *UserData){
	tSshContext	*Context=(tSshContext *)UserData;

	qprintf("Haas Channel EOF Callback(%p)\n",UserData);

	if (Context->TrapChannel!=NULL){
		int	i;

		i=ssh_channel_send_eof(Context->TrapChannel);
		qprintf("ssh_channel_send_eof(Trap) -> %d\n",i);
		if (i){
			qdebugf(QLEVEL_DEBUG,"\terror: '%s'\n",ssh_get_error(Context->TrapSession));
		}
	}

	IdleTimerReset(Context);
}

static
void HaasChannelCloseCallback(ssh_session Session, ssh_channel Channel, 
									void *UserData){
	tSshContext	*Context=(tSshContext *)UserData;

	qprintf("Haas Channel CLOSE Callback(%p)\n",UserData);
	Context->Closed=1;
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

static
int ForwardChannelDataCallback(ssh_session Session, ssh_channel Channel,
										void *Data,uint32_t Len,int IsStderr,void *UserData){
	tFwPair	*Pair=(tFwPair *)UserData;
	int		Back=SSH_ERROR;

	qdebugf(QLEVEL_INFO,"Forward Channel DATA Callback(%p,%d,%d)\n",Data,Len,IsStderr);

	qhexdebug(QLEVEL_DEBUG,"ForwardData",Len,Data);

	if (Pair!=NULL){
		tSshContext	*Context;

		Context=Pair->Context;
		if (Context!=NULL){
			int	Mode;

			Mode=Context->Config->ForwardMode;
			if (Mode==FORWARD_MODE_ALLOW){	// send to the other end ...
				ssh_session	FwSession;
				ssh_channel	FwChannel;
				const char	*FwName;
				int			Blocking;

				if (Channel==Pair->Source.Channel){
					FwChannel=Pair->Destiny.Channel;
					FwSession=Context->HaasSession;
					FwName="FwHaas";
				}
				else{
					FwChannel=Pair->Source.Channel;
					FwSession=Context->TrapSession;
					FwName="FwTrap";
				}

				Blocking=ssh_is_blocking(FwSession);
				ssh_set_blocking(FwSession,0);
				if (!IsStderr){
					Back=ssh_channel_write(FwChannel,Data,Len);
					qdebugf(QLEVEL_INFO,"ssh_channel_write(%s,%d) -> %d\n",FwName,Len,Back);
				}
				else{
					Back=ssh_channel_write_stderr(FwChannel,Data,Len);
					qdebugf(QLEVEL_INFO,"ssh_channel_write_stderr(%s,%d) -> %d\n",FwName,Len,Back);
				}
				if (Back<0){
					qdebugf(QLEVEL_DEBUG,"\terror: '%s'\n",ssh_get_error(FwSession));
				}
				ssh_set_blocking(FwSession,Blocking);
			}	// ALLOW

			if (Mode==FORWARD_MODE_ECHO){		// send back 
				int	Blocking;

				Blocking=ssh_is_blocking(Session);
				ssh_set_blocking(Session,0);
				if (!IsStderr){
					Back=ssh_channel_write(Channel,Data,Len);
					qdebugf(QLEVEL_INFO,"ssh_channel_write(FwEcho,%d) -> %d\n",Len,Back);
				}
				else{
					Back=ssh_channel_write_stderr(Channel,Data,Len);
					qdebugf(QLEVEL_INFO,"ssh_channel_write_stderr(FwEcho,%d) -> %d\n",Len,Back);
				}
				if (Back<0){
					qdebugf(QLEVEL_DEBUG,"\terror: '%s'\n",ssh_get_error(Session));
				}
				ssh_set_blocking(Session,Blocking);
			}	// ECHO 

			if (Mode==FORWARD_MODE_NULL){
				Back=Len;
			}	// NULL

			IdleTimerReset(Context);
		}	// Context != NULL
	}	// Pair != NULL

	qdebugf(QLEVEL_INFO,"Haas Channel DATA Callback() -> %d\n",Back);

	return Back;
}

static
void ForwardChannelEofCallback(ssh_session Session, ssh_channel Channel, 
									void *UserData){
	tFwPair	*Pair=(tFwPair *)UserData;

	qprintf("Forward Channel EOF Callback(%p)\n",UserData);

	if (Pair!=NULL){
		tSshContext	*Context;

		Context=Pair->Context;
		if (Context!=NULL){
			int	Mode;

			Mode=Context->Config->ForwardMode;
			if (Mode==FORWARD_MODE_ALLOW){	// send to the other end ...
				ssh_session	FwSession;
				ssh_channel	FwChannel;
				const char	*FwName;
				int			i;

				if (Channel==Pair->Source.Channel){
					FwChannel=Pair->Destiny.Channel;
					FwSession=Context->HaasSession;
					FwName="FwHaas";
				}
				else{
					FwChannel=Pair->Source.Channel;
					FwSession=Context->TrapSession;
					FwName="FwTrap";
				}

				i=ssh_channel_send_eof(FwChannel);
				qprintf("ssh_channel_send_eof(%s) -> %d\n",FwName,i);
				if (i){
					qdebugf(QLEVEL_DEBUG,"\terror: '%s'\n",ssh_get_error(FwSession));
				}
			}
	
			IdleTimerReset(Context);
		}	// Context != NULL
	}	// Pair != NULL
}

static
void ForwardChannelCloseCallback(ssh_session Session, ssh_channel Channel, 
									void *UserData){
	tFwPair	*Pair=(tFwPair *)UserData;

	qprintf("Forward Channel CLOSE Callback(%p)\n",UserData);
	Pair->Closed=1;
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

static
int TrapAuthNone(ssh_session Session, const char *User, void *UserData){
	tSshContext	*Context=(tSshContext *)UserData;
	int			Back=SSH_AUTH_DENIED;

	qprintf("Auth NONE: user '%s'\n",User);

	IdleTimerReset(Context);

	return Back;
}

static
int TrapAuthPassword(ssh_session Session,const char *User,
								const char *Password,void *UserData){
	tSshContext	*Context=(tSshContext *)UserData;
	int			Back=SSH_AUTH_DENIED;
	int			i;

	qprintf("Auth PASSWORD: user '%s' with '%s'\n",User,Password);

	i=HaasConnect(Context,User,Password);
	qprintf("HaasConnect('%s','%s') -> %d\n",User,Password,i);
	if (i){
		Context->Authenticated=1;
		Back=SSH_AUTH_SUCCESS;
	}
	else{
		Context->Retries+=1;
		if (Context->Retries>=MAX_RETRIES){
			qprintf("Too many authentication tries\n");
			ssh_disconnect(Session);
			Context->Error=1;
		}
	}

	IdleTimerReset(Context);

	return Back;
}

static
int TrapAuthPubkey(ssh_session Session, const char *User, 
							struct ssh_key_struct *Pubkey, 
							char SignatureState, void *UserData){
	tSshContext	*Context=(tSshContext *)UserData;
	int			Back=SSH_AUTH_DENIED;

	qprintf("Auth PUBKEY: user '%s' with %p at %d\n",User,Pubkey,SignatureState);

	IdleTimerReset(Context);

	return Back;
}

static
int TrapAuthGssapiMic(ssh_session session, const char *User, 
							 const char *Principal, void *UserData){
	tSshContext	*Context=(tSshContext *)UserData;
	int			Back=SSH_AUTH_DENIED;

	qprintf("Auth GSSAPI_MIC: user '%s' with '%s'\n",User,Principal);

	IdleTimerReset(Context);

	return Back;
}

static
ssh_channel TrapNewChannel(ssh_session Session,void *UserData){
	tSshContext	*Context=(tSshContext *)UserData;
	ssh_channel	Back=NULL;

	qdebugf(QLEVEL_DEBUG,"Trap new_session_channel .....\n");

	if (Context->TrapChannel==NULL){	// 1st
		ssh_channel	TrapChannel;

		TrapChannel=ssh_channel_new(Session);
		qdebugf(QLEVEL_DEBUG,"ssh_channel_new(Trap) -> %p\n",TrapChannel);
		if (TrapChannel!=NULL){
			ssh_channel	HaasChannel;

			HaasChannel=ssh_channel_new(Context->HaasSession);
			qdebugf(QLEVEL_DEBUG,"ssh_channel_new(Haas) -> %p\n",HaasChannel);
			if (HaasChannel!=NULL){
				int	i;

				i=ssh_channel_open_session(HaasChannel);
				qdebugf(QLEVEL_DEBUG,"ssh_channel_open_session(Haas) -> %d\n",i);
				if (i){
					qdebugf(QLEVEL_DEBUG,"\terror: '%s'\n",ssh_get_error(Context->HaasSession));
				}
				else{
					ssh_set_channel_callbacks(TrapChannel,Context->TrapChannelCB);
					ssh_set_channel_callbacks(HaasChannel,Context->HaasChannelCB);

					Context->TrapChannel=TrapChannel;
					Context->HaasChannel=HaasChannel;

					TrapChannel=NULL;
					HaasChannel=NULL;
				}

				if (HaasChannel!=NULL){
					int	Blocking;

					Blocking=ssh_is_blocking(Context->HaasSession);
					ssh_set_blocking(Context->HaasSession,0);

					ssh_channel_close(HaasChannel);
					ssh_channel_free(HaasChannel);

					ssh_set_blocking(Context->HaasSession,Blocking);
				}
			}

			if (TrapChannel!=NULL){
				int	Blocking;

				Blocking=ssh_is_blocking(Context->TrapSession);
				ssh_set_blocking(Context->TrapSession,0);

				ssh_channel_close(TrapChannel);
				ssh_channel_free(TrapChannel);

				ssh_set_blocking(Context->TrapSession,Blocking);
			}
		}
	}

	Back=Context->TrapChannel;

	IdleTimerReset(Context);

	return Back;
}

static 
int TrapServiceRequest(ssh_session Session,const char *Service, void *UserData) {
	tSshContext	*Context=(tSshContext *)UserData;
	int			Back=0;

	qdebugf(QLEVEL_INFO,"Trap SERVICE Request ('%s',%p)\n",Service,UserData);

	IdleTimerReset(Context);

	if (strcmp(Service,"ssh-userauth")==0){
		ssh_message	Msg=NULL;

		qprintf("Here should respond default OK\n");
//		Msg=ssh_message_get(Session);
		qprintf("ssh_message_get() -> %p\n",Msg);
		if (Msg!=NULL){
			ssh_message_service_reply_success(Msg);
			qprintf("And send auth banner ....\n");
			Back=1;	// handled ...
		}
	}

	return Back;
}


////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

static
void SessionClose(ssh_session Session,ssh_channel Channel){
	qprintf("SessionClose(%p,%p)\n",Session,Channel);

	if (Channel!=NULL){
		if (ssh_channel_is_open(Channel)){
			ssh_channel_set_blocking(Channel,0);	// for sure
			ssh_channel_send_eof(Channel);
			ssh_channel_close(Channel);
		}
		ssh_channel_free(Channel);
	}

	if (Session!=NULL){
		ssh_disconnect(Session);
		ssh_free(Session);
	}
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

static
void HandleForwards(tSshContext *Context){
	int	Count;

	Count=FwPoolCount(Context->ForwardPool);
	if (Count>0){
		int	i;

		for (i=0;i<Count;i++){
			tFwPair	*Pair;

			Pair=FwPoolGet(Context->ForwardPool,i);
			if (Pair!=NULL){
				if (Pair->Source.Channel!=NULL){
					int	Len;

					Len=ssh_channel_poll(Pair->Source.Channel,0);
					if (Len!=0){
						qdebugf(QLEVEL_INFO,"ssh_channel_poll(Fw-Src) -> %d\n",Len);
					}
					if (Len<0){
						qdebugf(QLEVEL_INFO,"\terror: '%s'\n",ssh_get_error(Context->TrapSession));
						if (Len==SSH_EOF){
							Pair->Closed=1;
						}
					}
				}
				if (Pair->Destiny.Channel!=NULL){
					int	Len;

					Len=ssh_channel_poll(Pair->Destiny.Channel,0);
					if (Len!=0){
						qdebugf(QLEVEL_INFO,"ssh_channel_poll(Fw-Dst) -> %d\n",Len);
					}
					if (Len<0){
						qdebugf(QLEVEL_INFO,"\terror: '%s'\n",ssh_get_error(Context->HaasSession));
						if (Len==SSH_EOF){
							Pair->Closed=1;
						}
					}
				}
			}
		}	// for

		while(1){
			tFwPair	*Pair;
			int		Index;

			Index=FwPoolFindClosed(Context->ForwardPool);
			if (Index<0){
				break;
			}
			qprintf("Found Closed Forward at %d\n",Index);
			Pair=FwPoolGet(Context->ForwardPool,Index);
			FwPoolRemove(Context->ForwardPool,Index);

			if (Pair!=NULL){
				FwPairRelease(Pair);								
			}
		}
	}
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

//#ifdef SSH_OPTIONS_STATUS_CALLBACK
static
void ServerStatusCallback(void *Context,float Progress){
	qprintf("ServerStatusCallback(%p,%1.1f)\n",Context,Progress);
}
//#endif // SSH_OPTIONS_STATUS_CALLBACK

////////////////////////////////////////////////////////////////////////////////

void ProxyProcess(int TrapSock,struct sockaddr_in *Sin,const tConfig *Config){
	tSshContext	Context;
	
	struct ssh_server_callbacks_struct TrapSessionCB = {
						.userdata = &Context,
						.auth_none_function = TrapAuthNone,
						.auth_password_function = TrapAuthPassword,
						.auth_pubkey_function = TrapAuthPubkey,
						.auth_gssapi_mic_function = TrapAuthGssapiMic,
						.service_request_function = TrapServiceRequest,
						.channel_open_request_session_function = TrapNewChannel,
				};

	struct ssh_channel_callbacks_struct TrapChannelCB = {
						.userdata = &Context,
						.channel_pty_request_function = TrapChannelPtyRequest,
						.channel_pty_window_change_function = TrapChannelPtyWindowChange,
						.channel_env_request_function = TrapChannelEnvRequest,
						.channel_shell_request_function = TrapChannelShellRequest,
						.channel_auth_agent_req_function = TrapChannelAuthAgentCallback,
						.channel_x11_req_function = TrapChannelX11Callback,
						.channel_exec_request_function = TrapChannelExecRequest,
						.channel_subsystem_request_function = TrapChannelSubsystemRequest,
						.channel_signal_function = TrapChannelSignalCallback,
						.channel_exit_signal_function = TrapChannelExitSignalCallback,
						.channel_exit_status_function = TrapChannelExitStatusCallback,
						.channel_data_function = TrapChannelDataCallback,
						.channel_eof_function = TrapChannelEofCallback,
						.channel_close_function = TrapChannelCloseCallback,
					};

	struct ssh_channel_callbacks_struct HaasChannelCB = {
						.userdata = &Context,
						.channel_signal_function = HaasChannelSignalCallback,
						.channel_exit_signal_function = HaasChannelExitSignalCallback,
						.channel_exit_status_function = HaasChannelExitStatusCallback,
						.channel_data_function = HaasChannelDataCallback,
						.channel_eof_function = HaasChannelEofCallback,
						.channel_close_function = HaasChannelCloseCallback,
					};


	struct ssh_channel_callbacks_struct ForwardChannelCB = {
//						.userdata = &Context,	// will fill self !!!!
						.channel_data_function = ForwardChannelDataCallback,
						.channel_eof_function = ForwardChannelEofCallback,
						.channel_close_function = ForwardChannelCloseCallback,
					};

	struct ssh_callbacks_struct ServerSshCB = {
						.userdata = &Context,
						.connect_status_function = ServerStatusCallback,
					};

	struct ssh_callbacks_struct ClientSshCB = {
						.userdata = &Context,
						.connect_status_function = ClientStatusCallback,
					};

	struct timeval	StartTv,StopTv;
	ssh_session		TrapSession;
	ssh_bind			Bind;
	int				Error=0;
	int				i;

	gettimeofday(&StartTv,NULL);

/*
	i=ssh_set_log_callback(&LoggingCallback);
	qdebugf(QLEVEL_INFO,"ssh_set_log_callback() -> %d\n",i);

	i=ssh_set_log_level(SSH_LOG_TRACE);
	qdebugf(QLEVEL_INFO,"ssh_set_log_level(%d) -> %d\n",SSH_LOG_TRACE,i);
*/

	TrapSession=ssh_new();
	qdebugf(QLEVEL_INFO,"ssh_new(Trap) -> %p\n",TrapSession);

	Bind=ssh_bind_new();
	qdebugf(QLEVEL_INFO,"ssh_bind_new() -> %p\n",Bind);

	if (Config->TrapLog!=NULL){
		i=ssh_bind_options_set(Bind,SSH_BIND_OPTIONS_LOG_VERBOSITY_STR,Config->TrapLog);
		qdebugf(QLEVEL_INFO,"ssh_bind_options_set(LOG_STR,'%s') -> %d\n",Config->TrapLog,i);
	}

	ssh_callbacks_init(&ServerSshCB);
	ssh_set_callbacks(TrapSession,&ServerSshCB);
	
#ifdef SSH_OPTIONS_STATUS_CALLBACK
	i=ssh_options_set(TrapSession, SSH_OPTIONS_STATUS_CALLBACK, &ServerStatusCallback);
	qdebugf(QLEVEL_INFO,"ssh_options_set(STATUS_CLB) -> %d\n",i);
#endif // SSH_OPTIONS_STATUS_CALLBACK

	{
		char	KeyPath[1024];
		int	Pos;

		Pos=sprintf(KeyPath,"%s",Config->KeysDir);
		if (KeyPath[Pos-1]!='/'){
			KeyPath[Pos++]='/';
		}

		sprintf(KeyPath+Pos,"ssh_host_dsa_key");
		i=ssh_bind_options_set(Bind, SSH_BIND_OPTIONS_DSAKEY, KeyPath );
		qdebugf(QLEVEL_INFO,"ssh_bind_options_set(DSAKEY,'%s') -> %d\n",KeyPath,i);

		sprintf(KeyPath+Pos,"ssh_host_rsa_key");
		i=ssh_bind_options_set(Bind, SSH_BIND_OPTIONS_RSAKEY, KeyPath);
		qdebugf(QLEVEL_INFO,"ssh_bind_options_set(RSAKEY,'%s') -> %d\n",KeyPath,i);

#ifdef  SSH_BIND_OPTIONS_ECDSAKEY
		sprintf(KeyPath+Pos,"ssh_host_ecc_key");
		i=ssh_bind_options_set(Bind, SSH_BIND_OPTIONS_ECDSAKEY, KeyPath);
		qdebugf(QLEVEL_INFO,"ssh_bind_options_set(ECDSAKEY,'%s') -> %d\n",KeyPath,i);
#endif // SSH_BIND_OPTIONS_ECDSAKEY
	}

	i=ssh_bind_options_set(Bind, SSH_BIND_OPTIONS_BANNER, Config->Banner);
	qdebugf(QLEVEL_INFO,"ssh_bind_options_set(BANNER) -> %d\n",i);

	i=ssh_bind_accept_fd(Bind,TrapSession,TrapSock);
	qdebugf(QLEVEL_INFO,"ssh_bind_accept_fd() -> %d\n",i);

	memset(&Context,0,sizeof(Context));

	Context.Sin=Sin;
	Context.Config=Config;

	Context.TrapSession=TrapSession;
	Context.Bind=Bind;
	Context.Retries=0;
	Context.Authenticated=0;
	Context.TrapChannel=NULL;

	Context.HavePty=0;
	Context.HaveShell=0;

	Context.HaasSession=NULL;
	Context.HaasChannel=NULL;

	ssh_callbacks_init(&TrapChannelCB);
	Context.TrapChannelCB=&TrapChannelCB;

	ssh_callbacks_init(&HaasChannelCB);
	Context.HaasChannelCB=&HaasChannelCB;

	ssh_callbacks_init(&ClientSshCB);
	Context.ClientSshCB=Context.ClientSshCB;

	ssh_callbacks_init(&ForwardChannelCB);
	Context.ForwardChannelCB=&ForwardChannelCB;
	if ((Config->ForwardMode!=FORWARD_MODE_DENY)
		 &&(Config->ForwardMode!=FORWARD_MODE_FAKE)){
		Context.ForwardPool=FwPoolInit();
	}
	else{
		Context.ForwardPool=NULL;
	}

	ssh_callbacks_init(&TrapSessionCB);
	TrapSessionCB.userdata=&Context;
	ssh_set_server_callbacks(TrapSession,&TrapSessionCB);

	ssh_set_message_callback(TrapSession,&TrapMessageCallback,&Context);

	i=ssh_handle_key_exchange(TrapSession);
	qprintf("ssh_handle_key_exchange() -> %d\n",i);
	if (i!=0){
		qprintf("\t-> '%s'\n",ssh_get_error(TrapSession));
		Error=1;
	}

	if (!Error){
		ssh_event	MainLoop;
		int			AuthMethods;

		AuthMethods=0;
		AuthMethods|=SSH_AUTH_METHOD_PASSWORD;
//		AuthMethods|=SSH_AUTH_METHOD_INTERACTIVE;
		AuthMethods|=SSH_AUTH_METHOD_PUBLICKEY;
		AuthMethods|=SSH_AUTH_METHOD_GSSAPI_MIC;

		ssh_set_auth_methods(TrapSession,AuthMethods);
		qprintf("ssh_set_auth_methods(0x%04x)\n",AuthMethods);

		MainLoop=ssh_event_new();
		qdebugf(QLEVEL_INFO,"ssh_event_new() -> %p\n",MainLoop);

		i=ssh_event_add_session(MainLoop,TrapSession);
		qdebugf(QLEVEL_INFO,"ssh_event_add_session(Trap) -> %d\n",i);

		IdleTimerReset(&Context);
		if (Config->SessionTimeout>0){
			TimerStart(&Context.SessionTimer,Config->SessionTimeout);
		}

		while ((!Error)&&(!ShouldStop)){
			int	Ret;
			int	Time;

			if (!CheckUsage(&Context)){
				qprintf("Usage limit reached :-(\n");
				Error=1;
				break;
			}

			Time=IdleTimerGet(&Context);
			if (Time==0){
				qprintf("IdleTimer - timeout :-(\n");
				Error=1;
				break;
			}

			if (Config->SessionTimeout>0){
				int	i;

				i=TimerGet(&Context.SessionTimer);
				if (i==0){
					qprintf("SessionTimer - timeout :-(\n");
					Error=1;
					break;
				}
				if (i<Time){
					Time=i;
				}
			}

			Ret=ssh_event_dopoll(MainLoop,Time);
			if (Ret==SSH_EINTR){
				continue;
			}
			if (Ret==SSH_ERROR){
				qprintf("ssh_event_dopoll() -> %d\n",Ret);
				Error=1;
				break;
			}

			if (Context.Closed){
				Error=1;
				break;
			}

			if ((Context.TrapChannel!=NULL)&&(Context.Authenticated)){
				break;
			}
		}	// wait for events .....

		if ((!Error)&&(!ShouldStop)){
			if ((Context.TrapChannel!=NULL)&&(Context.Authenticated)){

				i=ssh_event_add_session(MainLoop,Context.HaasSession);
				qprintf("ssh_event_add_session(HaaS) -> %d\n",i);

				IdleTimerReset(&Context);

				while ((!Error)&&(!ShouldStop)){
					int	Ret;
					int	Time;
	
					// flush all pending packets ....
					ssh_channel_poll(Context.TrapChannel,0);
					ssh_channel_poll(Context.HaasChannel,0);

					if (Context.ForwardPool!=NULL){
						HandleForwards(&Context);
					}	// ForwardPool != NULL

					ssh_blocking_flush(Context.TrapSession,1000);	// 1 sec
					ssh_blocking_flush(Context.HaasSession,1000);	// 1 sec

					Time=IdleTimerGet(&Context);
					if (Time==0){
						qprintf("IdleTimer - timeout :-(\n");
						break;
					}
					if (Config->SessionTimeout>0){
						int	i;

						i=TimerGet(&Context.SessionTimer);
						if (i==0){
							qprintf("SessionTimer - timeout :-(\n");
							Error=1;
							break;
						}
						if (i<Time){
							Time=i;
						}
					}

					Ret=ssh_event_dopoll(MainLoop,Time);
					if (Ret==SSH_EINTR){
						continue;
					}
					if (Ret==SSH_ERROR){
						qprintf("ssh_event_dopoll() -> %d\n",Ret);
						Error=1;
						break;
					}

					if (Context.Closed){
						break;
					}
				}	// wait for events .....

				ssh_event_remove_session(MainLoop,Context.HaasSession);
			}
		}

		ssh_event_remove_session(MainLoop,TrapSession);
		ssh_event_free(MainLoop);
	}

	if (Context.ForwardPool!=NULL){
		
		FwPoolDone(Context.ForwardPool);
	}

	SessionClose(Context.HaasSession,Context.HaasChannel);

	ssh_bind_free(Bind);

	SessionClose(Context.TrapSession,Context.TrapChannel);

	gettimeofday(&StopTv,NULL);

	i=StopTv.tv_sec-StartTv.tv_sec;
	i*=1000;
	i+=StopTv.tv_usec/1000;
	i-=StartTv.tv_usec/1000;

	qprintf("Client ending (%d msec)\n",i);
}

void ProxyInit(void){
	int	i;

	i=ssh_init();
	qdebugf(QLEVEL_INFO,"ssh_init() -> %d\n",i);
}

void ProxyDone(void){
	int	i;

	i=ssh_finalize();
	qprintf("ssh_finalize() -> %d\n",i);
}
