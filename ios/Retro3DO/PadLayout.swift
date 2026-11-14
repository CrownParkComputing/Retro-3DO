import SwiftUI

final class PadLayout: ObservableObject {
    static let shared = PadLayout()
    struct Position: Codable { var x: Double; var y: Double }
    @Published private var positions: [String: Position]
    private let defaults: UserDefaults
    private let storageKey = "onscreenPadLayout.v1"

    init(defaults: UserDefaults = .standard) {
        self.defaults = defaults
        positions = defaults.data(forKey: storageKey)
            .flatMap { try? JSONDecoder().decode([String: Position].self, from: $0) } ?? [:]
    }

    private func key(_ group: String, _ size: CGSize) -> String {
        (size.width > size.height ? "landscape." : "portrait.") + group
    }

    func point(_ group: String, size: CGSize, fallback: CGPoint) -> CGPoint {
        guard let value = positions[key(group, size)] else { return fallback }
        return CGPoint(x: CGFloat(min(1, max(0, value.x))) * size.width,
                       y: CGFloat(min(1, max(0, value.y))) * size.height)
    }

    func move(_ group: String, to point: CGPoint, size: CGSize, bounds: CGSize) {
        guard size.width > 0, size.height > 0 else { return }
        let halfWidth = min(bounds.width / 2, size.width / 2)
        let halfHeight = min(bounds.height / 2, size.height / 2)
        let x = min(size.width - halfWidth, max(halfWidth, point.x))
        let y = min(size.height - halfHeight, max(halfHeight, point.y))
        positions[key(group, size)] = Position(x: Double(x / size.width), y: Double(y / size.height))
    }

    func save() {
        if let data = try? JSONEncoder().encode(positions) {
            defaults.set(data, forKey: storageKey)
        }
    }

    func reset() { positions = [:]; defaults.removeObject(forKey: storageKey) }
}

// The editor and a running game use the same controls and normalized positions.
// Editing never starts a ROM or sends button presses to the machine.
struct PadOverlay: View {
    @Binding var mask: UInt32
    var editing = false
    @ObservedObject private var layout = PadLayout.shared

    var body: some View {
        GeometryReader { geo in
            let portrait = geo.size.height > geo.size.width
            let picture = geo.size.width * 3 / 4
            let bar = max(150, min(260, (geo.size.height - picture) * 0.8))
            let side = max(120, min(190, (geo.size.width - geo.size.height * 4 / 3) / 2))
            let directionUnit = portrait ? bar * 0.8 : side
            let actionUnit = portrait ? bar * 0.62 : side
            let diameter = max(120, directionUnit * 0.95)
            let face = max(44, actionUnit * 0.4)
            let small = max(40, actionUnit * 0.29)
            let directionBounds = CGSize(width: diameter, height: diameter)
            let actionBounds = CGSize(width: max(face * 2 + actionUnit * 0.07,
                                                 small * 2 + actionUnit * 0.06),
                                      height: small * 2 + face * 2 + actionUnit * 0.24)
            let y = portrait ? picture + (geo.size.height - picture) / 2 : geo.size.height / 2
            let left = CGPoint(x: portrait ? 28 + diameter / 2 : side / 2, y: y)
            let right = CGPoint(x: portrait ? geo.size.width - 28 - actionBounds.width / 2
                                            : geo.size.width - side / 2, y: y)
            ZStack {
                group("directions", size: geo.size, bounds: directionBounds, fallback: left) {
                    Pad.Directions(mask: $mask, unit: directionUnit)
                }
                group("actions", size: geo.size, bounds: actionBounds, fallback: right) {
                    Pad.Actions(mask: $mask, unit: actionUnit)
                }
            }
            .frame(width: geo.size.width, height: geo.size.height)
            .coordinateSpace(name: "padCanvas")
        }
    }

    private func group<Content: View>(_ name: String, size: CGSize, bounds: CGSize,
                                      fallback: CGPoint, @ViewBuilder content: () -> Content) -> some View {
        ZStack {
            if editing {
                content().allowsHitTesting(false)
                RoundedRectangle(cornerRadius: 18)
                    .strokeBorder(Theme.accent.opacity(0.7), style: StrokeStyle(lineWidth: 2, dash: [6]))
                    .frame(width: bounds.width + 12, height: bounds.height + 12)
                    .contentShape(Rectangle())
                    .gesture(DragGesture(minimumDistance: 0, coordinateSpace: .named("padCanvas"))
                        .onChanged { layout.move(name, to: $0.location, size: size, bounds: bounds) }
                        .onEnded { _ in layout.save() })
                    .accessibilityLabel(name == "directions" ? "Move directional controls" : "Move action buttons")
            } else {
                content()
            }
        }
        .frame(width: bounds.width, height: bounds.height)
        .position(layout.point(name, size: size, fallback: fallback))
    }
}

struct PadLayoutEditor: View {
    @Environment(\.dismiss) private var dismiss
    @ObservedObject private var layout = PadLayout.shared

    var body: some View {
        ZStack(alignment: .top) {
            Theme.background.ignoresSafeArea()
            PadOverlay(mask: .constant(0), editing: true)
                .ignoresSafeArea(edges: .vertical)
            HStack(spacing: 20) {
                Button("Reset") { layout.reset() }
                Spacer()
                Text("Drag the controls").font(.headline)
                Spacer()
                Button("Done") { layout.save(); dismiss() }.fontWeight(.semibold)
            }
            .foregroundStyle(Theme.accent)
            .padding(16)
            .background(Theme.tile.opacity(0.95))
            .clipShape(RoundedRectangle(cornerRadius: 16))
            .frame(maxWidth: 520)
            .padding(.horizontal, 16)
            .padding(.top, 12)
        }
        .statusBarHidden()
        .onDisappear { layout.save(); r3_pad(0) }
    }
}
