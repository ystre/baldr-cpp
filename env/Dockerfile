FROM ubuntu:24.04

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    ninja-build \
    git \
    curl \
    python3 \
    python3-pip \
    pipx \
    pkg-config \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

ARG USERNAME=builder
ARG USER_UID=9999
ARG USER_GID=$USER_UID

RUN groupadd --gid $USER_GID $USERNAME \
    && useradd --uid $USER_UID --gid $USER_GID -m -s /bin/bash $USERNAME

USER $USER_UID

RUN pipx install conan
ENV BUILDER_HOME=/home/$USERNAME
ENV PATH=$BUILDER_HOME/.local/bin:$PATH
ENV CONAN_HOME=/tmp/workspace/.conan2

USER 0

COPY conan-profile $CONAN_HOME/profiles/default
COPY conan_provider.cmake /tmp/workspace/
RUN chmod -R 777 $BUILDER_HOME $CONAN_HOME

USER $USER_UID
