# CANgaroo via Docker

Runs the [CANgaroo](https://github.com/Schildkroet/CANgaroo) AppImage on systems with an older glibc (e.g. Ubuntu 22.04), which fails natively with `GLIBC_2.38 not found`.

## Setup

Download the latest [CANgaroo AppImage](https://github.com/Schildkroet/CANgaroo/releases), place `CANgaroo-x86_64.AppImage` next to the `Dockerfile`, then:

```bash
chmod +x CANgaroo-x86_64.AppImage
docker build -t cangaroo .
```

## Run

Bring up the CAN interface on the host first (slcan example, 500 kbit/s):

```bash
sudo slcand -o -c -s6 /dev/ttyACM0 can0
sudo ip link set can0 up
```

Start CANgaroo:

```bash
docker run -it --rm --net=host \
  -v /tmp/.X11-unix:/tmp/.X11-unix \
  -v $(pwd)/CANgaroo-x86_64.AppImage:/app/cangaroo.AppImage \
  cangaroo
```

## Troubleshooting

- **No window:** if `echo $DISPLAY` isn't `:0`, add `-e DISPLAY=$DISPLAY`; if it still fails, run `xhost +local:docker`.
- **Missing `libXXX.so`:** add the package to the `Dockerfile` and rebuild.
- **`can0` not listed:** check it's `UP` on the host (`ip link show can0`) before starting CANgaroo.
