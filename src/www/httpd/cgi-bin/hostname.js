#!/bin/sh

printf "Content-type: text/javascript\r\n\r\n"

printf "hostname=\"%s\";" $(cat tmp/sd/yi-hack-v4/etc/hostname)
