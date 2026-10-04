"""Run a Python file inside the running Unreal Editor (needs remote execution on).

    python Scripts/ue_remote.py path/to/script.py
"""
import sys
import time

sys.path.append(r"C:\Program Files\Epic Games\UE_5.8\Engine\Plugins\Experimental\PythonScriptPlugin\Content\Python")
import remote_execution as rex  # noqa: E402

code = open(sys.argv[1], encoding="utf-8").read()
r = rex.RemoteExecution()
r.start()
try:
    for _ in range(60):
        if r.remote_nodes:
            break
        time.sleep(1)
    else:
        sys.exit("No Unreal Editor with remote execution found")
    r.open_command_connection(r.remote_nodes[0]["node_id"])
    res = r.run_command(code, unattended=True, exec_mode=rex.MODE_EXEC_FILE)
    for o in res.get("output", []):
        print(o["output"].rstrip())
    if not res.get("success"):
        print("RESULT:", res.get("result"))
        sys.exit(1)
finally:
    r.stop()
