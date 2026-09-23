# -*- coding: utf-8 -*-
"""Диаграммы подпроекта myRPC: архитектура, алгоритмы клиента и сервера,
обработка сигналов."""

import os
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.patches import FancyBboxPatch, Polygon, Rectangle, FancyArrowPatch

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "diagrams")
os.makedirs(OUT, exist_ok=True)

plt.rcParams["font.family"] = "DejaVu Sans"
plt.rcParams["font.size"] = 9.5

BLUE = "#1f3864"
GREY = "#555555"


def shape(ax, kind, x, y, w, h, text, fc="white", fontsize=9.5):
    if kind == "term":
        p = FancyBboxPatch((x - w / 2, y - h / 2), w, h,
                           boxstyle="round,pad=0,rounding_size=0.22",
                           fc=fc, ec="black", lw=1.1)
    elif kind == "io":
        d = w * 0.12
        p = Polygon([(x - w / 2 + d, y + h / 2), (x + w / 2, y + h / 2),
                     (x + w / 2 - d, y - h / 2), (x - w / 2, y - h / 2)],
                    closed=True, fc=fc, ec="black", lw=1.1)
    elif kind == "dec":
        p = Polygon([(x, y + h / 2), (x + w / 2, y), (x, y - h / 2),
                     (x - w / 2, y)], closed=True, fc=fc, ec="black", lw=1.1)
    else:
        p = Rectangle((x - w / 2, y - h / 2), w, h, fc=fc, ec="black", lw=1.1)
    ax.add_patch(p)
    ax.text(x, y, text, ha="center", va="center", fontsize=fontsize,
            linespacing=1.35)


def arrow(ax, x1, y1, x2, y2, color="black"):
    ax.add_patch(FancyArrowPatch((x1, y1), (x2, y2), arrowstyle="-|>",
                                 mutation_scale=11, lw=1.0, color=color))


def line(ax, pts, color="black"):
    xs, ys = zip(*pts)
    ax.plot(xs, ys, color=color, lw=1.0)


def new_axes(figsize, xlim, ylim):
    fig, ax = plt.subplots(figsize=figsize)
    ax.set_xlim(*xlim)
    ax.set_ylim(*ylim)
    ax.axis("off")
    return fig, ax


# ---------------------------------------------------------------- 1
def diagram_architecture(path):
    fig, ax = new_axes((9.5, 5.6), (0, 10), (0, 6.4))

    shape(ax, "proc", 1.9, 4.6, 3.0, 1.5,
          "myRPC-client\n\n-c «команда»\n-h адрес  -p порт\n-s | -d",
          fc="#e8eef8")
    shape(ax, "proc", 7.6, 4.6, 3.4, 1.5,
          "myRPC-server (демон)\n\nосновной процесс:\nслушает сокет", fc="#e8eef8")

    arrow(ax, 3.45, 4.95, 5.85, 4.95)
    ax.text(4.65, 5.15, '{"login":"student",\n "command":"ls -la"}',
            ha="center", fontsize=8.5, color=BLUE)

    arrow(ax, 5.85, 4.25, 3.45, 4.25)
    ax.text(4.65, 3.85, '{"code":0,"result":"…"}', ha="center",
            fontsize=8.5, color=BLUE)

    # рабочие процессы
    for i, x in enumerate((6.4, 7.6, 8.8)):
        shape(ax, "proc", x, 2.6, 1.1, 0.8,
              "рабочий\nпроцесс", fc="#f2f2f2", fontsize=8)
        arrow(ax, 7.6, 3.85, x, 3.0, color=GREY)

    shape(ax, "io", 2.0, 2.3, 3.2, 1.0,
          "/etc/myRPC/myRPC.conf\n/etc/myRPC/users.conf", fc="#fff7e0")
    arrow(ax, 3.6, 2.3, 5.9, 3.9)
    ax.text(4.6, 2.75, "чтение при запуске\nи по SIGHUP", ha="center",
            fontsize=8, color=GREY)

    shape(ax, "io", 7.6, 1.15, 3.6, 0.8,
          "/tmp/myRPC_XXXXXX.stdout\n/tmp/myRPC_XXXXXX.stderr", fc="#fff7e0")
    for x in (6.4, 7.6, 8.8):
        arrow(ax, x, 2.2, 7.6, 1.55, color=GREY)

    shape(ax, "io", 2.0, 0.9, 3.2, 0.7, "syslog или файл журнала",
          fc="#e9f5e9")
    arrow(ax, 5.9, 4.0, 3.6, 1.15, color=GREY)

    ax.text(5.0, 6.15, "Архитектура myRPC", ha="center", fontsize=12,
            weight="bold")
    fig.savefig(path, dpi=200, bbox_inches="tight")
    plt.close(fig)


