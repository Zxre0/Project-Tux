# Troubleshooting

- `Build is not registered`: use `discover ROOT`, then `add PATH`. The repository
  does not download Fortnite assets. A missing build prevents any game launch.
- `Version unknown` / conflicting metadata: inspect `verify PATH` evidence. Do
  not rename the directory to pretend detection succeeded.
- Missing Vulkan headers during compilation: install developer dependencies or
  supply a pinned Khronos headers path to CMake. Runtime loader alone is insufficient.
- Missing Vulkan devices: inspect `diagnose` loader/instance/enumeration results,
  PCI drivers and distribution packages. GPU presence alone does not prove Vulkan.
- Prefix failure: the error identifies its initialization log. Only dedicated
  launcher-owned prefixes are permitted; unrelated Wine prefixes are never adopted.
- Private-development path unverified: this is a compatibility blocker. An
  `--offline-probe` can investigate direct startup without network connectivity;
  it cannot provide authentication or private gameplay.
- Unknown failure classification: inspect the session's Wine and process logs.
  Classifications are evidence hints, not asserted root causes.
- EAC detected: the owned probe is stopped. Inspect process ancestry and launch
  chain. Do not modify, spoof or patch EAC.

Every launch request has `result.json` and `launcher.log`; planned commands also
have `launch-command.txt`. Executed probes add `system.json`, `wine.log` and
`processes.log`; graphics components may emit additional logs if active.
No empty placeholder logs are created to imply that components ran.
