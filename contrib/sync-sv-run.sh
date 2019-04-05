#!/bin/bash

VERSION=`cat /etc/morphenedversion`

# if the writer node dies by itself, kill runsv causing the container to exit
MORPHENED_PID=`pgrep -f p2p-endpoint`
if [[ ! $? -eq 0 ]]; then
  echo NOTIFYALERT! morphenedsync has quit unexpectedly, checking for coredump and then starting a new instance..
  sleep 30
  SAVED_PID=`cat /tmp/morphenedpid`
  if [[ -e /tmp/core.$SAVED_PID ]]; then
    gdb --batch --quiet -ex "thread apply all bt full" -ex "quit" /usr/local/morphened-full/bin/morphened /tmp/core.$SAVED_PID >> /tmp/stacktrace
    STACKTRACE=`cat /tmp/stacktrace`
    echo NOTIFYALERT! morphenedsync stacktrace from coredump:
    for ((i=0;i<${#STACKTRACE};i+=120)); do
      echo "${STACKTRACE:i:120}"
    done
    CORE_FILE_NAME=coredump-`date '+%Y%m%d-%H%M%S'`.$SAVED_PID
    aws s3 cp /tmp/core.$SAVED_PID s3://$S3_BUCKET/$CORE_FILE_NAME
  fi
  RUN_SV_PID=`pgrep -f /etc/service/morphened`
  kill -9 $RUN_SV_PID
fi

# check if we are synced, if so start the process of uploading to s3
# after uploading, kill runsv causing the container to exit
# and ecs-agent will start a new container starting the process over
BLOCKCHAIN_TIME=$(
    curl --silent --max-time 20 \
        --data '{"jsonrpc":"2.0","id":39,"method":"database_api.get_dynamic_global_properties"}' \
        localhost:8090 | jq -r .result.time
)

if [[ ! -z "$BLOCKCHAIN_TIME" ]]; then
  BLOCKCHAIN_SECS=`date -d $BLOCKCHAIN_TIME +%s`
  CURRENT_SECS=`date +%s`

  # if we're within 10 seconds of current time, call it synced and begin the upload
  BLOCK_AGE=$((${CURRENT_SECS} - ${BLOCKCHAIN_SECS}))
  if [[ ${BLOCK_AGE} -le 10 ]]; then
    MORPHENED_PID=`pgrep -f p2p-endpoint`
    kill -SIGINT $MORPHENED_PID
    echo morphenedsync: waiting for morphened to exit cleanly

    # wait 60 seconds for morphened to exit, to be safe.
    let WAIT_TIME=0
    while ( kill -0 $MORPHENED_PID ) && [[ WAIT_TIME -le 60 ]]; do
       sleep 1
       let WAIT_TIME++
    done

    echo morphenedsync: starting a new blockchainstate upload operation
    cd ${COMPRESSPATH:-$HOME}
    echo morphenedsync: compressing blockchainstate...
    if [[ "$USE_RAMDISK" ]]; then
      tar vcf blockchain.tar.lz4 --use-compress-prog=lz4 -C $HOME blockchain -C /mnt/ramdisk blockchain
    else
      tar cf blockchain.tar.lz4 --use-compress-prog=lz4 -C $HOME blockchain
    fi
    if [[ ! $? -eq 0 ]]; then
      echo NOTIFYALERT! morphenedsync was unable to compress shared memory file, check the logs.
      exit 1
    fi
    if [[ "$IS_BROADCAST_NODE" ]]; then
      FILE_NAME=broadcast-$VERSION-`date '+%Y%m%d-%H%M%S'`.tar.lz4
    elif [[ "$IS_AH_NODE" ]]; then
      FILE_NAME=ahnode-$VERSION-`date '+%Y%m%d-%H%M%S'`.tar.lz4
    else
      FILE_NAME=blockchain-$VERSION-`date '+%Y%m%d-%H%M%S'`.tar.lz4
    fi
    echo morphenedsync: uploading $FILE_NAME to $S3_BUCKET
    aws s3 cp blockchain.tar.lz4 s3://$S3_BUCKET/$FILE_NAME
    if [[ ! $? -eq 0 ]]; then
      echo NOTIFYALERT! morphenedsync was unable to upload $FILE_NAME to s3://$S3_BUCKET
      exit 1
    fi
    echo morphenedsync: replacing current version of blockchain state with $FILE_NAME
    if [[ "$IS_BROADCAST_NODE" ]]; then
      aws s3 cp s3://$S3_BUCKET/$FILE_NAME s3://$S3_BUCKET/broadcast-$VERSION-latest.tar.lz4
      aws s3api put-object-acl --bucket $S3_BUCKET --key broadcast-$VERSION-latest.tar.lz4 --acl public-read
    elif [[ "$IS_AH_NODE" ]]; then
      aws s3 cp s3://$S3_BUCKET/$FILE_NAME s3://$S3_BUCKET/ahnode-$VERSION-latest.tar.lz4
      aws s3api put-object-acl --bucket $S3_BUCKET --key ahnode-$VERSION-latest.tar.lz4 --acl public-read
    else
      aws s3 cp s3://$S3_BUCKET/$FILE_NAME s3://$S3_BUCKET/blockchain-$VERSION-latest.tar.lz4
      aws s3api put-object-acl --bucket $S3_BUCKET --key blockchain-$VERSION-latest.tar.lz4 --acl public-read
    fi
    if [[ ! $? -eq 0 ]]; then
      echo NOTIFYALERT! morphenedsync was unable to overwrite the current blockchainstate with $FILE_NAME
      exit 1
    fi
    # upload a current block_log
    cd $HOME
    if [[ ! "$IS_BROADCAST_NODE" ]] && [[ ! "$IS_AH_NODE" ]]; then
      aws s3 cp blockchain/block_log s3://$S3_BUCKET/block_log-intransit
      aws s3 cp s3://$S3_BUCKET/block_log-intransit s3://$S3_BUCKET/block_log-latest
      aws s3api put-object-acl --bucket $S3_BUCKET --key block_log-latest --acl public-read
    fi
    # kill the container starting the process over again
    echo morphenedsync: stopping the container after a sync operation
    if [[ -e /tmp/isnewsync ]]; then
      echo notifymorphenedsync: morphenedsync: successfully generated and uploaded new blockchain-$VERSION-latest.tar.lz4 to s3://$S3_BUCKET
    fi
    RUN_SV_PID=`pgrep -f /etc/service/morphened`
    kill -9 $RUN_SV_PID
  fi
fi

# check on this every 60 seconds
sleep 60
