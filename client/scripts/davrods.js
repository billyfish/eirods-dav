$(document).ready (function () {
  if ($("#listings_table").hasClass ("ajax")) {
    AddMetadataAJAXToggleButtons ();
  } else {
    CalculateWidth ();
    AddMetadataToggleButtons ();
  }

  SetUpMetadataEditor ();

	var listings_table = $("#listings_table");
  SetUpAddMetadataButtons (listings_table);
  SetUpEditMetadataButtons (listings_table);
  SetUpDeleteMetadataButtons (listings_table);
 // SetUpMouseOvers ();
});


function AddMetadataAJAXToggleButtons () {
 $('#listings_table a.get_metadata').each (function () {
    $(this).html ("");
    AddMetadataAJAXToggleButton ($ (this));
  });
}


function AddMetadataAJAXToggleButton (parent_element) {
  var collapse_src = "/davrods_files/images/list_node_collaps";
  var expand_src = "/davrods_files/images/list_node_expand";
  var img_def = "<img class='button' src='" + expand_src + "' />";
  var button = $(img_def);

  $(parent_element).append ($(button));
  
  $(button).on('click', function() {
    var metadata_list = $(parent_element).find ("ul.metadata");

    if (metadata_list.length == 0) {
      var irods_id = $(t).attr ('id');
      GetMetadataList ($ (t), irods_id, false);
      metadata_list  = $(t).find ("ul.metadata"); 
    }

    
    if ($(this).attr ('src') === collapse_src) {
      $(this).attr ('src', expand_src);
      metadata_list.each (function () {
        $(this).hide ();
      });
		
			$(parent_element).find (".add_metadata").each (function () {
        $(this).hide ();
			});
    
		} else if ($(this).attr ('src') === expand_src) {
      $(this).attr ('src', collapse_src);
      metadata_list.each (function () {
        $(this).show ();
      });

		$(parent_element).find (".add_metadata").each (function () {
        $(this).show ();
			});
    
 		}
  });

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
            var rest_url = "/davrods/api/metadata/delete?id=" + metadata_id + "&key=" + encodeURIComponent (key) + "&value=" + encodeURIComponent (value);
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
              $("#metadata_editor").attr ("action", "/davrods/api/metadata/edit");

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


function AddMetadataToggleButtons () {
  $('#listings_table > tbody > tr').each(function() {
    var t = $(this);
    var i = $(this).attr("id");
    var container = $(this).find("ul.metadata");

    var collapse_src = "/davrods_files/images/list_node_collaps";
    var expand_src = "/davrods_files/images/list_node_expand";

    container.before("<img class='button node' src=" + expand_src + " />");
    
    $(this).find(".metatable img.node").on('click', function() {
      
      if ($(this).attr ('src') === collapse_src) {
        $(this).attr ('src', expand_src);
				$(container).hide ("slideDown");
 
     	} else {
        $(this).attr ('src', collapse_src);
				$(container).show ("slideDown");
			}
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

		      AddMetadataAJAXToggleButton ($ (table_cell))
		      GetMetadataList ($(table_cell), object_id, true);
				}
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

function DoLocalStyling() {

  // convert lists into listgroups
  $(".metadata").addClass("list-group panel-collapse collapse");
   
  $(".metadata li").addClass("list-group-item");


  $('#expcoll').on('click', function () {
    $('.panel-collapse').collapse('toggle');
    $(".metatable").children('a').toggle();
  });

  // show main
  $(".startsUgly").show();

  // remove loader
  $(".loading").remove();
  
  
}


function GetTextWidth (value) {
	var html_org = $(value).html();
  var html_calc = '<span>' + html_org + '</span>';
  $(value).html(html_calc);
  var width =  $(value).find('span:first').width();
  $(value).html(html_org);
  return width;
 }



function GetMetadataList (parent_element, irods_id, show_flag) {
	var table_cell = $(parent_element).parent ();
	var rest_url = "/davrods/api/metadata/get?id=" + irods_id;

	$.ajax (rest_url, {
		dataType: "html",
		success: function (data, status) {
		  if (status === "success") {
				$(parent_element).html (data);

				SetUpAddMetadataButtons (parent_element);
				SetUpEditMetadataButtons (parent_element);
				SetUpDeleteMetadataButtons (parent_element);
        AddMetadataToggleButtons (parent_element);

        if (show_flag) {
          var metadata_container = $(parent_element).find ("ul.metadata");
          $(metadata_container).show ();
        }
    	}
		}
	});
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
