FROM nvcr.io/nvidia/tensorrt:21.03-py3
ARG distribution=$(. /etc/os-release;echo $ID$VERSION_ID | sed -e 's/\.//g')
# RUN distribution=$(. /etc/os-release;echo $ID$VERSION_ID | sed -e 's/\.//g') \
RUN echo "deb http://developer.download.nvidia.com/compute/cuda/repos/$distribution/x86_64 /" | tee /etc/apt/sources.list.d/cuda.list

RUN && apt-key adv --fetch-keys https://developer.download.nvidia.com/compute/cuda/repos/${distribution}/x86_64/7fa2af80.pub \
    && wget https://developer.download.nvidia.com/compute/cuda/repos/$distribution/x86_64/cuda-${distribution}.pin \
    && mv cuda-${distribution}.pin /etc/apt/preferences.d/cuda-repository-pin-600 \

RUN apt-get update \
    && apt-get install -y datacenter-gpu-manager

