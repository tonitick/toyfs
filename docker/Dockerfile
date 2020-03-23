FROM ubuntu:bionic

# ENV http_proxy=http://proxy.cse.cuhk.edu.hk:8000/
# ENV https_proxy=http://proxy.cse.cuhk.edu.hk:8000/
# ENV ftp_proxy=http://proxy.cse.cuhk.edu.hk:8000/
# ENV gopher_proxy=http://proxy.cse.cuhk.edu.hk:8000/

RUN apt update
RUN apt install -y git vim gcc make pkg-config fuse libfuse-dev screen

WORKDIR /root
RUN git clone https://github.com/tonitick/toyfs.git
RUN cd toyfs && make

