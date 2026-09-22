package db

import (
	"database/sql"
	"fmt"
	"time"

	_ "github.com/lib/pq"
)

// Connect opens a PostgreSQL connection pool with sane defaults for
// a high-concurrency service.
func Connect(dsn string) (*sql.DB, error) {
	conn, err := sql.Open("postgres", dsn)
	if err != nil {
		return nil, fmt.Errorf("open postgres: %w", err)
	}

	conn.SetMaxOpenConns(50)
	conn.SetMaxIdleConns(10)
	conn.SetConnMaxLifetime(30 * time.Minute)
	conn.SetConnMaxIdleTime(5 * time.Minute)

	if err := conn.Ping(); err != nil {
		return nil, fmt.Errorf("ping postgres: %w", err)
	}

	return conn, nil
}
