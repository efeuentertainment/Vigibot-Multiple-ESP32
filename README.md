# Vigibot-Multiple-ESP32

Note:
- if you want to control a single ESP32 robot/gadget, use this guide instead:
https://github.com/efeuentertainment/Vigibot-Single-ESP32
- if you want to control multiple ESP32 robots/gadgets, continue.

This code allows control of multiple ESP32 robots or gadgets over WLAN using vigibot.
A raspberry is still required for the camera and needs to be on the same network. the serial data usually sent and received to an Arduino over UART ( https://www.robot-maker.com/forum/topic/12961-vigibot-pi-arduino-serial-communication-example/ ), is being sent over WLAN instead, allowing control of small robots or gadgets.
usage example: a pi with a camera watches an arena where multiple small ESP32 robots can be driven, the ESP32 bots do not have an on-board camera.
second usage example: open or close a door or toy controlled fron an ESP32.

1) assign your pi a static IP address
https://thepihut.com/blogs/raspberry-pi-tutorials/how-to-give-your-raspberry-pi-a-static-ip-address-update

2) the pi needs to create a virtual serial point that's accessible over WLAN from the ESP32. install socat and ncat on your raspberry:
`sudo apt install socat ncat`

3) create a new script:
`sudo nano /usr/local/ncat_esp.sh`

4) paste the following code:
```
#!/bin/bash

sudo socat pty,link=/dev/ttyVigi0,rawer,shut-none pty,link=/dev/ttySrv,rawer,shut-none &

while true
do
  sleep 1
  sudo ncat -l -k -4 --idle-timeout 15 7070 </dev/ttySrv >/dev/ttySrv
  date
  echo "restarting ncat"
done
```

5) add permissions
`sudo chmod +x /usr/local/ncat_esp.sh`

6) run the script
`sudo /usr/local/ncat_esp.sh`

7) insert your
- network info
- raspberry host IP
- vigibot NBpositions (if different from default)
into the .ino sketch and upload it to your ESP32 from the arduino IDE using a usb cable.
note: only one esp can send a reply, make sure `client.write();` is only being called on one ESP32, all others only listen without sending any data back. (i recommend that one ESP32 sends data back, this way the vigibot robot won't wake up if the ESP32 isn't connected properly, which makes it easier to notice that it isn't working.)

8) open the serial monitor to confirm a successful WLAN and TCP client connection.

9) test the pi <-> ESP connection by logging in as root in a new shell (needs root) :
`su -`
and sending a test text frame:
`echo '$T  hello from cli       $n' > /dev/ttyVigi0`
the arduino serial monitor now says "hello from cli "

10) if it works add it to start at boot:
`sudo nano /etc/rc.local`
and add
`/usr/local/ncat_esp.sh &`
above the line "exit 0".

11) add an entry on the vigibot online remote control config -> SERIALPORTS -> 3 with value: "/dev/ttyVigi0"

12) set WRITEUSERDEVICE to "3"

13) if you set up an ESP32 to send data back:
set READUSERDEVICE to "3"

restart the client and wake your robot up, if it wakes up then everything works.


related notes:
you can add a dummy client from the command line, in case you want to check if the vigibot frames are made available:   
`sudo socat tcp:localhost:7070 -` 

similarly it's theoretically also possible to fetch this data from another computer or custom script/application.

there's an outage happening every few weeks or so, from which it doesn't recover.   
it looks like ncat doesn't open the socat socket if there's too much data buffered. the workaround is to manually clear the socat socket using
`sudo cat /dev/ttySrv` and then cancel with `ctrl-c`
