#!/bin/bash

VIDEO_DRIVERS="xserver-xorg-video-all xserver-xorg-video-ati xserver-xorg-video-radeon xserver-xorg-video-nv xserver-xorg-video-intel xserver-xorg-video-geode xserver-xorg-video-glide xserver-xorg-video-glint xserver-xorg-video-i128 xserver-xorg-video-i740 xserver-xorg-video-mach64 xserver-xorg-video-geode xserver-xorg-video-cirrus xserver-xorg-video-mga xserver-xorg-video-openchrome xserver-xorg-video-via xserver-xorg-video-fbdev xserver-xorg-video-dummy xserver-xorg-video-glamo xserver-xorg-video-apm  xserver-xorg-video-ark  xserver-xorg-video-chips xserver-xorg-video-neomagic xserver-xorg-video-nouveau xserver-xorg-video-qxl  xserver-xorg-video-r128 xserver-xorg-video-radeonhd xserver-xorg-video-rendition xserver-xorg-video-s3 xserver-xorg-video-s3virge xserver-xorg-video-savage xserver-xorg-video-siliconmotion xserver-xorg-video-sis  xserver-xorg-video-sisusb xserver-xorg-video-tdfx xserver-xorg-video-tga xserver-xorg-video-trident xserver-xorg-video-tseng xserver-xorg-video-vesa xserver-xorg-video-vmware xserver-xorg-video-voodoo"

PKG_WHITE="debconf-english localepurge dialog sudo mplayer-nogui thttpd feh mpd mpc xdotool linux-image-686 alsa-utils awesome psmisc clive midori dos2unix curl dropbear xinit autofs smbfs mingetty"

PKG_BLACK="info manpages rsyslog tasksel tasksel-data aptitude"

FILES_BLACK="/var/cache/apt/pkgcache.bin /var/cache/apt/srcpkgcache.bin /usr/share/man/?? /usr/share/man/??_* /usr/share/doc/* /usr/share/icons/* /root/.bash_history"

export LC_ALL="C"
DEBIAN_MIRROR="http://ftp.at.debian.org/debian"
DEBIAN_MULTIMEDIA_MIRROR="http://www.debian-multimedia.org"

dir="`dirname $0`"
BOOTSTRAP_DIR="`cd $dir; pwd`"
BOOTSTRAP_LOG="bootstrap.log"
ARCH=i386
APTCACHER_PORT=
NOINSTALL=
NOUSERCONF=
NODEBOOT=
CHROOT_DIR=
CHRT=


function printUsage() {
  cat 1>&2 <<EOUSAGE
Bootstrap a Lounge Media Center installation.

$0 [-a <arch>][-l <logfile>][-c <apt-cacher-port>][-u -n -x -d ] <bootstrapdir>"
Options:"
  -a <arch> Bootstrap a system with of the given architecture
  -l <file> Specify the log file
  -c <port> Enables using apt-cacher-ng on the specified port
  -n        Don't configure and install packages
  -x        Don't do user configuration
  -d        Don't debootstrap
  -u        Combined -n, -x and -d
EOUSAGE
  exit 1
}

function absDir() {
  dir="`dirname $1`"
  absdir="`cd $dir; pwd`"
  echo $absdir
}

function absPath() {
  dir="`dirname $1`"
  base="`basename $1`"
  absdir="`cd $dir; pwd`"
  echo $absdir/$base
}

