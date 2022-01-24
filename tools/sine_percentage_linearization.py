import math


def find_closest(value, values):
    found = values[0]
    minimum = abs(value - found)

    for v in values:
        diff = abs(value - v)
        if diff < minimum:
            found = v
            minimum = diff

    return found


def main():
    PIECES = 100
    PRECISION = 10000
    MAXIMUM = 10000

    xs = [(x * 3.14)/PRECISION for x in range(PRECISION)]
    values = [((math.cos(0) - math.cos(x)) * 100) / 2. for x in xs[::-1]]

    print(
        f"static const uint16_t sine_percentage_linearization[{PIECES}] = {{")
    for i in range(PIECES):
        perc = i * (100/PIECES)
        approx = values.index(find_closest(perc, values)) * (100/PRECISION)
        approx = approx * MAXIMUM / PIECES
        print(f"    {int(approx)},")
    print("};")


if __name__ == "__main__":
    main()
