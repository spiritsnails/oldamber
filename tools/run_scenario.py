
from __future__ import annotations
import argparse, pathlib, re, shutil, subprocess, sys, tempfile
import yaml

ROOT = pathlib.Path(__file__).resolve().parents[1]
BADGES = {n: i for i, n in enumerate(("BOULDER","CASCADE","THUNDER","RAINBOW","SOUL","MARSH","VOLCANO","EARTH"))}
TOP = {"schema","game","base","player","location","respawn","party","boxes","daycare","bag","pc_items","pokedex","events","progress_flags","rng","options","play_time","movement","rival_starter"}

class ScenarioError(Exception): pass

def constants(path: pathlib.Path, prefix: str):
    out = {}
    for name, value in re.findall(r"^#define\s+"+re.escape(prefix)+r"([A-Z0-9_]+)\s+(0x[0-9A-Fa-f]+|\d+)", path.read_text(encoding="utf-8"), re.M):
        out[name] = int(value, 0)
    return out

def table(path: pathlib.Path):
    return {n: int(v,0) for n,v in re.findall(r'\{\s*"([A-Z0-9_]+)"\s*,\s*(0x[0-9A-Fa-f]+|\d+)u?\s*\}',path.read_text(encoding="utf-8"))}

SPECIES=constants(ROOT/"src/game/constants.h","SPECIES_")
MOVES={n:int(v,16) for n,v in re.findall(r"^\s*const\s+([A-Z0-9_]+)\s*;\s*([0-9a-fA-F]{2})",(ROOT/"pokered-master/constants/move_constants.asm").read_text(encoding="utf-8"),re.M)}
MOVES.update({"THUNDER_SHOCK":MOVES["THUNDERSHOCK"]})
ITEMS=table(ROOT/"src/data/item_names_gen.h")
EVENTS=table(ROOT/"src/data/event_flag_ids.h")
MAPS={}
for _path in (ROOT/"mod_runtime/blocks").rglob("vmap_*_properties.block"):
    _name=_path.stem[len("vmap_"):-len("_properties")]
    MAPS[_name.upper()]=_name
for _path in (ROOT/"mod_runtime/generatedmaps").glob("*/blocks/*.block"):
    MAPS[_path.stem.upper()]=_path.stem
MAP_DIMS={}
for _path in (ROOT/"mod_runtime").rglob("*.block"):
    _match=re.search(r"^mapsize\s+(\S+)\s+(\d+)\s+(\d+)\s*$",_path.read_text(encoding="utf-8",errors="ignore"),re.M)
    if _match:MAP_DIMS[_match.group(1).upper()]=(int(_match.group(2))*2,int(_match.group(3))*2)

def mapping_id(value, values, kind, minimum=1, maximum=255):
    if isinstance(value,int): result=value
    elif isinstance(value,str):
        key=value.strip().upper().replace(" ","_").replace("-","_")
        if key not in values: raise ScenarioError(f"unknown {kind} {value!r}")
        result=values[key]
    else: raise ScenarioError(f"{kind} must be a name or number")
    if not minimum <= result <= maximum: raise ScenarioError(f"{kind} id {result} is outside {minimum}..{maximum}")
    return result

def integer(value, name, lo, hi):
    if isinstance(value,bool) or not isinstance(value,int) or not lo <= value <= hi:
        raise ScenarioError(f"{name} must be an integer from {lo} to {hi}")
    return value

def boolean(value,name):
    if not isinstance(value,bool):raise ScenarioError(f"{name} must be true or false")
    return value

def keys(obj, allowed, where):
    if not isinstance(obj,dict): raise ScenarioError(f"{where} must be a mapping")
    extra=set(obj)-set(allowed)
    if extra: raise ScenarioError(f"unknown {where} field(s): {', '.join(sorted(extra))}")

MON_FIELDS={"species","level","moves","nickname","nickname_bytes","ot_name","ot_name_bytes","status","current_hp","dvs","stat_exp","ot_id","experience","pp"}

def encoded_name_bytes(value,where):
    if value is None:return "-"
    if not isinstance(value,str) or not re.fullmatch(r"[0-9a-fA-F]{22}",value):
        raise ScenarioError(f"{where} must be exactly 22 hexadecimal characters")
    return value.lower()
