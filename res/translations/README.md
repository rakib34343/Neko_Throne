# Translations

Translation files are stored as Qt `.ts` (XML) files in this directory.
They are human-readable and can be edited in any text editor.

## File format

Each `.ts` file is standard XML:

```xml
<TS version="2.1" language="ru_RU">
  <context>
    <name>MainWindow</name>
    <message>
      <source>Settings</source>
      <translation>Настройки</translation>
    </message>
  </context>
</TS>
```

## How to edit a translation

1. Open `res/translations/ru_RU.ts` (or `zh_CN.ts`, `fa_IR.ts`) in any text editor.
2. Find the `<source>` text you want to change.
3. Edit the `<translation>` value.
4. Recompile: run `lrelease ru_RU.ts` or rebuild the project (`cmake --build .`).
5. Copy the resulting `ru_RU.qm` to the `lang/` folder next to the application binary.

## Live reload (no rebuild needed)

The app's `TranslationManager` supports live reload:

- Replace the `.qm` file in the `lang/` directory.
- Use "Settings → Language → Reload" (or restart the app) to apply.
- Developers can also call `TranslationManager::instance()->reloadCurrentLanguage()`
  from code to re-read the `.qm` from disk without rebuilding.

## Building translations

The build system compiles `.ts` → `.qm` automatically via `qt_add_lrelease`:

```bash
cmake --build . --target Throne_lrelease
```

Or manually with the `lrelease` tool:

```bash
lrelease res/translations/ru_RU.ts -qm lang/ru_RU.qm
```

## Do NOT embed `.qm` files into `.qrc`

The `.qm` files are deployed as external files in the `lang/` directory next to
the application binary. This allows users to edit translations and reload without
recompiling the application.

## Supported locales

| File        | Language             |
|-------------|----------------------|
| `zh_CN.ts`  | Chinese (Simplified) |
| `ru_RU.ts`  | Russian              |
| `fa_IR.ts`  | Persian (Farsi)      |
