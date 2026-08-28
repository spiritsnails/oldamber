
def emit(line: str) -> None:
    print(line)

def main() -> int:

    emit("type_define FAIRY 0x1C")

    emit("type_chart_set DRAGON FAIRY IMMUNE")
    emit("type_chart_set FIGHTING FAIRY NOT_VERY")
    emit("type_chart_set BUG FAIRY NOT_VERY")
    emit("type_chart_set DARK FAIRY NOT_VERY")
    emit("type_chart_set POISON FAIRY SUPER")

    emit("species_set_types CLEFAIRY NORMAL FAIRY")
    emit("species_set_types CLEFABLE NORMAL FAIRY")

    emit("type_bank on")
    emit("type_bank save")
    emit("type_bank status")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
