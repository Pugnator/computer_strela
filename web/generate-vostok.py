#!/usr/bin/env python3
"""Составляет восьмеричную программу выведения корабля «Восток»."""

import math
from pathlib import Path

addresses = {}
constants = {}
results = [
    "HP", "HA", "A", "E", "P", "B", "PERIOD", "INCLINATION",
    "VP", "VA", "R_FINAL", "HEIGHT_FINAL", "VR_FINAL", "VT_FINAL",
    "SPEED_FINAL", "MASS_FINAL", "HEIGHT_119", "HEIGHT_300",
    "ASCENT_PATH", "ENERGY", "MOMENT", "PATH_ANGLE", "MEAN_MOTION",
    "RP", "RA", "NODE", "ARG_PERIGEE", "MEAN_ANOMALY", "TRUE_ANOMALY",
]
for index, name in enumerate(results):
    addresses[name] = 0o1000 + index
scratch_names = [
    "R_CUR", "VR", "VT", "MASS", "THETA", "Q", "ALT", "INV_R",
    "VREL", "VROT", "SPEED", "INV_M", "THRUST", "ACC", "SIN", "COS",
    "DRAG", "AR", "AT", "TMP1", "TMP2", "TMP3", "COUNTER",
    "PATH", "LAMBDA", "COS_I", "INV_A", "E2", "INCL_RAD",
    "SIN_LAT", "SIN_AZ", "COS_AZ", "U_END", "F_RAD", "ECC_ANOM",
]
for index, name in enumerate(scratch_names):
    addresses[name] = 0o1400 + index
addresses["BRANCH"] = 0o1777

def const(name, value):
    addresses[name] = 0o1200 + len(constants)
    constants[name] = float(value)

deg = math.radians
pitch1, pitch2, pitch3 = 50.21, 101.90, 69.36
const("R", 6371000)
const("MU", 3.986004418e14)
const("R_KM", 6371)
const("MU_KM", 398600.4418)
const("KM_FACTOR", 0.001)
const("ZERO", 0)
const("ONE", 1)
const("TWO", 2)
const("HALF", 0.5)
const("PI", math.pi)
const("PI_HALF", math.pi / 2)
const("TWO_PI", 2 * math.pi)
const("RAD_TO_DEG", 180 / math.pi)
const("LATITUDE", deg(45.92))
const("AZIMUTH", deg(37.5))
const("LONGITUDE", deg(63.342))
const("EARTH_EQUATOR_SPEED", 465.1)
const("START_MASS", 287000)
const("NEG_INV_ATMOSPHERE_HEIGHT", -1 / 8500)
const("DRAG_K", 0.5 * 1.225 * 3.0)
const("THRUST1_VAC", 4 * 995000 + 941000)
const("THRUST1_DIFF", 4 * (995000 - 819000) + (941000 - 744000))
const("FLOW1", 4 * 39500 / 119 + 93400 / 300)
const("PITCH1_MID", deg(pitch1 / 119 / 2))
const("PITCH1_STEP", deg(pitch1 / 119))
const("COUNT1", 118)
const("DROP1", 15000)
const("THRUST2_VAC", 941000)
const("THRUST2_DIFF", 941000 - 744000)
const("FLOW2", 93400 / 300)
const("PITCH2_MID", deg(pitch1 + (pitch2 - pitch1) / 181 / 2))
const("PITCH2_STEP", deg((pitch2 - pitch1) / 181))
const("COUNT2A", 36)
const("COUNT2B", 143)
const("FAIRING", 650)
const("DROP2", 7400)
const("THRUST3_VAC", 54500)
const("THRUST3_DIFF", 0)
const("FLOW3", 6390 / 366)
const("PITCH3_MID", deg(pitch2 + (pitch3 - pitch2) / 366 / 2))
const("PITCH3_STEP", deg((pitch3 - pitch2) / 366))
const("COUNT3", 365)
const("DROP3", 1435)

commands = []
labels = {}
def emit(op, a="ZERO", b="ZERO", c="ZERO"):
    commands.append((op, a, b, c))
def mark(name):
    labels[name] = 0o20 + len(commands)
