/*
** Copyright 2014-2016 The Earlham Institute
** 
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
** 
**     http://www.apache.org/licenses/LICENSE-2.0
** 
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/

var G_METADATA_API_URL_S = "/data/api/metadata/";
var G_ROOT_URL_S = "/eirods_dav_files/";

$(document).ready (function () {

  var listings_table = $("#listings_table");

  var metadata_cells = $(listings_table).find ("td.metatable")

  $(metadata_cells).each (function () {
    SetUpGeneralMetadataButtons ($(this));
  });

  if ($(listings_table).hasClass ("ajax")) {
    AddMetadataToggleButtons (metadata_cells, CallGetMetadata, true);
  } else {
    CalculateWidth ();

    AddMetadataToggleButtons (metadata_cells, null, true);
  
    if ($(listings_table).hasClass ("editable")) {
      $(metadata_cells).each (function () {
        SetUpEditMetadataButtons ($(this));
        SetUpDeleteMetadataButtons ($(this)); 
      });

      SetUpMetadataEditor ();

    } 


  }

	SetUpMetadataKeysAutoCompleteList ();
	SetUpMetadataValuesAutoCompleteList ();

 // SetUpMouseOvers ();
});


function AddMetadataToggleButtons (cells, callback_fn, closed_flag) {
 $(cells).each (function () {
    $(this).html ("");
    AddMetadataToggleButton ($ (this), callback_fn, closed_flag);
  });
}



function AddMetadataToggleButton (table_cell, callback_fn, closed_flag) {
  var table_row = $(table_cell).parent ();
  var i = $(table_row).attr ("id");
  var container = $(table_cell).find ("ul.metadata:first");
	var IMAGES_DIR_S = G_ROOT_URL_S + "images/";

  var collapse_src = IMAGES_DIR_S + "list_node_collapse";
  var expand_src = IMAGES_DIR_S + "list_node_expand";

	/* 
	** If no button exists, then add it 
	*/
	if ($(table_cell).find ("img.node").length === 0) {
		var button_src = "<img class='button node' src=" + ((closed_flag === true) ? expand_src : collapse_src) + " />";

		if (container.length != 0) {
		  container.before (button_src);
		} else {
		  $(table_cell).append (button_src);
		}
	}

  $(table_cell).find ("img.node").on ('click', function () {
//	  var metadata_list = $(table_cell).find ("ul.metadata:first");
	var metadata_list = $(table_cell).find ("div.metadata_container");
    
    if ($(this).attr ('src') === collapse_src) {
      $(this).attr ('src', expand_src);

//      if (callback_fn != null) {
//        callback_fn ($(table_cell), false);        
//      }

      $(metadata_list).hide ("slideDown");

    } else {
      $(this).attr ('src', collapse_src);

      if (callback_fn != null) {
        callback_fn ($(table_cell), true);        
      }

      $(metadata_list).show ("slideDown");

    }
  });

}


function GetMetadataList (table_cell, irods_id, show_flag) {
	var rest_url = G_METADATA_API_URL_S + "get?id=" + irods_id;

	$.ajax (rest_url, {
		dataType: "html",
		success: function (data, status) {
		  if (status === "success") {

				/* if there is an old list, then remove it */
        var metadata_containers = $(table_cell).find ("div.metadata_container:first");

				if ($(metadata_containers).length > 0) {
					$(metadata_containers).each (function () {
						$(this).replaceWith (data);
					});
				} else {
					$(table_cell).append (data);
				}

				SetUpGeneralMetadataButtons (table_cell);
				SetUpEditMetadataButtons (table_cell);
				SetUpDeleteMetadataButtons (table_cell);

	      SetUpMetadataEditor ();

				/*
				var callback_fn = null;
 		 		if ($("#listings_table").hasClass ("ajax")) {
					callback_fn = CallGetMetadata;
				}

        AddMetadataToggleButton (table_cell, callback_fn, false);
				*/

        if (show_flag) {
          metadata_container = $(table_cell).find ("ul.metadata");
          $(metadata_container).show ('slideDown');
        }
    	} else {
        alert ("Failed to get metadata");
      }
		}
	});
}


function SetUpGeneralMetadataButtons (table_cell) {
	SetUpAddMetadataButtons (table_cell);
	SetUpDownloadMetadataButtons (table_cell);
}


function CallGetMetadata (table_cell, show_flag) {
  var metadata_list = $(table_cell).find ("ul.metadata");

  if (metadata_list.length == 0) {
    var table_row = $(table_cell).parent ();
    var irods_id = $(table_row).attr ('id');

    GetMetadataList ($(table_cell), irods_id, show_flag);
    metadata_list  = $(table_cell).find ("ul.metadata:first");

    if (show_flag === true) {
      $(metadata_list).show ();
    } else if (show_flag === false) {
      $(metadata_list).hide ();
    }

  }

}



