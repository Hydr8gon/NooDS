/*
    Copyright 2019-2026 Hydr8gon

    This file is part of NooDS.

    NooDS is free software: you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    NooDS is distributed in the hope that it will be useful, but
    WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
    General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with NooDS. If not, see <https://www.gnu.org/licenses/>.
*/

import GameController
import SwiftUI

private var fpsLimiterBackup = 0 as CInt
private var prevKeys = 0 as UInt32

private func updateKey(elem: GCControllerElement, cond: Bool, i: Int) {
    // Handle a key press or release if the mapping matches
    let binds = UnsafePointer<std.string>(&CoreBridge.keyBinds.0)
    if elem.localizedName == String(binds[i]) {
        if cond && (prevKeys & (1 << i)) == 0 { // Pressed
            prevKeys |= (1 << i)
            switch i {
            case 12: // Fast forward hold
                // Disable the FPS limiter
                if (Settings.fpsLimiter != 0) {
                    fpsLimiterBackup = Settings.fpsLimiter
                    Settings.fpsLimiter = 0
                }

            case 13: // Fast forward toggle
                // Toggle between disabling and restoring the FPS limiter
                if (Settings.fpsLimiter != 0) {
                    fpsLimiterBackup = Settings.fpsLimiter;
                    Settings.fpsLimiter = 0
                }
                else if (fpsLimiterBackup != 0) {
                    Settings.fpsLimiter = fpsLimiterBackup;
                    fpsLimiterBackup = 0;
                }

            case 14: // Screen swap toggle
                // Toggle between favoring the top or bottom screen
                ScreenLayout.screenSizing = (ScreenLayout.screenSizing == 1) ? 2 : 1

            default:
                // Send a key press to the core
                CoreBridge.pressKey(CInt(i))
            }
        }
        else if !cond && (prevKeys & (1 << i)) != 0 { // Released
            prevKeys &= ~(1 << i)
            switch i {
            case 12: // Fast forward hold
                // Restore the FPS limiter
                if (fpsLimiterBackup != 0) {
                    Settings.fpsLimiter = fpsLimiterBackup;
                    fpsLimiterBackup = 0;
                }

            default:
                // Send a key release to the core
                CoreBridge.releaseKey(CInt(i))
            }
        }
    }
}

struct SaveRow: View {
    let reload: (() -> Void)
    let title: String
    let size: CInt
    @State private var present = false

    var body: some View {
        // List a save type that shows a resize confirmation alert when selected
        Button(title) {
            present = true
        }
        .foregroundColor(.primary)
        .alert(isPresented: $present) {
            Alert(title: Text("Change Save Type"), message: Text("Are you sure? This may result in data loss!"),
                primaryButton: .default(Text("OK"), action: {
                    CoreBridge.stop()
                    CoreBridge.resizeSave(size)
                    reload()
                }),
                secondaryButton: .destructive(Text("Cancel")))
        }
    }
}

struct NooView: View {
    @Binding private var running: Bool
    @State private var menu = false
    @State private var controls = true
    @State private var showSave = false
    @State private var showLoad = false

    private let space = CGColorSpaceCreateDeviceRGB()
    private let info = CGBitmapInfo(alpha: .noneSkipLast, component: .integer, byteOrder: .orderDefault)
    private let topProv: CGDataProvider
    private let botProv: CGDataProvider
    private var ids = [CChar]([0, 1])

    private let ndsPath: String
    private let gbaPath: String

    @Environment(\.displayScale) private var scale: CGFloat

