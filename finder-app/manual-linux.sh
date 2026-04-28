#!/bin/bash
# Script outline to install and build kernel.
# Author: Siddhant Jajoo.

set -e
set -u

OUTDIR=/tmp/aeld
KERNEL_REPO=git://git.kernel.org/pub/scm/linux/kernel/git/stable/linux-stable.git
KERNEL_VERSION=v5.15.163
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

    # kernel build steps here
    make ARCH=arm64 CROSS_COMPILE=${CROSS_COMPILE} mrproper
    make ARCH=arm64 CROSS_COMPILE=${CROSS_COMPILE} defconfig
    make -j4 ARCH=arm64 CROSS_COMPILE=${CROSS_COMPILE} all
    # skip modules
    make ARCH=arm64 CROSS_COMPILE=${CROSS_COMPILE} dtbs
fi

echo "Adding the Image in outdir"
cp "${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image" "${OUTDIR}/Image"

echo "Creating the staging directory for the root filesystem"
cd "$OUTDIR"
if [ -d "${OUTDIR}/rootfs" ]
then
	echo "Deleting rootfs directory at ${OUTDIR}/rootfs and starting over"
    sudo rm  -rf ${OUTDIR}/rootfs/home/*
else
    mkdir -p rootfs && cd rootfs
    mkdir -p bin dev etc home lib lib64 proc sbin sys tmp usr var
    mkdir -p usr/bin usr/lib usr/sbin
    mkdir -p var/log
fi

cd "$OUTDIR"
if [ ! -d "${OUTDIR}/busybox" ]
then
git clone git://busybox.net/busybox.git
    cd busybox
    git checkout ${BUSYBOX_VERSION}

    # Configure busybox
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} distclean
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} defconfig

    # Make and install busybox
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE}
    make CONFIG_PREFIX="${OUTDIR}/rootfs" ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} install
else
    cd busybox
fi


# Open rootfs
cd "${OUTDIR}/rootfs"

# Print dependencies
echo "Library dependencies"
${CROSS_COMPILE}readelf -a bin/busybox | grep "program interpreter"
${CROSS_COMPILE}readelf -a bin/busybox | grep "Shared library"

# Get the clean absolute path for the sysroot
SYSROOT=$(${CROSS_COMPILE}gcc -print-sysroot)

# Get the Program Interpreter path and clean it up
INTERPRETER=$(${CROSS_COMPILE}readelf -a bin/busybox | grep "program interpreter" | awk -F': ' '{print $2}' | tr -d '[]' | sed 's|^/||')

if [ -n "$INTERPRETER" ]; then
    # Check if interpreter already exists in rootfs
    if [ ! -f "${OUTDIR}/rootfs/lib/$(basename $INTERPRETER)" ]; then
        echo "Copying interpreter: $INTERPRETER"
        cp -a "${SYSROOT}/${INTERPRETER}" "${OUTDIR}/rootfs/lib/"
    else
        echo "Interpreter already exists, skipping."
    fi
fi

# Get the Shared Libraries
LIBS=$(${CROSS_COMPILE}readelf -a bin/busybox | grep "Shared library" | awk -F'[' '{print $2}' | tr -d ']')

for lib in $LIBS; do
    # Check if the specific library already exists in lib64
    if [ ! -f "${OUTDIR}/rootfs/lib64/$lib" ]; then
        echo "Finding and copying library: $lib"
        find "$SYSROOT" -name "$lib" -exec cp -a {} "${OUTDIR}/rootfs/lib64/" \;
    else
        echo "Library $lib already exists, skipping."
    fi
done

# Make device nodes
if [ ! -e dev/null ]; then
    sudo mknod -m 666 dev/null c 1 3
fi

if [ ! -e dev/console ]; then
    sudo mknod -m 666 dev/console c 5 1
fi

# Clean and build the writer utility
cd "${FINDER_APP_DIR}"
make clean
make CROSS_COMPILE=${CROSS_COMPILE}

# Copy the finder related scripts and executables to the /home directory on the target rootfs

sudo mkdir -p "${OUTDIR}/rootfs/home"
sudo cp "${FINDER_APP_DIR}/writer" "${OUTDIR}/rootfs/home/"
sudo cp "${FINDER_APP_DIR}/finder.sh" "${OUTDIR}/rootfs/home/"
sudo cp "${FINDER_APP_DIR}/finder-test.sh" "${OUTDIR}/rootfs/home/"
sudo cp "${FINDER_APP_DIR}/autorun-qemu.sh" "${OUTDIR}/rootfs/home/"

sudo mkdir -p "${OUTDIR}/rootfs/home/conf"
sudo cp "${FINDER_APP_DIR}/conf/username.txt" "${OUTDIR}/rootfs/home/conf/"
sudo cp "${FINDER_APP_DIR}/conf/assignment.txt" "${OUTDIR}/rootfs/home/conf/"

sudo mkdir -p "${OUTDIR}/rootfs/conf"
sudo cp "${FINDER_APP_DIR}/conf/assignment.txt" "${OUTDIR}/rootfs/conf/"
sudo cp "${FINDER_APP_DIR}/conf/username.txt" "${OUTDIR}/rootfs/conf/"

# Chown the root directory
cd "${OUTDIR}"
sudo chown -R root:root rootfs

# Create initramfs.cpio.gz
cd "${OUTDIR}/rootfs"
find . | cpio -H newc -ov --owner root:root > "${OUTDIR}/initramfs.cpio"
gzip -f "${OUTDIR}/initramfs.cpio"