def status_value(value, where):
    if value is None:return 0
    if isinstance(value,int):return integer(value,where,0,255)
    text=str(value).lower(); simple={"healthy":0,"none":0,"poisoned":8,"poison":8,"burned":16,"burn":16,"frozen":32,"freeze":32,"paralyzed":64,"paralysis":64}
    if text in simple:return simple[text]
    if text.startswith("sleep:"):return integer(int(text.split(":",1)[1]),where,1,7)
    raise ScenarioError(f"unknown {where} {value!r}")

def dvs_value(value,where):
    if value is None:return 0
    if isinstance(value,int):return integer(value,where,0,65535)
    keys(value,{"attack","defense","speed","special"},where)
    vals=[integer(value.get(k,0),where+"."+k,0,15) for k in ("attack","defense","speed","special")]
    return (vals[0]<<12)|(vals[1]<<8)|(vals[2]<<4)|vals[3]

def stat_exp_values(value,where):
    if value is None:return [0]*5
    keys(value,{"hp","attack","defense","speed","special"},where)
    return [integer(value.get(k,0),where+"."+k,0,65535) for k in ("hp","attack","defense","speed","special")]

def pp_values(value,where):
    if value is None:return [-1]*4
    if not isinstance(value,list) or len(value)>4:raise ScenarioError(f"{where} must contain at most 4 entries")
    out=[]
    for i,entry in enumerate(value):
        if isinstance(entry,int):out.append(integer(entry,f"{where}[{i}]",0,63))
        else:
            keys(entry,{"current","pp_ups"},f"{where}[{i}]")
            out.append(integer(entry.get("current"),f"{where}[{i}].current",0,63)|(integer(entry.get("pp_ups",0),f"{where}[{i}].pp_ups",0,3)<<6))
    return out+[-1]*(4-len(out))

def mon_values(mon,where):
    keys(mon,MON_FIELDS,where)
    sid=mapping_id(mon.get("species"),SPECIES,"species")
    level=integer(mon.get("level"),where+".level",1,100)
    if "moves" in mon:
        moves=mon["moves"]
        if not isinstance(moves,list) or len(moves)>4:raise ScenarioError(f"{where}.moves must contain at most 4 moves")
        mids=[mapping_id(v,MOVES,"move",0) for v in moves]+[0]*(4-len(moves))
    else:mids=[-1]*4
    nick=str(mon.get("nickname","-"))
    if nick!="-" and not 1<=len(nick)<=10:raise ScenarioError(f"{where}.nickname must contain 1..10 characters")
    ot_name=str(mon.get("ot_name","-"))
    if ot_name!="-" and not 1<=len(ot_name)<=7:raise ScenarioError(f"{where}.ot_name must contain 1..7 characters")
    nick_bytes=encoded_name_bytes(mon.get("nickname_bytes"),where+".nickname_bytes")
    ot_bytes=encoded_name_bytes(mon.get("ot_name_bytes"),where+".ot_name_bytes")
    meta=[status_value(mon.get("status"),where+".status"),integer(mon["current_hp"],where+".current_hp",0,65535) if "current_hp" in mon else -1,dvs_value(mon.get("dvs"),where+".dvs"),*stat_exp_values(mon.get("stat_exp"),where+".stat_exp"),integer(mon["ot_id"],where+".ot_id",0,65535) if "ot_id" in mon else -1,integer(mon["experience"],where+".experience",0,16777215) if "experience" in mon else -1,*pp_values(mon.get("pp"),where+".pp")]
    return sid,level,mids,nick.upper(),ot_name.upper(),nick_bytes,ot_bytes,meta

