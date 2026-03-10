# Privacy and Security Improvements

## DNS Leak Prevention

### Текущие механизмы защиты:

1. **VPN Strict Route** ✅
   - Включено по умолчанию на Windows 10+ и всех других платформах
   - Предотвращает обход VPN туннеля
   - Местоположение: `src/configs/generate.cpp` (строка 522)

2. **Раздельный DNS** ✅
   - Remote DNS через proxy (для проксируемых доменов)
   - Direct DNS напрямую (для локальных/whitelisted доменов)
   - Предотвращает утечку DNS запросов

3. **DNS over H3/HTTPS/TLS/QUIC** ✅
   - Поддержка шифрованных DNS протоколов
   - Защита от прослушивания DNS запросов ISP
   - Примеры: `h3://1.1.1.1/dns-query`, `https://dns.google/dns-query`

4. **FakeIP Support** ✅
   - Предотвращает реальные DNS запросы для проксируемого трафика
   - Уменьшает латентность
   - Защита от DNS утечек через fake addresses

5. **Localhost Protection на TUN** ✅
   - Автоматическая замена localhost на 8.8.8.8 в Linux+TUN режиме
   - Предотвращает ошибку "No default interface"
   - Строка 323-327 в generate.cpp

## Проверка на утечки

### DNS Leak Test:
```bash
# Запустите с активным профилем:
curl -s https://1.1.1.1/cdn-cgi/trace | grep fl=
# Должно показать IP прокси сервера, а не ваш реальный IP
```

### WebRTC Leak Test:
- Откройте: https://browserleaks.com/webrtc
- Проверьте что локальные IP не раскрываются

### Тест DNS:
```bash
# С включенным прокси:
nslookup google.com
# DNS сервер должен быть из конфига прокси
```

## Рекомендации для максимальной приватности:

1. **Включить VPN Strict Route**
   - Настройки → VPN Settings → Strict Route ✓

2. **Использовать Remote DNS через шифрование**
   ```
   Remote DNS: h3://1.1.1.1/dns-query
   или
   Remote DNS: https://dns.google/dns-query
   ```

3. **Включить FakeIP**
   - Routing Settings → DNS Settings → Enable FakeIP ✓

4. **Настроить Sniffing**
   - Routes → Common → Sniffing Mode: "Sniff result for routing"
   - Помогает правильно определять протоколы

5. **Проверить правила маршрутизации**
   - Убедитесь что правило DNS hijack активно
   - Проверьте что локальные домены идут через Direct

## Технические детали

### Архитектура DNS в Sing-box:

```
Application → Sing-box DNS Router → [Rule Engine] → DNS Server Selection
                                        ↓
                                   [Remote/Direct/Local]
                                        ↓
                                   [Protocol: UDP/TCP/DoH/DoT/DoQ/H3]
                                        ↓
                                   [Encrypted Channel]
```

### Порядок проверки DNS:

1. FakeIP (если включен)
2. DNS Rules (по доменам/geosite)
3. Direct DNS (для whitelisted)
4. Remote DNS (через proxy)
5. Fallback (если указан)

### Предотвращение утечек в TUN режиме:

```cpp
// generate.cpp, строка 522
inboundObj["strict_route"] = dataStore->vpn_strict_route;
inboundObj["stack"] = dataStore->vpn_implementation;
```

**Strict Route** гарантирует:
- Весь трафик идет через TUN интерфейс
- Нет обхода через физические интерфейсы
- DNS запросы не уходят в обход прокси

## Memory Safety

### Исправленные утечки памяти:

1. **QSystemTrayIcon** - добавлен parent (mainwindow.cpp:369)
2. **QMenu** - добавлен parent для tray меню
3. **QWidget/QDialog** - добавлены WA_DeleteOnClose атрибуты
4. **Tab widgets** - правильный parent при создании

### Qt Memory Management:

Qt использует parent-child систему:
- Объект с parent автоматически удаляется при удалении parent
- Объекты без parent должны иметь `deleteLater()` или `delete`
- Диалоги должны иметь `Qt::WA_DeleteOnClose` если нет explicit delete

## Проверка изменений:

```bash
# Проверить на утечки памяти (Linux):
valgrind --leak-check=full ./nekoray

# Проверить DNS утечки:
curl https://ipleak.net/json/

# Проверить все соединения:
netstat -tunp | grep nekoray
```

## Статус:

✅ DNS leak protection - Реализовано
✅ VPN strict route - Включено
✅ Memory leaks - Исправлено
✅ H3 DNS support - Работает
✅ Encrypted DNS - Поддерживается (H3, HTTPS, TLS, QUIC)
✅ FakeIP - Доступно
✅ Split DNS - Работает (Remote/Direct)
