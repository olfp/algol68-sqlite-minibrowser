# 1. Start from the official Ubuntu 26.04 base image
FROM ubuntu:26.04

# 2. Suppress interactive installation menus 
ENV DEBIAN_FRONTEND=noninteractive

# 3. Update repositories and install ga68 alongside Python 3 
RUN apt-get update && apt-get install -y \
    ga68-16-x86-64-linux-gnu \
    python3 \
    python3-pip \
    && rm -rf /var/lib/apt/lists/*

# 3a. CREATE SYMLINK: Maps 'ga68' to the actual installed compiler binary
RUN ln -s /usr/bin/x86_64-linux-gnu-ga68-16 /usr/bin/ga68-16
RUN ln -s /usr/bin/x86_64-linux-gnu-ga68-16 /usr/bin/ga68

# 4. Inject your custom script into the global system path
COPY u682a68 /usr/local/bin/u682a68

# 5. Provide execution permissions to your python script
RUN chmod +x /usr/local/bin/u682a68