def normalize(doc, source):
    if not isinstance(doc,dict): raise ScenarioError("scenario root must be a mapping")
    keys(doc,TOP,"top-level")
    if doc.get("schema") != 1: raise ScenarioError("schema must be 1")
    game=str(doc.get("game","")).lower()
    if game not in ("red","blue"): raise ScenarioError("game must be red or blue")
    base=doc.get("base","fresh"); current_save=None
    if base == "fresh": base_kind="fresh"
    elif isinstance(base,dict):
        keys(base,{"type","path"},"base")
        if base.get("type")!="current_save" or not base.get("path"):
            raise ScenarioError("current-save base requires type: current_save and an explicit path")
        current_save=pathlib.Path(str(base["path"])).expanduser().resolve()
        if not current_save.is_file(): raise ScenarioError(f"current save does not exist: {current_save}")
        base_kind="current_save"
    else: raise ScenarioError("base must be fresh or an explicit {type: current_save, path: ...} mapping")
    lines=["scenario\t1",f"base\t{base_kind}"]

    if "player" in doc:
        p=doc["player"]; keys(p,{"name","rival_name","trainer_id","money","coins","badges"},"player")
        if base_kind=="current_save" and set(p)!={"name","rival_name","trainer_id","money","coins","badges"}:
            raise ScenarioError("overriding player on current_save currently requires all player fields")
        name=str(p.get("name","ASH")).upper(); rival=str(p.get("rival_name","GARY")).upper()
        if not 1<=len(name)<=7 or not 1<=len(rival)<=7: raise ScenarioError("player and rival names must contain 1..7 characters")
        badges=p.get("badges",[])
        if not isinstance(badges,list): raise ScenarioError("player.badges must be a list")
        mask=0
        for badge in badges:
            key=str(badge).upper()
            if key not in BADGES: raise ScenarioError(f"unknown badge {badge!r}")
            mask |= 1<<BADGES[key]
        lines.append("player\t%s\t%s\t%d\t%d\t%d\t%d"%(name,rival,integer(p.get("trainer_id",0),"trainer_id",0,65535),integer(p.get("money",3000),"money",0,999999),integer(p.get("coins",0),"coins",0,9999),mask))

    loc=doc.get("location",{"vmap":"RedsHouse2F","x":3,"y":6,"facing":"down"})
    keys(loc,{"vmap","x","y","facing"},"location")
    rawmap=str(loc.get("vmap","")); mapname=MAPS.get(rawmap.upper())
    if not mapname: raise ScenarioError(f"unknown vmap {rawmap!r}")
    facing_names={"down":0,"up":1,"left":2,"right":3}; face=loc.get("facing","down")
    if isinstance(face,str):
        if face.lower() not in facing_names: raise ScenarioError(f"unknown facing {face!r}")
        face=facing_names[face.lower()]
    x=integer(loc.get("x"),"location.x",0,255);y=integer(loc.get("y"),"location.y",0,255)
    if rawmap.upper() in MAP_DIMS:
        width,height=MAP_DIMS[rawmap.upper()]
        if x>=width or y>=height:raise ScenarioError(f"location ({x}, {y}) is outside {mapname}'s {width}x{height} tile bounds")
    lines.append(f"location\t{mapname}\t{x}\t{y}\t{integer(face,'location.facing',0,3)}")
    if "respawn" in doc:
        respawn=doc["respawn"]
        if respawn is None: lines.append("respawn\t-")
        else:
            keys(respawn,{"last_healed_vmap"},"respawn")
            raw_respawn=str(respawn.get("last_healed_vmap","")); respawn_map=MAPS.get(raw_respawn.upper())
            if not respawn_map:raise ScenarioError(f"unknown respawn.last_healed_vmap {raw_respawn!r}")
            lines.append(f"respawn\t{respawn_map}")

    if "party" in doc:
        party=doc["party"]
        if not isinstance(party,list) or len(party)>6: raise ScenarioError("party must be a list of at most 6 monsters")
        lines.append("clear_party")
        for i,mon in enumerate(party):
            sid,level,mids,nick,ot_name,nick_bytes,ot_bytes,meta=mon_values(mon,f"party[{i}]")
            lines.append("party\t%d\t%d\t%d\t%d\t%d\t%d\t%s\t%s\t%s\t%s"%(sid,level,*mids,nick,ot_name,nick_bytes,ot_bytes))
            if any(v>=0 for v in meta):lines.append("party_meta\t%d\t%s"%(i,"\t".join(map(str,meta))))

    if "boxes" in doc:
        boxes=doc["boxes"];keys(boxes,{"current","pokemon"},"boxes");current_box=integer(boxes.get("current",1),"boxes.current",1,12)-1;mons=boxes.get("pokemon",[])
        if not isinstance(mons,list) or len(mons)>240:raise ScenarioError("boxes.pokemon must contain at most 240 monsters")
        lines.append("clear_boxes");slots=[0]*12
        for i,mon in enumerate(mons):
            if not isinstance(mon,dict):raise ScenarioError(f"boxes.pokemon[{i}] must be a mapping")
            box=integer(mon.get("box"),f"boxes.pokemon[{i}].box",1,12)-1;body=dict(mon);body.pop("box",None)
            sid,level,mids,nick,ot_name,nick_bytes,ot_bytes,meta=mon_values(body,f"boxes.pokemon[{i}]")
            if slots[box]>=20:raise ScenarioError(f"box {box+1} contains more than 20 monsters")
            lines.append("box\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%s\t%s\t%s\t%s"%(box,sid,level,*mids,nick,ot_name,nick_bytes,ot_bytes))
            if any(v>=0 for v in meta):lines.append("box_meta\t%d\t%d\t%s"%(box,slots[box],"\t".join(map(str,meta))))
            slots[box]+=1
        lines.append(f"current_box\t{current_box}")

    if "daycare" in doc:
        if doc["daycare"] is None:lines.append("clear_daycare")
        else:
            sid,level,mids,nick,ot_name,nick_bytes,ot_bytes,meta=mon_values(doc["daycare"],"daycare")
            lines.append("daycare\t%d\t%d\t%d\t%d\t%d\t%d\t%s\t%s\t%s\t%s"%(sid,level,*mids,nick,ot_name,nick_bytes,ot_bytes))
            lines.append("daycare_meta\t%s"%"\t".join(map(str,meta)))

    for field,record,values,cap in (("bag","bag",ITEMS,20),("pc_items","pc_item",ITEMS,50)):
        if field in doc:
            entries=doc[field]
            if not isinstance(entries,list) or len(entries)>cap: raise ScenarioError(f"{field} must contain at most {cap} entries")
            lines.append("clear_"+("bag" if field=="bag" else "pc_items"))
            for i,item in enumerate(entries):
                keys(item,{"item","quantity"},f"{field}[{i}]")
                lines.append(f"{record}\t{mapping_id(item.get('item'),values,'item')}\t{integer(item.get('quantity'),field+'.quantity',1,255)}")

    if "pokedex" in doc:
        dex=doc["pokedex"]; keys(dex,{"seen","caught"},"pokedex"); lines.append("clear_dex")
        seen=dex.get("seen",[]); caught=dex.get("caught",[])
        if not isinstance(seen,list) or not isinstance(caught,list): raise ScenarioError("pokedex seen/caught must be lists")
        caught_ids={mapping_id(v,SPECIES,"species") for v in caught}
        for sid in sorted({mapping_id(v,SPECIES,"species") for v in seen}|caught_ids): lines.append(f"dex\t{sid}\t{2 if sid in caught_ids else 1}")

    if "events" in doc:
        ev=doc["events"]; keys(ev,{"set","clear"},"events")
        if not isinstance(ev.get("set",[]),list) or not isinstance(ev.get("clear",[]),list):raise ScenarioError("events set/clear must be lists")
        yes={mapping_id(v,EVENTS,"event",0,3615) for v in ev.get("set",[])}
        no={mapping_id(v,EVENTS,"event",0,3615) for v in ev.get("clear",[])}
        both=yes&no
        if both: raise ScenarioError("the same event cannot appear in both set and clear")
        lines += [f"event\t{x}\t1" for x in sorted(yes)] + [f"event\t{x}\t0" for x in sorted(no)]
    if "progress_flags" in doc:
        progress=doc["progress_flags"]
        keys(progress,{"completed_trades","visited_towns_mask","picked_up_items"},"progress_flags")
        lines.append("clear_progress_flags")
        trades=progress.get("completed_trades",[])
        if not isinstance(trades,list):raise ScenarioError("progress_flags.completed_trades must be a list")
        trade_mask=0
        for i,trade in enumerate(trades):trade_mask|=1<<integer(trade,f"progress_flags.completed_trades[{i}]",0,15)
        lines.append(f"completed_trades\t{trade_mask}")
        lines.append(f"visited_towns\t{integer(progress.get('visited_towns_mask',0),'progress_flags.visited_towns_mask',0,65535)}")
        pickups=progress.get("picked_up_items",[])
        if not isinstance(pickups,list):raise ScenarioError("progress_flags.picked_up_items must be a list")
        seen_maps=set()
        for i,pickup in enumerate(pickups):
            keys(pickup,{"map_id","slots"},f"progress_flags.picked_up_items[{i}]")
            map_id=integer(pickup.get("map_id"),f"progress_flags.picked_up_items[{i}].map_id",0,247)
            if map_id in seen_maps:raise ScenarioError(f"progress_flags.picked_up_items repeats map id {map_id}")
            seen_maps.add(map_id)
            slots=pickup.get("slots",[])
            if not isinstance(slots,list):raise ScenarioError(f"progress_flags.picked_up_items[{i}].slots must be a list")
            mask=0
            for j,slot in enumerate(slots):mask|=1<<integer(slot,f"progress_flags.picked_up_items[{i}].slots[{j}]",0,15)
            if mask:lines.append(f"picked_up_items\t{map_id}\t{mask}")
    if "rng" in doc:
        rng=doc["rng"]; keys(rng,{"add","sub"},"rng")
        lines.append(f"rng\t{integer(rng.get('add'),'rng.add',0,255)}\t{integer(rng.get('sub'),'rng.sub',0,255)}")
    if "options" in doc:
        opt=doc["options"];keys(opt,{"text_speed","battle_animations","battle_style","sound"},"options");speed={"fast":1,"medium":3,"slow":5}.get(str(opt.get("text_speed","medium")).lower())
        if speed is None:raise ScenarioError("options.text_speed must be fast, medium, or slow")
        style=str(opt.get("battle_style","shift")).lower();sound=str(opt.get("sound","mono")).lower()
        if style not in ("shift","set"):raise ScenarioError("options.battle_style must be shift or set")
        if sound not in ("mono","stereo"):raise ScenarioError("options.sound must be mono or stereo")
        animations=boolean(opt.get("battle_animations",True),"options.battle_animations")
        value=speed|(0 if animations else 128)|(64 if style=="set" else 0)|(32 if sound=="stereo" else 0);lines.append(f"options\t{value}")
    if "play_time" in doc:
        pt=doc["play_time"];keys(pt,{"hours","minutes","seconds","frames"},"play_time")
        frames=integer(pt.get("frames",0),"play_time.frames",0,59)+60*(integer(pt.get("seconds",0),"play_time.seconds",0,59)+60*(integer(pt.get("minutes",0),"play_time.minutes",0,59)+60*integer(pt.get("hours",0),"play_time.hours",0,9999)))
        lines.append(f"play_time\t{frames}")
    if "movement" in doc:
        movement={"walking":0,"bike":1,"biking":1,"surf":2,"surfing":2}.get(str(doc["movement"]).lower())
        if movement is None:raise ScenarioError("movement must be walking, biking, or surfing")
        lines.append(f"movement\t{movement}")
    if "rival_starter" in doc:
        lines.append(f"rival_starter\t{mapping_id(doc['rival_starter'],SPECIES,'species')}")
    return game,current_save,"\n".join(lines)+"\n"

