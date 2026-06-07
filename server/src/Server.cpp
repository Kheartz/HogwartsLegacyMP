#include "Server.h"
#include "Protocol.h"
#include <httplib.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cinttypes>

// ---------------------------------------------------------------------------
// Embedded dashboard — served at http://localhost:8080/
// ---------------------------------------------------------------------------
static const char* DASHBOARD_HTML = R"html(<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<title>HogwartsLegacyMP Dashboard</title>
<style>
* { box-sizing: border-box; margin: 0; padding: 0; }
body { background: #0d0d1a; color: #d0d0e8; font-family: monospace; padding: 20px; }
h1 { color: #c8a84b; font-size: 1.1em; letter-spacing: 2px; text-transform: uppercase; margin-bottom: 16px; }
.grid { display: grid; grid-template-columns: 1fr 1fr; gap: 16px; }
table { width: 100%; border-collapse: collapse; background: #12121f; border: 1px solid #2a2a44; }
th { background: #1a1a30; color: #c8a84b; padding: 8px 10px; font-size: 0.8em; text-align: left; border-bottom: 1px solid #2a2a44; }
td { padding: 7px 10px; border-top: 1px solid #1a1a2e; font-size: 0.82em; font-variant-numeric: tabular-nums; }
tr.stale td { color: #44445a; }
canvas { width: 100%; height: 300px; background: #080812; border: 1px solid #2a2a44; display: block; }
#status { margin-top: 10px; color: #44445a; font-size: 0.72em; }
</style>
</head>
<body>
<h1>HogwartsLegacyMP &mdash; Server Dashboard</h1>
<div class="grid">
  <div>
    <table>
      <thead><tr><th>Player</th><th>X</th><th>Y</th><th>Z</th><th>Yaw</th><th>Age</th></tr></thead>
      <tbody id="tbody"><tr><td colspan="6" style="color:#44445a;text-align:center;padding:16px">No players connected</td></tr></tbody>
    </table>
  </div>
  <div><canvas id="map"></canvas></div>
</div>
<div id="warp-panel" style="margin-top:14px;background:#12121f;border:1px solid #2a2a44;padding:10px 14px;display:flex;align-items:center;gap:10px;flex-wrap:wrap;">
  <span style="color:#c8a84b;font-size:0.8em;text-transform:uppercase;letter-spacing:1px;">Warp</span>
  <select id="warp-src" style="background:#1a1a30;color:#d0d0e8;border:1px solid #2a2a44;padding:4px 6px;font-family:monospace;font-size:0.82em;"></select>
  <span style="color:#555;">&#8594;</span>
  <select id="warp-tgt" style="background:#1a1a30;color:#d0d0e8;border:1px solid #2a2a44;padding:4px 6px;font-family:monospace;font-size:0.82em;"></select>
  <button onclick="doWarp()" style="background:#c8a84b;color:#0d0d1a;border:none;padding:4px 14px;font-family:monospace;font-size:0.82em;cursor:pointer;">Warp</button>
  <span id="warp-msg" style="color:#555;font-size:0.75em;"></span>
</div>
<div id="status">Connecting...</div>
<script>
const canvas = document.getElementById('map');
const ctx = canvas.getContext('2d');
let players = [];

function resize() { canvas.width = canvas.clientWidth; canvas.height = canvas.clientHeight; }
window.addEventListener('resize', () => { resize(); draw(); });
resize();

function toCanvas(x, y, x0, y0, xr, yr) {
  const pad = 28;
  return [
    pad + (x - x0) / xr * (canvas.width  - pad * 2),
    pad + (1 - (y - y0) / yr) * (canvas.height - pad * 2)
  ];
}

function draw() {
  const W = canvas.width, H = canvas.height;
  ctx.fillStyle = '#080812';
  ctx.fillRect(0, 0, W, H);

  if (!players.length) {
    ctx.fillStyle = '#2a2a44';
    ctx.font = '11px monospace';
    ctx.textAlign = 'center';
    ctx.fillText('No players connected', W / 2, H / 2);
    return;
  }

  const pad = 28, margin = 2000;
  const xs = Math.min(...players.map(p => p.x)) - margin;
  const xe = Math.max(...players.map(p => p.x)) + margin;
  const ys = Math.min(...players.map(p => p.y)) - margin;
  const ye = Math.max(...players.map(p => p.y)) + margin;
  const xr = (xe - xs) || 1;
  const yr = (ye - ys) || 1;

  // Grid lines
  ctx.strokeStyle = '#10102a';
  ctx.lineWidth = 1;
  for (let i = 0; i <= 8; i++) {
    const gx = pad + i * (W - pad * 2) / 8;
    const gy = pad + i * (H - pad * 2) / 8;
    ctx.beginPath(); ctx.moveTo(gx, pad); ctx.lineTo(gx, H - pad); ctx.stroke();
    ctx.beginPath(); ctx.moveTo(pad, gy); ctx.lineTo(W - pad, gy); ctx.stroke();
  }

  players.forEach(p => {
    const stale = p.age_ms > 2000;
    const [cx, cy] = toCanvas(p.x, p.y, xs, ys, xr, yr);
    const rad = p.yaw * Math.PI / 180;

    ctx.beginPath();
    ctx.moveTo(cx, cy);
    ctx.lineTo(cx + Math.sin(rad) * 16, cy - Math.cos(rad) * 16);
    ctx.strokeStyle = stale ? '#333355' : '#e07030';
    ctx.lineWidth = 2;
    ctx.stroke();

    ctx.beginPath();
    ctx.arc(cx, cy, 5, 0, Math.PI * 2);
    ctx.fillStyle = stale ? '#2a2a44' : '#c8a84b';
    ctx.fill();
    ctx.strokeStyle = stale ? '#44445a' : '#ffffff';
    ctx.lineWidth = 1;
    ctx.stroke();

    ctx.fillStyle = stale ? '#44445a' : '#d0d0e8';
    ctx.font = '10px monospace';
    ctx.textAlign = 'left';
    ctx.fillText('P' + p.id, cx + 8, cy - 6);
  });
}

async function refresh() {
  try {
    const r = await fetch('/api/players');
    players = await r.json();

    const tbody = document.getElementById('tbody');
    if (!players.length) {
      tbody.innerHTML = '<tr><td colspan="6" style="color:#44445a;text-align:center;padding:16px">No players connected</td></tr>';
    } else {
      tbody.innerHTML = players.map(p => {
        const stale = p.age_ms > 2000;
        return '<tr class="' + (stale ? 'stale' : '') + '">' +
          '<td>P' + p.id + '</td>' +
          '<td>' + p.x.toFixed(0) + '</td>' +
          '<td>' + p.y.toFixed(0) + '</td>' +
          '<td>' + p.z.toFixed(0) + '</td>' +
          '<td>' + p.yaw.toFixed(1) + '&deg;</td>' +
          '<td>' + p.age_ms + 'ms</td></tr>';
      }).join('');
    }
    populateSelects();
    draw();
    document.getElementById('status').textContent =
      'Updated ' + new Date().toLocaleTimeString() + ' — ' + players.length + ' player(s)';
  } catch (e) {
    document.getElementById('status').textContent = 'Connection error: ' + e.message;
    draw();
  }
}

function populateSelects() {
  const src = document.getElementById('warp-src');
  const tgt = document.getElementById('warp-tgt');
  const sv = src.value, tv = tgt.value;
  const opts = players.map(p => '<option value="' + p.id + '">P' + p.id + '</option>').join('');
  src.innerHTML = opts;
  tgt.innerHTML = opts;
  if (sv) src.value = sv;
  if (tv) tgt.value = tv;
}

async function doWarp() {
  const src = document.getElementById('warp-src').value;
  const tgt = document.getElementById('warp-tgt').value;
  const msg = document.getElementById('warp-msg');
  if (!src || !tgt) { msg.textContent = 'No players connected.'; return; }
  if (src === tgt)  { msg.textContent = 'Source = target.';      return; }
  try {
    // src (left) is the player who moves; tgt (right) provides the destination position
    const r = await fetch('/api/warp?target_id=' + src + '&source_id=' + tgt, { method: 'POST' });
    const j = await r.json();
    msg.textContent = j.ok ? 'Warped P' + src + ' to P' + tgt : (j.error || 'Error');
    msg.style.color = j.ok ? '#4a4' : '#a44';
  } catch(e) {
    msg.textContent = 'Error: ' + e.message;
    msg.style.color = '#a44';
  }
}

refresh();
setInterval(refresh, 500);
</script>
</body>
</html>)html";

// ---------------------------------------------------------------------------

bool Server::init(uint16_t port, int max_clients)
{
    if (enet_initialize() != 0)
    {
        printf("[server] Failed to initialize ENet.\n");
        return false;
    }

    ENetAddress addr{};
    addr.host = ENET_HOST_ANY;
    addr.port = port;

    m_host = enet_host_create(&addr, max_clients, 2, 0, 0);
    if (!m_host)
    {
        printf("[server] Failed to create ENet host on port %u.\n", port);
        return false;
    }

    printf("[server] Listening on port %u (max %d clients).\n", port, max_clients);

    start_http(8080);
    return true;
}

void Server::run()
{
    ENetEvent event{};
    while (true)
    {
        while (enet_host_service(m_host, &event, 16) > 0)
        {
            switch (event.type)
            {
            case ENET_EVENT_TYPE_CONNECT:
                on_connect(event.peer);
                break;
            case ENET_EVENT_TYPE_RECEIVE:
                on_packet(event.peer, event.packet);
                enet_packet_destroy(event.packet);
                break;
            case ENET_EVENT_TYPE_DISCONNECT:
                on_disconnect(event.peer);
                break;
            default:
                break;
            }
        }

        // Drain warp commands queued by the HTTP thread
        {
            std::lock_guard<std::mutex> lk(m_warp_mutex);
            while (!m_warp_queue.empty())
            {
                auto cmd = m_warp_queue.front();
                m_warp_queue.pop();
                send_warp(cmd.target_id, cmd.x, cmd.y, cmd.z);
            }
        }
    }
}

void Server::shutdown()
{
    if (m_host)
    {
        enet_host_destroy(m_host);
        m_host = nullptr;
    }
    enet_deinitialize();
}

void Server::on_connect(ENetPeer* peer)
{
    printf("[server] Client connected: %u:%u\n", peer->address.host, peer->address.port);
    // Stash a heap-allocated player_id so we can remove it on disconnect.
    // Set to 0 (unknown) until the first PlayerMove arrives.
    peer->data = new uint32_t(0);
}

void Server::on_disconnect(ENetPeer* peer)
{
    auto* id_ptr = static_cast<uint32_t*>(peer->data);
    if (id_ptr)
    {
        if (*id_ptr != 0)
        {
            std::lock_guard lock(m_mutex);
            m_players.erase(*id_ptr);
            m_peers.erase(*id_ptr);
        }
        delete id_ptr;
        peer->data = nullptr;
    }
    printf("[server] Client disconnected: %u:%u\n", peer->address.host, peer->address.port);
}

void Server::on_packet(ENetPeer* peer, ENetPacket* packet)
{
    if (packet->dataLength < sizeof(MsgHeader))
        return;

    const auto* header = reinterpret_cast<const MsgHeader*>(packet->data);

    switch (header->opcode)
    {
    case Opcode::Heartbeat:
        // Heartbeats are silent — they just keep the connection alive.
        break;

    case Opcode::PlayerMove:
        if (packet->dataLength >= sizeof(MsgPlayerMove))
        {
            const auto* msg = reinterpret_cast<const MsgPlayerMove*>(packet->data);

            // Update state map
            {
                std::lock_guard lock(m_mutex);
                auto& p = m_players[msg->player_id];
                p.id       = msg->player_id;
                p.x        = msg->x;
                p.y        = msg->y;
                p.z        = msg->z;
                p.yaw      = msg->yaw;
                p.last_seen = std::chrono::steady_clock::now();
            }

            // Track which player_id this peer owns
            if (auto* id_ptr = static_cast<uint32_t*>(peer->data))
                *id_ptr = msg->player_id;
            m_peers[msg->player_id] = peer;

            // Broadcast to every other connected peer
            for (size_t i = 0; i < m_host->peerCount; ++i)
            {
                ENetPeer* other = &m_host->peers[i];
                if (other != peer && other->state == ENET_PEER_STATE_CONNECTED)
                {
                    ENetPacket* fwd = enet_packet_create(
                        packet->data, packet->dataLength, ENET_PACKET_FLAG_UNSEQUENCED);
                    enet_peer_send(other, 0, fwd);
                }
            }
            enet_host_flush(m_host);
        }
        break;

    default:
        printf("[server] Unknown opcode 0x%02x\n", static_cast<uint8_t>(header->opcode));
        break;
    }
}

void Server::send_warp(uint32_t target_id, float x, float y, float z)
{
    auto it = m_peers.find(target_id);
    if (it == m_peers.end()) return;

    ENetPeer* peer = it->second;
    if (peer->state != ENET_PEER_STATE_CONNECTED) return;

    MsgWarp msg{};
    msg.header.opcode = Opcode::Warp;
    msg.x = x; msg.y = y; msg.z = z;

    ENetPacket* pkt = enet_packet_create(&msg, sizeof(msg), ENET_PACKET_FLAG_RELIABLE);
    enet_peer_send(peer, 0, pkt);
    enet_host_flush(m_host);
    printf("[server] Warped P%u to %.0f,%.0f,%.0f\n", target_id, x, y, z);
}

void Server::start_http(uint16_t http_port)
{
    m_http_thread = std::thread([this, http_port]()
    {
        httplib::Server svr;

        svr.Get("/", [](const httplib::Request&, httplib::Response& res)
        {
            res.set_content(DASHBOARD_HTML, "text/html");
        });

        svr.Get("/api/players", [this](const httplib::Request&, httplib::Response& res)
        {
            auto now = std::chrono::steady_clock::now();
            std::string json = "[";
            bool first = true;

            std::lock_guard lock(m_mutex);
            for (auto& [id, p] : m_players)
            {
                if (!first) json += ",";
                first = false;

                auto age_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                    now - p.last_seen).count();

                char buf[256];
                std::snprintf(buf, sizeof(buf),
                    "{\"id\":%u,\"x\":%.1f,\"y\":%.1f,\"z\":%.1f,\"yaw\":%.1f,\"age_ms\":%" PRId64 "}",
                    p.id, p.x, p.y, p.z, p.yaw, static_cast<int64_t>(age_ms));
                json += buf;
            }
            json += "]";

            res.set_header("Access-Control-Allow-Origin", "*");
            res.set_content(json, "application/json");
        });

        // POST /api/warp?target_id=10&source_id=1
        // Warps target to source's current position.
        svr.Post("/api/warp", [this](const httplib::Request& req, httplib::Response& res)
        {
            auto target_str = req.get_param_value("target_id");
            auto source_str = req.get_param_value("source_id");

            if (target_str.empty() || source_str.empty())
            {
                res.status = 400;
                res.set_content(R"({"error":"missing target_id or source_id"})", "application/json");
                return;
            }

            auto target_id = static_cast<uint32_t>(std::atoi(target_str.c_str()));
            auto source_id = static_cast<uint32_t>(std::atoi(source_str.c_str()));

            float x, y, z;
            {
                std::lock_guard lock(m_mutex);
                auto src = m_players.find(source_id);
                auto tgt = m_players.find(target_id);
                if (src == m_players.end())
                {
                    res.status = 404;
                    res.set_content(R"({"error":"source not found"})", "application/json");
                    return;
                }
                if (tgt == m_players.end())
                {
                    res.status = 404;
                    res.set_content(R"({"error":"target not found"})", "application/json");
                    return;
                }
                x = src->second.x;
                y = src->second.y;
                z = src->second.z;
            }

            {
                std::lock_guard lk(m_warp_mutex);
                m_warp_queue.push({target_id, x, y, z});
            }

            res.set_header("Access-Control-Allow-Origin", "*");
            res.set_content(R"({"ok":true})", "application/json");
        });

        printf("[dashboard] HTTP dashboard on http://localhost:%u/\n", http_port);
        svr.listen("0.0.0.0", static_cast<int>(http_port));
    });
    m_http_thread.detach();
}
