"""Independent Python readers for Warcraft III war3map files (classic through 3.0.1).

Written from the public format descriptions (WC3MapSpecification, War3Net) and checked by
exact-EOF consumption over the 466 stock 3.0.1 maps. Not a port of HiveWE's C++ readers:
nothing here re-derives z, clamps flags, splits units from items or drops unknown fields.

Every parser returns a tree of dicts, lists, scalars and Recs (keyed record lists), which
fdiff.py compares. A parser raises ParseError on a short read; leftover bytes are kept in the
tree as `_trailing` so they are compared, not hidden.
"""
import struct


class ParseError(Exception):
    pass


class Recs:
    """A record list whose records are matched by key, not position. `order` diffs are separate."""

    __slots__ = ("items", "keyname")

    def __init__(self, items, keyname):
        self.items = items  # list of (key, record)
        self.keyname = keyname


class Blob:
    """Raw bytes compared position by position (summarised diff)."""

    __slots__ = ("data", "stride")

    def __init__(self, data, stride=1):
        self.data = bytes(data)
        self.stride = stride


class R:
    def __init__(self, b, name=""):
        self.b = b
        self.o = 0
        self.name = name

    def need(self, n):
        if self.o + n > len(self.b):
            raise ParseError(f"{self.name}: short read of {n} bytes at offset {self.o} (size {len(self.b)})")

    def u8(self):
        self.need(1)
        v = self.b[self.o]
        self.o += 1
        return v

    def u16(self):
        self.need(2)
        v = struct.unpack_from("<H", self.b, self.o)[0]
        self.o += 2
        return v

    def u32(self):
        self.need(4)
        v = struct.unpack_from("<I", self.b, self.o)[0]
        self.o += 4
        return v

    def i32(self):
        self.need(4)
        v = struct.unpack_from("<i", self.b, self.o)[0]
        self.o += 4
        return v

    def f32(self):
        self.need(4)
        v = struct.unpack_from("<f", self.b, self.o)[0]
        self.o += 4
        return v

    def raw(self, n):
        self.need(n)
        v = self.b[self.o:self.o + n]
        self.o += n
        return v

    def id4(self):
        return self.raw(4).decode("latin1")

    def cstr(self):
        e = self.b.find(b"\0", self.o)
        if e < 0:
            raise ParseError(f"{self.name}: unterminated string at offset {self.o}")
        v = self.b[self.o:e].decode("utf-8", "backslashreplace")
        self.o = e + 1
        return v

    def count(self, min_record):
        n = self.u32()
        if min_record and n * min_record > len(self.b) - self.o:
            raise ParseError(f"{self.name}: count {n} at offset {self.o - 4} cannot fit")
        return n

    def remaining(self):
        return len(self.b) - self.o

    def trailing(self):
        rest = self.b[self.o:]
        if not rest:
            return None
        return {"length": len(rest), "all_zero": not any(rest), "head_hex": rest[:32].hex()}


def keyed(records, keyfn, keyname):
    return Recs([(keyfn(r), r) for r in records], keyname)


def finish(r, out):
    t = r.trailing()
    if t:
        out["_trailing"] = t
    return out


# --------------------------------------------------------------------------------------- w3i


