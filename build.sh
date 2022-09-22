#!/bin/bash

# Echo usage information
case $1 in
   -h|-help)
      cat <<EOF
      -d|--debug:   debug version
      -c|--cpu:     build for CPUs
     -cu|--cuda:    build for CUDA
     -hi|--hip:     build for HIP
      -t|--test:    Test the library
EOF
      exit
      ;;
esac

DEBUG="no"
CPU_BUILD="no"
CUDA_BUILD="no"
HIP_BUILD="no"
TEST="no"
MODE="rel"
while [ "$*" ]; do
   case $1 in
      -d|--debug)
         MODE="dbg"; shift
         ;;

      -c|--cpu)
         CPU_BUILD="yes"; shift
         ;;

     -cu|--cuda)
         CUDA_BUILD="yes"; shift
         ;;

     -hi|--hip)
         HIP_BUILD="yes"; shift
         ;;

      -t|--test)
         TEST="yes"; shift
         ;;

      *)
         echo "Unknown input: $1"
	 exit
         ;;
   esac
done

MFEM_DIR=${HOME}/projects/mfem-vm
HOST=${HOST:-$(hostname)}
echo -e "Running on Host: ${HOST}"

# Set host specific options
case ${HOST} in
    "lassen")
        COMPILER="xlc16"
        module load cuda/11.2.0
        NP=40
        ;;

    "nztux")
        COMPILER="gcc11"
        NP=24
        ;;

    *)
        echo -e "Unknown host!"
        exit
        ;;
esac

if [[ $CPU_BUILD == "yes" ]]; then
    BUILD_DIR=${MFEM_DIR}/build
    USER_CONFIG=${MFEM_DIR}/config/${HOST}_${COMPILER}_${MODE}.mk

    rm -rf ${BUILD_DIR}
    mkdir ${BUILD_DIR}
    make BUILD_DIR=${BUILD_DIR} config USER_CONFIG=${USER_CONFIG}
    cp ${USER_CONFIG} ${BUILD_DIR}/config
    cd ${BUILD_DIR}
    make -j${NP}
    make examples -j${NP}
    make install
else
    echo -e "Not building for CPUs..."
fi

if [[ $CUDA_BUILD == "yes" ]]; then
    BUILD_DIR=${MFEM_DIR}/build
    USER_CONFIG=${MFEM_DIR}/config/${HOST}_${COMPILER}_cuda_${MODE}.mk

    rm -rf ${BUILD_DIR}
    mkdir ${BUILD_DIR}
    make BUILD_DIR=${BUILD_DIR} config USER_CONFIG=${USER_CONFIG}
    cp ${USER_CONFIG} ${BUILD_DIR}/config
    cd ${BUILD_DIR}
    make -j${NP}
    make examples -j${NP}
    make install
else
    echo -e "\n\nNot building for CUDA..."
fi

if [[ $TEST == "yes" ]]; then
    echo -e "\n\n\nTesting MFEM-Hypre integration..."
    make test
fi
