# DirectX UWP Application

## Overview
The **DirectX UWP Application** is a Windows Universal Platform (UWP) project written in C++/CX. It utilizes **DirectX 11** to render 3D graphics in real time. The application features an event-driven rendering loop, handling user input (mouse, touch, pen) and maintaining a consistent FPS.

## Features
- **DirectX 11 Rendering**: Uses Direct3D 11 for real-time graphics.
- **XAML + DirectX Integration**: Combines DirectX rendering with a UWP UI framework.
- **Frame Timing**: Implements `StepTimer` for smooth animation and frame control.
- **Multithreaded Rendering**: Uses `ThreadPool::RunAsync()` for background rendering.
- **Event Handling**: Responds to window visibility changes and input events.

## Installation
To run this project, ensure you have:
- **Windows 10 or higher**
- **Visual Studio 2019/2022** with UWP development tools
- **DirectX SDK (included in Windows 10 SDK)**

### Steps:
1. **Clone the repository:**
   ```bash
   git clone https://github.com/yourusername/directx-uwp-app.git
   ```
2. **Open the project in Visual Studio:**
   - Open `App1.sln` in Visual Studio.
   - Ensure the target platform is set to **x64** or **x86**.
3. **Build and Run:**
   - Click **Build** → **Deploy Solution**.
   - Click **Start Debugging** (F5) to run the application.

## Usage
1. **Rendering Loop:**
   - The app continuously updates and renders frames using `App1Main`.
2. **User Input Handling:**
   - The app captures mouse, touch, and pen input for interaction.
3. **Performance Monitoring:**
   - FPS is displayed using `SampleFpsTextRenderer`.
4. **Customization:**
   - Modify `Sample3DSceneRenderer` to change rendered objects.

## Example Code Snippets
#### Start Rendering Loop (from `App1Main.cpp`):
```cpp
void App1Main::StartRenderLoop() {
    if (m_renderLoopWorker != nullptr && m_renderLoopWorker->Status == AsyncStatus::Started)
        return;

    auto workItemHandler = ref new WorkItemHandler([this](IAsyncAction^ action) {
        while (action->Status == AsyncStatus::Started) {
            critical_section::scoped_lock lock(m_criticalSection);
            Update();
            if (Render()) {
                m_deviceResources->Present();
            }
        }
    });
    m_renderLoopWorker = ThreadPool::RunAsync(workItemHandler, WorkItemPriority::High, WorkItemOptions::TimeSliced);
}
```

## License
This project is licensed under the **MIT License**.

## Contributions
Contributions are welcome! To contribute:
1. Fork the repository.
2. Create a new branch (`feature-new-feature`).
3. Commit and push your changes.
4. Open a pull request.

## Contact
For any questions or support, please open an issue on GitHub.

