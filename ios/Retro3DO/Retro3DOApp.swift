import SwiftUI

@main
struct Retro3DOApp: App {
    @StateObject private var engine = Engine()
    @Environment(\.scenePhase) private var phase

    var body: some Scene {
        WindowGroup {
            RootView()
                .environmentObject(engine)
                .task {
                    engine.start()
                    // CI photographs the app on a simulator, where there is no
                    // way to tap a tile: simctl can boot, install and launch,
                    // and nothing else. This is the only way a screenshot can
                    // show the emulator actually running rather than the shelf
                    // standing still. It does nothing unless the argument is
                    // passed, which no shipped launch ever does.
                    if CommandLine.arguments.contains("-autoplay-demo") {
                        engine.playDemo()
                    }
                }
                .preferredColorScheme(.dark)
        }
        .onChange(of: phase) { newPhase in
            // Pausing on the way out is not a nicety: the bridge flushes the
            // 3DO's save memory whenever it pauses or stops, so a game's saves
            // survive iOS killing the app in the background.
            switch newPhase {
            case .active:   engine.setPaused(false)
            case .inactive: engine.setPaused(true)
            case .background: engine.setPaused(true)
            @unknown default: break
            }
        }
    }
}

// The whole app is two states: the shelf, or a game. There is no navigation
// stack, no tab bar and no settings page — the gear is a sheet over the shelf,
// and a game is a full-screen cover over it.
struct RootView: View {
    @EnvironmentObject private var engine: Engine

    var body: some View {
        Shelf()
            .fullScreenCover(isPresented: Binding(
                get: { engine.playing },
                set: { if !$0 { engine.stop() } })) {
                Session()
                    .environmentObject(engine)
            }
    }
}
