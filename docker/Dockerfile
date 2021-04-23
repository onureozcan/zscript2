FROM ubuntu:18.04

RUN apt-get update
RUN apt-get install -y ssh
RUN apt-get install -y build-essential
RUN apt-get install -y gcc
RUN apt-get install -y g++
RUN apt-get install -y gdb
RUN apt-get install -y clang
RUN apt-get install -y cmake

RUN apt-get install -y valgrind
RUN apt-get install -y git
RUN apt-get install -y default-jre
RUN apt-get install -y pkg-config
RUN apt-get install -y uuid-dev

 # Required to not get a 'Missing privilege separation directory' error
RUN mkdir /var/run/sshd && mkdir -p /root/.ssh

RUN useradd -m user && yes password | passwd user

EXPOSE 22

CMD /usr/sbin/sshd -D
