# HaaS-libssh
nic.cz HaaS implementation in C-language with libssh

```
Loosely based on libssh samples:
samplesshd-cb = server
connect-ssh = client
ssh_proxy = proxy :-)
```

Usage:
```
haas [switches] [options] -t TOKEN

options:
     [-b BANNER] - banner to send to client (default is 'OpenSSH_7.2p2 Ubuntu-4ubuntu2.2')
     [-p LOCAL_PORT]   - where to run the 'trap' (default is 2222)
     [-a HAAS_HOST]    - where to connect (default is 'haas-app.nic.cz')
     [-r HAAS_PORT]    - where to connect (default is 10000)
     [-k KEYS_DIR]     - where to look for RSA/DSA keys (default is '/etc/HaaS/keys/')
     [-c CONN_LIMIT]   - connection limit (default is 30 clients)
     [-i IDLE_TIMEOUT] - inactivity timeout (default is 30 sec)
     [-s SESSION_TIMEOUT] - maximum session length in sec (default is unlimited)
     [-u CPU_USAGE_LIMIT] - maximum cpu suage limit in sec (default is unlimited)
     [-n PROCESS_NICE]    - nice of the process (default is 0)
     [-m FORWARD_MODE] - how to handle port forwrading (default mode is 2)
                         0-deny, 1-allow, 2-fake, 3-null, 4-echo

switches:
     [-f] - foreground mode (don't fork)
     [-d] - increase debug level
     [-h] - this help :-)
     [-?] - this help :-)

```
