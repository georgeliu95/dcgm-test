FROM nvcr.io/nvidia/tensorrt:21.03-py3
# ARG DISTRIBUTION=$(. /etc/os-release;echo $ID$VERSION_ID | sed -e 's/\.//g')
RUN DISTRIBUTION=$(. /etc/os-release;echo $ID$VERSION_ID | sed -e 's/\.//g') \
    && echo "deb http://developer.download.nvidia.com/compute/cuda/repos/$DISTRIBUTION/x86_64 /" | tee /etc/apt/sources.list.d/cuda.list \
    && apt-key adv --fetch-keys https://developer.download.nvidia.com/compute/cuda/repos/${DISTRIBUTION}/x86_64/7fa2af80.pub \
    && wget https://developer.download.nvidia.com/compute/cuda/repos/$DISTRIBUTION/x86_64/cuda-${DISTRIBUTION}.pin \
    && mv cuda-${DISTRIBUTION}.pin /etc/apt/preferences.d/cuda-repository-pin-600

RUN apt-get update \
    && apt-get install -y datacenter-gpu-manager

