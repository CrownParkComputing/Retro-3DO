import SwiftUI

// The walkthrough a first-time user is shown, and can open again whenever.
//
// The words are not here. They live in the core beside the emulator, so that
// this, the Android app and the desktop build all say the same thing — see
// src/core/guide.cpp. This file is only the shape it is read in.
struct GuideView: View {
    @Environment(\.dismiss) private var dismiss
    @State private var step = 0

    /// Named here rather than in the shared text because each platform's
    /// control has a different name; the core leaves a {ADD} token for it.
    private static let addControl = "the + tile"

    private var count: Int { Int(r3_guide_count()) }

    var body: some View {
        VStack(alignment: .leading, spacing: 0) {
            progress
                .padding(.horizontal, 28)
                .padding(.top, 22)

            ScrollView {
                VStack(alignment: .leading, spacing: 18) {
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

            controls
                .padding(.horizontal, 28)
                .padding(.bottom, 24)
        }
        .frame(maxWidth: .infinity, maxHeight: .infinity, alignment: .topLeading)
        .background(Theme.background.ignoresSafeArea())
    }

    // A row of bars rather than "3 of 7": it shows how much is left without
    // asking anyone to do arithmetic, and the whole thing is seven short steps.
    private var progress: some View {
        HStack(spacing: 5) {
            ForEach(0..<count, id: \.self) { i in
                Capsule()
                    .fill(i <= step ? AnyShapeStyle(Theme.spectrum)
                                    : AnyShapeStyle(Theme.tileEdge))
                    .frame(height: 4)
            }
        }
        .animation(.easeOut(duration: 0.25), value: step)
        .accessibilityLabel("Step \(step + 1) of \(count)")
    }

    private var controls: some View {
        HStack {
            if step > 0 {
                Button("Back") { withAnimation { step -= 1 } }
                    .foregroundStyle(Theme.inkDim)
            }
            Spacer()
            Button(step == count - 1 ? "Start playing" : "Next") {
                if step == count - 1 { dismiss() } else { withAnimation { step += 1 } }
            }
            .font(.system(size: 17, weight: .semibold, design: .rounded))
            .foregroundStyle(Color.black)
            .padding(.horizontal, 26)
            .padding(.vertical, 13)
            .background(Theme.accent)
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

    private func title(_ i: Int) -> String {
        var buffer = [CChar](repeating: 0, count: 256)
        _ = r3_guide_title(Int32(i), &buffer, 256)
        return String(cString: buffer)
    }

    private func body(_ i: Int) -> String {
        var buffer = [CChar](repeating: 0, count: 4096)
        _ = r3_guide_body(Int32(i), GuideView.addControl, &buffer, 4096)
        return String(cString: buffer)
    }
}
