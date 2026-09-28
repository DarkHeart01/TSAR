package tests

import (
	"context"

	"github.com/redis/go-redis/v9"

	"endpoint-management-server/internal/queue"
)

// checkRedisVersionLocal tries to connect to addr with optional password
// and calls CheckRedisVersion.  Returns an error if the ping fails.
func checkRedisVersionLocal(ctx context.Context, addr, password string) (bool, error) {
	client := redis.NewClient(&redis.Options{
		Addr:     addr,
		Password: password,
	})
	defer client.Close()

	if err := client.Ping(ctx).Err(); err != nil {
		return false, err
	}
	return queue.CheckRedisVersion(ctx, client)
}
