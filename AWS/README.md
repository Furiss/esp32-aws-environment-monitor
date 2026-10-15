# Конфігурації AWS

Актуальні конфігурації, виділені з [шаблону CloudFormation](../CloudFormation/cloudformation-template.json). Основний спосіб розгортання — за [інструкцією запуску](../docs/setup.md); ці файли потрібні для перегляду або ручного налаштування окремих ресурсів. Після розгортання стека повторно застосовувати їх не потрібно.

У JSON замініть `YOUR_ACCOUNT_ID` на ID свого акаунта, `YOUR_FIRMWARE_BUCKET` — на назву бакета. Значення за замовчуванням: регіон `eu-north-1`, Thing `ESP32-Vlasenko`, префікс топіків `iot-course/vlasenko`. Якщо змінювали ці параметри, узгодьте їх і в JSON. Регістр імен таблиць та шлях `/service-role/` у ARN ролей мають значення.

## Політики пристрою та бекенда

| Файл | Політика / призначення |
|---|---|
| [iot-policy.json](iot-policy.json) | `ESP32-telemetry-and-comands`: Connect, телеметрія, події, команди LED |
| [iot-shadow-policy.json](iot-shadow-policy.json) | `ESP32-Shadow`: читання й оновлення Shadow, відповіді та delta |
| [iot-jobs-policy.json](iot-jobs-policy.json) | `ESP32-Jobs`: отримання завдань і публікація статусу |
| [backend-policy.json](backend-policy.json) | `ESP32-Vlasenko-backend-policy`: Query двох таблиць, команда LED, UpdateThingShadow; прикріплена до `IoT_backend` |

Три IoT-політики прикріплюються до сертифіката пристрою. IAM-політика бекенда — до користувача `IoT_backend`.

## Правила IoT

Файли містять payload правила у форматі AWS IoT API: SQL, основну дію та Error action. Ім'я правила задається окремо.

| Файл | Ім'я правила | Основна дія |
|---|---|---|
| [rule-telemetry.json](rule-telemetry.json) | `ddb_telemetry` | DynamoDBv2 → `IoT_telemetry`, роль `dbb_telemetry` |
| [rule-events.json](rule-events.json) | `DynamoDB_events` | DynamoDBv2 → `IoT_Events`, роль `iot-dynamodb-events-role` |
| [rule-temperature.json](rule-temperature.json) | `out_of_range_temperature` | CloudWatch → `iot-out-of-range-temperature`, роль `iot_cloudwatch_role`; температура > 28 або < 15 |

У всіх трьох правилах Error action пише в `iot-errors`. Для телеметрії та температури використовується `CloudWatch-error-role`, для подій — `iot_cloudwatch_role`.

## Ролі та їхні IAM-політики

Файли `roles/*.json` містять властивості ролей із шаблону: ім'я, шлях, trust policy (`AssumeRolePolicyDocument`) та прикріплені політики (`ManagedPolicyArns`). Це довідкові конфігурації, не payload команди `create-role`. У `policies/*.json` наведені самі документи дозволів.

| Роль | Прикріплені політики |
|---|---|
| [CloudWatch-error-role](roles/CloudWatch-error-role.json) | [aws-iot-rule-out_of_range_temperature-erroraction-role-CloudWatch-error-role](policies/aws-iot-rule-out_of_range_temperature-erroraction-role-CloudWatch-error-role.json), [aws-iot-rule-DynamoDM_events-erroraction-role-CloudWatch-error-role](policies/aws-iot-rule-DynamoDM_events-erroraction-role-CloudWatch-error-role.json), [aws-iot-rule-ddb_telemetry-erroraction-role-CloudWatch-error-role](policies/aws-iot-rule-ddb_telemetry-erroraction-role-CloudWatch-error-role.json) |
| [ota](roles/ota.json) | [aws-iot-role-s3GetObjectVersion_1790850083401](policies/aws-iot-role-s3GetObjectVersion_1790850083401.json) |
| [iot-dynamodb-events-role](roles/iot-dynamodb-events-role.json) | [aws-iot-rule-DynamoDB_events-action-1-role-iot-dynamodb-events-role](policies/aws-iot-rule-DynamoDB_events-action-1-role-iot-dynamodb-events-role.json) |
| [iot_cloudwatch_role](roles/iot_cloudwatch_role.json) | [aws-iot-rule-DynamoDB_events-erroraction-role-iot_cloudwatch_role](policies/aws-iot-rule-DynamoDB_events-erroraction-role-iot_cloudwatch_role.json), [aws-iot-rule-out_of_range_temperature-action-1-role-iot_cloudwatch_role](policies/aws-iot-rule-out_of_range_temperature-action-1-role-iot_cloudwatch_role.json) |
| [dbb_telemetry](roles/dbb_telemetry.json) | [aws-iot-rule-ddb_telemetry-action-1-role-dbb_telemetry](policies/aws-iot-rule-ddb_telemetry-action-1-role-dbb_telemetry.json) |

Назви на кшталт `DynamoDM_events` усередині назв IAM-політик збережено, оскільки вони досі присутні у перевіреному шаблоні. Ім'я чинного правила подій — `DynamoDB_events`.

Стару спільну роль `iot_course_rules_role` замінено окремими ролями з таблиці. Єдиний шаблон для розгортання зберігається в папці `CloudFormation`.
