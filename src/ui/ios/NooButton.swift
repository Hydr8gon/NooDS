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

import SwiftUI

struct NooButton: View {
    let ids: [Int32]
    var images = [Image]()
    @Binding private var menu: Bool
    @State private var state = 0

    init(menu: Binding<Bool>, ids: [Int32], name: String) {
        // Set values and add the button's released image
        self._menu = menu
        self.ids = ids
        images.append(Image(name).resizable())

        // Add the button's pressed images depending on type
        if ids.count == 1 { // Normal
            images.append(Image(name + "Pressed").resizable())
        }
        else { // D-pad
            for i in 1...8 {
                images.append(Image(name + "Pressed\(i)").resizable())
            }
        }
    }

    var body: some View {
        // Show an image selected by the current button state
        GeometryReader { proxy in
            images[state].opacity(0.5).simultaneousGesture(DragGesture(minimumDistance: 0)
                .onChanged({ touch in
                    // For normal buttons, check the ID for behavior
                    if ids.count == 1 {
                        if state != 1 && !menu {
                            if ids[0] == 12 {
                                // Open the in-game menu
                                menu = true
                            }
                            else {
                                // Change to pressed state and notify the core
                                state = 1
                                CoreBridge.pressKey(ids[0])
                            }
                            vibrate()
                        }
                        return
                    }

                    // For D-pads, check each direction and map the bitmask to an index
                    let old = state
                    state = 0
                    checkDir(cond: touch.location.x > proxy.size.width * 2 / 3, i: 0) // Right
                    checkDir(cond: touch.location.x < proxy.size.width * 1 / 3, i: 1) // Left
                    checkDir(cond: touch.location.y < proxy.size.height * 1 / 3, i: 2) // Up
                    checkDir(cond: touch.location.y > proxy.size.height * 2 / 3, i: 3) // Down
                    state = ([Int])([0, 1, 5, 0, 7, 8, 6, 0, 3, 2, 4])[state]
                    if state != old {
                        vibrate()
                    }
                })
                .onEnded({ _ in
                    // Change to released state and release all keys
                    state = 0
                    for id in ids {
                        CoreBridge.releaseKey(id)
                    }
                    vibrate()
                })
            )
        }
    }

    private func vibrate() {
        // Vibrate with the configured strength for button presses/releases
        if CoreBridge.vibrateStrength > 0 {
            let styles = ([UIImpactFeedbackGenerator.FeedbackStyle])([.soft, .light, .medium, .heavy, .rigid])
            UIImpactFeedbackGenerator(style: styles[Int(CoreBridge.vibrateStrength - 1)]).impactOccurred()
        }
    }

    private func checkDir(cond: Bool, i: Int) -> Void {
        // Update a direction's state and press/release its key
        if cond {
            state |= 1 << i
            CoreBridge.pressKey(ids[i])
        }
        else {
            CoreBridge.releaseKey(ids[i])
        }
    }
}
