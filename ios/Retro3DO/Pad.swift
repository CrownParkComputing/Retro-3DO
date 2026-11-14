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
        var body: some View { Pad(mask: $mask).dpad }
    }

    struct Actions: View {
        @Binding var mask: UInt32
        private var pad: Pad { Pad(mask: $mask) }
        var body: some View {
            // Two rows of two rather than one row of three: three 68pt circles
            // side by side are wider than the margin beside a 4:3 picture.
            VStack(spacing: 14) {
                HStack(spacing: 10) {
                    pad.small("L", R3_PAD_LEFT_SHIFT)
                    pad.small("R", R3_PAD_RIGHT_SHIFT)
                }
                HStack(spacing: 14) {
                    pad.face("C", R3_PAD_C)
                    pad.face("B", R3_PAD_B)
                }
                HStack(spacing: 14) {
                    pad.small("P", R3_PAD_PLAY)
                    pad.face("A", R3_PAD_A)
                    pad.small("X", R3_PAD_STOP)
                }
            }
        }
    }

    @Binding var mask: UInt32

    // A cross, not four separate buttons: a finger between up and left should
    // give both, which is how anything with a diagonal is actually played.
    var dpad: some View {
        ZStack {
            Circle()
                .fill(Theme.tile.opacity(0.55))
                .frame(width: 168, height: 168)
                .overlay(Circle().strokeBorder(Theme.tileEdge.opacity(0.7), lineWidth: 1))
            ForEach(Array(["chevron.up": R3_PAD_UP,
                           "chevron.down": R3_PAD_DOWN,
                           "chevron.left": R3_PAD_LEFT,
                           "chevron.right": R3_PAD_RIGHT].sorted(by: { $0.key < $1.key })),
                    id: \.key) { symbol, bit in
                Image(systemName: symbol)
                    .font(.system(size: 20, weight: .bold))
                    .foregroundStyle(held(bit) ? Theme.accent : Theme.inkDim)
                    .offset(offset(for: symbol))
            }
        }
        .frame(width: 168, height: 168)
        .contentShape(Circle())
        .gesture(
            DragGesture(minimumDistance: 0)
                .onChanged { value in
                    let dx = value.location.x - 84
                    let dy = value.location.y - 84
                    let dead: CGFloat = 22
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

    private func offset(for symbol: String) -> CGSize {
        switch symbol {
        case "chevron.up":    return CGSize(width: 0, height: -52)
        case "chevron.down":  return CGSize(width: 0, height: 52)
        case "chevron.left":  return CGSize(width: -52, height: 0)
        default:              return CGSize(width: 52, height: 0)
        }
    }

    func face(_ label: String, _ bit: UInt32) -> some View {
        button(label, bit, size: 68, font: 24)
    }

    func small(_ label: String, _ bit: UInt32) -> some View {
        button(label, bit, size: 44, font: 15)
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
        r3_pad(next)
    }
}
