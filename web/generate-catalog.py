#!/usr/bin/env python3
"""Build the web card file (картотека перфокарт).

Reads the verified listings in examples/book, writes Russian-language copies
with labelled example inputs to web/programs/, and writes the deck catalog
web/public/catalog.json. Run from any directory: python web/generate-catalog.py
"""
import json
import math
import pathlib
import re
from decimal import Decimal, ROUND_HALF_UP

WEB = pathlib.Path(__file__).resolve().parent
BOOK = WEB.parent / "examples" / "book"
OUT = WEB / "programs"

B58 = "А. И. Китов, Н. А. Криницкий, «Электронные вычислительные машины», 1958"
B59 = "А. И. Китов, Н. А. Криницкий, «Электронные цифровые машины и программирование», 1959"
URL58 = "https://www.computer-museum.ru/books/kitov_evm.pdf"
URL59 = "https://kitov.rea.ru/uploads/files/elect2.pdf"


def fields(w):
    return f"{(w >> 31) & 0o7777:04o} {(w >> 19) & 0o7777:04o} {(w >> 7) & 0o7777:04o} {(w >> 6) & 1} {w & 0o77:02o}"


def bcd(value):
    """Decimal word: sign, nine BCD digits, exponent sign, five exponent bits."""
    v = Decimal(str(value))
    if v == 0:
        return 0
    neg = v < 0
    v = abs(v)
    e = v.adjusted() + 1
    m = int((v.scaleb(-e) * 10**9).quantize(Decimal(1), rounding=ROUND_HALF_UP))
    if m >= 10**9:
        m //= 10
        e += 1
    digits = 0
    for place in range(9):
        digits |= (m % 10) << (place * 4)
        m //= 10
    return (1 << 42 if neg else 0) | (digits << 6) | ((1 << 5) if e < 0 else 0) | abs(e)


def binary(value):
    """Binary floating word: sign, 35-bit mantissa, exponent sign, six exponent bits."""
    if value == 0:
        return 0
    f, e = math.frexp(abs(value))
    m = round(f * 2**35)
    return (1 << 42 if value < 0 else 0) | (m << 7) | ((64 | -e) if e < 0 else e)


def words(name):
    """START line and every word line of a verified book listing."""
    text = (BOOK / name).read_text(encoding="utf-8")
    return [l for l in text.splitlines() if re.match(r"^(START [0-7]{4}|[0-7]{4} [0-7]{4} )", l)]


def write(name, header, body, data_note=None, data=()):
    lines = ["# " + h for h in header] + body
    if data:
        lines.append("# " + data_note)
        for addr, word, note in data:
            lines.append(f"# {addr}: {note}")
            lines.append(f"{addr} {fields(word)}")
    (OUT / name).write_text("\n".join(lines) + "\n", encoding="utf-8")


DECKS = []


def deck(**entry):
    DECKS.append(entry)


OUT.mkdir(exist_ok=True)

deck(id="linear-1958", file="kitov-linear.strela", year=1958, book="1958", pages="с. 43–47",
     title="Решение двух линейных уравнений",
     description="Ax + By = C, Dx + Ey = F при A = 1, B = 2, C = 5, D = 7, E = 5, F = 17. "
                 "Машина находит x ≈ 1 и y ≈ 2 в ячейках 0051 и 0052 и выдаёт их на перфокарту.",
     source=B58 + ", таблица 4, с. 45–46", href=URL58, kind="book")

write("integral.strela", [
    "Определённый интеграл от t^n в пределах от 1 до x (x > 0).",
    "При n ≠ −1 машина считает (x^(n+1) − 1)/(n+1), при n = −1 — ln x.",
    f"Источник: {B59}, § 31, пример 1, с. 155–156.",
    "Исходные данные: 0037 — x, 0040 — n, 0041 — число −1. Ответ: ячейка 0044,",
    "затем он переводится в десятичную запись и выдаётся на перфокарту.",
], words("1959-integral-t-power-n.strela"),
    "Пример исходных данных; в книге значения x и n не заданы.",
    [("0037", bcd(2), "x = 2"), ("0040", bcd(3), "n = 3; ответ (2^4 − 1)/4 = 3,75")])
deck(id="integral-1959", file="programs/integral.strela", year=1959, book="1959", pages="с. 155–156",
     title="Интеграл от tⁿ",
     description="Ветвящаяся программа: при n = −1 вычисляется ln x. Пример: x = 2, n = 3, ответ 3,75 в ячейке 0044.",
     source=B59 + ", § 31, пример 1", href=URL59, kind="book")

write("quadratic.strela", [
    "Квадратное уравнение px^2 + qx + r = 0.",
    f"Источник: {B59}, § 31, пример 2, с. 156–157.",
    "Буквенные адреса заменены числами по распределению памяти из книги:",
    "a = 0020, h = 0040, g = 0045, b = 0050, c = 0054, d = 0060.",
    "Опечатка книги исправлена: команда h+1 имеет код 05, а не 13.",
    "Ответ: 0061 — 2 для действительных корней или 4 для комплексных;",
    "0062 и 0063 — корни или действительная и мнимая части.",
], words("1959-quadratic-equation.strela"),
    "Пример исходных данных; в книге значения p, q и r не заданы.",
    [("0051", bcd(1), "p = 1"), ("0052", bcd(-3), "q = −3"), ("0053", bcd(2), "r = 2; корни 1 и 2")])
deck(id="quadratic-1959", file="programs/quadratic.strela", year=1959, book="1959", pages="с. 156–157",
     title="Квадратное уравнение",
     description="Признак 2 или 4 и два корня — действительные или комплексные. Пример: x² − 3x + 2 = 0, корни 1 и 2.",
     source=B59 + ", § 31, пример 2", href=URL59, kind="book")

