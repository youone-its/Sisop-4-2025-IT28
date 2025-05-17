FROM ubuntu:20.04

ENV DEBIAN_FRONTEND=noninteractive

# Gunakan cache yang lebih efisien
RUN apt-get update && apt-get install -y --no-install-recommends \
    fuse3 \
    gcc \
    make \
    pkg-config \
    libfuse3-dev \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*

WORKDIR /app

# Copy source code
COPY antink.c .

# Compile
RUN gcc antink.c -o antink `pkg-config fuse3 --cflags --libs` -pthread \
    && rm antink.c

# Create directories
RUN mkdir -p /it24_host /antink_mount /var/log \
    && touch /var/log/it24.log

# Simple entrypoint
ENTRYPOINT ["./antink", "/it24_host", "/antink_mount"]