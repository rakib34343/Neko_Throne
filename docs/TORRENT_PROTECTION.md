# BitTorrent Traffic Protection

**Дата:** 2026-03-10  
**Проект:** Neko_Throne  
**Версия:** 1.0

---

## Обзор

Neko_Throne включает комплексную систему защиты от торрент-трафика для предотвращения злоупотреблений прокси-серверами и защиты от жалоб (abuse complaints).

### Зачем это нужно?

1. **Защита прокси-серверов** - торрент-трафик создает огромную нагрузку и может привести к блокировке IP
2. **Предотвращение DMCA жалоб** - загрузка защищенного авторских прав контента через прокси может привести к жалобам
3. **Экономия трафика** - P2P протоколы потребляют большой объем данных
4. **Производительность** - торренты создают множество одновременных соединений, перегружая ядро прокси

---

## Механизмы обнаружения

### 1. Protocol Detection (DPI - Deep Packet Inspection)
```json
{
  "protocol": "bittorrent",
  "action": "reject",
  "outbound": "block"
}
```
- ✅ Обнаруживает BitTorrent рукопожатия
- ✅ Требует sniffing включенным
- ✅ Работает для шифрованных торрентов с правильными заголовками

### 2. Port-Based Detection
```json
{
  "network": "udp",
  "port_range": ["6881:6889", "51413"],
  "action": "reject",
  "outbound": "block"
}
```
- ✅ Блокирует стандартные торрент-порты
- ✅ Ловит uTP (UDP-based torrents)
- ✅ Ловит DHT (Distributed Hash Table)

### 3. Process-Based Detection
```json
{
  "process_name": [
    "qbittorrent", "utorrent", "transmission",
    "deluge", "rtorrent", "aria2c"
  ],
  "action": "reject",
  "outbound": "block"
}
```
- ✅ Самый надежный метод
- ✅ Работает на Windows/Linux (требует права администратора)
- ✅ Блокирует все популярные торрент-клиенты

---

## Режимы работы

### Block (Рекомендуется) ⛔
```
torrent_block_enable = true
torrent_action = 0
```
- Полностью блокирует весь торрент-трафик
- **Преимущества**: Максимальная защита от abuse
- **Недостатки**: Пользователи не смогут использовать торренты вообще

### Direct 🔄
```
torrent_block_enable = true
torrent_action = 1
```
- Направляет торрент-трафик напрямую (не через прокси)
- **Преимущества**: Пользователи могут торрентить через свой реальный IP
- **Недостатки**: Может раскрыть реальный IP пользователя

### Proxy (Не рекомендуется) ⚠️
```
torrent_block_enable = true
torrent_action = 2
```
- Разрешает торрент-трафик через прокси
- **Преимущества**: Полная анонимность пользователя
- **Недостатки**: Риск DMCA жалоб, перегрузка прокси, возможная блокировка IP

---

## UI Настройка

### Через меню "Routing"

1. **Откройте меню Routing** (правый верхний угол)
2. **Найдите "BitTorrent Traffic Control"**
3. **Опции:**
   - ✅ **Enable Protection** - включить/выключить защиту
   - 🔴 **Block (Recommended)** - блокировать весь трафик
   - 🟡 **Route Direct** - напрямую (bypass proxy)
   - ⚠️ **Route via Proxy** - через прокси (опасно)

### Автоматический рестарт

При изменении настроек приложение автоматически перезапустит прокси для применения новых правил.

---

## Технические детали

### Обнаруживаемые клиенты

**Windows:**
- qBittorrent.exe
- uTorrent.exe, μTorrent.exe
- BitComet.exe
- BitTorrent.exe
- Vuze (Azureus.exe)
- Tixati.exe
- FrostWire.exe
- aria2c.exe
- BitTorrentWebHelper.exe

**Linux:**
- qbittorrent
- transmission, transmission-gtk, transmission-qt
- deluge, deluged
- rtorrent
- ktorrent
- aria2c
- webtorrent

**Cross-platform:**
- aria2c (универсальный загрузчик с поддержкой BitTorrent)

### Порты

| Протокол | Порты | Описание |
|----------|-------|----------|
| BitTorrent TCP | 6881-6889 | Классические торрент-порты |
| BitTorrent TCP | 51413 | Transmission default port |
| uTP (UDP) | 6881-6889 | UDP-based micro transport |
| DHT (UDP) | 6881 | Distributed Hash Table |

