#!/usr/bin/python3
from subprocess import Popen, PIPE
from argparse import ArgumentParser
from tqdm import tqdm
from multiprocessing import Pool, Lock
from xlsxwriter import Workbook

mutex = Lock()

#configs = ["32 32 32 32", "64 16 32 32", "128 8 32 32", "256 4 32 32"]
#configs = ["8 32 32 32", "16 16 32 32", "32 8 32 32", "64 4 32 32"]
configs = ["8 32 32 32"]
names = ["sibenik", "sponza", "warehouse", "station"]
variants = ["", "-multicore", "-multicore-fragmented"]
#weights = [0.0, 0.5, 1.0, 1.5, 2.0]
weights = [0.0, 1.0, 2.0, 3.0, 4.0, 5.0, 6.0]

args_list = []
for config in configs:
    for name in names:
        for variant in variants:
            for weight in weights:
                args = {
                    "config": config,
                    "name": name,
                    "variant": variant,
                    "weight": weight
                }
                if "fragmented" in name:
                    args["fname"] = "{}-freq-fragmented".format(name)
                else:
                    args["fname"] = "{}-freq".format(name)
                args_list.append(args)

results = {}

def runsim(args):
    cmd = (
        "./tcache -w {weight} -f input/{fname}.txt "
        "input/{name}-access-sorted{variant}.txt "
        "{config}"
    ).format(**args)
    proc = Popen(cmd, shell=True, stdout=PIPE, stderr=PIPE)
    stdout, stderr = proc.communicate()
    
    mutex.acquire()
    if proc.returncode != 0:
        print("error for '{}'".format(cmd))
        print(str(stderr))
        mutex.release()
        return None
    mutex.release()

    data = {}
    lines = stdout.decode("utf-8").splitlines()
    for line in lines:
        tokens = line.split()
        if len(tokens) >= 2:
            data[tokens[0]] = int(tokens[1])
    data["hits"] = data["total"] - data["misses"]
    data["mRate"] = float(data["misses"]) / data["total"]
    data.update(args)
    return data

with Pool(12) as pool:
    n = len(args_list)
    for data in tqdm(pool.imap_unordered(runsim, args_list), total=n):
        if data is None:
            continue
        key = (data["name"], data["variant"], data["weight"], data["config"])
        results[key] = data

headers = [
    "weight", "nSets", "nWays", "bWidth", "bHeight",
    "hits", "misses", "total", "mRate"
]
for name in names:
    book = Workbook("data4/{}.xlsx".format(name))
    bold = book.add_format({"bold": True})

    for variant in variants:
        sheet = book.add_worksheet(name + variant)
        for i, header in enumerate(headers):
            sheet.write(0, i, header, bold)
        row = 1
        for weight in weights:
            for config in configs:
                key = (name, variant, weight, config)
                if key not in results:
                    continue
                data = results[key]
                for i, header in enumerate(headers):
                    sheet.write(row, i, data[header])
                row += 1
    book.close()