def parse_w3i(b, ctx=None):
    r = R(b, "w3i")
    out = {}
    ver = out["version"] = r.u32()
    if ver >= 16:
        out["map_version"] = r.u32()
        out["editor_version"] = r.u32()
    if ver >= 27:
        out["game_version"] = [r.u32(), r.u32(), r.u32(), r.u32()]
    for k in ("name", "author", "description", "recommended_players"):
        out[k] = r.cstr()
    out["camera_bounds"] = [r.f32() for _ in range(8)]
    out["camera_complements"] = [r.u32() for _ in range(4)]
    out["playable_width"] = r.u32()
    out["playable_height"] = r.u32()
    out["flags"] = r.u32()
    out["main_tileset"] = chr(r.u8())
    if ver >= 25:
        out["loading_screen_number"] = r.i32()
        if ver >= 39:
            out["race_hud"] = r.i32()
        out["loading_screen_model"] = r.cstr()
        out["loading_screen_text"] = r.cstr()
        out["loading_screen_title"] = r.cstr()
        out["loading_screen_subtitle"] = r.cstr()
        out["game_data_set"] = r.i32()
        out["prologue_screen_model"] = r.cstr()
        out["prologue_text"] = r.cstr()
        out["prologue_title"] = r.cstr()
        out["prologue_subtitle"] = r.cstr()
        out["fog_style"] = r.i32()
        out["fog_start_z"] = r.f32()
        out["fog_end_z"] = r.f32()
        out["fog_density"] = r.f32()
        out["fog_color_rgba"] = list(r.raw(4))
        if ver >= 39:
            out["fog_height_start"] = r.f32()
            out["fog_height_end"] = r.f32()
            out["fog_linear_start"] = r.f32()
            out["fog_linear_end"] = r.f32()
            out["fog_max_opacity"] = r.f32()
            out["fog_draw_over_sky"] = r.i32()
        out["weather_id"] = r.id4()
        out["sound_environment"] = r.cstr()
        out["light_environment_tileset"] = chr(r.u8())
        out["water_color_rgba"] = list(r.raw(4))
        if ver >= 28:
            out["script_language"] = r.u32()
        if ver >= 31:
            out["supported_modes"] = r.u32()
            out["game_data_version"] = r.u32()
        if ver >= 32:
            out["camera_distance_default"] = r.u32()
            out["camera_distance_max"] = r.u32()
            if ver >= 33:
                out["camera_distance_min"] = r.u32()
        if ver >= 39:
            hd = out["hd_water"] = {}
            for k in ("min_opacity", "max_opacity", "reflectivity", "emissivity", "edge_softness",
                      "waves_vertex_displacement", "waves_normal_map_strength"):
                hd[k] = r.i32()
            hd["override_color_rgba"] = list(r.raw(4))
            hd["envmap_reflectivity"] = r.i32()
            out["alpha_tile_minimap_color_rgba"] = list(r.raw(4))
    elif ver == 18:
        out["loading_screen_number"] = r.i32()
        out["loading_screen_text"] = r.cstr()
        out["loading_screen_title"] = r.cstr()
        out["loading_screen_subtitle"] = r.cstr()
        out["prologue_screen_number"] = r.i32()
        out["prologue_text"] = r.cstr()
        out["prologue_title"] = r.cstr()
        out["prologue_subtitle"] = r.cstr()
    # v39 player HUD skin appears with editor 6140 (6117/6127 have none).
    hud = ver >= 39 and out.get("editor_version", 0) >= 6140
    players = []
    for _ in range(r.count(33)):
        p = {"number": r.i32(), "type": r.i32(), "race": r.i32()}
        if hud:
            p["hud_skin"] = r.i32()
        p["fixed_start"] = r.u32()
        p["name"] = r.cstr()
        p["start_x"] = r.f32()
        p["start_y"] = r.f32()
        p["ally_low_mask"] = r.u32()
        p["ally_high_mask"] = r.u32()
        if ver >= 31:
            p["enemy_low_mask"] = r.u32()
            p["enemy_high_mask"] = r.u32()
        players.append(p)
    out["players"] = keyed(players, lambda p: p["number"], "number")
    forces = []
    for _ in range(r.count(9)):
        forces.append({"flags": r.u32(), "player_mask": r.u32(), "name": r.cstr()})
    out["forces"] = forces
    # Classic maps may end early; a missing section is recorded as absent, not a failure.
    if r.remaining() >= 4 or ver >= 39:
        ups = []
        for _ in range(r.count(16)):
            ups.append({"player_mask": r.u32(), "id": r.id4(), "level": r.u32(), "availability": r.u32()})
        out["upgrades"] = ups
    if r.remaining() >= 4 or ver >= 39:
        tech = []
        for _ in range(r.count(8)):
            tech.append({"player_mask": r.u32(), "id": r.id4()})
        out["tech"] = tech
    if r.remaining() >= 4 or ver >= 39:
        tables = []
        for _ in range(r.count(12)):
            t = {"number": r.i32(), "name": r.cstr()}
            npos = r.count(4)
            t["positions"] = [r.i32() for _ in range(npos)]
            rows = []
            for _ in range(r.count(4)):
                rows.append({"chance": r.u32(), "ids": [r.id4() for _ in range(npos)]})
            t["rows"] = rows
            tables.append(t)
        out["random_unit_tables"] = keyed(tables, lambda t: t["number"], "number")
    if ver >= 25 and (r.remaining() >= 4 or ver >= 39):
        tables = []
        for _ in range(r.count(9)):
            t = {"number": r.i32(), "name": r.cstr(), "sets": []}
            for _ in range(r.count(4)):
                t["sets"].append([{"chance": r.u32(), "id": r.id4()} for _ in range(r.count(8))])
            tables.append(t)
        out["random_item_tables"] = keyed(tables, lambda t: t["number"], "number")
    return finish(r, out)


