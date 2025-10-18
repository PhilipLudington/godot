extends Node

# Simple test to verify file/line info in compiler warnings

func _ready():
	# This should generate an INCOMPATIBLE_TERNARY warning on line 7
	# The warning should show: res://simple_warning_test.gd:7
	var result = 5 if true else "hello"
	print(result)

	# Another warning on line 12
	# Should show: res://simple_warning_test.gd:12
	var x: int = 3.14  # NARROWING_CONVERSION warning