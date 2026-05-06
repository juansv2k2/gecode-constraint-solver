Gecode Solver Max Package (v0.1.0)

Object name:
- gecode.solver

Messages:
- config <json-string>
- solve
- cancel
- status
- get_last

Outlets (left to right):
1) list outlet: voice_pitch / voice_rhythm lists
2) json outlet: json payloads (status snapshot or solution json)
3) status outlet: status <state> <message>

Build:
1) Build external
   make max-external MAX_SDK_PATH=~/dev/max-sdk

2) Create package scaffold and copy the built external
   make max-package-layout

3) Place package folder where Max can discover packages, or open Max and add:
   max-package/gecode-solver

Quick test:
1) Open gecode.solver.maxhelp
2) Click config message
3) Click solve
4) Observe outputs in print objects and status outlet

Notes:
- Solving is asynchronous. The object polls and emits results when ready.
- Cancel is cooperative; it requests cancellation but may not interrupt deep search instantly.
