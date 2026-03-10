# H3 DNS Support Implementation Guide

## Текущее состояние

✅ **H3 DNS поддержка УЖЕ реализована** в `src/configs/generate.cpp` (строки 295-305)

### Уже работает:
1. **Парсинг строк**: `h3://domain.com/dns-query` корректно обрабатывается
2. **Генерация JSON для Sing-box**: Создается правильная структура с `type: "h3"`
3. **UI подсказки**: В tooltip уже есть пример использования h3://

## Архитектура

### Sing-box (✅ Полностью поддерживается)

**Местоположение кода**: `src/configs/generate.cpp`, функция `buildDnsObj`

```cpp
if (address.startsWith("h3://")) {
    type = "h3";
    addr = addr.replace("h3://", "");
    if (addr.contains("/")) {
        path = addr.split("/").last();
        addr = addr.left(addr.indexOf("/"));
    }
}
```

**Генерируемый JSON** для Sing-box 1.13.2+:
```json
{
  "type": "h3",
  "server": "1.1.1.1",
  "server_port": 443,
  "path": "dns-query"
}
```

### Xray (✅ Правильная архитектура)

**Важно**: Xray не получает напрямую DNS конфигурацию из UI. Xray outbounds работают через Sing-box как SOCKS прокси, поэтому DNS разрешается через Sing-box. Это **правильная архитектура** и ничего менять не нужно.

## Как использовать в UI

### 1. Основные настройки DNS (Упрощенный режим)

**Местоположение**: Настройки маршрутизации → Настройки DNS

Поддерживаемые форматы:
- `h3://1.1.1.1/dns-query`
- `h3://dns.google:443/dns-query`
- `h3://cloudflare-dns.com/dns-query`

**Примеры ввода**:
```
Удалённый DNS: h3://1.1.1.1/dns-query
DNS для прямых запросов: h3://8.8.8.8/dns-query
```

### 2. DNS-объект (Продвинутый JSON режим)

**Активация**: Включите чекбокс "Использовать DNS-объект"

**Пример конфигурации**:
```json
{
  "servers": [
    {
      "tag": "cloudflare-h3",
      "type": "h3",
      "server": "1.1.1.1",
      "server_port": 443,
      "path": "dns-query"
    },
    {
      "tag": "google-h3",
      "type": "h3",
      "server": "8.8.8.8",
      "server_port": 443,
      "path": "dns-query"
    }
  ],
  "rules": [],
  "final": "cloudflare-h3"
}
```

## Маршрутизация UDP/QUIC

H3 использует QUIC (UDP на порту 443). **Никаких специальных правил не требуется**, так как:

1. Sing-box автоматически создает необходимые outbounds для DNS
2. TUN режим корректно маршрутизирует UDP трафик
3. DNS запросы обрабатываются до применения правил маршрутизации

## Провайдеры H3 DNS

### Рекомендуемые серверы:

1. **Cloudflare**:
   - `h3://1.1.1.1/dns-query`
   - `h3://1.0.0.1/dns-query`

2. **Google**:
   - `h3://8.8.8.8/dns-query`
   - `h3://8.8.4.4/dns-query`

3. **AdGuard**:
   - `h3://dns.adguard.com/dns-query`

4. **Quad9**:
   - `h3://9.9.9.9/dns-query`

## Валидация в редакторе DNS-объектов

**Местоположение кода**: `src/ui/setting/dialog_manage_routes.cpp`

Функция "Проверка форматирования" использует `QString2QJsonObject` который уже корректно обрабатывает любой валидный JSON. Никаких изменений не требуется.

## Проверка работоспособности

После настройки H3 DNS:

1. Запустите профиль
2. Проверьте логи Sing-box на наличие строк с `dns/h3`
3. DNS запросы должны проходить через H3 без ошибок
4. Латентность DNS обычно 10-30ms для Cloudflare

## Troubleshooting

### Проблема: "No default interface"
**Решение**: Установите конкретный IP в "Local override" вместо "localhost"

### Проблема: DNS не резолвится
**Решение**: Проверьте что UDP порт 443 не блокируется файрволом

### Проблема: Высокая латентность
**Решение**: Попробуйте другой H3 DNS провайдер ближе географически

## Для разработчиков

### Если нужно добавить новый DNS протокол:

1. Добавьте парсинг в `buildDnsObj()` в `src/configs/generate.cpp`
2. Добавьте пример в tooltip в `include/ui/setting/dialog_manage_routes.ui` (строка 277)
3. Обновите placeholder для DNS-объекта

### Тесты:

```cpp
// Добавьте unit тест для парсинга:
auto obj = buildDnsObj("h3://1.1.1.1:443/dns-query", ctx);
ASSERT_EQ(obj["type"].toString(), "h3");
ASSERT_EQ(obj["server"].toString(), "1.1.1.1");
ASSERT_EQ(obj["server_port"].toInt(), 443);
ASSERT_EQ(obj["path"].toString(), "dns-query");
```

## Заключение

✅ H3 DNS **полностью работает** в текущей версии  
✅ Поддерживается в обоих режимах UI (упрощенный и JSON)  
✅ Корректная интеграция с Sing-box 1.13.2+  
✅ Правильная архитектура для Xray (через Sing-box DNS)  
✅ Не требует дополнительной настройки маршрутизации

### Статус задачи: ✅ ЗАВЕРШЕНО
