import SwiftUI

// A game, playing.
//
// One control that is not the game itself: a close button that goes back to
// the shelf. Everything a person might want mid-game — pause, restart — is
// behind a tap on that same button, so the picture is never covered by a menu
// bar and there is nothing to learn.
struct Session: View {
    @EnvironmentObject private var engine: Engine
    @Environment(\.dismiss) private var dismiss

    @State private var mask: UInt32 = 0
    @State private var showingMenu = false
    @State private var sound = Sound()
    @State private var controlsVisible = true

    var body: some View {
        ZStack {
            Color.black.ignoresSafeArea()

            // Explicit geometry, not flexible frames. maxWidth/maxHeight on the
            // row and on the picture changed nothing measurable: the picture
            // still came back 832px tall in a 1206px screen, byte for byte the
            // same screenshot. Whatever SwiftUI was proposing to a
            // UIViewRepresentable in an HStack, it was not the container's
            // height. Measuring the container and saying the sizes out loud
            // removes the question.
            GeometryReader { geo in
                // Two shapes, chosen by the window rather than the device. An
                // iPad can be either way up and can be a split-screen sliver,
                // so asking "is this taller than it is wide" is the only
                // question that stays true.
                let portrait = geo.size.height > geo.size.width
                if portrait {
                    // The picture takes the width and the controls sit under
                    // it, which is the only arrangement where a 4:3 picture in
                    // a tall window keeps any size at all.
                    let picture = geo.size.width * 3.0 / 4.0
                    let bar = max(150, min(260, (geo.size.height - picture) * 0.8))
                    VStack(spacing: 0) {
                        Screen()
                            .frame(width: geo.size.width, height: picture)
                            .onTapGesture {
                                withAnimation(.easeInOut(duration: 0.2)) { controlsVisible.toggle() }
                            }
                        if controlsVisible {
                            HStack {
                                Pad.Directions(mask: $mask, unit: bar * 0.8)
                                Spacer(minLength: 0)
                                Pad.Actions(mask: $mask, unit: bar * 0.62)
                            }
                            .padding(.horizontal, 28)
                            .frame(maxWidth: .infinity, maxHeight: .infinity)
                            .transition(.opacity)
                        }
                    }
                    .frame(width: geo.size.width, height: geo.size.height)
                } else {
                    // The columns are the letterbox a 4:3 picture leaves at
                    // full height, so the controls take exactly the space
                    // nothing else wants and the picture keeps its height.
                    let margin = (geo.size.width - geo.size.height * 4.0 / 3.0) / 2
                    let side = max(120, min(190, margin))
                    HStack(spacing: 0) {
                        if controlsVisible {
                            Pad.Directions(mask: $mask, unit: side)
                                .frame(width: side, height: geo.size.height)
                                .transition(.opacity)
                        }
                        Screen()
                            .frame(width: geo.size.width - (controlsVisible ? side * 2 : 0),
                                   height: geo.size.height)
                            .onTapGesture {
                                withAnimation(.easeInOut(duration: 0.2)) { controlsVisible.toggle() }
                            }
                        if controlsVisible {
                            Pad.Actions(mask: $mask, unit: side)
                                .frame(width: side, height: geo.size.height)
                                .transition(.opacity)
                        }
                    }
                }
            }
            .ignoresSafeArea(edges: .vertical)

            if controlsVisible {
                VStack {
                    HStack {
                        Button { showingMenu = true } label: {
                            Image(systemName: "chevron.left.circle.fill")
                                .font(.system(size: 28))
                                .foregroundStyle(Theme.ink.opacity(0.85))
                                .padding(12)
                        }
                        .accessibilityLabel("Back to the shelf")
                        Spacer()
                    }
                    Spacer()
                }
                .transition(.opacity)
            }
        }
        .statusBarHidden()
        .persistentSystemOverlays(.hidden)
        .onAppear {
            sound.start()
            engine.setPaused(false)
        }
        .onDisappear {
            sound.stop()
            r3_pad(0)
        }
        .confirmationDialog("", isPresented: $showingMenu, titleVisibility: .hidden) {
            Button("Keep playing", role: .cancel) {}
            Button("Start again") { engine.reset() }
            // There is no way out to the shelf from a demo the walkthrough
            // started. It goes back to the walkthrough, which is where the
            // user was and what they were in the middle of.
            Button(engine.startedFromGuide ? "Back to the guide" : "Back to the shelf") {
                dismiss()
            }
        }
        .onChange(of: showingMenu) { open in engine.setPaused(open) }
    }
}
