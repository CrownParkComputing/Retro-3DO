import Foundation
import Combine

// The emulator, as Swift sees it.
//
// Everything below forwards to the plain-C bridge. No SwiftUI view touches the
// bridge directly: a view asks this object, and this object is the only thing
// that knows the C names. That is what keeps the front end replaceable without
// anyone reading C++.
@MainActor
final class Engine: ObservableObject {

    struct Disc: Identifiable, Hashable {
        let id: Int
        let name: String
        let path: String
    }

    @Published private(set) var discs: [Disc] = []
    @Published private(set) var hasBios = false
    @Published private(set) var playing = false
    @Published var message: String?
    /// True while a session that the walkthrough started is running. The
    /// session hides its way out to the shelf and goes back to the guide
    /// instead: somebody being walked through the app should not be dropped
    /// somewhere else halfway through a sentence.
    @Published var startedFromGuide = false

    private var started = false

    // Application Support, not Documents. Documents is visible in the Files app
    // because the app enables file sharing, and the save memory and the library
    // index are not things a person should be invited to edit or delete.
    private static var supportDirectory: URL {
        let base = FileManager.default.urls(for: .applicationSupportDirectory,
                                            in: .userDomainMask)[0]
        try? FileManager.default.createDirectory(at: base, withIntermediateDirectories: true)
        return base
    }

    // Where imported files are kept. Copies, not references: a document picker
    // URL is a temporary grant, and a library of dead links is the single most
    // confusing thing an emulator front end can show.
    static var contentDirectory: URL {
        let dir = supportDirectory.appendingPathComponent("Content", isDirectory: true)
        try? FileManager.default.createDirectory(at: dir, withIntermediateDirectories: true)
        return dir
    }

    func start() {
        guard !started else { return }
        started = r3_start(Engine.supportDirectory.path)
        refresh()
    }

    func shutdown() {
        guard started else { return }
        r3_shutdown()
        started = false
    }

    func refresh() {
        hasBios = r3_has_bios()
        var found: [Disc] = []
        for index in 0..<Int(r3_library_count()) {
            found.append(Disc(id: index,
                              name: Engine.text { r3_library_name(Int32(index), $0, $1) },
                              path: Engine.text { r3_library_path(Int32(index), $0, $1) }))
        }
        discs = found
    }

    // --- playing ----------------------------------------------------------

    func playDemo(fromGuide: Bool = false) {
        startedFromGuide = fromGuide
        if r3_play_demo() { playing = true } else { report() }
    }

    func play(_ disc: Disc) {
        startedFromGuide = false
        if r3_play_disc(disc.path) { playing = true } else { report() }
    }

    func stop() {
        r3_stop()
        playing = false
    }

    func setPaused(_ paused: Bool) { r3_set_paused(paused) }
    func reset() { r3_reset() }

    // --- content ----------------------------------------------------------

    /// Takes a file the user picked and decides for them what it was.
    /// Returns the sentence to show, or nil when there is nothing to say
    /// because the obvious thing just happened.
    @discardableResult
    func importFile(at url: URL) -> String? {
        let destination = Engine.contentDirectory
            .appendingPathComponent(url.lastPathComponent)
        do {
            if FileManager.default.fileExists(atPath: destination.path) {
                try FileManager.default.removeItem(at: destination)
            }
            try FileManager.default.copyItem(at: url, to: destination)
        } catch {
            message = "That file could not be copied into the app."
            return message
        }

        var name = [CChar](repeating: 0, count: 256)
        let kind = r3_import(destination.path, &name, 256)
        refresh()

        switch kind {
        case R3_IMPORT_BIOS:
            return "System ROM added. Your discs will play now."
        case R3_IMPORT_DISC:
            return nil                      // the tile appearing says it
        default:
            try? FileManager.default.removeItem(at: destination)
            message = String(cString: r3_last_error())
            if message?.isEmpty != false {
                message = "That file is not a 3DO disc or system ROM."
            }
            return message
        }
    }

    func forget(_ disc: Disc) {
        r3_library_forget(Int32(disc.id))
        try? FileManager.default.removeItem(at: URL(fileURLWithPath: disc.path))
        refresh()
    }

    // --- settings ---------------------------------------------------------

    var isPal: Bool {
        get { r3_region_is_pal() }
        set { r3_set_region(newValue) }
    }

    var framesPerSecond: Double { Double(r3_fps_x100()) / 100.0 }

    // --- helpers ----------------------------------------------------------

    private func report() {
        let text = String(cString: r3_last_error())
        message = text.isEmpty ? "That disc could not be started." : text
    }

    private static func text(_ fill: (UnsafeMutablePointer<CChar>, Int32) -> Int32) -> String {
        var buffer = [CChar](repeating: 0, count: 1024)
        _ = fill(&buffer, 1024)
        return String(cString: buffer)
    }
}
