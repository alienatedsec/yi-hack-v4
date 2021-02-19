#!/bin/sh

CONF_FILE="etc/system.conf"

if [ -d "/usr/yi-hack-v4" ]; then
        YI_HACK_PREFIX="/usr/yi-hack-v4"
elif [ -d "/home/yi-hack-v4" ]; then
        YI_HACK_PREFIX="/home/yi-hack-v4"
fi

get_config()
{
    key=$1
    grep -w $1 $YI_HACK_PREFIX/$CONF_FILE | cut -d "=" -f2
}

if [ -d "/usr/yi-hack-v4" ]; then
	export LD_LIBRARY_PATH=/home/libusr:$LD_LIBRARY_PATH:/usr/yi-hack-v4/lib:/home/hd1/yi-hack-v4/lib
	export PATH=$PATH:/usr/yi-hack-v4/bin:/usr/yi-hack-v4/sbin:/home/hd1/yi-hack-v4/bin:/home/hd1/yi-hack-v4/sbin
elif [ -d "/home/yi-hack-v4" ]; then
	export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/home/lib:/home/yi-hack-v4/lib:/tmp/sd/yi-hack-v4/lib
	export PATH=$PATH:/home/base/tools:/home/yi-hack-v4/bin:/home/yi-hack-v4/sbin:/tmp/sd/yi-hack-v4/bin:/tmp/sd/yi-hack-v4/sbin
fi

ulimit -s 1024
hostname -F /etc/hostname

if [[ $(get_config DISABLE_CLOUD) == "no" ]] ; then
    (
        cd /home/app
        sleep 2
        ./mp4record &
        ./cloud &
        ./p2p_tnp &
        if [[ $(cat /home/app/.camver) != "yi_dome" ]] ; then
            ./oss &
        fi
        ./watch_process &
    )
elif [[ $(get_config REC_WITHOUT_CLOUD) == "yes" ]] ; then
    (
        cd /home/app
        sleep 2
        ./mp4record &
    )
fi

if [[ $(get_config HTTPD) == "yes" ]] ; then
    httpd -p 80 -h $YI_HACK_PREFIX/www/
fi

if [[ $(get_config TELNETD) == "yes" ]] ; then
    telnetd
fi

if [[ $(get_config FTPD) == "yes" ]] ; then
    if [[ $(get_config BUSYBOX_FTPD) == "yes" ]] ; then
        tcpsvd -vE 0.0.0.0 21 ftpd -w &
    else
        pure-ftpd -B
    fi
fi

if [[ $(get_config SSHD) == "yes" ]] ; then
    dropbear -R
fi

if [[ $(get_config NTPD) == "yes" ]] ; then
    # Wait until all the other processes have been initialized
    sleep 5 && ntpd -p $(get_config NTP_SERVER) &
fi

if [[ $(get_config MQTT) == "yes" ]] ; then
    mqttv4 &
fi

if [[ $(get_config RTSP) == "yes" ]] ; then
#    if [[ -f "$YI_HACK_PREFIX/bin/viewd" && -f "$YI_HACK_PREFIX/bin/rtspv4" ]]
#        viewd -D -S
#        rtspv4 -D -S
#    fi

if [[ -f "$YI_HACK_PREFIX/bin/h264grabber" && -f "$YI_HACK_PPREFIX/bin/rRTSPServer" ]] ; then
    h264grabber -f &
    RRTSP_RES=both rRTSPServer &
fi
fi

#ToDo
#SERIAL_NUMBER=$(dd status=none bs=1 count=20 skip=661 if=/tmp/mmap.info)
#HW_ID=$(dd status=none bs=1 count=4 skip=661 if=/tmp/mmap.info)
YI_HACK_VER="1.0.0"
HW_ID="HW_ID"
SERIAL_NUMBER="SN1234"
ONVIF_PORT=8080

if [[ $(get_config ONVIF) == "yes" ]] ; then
    ONVIF_PROFILE_0="--name Profile_0 --width 1920 --height 1080 --url rtsp://%s/ch0_0.h264 --type H264"
    ONVIF_PROFILE_1="--name Profile_1 --width 640 --height 360 --url rtsp://%s/ch0_1.h264 --type H264"
    onvif_srvd --pid_file /var/run/onvif_srvd.pid --model "Yi Hack" --manufacturer "Yi" --firmware_ver "$YI_HACK_VER" --hardware_id $HW_ID --serial_num $SERIAL_NUMBER --ifs wlan0 --port $ONVIF_PORT --scope onvif://www.onvif.org/Profile/S $ONVIF_PROFILE_0 $ONVIF_PROFILE_1

    if [[ $(get_config ONVIF_WSDD) == "yes" ]] ; then
        wsdd --pid_file /var/run/wsdd.pid --if_name wlan0 --type tdn:NetworkVideoTransmitter --xaddr http://%s:$ONVIF_PORT --scope "onvif://www.onvif.org/name/Unknown onvif://www.onvif.org/Profile/Streaming"
    fi
fi

sleep 25 && camhash > /tmp/camhash &

# First run on startup, then every day via crond
$YI_HACK_PREFIX/script/check_update.sh

crond -c $YI_HACK_PREFIX/etc/crontabs

if [ -f "/tmp/sd/yi-hack-v4/startup.sh" ]; then
    /tmp/sd/yi-hack-v4/startup.sh
elif [ -f "/home/hd1/yi-hack-v4/startup.sh" ]; then
    /home/hd1/yi-hack-v4/startup.sh
fi