def jump(ordinary, negative):
    emit(0o20, ordinary, negative, "BRANCH")
def add(a, b, c):
    emit(0o01, a, b, c)
def sub(a, b, c):
    emit(0o03, a, b, c)
def neg(a, c):
    emit(0o04, "ZERO", a, c)
def mul(a, b, c):
    emit(0o05, a, b, c)
def fun(op, a, c):
    emit(op, a, 0, c)
def copy(a, c):
    add(a, "ZERO", c)

# Широта и азимут определяют наклонение плоскости и начальную скорость.
add("LATITUDE", "PI_HALF", "TMP1")
fun(0o67, "TMP1", "TMP1")
fun(0o67, "AZIMUTH", "SIN_AZ")
fun(0o67, "LATITUDE", "SIN_LAT")
add("AZIMUTH", "PI_HALF", "TMP2")
fun(0o67, "TMP2", "COS_AZ")
mul("TMP1", "SIN_AZ", "COS_I")
mul("EARTH_EQUATOR_SPEED", "COS_I", "VROT")
fun(0o74, "COS_I", "TMP1")
sub("PI_HALF", "TMP1", "INCL_RAD")
mul("INCL_RAD", "RAD_TO_DEG", "INCLINATION")
copy("R", "R_CUR")
copy("ZERO", "VR")
copy("VROT", "VT")
copy("START_MASS", "MASS")
copy("ZERO", "PATH")
copy("ZERO", "LAMBDA")

def integration_segment(tag, thrust_vac, thrust_diff, flow, angle_initial, angle_step, count):
    copy(angle_initial, "THETA")
    copy(count, "COUNTER")
    mark(tag)
    sub("R_CUR", "R", "ALT")
    mul("ALT", "NEG_INV_ATMOSPHERE_HEIGHT", "TMP1")
    fun(0o64, "TMP1", "Q")
    mul(thrust_diff, "Q", "TMP1")
    sub(thrust_vac, "TMP1", "THRUST")
    fun(0o62, "R_CUR", "INV_R")
    fun(0o62, "MASS", "INV_M")
    mul("THRUST", "INV_M", "ACC")
    add("THETA", "PI_HALF", "TMP1")
    fun(0o67, "TMP1", "COS")
    fun(0o67, "THETA", "SIN")
    mul("ACC", "COS", "AR")
    mul("ACC", "SIN", "AT")
    mul("INV_R", "INV_R", "TMP1")
    mul("MU", "TMP1", "TMP1")
    sub("AR", "TMP1", "AR")
    mul("VT", "VT", "TMP1")
    mul("TMP1", "INV_R", "TMP1")
    add("AR", "TMP1", "AR")
    mul("VR", "VT", "TMP1")
    mul("TMP1", "INV_R", "TMP1")
    sub("AT", "TMP1", "AT")
    mul("VROT", "R", "TMP1")
    mul("TMP1", "INV_R", "TMP1")
    sub("VT", "TMP1", "VREL")
    mul("VR", "VR", "TMP1")
    mul("VREL", "VREL", "TMP2")
    add("TMP1", "TMP2", "TMP1")
    fun(0o63, "TMP1", "SPEED")
    mul("DRAG_K", "Q", "TMP1")
    mul("TMP1", "SPEED", "TMP1")
    mul("TMP1", "INV_M", "DRAG")
    mul("DRAG", "VR", "TMP1")
    sub("AR", "TMP1", "AR")
    mul("DRAG", "VREL", "TMP1")
    sub("AT", "TMP1", "AT")
    add("VR", "AR", "VR")
    add("VT", "AT", "VT")
    add("R_CUR", "VR", "R_CUR")
    sub("MASS", flow, "MASS")
    add("THETA", angle_step, "THETA")
    mul("VR", "VR", "TMP1")
    mul("VT", "VT", "TMP2")
    add("TMP1", "TMP2", "TMP1")
    fun(0o63, "TMP1", "TMP1")
    add("PATH", "TMP1", "PATH")
    mul("VT", "INV_R", "TMP1")
    add("LAMBDA", "TMP1", "LAMBDA")
    sub("COUNTER", "ONE", "COUNTER")
    jump(tag, tag + "_end")
    mark(tag + "_end")

