FROM ubuntu:18.04
RUN apt-get update -y
RUN apt-get install wget build-essential unzip -y
WORKDIR /opt/
RUN wget https://github.com/hostmaria/mariacoin/releases/download/v5.5.1/maria-5.5.1-ubuntu18-daemon.zip
RUN unzip maria-5.5.1-ubuntu18-daemon.zip
RUN chmod +x mariad
RUN chmod +x maria-cli
RUN mv mariad /usr/bin
RUN mv maria-cli /usr/bin
RUN rm maria*
RUN wget https://github.com/hostmaria/mariacoin/releases/download/v5.5.1/util.zip
RUN unzip util.zip
RUN chmod +x /util/fetch-params.sh
RUN ./util/fetch-params.sh
RUN rm -R util
RUN rm util.zip
RUN wget https://raw.githubusercontent.com/TheRetroMike/rmt-nomp/master/scripts/blocknotify.c
RUN gcc blocknotify.c -o /usr/bin/blocknotify
CMD /usr/bin/mariad -printtoconsole
