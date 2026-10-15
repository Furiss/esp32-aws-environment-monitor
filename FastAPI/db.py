import os
import time
import boto3
from boto3.dynamodb.conditions import Key, Attr

# ═══════════════ Підключення до DynamoDB ═══════════════
# Регіон задаємо явно — щоб не залежати від налаштувань aws config
dynamodb = boto3.resource("dynamodb", region_name=os.getenv("AWS_DEFAULT_REGION"))

TABLE_NAME = os.getenv("TABLE_NAME")
EVENTS_TABLE_NAME = os.getenv("EVENTS_TABLE_NAME")
DEVICE_ID = os.getenv("DEVICE_ID")

table = dynamodb.Table(TABLE_NAME)
events_table = dynamodb.Table(EVENTS_TABLE_NAME)

# ═══════════════ Запити до таблиці ═══════════════
def get_latest():
    """Останній замір пристрою: у його партицію, згори, один."""
    response = table.query(
        KeyConditionExpression=Key("device_id").eq(DEVICE_ID),
        ScanIndexForward=False,   # найновіші згори
        Limit=1,
    )
    items = response["Items"]
    return items[0] if items else None


def get_history(hours):
    now_ms = int(time.time() * 1000)
    cutoff_ms = now_ms - hours * 60 * 60 * 1000

    query = {
        "KeyConditionExpression": (
            Key("device_id").eq(DEVICE_ID)
            & Key("received_at").between(cutoff_ms, now_ms)
        )
    }

    items = []

    while True:
        response = table.query(**query)
        items.extend(response["Items"])

        last_key = response.get("LastEvaluatedKey")
        if not last_key:
            break

        query["ExclusiveStartKey"] = last_key

    return items



def get_events(limit):
    query = {
        "KeyConditionExpression": Key("device_id").eq(DEVICE_ID),
        "FilterExpression": Attr("event").is_in(["manual_trigger", "led_changed"]),
        "ScanIndexForward": False,
    }

    items = []

    while len(items) < limit:
        query["Limit"] = limit - len(items)

        response = events_table.query(**query)
        items.extend(response["Items"])

        last_key = response.get("LastEvaluatedKey")
        if not last_key:
            break

        query["ExclusiveStartKey"] = last_key

    return items
