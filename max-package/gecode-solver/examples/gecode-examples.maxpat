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
		"rect" : [ -2526.0, -378.0, 2492.0, 1344.0 ],
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
					"id" : "obj-22",
					"maxclass" : "newobj",
					"numinlets" : 6,
					"numoutlets" : 1,
					"outlettype" : [ "" ],
					"patching_rect" : [ 167.682930827140808, 313.414641618728638, 100.0, 22.0 ],
					"saved_object_attributes" : 					{
						"versionnumber" : 80300
					}
,
					"text" : "bach.beatbox"
				}

			}
, 			{
				"box" : 				{
					"id" : "obj-11",
					"maxclass" : "message",
					"numinlets" : 2,
					"numoutlets" : 1,
					"outlettype" : [ "" ],
					"patching_rect" : [ 276.0, 53.0, 162.0, 22.0 ],
					"text" : "config_dict_debug demo_cfg"
				}

			}
, 			{
				"box" : 				{
					"id" : "obj-13",
					"maxclass" : "message",
					"numinlets" : 2,
					"numoutlets" : 1,
					"outlettype" : [ "" ],
					"patching_rect" : [ 51.219513416290283, 371.951228380203247, 321.0, 22.0 ],
					"text" : "77 68 63 73 67 61 64 75 69 65 71 72 70 62 74 76 66 60"
				}

			}
, 			{
				"box" : 				{
					"id" : "obj-12",
					"maxclass" : "message",
					"numinlets" : 2,
					"numoutlets" : 1,
					"outlettype" : [ "" ],
					"patching_rect" : [ 42.682927846908569, 418.902449011802673, 321.0, 22.0 ],
					"text" : "65 64 65 64 65 64"
				}

			}
, 			{
				"box" : 				{
					"id" : "obj-10",
					"maxclass" : "newobj",
					"numinlets" : 3,
					"numoutlets" : 3,
					"outlettype" : [ "", "", "" ],
					"patching_rect" : [ 26.829268932342529, 321.951227188110352, 56.0, 22.0 ],
					"text" : "route 0 1"
				}

			}
, 			{
				"box" : 				{
					"id" : "obj-8",
					"maxclass" : "newobj",
					"numinlets" : 2,
					"numoutlets" : 2,
					"outlettype" : [ "", "" ],
					"patching_rect" : [ 28.048781156539917, 183.536589741706848, 99.0, 22.0 ],
					"text" : "route voice_pitch"
				}

			}
, 			{
				"box" : 				{
					"id" : "obj-9",
					"maxclass" : "number",
					"numinlets" : 1,
					"numoutlets" : 2,
					"outlettype" : [ "", "bang" ],
					"parameter_enable" : 0,
					"patching_rect" : [ 1150.609783530235291, 274.390250444412231, 50.0, 22.0 ]
				}

			}
, 			{
				"box" : 				{
					"fontname" : "Arial",
					"fontsize" : 13.0,
					"id" : "obj-6",
					"maxclass" : "message",
					"numinlets" : 2,
					"numoutlets" : 1,
					"outlettype" : [ "" ],
					"patching_rect" : [ 1150.609783530235291, 303.658543825149536, 222.0, 23.0 ],
					"text" : "set search_options::random_seed $1"
				}

			}
, 			{
				"box" : 				{
					"fontname" : "Arial",
					"fontsize" : 13.0,
					"id" : "obj-1",
					"maxclass" : "message",
					"numinlets" : 2,
					"numoutlets" : 1,
					"outlettype" : [ "" ],
					"patching_rect" : [ 760.365871787071228, 303.658543825149536, 357.0, 23.0 ],
					"text" : "set engine_domains::engine_0::duration_values 1/8 1/4 1/16"
				}

			}
, 			{
				"box" : 				{
					"id" : "obj-3",
					"maxclass" : "button",
					"numinlets" : 1,
					"numoutlets" : 1,
					"outlettype" : [ "bang" ],
					"parameter_enable" : 0,
					"patching_rect" : [ 487.195133566856384, 210.365858674049377, 24.0, 24.0 ]
				}

			}
, 			{
				"box" : 				{
					"id" : "obj-7",
					"maxclass" : "dict.view",
					"numinlets" : 1,
					"numoutlets" : 0,
					"patching_rect" : [ 487.195133566856384, 348.780496120452881, 625.0, 488.0 ]
				}

			}
