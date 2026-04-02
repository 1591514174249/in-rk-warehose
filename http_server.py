#!/usr/bin/env python3
import http.server
import socketserver
import subprocess
import os
import json
import signal
import threading
import re

PORT = 8080
BASE_DIR = "/root/rknn_yolo11_demo"
CONFIG_FILE = os.path.join(BASE_DIR, "config.conf")

yolo_process = None
yolo_output = []

def read_stdout(proc):
    for line in proc.stdout:
        print("[YOLO]", line.strip())
        yolo_output.append(line.strip())

# --- 新增：配置文件处理函数 ---
def read_config():
    """读取配置文件并返回字典"""
    config = {}
    if not os.path.exists(CONFIG_FILE):
        return {}
    
    with open(CONFIG_FILE, 'r') as f:
        for line in f:
            line = line.strip()
            # 跳过注释和空行
            if not line or line.startswith("#"):
                continue
            if "=" in line:
                key, value = line.split("=", 1)
                config[key.strip()] = value.strip()
    return config

def update_config(updates):
    """更新配置文件，保留注释和格式"""
    if not os.path.exists(CONFIG_FILE):
        return False, "Config file not found"

    # 读取所有行
    with open(CONFIG_FILE, 'r') as f:
        lines = f.readlines()

    new_lines = []
    updated_keys = set()

    for line in lines:
        stripped = line.strip()
        # 如果是注释或空行，直接保留
        if not stripped or stripped.startswith("#"):
            new_lines.append(line)
            continue
        
        # 尝试解析 key=value
        if "=" in stripped:
            key_part, val_part = stripped.split("=", 1)
            key = key_part.strip()
            
            # 如果这个key在更新列表中，替换整行
            if key in updates:
                new_value = str(updates[key])
                new_lines.append(f"{key}={new_value}\n")
                updated_keys.add(key)
            else:
                new_lines.append(line) # 没改动，保留原样
        else:
            new_lines.append(line)

    # (可选) 如果有新参数不在文件里，追加到末尾
    for key, val in updates.items():
        if key not in updated_keys:
            new_lines.append(f"\n{key}={val}\n")

    # 写回文件
    try:
        with open(CONFIG_FILE, 'w') as f:
            f.writelines(new_lines)
        return True, "Updated successfully"
    except Exception as e:
        return False, str(e)
# ---------------------------

class Handler(http.server.BaseHTTPRequestHandler):
    def _send_response(self, code, data):
        self.send_response(code)
        self.send_header("Content-Type", "application/json")
        self.send_header("Access-Control-Allow-Origin", "*") # 允许跨域，方便前端调试
        self.end_headers()
        self.wfile.write(json.dumps(data).encode("utf-8"))

    def do_GET(self):
        path = self.path.lower()
        
        # 获取当前配置
        if path == "/config":
            config_data = read_config()
            self._send_response(200, config_data)
        else:
            self._send_response(200, {"status": "ok"})

    def do_POST(self):
        global yolo_process
        path = self.path.lower()

        # --- 新增：修改配置接口 ---
        if path == "/config":
            try:
                content_length = int(self.headers['Content-Length'])
                post_data = self.rfile.read(content_length)
                updates = json.loads(post_data)
                
                success, msg = update_config(updates)
                if success:
                    self._send_response(200, {"status": "updated", "msg": "Changes will apply on next start"})
                else:
                    self._send_response(500, {"error": msg})
            except Exception as e:
                self._send_response(400, {"error": str(e)})
        # ------------------------

        elif path == "/start":
            if yolo_process is None or yolo_process.poll() is not None:
                try:
                    os.chdir(BASE_DIR)
                    yolo_output.clear()
                    # 确保有执行权限
                    if not os.access("./rknn_yolo11_demo", os.X_OK):
                         os.chmod("./rknn_yolo11_demo", 0o755)
                         
                    yolo_process = subprocess.Popen(
                        ["./rknn_yolo11_demo"],
                        stdout=subprocess.PIPE,
                        stderr=subprocess.STDOUT,
                        text=True,
                        bufsize=1
                    )
                    threading.Thread(
                        target=read_stdout,
                        args=(yolo_process,),
                        daemon=True
                    ).start()
                    self._send_response(200, {"status": "started"})
                except Exception as e:
                    self._send_response(500, {"error": str(e)})
            else:
                self._send_response(400, {"error": "already running"})

        elif path == "/stop":
            if yolo_process is not None and yolo_process.poll() is None:
                yolo_process.send_signal(signal.SIGTERM)
                try:
                    yolo_process.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    yolo_process.kill()
                    
                code = None
                copper_num = 0
                for line in yolo_output: 
                    if line.startswith("CODE="):
                        try: code = int(line.split("=")[1])
                        except: pass
                    if line.startswith("COPPER_NUM="):
                        try: copper_num = int(line.split("=")[1])
                        except: pass
                
                self._send_response(200, {
                    "status": "stopped",
                    "code": code,
                    "copperNum": copper_num
                })
            else:
                self._send_response(400, {"error": "not running"})

        elif path == "/status":
            running = yolo_process is not None and yolo_process.poll() is None
            self._send_response(200, {"running": running})

        else:
            self._send_response(404, {"error": "unknown endpoint"})

if __name__ == "__main__":
    # 切换到工作目录，防止路径错误
    if os.path.exists(BASE_DIR):
        os.chdir(BASE_DIR)
        
    with socketserver.TCPServer(("", PORT), Handler) as httpd:
        print(f"Serving at port {PORT}")
        print(f"Config file: {CONFIG_FILE}")
        httpd.serve_forever()