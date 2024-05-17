#!/bin/bash
# Script outline to install and build kernel.
# Author: Siddhant Jajoo.

set -e
set -u

OUTDIR=/tmp/aeld
KERNEL_REPO=git://git.kernel.org/pub/scm/linux/kernel/git/stable/linux-stable.git
KERNEL_VERSION=v5.1.10
BUSYBOX_VERSION=1_33_1
FINDER_APP_DIR=$(realpath $(dirname $0))
ARCH=arm64
CROSS_COMPILE=aarch64-none-linux-gnu-

if [ $# -lt 1 ]
then
	echo "Using default directory ${OUTDIR} for output"
else
	OUTDIR=$1
	echo "Using passed directory ${OUTDIR} for output"
fi

mkdir -p ${OUTDIR}

cd "$OUTDIR"
if [ ! -d "${OUTDIR}/linux-stable" ]; then
    #Clone only if the repository does not exist.
	echo "CLONING GIT LINUX STABLE VERSION ${KERNEL_VERSION} IN ${OUTDIR}"
	git clone ${KERNEL_REPO} --depth 1 --single-branch --branch ${KERNEL_VERSION}
fi
if [ ! -e ${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image ]; then
    cd linux-stable
    echo "Checking out version ${KERNEL_VERSION}"
    git checkout ${KERNEL_VERSION}

    # TODO: Add your kernel build steps here
    echo "building kernel"
    sed -i '41d' scripts/dtc/dtc-lexer.l
    make ARCH=$ARCH CROSS_COMPILE=$CROSS_COMPILE defconfig
    make ARCH=$ARCH CROSS_COMPILE=$CROSS_COMPILE -j`nproc`
fi

echo "Adding the Image in outdir"
cp ${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image ${OUTDIR}

echo "Creating the staging directory for the root filesystem"
cd "$OUTDIR"
if [ -d "${OUTDIR}/rootfs" ]
then
	echo "Deleting rootfs directory at ${OUTDIR}/rootfs and starting over"
    sudo rm  -rf ${OUTDIR}/rootfs
fi

# TODO: Create necessary base directories
echo "creating rootfs"
cd "$OUTDIR"
mkdir rootfs && cd rootfs
mkdir -p usr/bin usr/sbin usr/lib lib bin sbin proc sys etc home/root dev net var/log

cd "$OUTDIR"
if [ ! -d "${OUTDIR}/busybox" ]
then
git clone git://busybox.net/busybox.git
    cd busybox
    git checkout ${BUSYBOX_VERSION}
    # TODO:  Configure busybox\
else
    cd busybox
fi

if [ ! -e ${OUTDIR}/busybox/busybox ]; then
# TODO: Make and install busybox
echo "building busybox"
make ARCH=$ARCH CROSS_COMPILE=$CROSS_COMPILE defconfig
sed -i 's/^CONFIG_TC=.*/# CONFIG_TC is not set/' .config
make ARCH=$ARCH CROSS_COMPILE=$CROSS_COMPILE CONFIG_PREFIX=${OUTDIR}/rootfs -j`nproc` all
fi
echo "installing busybox"
make ARCH=$ARCH CROSS_COMPILE=$CROSS_COMPILE CONFIG_PREFIX=${OUTDIR}/rootfs -j`nproc` install
cd ${OUTDIR}/rootfs

echo "Library dependencies"
${CROSS_COMPILE}readelf -a bin/busybox | grep "program interpreter"
${CROSS_COMPILE}readelf -a bin/busybox | grep "Shared library"

# TODO: Add library dependencies to rootfs
echo "resolving busybox dependencies"
aarch64-none-linux-gnu-gcc -print-sysroot -v
SYSROOT=/usr/aarch64-none-linux-gnu

find / -name "libm.so.6"
find / -name "libresolv.so.2"
find / -name "libc.so.6"
find / -name "ld-linux-aarch64.so.1"

#dep1=$(find $SYSROOT -name "libm.so.6")
#dep2=$(find $SYSROOT -name "libresolv.so.2")
#dep3=$(find $SYSROOT -name "libc.so.6")
#dep4=$(find $SYSROOT -name "ld-linux-aarch64.so.1")
#echo "dependencies are"
#echo "  ${dep1}"
#echo "  ${dep2}"
#echo "  ${dep3}"
#echo "  ${dep4}"

cp ${dep1} ${dep2} ${dep3} ${dep4} ${OUTDIR}/rootfs/lib

# TODO: Make device nodes
cd $OUTDIR/rootfs
sudo mknod -m 666 dev/null c 1 3
sudo mknod -m 666 dev/console c 1 5

# TODO: Clean and build the writer utility
echo " building writer app"
cd ${FINDER_APP_DIR}
pwd
make CROSS_COMPILE=$CROSS_COMPILE clean
make CROSS_COMPILE=$CROSS_COMPILE -j`nproc`

# TODO: Copy the finder related scripts and executables to the /home directory
# on the target rootfs
echo "shipping finder-app content to rootfs"
cp -rL ${FINDER_APP_DIR}/* ${OUTDIR}/rootfs/home
cp -r ${FINDER_APP_DIR}/../conf/ ${OUTDIR}/rootfs

# TODO: Chown the root directory
echo "changing rootfs permission"
sudo chown -R root:root ${OUTDIR}/rootfs

# TODO: Create initramfs.cpio.gz
echo "creating initramfs.cpio.gz"
cd "$OUTDIR/rootfs"
find . | cpio -H newc -ov --owner root:root > ${OUTDIR}/initramfs.cpio
gzip -f ${OUTDIR}/initramfs.cpio
echo "Done..."