# ---------------------------------------------------------------- 2
def diagram_client(path):
    fig, ax = new_axes((5.4, 10.6), (0, 6), (0, 13.2))
    X = 3.0

    shape(ax, "term", X, 12.6, 2.0, 0.6, "Начало")
    arrow(ax, X, 12.3, X, 11.95)
    shape(ax, "io", X, 11.5, 4.2, 0.85,
          "Разбор аргументов\ngetopt_long: -c -h -p -s -d -l")
    arrow(ax, X, 11.07, X, 10.7)
    shape(ax, "dec", X, 10.2, 3.2, 1.1, "команда\nзадана?")
    ax.text(X + 1.75, 10.35, "нет", fontsize=8.5)
    line(ax, [(X + 1.6, 10.2), (5.5, 10.2), (5.5, 1.1)])
    arrow(ax, 5.5, 1.1, X + 1.05, 1.1)
    arrow(ax, X, 9.65, X, 9.3)
    ax.text(X + 0.12, 9.45, "да", fontsize=8.5)

    shape(ax, "proc", X, 8.85, 4.2, 0.85,
          "getpwuid(getuid()):\nимя текущего пользователя")
    arrow(ax, X, 8.42, X, 8.05)
    shape(ax, "proc", X, 7.6, 4.2, 0.85,
          "Формирование запроса JSON,\nэкранирование спецсимволов")
    arrow(ax, X, 7.17, X, 6.8)
    shape(ax, "proc", X, 6.35, 4.2, 0.85,
          "socket() + connect()\nSO_RCVTIMEO = 10 с")
    arrow(ax, X, 5.92, X, 5.55)
    shape(ax, "dec", X, 5.05, 3.2, 1.1, "соединение\nустановлено?")
    ax.text(X - 1.95, 5.2, "нет", fontsize=8.5)
    line(ax, [(X - 1.6, 5.05), (0.6, 5.05), (0.6, 1.1)])
    arrow(ax, 0.6, 1.1, X - 1.05, 1.1)
    arrow(ax, X, 4.5, X, 4.15)
    ax.text(X + 0.12, 4.3, "да", fontsize=8.5)

    shape(ax, "proc", X, 3.7, 4.2, 0.7, "send() запроса, recv() ответа")
    arrow(ax, X, 3.35, X, 3.0)
    shape(ax, "proc", X, 2.55, 4.2, 0.7, "Разбор ответа: code, result")
    arrow(ax, X, 2.2, X, 1.85)
    shape(ax, "dec", X, 1.35, 2.6, 0.9, "code = 0 ?")
    arrow(ax, X, 0.9, X, 0.55)

    shape(ax, "io", X, 1.1, 0.0, 0.0, "")  # заглушка для выравнивания
    shape(ax, "term", X, 0.35, 2.4, 0.55, "Конец")
    ax.text(X + 1.5, 1.6, "result → stdout\n(код 0)", fontsize=8,
            ha="center", color=GREY)
    ax.text(X - 1.5, 1.6, "result → stderr\n(код 1)", fontsize=8,
            ha="center", color=GREY)

    fig.savefig(path, dpi=200, bbox_inches="tight")
    plt.close(fig)


# ---------------------------------------------------------------- 3
def diagram_server(path):
    fig, ax = new_axes((6.6, 11.6), (0, 7.4), (0, 14.4))
    X = 3.3

    shape(ax, "term", X, 13.8, 2.0, 0.55, "Начало")
    arrow(ax, X, 13.53, X, 13.2)
    shape(ax, "io", X, 12.8, 4.6, 0.75,
          "Разбор аргументов\n-c конфиг  -u пользователи  -f  -l")
    arrow(ax, X, 12.43, X, 12.1)
    shape(ax, "proc", X, 11.7, 4.6, 0.75,
          "Чтение myRPC.conf и users.conf")
    arrow(ax, X, 11.33, X, 11.0)
    shape(ax, "dec", X, 10.5, 3.0, 1.0, "daemon = yes ?")
    ax.text(X + 1.65, 10.65, "да", fontsize=8.5)
    line(ax, [(X + 1.5, 10.5), (6.6, 10.5), (6.6, 9.35)])
    arrow(ax, 6.6, 9.35, X + 1.35, 9.35)
    shape(ax, "proc", X, 9.35, 4.6, 0.75,
          "fork, setsid, fork, umask,\nchdir /, /dev/null, PID-файл")
    arrow(ax, X, 10.0, X, 9.75)
    ax.text(X + 0.12, 9.9, "нет", fontsize=8.5)
    arrow(ax, X, 8.98, X, 8.65)

    shape(ax, "proc", X, 8.25, 4.6, 0.75,
          "sigaction: SIGINT, SIGTERM,\nSIGHUP, SIGCHLD (без SA_RESTART)")
    arrow(ax, X, 7.88, X, 7.55)
    shape(ax, "proc", X, 7.15, 4.6, 0.75,
          "socket, bind, listen\n(stream или dgram)")
    arrow(ax, X, 6.78, X, 6.45)

    shape(ax, "dec", X, 5.95, 3.2, 1.0, "получен\nсигнал останова?")
    ax.text(X + 1.75, 6.1, "да", fontsize=8.5)
    line(ax, [(X + 1.6, 5.95), (6.9, 5.95), (6.9, 1.85)])
    arrow(ax, 6.9, 1.85, X + 1.6, 1.85)
    arrow(ax, X, 5.45, X, 5.1)
    ax.text(X + 0.12, 5.25, "нет", fontsize=8.5)

    shape(ax, "dec", X, 4.6, 3.2, 1.0, "получен\nSIGHUP?")
    ax.text(X - 1.95, 4.75, "да", fontsize=8.5)
    line(ax, [(X - 1.6, 4.6), (0.45, 4.6), (0.45, 5.95)])
    arrow(ax, 0.45, 5.95, X - 1.65, 5.95)
    ax.text(0.9, 5.35, "перечитать\nконфигурацию", fontsize=7.5,
            color=GREY, ha="left")
    arrow(ax, X, 4.1, X, 3.75)
    ax.text(X + 0.12, 3.9, "нет", fontsize=8.5)

    shape(ax, "proc", X, 3.35, 4.6, 0.75,
          "accept() / recvfrom()\nожидание запроса")
    arrow(ax, X, 2.98, X, 2.65)
    shape(ax, "proc", X, 2.25, 4.6, 0.75,
          "fork: обслуживание запроса\nв отдельном процессе")
    line(ax, [(X - 2.3, 2.25), (0.45, 2.25), (0.45, 4.6)])

    shape(ax, "proc", X, 1.85, 0.0, 0.0, "")
    shape(ax, "proc", X, 1.2, 4.6, 0.75,
          "Остановка рабочих процессов,\nожидание их завершения")
    arrow(ax, X, 0.82, X, 0.5)
    shape(ax, "term", X, 0.25, 2.2, 0.5, "Конец")

    fig.savefig(path, dpi=200, bbox_inches="tight")
    plt.close(fig)