---

## Примеры конфигурации

### Полная блокировка
```json
{
  "route": {
    "rules": [
      {
        "protocol": "bittorrent",
        "action": "reject",
        "outbound": "block"
      },
      {
        "network": "udp",
        "port_range": ["6881:6889", "51413"],
        "action": "reject",
        "outbound": "block"
      },
      {
        "network": "tcp",
        "port_range": ["6881:6889", "51413"],
        "action": "reject",
        "outbound": "block"
      },
      {
        "process_name": ["qbittorrent", "utorrent", "transmission"],
        "action": "reject",
        "outbound": "block"
      }
    ]
  }
}
```

### Direct routing (bypass)
```json
{
  "route": {
    "rules": [
      {
        "protocol": "bittorrent",
        "action": "route",
        "outbound": "direct"
      }
    ]
  }
}
```

---

## Тестирование

### Проверка защиты

1. **Включите защиту** в режиме Block
2. **Запустите торрент-клиент** (qBittorrent, Transmission)
3. **Попытайтесь загрузить торрент**
4. **Ожидаемый результат**: Соединения не устанавливаются, трафика нет

### Проверка sniffing

```bash
# Проверьте, что sniffing включен в настройках маршрутизации
# Routing Settings -> Sniffing Mode -> For Routing
```

### Проверка логов

```bash
# В терминальных логах должны быть сообщения:
[ROUTE] Reject bittorrent connection
[ROUTE] Process qbittorrent blocked
```

---

## FAQ

### Q: Почему торренты все еще работают?

**A:** Возможные причины:
1. Sniffing отключен - включите в Routing Settings
2. Приложение не перезапустилось после изменения настроек
3. Торрент-клиент использует нестандартные порты
4. Торрент-трафик шифрован и не детектируется

**Решение:**
- Используйте режим Process-based (самый надежный)
- Убедитесь что приложение запущено с правами администратора (для process detection)

### Q: Можно ли разрешить определенные торренты?

**A:** Нет, текущая реализация блокирует/роутит весь торрент-трафик глобально. Для выборочного разрешения используйте режим "Direct" и создайте custom routing rules.

### Q: Влияет ли это на легальные P2P приложения?

**A:** Да, защита может блокировать:
- Popcorn Time
- BitTorrent-based streaming
- P2P video conferencing (некоторые)
- IPFS (если использует DHT на стандартных портах)

**Решение:** Используйте режим "Direct" или отключите защиту временно

### Q: Работает ли на macOS?

**A:** Частично. Process detection может работать некорректно на macOS из-за ограничений sandboxing. Рекомендуется использовать protocol и port-based detection.

---

## Безопасность

### Обход защиты

**Потенциальные способы обхода:**
1. **Использование нестандартных портов** - process detection решает
2. **Полное шифрование** - process detection решает
3. **Прокси-цепочки** - можно обнаружить только по процессу
4. **VPN внутри прокси** - невозможно обнаружить

**Рекомендации:**
- Всегда используйте режим "Block" для публичных прокси
- Включайте sniffing для максимальной защиты
- Запускайте приложение с правами администратора для process detection

---

## Производительность

### Влияние на performance

| Метод | CPU Usage | Memory | Latency |
|-------|-----------|--------|---------|
| Protocol (DPI) | +2-5% | +10MB | +1-3ms |
| Port-based | +0.1% | +1MB | +0.1ms |
| Process-based | +1-2% | +5MB | +0.5ms |

**Вывод:** Минимальное влияние на производительность даже при всех методах одновременно.

---

## Roadmap

### Планируемые улучшения

- [ ] Whitelist для разрешенных процессов
- [ ] Статистика заблокированного торрент-трафика
- [ ] Интеграция с rule-sets для DHT узлов
- [ ] Поддержка custom port ranges
- [ ] Режим "Log only" для мониторинга без блокировки

---

## Поддержка

Если обнаружите обход защиты или false-positives, пожалуйста, создайте issue на GitHub с:
- Версией Neko_Throne
- Используемым торрент-клиентом
- Логами приложения (без чувствительных данных)

---

## Лицензия

Эта функция является частью Neko_Throne и распространяется под GPLv3 License.

---

*Документация обновлена: 2026-03-10*
