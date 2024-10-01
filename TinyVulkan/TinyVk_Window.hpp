#pragma once
#ifndef TINYVK_TINYVKWINDOW
#define TINYVK_TINYVKWINDOW
	#include "./TinyVulkan.hpp"
	
	typedef void (*GLFWgamepadbuttonfun)(const GLFWgamepadstate* gamepad, int32_t gpad, int32_t button, int32_t action);
	typedef void (*GLFWgamepadaxisfun)(const GLFWgamepadstate* gamepad, int32_t gpad, int32_t axisXID, float_t axisX, int32_t axisYID, float_t axisY);
	typedef void (*GLFWgamepadtriggerfun)(const GLFWgamepadstate* gamepad, int32_t gpad, int32_t axisXID, float_t axis);
	GLFWAPI GLFWgamepadbuttonfun glfwGamepadButtonCallback = NULL;
	GLFWAPI GLFWgamepadaxisfun glfwGamepadAxisCallback = NULL;
	GLFWAPI GLFWgamepadtriggerfun glfwGamepadTriggerCallback = NULL;
	GLFWAPI void glfwSetGamepadButtonCallback(GLFWgamepadbuttonfun callback) { glfwGamepadButtonCallback = callback; }
	GLFWAPI void glfwSetGamepadAxisCallback(GLFWgamepadaxisfun callback) { glfwGamepadAxisCallback = callback; }
	GLFWAPI void glfwSetGamepadTriggerCallback(GLFWgamepadtriggerfun callback) { glfwGamepadTriggerCallback = callback; }
	GLFWAPI GLFWgamepadstate glfwGamepadCache[GLFW_JOYSTICK_LAST + 1]{};
	GLFWAPI float_t glfwRoundfd(float_t value, float_t precision) { return std::round(value * precision) / precision; }
	GLFWAPI void glfwPollGamepads() {
		for (int32_t i = 0; i <= GLFW_JOYSTICK_LAST; i++) {
			if (glfwJoystickPresent(i) == GLFW_FALSE) continue;

			GLFWgamepadstate gpstate;
			glfwGetGamepadState(i, &gpstate);

			for(int32_t b = 0; b <= GLFW_GAMEPAD_BUTTON_LAST; b++) {
				if (gpstate.buttons[b] != glfwGamepadCache[i].buttons[b]) {
					glfwGamepadCache[i].buttons[b] = gpstate.buttons[b];
					
					if (glfwGamepadButtonCallback != NULL)
						glfwGamepadButtonCallback(&gpstate, i, b, gpstate.buttons[b]);
				}
			}

			if (glfwRoundfd(gpstate.axes[GLFW_GAMEPAD_AXIS_LEFT_X], 1000) != glfwRoundfd(glfwGamepadCache[i].axes[GLFW_GAMEPAD_AXIS_LEFT_X], 1000)
				|| glfwRoundfd(gpstate.axes[GLFW_GAMEPAD_AXIS_LEFT_Y], 1000) != glfwRoundfd(glfwGamepadCache[i].axes[GLFW_GAMEPAD_AXIS_LEFT_Y], 1000)) {
				glfwGamepadCache[i].axes[GLFW_GAMEPAD_AXIS_LEFT_X] = gpstate.axes[GLFW_GAMEPAD_AXIS_LEFT_X];
				glfwGamepadCache[i].axes[GLFW_GAMEPAD_AXIS_LEFT_Y] = gpstate.axes[GLFW_GAMEPAD_AXIS_LEFT_Y];

				if (glfwGamepadAxisCallback != NULL)
					glfwGamepadAxisCallback(&gpstate, i, GLFW_GAMEPAD_AXIS_LEFT_X, gpstate.axes[GLFW_GAMEPAD_AXIS_LEFT_X], GLFW_GAMEPAD_AXIS_LEFT_Y, gpstate.axes[GLFW_GAMEPAD_AXIS_LEFT_Y]);
			}

			if (glfwRoundfd(gpstate.axes[GLFW_GAMEPAD_AXIS_RIGHT_X], 1000) != glfwRoundfd(glfwGamepadCache[i].axes[GLFW_GAMEPAD_AXIS_RIGHT_X], 1000)
				|| glfwRoundfd(gpstate.axes[GLFW_GAMEPAD_AXIS_RIGHT_Y], 1000) != glfwRoundfd(glfwGamepadCache[i].axes[GLFW_GAMEPAD_AXIS_RIGHT_Y], 1000)) {
				glfwGamepadCache[i].axes[GLFW_GAMEPAD_AXIS_RIGHT_X] = gpstate.axes[GLFW_GAMEPAD_AXIS_RIGHT_X];
				glfwGamepadCache[i].axes[GLFW_GAMEPAD_AXIS_RIGHT_Y] = gpstate.axes[GLFW_GAMEPAD_AXIS_RIGHT_Y];
				
				if (glfwGamepadAxisCallback != NULL)
					glfwGamepadAxisCallback(&gpstate, i, GLFW_GAMEPAD_AXIS_RIGHT_X, gpstate.axes[GLFW_GAMEPAD_AXIS_RIGHT_X], GLFW_GAMEPAD_AXIS_RIGHT_Y, gpstate.axes[GLFW_GAMEPAD_AXIS_RIGHT_Y]);
			}

			if (glfwRoundfd(gpstate.axes[GLFW_GAMEPAD_AXIS_LEFT_TRIGGER], 1000) != glfwRoundfd(glfwGamepadCache[i].axes[GLFW_GAMEPAD_AXIS_LEFT_TRIGGER], 1000)) {
				glfwGamepadCache[i].axes[GLFW_GAMEPAD_AXIS_LEFT_TRIGGER] = gpstate.axes[GLFW_GAMEPAD_AXIS_LEFT_TRIGGER];
				
				if (glfwGamepadTriggerCallback != NULL)
					glfwGamepadTriggerCallback(&gpstate, i, GLFW_GAMEPAD_AXIS_LEFT_TRIGGER, gpstate.axes[GLFW_GAMEPAD_AXIS_LEFT_TRIGGER]);
			}

			if (glfwRoundfd(gpstate.axes[GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER], 1000) != glfwRoundfd(glfwGamepadCache[i].axes[GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER], 1000)) {
				glfwGamepadCache[i].axes[GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER] = gpstate.axes[GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER];
				
				if (glfwGamepadTriggerCallback != NULL)
					glfwGamepadTriggerCallback(&gpstate, i, GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER, gpstate.axes[GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER]);
			}
		}
	}

	float glfwGetGamepadAxis(tinyvk::TinyVkGamepads id, tinyvk::TinyVkGamepadAxis axis) { return glfwGamepadCache[int(id)].axes[int(axis)]; }
	int glfwGetGamepadButton(tinyvk::TinyVkGamepads id, tinyvk::TinyVkGamepadButtons button) { return static_cast<int>(glfwGamepadCache[int(id)].buttons[int(button)]); }

	namespace TINYVULKAN_NAMESPACE {
		/// <summary>GLFW window handler for TinyVulkan that will link to and initialize GLFW and Vulkan to create your game/application window</summary>
		class TinyVkWindow : public TinyVkDisposable {
		private:
			bool hwndResizable;
			int hwndWidth, hwndHeight, hwndXpos, hwndYpos;
			std::string title;
			GLFWwindow* hwndWindow;

			inline static TinyVkInvokable<GLFWwindow*, int, int> onWindowResized;
			inline static TinyVkInvokable<GLFWwindow*, int, int> onWindowPositionMoved;

			/// <summary>GLFWwindow unique pointer constructor.</summary>
			GLFWwindow* InitiateWindow(std::string title, int width, int height, bool resizable = true, bool transparentFramebuffer = false) {
				glfwInit();
				glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
				glfwWindowHint(GLFW_RESIZABLE, (resizable) ? GLFW_TRUE : GLFW_FALSE);
				glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, (transparentFramebuffer) ? GLFW_TRUE : GLFW_FALSE);

				if (!glfwVulkanSupported())
					throw new std::runtime_error("TinyVulkan: GLFW implementation could not locate Vulkan loader.");

				hwndResizable = resizable;
				hwndWidth = width;
				hwndHeight = height;
				return glfwCreateWindow(hwndWidth, hwndHeight, title.c_str(), VK_NULL_HANDLE, VK_NULL_HANDLE);
			}

			/// <summary>Generates an event for window framebuffer resizing.</summary>
			inline static void OnFrameBufferNotifyReSizeCallback(GLFWwindow* hwnd, int width, int height) {
				onResizeFrameBuffer.invoke(hwnd, width, height);
				onWindowResized.invoke(hwnd, width, height);
			}

			/// <summary>Generates an event for window position moved.</summary>
			inline static void OnWindowPositionCallback(GLFWwindow* hwnd, int xpos, int ypos) {
				onWindowPositionMoved.invoke(hwnd, xpos, ypos);
			}
			
		public:
			/// <summary>Remove default copy destructor.</summary>
			TinyVkWindow& operator=(const TinyVkWindow&) = delete;

			/// <summary>Calls the disposable interface dispose event.</summary>
			~TinyVkWindow() { this->Dispose(); }

			/// <summary>Disposable function for disposable class interface and window resource cleanup.</summary>
			void Disposable(bool waitIdle) {
				glfwDestroyWindow(hwndWindow);
				glfwTerminate();
			}

			/// <summary>Initiialize managed GLFW Window and Vulkan API. Initialize GLFW window unique_ptr.</summary>
			TinyVkWindow(std::string title, int width, int height, bool resizable, bool transparentFramebuffer = false, bool hasMinSize = false, int minWidth = 200, int minHeight = 200) {
				onDispose.hook(TinyVkCallback<bool>([this](bool forceDispose){this->Disposable(forceDispose); }));
				onWindowResized.hook(TinyVkCallback<GLFWwindow*, int, int>([this](GLFWwindow* hwnd, int width, int height) { if (hwnd != hwndWindow) return; hwndWidth = width; hwndHeight = height; }));
				onWindowPositionMoved.hook(TinyVkCallback<GLFWwindow*, int, int>([this](GLFWwindow* hwnd, int xpos, int ypos) { if (hwnd != hwndWindow) return; hwndXpos = xpos; hwndYpos = ypos; }));

				hwndWindow = InitiateWindow(title, width, height, resizable, transparentFramebuffer);
				glfwSetWindowUserPointer(hwndWindow, this);
				glfwSetFramebufferSizeCallback(hwndWindow, TinyVkWindow::OnFrameBufferNotifyReSizeCallback);
				glfwSetWindowPosCallback(hwndWindow, TinyVkWindow::OnWindowPositionCallback);

				if (hasMinSize) glfwSetWindowSizeLimits(hwndWindow, minWidth, minHeight, GLFW_DONT_CARE, GLFW_DONT_CARE);
			}

			/// <summary>Remove default copy constructor.</summary>
			TinyVkWindow(const TinyVkWindow&) = delete;

			/// <summary>Invokable callback to respond to Vulkan API when the active frame buffer is resized.</suimmary>
			inline static TinyVkInvokable<GLFWwindow*, int, int> onResizeFrameBuffer;

			/// <summary>Pass to render engine for swapchain resizing.</summary>
			void OnFrameBufferReSizeCallback(int& width, int& height) {
				width = 0;
				height = 0;

				while (width <= 0 || height <= 0)
					glfwGetFramebufferSize(hwndWindow, &width, &height);

				hwndWidth = width;
				hwndHeight = height;
			}

			/// <summary>Checks if the GLFW window should close.</summary>
			bool ShouldClose() { return glfwWindowShouldClose(hwndWindow) == GLFW_TRUE; }

			/// <summary>Returns the active GLFW window handle.</summary>
			GLFWwindow* GetHandle() { return hwndWindow; }

			/// <summary>Returns [BOOL] should close and polls input events (optional).</summary>
			bool ShouldClosePollEvents() {
				bool shouldClose = glfwWindowShouldClose(hwndWindow) == GLFW_TRUE;
				glfwPollEvents();
				#ifdef TINYVK_ALLOWS_POLLING_GAMEPADS
				glfwPollGamepads();
				#endif
				return shouldClose;
			}

			/// <summary>Returns [BOOL] should close and polls input events (optional).</summary>
			bool ShouldCloseWaitEvents() {
				bool shouldClose = ShouldClose();
				glfwWaitEvents();
				#ifdef TINYVK_ALLOWS_POLLING_GAMEPADS
				glfwPollGamepads();
				#endif
				return shouldClose;
			}

			/// <summary>Sets the callback pointer for the window.</summary>
			void SetCallbackPointer(void* data) { glfwSetWindowUserPointer(hwndWindow, data); }

			/// <summary>Gets the callback pointer for the window.</summary>
			void* GetCallbackPointer() { return glfwGetWindowUserPointer(hwndWindow); }

			/// <summary>Creates a Vulkan surface for this GLFW window.</summary>
			VkSurfaceKHR CreateWindowSurface(VkInstance instance) {
				VkSurfaceKHR wndSurface;
				if (glfwCreateWindowSurface(instance, hwndWindow, VK_NULL_HANDLE, &wndSurface) != VK_SUCCESS)
					throw std::runtime_error("TinyVulkan: Failed to create GLFW Window Surface!");
				return wndSurface;
			}

			/// <summary>Gets the GLFW window handle.</summary>
			GLFWwindow* GetWindowHandle() { return hwndWindow; }

			/// <summary>Gets the required GLFW extensions.</summary>
			static std::vector<const char*> QueryRequiredExtensions(bool enableValidationLayers) {
				glfwInit();

				uint32_t glfwExtensionCount = 0;
				const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
				std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

				if (enableValidationLayers)
					extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

				return extensions;
			}

			/// <summary>Returns the window's framebuffer width.</summary>
			int GetWidth() { return std::max(hwndWidth, 1); }

			/// <summary>Returns the window's framebuffer height.</summary>
			int GetHeight() { return std::max(hwndHeight, 1); }

			/// <summary>Returns the window's screen X-position on the relevant monitor.</summary>
			int GetXpos() { return hwndXpos; }

			/// <summary>Returns the window's screen Y-position on the relevant monitor.</summary>
			int GetYpos() { return hwndYpos; }

			/// <summary>Executes functions in the main window loop (w/ ref to bool to exit loop as needed).</summary>
			TinyVkInvokable<std::atomic<bool>&> onWhileMain;

			/// <summary>Executes the main window loop.</summary>
			virtual void WhileMain(const bool waitOrPollEvents = true) {
				std::atomic<bool> shouldRun = true;

				while (shouldRun) {
					onWhileMain.invoke(shouldRun);

					if (!shouldRun) break;
					shouldRun = (waitOrPollEvents)? !ShouldCloseWaitEvents() : !ShouldClosePollEvents();
				}
			}
		};
	}
#endif