# Запуск і налаштування

Інструкція передбачає запуск Wokwi, FastAPI, Grafana та HTML-панелі на одному ПК з Windows. Інфраструктуру перевірено розгортанням в окремому AWS-акаунті в регіоні `eu-north-1`.

Потрібні AWS-акаунт із правами створювати ресурси шаблону, Python, VS Code з розширеннями **PlatformIO IDE** та **Wokwi Simulator** (з активованою ліцензією), а також встановлена Grafana. Локальне середовище бекенда — **Python 3.14.6**; проєкт виконано на **Grafana 13.2.2**.

Завантажте репозиторій або клонуйте його.

| Файли | Призначення |
|---|---|
| [CloudFormation/cloudformation-template.json](../CloudFormation/cloudformation-template.json) | Актуальний шаблон інфраструктури AWS |
| [AWS](../AWS/README.md) | Окремі JSON політик, ролей і правил із цього шаблону |
| [ESP32](../ESP32) | Прошивка PlatformIO та схема Wokwi |
| [FastAPI](../FastAPI) | Бекенд |
| [HTML/index.html](../HTML/index.html) | Панель керування LED |
| [Grafana](../Grafana) | JSON дашборда для імпорту |
| [screenshots](../screenshots) | Підтвердження роботи |


### 1. Ресурси AWS через CloudFormation

1. У потрібному AWS-акаунті виберіть регіон `eu-north-1`. Відкрийте **CloudFormation → Create stack → With new resources**.
2. Виберіть **Choose an existing template → Upload a template file** та завантажте [cloudformation-template.json](../CloudFormation/cloudformation-template.json).
3. Укажіть назву стека, наприклад `esp32-monitor-test`, і параметри:

   | Параметр | Значення |
   |---|---|
   | `ThingName` | `ESP32-Vlasenko` |
   | `TopicPrefix` | `iot-course/vlasenko` без кінцевого `/` |
   | `FirmwareBucketName` | Власна глобально унікальна назва S3-бакета, малими літерами |

4. На **Configure stack options** підтвердьте створення IAM-ресурсів із власними назвами в **Capabilities**. Решту налаштувань залиште без змін.
5. Перевірте параметри та натисніть **Submit**. Дочекайтеся статусу стека **CREATE_COMPLETE**. Старі записи `CREATE_IN_PROGRESS` у **Events** залишаються як історія.

Шаблон створює:

| Ресурси | Назви / призначення |
|---|---|
| IoT Thing | Значення `ThingName` |
| IoT Policies | `ESP32-telemetry-and-comands`, `ESP32-Shadow`, `ESP32-Jobs` |
| DynamoDB | `IoT_telemetry`, `IoT_Events`; ключі `device_id` (String), `received_at` (Number) |
| IoT Rules | `ddb_telemetry` → телеметрія; `DynamoDB_events` → події; `out_of_range_temperature` → CloudWatch |
| CloudWatch Log Groups | `iot-out-of-range-temperature`, `iot-errors` |
| S3 | Приватний бакет зі значення `FirmwareBucketName` |
| IAM | Ролі та політики для IoT Rules і OTA, користувач `IoT_backend` із політикою бекенда |

Thing створюється з шаблону за параметром `ThingName`; подальша інструкція передбачає, що він уже існує. У локальних файлах, створених із `.example`, вручну впишіть це саме ім'я: у `THINGNAME` для ESP32 та `DEVICE_ID` для бекенда. Автоматично в ці файли воно не підставляється.

SQL, дії правил, політики та їхні прив'язки створюються шаблоном. Їхній вміст наведено нижче в README; повторно створювати їх вручну не потрібно.

Шаблон призначений для акаунта без однойменних ресурсів. Частина назв фіксована: інша назва стека не усуває конфлікт із наявними таблицями або IAM-ресурсами. Ресурси мають `DeletionPolicy: Retain`, тому після видалення стека вони залишаються в акаунті.

### 2. Сертифікат пристрою та ключі бекенда

**Пристрій.** CloudFormation не переносить приватний ключ пристрою. У новому акаунті створіть новий сертифікат:

