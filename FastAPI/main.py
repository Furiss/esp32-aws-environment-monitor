from fastapi import FastAPI, HTTPException, Query
from fastapi.middleware.cors import CORSMiddleware

from dotenv import load_dotenv
from pydantic import BaseModel
from typing import Literal
from iot_client import send_led_command
load_dotenv()          # ← до імпорту db, щоб змінні вже були в середовищі
import db

class LedCommand(BaseModel):
    value: Literal["on", "off"]

app = FastAPI(title="IoT Backend")

app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],            # демо; у проді — список конкретних доменів
    allow_credentials=False,        # кукі не використовуємо
    allow_methods=["GET", "POST"],  # default — тільки GET, POST треба дописати
    allow_headers=["Content-Type"], # без нього preflight на JSON не пройде
)

@app.get("/health")
def health():
    return {"status": "ok"}

@app.get("/sensors/latest")
def sensors_latest():
    item = db.get_latest()
    if item is None:
        raise HTTPException(status_code=404, detail="No data yet")
    return item

@app.get("/sensors/history")
def sensors_history(hours: int = Query(default=6, ge=1, le=24)):
    return db.get_history(hours)


@app.post("/actuators/led", status_code=202)
def set_led(cmd:LedCommand):
    try:
        send_led_command(cmd.value)
    except Exception:
        raise HTTPException(502, "AWS IoT недоступний")
    
    return {"accepted": True, "value": cmd.value}

@app.get("/events")
def events(limit: int = Query(default=10, ge=1)):
    return db.get_events(limit)
