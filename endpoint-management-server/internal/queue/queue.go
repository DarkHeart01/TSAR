package queue

import (
	"context"
	"encoding/json"
	"fmt"
	"time"

	"github.com/google/uuid"
	"github.com/redis/go-redis/v9"
)

// TaskQueue manages per-agent task queues in Redis using Lists,
// giving FIFO delivery with O(1) push/pop under concurrent load.
type TaskQueue struct {
	client *redis.Client
}

func NewTaskQueue(client *redis.Client) *TaskQueue {
	return &TaskQueue{client: client}
}

func agentQueueKey(agentID uuid.UUID) string {
	return fmt.Sprintf("agent:queue:%s", agentID.String())
}

func agentLockKey(agentID uuid.UUID) string {
	return fmt.Sprintf("agent:lock:%s", agentID.String())
}

// QueuedTask is the wire format stored in Redis.
type QueuedTask struct {
	TaskID      uuid.UUID       `json:"task_id"`
	CommandType string          `json:"command_type"`
	Payload     json.RawMessage `json:"payload"`
}

// Push enqueues a task for delivery to a specific agent.
func (q *TaskQueue) Push(ctx context.Context, agentID uuid.UUID, task QueuedTask) error {
	data, err := json.Marshal(task)
	if err != nil {
		return err
	}
	key := agentQueueKey(agentID)
	pipe := q.client.TxPipeline()
	pipe.RPush(ctx, key, data)
	pipe.Expire(ctx, key, 48*time.Hour)
	_, err = pipe.Exec(ctx)
	return err
}

// PopAll atomically drains all pending tasks for an agent poll cycle.
func (q *TaskQueue) PopAll(ctx context.Context, agentID uuid.UUID, maxTasks int64) ([]QueuedTask, error) {
	key := agentQueueKey(agentID)

	var results []QueuedTask
	for i := int64(0); i < maxTasks; i++ {
		val, err := q.client.LPop(ctx, key).Result()
		if err == redis.Nil {
			break
		}
		if err != nil {
			return nil, err
		}
		var t QueuedTask
		if err := json.Unmarshal([]byte(val), &t); err != nil {
			continue
		}
		results = append(results, t)
	}
	return results, nil
}

// QueueLength returns the number of pending tasks for an agent.
func (q *TaskQueue) QueueLength(ctx context.Context, agentID uuid.UUID) (int64, error) {
	return q.client.LLen(ctx, agentQueueKey(agentID)).Result()
}

// AcquirePollLock prevents duplicate concurrent polls from the same agent
// (e.g. retried requests) from double-draining the queue.
func (q *TaskQueue) AcquirePollLock(ctx context.Context, agentID uuid.UUID, ttl time.Duration) (bool, error) {
	return q.client.SetNX(ctx, agentLockKey(agentID), "1", ttl).Result()
}

// SetPresence marks an agent as currently connected, used for fast
// online/offline lookups without hitting Postgres.
func (q *TaskQueue) SetPresence(ctx context.Context, agentID uuid.UUID, ttl time.Duration) error {
	key := fmt.Sprintf("agent:presence:%s", agentID.String())
	return q.client.Set(ctx, key, time.Now().UTC().Format(time.RFC3339), ttl).Err()
}

func (q *TaskQueue) IsOnline(ctx context.Context, agentID uuid.UUID) (bool, error) {
	key := fmt.Sprintf("agent:presence:%s", agentID.String())
	exists, err := q.client.Exists(ctx, key).Result()
	if err != nil {
		return false, err
	}
	return exists == 1, nil
}