1. Відкрийте **AWS IoT Core → All devices → Things → ESP32-Vlasenko → Certificates → Create certificate** (якщо змінили `ThingName`, виберіть свій Thing). Створіть сертифікат через автоматичну генерацію — він буде прикріплений до вибраного Thing.
2. Активуйте сертифікат (**Active**), завантажте сертифікат пристрою, приватний ключ та **Amazon Root CA 1**. Прикріпіть політики `ESP32-telemetry-and-comands`, `ESP32-Shadow`, `ESP32-Jobs`; далі вставте завантажені PEM у `secrets.h`, як описано нижче.
3. У **AWS IoT Core → Settings** скопіюйте **Device data endpoint** цього акаунта й регіону.

**Бекенд.** У **IAM → Users → IoT_backend → Security credentials → Create access key** створіть ключ для локального коду. Збережіть **Access key ID** і **Secret access key** для `.env`. Політику користувача вже прикріплено шаблоном: читання конкретних таблиць, публікація команди LED й оновлення Shadow потрібного Thing.

`secrets.h`, `.env`, сертифікати та приватні ключі не додаються до Git. Приватний ключ пристрою і секретний ключ IAM після створення треба зберегти локально.

### 3. ESP32 / Wokwi

1. Відкрийте у VS Code саме папку **ESP32**.
2. Скопіюйте [src/secrets.h.example](../ESP32/src/secrets.h.example) у `src/secrets.h` і заповніть:

   | Поле | Значення |
   |---|---|
   | `WIFI_SSID`, `WIFI_PASSWORD` | Для Wokwi: `Wokwi-GUEST`, порожній пароль |
   | `THINGNAME` | Точне значення `ThingName`, типово `ESP32-Vlasenko` |
   | `AWS_IOT_ENDPOINT` | Endpoint без `https://` та без шляху |
   | `AWS_CERT_CA` | Повний вміст AmazonRootCA1.pem |
   | `AWS_CERT_CRT` | Повний вміст сертифіката пристрою |
   | `AWS_CERT_PRIVATE` | Повний вміст приватного ключа |

   Усередині кожного `R"EOF(...)EOF"` замініть увесь приклад відповідним PEM, включно з його рядками `BEGIN` та `END`.

3. Залиште початкову версію `1.0.0` у [src/version.h](../ESP32/src/version.h). Виконайте **PlatformIO: Build**.
4. Відкрийте `diagram.json` і запустіть **Wokwi: Start Simulator**. Бібліотеки, розділи OTA та генерація `merged.bin` уже налаштовані в проєкті.

Піни: DHT22 — GPIO 4, датчик освітленості — GPIO 34, LED — GPIO 2, кнопка — GPIO 25. Після підключення пристрій синхронізує час, надсилає дані й переходить у сон на 60 секунд. Час між повідомленнями також включає підключення та обробку запитів.

Для перегляду повідомлень відкрийте **AWS IoT Core → MQTT test client** і підпишіться на `iot-course/vlasenko/telemetry` та `iot-course/vlasenko/events`. Для `manual_trigger` у Wokwi утримуйте кнопку до пробудження пристрою, потім відпустіть.

Якщо змінюєте `TopicPrefix`, змініть також три топіки в [ESP32/src/config/config.h](../ESP32/src/config/config.h) і `TOPIC_CMD` у [FastAPI/iot_client.py](../FastAPI/iot_client.py). Якщо змінюєте `ThingName`, однакове ім'я має бути в шаблоні, `THINGNAME` і `DEVICE_ID` бекенда.

### 4. Device Shadow

У **AWS IoT Core → Things → ваш Thing → Device Shadows** створіть або відкрийте **Classic Shadow**. Задайте початковий бажаний стан:

```json
{
  "state": {
    "desired": {
      "led": "off",
      "sleep_interval_sec": 60
    }
  }
}
```

Пристрій застосує його під час наступного підключення та запише фактичний стан і `firmware_version` у `reported`. Для керування змінюйте лише `desired`: `led` приймає `on` або `off`, `sleep_interval_sec` — додатну цілу кількість секунд. Значення `reported` заповнює прошивка.

### 5. FastAPI та HTML-панель

У терміналі з кореня репозиторію виконайте:

```powershell
cd FastAPI
py -m venv .venv
.\.venv\Scripts\Activate.ps1
python -m pip install -r requirements.txt
```

Скопіюйте `.env.example` у `.env` та підставте власні ключі IAM і параметри розгорнутого стека:

```dotenv
AWS_ACCESS_KEY_ID=ваш_access_key_id
AWS_SECRET_ACCESS_KEY=ваш_secret_access_key
AWS_DEFAULT_REGION=eu-north-1
TABLE_NAME=IoT_telemetry
EVENTS_TABLE_NAME=IoT_Events
DEVICE_ID=ESP32-Vlasenko
```

