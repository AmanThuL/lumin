# MyD3D12Renderer
A C++ rendering engine with DirectX 12 API and shaders based on Frank D. Luna's *Introduction to 3D Game Programming with DirectX 12*.
More general information about this book can be found [here](http://www.d3dcoder.net/d3d12.htm).

---
## Modifications
1. **Added Main.cpp**: 
- Refactored `WinMain()` out from the App class to increase maintainability of the application.

2. **Added `void CheckMaxFeatureSupport()`**: 
- The function outputs the maximum supported Direct3D feature level by the hardware.
- The output will also be displayed as the DirectX version on the application title bar.

3. **Modified `void CalculateFrameStats()` and renamed to `void UpdateTitleBarStats()`**:
- In addition to computation of the average FPS and MSPF values, the function added width, height, and DirectX version to display them on the application title bar.

4. **Added `void CreateConsoleWindow()`**:
- The function allocates a console window to help print debugging information.

5. **Combined Shapes demo and LandAndWaves demo into one solution**

6. **Added RenderItem.h**:
- Refactored `struct RenderItem` out from the ShapesApp class to allow universal usage by other classes.
