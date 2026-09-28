package tests

import (
	"context"
	"encoding/json"
	"testing"
	"time"

	"github.com/google/uuid"

	"endpoint-management-server/internal/queue"
)

// ─── QueuedTask JSON serialisation ──────────────────────────────────────────

func TestQueuedTask_JSONRoundTrip(t *testing.T) {
	id := uuid.New()
	orig := queue.QueuedTask{
		TaskID:      id,
		CommandType: "run_shell",
		Payload:     json.RawMessage(`{"cmd":"whoami"}`),
	}

	data, err := json.Marshal(orig)
	if err != nil {
		t.Fatalf("marshal: %v", err)
	}

	var decoded queue.QueuedTask
	if err := json.Unmarshal(data, &decoded); err != nil {
		t.Fatalf("unmarshal: %v", err)
	}

	if decoded.TaskID != id {
		t.Errorf("TaskID: want %v, got %v", id, decoded.TaskID)
	}
	if decoded.CommandType != "run_shell" {
		t.Errorf("CommandType: want run_shell, got %q", decoded.CommandType)
	}
	if string(decoded.Payload) != `{"cmd":"whoami"}` {
		t.Errorf("Payload: want {\"cmd\":\"whoami\"}, got %q", string(decoded.Payload))
	}
}

// ─── NewTaskQueue ─────────────────────────────────────────────────────────────

func TestNewTaskQueue_UseLMPOPFlagStored(t *testing.T) {
	// We can't easily introspect the private flag, but we can verify construction
	// doesn't panic for both modes — a nil client is fine here since we don't
	// call any Redis methods.
	q := queue.NewTaskQueue(nil, true)
	if q == nil {
		t.Error("NewTaskQueue returned nil (useLMPOP=true)")
	}

	q2 := queue.NewTaskQueue(nil, false)
	if q2 == nil {
		t.Error("NewTaskQueue returned nil (useLMPOP=false)")
	}
}

// ─── CheckRedisVersion ───────────────────────────────────────────────────────

func TestCheckRedisVersion_SkipIfNoRedis(t *testing.T) {
	// This test requires a running Redis; skip gracefully when not available.
	ctx, cancel := context.WithTimeout(context.Background(), 2*time.Second)
	defer cancel()

	// Use the public test helper to try to connect to a local Redis.
	// We use a minimal client with a very short dial timeout.
	ok, err := tryCheckRedisVersion(ctx)
	if err != nil {
		t.Skipf("no local Redis available, skipping: %v", err)
	}
	// Redis 7 is in go-redis docker image but might be 6 locally. Either is fine.
	t.Logf("local Redis >= 7: %v", ok)
}

// tryCheckRedisVersion tries to connect to localhost:6379 and run
// CheckRedisVersion.  Returns an error if Redis is not reachable.
func tryCheckRedisVersion(ctx context.Context) (bool, error) {
	// Import redis inline via the package we already depend on.
	// We need a real *redis.Client — but only for an optional test.
	// Use the already-imported go-redis library.
	return checkRedisVersionLocal(ctx, "localhost:6379", "")
}

// ─── QueuedTask field validation ─────────────────────────────────────────────

func TestQueuedTask_EmptyPayloadIsValid(t *testing.T) {
	qt := queue.QueuedTask{
		TaskID:      uuid.New(),
		CommandType: "ping",
		Payload:     json.RawMessage(`{}`),
	}
	data, err := json.Marshal(qt)
	if err != nil {
		t.Fatalf("marshal: %v", err)
	}
	var out queue.QueuedTask
	if err := json.Unmarshal(data, &out); err != nil {
		t.Fatalf("unmarshal: %v", err)
	}
	if string(out.Payload) != `{}` {
		t.Errorf("empty payload roundtrip failed: %q", string(out.Payload))
	}
}

func TestQueuedTask_NullPayloadPreserved(t *testing.T) {
	qt := queue.QueuedTask{
		TaskID:      uuid.New(),
		CommandType: "noop",
		Payload:     json.RawMessage(`null`),
	}
	data, err := json.Marshal(qt)
	if err != nil {
		t.Fatal(err)
	}
	var out queue.QueuedTask
	if err := json.Unmarshal(data, &out); err != nil {
		t.Fatal(err)
	}
	if string(out.Payload) != "null" {
		t.Errorf("null payload roundtrip: got %q", string(out.Payload))
	}
}
