{
    "patcher": {
        "fileversion": 1,
        "appversion": {
            "major": 9,
            "minor": 1,
            "revision": 4,
            "architecture": "x64",
            "modernui": 1
        },
        "classnamespace": "box",
        "rect": [ 34.0, 94.0, 796.0, 989.0 ],
        "boxes": [
            {
                "box": {
                    "id": "obj-14",
                    "maxclass": "message",
                    "numinlets": 2,
                    "numoutlets": 1,
                    "outlettype": [ "" ],
                    "patching_rect": [ 142.0, 336.0, 655.0, 22.0 ],
                    "text": "read \"/Users/juanvassallo/Documents/Max 8/Packages/gecode-solver/configs/heuristic_example_reproducible_seed.json\""
                }
            },
            {
                "box": {
                    "id": "obj-11",
                    "maxclass": "message",
                    "numinlets": 2,
                    "numoutlets": 1,
                    "outlettype": [ "" ],
                    "patching_rect": [ 439.0, 53.0, 162.0, 22.0 ],
                    "text": "config_dict_debug demo_cfg"
                }
            },
            {
                "box": {
                    "id": "obj-13",
                    "maxclass": "message",
                    "numinlets": 2,
                    "numoutlets": 1,
                    "outlettype": [ "" ],
                    "patching_rect": [ 117.0, 227.0, 321.0, 22.0 ],
                    "text": "77 68 63 73 67 61 64 75 69 65 71 72 70 62 74 76 66 60"
                }
            },
            {
                "box": {
                    "id": "obj-12",
                    "maxclass": "message",
                    "numinlets": 2,
                    "numoutlets": 1,
                    "outlettype": [ "" ],
                    "patching_rect": [ 117.0, 266.0, 321.0, 22.0 ],
                    "text": "65 64 65 64 65 64"
                }
            },
            {
                "box": {
                    "id": "obj-10",
                    "maxclass": "newobj",
                    "numinlets": 3,
                    "numoutlets": 3,
                    "outlettype": [ "", "", "" ],
                    "patching_rect": [ 30.0, 190.0, 100.0, 22.0 ],
                    "text": "route 0 1"
                }
            },
            {
                "box": {
                    "id": "obj-8",
                    "maxclass": "newobj",
                    "numinlets": 2,
                    "numoutlets": 2,
                    "outlettype": [ "", "" ],
                    "patching_rect": [ 30.0, 147.0, 99.0, 22.0 ],
                    "text": "route voice_pitch"
                }
            },
            {
                "box": {
                    "id": "obj-9",
                    "maxclass": "number",
                    "numinlets": 1,
                    "numoutlets": 2,
                    "outlettype": [ "", "bang" ],
                    "parameter_enable": 0,
                    "patching_rect": [ 693.0, 410.0, 50.0, 22.0 ]
                }
            },
            {
                "box": {
                    "fontname": "Arial",
                    "fontsize": 13.0,
                    "id": "obj-6",
                    "maxclass": "message",
                    "numinlets": 2,
                    "numoutlets": 1,
                    "outlettype": [ "" ],
                    "patching_rect": [ 693.0, 439.0, 222.0, 23.0 ],
                    "text": "set search_options::random_seed $1"
                }
            },
            {
                "box": {
                    "id": "obj-5",
                    "maxclass": "message",
                    "numinlets": 2,
                    "numoutlets": 1,
                    "outlettype": [ "" ],
                    "patching_rect": [ 142.0, 365.0, 665.0, 22.0 ],
                    "text": "read \"/Users/juanvassallo/Documents/Max 8/Packages/gecode-solver/examples/twelve_tone_palindrome_config.json\""
                }
            },
            {
                "box": {
                    "fontname": "Arial",
                    "fontsize": 13.0,
                    "id": "obj-1",
                    "maxclass": "message",
                    "numinlets": 2,
                    "numoutlets": 1,
                    "outlettype": [ "" ],
                    "patching_rect": [ 303.0, 439.0, 357.0, 23.0 ],
                    "text": "set engine_domains::engine_0::duration_values 1/8 1/4 1/16"
                }
            },
            {
                "box": {
                    "id": "obj-3",
                    "maxclass": "button",
                    "numinlets": 1,
                    "numoutlets": 1,
                    "outlettype": [ "bang" ],
                    "parameter_enable": 0,
                    "patching_rect": [ 30.0, 346.0, 24.0, 24.0 ]
                }
            },
            {
                "box": {
                    "id": "obj-7",
                    "maxclass": "dict.view",
                    "numinlets": 1,
                    "numoutlets": 0,
                    "patching_rect": [ 30.0, 484.0, 625.0, 488.0 ]
                }
            },
            {
                "box": {
                    "id": "obj-dict",
                    "maxclass": "newobj",
                    "numinlets": 2,
                    "numoutlets": 5,
                    "outlettype": [ "dictionary", "", "", "", "" ],
                    "patching_rect": [ 30.0, 410.0, 100.0, 22.0 ],
                    "saved_object_attributes": {
                        "legacy": 1,
                        "parameter_enable": 0,
                        "parameter_mappable": 0
                    },
                    "text": "dict demo_cfg"
                }
            },
            {
                "box": {
                    "id": "obj-dict-read",
                    "maxclass": "message",
                    "numinlets": 2,
                    "numoutlets": 1,
                    "outlettype": [ "" ],
                    "patching_rect": [ 142.0, 393.0, 535.0, 22.0 ],
                    "text": "read \"/Users/juanvassallo/Documents/Max 8/Packages/gecode-solver/examples/demo_config.json\""
                }
            },
            {
                "box": {
                    "id": "obj-dict-set-len",
                    "maxclass": "message",
                    "numinlets": 2,
                    "numoutlets": 1,
                    "outlettype": [ "" ],
                    "patching_rect": [ 142.0, 439.0, 145.0, 22.0 ],
                    "text": "set solution_length 24"
                }
            },
            {
                "box": {
                    "id": "obj-dict-config",
                    "maxclass": "message",
                    "numinlets": 2,
                    "numoutlets": 1,
                    "outlettype": [ "" ],
                    "patching_rect": [ 293.0, 53.0, 125.0, 22.0 ],
                    "text": "config_dict demo_cfg"
                }
            },
            {
                "box": {
                    "id": "obj-comment-title",
                    "maxclass": "comment",
                    "numinlets": 1,
                    "numoutlets": 0,
                    "patching_rect": [ 30.0, 16.0, 282.0, 20.0 ],
                    "text": "gecode.solver help - async musical constraint solve"
                }
            },
            {
                "box": {
                    "id": "obj-solve",
                    "maxclass": "message",
                    "numinlets": 2,
                    "numoutlets": 1,
                    "outlettype": [ "" ],
                    "patching_rect": [ 30.0, 53.0, 45.0, 22.0 ],
                    "text": "solve"
                }
            },
            {
                "box": {
                    "id": "obj-status",
                    "maxclass": "message",
                    "numinlets": 2,
                    "numoutlets": 1,
                    "outlettype": [ "" ],
                    "patching_rect": [ 86.0, 53.0, 48.0, 22.0 ],
                    "text": "status"
                }
            },
            {
                "box": {
                    "id": "obj-getlast",
                    "maxclass": "message",
                    "numinlets": 2,
                    "numoutlets": 1,
                    "outlettype": [ "" ],
                    "patching_rect": [ 145.0, 53.0, 62.0, 22.0 ],
                    "text": "get_last"
                }
            },
            {
                "box": {
                    "id": "obj-cancel",
                    "maxclass": "message",
                    "numinlets": 2,
                    "numoutlets": 1,
                    "outlettype": [ "" ],
                    "patching_rect": [ 218.0, 53.0, 52.0, 22.0 ],
                    "text": "cancel"
                }
            },
            {
                "box": {
                    "id": "obj-solver",
                    "maxclass": "newobj",
                    "numinlets": 1,
                    "numoutlets": 4,
                    "outlettype": [ "", "", "", "" ],
                    "patching_rect": [ 30.0, 97.0, 100.0, 22.0 ],
                    "text": "gecode.solver"
                }
            },
            {
                "box": {
                    "id": "obj-print-json",
                    "maxclass": "newobj",
                    "numinlets": 1,
                    "numoutlets": 0,
                    "patching_rect": [ 166.5, 147.0, 96.0, 22.0 ],
                    "text": "print solver_json"
                }
            },
            {
                "box": {
                    "id": "obj-print-status",
                    "maxclass": "newobj",
                    "numinlets": 1,
                    "numoutlets": 0,
                    "patching_rect": [ 272.5, 147.0, 106.0, 22.0 ],
                    "text": "print solver_status"
                }
            },
            {
                "box": {
                    "id": "obj-comment-note",
                    "maxclass": "comment",
                    "numinlets": 1,
                    "numoutlets": 0,
                    "patching_rect": [ 145.0, 309.0, 711.0, 20.0 ],
                    "text": "Tip: for live edits use dict demo_cfg -> set ... -> config_dict demo_cfg. This avoids JSON tokenization issues in Max message boxes."
                }
            },
            {
                "box": {
                    "fontface": 1,
                    "fontsize": 16.0,
                    "id": "obj-heur-title",
                    "maxclass": "comment",
                    "numinlets": 1,
                    "numoutlets": 0,
                    "patching_rect": [ 742.0, 593.0, 750.0, 24.0 ],
                    "text": "HEURISTIC-GUIDED MUSICAL CONSTRAINT SOLVING EXAMPLE"
                }
            },
            {
                "box": {
                    "id": "obj-heur-desc",
                    "linecount": 5,
                    "maxclass": "comment",
                    "numinlets": 1,
                    "numoutlets": 0,
                    "patching_rect": [ 742.0, 623.0, 785.0, 74.0 ],
                    "text": "This example demonstrates combining hard constraints with multi-priority heuristics. The solver generates smooth 8-note melodies constrained by:\n • Hard: No repeated adjacent notes, max interval = major sixth\n • Priority 5 (highest): Prefer stepwise motion for smooth melody\n • Priority 3 (medium): Prefer consonant scale degrees\n • Priority 1 (lowest): Prefer pitches near middle of range"
                }
            },
            {
                "box": {
                    "id": "obj-heur-guide",
                    "linecount": 3,
                    "maxclass": "comment",
                    "numinlets": 1,
                    "numoutlets": 0,
                    "patching_rect": [ 742.0, 723.0, 750.0, 47.0 ],
                    "text": "How to use: Click the message above to load the config, then click 'solve' button at top. The heuristic priorities guide the search—higher priority heuristics dominate when multiple preferences apply. Result shows melody that satisfies hard constraints while optimizing heuristic preferences."
                }
            }
        ],
        "lines": [
            {
                "patchline": {
                    "destination": [ "obj-dict", 0 ],
                    "source": [ "obj-1", 0 ]
                }
            },
            {
                "patchline": {
                    "destination": [ "obj-12", 1 ],
                    "midpoints": [ 39.5, 259.0, 428.5, 259.0 ],
                    "source": [ "obj-10", 0 ]
                }
            },
            {
                "patchline": {
                    "destination": [ "obj-13", 1 ],
                    "midpoints": [ 80.0, 219.5, 428.5, 219.5 ],
                    "source": [ "obj-10", 1 ]
                }
            },
            {
                "patchline": {
                    "destination": [ "obj-solver", 0 ],
                    "midpoints": [ 448.5, 86.0, 39.5, 86.0 ],
                    "source": [ "obj-11", 0 ]
                }
            },
            {
                "patchline": {
                    "destination": [ "obj-dict", 0 ],
                    "source": [ "obj-14", 0 ]
                }
            },
            {
                "patchline": {
                    "destination": [ "obj-dict", 0 ],
                    "source": [ "obj-3", 0 ]
                }
            },
            {
                "patchline": {
                    "destination": [ "obj-dict", 0 ],
                    "source": [ "obj-5", 0 ]
                }
            },
            {
                "patchline": {
                    "destination": [ "obj-dict", 0 ],
                    "source": [ "obj-6", 0 ]
                }
            },
            {
                "patchline": {
                    "destination": [ "obj-10", 0 ],
                    "source": [ "obj-8", 0 ]
                }
            },
            {
                "patchline": {
                    "destination": [ "obj-6", 0 ],
                    "source": [ "obj-9", 0 ]
                }
            },
            {
                "patchline": {
                    "destination": [ "obj-solver", 0 ],
                    "midpoints": [ 227.5, 86.0, 39.5, 86.0 ],
                    "source": [ "obj-cancel", 0 ]
                }
            },
            {
                "patchline": {
                    "destination": [ "obj-7", 0 ],
                    "source": [ "obj-dict", 0 ]
                }
            },
            {
                "patchline": {
                    "destination": [ "obj-solver", 0 ],
                    "midpoints": [ 302.5, 86.0, 39.5, 86.0 ],
                    "source": [ "obj-dict-config", 0 ]
                }
            },
            {
                "patchline": {
                    "destination": [ "obj-dict", 0 ],
                    "source": [ "obj-dict-read", 0 ]
                }
            },
            {
                "patchline": {
                    "destination": [ "obj-dict", 0 ],
                    "source": [ "obj-dict-set-len", 0 ]
                }
            },
            {
                "patchline": {
                    "destination": [ "obj-solver", 0 ],
                    "midpoints": [ 154.5, 86.0, 39.5, 86.0 ],
                    "source": [ "obj-getlast", 0 ]
                }
            },
            {
                "patchline": {
                    "destination": [ "obj-solver", 0 ],
                    "midpoints": [ 39.5, 86.0, 39.5, 86.0 ],
                    "source": [ "obj-solve", 0 ]
                }
            },
            {
                "patchline": {
                    "destination": [ "obj-8", 0 ],
                    "source": [ "obj-solver", 0 ]
                }
            },
            {
                "patchline": {
                    "destination": [ "obj-print-json", 0 ],
                    "midpoints": [ 66.5, 137.5, 176.0, 137.5 ],
                    "source": [ "obj-solver", 1 ]
                }
            },
            {
                "patchline": {
                    "destination": [ "obj-print-status", 0 ],
                    "midpoints": [ 120.5, 131.5, 282.0, 131.5 ],
                    "source": [ "obj-solver", 3 ]
                }
            },
            {
                "patchline": {
                    "destination": [ "obj-solver", 0 ],
                    "midpoints": [ 95.5, 86.0, 39.5, 86.0 ],
                    "source": [ "obj-status", 0 ]
                }
            }
        ],
        "autosave": 0
    }
}