# HaaS-libssh
nic.cz HaaS implementation in C-language with libssh

```
Losely based on libssh samples:
samplesshd-cb = server
connect-ssh = client
ssh_proxy = proxy :-)
```

Usage:
```
	haas [switches] [options] -t TOKEN
		options:
			[-p LOCAL_PORT] - where to run the 'trap' (default is 22)
			[-a HAAS_HOST] -  where to connect (default is haas-ap.nic.cz)
			[-r HAAS_PORT] -  where to connect (default is 10000)
			[-k KEYS_DIR]  -  where to look for RSA/DSA/ECC keys (default is /etc/HaaS/keys)
			[-i IDLE_TIMEOUT] - inactivity timeout (default is 30 sec)
			[-m FORWARD_MODE] - how to handle port forwrading (default mode is 2)
						0-deny, 1-allow, 2-fake, 3-null, 4-echo
```

