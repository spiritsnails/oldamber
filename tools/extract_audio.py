
import re, os, sys

NOTE_IDX = {
    'C_': 0, 'C#': 1, 'Db': 1,
    'D_': 2, 'D#': 3, 'Eb': 3,
    'E_': 4,
    'F_': 5, 'F#': 6, 'Gb': 6,
    'G_': 7, 'G#': 8, 'Ab': 8,
    'A_': 9, 'A#': 10, 'Bb': 10,
    'B_': 11,
}

GB_PITCH_BASE = [
    0xF82C, 0xF89D, 0xF907, 0xF96B, 0xF9CA, 0xFA23,
    0xFA77, 0xFAC7, 0xFB12, 0xFB58, 0xFB9B, 0xFBDA,
]

def calc_freq(note_idx, pokered_octave, perfect_pitch=False):
    de = GB_PITCH_BASE[note_idx]
    if de >= 0x8000:
        de -= 0x10000
    for _ in range(pokered_octave - 1):
        de >>= 1
    d = ((de >> 8) & 0xFF)
    e = de & 0xFF
    d = (d + 8) & 0xFF
    freq = ((d & 7) << 8) | e
    if perfect_pitch:
        freq = (freq + 1) & 0x7FF
    return freq

def calc_delay(note_len, speed, tempo, frac):
    full  = note_len * speed * tempo + frac
    delay = (full >> 8) & 0xFF
    frac  = full & 0xFF
    return max(1, delay), frac

class State:
    def __init__(self):
        self.tempo        = 256
        self.speed        = 6
        self.volume       = 7
        self.duty         = 2
        self.octave       = 4
        self.frac         = 0
        self.perfect_pitch= False
        self.env_nibble   = 0
        self.wave_inst    = 0
        self.vib_delay    = 0
        self.vib_rate     = 0
        self.vib_depth    = 0

        self.slide_pending = False
        self.slide_len_mod = 0
        self.slide_target  = 0

    def copy(self):
        s = State()
        s.__dict__.update(self.__dict__)
        return s

def preprocess(raw_lines):
    flat          = []
    labels        = {}
    current_scope = ''
    for raw in raw_lines:
        s = raw.strip()
        if ';' in s:
            s = s[:s.index(';')].rstrip()
        s = s.strip()
        if not s:
            continue
        if s.endswith(':'):
            lname = s[:-1]
            if lname.startswith('.'):

                key = current_scope + lname
            else:

                current_scope = lname.rstrip(':')
                key = lname
            labels[key] = len(flat)
            flat.append(s)
        else:
            flat.append(s)
    return flat, labels