integration_segment("FIRST", "THRUST1_VAC", "THRUST1_DIFF",
                    "FLOW1", "PITCH1_MID", "PITCH1_STEP", "COUNT1")
sub("R_CUR", "R", "HEIGHT_119")
mul("HEIGHT_119", "KM_FACTOR", "HEIGHT_119")
sub("MASS", "DROP1", "MASS")

# Вторая ступень работает непрерывно; после 37 секунд сбрасывается обтекатель.
copy("PITCH2_MID", "THETA")
def second_part(tag, count):
    copy(count, "COUNTER")
    mark(tag)
    sub("R_CUR", "R", "ALT")
    mul("ALT", "NEG_INV_ATMOSPHERE_HEIGHT", "TMP1")
    fun(0o64, "TMP1", "Q")
    mul("THRUST2_DIFF", "Q", "TMP1")
    sub("THRUST2_VAC", "TMP1", "THRUST")
    fun(0o62, "R_CUR", "INV_R")
    fun(0o62, "MASS", "INV_M")
    mul("THRUST", "INV_M", "ACC")
    add("THETA", "PI_HALF", "TMP1")
    fun(0o67, "TMP1", "COS")
    fun(0o67, "THETA", "SIN")
    mul("ACC", "COS", "AR")
    mul("ACC", "SIN", "AT")
    mul("INV_R", "INV_R", "TMP1")
    mul("MU", "TMP1", "TMP1")
    sub("AR", "TMP1", "AR")
    mul("VT", "VT", "TMP1")
    mul("TMP1", "INV_R", "TMP1")
    add("AR", "TMP1", "AR")
    mul("VR", "VT", "TMP1")
    mul("TMP1", "INV_R", "TMP1")
    sub("AT", "TMP1", "AT")
    mul("VROT", "R", "TMP1")
    mul("TMP1", "INV_R", "TMP1")
    sub("VT", "TMP1", "VREL")
    mul("VR", "VR", "TMP1")
    mul("VREL", "VREL", "TMP2")
    add("TMP1", "TMP2", "TMP1")
    fun(0o63, "TMP1", "SPEED")
    mul("DRAG_K", "Q", "TMP1")
    mul("TMP1", "SPEED", "TMP1")
    mul("TMP1", "INV_M", "DRAG")
    mul("DRAG", "VR", "TMP1")
    sub("AR", "TMP1", "AR")
    mul("DRAG", "VREL", "TMP1")
    sub("AT", "TMP1", "AT")
    add("VR", "AR", "VR")
    add("VT", "AT", "VT")
    add("R_CUR", "VR", "R_CUR")
    sub("MASS", "FLOW2", "MASS")
    add("THETA", "PITCH2_STEP", "THETA")
    mul("VR", "VR", "TMP1")
    mul("VT", "VT", "TMP2")
    add("TMP1", "TMP2", "TMP1")
    fun(0o63, "TMP1", "TMP1")
    add("PATH", "TMP1", "PATH")
    mul("VT", "INV_R", "TMP1")
    add("LAMBDA", "TMP1", "LAMBDA")
    sub("COUNTER", "ONE", "COUNTER")
    jump(tag, tag + "_end")
    mark(tag + "_end")
second_part("SECOND_A", "COUNT2A")
sub("MASS", "FAIRING", "MASS")
second_part("SECOND_B", "COUNT2B")
sub("R_CUR", "R", "HEIGHT_300")
mul("HEIGHT_300", "KM_FACTOR", "HEIGHT_300")
sub("MASS", "DROP2", "MASS")
integration_segment("THIRD", "THRUST3_VAC", "THRUST3_DIFF",
                    "FLOW3", "PITCH3_MID", "PITCH3_STEP", "COUNT3")
sub("MASS", "DROP3", "MASS")

