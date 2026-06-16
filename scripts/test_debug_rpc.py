#!/usr/bin/env python3
# Copyright 2026 The Verible Authors.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0

"""End-to-end test for the verilog/debug/* and verilog/workspace/* RPC family.

Spawns verible-verilog-ls as a subprocess, drives it over stdio with the LSP
JSON-RPC framing, exercises:
  - initialize
  - verilog/workspace/open
  - verilog/workspace/list
  - verilog/debug/listRules
  - verilog/debug/scanRules
  - verilog/debug/validateRule (for one rule)
  - verilog/debug/reloadRules
  - verilog/workspace/close
  - shutdown / exit

Run after a successful `bazel build //verible/verilog/tools/ls:verible-verilog-ls`.
The script auto-locates the binary under bazel-bin/.
"""

from __future__ import annotations

import json
import os
import subprocess
import sys
from pathlib import Path
from typing import Any


REPO_ROOT = Path(__file__).resolve().parents[1]
LS_BIN_CANDIDATES = [
    REPO_ROOT / "bazel-bin/verible/verilog/tools/ls/verible-verilog-ls.exe",
    REPO_ROOT / "bazel-bin/verible/verilog/tools/ls/verible-verilog-ls",
]


def find_binary() -> Path:
    for c in LS_BIN_CANDIDATES:
        if c.exists():
            return c
    raise FileNotFoundError(
        f"verible-verilog-ls not built. tried: {[str(c) for c in LS_BIN_CANDIDATES]}"
    )


class LspClient:
    def __init__(self, proc: subprocess.Popen):
        self.proc = proc
        self._next_id = 1

    def _write(self, msg: dict) -> None:
        body = json.dumps(msg).encode("utf-8")
        header = f"Content-Length: {len(body)}\r\n\r\n".encode("ascii")
        assert self.proc.stdin is not None
        self.proc.stdin.write(header)
        self.proc.stdin.write(body)
        self.proc.stdin.flush()

    def _read(self) -> dict:
        assert self.proc.stdout is not None
        # Parse "Content-Length: N\r\n\r\n" then exactly N bytes
        content_length = -1
        while True:
            line = self.proc.stdout.readline()
            if not line:
                raise EOFError("daemon closed stdout")
            line = line.rstrip(b"\r\n")
            if not line:
                break  # end of headers
            if line.lower().startswith(b"content-length:"):
                content_length = int(line.split(b":", 1)[1].strip())
        if content_length < 0:
            raise RuntimeError("missing Content-Length header")
        body = self.proc.stdout.read(content_length)
        return json.loads(body)

    def request(self, method: str, params: Any = None) -> Any:
        rid = self._next_id
        self._next_id += 1
        msg = {"jsonrpc": "2.0", "id": rid, "method": method}
        if params is not None:
            msg["params"] = params
        self._write(msg)
        while True:
            resp = self._read()
            # Skip server-initiated notifications (e.g. publishDiagnostics).
            if "id" not in resp:
                continue
            if resp.get("id") == rid:
                if "error" in resp:
                    raise RuntimeError(f"rpc error: {resp['error']}")
                return resp.get("result")
            # ignore mismatched ids

    def notify(self, method: str, params: Any = None) -> None:
        msg = {"jsonrpc": "2.0", "method": method}
        if params is not None:
            msg["params"] = params
        self._write(msg)


def assert_eq(actual: Any, expected: Any, msg: str) -> None:
    if actual != expected:
        raise AssertionError(f"{msg}: got {actual!r}, expected {expected!r}")


def assert_true(cond: bool, msg: str) -> None:
    if not cond:
        raise AssertionError(msg)


