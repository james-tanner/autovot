# auotovot predict VOTs by selecting a Sound object and a TextGrid object. 
# Then specify a tier in the TextGrid with intervals of the arround the stop consonants.
# 
# Written by Joseph Keshet (17 July 2014)
# joseph.keshet@biu.ac.il
#
# This script is free software: you can redistribute it and/or modify 
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# A copy of the GNU General Public License is available at
# <http://www.gnu.org/licenses/>.


clearinfo

# save files to a temporary directory
if numberOfSelected ("Sound") <> 1 or numberOfSelected ("TextGrid") <> 1
    exit Please select a Sound and a TextGrid first.
endif

# keep the selected object safe
sound = selected ("Sound")
textgrid = selected ("TextGrid")

# get tiers name
selectObject: textgrid
num_tiers = Get number of tiers
for i from 1 to 'num_tiers'
	tier_name$[i] = Get tier name... 'i'
 	#appendInfoLine: tier_name$[i]
endfor

selectObject: sound, textgrid
selected_tier_name = 1
min_vot_length = 5
max_vot_length = 500
vot_classifier_model$ = "../models/vot_predictor.amanda.max_num_instances_1000.model"
beginPause: "AutoVOT"
	comment: "This is a script for automatic measurement of voice onset time (VOT)"
	optionMenu: "Select tier", selected_tier_name
	for i from 1 to 'num_tiers'
		option: tier_name$[i]
	endfor
    natural: "min vot length", 5
    natural: "max vot length", 500
    comment: "The script directory is:"
    comment: defaultDirectory$
    comment: "Training model files (relative to the above directory):"
    text: "vot classifier model", "../models/vot_predictor.amanda.max_num_instances_1000.model"
endPause: "Next", 1
#appendInfoLine: "selected_tier_name=", select_tier$
#appendInfoLine: "min_vot_length=", min_vot_length
#appendInfoLine: "max_vot_length=", max_vot_length
#appendInfoLine: "model=", vot_classifier_model$


#form AutoVOT
#    comment This is a script for automatic measurement of voice onset time (VOT), using
#    comment an algorithm which is trained to mimic VOT measurement by human annotators.
#    comment Please select a Sound object and a TextGrid object and specify the name
#    comment of the tier with interval roughly specify where each stop consonant is located.
#    text Window_tier SearchVotTier
#    positive Min_vot_length 5
#    positive Max_vot_length 500
#    comment Location of the training model (relative to the path above):
#    text Vot_classifier_model ../models/vot_predictor.amanda.max_num_instances_1000.model 
#endform

selectObject: sound
sound_name$ = selected$( "Sound")
#appendInfoLine: "name=", name$
sound_filename$ = temporaryDirectory$ + "/" + sound_name$ + ".wav"
#appendInfoLine: "Saving ", name$, " as ", sound_filename$
current_rate = Get sample rate
if current_rate <> 16000
	exitScript:  "The Sound object should have the following properties: 16000Hz, 16bit, 1 channel. Please convert it."
endif
Save as WAV file: sound_filename$
selectObject: textgrid
textgrid_name$ = selected$( "TextGrid")
#appendInfoLine: "name=", name$
textgrid_filename$ = temporaryDirectory$  + "/" + textgrid_name$ + ".TextGrid"
new_textgrid_filename$ = temporaryDirectory$ + "/" + textgrid_name$ + "_vad.TextGrid"
#appendInfoLine: "Saving ", name$, " as ", textgrid_filename$
Save as text file: textgrid_filename$
selectObject: sound, textgrid

# call vot prediction
exec_name$ = "export PATH=$PATH:.;  auto_vot_decode.py "
#vot_classifier_model$ = "../models/vot_predictor.amanda.max_num_instances_1000.model"
exec_params$ = "--min_vot_length " +  string$(min_vot_length) 
exec_params$ = exec_params$ + " --max_vot_length " +  string$(max_vot_length) 
exec_params$ = exec_params$ + " --window_tier " + "'" + select_tier$ + "'"
cmd_line$ = exec_name$ + exec_params$
cmd_line$ = cmd_line$ + " " + sound_filename$ + " " + textgrid_filename$ 
cmd_line$ = cmd_line$ + " " + defaultDirectory$ + "/" + vot_classifier_model$
appendInfoLine: "Executing in the shell the following command:"
appendInfoLine: cmd_line$
system 'cmd_line$'
appendInfoLine: "Done."

# read new TextGrid
system cp 'textgrid_filename$' 'new_textgrid_filename$'
Read from file... 'new_textgrid_filename$'
#sound_obj_name$ = "Sound " + sound_name$
textgrid_obj_name$ = "TextGrid " + textgrid_name$ + "_vad"
selectObject: sound, textgrid_obj_name$

# remove unecessary files
deleteFile: sound_filename$
deleteFile: textgrid_filename$
deleteFile: new_textgrid_filename$
