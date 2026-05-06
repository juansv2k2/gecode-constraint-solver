{
	"patcher" : 	{
		"fileversion" : 1,
		"appversion" : 		{
			"major" : 8,
			"minor" : 6,
			"revision" : 5,
			"architecture" : "x64",
			"modernui" : 1
		}
,
		"classnamespace" : "box",
		"rect" : [ 34.0, 94.0, 900.0, 700.0 ],
		"bglocked" : 0,
		"openinpresentation" : 0,
		"default_fontsize" : 12.0,
		"default_fontface" : 0,
		"default_fontname" : "Arial",
		"gridonopen" : 1,
		"gridsize" : [ 15.0, 15.0 ],
		"gridsnaponopen" : 1,
		"objectsnaponopen" : 1,
		"statusbarvisible" : 2,
		"toolbarvisible" : 1,
		"lefttoolbarpinned" : 0,
		"toptoolbarpinned" : 0,
		"righttoolbarpinned" : 0,
		"bottomtoolbarpinned" : 0,
		"toolbars_unpinned_last_save" : 0,
		"tallnewobj" : 0,
		"boxanimatetime" : 200,
		"enablehscroll" : 1,
		"enablevscroll" : 1,
		"devicewidth" : 0.0,
		"description" : "",
		"digest" : "",
		"tags" : "",
		"style" : "",
		"subpatcher_template" : "",
		"assistshowspatchername" : 0,
		"boxes" : [ 			{
				"box" : 				{
					"id" : "obj-title",
					"maxclass" : "comment",
					"numinlets" : 1,
					"numoutlets" : 0,
					"patching_rect" : [ 30.0, 20.0, 600.0, 30.0 ],
					"fontface" : 1,
					"fontsize" : 18.0,
					"text" : "Heuristic-Guided Musical Melody Generator"
				}

			}
, 			{
				"box" : 				{
					"id" : "obj-desc",
					"maxclass" : "comment",
					"numinlets" : 1,
					"numoutlets" : 0,
					"patching_rect" : [ 30.0, 55.0, 700.0, 100.0 ],
					"text" : "This patcher demonstrates multi-priority heuristic constraint solving for music generation.\nIt generates 8-note melodies with:\n  • Hard constraints: No adjacent repeated notes, max interval = major sixth\n  • Heuristic priorities: Stepwise motion (P5) > Consonance (P3) > Centered pitch (P1)\nClicking 'Solve' will generate a new melody guided by these priorities."
				}

			}
, 			{
				"box" : 				{
					"id" : "obj-load",
					"maxclass" : "message",
					"numinlets" : 2,
					"numoutlets" : 1,
					"outlettype" : [ "" ],
					"patching_rect" : [ 30.0, 160.0, 550.0, 22.0 ],
					"text" : "config_file \"/Users/juanvassallo/Documents/Max 8/Packages/gecode-solver/configs/heuristic_example_musical_melody_8x1.json\""
				}

			}
, 			{
				"box" : 				{
					"id" : "obj-solve",
					"maxclass" : "message",
					"numinlets" : 2,
					"numoutlets" : 1,
					"outlettype" : [ "" ],
					"patching_rect" : [ 30.0, 195.0, 50.0, 22.0 ],
					"text" : "solve"
				}

			}
, 			{
				"box" : 				{
					"id" : "obj-status",
					"maxclass" : "message",
					"numinlets" : 2,
					"numoutlets" : 1,
					"outlettype" : [ "" ],
					"patching_rect" : [ 90.0, 195.0, 50.0, 22.0 ],
					"text" : "status"
				}

			}
, 			{
				"box" : 				{
					"id" : "obj-solver",
					"maxclass" : "newobj",
					"numinlets" : 1,
					"numoutlets" : 3,
					"outlettype" : [ "", "", "" ],
					"patching_rect" : [ 30.0, 235.0, 100.0, 22.0 ],
					"text" : "gecode.solver"
				}

			}
, 			{
				"box" : 				{
					"id" : "obj-print-solution",
					"maxclass" : "print",
					"numinlets" : 1,
					"numoutlets" : 0,
					"patching_rect" : [ 30.0, 280.0, 200.0, 80.0 ]
				}

			}
, 			{
				"box" : 				{
					"id" : "obj-print-status",
					"maxclass" : "print",
					"numinlets" : 1,
					"numoutlets" : 0,
					"patching_rect" : [ 240.0, 280.0, 200.0, 80.0 ]
				}

			}
, 			{
				"box" : 				{
					"id" : "obj-print-debug",
					"maxclass" : "print",
					"numinlets" : 1,
					"numoutlets" : 0,
					"patching_rect" : [ 450.0, 280.0, 200.0, 80.0 ]
				}

			}
, 			{
				"box" : 				{
					"id" : "obj-info",
					"maxclass" : "comment",
					"numinlets" : 1,
					"numoutlets" : 0,
					"patching_rect" : [ 30.0, 375.0, 700.0, 120.0 ],
					"text" : "How to use:\n1. Click 'Load Config' message to initialize with the heuristic config\n2. Click 'Solve' button to generate a melody\n3. The three print objects show:\n   - Left: Generated melody pitches (voice_pitch outlet)\n   - Middle: Solver status and performance\n   - Right: Debug info and raw JSON output\n\nEach solve will generate a different melody, all satisfying hard constraints while being guided by heuristic priorities.\nThe higher-priority heuristics dominate when multiple preferences apply."
				}

			}
 ],
		"lines" : [ 			{
				"patchline" : 				{
					"destination" : [ "obj-solver", 0 ],
					"source" : [ "obj-load", 0 ]
				}

			}
, 			{
				"patchline" : 				{
					"destination" : [ "obj-solver", 0 ],
					"source" : [ "obj-solve", 0 ]
				}

			}
, 			{
				"patchline" : 				{
					"destination" : [ "obj-solver", 0 ],
					"source" : [ "obj-status", 0 ]
				}

			}
, 			{
				"patchline" : 				{
					"destination" : [ "obj-print-solution", 0 ],
					"source" : [ "obj-solver", 0 ]
				}

			}
, 			{
				"patchline" : 				{
					"destination" : [ "obj-print-status", 0 ],
					"source" : [ "obj-solver", 2 ]
				}

			}
, 			{
				"patchline" : 				{
					"destination" : [ "obj-print-debug", 0 ],
					"source" : [ "obj-solver", 1 ]
				}

			}
 ],
		"dependency_cache" : [ 			{
				"name" : "gecode.solver.mxo",
				"type" : "iLaX"
			}
 ],
		"autosave" : 0
	}

}
