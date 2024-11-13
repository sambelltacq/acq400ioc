# nchan deprecated, NCHAN preferred

get_nchan() {
        SITE=$1
        if [ -e /etc/acq400/$SITE/active_chan ]; then
		cat /etc/acq400/$SITE/active_chan
        elif [ -e /etc/acq400/$SITE/NCHAN ]; then
                cat /etc/acq400/$SITE/NCHAN
        elif [ -e /etc/acq400/$SITE/nchan ]; then
                cat /etc/acq400/$SITE/nchan
        else
                echo 4
        fi
}

dblr() {
	echo 'dbLoadRecords(' $* ')'
}
get_range() {
	site=$1
	if [ -e /etc/acq400/${site}/PART_NUM ]; then
		VSPEC=$(tr -s -- -\  \\n   </etc/acq400/${site}/PART_NUM | grep V)
		vr1=${VSPEC%*V}
		VR=${vr1/V/.}
	fi
	
	if [ "x$VR" != "x" ]; then
		echo $VR
	else
		model=$(cat /dev/acq400.${site}.knobs/module_type)
		case $model in
		$MT_ACQ424)	echo 10.24;;
		$MT_ACQ480) echo 2.5;;
		*)			echo 10;;
		esac
	fi
}

create_asyn_channel() {
		echo "# 1 $1 2 $2 3 $3 4 $4 defaults ${3:-0} ${4:-0}"
		cat - <<EOF
drvAsynIPPortConfigure("$1", "$2")
dbLoadRecords("db/asynRecord.db","P=${HOST}:,R=asyn:$1,PORT=$1,ADDR=0,IMAX=100,OMAX=100,TB3=${3:-0},TIB0=${4:-0}")
EOF
			
}