def parse_block(flat, start_idx, state, labels, stop_on_ret=False, scope='',
                is_wave=False, is_drum=False, drum_map=None):
    events          = []
    loop_start      = -1
    label_event_idx = {}
    i               = start_idx

    def resolve(lbl):
        return (scope + lbl) if lbl.startswith('.') else lbl

    while i < len(flat):
        s = flat[i]; i += 1

        if s.endswith(':'):
            lname = s[:-1]
            label_event_idx[lname] = len(events)
            if '.mainloop' in s:
                loop_start = len(events)
            continue

        if s.startswith('tempo '):
            m = re.match(r'tempo\s+(\d+)', s)
            if m: state.tempo = int(m.group(1))

        elif s.startswith('volume '):
            pass

        elif s.startswith('duty_cycle '):
            m = re.match(r'duty_cycle\s+(\d+)', s)
            if m: state.duty = int(m.group(1))

        elif s.startswith('note_type '):
            m = re.match(r'note_type\s+(\d+)\s*,\s*(\d+)\s*,\s*([-\d]+)', s)
            if m:
                state.speed  = int(m.group(1))
                state.volume = int(m.group(2))
                fade_raw = int(m.group(3))
                if is_wave:

                    state.wave_inst  = fade_raw & 0xF
                    state.env_nibble = 0
                else:

                    if fade_raw < 0:
                        state.env_nibble = 0x8 | ((-fade_raw) & 0x7)
                    else:
                        state.env_nibble = fade_raw & 0xF

        elif s.startswith('drum_speed '):
            m = re.match(r'drum_speed\s+(\d+)', s)
            if m:
                state.speed = int(m.group(1))

        elif s.startswith('octave '):
            m = re.match(r'octave\s+(\d+)', s)
            if m: state.octave = int(m.group(1))

        elif s == 'toggle_perfect_pitch':
            state.perfect_pitch = not state.perfect_pitch

        elif s.startswith('vibrato '):
            m = re.match(r'vibrato\s+(\d+)\s*,\s*(\d+)\s*,\s*(\d+)', s)
            if m:
                state.vib_delay = int(m.group(1))
                state.vib_rate  = int(m.group(2))
                state.vib_depth = int(m.group(3))

        elif s.startswith('pitch_slide'):
            m = re.match(r'pitch_slide\s+(\d+)\s*,\s*(\d+)\s*,\s*([A-G][b#_])', s)
            if m:
                state.slide_pending = True
                state.slide_len_mod = int(m.group(1))
                tgt_oct = int(m.group(2))
                tgt_name = m.group(3)
                if tgt_name in NOTE_IDX:
                    state.slide_target = calc_freq(
                        NOTE_IDX[tgt_name], tgt_oct, state.perfect_pitch)
                else:
                    state.slide_target = 0

        elif s.startswith('stereo_panning') or s.startswith('duty_cycle_pattern') \
             or s.startswith('pitch_sweep'):
            pass

        elif is_drum and s.startswith('drum_note '):
            m = re.match(r'drum_note\s+(\d+)\s*,\s*(\d+)', s)
            if m:
                inst = int(m.group(1))
                note_len = int(m.group(2))
                delay, state.frac = calc_delay(
                    note_len, state.speed, state.tempo, state.frac)

                if not drum_map or inst not in drum_map:
                    inst = 0
                events.append((0, delay, inst, 0, 0,
                               0, 0, 0, 0, 0))

        elif s.startswith('note '):
            m = re.match(r'note\s+([A-G][b#_]),\s*(\d+)', s)
            if m:
                name     = m.group(1)
                note_len = int(m.group(2))
                if name in NOTE_IDX:
                    freq = calc_freq(NOTE_IDX[name], state.octave,
                                     state.perfect_pitch)
                    delay, state.frac = calc_delay(
                        note_len, state.speed, state.tempo, state.frac)
                    env_byte = (state.volume << 4) | state.env_nibble
                    duty_val = state.wave_inst if is_wave else state.duty
                    slide_target = 0
                    slide_frames = 0
                    if state.slide_pending:
                        slide_target = state.slide_target
                        slide_frames = max(1, delay - state.slide_len_mod)
                        state.slide_pending = False
                        state.slide_len_mod = 0
                        state.slide_target = 0
                    events.append((freq, delay, duty_val, state.volume, env_byte,
                                   state.vib_delay, state.vib_rate, state.vib_depth,
                                   slide_target, slide_frames))

        elif s.startswith('rest '):
            m = re.match(r'rest\s+(\d+)', s)
            if m:
                note_len = int(m.group(1))
                delay, state.frac = calc_delay(
                    note_len, state.speed, state.tempo, state.frac)
                duty_val = state.wave_inst if is_wave else state.duty

                events.append((0, delay, duty_val, 0, 0, 0, 0, 0, 0, 0))

        elif s.startswith('sound_call '):
            m = re.match(r'sound_call\s+(\S+)', s)
            if m:
                sub_label = m.group(1)
                key = resolve(sub_label)
                if key in labels:
                    sub_start = labels[key] + 1
                    sub_events, _, _ = parse_block(flat, sub_start, state,
                                                   labels, stop_on_ret=True,
                                                   scope=scope, is_wave=is_wave,
                                                   is_drum=is_drum, drum_map=drum_map)
                    events.extend(sub_events)

        elif s.startswith('sound_ret'):
            if stop_on_ret:
                break

        elif s.startswith('sound_loop'):
            m = re.match(r'sound_loop\s+(\d+)\s*,\s*(\S+)', s)
            if m:
                count = int(m.group(1))
                lbl   = m.group(2)
                if count > 0:

                    seg_start = label_event_idx.get(lbl)
                    if seg_start is not None:
                        seg = list(events[seg_start:])
                        for _ in range(count):
                            events.extend(seg)

                else:

                    break
            else:
                break

    return events, i, loop_start

