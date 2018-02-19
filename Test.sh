#!/bin/bash -x

TOKEN="--------------------------------------"
LOCAL_PORT=2222
HAAS_HOST="haas-app.nic.cz"
HAAS_PORT=10000

./haas -f -ddd -m 1 -k ./keys -t $TOKEN
