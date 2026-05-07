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
		"rect" : [ 34.0, 94.0, 1660.0, 989.0 ],
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
					"id" : "obj-27",
					"maxclass" : "newobj",
					"numinlets" : 1,
					"numoutlets" : 1,
					"outlettype" : [ "" ],
					"patching_rect" : [ 861.0, 497.0, 231.0, 22.0 ],
					"text" : "prepend set dynamic_rules[0]::expression"
				}

			}
, 			{
				"box" : 				{
					"id" : "obj-23",
					"maxclass" : "number",
					"numinlets" : 1,
					"numoutlets" : 2,
					"outlettype" : [ "", "bang" ],
					"parameter_enable" : 0,
					"patching_rect" : [ 861.0, 432.0, 50.0, 22.0 ]
				}

			}
, 			{
				"box" : 				{
					"id" : "obj-21",
					"maxclass" : "newobj",
					"numinlets" : 1,
					"numoutlets" : 1,
					"outlettype" : [ "" ],
					"patching_rect" : [ 861.0, 468.0, 500.0, 22.0 ],
					"text" : "sprintf symout \"24 - abs((voice[1].pitch[i] - voice[0].pitch[i]) - %d)\""
				}

			}
, 			{
				"box" : 				{
					"id" : "obj-7",
					"maxclass" : "dict.view",
					"numinlets" : 1,
					"numoutlets" : 0,
					"patching_rect" : [ 27.0, 622.0, 427.0, 403.5 ]
				}

			}
, 			{
				"box" : 				{
					"id" : "obj-3",
					"maxclass" : "comment",
					"numinlets" : 1,
					"numoutlets" : 0,
					"patching_rect" : [ 1034.0, 87.0, 173.0, 20.0 ],
					"text" : "prefer fixed intervals (heuristic)"
				}

			}
, 			{
				"box" : 				{
					"id" : "obj-18",
					"maxclass" : "message",
					"numinlets" : 2,
					"numoutlets" : 1,
					"outlettype" : [ "" ],
					"patching_rect" : [ 41.0, 409.0, 203.0, 22.0 ],
					"text" : "6 5 5 5 5 5 5 6"
				}

			}
, 			{
				"box" : 				{
					"id" : "obj-2",
					"maxclass" : "newobj",
					"numinlets" : 2,
					"numoutlets" : 1,
					"outlettype" : [ "" ],
					"patching_rect" : [ 27.0, 365.0, 87.0, 22.0 ],
					"text" : "bach.- @out m"
				}

			}
, 			{
				"box" : 				{
					"id" : "obj-20",
					"maxclass" : "message",
					"numinlets" : 2,
					"numoutlets" : 1,
					"outlettype" : [ "" ],
					"patching_rect" : [ 354.0, 86.0, 666.0, 22.0 ],
					"text" : "config_file \"/Users/juanvassallo/Documents/Max 8/Packages/gecode-solver/examples/heuristic_all_positions_example.json\""
				}

			}
, 			{
				"box" : 				{
					"id" : "obj-16",
					"maxclass" : "comment",
					"numinlets" : 1,
					"numoutlets" : 0,
					"patching_rect" : [ 1029.0, 54.0, 112.0, 20.0 ],
					"text" : "12-tone palindrome"
				}

			}
, 			{
				"box" : 				{
					"id" : "obj-14",
					"maxclass" : "message",
					"numinlets" : 2,
					"numoutlets" : 1,
					"outlettype" : [ "" ],
					"patching_rect" : [ 166.0, 468.0, 669.0, 22.0 ],
					"text" : "read \"/Users/juanvassallo/Documents/Max 8/Packages/gecode-solver/examples/heuristic_all_positions_example.json\", bang"
				}

			}
, 			{
				"box" : 				{
					"id" : "obj-11",
					"maxclass" : "message",
					"numinlets" : 2,
					"numoutlets" : 1,
					"outlettype" : [ "" ],
					"patching_rect" : [ 508.0, 121.0, 162.0, 22.0 ],
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
					"patching_rect" : [ 174.0, 318.0, 321.0, 22.0 ],
					"text" : "83 81 79 77 76 74 72 71"
				}

			}
, 			{
				"box" : 				{
					"id" : "obj-12",
					"maxclass" : "message",
					"numinlets" : 2,
					"numoutlets" : 1,
					"outlettype" : [ "" ],
					"patching_rect" : [ 174.0, 355.0, 321.0, 22.0 ],
					"text" : "77 76 74 72 71 69 67 65"
				}

			}
