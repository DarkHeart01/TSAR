FROM golang:1.21-alpine AS builder

WORKDIR /app

COPY go.mod go.sum ./
RUN go mod download

COPY . .
RUN go build -o jocky-backend .

FROM alpine:latest
RUN apk add --no-cache ca-certificates wget

WORKDIR /app
COPY --from=builder /app/jocky-backend .

EXPOSE 8080

CMD ["./jocky-backend"]