function printVideoDrivers() {
  PAD=18
  i=0

  echo $VIDEO_DRIVERS | sed 's/ /\n/g' | cut -d"-" -f4| while read vd; do
    LEN=$[ ${#i} + ${#vd} + 2 ]
    echo -n "($i) $vd"
  
    for j in `seq 0 $[$PAD - $LEN]`; do echo -n " "; done
    i=$[$i + 1]
    if [ $[ $i % 3 ] -eq 0 ]; then
      echo
    fi
  done  
  echo
}

function askVideoDriver() {
  NUM="`echo $VIDEO_DRIVERS | wc -w`"
  (
    echo -n "Please select a video driver (default=0):"
    DRIVER=""
    while read idx; do
      [ -z "$idx" ] && idx=0
      if printf "%d" $idx > /dev/null 2>&1; then
	if [ $idx -lt 0 -o $idx -ge $NUM ]; then
				  echo "Out of range: $idx."
	else
	  DRIVER="`echo $VIDEO_DRIVERS | sed 's/ /\n/g' | sed -n "$[ $idx + 1 ]p"`"
	  echo "Selected: `echo "$DRIVER"  | cut -d"-" -f4`" 1>&2
	  break
	fi
      else
	echo "Invalid input: $idx. Please select the video driver by entering a number."
      fi
      echo -n "Please select a video driver (default=0):"
    done
  ) 1>&2
  echo $DRIVER
}

function skip() {
  echo -n "$1: "
  green "skipped\n"
}

function doDebootstrap() {
  check "create target dir" \
    "mkdir -p \"$CHROOT_DIR\""

  BOOTSTRAP_MIRROR=$DEBIAN_MIRROR

  [ -n "$APTCACHER_PORT" ] && BOOTSTRAP_MIRROR=$(
    HOST="`echo $DEBIAN_MIRROR | sed 's/^http*:\/\///g' | sed 's/\/.*$//g'`"
    echo "http://127.0.0.1:$APTCACHER_PORT/$HOST/debian"
  )

  check "bootstrap debian" \
    "debootstrap --arch $ARCH squeeze "$CHROOT_DIR" $BOOTSTRAP_MIRROR"
}

function doUserConf() {
  checkcat "Configure root user" \
    "$CHRT passwd"

  check "Set root login shell" \
    "$CHRT usermod -s /setup/configure.sh root"

  check "Add user lounge" \
    "$CHRT adduser lounge --disabled-password --gecos \"\" "
  
  check "Add group audio" \
    "$CHRT usermod -G audio lounge"
}

function doPackageConf() {
  export DEBIAN_FRONTEND=noninteractive
  aptni="apt-get -q -y --no-install-recommends --force-yes -o Dpkg::Options::=\"--force-confdef\" -o Dpkg::Options::=\"--force-confold\" ";

  check "Prepare package manager" \
    "$CHRT dpkg --configure -a"

  check "Fix dependencies" \
    "$CHRT $aptni install -f"

  check "Update Repositories" \
    "$CHRT $aptni update"

  check "Install white listed packages" \
    "$CHRT $aptni install $PKG_WHITE"

  check "Remove black listed packages" \
    "$CHRT $aptni remove $PKG_BLACK"
}

function doCleanup() {
  check "Clean apt cache" \
    "$CHRT apt-get clean"

  check "Copy data" \
    "rsync -axh --delete $BOOTSTRAP_DIR/data/* $CHROOT_DIR/"
  
  check "Remove black listed files" \
    "$CHRT rm -rf $FILES_BLACK"
}

function doPrepareChroot() {
  ( 
    cd "$CHROOT_DIR"
    mount --bind /dev/ dev
    mount -t proc none proc
    mount -t sysfs none sys
    mount -t tmpfs none tmp
    mount -t devpts none dev/pts

    mkdir -p "$CHROOT_DIR/etc/apt/"
    cat > "$CHROOT_DIR/etc/apt/sources.list" <<EOSOURCES
deb $DEBIAN_MIRROR  squeeze main
deb $DEBIAN_MULTIMEDIA_MIRROR squeeze main non-free 
EOSOURCES

    if [ -n "$APTCACHER_PORT" ]; then
      # use apt-cacher-ng to cache packages during install
      mkdir -p "$CHROOT_DIR/etc/apt/apt.conf.d/"
      cat > "$CHROOT_DIR/etc/apt/apt.conf.d/00aptcacher" <<EOAPTCONF
acquire::http { Proxy "http://127.0.0.1:$APTCACHER_PORT"; };
EOAPTCONF
    fi

    # disable starting daemons after install
    mkdir -p "$CHROOT_DIR/usr/sbin"
    cat > "$CHROOT_DIR/usr/sbin/policy-rc.d" <<EOPOLICY
#!/bin/sh
exit 101
EOPOLICY

    chmod 755 "$CHROOT_DIR/usr/sbin/policy-rc.d"
  )
}

function doFreeChroot() {
	pkill -KILL -P $$ &> /dev/null
	( 
		cd "$CHROOT_DIR"
    umount dev/pts
    umount tmp
    umount sys 
    umount proc
    umount dev
    umount -l dev
  ) &>/dev/null
  rm -rf "$CHROOT_DIR/etc/apt/apt.conf.d/00aptcacher"
  rm -rf "$CHROOT_DIR/usr/sbin/policy-rc.d"
	exit
}

###### main

while getopts 'a:l:c:nxud' c
do
  case $c in
    a) ARCH="$OPTARG";;
    l) BOOTSTRAP_LOG="`absPath $OPTARG`";;
    c) APTCACHER_PORT="$OPTARG";;
    n) NOINSTALL="YES";;
    x) NOUSERCONF="YES";;
    d) NODEBOOT="YES";;
    u) NOUSERCONF="YES"; NOINSTALL="YES"; NODEBOOT="YES";;
    \?) printUsage;;
  esac
done

shift $(($OPTIND - 1))

echo > "$BOOTSTRAP_LOG"
export BOOTSTRAP_LOG
source "$BOOTSTRAP_DIR/.functions.sh"
export CHROOT_DIR="`absPath $1`"
export CHRT="chroot \"$CHROOT_DIR\" "

if [ $# -ne 1 -o ! -d "$CHROOT_DIR" ]; then
	printUsage
else
  printVideoDrivers
  PKG_WHITE="${PKG_WHITE} $(askVideoDriver)"

  if [ -z "$NODEBOOT" ]; then 
		doDebootstrap
	else
		skip "debootstrap"
	fi

  doPrepareChroot
  trap doFreeChroot SIGINT SIGTERM EXIT
  if [ -z "$NOUSERCONF" ]; then
		doUserConf
	else
		skip "user configuration"
	fi

  if [ -z "$NOINSTALL" ]; then 
		doPackageConf
	else
		skip "package configuration"
	fi

	doCleanup
fi