write("three-branch.strela", [
    "φ(x) = f(x^2 + x − 1), где f(y) = y^3 − 1 при |y| ≤ 1, 2y − 1 при 1 < |y| ≤ 2,",
    "y^5/8 − 1 при |y| > 2.",
    f"Источник: {B59}, § 32, пример 2, таблица 36, с. 165.",
    "Опечатка таблицы 36 исправлена: в ячейке 0024 первые два адреса",
    "переставлены по оператору P3 со с. 164.",
    "Исходные данные: 0047 — x; 0050, 0051, 0052 — числа 1, 2 и 1/8. Ответ: 0055.",
], words("1959-three-branch-function.strela"),
    "Пример исходного значения; в книге x не задан.",
    [("0047", bcd(1.5), "x = 1,5; y = 2,75, ответ y^5/8 − 1 ≈ 18,6595459")])
deck(id="three-branch-1959", file="programs/three-branch.strela", year=1959, book="1959", pages="с. 163–165",
     title="Функция с тремя ветвями",
     description="Два последовательных разветвления выбирают одну из трёх формул. Пример: x = 1,5, ответ ≈ 18,66 в ячейке 0055.",
     source=B59 + ", § 32, таблица 36", href=URL59, kind="book")

write("iteration.strela", [
    "Корень уравнения x^3/65 + x − 5,001 = 0 методом итераций",
    "x(i+1) = 5,001 − x(i)^3/65 при x0 = 4, пока |x(i+1) − x(i)| ≥ 0,0001.",
    f"Источник: {B59}, § 33, пример 1, с. 166–167.",
    "Буквенные адреса заменены числами по распределению памяти из книги:",
    "a = 0017, b = 0033, c = 0037. Останов в книге напечатан как b+14; это a+14.",
    "Исходные данные из книги стоят в ячейках 0034–0037; число 1/65 округлено",
    "до девяти цифр. Корень ≈ 4,00944774 остаётся в ячейке 0034.",
], words("1959-iteration-cubic.strela"))
deck(id="iteration-1959", file="programs/iteration.strela", year=1959, book="1959", pages="с. 166–167",
     title="Корень уравнения итерациями",
     description="Итерационный цикл с проверкой точности 0,0001. Все исходные числа даны в книге; корень ≈ 4,0094 в ячейке 0034.",
     source=B59 + ", § 33, пример 1", href=URL59, kind="book")

group_src = f"Источник: {B59}, § 28, групповые операции, с. 139."
group_note = "В книге напечатаны только две команды; они размещены с 0010, добавлен останов."
write("group-sum.strela", [
    "Пример 9: сумма чисел из ячеек 0101–0120 записывается в ячейку 0121.",
    group_src, group_note, "Ответ — двоичное число в ячейке 0121.",
], words("1959-group-sum.strela"),
    "Пример исходных данных: двоичные числа 1, 2, …, 16; в книге они не заданы.",
    [(f"{0o101 + i:04o}", binary(i + 1), f"{i + 1}") for i in range(16)])
deck(id="group-sum-1959", file="programs/group-sum.strela", year=1959, book="1959", pages="с. 139",
     title="Групповая операция 34: сумма",
     description="Одна предварительная команда повторяет сложение 16 раз. Пример: 1 + 2 + … + 16 = 136 в ячейке 0121.",
     source=B59 + ", § 28, пример 9", href=URL59, kind="custom")

write("group-add.strela", [
    "Пример 10: к каждому из чисел в ячейках 0050–0077 прибавляется число из 0100.",
    group_src, group_note, "Результаты — двоичные числа на местах исходных.",
], words("1959-group-add-constant.strela"),
    "Пример исходных данных: числа 0, 1, …, 23 и прибавляемое 0,5; в книге они не заданы.",
    [(f"{0o50 + i:04o}", binary(i), f"{i}") for i in range(24)] + [("0100", binary(0.5), "0,5")])
deck(id="group-add-1959", file="programs/group-add.strela", year=1959, book="1959", pages="с. 139",
     title="Групповая операция 35: прибавление",
     description="Изменяются первый и третий адреса исполнительной команды. Пример: к 24 числам прибавляется 0,5.",
     source=B59 + ", § 28, пример 10", href=URL59, kind="custom")

write("group-product.strela", [
    "Пример 11: попарные произведения чисел из ячеек 0100–0177 и 0200–0277",
    "записываются в ячейки 0300–0377.",
    group_src, group_note,
], words("1959-group-pairwise-product.strela"),
    "Пример исходных данных: 1, 2, …, 64 и 64 двойки; в книге они не заданы.",
    [(f"{0o100 + i:04o}", binary(i + 1), f"{i + 1}") for i in range(64)] +
    [(f"{0o200 + i:04o}", binary(2), "2") for i in range(64)])
deck(id="group-product-1959", file="programs/group-product.strela", year=1959, book="1959", pages="с. 139",
     title="Групповая операция 37: произведения",
     description="Все три адреса растут при каждом повторении: 64 произведения в ячейках 0300–0377.",
     source=B59 + ", § 28, пример 11", href=URL59, kind="custom")

deck(id="vostok", file="vostok-orbit.strela", year=1961, book="vostok", pages="расчёт для пульта",
     title="Выведение «Востока-1»",
     description="Новая программа, составленная для этого пульта, а не подлинная программа 1961 года. "
                 "Считает выведение и элементы орбиты и печатает 29 величин.",
     source="Программа проекта, параметры ракеты 8К72К", href="https://ru.djvu.online/file/c5OWKbCfqjM34",
     kind="orbit")

(WEB / "public" / "catalog.json").write_text(
    json.dumps({"decks": DECKS}, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
print(f"{len(DECKS)} decks; listings in {OUT}")
