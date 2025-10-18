extends Node

# Test script to generate compiler warnings

var _unused_variable: int = 5  # Should generate unused variable warning

func test():
	# Incompatible ternary warning
	var result = 5.0 if true else 0

	# Another unused variable
	var _another_unused = "test"

	# Unreachable code warning
	return
	print("This will never be printed")

func _ready():
	test()