FROM ubuntu:20.04

RUN apt-get update && apt-get install -y --no-install-recommends \
    git ca-certificates sudo curl mawk dstat procps && \
    update-ca-certificates && \
    alias python=python3 && \
    git clone https://github.com/named-data/mini-ndn.git && \
    cd mini-ndn && ./install.sh -y && \
    git clone https://github.com/italovalcy/ndvr && \
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
    cd ../ && patch -p1 < ndvr/minindn/adjustments-minindn.patch && \
    git clone --branch ndn-tools-22.02 https://github.com/named-data/ndn-tools && \
    cd ndn-tools && patch -p1 < ../ndvr/minindn/ndn-tools-22.02-ndn-ping-variable-bit-rate.patch && \
    ./waf configure --prefix=/usr && ./waf && ./waf install && cd ../ && \
    rm -rf /var/lib/apt/lists/*

COPY docker-entrypoint.sh /docker-entrypoint.sh

# Change the working directory to /mini-ndn
WORKDIR /mini-ndn

ENTRYPOINT ["/docker-entrypoint.sh"]