function SetUpDeleteMetadataButtons (parent_element) {
	$(parent_element).find ("img.delete_metadata").each (function () {
    var t = $(this);
    
    $(this).on ('click', function () {
      var listitem = $(t).parent ();
      var metadata_list = $(listitem).parent ();
      var table_cell = $(metadata_list).parent ().parent ();
      var table_row = $(table_cell).parent ();
      var key = null; 

      $(listitem).find ("span.key:first").each (function () {
        key = ($(this).html ());
      });

      if (key) {
        var value = null;
  
        $(listitem).find ("span.value:first").each (function () {
          value = ($(this).html ());
        });

        if (value) {

          var metadata_id = $(table_row).attr ('id');

          if (metadata_id) {
            var rest_url = G_METADATA_API_URL_S + "delete?id=" + metadata_id + "&key=" + encodeURIComponent (key) + "&value=" + encodeURIComponent (value);
						var obj_name = null;

			      $(table_row).find ("td.name:first a").each (function () {
			        obj_name = $(this).html ();
			      });

		        var confirm = window.confirm ("Are you sure you wish to delete \"" + key + " = " + value  + "\" from " + obj_name + "?");
   
						if (confirm === true) {

							$.ajax (rest_url, {
								dataType: "json",
								success: function (data, status) {
									if (status === "success") {
//					          AddMetadataAJAXToggleButton ($ (table_cell))
					          GetMetadataList ($(table_cell), metadata_id, true);
									}
								},
                error: function (header, status, error_string) {
                  alert ("failed to delete metadata");
                }
							});
						}

          }
      
        }
      
      }

    });

  });
}


function SetUpEditMetadataButtons (parent_element) {
	$(parent_element).find ("img.edit_metadata").each (function () {
    var t = $(this);
    
    $(this).on ('click', function () {

      var listitem = $(t).parent ();
      var metadata_list = $(listitem).parent ();
      var table_cell = $(metadata_list).parent ().parent ();
      var table_row = $(table_cell).parent ();
      var key = null; 

      $(listitem).find ("span.key:first").each (function () {
        key = $(this).html ();
      });

      if (key) {
        var value = null;

        $(listitem).find ("span.value:first").each (function () {
          value = $(this).html ();
        });

        if (value) {
          var units = "";

          $(listitem).find ("span.units:first").each (function () {
            units = $(this).html ();
          });

          var metadata_id = $(table_row).attr ('id');
					var obj_name = null;

          $(table_row).find ("td.name:first a").each (function () {
            obj_name = $(this).html ();
          });

          if (metadata_id) {

            if (obj_name) {
              $("#metadata_editor_title").html ("Edit metadata for " + obj_name);
            
              $("#id_editor").val (metadata_id);
              $("#metadata_editor").attr ("action", G_METADATA_API_URL_S + "edit");

              $("#attribute_editor").val (key);
              $("#value_editor").val (value);
              $("#units_editor").val (units);
              $("#new_attribute_editor").val (key);
              $("#new_value_editor").val (value);
              $("#new_units_editor").val (units);

              $("#edit_metadata_pop_up .add_group").hide ();
              $("#edit_metadata_pop_up .edit_group").show ();

              $("#edit_metadata_pop_up").show ();
            } 


          }

    
        }

      }

    });

  });

}


function SetUpMouseOvers () {
  $("img.button").each (function () {
    var button = $(this);

    $(this).hover (function () {
      var src = $(this).attr ("src");
      src += "_s";

      $(this).attr (src);
    }, function () {
      var src = $(this).attr ("src");
      var i = src.lastIndexOf ("_s");

      if (i != -1) {
        src = src.substring (0, i);
        $(this).attr (src);
      }
    });
  });
}

function SetUpAddMetadataButtons (parent_element) {
	$(parent_element).find ("span.add_metadata img.button").each (function () {
    var t = $(this); 
    
    $(this).on ('click', function () {
      var this_table_cell = $(t).parent ().parent ().parent ();
      var table_row = $(this_table_cell).parent ();
      var obj_name = null;
      var metadata_id = $(table_row).attr ('id');

      $(table_row).find ("td.name:first a").each (function () {
        obj_name = $(this).html ();
      });

      if (metadata_id) {

        if (obj_name) {
          $("#metadata_editor_title").html ("Add new metadata for " + obj_name);
        } else {
          $("#metadata_editor_title").html ("Add new metadata");          
        }

        $("#id_editor").val (metadata_id);
        $("#id_metadata_editor").attr ("action", "add");

        $("#attribute_editor").val ("");
        $("#value_editor").val ("");
        $("#units_editor").val ("");

        $("#new_attribute_editor").val ("");
        $("#new_value_editor").val ("");
        $("#new_units_editor").val ("");

        $("#edit_metadata_pop_up .add_group").show ();
        $("#edit_metadata_pop_up .edit_group").hide ();

        $("#edit_metadata_pop_up").show ();
      }

    });

  });

}


function SetUpDownloadMetadataButtons (table_cell) {
	$(table_cell).find ("form.download_metadata").each (function () {
		var f = $(this);

		$(this).find ("img.submit").each (function () {
				$(this).on ('click', function () {
					f.submit ();
				});
		});

		$(this).find ("a.submit").each (function () {
				$(this).on ('click', function () {
					f.submit ();
				});
		});

	});

}



