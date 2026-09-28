package handlers

import (
	"encoding/json"
	"net/http"

	"github.com/gin-gonic/gin"
	"github.com/redis/go-redis/v9"

	"endpoint-management-server/internal/repository"
)

// DashboardHandler serves the operator HTML dashboard and its JSON data API.
// NOTE: Neither endpoint requires authentication — convenient for the
// SIH demo.  Add ValidateOperatorJWT middleware in production.
type DashboardHandler struct {
	dashRepo  *repository.DashboardRepository
	auditRepo *repository.AuditRepository
	redis     *redis.Client
}

func NewDashboardHandler(
	dashRepo *repository.DashboardRepository,
	auditRepo *repository.AuditRepository,
	redisClient *redis.Client,
) *DashboardHandler {
	return &DashboardHandler{dashRepo: dashRepo, auditRepo: auditRepo, redis: redisClient}
}

// HTML serves GET /dashboard — the static shell page that JS populates.
func (h *DashboardHandler) HTML(c *gin.Context) {
	c.Data(http.StatusOK, "text/html; charset=utf-8", []byte(dashboardHTML))
}

// Data serves GET /api/v1/dashboard/data — JSON consumed by the dashboard JS.
func (h *DashboardHandler) Data(c *gin.Context) {
	ctx := c.Request.Context()

	agents, _ := h.dashRepo.ListAgentsWithStats(ctx)
	tasks, _ := h.dashRepo.ListTasks(ctx, "", 200)
	telemetry, _ := h.dashRepo.ListRecentTelemetry(ctx, 50)
	auditLogs, _ := h.auditRepo.ListRecent(ctx, 50)

	// Payload manifest from Redis (best-effort; nil on miss).
	var manifest interface{}
	raw, err := h.redis.Get(ctx, "payload:manifest").Result()
	if err == nil {
		var m map[string]interface{}
		if json.Unmarshal([]byte(raw), &m) == nil {
			manifest = m
		}
	}

	c.JSON(http.StatusOK, gin.H{
		"agents":     agents,
		"tasks":      tasks,
		"payload":    manifest,
		"telemetry":  telemetry,
		"audit_logs": auditLogs,
	})
}

