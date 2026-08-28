
def emit(line: str) -> None:
    print(line)

def main() -> int:

    emit("type_define DARK 0x1D")

    emit("type_chart_set PSYCHIC DARK IMMUNE")
    emit("type_chart_set GHOST DARK HALF")
    emit("type_chart_set FIGHTING DARK SUPER")
    emit("type_chart_set BUG DARK SUPER")

    emit("type_bank on")
    emit("type_bank save")
    emit("type_bank status")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
