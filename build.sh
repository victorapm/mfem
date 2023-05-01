#!/bin/bash

# Echo usage information
case $1 in
   -h|-help)
      cat <<EOF
      -d|--debug:       Debug version
     -bt|--build-type:  Build type (CPU/CUDA/HIP)
      -t|--test:        Test the library
     -nc|--no-clean:    Do not run clean build
EOF
      exit
      ;;
esac

CWD=$(pwd)
MFEM_DIR=${HOME}/projects/forks/mfem-dev
HOST=${HOST:-$(hostname)}
DEBUG="no"
BUILD_TYPE="none"
TEST="no"
CLEAN="yes"
MODE="rel"
echo -e "Running on Host: ${HOST}"

while [ "$*" ]; do
   case $1 in
      -d|--debug)
         MODE="dbg"; shift
         ;;

     -bt|--build-type)
         shift;
         case $1 in
             "cpu"|"CPU")
                 BUILD_TYPE="cpu"
                 ;;

             "cuda"|"CUDA")
                 BUILD_TYPE="cuda"
                 ;;

             "hip"|"HIP"|"rocm")
                 BUILD_TYPE="rocm"
                 ;;

             *)
                 echo "Unknown input: $1"
	         exit
                 ;;
         esac
         shift
         ;;

      -t|--test)
         TEST="yes"; shift
         ;;

     -nc|--no-clean)
         CLEAN="no"; shift
         ;;

      *)
         echo "Unknown input: $1"
	 exit
         ;;
   esac
done

if [[ ${BUILD_TYPE} == "none" ]]; then
    echo -e "Please specify a build type! -bt <CPU|CUDA|HIP>"
    exit 0
fi

# Set host specific options
case ${HOST} in
    "lassen")
        NP=40
        COMPILER="xlc"
        COMPILER_VERSION=16
        CUDA_VERSION=11.2.0
        WORK=${WORK}
        module load cuda/${CUDA_VERSION}
        ;;

    "nztux")
        NP=24
        COMPILER="gcc"
        COMPILER_VERSION=11.3.0
        WORK=${HOME}
        ;;

    "tioga")
        NP=24
        COMPILER="cce"
        COMPILER_VERSION=15.0.1
        ROCM_VERSION=5.4.0
        BUILD_TYPE+="-${ROCM_VERSION}"
        WORK=${WORK}
        module load rocm/${ROCM_VERSION}
        ;;

    *)
        echo -e "Unknown host!"
        exit
        ;;
esac

MFEM_BUILD_DIR=${WORK}/projects/forks/mfem-dev/build
if [[ ${CLEAN} == "yes" ]]; then
    rm -rf ${MFEM_BUILD_DIR}
fi
mkdir -p ${MFEM_BUILD_DIR}

# Set user config
CONFIG_SUFFIX=${COMPILER}-${COMPILER_VERSION}_${BUILD_TYPE}_${MODE}
USER_CONFIG=${MFEM_DIR}/config/${HOST}_${CONFIG_SUFFIX}.mk
if [[ ! -f ${USER_CONFIG} ]]; then
    echo -e "Config file not found! ${USER_CONFIG}"
    exit 0
fi
MFEM_INSTALL_DIR=${WORK}/projects/forks/mfem-dev/install/${CONFIG_SUFFIX}

make BUILD_DIR=${MFEM_BUILD_DIR} config USER_CONFIG=${USER_CONFIG}
cp ${USER_CONFIG} ${MFEM_BUILD_DIR}/config
cd ${MFEM_BUILD_DIR}
make -j${NP}
make examples -j${NP}
make install PREFIX=${MFEM_INSTALL_DIR}

# Test library integration
if [[ $TEST == "yes" ]]; then
    echo -e "\n\n\nTesting MFEM-Hypre integration..."
    make test
fi

# Build a few drivers
mkdir -p ${MFEM_INSTALL_DIR}/bin
cp -rfv ${MFEM_BUILD_DIR}/examples/ex2p ${MFEM_INSTALL_DIR}/bin

# Go back to starting folder
cd ${CWD}