def w3i_game_version(w3i):
    gv = w3i.get("game_version") if w3i else None
    if not gv:
        return None
    return gv[0] * 100 + gv[1]


# --------------------------------------------------------------------------------------- doo


def _try_variants(fn, b, variants):
    """Use the first variant that parses and consumes the file exactly; else the first that parses."""
    first_ok = None
    errs = []
    for v in variants:
        try:
            out = fn(b, v)
        except ParseError as e:
            errs.append(str(e))
            continue
        if "_trailing" not in out:
            return out
        if first_ok is None:
            first_ok = out
    if first_ok is not None:
        return first_ok
    raise ParseError("; ".join(errs))


def _skin_variants(ctx, version):
    gv = w3i_game_version((ctx or {}).get("w3i"))
    if gv is not None:
        return [gv >= 132]
    return [True, False] if version < 12 else [True]


def _item_sets(r):
    sets = []
    for _ in range(r.count(4)):
        sets.append([{"id": r.id4(), "chance": r.u32()} for _ in range(r.count(8))])
    return sets


def _doo_body(b, has_skin):
    r = R(b, "doo")
    out = {}
    magic = r.raw(4)
    if magic != b"W3do":
        raise ParseError(f"doo: bad magic {magic!r}")
    ver = r.u32()
    sub = r.u32()
    out["header"] = {"version": ver, "subversion": sub, "has_skin(derived)": has_skin}
    recs = []
    for _ in range(r.count(42)):
        d = {"id": r.id4(), "variation": r.u32(), "x": r.f32(), "y": r.f32(), "z": r.f32(),
             "angle": r.f32(), "scale_x": r.f32(), "scale_y": r.f32(), "scale_z": r.f32()}
        if has_skin:
            d["skin"] = r.id4()
        if ver >= 12:
            d["group_id"] = r.u32()
        d["flags"] = r.u8()
        d["life"] = r.u8()
        if ver >= 8:
            d["item_table_pointer"] = r.i32()
            d["item_sets"] = _item_sets(r)
        if ver >= 13:
            d["unknown_v13"] = r.i32()
        d["creation_number"] = r.u32()
        if ver >= 12:
            d["roll"] = r.f32()
            d["pitch"] = r.f32()
            lights = []
            for _ in range(r.count(36)):
                lights.append({"index": r.u32(), "shadow_casting": r.u32(), "color_rgba": list(r.raw(4)),
                               "intensity": r.f32(), "shadow_start": r.f32(), "shadow_end": r.f32(),
                               "quadratic_falloff": r.f32(), "linear_falloff": r.f32(), "damping": r.f32()})
            d["lights"] = lights
        recs.append(d)
    out["doodads"] = keyed(recs, lambda d: d["creation_number"], "creation_number")
    if r.remaining() >= 4:
        out["special_version"] = r.u32()
        specials = []
        for _ in range(r.count(16)):
            specials.append({"id": r.id4(), "variation": r.u32(), "x": r.i32(), "y": r.i32()})
        out["special_doodads"] = specials
    return finish(r, out)


def parse_doo(b, ctx=None):
    ver = struct.unpack_from("<I", b, 4)[0] if len(b) >= 8 else 0
    return _try_variants(_doo_body, b, _skin_variants(ctx, ver))


