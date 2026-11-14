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

            HStack(spacing: 0) {
                if controlsVisible {
                    Pad.Directions(mask: $mask)
                        .frame(width: 190)
                        .transition(.opacity)
                }
                Screen()
                    .onTapGesture {
                        withAnimation(.easeInOut(duration: 0.2)) { controlsVisible.toggle() }
                    }
                if controlsVisible {
                    Pad.Actions(mask: $mask)
                        .frame(width: 190)
                        .transition(.opacity)
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
            Button("Back to the shelf") { dismiss() }
        }
        .onChange(of: showingMenu) { open in engine.setPaused(open) }
    }
}