def extract_global_tempo(raw_lines, ch1_label=None):
    if ch1_label:
        in_main = False
        for line in raw_lines:
            s = line.strip()
            if ';' in s:
                s = s[:s.index(';')].strip()
            if s == ch1_label + '::':
                in_main = True
                continue
            if in_main:
                m = re.match(r'tempo\s+(\d+)', s)
                if m:
                    return int(m.group(1))

                if s.endswith('::') and s != ch1_label + '::':
                    break

    for line in raw_lines:
        s = line.strip()
        if ';' in s:
            s = s[:s.index(';')].strip()
        m = re.match(r'tempo\s+(\d+)', s)
        if m:
            return int(m.group(1))
    return 256

def parse_channel(raw_lines, channel_label, global_tempo=None, is_wave=False,
                  is_drum=False, drum_map=None):
    flat, labels = preprocess(raw_lines)

    ch_start = None
    for idx, s in enumerate(flat):
        if s == channel_label + '::' or s == channel_label + ':':
            ch_start = idx + 1
            break
    if ch_start is None:

        if channel_label in labels:
            ch_start = labels[channel_label] + 1
    if ch_start is None:
        return [], -1

    state = State()
    if global_tempo is not None:
        state.tempo = global_tempo
    if is_drum:
        state.speed = 12
    scope = channel_label
    if '.' in scope and not scope.startswith('.'):
        scope = scope.split('.', 1)[0]
    events, _, loop_start = parse_block(flat, ch_start, state, labels,
                                        stop_on_ret=True, scope=scope,
                                        is_wave=is_wave, is_drum=is_drum,
                                        drum_map=drum_map)
    return events, loop_start

def parse_spliced_channel(raw_lines, prefix_label, base_channel_label, tempo, is_wave=False):
    prefix_events, _ = parse_channel(raw_lines, prefix_label, global_tempo=tempo,
                                     is_wave=is_wave)
    full_events, full_ls = parse_channel(raw_lines, base_channel_label, global_tempo=tempo,
                                        is_wave=is_wave)
    if full_ls < 0:
        return prefix_events, -1
    return prefix_events + full_events[full_ls:], len(prefix_events)

def load_noise_instruments(base, bank):
    out = {}
    for i in range(1, 20):
        path = os.path.join(base, 'audio', 'sfx', f'noise_instrument{i:02d}_{bank}.asm')
        if not os.path.exists(path):
            continue
        steps = []
        with open(path) as f:
            for line in f:
                s = line.strip()
                if ';' in s:
                    s = s[:s.index(';')].strip()
                m = re.match(r'noise_note\s+(\d+)\s*,\s*(\d+)\s*,\s*([-\d]+)\s*,\s*(\d+)', s)
                if not m:
                    continue
                step_len = int(m.group(1))
                vol = int(m.group(2)) & 0xF
                fade_raw = int(m.group(3))
                if fade_raw < 0:
                    env_nibble = 0x8 | ((-fade_raw) & 0x7)
                else:
                    env_nibble = fade_raw & 0xF
                env_byte = (vol << 4) | env_nibble
                nr43 = int(m.group(4)) & 0xFF
                steps.append((nr43, env_byte, step_len + 1))
        if steps:
            out[i] = steps
    return out

def emit_array(c_name, events, loop_start):
    lines = [f'static const note_evt_t {c_name}[] = {{']
    for idx, (freq, frames, duty, vol, env_byte, vd, vr, vdp, st, sf) in enumerate(events):
        lm = '  /* loop */' if idx == loop_start else ''
        lines.append(f'    {{{freq:5}, {frames:3}, {duty}, {vol}, 0x{env_byte:02X}, {vd}, {vr}, {vdp}, {st:5}, {sf:3}}},' + lm)
    lines.append(f'}};  /* {len(events)} events, loop@{loop_start} */')
    return '\n'.join(lines)

def emit_ch_data(c_name, arr_name, count, loop_start):
    return (f'static const ch_data_t {c_name} = '
            f'{{ {arr_name}, {count}, {loop_start} }};')

