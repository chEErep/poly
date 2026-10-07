# БСИТ: команды и сценарий сдачи

Короткая оперативная шпаргалка. Теория и все функции — в [defense-from-zero.md](<D:/5Сем/poly/semester-5/information-security/lab-01/docs/defense-from-zero.md>)

Хост = основная Windows. VM = Windows внутри VirtualBox. Команды PowerShell вводи в указанной машине, а пункты меню — в окне нашего клиента. В меню пути вводятся БЕЗ кавычек

## 1. Перед сдачей

- Закрой клиенты пунктом0; сервер останови Ctrl+C в его окне
- Собери актуальные exe
- Перенеси актуальный сервер в VM, сравни SHA256
- Проверь host-only IP: ранее хост192.168.56.1, VM192.168.56.101
- Открой два сервера и нужные окна клиентов
- Подготовь Wireshark, отдельную тестовую папку и флешку либо заранее обсуди её отсутствие
- Не меняй ACL системных папок и не отключай весь firewall

### Сборка — на хосте

```powershell
Set-Location 'D:\5Сем\poly\semester-5\information-security\lab-01'
Set-ExecutionPolicy -Scope Process -ExecutionPolicy Bypass -Force
.\build_stage1.ps1
```

Ожидаем 2/2 tests passed. При failed сборка не успешна: exe может остаться старым

```powershell
Get-FileHash '.\build-ok\lab_server.exe' -Algorithm SHA256
Get-Item '.\build-ok\lab_server.exe' | Select-Object FullName,Length,LastWriteTime
```

Скопируй этот exe в VM в C:\BSIT\Lab1, предварительно остановив там старый сервер. В VM:

```powershell
Set-Location 'C:\BSIT\Lab1'
Get-FileHash '.\lab_server.exe' -Algorithm SHA256
```

Хэш равен хостовому. 2 workers в VM и 16 на хосте — нормально, это не версия программы

## 2. Запуск: держи окна открытыми

### VM, окно1: сервер1

```powershell
Set-Location 'C:\BSIT\Lab1'
.\lab_server.exe
```

Ожидаем Listening on 0.0.0.0:27015. Не вводи команды в это окно, пока сервер работает

### Хост, окно1: сервер2

```powershell
Set-Location 'D:\5Сем\poly\semester-5\information-security\lab-01'
.\build-ok\lab_server.exe
```

### Хост, окно2: клиентA к VM

```powershell
Set-Location 'D:\5Сем\poly\semester-5\information-security\lab-01'
.\build-ok\lab_client.exe 192.168.56.101
```

### Хост, окно3: клиентB к той же VM

```powershell
Set-Location 'D:\5Сем\poly\semester-5\information-security\lab-01'
.\build-ok\lab_client.exe 192.168.56.101
```

### Хост, окно4: клиентC к серверу2

```powershell
Set-Location 'D:\5Сем\poly\semester-5\information-security\lab-01'
.\build-ok\lab_client.exe 127.0.0.1
```

В A и B по очереди запроси4, не закрывая ни один. На сервере1 покажи два peer с разными портами. В A и C запроси1 и покажи разные ОС. Один процесс клиента работает с одним сервером; мы показываем несколько процессов

## 3. Что выбирать по просьбе преподавателя

| Просьба | В меню | Что вводить после Path on server |
| --- | --- | --- |
| ОС | 1 | Ничего |
| Текущее время | 2 | Ничего |
| Uptime | 3 | Ничего |
| Память | 4 | Ничего |
| Типы дисков и filesystem | 5 | Ничего |
| Свободное место | 6 | Ничего |
| Владелец папки | 7 | C:\Windows |
| Права папки | 8 | C:\Windows |
| Владелец ключа | 7 | reg:CURRENT_USER\Software |
| Права ключа | 8 | reg:CURRENT_USER\Software |
| Завершить | 0 | Ничего |

Путь относится к машине СЕРВЕРА. Проверить ошибку: пункт7 → `C:\BSIT-NoSuchFile-12345`, затем пункт5. Ожидаем error с win32_error=2, а следующий запрос успешен

Если просят «отправить пакет GET_OS», выбираешь1: клиент сам формирует и шифрует запрос. Wireshark — просмотр, не отправка нашего запроса

## 4. Wireshark: обязательно начать ДО нового подключения

1. Старый клиентA заверши0. Сервер1 оставь запущенным
2. На хосте открой Wireshark. Выбери VirtualBox Host-Only интерфейс, ранее называвшийся Ethernet, и начни захват двойным щелчком
3. В верхнюю строку display filter вставь:

```text
tcp.port == 27015 && ip.addr == 192.168.56.101
```

4. Снова запусти клиентA к VM. Дождись Key exchange complete
5. В клиенте выбери1, получи ОС
6. Останови capture красным квадратом
7. Покажи SYN/SYN-ACK/ACK, затем BSI1 type100 и type101
8. Правая кнопка по TCP-пакету → Follow → TCP Stream. Для байтов выбери Hex Dump; если смотришь отдельный пакет — раскрывай Data
9. Сохрани capture через File → Save As в pcapng

