
# Build arguments that affect multiple stages, changeable by the command line
# at build-time.
ARG ARM_COMPUTE_ROOT="/opt/ComputeLibrary"
ARG ARM_COMPUTE_LIB="linux-arm64-v8a-neon"

#
# STAGE: Download ARM Compute Library
#
# This stage encapsulates the ARM compute library so that it can be downloaded
# once and then be extracted from the cache as long as the stage doesn't change.
# Since the ARM compute library is upwards of 1GB, this can save a lot of build
# time.
#
# Also, files not copied into later stages are not included in the final image,
# reducing image size versus downloading this there.
#
FROM arm64v8/ubuntu:18.04 as armcompute
RUN apt-get update && apt-get install --no-install-recommends -y \
ca-certificates \
curl \
&& rm -rf /var/lib/apt/lists/*

ARG ARM_COMPUTE_ROOT

RUN mkdir -p $ARM_COMPUTE_ROOT \
&& curl -SL https://github.com/ARM-software/ComputeLibrary/releases/download/v20.05/arm_compute-v20.05-bin-linux.tar.gz \
| tar -xzC $ARM_COMPUTE_ROOT --strip-components=1 \
&& find $ARM_COMPUTE_ROOT/lib -type f -name "*.a" -size +1M -delete \
&& find $ARM_COMPUTE_ROOT/lib -type d -name "*armv7a*" -exec rm -rf {} +

#
# STAGE: Build main image
#
# Main build stage. Produces the project image, copying necessary ARM compute
# library files into its image to be used for building with the library.
#
FROM arm64v8/ubuntu:18.04

# Inherit global build args
ARG ARM_COMPUTE_ROOT
ARG ARM_COMPUTE_LIB

# Default UID/GID for Linux
ARG UID=1000
ARG GID=1000

# Disable interactive mode
ENV DEBIAN_FRONTEND noninteractive

RUN apt-get update && apt-get upgrade -y
RUN apt-get install --no-install-recommends -y \
build-essential \
gcc-8 \
g++-8 \
git \
libgl1-mesa-dri \
libglx-mesa0 \
libpng-dev \
libsdl2-dev \
mesa-utils \
make \
network-manager \
software-properties-common \
sudo \
&& rm -rf /var/lib/apt/lists/*

ENV ARM_COMPUTE_DIR="$ARM_COMPUTE_ROOT"

COPY --from=armcompute $ARM_COMPUTE_DIR $ARM_COMPUTE_DIR

ENV LD_LIBRARY_PATH="lib:$ARM_COMPUTE_DIR/lib/$ARM_COMPUTE_LIB"
ENV ARCH="aarch64"
ENV TERM="xterm-256color"

# Configure the container user to avoid always running as root (bad practice)
# and outputting built files with root:root permissions.
RUN groupadd -g $GID -o user \
&& useradd -u $UID -m -g user -G plugdev user \
&& echo 'user ALL = NOPASSWD: ALL' > /etc/sudoers.d/user \
&& chmod 0440 /etc/sudoers.d/user

# Default to a command prompt entrypoint
CMD ["/bin/bash"]
USER user

# Start in the /workdir directory (host $PWD)
WORKDIR /workdir
VOLUME ["/workdir"]
