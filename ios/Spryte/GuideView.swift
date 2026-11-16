import SwiftUI

// The walkthrough a first-time user is shown, and can open again whenever.
//
// The words are not here. They live in the core beside the emulator, so that
// this, the Android app and the desktop build all say the same thing — see
// src/core/guide.cpp. This file is only the shape it is read in.
struct GuideView: View {
    @EnvironmentObject private var engine: Engine
    @Environment(\.dismiss) private var dismiss
    @Binding var step: Int
    var playDemo: () -> Void
    @State private var importing = false
    @State private var editingControls = false
    @State private var note: String?

    /// Named here rather than in the shared text because each platform's
    /// control has a different name; the core leaves a {ADD} token for it.
    private static let addControl = "the + tile"

    private var count: Int { Int(r3_guide_count()) }

    /// The step's own colour, from the shared table beside the words, so a
    /// step is the same colour here as it is on Android and the desktop.
    private func accent(_ i: Int) -> Color {
        let rgb = r3_guide_accent(Int32(i))
        return Color(red: Double((rgb >> 16) & 0xFF) / 255,
                     green: Double((rgb >> 8) & 0xFF) / 255,
                     blue: Double(rgb & 0xFF) / 255)
    }

    var body: some View {
        VStack(alignment: .leading, spacing: 0) {
            progress
                .padding(.horizontal, 28)
                .padding(.top, 22)

            ScrollView {
                VStack(alignment: .leading, spacing: 18) {
                    // The mark, on the first step only. It is the app saying
                    // hello; repeating it on all nine would be wallpaper.
                    if step == 0 {
                        Image("GuideMark")
                            .resizable()
                            .interpolation(.high)
                            .frame(width: 76, height: 76)
                            .clipShape(RoundedRectangle(cornerRadius: 17, style: .continuous))
                            .overlay(
                                RoundedRectangle(cornerRadius: 17, style: .continuous)
                                    .strokeBorder(Theme.tileEdge, lineWidth: 1))
                            .padding(.bottom, 2)
                    }

                    Text(stepNumber)
                        .font(.system(size: 12, weight: .semibold, design: .monospaced))
                        .tracking(1.4)
                        .foregroundStyle(accent(step))

                    Text(title(step))
                        .font(.system(size: 27, weight: .semibold, design: .rounded))
                        .foregroundStyle(Theme.ink)
                        .fixedSize(horizontal: false, vertical: true)

                    Text(body(step))
                        .font(.system(size: 16))
                        .foregroundStyle(Theme.inkDim)
                        .lineSpacing(4)
                        .fixedSize(horizontal: false, vertical: true)
                }
                .frame(maxWidth: 560, alignment: .leading)
                .padding(.horizontal, 28)
                .padding(.vertical, 24)
            }

            actions
                .padding(.horizontal, 28)

            controls
                .padding(.horizontal, 28)
                .padding(.bottom, 24)
        }
        .frame(maxWidth: .infinity, maxHeight: .infinity, alignment: .topLeading)
        .background(Theme.background.ignoresSafeArea())
        .sheet(isPresented: $importing) {
            Importer { urls in
                importing = false
                var said: String?
                for url in urls { said = engine.importFile(at: url) ?? said }
                // Silence means the obvious thing happened and the tile is
                // already there, which the user is about to be shown.
                note = said ?? "Added. It is on your shelf."
            }
            .ignoresSafeArea()
        }
        .fullScreenCover(isPresented: $editingControls) { PadLayoutEditor() }
        .onAppear {
            if CommandLine.arguments.contains("-capture-pad-layout") { editingControls = true }
        }
    }

    // A row of bars rather than "3 of 7": it shows how much is left without
    // asking anyone to do arithmetic, and the whole thing is seven short steps.
    private var progress: some View {
        HStack(spacing: 5) {
            ForEach(0..<count, id: \.self) { i in
                Capsule()
                    .fill(i <= step ? AnyShapeStyle(accent(i))
                                    : AnyShapeStyle(Theme.tileEdge))
                    .frame(height: 4)
            }
        }
        .animation(.easeOut(duration: 0.25), value: step)
        .accessibilityLabel("Step \(step + 1) of \(count)")
    }