# Из радиуса и составляющих скорости получаются элементы орбиты.
mul("R_CUR", "KM_FACTOR", "R_CUR")
mul("VR", "KM_FACTOR", "VR")
mul("VT", "KM_FACTOR", "VT")
mul("PATH", "KM_FACTOR", "PATH")
copy("R_CUR", "R_FINAL")
sub("R_CUR", "R_KM", "HEIGHT_FINAL")
copy("VR", "VR_FINAL")
copy("VT", "VT_FINAL")
copy("MASS", "MASS_FINAL")
copy("PATH", "ASCENT_PATH")
fun(0o62, "R_CUR", "INV_R")
mul("VR", "VR", "TMP1")
mul("VT", "VT", "TMP2")
add("TMP1", "TMP2", "TMP1")
fun(0o63, "TMP1", "SPEED_FINAL")
mul("TMP1", "HALF", "TMP1")
mul("MU_KM", "INV_R", "TMP2")
sub("TMP1", "TMP2", "ENERGY")
mul("R_CUR", "VT", "MOMENT")
mul("MOMENT", "MOMENT", "TMP1")
fun(0o62, "MU_KM", "TMP2")
mul("TMP1", "TMP2", "P")
mul("TWO", "ENERGY", "TMP1")
fun(0o62, "TMP1", "TMP1")
mul("MU_KM", "TMP1", "TMP1")
emit(0o04, "TMP1", "ZERO", "A")
fun(0o62, "A", "INV_A")
mul("P", "INV_A", "TMP1")
sub("ONE", "TMP1", "E2")
fun(0o63, "E2", "E")
sub("ONE", "E", "TMP1")
mul("A", "TMP1", "RP")
add("ONE", "E", "TMP1")
mul("A", "TMP1", "RA")
sub("RP", "R_KM", "HP")
sub("RA", "R_KM", "HA")
mul("A", "P", "TMP1")
fun(0o63, "TMP1", "B")
mul("A", "A", "TMP1")
mul("TMP1", "A", "TMP1")
mul("TMP1", "TMP2", "TMP1")
fun(0o63, "TMP1", "TMP1")
mul("TWO", "PI", "TMP3")
mul("TMP3", "TMP1", "PERIOD")
fun(0o62, "PERIOD", "TMP1")
mul("TMP3", "TMP1", "MEAN_MOTION")
for radius, result in [("RP", "VP"), ("RA", "VA")]:
    fun(0o62, radius, "TMP1")
    mul("TWO", "TMP1", "TMP1")
    sub("TMP1", "INV_A", "TMP1")
    mul("MU_KM", "TMP1", "TMP1")
    fun(0o63, "TMP1", result)
fun(0o62, "VT", "TMP1")
mul("VR", "TMP1", "TMP1")
fun(0o73, "TMP1", "TMP1")
mul("TMP1", "RAD_TO_DEG", "PATH_ANGLE")

# Узел отсчитывается от меридиана старта в момент пуска.
mul("SIN_LAT", "SIN_AZ", "TMP1")
fun(0o62, "COS_AZ", "TMP2")
mul("TMP1", "TMP2", "TMP1")
fun(0o73, "TMP1", "TMP1")
sub("LONGITUDE", "TMP1", "TMP1")
mul("TMP1", "RAD_TO_DEG", "NODE")
fun(0o67, "INCL_RAD", "TMP1")
fun(0o62, "TMP1", "TMP1")
mul("SIN_LAT", "TMP1", "TMP1")
fun(0o74, "TMP1", "TMP1")
add("TMP1", "LAMBDA", "U_END")

# Истинная аномалия и направление перигея.
mul("P", "INV_R", "TMP1")
sub("TMP1", "ONE", "TMP1")
fun(0o62, "E", "TMP2")
mul("TMP1", "TMP2", "TMP1")
fun(0o74, "TMP1", "TMP1")
sub("PI_HALF", "TMP1", "TMP1")
sub("VR", "ZERO", "TMP2")
jump("TRUE_POS", "TRUE_NEG")
mark("TRUE_POS")
copy("TMP1", "F_RAD")
jump("TRUE_END", "TRUE_END")
mark("TRUE_NEG")
sub("TWO_PI", "TMP1", "F_RAD")
mark("TRUE_END")
mul("F_RAD", "RAD_TO_DEG", "TRUE_ANOMALY")
sub("U_END", "F_RAD", "TMP1")
sub("TMP1", "ZERO", "TMP2")
jump("OMEGA_OK", "OMEGA_WRAP")
mark("OMEGA_WRAP")
add("TMP1", "TWO_PI", "TMP1")
mark("OMEGA_OK")
mul("TMP1", "RAD_TO_DEG", "ARG_PERIGEE")

