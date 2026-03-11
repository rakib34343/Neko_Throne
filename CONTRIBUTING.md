# Contributing Guidelines / Правила участия в проекте

## GitHub Copilot Coding Agent — Usage Rules / Правила использования

### ❌ Do NOT use the Coding Agent (PR via agent) for / НЕ запускать Coding Agent (PR через агента) для:

- Fixes to one or two files / Правок одного-двух файлов
- Fixing typos, version numbers, or URLs / Исправления опечаток, версий, URL
- Minor CI/CD changes / Мелких изменений в CI/CD
- **Any change touching fewer than 5 files / Любых правок, где изменений < 5 файлов**

For these simple fixes, commit directly using `githubwrite` or your own commit.
This is cost-efficient and does not consume Copilot premium requests.

Для простых правок выполняйте коммит напрямую через `githubwrite` (или сами).
Это дёшево — не тратит premium requests.

---

### ✅ Use the Coding Agent only for / Coding Agent только для:

- Large-scale refactoring touching **10+ files** / Крупного рефакторинга (10+ файлов)
- Tasks that require analysis of the **entire codebase** / Когда задача требует анализа всего кода
- When you **explicitly write** "create PR via agent" / Когда явно написано «создай PR через агента»

---

## Project Context / Контекст проекта

| Repository | Purpose |
|---|---|
| [`rakib34343/Neko_Throne`](https://github.com/rakib34343/Neko_Throne) | Testing / Тестирование |
| [`DpaKc404/Neko_Throne`](https://github.com/DpaKc404/Neko_Throne) | Upstream releases / Апстрим (релизы) |

Updates are pulled from **DpaKc404/Neko_Throne** (upstream). After testing in this repository, fixes are sent back upstream.

Обновления скачиваются из **DpaKc404/Neko_Throne** — туда идут исправления после тестирования.
