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
		"rect" : [ 134.0, 166.0, 980.0, 620.0 ],
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
					"id" : "obj-2",
					"maxclass" : "message",
					"numinlets" : 2,
					"numoutlets" : 1,
					"outlettype" : [ "" ],
					"patching_rect" : [ 287.0, 53.0, 565.0, 22.0 ],
					"text" : "config_file \"/Users/juanvassallo/Documents/Max 8/Packages/gecode-solver/examples/demo_config.json\""
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
					"patching_rect" : [ 30.0, 97.0, 100.0, 22.0 ],
					"text" : "gecode.solver"
				}

			}
, 			{
				"box" : 				{
					"id" : "obj-print-list",
					"maxclass" : "newobj",
					"numinlets" : 1,
					"numoutlets" : 0,
					"patching_rect" : [ 30.0, 147.0, 130.0, 22.0 ],
					"text" : "print solver_list"
				}

			}
, 			{
				"box" : 				{
					"id" : "obj-print-json",
					"maxclass" : "newobj",
					"numinlets" : 1,
					"numoutlets" : 0,
					"patching_rect" : [ 180.0, 147.0, 132.0, 22.0 ],
					"text" : "print solver_json"
				}

			}
, 			{
				"box" : 				{
					"id" : "obj-print-status",
					"maxclass" : "newobj",
					"numinlets" : 1,
					"numoutlets" : 0,
					"patching_rect" : [ 330.0, 147.0, 140.0, 22.0 ],
					"text" : "print solver_status"
				}

			}
, 			{
				"box" : 				{
					"id" : "obj-comment-note",
					"maxclass" : "comment",
					"numinlets" : 1,
					"numoutlets" : 0,
					"patching_rect" : [ 30.0, 187.0, 508.0, 20.0 ],
					"text" : "Tip: replace the config payload with contents from examples/demo_config.json for larger tests."
				}

			}
 ],
		"lines" : [ 			{
				"patchline" : 				{
					"destination" : [ "obj-solver", 0 ],
					"source" : [ "obj-2", 0 ]
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
					"destination" : [ "obj-print-json", 0 ],
					"source" : [ "obj-solver", 1 ]
				}

			}
, 			{
				"patchline" : 				{
					"destination" : [ "obj-print-list", 0 ],
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
					"destination" : [ "obj-solver", 0 ],
					"source" : [ "obj-status", 0 ]
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
