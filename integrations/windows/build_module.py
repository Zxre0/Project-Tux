#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
"""Build pinned upstream Windows server research sources on a Linux host.

This creates a research artifact, not a compatibility or safety certificate.
Never modifies the reference checkout or loads the resulting DLL into a game.
"""
import argparse
import concurrent.futures
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import xml.etree.ElementTree as ET

REPO = Path(__file__).resolve().parents[2]
PROVIDERS = {
    "erbium": {
        "pin": "44568e08cbe411ba197d3e0e900685750d46afdc",
        "project": "Erbium", "definition": "Erbium.vcxproj",
        "export": "ERBIUM_EXPORTS", "libraries": [],
    },
    "reboot": {
        "pin": "3f6417ee6b07063bb6f83902ad3971f998888bab",
        "project": "Project Reboot 3.0", "definition": "Project Reboot 3.0.vcxproj",
        "export": "PROJECTREBOOT30_EXPORTS",
        "libraries": ["fmt/fmt.lib", "spdlog/spdlog.lib", "curl/libcurl.lib",
                      "curl/zlib.lib", "MinHook/minhook.x64.lib"],
    },
    "nova": {
        "pin": "b6ee4d3121e4d01846f7ce0cc00946237bea3837",
        "project": "Nova", "definition": "Nova.vcxproj",
        "export": "NOVAUNIVERSAL_EXPORTS", "libraries": [],
        "definitions": ["STAGING", "_TCHAR_DEFINED", "WIN32_LEAN_AND_MEAN",
                        "_CRT_SECURE_NO_WARNINGS", "_WINSOCK_DEPRECATED_NO_WARNINGS"],
    },
}


