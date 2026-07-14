# SmartType

SmartType — контекстный автокорректор русского и английского текста для Linux.
Он работает как метод ввода Fcitx 5: исправляет опечатки и неверную раскладку,
показывает кандидатов возле курсора и умеет вернуть последнее исправление
клавишей Backspace.

Проект находится в стадии раннего публичного релиза.

Исходный код распространяется по лицензии
[GNU General Public License v3.0](LICENSE). Страница проекта и поддержка:
[github.com/Mekhanic/smart_type](https://github.com/Mekhanic/smart_type).

## Поддерживаемые окружения

| Окружение | Статус |
|---|---|
| Fedora 44, KDE Plasma, Wayland | проверено на реальной рабочей сессии |
| Ubuntu 26.04, KDE Plasma, Wayland | проверено в отдельной ВМ и после перезагрузки |
| Kali Rolling, Xfce, X11 | проверено в Mousepad, Firefox ESR и Kate после холодного входа |

Поддержка относится к указанным окружениям, а не ко всем выпускам дистрибутива.
GNOME, XWayland, Ubuntu 24.04 и Arch пока не являются подтверждёнными целями
релиза. Flatpak и AppImage не подходят: аддон должен загружаться процессом
Fcitx на хосте.

## Быстрая установка без компиляции

Откройте терминал внутри графического сеанса и выполните:

```bash
curl -fsSL https://raw.githubusercontent.com/Mekhanic/smart_type/main/scripts/install-release.sh | bash
```

Установщик определяет Fedora/Ubuntu/Kali и Wayland/X11, скачивает готовую
сборку для системы, проверяет её SHA-256, устанавливает runtime-зависимости и
SmartType в `~/.local`, добавляет оба метода ввода в Fcitx и включает
автозапуск трея. Компилятор на компьютере пользователя не нужен.

Если вы предпочитаете сначала прочитать установщик:

```bash
curl -fLO https://raw.githubusercontent.com/Mekhanic/smart_type/main/scripts/install-release.sh
less install-release.sh
bash install-release.sh
```

После первой установки один раз выйдите из графического сеанса и войдите
снова. Затем проверьте установку:

```bash
~/.local/share/smarttype/doctor.sh
```

Для автоматической установки пакетов без дополнительного подтверждения:

```bash
curl -fsSL https://raw.githubusercontent.com/Mekhanic/smart_type/main/scripts/install-release.sh | bash -s -- --yes
```

Подробности и ручной режим: [docs/INSTALL.md](docs/INSTALL.md).

## Быстрая проверка

Откройте браузер, Kate или обычный текстовый редактор и попробуйте:

- `севодня ` → `сегодня `;
- `руддщ ` в русской раскладке → `hello `;
- введите начало слова и выберите вариант в панели кандидатов;
- сразу после исправления нажмите Backspace — исходный текст должен вернуться.

На X11 набираемый текст остаётся в поле приложения; панель показывает только
кандидатов.

## Возможности

- автокоррекция и подсказки без отправки текста в облако;
- исправление неверной раскладки RU/EN;
- личный словарь, пользовательские правила и обучение;
- защита URL, email, путей, IP-адресов и технических токенов;
- отключение в паролях, терминалах и редакторах кода;
- история исправлений и Markdown-отчёт о проблеме из меню трея;
- единая иконка приложения и адаптивный логотип трея для светлой/тёмной темы.

Личная база находится в `~/.local/share/smarttype/personal.sqlite3`. В журнале
решений сохраняются действие, причина и хеш токена, но не полный набранный
текст, пароли или буфер обмена.

## Управление

```bash
smarttypectl add-word Happ
smarttypectl add-rule вопщем "в общем"
smarttypectl status
```

Настройки, история, словарь и отчёт о проблеме доступны через значок SmartType
в системном трее.

## Удаление

```bash
~/.local/share/smarttype/uninstall-user.sh
```

Личная база по умолчанию сохраняется.

## Разработка

Для принудительной сборки и тестирования из исходников:

```bash
git clone https://github.com/Mekhanic/smart_type.git
cd smart_type
./install.sh --build-from-source
```

Рекомендуется 4 ГБ ОЗУ, два ядра и около 5 ГБ свободного места. Обычная
установка готового релиза этих ресурсов не требует.

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
```

Архитектура и границы поддержки описаны в
[docs/ARCHITECTURE.md](docs/ARCHITECTURE.md), ручная матрица — в
[docs/DISTRO_TEST_PLAN.md](docs/DISTRO_TEST_PLAN.md).