def _units_body(b, has_skin):
    r = R(b, "units")
    out = {}
    magic = r.raw(4)
    if magic != b"W3do":
        raise ParseError(f"units: bad magic {magic!r}")
    ver = r.u32()
    sub = r.u32()
    out["header"] = {"version": ver, "subversion": sub, "has_skin(derived)": has_skin}
    recs = []
    for _ in range(r.count(91)):
        u = {"id": r.id4(), "variation": r.u32(), "x": r.f32(), "y": r.f32(), "z": r.f32(),
             "angle": r.f32(), "scale_x": r.f32(), "scale_y": r.f32(), "scale_z": r.f32()}
        if has_skin:
            u["skin"] = r.id4()
        if ver >= 12:
            u["group_id"] = r.u32()
        u["flags"] = r.u8()
        u["player"] = r.u32()
        u["unknown1"] = r.u8()
        u["unknown2"] = r.u8()
        u["hp"] = r.i32()
        u["mp"] = r.i32()
        if sub >= 11:
            u["item_table_pointer"] = r.i32()
        u["item_sets"] = _item_sets(r)
        u["gold"] = r.u32()
        u["target_acquisition"] = r.f32()
        u["hero_level"] = r.u32()
        if sub >= 11:
            u["strength"] = r.u32()
            u["agility"] = r.u32()
            u["intelligence"] = r.u32()
        u["inventory"] = [{"slot": r.u32(), "id": r.id4()} for _ in range(r.count(8))]
        u["abilities"] = [{"id": r.id4(), "autocast": r.u32(), "level": r.u32()} for _ in range(r.count(12))]
        rt = u["random_type"] = r.i32()
        if rt == 0:
            u["random"] = {"level_id": r.raw(3).hex(), "class": r.u8()}
        elif rt == 1:
            u["random"] = {"group": r.u32(), "position": r.u32()}
        elif rt == 2:
            u["random"] = [{"id": r.id4(), "chance": r.u32()} for _ in range(r.count(8))]
        u["custom_color"] = r.i32()
        u["waygate"] = r.i32()
        u["creation_number"] = r.u32()
        if ver >= 12:
            u["trailer"] = [r.u32(), r.u32(), r.u32()]
        recs.append(u)
    out["units"] = keyed(recs, lambda u: u["creation_number"], "creation_number")
    return finish(r, out)


def parse_units(b, ctx=None):
    ver = struct.unpack_from("<I", b, 4)[0] if len(b) >= 8 else 0
    return _try_variants(_units_body, b, _skin_variants(ctx, ver))


# --------------------------------------------------------------------------------------- simple files


def parse_imp(b, ctx=None):
    r = R(b, "imp")
    out = {"version": r.u32()}
    ents = []
    for _ in range(r.count(2)):
        flag = r.u8()
        path = r.cstr()
        ents.append({"flag": flag, "path": path})
    out["count"] = len(ents)
    out["entries"] = keyed(ents, lambda e: e["path"].lower().replace("/", "\\"), "path(ci)")
    return finish(r, out)


def parse_w3s(b, ctx=None):
    r = R(b, "w3s")
    ver = r.u32()
    out = {"version": ver}
    snd = []
    for _ in range(r.count(71)):
        s = {"name": r.cstr(), "file": r.cstr(), "eax": r.cstr(), "flags": r.u32(),
             "fade_in": r.i32(), "fade_out": r.i32(), "volume": r.i32(), "pitch": r.f32(),
             "pitch_variance": r.f32(), "priority": r.i32(), "channel": r.i32(), "min_distance": r.f32(),
             "max_distance": r.f32(), "distance_cutoff": r.f32(), "cone_inside": r.f32(),
             "cone_outside": r.f32(), "cone_outside_volume": r.i32(), "cone_x": r.f32(),
             "cone_y": r.f32(), "cone_z": r.f32()}
        if ver >= 2:
            s["v2_sound_name"] = r.cstr()
            s["v2_sound_label"] = r.cstr()
            s["v2_file2"] = r.cstr()
            s["v2_int1"] = r.i32()
            s["v2_str5"] = r.cstr()
            s["v2_int2"] = r.i32()
            s["v2_str6"] = r.cstr()
            s["v2_int3"] = r.i32()
            s["v2_str7"] = r.cstr()
            s["v2_str8"] = r.cstr()
            s["v2_str9"] = r.cstr()
            s["v2_str10"] = r.cstr()
            if ver >= 3:
                s["v3_int"] = r.i32()
        snd.append(s)
    out["sounds"] = keyed(snd, lambda s: s["name"], "name")
    return finish(r, out)


