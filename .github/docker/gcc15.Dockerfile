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
# cmake and lcov are what the test suite is built and measured with; it moved
# off test/Makefile, so make is now only needed for the top-level shared-library
# packaging targets. curl fetches the GoogleTest source below.
RUN apt-get update \
 && apt-get install -y \
      clang \
      clang-tidy \
      cmake \
      curl \
      lcov \
      libbotan-2-dev \
      locales-all \
      make \
      pkg-config \
      tzdata \
      valgrind \
      zlib1g-dev \
 && rm -rf /var/lib/apt/lists/*

# GoogleTest, as source rather than a package, and unpacked rather than built.
#
# It has to be compiled with the same flags as the code under test, which rules
# out any pre-built copy: the suite is built five ways (release, debug,
# sanitizer, tsan, coverage) and under TSan in particular, linking uninstrumented
# library code is how you manufacture false positives. A distro libgtest is also
# built to some other -std, which pairs C++17 template instantiations with C++23
# ones across an ODR boundary.
#
# Baked in rather than downloaded by FetchContent at configure time, for the same
# reason the apt packages are: nothing on a job's critical path should depend on
# a network that has failed to answer before. CMake points
# FETCHCONTENT_SOURCE_DIR_GOOGLETEST here and skips the download; a developer who
# does not set it just downloads the same pinned tag.
#
# Pinned by tag and checked by digest, matching how the rest of this image is
# frozen. v1.17.0 (2025-04-30) rather than v1.18.0 (2026-08-10): both want
# C++17 and CMake 3.16, so the newer one buys nothing here that being three
# weeks old does not cost. googlemock rides along in the same tarball.
ARG GTEST_VERSION=1.17.0
ARG GTEST_SHA256=65fab701d9829d38cb77c14acdc431d2108bfdbf8979e40eb8ae567edf10b27c
RUN curl -fsSL -o /tmp/googletest.tar.gz \
      "https://github.com/google/googletest/archive/refs/tags/v${GTEST_VERSION}.tar.gz" \
 && echo "${GTEST_SHA256}  /tmp/googletest.tar.gz" | sha256sum -c - \
 && tar -xzf /tmp/googletest.tar.gz -C /opt \
 && mv "/opt/googletest-${GTEST_VERSION}" /opt/googletest \
 && rm /tmp/googletest.tar.gz
