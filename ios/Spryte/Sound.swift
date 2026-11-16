import AVFoundation

// The sound.
//
// The bridge's ring is lock-free and always fills the buffer it is given,
// padding with silence, so this render block never blocks and never has to
// decide what to do about an underrun — which is exactly what an audio unit's
// realtime thread needs.
final class Sound {
    private let engine = AVAudioEngine()
    private var source: AVAudioSourceNode?
    private var started = false

    func start() {
        guard !started else { return }

        // .ambient with mixWithOthers: an emulator is not a music app. Someone
        // playing their own music should not have it stopped by opening this.
        try? AVAudioSession.sharedInstance().setCategory(.ambient, options: [.mixWithOthers])
        try? AVAudioSession.sharedInstance().setActive(true)

        let format = AVAudioFormat(standardFormatWithSampleRate: 44100, channels: 2)!
        let node = AVAudioSourceNode(format: format) { _, _, frameCount, audioBufferList in
            let frames = Int(frameCount)
            var scratch = [Int16](repeating: 0, count: frames * 2)
            scratch.withUnsafeMutableBufferPointer { raw in
                _ = r3_pull_audio(raw.baseAddress, Int32(frames))
            }
            let buffers = UnsafeMutableAudioBufferListPointer(audioBufferList)
            guard buffers.count >= 2,
                  let left = buffers[0].mData?.assumingMemoryBound(to: Float.self),
                  let right = buffers[1].mData?.assumingMemoryBound(to: Float.self)
            else { return noErr }
            for i in 0..<frames {
                left[i]  = Float(scratch[i * 2])     / 32768.0
                right[i] = Float(scratch[i * 2 + 1]) / 32768.0
            }
            return noErr
        }

        engine.attach(node)
        engine.connect(node, to: engine.mainMixerNode, format: format)
        source = node
        try? engine.start()
        started = true
    }

    func stop() {
        guard started else { return }
        engine.stop()
        if let source { engine.detach(source) }
        source = nil
        started = false
        try? AVAudioSession.sharedInstance().setActive(false)
    }
}