def parse_w3r(b, ctx=None):
    r = R(b, "w3r")
    ver = r.u32()
    out = {"version": ver}
    regs = []
    for _ in range(r.count(30)):
        g = {"left": r.f32(), "bottom": r.f32(), "right": r.f32(), "top": r.f32(), "name": r.cstr(),
             "creation_number": r.i32(), "weather_id": r.id4(), "ambient_sound": r.cstr(),
             "color_bgra": list(r.raw(4))}
        if ver >= 7:
            g["camera_blocker"] = r.i32()
            g["alpha_tile_minimap_color_rgba"] = list(r.raw(4))
        regs.append(g)
    out["regions"] = keyed(regs, lambda g: g["creation_number"], "creation_number")
    return finish(r, out)


def _w3c_body(b, local_rot):
    r = R(b, "w3c")
    ver = r.u32()
    out = {"version": ver, "local_rotation(derived)": local_rot}
    cams = []
    for _ in range(r.count(41)):
        c = {"target_x": r.f32(), "target_y": r.f32(), "z_offset": r.f32(), "rotation": r.f32(),
             "angle_of_attack": r.f32(), "distance": r.f32(), "roll": r.f32(), "fov": r.f32(),
             "far_z": r.f32(), "near_z": r.f32()}
        if local_rot:
            c["local_pitch"] = r.f32()
            c["local_yaw"] = r.f32()
            c["local_roll"] = r.f32()
        if ver >= 3:
            c["dof_distance"] = r.f32()
            c["dof_scale"] = r.f32()
            c["absolute_z"] = r.f32()
        c["name"] = r.cstr()
        if ver >= 3:
            c["camera_type"] = r.i32()
        cams.append(c)
    out["cameras"] = cams
    return finish(r, out)


def parse_w3c(b, ctx=None):
    gv = w3i_game_version((ctx or {}).get("w3i"))
    variants = [gv >= 131] if gv is not None else [True, False]
    return _try_variants(_w3c_body, b, variants)


def parse_w3e(b, ctx=None):
    r = R(b, "w3e")
    magic = r.raw(4)
    if magic != b"W3E!":
        raise ParseError(f"w3e: bad magic {magic!r}")
    ver = r.u32()
    out = {"version": ver, "tileset": chr(r.u8()), "custom_tileset": r.u32()}
    out["ground_ids"] = [r.id4() for _ in range(r.count(4))]
    out["cliff_ids"] = [r.id4() for _ in range(r.count(4))]
    w = out["width"] = r.u32()
    h = out["height"] = r.u32()
    out["offset_x"] = r.f32()
    out["offset_y"] = r.f32()
    size = 8 if ver >= 12 else 7
    r.need(w * h * size)
    body = r.b[r.o:r.o + w * h * size]
    r.o += w * h * size
    # Corners compared as raw records: a summarised diff gives the corner index and byte column.
    out["corners"] = Blob(body, size)
    return finish(r, out)


def parse_wpm(b, ctx=None):
    r = R(b, "wpm")
    magic = r.raw(4)
    if magic != b"MP3W":
        raise ParseError(f"wpm: bad magic {magic!r}")
    out = {"version": r.u32()}
    w = out["width"] = r.u32()
    h = out["height"] = r.u32()
    out["cells"] = Blob(r.raw(w * h))
    return finish(r, out)


def parse_shd(b, ctx=None):
    return {"length": len(b), "cells": Blob(b)}


def parse_mmp(b, ctx=None):
    r = R(b, "mmp")
    out = {"version": r.u32()}
    icons = []
    for _ in range(r.count(16)):
        icons.append({"type": r.u32(), "x": r.u32(), "y": r.u32(), "color_bgra": list(r.raw(4))})
    out["icons"] = icons
    return finish(r, out)


# --------------------------------------------------------------------------------------- object data