, 			{
				"box" : 				{
					"id" : "obj-dict",
					"maxclass" : "newobj",
					"numinlets" : 2,
					"numoutlets" : 5,
					"outlettype" : [ "dictionary", "", "", "", "" ],
					"patching_rect" : [ 487.195133566856384, 274.390250444412231, 100.0, 22.0 ],
					"saved_object_attributes" : 					{
						"embed" : 0,
						"legacy" : 1,
						"parameter_enable" : 0,
						"parameter_mappable" : 0
					}
,
					"text" : "dict demo_cfg"
				}

			}
, 			{
				"box" : 				{
					"id" : "obj-dict-set-len",
					"maxclass" : "message",
					"numinlets" : 2,
					"numoutlets" : 1,
					"outlettype" : [ "" ],
					"patching_rect" : [ 599.390258193016052, 303.658543825149536, 145.0, 22.0 ],
					"text" : "set solution_length 24"
				}

			}
, 			{
				"box" : 				{
					"id" : "obj-dict-config",
					"maxclass" : "message",
					"numinlets" : 2,
					"numoutlets" : 1,
					"outlettype" : [ "" ],
					"patching_rect" : [ 444.512205719947815, 53.0, 125.0, 22.0 ],
					"text" : "config_dict demo_cfg"
				}

			}
, 			{
				"box" : 				{
					"id" : "obj-comment-title",
					"maxclass" : "comment",
					"numinlets" : 1,
					"numoutlets" : 0,
					"patching_rect" : [ 30.0, 16.0, 282.0, 20.0 ],
					"text" : "gecode.solver help - async musical constraint solve"
				}

			}
, 			{
				"box" : 				{
					"id" : "obj-solve",
					"maxclass" : "message",
					"numinlets" : 2,
					"numoutlets" : 1,
					"outlettype" : [ "" ],
					"patching_rect" : [ 30.0, 53.0, 45.0, 22.0 ],
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
					"patching_rect" : [ 86.0, 53.0, 48.0, 22.0 ],
					"text" : "status"
				}

			}
, 			{
				"box" : 				{
					"id" : "obj-getlast",
					"maxclass" : "message",
					"numinlets" : 2,
					"numoutlets" : 1,
					"outlettype" : [ "" ],
					"patching_rect" : [ 145.0, 53.0, 62.0, 22.0 ],
					"text" : "get_last"
				}

			}
, 			{
				"box" : 				{
					"id" : "obj-cancel",
					"maxclass" : "message",
					"numinlets" : 2,
					"numoutlets" : 1,
					"outlettype" : [ "" ],
					"patching_rect" : [ 218.0, 53.0, 52.0, 22.0 ],
					"text" : "cancel"
				}

			}
, 			{
				"box" : 				{
					"id" : "obj-solver",
					"maxclass" : "newobj",
					"numinlets" : 1,
					"numoutlets" : 3,
					"outlettype" : [ "", "", "" ],
					"patching_rect" : [ 28.048781156539917, 133.536588549613953, 100.0, 22.0 ],
					"text" : "gecode.solver"
				}

			}
, 			{
				"box" : 				{
					"id" : "obj-print-json",
					"maxclass" : "newobj",
					"numinlets" : 1,
					"numoutlets" : 0,
					"patching_rect" : [ 187.195126414299011, 183.536589741706848, 96.0, 22.0 ],
					"text" : "print solver_json"
				}

			}
, 			{
				"box" : 				{
					"id" : "obj-print-status",
					"maxclass" : "newobj",
					"numinlets" : 1,
					"numoutlets" : 0,
					"patching_rect" : [ 285.365860462188721, 183.536589741706848, 106.0, 22.0 ],
					"text" : "print solver_status"
				}

			}
, 			{
				"box" : 				{
					"fontface" : 1,
					"fontsize" : 16.0,
					"id" : "obj-heur-title",
					"maxclass" : "comment",
					"numinlets" : 1,
					"numoutlets" : 0,
					"patching_rect" : [ 1199.390272498130798, 457.92684018611908, 750.0, 24.0 ],
					"text" : "HEURISTIC-GUIDED MUSICAL CONSTRAINT SOLVING EXAMPLE"
				}

			}
