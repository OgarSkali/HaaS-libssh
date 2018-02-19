// $Id: debug.h,v 1.3 2017/12/12 12:01:15 skalak Exp $ 

#ifndef _DEBUG_H_
#define _DEBUG_H_

#include <stdarg.h>

#define QLEVEL_ERROR		1
#define QLEVEL_WARNING	2
#define QLEVEL_INFO		3
#define QLEVEL_DEBUG		4

int  qinit_syslog(const char *Name);
int  qinit_file_handle(FILE *Handle);
int  qinit_file_name(const char *Name);

void qdone(void);

void qsetlevel(int Level);

int  qprintv(const char *Format,va_list VaList);
int  qprintf(const char *Format,...);
int  qhexprint(const char *Msg,int Size,const void *Value);

int  qdebugv(int Level,const char *Format,va_list VaList);
int  qdebugf(int Level,const char *Format,...);
int  qhexdebug(int Level,const char *Msg,int Size,const void *Value);

#endif // _DEBUG_H_