У `DEVICE_ID` впишіть ім'я Thing, створеного шаблоном. У тому самому терміналі з активованим середовищем, із папки `FastAPI`, запустіть:

```powershell
fastapi dev main.py
```

Залиште термінал відкритим. API працює на `http://127.0.0.1:8000`, Swagger UI — на `http://127.0.0.1:8000/docs`.

| Метод і маршрут | Призначення |
|---|---|
| `GET /sensors/latest` | Останній запис телеметрії |
| `GET /sensors/history?hours=6` | Історія за 1–24 години |
| `GET /events?limit=10` | Останні події `manual_trigger` та `led_changed` |
| `POST /actuators/led` | Тіло `{"value":"on"}` або `{"value":"off"}` |
| `GET /health` | Стан API |

Відкрийте [HTML/index.html](../HTML/index.html) у браузері й натисніть кнопку LED. Бекенд оновлює `desired.led` у Shadow та публікує MQTT-команду. Якщо пристрій спить, новий стан застосовується після пробудження. Відповідь `202` означає прийняття команди бекендом.

### 6. Grafana

Перед імпортом запустіть FastAPI та пристрій, щоб у таблицях були дані. Відкрийте запущену Grafana на `http://localhost:3000` (за стандартного порту); для першого входу — `admin` / `admin`, після чого задайте новий пароль.

1. У Grafana відкрийте **Connections → Add new connection**, знайдіть **Infinity**, відкрийте сторінку плагіна й натисніть **Install**. Імпорт JSON дашборда сам плагін не встановлює.
2. Відкрийте **Connections → Data sources → Add new data source**, виберіть **Infinity** та натисніть **Save & test**, залишивши налаштування без змін.
3. У **Dashboards → New → Import** завантажте [JSON дашборда](../Grafana/IoT%20dashboard-1791798786783.json), виберіть створене джерело Infinity та натисніть **Import**.

Дашборд містить графік температури й вологості за 6 годин, Gauge освітленості, час отримання даних / uptime та таблицю останніх 10 подій. Адреси запитів `http://127.0.0.1:8000` вже збережені в JSON; ключі AWS до Grafana вводити не потрібно.

### 7. Оновлення OTA: 1.0.0 → 1.1.0

1. Переконайтеся, що початкова прошивка працює з версією `1.0.0`. Змініть `FIRMWARE_VERSION` у [version.h](../ESP32/src/version.h) на `1.1.0` і виконайте **PlatformIO: Build**. Версія відображається в Serial, телеметрії, подіях і Shadow.
2. Завантажте **ESP32/.pio/build/esp32dev/firmware.bin** у створений S3-бакет. Для OTA потрібен цей файл; `merged.bin` використовується для початкового запуску Wokwi.
3. У [ESP32/job.json](../ESP32/job.json) укажіть свою назву бакета та регіон, залишивши службову конструкцію URL. Завантажте файл як `job.json` у той самий бакет:

   ```json
   {
     "operation": "ota",
     "version": "1.1.0",
     "url": "${aws:iot:s3-presigned-url-v2:https://YOUR_BUCKET.s3.eu-north-1.amazonaws.com/firmware.bin}"
   }
   ```

4. Для демонстрації в Wokwi поверніть локальний `version.h` до `1.0.0`, зберіть і запустіть симулятор. У S3 залиште вже завантажений образ **1.1.0**, не замінюйте його локальною збіркою 1.0.0.
5. У **AWS IoT Core → Remote actions → Jobs → Create job** створіть власне завдання (**custom job**) з новим ID, наприклад `OTA_1210`. Виберіть свій Thing, документ `job.json` із S3 та одноразове виконання **Snapshot**.
6. У конфігурації підписаних URL виберіть створену шаблоном роль `ota`. Решту налаштувань залиште без змін. Створіть завдання й дочекайтеся наступного підключення пристрою.
7. Після завантаження й перезапуску перевірте **Job executions → Succeeded** та `firmware_version: 1.1.0` у новій телеметрії й `reported` Shadow.

Для повторного тесту створюйте завдання з новим ID. Версія в документі має збігатися з версією завантаженого образу й відрізнятися від поточної версії пристрою. Саме редагування вихідного коду не змінює образ, який уже лежить у S3.
