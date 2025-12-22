# ~/.bashrc: executed by bash(1) for non-login shells.

# Larger right triangle
TRICODE=$'\uE0B0'

if [ -n "$SSH_CLIENT" ]; then
    export PS1='\[\e[44m\e[37m\u>\e[43m\e[37m\h>\e[46m\e[30m\W>\e[0m\] '
else
    export PS1='\[\e[44m\e[37m\u\e[43m\e[34m'$TRICODE'\e[43m\e[37m\h\e[46m\e[33m'$TRICODE'\e[46m\e[30m\W\e[40m\e[36m'$TRICODE'\e[0m\] '
fi

umask 022

# You may uncomment the following lines if you want `ls' to be colorized:
export LS_OPTIONS='--color=auto'
export TERM=linux
eval "$(dircolors -b ~/.dircolors)"
alias ls='ls $LS_OPTIONS'

echo "Welcome to sandpiper"