Что объяснять: public BLOB276 байт — публичный RSA-ключ клиента; wrapped BLOB268 — защищённый AES-ключ. После этого полезные данные находятся в ciphertext; внешний BSI1 и IV открыты. ACK без Data не является ответом ОС. Length всего сетевого кадра не равен payload_len нашего протокола

Старый Wireshark.pcapng можно оставить как дополнительное свидетельство, но преподавателю показываем новый живой обмен

## 5. Права доступа: before → изменение → after → восстановление

Тест только на отдельной папке VM. Для новой репетиции используем `C:\BSIT\Defense-ACL-Demo`, чтобы не спутать её с ранее изменённой ACL-Demo. Открой отдельный PowerShell VM **от имени администратора**: меню Пуск → PowerShell → правой кнопкой → Запуск от имени администратора. Не останавливай сервер для смены DACL

Это важно для /restore: при проверке в неповышенном окне команда выдала ошибку отсутствующих привилегий. Успешный /grant не гарантирует, что /restore разрешён в том же обычном окне

### Создать папку и сохранить исходную DACL — в VM

```powershell
New-Item -ItemType Directory -Path 'C:\BSIT\Defense-ACL-Demo' -Force
Set-Location 'C:\BSIT'
if (Test-Path -LiteralPath '.\Defense-ACL-Demo-before.acl') {
    throw 'Baseline already exists: do not overwrite it; check previous restore first'
}
icacls.exe '.\Defense-ACL-Demo' /save '.\Defense-ACL-Demo-before.acl'
if ($LASTEXITCODE -ne 0) { throw 'Saving baseline failed' }
icacls.exe '.\Defense-ACL-Demo'
```

Baseline-файл не перезаписываем после изменения. Если он уже существует, выясни состояние прежнего испытания, а не просто удаляй резервную копию. Обычный вывод icacls позволяет заранее заметить уже добавленную явную ACE Users; повторный grant тех же прав может не увеличить count

### Before — клиентA на хосте, подключённый к VM

```text
8
C:\BSIT\Defense-ACL-Demo
```

Запиши ace_count и исходные ACE. Для сервера2 в клиентC отдельно запроси8 → C:\Windows

### Изменить — PowerShell VM

```powershell
icacls.exe 'C:\BSIT\Defense-ACL-Demo' /grant '*S-1-5-32-545:M'
if ($LASTEXITCODE -ne 0) { throw 'Changing test DACL failed' }
```

Это разрешение Modify для встроенной группы Users. * у числового SID — синтаксис icacls. Нет (OI)/(CI): показываем явное правило на саму папку, не рекурсивную смену прав

### After — тот же клиентA, без нового подключения

```text
8
C:\BSIT\Defense-ACL-Demo
```

Ищи SID S-1-5-32-545, allow, mask0x001301BF, inherited=no. Обычно count увеличится на1, если такого явного правила до этого не было. Индексы других ACE могут сдвинуться. Сравнение по SID/type/mask/flags, а не только ace_0/ace_5

### Восстановить — PowerShell VM

```powershell
icacls.exe 'C:\BSIT' /restore 'C:\BSIT\Defense-ACL-Demo-before.acl'
if ($LASTEXITCODE -ne 0) { throw 'Restoring test DACL failed' }
icacls.exe 'C:\BSIT\Defense-ACL-Demo'
```

Запроси8 ещё раз и сравни с before. /save сохраняет DACL, не весь security descriptor; в этом испытании owner не изменялся. Если restore сообщил ошибку отсутствующих привилегий, проверь, что это повышенное окно VM, и повтори только восстановление. При другой ошибке остановись и разберись, не добавляй новые правила поверх

## 6. Диски и флешка

КлиентA, пункт6 → сохранить исходный список. Физическую флешку подключить к хосту и при необходимости передать VM через Устройства → USB → нужное устройство. Дождаться буквы в проводнике VM. Снова пункт6 → новая буква/free_bytes. Пункт5 покажет type/filesystem

Не форматируй флешку, не меняй её файлы. Не передавай VM устройство, на которое хост сейчас пишет. Если флешки нет, этот пункт честно не подтверждён: одолжи или согласуй с преподавателем замену. Добавление fixed-VHD не равно проверке флешки

## 7. Команды диагностики: что каждая проверяет

Текущая папка / есть ли exe — на хосте:

```powershell
Get-Location
Test-Path '.\build-ok\lab_client.exe'
```

Какие процессы реально запущены — на соответствующей машине:

```powershell
Get-Process lab_server,lab_client -ErrorAction SilentlyContinue | Select-Object Id,Path
Get-NetTCPConnection -LocalPort 27015 -State Listen -ErrorAction SilentlyContinue
```