, 			{
				"box" : 				{
					"id" : "obj-10",
					"maxclass" : "newobj",
					"numinlets" : 3,
					"numoutlets" : 3,
					"outlettype" : [ "", "", "" ],
					"patching_rect" : [ 27.0, 280.0, 100.0, 22.0 ],
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
					"patching_rect" : [ 27.0, 237.0, 99.0, 22.0 ],
					"text" : "route voice_pitch"
				}

			}
, 			{
				"box" : 				{
					"id" : "obj-5",
					"maxclass" : "message",
					"numinlets" : 2,
					"numoutlets" : 1,
					"outlettype" : [ "" ],
					"patching_rect" : [ 166.0, 497.0, 669.0, 22.0 ],
					"text" : "read \"/Users/juanvassallo/Documents/Max 8/Packages/gecode-solver/examples/twelve_tone_palindrome_config.json\", bang"
				}

			}
, 			{
				"box" : 				{
					"id" : "obj-4",
					"maxclass" : "message",
					"numinlets" : 2,
					"numoutlets" : 1,
					"outlettype" : [ "" ],
					"patching_rect" : [ 335.0, 53.0, 665.0, 22.0 ],
					"text" : "config_file \"/Users/juanvassallo/Documents/Max 8/Packages/gecode-solver/examples/twelve_tone_palindrome_config.json\""
				}

			}
, 			{
				"box" : 				{
					"id" : "obj-dict",
					"maxclass" : "newobj",
					"numinlets" : 2,
					"numoutlets" : 5,
					"outlettype" : [ "dictionary", "", "", "", "" ],
					"patching_rect" : [ 27.0, 587.0, 100.0, 22.0 ],
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
					"patching_rect" : [ 189.0, 533.0, 145.0, 22.0 ],
					"text" : "set solution_length 5"
				}

			}
, 			{
				"box" : 				{
					"id" : "obj-dict-config",
					"maxclass" : "message",
					"numinlets" : 2,
					"numoutlets" : 1,
					"outlettype" : [ "" ],
					"patching_rect" : [ 374.0, 121.0, 125.0, 22.0 ],
					"text" : "config_dict demo_cfg"
				}

			}
, 			{
				"box" : 				{
					"id" : "obj-comment-title",
					"maxclass" : "comment",
					"numinlets" : 1,
					"numoutlets" : 0,
					"patching_rect" : [ 30.0, 16.0, 138.0, 20.0 ],
					"text" : "gecode.solver examples"
				}

			}
, 			{
				"box" : 				{
					"id" : "obj-solve",
					"maxclass" : "message",
					"numinlets" : 2,
					"numoutlets" : 1,
					"outlettype" : [ "" ],
					"patching_rect" : [ 27.0, 60.0, 45.0, 22.0 ],
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
					"patching_rect" : [ 84.0, 60.0, 48.0, 22.0 ],
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
					"patching_rect" : [ 143.0, 60.0, 62.0, 22.0 ],
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
					"patching_rect" : [ 216.0, 60.0, 52.0, 22.0 ],
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
					"patching_rect" : [ 27.0, 187.0, 100.0, 22.0 ],
					"text" : "gecode.solver"
				}

			}
, 			{
				"box" : 				{
					"id" : "obj-print-json",
					"maxclass" : "newobj",
					"numinlets" : 1,
					"numoutlets" : 0,
					"patching_rect" : [ 130.0, 237.0, 132.0, 22.0 ],
					"text" : "print solver_json"
				}

			}
