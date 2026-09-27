# 1. Start from the Microsoft provided Ubuntu 26.04 base image
FROM ubuntu:26.04

# 2. Suppress interactive installation menus 
ENV DEBIAN_FRONTEND=noninteractive

# Installieren Sie grundlegende Tools, die für die Arbeit im Terminal nützlich sind
RUN apt-get update && apt-get install -y \
    curl \
    git \
    sudo \
    wget \
    build-essential

# 3. install ga68 alongside Python 3 
RUN apt-get install -y \
    ga68-16-x86-64-linux-gnu \
    python3 \
    python3-pip \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*

# 3a. CREATE SYMLINK: Maps 'ga68' to the actual installed compiler binary
RUN ln -s /usr/bin/x86_64-linux-gnu-ga68-16 /usr/bin/ga68-16
RUN ln -s /usr/bin/x86_64-linux-gnu-ga68-16 /usr/bin/ga68

# 4. Inject your custom script into the global system path
COPY u682a68 /usr/local/bin/u682a68

# 5. Provide execution permissions to your python script
RUN chmod +x /usr/local/bin/u682a68

# WICHTIG: Entfernt den standardmäßigen Ubuntu-User, um UID/GID 1000 freizugeben
RUN if id -u ubuntu >/dev/null 2>&1; then userdel -r ubuntu; fi \
    && if getent group ubuntu >/dev/null 2>&1; then groupdel ubuntu; fi

# Erstellen Sie einen non-root Benutzer "vscode" (Standard für Codespaces) mit sudo-Rechten
ARG USERNAME=vscode
ARG USER_UID=1000
ARG USER_GID=$USER_UID

RUN groupadd --gid $USER_GID $USERNAME \
    && useradd --uid $USER_UID --gid $USER_GID -m $USERNAME \
    && echo $USERNAME ALL=\(root\) NOPASSWD:ALL > /etc/sudoers.d/$USERNAME \
    && chmod 0440 /etc/sudoers.d/$USERNAME

# Wechseln Sie standardmäßig auf den neuen Benutzer
USER $USERNAME
