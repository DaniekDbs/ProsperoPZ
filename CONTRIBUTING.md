# Contributing to ProsperoPZ

Keep the native frontend, Java backend and Wine bridge contracts in docs/architecture.md synchronized. Run the backend, frontend and runtime test suites for changes affecting those parts. Native changes must cross-compile with scripts/build-ps5.sh before packaging.

A title's STAGE-REPORT.json reports structural packaging and console validation separately. Never report a placeholder as a working runtime or game. Reject malformed archives and unsafe paths. Do not commit game files, worlds, account state or generated artifacts; Project Zomboid must always come from the user's own copy.

The vendor directory retains prospero-win's upstream licenses and sources. New functionality belongs in the Project Zomboid bridge/frontend/backend.

Build with a JDK 21 (`bash scripts/build-launcher.sh`), then run:

- `python3 -m unittest discover -s tests/launcher -p 'test_*.py' -v` (backend; Java 8/17/25 tests skip unless `JAVA8_HOME`, `JAVA17_HOME`, `JAVA25_HOME` are set)
- `bash tests/frontend/run.sh` (UI navigation, framebuffer and previews)
- `bash tests/runtime/run.sh` (bridge, icons, staging; uses `clang-18` with sanitizers)

GitHub Actions runs all three. Local logs, performance captures and scratch tools belong under ignored directories.
