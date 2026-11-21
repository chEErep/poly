FROM debian:bookworm-slim
RUN apt-get update -qq && apt-get install -y --no-install-recommends clang lld nasm python3 && rm -rf /var/lib/apt/lists/*
WORKDIR /src
