FROM ubuntu:16.10
MAINTAINER Christian Schafmeister <meister@temple.edu>

# install all clasp build deps: mostly clang, boost, LLVM4
RUN apt-get update && apt-get -y upgrade && apt-get install -y \
  software-properties-common python-software-properties wget curl
RUN curl http://apt.llvm.org/llvm-snapshot.gpg.key | apt-key add -
RUN apt-add-repository "deb http://apt.llvm.org/yakkety/ llvm-toolchain-yakkety-4.0 main"
RUN apt-get update && apt-get install -y \
  gcc g++ llvm clang cmake libgc-dev libgmp-dev binutils-gold binutils-dev \
  zlib1g-dev libncurses-dev libboost-filesystem-dev libboost-regex-dev \
  libboost-date-time-dev libboost-program-options-dev libboost-system-dev \
  libboost-iostreams-dev csh flex gfortran zlib1g-dev libbz2-dev patch \
  git sbcl libexpat-dev wget vim libzmq3-dev \
  clang-4.0 libclang-common-4.0-dev libclang-4.0-dev libclang1-4.0 clang1-4.0-dbg \
  libllvm4.0 libllvm4.0-dbg lldb-4.0 llvm-4.0 llvm-4.0-dev llvm-4.0-doc \
  llvm-4.0-runtime clang-format-4.0 python-clang-4.0 lld-4.0
ENV CC=/usr/bin/clang-4.0 CXX=/usr/bin/clang++-4.0

# Copy the entire clasp directory less .dockerignore and clean the build dir
ADD . clasp
WORKDIR /clasp
RUN rm -rf build/* && mkdir -p /out/clasp

# checkout submodules, configure, and build
RUN echo "LLVM_CONFIG_BINARY='/usr/bin/llvm-config-4.0'" > wscript.config
RUN echo "INSTALL_PATH_PREFIX='/out/clasp'" >> wscript.config
RUN echo "CLASP_VERSION='clasp_docker'" >> wscript.config
RUN ./waf update_submodules
RUN ./waf configure
RUN ./waf -j $(nproc) build_cboehm -v
RUN ./waf install_cboehm

# add app user, to set correct UID and GID on files
RUN groupadd -g 9999 app && useradd -u 9999 -g 9999 -ms /bin/bash app
RUN chown -R app:app /out/clasp