def _objects(b, levels):
    r = R(b, "objects")
    ver = r.u32()
    out = {"version": ver}
    for table in ("original", "custom"):
        objs = []
        for _ in range(r.count(12)):
            o = {"base_id": r.id4(), "new_id": r.id4()}
            nsets = r.u32() if ver >= 3 else 1
            sets = []
            for _ in range(nsets):
                flags = r.u32() if ver >= 3 else 0
                mods = []
                for _ in range(r.count(12)):
                    m = {"field": r.id4()}
                    t = m["type"] = r.u32()
                    if levels:
                        m["level"] = r.u32()
                        m["data_pointer"] = r.u32()
                    if t == 0:
                        m["value"] = r.i32()
                    elif t in (1, 2):
                        m["value"] = r.f32()
                    elif t == 3:
                        m["value"] = r.cstr()
                    else:
                        raise ParseError(f"objects: unknown modification type {t} at offset {r.o - 4}")
                    m["end"] = r.id4()
                    mods.append(m)
                sets.append({"flags": flags,
                             "mods": keyed(mods, lambda m: (m["field"], m.get("level", 0), m.get("data_pointer", 0)),
                                           "field,level,data")})
            o["sets"] = keyed(sets, lambda s: s["flags"], "flags")
            objs.append(o)
        out[table] = keyed(objs, lambda o: (o["base_id"], o["new_id"]), "base,new")
    return finish(r, out)


def parse_obj_nolevels(b, ctx=None):
    return _objects(b, False)


def parse_obj_levels(b, ctx=None):
    return _objects(b, True)


# --------------------------------------------------------------------------------------- text files


def _decode_text(b):
    bom = b.startswith(b"\xef\xbb\xbf")
    if bom:
        b = b[3:]
    return bom, b.decode("utf-8", "backslashreplace")


def parse_wts(b, ctx=None):
    bom, t = _decode_text(b)
    lines = t.replace("\r\n", "\n").split("\n")
    strings = []
    i = 0
    stray = []
    while i < len(lines):
        ln = lines[i].strip()
        if ln.upper().startswith("STRING "):
            num = ln[7:].strip()
            i += 1
            comments = []
            while i < len(lines) and lines[i].strip() != "{":
                if lines[i].strip():
                    comments.append(lines[i].strip())
                i += 1
            i += 1
            body = []
            while i < len(lines) and lines[i].rstrip() != "}":
                body.append(lines[i])
                i += 1
            if i >= len(lines):
                raise ParseError(f"wts: STRING {num} has no closing brace")
            i += 1
            try:
                key = int(num)
            except ValueError:
                key = num
            strings.append({"number": key, "comment": "\n".join(comments), "text": "\n".join(body)})
        else:
            if ln:
                stray.append(ln)
            i += 1
    out = {"bom": bom, "crlf": "\r\n" in t, "count": len(strings),
           "strings": keyed(strings, lambda s: s["number"], "number")}
    if stray:
        out["stray_lines"] = stray
    return out


def parse_ini(b, ctx=None):
    bom, t = _decode_text(b)
    sections = []
    cur = None
    pre = []
    for raw in t.replace("\r\n", "\n").split("\n"):
        ln = raw.strip()
        if not ln or ln.startswith("//") or ln.startswith(";"):
            continue
        if ln.startswith("[") and "]" in ln:
            cur = {"name": ln[1:ln.index("]")], "keys": []}
            sections.append(cur)
            continue
        k, sep, v = ln.partition("=")
        ent = {"key": k.strip(), "value": v.strip() if sep else None}
        (cur["keys"] if cur else pre).append(ent)
    for s in sections:
        s["keys"] = keyed(s["keys"], lambda e: e["key"], "key(case-sensitive)")
    out = {"bom": bom, "crlf": "\r\n" in t,
           "sections": keyed(sections, lambda s: s["name"], "section(case-sensitive)")}
    if pre:
        out["keys_before_first_section"] = pre
    return out


# --------------------------------------------------------------------------------------- triggers


