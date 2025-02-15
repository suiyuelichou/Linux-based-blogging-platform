#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import time
import os
from watchdog.observers import Observer
from watchdog.events import FileSystemEventHandler
from datetime import datetime
from collections import deque
from threading import Lock, Thread
import queue

# 邮件发送功能
import smtplib
from email.mime.text import MIMEText
from email.mime.multipart import MIMEMultipart

# 配置
ALERT_KEYWORDS = ALERT_KEYWORDS = [
    "error", "bad request", "internal server error", 
    "service unavailable", "unauthorized", "denied", "forbidden", "access denied", 
    "sql injection", "xss", "csrf", "attack", "exploit", "brute force", "malware", 
    "ddos", "rce", "shell", "overload", "high memory", "disk full", 
    "out of memory", "too many requests", "rate limit", "latency", "cpu usage", 
    "login failed", "unauthenticated", "account locked", "invalid token", 
    "session expired", "connection refused", "connection timeout", "dns error", 
    "proxy error", "ssl handshake failed", "tls error", "<script>"
]

LOG_DIRECTORY = "/root/projects/C-WebServer/"

# 邮件配置
SMTP_SERVER = "smtp.163.com"
SMTP_PORT = 465
EMAIL_SENDER = "13536393626@163.com"
EMAIL_PASSWORD = "PJvyGDA7ySdcDAzZ"
EMAIL_RECEIVER = "1959503231@qq.com"

# 告警阈值配置
MAX_ALERTS_PER_MINUTE = 10
BUFFER_SIZE = 1000

class LogMonitorHandler(FileSystemEventHandler):
    def __init__(self):
        self.last_position = 0
        self.current_file = None
        self.buffer = deque(maxlen=BUFFER_SIZE)
        self.lock = Lock()
        self.alert_count = 0
        self.last_alert_reset = time.time()
        self.alert_queue = queue.Queue()
        self.start_alert_worker()

    def start_alert_worker(self):
        """
        启动告警处理线程
        """
        self.alert_thread = Thread(target=self.alert_worker, daemon=True)
        self.alert_thread.start()

    def alert_worker(self):
        """
        处理告警队列的工作线程
        """
        while True:
            try:
                message = self.alert_queue.get()
                if message:
                    self._send_alert(message)
                self.alert_queue.task_done()
            except Exception as e:
                print(f"[ERROR] Alert worker error: {e}")
            time.sleep(0.1)

    def on_modified(self, event):
        """
        处理文件修改事件
        """
        if not event.is_directory:
            self.process(event)

    def process(self, event):
        """
        处理文件变化事件
        """
        if event.event_type == 'modified':
            # print(f"[INFO] File modified: {event.src_path}")
            self.check_file(event.src_path)

    def check_file(self, file_path):
        """
        检查文件内容中的关键字
        """
        try:
            if self.current_file != file_path:
                self.current_file = file_path
                self.last_position = 0

            with open(file_path, 'r', encoding='utf-8') as file:
                file.seek(self.last_position)
                new_lines = file.readlines()

                if new_lines:
                    self.last_position = file.tell()
                    with self.lock:
                        self.buffer.extend(new_lines)

            # 处理缓冲区
            self.process_buffer()

        except Exception as e:
            print(f"[ERROR] Could not read file {file_path}: {e}")

    def process_buffer(self):
        """
        处理缓冲区中的日志
        """
        with self.lock:
            while self.buffer:
                line = self.buffer.popleft()
                for keyword in ALERT_KEYWORDS:
                    if keyword in line.lower():
                        self.alert(line.strip())
                        break

    def check_alert_rate(self):
        """
        检查告警频率并重置计数器
        """
        current_time = time.time()
        if current_time - self.last_alert_reset >= 60:
            self.alert_count = 0
            self.last_alert_reset = current_time
        
        if self.alert_count < MAX_ALERTS_PER_MINUTE:
            self.alert_count += 1
            return True
        return False

    def alert(self, message):
        """
        将告警消息加入队列
        """
        if self.check_alert_rate():
            self.alert_queue.put(message)
            print(f"[ALERT] {message}")
        else:
            print("[WARN] Alert rate limit exceeded")

    def _send_alert(self, message):
        """
        发送告警邮件
        """
        try:
            msg = MIMEMultipart()
            msg["From"] = EMAIL_SENDER
            msg["To"] = EMAIL_RECEIVER
            msg["Subject"] = "Log Alert Notification"
            msg.attach(MIMEText(message, "plain"))

            with smtplib.SMTP_SSL(SMTP_SERVER, SMTP_PORT) as server:
                # server.starttls()
                server.login(EMAIL_SENDER, EMAIL_PASSWORD)
                server.sendmail(EMAIL_SENDER, EMAIL_RECEIVER, msg.as_string())

            print("[INFO] Alert email sent successfully")
        except Exception as e:
            print(f"[ERROR] Failed to send alert email: {e}")

def start_monitoring():
    """
    启动日志监控
    """
    print(f"[INFO] Starting log monitor in directory: {LOG_DIRECTORY}")
    event_handler = LogMonitorHandler()
    observer = Observer()
    observer.schedule(event_handler, path=LOG_DIRECTORY, recursive=False)
    observer.start()

    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        print("[INFO] Stopping log monitor...")
        observer.stop()
        observer.join()
        print("[INFO] Log monitor stopped")

if __name__ == "__main__":
    try:
        # 确保日志目录存在
        if not os.path.exists(LOG_DIRECTORY):
            os.makedirs(LOG_DIRECTORY)
        start_monitoring()
    except Exception as e:
        print(f"[ERROR] Failed to start monitoring: {e}")