def main():
    ap=argparse.ArgumentParser(); ap.add_argument("scenario"); ap.add_argument("--exe"); ap.add_argument("--keep",action="store_true"); ap.add_argument("--validate-only",action="store_true"); args=ap.parse_args()
    source=pathlib.Path(args.scenario).resolve()
    try: game,current,state=normalize(yaml.safe_load(source.read_text(encoding="utf-8")),source)
    except (OSError,yaml.YAMLError,ScenarioError) as exc: print(f"scenario error: {exc}",file=sys.stderr); return 2
    print(f"scenario valid: {source}")
    if args.validate_only: return 0
    if args.exe: exe=pathlib.Path(args.exe).resolve()
    else:
        choices=(ROOT/"build-red/oldamber.exe",ROOT/"build-nopy/oldamber.exe",ROOT/"build/OldAmber/OldAmber.exe")
        exe=next((candidate for candidate in choices if candidate.is_file()),choices[0])
    if not exe.is_file(): print(f"scenario error: game executable not found: {exe}",file=sys.stderr); return 2
    work=pathlib.Path(tempfile.mkdtemp(prefix=".scenario-",dir=ROOT))
    try:
        state_path=work/"scenario.state"; state_path.write_text(state,encoding="utf-8",newline="\n")
        if current: shutil.copy2(current,work/("pokered.sav" if game=="red" else "pokeblue.sav"))
        print(f"temporary scenario directory: {work}")
        return subprocess.call([str(exe),f"--workdir={work}",f"--version={game}",f"--scenario-state={state_path}"])
    finally:
        if args.keep: print(f"kept scenario directory: {work}")
        else: shutil.rmtree(work,ignore_errors=True)
if __name__=="__main__": raise SystemExit(main())
