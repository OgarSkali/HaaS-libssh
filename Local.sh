#!/bin/bash -x

LOCAL_PORT=2222
HAAS_HOST=127.0.0.1
HAAS_PORT=22

# allow
MODE=1

# echo
#MODE=4

# null
#MODE=3

rm core*


./haas -f -dddddd -k ./keys -i 55 -m $MODE -p $LOCAL_PORT -a $HAAS_HOST -r $HAAS_PORT -t-
