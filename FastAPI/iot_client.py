import json
import boto3
import os
from dotenv import load_dotenv

load_dotenv()
DEVICE_ID = os.getenv("DEVICE_ID")

TOPIC_CMD = "iot-course/vlasenko/commands/led"

# Регіон boto3 сам візьме з AWS_DEFAULT_REGION
iot = boto3.client("iot-data")


def send_led_command(value: str):
    shadow_payload = json.dumps({
        "state": {
            "desired": {
                "led": value
            }
        }
    })

    response = iot.update_thing_shadow(
        thingName=DEVICE_ID,
        payload=shadow_payload.encode("utf-8"),
    )
    response["payload"].close()

    payload = json.dumps(
        {"action": "set_led", "value": value},
        separators=(",", ":"),
    )
    iot.publish(topic=TOPIC_CMD, qos=1, payload=payload)

    