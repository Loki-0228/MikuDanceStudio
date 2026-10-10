# Compare two mdump.py bone dumps.  Matches models by (name, boneCount),
# then per bone per field classifies: bit-exact / small (<=1e-3) / divergent.
import json, math, sys

VEC = {"pos": 3, "rotQuat": 4, "rotQuat2": 4, "phyOff": 3, "phyQuat": 4}
MAT = {"matInit": 16, "matLocal": 16, "matWorld": 16, "matExtra": 16}
INT = ["parent", "type", "flags", "layer", "tailIdx", "hasRB", "physDis",
       "hasFlag", "slotIdx"]
FLT1 = ["ratio"]

SMALL = 1e-3
BIG = 5e-2


def cmp_vec(a, b):
    exact = all(x == y for x, y in zip(a, b))
    md = max((abs(x - y) for x, y in zip(a, b)), default=0.0)
    return exact, md


def main():
    orig = json.load(open(sys.argv[1], encoding="utf-8"))
    port = json.load(open(sys.argv[2], encoding="utf-8"))
    om = {(m["name_hex"], m["boneCount"]): m for m in orig["models"]}
    pm = {(m["name_hex"], m["boneCount"]): m for m in port["models"]}
    verbose = "-v" in sys.argv
    only = None
    for a in sys.argv[3:]:
        if a.startswith("--model="):
            only = a.split("=", 1)[1]
    for key in pm:
        if key not in om:
            print("model only in port:", key)
    for key in om:
        if key not in pm:
            print("model only in orig:", key)
    for key in om:
        if key not in pm:
            continue
        name = om[key]["name"]
        if only and only not in name:
            continue
        ob, pb = om[key]["bones"], pm[key]["bones"]
        n = min(len(ob), len(pb))
        stats = {}
        first = None
        ndiv = 0
        for i in range(n):
            for fld in INT:
                if ob[i][fld] != pb[i][fld]:
                    stats[fld] = stats.get(fld, 0) + 1
                    if first is None:
                        first = (i, ob[i]["name"], fld, ob[i][fld], pb[i][fld])
            for fld in FLT1 + list(VEC) + list(MAT):
                a, b = ob[i][fld], pb[i][fld]
                if not isinstance(a, list):
                    a, b = [a], [b]
                exact, md = cmp_vec(a, b)
                if md > SMALL:
                    stats[fld] = stats.get(fld, 0) + 1
                    if md > BIG:
                        ndiv += 1
                        if first is None:
                            first = (i, ob[i]["name"], fld, max(
                                ((abs(x - y), j) for j, (x, y) in
                                 enumerate(zip(a, b)))))
        print(f"== {name} bones={n} divergent(>{BIG})={ndiv} first={first}")
        if stats:
            print("   fields:", {k: v for k, v in sorted(
                stats.items(), key=lambda kv: -kv[1])})
        if verbose:
            for i in range(n):
                for fld in list(MAT) + list(VEC):
                    a, b = ob[i][fld], pb[i][fld]
                    exact, md = cmp_vec(a, b)
                    if md > BIG:
                        print(f"   bone[{i}] {ob[i]['name']!r} {fld} "
                              f"maxdiff={md:.4g}")
                        print("     orig:", " ".join(f"{x:+.4f}" for x in a))
                        print("     port:", " ".join(f"{x:+.4f}" for x in b))
                        break


if __name__ == "__main__":
    main()