class TriggerData:
    """Parameter counts from TriggerData.txt, counted from field positions (not from type names)."""

    def __init__(self, path):
        self.counts = {}
        section = None
        with open(path, "rb") as f:
            text = f.read().decode("utf-8", "replace")
        for raw in text.splitlines():
            ln = raw.strip()
            if not ln or ln.startswith("//"):
                continue
            if ln.startswith("["):
                section = ln[1:ln.index("]")]
                continue
            if section not in ("TriggerEvents", "TriggerConditions", "TriggerActions", "TriggerCalls"):
                continue
            k, sep, v = ln.partition("=")
            if not sep or k.startswith("_"):
                continue
            vals = [x.strip() for x in v.split(",")]
            # Events/conditions/actions: version, then argument types. Calls: version, events flag,
            # return type, then argument types. "nothing" is a placeholder for no argument.
            args = vals[3:] if section == "TriggerCalls" else vals[1:]
            self.counts[k] = len([a for a in args if a and a != "nothing"])


def _wtg_param(r, td, sub, depth):
    if depth > 64:
        raise ParseError("wtg: parameter nesting too deep")
    p = {"type": r.u32(), "value": r.cstr()}
    has_sub = r.u32()
    if has_sub:
        call = {"type": r.u32(), "name": r.cstr()}
        has_params = r.u32()
        call["has_params"] = has_params
        if has_params:
            n = td.counts.get(call["name"])
            if n is None:
                raise ParseError(f"wtg: call {call['name']!r} not in TriggerData.txt (offset {r.o})")
            call["params"] = [_wtg_param(r, td, sub, depth + 1) for _ in range(n)]
        p["call"] = call
    if sub == 4:
        # Pre-1.31 layout: functions carry an unknown int instead of the array flag.
        if p["type"] == 2:
            p["unknown"] = r.u32()
            arr = 0
        else:
            arr = r.u32()
    else:
        if has_sub:
            p["unknown"] = r.u32()
        arr = r.u32()
    p["is_array"] = arr
    if arr:
        p["index"] = _wtg_param(r, td, sub, depth + 1)
    return p


def _wtg_eca(r, td, sub, child, depth):
    if depth > 64:
        raise ParseError("wtg: ECA nesting too deep")
    e = {"type": r.u32()}
    if child:
        e["group"] = r.u32()
    e["name"] = r.cstr()
    e["enabled"] = r.u32()
    n = td.counts.get(e["name"])
    if n is None:
        raise ParseError(f"wtg: function {e['name']!r} not in TriggerData.txt (offset {r.o})")
    e["params"] = [_wtg_param(r, td, sub, depth + 1) for _ in range(n)]
    if sub == 7:
        e["children"] = [_wtg_eca(r, td, sub, True, depth + 1) for _ in range(r.count(17))]
    return e


def parse_wtg(b, ctx=None):
    td = (ctx or {}).get("triggerdata")
    if td is None:
        raise ParseError("wtg: no TriggerData.txt given (--triggerdata)")
    r = R(b, "wtg")
    if r.raw(4) != b"WTG!":
        raise ParseError("wtg: bad magic")
    fmt = r.u32()
    if fmt != 0x80000004:
        raise ParseError(f"wtg: pre-1.31 format {fmt} is not supported by this tool")
    sub = r.u32()
    out = {"format": fmt, "subversion": sub}
    counts = {}
    for kind in ("maps", "libraries", "categories", "triggers", "comments", "scripts", "variables"):
        counts[kind] = r.u32()
        counts[kind + "_deleted_ids"] = [r.u32() for _ in range(r.count(4))]
    out["counts"] = counts
    # 465 stock files have (0, 0, 2) here. human05 (reforged roc) has (1, 1, 0, 2): read as a
    # counted list of ints, then an unknown int, then the variable format version (2).
    # The meaning of the list is unknown (Needs verification).
    out["unknown_list"] = [r.u32() for _ in range(r.count(4))]
    out["unknown2"] = r.u32()
    out["trigger_definition_version"] = r.u32()
    vars_ = []
    for _ in range(r.count(20)):
        v = {"name": r.cstr(), "type": r.cstr(), "unknown": r.u32(), "is_array": r.u32()}
        if sub == 7:
            v["array_size"] = r.u32()
        v["initialized"] = r.u32()
        v["initial_value"] = r.cstr()
        v["id"] = r.u32()
        v["parent_id"] = r.u32()
        vars_.append(v)
    out["variables"] = keyed(vars_, lambda v: v["name"], "name")
    elements = []
    for _ in range(r.count(4)):
        cls = r.u32()
        el = {"classifier": cls}
        if cls in (1, 2, 4):
            el["id"] = r.u32()
            el["name"] = r.cstr()
            if sub == 7:
                el["is_comment"] = r.u32()
            el["open"] = r.u32()
            el["parent_id"] = r.u32()
        elif cls in (8, 16, 32):
            el["name"] = r.cstr()
            el["description"] = r.cstr()
            if sub == 7:
                el["is_comment"] = r.u32()
            el["id"] = r.u32()
            el["enabled"] = r.u32()
            el["is_custom_text"] = r.u32()
            el["initially_off"] = r.u32()
            el["run_on_init"] = r.u32()
            el["parent_id"] = r.u32()
            el["ecas"] = [_wtg_eca(r, td, sub, False, 0) for _ in range(r.count(9))]
        elif cls == 64:
            el["id"] = r.u32()
            el["name"] = r.cstr()
            el["parent_id"] = r.u32()
        else:
            raise ParseError(f"wtg: unknown element classifier {cls} at offset {r.o - 4}")
        elements.append(el)
    out["elements"] = keyed(elements, lambda e: (e["classifier"], e.get("id")), "classifier,id")
    return finish(r, out)


