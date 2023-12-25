::
:: The ^ (carret) forces next parameter to CLI command on a new-line for readability.
:: Note the extra   (space) at the beginning of each new-line to separate parameters when combined into one line during execution.
::
clang-cl^
 /D _DEBUG^
 /D _CRT_SECURE_NO_WARNINGS^
 /std:c++20^
 /MP^
 /MDd^
 /O2^
 /Oi^
 /GL^
 /Gy^
 /permissive^
 /EHsc^
 /W0^
 /I "%CD%"^
 /I "%GLFW%/include/"^
 /I "%VULKAN%/Include/"^
 /Fe:"%CD%/x64/DEBUG/"^
 *.cpp^
 /link^
 /opt:ref^
 /subsystem:console^
 shell32.lib^
 gdi32.lib^
 user32.lib^
 "%GLFW%/lib-vc2022/glfw3.lib"^
 "%VULKAN%/Lib/vulkan-1.lib"^