Petalinux configuration

Prerequisites:
----
Connect your board to the host PC with a USB cable, then use a serial terminall to connect to the serial port that shows up.
You can then log in with the default username/password pair:
peta : peta

Network:
----
By default eth0 gets renamed to enx000a35001e53 on petalinux
To fix that:
sudo vi /etc/network/interfaces
and replace all eth0 with auto enx000a35001e53 then :wq and reboot
NOTE: Delete any iface eth1 line you may find as that seems to interfere with networking

Remove login splash and login timeout:
----
First, remove the timeout by editing login.defs file
sudo vi /etc/login.defs
Change the 60 to 0 next to the line LOGIN_TIMEOUT, then :wq to save
Second thing is the nag screen, remove it via:
sudo mv /etc/issue /etc/issue.orig
On next boot you should no longer have timeout warnings or see long splash text from Petalinux

Sandpiper device driver access config:
----
We need to allow all users access to the sandpiper device driver so that they can work with the custom hardware without issues.

To achieve this, create a new file:
```
sudo vi /etc/udev/rules.d/30-sandpiper-device.rules
```

and type the following as contents (warning: except MODE all are double = signs)
```
KERNEL=="sandpiper", SUBSYSTEM=="sandpiper", GROUP=="users", MODE="0666"
```
:wq then reboot to gain unhindered access to hardware devices.

Switching to SSH:
----
At this point you can switch to SSH since it's a lot easier to work with a window where you can copy/paste text. Simply run something similar to the following, but with your device's IP:
```
ssh peta@192.168.1.87
```
When asked if you want to add this machine, type 'yes' and then 'peta' at the password prompt and you should be logged in.

P.S. You can learn what IP your machine has using the following command:
```
ifconfig
```

Fbterm:
----
This allows us to see the terminal so that we don't need to use a serial terminal after this point.

First, create the following folder if it doesn't exist:
```
sudo mkdir /usr/local
sudo mkdir /usr/local/bin
```

copy the provided fbterm in image/binaries folder to this folder and make it executable:
```
sudo cp fbterm /usr/local/bin/
sudo chmod +x /usr/local/bin/fbterm
```

Now we need to add some color to the terminal. First, run the following:
```
dircolors -p > ~/.dircolors
```

and then add this to the end of your ~/.profile file for 256 color support:
```
if [ "$TERM" = "linux" ]; then
    echo -en "\e]P0222222" #black
    echo -en "\e]P8222222" #darkgrey
    echo -en "\e]P1803232" #darkred
    echo -en "\e]P9982b2b" #red
    echo -en "\e]P25b762f" #darkgreen
    echo -en "\e]PA89b83f" #green
    echo -en "\e]P3aa9943" #brown
    echo -en "\e]PBefef60" #yellow
    echo -en "\e]P4324c80" #darkblue
    echo -en "\e]PC2b4f98" #blue
    echo -en "\e]P5706c9a" #darkmagenta
    echo -en "\e]PD826ab1" #magenta
    echo -en "\e]P692b19e" #darkcyan
    echo -en "\e]PEa1cdcd" #cyan
    echo -en "\e]P7ffffff" #lightgrey
    echo -en "\e]PFdedede" #white
fi
```

then make sure your .bashrc has the following lines:
```
# Larger right triangle
TRICODE=$'\uE0B0'

if [ -n "$SSH_CLIENT" ]; then
    export PS1='\[\e[44m\e[37m\u>\e[43m\e[37m\h>\e[46m\e[30m\W>\e[0m\] '
else
    export PS1='\[\e[44m\e[37m\u\e[43m\e[34m'$TRICODE'\e[43m\e[37m\h\e[46m\e[33m'$TRICODE'\e[46m\e[30m\W\e[40m\e[36m'$TRICODE'\e[0m\] '
fi
export LS_OPTIONS='--color=auto'
export TERM=linux
eval "$(dircolors -b ~/.dircolors)"
alias ls='ls $LS_OPTIONS'
echo "Welcome to sandpiper"
```
To use fbterm on login, we need to create a script file first:
```
sudo vi /usr/local/bin/fbterm-login
```

then add the following to this file:
```
#!/bin/sh
export FBTERM=1
export LANG=en_US.UTF-8
export HOME=/root
cat /usr/share/misc/gray.bin > /dev/fb0
export FBTERM_BACKGROUND_IMAGE=1
exec /usr/local/bin/fbterm -- /bin/login
```
IMPORTANT! DO NOT FORGET TO MAKE IT EXECUTABLE OR THERE WON'T BE A CONSOLE:
sudo chmod +x /usr/local/bin/fbterm-login

After this do NOT forget to make it executable or there won't be a terminal:
```
sudo chmod +x /usr/local/bin/fbterm-login
```

Now we need to copy the gray.bin and poweroff.bin files from the image/binaries folder to /usr/share/misc/
and set their owner to root:
```
sudo chown root:root /usr/share/misc/gray.bin
sudo chown root:root /usr/share/misc/poweroff.bin
```

And as the last step in being able to use fbterm on login, we need to edit the /etc/inittab:
```
sudo vi /etc/inittab
```
Now replace the existing 1:12345:respawn:/sbin/getty with the following:
```
1:12345:respawn:/sbin/agetty --noclear -n -l /usr/local/bin/fbterm-login tty1 linux
```
and save, and reboot.

Shutdown image:
---
Now we can add our shutdown image.
Edit the /etc/rc0.d/S90halt file:
```
sudo vi /etc/rc0.d/S90halt
```

and add this right above the 'halt' line:
```
cat /usr/share/misc/poweroff.bin > /dev/fb0
```

Font setup for fbterm:
---

First, we need to copy all fonts from image/binaries to the /usr/share/fonts/ttf folder.

Then we edit the .fbtermrc file in your home folder:
```
vi .fbtermrc
```

and edit the font name and size, for example this one is a good choice:
```
font-names=Liberation Mono:style=Regular
font-size=12
```

Now we have to make a new 'root' folder which is where fbterm looks on startup:
```
sudo mkdir /root
```

Then we copy the .fbtermrc file there and set its owner to root:
```
sudo cp ~/.fbtermrc /root/
sudo chown root:root /root/.fbtermrc
```

Bluetooth:
---
For this to work, we need to copy [brcm/BCM20702A1-0a5c-21e8.hcd](https://github.com/winterheart/broadcom-bt-firmware/blob/master/brcm/BCM20702A1-0a5c-21e8.hcd) from the git repo into the /lib/firmware/brcm folder.

```
sudo mkdir /lib/firmware
sudo mkdir /lib/firmware/brcm
sudo cp BCM20702A1-0a5c-21e8.hcd /lib/firmware/brcm
```

Might want to do the following to test:

```
bluetoothctl
power on
agent on
default-agent
scan on
```

If successful, it will list bluetooth device MAC addresses available with short names.

To stop, CTRL+C once then:
```
scan off
exit
```