    // What this step can do, rather than only describe. A step offers at most
    // two, which is what Android needs; iOS never sees more than one.
    @ViewBuilder private var actions: some View {
        let first = Int(r3_guide_action(Int32(step), 0))
        // Nothing to offer when it is already done: the step explains itself
        // and the user moves on.
        if first != R3_GUIDE_NONE && !satisfied(step) {
            VStack(spacing: 10) {
                actionButton(slot: 0, action: first)
                if let note {
                    Text(note)
                        .font(.system(size: 14))
                        .foregroundStyle(Theme.inkDim)
                        .multilineTextAlignment(.center)
                        .frame(maxWidth: .infinity)
                }
            }
            .padding(.bottom, 14)
        }
    }

    private func actionButton(slot: Int, action: Int) -> some View {
        Button {
            note = nil
            switch action {
            case R3_GUIDE_PLAY_DEMO:
                // Started as a guide session, so leaving it comes back here
                // rather than dropping the user on the shelf mid-walkthrough.
                playDemo()
            case R3_GUIDE_ADD_GAME, R3_GUIDE_ADD_BIOS:
                importing = true
            case R3_GUIDE_EDIT_LAYOUT:
                editingControls = true
            default:
                break
            }
        } label: {
            Text(label(slot: slot))
                .font(.system(size: 16, weight: .semibold, design: .rounded))
                .foregroundStyle(accent(step))
                .frame(maxWidth: .infinity)
                .padding(.vertical, 13)
                .background(accent(step).opacity(0.16))
                .clipShape(RoundedRectangle(cornerRadius: 13, style: .continuous))
                .overlay(
                    RoundedRectangle(cornerRadius: 13, style: .continuous)
                        .strokeBorder(accent(step).opacity(0.45), lineWidth: 1))
        }
    }

    private func label(slot: Int) -> String {
        var buffer = [CChar](repeating: 0, count: 128)
        _ = r3_guide_action_label(Int32(step), Int32(slot), &buffer, 128)
        return String(cString: buffer)
    }

    private var controls: some View {
        HStack {
            if step > 0 {
                Button("Back") { note = nil; withAnimation { step -= 1 } }
                    .foregroundStyle(Theme.inkDim)
            }
            Spacer()
            Button(step == count - 1 ? "Start playing" : "Next") {
                note = nil
                if step == count - 1 { dismiss() } else { withAnimation { step += 1 } }
            }
            .font(.system(size: 17, weight: .semibold, design: .rounded))
            .foregroundStyle(Color.black)
            .padding(.horizontal, 26)
            .padding(.vertical, 13)
            .background(accent(step))
            .clipShape(Capsule())
        }
        .overlay(alignment: .center) {
            // Always available, never the obvious choice: somebody who opened
            // this by accident should not have to read seven screens.
            if step < count - 1 {
                Button("Skip") { dismiss() }
                    .font(.system(size: 15))
                    .foregroundStyle(Theme.inkDim)
            }
        }
    }

    private var stepNumber: String { "STEP \(step + 1) OF \(count)" }

    private func title(_ i: Int) -> String {
        var buffer = [CChar](repeating: 0, count: 256)
        _ = r3_guide_title(Int32(i), &buffer, 256)
        return String(cString: buffer)
    }

    /// Whether what this step asks for is already in place. The step says
    /// which state to look at through its action; nobody should be walked
    /// through importing a system ROM they imported last week.
    private func satisfied(_ i: Int) -> Bool {
        switch Int(r3_guide_action(Int32(i), 0)) {
        case R3_GUIDE_ADD_BIOS:               return engine.hasBios
        case R3_GUIDE_ADD_GAME,
             R3_GUIDE_CHOOSE_FOLDER:          return !engine.discs.isEmpty
        default:                                        return false
        }
    }

    private func body(_ i: Int) -> String {
        var buffer = [CChar](repeating: 0, count: 4096)
        if satisfied(i),
           r3_guide_body_done(Int32(i), GuideView.addControl, &buffer, 4096) > 0 {
            return String(cString: buffer)
        }
        _ = r3_guide_body(Int32(i), GuideView.addControl, &buffer, 4096)
        return String(cString: buffer)
    }
}
