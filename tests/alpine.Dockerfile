FROM alpine:latest

COPY baldr /baldr

ENTRYPOINT ["/baldr"]
