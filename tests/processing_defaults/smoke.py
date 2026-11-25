"""Exercise the packaged daemon with a disposable profile on an isolated build host."""

import copy
import json
from pathlib import Path
import socket
import sqlite3
import subprocess
import sys
import tempfile
import time
from urllib.request import urlopen


def check_reset(process, profile):
    for _ in range(200):
        if process.poll() is not None:
            raise RuntimeError(f"Daemon exited with status {process.returncode}")
        try:
            connection = socket.create_connection(("127.0.0.1", 19444), timeout=1)
            break
        except OSError:
            time.sleep(0.1)
    else:
        raise RuntimeError("Daemon did not open its JSON API")

    with connection, connection.makefile("rb") as replies:
        connection.settimeout(15)
        sequence = 0

        def rpc(command, subcommand, **args):
            nonlocal sequence
            sequence += 1
            request = dict(command=command, subcommand=subcommand, tan=sequence, **args)
            connection.sendall(json.dumps(request).encode() + b"\n")
            while True:
                reply = json.loads(replies.readline())
                if reply.get("tan") == sequence:
                    return reply

        def config():
            reply = rpc("config", "getconfig")
            assert reply["success"], reply
            return reply["info"]

        assert not rpc("config", "restore-processing")["success"]
        assert rpc("authorize", "login", password="hyperhdr")["success"]
        for _ in range(100):
            response = rpc("config", "getconfig")
            if response.get("success"):
                break
            time.sleep(0.1)
        fresh = config()
        with urlopen("http://127.0.0.1:8090/content/processing.html", timeout=10) as page:
            assert b'id="restore_processing_defaults"' in page.read()
        with urlopen("http://127.0.0.1:8090/js/processing.js", timeout=10) as script:
            assert b"'restore-processing'" in script.read()
        changed = copy.deepcopy(fresh)
        grade = changed["color"]["channelAdjustment"][0]
        for band in ("shadows", "midtones", "highlights"):
            for control in ("Luminance", "Saturation", "Gamma"):
                assert grade[band + control] == 1
                grade[band + control] = 1.35
        changed["color"]["sparse_processing"] = True
        changed["smoothing"]["time_ms"] += 25
        changed["blackborderdetector"]["enable"] = not fresh["blackborderdetector"]["enable"]
        changed["automaticToneMapping"]["enable"] = not fresh["automaticToneMapping"]["enable"]
        changed["general"]["showOptHelp"] = not fresh["general"]["showOptHelp"]
        assert rpc("config", "setconfig", config=changed)["success"]
        assert config() == changed

        # Inject a failure after the first section write to exercise atomicity.
        database, = profile.rglob("hyperhdr.db")
        with sqlite3.connect(database) as db:
            db.execute("""CREATE TRIGGER fail_smoothing BEFORE UPDATE ON settings
                WHEN OLD.type = 'smoothing' BEGIN SELECT RAISE(ABORT, 'test'); END""")
            assert not rpc("config", "restore-processing")["success"]
            assert config() == changed
            db.execute("DROP TRIGGER fail_smoothing")

        expected = copy.deepcopy(changed)
        sections = ["color", "smoothing", "blackborderdetector"]
        for include_global in [False, False, True]:
            if include_global:
                sections.append("automaticToneMapping")
            defaults = {key: fresh[key] for key in sections}
            expected.update(defaults)
            reply = rpc("config", "restore-processing", includeToneMapping=include_global)
            assert reply["success"], reply
            assert reply["info"] == defaults, "Reset differs from fresh-install defaults"
            assert config() == expected, "Reset changed settings outside its scope"


if __name__ == "__main__":
    with tempfile.TemporaryDirectory() as folder:
        with subprocess.Popen([*sys.argv[1:], "--service", "--userdata", folder]) as daemon:
            try:
                check_reset(daemon, Path(folder))
                print("Verified processing defaults, authorization, scope, retry and rollback.")
            finally:
                daemon.terminate()
                try:
                    daemon.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    daemon.kill()
