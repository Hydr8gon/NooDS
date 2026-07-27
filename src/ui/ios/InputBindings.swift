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

struct InputRow: View {
    let title: String
    let value: UnsafeMutablePointer<std.string>
    @State private var present = false

    var body: some View {
        // Show an input with its current mapping that opens a remap alert when selected
        Button {
            present = true
        }
        label: {
            HStack {
                Text(title).foregroundColor(.primary)
                Spacer()
                Text(value.pointee.empty() ? "None" : String(value.pointee))
                    .font(.subheadline).foregroundColor(.gray)
            }
        }
        .alert(isPresented: $present) {
            // Override the gamepad handler while the remap alert is active
            let old = gamepadHandler
            setGamepadHandler { _, element in
                // Map the input to a pressed button or direction's name
                if let input = element as? GCControllerButtonInput {
                    value.pointee = std.string(input.localizedName)
                }
                else if let input = element as? GCControllerDirectionPad {
                    if input.yAxis.value >= 0.5 {
                        value.pointee = std.string(input.up.localizedName)
                    }
                    else if input.yAxis.value <= -0.5 {
                        value.pointee = std.string(input.down.localizedName)
                    }
                    else if input.xAxis.value <= -0.5 {
                        value.pointee = std.string(input.left.localizedName)
                    }
                    else if input.xAxis.value >= 0.5 {
                        value.pointee = std.string(input.right.localizedName)
                    }
                    else {
                        return
                    }
                }
                else {
                    return
                }

                // Save settings and close the alert
                Settings.save()
                setGamepadHandler(old)
                present = false
            }

            // Show the remap alert with a button to clear the mapping
            return Alert(title: Text(title), message: Text("Press a key to bind it to this input."),
                primaryButton: .default(Text("Clear"), action: {
                    value.pointee = ""
                    Settings.save()
                    setGamepadHandler(old)
                }),
                secondaryButton: .destructive(Text("Cancel")))
        }
    }
}

struct InputBindings: View {
    var body: some View {
        List {
            // List the button bindings category
            SettingsHeader(title: "Buttons")
            InputRow(title: "A Button", value: &CoreBridge.keyBinds.0)
            InputRow(title: "B Button", value: &CoreBridge.keyBinds.1)
            InputRow(title: "X Button", value: &CoreBridge.keyBinds.10)
            InputRow(title: "Y Button", value: &CoreBridge.keyBinds.11)
            InputRow(title: "Start Button", value: &CoreBridge.keyBinds.3)
            InputRow(title: "Select Button", value: &CoreBridge.keyBinds.2)
            InputRow(title: "Up Button", value: &CoreBridge.keyBinds.6)
            InputRow(title: "Down Button", value: &CoreBridge.keyBinds.7)
            InputRow(title: "Left Button", value: &CoreBridge.keyBinds.5)
            InputRow(title: "Right Button", value: &CoreBridge.keyBinds.4)
            InputRow(title: "L Button", value: &CoreBridge.keyBinds.9)
            InputRow(title: "R Button", value: &CoreBridge.keyBinds.8)

            // List the hotkey bindings category
            SettingsHeader(title: "Hotkeys")
            InputRow(title: "Fast Forward Hold", value: &CoreBridge.keyBinds.12)
            InputRow(title: "Fast Forward Toggle", value: &CoreBridge.keyBinds.13)
            InputRow(title: "Screen Swap Toggle", value: &CoreBridge.keyBinds.14)
        }
        .navigationTitle("Input Bindings")
    }
}