# ---------------------------------------------------------------- 4
def diagram_worker(path):
    fig, ax = new_axes((5.6, 8.6), (0, 6), (0, 10.6))
    X = 3.0

    shape(ax, "term", X, 10.1, 2.6, 0.55, "Рабочий процесс")
    arrow(ax, X, 9.83, X, 9.5)
    shape(ax, "io", X, 9.1, 4.4, 0.7, "recv(): строка JSON")
    arrow(ax, X, 8.75, X, 8.4)
    shape(ax, "proc", X, 8.0, 4.4, 0.7, "Разбор: login и command")
    arrow(ax, X, 7.65, X, 7.3)
    shape(ax, "dec", X, 6.75, 3.4, 1.1, "login есть\nв users.conf?")
    ax.text(X - 2.05, 6.9, "нет", fontsize=8.5)
    line(ax, [(X - 1.7, 6.75), (0.45, 6.75), (0.45, 2.0)])
    arrow(ax, 0.45, 2.0, X - 2.2, 2.0)
    arrow(ax, X, 6.2, X, 5.85)
    ax.text(X + 0.12, 6.0, "да", fontsize=8.5)

    shape(ax, "proc", X, 5.4, 4.4, 0.9,
          "mkstemps:\n/tmp/myRPC_XXXXXX.stdout\n/tmp/myRPC_XXXXXX.stderr")
    arrow(ax, X, 4.95, X, 4.6)
    shape(ax, "proc", X, 4.2, 4.4, 0.8,
          "fork + execl(/bin/sh -c команда)\ndup2 потоков в файлы")
    arrow(ax, X, 3.8, X, 3.45)
    shape(ax, "dec", X, 2.95, 3.4, 1.0, "код возврата\nравен 0?")
    ax.text(X + 1.85, 3.1, "да", fontsize=8.5)
    ax.text(X - 1.95, 3.1, "нет", fontsize=8.5)

    line(ax, [(X + 1.7, 2.95), (5.5, 2.95), (5.5, 2.0)])
    arrow(ax, 5.5, 2.0, X + 2.2, 2.0)

    shape(ax, "proc", X, 2.0, 4.0, 0.7,
          "Формирование ответа:\ncode = 0 → stdout, code = 1 → stderr")
    arrow(ax, X, 1.65, X, 1.3)
    shape(ax, "proc", X, 0.95, 4.0, 0.6, "send() ответа, удаление файлов")
    arrow(ax, X, 0.65, X, 0.4)
    shape(ax, "term", X, 0.2, 2.2, 0.45, "_exit()")

    fig.savefig(path, dpi=200, bbox_inches="tight")
    plt.close(fig)


if __name__ == "__main__":
    diagram_architecture(os.path.join(OUT, "01_architecture.png"))
    diagram_client(os.path.join(OUT, "02_client_flow.png"))
    diagram_server(os.path.join(OUT, "03_server_flow.png"))
    diagram_worker(os.path.join(OUT, "04_worker_flow.png"))
    print("diagrams written to", OUT)
