import SwiftUI

// The whole front end: one screen of tiles. Tap one and it plays.
//
// There is no library page, no settings page, no system page and no about
// page, because every one of those was a place a person could get lost. What
// is left is a shelf, a gear, and a plus.
struct Shelf: View {
    @EnvironmentObject private var engine: Engine
    @State private var picking = false
    @State private var showingSettings = false
    @State private var note: String?
    @State private var confirmingRemoval: Engine.Disc?
    @State private var showingGuide = false
    // The only thing the app remembers about the person using it.
    @AppStorage("hasSeenGuide") private var hasSeenGuide = false

    private let columns = [GridItem(.adaptive(minimum: 190, maximum: 240), spacing: 22)]

    var body: some View {
        ZStack {
            Theme.background.ignoresSafeArea()

            VStack(alignment: .leading, spacing: 0) {
                header

                ScrollView {
                    LazyVGrid(columns: columns, spacing: 22) {
                        ForEach(engine.discs) { disc in
                            DiscTile(disc: disc)
                                .onTapGesture { engine.play(disc) }
                                .contextMenu {
                                    Button(role: .destructive) {
                                        confirmingRemoval = disc
                                    } label: {
                                        Label("Remove", systemImage: "trash")
                                    }
                                }
                        }

                        AddTile { picking = true }
                    }
                    .padding(.horizontal, 28)
                    .padding(.bottom, 28)
                }
            }

            if let note {
                Note(text: note) { self.note = nil }
            }
        }
        .sheet(isPresented: $picking) {
            Importer { urls in
                picking = false
                var said: String?
                for url in urls { said = engine.importFile(at: url) ?? said }
                note = said
            }
            .ignoresSafeArea()
        }
        .sheet(isPresented: $showingSettings) {
            SettingsSheet(showGuide: { showingSettings = false; showingGuide = true })
                .environmentObject(engine)
        }
        .sheet(isPresented: $showingGuide) { GuideView().environmentObject(engine) }
        .onChange(of: engine.playing) { playing in
            // Coming out of a session the walkthrough started puts the user
            // back in the walkthrough rather than on a shelf they have not
            // been introduced to yet.
            if !playing && engine.startedFromGuide {
                engine.startedFromGuide = false
                showingGuide = true
            }
        }
        .task {
            // First launch only, and marked as seen the moment it is shown
            // rather than when it is finished: somebody who skips it has still
            // seen it, and being shown it twice is worse than missing the end.
            if !hasSeenGuide {
                hasSeenGuide = true
                showingGuide = true
            }
        }
        .alert("Remove this disc?", isPresented: Binding(
            get: { confirmingRemoval != nil },
            set: { if !$0 { confirmingRemoval = nil } })) {
            Button("Remove", role: .destructive) {
                if let disc = confirmingRemoval { engine.forget(disc) }
                confirmingRemoval = nil
            }
            Button("Keep", role: .cancel) { confirmingRemoval = nil }
        } message: {
            Text("The file is deleted from the app. Your original is untouched.")
        }
        .onChange(of: engine.message) { text in
            if let text, !text.isEmpty { note = text; engine.message = nil }
        }
    }

    private var header: some View {
        HStack(alignment: .firstTextBaseline) {
            Text("Retro-3DO")
                .font(.system(size: 30, weight: .semibold, design: .rounded))
                .foregroundStyle(Theme.ink)
            Spacer()
            Button { showingSettings = true } label: {
                Image(systemName: "gearshape.fill")
                    .font(.system(size: 20, weight: .medium))
                    .foregroundStyle(Theme.inkDim)
                    .padding(10)
            }
            .accessibilityLabel("Settings")
        }
        .padding(.horizontal, 28)
        .padding(.top, 14)
        .padding(.bottom, 18)
    }
}

// --------------------------------------------------------------------- tiles --

private struct TileShape<Content: View>: View {
    @ViewBuilder var content: Content

    // The aspect ratio is applied to an empty shape with the content overlaid,
    // not to the content. Put it on the content and the content drives the
    // frame: the Demo tile has three lines of text and came out both taller
    // and wider than the two-line Add tile beside it.
    var body: some View {
        Color.clear
            .frame(maxWidth: .infinity)
            .aspectRatio(4.0 / 3.0, contentMode: .fit)   // the machine's own shape
            .overlay(content)
            .background(Theme.tile)
            .clipShape(RoundedRectangle(cornerRadius: Theme.tileCorner, style: .continuous))
            .overlay(
                RoundedRectangle(cornerRadius: Theme.tileCorner, style: .continuous)
                    .strokeBorder(Theme.tileEdge, lineWidth: 1))
    }
}

private struct DiscTile: View {
    let disc: Engine.Disc

    var body: some View {
        TileShape {
            VStack(spacing: 10) {
                Image(systemName: "opticaldisc.fill")
                    .font(.system(size: 42, weight: .regular))
                    .foregroundStyle(Theme.inkDim)
                Text(disc.name)
                    .font(.system(size: 17, weight: .medium, design: .rounded))
                    .foregroundStyle(Theme.ink)
                    .multilineTextAlignment(.center)
                    .lineLimit(2)
                    .padding(.horizontal, 12)
            }
        }
        .contentShape(Rectangle())
        .accessibilityLabel("Play \(disc.name)")
    }
}

private struct AddTile: View {
    var action: () -> Void

    var body: some View {
        Button(action: action) {
            TileShape {
                VStack(spacing: 12) {
                    Image(systemName: "plus")
                        .font(.system(size: 38, weight: .medium))
                        .foregroundStyle(Theme.spectrum)
                    Text("Add a disc")
                        .font(.system(size: 17, weight: .medium, design: .rounded))
                        .foregroundStyle(Theme.ink)
                }
            }
            .overlay(
                RoundedRectangle(cornerRadius: Theme.tileCorner, style: .continuous)
                    .strokeBorder(Theme.tileEdge, style: StrokeStyle(lineWidth: 1.5, dash: [7, 6])))
        }
        .buttonStyle(.plain)
        .accessibilityLabel("Add a disc or system ROM")
    }
}

// One sentence, dismissed by tapping it. The app has no error pages: anything
// that goes wrong is said here in a line a person can act on.
private struct Note: View {
    let text: String
    var dismiss: () -> Void

    var body: some View {
        VStack {
            Spacer()
            Text(text)
                .font(.system(size: 15, weight: .medium))
                .foregroundStyle(Theme.ink)
                .multilineTextAlignment(.center)
                .padding(.horizontal, 22)
                .padding(.vertical, 14)
                .background(Theme.tile)
                .clipShape(Capsule())
                .overlay(Capsule().strokeBorder(Theme.tileEdge, lineWidth: 1))
                .padding(.bottom, 28)
                .onTapGesture(perform: dismiss)
                .task {
                    try? await Task.sleep(nanoseconds: 4_000_000_000)
                    dismiss()
                }
        }
        .transition(.opacity)
    }
}
