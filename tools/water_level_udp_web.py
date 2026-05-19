import json
import socket
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import urlparse


UDP_LISTEN_IP = "0.0.0.0"
UDP_LISTEN_PORT = 4210
WEB_HOST = "0.0.0.0"
WEB_PORT = 8000
STALE_AFTER_SECONDS = 3.0


state_lock = threading.Lock()
latest_state = {
    "waterLevel": 0,
    "distanceCm": 0,
    "motor": False,
    "ip": "-",
    "updatedAt": 0,
}


HTML_PAGE = """<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Water Level UDP Monitor</title>
  <style>
    :root {
      color-scheme: dark;
      font-family: "Segoe UI", system-ui, sans-serif;
      background: #111827;
      color: #f9fafb;
    }
    * { box-sizing: border-box; }
    body {
      margin: 0;
      min-height: 100vh;
      display: grid;
      place-items: center;
      background: #111827;
    }
    main {
      width: min(920px, 100%);
      min-height: 100vh;
      display: grid;
      grid-template-columns: minmax(280px, 360px) 1fr;
      gap: 28px;
      align-items: center;
      padding: 28px;
    }
    .panel {
      min-width: 0;
    }
    h1 {
      margin: 0 0 8px;
      font-size: clamp(28px, 5vw, 44px);
      line-height: 1.05;
      letter-spacing: 0;
    }
    .status {
      margin-bottom: 28px;
      color: #fbbf24;
      font-size: 15px;
    }
    .status.live { color: #34d399; }
    .value {
      font-family: Consolas, "SFMono-Regular", monospace;
      font-size: clamp(64px, 14vw, 128px);
      line-height: 0.95;
      color: #38bdf8;
      font-weight: 800;
    }
    .meta {
      display: grid;
      gap: 10px;
      margin-top: 26px;
      color: #d1d5db;
      font-size: 16px;
    }
    .meta div {
      display: flex;
      justify-content: space-between;
      gap: 18px;
      border-bottom: 1px solid #1f2937;
      padding-bottom: 10px;
    }
    .meta span:first-child { color: #9ca3af; }
    .tank {
      position: relative;
      height: min(76vh, 620px);
      min-height: 420px;
      border: 4px solid #e5e7eb;
      background:
        repeating-linear-gradient(
          to bottom,
          transparent 0,
          transparent calc(10% - 1px),
          #1f2937 calc(10% - 1px),
          #1f2937 10%
        ),
        #0f172a;
      overflow: hidden;
    }
    .water {
      position: absolute;
      inset: auto 0 0;
      height: 0%;
      background: #0ea5e9;
      transition: height 240ms ease, background-color 240ms ease;
    }
    .water::before {
      content: "";
      position: absolute;
      left: -5%;
      right: -5%;
      top: -12px;
      height: 24px;
      background:
        radial-gradient(24px 14px at 24px 14px, #bae6fd 48%, transparent 52%) 0 0 / 48px 24px repeat-x;
      animation: wave 1.4s linear infinite;
    }
    .offline .water {
      background: #4b5563;
    }
    .labels {
      position: absolute;
      inset: 18px;
      pointer-events: none;
      display: flex;
      flex-direction: column;
      justify-content: space-between;
      color: #6b7280;
      font-size: 13px;
      text-align: right;
    }
    @keyframes wave {
      from { transform: translateX(0); }
      to { transform: translateX(48px); }
    }
    @media (max-width: 760px) {
      main {
        grid-template-columns: 1fr;
        align-items: start;
      }
      .tank {
        height: 54vh;
        min-height: 320px;
      }
    }
  </style>
</head>
<body class="offline">
  <main>
    <section class="panel">
      <h1>Water Level Monitor</h1>
      <div id="status" class="status">Waiting for UDP telemetry...</div>
      <div id="value" class="value">0%</div>
      <div class="meta">
        <div><span>Distance</span><strong id="distance">- cm</strong></div>
        <div><span>Motor</span><strong id="motor">-</strong></div>
        <div><span>Source</span><strong id="source">-</strong></div>
      </div>
    </section>
    <section class="tank" aria-label="Water tank">
      <div id="water" class="water"></div>
      <div class="labels">
        <span>100%</span><span>75%</span><span>50%</span><span>25%</span><span>0%</span>
      </div>
    </section>
  </main>
  <script>
    const statusEl = document.getElementById("status");
    const valueEl = document.getElementById("value");
    const distanceEl = document.getElementById("distance");
    const motorEl = document.getElementById("motor");
    const sourceEl = document.getElementById("source");
    const waterEl = document.getElementById("water");

    function applyState(data) {
      const level = Math.max(0, Math.min(Number(data.waterLevel || 0), 100));
      const live = Boolean(data.live);
      document.body.classList.toggle("offline", !live);
      statusEl.classList.toggle("live", live);
      statusEl.textContent = live ? "Receiving UDP telemetry" : "Waiting for UDP telemetry...";
      valueEl.textContent = `${Math.round(level)}%`;
      distanceEl.textContent = live ? `${Number(data.distanceCm || 0).toFixed(2)} cm` : "- cm";
      motorEl.textContent = live ? (data.motor ? "ON" : "OFF") : "-";
      sourceEl.textContent = live ? (data.ip || "-") : "-";
      waterEl.style.height = `${level}%`;
      waterEl.style.backgroundColor = level < 20 ? "#ef4444" : level > 85 ? "#f59e0b" : "#0ea5e9";
    }

    async function refresh() {
      try {
        const response = await fetch("/api/state", { cache: "no-store" });
        applyState(await response.json());
      } catch {
        document.body.classList.add("offline");
        statusEl.classList.remove("live");
        statusEl.textContent = "Web server connection lost";
      }
    }

    refresh();
    setInterval(refresh, 500);
  </script>
</body>
</html>
"""


