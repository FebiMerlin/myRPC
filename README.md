# myRPC — удалённое выполнение команд поверх сокетов

Подпроект состоит из двух программ:

| Модуль | Назначение | Документация |
|---|---|---|
| [`client`](client/README.md) | консольная утилита `myRPC-client` | [client/README.md](client/README.md) |
| [`server`](server/README.md) | демон `myRPC-server` | [server/README.md](server/README.md) |
| `common` | общий код: журнал, конфигурация, протокол | — |
| `tests` | модульные и интеграционные тесты | — |
| `docs` | методология разработки и диаграммы | [docs/methodology.md](docs/methodology.md) |

## Как это работает

```
 ┌────────────────┐   {"login":"student",         ┌────────────────┐
 │  myRPC-client  │    "command":"ls -la"}        │  myRPC-server  │
 │                │ ────────────────────────────► │    (демон)     │
 │ -c "ls -la"    │                               │                │
 │ -h 10.0.0.2    │ ◄──────────────────────────── │  fork на каждый│
 └────────────────┘   {"code":0,"result":"…"}     │     запрос     │
                                                  └───────┬────────┘
                                                          │
                                   /etc/myRPC/myRPC.conf ──┤ порт, тип сокета
                                   /etc/myRPC/users.conf ──┤ список допущенных
                                   /tmp/myRPC_XXXXXX.*  ◄──┘ вывод команды
```

1. Клиент определяет имя пользователя, от которого запущен, и отправляет
   серверу запрос в формате JSON.
2. Сервер разбирает запрос, проверяет имя по списку допущенных
   пользователей и, если проверка пройдена, выполняет команду в отдельном
   процессе.
3. Вывод команды собирается во временные файлы и возвращается клиенту:
   код `0` и содержимое `stdout` при успехе, код `1` и содержимое `stderr`
   при ошибке.

## Протокол

Запрос:

```json
{"login":"имя_пользователя","command":"команда bash"}
```

Ответ:

```json
{"code":0,"result":"результат выполнения или описание ошибки"}
```

Специальные символы (кавычки, обратная косая черта, переводы строк,
табуляции) экранируются, поэтому произвольная команда BASH передаётся без
искажений.

## Сборка

```sh
make            # собрать обе программы
make test       # модульные и интеграционные тесты
make deb        # deb-пакеты обеих программ в каталоге build/
make clean      # удалить всё, что создаётся при сборке
```

Требования: `gcc`, `make`, `dpkg-dev`.

## Быстрая проверка на одной машине

```sh
make
printf 'port = 1234\nsocket_type = stream\ndaemon = no\n' > /tmp/myRPC.conf
id -un > /tmp/users.conf
./server/myRPC-server -c /tmp/myRPC.conf -u /tmp/users.conf -f -l /tmp/s.log &
./client/myRPC-client -h 127.0.0.1 -p 1234 -s -c "uname -a"
kill %1
```

## Развёртывание на стенде

Стенд состоит из двух машин Astra Linux в одной сети: на первой
устанавливается `myrpc-client`, на второй — `myrpc-server`.

```sh
# на сервере
sudo dpkg -i myrpc-server_1.0.0_amd64.deb
echo student | sudo tee -a /etc/myRPC/users.conf
sudo systemctl enable --now myRPC-server

# на клиенте
sudo dpkg -i myrpc-client_1.0.0_amd64.deb
myRPC-client -h <адрес сервера> -p 1234 -s -c "hostname"
```

Подробный порядок — в [docs/methodology.md](docs/methodology.md).

## Требования задания и где они выполнены

| Требование | Где |
|---|---|
| ветвление GitFlow | ветви `feature/*`, `release/*`, теги в `master` |
| оформление исходных текстов по GNU Coding Standards | `common/*.c`, `client/*.c`, `server/*.c` |
| разбор аргументов `getopt_long` | `client/myrpc_client.c`, `server/myrpc_server.c` |
| программа-демон, обработка сигналов | `server/myrpc_server.c` |
| конфигурационные файлы, читаемые при запуске | `common/myrpc_config.c`, `server/conf/` |
| список допущенных пользователей | `/etc/myRPC/users.conf` |
| экранирование специальных символов | `common/myrpc_proto.c` |
| временные файлы вывода | `execute_command` в `server/myrpc_server.c` |
| журнал через syslog или файл | `common/myrpc_log.c` |
| Makefile с целями `all`, `clean`, `deb` | `Makefile`, `client/Makefile`, `server/Makefile` |
| deb-пакеты | `client/debian/`, `server/debian/` |
| тесты в формате CI/CD | `tests/`, `.github/workflows/ci.yml`, `.gitlab-ci.yml` |
| описание каждого модуля | `client/README.md`, `server/README.md` |