def parse_wct(b, ctx=None):
    r = R(b, "wct")
    fmt = r.u32()
    out = {"format": fmt}
    if fmt == 0x80000004:
        sub = out["subversion"] = r.u32()
        if sub == 1:
            out["header_comment"] = r.cstr()
            n = r.u32()
            out["header_script"] = r.raw(n).decode("utf-8", "backslashreplace")
    elif fmt in (0, 1):
        if fmt == 1:
            out["header_comment"] = r.cstr()
            out["header_script"] = r.raw(r.u32()).decode("utf-8", "backslashreplace")
        out["declared_count"] = r.u32()
    else:
        raise ParseError(f"wct: unknown format {fmt}")
    texts = []
    while r.remaining() >= 4:
        n = r.u32()
        texts.append(r.raw(n).decode("utf-8", "backslashreplace"))
    # Count trailing empty entries separately: stock files often end in zero-length entries.
    tail = 0
    while texts and texts[-1] == "":
        texts.pop()
        tail += 1
    # Entries follow the wtg element order and carry no name, so they are keyed by their first line;
    # a reorder is then an order diff and an edit a field diff.
    ents = [{"text": t} for t in texts]
    out["count"] = len(texts)
    out["texts"] = keyed(ents, lambda e: e["text"].splitlines()[0].strip()[:120] if e["text"] else "", "first line")
    out["trailing_empty_entries"] = tail
    return finish(r, out)


# --------------------------------------------------------------------------------------- dispatch

PARSERS = {
    "war3map.w3i": parse_w3i,
    "war3map.w3s": parse_w3s,
    "war3map.doo": parse_doo,
    "war3mapunits.doo": parse_units,
    "war3map.imp": parse_imp,
    "war3map.wts": parse_wts,
    "war3map.w3r": parse_w3r,
    "war3map.w3c": parse_w3c,
    "war3map.w3e": parse_w3e,
    "war3map.wpm": parse_wpm,
    "war3map.shd": parse_shd,
    "war3map.mmp": parse_mmp,
    "war3mapmisc.txt": parse_ini,
    "war3mapskin.txt": parse_ini,
    "war3mapextra.txt": parse_ini,
    "war3map.wtg": parse_wtg,
    "war3map.wct": parse_wct,
}
for _p in ("war3map", "war3mapskin"):
    for _e in ("w3u", "w3t", "w3b", "w3h"):
        PARSERS[f"{_p}.{_e}"] = parse_obj_nolevels
    for _e in ("w3a", "w3d", "w3q"):
        PARSERS[f"{_p}.{_e}"] = parse_obj_levels


def parser_for(relpath):
    return PARSERS.get(relpath.replace("\\", "/").split("/")[-1].lower())