    init(running: Binding<Bool>, ndsPath: String, gbaPath: String) {
        // Initialize the top/bottom framebuffer providers
        var cbs = CGDataProviderSequentialCallbacks(version: 0, getBytes:
            bytesCb, skipForward: forwardCb, rewind: rewindCb, releaseInfo: nil)
        topProv = CGDataProvider(sequentialInfo: &ids[0], callbacks: &cbs)!
        botProv = CGDataProvider(sequentialInfo: &ids[1], callbacks: &cbs)!

        // Set other values and start the core
        _running = running
        self.ndsPath = ndsPath
        self.gbaPath = gbaPath
        CoreBridge.start()

        // Set the gamepad handler to update keys based on mappings
        setGamepadHandler { _, element in
            if let input = element as? GCControllerButtonInput {
                for i in 0..<16 {
                    updateKey(elem: input, cond: input.isPressed, i: i)
                }
            }
            else if let input = element as? GCControllerDirectionPad {
                for i in 0..<16 {
                    updateKey(elem: input.up, cond: input.yAxis.value >= 0.5, i: i)
                    updateKey(elem: input.down, cond: input.yAxis.value <= -0.5, i: i)
                    updateKey(elem: input.left, cond: input.xAxis.value <= -0.5, i: i)
                    updateKey(elem: input.right, cond: input.xAxis.value >= 0.5, i: i)
                }
            }
        }
    }