def run(command, **kwargs):
    return subprocess.run(command, check=True, **kwargs)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--reference", type=Path, required=True)
    parser.add_argument("--toolchain", type=Path, required=True)
    parser.add_argument("--provider", choices=PROVIDERS, default="erbium")
    parser.add_argument("--compiler", choices=("clang", "msvc"))
    parser.add_argument("--jobs", type=int, default=2)
    parser.add_argument("--source", help="Compile just one project source; do not link")
    parser.add_argument("--link-only", action="store_true",
                        help="Relink existing objects from this pinned research build")
    parser.add_argument("--config", type=Path,
                        help="Erbium: replace Public/Configuration.h in the generated copy")
    parser.add_argument("--sdk-probe", action="store_true",
                        help="Erbium SDK-only diagnostic; excludes upstream server code")
    args = parser.parse_args()
    if not 1 <= args.jobs <= 8:
        parser.error("--jobs must be between 1 and 8")
    if args.sdk_probe and (args.provider != "erbium" or args.source or args.link_only):
        parser.error("--sdk-probe requires erbium and cannot combine --source or --link-only")
    reference = args.reference.resolve(strict=True)
    toolchain = args.toolchain.resolve(strict=True)
    provider = PROVIDERS[args.provider]
    if args.compiler is None:
        args.compiler = "clang" if args.provider == "reboot" else "msvc"
    pin = provider["pin"]
    for tool in ("clang-cl", "lld-link", "git", "tar", "bash"):
        if not shutil.which(tool):
            parser.error(f"Missing native build tool: {tool}")
    run(["git", "-C", str(reference), "cat-file", "-e", pin + "^{commit}"])
    root = REPO / ("build-" + args.provider) / pin
    source = root / "source"
    if not source.exists():
        root.mkdir(parents=True, exist_ok=True)
        archive = root / "upstream.tar"
        run(["git", "-C", str(reference), "archive", "--output", str(archive), pin])
        source.mkdir()
        run(["tar", "-xf", str(archive), "-C", str(source)])
    # Original licensed dependency notices accompany the generated artifact.
    shutil.copy2(source / "LICENSE", root / ("LICENSE." + args.provider))
    # Windows include paths ignore case. Add aliases only in our generated copy,
    # preserving the pinned reference and the original names/content.
    for directory, dirs, files in os.walk(source, followlinks=False):
        for name in dirs + files:
            original = Path(directory) / name
            alias = Path(directory) / name.lower()
            if alias != original and not alias.exists() and not alias.is_symlink():
                alias.symlink_to(name, target_is_directory=original.is_dir())
    environment_script = toolchain / "bin/x64/msvcenv.sh"
    settings = run(["bash", "-c", '. "$1"; printf "%s\\n" "$INCLUDE" "$LIB"',
                    "fol-module-toolchain", str(environment_script)],
                   capture_output=True, text=True).stdout.splitlines()
    if len(settings) != 2:
        raise RuntimeError("Toolchain environment did not provide INCLUDE and LIB")
    env = os.environ.copy()
    env["INCLUDE"], env["LIB"] = [value.replace("z:", "").replace("\\", "/")
                                   for value in settings]
    project = source / provider["project"]
    definition = ET.parse(project / provider["definition"])
    ns = {"ms": "http://schemas.microsoft.com/developer/msbuild/2003"}
    include_flags = []
    if args.provider == "nova":
        # Resolve only the pinned Staging include directories. Never execute
        # MSBuild events or import upstream's machine-specific vcpkg paths.
        groups = definition.findall(".//ms:PropertyGroup", ns)
        staging = next(group for group in groups
                       if group.attrib.get("Condition", "").endswith("=='Staging|x64'")
                       and group.find("ms:IncludePath", ns) is not None)
        for item in staging.find("ms:IncludePath", ns).text.split(";"):
            if not item.startswith("$(ProjectDir)") or "$(" in item[len("$(ProjectDir)"):]:
                raise RuntimeError("Unsupported Nova include-path variable")
            path = (project / item[len("$(ProjectDir)"):].replace("\\", "/")).resolve()
            if not path.is_relative_to(project):
                raise RuntimeError("Nova include path escapes generated project")
            if not path.is_dir():
                # MSBuild also tolerates unused/nonexistent search directories;
                # retain the path and report it, never invent missing headers.
                print(json.dumps({"include_directory": str(path),
                                  "status": "absent-in-pinned-tree"}), flush=True)
            include_flags.append("/I" + str(path))
    if args.config:
        if args.provider != "erbium":
            parser.error("--config is only supported for erbium")
        shutil.copy2(args.config.resolve(strict=True),
                     project / "Erbium/Public/Configuration.h")
    sources = [entry.attrib["Include"].replace("\\", "/")
               for entry in definition.findall(".//ms:ClCompile[@Include]", ns)
               if entry.attrib["Include"].endswith(".cpp")]
    if args.sdk_probe:
        shutil.copy2(REPO / "integrations/windows/solo_sdk_probe.cpp",
                     project / "fol_solo_sdk_probe.cpp")
        sources = ["fol_solo_sdk_probe.cpp"]
    if args.source:
        if args.source not in sources:
            parser.error("--source must name a .cpp in the pinned project")
        sources = [args.source]
    objects = root / "objects"
    logs = root / "logs"
    objects.mkdir(exist_ok=True)
    logs.mkdir(exist_ok=True)
    compiler = (str(toolchain / "bin/x64/cl") if args.compiler == "msvc"
                else shutil.which("clang-cl"))
    flags = ["/nologo", "/c", "/std:c++latest",
             "/EHsc", "/MT", "/O2", "/utf-8", "/DNDEBUG", "/DUNICODE", "/D_UNICODE",
             "/D" + provider["export"], "/D_WINDOWS", "/D_USRDLL", "/I.", "/I../vendor"]
    flags += include_flags + ["/D" + value for value in provider.get("definitions", [])]
    if args.sdk_probe:
        flags.append("/I../SDK")
    if args.provider == "erbium":
        flags.append("/FI" + str(REPO / "integrations/windows/erbium_build_compat.h"))
    if args.compiler == "clang":
        flags += ["--target=x86_64-pc-windows-msvc",
             # Select fmt11's existing pre-1929 compatibility branch: its
             # consteval parser fails with clang22/MSVC STL literal merging.
             # This does not change game offsets or hook semantics.
             "-fms-compatibility-version=19.28",
             # MSVC accepts the upstream float->int RepairBuilding initializer.
             # Keep the conversion visible as a warning rather than an error.
                  "/clang:-Wno-error=c++11-narrowing"]
    else:
        # The original MSVC wrapper supplies its Windows include paths. Wine is
        # restricted to a new compiler-only prefix, never the game/default one.
        env["WINEPREFIX"] = str(toolchain / "compiler-prefix")
        env["WINEDEBUG"] = "-all"

    def compile_one(relative):
        key = hashlib.sha256(relative.encode()).hexdigest()[:12]
        obj = objects / (Path(relative).stem + "-" + key + ".obj")
        if args.link_only:
            if not obj.is_file() or obj.stat().st_size == 0:
                raise RuntimeError(f"Missing object for --link-only: {obj}")
            return obj
        log = logs / (obj.stem + ".log")
        command = [compiler, *flags, relative, "/Fo" + str(obj)]
        with log.open("w") as output:
            result = subprocess.run(command, cwd=project, env=env, stdout=output,
                                    stderr=subprocess.STDOUT)
        print(json.dumps({"source": relative, "exit_code": result.returncode,
                          "log": str(log)}), flush=True)
        if result.returncode:
            raise RuntimeError(f"Compilation failed: {relative}; inspect {log}")
        return obj

    with concurrent.futures.ThreadPoolExecutor(max_workers=args.jobs) as pool:
        compiled = list(pool.map(compile_one, sources))
    if args.source:
        return
    output = root / ("fol-solo-sdk-probe.dll" if args.sdk_probe
                     else "erbium-solo.dll" if args.config else args.provider + "-research.dll")
    libraries = provider["libraries"]
    command = ["lld-link", "/dll", "/machine:x64", "/subsystem:windows", "/opt:ref",
               "/opt:icf", "/timestamp:0", "/out:" + str(output), *map(str, compiled),
               *[str(source / "vendor" / item) for item in libraries],
               "kernel32.lib", "user32.lib", "gdi32.lib", "winspool.lib", "comdlg32.lib",
               "advapi32.lib", "shell32.lib", "ole32.lib", "oleaut32.lib", "uuid.lib",
               "odbc32.lib", "odbccp32.lib", "onecore.lib", "d3d9.lib", "ws2_32.lib",
               "crypt32.lib", "wldap32.lib", "/nodefaultlib:Dbghelp.lib", "dbghelp.lib",
               "/nodefaultlib:Ws2_32.lib", "/nodefaultlib:Wldap32.lib",
               "/nodefaultlib:Crypt32.lib", "/nodefaultlib:Normaliz.lib", "normaliz.lib"]
    with (logs / "link.log").open("w") as log:
        run(command, cwd=project, env=env, stdout=log, stderr=subprocess.STDOUT)
    digest = hashlib.sha256(output.read_bytes()).hexdigest()
    if args.sdk_probe:
        output.with_suffix(".dll.toml").write_text(
            'fortnite_version = "12.50"\nrole = "solo-sdk-probe"\n' +
            f'sha256 = "{digest}"\nupstream_commit = "{pin}"\n' +
            '# SDK observations only; no runtime compatibility or gameplay claim.\n')
    print(json.dumps({"artifact": str(output), "upstream_commit": pin,
                      "sha256": digest, "variant": "sdk-probe" if args.sdk_probe else "upstream-research",
                      "compiler": args.compiler, "source_count": len(compiled),
                      "runtime_compatibility": "unverified", "loaded": False}))


if __name__ == "__main__":
    try:
        main()
    except subprocess.CalledProcessError as error:
        print(f"{error.cmd[0]} exited {error.returncode}; inspect the provider build logs",
              file=sys.stderr)
        sys.exit(1)
    except (OSError, RuntimeError) as error:
        print(str(error), file=sys.stderr)
        sys.exit(1)
