import SwiftUI
import UniformTypeIdentifiers

// The only way content enters the app: iOS's own document picker.
//
// What this replaces is worth stating. The previous front end asked the user
// to copy files into the app's Documents folder with the Files app, then find
// them again through an in-app file browser, then type or browse a path into a
// text field and press a separate Load button. None of that survives: the user
// taps +, picks a file wherever it lives — iCloud, a USB drive, Downloads —
// and the app works out what it is.
struct Importer: UIViewControllerRepresentable {
    var onPick: ([URL]) -> Void

    // Deliberately broad. A 3DO disc may arrive as .iso, .bin, .cue, .img or
    // .chd and a system ROM as .rom or .bin; half of those have no registered
    // type on iOS, so filtering by type would hide the user's own files from
    // them. The app identifies what it was given instead of refusing to look.
    private static var types: [UTType] {
        var list: [UTType] = [.data]
        for ext in ["iso", "chd", "cue", "img", "rom", "bin"] {
            if let t = UTType(filenameExtension: ext) { list.append(t) }
        }
        return list
    }

    func makeUIViewController(context: Context) -> UIDocumentPickerViewController {
        // asCopy: the picker's URL is a temporary grant that does not survive
        // a relaunch. Copying is what makes a library entry still work tomorrow.
        let picker = UIDocumentPickerViewController(forOpeningContentTypes: Importer.types,
                                                    asCopy: true)
        picker.allowsMultipleSelection = true
        picker.delegate = context.coordinator
        return picker
    }

    func updateUIViewController(_ controller: UIDocumentPickerViewController, context: Context) {}

    func makeCoordinator() -> Coordinator { Coordinator(onPick: onPick) }

    final class Coordinator: NSObject, UIDocumentPickerDelegate {
        let onPick: ([URL]) -> Void
        init(onPick: @escaping ([URL]) -> Void) { self.onPick = onPick }

        func documentPicker(_ controller: UIDocumentPickerViewController,
                            didPickDocumentsAt urls: [URL]) {
            onPick(urls)
        }
    }
}
