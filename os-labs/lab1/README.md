# Лабораторная работа №1. Процессы и каналы

**Вариант 15.** Linux, язык C (C11), компилятор gcc.

## Условие

Родительский процесс создаёт дочерний процесс. Первой строкой пользователь вводит имя файла, который открывается на запись. Родитель принимает строки произвольной длины и пересылает их ребёнку через `pipe1`.

Дочерний процесс проверяет каждую строку: **строка должна начинаться с заглавной буквы**.

- строка подходит → выводится в стандартный поток вывода ребёнка, который перенаправлен в файл;
- не подходит → в `pipe2` отправляется сообщение об ошибке, и родитель печатает его в свой стандартный вывод.

Родительский и дочерний процессы — разные программы.

```
User ──stdin──► parent ──pipe1──► child(stdin)
                  ▲                  │ stdout ──► File
                  └──────pipe2───────┘ stderr
```

## Структура

| Путь | Назначение |
|------|------------|
| `src/parent.c` | Родитель: имя файла, каналы, `fork` + `execv`, пересылка данных через `poll`, `waitpid` |
| `src/child.c` | Ребёнок: читает строки из stdin, корректные пишет в stdout, ошибки в stderr |
| `src/common.h` | Вывод через `write` вместо `stdio.h` |
| `tests/input.txt` | Пример входных данных |
| `report/` | Отчёт в PDF |

## Сборка и запуск

Нужны `gcc` и `make` (`sudo apt install build-essential`).

```bash
make                       # собрать parent и child
./parent                   # первая строка — имя файла, дальше строки, Ctrl+D — конец ввода
cat result.txt             # строки, прошедшие проверку
make clean                 # удалить собранные программы
```

Ввод из файла:

```bash
./parent < tests/input.txt
```

## Пример

```
$ ./parent
result.txt
Hello world
hello world
Error: line "hello world" does not start with a capital letter
123 abc
Error: line "123 abc" does not start with a capital letter
Ok.
^D
$ cat result.txt
Hello world
Ok.
```

## Использованные системные вызовы

`fork`, `execv`, `pipe`, `dup2`, `open`, `close`, `read`, `write`, `poll`, `waitpid`, `readlink`, `signal`, `_exit`.
