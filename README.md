# Flip3DComp
Original version by: [ALTaleX531](https://github.com/ALTaleX531)


DirectComposition Flip3D switcher. A faster, more complete successor to the [flip3d](https://github.com/ALTaleX531/flip3d) D3D11 prototype.

## Requirements

- a Windows version compatible with DWM and DirectComposition
- CMake 3.21+
- Visual Studio / MSVC with the Windows SDK

## Demo

<img width="800" height="439" alt="Win10 Flip3DComp test" src="https://github.com/user-attachments/assets/7f414433-018c-4df0-866e-0defad08a631" />

## Build

```powershell
cmake -S . -B build
cmake --build build --config Release
```

## Run

Launch `build/Release/Flip3D.exe`.

Eligible windows are shown as DWM shared thumbnails on a DirectComposition 3D carousel. No D3D11 scene pass, no Windows.Graphics.Capture.

## Controls

- `Tab` / `Shift+Tab`, arrow keys, mouse wheel: scroll the carousel
- `Enter` or left click a card: activate the selected window
- `Home`: return to the original front window
- `Esc`: exit
- `F5`: replay the enter animation

## Notes

- **vs [flip3d](https://github.com/ALTaleX531/flip3d):** compositor-native visuals (lower overhead), smooth fractional scroll while browsing, uDWM-aligned parallel exit rotation
- **More complete:** `IAccessible` + `NotifyWinEvent` accessibility, Shell Hook live card add/remove
