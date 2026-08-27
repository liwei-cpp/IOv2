# Build environment for the C++ CI workflow.
#
# Every job in ci.yml used to apt-get the same handful of packages on every run,
# which put Docker Hub and the Debian mirrors on the critical path six times per
# pipeline -- both of which have failed to answer in practice. Baking them in
# removes both, and freezes what the jobs build against until this image is
# rebuilt on purpose.
#
# A single image serves every job, including the clang++ leg of the build matrix:
# sharing one base is what keeps both compilers on the same libstdc++.
FROM gcc:15

# tzdata prompts for a timezone when it can find any interactive frontend.
ARG DEBIAN_FRONTEND=noninteractive

# Recommends are deliberately left on, matching what the workflow's apt-get
# installed before, so the image reproduces the environment the tests already
# pass in rather than a minimized guess at it.
#
# cmake arrives ahead of its first user. The test suite is moving from
# test/Makefile to CMake/CTest, and that migration runs both build systems side
# by side for a release so their results can be compared; those shadow jobs
# cannot start until an image carrying cmake has been published. No job consumes
# it yet, so this addition changes nothing for the existing six.
RUN apt-get update \
 && apt-get install -y \
      clang \
      clang-tidy \
      cmake \
      lcov \
      libbotan-2-dev \
      locales-all \
      make \
      pkg-config \
      tzdata \
      valgrind \
      zlib1g-dev \
 && rm -rf /var/lib/apt/lists/*