, 			{
				"box" : 				{
					"id" : "obj-print-status",
					"maxclass" : "newobj",
					"numinlets" : 1,
					"numoutlets" : 0,
					"patching_rect" : [ 273.0, 237.0, 140.0, 22.0 ],
					"text" : "print solver_status"
				}

			}
 ],
		"lines" : [ 			{
				"patchline" : 				{
					"destination" : [ "obj-12", 1 ],
					"midpoints" : [ 36.5, 349.5, 485.5, 349.5 ],
					"order" : 0,
					"source" : [ "obj-10", 0 ]
				}

			}
, 			{
				"patchline" : 				{
					"destination" : [ "obj-13", 1 ],
					"midpoints" : [ 77.0, 310.0, 485.5, 310.0 ],
					"order" : 0,
					"source" : [ "obj-10", 1 ]
				}

			}
, 			{
				"patchline" : 				{
					"destination" : [ "obj-2", 0 ],
					"order" : 1,
					"source" : [ "obj-10", 1 ]
				}

			}
, 			{
				"patchline" : 				{
					"destination" : [ "obj-2", 1 ],
					"order" : 1,
					"source" : [ "obj-10", 0 ]
				}

			}
, 			{
				"patchline" : 				{
					"destination" : [ "obj-solver", 0 ],
					"midpoints" : [ 517.5, 178.0, 36.5, 178.0 ],
					"source" : [ "obj-11", 0 ]
				}

			}
, 			{
				"patchline" : 				{
					"destination" : [ "obj-dict", 0 ],
					"midpoints" : [ 175.5, 571.0, 36.5, 571.0 ],
					"source" : [ "obj-14", 0 ]
				}

			}
, 			{
				"patchline" : 				{
					"destination" : [ "obj-18", 1 ],
					"midpoints" : [ 36.5, 397.5, 234.5, 397.5 ],
					"source" : [ "obj-2", 0 ]
				}

			}
, 			{
				"patchline" : 				{
					"destination" : [ "obj-solver", 0 ],
					"midpoints" : [ 363.5, 130.5, 36.5, 130.5 ],
					"source" : [ "obj-20", 0 ]
				}

			}
, 			{
				"patchline" : 				{
					"destination" : [ "obj-27", 0 ],
					"source" : [ "obj-21", 0 ]
				}

			}
, 			{
				"patchline" : 				{
					"destination" : [ "obj-21", 0 ],
					"source" : [ "obj-23", 0 ]
				}

			}
, 			{
				"patchline" : 				{
					"destination" : [ "obj-dict", 0 ],
					"midpoints" : [ 870.5, 573.5, 36.5, 573.5 ],
					"source" : [ "obj-27", 0 ]
				}

			}
, 			{
				"patchline" : 				{
					"destination" : [ "obj-solver", 0 ],
					"midpoints" : [ 344.5, 114.0, 36.5, 114.0 ],
					"source" : [ "obj-4", 0 ]
				}

			}
, 			{
				"patchline" : 				{
					"destination" : [ "obj-dict", 0 ],
					"midpoints" : [ 175.5, 571.5, 36.5, 571.5 ],
					"source" : [ "obj-5", 0 ]
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
					"destination" : [ "obj-solver", 0 ],
					"midpoints" : [ 225.5, 105.5, 36.5, 105.5 ],
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
					"midpoints" : [ 383.5, 161.0, 36.5, 161.0 ],
					"source" : [ "obj-dict-config", 0 ]
				}

			}
, 			{
				"patchline" : 				{
					"destination" : [ "obj-dict", 0 ],
					"midpoints" : [ 198.5, 570.0, 36.5, 570.0 ],
					"source" : [ "obj-dict-set-len", 0 ]
				}

			}
, 			{
				"patchline" : 				{
					"destination" : [ "obj-solver", 0 ],
					"midpoints" : [ 152.5, 105.5, 36.5, 105.5 ],
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
					"midpoints" : [ 77.0, 228.0, 139.5, 228.0 ],
					"source" : [ "obj-solver", 1 ]
				}

			}
, 			{
				"patchline" : 				{
					"destination" : [ "obj-print-status", 0 ],
					"midpoints" : [ 117.5, 222.0, 282.5, 222.0 ],
					"source" : [ "obj-solver", 2 ]
				}

			}
, 			{
				"patchline" : 				{
					"destination" : [ "obj-solver", 0 ],
					"midpoints" : [ 93.5, 105.5, 36.5, 105.5 ],
					"source" : [ "obj-status", 0 ]
				}

			}
 ],
		"dependency_cache" : [ 			{
				"name" : "bach.-.maxpat",
				"bootpath" : "~/Documents/Max 8/Packages/bach/patchers",
				"patcherrelativepath" : "../../bach/patchers",
				"type" : "JSON",
				"implicit" : 1
			}
, 			{
				"name" : "bach.args.mxo",
				"type" : "iLaX"
			}
, 			{
				"name" : "bach.expr.mxo",
				"type" : "iLaX"
			}
, 			{
				"name" : "bach.portal.mxo",
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