MUSIC_ID = {
    'MUSIC_NONE':           0,
    'MUSIC_PALLET_TOWN':    1,
    'MUSIC_POKECENTER':     2,
    'MUSIC_GYM':            3,
    'MUSIC_CITIES1':        4,
    'MUSIC_CITIES2':        5,
    'MUSIC_CELADON':        6,
    'MUSIC_CINNABAR':       7,
    'MUSIC_VERMILION':      8,
    'MUSIC_LAVENDER':       9,
    'MUSIC_SS_ANNE':        10,
    'MUSIC_ROUTES1':        11,
    'MUSIC_ROUTES2':        12,
    'MUSIC_ROUTES3':        13,
    'MUSIC_ROUTES4':        14,
    'MUSIC_INDIGO_PLATEAU': 15,
    'MUSIC_OAKS_LAB':       16,
    'MUSIC_DUNGEON1':       17,
    'MUSIC_DUNGEON2':       18,
    'MUSIC_DUNGEON3':       19,
    'MUSIC_POKEMON_TOWER':  20,
    'MUSIC_SILPH_CO':       21,
    'MUSIC_SAFARI_ZONE':    22,
    'MUSIC_TITLE':          23,
    'MUSIC_JIGGLYPUFF':     24,
    'MUSIC_WILD_BATTLE':    25,
    'MUSIC_PKMN_HEALED':   26,
    'MUSIC_INTRO_BATTLE':  39,
    'MUSIC_GAME_CORNER':   40,
    'MUSIC_CINNABAR_MANSION': 42,
    'MUSIC_HALL_OF_FAME':     44,
    'MUSIC_CREDITS':          45,
}

def parse_songs(songs_asm_path):
    ids = []
    with open(songs_asm_path) as f:
        for line in f:
            s = line.strip()
            if ';' in s:
                s = s[:s.index(';')].strip()
            if s.startswith('db '):
                parts = s[3:].split(',')
                if parts:
                    sym = parts[0].strip()
                    ids.append(MUSIC_ID.get(sym, 0))
    return ids

