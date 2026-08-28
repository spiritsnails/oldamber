
WARP_OVERRIDES = {
    13: [
        (12, 9, 46, 0), (3, 11, 47, 0), (15, 19, 48, 0),
        (16, 35, 49, 0), (15, 39, 49, 1), (3, 43, 50, 1),
    ],
    47: [
        (5, 0, 255, 1), (4, 7, 51, 0), (5, 7, 51, 0),
    ],
    49: [
        (5, 0, 255, 3), (4, 7, 255, 4), (5, 7, 255, 4),
    ],
    50: [
        (5, 0, 51, 3), (4, 7, 255, 5), (5, 7, 255, 5),
    ],
    51: [
        (1, 0, 47, 1), (2, 0, 47, 2), (15, 47, 50, 0),
        (16, 47, 50, 0), (17, 47, 50, 0), (18, 47, 50, 0),
    ],

    235: [
        (9, 0, 234, 1), (13, 0, 236, 0), (5, 5, 255, 9), (3, 2, 212, 3),
    ],
}

WARP_LASTMAP_RESOLVED = {
    46: 13,
    71: 16,
    74: 17,
    75: 17,
    85: 22,
}

def warps_for_map(map_id, rom_warps):
    if map_id in WARP_OVERRIDES:
        return list(WARP_OVERRIDES[map_id])
    dest = WARP_LASTMAP_RESOLVED.get(map_id)
    if dest is not None:
        return [(x, y, dest if dm == 255 else dm, dw)
                for (x, y, dm, dw) in rom_warps]
    return list(rom_warps)

MUSIC_OVERRIDES = {
    113: "indigo_plateau",
}

def music_for_map(map_id, rom_track):
    return MUSIC_OVERRIDES.get(map_id, rom_track)

MAP_BLOCKS_ALLOW_OVERREAD = {119}

TRAINER_PIC_ROM_WINS = {27}

AUDIO_REFERENCE_DIVERGENCES = {
    ("MeetRivalAltStartTempo", 0): (
        "Reference splices `Music_MeetRival_Ch1_AlternateStart.body` into "
        "`Music_MeetRival_Ch1` by re-parsing the base channel FROM ITS OWN "
        "START, to recover preamble state (octave/duty/vibrato) that a bare "
        "'.mainloop:' label does not re-declare. That re-parse also re-runs the "
        "base channel's `tempo 112`, which overrides the global_tempo=100 it "
        "was handed (extract_audio.py:138). The ROM never executes that "
        "command: Ch1_AlternateStartAndTempo sets tempo 100 then "
        "`sound_loop 0, $71a5`, and $71a5 is AlternateStart+3 -- exactly past "
        "its `ed 00 70`. Skipping the tempo is the whole point of the symbol. "
        "Proof it is a reference bug and not a port choice: committed ch1 runs "
        "915 frames against ch2/ch3's 825, so the three channels drift 90 "
        "frames apart per loop. The ROM gives all three 825, and every other "
        "song in the set has equal-length channels."
    ),
}