def main() -> int:
    bin_path = find_binary()
    rules_root = REPO_ROOT / "rules"
    if not rules_root.is_dir():
        raise FileNotFoundError(f"rules/ not found at {rules_root}")

    proc = subprocess.Popen(
        [str(bin_path)],
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        bufsize=0,
    )
    try:
        client = LspClient(proc)

        # 1) initialize
        init_res = client.request(
            "initialize",
            {
                "processId": os.getpid(),
                "rootUri": "",
                "capabilities": {},
            },
        )
        assert_true("capabilities" in init_res, "initialize response missing capabilities")
        client.notify("initialized", {})

        # 2) open a workspace pointing at our repo root + rules/
        ws_root = str(REPO_ROOT).replace("\\", "/")
        open_res = client.request(
            "verilog/workspace/open",
            {
                "root": ws_root,
                "scope_paths": {"project": str(rules_root).replace("\\", "/")},
            },
        )
        assert_eq(open_res.get("ok"), True, "workspace/open ok")
        assert_true(open_res.get("rule_count", 0) >= 5, "expected >=5 rules loaded")

        # 3) workspace/list shows it
        list_ws = client.request("verilog/workspace/list", {})
        assert_true(ws_root in list_ws.get("workspaces", []), "open workspace not listed")

        # 4) debug/listRules returns rule metadata
        list_rules = client.request("verilog/debug/listRules", {"workspace": ws_root})
        rule_names = sorted(r["name"] for r in list_rules.get("rules", []))
        for required in [
            "always_statement_present",
            "binary_expression_present",
            "case_statement_present",
            "for_loop_present",
            "function_declaration_present",
        ]:
            assert_true(required in rule_names, f"missing seed rule: {required}")

        # 5) debug/validateRule on one rule should pass
        val = client.request(
            "verilog/debug/validateRule",
            {"workspace": ws_root, "ruleId": "always_statement_present"},
        )
        assert_eq(val.get("passed"), True, "always_statement_present should pass corpus")

        # 6) debug/scanRules over the bad.sv corpus file (workspace project)
        # We open a corpus file as a translation unit then scan.
        bad_file = "rules/_corpus/always_statement_present/bad.sv"
        scan_res = client.request(
            "verilog/debug/scanRules",
            {
                "workspace": ws_root,
                "ruleIds": ["always_statement_present"],
                "source_files": [bad_file],
            },
        )
        hits = scan_res.get("hits", [])
        assert_true(
            any(h["rule"] == "always_statement_present" for h in hits),
            f"scanRules should hit always_statement_present in {bad_file}; got {hits}",
        )

        # 7) reloadRules should succeed even without filesystem changes
        reload_res = client.request(
            "verilog/debug/reloadRules", {"workspace": ws_root}
        )
        assert_eq(reload_res.get("ok"), True, "reloadRules ok")
        assert_true(reload_res.get("rule_count", 0) >= 5, "rule_count still >=5")

        # 7b) getCstNode on a corpus file returns a tree
        cst_res = client.request(
            "verilog/debug/getCstNode",
            {"workspace": ws_root, "file": bad_file, "max_depth": 2},
        )
        assert_true("tree" in cst_res, f"getCstNode response missing tree: {cst_res}")
        assert_true(
            cst_res["tree"].get("kind") in ("node", "leaf"),
            f"unexpected tree kind: {cst_res['tree'].get('kind')}",
        )

        # 7c) listSymbols enumerates module declarations
        sym_res = client.request("verilog/debug/listSymbols", {"workspace": ws_root})
        sym_kinds = {s.get("kind") for s in sym_res.get("symbols", [])}
        assert_true(
            "module" in sym_kinds,
            f"listSymbols should find at least one module; got kinds={sym_kinds}",
        )

        # 7d) countReferences for "m" (the module name in our corpus files)
        ref_res = client.request(
            "verilog/debug/countReferences",
            {"workspace": ws_root, "symbol_name": "m"},
        )
        assert_true(
            ref_res.get("count", 0) >= 1,
            f"countReferences('m') should >= 1; got {ref_res}",
        )

        # 8) workspace/close
        close_res = client.request("verilog/workspace/close", {"root": ws_root})
        assert_eq(close_res.get("ok"), True, "workspace/close ok")

        # 9) shutdown / exit
        client.request("shutdown", None)
        client.notify("exit", None)
        proc.wait(timeout=5)

        print("ALL DEBUG RPC TESTS PASSED")
        return 0

    finally:
        if proc.poll() is None:
            proc.kill()
            proc.wait()


if __name__ == "__main__":
    sys.exit(main())
