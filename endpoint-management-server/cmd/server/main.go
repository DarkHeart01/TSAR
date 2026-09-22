package main

import (
	"context"
	"log"
	"net/http"
	"os"
	"os/signal"
	"syscall"
	"time"

	"github.com/joho/godotenv"
	"github.com/redis/go-redis/v9"

	"endpoint-management-server/internal/db"
	"endpoint-management-server/internal/handlers"
	"endpoint-management-server/internal/queue"
	"endpoint-management-server/internal/repository"
	"endpoint-management-server/internal/router"
)

func main() {
	_ = godotenv.Load()

	pgDSN := requireEnv("POSTGRES_DSN")
	redisAddr := requireEnv("REDIS_ADDR")
	redisPassword := os.Getenv("REDIS_PASSWORD")
	port := getEnvDefault("PORT", "8080")

	conn, err := db.Connect(pgDSN)
	if err != nil {
		log.Fatalf("postgres connection failed: %v", err)
	}
	defer conn.Close()

	redisClient := redis.NewClient(&redis.Options{
		Addr:         redisAddr,
		Password:     redisPassword,
		DB:           0,
		PoolSize:     100,
		MinIdleConns: 10,
	})
	defer redisClient.Close()

	if err := redisClient.Ping(context.Background()).Err(); err != nil {
		log.Fatalf("redis connection failed: %v", err)
	}

	agentRepo := repository.NewAgentRepository(conn)
	taskRepo := repository.NewTaskRepository(conn)
	telemetryRepo := repository.NewTelemetryRepository(conn)
	taskQueue := queue.NewTaskQueue(redisClient)

	agentHandler := handlers.NewAgentHandler(agentRepo, taskRepo, telemetryRepo, taskQueue)

	go staleAgentSweeper(agentRepo)

	r := router.New(agentHandler, agentRepo, redisClient)

	srv := &http.Server{
		Addr:              ":" + port,
		Handler:           r,
		ReadHeaderTimeout: 5 * time.Second,
		ReadTimeout:       15 * time.Second,
		WriteTimeout:      15 * time.Second,
		IdleTimeout:       60 * time.Second,
	}

	go func() {
		log.Printf("endpoint management server listening on :%s", port)
		if err := srv.ListenAndServe(); err != nil && err != http.ErrServerClosed {
			log.Fatalf("server error: %v", err)
		}
	}()

	quit := make(chan os.Signal, 1)
	signal.Notify(quit, syscall.SIGINT, syscall.SIGTERM)
	<-quit

	log.Println("shutting down gracefully...")
	ctx, cancel := context.WithTimeout(context.Background(), 15*time.Second)
	defer cancel()
	if err := srv.Shutdown(ctx); err != nil {
		log.Fatalf("forced shutdown: %v", err)
	}
}

func staleAgentSweeper(agentRepo *repository.AgentRepository) {
	ticker := time.NewTicker(30 * time.Second)
	defer ticker.Stop()
	for range ticker.C {
		ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
		if n, err := agentRepo.MarkStaleOffline(ctx, 120); err != nil {
			log.Printf("stale sweeper error: %v", err)
		} else if n > 0 {
			log.Printf("marked %d agents offline", n)
		}
		cancel()
	}
}

func requireEnv(key string) string {
	v := os.Getenv(key)
	if v == "" {
		log.Fatalf("missing required env var: %s", key)
	}
	return v
}

func getEnvDefault(key, fallback string) string {
	if v := os.Getenv(key); v != "" {
		return v
	}
	return fallback
}
