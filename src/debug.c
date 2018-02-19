static const char ___[]=" $Id: debug.c,v 1.4 2017/12/12 22:25:37 skalak Exp $ ";

#define _GNU_SOURCE

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>
#include <syslog.h>

#include "debug.h"

static int 	DebugSyslog=0;
static FILE	*DebugHandle=NULL;
static int	DebugClose=0;
static int 	DebugLevel=0;

////////////////////////////////////////////////////////////////////////////////

int qinit_syslog(const char *Name){
	openlog(Name,LOG_PID,LOG_DAEMON);
	DebugSyslog=1;
	return 1;
}

int qinit_file_handle(FILE *Handle){
	DebugClose=0;
	DebugHandle=Handle;
	return 1;
}

int qinit_file_name(const char *Name){
	int	Back=0;

	if (DebugHandle!=NULL){
		if (DebugClose){
			fclose(DebugHandle);
			DebugClose=0;
		}
	}

	DebugHandle=fopen(Name,"a");
	if (DebugHandle!=NULL){
		DebugClose=1;
		Back=1;
	}

	return Back;
}

void qdone(void){
	if (DebugSyslog){
		closelog();
		DebugSyslog=0;
	}
	if (DebugHandle!=NULL){
		if (DebugClose){
			fclose(DebugHandle);			
			DebugClose=0;
		}	
		DebugHandle=NULL;
	}
}

void qsetlevel(int Level){
	DebugLevel=Level;
}

////////////////////////////////////////////////////////////////////////////////

static
int qoutputv(int Level,const char *Format,va_list VaList){
	int	Back;

	if (DebugSyslog){
		int	SysLevel;
	
		SysLevel=LOG_INFO;
		vsyslog(SysLevel,Format,VaList);
	}
	if (DebugHandle!=NULL){
		Back=vfprintf(DebugHandle,Format,VaList);
		if (Back>0){
			fflush(DebugHandle);
		}
	}

	return Back;
}

static
int qoutputf(int Level,const char *Format,...){
	va_list	VaList;
	int		Back;

	va_start(VaList,Format);
	Back=qoutputv(Level,Format,VaList);
	va_end(VaList);

	return Back;
}

#define HEX_DEBUG_SIZE		16

static
int qhexoutput(int Level,const char *Msg,int Size,const void *Value){
	int	Back;

	Back=qoutputf(Level,"HexDebug '%s': Size=%d, Data=%p\n",Msg,Size,Value);
	if (Back>0){
		if ((Size>0)&&(Value!=NULL)){
			const unsigned char	*Data=Value;
			int						i;

			for (i=0;i<Size;i+=HEX_DEBUG_SIZE){
				char	 Buffer[256];
				int	 Pos,j;

				Pos=sprintf(Buffer,"  0x%04x:",i);
				for (j=0;j<HEX_DEBUG_SIZE;j++){
					if (i+j<Size){
						Pos+=sprintf(Buffer+Pos," %02x",Data[i+j]);
					}
					else{
						Pos+=sprintf(Buffer+Pos,"   ");
					}
				}
				Pos+=sprintf(Buffer+Pos," = '");
				for (j=0;j<HEX_DEBUG_SIZE;j++){
					unsigned char c;
	
					if (i+j<Size){
						c=Data[i+j];
						if ((c<' ')||(c>=0x7F)){	// 0x7F is DEL :-)
							c='.';
						}
					}
					else{
						c=' ';
					}
					Buffer[Pos++]=c;
				}
				Pos+=sprintf(Buffer+Pos,"'");
	
				qoutputf(Level,"%.*s\n",Pos,Buffer);
			}
		}
	}

	return Back;
}

////////////////////////////////////////////////////////////////////////////////

int qprintv(const char *Format,va_list VaList){
	return qoutputv(0,Format,VaList);
}

int qprintf(const char *Format,...){
	va_list	VaList;
	int		Back;

	va_start(VaList,Format);
	Back=qprintv(Format,VaList);
	va_end(VaList);

	return Back;
}

int qhexprint(const char *Msg,int Size,const void *Value){
	return qhexoutput(0,Msg,Size,Value);
}

////////////////////////////////////////////////////////////////////////////////

int qdebugv(int Level,const char *Format,va_list VaList){
	int	Back=0;

	if (Level<=DebugLevel){
		Back=qoutputv(Level,Format,VaList);
	}

	return Back;
}

int qdebugf(int Level,const char *Format,...){
	int	Back=0;

	if (Level<=DebugLevel){
		va_list	VaList;

		va_start(VaList,Format);
		Back=qoutputv(Level,Format,VaList);
		va_end(VaList);
	}

	return Back;
}

int qhexdebug(int Level,const char *Msg,int Size,const void *Value){
	int	Back=0;

	if (Level<=DebugLevel){
		Back=qhexoutput(Level,Msg,Size,Value);
	}

	return Back;
}

