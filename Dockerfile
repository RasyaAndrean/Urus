FROM gcc:latest

LABEL org.opencontainers.image.source=https://github.com/Urus-Foundation/Urus
LABEL org.opencontainers.image.description="URUS Programming Language Compiler v1.0.1(F)"
LABEL org.opencontainers.image.licenses=Apache-2.0

RUN apt-get update && apt-get install -y cmake && rm -rf /var/lib/apt/lists/*

WORKDIR /urus

COPY compiler/ ./compiler/
COPY examples/ ./examples/

RUN cd compiler && \
    cmake -S . -B build && \
    cmake --build build && \
    cmake --install build

WORKDIR /workspace

ENTRYPOINT ["urusc"]
CMD ["--help"]
