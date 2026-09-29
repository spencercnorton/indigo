# syntax=docker/dockerfile:1
# The emulator runner: Dolphin (pinned), a virtual X server with software
# OpenGL, FFmpeg and the Python the harness in buildtools/ui/emulator/ uses,
# gxtexconv for the demonstration disc's posters, and a GitHub Actions runner
# that takes one job.
FROM ghcr.io/extremscorner/libogc2@sha256:e6531ecaa458d0b5d8c9ba57cee1facc5fb9120808c6d9eebb2c05e4ffaf6f0f AS toolchain

FROM ubuntu:26.04@sha256:da6fc2be547864451aa253836dd926da33623312df4a9a243e35dc877c378a78

# Debian and Ubuntu install Dolphin in /usr/games.
ENV DEBIAN_FRONTEND=noninteractive LC_ALL=C.UTF-8 TZ=UTC \
    PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin:/usr/games

RUN apt-get update \
 && apt-get install -y --no-install-recommends \
      ca-certificates curl dolphin-emu=2512+dfsg-3 dolphin-emu-data=2512+dfsg-3 \
      ffmpeg genisoimage git jq libegl-mesa0 libgl1-mesa-dri libicu78 libkrb5-3 \
      liblttng-ust1t64 libssl3t64 python3 python3-numpy python3-pil unzip xauth \
      xvfb xz-utils \
 && rm -rf /var/lib/apt/lists/*

# The same static gxtexconv the build runner has, from the same image.
COPY --from=toolchain /opt/devkitpro/tools/bin/gxtexconv /usr/local/bin/gxtexconv

# X keeps its sockets in /tmp/.X11-unix, which only root may create.
RUN useradd --create-home --uid 1001 --shell /bin/bash runner \
 && mkdir -p /etc/indigo-ci \
 && mkdir -m 1777 /tmp/.X11-unix \
 && dolphin-emu-nogui --version > /etc/indigo-ci/emulator \
 && gxtexconv --version

ARG RUNNER_VERSION
ARG RUNNER_SHA256
RUN mkdir /home/runner/actions-runner \
 && curl -fsSL -o /tmp/runner.tgz \
      "https://github.com/actions/runner/releases/download/v${RUNNER_VERSION}/actions-runner-linux-x64-${RUNNER_VERSION}.tar.gz" \
 && echo "${RUNNER_SHA256}  /tmp/runner.tgz" | sha256sum -c - \
 && tar -xzf /tmp/runner.tgz -C /home/runner/actions-runner \
 && rm /tmp/runner.tgz \
 && chown -R runner:runner /home/runner
COPY --chmod=0755 entrypoint.sh /opt/indigo-ci/

USER runner
WORKDIR /home/runner
ENTRYPOINT ["/opt/indigo-ci/entrypoint.sh"]
