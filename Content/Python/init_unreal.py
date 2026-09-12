# Copyright Epic Games, Inc. All Rights Reserved. Project: Anastasis_UnrealV2 / UE 5.8.2

import unreal

try:
    from anastasis_toolset.toolsets import _registration
    from anastasis_toolset import tests

    if _registration.register():
        unreal.log("AnastasisInspectTools registered with ToolsetRegistry.")
        tests._test_runner = unreal.PythonTestRunner.create(
            "AI.Toolsets.AnastasisInspect",
            unreal.PythonTestRunnerSearchOptions(root_module=tests.__name__),
        )
    else:
        unreal.log_warning(
            "ToolsetRegistry unavailable; AnastasisInspectTools not registered."
        )
except Exception as exc:
    unreal.log_error(f"Anastasis agentic Python init failed: {exc}")
