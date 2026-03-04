FROM gcc:latest

LABEL org.opencontainers.image.source=https://github.com/RasyaAndrean/Urus
LABEL org.opencontainers.image.description="URUS Programming Language Compiler"
LABEL org.opencontainers.image.licenses=Apache-2.0

WORKDIR /urus

COPY compiler/ ./compiler/
COPY examples/ ./examples/

RUN cd compiler && \
    gcc -Wall -Wextra -std=c11 -D_POSIX_C_SOURCE=200809L -O2 -Iinclude -o /usr/local/bin/urusc \
    src/main.c src/lexer.c src/ast.c src/parser.c src/util.c src/sema.c src/codegen.c -lm

WORKDIR /workspace

ENTRYPOINT ["urusc"]
CMD ["--help"]
