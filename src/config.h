// $Id: config.h,v 1.12 2018/05/08 21:39:46 skalak Exp $

#ifndef _CONFIG_H_
#define _CONFIG_H_

//#define DEFAULT_KEYS_FOLDER		"./keys/"
#define DEFAULT_KEYS_FOLDER		"/etc/HaaS/keys/"

#define DEFAULT_IDLE_TIMEOUT		30		// 30 sec

#define DEFAULT_PROCESS_NICE		0		// nothing :-)

#define DEFAULT_CONN_LIMIT			30		// 30 clients max

#define DEFAULT_TRAP_PORT			2222

#define DEFAULT_HAAS_HOST			"haas-app.nic.cz"
#define DEFAULT_HAAS_PORT			10000

#define DEFAULT_BANNER_STR			"OpenSSH_7.2p2 Ubuntu-4ubuntu2.2"

enum{
	FORWARD_MODE_DENY,		//			0
	FORWARD_MODE_ALLOW,		//			1
	FORWARD_MODE_FAKE,		//			2
	FORWARD_MODE_NULL,		//			3
	FORWARD_MODE_ECHO,		//			4
	FORWARD_MODE_MAX,	
};

#define DEFAULT_FORWARD_MODE		FORWARD_MODE_FAKE

typedef struct{
					int				WantHelp;

					int				Foreground;

					int				DebugLevel;

					int				IdleTimeout;
					int				SessionTimeout;
					int				CpuUsage;
					int				ProcessNice;

					int				ConnLimit;

					const char 		*TrapLog;
					const char		*HaasLog;

					int				TrapPort;

					int				ForwardMode;

					const char		*Banner;

					const char		*KeysDir;

					const char		*HaasAddr;
					int				HaasPort;

					const char		*HaasToken;
				}tConfig;

void PrintUsage(const char *argv0);

void ConfigInit(tConfig *Config);

int ConfigParse(int argc,char **argv,tConfig *Config);

#endif // _CONFIG_H_

