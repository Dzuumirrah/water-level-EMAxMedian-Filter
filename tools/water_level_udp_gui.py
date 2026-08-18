import json
import math
import socket
import threading
import time
import tkinter as tk
from tkinter import font


UDP_LISTEN_IP = "0.0.0.0"
UDP_LISTEN_PORT = 4210
STALE_AFTER_SECONDS = 3.0


class WaterLevelUdpGui:
    def __init__(self, root):
        self.root = root
        self.root.title("Water Level UDP Monitor")
        self.root.geometry("560x720")
        self.root.minsize(480, 640)
        self.root.configure(bg="#111827")

        self.running = True
        self.current_percent = 0.0
        self.target_percent = 0.0
        self.distance_cm = 0.0
        self.motor_on = False
        self.source_ip = "-"
        self.last_packet_time = 0.0
        self.wave_phase = 0.0

        self._build_ui()
        self._start_udp_thread()
        self._animate()

    def _build_ui(self):
        title_font = font.Font(family="Segoe UI", size=20, weight="bold")
        value_font = font.Font(family="Consolas", size=42, weight="bold")
        label_font = font.Font(family="Segoe UI", size=10)

        header = tk.Frame(self.root, bg="#111827")
        header.pack(fill="x", padx=24, pady=(22, 8))

        tk.Label(
            header,
            text="Water Level Monitor",
            font=title_font,
            bg="#111827",
            fg="#f9fafb",
        ).pack(anchor="w")

        self.status_label = tk.Label(
            header,
            text=f"Listening on UDP {UDP_LISTEN_PORT}...",
            font=label_font,
            bg="#111827",
            fg="#fbbf24",
        )
        self.status_label.pack(anchor="w", pady=(4, 0))

        self.canvas = tk.Canvas(self.root, bg="#0f172a", highlightthickness=0)
        self.canvas.pack(fill="both", expand=True, padx=24, pady=16)
        self.canvas.bind("<Configure>", lambda _event: self._redraw_static_tank())

        self.value_label = tk.Label(
            self.root,
            text="0%",
            font=value_font,
            bg="#111827",
            fg="#38bdf8",
        )
        self.value_label.pack(pady=(0, 4))

        self.detail_label = tk.Label(
            self.root,
            text="Distance: - cm    Motor: -    Source: -",
            font=label_font,
            bg="#111827",
            fg="#9ca3af",
        )
        self.detail_label.pack(pady=(0, 20))

        self.water_item = None
        self.foam_item = None
        self._redraw_static_tank()

    def _redraw_static_tank(self):
        self.canvas.delete("all")
        width = max(self.canvas.winfo_width(), 1)
        height = max(self.canvas.winfo_height(), 1)
        pad = 28

        for i in range(1, 10):
            y = pad + i * ((height - pad * 2) / 10)
            line_color = "#1f2937" if i % 5 else "#374151"
            self.canvas.create_line(pad, y, width - pad, y, fill=line_color, dash=(5, 3))
            if i % 5 == 0:
                self.canvas.create_text(
                    width - pad - 16,
                    y - 10,
                    text=f"{100 - i * 10}%",
                    fill="#6b7280",
                    font=("Segoe UI", 9),
                    anchor="e",
                )

        self.canvas.create_rectangle(
            pad,
            pad,
            width - pad,
            height - pad,
            outline="#e5e7eb",
            width=3,
        )
        self.water_item = self.canvas.create_polygon(0, height, width, height, fill="#0ea5e9")
        self.foam_item = self.canvas.create_line(0, height, width, height, fill="#bae6fd", width=3)

    def _start_udp_thread(self):
        thread = threading.Thread(target=self._udp_loop, daemon=True)
        thread.start()

    def _udp_loop(self):
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        sock.bind((UDP_LISTEN_IP, UDP_LISTEN_PORT))
        sock.settimeout(0.5)

        while self.running:
            try:
                data, addr = sock.recvfrom(1024)
            except socket.timeout:
                continue
            except OSError:
                break

            try:
                payload = json.loads(data.decode("utf-8"))
            except (UnicodeDecodeError, json.JSONDecodeError):
                continue

            self.root.after(0, self._apply_packet, payload, addr[0])

        sock.close()

    def _apply_packet(self, payload, sender_ip):
        water_level = float(payload.get("waterLevel", 0))
        self.target_percent = max(0.0, min(water_level, 100.0))
        self.distance_cm = float(payload.get("distanceCm", 0))
        self.motor_on = bool(payload.get("motor", False))
        self.source_ip = payload.get("ip") or sender_ip
        self.last_packet_time = time.time()

    def _animate(self):
        if not self.running:
            return

        connected = time.time() - self.last_packet_time <= STALE_AFTER_SECONDS
        self.current_percent += (self.target_percent - self.current_percent) * 0.12
        self.wave_phase += 0.18

        width = max(self.canvas.winfo_width(), 1)
        height = max(self.canvas.winfo_height(), 1)
        pad = 28
        inner_height = max(height - pad * 2, 1)
        surface_y = height - pad - (self.current_percent / 100.0) * inner_height

        wave_points = []
        for x in range(pad, width - pad + 1, 6):
            y = surface_y + math.sin((x * 0.04) + self.wave_phase) * 6
            wave_points.extend([x, y])

        polygon = [pad, height - pad] + wave_points + [width - pad, height - pad]
        self.canvas.coords(self.water_item, *polygon)
        self.canvas.coords(self.foam_item, *wave_points)

        color = "#0ea5e9"
        if self.current_percent < 20:
            color = "#ef4444"
        elif self.current_percent > 85:
            color = "#f59e0b"

        self.canvas.itemconfig(self.water_item, fill=color if connected else "#4b5563")
        self.value_label.config(text=f"{self.current_percent:.0f}%")

        if connected:
            self.status_label.config(text="Receiving UDP telemetry", fg="#34d399")
            motor_text = "ON" if self.motor_on else "OFF"
            self.detail_label.config(
                text=f"Distance: {self.distance_cm:.2f} cm    Motor: {motor_text}    Source: {self.source_ip}"
            )
        else:
            self.status_label.config(text=f"Listening on UDP {UDP_LISTEN_PORT}...", fg="#fbbf24")
            self.detail_label.config(text="Distance: - cm    Motor: -    Source: -")

        self.root.after(33, self._animate)

    def close(self):
        self.running = False
        self.root.destroy()


if __name__ == "__main__":
    app = tk.Tk()
    gui = WaterLevelUdpGui(app)
    app.protocol("WM_DELETE_WINDOW", gui.close)
    app.mainloop()
