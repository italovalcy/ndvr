FROM ubuntu:20.04

RUN apt-get update && apt-get install -y --no-install-recommends \
    git ca-certificates sudo curl mawk dstat procps && \
    update-ca-certificates && \
    alias python=python3 && \
    git clone https://github.com/named-data/mini-ndn.git && \
    cd mini-ndn && ./install.sh -y

RUN cd /mini-ndn && git clone https://github.com/italovalcy/ndvr && \
    cd ndvr/ && git checkout ndvr-emu && \
    apt-get install -y --no-install-recommends protobuf-compiler libndn-cxx-dev libprotobuf-dev && \
    alias python=python2.7 && \
    ./waf configure --debug && \
    ./waf && \
    ./waf install && \
    cp config/validation.conf /etc/ndn/ndvr-validation.conf && \
    cp minindn/apps/ndvr.py /mini-ndn/minindn/apps/ndvr.py && \
    cp minindn/ndvr* /mini-ndn/examples/ && \
    cp minindn/nlsr-paper.py /mini-ndn/examples/nlsr/ && \
    cp minindn/topologies/* /mini-ndn/topologies/ && \
    cp minindn/get-cpu-usage.sh /usr/local/bin/ && \
    cd ../ && patch -p1 < ndvr/minindn/adjustments-minindn.patch

RUN cd /mini-ndn && git clone --branch ndn-tools-22.02 https://github.com/named-data/ndn-tools && \
    cd ndn-tools && patch -p1 < ../ndvr/minindn/ndn-tools-22.02-ndn-ping-variable-bit-rate.patch && \
    ./waf configure --prefix=/usr && ./waf install && \
    cd ../ && rm -rf ndn-tools && \
    git clone https://github.com/named-data/NLSR && cd NLSR && \
    git checkout a3a63975d13bcdf3a6851dcd8f9413049fb62c7d && \
    apt-get install -y --no-install-recommends libchronosync-dev libpsync-dev && \
    patch -p1 < ../ndvr/minindn/adjustments-nlsr.patch && \
    ./waf configure --bindir=/usr/bin --sysconfdir=/etc && ./waf install && \
    cd .. && rm -rf NLSR && \
    cp ndvr/minindn/nlsr.conf.sample-int1 /etc/ndn/nlsr.conf.sample-int1 && \
    mv /etc/ndn/nlsr.conf.sample /etc/ndn/nlsr.conf.sample-orig && \
    ln -s /etc/ndn/nlsr.conf.sample-int1 /etc/ndn/nlsr.conf.sample && \
    rm -rf ndvr && rm -rf /var/lib/apt/lists/*

COPY docker-entrypoint.sh /docker-entrypoint.sh

# Change the working directory to /mini-ndn
WORKDIR /mini-ndn

ENTRYPOINT ["/docker-entrypoint.sh"]
