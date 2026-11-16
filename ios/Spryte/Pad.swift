import SwiftUI

// The 3DO controller, on glass.
//
// The whole state is sent on every change rather than press and release pairs.
// That is deliberate: a finger that slides off a button, or a touch iOS
// cancels when a call arrives, cannot leave a direction held down — which is
// the classic touch-emulator bug where the character walks into a wall for
// ever.
struct Pad {
    // The two halves are separate views because the session puts the picture
    // between them. A single view spanning the screen would have to overlay
    // the game, and on a phone there is no margin to overlay into.
    struct Directions: View {
        @Binding var mask: UInt32
        /// The column's width. Everything is drawn relative to it, so the
        /// controls fit whatever margin the picture leaves rather than forcing
        /// the picture to shrink around a fixed 190pt.
        var unit: CGFloat
        var body: some View { Pad(mask: $mask).dpad(unit) }
    }

    struct Actions: View {
        @Binding var mask: UInt32
        var unit: CGFloat
        private var pad: Pad { Pad(mask: $mask) }

        var body: some View {
            // Stacked rather than spread out: the widest row here is two face
            // buttons, so the cluster fits a column as narrow as the letterbox
            // beside a 4:3 picture. Nothing drops below 40pt, which is the
            // smallest a finger should be asked to find.
            let face = max(44, unit * 0.40)
            let small = max(40, unit * 0.29)
            VStack(spacing: unit * 0.08) {
                HStack(spacing: unit * 0.06) {
                    pad.small("L", R3_PAD_LEFT_SHIFT, small)
                    pad.small("R", R3_PAD_RIGHT_SHIFT, small)
                }
                // A, B, C in reading order, matching the arc on the desktop
                // layout. The column is too narrow for three across, so it is
                // two then one rather than a different order.
                HStack(spacing: unit * 0.07) {
                    pad.face("A", R3_PAD_A, face)
                    pad.face("B", R3_PAD_B, face)
                }
                pad.face("C", R3_PAD_C, face)
                HStack(spacing: unit * 0.06) {
                    pad.small("P", R3_PAD_PLAY, small)
                    pad.small("X", R3_PAD_STOP, small)
                }
            }
        }
    }

    @Binding var mask: UInt32

    // A cross, not four separate buttons: a finger between up and left should
    // give both, which is how anything with a diagonal is actually played.
    func dpad(_ unit: CGFloat) -> some View {
        let d = max(120, unit * 0.95)
        return ZStack {
            Circle()
                .fill(Theme.tile.opacity(0.55))
                .frame(width: d, height: d)
                .overlay(Circle().strokeBorder(Theme.tileEdge.opacity(0.7), lineWidth: 1))
            ForEach(Array(["chevron.up": R3_PAD_UP,
                           "chevron.down": R3_PAD_DOWN,
                           "chevron.left": R3_PAD_LEFT,
                           "chevron.right": R3_PAD_RIGHT].sorted(by: { $0.key < $1.key })),
                    id: \.key) { symbol, bit in
                Image(systemName: symbol)
                    .font(.system(size: 20, weight: .bold))
                    .foregroundStyle(held(bit) ? Theme.accent : Theme.inkDim)
                    .offset(offset(for: symbol, d))
            }
        }
        .frame(width: d, height: d)
        .contentShape(Circle())
        .gesture(
            DragGesture(minimumDistance: 0)
                .onChanged { value in
                    let dx = value.location.x - d / 2
                    let dy = value.location.y - d / 2
                    let dead = d * 0.13
                    var next = mask & ~(R3_PAD_UP | R3_PAD_DOWN | R3_PAD_LEFT | R3_PAD_RIGHT)
                    if dy < -dead { next |= R3_PAD_UP }
                    if dy >  dead { next |= R3_PAD_DOWN }
                    if dx < -dead { next |= R3_PAD_LEFT }
                    if dx >  dead { next |= R3_PAD_RIGHT }
                    set(next)
                }
                .onEnded { _ in
                    set(mask & ~(R3_PAD_UP | R3_PAD_DOWN | R3_PAD_LEFT | R3_PAD_RIGHT))
                })
    }

    private func offset(for symbol: String, _ d: CGFloat) -> CGSize {
        let r = d * 0.31
        switch symbol {
        case "chevron.up":    return CGSize(width: 0, height: -r)
        case "chevron.down":  return CGSize(width: 0, height: r)
        case "chevron.left":  return CGSize(width: -r, height: 0)
        default:              return CGSize(width: r, height: 0)
        }
    }

    func face(_ label: String, _ bit: UInt32, _ size: CGFloat) -> some View {
        button(label, bit, size: size, font: size * 0.35)
    }

    func small(_ label: String, _ bit: UInt32, _ size: CGFloat) -> some View {
        button(label, bit, size: size, font: size * 0.34)
    }

    private func button(_ label: String, _ bit: UInt32,
                        size: CGFloat, font: CGFloat) -> some View {
        Text(label)
            .font(.system(size: font, weight: .semibold, design: .rounded))
            .foregroundStyle(held(bit) ? Color.black : Theme.ink)
            .frame(width: size, height: size)
            .background(held(bit) ? AnyShapeStyle(Theme.accent)
                                  : AnyShapeStyle(Theme.tile.opacity(0.55)))
            .clipShape(Circle())
            .overlay(Circle().strokeBorder(Theme.tileEdge.opacity(0.7), lineWidth: 1))
            .contentShape(Circle())
            .gesture(
                DragGesture(minimumDistance: 0)
                    .onChanged { _ in if !held(bit) { set(mask | bit) } }
                    .onEnded   { _ in set(mask & ~bit) })
            .accessibilityLabel("Button \(label)")
    }

    private func held(_ bit: UInt32) -> Bool { mask & bit != 0 }

    private func set(_ next: UInt32) {
        guard next != mask else { return }
        mask = next
        ControllerInput.shared.setTouch(next)
    }
}
