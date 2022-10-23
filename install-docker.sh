#!/usr/bin/env bash

# exit when any command fails
set -e
set -x

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
DOCKER_DIR=$SCRIPT_DIR/docker-compose
DST_DIR=$1
DOMAIN=$2
DUCKDNS_TOKEN=$3

mkdir -p "$DST_DIR/letsencrypt"
mkdir -p "$DST_DIR/postgres"
sed -e "s/__DOMAIN__/$DOMAIN/g" -e "s/__DUCKDNS_TOKEN__/$DUCKDNS_TOKEN/g" "$DOCKER_DIR/docker-compose.yml" > "$DST_DIR/docker-compose.yml"
sed -e "s/__DOMAIN__/$DOMAIN/g" "$DOCKER_DIR/nginx.conf" > "$DST_DIR/nginx.conf"
curl https://ssl-config.mozilla.org/ffdhe2048.txt > "$DST_DIR/letsencrypt/dhparam"
cp "$DOCKER_DIR/init.sql" "$DST_DIR/postgres/init.sql"
cp "$DOCKER_DIR/docker-compose.yml" "$DST_DIR/docker-compose.yml"