def get_public_state():
    with state_lock:
        state = dict(latest_state)

    state["live"] = time.time() - state["updatedAt"] <= STALE_AFTER_SECONDS
    return state


def udp_loop():
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    sock.bind((UDP_LISTEN_IP, UDP_LISTEN_PORT))
    print(f"Listening for ESP32 UDP telemetry on {UDP_LISTEN_IP}:{UDP_LISTEN_PORT}")

    while True:
        data, addr = sock.recvfrom(1024)
        try:
            payload = json.loads(data.decode("utf-8"))
        except (UnicodeDecodeError, json.JSONDecodeError):
            continue

        with state_lock:
            latest_state["waterLevel"] = float(payload.get("waterLevel", 0))
            latest_state["distanceCm"] = float(payload.get("distanceCm", 0))
            latest_state["motor"] = bool(payload.get("motor", False))
            latest_state["ip"] = payload.get("ip") or addr[0]
            latest_state["updatedAt"] = time.time()


class WaterLevelHandler(BaseHTTPRequestHandler):
    def do_GET(self):
        path = urlparse(self.path).path
        if path == "/":
            self._send_bytes(HTML_PAGE.encode("utf-8"), "text/html; charset=utf-8")
            return

        if path == "/api/state":
            payload = json.dumps(get_public_state()).encode("utf-8")
            self._send_bytes(payload, "application/json; charset=utf-8")
            return

        self.send_error(404, "Not found")

    def log_message(self, format, *args):
        return

    def _send_bytes(self, payload, content_type):
        self.send_response(200)
        self.send_header("Content-Type", content_type)
        self.send_header("Content-Length", str(len(payload)))
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        self.wfile.write(payload)


def main():
    thread = threading.Thread(target=udp_loop, daemon=True)
    thread.start()

    server = ThreadingHTTPServer((WEB_HOST, WEB_PORT), WaterLevelHandler)
    print(f"Open the web dashboard at http://localhost:{WEB_PORT}")
    print(f"Other devices on the same Wi-Fi can use http://{get_lan_ip()}:{WEB_PORT}")
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\nStopping web dashboard.")
    finally:
        server.server_close()


def get_lan_ip():
    probe = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        probe.connect(("8.8.8.8", 80))
        return probe.getsockname()[0]
    except OSError:
        return "YOUR_COMPUTER_IP"
    finally:
        probe.close()


if __name__ == "__main__":
    main()