# Средняя аномалия к моменту отделения корабля.
mul("R_CUR", "INV_A", "TMP1")
sub("ONE", "TMP1", "TMP1")
fun(0o62, "E", "TMP2")
mul("TMP1", "TMP2", "TMP1")
fun(0o74, "TMP1", "TMP1")
sub("PI_HALF", "TMP1", "TMP1")
sub("VR", "ZERO", "TMP2")
jump("ECC_POS", "ECC_NEG")
mark("ECC_POS")
copy("TMP1", "ECC_ANOM")
jump("ECC_END", "ECC_END")
mark("ECC_NEG")
sub("TWO_PI", "TMP1", "ECC_ANOM")
mark("ECC_END")
fun(0o67, "ECC_ANOM", "TMP1")
mul("E", "TMP1", "TMP1")
sub("ECC_ANOM", "TMP1", "TMP1")
mul("TMP1", "RAD_TO_DEG", "MEAN_ANOMALY")
emit(0o70, "HP", len(results) - 1, 0o1100)
emit(0o44, 0o1100, len(results) - 1, 0)
emit(0o40, "HP", "HA", 0)

def as_word(value):
    if value == 0:
        return 0
    fraction, exponent = math.frexp(abs(value))
    mantissa = math.floor(math.ldexp(fraction, 35) + 0.5)
    if mantissa == 1 << 35:
        mantissa >>= 1
        exponent += 1
    assert -63 <= exponent <= 63
    return ((1 << 42) if value < 0 else 0) | (mantissa << 7) | (
        (1 << 6) if exponent < 0 else 0) | abs(exponent)

def field(n):
    return f"{n:04o}"

def data_line(address, value):
    word = as_word(value)
    return (f"{field(address)} {field((word >> 31) & 0o7777)} "
            f"{field((word >> 19) & 0o7777)} "
            f"{field((word >> 7) & 0o7777)} {(word >> 6) & 1} "
            f"{word & 0o77:02o}")

def resolve(value):
    if isinstance(value, str):
        return labels.get(value, addresses.get(value))
    return value

lines = [
    "# Расчёт выведения корабля «Восток». Программа составлена для этого пульта.",
    "# Это не подлинная программа 1961 года.",
    "# Стартовая масса 287 т. Четыре боковых блока: тяга 819/995 кН,",
    "# отделение через 119 с. Центральный блок: 744/941 кН, до 300 с.",
    "# Блок Е: 54,5 кН, до 666 с. Обтекатель отделяется через 156 с.",
    "# Высоты орбиты НЕ заданы: их вычисляют по скорости после выведения.",
    "# Углы направления тяги 50,21; 101,90; 69,36 градуса подобраны",
    "# по опубликованной орбите; подлинная циклограмма не найдена.",
    "# Плоская модель учитывает тяготение, убывание массы и сопротивление воздуха.",
    "# Начальная широта 45,92, долгота 63,342, азимут 37,5 градуса.",
    "# Результаты 1000–1034 команда 70 переводит в десятичную запись,",
    "# помещает в 1100–1134; команда 44 выдаёт их на перфокарты.",
    "START 0020",
]
for index, (op, a, b, c) in enumerate(commands):
    a, b, c = resolve(a), resolve(b), resolve(c)
    assert a is not None and b is not None and c is not None
    lines.append(f"{field(0o20 + index)} {field(a)} {field(b)} "
                 f"{field(c)} 0 {op:02o}")
for name, value in constants.items():
    lines.append(data_line(addresses[name], value))
target = Path(__file__).with_name("vostok-orbit.strela")
target.write_text("\n".join(lines) + "\n", encoding="utf-8")
print(f"{target}: {len(commands)} команд, {len(constants)} констант")



