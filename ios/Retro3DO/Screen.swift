import SwiftUI
import MetalKit
import CoreImage

// The picture.
//
// The emulator hands over a buffer of XRGB8888 and nothing else — the Console
// was written never to present — so everything about how it reaches the glass
// is decided here. Core Image does the scaling onto the Metal drawable, which
// is why there is no .metal shader in this project: a shader would have to be
// compiled by a Metal toolchain step, and a 320x240 picture does not need one.
struct Screen: UIViewRepresentable {
    func makeUIView(context: Context) -> MTKView {
        let view = MTKView(frame: .zero, device: MTLCreateSystemDefaultDevice())
        view.delegate = context.coordinator
        view.framebufferOnly = false        // Core Image renders into the texture
        view.isOpaque = true
        view.backgroundColor = .black
        view.preferredFramesPerSecond = 60
        view.enableSetNeedsDisplay = false
        view.isPaused = false
        return view
    }

    func updateUIView(_ view: MTKView, context: Context) {}

    func makeCoordinator() -> Renderer { Renderer() }

    final class Renderer: NSObject, MTKViewDelegate {
        private let queue: MTLCommandQueue?
        private let ciContext: CIContext?
        private var pixels = [UInt32]()
        private var lastSerial: UInt64 = 0
        private var image: CIImage?

        override init() {
            let device = MTLCreateSystemDefaultDevice()
            queue = device?.makeCommandQueue()
            ciContext = device.map { CIContext(mtlDevice: $0,
                                               options: [.cacheIntermediates: false]) }
            super.init()
        }

        func mtkView(_ view: MTKView, drawableSizeWillChange size: CGSize) {}

        func draw(in view: MTKView) {
            guard let queue, let ciContext,
                  let drawable = view.currentDrawable else { return }

            // Only rebuild the image when the emulator has actually produced a
            // new frame. A 60 Hz screen showing a 50 Hz PAL machine would
            // otherwise convert the same pixels ten extra times a second.
            let serial = r3_frame_serial()
            if serial != lastSerial {
                lastSerial = serial
                var w: Int32 = 0, h: Int32 = 0
                r3_copy_frame(nil, 0, &w, &h)
                if w > 0 && h > 0 {
                    let count = Int(w) * Int(h)
                    if pixels.count != count { pixels = [UInt32](repeating: 0, count: count) }
                    let copied = pixels.withUnsafeMutableBufferPointer { buffer in
                        r3_copy_frame(buffer.baseAddress, Int32(count), &w, &h)
                    }
                    if copied == Int32(count) {
                        let data = Data(bytes: pixels, count: count * 4)
                        image = CIImage(bitmapData: data,
                                        bytesPerRow: Int(w) * 4,
                                        size: CGSize(width: Int(w), height: Int(h)),
                                        format: .BGRA8,
                                        colorSpace: CGColorSpaceCreateDeviceRGB())
                    }
                }
            }

            guard let image, let buffer = queue.makeCommandBuffer() else { return }

            // Fit the machine's 4:3 picture inside whatever the device is,
            // letterboxed, never stretched. A 3DO game drawn 16:9 is a game
            // drawn wrong, and on a phone that is the first thing anyone sees.
            let target = CGSize(width: drawable.texture.width, height: drawable.texture.height)
            let source = image.extent.size
            let aspect = CGFloat(r3_pixel_aspect_x1000()) / 1000.0
            let wide = source.width * aspect
            let scale = min(target.width / wide, target.height / source.height)
            let drawn = CGSize(width: wide * scale, height: source.height * scale)
            let origin = CGPoint(x: (target.width - drawn.width) / 2,
                                 y: (target.height - drawn.height) / 2)

            let placed = image
                .transformed(by: CGAffineTransform(scaleX: scale * aspect, y: scale))
                .transformed(by: CGAffineTransform(translationX: origin.x, y: origin.y))

            ciContext.render(placed,
                             to: drawable.texture,
                             commandBuffer: buffer,
                             bounds: CGRect(origin: .zero, size: target),
                             colorSpace: CGColorSpaceCreateDeviceRGB())
            buffer.present(drawable)
            buffer.commit()
        }
    }
}
