# CI image for the Linux jobs: the act base plus every dependency the lint and
# build jobs previously apt-installed and pip-installed on EVERY run, which cost
# one to two minutes per job before any real work started. Baking them here
# retires that step; the workflow keeps a presence-check fallback so it still
# functions on a generic ubuntu-22.04 runner, just slower.
#
# Registry: git.snalpha.com/alex/jak-ci:act-22.04, the forge's own package
# registry (the 5001 registry is a read-only pull-through cache for
# mcr.microsoft.com and rejects pushes; found out the hard way). Anonymous
# pull verified working, which is what the runner does. Rebuild and push
# after editing (docker login git.snalpha.com first):
#
#   docker build -t git.snalpha.com/alex/jak-ci:act-22.04 .forgejo/ci-image
#   docker push git.snalpha.com/alex/jak-ci:act-22.04
#
# The runner label ubuntu-22.04-jak maps to this image; the label registration
# lives in the runner's .runner file on atlas, not in this repository. This
# image is outside Watchtower (local registry), so base-image updates are
# manual: rebuild when catthehacker/ubuntu:act-22.04 moves or when the
# dependency list below changes.
FROM catthehacker/ubuntu:act-22.04

# The exact list the build job installed per run, plus ccache (the build now
# routes compiles through it) and clang-format pinned to CI's checked version.
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential cmake clang gcc g++ lcov make nasm \
    libxrandr-dev libxinerama-dev libxcursor-dev libpulse-dev \
    libxi-dev zip ninja-build libgl1-mesa-dev libssl-dev ccache \
  && rm -rf /var/lib/apt/lists/*

# System-wide, not --user: the per-run pip install landed in /root/.local/bin
# off PATH and warned about it on every single run.
RUN pip install --no-cache-dir clang-format==18.1.8
