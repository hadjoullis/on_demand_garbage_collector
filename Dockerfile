# use Arch Linux as the base image
FROM archlinux:latest

# update the system fully and install the required packages
RUN pacman -Syu --noconfirm \
       clang \
       gdb \
       git \
       make \
       mimalloc \
       openssh \
       python \
       sudo \
       vi \
       vim

# create simple user
RUN useradd -m user && \
    echo "user:user" | chpasswd && \
    echo "user ALL=(ALL) NOPASSWD: ALL" >> /etc/sudoers

# password for root: 'root'
RUN echo "root:root" | chpasswd

# fix permitions for new user
RUN chown -R user:user /home/user/

# set user
USER user

# set default dir
WORKDIR /home/user

# start container shell
CMD ["/bin/sh"]
