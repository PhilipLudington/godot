extends Node

# Test script to generate various compiler warnings

# 1. Unused variable warning
var _unused_variable: int = 5

# 2. Unused private class variable
var _private_unused: String = "test"

# 3. Untyped declaration warning
var untyped_var = 10

func _ready():
	test_warnings()

func test_warnings():
	# 4. Incompatible ternary warning - THIS IS THE MAIN ONE WE'RE TESTING
	var x = true
	var result = 5 if x else "hello"  # Line 19: Should generate INCOMPATIBLE_TERNARY warning
	print(result)

	# 5. Another incompatible ternary
	var y = 10.0 if x else "string"  # Line 23: Another INCOMPATIBLE_TERNARY

	# 6. Narrowing conversion warning
	var int_val: int = 5.7  # Should warn about float to int conversion

	# 7. Unused local variable
	var _local_unused = "never used"

	# 8. Integer as enum without match
	enum MyEnum { VALUE_A = 1, VALUE_B = 2 }
	var enum_val: MyEnum = 3  # Warning: int doesn't match enum values

	# 9. Assert always true/false
	assert(true)  # Always true warning
	assert(1 == 2)  # Always false warning

	# 10. Unreachable code (after return)
	return
	print("This will never be printed")  # Unreachable code warning

# 11. Unused function parameter
func unused_param_test(param1: int, _param2: String):
	print(param1)
	# _param2 is never used (but prefixed with _ so might not warn)

# 12. Function without return type annotation
func untyped_function():
	return "test"