function SetMetadataEditorSubmission () {
  $("#metadata_editor").submit (function (e) {
    var object_id = $("#id_editor").val ();
    var table_row = $(GetEscapedId (object_id));
    var table_cell = $(table_row).find ("td.metatable:first");
    var rest_url = $("#metadata_editor").attr ("action");
    var form_data = $("#metadata_editor").serialize ();

		rest_url = rest_url + "?" + form_data;

		$.ajax (rest_url, {
			dataType: "json",
			success: function (data, status) {
				if (status === "success") {
		      GetMetadataList ($(table_cell), object_id, true);
				}
			},
      error: function (header, status, error_string) {
        alert ("failed to delete metadata");
      }
		});

    /* stop the non-ajax submission */
    e.preventDefault (); 

    $("#edit_metadata_pop_up").hide ();
  });

  $("#cancel_metadata").click (function (e) {
    $("#edit_metadata_pop_up").hide ();
    e.preventDefault ();
  });
}


function SetUpMetadataEditor () {
  $("img.edit_metadata").click (function () {
    $("#edit_metadata_pop_up").show ();
  });

  $("img#save_metadata").click (function () {
    $("#metadata_editor").submit ();
  });

  SetMetadataEditorSubmission ();
}


/*
Taken from 
http://learn.jquery.com/using-jquery-core/faq/how-do-i-select-an-element-by-an-id-that-has-characters-used-in-css-notation/
*/
function GetEscapedId (id_string) {
  return "#" + id_string.replace (/(:|\.|\[|\]|,|=|@)/g, "\\$1"); 
}

function CalculateWidth () {
  var width = 0;
  
  $("#listings_table tr").each (function() {
    var t = $(this);
    var i = $(this).attr("id");
    var metadata_container = $(this).find(".metadata_container");
    
    var properties_cell = $(t).find ("td.metatable");
    
    var cloned_metalist = $(metadata_container).clone().css({
        'visibility': 'visible',
        'position': 'absolute',
        'z-index': '-99999',
        'left': '99999999px',
        'top': '0px'
    }).appendTo('body');

    $(cloned_metalist).width ("10000px");
      
    $(cloned_metalist).find ("ul.metadata").show ();
    $(cloned_metalist).find ('li').each (function() {
      $(this).show ();
      
      var w = GetTextWidth ($(this)); // $(this).width (); //
      if (width < w) {
        width = w;
      }
    });

    $(cloned_metalist).remove();
  });

  width += 50;
  $("th.properties").width (width);
}

function GetTextWidth (value) {
	var html_org = $(value).html();
  var html_calc = '<span>' + html_org + '</span>';
  $(value).html(html_calc);
  var width =  $(value).find('span:first').width();
  $(value).html(html_org);
  return width;
 }


function SetUpMetadataKeysAutoCompleteList () {
	$("#search_key").keyup (function () {
		var key = $(this).val ();	
		var rest_url = G_METADATA_API_URL_S + "keys?key=" + key;

		$.ajax (rest_url, {
			dataType: "json",
			success: function (data, status) {
				if (status === "success") {
					PopulateAutoCompleteList ($ ("#search_keys_autocomplete_list"), $("#search_key"), data.keys);
				}
			},
		  error: function (header, status, error_string) {
		    alert ("failed to get AutoCompleteList");
		  }
		});
	});
}



function SetUpMetadataValuesAutoCompleteList () {
	$("#search_value").keyup (function () {
		var key = $("#search_key").val ();	
		var value = $("#search_value").val ();	
		var rest_url = G_METADATA_API_URL_S + "values?key=" + key + "&value=" + value;

		$.ajax (rest_url, {
			dataType: "json",
			success: function (data, status) {
				if (status === "success") {
					PopulateAutoCompleteList ($ ("#search_values_autocomplete_list"), $("#search_value"), data.values);
				}
			},
		  error: function (header, status, error_string) {
		    alert ("failed to get AutoCompleteList");
		  }
		});
	});
}



function PopulateAutoCompleteList (list_id, input_box_id, values_array) {
	var i;

	$(list_id).empty ();

	if (values_array.length > 0) {
		for (i = 0; i < values_array.length; ++ i) {
			var list_item = $ ("<li>" + values_array [i] + "</li>");
			$(list_id).append ($(list_item));

			$(list_item).css ({
				"padding-left": $(input_box_id).css ("padding-left")
			});

			$(list_item).click (function () {
				$(input_box_id).val ($(this).text ());
				$(list_id).hide ();
			});
		}

		$(list_id).css ({
			"left": $(input_box_id).offset ().left,
			"top": $(input_box_id).offset ().top + $(input_box_id).height () + 10,
			"width": $(input_box_id).width (),
			"padding-left": $(input_box_id).css ("padding-left"),
			"font-size": $(input_box_id).css ("font-size")
		});

		$(list_id).show ();
	}

} 


