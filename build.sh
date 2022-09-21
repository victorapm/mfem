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
while [ "$*" ]; do
   case $1 in
      -d|--debug)
         DEBUG="yes"; shift
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

MFEM_DIR=/home/victor/projects/mfem-vm
HOST=$(hostname)
NP=24

if [[ $CPU_BUILD == "yes" ]]; then
    BUILD_DIR=${MFEM_DIR}/build
    if [[ ${DEBUG} == "yes" ]]; then
        USER_CONFIG=${MFEM_DIR}/config/${HOST}_gcc11_dbg.mk
    else
        USER_CONFIG=${MFEM_DIR}/config/${HOST}_gcc11_rel.mk
    fi

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
    USER_CONFIG=${MFEM_DIR}/config/${HOST}_gcc11_cuda_rel.mk

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