const dashboardHTML = `<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>JOCKY C2</title>
<style>
:root{--bg:#0d1117;--surface:#161b22;--border:#30363d;--text:#e6edf3;
--muted:#8b949e;--green:#3fb950;--red:#f85149;--yellow:#d29922;--blue:#58a6ff;}
*{box-sizing:border-box;margin:0;padding:0}
body{background:var(--bg);color:var(--text);font-family:'Segoe UI',monospace;font-size:13px}
header{background:var(--surface);border-bottom:1px solid var(--border);
padding:12px 24px;display:flex;align-items:center;gap:16px}
header h1{font-size:16px;letter-spacing:.05em;color:var(--blue)}
header span{color:var(--muted);font-size:11px}
#ts{margin-left:auto;color:var(--muted);font-size:11px}
.panels{display:grid;grid-template-columns:1fr 1fr;gap:16px;padding:16px}
@media(max-width:900px){.panels{grid-template-columns:1fr}}
.panel{background:var(--surface);border:1px solid var(--border);border-radius:6px;overflow:hidden}
.panel-title{padding:10px 14px;font-weight:600;border-bottom:1px solid var(--border);
display:flex;align-items:center;justify-content:space-between}
.panel-body{overflow-x:auto}
table{width:100%;border-collapse:collapse}
th,td{padding:7px 12px;text-align:left;white-space:nowrap}
th{color:var(--muted);font-weight:500;font-size:11px;text-transform:uppercase;
border-bottom:1px solid var(--border)}
tr:hover td{background:rgba(255,255,255,.03)}
.badge{display:inline-block;padding:1px 8px;border-radius:10px;font-size:11px;font-weight:600}
.badge-online{background:#1a3a1a;color:var(--green)}
.badge-offline{background:#2a1a1a;color:var(--red)}
.badge-pending{background:#2a2a1a;color:var(--yellow)}
.badge-executed{background:#1a2a1a;color:var(--green)}
.badge-failed{background:#2a1a1a;color:var(--red)}
.badge-expired{background:#1e1e1e;color:var(--muted)}
.badge-sent{background:#1a1a2a;color:var(--blue)}
.card{padding:14px}
.card-row{display:flex;justify-content:space-between;padding:4px 0;
border-bottom:1px solid var(--border)}
.card-row:last-child{border-bottom:none}
.card-label{color:var(--muted)}
.filters{padding:8px 14px;border-bottom:1px solid var(--border);display:flex;gap:6px}
.filter-btn{background:var(--border);border:none;color:var(--text);padding:3px 10px;
border-radius:4px;cursor:pointer;font-size:11px}
.filter-btn.active{background:var(--blue);color:#000}
.empty{color:var(--muted);padding:16px;text-align:center}
</style>
</head>
<body>
<header>
  <h1>&#x2620; JOCKY C2</h1>
  <span>endpoint management &amp; payload delivery</span>
  <span id="ts">loading...</span>
</header>
<div class="panels">

<div class="panel" style="grid-column:1/-1">
  <div class="panel-title">Agents <span id="agent-count" style="color:var(--muted);font-weight:400"></span></div>
  <div class="panel-body"><table>
    <thead><tr><th>Agent ID</th><th>Hostname</th><th>IP</th><th>Status</th>
    <th>Last Seen</th><th>Pending</th><th>Telemetry</th></tr></thead>
    <tbody id="agents-body"></tbody>
  </table></div>
</div>

<div class="panel" style="grid-column:1/-1">
  <div class="panel-title">Task Queue
    <div class="filters" id="task-filters">
      <button class="filter-btn active" data-status="all">All</button>
      <button class="filter-btn" data-status="pending">Pending</button>
      <button class="filter-btn" data-status="sent">Sent</button>
      <button class="filter-btn" data-status="executed">Executed</button>
      <button class="filter-btn" data-status="failed">Failed</button>
      <button class="filter-btn" data-status="expired">Expired</button>
    </div>
  </div>
  <div class="panel-body"><table>
    <thead><tr><th>Task ID</th><th>Agent ID</th><th>Command</th><th>Status</th>
    <th>Created</th><th>Retries</th><th>Expires</th></tr></thead>
    <tbody id="tasks-body"></tbody>
  </table></div>
</div>

<div class="panel">
  <div class="panel-title">Payload Status</div>
  <div class="card" id="payload-card"><div class="empty">No payload loaded</div></div>
</div>

<div class="panel">
  <div class="panel-title">Recent Telemetry</div>
  <div class="panel-body"><table>
    <thead><tr><th>Agent</th><th>Type</th><th>Time</th><th>Data</th></tr></thead>
    <tbody id="tel-body"></tbody>
  </table></div>
</div>

<div class="panel" style="grid-column:1/-1">
  <div class="panel-title">Audit Log</div>
  <div class="panel-body"><table>
    <thead><tr><th>Time</th><th>Operation</th><th>Resource</th><th>IP</th><th>Details</th></tr></thead>
    <tbody id="audit-body"></tbody>
  </table></div>
</div>

</div>
<script>
let allTasks = [];
let taskFilter = 'all';

function rel(ts){
  const d = (Date.now() - new Date(ts).getTime()) / 1000;
  if(d<60) return Math.floor(d)+'s ago';
  if(d<3600) return Math.floor(d/60)+'m ago';
  if(d<86400) return Math.floor(d/3600)+'h ago';
  return Math.floor(d/86400)+'d ago';
}
function badge(status){
  return '<span class="badge badge-'+status+'">'+status+'</span>';
}
function trunc(s,n){return s&&s.length>n?s.slice(0,n)+'…':s||''}

function renderAgents(agents){
  const b=document.getElementById('agents-body');
  document.getElementById('agent-count').textContent='('+( agents?agents.length:0)+')';
  if(!agents||!agents.length){b.innerHTML='<tr><td colspan=7 class=empty>No agents registered</td></tr>';return}
  b.innerHTML=agents.map(a=>'<tr>'+
    '<td>'+a.agent_id.slice(0,8)+'…</td>'+
    '<td>'+a.hostname+'</td>'+
    '<td>'+a.ip_address+'</td>'+
    '<td>'+badge(a.status)+'</td>'+
    '<td>'+rel(a.last_seen)+'</td>'+
    '<td>'+a.pending_tasks+'</td>'+
    '<td>'+a.telemetry_count+'</td>'+
  '</tr>').join('');
}

function renderTasks(tasks){
  allTasks = tasks||[];
  applyTaskFilter();
}

function applyTaskFilter(){
  const b=document.getElementById('tasks-body');
  const rows=taskFilter==='all'?allTasks:allTasks.filter(t=>t.status===taskFilter);
  if(!rows.length){b.innerHTML='<tr><td colspan=7 class=empty>No tasks</td></tr>';return}
  b.innerHTML=rows.map(t=>'<tr>'+
    '<td>'+t.task_id.slice(0,8)+'…</td>'+
    '<td>'+t.agent_id.slice(0,8)+'…</td>'+
    '<td>'+trunc(t.command_type,60)+'</td>'+
    '<td>'+badge(t.status)+'</td>'+
    '<td>'+rel(t.created_at)+'</td>'+
    '<td>'+t.retry_count+'/'+t.max_retries+'</td>'+
    '<td>'+rel(t.expires_at)+'</td>'+
  '</tr>').join('');
}

document.getElementById('task-filters').addEventListener('click',e=>{
  const btn=e.target.closest('.filter-btn');
  if(!btn)return;
  document.querySelectorAll('.filter-btn').forEach(b=>b.classList.remove('active'));
  btn.classList.add('active');
  taskFilter=btn.dataset.status;
  applyTaskFilter();
});

function renderPayload(p){
  const card=document.getElementById('payload-card');
  if(!p){card.innerHTML='<div class=empty>No payload loaded</div>';return}
  card.innerHTML=[
    ['SHA-256',trunc(p.sha256,32)],
    ['Chunks',p.total_chunks],
    ['Total size',p.total_size+' chars'],
    ['Version',p.version],
    ['Uploaded',rel(p.uploaded_at)],
  ].map(([k,v])=>'<div class=card-row><span class=card-label>'+k+'</span><span>'+v+'</span></div>').join('');
}

function renderTelemetry(tel){
  const b=document.getElementById('tel-body');
  if(!tel||!tel.length){b.innerHTML='<tr><td colspan=4 class=empty>No telemetry</td></tr>';return}
  b.innerHTML=tel.map(t=>'<tr>'+
    '<td>'+t.agent_id.slice(0,8)+'…</td>'+
    '<td>'+t.log_type+'</td>'+
    '<td>'+rel(t.received_at)+'</td>'+
    '<td>'+trunc(JSON.stringify(t.result_data),100)+'</td>'+
  '</tr>').join('');
}

function renderAudit(logs){
  const b=document.getElementById('audit-body');
  if(!logs||!logs.length){b.innerHTML='<tr><td colspan=5 class=empty>No audit entries</td></tr>';return}
  b.innerHTML=logs.map(l=>'<tr>'+
    '<td>'+rel(l.created_at)+'</td>'+
    '<td>'+l.action+'</td>'+
    '<td>'+(l.resource_type||'')+(l.resource_id?' / '+l.resource_id.slice(0,8)+'…':'')+'</td>'+
    '<td>'+l.actor_ip+'</td>'+
    '<td>'+trunc(JSON.stringify(l.details),80)+'</td>'+
  '</tr>').join('');
}

async function refresh(){
  try{
    const r=await fetch('/api/v1/dashboard/data');
    const d=await r.json();
    renderAgents(d.agents);
    renderTasks(d.tasks);
    renderPayload(d.payload);
    renderTelemetry(d.telemetry);
    renderAudit(d.audit_logs);
    document.getElementById('ts').textContent='updated '+new Date().toLocaleTimeString();
  }catch(e){document.getElementById('ts').textContent='refresh failed: '+e.message}
}

refresh();
setInterval(refresh,10000);
</script>
</body>
</html>`
