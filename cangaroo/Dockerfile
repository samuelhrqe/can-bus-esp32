FROM ubuntu:24.04

ENV LANG=C.UTF-8
ENV LC_ALL=C.UTF-8
ENV DISPLAY=:0

RUN apt-get update && apt-get install -y --no-install-recommends \
    libusb-1.0-0 \
    libgl1 libegl1 \
    libxkbcommon0 libxkbcommon-x11-0 \
    libxcb-cursor0 libxcb-icccm4 libxcb-image0 libxcb-keysyms1 \
    libxcb-randr0 libxcb-render-util0 libxcb-shape0 libxcb-xfixes0 \
    libxcb-xinerama0 libxcb-xkb1 \
    libdbus-1-3 libglib2.0-0 \
    libnl-3-200 libnl-route-3-200 \
    libgcrypt20 libkrb5-3 libgssapi-krb5-2 \
    fonts-dejavu-core \
    libfontconfig1 \
    libxrender1 libxext6 libsm6 libice6 libxi6 \
 && rm -rf /var/lib/apt/lists/*

WORKDIR /app
ENTRYPOINT ["/app/cangaroo.AppImage", "--appimage-extract-and-run"]