В VM узнать IP:

```powershell
ipconfig
Get-NetIPAddress -AddressFamily IPv4 | Select-Object InterfaceAlias,IPAddress
```

На хосте узнать адреса и TCP-доступность VM:

```powershell
Get-NetIPAddress -AddressFamily IPv4 | Select-Object InterfaceAlias,IPAddress
Test-NetConnection 192.168.56.101 -Port 27015
```

TcpTestSucceeded=True подтверждает только установление TCP, не CryptoAPI и не ответ приложения. Этот тест может дать connect/disconnect в логе сервера без request — нормально

Локально в VM проверить слушатель:

```powershell
Test-NetConnection 127.0.0.1 -Port 27015
```

Посмотреть реальные права Windows в VM:

```powershell
(Get-Acl -LiteralPath 'C:\BSIT\Defense-ACL-Demo').Owner
(Get-Acl -LiteralPath 'C:\BSIT\Defense-ACL-Demo').Access | Format-List
(Get-Acl -LiteralPath 'Registry::HKEY_CURRENT_USER\Software').Owner
```

Если нет inbound-правила для нашей VM и этот порт действительно требуется, в повышенном PowerShell VM можно создать узкое правило для host-only подсети. Не создавай дубликат, сначала посмотри:

```powershell
Get-NetFirewallRule -DisplayName 'BSIT-Lab1-HostOnly-27015' -ErrorAction SilentlyContinue |
    Select-Object DisplayName,Enabled,Direction,Action
```

Только если правила нет и IP подтверждены:

```powershell
New-NetFirewallRule -DisplayName 'BSIT-Lab1-HostOnly-27015' -Direction Inbound -Action Allow -Protocol TCP -LocalPort 27015 -LocalAddress 192.168.56.101 -RemoteAddress 192.168.56.0/24 -Profile Any
```

При наличии явного block-правила дополнительное allow не всегда решает проблему. Не удаляй/не отключай чужие правила без понимания их назначения. Новый узкий rule можно убрать по его конкретному DisplayName, если он больше не нужен; не отключай firewall целиком

## 8. Быстрые аварийные действия

- Серверная консоль «замёрзла», в заголовке Select/Выбрать → Esc, затем убери QuickEdit в свойствах старой консоли
- Build Permission denied → клиент0 и серверCtrl+C, затем повторить сборку
- Script disabled → process-scope Bypass в том же окне
- Cannot connect10061 → есть ли сервер и тот ли IP/порт
- Cannot connect10060 → host-only и правила firewall
- error2 → объект не существует на стороне сервера
- error5 → недостаточно прав у сервера; не менять системную ACL, выбрать тестовую папку
- Нет exchange в capture → capture должен начаться раньше нового connect
- Нечитаемое имя → проверь актуальность UTF-8-сборки и перезапусти клиент; не менять AES из-за кодировки текста

## 9. На что показать в коде, если спросят

| Вопрос | Файл и имя для поиска |
| --- | --- |
| Где номер запроса/формат | protocol.h: lab_request_type, lab_header |
| Где сериализация | protocol.c: lab_encode_header / lab_decode_header |
| Кто отправляет пункт меню | client.c: main → perform_request |
| Кто подключается | client.c: connect_to_server |
| Где RSA-обмен | client.c: perform_key_exchange; server.c: prepare_key_exchange_response |
| Где IV и AES | crypto.c: lab_crypto_encrypt_message / decrypt_message |
| Где IOCP | server.c: main, worker_thread, post_receive/post_send |
| Как собираются части | server.c: complete_receive / complete_send |
| Откуда память | system_info.c: build_memory |
| Откуда ACL | system_info.c: build_acl → append_acl_ace |
| Откуда имя владельца | system_info.c: build_owner |
| Имена битов реестра/файла | system_info.c: registry_right_name / file_right_name |

Не обещай реализованные AcceptEx, authenticated channel, нагрузочный benchmark и проверку флешки без устройства. Если не знаешь ответ, лучше найти конкретную функцию и разобрать её, чем придумывать

## 10. Контроль перед выходом в вуз

- [ ] Сам собрал и запустил актуальный exe
- [ ] Перенёс сервер в VM и сверил хэш
- [ ] Все пункты1–8 работают на VM
- [ ] Проверены файл/папка и ключ реестра
- [ ] Error не убивает следующее корректное действие
- [ ] Два сервера и два клиента к одному серверу одновременно работают
- [ ] Могу заново снять обмен ключами и GET_OS в Wireshark
- [ ] Могу изменить и восстановить DACL тестовой папки
- [ ] Флешка есть / отсутствие и замена согласованы
- [ ] Могу своими словами объяснить полный GET_MEMORY, IOCP, SID/ACE/mask и RSA→AES

Отчёт следующий этап, после репетиции и уточнения требований. Эта шпаргалка не заменяет понимание, но позволяет не потеряться в окнах и командах
