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

import AVFAudio
import GameController
import SwiftUI

var gamepadHandler = nil as GCExtendedGamepadValueChangedHandler?
private var controllers = GCController.controllers()

func setGamepadHandler(_ handler: GCExtendedGamepadValueChangedHandler?) {
    // Set the gamepad handler and assign it to controller 0 if present
    gamepadHandler = handler
    if !controllers.isEmpty {
        controllers[0].extendedGamepad!.valueChangedHandler = handler
    }
}

@main
struct NooApp: App {
    @State private var running = false
    @State private var ndsPath = String()
    @State private var gbaPath = String()
    private let notifs = NotificationCenter.default
    private let audio = AVAudioEngine()
    private let firstBoot: Bool

    init() {
        // Initialize settings using the app's documents folder
        let docsUrl = FileManager.default.urls(for: .documentDirectory, in: .userDomainMask).first!
        firstBoot = !CoreBridge.loadSettings(docsUrl.path + "/noods")

        // Configure the audio session for playback
        let session = AVAudioSession.sharedInstance()
        try! session.setCategory(.playback, mode: .moviePlayback)
        try! session.setActive(true)

        // Hook up the audio callback and start playing
        let format = AVAudioFormat(commonFormat: .pcmFormatInt16, sampleRate: 44100, channels: 2, interleaved: true)!
        let source = AVAudioSourceNode(format: format, renderBlock: audioCb)
        audio.attach(source)
        audio.connect(source, to: audio.outputNode, format: format)
        audio.prepare()
        try! audio.start()

        // Track connected controllers and keep the handler updated
        notifs.addObserver(forName: .GCControllerDidConnect, object: nil, queue: .main) { notif in
            controllers.append(notif.object as! GCController)
            setGamepadHandler(gamepadHandler)
        }

        // Track disconnected controllers and keep the handler updated
        notifs.addObserver(forName: .GCControllerDidDisconnect, object: nil, queue: .main) { notif in
            controllers.removeAll { $0 == notif.object as! GCController }
            setGamepadHandler(gamepadHandler)
        }
    }

    var body: some Scene {
        // Choose the current window based on run state
        WindowGroup {
            if running {
                NooView(running: $running, ndsPath: ndsPath, gbaPath: gbaPath)
            }
            else {
                FileBrowser(running: $running, ndsPath: $ndsPath, gbaPath: $gbaPath, firstBoot: firstBoot)
            }
        }
    }

    private func audioCb(isSilence: UnsafeMutablePointer<ObjCBool>, timestamp: UnsafePointer<AudioTimeStamp>,
        frameCount: AVAudioFrameCount, outputData: UnsafeMutablePointer<AudioBufferList>) -> OSStatus {
        // Get the core to fill the audio buffer
        CoreBridge.getSamples(outputData.pointee.mBuffers.mData, frameCount)
        return noErr
    }
}