def main():
    base      = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                             '..', 'pokered-master')
    music_dir = os.path.join(base, 'audio', 'music')
    drums3    = load_noise_instruments(base, 3)

    songs = [
        ('pallettown', 'Music_PalletTown',  3),
        ('routes1',    'Music_Routes1',     3),
        ('routes2',    'Music_Routes2',     3),
        ('oakslab',    'Music_OaksLab',     3),
        ('wildbattle', 'Music_WildBattle',  3),
        ('finalbattle', 'Music_FinalBattle', 3),
        ('cities1',    'Music_Cities1',     3),
        ('routes3',    'Music_Routes3',     3),
        ('routes4',    'Music_Routes4',     4),
        ('gym',        'Music_Gym',         3),
        ('pokecenter', 'Music_Pokecenter',  3),
        ('dungeon1',   'Music_Dungeon1',    3),
        ('dungeon2',   'Music_Dungeon2',    3),
        ('cities2',    'Music_Cities2',     3),
        ('celadon',    'Music_Celadon',     3),
        ('cinnabar',   'Music_Cinnabar',    3),
        ('cinnabarmansion', 'Music_CinnabarMansion', 4),
        ('indigoplateau', 'Music_IndigoPlateau', 4),
        ('silphco',    'Music_SilphCo',     3),
        ('dungeon3',          'Music_Dungeon3',          3),
        ('lavender',          'Music_Lavender',          3),
        ('pokemontower',      'Music_PokemonTower',      3),
        ('jigglypuffsong',    'Music_JigglypuffSong',    2),
        ('defeatedwildmon',   'Music_DefeatedWildMon',   3),
        ('defeatedtrainer',   'Music_DefeatedTrainer',   3),
        ('defeatedgymleader', 'Music_DefeatedGymLeader', 3),
        ('pkmnhealed',        'Music_PkmnHealed',        3),
        ('gymleaderbattle', 'Music_GymLeaderBattle', 3),
        ('trainerbattle',    'Music_TrainerBattle',    3),
        ('meetrival',         'Music_MeetRival',         3),
        ('meetmaletrainer',   'Music_MeetMaleTrainer',   3),
        ('meeteviltrainer',   'Music_MeetEvilTrainer',   3),
        ('meetfemaletrainer', 'Music_MeetFemaleTrainer', 3),
        ('meetprofoak',       'Music_MeetProfOak',       3),
        ('museumguy',         'Music_MuseumGuy',         3),
        ('vermilion',         'Music_Vermilion',         3),
        ('ssanne',            'Music_SSAnne',            3),
        ('titlescreen',       'Music_TitleScreen',       4),
        ('safarizone',        'Music_SafariZone',        3),
        ('surfing',           'Music_Surfing',           3),
        ('bikeriding',        'Music_BikeRiding',        4),
        ('introbattle',       'Music_IntroBattle',       4),
        ('gamecorner',        'Music_GameCorner',        3),
        ('halloffame',        'Music_HallOfFame',        3),
        ('credits',           'Music_Credits',           3),
    ]

    print('/* AUTO-GENERATED by tools/extract_audio.py — do not edit manually */')
    print('#pragma once')
    print('#include <stdint.h>')
    print()
    print('typedef struct { uint16_t freq; uint16_t frames;')
    print('                 uint8_t duty;  uint8_t volume; uint8_t env_byte;')
    print('                 uint8_t vib_delay; uint8_t vib_rate; uint8_t vib_depth;')
    print('                 uint16_t slide_target; uint8_t slide_frames; } note_evt_t;')
    print('typedef struct { const note_evt_t *notes; int count;')
    print('                 int loop_start; } ch_data_t;')
    print('typedef struct { uint8_t nr43; uint8_t env_byte; uint8_t frames; } drum_step_t;')
    print('typedef struct { const drum_step_t *steps; uint8_t count; } drum_inst_t;')
    print()

    for inst in range(1, 20):
        steps = drums3.get(inst, [])
        print(f'static const drum_step_t kDrumInst{inst}_steps[] = {{')
        for nr43, env_byte, frames in steps:
            print(f'    {{ 0x{nr43:02X}, 0x{env_byte:02X}, {frames} }},')
        print('};')
        print()
    print('static const drum_inst_t kDrumInst[20] = {')
    print('    { NULL, 0 },')
    for inst in range(1, 20):
        print(f'    {{ kDrumInst{inst}_steps, (uint8_t)(sizeof(kDrumInst{inst}_steps)/sizeof(kDrumInst{inst}_steps[0])) }},')
    print('};')
    print()

    for stem, label_base, num_ch in songs:
        with open(os.path.join(music_dir, stem + '.asm')) as f:
            raw = f.readlines()
        global_tempo = extract_global_tempo(raw, f'{label_base}_Ch1')
        short = label_base.replace('Music_', '')
        for ch in range(1, num_ch + 1):
            ch_label  = f'{label_base}_Ch{ch}'
            arr_name  = f'k{short}_Ch{ch}_notes'
            data_name = f'k{short}_Ch{ch}'
            events, ls = parse_channel(raw, ch_label, global_tempo=global_tempo,
                                       is_wave=(ch == 3), is_drum=(ch == 4),
                                       drum_map=drums3)
            if not events:
                print(f'/* WARNING: {ch_label} produced 0 events */')

                events = [(0, 1, 0, 0, 0, 0, 0, 0, 0, 0)]
                ls = 0
            print(emit_array(arr_name, events, ls))
            print(emit_ch_data(data_name, arr_name, len(events), ls))
            print()

    sfx_as_music = [
        (os.path.join(base, 'audio', 'sfx', 'pokeflute.asm'),
         'SFX_Pokeflute_Ch3', 'kPokeflute_Ch3', 3),
    ]
    for path, ch_label, data_name, ch in sfx_as_music:
        with open(path) as f:
            raw = f.readlines()
        events, ls = parse_channel(raw, ch_label, is_wave=(ch == 3),
                                   is_drum=(ch == 4), drum_map=drums3)
        if not events:
            print(f'/* WARNING: {ch_label} produced 0 events */')
            events = [(0, 1, 0, 0, 0, 0, 0, 0, 0, 0)]
            ls = 0
        print(emit_array(f'{data_name}_notes', events, ls))
        print(emit_ch_data(data_name, f'{data_name}_notes', len(events), ls))
        print()

    alt_channels = [

        ('cities1', 'Music_Cities1_Ch1_AlternateTempo', 'Music_Cities1_Ch1', 'kCities1AltTempo_Ch1', 1, True),

        ('cities1', 'Music_Cities1_Ch1_AlternateTempo', 'Music_Cities1_Ch2', 'kCities1AltTempo_Ch2', 2, False),
        ('cities1', 'Music_Cities1_Ch1_AlternateTempo', 'Music_Cities1_Ch3', 'kCities1AltTempo_Ch3', 3, False),

        ('meetrival', 'Music_MeetRival_Ch1_AlternateTempo', 'Music_MeetRival_Ch1', 'kMeetRivalAltTempo_Ch1', 1, True),
        ('meetrival', 'Music_MeetRival_Ch1_AlternateTempo', 'Music_MeetRival_Ch2', 'kMeetRivalAltTempo_Ch2', 2, False),
        ('meetrival', 'Music_MeetRival_Ch1_AlternateTempo', 'Music_MeetRival_Ch3', 'kMeetRivalAltTempo_Ch3', 3, False),
    ]
    for stem, tempo_label, source_label, data_name, ch, use_loop_target in alt_channels:
        with open(os.path.join(music_dir, stem + '.asm')) as f:
            raw = f.readlines()
        arr_name = f'{data_name}_notes'

        parse_label = source_label
        if use_loop_target:
            target_label = None
            for i, line in enumerate(raw):
                s = line.strip()
                if ';' in s:
                    s = s[:s.index(';')].strip()
                if s == f"{tempo_label}::" or s == f"{tempo_label}:":
                    for j in range(i + 1, min(i + 8, len(raw))):
                        t = raw[j].strip()
                        if ';' in t:
                            t = t[:t.index(';')].strip()
                        m = re.match(r'^sound_loop\s+\d+\s*,\s*([A-Za-z0-9_.]+)$', t)
                        if m:
                            target_label = m.group(1)
                            break
                    break
            if target_label is None:
                print(f'/* WARNING: could not resolve sound_loop target for {tempo_label} */')
                target_label = source_label
            parse_label = target_label

        events, ls = parse_channel(raw, parse_label,
                                   global_tempo=extract_global_tempo(raw, tempo_label),
                                   is_wave=(ch == 3), is_drum=(ch == 4),
                                   drum_map=drums3)
        if not events:
            print(f'/* WARNING: {data_name} produced 0 events */')
            events = [(0, 1, 0, 0, 0, 0, 0, 0, 0, 0)]
            ls = 0
        print(emit_array(arr_name, events, ls))
        print(emit_ch_data(data_name, arr_name, len(events), ls))
        print()

    with open(os.path.join(music_dir, 'meetrival.asm')) as f:
        meetrival_raw = f.readlines()
    alt_start_variants = [

        ('kMeetRivalAltStart_Ch1', 'Music_MeetRival_Ch1_AlternateStart', 'Music_MeetRival_Ch1', 112, False),
        ('kMeetRivalAltStart_Ch2', 'Music_MeetRival_Ch2_AlternateStart', 'Music_MeetRival_Ch2', 112, False),
        ('kMeetRivalAltStart_Ch3', 'Music_MeetRival_Ch3_AlternateStart', 'Music_MeetRival_Ch3', 112, True),
        ('kMeetRivalAltStartTempo_Ch1', 'Music_MeetRival_Ch1_AlternateStart.body', 'Music_MeetRival_Ch1', 100, False),
        ('kMeetRivalAltStartTempo_Ch2', 'Music_MeetRival_Ch2_AlternateStart', 'Music_MeetRival_Ch2', 100, False),
        ('kMeetRivalAltStartTempo_Ch3', 'Music_MeetRival_Ch3_AlternateStart', 'Music_MeetRival_Ch3', 100, True),
    ]
    for data_name, prefix_label, base_channel_label, tempo, is_wave in alt_start_variants:
        arr_name = f'{data_name}_notes'
        events, ls = parse_spliced_channel(meetrival_raw, prefix_label, base_channel_label,
                                           tempo, is_wave=is_wave)
        if not events:
            print(f'/* WARNING: {data_name} produced 0 events */')
            events = [(0, 1, 0, 0, 0, 0, 0, 0, 0, 0)]
            ls = 0
        print(emit_array(arr_name, events, ls))
        print(emit_ch_data(data_name, arr_name, len(events), ls))
        print()

    songs_path = os.path.join(base, 'data', 'maps', 'songs.asm')
    ids = parse_songs(songs_path)
    print(f'/* Map music IDs: index = map ID, 0 = none/unknown */')
    print(f'static const uint8_t kMapMusicID[{len(ids)}] = {{')
    for i in range(0, len(ids), 8):
        chunk = ids[i:i+8]
        print('    ' + ', '.join(str(x) for x in chunk) + ',')
    print('};')

if __name__ == '__main__':
    main()
