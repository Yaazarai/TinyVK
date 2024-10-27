:: Change the "1.3.211.0" to your Vulkan version.
:: Change "sample_shader" to the shader you want to compile.
mkdir "%CD%/x64/DEBUG/Shaders/"
mkdir "%CD%/x64/RELEASE/Shaders/"
	"%VULKAN%/Bin/glslc.exe" "%CD%/Shaders/passthrough_vert.vert" -o "%CD%/x64/DEBUG/Shaders/passthrough_vert.spv"
	"%VULKAN%/Bin/glslc.exe" "%CD%/Shaders/passthrough_vert.vert" -o "%CD%/x64/RELEASE/Shaders/passthrough_vert.spv"

	"%VULKAN%/Bin/glslc.exe" "%CD%/Shaders/passthrough_frag.frag" -o "%CD%/x64/DEBUG/Shaders/passthrough_frag.spv"
	"%VULKAN%/Bin/glslc.exe" "%CD%/Shaders/passthrough_frag.frag" -o "%CD%/x64/RELEASE/Shaders/passthrough_frag.spv"

	"%VULKAN%/Bin/glslc.exe" "%CD%/Shaders/passthrough_comp.comp" -o "%CD%/x64/DEBUG/Shaders/passthrough_comp.spv"
	"%VULKAN%/Bin/glslc.exe" "%CD%/Shaders/passthrough_comp.comp" -o "%CD%/x64/RELEASE/Shaders/passthrough_comp.spv"
pause