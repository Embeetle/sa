if [ "$TERM" == "dumb" ]; then stty -echo; fi
alias l='ls -Alh'
. ~/env/bin/activate
export PATH=~/bin:$PATH
export PS1="docker:\w\> "