, 			{
				"box" : 				{
					"id" : "obj-heur-desc",
					"linecount" : 5,
					"maxclass" : "comment",
					"numinlets" : 1,
					"numoutlets" : 0,
					"patching_rect" : [ 1199.390272498130798, 487.804889678955078, 785.0, 74.0 ],
					"text" : "This example demonstrates combining hard constraints with multi-priority heuristics. The solver generates smooth 8-note melodies constrained by:\n • Hard: No repeated adjacent notes, max interval = major sixth\n • Priority 5 (highest): Prefer stepwise motion for smooth melody\n • Priority 3 (medium): Prefer consonant scale degrees\n • Priority 1 (lowest): Prefer pitches near middle of range"
				}

			}
, 			{
				"box" : 				{
					"id" : "obj-heur-guide",
					"linecount" : 3,
					"maxclass" : "comment",
					"numinlets" : 1,
					"numoutlets" : 0,
					"patching_rect" : [ 1199.390272498130798, 587.804892063140869, 750.0, 47.0 ],
					"text" : "How to use: Click the message above to load the config, then click 'solve' button at top. The heuristic priorities guide the search—higher priority heuristics dominate when multiple preferences apply. Result shows melody that satisfies hard constraints while optimizing heuristic preferences."
				}

			}
 ],
		"lines" : [ 			{
				"patchline" : 				{
					"destination" : [ "obj-dict", 0 ],
					"source" : [ "obj-1", 0 ]
				}

			}
, 			{
				"patchline" : 				{
					"destination" : [ "obj-12", 1 ],
					"midpoints" : [ 36.329268932342529, 407.731714367866516, 354.182927846908569, 407.731714367866516 ],
					"source" : [ "obj-10", 0 ]
				}

			}
, 			{
				"patchline" : 				{
					"destination" : [ "obj-13", 1 ],
					"midpoints" : [ 54.829268932342529, 353.597567677497864, 362.719513416290283, 353.597567677497864 ],
					"source" : [ "obj-10", 1 ]
				}

			}
, 			{
				"patchline" : 				{
					"destination" : [ "obj-solver", 0 ],
					"source" : [ "obj-11", 0 ]
				}

			}
, 			{
				"patchline" : 				{
					"destination" : [ "obj-dict", 0 ],
					"source" : [ "obj-3", 0 ]
				}

			}
, 			{
				"patchline" : 				{
					"destination" : [ "obj-dict", 0 ],
					"source" : [ "obj-6", 0 ]
				}

			}
, 			{
				"patchline" : 				{
					"destination" : [ "obj-10", 0 ],
					"source" : [ "obj-8", 0 ]
				}

			}
, 			{
				"patchline" : 				{
					"destination" : [ "obj-6", 0 ],
					"source" : [ "obj-9", 0 ]
				}

			}
, 			{
				"patchline" : 				{
					"destination" : [ "obj-solver", 0 ],
					"source" : [ "obj-cancel", 0 ]
				}

			}
, 			{
				"patchline" : 				{
					"destination" : [ "obj-7", 0 ],
					"source" : [ "obj-dict", 0 ]
				}

			}
, 			{
				"patchline" : 				{
					"destination" : [ "obj-solver", 0 ],
					"source" : [ "obj-dict-config", 0 ]
				}

			}
, 			{
				"patchline" : 				{
					"destination" : [ "obj-dict", 0 ],
					"source" : [ "obj-dict-set-len", 0 ]
				}

			}
, 			{
				"patchline" : 				{
					"destination" : [ "obj-solver", 0 ],
					"source" : [ "obj-getlast", 0 ]
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
					"destination" : [ "obj-8", 0 ],
					"source" : [ "obj-solver", 0 ]
				}

			}
, 			{
				"patchline" : 				{
					"destination" : [ "obj-print-json", 0 ],
					"midpoints" : [ 78.048781156539917, 174.036589741706848, 196.695126414299011, 174.036589741706848 ],
					"source" : [ "obj-solver", 1 ]
				}

			}
, 			{
				"patchline" : 				{
					"destination" : [ "obj-print-status", 0 ],
					"midpoints" : [ 118.548781156539917, 168.036589741706848, 294.865860462188721, 168.036589741706848 ],
					"source" : [ "obj-solver", 2 ]
				}

			}
, 			{
				"patchline" : 				{
					"destination" : [ "obj-solver", 0 ],
					"source" : [ "obj-status", 0 ]
				}

			}
 ],
		"dependency_cache" : [ 			{
				"name" : "bach.beatbox.mxo",
				"type" : "iLaX"
			}
, 			{
				"name" : "gecode.solver.mxo",
				"type" : "iLaX"
			}
 ],
		"autosave" : 0
	}

}
