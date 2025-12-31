FROM fedora:latest

# Install requested packages
RUN dnf -y update && dnf -y install --skip-unavailable \
    git \
    cmake \
    gdb \
    gcc \
    gcc-c++ \
    make \
    clang \
    clang-tools-extra \
    pkgconfig \
    glibc-doc \
    tcpdump \
    wireshark-cli \
    && dnf clean all

WORKDIR /workspace