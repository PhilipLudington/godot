extends SceneTree

func _initialize():
	print("Testing WarningCaptureEditor module...")
	if ClassDB.class_exists("WarningCaptureEditor"):
		print("✅ WarningCaptureEditor module is available in Godot-Custom.app!")
		var capture = ClassDB.instantiate("WarningCaptureEditor")
		print("   Module instantiated successfully")
	else:
		print("❌ WarningCaptureEditor module NOT found")
	quit()