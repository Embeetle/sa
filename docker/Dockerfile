# Manylinux image with glibc 2.28 used for Python wheels, based on AlmaLinux 8
# On this docker image, there are several Python versions installed.
FROM quay.io/pypa/manylinux_2_28_x86_64:latest

# Install required build tools.
# epel-release must be installed for p7zip to be found
# 7zip needed to unpack downloaded toolchain
# OpenSSH needed for git pull
# ninja-build needed for LLVM build
#
RUN yum -y install epel-release 
RUN yum -y install openssh-clients ninja-build epel-release p7zip p7zip-plugins

# Set CWD (like $cd )
WORKDIR /root

# Make environment variable with the Docker 'ARG' command. This environment
# variable is retained throughout the docker script.
ARG ENV=/root/env

# Make virtual environment with python3.9 at the given location. The location is
# stored in the environment variable 'ENV'.  PS: If this image also has
# python3.10, it could be used here instead.
RUN python3.9 -m venv --prompt Py3.9 ${ENV}

# Activate virtual environment and upgrade pip. The activation is not retained
# throughout the docker script, because this kind of activation sets environment
# variables (such as $PATH), which are not retained during the docker
# script. 'RUN' starts a shell, processes the commands and then closes.
RUN . ${ENV}/bin/activate && pip install --upgrade pip

# Activate virtual environment again, then pip-install everything required for
#  Embeetle.
#RUN . ${ENV}/bin/activate && pip install \
#  setuptools \
#  wheel \
#  regex \
#  pyserial \
#  packaging \
#  hypothesis \
#  zmq \
#  watchdog \
#  xmltodict \
#  six \
#  pyelftools \
#  colorama \
#  lowbar \
#  cython \
#  cx_freeze \
#  feedparser \
#  pyte

# Virtual copy (through symlink) of '/data/llvm' to '.'
RUN ln -s /data/llvm .
RUN ln -s /data/sa .
RUN ln -s /data/bld .

# 'COPY' will copy something from outside the Docker image into the Docker
# image.  Copy the SA build script, '.bashrc' script and '.ssh' folder.
COPY .bashrc .bashrc
COPY .gitconfig .gitconfig
COPY .ssh .ssh
COPY bin bin
RUN chmod go-rwx .ssh/id_rsa .ssh/known_hosts .ssh/private_key.ssh
RUN chmod +x bin/*

# Set initial PATH to include ~/bin. If it is set in .bashrc, it will not apply
# when directly passing a script or command to `docker run`.
ENV PATH "~/bin:$PATH"

# Default command when this docker image is launched.
CMD ["/bin/bash","-l"]
