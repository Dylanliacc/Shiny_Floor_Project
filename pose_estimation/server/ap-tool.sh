#!/bin/bash

start_ap() {
    nmcli radio wifi off
    sudo rfkill unblock wlan

    sudo ifconfig wlan0 192.168.1.1/24 up
    sudo hostapd /etc/hostapd/hostapd.conf -B

    sudo systemctl stop systemd-resolved.service
    sudo dnsmasq -C /etc/dnsmasq.conf
}

stop_ap() {
    sudo kill -9 `pidof hostapd`
    sudo kill -9 `pidof dnsmasq`
    sudo ifconfig wlan0 down
}

case $1 in
    start)
        start_ap
        ;;
    stop)
        stop_ap
        ;;
    *)
        echo "miss something"
        ;;
esac
