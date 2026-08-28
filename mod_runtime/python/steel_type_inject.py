
def emit(line: str) -> None:
    print(line)

def main() -> int:

    emit("type_define STEEL 0x1B")

    emit("type_chart_set NORMAL STEEL NOT_VERY")
    emit("type_chart_set FIRE STEEL SUPER")
    emit("type_chart_set WATER STEEL NOT_VERY")
    emit("type_chart_set ELECTRIC STEEL NOT_VERY")
    emit("type_chart_set GRASS STEEL NOT_VERY")
    emit("type_chart_set ICE STEEL NOT_VERY")
    emit("type_chart_set FLYING STEEL NOT_VERY")
    emit("type_chart_set PSYCHIC STEEL NOT_VERY")
    emit("type_chart_set BUG STEEL NOT_VERY")
    emit("type_chart_set ROCK STEEL SUPER")
    emit("type_chart_set GHOST STEEL NOT_VERY")
    emit("type_chart_set DRAGON STEEL NOT_VERY")
    emit("type_chart_set POISON STEEL IMMUNE")
    emit("type_chart_set GROUND STEEL SUPER")
    emit("type_chart_set FIGHTING STEEL SUPER")

    emit("species_set_types MAGNEMITE ELECTRIC STEEL")
    emit("species_set_types MAGNETON ELECTRIC STEEL")

    emit("type_bank on")
    emit("type_bank save")
    emit("type_bank status")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
