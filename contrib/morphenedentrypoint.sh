#!/bin/bash

echo /tmp/core | tee /proc/sys/kernel/core_pattern
ulimit -c unlimited

# if we're not using PaaS mode then start morphened traditionally with sv to control it
if [[ ! "$USE_PAAS" ]]; then
  mkdir -p /etc/service/morphened
  cp /usr/local/bin/morphene-sv-run.sh /etc/service/morphened/run
  chmod +x /etc/service/morphened/run
  runsv /etc/service/morphened
else
  /usr/local/bin/startpaasmorphened.sh
fi
