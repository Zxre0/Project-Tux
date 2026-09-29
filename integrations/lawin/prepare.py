#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Create an exclusive project-owned deployment of the pinned upstream source."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import tarfile
import tempfile

REVISION = "2205696e4e1f828b4f6457233901373c07f3e180"


def main():
    parser = argparse.ArgumentParser()
    for name in ("checkout", "deployment", "node", "mongod", "login", "config"):
        parser.add_argument("--" + name, required=True, type=Path)
    args = parser.parse_args()
    os.umask(0o077)
    root = args.deployment.resolve()
    if root.exists() or args.config.exists():
        raise RuntimeError("Deployment/config already exists; refusing overwrite")
    revision = subprocess.check_output(
        ["git", "-C", str(args.checkout), "rev-parse", "HEAD"], text=True
    ).strip()
    if revision != REVISION:
        raise RuntimeError("Unexpected upstream revision")
    # Archive a committed tree, not arbitrary untracked or modified reference files.
    root.mkdir(parents=True)
    (root / "ownership.json").write_text(json.dumps({"revision": REVISION, "root": str(root)}))
    source = root / "source"
    source.mkdir()
    with tempfile.TemporaryFile() as archive:
        subprocess.run(["git", "-C", str(args.checkout), "archive", REVISION],
                       stdout=archive, check=True)
        archive.seek(0)
        with tarfile.open(fileobj=archive) as tree:
            tree.extractall(source, filter="data")
    # Configure only deployment-owned runtime files. Never touch the reference repo.
    config_path = source / "Config/config.json"
    config = json.loads(config_path.read_text())
    config["mongodb"]["database"] = "mongodb://127.0.0.1:27017/fol_lawin"
    config["discord"]["bot_token"] = ""
    config_path.write_text(json.dumps(config, indent=2))
    (root / "database").mkdir()
    (source / "ClientSettings").mkdir(exist_ok=True)
    (source / "tokenManager/tokens.json").write_text(
        json.dumps(dict(accessTokens=[], refreshTokens=[], clientTokens=[])))
    args.config.parent.mkdir(parents=True, exist_ok=True)
    configuration = dict(revision=revision, root=str(root), node=str(args.node.resolve()),
                         mongod=str(args.mongod.resolve()), login=str(args.login.resolve()),
                         runner=str(Path(__file__).with_name("session.cjs").resolve()))
    with args.config.open("x") as output:
        json.dump(configuration, output, indent=2)
    print("Created pinned deployment; install its locked npm dependencies before launch.")


if __name__ == "__main__":
    main()