    var body: some View {
        // Redraw the display on a canvas at 60 FPS
        var layout = ScreenLayout()
        TimelineView(.periodic(from: Date(), by: 1.0 / 60)) { _ in
            ZStack {
                Canvas { context, size in
                    // Update the layout with current canvas dimensions scaled to pixels
                    let gbaMode = CoreBridge.getGbaMode() && ScreenLayout.gbaCrop != 0
                    layout.update(Int32(size.width * scale), Int32(size.height * scale), gbaMode)
                    let orient = ([Image.Orientation])([.up, .right, .left])[Int(min(ScreenLayout.screenRotation, 2))]
                    let interp = (Settings.screenFilter == 0) ? Image.Interpolation.none : Image.Interpolation.high
                    let shift = (Settings.highRes3D != 0 || Settings.screenFilter == 1) ? 1 : 0
                    CoreBridge.updateFrame()

                    // Draw screens depending on the configuration
                    if gbaMode {
                        // Get the GBA screen buffer and draw it
                        let gbaBuf = CGImage(width: 240 << shift, height: 160 << shift, bitsPerComponent: 8,
                            bitsPerPixel: 32, bytesPerRow: (240 * 4) << shift, space: space, bitmapInfo: info,
                            provider: topProv, decode: nil, shouldInterpolate: false, intent: .defaultIntent)!
                        let gbaImg = Image(decorative: gbaBuf, scale: 1.0, orientation: orient).interpolation(interp)
                        let gbaRect = CGRect(x: CGFloat(layout.topX) / scale, y: CGFloat(layout.topY) / scale,
                            width: CGFloat(layout.topWidth) / scale, height: CGFloat(layout.topHeight) / scale)
                        context.draw(gbaImg, in: gbaRect, style: FillStyle())
                    }
                    else {
                        // Get the top screen buffer and draw it
                        if ScreenLayout.screenArrangement != 3 || ScreenLayout.screenSizing < 2 {
                            let topBuf = CGImage(width: 256 << shift, height: 192 << shift, bitsPerComponent: 8,
                                bitsPerPixel: 32, bytesPerRow: (256 * 4) << shift, space: space, bitmapInfo: info,
                                provider: topProv, decode: nil, shouldInterpolate: false, intent: .defaultIntent)!
                            let topImg = Image(decorative: topBuf, scale: 1.0, orientation: orient).interpolation(interp)
                            let topRect = CGRect(x: CGFloat(layout.topX) / scale, y: CGFloat(layout.topY) / scale,
                                width: CGFloat(layout.topWidth) / scale, height: CGFloat(layout.topHeight) / scale)
                            context.draw(topImg, in: topRect, style: FillStyle())
                        }

                        // Get the bottom screen buffer and draw it
                        if ScreenLayout.screenArrangement != 3 || ScreenLayout.screenSizing == 2 {
                            let botBuf = CGImage(width: 256 << shift, height: 192 << shift, bitsPerComponent: 8,
                                bitsPerPixel: 32, bytesPerRow: (256 * 4) << shift, space: space, bitmapInfo: info,
                                provider: botProv, decode: nil, shouldInterpolate: false, intent: .defaultIntent)!
                            let botImg = Image(decorative: botBuf, scale: 1.0, orientation: orient).interpolation(interp)
                            let botRect = CGRect(x: CGFloat(layout.botX) / scale, y: CGFloat(layout.botY) / scale,
                                width: CGFloat(layout.botWidth) / scale, height: CGFloat(layout.botHeight) / scale)
                            context.draw(botImg, in: botRect, style: FillStyle())
                        }
                    }
                }
                .simultaneousGesture(DragGesture(minimumDistance: 0)
                    .onChanged({ touch in
                        // Send a touch press to the core, with coordinates relative to the layout
                        let touchX = layout.getTouchX(Int32(touch.location.x * scale), Int32(touch.location.y * scale))
                        let touchY = layout.getTouchY(Int32(touch.location.x * scale), Int32(touch.location.y * scale))
                        CoreBridge.pressScreen(touchX, touchY)
                    })
                    .onEnded({ _ in
                        // Send a touch release to the core
                        CoreBridge.releaseScreen()
                    })
                )

                // Show the FPS counter in the top-left corner with some padding
                if CoreBridge.showFpsCounter != 0 {
                    VStack {
                        HStack {
                            Text(String("\(CoreBridge.getFps()) FPS"))
                                .foregroundColor(.white)
                                .font(.title)
                            Spacer()
                        }
                        .padding(.all, 5)
                        Spacer()
                    }
                    .padding(.all, 5)
                }

                // Convert button settings to float-based weights
                let btnScale = CGFloat(CoreBridge.buttonScale + 5) / 10
                let btnSpace = CGFloat(CoreBridge.buttonSpacing) / 5

                // Position on-screen triggers and face buttons based on weights if enabled
                if controls {
                    VStack {
                        Spacer()
                        HStack {
                            NooButton(menu: $menu, ids: [9], name: "L")
                                .frame(width: 110 * btnScale, height: 44 * btnScale)
                            Spacer()
                            NooButton(menu: $menu, ids: [8], name: "R")
                                .frame(width: 110 * btnScale, height: 44 * btnScale)
                        }
                        .padding(.all, 1 + 4 * btnSpace)
                        Spacer().frame(width: 0, height: 44 * btnSpace)
                        HStack {
                            NooButton(menu: $menu, ids: [4, 5, 6, 7], name: "Dpad")
                                .frame(width: 132 * btnScale, height: 132 * btnScale)
                            Spacer()
                            NooButton(menu: $menu, ids: [0, 11, 10, 1], name: "Abxy")
                                .frame(width: 165 * btnScale, height: 165 * btnScale)
                        }
                        .padding(.all, 1 + 4 * btnSpace)
                        Spacer().frame(width: 0, height: 33 * btnSpace)
                    }
                }

                // Position on-screen start/select if enabled, and always show the menu button
                VStack {
                    Spacer()
                    HStack {
                        if controls {
                            NooButton(menu: $menu, ids: [2], name: "Select")
                                .frame(width: 33 * btnScale, height: 33 * btnScale)
                            Spacer().frame(width: 1 + 15 * btnSpace, height: 0)
                        }
                        NooButton(menu: $menu, ids: [12], name: "Menu")
                            .frame(width: 33 * btnScale, height: 33 * btnScale)
                        if controls {
                            Spacer().frame(width: 1 + 15 * btnSpace, height: 0)
                            NooButton(menu: $menu, ids: [3], name: "Start")
                                .frame(width: 33 * btnScale, height: 33 * btnScale)
                        }
                    }
                    .padding(.all, 1 + 4 * btnSpace)
                }
            }
        }
        .background(.black)
        .statusBar(hidden: true)
        .sheet(isPresented: $menu) {
            // Build a menu that can be opened while the core is running
            NavigationView {
                List {
                    // Add a way back to the file browser, which stops the core
                    Button("File Browser") {
                        CoreBridge.stop()
                        running = false
                        menu = false
                    }

                    // Add a selection that restarts the core
                    Button("Restart") {
                        CoreBridge.stop()
                        reload()
                    }

                    // Add a save state button, with an extended warning on first save
                    Button("Save State") {
                        showSave = true
                    }
                    .alert(isPresented: $showSave) {
                        Alert(title: Text("Save State"), message: Text((CoreBridge.checkState() == STATE_FILE_FAIL) ?
                            "Saving and loading states is dangerous and can lead to data loss. States " +
                            "are also not guaranteed to be compatible across emulator versions. " +
                            "Please rely on in-game saving to keep your progress, and back up .sav " +
                            "files before using this feature. Do you want to save the current state?" :
                            "Do you want to overwrite the saved state with the current state? This can't be undone!"),
                            primaryButton: .default(Text("OK"), action: {
                                CoreBridge.stop()
                                CoreBridge.saveState()
                                CoreBridge.start()
                                menu = false
                            }),
                            secondaryButton: .destructive(Text("Cancel")))
                    }

                    // Add a load state button, with explanations for fail states
                    Button("Load State") {
                        showLoad = true
                    }
                    .alert(isPresented: $showLoad) {
                        switch (CoreBridge.checkState()) {
                        case STATE_FILE_FAIL:
                            Alert(title: Text("Load State"), message:
                                Text("The state file doesn't exist or couldn't be opened."))
                        case STATE_FORMAT_FAIL:
                            Alert(title: Text("Load State"), message:
                                Text("The state file doesn't have a valid format."))
                        case STATE_VERSION_FAIL:
                            Alert(title: Text("Load State"), message:
                                Text("The state file isn't compatible with this version of NooDS."))
                        default:
                            Alert(title: Text("Load State"), message: Text("Do you want to load " +
                                "the saved state and lose the current state? This can't be undone!"),
                                primaryButton: .default(Text("OK"), action: {
                                    CoreBridge.stop()
                                    CoreBridge.loadState()
                                    CoreBridge.start()
                                    menu = false
                                }),
                                secondaryButton: .destructive(Text("Cancel")))
                        }
                    }

                    // Add a link to a list of save types the current file can be changed to
                    NavigationLink("Change Save Type") {
                        List {
                            SaveRow(reload: reload, title: "None", size: 0)
                            SaveRow(reload: reload, title: "EEPROM 0.5KB", size: 0x200)
                            SaveRow(reload: reload, title: "EEPROM 8KB", size: 0x2000)
                            if CoreBridge.getGbaMode() {
                                SaveRow(reload: reload, title: "SRAM 32KB", size: 0x8000)
                                SaveRow(reload: reload, title: "FLASH 64KB", size: 0x10000)
                                SaveRow(reload: reload, title: "FLASH 128KB", size: 0x20000)
                            }
                            else {
                                SaveRow(reload: reload, title: "EEPROM 64KB", size: 0x10000)
                                SaveRow(reload: reload, title: "EEPROM 128KB", size: 0x20000)
                                SaveRow(reload: reload, title: "FRAM 32KB", size: 0x8000)
                                SaveRow(reload: reload, title: "FLASH 256KB", size: 0x40000)
                                SaveRow(reload: reload, title: "FLASH 512KB", size: 0x80000)
                                SaveRow(reload: reload, title: "FLASH 1024KB", size: 0x100000)
                                SaveRow(reload: reload, title: "FLASH 8192KB", size: 0x800000)
                            }
                        }
                        .navigationTitle("Change Save Type")
                    }

                    // Add a link to the settings menu
                    NavigationLink("Settings") {
                        SettingsMenu()
                    }

                    // Add a selection that toggles on-screen controls
                    Button("Toggle Controls") {
                        controls.toggle()
                        menu = false
                    }
                }
                .foregroundColor(.primary)
            }
        }
    }

    private func reload() -> Void {
        // Reload the core or bail on error
        if CoreBridge.loadRom(ndsPath, gbaPath) == 0 {
            CoreBridge.start()
        }
        else {
            running = false
        }
        menu = false
    }
}

private func bytesCb(info: UnsafeMutableRawPointer?, buffer: UnsafeMutableRawPointer, count: Int) -> Int {
    // Forward the bytes callback to the C++ bridge
    return Int(CoreBridge.bytesCb(info, buffer, Int32(count)))
}

private func forwardCb(info: UnsafeMutableRawPointer?, count: off_t) -> off_t {
    // Forward the forward callback to the C++ bridge
    return off_t(CoreBridge.forwardCb(info, Int32(count)))
}

private func rewindCb(info: UnsafeMutableRawPointer?) -> Void {
    // Forward the rewind callback to the C++ bridge
    return CoreBridge.rewindCb(info)
}
